// Copyright (c) 2015, PG, All rights reserved.
#include "NetworkHandler.h"
#include "Engine.h"
#include "Thread.h"
#include "SString.h"
#include "ConVar.h"
#include "Timing.h"
#include "Logging.h"

#include "curl_blob.h"

#include "curl/curl.h"

#include <utility>

// internal request structure
struct NetworkRequest {
    UString url;
    NetworkHandler::AsyncCallback callback;
    NetworkHandler::RequestOptions options;
    NetworkHandler::Response response;
    CURL* easy_handle{nullptr};
    struct curl_slist* headers_list{nullptr};
    curl_mime* mime{nullptr};

    // for sync requests
    bool is_sync{false};
    void* sync_id{nullptr};

    NetworkRequest(UString u, NetworkHandler::AsyncCallback cb, NetworkHandler::RequestOptions opts)
        : url(std::move(u)), callback(std::move(cb)), options(std::move(opts)) {}
};

NetworkHandler::NetworkHandler() : multi_handle(nullptr) {
    // this needs to be called once to initialize curl on startup
    curl_global_init(CURL_GLOBAL_DEFAULT);

    this->multi_handle = curl_multi_init();
    if(!this->multi_handle) {
        debugLog("ERROR: Failed to initialize curl multi handle!");
        return;
    }

    // start network thread
    this->network_thread = std::make_unique<Sync::jthread>(
        [this](const Sync::stop_token& stopToken) { this->networkThreadFunc(stopToken); });

    if(!this->network_thread->joinable()) {
        debugLog("ERROR: Failed to create network thread!");
    }
}

NetworkHandler::~NetworkHandler() {
    // Sync::jthread destructor automatically requests stop and joins the thread
    this->network_thread.reset();

    // cleanup any remaining requests
    {
        Sync::scoped_lock<Sync::mutex> lock{this->active_requests_mutex};
        for(auto& [handle, request] : this->active_requests) {
            curl_multi_remove_handle(this->multi_handle, handle);
            curl_easy_cleanup(handle);
            if(request->headers_list) {
                curl_slist_free_all(request->headers_list);
            }
        }
        this->active_requests.clear();
    }

    if(this->multi_handle) {
        curl_multi_cleanup(this->multi_handle);
    }

    curl_global_cleanup();
}

void NetworkHandler::networkThreadFunc(const Sync::stop_token& stopToken) {
    McThread::set_current_thread_name("net_manager");
    McThread::set_current_thread_prio(false); // reset priority

    while(!stopToken.stop_requested()) {
        processNewRequests();

        if(!this->active_requests.empty()) {
            int running_handles;
            CURLMcode mres = curl_multi_perform(this->multi_handle, &running_handles);

            if(mres != CURLM_OK) {
                debugLog("curl_multi_perform error: {}", curl_multi_strerror(mres));
            }

            processCompletedRequests();
        }

        if(this->active_requests.empty()) {
            // wait for new requests
            Sync::unique_lock<Sync::mutex> lock{this->request_queue_mutex};

            Sync::stop_callback stopCallback(stopToken, [&]() { this->request_queue_cv.notify_all(); });

            this->request_queue_cv.wait(lock, stopToken, [this] { return !this->pending_requests.empty(); });
        } else {
            // brief sleep to avoid busy waiting
            Timing::sleepMS(1);
        }
    }
}

void NetworkHandler::processNewRequests() {
    Sync::scoped_lock<Sync::mutex> lock{this->request_queue_mutex};

    while(!this->pending_requests.empty()) {
        auto request = std::move(this->pending_requests.front());
        this->pending_requests.pop();

        request->easy_handle = curl_easy_init();
        if(!request->easy_handle) {
            request->response.success = false;
            request->callback(request->response);
            continue;
        }

        setupCurlHandle(request->easy_handle, request.get());

        CURLMcode mres = curl_multi_add_handle(this->multi_handle, request->easy_handle);
        if(mres != CURLM_OK) {
            curl_easy_cleanup(request->easy_handle);
            request->response.success = false;
            request->callback(request->response);
            continue;
        }

        Sync::scoped_lock<Sync::mutex> active_lock{this->active_requests_mutex};
        this->active_requests[request->easy_handle] = std::move(request);
    }
}

void NetworkHandler::processCompletedRequests() {
    CURLMsg* msg;
    int msgs_left;

    // collect completed requests without holding locks during callback execution
    std::vector<std::unique_ptr<NetworkRequest>> completed_requests;

    while((msg = curl_multi_info_read(this->multi_handle, &msgs_left))) {
        if(msg->msg == CURLMSG_DONE) {
            CURL* easy_handle = msg->easy_handle;

            {
                Sync::scoped_lock<Sync::mutex> lock{this->active_requests_mutex};
                auto it = this->active_requests.find(easy_handle);
                if(it != this->active_requests.end()) {
                    auto request = std::move(it->second);
                    this->active_requests.erase(it);

                    curl_multi_remove_handle(this->multi_handle, easy_handle);

                    // get response code
                    curl_easy_getinfo(easy_handle, CURLINFO_RESPONSE_CODE, &request->response.responseCode);
                    request->response.success = (msg->data.result == CURLE_OK);

                    if(request->headers_list) {
                        curl_slist_free_all(request->headers_list);
                    }
                    curl_easy_cleanup(easy_handle);

                    if(request->mime) {
                        curl_mime_free(request->mime);
                        request->mime = nullptr;
                    }

                    if(request->is_sync) {
                        // handle sync request immediately
                        Sync::scoped_lock<Sync::mutex> sync_lock{this->sync_requests_mutex};
                        this->sync_responses[request->sync_id] = request->response;
                        auto cv_it = this->sync_request_cvs.find(request->sync_id);
                        if(cv_it != this->sync_request_cvs.end()) {
                            cv_it->second->notify_one();
                        }
                    } else {
                        // defer async callback execution
                        completed_requests.push_back(std::move(request));
                    }
                }
            }  // release active_requests_mutex here
        }
    }

    // execute callbacks without holding any locks
    for(auto& request : completed_requests) {
        request->callback(request->response);
    }
}

int NetworkHandler::progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t /*unused*/,
                                     curl_off_t /*unused*/) {
    auto* request = static_cast<NetworkRequest*>(clientp);
    if(request->options.progressCallback && dltotal > 0) {
        float progress = static_cast<float>(dlnow) / static_cast<float>(dltotal);
        request->options.progressCallback(progress);
    }
    return 0;
}

void NetworkHandler::setupCurlHandle(CURL* handle, NetworkRequest* request) {
    curl_easy_setopt(handle, CURLOPT_VERBOSE, cv::debug_network.getVal<long>());
    curl_easy_setopt(handle, CURLOPT_URL, request->url.toUtf8());
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, request->options.connectTimeout);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, request->options.timeout);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, request);
    curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(handle, CURLOPT_HEADERDATA, request);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, cv::ssl_verify.getBool() ? 2L : 0L);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, cv::ssl_verify.getBool() ? 1L : 0L);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1L);  // fail on HTTP responses >= 400

    if(!request->options.userAgent.empty()) {
        curl_easy_setopt(handle, CURLOPT_USERAGENT, request->options.userAgent.c_str());
    }

    if(request->options.followRedirects) {
        curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    }

    curl_easy_setopt_CAINFO_BLOB_embedded(handle);

    // setup headers
    if(!request->options.headers.empty()) {
        for(const auto& [key, value] : request->options.headers) {
            std::string header = fmt::format("{}: {}", key, value);
            request->headers_list = curl_slist_append(request->headers_list, header.c_str());
        }
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, request->headers_list);
    }

    // setup POST data
    if(!request->options.postData.empty()) {
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, request->options.postData.c_str());
        curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, request->options.postData.length());
    }

    // setup MIME data
    if(!request->options.mimeParts.empty()) {
        request->mime = curl_mime_init(handle);

        for(const auto& it : request->options.mimeParts) {
            auto part = curl_mime_addpart(request->mime);
            if(!it.filename.empty()) {
                curl_mime_filename(part, it.filename.c_str());
            }
            curl_mime_name(part, it.name.c_str());
            curl_mime_data(part, (const char*)it.data.data(), it.data.size());
        }

        curl_easy_setopt(handle, CURLOPT_MIMEPOST, request->mime);
    }

    // setup progress callback if provided
    if(request->options.progressCallback) {
        curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(handle, CURLOPT_XFERINFOFUNCTION, progressCallback);
        curl_easy_setopt(handle, CURLOPT_XFERINFODATA, request);
    }
}

size_t NetworkHandler::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* request = static_cast<NetworkRequest*>(userp);
    size_t real_size = size * nmemb;
    request->response.body.append(static_cast<char*>(contents), real_size);
    return real_size;
}

size_t NetworkHandler::headerCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    auto* request = static_cast<NetworkRequest*>(userdata);
    size_t real_size = size * nitems;

    std::string header(buffer, real_size);
    size_t colon_pos = header.find(':');
    if(colon_pos != std::string::npos) {
        std::string key = header.substr(0, colon_pos);
        std::string value = header.substr(colon_pos + 1);

        // lowercase the key for consistency between platforms/curl builds
        SString::to_lower(key);

        // trim whitespace
        SString::trim(&key);
        SString::trim(&value);

        request->response.headers[key] = value;
    }

    return real_size;
}

void NetworkHandler::httpRequestAsync(const UString& url, AsyncCallback callback, const RequestOptions& options) {
    auto request = std::make_unique<NetworkRequest>(url, std::move(callback), options);

    Sync::scoped_lock<Sync::mutex> lock{this->request_queue_mutex};
    this->pending_requests.push(std::move(request));
    this->request_queue_cv.notify_one();
}

// synchronous API (blocking)
NetworkHandler::Response NetworkHandler::performSyncRequest(const UString& url, const RequestOptions& options) {
    Response result;
    Sync::condition_variable cv;
    Sync::mutex cv_mutex;

    void* sync_id = &cv;

    // register sync request
    {
        Sync::scoped_lock<Sync::mutex> lock{this->sync_requests_mutex};
        this->sync_request_cvs[sync_id] = &cv;
    }

    // create sync request
    auto request = std::make_unique<NetworkRequest>(url, [](const Response&) {}, options);
    request->is_sync = true;
    request->sync_id = sync_id;

    // submit request
    {
        Sync::scoped_lock<Sync::mutex> lock{this->request_queue_mutex};
        this->pending_requests.push(std::move(request));
        this->request_queue_cv.notify_one();
    }

    // wait for completion
    Sync::unique_lock<Sync::mutex> lock{cv_mutex};
    cv.wait(lock, [&] {
        Sync::scoped_lock<Sync::mutex> sync_lock{this->sync_requests_mutex};
        return this->sync_responses.find(sync_id) != this->sync_responses.end();
    });

    // get result and cleanup
    {
        Sync::scoped_lock<Sync::mutex> sync_lock{this->sync_requests_mutex};
        result = this->sync_responses[sync_id];
        this->sync_responses.erase(sync_id);
        this->sync_request_cvs.erase(sync_id);
    }

    return result;
}

UString NetworkHandler::httpGet(const UString& url, long timeout, long connectTimeout) {
    RequestOptions options;
    options.timeout = timeout;
    options.connectTimeout = connectTimeout;

    Response response = performSyncRequest(url, options);

    if(!response.success) {
        debugLog("Error while fetching {}: HTTP {}", url.toUtf8(), response.responseCode);
        return {""};
    }

    return UString{response.body};
}

std::string NetworkHandler::httpDownload(const UString& url, long timeout, long connectTimeout) {
    RequestOptions options;
    options.timeout = timeout;
    options.connectTimeout = connectTimeout;
    options.followRedirects = true;

    Response response = performSyncRequest(url, options);

    if(!response.success) {
        debugLog("ERROR ({}): HTTP request failed", response.responseCode);
        return {""};
    }

    return response.body;
}

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
struct NetworkHandler::NetworkRequest {
    std::string url;
    AsyncCallback callback;
    RequestOptions options;
    Response response;
    CURL* easy_handle{nullptr};
    struct curl_slist* headers_list{nullptr};
    curl_mime* mime{nullptr};

    // for sync requests
    bool is_sync{false};
    void* sync_id{nullptr};

    NetworkRequest(std::string_view url, AsyncCallback cb, RequestOptions opts)
        : url(url), callback(std::move(cb)), options(std::move(opts)) {}
};

NetworkHandler::NetworkHandler() {
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
        Sync::scoped_lock active_lock{this->active_requests_mutex};
        for(auto& [handle, request] : this->active_requests) {
            curl_multi_remove_handle(this->multi_handle, handle);
            curl_easy_cleanup(handle);
            if(request->headers_list) {
                curl_slist_free_all(request->headers_list);
            }
        }
        this->active_requests.clear();

        Sync::scoped_lock completed_lock{this->completed_requests_mutex};
        this->completed_requests.clear();
    }

    if(this->multi_handle) {
        curl_multi_cleanup(this->multi_handle);
    }

    curl_global_cleanup();
}

void NetworkHandler::networkThreadFunc(const Sync::stop_token& stopToken) {
    McThread::set_current_thread_name("net_manager");
    McThread::set_current_thread_prio(false);  // reset priority

    while(!stopToken.stop_requested()) {
        processNewRequests();

        if(!this->active_requests.empty()) {
            i32 running_handles;
            CURLMcode mres = curl_multi_perform(this->multi_handle, &running_handles);

            if(mres != CURLM_OK) {
                debugLog("curl_multi_perform error: {}", curl_multi_strerror(mres));
            }

            processCompletedRequests();
        }

        if(this->active_requests.empty()) {
            // wait for new requests
            Sync::unique_lock lock{this->request_queue_mutex};

            Sync::stop_callback stopCallback(stopToken, [&]() { this->request_queue_cv.notify_all(); });

            this->request_queue_cv.wait(lock, stopToken, [this] { return !this->pending_requests.empty(); });
        } else {
            // brief sleep to avoid busy waiting
            Timing::sleepMS(1);
        }
    }
}

void NetworkHandler::processNewRequests() {
    Sync::scoped_lock requests_lock{this->request_queue_mutex};

    while(!this->pending_requests.empty()) {
        auto request = std::move(this->pending_requests.front());
        this->pending_requests.pop();

        request->easy_handle = curl_easy_init();
        if(!request->easy_handle) {
            request->response.success = false;

            Sync::scoped_lock completed_lock{this->completed_requests_mutex};
            this->completed_requests.push_back(std::move(request));
            continue;
        }

        setupCurlHandle(request->easy_handle, request.get());

        // curl_multi broken on websockets
        // HACK: we're blocking whole network thread here, while websocket is connecting
        if(request->options.is_websocket) {
            auto res = curl_easy_perform(request->easy_handle);
            curl_easy_getinfo(request->easy_handle, CURLINFO_RESPONSE_CODE, &request->response.response_code);
            request->response.success = (res == CURLE_OK) && (request->response.response_code == 101);

            if(request->headers_list) {
                curl_slist_free_all(request->headers_list);
            }
            if(request->mime) {
                curl_mime_free(request->mime);
                request->mime = nullptr;
            }

            if(!request->response.success) {
                curl_easy_cleanup(request->easy_handle);
                continue;
            }

            // pass websocket handle
            request->response.easy_handle = request->easy_handle;

            // defer async callback execution
            Sync::scoped_lock completed_lock{this->completed_requests_mutex};
            this->completed_requests.push_back(std::move(request));
            continue;
        }

        CURLMcode mres = curl_multi_add_handle(this->multi_handle, request->easy_handle);
        if(mres != CURLM_OK) {
            curl_easy_cleanup(request->easy_handle);
            request->response.success = false;

            Sync::scoped_lock completed_lock{this->completed_requests_mutex};
            this->completed_requests.push_back(std::move(request));
            continue;
        }

        Sync::scoped_lock active_lock{this->active_requests_mutex};
        this->active_requests[request->easy_handle] = std::move(request);
    }
}

void NetworkHandler::processCompletedRequests() {
    CURLMsg* msg;
    i32 msgs_left;

    // collect completed requests without holding locks during callback execution
    while((msg = curl_multi_info_read(this->multi_handle, &msgs_left))) {
        if(msg->msg == CURLMSG_DONE) {
            CURL* easy_handle = msg->easy_handle;

            {
                Sync::scoped_lock active_lock{this->active_requests_mutex};
                auto it = this->active_requests.find(easy_handle);
                if(it != this->active_requests.end()) {
                    auto request = std::move(it->second);
                    this->active_requests.erase(it);

                    curl_multi_remove_handle(this->multi_handle, easy_handle);

                    // get response code
                    curl_easy_getinfo(easy_handle, CURLINFO_RESPONSE_CODE, &request->response.response_code);
                    request->response.success = (msg->data.result == CURLE_OK);

                    if(request->headers_list) {
                        curl_slist_free_all(request->headers_list);
                    }
                    if(request->mime) {
                        curl_mime_free(request->mime);
                        request->mime = nullptr;
                    }

                    curl_easy_cleanup(request->easy_handle);
                    request->easy_handle = nullptr;

                    if(request->is_sync) {
                        // handle sync request immediately
                        Sync::scoped_lock sync_lock{this->sync_requests_mutex};
                        this->sync_responses[request->sync_id] = request->response;
                        auto cv_it = this->sync_request_cvs.find(request->sync_id);
                        if(cv_it != this->sync_request_cvs.end()) {
                            cv_it->second->notify_one();
                        }
                    } else {
                        // defer async callback execution
                        Sync::scoped_lock completed_lock{this->completed_requests_mutex};
                        this->completed_requests.push_back(std::move(request));
                    }
                }
            }  // release active_requests_mutex here
        }
    }
}

i32 NetworkHandler::progressCallback(void* clientp, i64 dltotal, i64 dlnow, i64 /*unused*/, i64 /*unused*/) {
    auto* request = static_cast<NetworkRequest*>(clientp);
    if(request->options.progress_callback && dltotal > 0) {
        float progress = static_cast<float>(dlnow) / static_cast<float>(dltotal);
        request->options.progress_callback(progress);
    }
    return 0;
}

void NetworkHandler::setupCurlHandle(CURL* handle, NetworkRequest* request) {
    curl_easy_setopt(handle, CURLOPT_VERBOSE, cv::debug_network.getVal<long>());
    curl_easy_setopt(handle, CURLOPT_URL, request->url.c_str());
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, request->options.connect_timeout);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, request->options.timeout);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, request);
    curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(handle, CURLOPT_HEADERDATA, request);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, cv::ssl_verify.getBool() ? 2L : 0L);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, cv::ssl_verify.getBool() ? 1L : 0L);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1L);  // fail on HTTP responses >= 400

    if(!request->options.user_agent.empty()) {
        curl_easy_setopt(handle, CURLOPT_USERAGENT, request->options.user_agent.c_str());
    }

    if(request->options.follow_redirects) {
        curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    }

    if(request->options.is_websocket) {
        // Special behavior: on CURLOPT_CONNECT_ONLY == 2,
        // curl actually waits for server response on perform
        curl_easy_setopt(handle, CURLOPT_CONNECT_ONLY, 2L);
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
    if(!request->options.post_data.empty()) {
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, request->options.post_data.c_str());
        curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, request->options.post_data.length());
    }

    // setup MIME data
    if(!request->options.mime_parts.empty()) {
        request->mime = curl_mime_init(handle);

        for(const auto& it : request->options.mime_parts) {
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
    if(request->options.progress_callback) {
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
        SString::lower_inplace(key);

        // trim whitespace
        SString::trim_inplace(key);
        SString::trim_inplace(value);

        request->response.headers[key] = value;
    }

    return real_size;
}

// Callbacks will all be run on the main thread, in engine->update()
void NetworkHandler::update() {
    std::vector<std::unique_ptr<NetworkRequest>> responses_to_handle;
    {
        Sync::scoped_lock lock{this->completed_requests_mutex};
        responses_to_handle = std::move(this->completed_requests);
        this->completed_requests.clear();
    }
    for(auto& request : responses_to_handle) {
        request->callback(request->response);
    }

    // websocket recv
    for(auto& ws : this->active_websockets) {
        u64 bytes_available = ws->max_recv - (ws->in.size() + ws->in_partial.size());

        CURLcode res = CURLE_OK;
        while(res == CURLE_OK && bytes_available > 0) {
            u8 buf[65000];
            size_t nb_read = 0;
            const struct curl_ws_frame* meta = nullptr;
            res = curl_ws_recv(ws->handle, buf, sizeof(buf), &nb_read, &meta);

            if(res == CURLE_OK) {
                if(nb_read > 0 && (meta->flags & CURLWS_BINARY)) {
                    ws->in_partial.insert(ws->in_partial.end(), buf, buf + nb_read);
                    bytes_available -= nb_read;
                }
                if(!ws->in_partial.empty() && meta->bytesleft == 0) {
                    ws->in.insert(ws->in.end(), ws->in_partial.begin(), ws->in_partial.end());
                    ws->in_partial.clear();
                }
            } else if(res == CURLE_AGAIN) {
                // nothing to do
            } else if(res == CURLE_GOT_NOTHING) {
                debugLog("Websocket connection closed.");
                ws->status = WEBSOCKET_DISCONNECTED;
            } else {
                debugLog("Failed to receive data on websocket: {}", curl_easy_strerror(res));
                ws->status = WEBSOCKET_DISCONNECTED;
            }
        }
    }
    std::erase_if(this->active_websockets, [](const auto& ws) { return ws->status != WEBSOCKET_CONNECTED; });

    // websocket send
    for(auto& ws : this->active_websockets) {
        CURLcode res = CURLE_OK;
        while(res == CURLE_OK && !ws->out.empty()) {
            size_t nb_sent = 0;
            res = curl_ws_send(ws->handle, ws->out.data(), ws->out.size(), &nb_sent, 0, CURLWS_BINARY);
            ws->out.erase(ws->out.begin(), ws->out.begin() + nb_sent);
        }

        if(res != CURLE_AGAIN && !ws->out.empty()) {
            debugLog("Failed to send data on websocket: {}", curl_easy_strerror(res));
            ws->status = WEBSOCKET_DISCONNECTED;
        }
    }
    std::erase_if(this->active_websockets, [](const auto& ws) { return ws->status != WEBSOCKET_CONNECTED; });
}

void NetworkHandler::httpRequestAsync(std::string_view url, AsyncCallback callback, RequestOptions options) {
    auto request = std::make_unique<NetworkRequest>(url, std::move(callback), std::move(options));

    Sync::scoped_lock lock{this->request_queue_mutex};
    this->pending_requests.push(std::move(request));
    this->request_queue_cv.notify_one();
}

NetworkHandler::Websocket::~Websocket() {
    // This might be a bit mean to the server, not sending CLOSE message
    // But we send LOGOUT packet on http anyway, so doesn't matter
    curl_easy_cleanup(this->handle);
    this->handle = nullptr;
}

std::shared_ptr<NetworkHandler::Websocket> NetworkHandler::initWebsocket(const WebsocketOptions& options) {
    assert(options.url.starts_with("ws://") || options.url.starts_with("wss://"));

    auto websocket = std::make_shared<Websocket>();
    websocket->max_recv = options.max_recv;
    websocket->time_created = engine->getTime();

    RequestOptions httpOptions;
    httpOptions.headers = options.headers;
    httpOptions.user_agent = options.user_agent;
    httpOptions.timeout = options.timeout;
    httpOptions.connect_timeout = options.connect_timeout;
    httpOptions.is_websocket = true;

    this->httpRequestAsync(
        options.url,
        [this, websocket](const NetworkHandler::Response& response) {
            if(response.success) {
                websocket->handle = response.easy_handle;
                websocket->status = WEBSOCKET_CONNECTED;
                this->active_websockets.push_back(websocket);
            } else {
                websocket->status = WEBSOCKET_UNSUPPORTED;
            }
        },
        httpOptions);

    return websocket;
}

// synchronous API (blocking)
NetworkHandler::Response NetworkHandler::httpRequestSynchronous(std::string_view url, RequestOptions options) {
    Response result;
    Sync::condition_variable cv;
    Sync::mutex cv_mutex;

    void* sync_id = &cv;

    // register sync request
    {
        Sync::scoped_lock lock{this->sync_requests_mutex};
        this->sync_request_cvs[sync_id] = &cv;
    }

    // create sync request
    auto request = std::make_unique<NetworkRequest>(url, [](const Response&) {}, std::move(options));
    request->is_sync = true;
    request->sync_id = sync_id;

    // submit request
    {
        Sync::scoped_lock lock{this->request_queue_mutex};
        this->pending_requests.push(std::move(request));
        this->request_queue_cv.notify_one();
    }

    // wait for completion
    Sync::unique_lock lock{cv_mutex};
    cv.wait(lock, [&] {
        Sync::scoped_lock sync_lock{this->sync_requests_mutex};
        return this->sync_responses.find(sync_id) != this->sync_responses.end();
    });

    // get result and cleanup
    {
        Sync::scoped_lock sync_lock{this->sync_requests_mutex};
        result = this->sync_responses[sync_id];
        this->sync_responses.erase(sync_id);
        this->sync_request_cvs.erase(sync_id);
    }

    return result;
}

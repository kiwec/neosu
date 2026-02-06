// Copyright (c) 2026, WH & 2026, kiwec, All rights reserved.
// WASM networking implementation using Emscripten Fetch API (no curl dependency)
#include "config.h"

#ifdef MCENGINE_PLATFORM_WASM

#include "NetworkHandler.h"
#include "Engine.h"
#include "Logging.h"

#include <emscripten/fetch.h>
#include <emscripten/em_js.h>
#include <cstring>
#include <cstdlib>

// the unnecessary-value-param one is a bit unfortunate, but the main NetworkHandler implementation expects these to be moved
// into internally-held data, and changing the interface is a bit more convoluted than what's worth it

// NOLINTBEGIN(performance-unnecessary-value-param, cppcoreguidelines-pro-bounds-array-to-pointer-decay, hicpp-no-array-decay)

// clang-format off
// NOLINTBEGIN
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Winvalid-pp-token" // not 100% sure this is ignoreable, but i think it is?

// raw synchronous XMLHttpRequest:
// - works on the main browser thread unlike EMSCRIPTEN_FETCH_SYNCHRONOUS
//   (which requires Atomics.wait, blocked on the main thread by browsers).
// - allocates response body/headers/error via _malloc; caller must free() them.
EM_JS(int, sync_xhr, (const char* method, const char* url, const char* req_headers,
                       const char* body, int body_len,
                       char** out_body, char** out_headers, char** out_error), {
    try {
        var xhr = new XMLHttpRequest();
        xhr.open(UTF8ToString(method), UTF8ToString(url), false);

        if (req_headers) {
            UTF8ToString(req_headers).split('\r\n').forEach(function(line) {
                if (!line) return;
                var sep = line.indexOf(': ');
                if (sep > 0) xhr.setRequestHeader(line.substring(0, sep), line.substring(sep + 2));
            });
        }

        if (body && body_len > 0) {
            xhr.send(HEAPU8.slice(body, body + body_len));
        } else {
            xhr.send();
        }

        var text = xhr.responseText || '';
        if (text.length) {
            var n = lengthBytesUTF8(text) + 1;
            var p = _malloc(n);
            stringToUTF8(text, p, n);
            HEAPU32[out_body >> 2] = p;
        }

        var hdrs = xhr.getAllResponseHeaders() || '';
        if (hdrs.length) {
            var n = lengthBytesUTF8(hdrs) + 1;
            var p = _malloc(n);
            stringToUTF8(hdrs, p, n);
            HEAPU32[out_headers >> 2] = p;
        }

        return xhr.status;
    } catch (e) {
        var msg = '' + (e.message || e);
        var n = lengthBytesUTF8(msg) + 1;
        var p = _malloc(n);
        stringToUTF8(msg, p, n);
        HEAPU32[out_error >> 2] = p;
        return 0;
    }
})
#pragma GCC diagnostic pop
// clang-format on
// NOLINTEND

namespace Mc::Net {

WSInstance::~WSInstance() = default;

std::string urlEncode(std::string_view input) noexcept {
    std::string result;
    result.reserve(input.size());

    static constexpr char hex[] = "0123456789ABCDEF";
    for(unsigned char c : input) {
        // RFC 3986 unreserved characters
        if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
           c == '.' || c == '~') {
            result += static_cast<char>(c);
        } else {
            result += '%';
            result += hex[c >> 4];
            result += hex[c & 0x0F];
        }
    }

    return result;
}

struct NetworkImpl {
   private:
    NOCOPY_NOMOVE(NetworkImpl)

   public:
    NetworkImpl() = default;
    ~NetworkImpl() = default;

    struct Request {
        std::string url;
        RequestOptions options;
        AsyncCallback callback;
        NetworkImpl* impl;

        // storage for header C strings (kept alive until fetch completes)
        std::vector<std::string> header_storage;
        std::vector<const char*> header_ptrs;  // null-terminated array of alternating key/value

        Request(NetworkImpl* impl, std::string_view url, RequestOptions opts, AsyncCallback cb = {})
            : url(url), options(std::move(opts)), callback(std::move(cb)), impl(impl) {}
    };

    // necessary data for deferred callback execution
    struct CompletedRequest {
        AsyncCallback callback;
        Response response;
    };

    void httpRequestAsync(std::string_view url, RequestOptions options, AsyncCallback callback);
    Response httpRequestSynchronous(std::string_view url, const RequestOptions& options);
    void update();

    std::vector<CompletedRequest> completed_requests;

   private:
    static Hash::unstable_stringmap<std::string> extractHeaders(emscripten_fetch_t* fetch);
    static void fetchSuccess(emscripten_fetch_t* fetch);
    static void fetchError(emscripten_fetch_t* fetch);
    static void fetchProgress(emscripten_fetch_t* fetch);
};

Hash::unstable_stringmap<std::string> NetworkImpl::extractHeaders(emscripten_fetch_t* fetch) {
    Hash::unstable_stringmap<std::string> out;

    size_t s_headers = emscripten_fetch_get_response_headers_length(fetch);
    if(s_headers > 0) {
        s_headers++;  // null terminator

        auto raw_headers = std::make_unique<char[]>(s_headers);
        emscripten_fetch_get_response_headers(fetch, raw_headers.get(), s_headers);

        // returns {"key1", "value1", "key2", "value2", ..., 0}
        char** kv_headers = emscripten_fetch_unpack_response_headers(raw_headers.get());
        for(int i = 0; kv_headers[i] != nullptr && kv_headers[i + 1] != nullptr; i += 2) {
            out[kv_headers[i]] = kv_headers[i + 1];
        }
        emscripten_fetch_free_unpacked_response_headers(kv_headers);
    }

    return out;
}

void NetworkImpl::fetchSuccess(emscripten_fetch_t* fetch) {
    auto* request = static_cast<Request*>(fetch->userData);

    Response res;
    res.response_code = fetch->status;
    res.body = std::string(fetch->data, fetch->numBytes);
    res.headers = extractHeaders(fetch);
    res.success = true;

    if(request->callback) {
        request->impl->completed_requests.push_back({std::move(request->callback), std::move(res)});
    }

    delete request;
    emscripten_fetch_close(fetch);
}

void NetworkImpl::fetchError(emscripten_fetch_t* fetch) {
    auto* request = static_cast<Request*>(fetch->userData);

    Response res;
    res.response_code = fetch->status;
    if(fetch->data && fetch->numBytes > 0) {
        res.body = std::string(fetch->data, fetch->numBytes);
    }
    res.headers = extractHeaders(fetch);
    res.error_msg = fetch->statusText;
    res.success = false;

    if(request->callback) {
        request->impl->completed_requests.push_back({std::move(request->callback), std::move(res)});
    }

    delete request;
    emscripten_fetch_close(fetch);
}

void NetworkImpl::fetchProgress(emscripten_fetch_t* fetch) {
    auto* request = static_cast<Request*>(fetch->userData);

    if(request->options.progress_callback && fetch->totalBytes > 0) {
        float progress = static_cast<float>(fetch->dataOffset) / static_cast<float>(fetch->totalBytes);
        request->options.progress_callback(progress);
    }
}

void NetworkImpl::httpRequestAsync(std::string_view url, RequestOptions options, AsyncCallback callback) {
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);

    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_REPLACE;
    attr.timeoutMSecs = options.timeout * 1000;
    attr.withCredentials = false;

    // request owns the strings; pointers into header_storage are valid until Request is deleted
    auto* request = new Request(this, url, std::move(options), std::move(callback));

    if(!request->options.headers.empty()) {
        request->header_storage.reserve(request->options.headers.size() * 2);
        request->header_ptrs.reserve(request->options.headers.size() * 2 + 1);

        for(const auto& [key, value] : request->options.headers) {
            request->header_storage.push_back(key);
            request->header_storage.push_back(value);
        }
        for(const auto& s : request->header_storage) {
            request->header_ptrs.push_back(s.c_str());
        }
        request->header_ptrs.push_back(nullptr);

        attr.requestHeaders = request->header_ptrs.data();
    }

    if(!request->options.post_data.empty()) {
        strcpy(attr.requestMethod, "POST");
        attr.requestData = request->options.post_data.c_str();
        attr.requestDataSize = request->options.post_data.length();
    }

    attr.userData = request;
    attr.onsuccess = &NetworkImpl::fetchSuccess;
    attr.onerror = &NetworkImpl::fetchError;
    attr.onprogress = &NetworkImpl::fetchProgress;

    emscripten_fetch(&attr, request->url.c_str());
}

// parse raw headers from XMLHttpRequest.getAllResponseHeaders() format ("Key: Value\r\n")
static Hash::unstable_stringmap<std::string> parseRawHeaders(const char* raw) {
    Hash::unstable_stringmap<std::string> out;
    if(!raw) return out;

    std::string_view sv(raw);
    size_t pos = 0;
    while(pos < sv.size()) {
        auto eol = sv.find("\r\n", pos);
        if(eol == std::string_view::npos) eol = sv.size();

        auto line = sv.substr(pos, eol - pos);
        auto colon = line.find(':');
        if(colon != std::string_view::npos) {
            std::string key(line.substr(0, colon));
            auto val = line.substr(colon + 1);
            if(!val.empty() && val.front() == ' ') val.remove_prefix(1);

            // lowercase key for consistency with curl implementation
            for(char& c : key) {
                if(c >= 'A' && c <= 'Z') c += 32;
            }

            out[std::move(key)] = std::string(val);
        }

        pos = (eol == sv.size()) ? eol : eol + 2;
    }

    return out;
}

Response NetworkImpl::httpRequestSynchronous(std::string_view url, const RequestOptions& options) {
    std::string method = options.post_data.empty() ? "GET" : "POST";
    std::string url_str(url);

    // format headers as "Key: Value\r\n" pairs
    std::string headers_str;
    for(const auto& [key, value] : options.headers) {
        headers_str += key;
        headers_str += ": ";
        headers_str += value;
        headers_str += "\r\n";
    }

    char* out_body = nullptr;
    char* out_headers = nullptr;
    char* out_error = nullptr;

    int status = sync_xhr(method.c_str(), url_str.c_str(), headers_str.empty() ? nullptr : headers_str.c_str(),
                          options.post_data.empty() ? nullptr : options.post_data.c_str(),
                          static_cast<int>(options.post_data.length()), &out_body, &out_headers, &out_error);

    Response res;
    res.response_code = status;
    res.success = (status >= 200 && status < 400);

    if(out_body) {
        res.body = out_body;
        free(out_body);
    }
    if(out_headers) {
        res.headers = parseRawHeaders(out_headers);
        free(out_headers);
    }
    if(out_error) {
        res.error_msg = out_error;
        free(out_error);
    } else if(!res.success) {
        res.error_msg = "HTTP " + std::to_string(status);
    }

    return res;
}

// callbacks are deferred to run here, during the engine update tick
void NetworkImpl::update() {
    auto pending = std::move(completed_requests);
    completed_requests.clear();
    for(auto& completed : pending) {
        completed.callback(std::move(completed.response));
    }
}

// passthroughs
NetworkHandler::NetworkHandler() : pImpl() {}
NetworkHandler::~NetworkHandler() = default;

Response NetworkHandler::httpRequestSynchronous(std::string_view url, RequestOptions options) {
    return pImpl->httpRequestSynchronous(url, options);
}

void NetworkHandler::httpRequestAsync(std::string_view url, RequestOptions options, AsyncCallback callback) {
    return pImpl->httpRequestAsync(url, std::move(options), std::move(callback));
}

std::shared_ptr<WSInstance> NetworkHandler::initWebsocket(const WSOptions& /*options*/) {
    debugLog("WARNING: WebSocket support is not yet implemented for WASM");
    auto ws = std::make_shared<WSInstance>();
    ws->status = WSStatus::UNSUPPORTED;
    return ws;
}

// no-op
void NetworkHandler::setIPCSocket(int /*fd*/, IPCCallback /*callback*/) {}

void NetworkHandler::update() { pImpl->update(); }

}  // namespace Mc::Net

// NOLINTEND(performance-unnecessary-value-param, cppcoreguidelines-pro-bounds-array-to-pointer-decay, hicpp-no-array-decay)

#endif  // MCENGINE_PLATFORM_WASM

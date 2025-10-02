#pragma once
// Copyright (c) 2015, PG, All rights reserved.
#include "noinclude.h"
#include "types.h"
#include "SyncJthread.h"
#include "SyncCV.h"

#include <string>
#include <string_view>
#include <functional>
#include <map>
#include <memory>
#include <queue>

// forward defs
typedef void CURLM;
typedef void CURL;

class NetworkHandler {
    NOCOPY_NOMOVE(NetworkHandler)
   public:
    NetworkHandler();
    ~NetworkHandler();

    enum WebsocketStatus : u8 {
        WEBSOCKET_CONNECTING,
        WEBSOCKET_CONNECTED,
        WEBSOCKET_DISCONNECTED,
        WEBSOCKET_UNSUPPORTED,
    };

    struct WebsocketOptions {
        std::string url;
        std::map<std::string, std::string> headers;
        std::string user_agent;
        long timeout{5};
        long connect_timeout{5};
        u64 max_recv{10ULL * 1024 * 1024};  // limit "in" buffer to 10Mb
    };

    struct Websocket {
       private:
        NOCOPY_NOMOVE(Websocket)
        friend class NetworkHandler;

       public:
        Websocket() = default;
        ~Websocket();

        std::atomic<u8> status{WEBSOCKET_CONNECTING};
        std::vector<u8> in;
        std::vector<u8> out;
        f64 time_created;

       private:
        CURL* handle{nullptr};

        // Servers can send fragmented packets, we want to only append them
        // to "in" once the packets are complete.
        std::vector<u8> in_partial;
        u64 max_recv{0};  // in bytes
    };

    // async request options
    struct RequestOptions {
       private:
        friend class NetworkHandler;

        struct MimePart {
            std::string filename{};
            std::string name{};
            std::vector<u8> data{};
        };

       public:
        RequestOptions() noexcept { ; }  // = default breaks clang
        std::map<std::string, std::string> headers;
        std::string post_data;
        std::string user_agent;
        std::vector<MimePart> mime_parts;
        std::function<void(float)> progress_callback;  // progress callback for downloads
        long timeout{5};
        long connect_timeout{5};
        bool follow_redirects{false};

       private:
        bool is_websocket{false};
    };

    // async response data
    struct Response {
       private:
        friend class NetworkHandler;

        // HACK for passing websocket handle
        CURL* easy_handle{nullptr};

       public:
        long response_code{0};
        std::string body;
        std::map<std::string, std::string> headers;
        bool success{false};
    };

    // callback update tick
    void update();

    // synchronous requests
    Response httpRequestSynchronous(std::string_view url, RequestOptions options);

    // asynchronous API
    using AsyncCallback = std::function<void(Response response)>;
    void httpRequestAsync(std::string_view url, AsyncCallback callback, RequestOptions options = {});

    // websockets
    std::shared_ptr<Websocket> initWebsocket(const WebsocketOptions& options);

   private:
    // forward declare for async requests
    struct NetworkRequest;

    // curl_multi implementation
    CURLM* multi_handle{nullptr};
    std::unique_ptr<Sync::jthread> network_thread;

    // request queuing
    Sync::mutex request_queue_mutex;
    Sync::condition_variable_any request_queue_cv;
    std::queue<std::unique_ptr<NetworkRequest>> pending_requests;

    // active requests tracking
    Sync::mutex active_requests_mutex;
    std::map<CURL*, std::unique_ptr<NetworkRequest>> active_requests;

    // completed requests
    Sync::mutex completed_requests_mutex;
    std::vector<std::unique_ptr<NetworkRequest>> completed_requests;

    // sync request support
    Sync::mutex sync_requests_mutex;
    std::map<void*, Sync::condition_variable*> sync_request_cvs;
    std::map<void*, Response> sync_responses;

    // websockets
    std::vector<std::shared_ptr<Websocket>> active_websockets;

    void processNewRequests();
    void processCompletedRequests();

    void setupCurlHandle(CURL* handle, NetworkRequest* request);
    static uSz headerCallback(char* buffer, uSz size, uSz nitems, void* userdata);
    static uSz writeCallback(void* contents, uSz size, uSz nmemb, void* userp);
    static i32 progressCallback(void* clientp, i64 dltotal, i64 dlnow, i64, i64);

    // main async thread
    void networkThreadFunc(const Sync::stop_token& stopToken);
};

extern std::unique_ptr<NetworkHandler> networkHandler;

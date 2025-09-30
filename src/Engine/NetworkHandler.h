#pragma once
// Copyright (c) 2015, PG, All rights reserved.
#include "UString.h"
#include "types.h"
#include "SyncJthread.h"
#include "SyncCV.h"

#include <functional>
#include <map>
#include <memory>
#include <queue>

// forward declare for async requests
struct NetworkRequest;

// forward defs
typedef void CURLM;
typedef void CURL;

typedef int64_t curl_off_t;

class NetworkHandler {
    NOCOPY_NOMOVE(NetworkHandler)

   public:
    enum WebsocketStatus {
        WEBSOCKET_CONNECTING,
        WEBSOCKET_CONNECTED,
        WEBSOCKET_DISCONNECTED,
        WEBSOCKET_UNSUPPORTED,
    };

    struct WebsocketOptions {
        std::string url;
        std::map<std::string, std::string> headers;
        std::string userAgent;
        long timeout{5};
        long connectTimeout{5};
        u64 maxRecvBytes{10485760};  // limit "in" buffer to 10Mb
    };

    struct Websocket {
        friend class NetworkHandler;

        Websocket() {}
        ~Websocket();

        std::atomic<u8> status{WEBSOCKET_CONNECTING};
        std::vector<u8> in;
        std::vector<u8> out;

    private:
        CURL* handle{nullptr};

        // Servers can send fragmented packets, we want to only append them
        // to "in" once the packets are complete.
        std::vector<u8> in_partial;
        u64 maxRecvBytes{0};
    };

    // async request options
    struct RequestOptions {
        friend class NetworkHandler;

       private:
        struct MimePart {
            std::string filename{};
            std::string name{};
            std::vector<u8> data{};
        };
        bool isWebsocket{false};

       public:
        RequestOptions() { ; }  // ?
        std::map<std::string, std::string> headers;
        std::string postData;
        std::string userAgent;
        std::vector<MimePart> mimeParts;
        long timeout{5};
        long connectTimeout{5};
        bool followRedirects{false};
        std::function<void(float)> progressCallback;  // progress callback for downloads
    };

    // async response data
    struct Response {
        friend class NetworkHandler;

        std::string body;
        long responseCode{0};
        std::map<std::string, std::string> headers;
        bool success{false};

    private:
        // HACK for passing websocket handle
        CURL *easy_handle{nullptr};
    };

    using AsyncCallback = std::function<void(Response response)>;

    NetworkHandler();
    ~NetworkHandler();

    // synchronous API
    UString httpGet(const UString& url, long timeout = 5, long connectTimeout = 5);
    std::string httpDownload(const UString& url, long timeout = 60, long connectTimeout = 5);

    // asynchronous API
    void update();
    void httpRequestAsync(const UString& url, AsyncCallback callback, const RequestOptions& options = {});
    std::shared_ptr<Websocket> initWebsocket(const WebsocketOptions& options);

    // sync request for special cases like logout
    Response performSyncRequest(const UString& url, const RequestOptions& options);

   private:
    // curl_multi implementation
    CURLM* multi_handle;
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

    // websockets
    std::vector<std::shared_ptr<Websocket>> active_websockets;

    // sync request support
    Sync::mutex sync_requests_mutex;
    std::map<void*, Sync::condition_variable*> sync_request_cvs;
    std::map<void*, Response> sync_responses;

    void networkThreadFunc(const Sync::stop_token& stopToken);
    void processNewRequests();
    void processCompletedRequests();
    std::unique_ptr<NetworkRequest> createRequest(const UString& url, AsyncCallback callback,
                                                  const RequestOptions& options);
    void setupCurlHandle(CURL* handle, NetworkRequest* request);
    static size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userdata);
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static int progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t);
};

extern std::unique_ptr<NetworkHandler> networkHandler;

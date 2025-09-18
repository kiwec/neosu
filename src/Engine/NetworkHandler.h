#pragma once
// Copyright (c) 2015, PG, All rights reserved.
#include "UString.h"
#include "types.h"

#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

// forward declare for async requests
struct NetworkRequest;

// forward defs
typedef void CURLM;
typedef void CURL;

typedef int64_t curl_off_t;

class NetworkHandler {
    NOCOPY_NOMOVE(NetworkHandler)

   public:
    // async request options
    struct RequestOptions {
       private:
        struct MimePart {
            std::string filename{};
            std::string name{};
            std::vector<u8> data{};
        };

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
        std::string body;
        long responseCode{0};
        std::map<std::string, std::string> headers;
        bool success{false};
    };

    using AsyncCallback = std::function<void(Response response)>;

    NetworkHandler();
    ~NetworkHandler();

    // synchronous API
    UString httpGet(const UString& url, long timeout = 5, long connectTimeout = 5);
    std::string httpDownload(const UString& url, long timeout = 60, long connectTimeout = 5);

    // asynchronous API
    void httpRequestAsync(const UString& url, AsyncCallback callback, const RequestOptions& options = {});

    // sync request for special cases like logout
    Response performSyncRequest(const UString& url, const RequestOptions& options);

   private:
    // curl_multi implementation
    CURLM* multi_handle;
    std::unique_ptr<std::jthread> network_thread;

    // request queuing
    std::mutex request_queue_mutex;
    std::condition_variable_any request_queue_cv;
    std::queue<std::unique_ptr<NetworkRequest>> pending_requests;

    // active requests tracking
    std::mutex active_requests_mutex;
    std::map<CURL*, std::unique_ptr<NetworkRequest>> active_requests;

    // sync request support
    std::mutex sync_requests_mutex;
    std::map<void*, std::condition_variable*> sync_request_cvs;
    std::map<void*, Response> sync_responses;

    void networkThreadFunc(const std::stop_token& stopToken);
    void processNewRequests();
    void processCompletedRequests();
    std::unique_ptr<NetworkRequest> createRequest(const UString& url, AsyncCallback callback,
                                                  const RequestOptions& options);
    void setupCurlHandle(CURL* handle, NetworkRequest* request);
    static size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userdata);
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static int progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t);
};

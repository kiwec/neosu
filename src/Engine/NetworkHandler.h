#pragma once
// Copyright (c) 2015, PG, All rights reserved.
#include "noinclude.h"
#include "types.h"
#include "StaticPImpl.h"
#include "templates.h"

#include <string>
#include <string_view>
#include <functional>
#include <memory>
#include <atomic>
#include <vector>

// forward defs
typedef void CURL;
class Engine;

// generic networking things, not BANCHO::Net
namespace NeoNet {
// public defs
enum class WSStatus : u8 {
    CONNECTING,
    CONNECTED,
    DISCONNECTED,
    UNSUPPORTED,
};

struct WSOptions {
    std::string url;
    sv_unordered_map<std::string> headers;
    std::string user_agent;
    long timeout{5};
    long connect_timeout{5};
    u64 max_recv{10ULL * 1024 * 1024};  // limit "in" buffer to 10Mb
};

struct WSInstance {
   private:
    NOCOPY_NOMOVE(WSInstance)
    friend class NetworkHandler;

   public:
    WSInstance() = default;
    ~WSInstance();

    std::atomic<WSStatus> status{WSStatus::CONNECTING};
    std::vector<u8> in;
    std::vector<u8> out;
    f64 time_created{0.f};

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
    sv_unordered_map<std::string> headers;
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
    std::string error_msg;
    sv_unordered_map<std::string> headers;
    bool success{false};
};

using AsyncCallback = std::function<void(Response response)>;

class NetworkHandler {
    NOCOPY_NOMOVE(NetworkHandler)
   public:
    NetworkHandler();
    ~NetworkHandler();

    // synchronous requests
    Response httpRequestSynchronous(std::string_view url, RequestOptions options);

    // asynchronous API
    void httpRequestAsync(std::string_view url, AsyncCallback callback, RequestOptions options = {});

    // websockets
    std::shared_ptr<WSInstance> initWebsocket(const WSOptions& options);

   private:
    // callback update tick
    friend class ::Engine;
    void update();

    struct NetworkImpl;
    StaticPImpl<NetworkImpl, 640> pImpl;  // implementation details
};

}  // namespace Net

using NeoNet::NetworkHandler;
extern std::unique_ptr<NetworkHandler> networkHandler;

#pragma once

#include <httplib.h>
#include <string>

namespace meos::net {

// HTTP server wrapping cpp-httplib with CORS support.
// All responses include Access-Control-Allow-Origin: *.
// OPTIONS /api/* returns 204 with full CORS preflight headers.
class HttpServer {
public:
    explicit HttpServer(int port = 2009);

    // Returns the underlying server for route registration.
    httplib::Server& server() { return svr_; }

    // Mounts static_dir at "/" using set_mount_point (with gzip via
    // CPPHTTPLIB_ZLIB_SUPPORT) and registers a catch-all GET handler for the
    // SPA fallback (non-API paths without a matching file get index.html).
    // Call this AFTER registering all API routes so the catch-all is last.
    void serveStaticFiles(const std::string& static_dir = "web");

    // Starts listening (blocks until stop() is called).
    void listen();

    // Stops the server.
    void stop();

private:
    int port_;
    httplib::Server svr_;
};

}  // namespace meos::net

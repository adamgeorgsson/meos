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

    // Registers a catch-all GET handler that serves static files from
    // static_dir.  If the requested file does not exist (and the path does not
    // start with /api/), index.html is returned (SPA fallback).
    // Call this AFTER registering all API routes so the catch-all is last.
    void serveStaticFiles(const std::string& static_dir = "src/ui/web/dist");

    // Starts listening (blocks until stop() is called).
    void listen();

    // Stops the server.
    void stop();

private:
    int port_;
    httplib::Server svr_;
};

}  // namespace meos::net

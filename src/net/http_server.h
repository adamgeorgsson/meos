#pragma once

#include <httplib.h>

namespace meos::net {

// HTTP server wrapping cpp-httplib with CORS support.
// All responses include Access-Control-Allow-Origin: *.
// OPTIONS /api/* returns 204 with full CORS preflight headers.
class HttpServer {
public:
    explicit HttpServer(int port = 2009);

    // Returns the underlying server for route registration.
    httplib::Server& server() { return svr_; }

    // Starts listening (blocks until stop() is called).
    void listen();

    // Stops the server.
    void stop();

private:
    int port_;
    httplib::Server svr_;
};

}  // namespace meos::net

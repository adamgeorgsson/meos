#include "http_server.h"

namespace meos::net {

HttpServer::HttpServer(int port) : port_(port) {
    // CORS: add Access-Control-Allow-Origin to every response.
    // For OPTIONS on /api/*, return 204 with full preflight headers.
    svr_.set_pre_routing_handler(
        [](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", "*");
            if (req.method == "OPTIONS" &&
                req.path.rfind("/api/", 0) == 0) {
                res.set_header("Access-Control-Allow-Methods",
                               "GET, POST, PUT, DELETE, OPTIONS");
                res.set_header("Access-Control-Allow-Headers", "Content-Type");
                res.status = 204;
                return httplib::Server::HandlerResponse::Handled;
            }
            return httplib::Server::HandlerResponse::Unhandled;
        });
}

void HttpServer::listen() {
    svr_.listen("0.0.0.0", port_);
}

void HttpServer::stop() {
    svr_.stop();
}

}  // namespace meos::net

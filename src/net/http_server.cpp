#include "http_server.h"
#include "api_utils.h"

#include <fstream>
#include <iterator>

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

    // Error handler: only fires when no route matched. Returns a JSON 404
    // envelope that matches the API's makeError format, WITHOUT overwriting
    // any body that a matched handler already set (httplib never calls the
    // error handler after a successful handler dispatch).
    svr_.set_error_handler(
        [](const httplib::Request&, httplib::Response& res) {
            if (res.body.empty()) {
                res.set_content(
                    makeError(res.status, "Not found").dump(),
                    "application/json");
            }
        });
}

void HttpServer::listen() {
    svr_.listen("0.0.0.0", port_);
}

void HttpServer::stop() {
    svr_.stop();
}

void HttpServer::serveStaticFiles(const std::string& static_dir) {
    // Mount static files; cpp-httplib handles content types and gzip
    // (CPPHTTPLIB_ZLIB_SUPPORT) automatically.
    svr_.set_mount_point("/", static_dir);

    // SPA fallback: non-API paths that don't match a real file get index.html.
    // set_mount_point is checked before route handlers for GET, so this
    // catch-all only fires when the file does not exist in static_dir.
    svr_.Get(".*", [static_dir](const httplib::Request& req, httplib::Response& res) {
        if (req.path.rfind("/api/", 0) == 0) return;  // let API 404s stay JSON

        std::ifstream idx(static_dir + "/index.html", std::ios::binary);
        if (idx) {
            std::string content((std::istreambuf_iterator<char>(idx)),
                                std::istreambuf_iterator<char>());
            res.set_content(content, "text/html");
        } else {
            res.status = 404;
            res.set_content("Not found", "text/plain");
        }
    });
}

}  // namespace meos::net

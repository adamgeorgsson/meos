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
    svr_.Get(".*", [static_dir](const httplib::Request& req, httplib::Response& res) {
        // Determine MIME type from file extension (C++17 compatible).
        auto endsWith = [](const std::string& s, const std::string& suffix) -> bool {
            return s.size() >= suffix.size() &&
                   s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
        };
        auto mimeType = [&](const std::string& path) -> std::string {
            if (endsWith(path, ".html"))                         return "text/html";
            if (endsWith(path, ".js") || endsWith(path, ".mjs")) return "application/javascript";
            if (endsWith(path, ".css"))                          return "text/css";
            if (endsWith(path, ".png"))                          return "image/png";
            if (endsWith(path, ".jpg") || endsWith(path, ".jpeg")) return "image/jpeg";
            if (endsWith(path, ".svg"))                          return "image/svg+xml";
            if (endsWith(path, ".ico"))                          return "image/x-icon";
            if (endsWith(path, ".json"))                         return "application/json";
            if (endsWith(path, ".woff"))                         return "font/woff";
            if (endsWith(path, ".woff2"))                        return "font/woff2";
            return "application/octet-stream";
        };

        std::string req_path = req.path;
        if (req_path == "/") req_path = "/index.html";

        std::string file_path = static_dir + req_path;
        std::ifstream f(file_path, std::ios::binary);
        if (f) {
            std::string content((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
            res.set_content(content, mimeType(req_path));
            return;
        }

        // SPA fallback: any path not matching a real file gets index.html.
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

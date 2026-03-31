#pragma once
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <atomic>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <thread>
#include "ApiError.h"
#include "ApiRouter.h"
#include "JsonUtils.h"

namespace meos_net {

// HttpServer hosts the httplib server on a background thread.
// Routes are registered via router() before calling start().
class HttpServer {
public:
    HttpServer() {
        installErrorHandler("");
    }

    ~HttpServer() { stop(); }

    // Return the router for registering /api/v1/* routes.
    ApiRouter router() { return ApiRouter(_srv); }

    // Serve static files from webRoot directory.
    // Implements SPA fallback: non-API GET 404s serve index.html.
    // Must be called before start().
    void serveStatic(const std::string& webRoot) {
        _webRoot = webRoot;
        _srv.set_mount_point("/", webRoot);
        installErrorHandler(webRoot);
    }

    // Start listening on the given host:port in a background thread.
    // Throws std::runtime_error if the server fails to bind.
    void start(const std::string& host, int port) {
        if (_running.load()) return;
        if (!_srv.bind_to_port(host.c_str(), port))
            throw std::runtime_error("HttpServer: cannot bind to " + host + ":" + std::to_string(port));
        _running.store(true);
        _thread = std::thread([this] { _srv.listen_after_bind(); });
    }

    void stop() {
        if (_running.exchange(false)) {
            _srv.stop();
            if (_thread.joinable()) _thread.join();
        }
    }

    bool isRunning() const noexcept { return _running.load(); }

    // Access the underlying httplib::Server for advanced configuration.
    httplib::Server& raw() { return _srv; }

private:
    // Install (or replace) the error handler.
    // When webRoot is non-empty, 404 GET requests that don't match /api/
    // are redirected to index.html for SPA client-side routing.
    void installErrorHandler(const std::string& webRoot) {
        _srv.set_error_handler([webRoot](const httplib::Request& req, httplib::Response& res) {
            // SPA fallback: serve index.html for unmatched non-API GET requests
            if (!webRoot.empty() &&
                res.status == 404 &&
                req.method == "GET" &&
                req.path.find("/api/") == std::string::npos)
            {
                std::ifstream f(webRoot + "/index.html", std::ios::binary);
                if (f) {
                    std::string content((std::istreambuf_iterator<char>(f)),
                                        std::istreambuf_iterator<char>());
                    res.set_content(content, "text/html");
                    res.status = 200;
                    return;
                }
            }
            if (!res.body.empty()) return;
            nlohmann::json body{{"error", {{"code", res.status},
                {"message", httplib::status_message(res.status)}}}};
            res.set_content(body.dump(), "application/json");
        });

        // Centralized exception handler
        _srv.set_exception_handler([](const httplib::Request& /*req*/, httplib::Response& res, std::exception_ptr ep) {
            try {
                if (ep) std::rethrow_exception(ep);
                nlohmann::json body{{"error", {{"code", 500}, {"message", "Unknown error"}}}};
                res.set_content(body.dump(), "application/json");
                res.status = 500;
            } catch (const ApiException& ex) {
                nlohmann::json body{{"error", {{"code", ex.httpStatus()}, {"message", ex.message()}}}};
                res.set_content(body.dump(), "application/json");
                res.status = ex.httpStatus();
            } catch (const std::exception& ex) {
                nlohmann::json body{{"error", {{"code", 500}, {"message", ex.what()}}}};
                res.set_content(body.dump(), "application/json");
                res.status = 500;
            }
        });
    }

    httplib::Server _srv;
    std::thread     _thread;
    std::atomic<bool> _running{false};
    std::string     _webRoot;
};

} // namespace meos_net

#pragma once
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <atomic>
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
        // Centralized error handler for 404 and unhandled paths.
        // Only override body when the route handler hasn't already set one.
        _srv.set_error_handler([](const httplib::Request& /*req*/, httplib::Response& res) {
            if (!res.body.empty()) return; // route handler already wrote a response
            nlohmann::json body{{"error", {{"code", res.status}, {"message", httplib::status_message(res.status)}}}};
            res.set_content(body.dump(), "application/json");
        });

        // Centralized exception handler — catches anything that escapes a raw handler
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

    ~HttpServer() { stop(); }

    // Return the router for registering /api/v1/* routes.
    ApiRouter router() { return ApiRouter(_srv); }

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
    httplib::Server _srv;
    std::thread     _thread;
    std::atomic<bool> _running{false};
};

} // namespace meos_net

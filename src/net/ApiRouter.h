#pragma once
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <functional>
#include <string>
#include "ApiError.h"
#include "JsonUtils.h"

namespace meos_net {

using json    = nlohmann::json;
using Request  = httplib::Request;
using Response = httplib::Response;

// Handler type: receives request + parsed JSON body (may be null), returns JSON to send.
using RouteHandler = std::function<json(const Request&)>;

// ApiRouter wraps an httplib::Server and registers routes under /api/v1/.
// All handlers automatically get JSON Content-Type, error wrapping, and
// exception handling.
class ApiRouter {
public:
    explicit ApiRouter(httplib::Server& server) : _srv(server) {}

    void GET(const std::string& path, RouteHandler handler) {
        _srv.Get(prefix(path), wrap(std::move(handler)));
    }
    void POST(const std::string& path, RouteHandler handler) {
        _srv.Post(prefix(path), wrap(std::move(handler)));
    }
    void PUT(const std::string& path, RouteHandler handler) {
        _srv.Put(prefix(path), wrap(std::move(handler)));
    }
    void DEL(const std::string& path, RouteHandler handler) {
        _srv.Delete(prefix(path), wrap(std::move(handler)));
    }

private:
    std::string prefix(const std::string& path) {
        return "/api/v1" + path;
    }

    httplib::Server::Handler wrap(RouteHandler handler) {
        return [h = std::move(handler)](const Request& req, Response& res) {
            res.set_header("Content-Type", "application/json");
            try {
                json result = h(req);
                res.set_content(result.dump(), "application/json");
                res.status = 200;
            } catch (const ApiException& ex) {
                res.set_content(errorResponse(ex).dump(), "application/json");
                res.status = ex.httpStatus();
            } catch (const std::exception& ex) {
                res.set_content(errorResponse(ex.what()).dump(), "application/json");
                res.status = 500;
            } catch (...) {
                res.set_content(errorResponse("Unknown internal error").dump(), "application/json");
                res.status = 500;
            }
        };
    }

    httplib::Server& _srv;
};

} // namespace meos_net

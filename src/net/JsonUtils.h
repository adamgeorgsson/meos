#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include "ApiError.h"

namespace meos_net {

using json = nlohmann::json;

// Build a successful JSON response envelope.
inline json okResponse(json data) {
    return json{{"data", std::move(data)}};
}

// Build an error response envelope from an ApiException.
inline json errorResponse(const ApiException& ex) {
    return json{
        {"error", {
            {"code",    static_cast<int>(ex.code())},
            {"message", ex.message()}
        }}
    };
}

// Build an error response envelope from a plain message (500).
inline json errorResponse(const std::string& msg) {
    return json{
        {"error", {
            {"code",    500},
            {"message", msg}
        }}
    };
}

// Parse a required string field from a JSON body; throws ApiException on failure.
inline std::string requireString(const json& body, const std::string& key) {
    auto it = body.find(key);
    if (it == body.end() || !it->is_string())
        throw badRequest("Missing or invalid field: " + key);
    return it->get<std::string>();
}

// Parse an optional int field; returns defaultVal if absent.
inline int optionalInt(const json& body, const std::string& key, int defaultVal = 0) {
    auto it = body.find(key);
    if (it == body.end()) return defaultVal;
    if (!it->is_number_integer()) throw badRequest("Field must be an integer: " + key);
    return it->get<int>();
}

// Parse the request body as JSON; throws ApiException if parsing fails.
inline json parseBody(const std::string& body) {
    try {
        return json::parse(body);
    } catch (const json::parse_error& e) {
        throw badRequest(std::string("Invalid JSON: ") + e.what());
    }
}

} // namespace meos_net

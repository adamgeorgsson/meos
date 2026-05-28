#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <stdexcept>
#include <string>

namespace meos::net {

// Returns {message, status} error JSON envelope.
inline nlohmann::json makeError(int status, const std::string& message) {
    return nlohmann::json{{"message", message}, {"status", status}};
}

// Parses a path parameter string as a positive integer.
// Returns nullopt if the string is not a valid positive integer.
inline std::optional<int> parsePathId(const std::string& s) {
    if (s.empty()) return std::nullopt;
    for (char c : s) {
        if (c < '0' || c > '9') return std::nullopt;
    }
    try {
        int v = std::stoi(s);
        if (v <= 0) return std::nullopt;
        return v;
    } catch (...) {
        return std::nullopt;
    }
}

// Validates that a JSON object contains a required string field.
// Throws std::invalid_argument with a descriptive message on failure.
inline const std::string& requireString(const nlohmann::json& j,
                                        const std::string& field) {
    if (!j.contains(field) || !j[field].is_string()) {
        throw std::invalid_argument("Missing required field: " + field);
    }
    return j[field].get_ref<const std::string&>();
}

// Validates that a JSON object contains a required int field.
// Throws std::invalid_argument with a descriptive message on failure.
inline int requireInt(const nlohmann::json& j, const std::string& field) {
    if (!j.contains(field) || !j[field].is_number_integer()) {
        throw std::invalid_argument("Missing required field: " + field);
    }
    return j[field].get<int>();
}

}  // namespace meos::net

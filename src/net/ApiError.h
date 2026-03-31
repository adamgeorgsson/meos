#pragma once
#include <stdexcept>
#include <string>

namespace meos_net {

enum class ApiErrorCode {
    BadRequest        = 400,
    NotFound          = 404,
    MethodNotAllowed  = 405,
    Conflict          = 409,
    UnprocessableEntity = 422,
    InternalError     = 500,
};

// Exception thrown from route handlers to produce a structured JSON error response.
class ApiException : public std::exception {
public:
    ApiException(ApiErrorCode code, std::string message)
        : _code(code), _message(std::move(message)) {}

    const char* what() const noexcept override { return _message.c_str(); }
    ApiErrorCode code() const noexcept { return _code; }
    const std::string& message() const noexcept { return _message; }
    int httpStatus() const noexcept { return static_cast<int>(_code); }

private:
    ApiErrorCode _code;
    std::string  _message;
};

// Convenience factory helpers
inline ApiException badRequest(const std::string& msg)       { return ApiException(ApiErrorCode::BadRequest, msg); }
inline ApiException notFound(const std::string& msg)         { return ApiException(ApiErrorCode::NotFound, msg); }
inline ApiException conflict(const std::string& msg)         { return ApiException(ApiErrorCode::Conflict, msg); }
inline ApiException unprocessable(const std::string& msg)    { return ApiException(ApiErrorCode::UnprocessableEntity, msg); }
inline ApiException internalError(const std::string& msg)    { return ApiException(ApiErrorCode::InternalError, msg); }

} // namespace meos_net

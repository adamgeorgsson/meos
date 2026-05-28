#pragma once

#include <stdexcept>
#include <string>
#include <cstring>

// Portable exception class matching legacy meosException interface.
// wwhat() returns the original wide-string message.
class meosException : public std::runtime_error {
  std::wstring wideMessage_;

public:
  explicit meosException(const std::wstring& wmsg)
    : std::runtime_error(std::string(wmsg.begin(), wmsg.end()))
    , wideMessage_(wmsg) {}

  explicit meosException(const std::string& msg)
    : std::runtime_error(msg.c_str())
    , wideMessage_(msg.begin(), msg.end()) {}

  explicit meosException(const char* msg)
    : std::runtime_error(msg)
    , wideMessage_(msg, msg + std::strlen(msg)) {}

  meosException() : std::runtime_error("") {}

  const std::wstring& wwhat() const { return wideMessage_; }
};

class meosCancel : public meosException {
public:
  using meosException::meosException;
  meosCancel() = default;
};

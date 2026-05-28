#pragma once

#include <string>

namespace meos::util {

// Convert CP-1252 encoded narrow string to wide string (Unicode).
void string2Wide(const std::string& in, std::wstring& out);

// Convert wide string to narrow string (ASCII/Latin-1 truncation — values > 0xFF become '?').
void wide2String(const std::wstring& in, std::string& out);

// Returns the wide (Unicode) version of a CP-1252 encoded narrow string.
// The returned reference points to a thread-local buffer — copy before the next call.
const std::wstring& widen(const std::string& input);

// Returns the narrow (Latin-1 truncated) version of a wide string.
// The returned reference points to a thread-local buffer — copy before the next call.
const std::string& narrow(const std::wstring& input);

// Encode a wide string as UTF-8.
// The returned reference points to a thread-local buffer — copy before the next call.
const std::string& toUTF8(const std::wstring& input);

// Decode a UTF-8 byte string to a wide string.
// The returned reference points to a thread-local buffer — copy before the next call.
const std::wstring& fromUTF8(const std::string& input);

}  // namespace meos::util

#pragma once

#include <string>

namespace meos::util {

std::string toUTF8(const std::wstring& ws);
std::wstring fromUTF8(const std::string& s);

}  // namespace meos::util

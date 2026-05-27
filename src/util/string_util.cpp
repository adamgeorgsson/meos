#include "string_util.h"

namespace meos::util {

std::string toUTF8(const std::wstring& ws) {
  std::string result;
  for (wchar_t wc : ws) {
    if (wc <= 0x7F) {
      result.push_back(static_cast<char>(wc));
    } else if (wc <= 0x7FF) {
      result.push_back(static_cast<char>(0xC0 | ((wc >> 6) & 0x1F)));
      result.push_back(static_cast<char>(0x80 | (wc & 0x3F)));
    } else if (wc <= 0xFFFF) {
      result.push_back(static_cast<char>(0xE0 | ((wc >> 12) & 0x0F)));
      result.push_back(static_cast<char>(0x80 | ((wc >> 6) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | (wc & 0x3F)));
    } else {
      result.push_back(static_cast<char>(0xF0 | ((wc >> 18) & 0x07)));
      result.push_back(static_cast<char>(0x80 | ((wc >> 12) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | ((wc >> 6) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | (wc & 0x3F)));
    }
  }
  return result;
}

std::wstring fromUTF8(const std::string& s) {
  std::wstring result;
  size_t i = 0;
  while (i < s.size()) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    wchar_t wc = 0;
    if (c <= 0x7F) {
      wc = c;
      i += 1;
    } else if ((c & 0xE0) == 0xC0) {
      wc = (c & 0x1F) << 6;
      wc |= (static_cast<unsigned char>(s[i + 1]) & 0x3F);
      i += 2;
    } else if ((c & 0xF0) == 0xE0) {
      wc = (c & 0x0F) << 12;
      wc |= (static_cast<unsigned char>(s[i + 1]) & 0x3F) << 6;
      wc |= (static_cast<unsigned char>(s[i + 2]) & 0x3F);
      i += 3;
    } else {
      wc = (c & 0x07) << 18;
      wc |= (static_cast<unsigned char>(s[i + 1]) & 0x3F) << 12;
      wc |= (static_cast<unsigned char>(s[i + 2]) & 0x3F) << 6;
      wc |= (static_cast<unsigned char>(s[i + 3]) & 0x3F);
      i += 4;
    }
    result.push_back(wc);
  }
  return result;
}

}  // namespace meos::util

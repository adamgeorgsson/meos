#pragma once

#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <list>
#include <memory>
#include <string>
#include <vector>

// Win32 type and function shims for cross-platform compilation
#ifndef _WIN32
using __int64 = int64_t;

inline int wcsncpy_s(wchar_t* dest, size_t destSize, const wchar_t* src, size_t count) {
  if (!dest || destSize == 0) return 22;
  size_t n = (count == static_cast<size_t>(-1)) ? destSize - 1 : std::min(count, destSize - 1);
  wcsncpy(dest, src, n);
  dest[n] = L'\0';
  return 0;
}

inline int _i64toa_s(int64_t val, char* buf, size_t bufSize, int radix) {
  if (!buf || bufSize == 0) return 22;
  if (radix == 10)
    snprintf(buf, bufSize, "%lld", static_cast<long long>(val));
  else if (radix == 16)
    snprintf(buf, bufSize, "%llx", static_cast<unsigned long long>(val));
  else
    snprintf(buf, bufSize, "%lld", static_cast<long long>(val));
  return 0;
}
#endif

// Common using declarations mirroring the legacy codebase's global namespace
using std::wstring;
using std::string;
using std::vector;
using std::list;
using std::shared_ptr;
using std::make_shared;
using std::pair;
using std::make_pair;

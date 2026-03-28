// domain_header.h — Common types, Win32 shims, and using-declarations for the domain layer.
// Mirrors what legacy StdAfx.h + Windows.h provided to all domain files.
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <list>
#include <set>
#include <map>
#include <utility>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <cassert>
#include <algorithm>
#include <stdexcept>
#include <functional>

// Bring std names into scope (legacy code uses bare string/vector/etc.)
using std::string;
using std::wstring;
using std::vector;
using std::list;
using std::set;
using std::map;
using std::shared_ptr;
using std::make_shared;
using std::pair;
using std::make_pair;
using std::min;
using std::max;
using std::move;

// ── Win32 type shims (non-Windows) ─────────────────────────────────────────
#ifndef _WIN32
  typedef uint8_t   BYTE;
  typedef uint8_t*  LPBYTE;
  typedef int64_t   __int64;

  #include <strings.h>  // for strcasecmp / strncasecmp
  inline int _stricmp(const char* a, const char* b) { return strcasecmp(a, b); }
  inline int _strnicmp(const char* a, const char* b, size_t n) { return strncasecmp(a, b, n); }

  // 3-arg strcpy_s
  inline void strcpy_s(char* dst, size_t dstsz, const char* src) noexcept {
    std::strncpy(dst, src, dstsz - 1);
    dst[dstsz - 1] = '\0';
  }
  // 2-arg strcpy_s (template picks up array size)
  template<size_t N>
  inline void strcpy_s(char (&dst)[N], const char* src) noexcept {
    std::strncpy(dst, src, N - 1);
    dst[N - 1] = '\0';
  }

  // wcsncpy_s
  inline void wcsncpy_s(wchar_t* dst, size_t dstsz, const wchar_t* src, size_t count) noexcept {
    size_t n = (count < dstsz - 1) ? count : dstsz - 1;
    std::wcsncpy(dst, src, n);
    dst[n] = L'\0';
  }

  // _wtof replacement
  inline double _wtof(const wchar_t* str) { return std::wcstod(str, nullptr); }

  // _i64toa_s replacement (no _TRUNCATE support needed)
  inline void _i64toa_s(int64_t val, char* buf, size_t bufsz, int /*radix*/) {
    snprintf(buf, bufsz, "%lld", static_cast<long long>(val));
  }
#endif

// ── SqlUpdated (extracted from oEvent.h to break circular dep) ─────────────
struct SqlUpdated {
  string updated;
  int counter = 0;
  bool changed = false;
  void reset() { updated.clear(); changed = false; counter = 0; }
};

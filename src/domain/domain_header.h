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

// -----------------------------------------------------------------------
// Portable time/value shims (replaces meos_util.h legacy functions)
// -----------------------------------------------------------------------

// Sentinel value meaning "no time set"
constexpr int NOTIME = -1;

// Convert int to wstring
inline wstring itow(int v) { return std::to_wstring(v); }

// Sub-second display mode for time formatting
enum class SubSecond { Off, Auto, Always };

// Format milliseconds as "M:SS" or "M:SS.t" (simplified cross-platform version)
inline wstring formatTimeMS(int ms, bool /*relative*/, SubSecond /*mode*/) {
  if (ms < 0) return L"-";
  int sec = ms / 1000;
  int min = sec / 60;
  sec %= 60;
  wchar_t buf[32];
  swprintf(buf, 32, L"%d:%02d", min, sec);
  return buf;
}

// Parse "M:SS" or plain seconds string to milliseconds
inline int convertAbsoluteTimeMS(const wstring& s) {
  if (s.empty()) return 0;
  int total = 0;
  size_t colon = s.find(L':');
  if (colon != wstring::npos) {
    int min = std::stoi(s.substr(0, colon));
    int sec = std::stoi(s.substr(colon + 1));
    total = (min * 60 + sec) * 1000;
  } else {
    try { total = std::stoi(s) * 1000; } catch (...) {}
  }
  return total;
}

// Return a dash placeholder (replaces legacy makeDash)
inline wstring makeDash(const wstring& s) { return s; }

// -----------------------------------------------------------------------
// Time constants (1/10-second units, matching legacy timeconstants.hpp)
// -----------------------------------------------------------------------
constexpr int timeUnitsPerSecond = 10;
constexpr int timeConstSecond    = timeUnitsPerSecond;          // 10
constexpr int timeConstMinute    = 60  * timeUnitsPerSecond;    // 600
constexpr int timeConstHour      = 3600 * timeUnitsPerSecond;   // 36000

// Format elapsed time in tenths-of-a-second units as "M:SS[.t]"
inline wstring formatTime(int rt, SubSecond mode = SubSecond::Auto) {
  if (rt <= 0) return makeDash(L"-");
  int tenths = rt % timeConstSecond;
  int sec    = rt / timeConstSecond;
  int min    = sec / 60;
  sec       %= 60;
  wchar_t buf[32];
  if (mode != SubSecond::Off && tenths > 0)
    swprintf(buf, 32, L"%d:%02d.%d", min, sec, tenths);
  else
    swprintf(buf, 32, L"%d:%02d", min, sec);
  return buf;
}

// Parse "H:MM:SS" or "MM:SS" into tenths-of-a-second units (stub: ignores daysZeroTime)
inline int convertAbsoluteTimeHMS(const wstring& m, int /*daysZeroTime*/) {
  if (m.empty()) return -1;
  int a = 0, b = 0, c = -1;
  if (swscanf(m.c_str(), L"%d:%d:%d", &a, &b, &c) >= 2) {
    int h  = (c >= 0) ? a : 0;
    int mn = (c >= 0) ? b : a;
    int s  = (c >= 0) ? c : b;
    return (h * 3600 + mn * 60 + s) * timeConstSecond;
  }
  return -1;
}

// Empty wstring sentinel (mirrors legacy _EmptyWString)
inline const wstring& _emptyWStringRef() { static const wstring s; return s; }
#define _EmptyWString _emptyWStringRef()

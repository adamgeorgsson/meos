#pragma once

#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Win32 type and function shims for cross-platform compilation
#ifndef _WIN32
#include <cwctype>  // towlower
using __int64 = int64_t;

inline int _wcsicmp(const wchar_t* a, const wchar_t* b) {
  while (*a && *b) {
    wchar_t la = static_cast<wchar_t>(towlower(static_cast<wint_t>(*a)));
    wchar_t lb = static_cast<wchar_t>(towlower(static_cast<wint_t>(*b)));
    if (la != lb) return la < lb ? -1 : 1;
    ++a; ++b;
  }
  wchar_t la = static_cast<wchar_t>(towlower(static_cast<wint_t>(*a)));
  wchar_t lb = static_cast<wchar_t>(towlower(static_cast<wint_t>(*b)));
  return la < lb ? -1 : (la > lb ? 1 : 0);
}

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
using std::map;
using std::shared_ptr;
using std::make_shared;
using std::pair;
using std::make_pair;

// -----------------------------------------------------------------------
// Portable time/value shims (replaces meos_util.h legacy functions)
// -----------------------------------------------------------------------

// Sentinel value meaning "no time set"
constexpr int NOTIME = -1;

// Sentinel value for an uninitialized split time slot (evaluateCard internal use)
constexpr int NOTATIME = 0x70000000;

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

// Convert wide-char string to int (mirrors meos_util::wtoi)
inline int wtoi(const wchar_t* s) {
  if (!s || !*s) return 0;
  return (int)std::wcstol(s, nullptr, 10);
}

// 2-arg formatTimeMS overload (relative=false means absolute display)
inline wstring formatTimeMS(int ms, bool relative) {
  return formatTimeMS(ms, relative, SubSecond::Off);
}

// Format an integer date (YYYYMMDD) as "YYYY-MM-DD"
inline wstring formatDate(int m, bool /*useIsoFormat*/) {
  wchar_t bf[24];
  if (m > 0 && m < 30000101)
    swprintf(bf, 24, L"%d-%02d-%02d", m / (100 * 100), (m / 100) % 100, m % 100);
  else
    bf[0] = L'-', bf[1] = 0;
  return bf;
}

// Convert int to narrow string (replaces legacy itos from meos_util)
inline string itos(int v) { return std::to_string(v); }

// Portable exception class matching legacy meosException interface
#include <stdexcept>
class meosException : public std::runtime_error {
  std::wstring wideMessage;
public:
  meosException(const std::wstring& wmsg)
    : std::runtime_error(std::string(wmsg.begin(), wmsg.end())),
      wideMessage(wmsg) {}
  meosException(const std::string& msg)  : std::runtime_error(msg.c_str()) {}
  meosException(const char* msg)         : std::runtime_error(msg) {}
  meosException()                        : std::runtime_error("") {}
  const std::wstring& wwhat() const { return wideMessage; }
};

// Compare two bib strings: shorter wins; equal length uses polynomial hash ordering
inline bool compareBib(const wstring& b1, const wstring& b2) {
  int l1 = (int)b1.length();
  int l2 = (int)b2.length();
  if (l1 != l2) return l1 < l2;
  if (l1 == 0) return false;
  wchar_t maxc = 0, minc = std::numeric_limits<wchar_t>::max();
  for (int k = 0; k < l1; k++) { maxc = std::max(maxc, b1[k]); minc = std::min(minc, b1[k]); }
  for (int k = 0; k < l2; k++) { maxc = std::max(maxc, b2[k]); minc = std::min(minc, b2[k]); }
  unsigned coeff = (unsigned)(maxc - minc) + 1;
  unsigned z1 = 0;
  for (int k = 0; k < l1; k++) z1 = coeff * z1 + (unsigned)(b1[k] - minc);
  unsigned z2 = 0;
  for (int k = 0; k < l2; k++) z2 = coeff * z2 + (unsigned)(b2[k] - minc);
  return z1 < z2;
}

// Split a wstring by a wide delimiter string into a vector
inline void splitW(const wstring& str, const wstring& delim, vector<wstring>& out) {
  out.clear();
  if (str.empty()) return;
  size_t start = 0;
  size_t dlen  = delim.size();
  while (true) {
    size_t pos = str.find(delim, start);
    out.push_back(str.substr(start, pos == wstring::npos ? wstring::npos : pos - start));
    if (pos == wstring::npos) break;
    start = pos + dlen;
  }
}

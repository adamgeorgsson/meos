/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2026 Melin Software HB

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Melin Software HB - software@melin.nu - www.melin.nu
    Eksoppsvägen 16, SE-75646 UPPSALA, Sweden

************************************************************************/

// Cross-platform implementation of meos_util.h
// Migrated from code/meos_util.cpp (Win32) and code/meosversion.cpp.
// All Win32 APIs replaced with std::filesystem, POSIX, and manual codecs.

#include "meos_util.h"
#include "meosexception.h"
#include "timeconstants.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <cwctype>
#include <filesystem>
#include <limits>
#include <numeric>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

using std::string;
using std::wstring;
using std::vector;

// ---- Platform-independent color macros -------------------------------------
#ifndef _WIN32
  #define HLSMAX  252
  #define RGBMAX  255
  #define UNDEFINED (HLSMAX*2/3)
  static inline uint32_t RGB_make(uint32_t r, uint32_t g, uint32_t b) {
    return r | (g << 8) | (b << 16);
  }
  #define RGB(r,g,b) RGB_make(r,g,b)
  static inline uint8_t GetRValue(uint32_t c) { return c & 0xFF; }
  static inline uint8_t GetGValue(uint32_t c) { return (c >> 8) & 0xFF; }
  static inline uint8_t GetBValue(uint32_t c) { return (c >> 16) & 0xFF; }
#else
  #define HLSMAX  252
  #define RGBMAX  255
  #define UNDEFINED (HLSMAX*2/3)
#endif

// ---- Codec helpers (UTF-8 <-> wstring, CP-1252 -> wstring) ------------------
namespace {

// Append a Unicode codepoint encoded as UTF-8 to 'out'.
void append_utf8(string &out, uint32_t cp) {
  if (cp < 0x80) {
    out.push_back((char)cp);
  } else if (cp < 0x800) {
    out.push_back((char)(0xC0 | (cp >> 6)));
    out.push_back((char)(0x80 | (cp & 0x3F)));
  } else if (cp < 0x10000) {
    out.push_back((char)(0xE0 | (cp >> 12)));
    out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back((char)(0x80 | (cp & 0x3F)));
  } else {
    out.push_back((char)(0xF0 | (cp >> 18)));
    out.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
    out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back((char)(0x80 | (cp & 0x3F)));
  }
}

// Decode a well-formed UTF-8 byte sequence to wstring.
// On Linux wchar_t = 4 bytes (UTF-32); on Windows = 2 bytes (UTF-16).
wstring utf8_to_wstring(const string &s) {
  wstring result;
  result.reserve(s.size());
  size_t i = 0;
  while (i < s.size()) {
    uint32_t cp;
    uint8_t c = (uint8_t)s[i];
    if (c < 0x80) {
      cp = c; i++;
    } else if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
      cp = (uint32_t)(c & 0x1F) << 6;
      cp |= (uint8_t)s[i+1] & 0x3F;
      i += 2;
    } else if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
      cp = (uint32_t)(c & 0x0F) << 12;
      cp |= ((uint8_t)s[i+1] & 0x3F) << 6;
      cp |= (uint8_t)s[i+2] & 0x3F;
      i += 3;
    } else if ((c & 0xF8) == 0xF0 && i + 3 < s.size()) {
      cp = (uint32_t)(c & 0x07) << 18;
      cp |= ((uint8_t)s[i+1] & 0x3F) << 12;
      cp |= ((uint8_t)s[i+2] & 0x3F) << 6;
      cp |= (uint8_t)s[i+3] & 0x3F;
      i += 4;
    } else {
      cp = 0xFFFD; // replacement character
      i++;
    }
#ifdef _WIN32
    if (cp < 0x10000) {
      result.push_back((wchar_t)cp);
    } else {
      // UTF-16 surrogate pair
      cp -= 0x10000;
      result.push_back((wchar_t)(0xD800 | (cp >> 10)));
      result.push_back((wchar_t)(0xDC00 | (cp & 0x3FF)));
    }
#else
    result.push_back((wchar_t)cp); // UTF-32
#endif
  }
  return result;
}

// Encode a wstring to UTF-8.
string wstring_to_utf8(const wstring &ws) {
  string result;
  result.reserve(ws.size() * 3);
#ifdef _WIN32
  for (size_t i = 0; i < ws.size(); i++) {
    uint32_t cp = (uint16_t)ws[i];
    if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < ws.size()) {
      uint32_t lo = (uint16_t)ws[i+1];
      if (lo >= 0xDC00 && lo <= 0xDFFF) {
        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
        i++;
      }
    }
    append_utf8(result, cp);
  }
#else
  for (wchar_t w : ws) append_utf8(result, (uint32_t)w);
#endif
  return result;
}

// CP-1252 extra mapping for bytes 0x80-0x9F (non-standard range).
static const uint16_t cp1252_upper[32] = {
  0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
  0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
  0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
  0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178
};

uint32_t cp1252_to_cp(uint8_t c) {
  if (c < 0x80) return c;
  if (c < 0xA0) return cp1252_upper[c - 0x80];
  return c; // 0xA0-0xFF map identically to Latin-1 / Unicode
}

} // anonymous namespace

// ---- Global constants -------------------------------------------------------
const wstring _EmptyWString;
const string  _EmptyString;
const string  _VacantName = "Vacant";

int defaultCodePage =
#ifdef _WIN32
  1252;
#else
  65001; // UTF-8
#endif

namespace MeOSUtil {
  int useHourFormat = 1;
}

// ---- StringCache ------------------------------------------------------------
StringCache &StringCache::getInstance() {
  static thread_local StringCache tl_cache;
  if (tl_cache.cache.empty()) {
    tl_cache.init();
  }
  return tl_cache;
}

// ---- Time helpers -----------------------------------------------------------
static bool myIsSpace(wchar_t b) {
  return iswspace(b) != 0 || b == 0x00A0 || b == 0x2007 || b == 0x202F;
}

string convertSystemTimeN(const std::tm &st) {
  char bf[64];
  snprintf(bf, sizeof(bf), "%d-%02d-%02d %02d:%02d:%02d",
           st.tm_year + 1900, st.tm_mon + 1, st.tm_mday,
           st.tm_hour, st.tm_min, st.tm_sec);
  return bf;
}

static string convertSystemTimeOnlyN(const std::tm &st) {
  char bf[32];
  snprintf(bf, sizeof(bf), "%02d:%02d:%02d", st.tm_hour, st.tm_min, st.tm_sec);
  return bf;
}

static string convertSystemDateN(const std::tm &st) {
  char bf[32];
  snprintf(bf, sizeof(bf), "%d-%02d-%02d", st.tm_year + 1900, st.tm_mon + 1, st.tm_mday);
  return bf;
}

wstring convertSystemTime(const std::tm &st) {
  wchar_t bf[64];
  swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%d-%02d-%02d %02d:%02d:%02d",
           st.tm_year + 1900, st.tm_mon + 1, st.tm_mday,
           st.tm_hour, st.tm_min, st.tm_sec);
  return bf;
}

wstring convertSystemTimeOnly(const std::tm &st) {
  wchar_t bf[32];
  swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%02d:%02d:%02d",
           st.tm_hour, st.tm_min, st.tm_sec);
  return bf;
}

wstring convertSystemDate(const std::tm &st) {
  wchar_t bf[32];
  swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%d-%02d-%02d",
           st.tm_year + 1900, st.tm_mon + 1, st.tm_mday);
  return bf;
}

string getLocalTimeN() {
  std::tm st = {};
  meos_localtime_now(&st);
  return convertSystemTimeN(st);
}

wstring getLocalTime() {
  std::tm st = {};
  meos_localtime_now(&st);
  return convertSystemTime(st);
}

wstring getLocalDate() {
  std::tm st = {};
  meos_localtime_now(&st);
  return convertSystemDate(st);
}

wstring getLocalTimeOnly() {
  std::tm st = {};
  meos_localtime_now(&st);
  return convertSystemTimeOnly(st);
}

int getLocalAbsTime() {
  return convertAbsoluteTimeHMS(getLocalTimeOnly(), -1);
}

wstring getLocalTimeFileName() {
  std::tm st = {};
  meos_localtime_now(&st);
  wchar_t bf[32];
  swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%d%02d%02d_%02d%02d%02d",
           st.tm_year + 1900, st.tm_mon + 1, st.tm_mday,
           st.tm_hour, st.tm_min, st.tm_sec);
  return bf;
}

int getRelativeDay() {
  std::tm local_st = {};
  meos_localtime_now(&local_st);
  std::tm st_copy = local_st;
#ifdef _WIN32
  std::time_t encoded = _mkgmtime(&st_copy);
#else
  std::time_t encoded = timegm(&st_copy);
#endif
  constexpr uint64_t WIN_UNIX_EPOCH_OFFSET = 11644473600ULL;
  uint64_t sec_since_1601 = (uint64_t)encoded + WIN_UNIX_EPOCH_OFFSET;
  uint64_t days_since_1601 = sec_since_1601 / 86400ULL;
  return int(days_since_1601) - 400*365;
}

int getThisYear() {
  static int thisYear = 0;
  if (thisYear == 0) {
    std::tm st = {};
    meos_localtime_now(&st);
    thisYear = st.tm_year + 1900;
  }
  return thisYear;
}

int64_t SystemTimeToInt64TenthSecond(const std::tm &st) {
  std::tm st_copy = st;
#ifdef _WIN32
  std::time_t tt = _mkgmtime(&st_copy);
#else
  std::time_t tt = timegm(&st_copy);
#endif
  constexpr int64_t WIN_UNIX_EPOCH_OFFSET = 11644473600LL;
  int64_t sec_since_1601 = (int64_t)tt + WIN_UNIX_EPOCH_OFFSET;
  return sec_since_1601 * (int64_t)timeUnitsPerSecond;
}

std::tm Int64TenthSecondToSystemTime(int64_t time) {
  constexpr int64_t WIN_UNIX_EPOCH_OFFSET = 11644473600LL;
  int64_t sec_since_1601 = time / (int64_t)timeUnitsPerSecond;
  std::time_t tt = (std::time_t)(sec_since_1601 - WIN_UNIX_EPOCH_OFFSET);
  std::tm st = {};
#ifdef _WIN32
  gmtime_s(&st, &tt);
#else
  gmtime_r(&tt, &st);
#endif
  return st;
}

// ---- Date parsing -----------------------------------------------------------
int extendYear(int year) {
  if (year < 0) return year;
  if (year >= 100) return year;
  int thisYear = getThisYear();
  int cLast = thisYear % 100;
  if (cLast == 0 && year == 0) return thisYear;
  if (year > thisYear % 100)
    return (thisYear - cLast) - 100 + year;
  else
    return (thisYear - cLast) + year;
}

int convertDateYMD(const string &m, std::tm &st, bool checkValid) {
  st = {};
  if (m.empty()) return -1;

  int len = (int)m.length();
  int dashCount = 0;
  for (int k = 0; k < len; k++) {
    uint8_t b = (uint8_t)m[k];
    if (b == 'T') break;
    if (!(b == '-' || b == ' ' || (b >= '0' && b <= '9'))) return -1;
    if (b == '-') dashCount++;
  }

  int year = atoi(m.c_str());

  if (dashCount == 0) {
    int day = year % 100;
    year /= 100;
    int month = year % 100;
    year /= 100;
    if ((year > 0 && year < 100) || (year == 0 && m.size() > 2 && m[0] == '0' && m[1] == '0'))
      year = extendYear(year);
    if (year < 1900 || year > 3000) return -1;
    if (month < 1 || month > 12) { if (checkValid) return -1; month = 1; }
    if (day < 1 || day > 31)     { if (checkValid) return -1; day = 1; }
    st.tm_year = year - 1900; st.tm_mon = month - 1; st.tm_mday = day;
    int t = year * 100 * 100 + month * 100 + day;
    return t < 0 ? -1 : t;
  }

  if ((year > 0 && year < 100) || (year == 0 && m.size() > 2 && m[0] == '0' && m[1] == '0'))
    year = extendYear(year);
  if (year < 1900 || year > 3000) return -1;

  int month = 0, day = 0;
  size_t kp = m.find_first_of('-');
  if (kp != string::npos) {
    string mtext = m.substr(kp + 1);
    month = atoi(mtext.c_str());
    if (month < 1 || month > 12) { if (checkValid) return -1; month = 1; }
    kp = mtext.find_last_of('-');
    if (kp != string::npos) {
      day = atoi(mtext.substr(kp + 1).c_str());
      if (day < 1 || day > 31) { if (checkValid) return -1; day = 1; }
    }
  }
  st.tm_year = year - 1900; st.tm_mon = month - 1; st.tm_mday = day;
  int t = year * 100 * 100 + month * 100 + day;
  return t < 0 ? -1 : t;
}

int convertDateYMD(const string &m, bool checkValid) {
  std::tm st = {};
  return convertDateYMD(m, st, checkValid);
}

int convertDateYMD(const wstring &m, std::tm &st, bool checkValid) {
  string ms(m.begin(), m.end());
  return convertDateYMD(ms, st, checkValid);
}

int convertDateYMD(const wstring &m, bool checkValid) {
  std::tm st = {};
  return convertDateYMD(m, st, checkValid);
}

bool checkValidDate(const wstring &date) {
  std::tm st = {};
  if (convertDateYMD(date, st, false) <= 0) return false;
  st.tm_hour = 12;
  std::tm st_copy = st;
  std::time_t tt = mktime(&st_copy);
  return tt != (std::time_t)-1;
}

wstring formatDate(int m, bool /*useIsoFormat*/) {
  wchar_t bf[24];
  if (m > 0 && m < 30000101)
    swprintf(bf, 24, L"%d-%02d-%02d", m/(100*100), (m/100)%100, m%100);
  else { bf[0] = '-'; bf[1] = 0; }
  return bf;
}

wstring addOrSubtractDays(const wstring &m, int days) {
  std::tm st = {};
  convertDateYMD(m, st, false);
  std::tm st_copy = st;
#ifdef _WIN32
  std::time_t tt = _mkgmtime(&st_copy);
#else
  std::time_t tt = timegm(&st_copy);
#endif
  tt += (std::time_t)days * 86400;
  std::tm new_st = {};
#ifdef _WIN32
  gmtime_s(&new_st, &tt);
#else
  gmtime_r(&tt, &new_st);
#endif
  return convertSystemDate(new_st);
}

void processGeneralTime(const wstring &generalTime, wstring &meosTime, wstring &meosDate) {
  meosTime = L""; meosDate = L"";
  vector<wstring> parts;
  split(generalTime, L":-,. /\t", parts);

  int year = -2, month = -2, day = -2;
  int hour = -2, minute = -2, second = -2, subsecond = -2;
  int found = 0, base = -1, iter = 0;
  bool pm = wcsstr(generalTime.c_str(), L"PM") != nullptr ||
            wcsstr(generalTime.c_str(), L"pm") != nullptr;

  while (iter < 2 && second == -2) {
    if (base == found) iter++;
    base = found;
    for (size_t k = 0; k < parts.size(); k++) {
      if (parts[k].empty()) continue;
      int number = wtoi(parts[k].c_str());
      if (number == 0 && parts[k][0] != '0') number = -1;

      if (iter == 0) {
        if (number > 1900 && number < 3000 && year < 0) { found++; year = (int)k; }
        else if (number >= 1 && number <= 12 && month < 0 && (year == (int)k+1 || year == (int)k-1)) { month = (int)k; found++; }
        else if (number >= 1 && number <= 31 && day < 0 && (month == (int)k+1 || month == (int)k-1)) { day = (int)k; found++; iter++; break; }
        else if (number > 1011900 && number < 30000101 && year < 0 && month < 0 && day < 0) { day = month = year = (int)k; found++; break; }
      } else if (iter == 1) {
        if (number >= 0 && number <= 24 && year != (int)k && day != (int)k && month != (int)k && hour < 0) { hour = (int)k; found++; }
        else if (number >= 0 && number <= 59 && minute < 0 && hour == (int)k-1) { minute = (int)k; found++; }
        else if (number >= 0 && number <= 59 && second < 0 && minute == (int)k-1) { second = (int)k; found++; }
        else if (number >= 0 && number < 1000 && subsecond < 0 && second == (int)k-1 && (int)k != year) { subsecond = (int)k; found++; iter++; break; }
      }
    }
  }

  if (second >= 0 && minute >= 0 && hour >= 0) {
    if (!pm)
      meosTime = parts[hour] + L":" + parts[minute] + L":" + parts[second];
    else {
      int rawHour = wtoi(parts[hour].c_str());
      if (rawHour < 12) rawHour += 12;
      meosTime = itow(rawHour) + L":" + parts[minute] + L":" + parts[second];
    }
  }
  if (year >= 0 && month >= 0 && day >= 0) {
    int y = -1, mo = -1, d = -1;
    if (year != month) {
      y = wtoi(parts[year].c_str()); mo = wtoi(parts[month].c_str()); d = wtoi(parts[day].c_str());
    } else {
      int td = wtoi(parts[year].c_str());
      int y1 = td / 10000, m1 = (td / 100) % 100, d1 = td % 100;
      bool ok = y1 > 2000 && y1 < 3000 && m1 >= 1 && m1 <= 12 && d1 >= 1 && d1 <= 31;
      if (!ok) { y1 = td % 10000; m1 = (td / 10000) % 100; d1 = td / 1000000; ok = y1 > 2000 && y1 < 3000 && m1 >= 1 && m1 <= 12 && d1 >= 1 && d1 <= 31; }
      meosDate = itow(y1) + L"-" + itow(m1) + L"-" + itow(d1);
      y = y1; mo = m1; d = d1;
    }
    if (y > 0) {
      wchar_t bf[24];
      swprintf(bf, 24, L"%d-%02d-%02d", y, mo, d);
      meosDate = bf;
    }
  }
}

// ---- Time parsing -----------------------------------------------------------
int convertAbsoluteTimeHMS(const string &m, int daysZeroTime) {
  int len = (int)m.length();
  if (len == 0 || m[0] == '-') return -1;

  int tix = -1;
  for (size_t k = 0; k < m.length(); k++) {
    int c = m[k];
    if (c == 'D' || c == 'd' || c == 'T' || c == 't') { tix = (int)k; break; }
  }
  if (tix != -1) {
    if (daysZeroTime < 0) return -1;
    int tpart = convertAbsoluteTimeHMS(m.substr(tix+1), -1);
    if (tpart != -1) {
      int days = atoi(m.c_str());
      if (days <= 0) return -1;
      if (tpart < daysZeroTime) days--;
      return days * timeConstHour * 24 + tpart;
    }
    return -1;
  }

  int plusIndex = -1;
  for (int k = 0; k < len; k++) {
    uint8_t b = (uint8_t)m[k];
    if (!(isspace(b) || b == ':' || (b >= '0' && b <= '9') || b == '.' || b == ',')) {
      if (b == '+' && plusIndex == -1 && k > 0) plusIndex = k;
      else return -1;
    }
  }
  if (plusIndex > 0) {
    int t = convertAbsoluteTimeHMS(m.substr(plusIndex+1), -1);
    int d = atoi(m.c_str());
    if (d > 0 && t >= 0) return d * 24 * timeConstHour + t;
    return -1;
  }

  int hour = atoi(m.c_str());
  if (hour < 0 || hour > 23) return -1;
  int minute = 0, second = 0, tenth = 0;
  size_t kp = m.find_first_of(':');
  if (kp != string::npos) {
    string mtext = m.substr(kp+1);
    minute = atoi(mtext.c_str());
    if (minute < 0 || minute > 60) minute = 0;
    kp = mtext.find_last_of(':');
    if (kp != string::npos) {
      second = atoi(mtext.c_str() + kp+1);
      if (second < 0 || second > 60) second = 0;
      if (timeConstSecond > 1) {
        size_t dp = mtext.find_last_of('.');
        if (dp == string::npos) dp = mtext.find_last_of(',');
        if (dp != string::npos) {
          tenth = atoi(mtext.c_str() + dp + 1);
          if (tenth < 0 || tenth >= 10) tenth = 0;
        }
      }
    }
  }
  int t = hour * timeConstHour + minute * timeConstMinute + second * timeConstSecond + tenth;
  return t < 0 ? 0 : t;
}

int convertAbsoluteTimeHMS(const wstring &m, int daysZeroTime) {
  string sm(m.begin(), m.end());
  return convertAbsoluteTimeHMS(sm, daysZeroTime);
}

static int convertAbsoluteTimeISO_str(const string &m) {
  if (m.empty() || m[0] == '-') return -1;
  string tmp = trim(m);
  if (tmp.length() < 3) return -1;

  string hStr = tmp.substr(0, 2);
  tmp = (!(tmp[2] >= '0' && tmp[2] <= '9')) ? tmp.substr(3) : tmp.substr(2);
  if (tmp.length() < 3) return -1;

  string mStr = tmp.substr(0, 2);
  tmp = (!(tmp[2] >= '0' && tmp[2] <= '9')) ? tmp.substr(3) : tmp.substr(2);
  if (tmp.length() < 2) return -1;

  string sStr = tmp.substr(0, 2);
  for (int i = 0; i < 2; i++) {
    if (hStr[i] < '0' || hStr[i] > '9') return -1;
    if (mStr[i] < '0' || mStr[i] > '9') return -1;
    if (sStr[i] < '0' || sStr[i] > '9') return -1;
  }
  int hour = atoi(hStr.c_str());   if (hour < 0 || hour > 23) return -1;
  int minute = atoi(mStr.c_str()); if (minute < 0 || minute > 60) return -1;
  int second = atoi(sStr.c_str()); if (second < 0 || second > 60) return -1;
  return hour * timeConstHour + minute * timeConstMinute + second;
}

int convertAbsoluteTimeISO(const wstring &m) {
  string mn(m.begin(), m.end());
  return convertAbsoluteTimeISO_str(mn);
}

int convertAbsoluteTimeMS(const string &m) {
  if (m.empty()) return NOTIME;
  int minute = 0, second = 0, sign = 1;
  size_t signpos = m.find_first_of('-');
  string mtext;
  if (signpos != string::npos) { sign = -1; mtext = m.substr(signpos+1); }
  else mtext = m;

  minute = atoi(mtext.c_str());
  int hour = 0;
  if (minute < 0 || minute > 60*24) minute = 0;

  size_t kp = mtext.find_first_of(':');
  bool gotSecond = false;
  if (kp != string::npos) {
    mtext = mtext.substr(kp+1);
    second = atoi(mtext.c_str());
    gotSecond = true;
    if (second < 0 || second > 60) second = 0;
  }
  kp = mtext.find_first_of(':');
  if (kp != string::npos) {
    hour = minute; minute = second;
    mtext = mtext.substr(kp+1);
    second = atoi(mtext.c_str());
    if (second < 0 || second > 60) second = 0;
  }
  int tenth = 0;
  if (timeConstSecond > 1) {
    kp = mtext.find_first_of('.');
    if (kp == string::npos) kp = mtext.find_last_of(',');
    if (kp != string::npos) {
      tenth = atoi(mtext.c_str() + kp + 1);
      if (!gotSecond) { second = minute; minute = 0; }
      if (tenth < 0 || tenth >= 10) tenth = 0;
    }
  }
  int t = hour * timeConstHour + minute * timeConstMinute + second * timeConstSecond + tenth;
  return sign * t;
}

int convertAbsoluteTimeMS(const wstring &m) {
  string mn(m.begin(), m.end());
  return convertAbsoluteTimeMS(mn);
}

int parseRelativeTime(const char *data) {
  if (!data) return 0;
  int ret = atoi(data);
  if (timeConstSecond == 10) {
    for (int j = 0; data[j]; j++) {
      if (data[j] == '.') {
        int t = data[j+1] - '0';
        if (t > 0 && t < 10) return (ret < 0 || data[0] == '-') ? ret * timeConstSecond - t : ret * timeConstSecond + t;
        break;
      }
    }
  } else if (timeConstSecond == 100) {
    for (int j = 0; data[j]; j++) {
      if (data[j] == '.') {
        int t = (data[j+1] - '0') * 10;
        int t2 = data[j+2] - '0';
        if (t2 > 0 && t2 < 10) t += t2;
        return (ret < 0 || data[0] == '-') ? ret * timeConstSecond - t : ret * timeConstSecond + t;
      }
    }
  }
  if (ret == -1) return ret;
  return ret * timeConstSecond;
}

int parseRelativeTime(const wchar_t *data) {
  if (!data) return 0;
  int ret = wtoi(data);
  if (timeConstSecond == 10) {
    for (int j = 0; data[j]; j++) {
      if (data[j] == '.') {
        int t = data[j+1] - '0';
        if (t > 0 && t < 10) return (ret < 0 || data[0] == '-') ? ret * timeConstSecond - t : ret * timeConstSecond + t;
        break;
      }
    }
  } else if (timeConstSecond == 100) {
    for (int j = 0; data[j]; j++) {
      if (data[j] == '.') {
        int t = (data[j+1] - '0') * 10;
        int t2 = data[j+2] - '0';
        if (t2 > 0 && t2 < 10) t += t2;
        return (ret < 0 || data[0] == '-') ? ret * timeConstSecond - t : ret * timeConstSecond + t;
      }
    }
  }
  if (ret == -1) return ret;
  return ret * timeConstSecond;
}

const wstring &codeRelativeTimeW(int rt) {
  wchar_t bf[32] = {};
  int subSec = timeConstSecond == 1 ? 0 : rt % timeConstSecond;
  if (timeConstSecond == 1 || rt == -1) return itow(rt);
  else if (subSec == 0 && rt != -timeConstSecond) return itow(rt / timeConstSecond);
  else if (rt > 0) {
    if (timeConstSecond == 10) swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%d.%d", rt/timeConstSecond, rt%timeConstSecond);
    else                       swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%d.%02d", rt/timeConstSecond, rt%timeConstSecond);
  } else {
    rt = -rt;
    if (timeConstSecond == 10) swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"-%d.%d", rt/timeConstSecond, rt%timeConstSecond);
    else                       swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"-%d.%02d", rt/timeConstSecond, rt%timeConstSecond);
  }
  wstring &res = StringCache::getInstance().wget();
  res = bf;
  return res;
}

const string &codeRelativeTime(int rt) {
  char bf[32] = {};
  int subSec = timeConstSecond == 1 ? 0 : rt % timeConstSecond;
  if (timeConstSecond == 1 || rt == -1) return itos(rt);
  else if (subSec == 0 && rt != -timeConstSecond) return itos(rt / timeConstSecond);
  else if (rt > 0) {
    if (timeConstSecond == 10) snprintf(bf, sizeof(bf), "%d.%d", rt/timeConstSecond, rt%timeConstSecond);
    else                       snprintf(bf, sizeof(bf), "%d.%02d", rt/timeConstSecond, rt%timeConstSecond);
  } else {
    rt = -rt;
    if (timeConstSecond == 10) snprintf(bf, sizeof(bf), "-%d.%d", rt/timeConstSecond, rt%timeConstSecond);
    else                       snprintf(bf, sizeof(bf), "-%d.%02d", rt/timeConstSecond, rt%timeConstSecond);
  }
  string &res = StringCache::getInstance().get();
  res = bf;
  return res;
}

// ---- Time formatting --------------------------------------------------------
const wstring &formatTimeMS(int m, bool force2digit, SubSecond mode) {
  wchar_t bf[32];
  int am = std::abs(m);
  if (am < timeConstHour || !MeOSUtil::useHourFormat) {
    if (force2digit) {
      if (mode == SubSecond::Off || (mode == SubSecond::Auto && m % 10 == 0))
        swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"-%02d:%02d", am/timeConstMinute, (am/timeConstSecond)%60);
      else if (timeUnitsPerSecond == 10)
        swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"-%02d:%02d.%d", am/timeConstMinute, (am/timeConstSecond)%60, am%timeConstSecond);
      else
        swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"-%02d:%02d.%02d", am/timeConstMinute, (am/timeConstSecond)%60, am%timeConstSecond);
    } else {
      if (mode == SubSecond::Off || (mode == SubSecond::Auto && m % 10 == 0))
        swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"-%d:%02d", am/timeConstMinute, (am/timeConstSecond)%60);
      else if (timeUnitsPerSecond == 10)
        swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"-%d:%02d.%d", am/timeConstMinute, (am/timeConstSecond)%60, am%timeConstSecond);
      else
        swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"-%d:%02d.%02d", am/timeConstMinute, (am/timeConstSecond)%60, am%timeConstSecond);
    }
  } else if (am < timeConstHour * 48) {
    if (force2digit) {
      if (mode == SubSecond::Off || (mode == SubSecond::Auto && m % 10 == 0))
        swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"-%02d:%02d:%02d", am/timeConstHour, (am/timeConstMinute)%60, (am/timeConstSecond)%60);
      else if (timeUnitsPerSecond == 10)
        swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"-%02d:%02d:%02d.%d", am/timeConstHour, (am/timeConstMinute)%60, (am/timeConstSecond)%60, am%timeConstSecond);
      else
        swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"-%02d:%02d:%02d.%02d", am/timeConstHour, (am/timeConstMinute)%60, (am/timeConstSecond)%60, am%timeConstSecond);
    } else {
      if (mode == SubSecond::Off || (mode == SubSecond::Auto && m % 10 == 0))
        swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"-%d:%02d:%02d", am/timeConstHour, (am/timeConstMinute)%60, (am/timeConstSecond)%60);
      else if (timeUnitsPerSecond == 10)
        swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"-%d:%02d:%02d.%d", am/timeConstHour, (am/timeConstMinute)%60, (am/timeConstSecond)%60, am%timeConstSecond);
      else
        swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"-%d:%02d:%02d.%02d", am/timeConstHour, (am/timeConstMinute)%60, (am/timeConstSecond)%60, am%timeConstSecond);
    }
  } else {
    m = 0;
    bf[0] = 0x2013; bf[1] = 0;
  }
  wstring &res = StringCache::getInstance().wget();
  res = (m < 0) ? bf : bf + 1;
  return res;
}

const wstring &formatTime(int rt, SubSecond mode) {
  wstring &res = StringCache::getInstance().wget();
  if (rt > 0 && rt < timeConstHour * 999) {
    wchar_t bf[40];
    if (mode == SubSecond::Off || (mode == SubSecond::Auto && rt % timeUnitsPerSecond == 0)) {
      if (rt >= timeConstHour && MeOSUtil::useHourFormat)
        swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%d:%02d:%02d", rt/timeConstHour, (rt/timeConstMinute)%60, (rt/timeConstSecond)%60);
      else
        swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%d:%02d", rt/timeConstMinute, (rt/timeConstSecond)%60);
    } else {
      if (timeUnitsPerSecond == 10) {
        if (rt >= timeConstHour && MeOSUtil::useHourFormat)
          swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%d:%02d:%02d.%d", rt/timeConstHour, (rt/timeConstMinute)%60, (rt/timeConstSecond)%60, rt%timeConstSecond);
        else
          swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%d:%02d.%d", rt/timeConstMinute, (rt/timeConstSecond)%60, rt%timeConstSecond);
      } else {
        if (rt >= timeConstHour && MeOSUtil::useHourFormat)
          swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%d:%02d:%02d.%02d", rt/timeConstHour, (rt/timeConstMinute)%60, (rt/timeConstSecond)%60, rt%timeConstSecond);
        else
          swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%d:%02d.%02d", rt/timeConstMinute, (rt/timeConstSecond)%60, rt%timeConstSecond);
      }
    }
    res = bf;
    return res;
  }
  wchar_t ret[2] = {0x2013, 0};
  res = ret;
  return res;
}

const wstring &formatTimeHMS(int rt, SubSecond mode) {
  wstring &res = StringCache::getInstance().wget();
  if (rt >= 0) {
    wchar_t bf[40];
    if (mode == SubSecond::Off || (mode == SubSecond::Auto && rt % timeUnitsPerSecond == 0))
      swprintf(bf, 16, L"%02d:%02d:%02d", rt/timeConstHour, (rt/timeConstMinute)%60, (rt/timeConstSecond)%60);
    else if (timeUnitsPerSecond == 10)
      swprintf(bf, 16, L"%02d:%02d:%02d.%d", rt/timeConstHour, (rt/timeConstMinute)%60, (rt/timeConstSecond)%60, rt%timeConstSecond);
    else
      swprintf(bf, 16, L"%02d:%02d:%02d.%02d", rt/timeConstHour, (rt/timeConstMinute)%60, (rt/timeConstSecond)%60, rt%timeConstSecond);
    res = bf;
    return res;
  }
  wchar_t ret[2] = {0x2013, 0};
  res = ret;
  return res;
}

wstring formatTimeIOF(int rt, int zeroTime) {
  if (rt > 0) {
    rt += zeroTime;
    wchar_t bf[16];
    swprintf(bf, 16, L"%02d:%02d:%02d", (rt/timeConstHour)%24, (rt/timeConstMinute)%60, (rt/timeConstSecond)%60);
    return bf;
  }
  return L"--:--:--";
}

// ---- Timezone ---------------------------------------------------------------
int getTimeZoneInfo(const wstring &date) {
  static wchar_t lastDate[16] = {0};
  static int lastValue = -1;
  if (lastValue != -1 && wstring(lastDate) == date) return lastValue;
  wcsncpy(lastDate, date.c_str(), 15);
  lastDate[15] = 0;

  std::tm st = {};
  convertDateYMD(date, st, false);
  st.tm_hour = 12;
  std::tm st_copy = st;
  std::time_t tt = mktime(&st_copy);
  if (tt == (std::time_t)-1) { lastValue = 0; return 0; }

  std::tm utc = {};
#ifdef _WIN32
  gmtime_s(&utc, &tt);
#else
  gmtime_r(&tt, &utc);
#endif
  int datecode    = (((st.tm_year+1900)*12 + (st.tm_mon+1))*31) + st.tm_mday;
  int datecodeUTC = (((utc.tm_year+1900)*12 + (utc.tm_mon+1))*31) + utc.tm_mday;
  int daydiff = (datecodeUTC > datecode) ? 1 : (datecodeUTC < datecode) ? -1 : 0;
  int t    = st.tm_hour * timeConstSecPerHour;
  int tUTC = daydiff * 24 * timeConstSecPerHour + utc.tm_hour * timeConstSecPerHour + utc.tm_min * timeConstSecPerMin + utc.tm_sec;
  lastValue = tUTC - t;
  return lastValue;
}

wstring getTimeZoneString(const wstring &date) {
  int a = getTimeZoneInfo(date);
  if (a == 0) return L"+00:00";
  wchar_t bf[12];
  if (a > 0) swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"-%02d:%02d", a/timeConstSecPerHour, (a/timeConstMinPerHour)%60);
  else       swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"+%02d:%02d", (-a)/timeConstSecPerHour, ((-a)/timeConstMinPerHour)%60);
  return bf;
}

// ---- Split/unsplit ----------------------------------------------------------
static size_t find_sep(const wstring &str, const wstring &sep, size_t start) {
  for (size_t m = start; m < str.length(); m++)
    for (size_t n = 0; n < sep.length(); n++)
      if (str[m] == sep[n]) return m;
  return str.npos;
}
static size_t find_sep(const string &str, const string &sep, size_t start) {
  for (size_t m = start; m < str.length(); m++)
    for (size_t n = 0; n < sep.length(); n++)
      if (str[m] == sep[n]) return m;
  return str.npos;
}

const vector<string> &split(const string &line, const string &sep, vector<string> &sv) {
  sv.clear();
  if (line.empty()) return sv;
  size_t s = 0, n = find_sep(line, sep, s);
  sv.push_back(line.substr(s, n - s));
  while (n != line.npos) { s = n+1; n = find_sep(line, sep, s); sv.push_back(line.substr(s, n-s)); }
  return sv;
}

const vector<wstring> &split(const wstring &line, const wstring &sep, vector<wstring> &sv) {
  sv.clear();
  if (line.empty()) return sv;
  size_t s = 0, n = find_sep(line, sep, s);
  sv.push_back(line.substr(s, n - s));
  while (n != line.npos) { s = n+1; n = find_sep(line, sep, s); sv.push_back(line.substr(s, n-s)); }
  return sv;
}

template<typename T>
const T &unsplit(const vector<T> &sv, const T &sep, T &line) {
  line.clear();
  for (size_t k = 0; k < sv.size(); k++) { if (k) line += sep; line += sv[k]; }
  return line;
}
template const string  &unsplit<string> (const vector<string>  &, const string  &, string  &);
template const wstring &unsplit<wstring>(const vector<wstring> &, const wstring &, wstring &);

// ---- String utilities -------------------------------------------------------
const wstring &limitText(const wstring &tIn, size_t numChar) {
  wstring &out = StringCache::getInstance().wget();
  out.clear();
  size_t L = tIn.length();
  int spacePos = -1, outP = 0;
  size_t i = 0;
  for (; i < L && (size_t)outP < numChar - 1; i++) {
    wchar_t c = tIn[i];
    if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    if (c == ' ' && spacePos == outP) continue;
    out.push_back(c); outP++;
    if (iswspace(c)) spacePos = outP;
  }
  if (i < L) {
    if (spacePos <= 2 || spacePos < (int)numChar - 10) spacePos = (int)numChar - 4;
    out = out.substr(0, spacePos - 1) + L"\u2026";
  }
  return out;
}

wstring ensureEndingColon(const wstring &text) {
  if (text.empty() || text.back() == ':') return text;
  return text + L":";
}

const wstring &makeDash(const wchar_t *t) {
  wstring &out = StringCache::getInstance().wget();
  out = t;
  for (size_t i = 0; i < out.length(); i++) if (t[i] == '-') out[i] = 0x2013;
  return out;
}

const wstring &makeDash(const wstring &t) { return makeDash(t.c_str()); }

wstring formatRank(int rank) {
  wchar_t r[16];
  swprintf(r, sizeof(r)/sizeof(wchar_t), L"(%04d)", rank);
  return r;
}

// ---- Integer <-> string conversion ------------------------------------------
const wstring &itow(int i) {
  wchar_t bf[32];
  swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%d", i);
  wstring &res = StringCache::getInstance().wget();
  res = bf;
  return res;
}
wstring itow(unsigned long i)  { wchar_t bf[32]; swprintf(bf,sizeof(bf)/sizeof(wchar_t),L"%lu",i); return bf; }
wstring itow(unsigned int i)   { wchar_t bf[32]; swprintf(bf,sizeof(bf)/sizeof(wchar_t),L"%u",i);  return bf; }
wstring itow(int64_t i)        { wchar_t bf[32]; swprintf(bf,sizeof(bf)/sizeof(wchar_t),L"%lld",(long long)i); return bf; }
// uint64_t == unsigned long on LP64 (Linux/Mac 64-bit); only define if distinct.
#if !defined(__LP64__) && !defined(_LP64)
wstring itow(uint64_t i)       { wchar_t bf[32]; swprintf(bf,sizeof(bf)/sizeof(wchar_t),L"%llu",(unsigned long long)i); return bf; }
#endif

const string &itos(int i) {
  char bf[32]; snprintf(bf, sizeof(bf), "%d", i);
  string &res = StringCache::getInstance().get();
  res = bf;
  return res;
}
string itos(unsigned int i)  { char bf[32]; snprintf(bf,sizeof(bf),"%u",i);  return bf; }
string itos(unsigned long i) { char bf[32]; snprintf(bf,sizeof(bf),"%lu",i); return bf; }
string itos(int64_t i)       { char bf[32]; snprintf(bf,sizeof(bf),"%lld",(long long)i); return bf; }
#if !defined(__LP64__) && !defined(_LP64)
string itos(uint64_t i)      { char bf[32]; snprintf(bf,sizeof(bf),"%llu",(unsigned long long)i); return bf; }
#endif

// ---- Character case / accent stripping -------------------------------------
int toLowerStripped(wchar_t c) {
  if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
  if (c < 128) return c;
  if ((uint32_t)c >= 65536) return (int)c;

  static wchar_t map[65536];
  static bool mapInit = false;
  if (!mapInit) {
    mapInit = true;
    for (int i = 0; i < 65536; i++) map[i] = (wchar_t)i;

    auto set = [](wchar_t pos, wchar_t val){ map[(uint16_t)pos] = val; };

    set(L'Å', L'a'); set(L'Ä', L'a'); set(L'Ö', L'o');
    set(L'É', L'e'); set(L'é', L'e'); set(L'è', L'e'); set(L'È', L'e');
    set(L'ë', L'e'); set(L'Ë', L'e'); set(L'ê', L'e'); set(L'Ê', L'e');
    set(L'û', L'u'); set(L'Û', L'u'); set(L'ü', L'u'); set(L'Ü', L'u');
    set(L'ú', L'u'); set(L'Ú', L'u'); set(L'ù', L'u'); set(L'Ù', L'u');
    set(L'ñ', L'n'); set(L'Ñ', L'n');
    set(L'ä', L'a'); set(L'å', L'a'); set(L'á', L'a'); set(L'Á', L'a');
    set(L'à', L'a'); set(L'À', L'a'); set(L'â', L'a'); set(L'Â', L'a');
    set(L'ã', L'a'); set(L'Ã', L'a');
    set(L'ï', L'i'); set(L'Ï', L'i'); set(L'î', L'i'); set(L'Î', L'i');
    set(L'í', L'i'); set(L'Í', L'i'); set(L'ì', L'i'); set(L'Ì', L'i');
    set(L'ó', L'o'); set(L'Ó', L'o'); set(L'ò', L'o'); set(L'Ò', L'o');
    set(L'õ', L'o'); set(L'Õ', L'o'); set(L'ô', L'o'); set(L'Ô', L'o');
    set(L'ö', L'o');
    set(L'ý', L'y'); set(L'Ý', L'Y'); set(L'ÿ', L'y');
    set(L'Æ', L'a'); set(L'æ', L'a');
    set(L'Ø', L'o'); set(L'ø', L'o');
    set(L'Ç', L'c'); set(L'ç', L'c');

    wstring srcEx = L"ĂăĄąĆćĈĉĊċČčĎďĐđĒēĔĕĖėĘęĚěĜĝĞğĠġĢģĤĥĦħĨĩĪīĬĭĮįİıĲĳĴĵĶķĸĹĺĻĽľĿŀŁł"
                    L"ŃńŅņŇňŉŊŋŌōŎŏŐőŒœŔŕŖŗŘřŚśŜŝŞşŠšŢţŤťŦŧŨũŪūŬŭŮůŰűŲųŴŵŶŷŸŹźŻżŽž";
    wstring dstEx = L"aaaaccccccccddddeeeeeeeeeegggggggghhhhiiiiiiiiiijjjjkkklllllllll"
                    L"nnnnnnnnnooooooaarrrrrrssssssssttttttuuuuuuuuuuuuwwyyyzzzzzz";
    assert(srcEx.size() == dstEx.size());
    for (size_t j = 0; j < srcEx.size(); j++) {
      uint32_t pos = (uint32_t)(uint16_t)srcEx[j];
      if (pos < 65536) map[pos] = dstEx[j];
    }
  }
  return map[(uint16_t)c];
}

void prepareMatchString(wchar_t *data_c, int size) {
  for (int j = 0; j < size; j++) {
    data_c[j] = (wchar_t)towlower(data_c[j]);
    data_c[j] = (wchar_t)toLowerStripped(data_c[j]);
  }
}

bool filterMatchString(const wstring &c, const wchar_t *filt_lc, int &score) {
  score = 0;
  if (filt_lc[0] == 0) return true;

  wstring key = c;
  for (auto &ch : key) {
    ch = (wchar_t)towlower(ch);
    ch = (wchar_t)toLowerStripped(ch);
  }
  const wchar_t *found = wcsstr(key.c_str(), filt_lc);
  if (found) {
    while (filt_lc[score] && key[score] && filt_lc[score] == key[score]) score++;
  }
  return found != nullptr;
}

int compareStringIgnoreCase(const wstring &a, const wstring &b) {
  size_t la = a.length(), lb = b.length(), minl = std::min(la, lb);
  for (size_t i = 0; i < minl; i++) {
    wchar_t ca = (wchar_t)towlower(a[i]);
    wchar_t cb = (wchar_t)towlower(b[i]);
    if (ca != cb) return ca < cb ? -1 : 1;
  }
  if (la != lb) return la < lb ? -1 : 1;
  return 0;
}

// ---- Text utilities ---------------------------------------------------------
int countWords(const wchar_t *p) {
  int nwords = 0;
  const wchar_t *ep = p;
  while (*ep) {
    if (!myIsSpace(*ep)) {
      nwords++;
      while (*ep && !myIsSpace(*ep)) ep++;
    }
    while (*ep && myIsSpace(*ep)) ep++;
  }
  return nwords;
}

string trim(const string &s) {
  const char *ptr = s.c_str();
  int len = (int)s.length(), i = 0, k = len - 1;
  while (i < len && ((uint8_t)ptr[i] == 160 || isspace((uint8_t)ptr[i]))) i++;
  while (k >= 0 && ((uint8_t)ptr[k] == 160 || isspace((uint8_t)ptr[k]))) k--;
  if (i == 0 && k == len-1) return s;
  if (k >= i && i < len) return s.substr(i, k-i+1);
  return "";
}

wstring trim(const wstring &s) {
  const wchar_t *ptr = s.c_str();
  int len = (int)s.length(), i = 0, k = len - 1;
  while (i < len && myIsSpace(ptr[i])) i++;
  while (k >= 0 && myIsSpace(ptr[k])) k--;
  if (i == 0 && k == len-1) return s;
  if (k >= i && i < len) return s.substr(i, k-i+1);
  return L"";
}

bool fileExists(const wstring &file) {
  return std::filesystem::exists(std::filesystem::path(file));
}

bool stringMatch(const wstring &a, const wstring &b) {
  return compareStringIgnoreCase(trim(a), trim(b)) == 0;
}

// ---- XML/HTML encode/decode -------------------------------------------------
const string &encodeXML(const string &in) {
  static string out;
  const char *bf = in.c_str();
  int len = (int)in.length();
  bool need = false;
  for (int k = 0; k < len; k++) need |= (bf[k]=='&') | (bf[k]=='>') | (bf[k]=='<') | (bf[k]=='"') | (bf[k]==0) | (bf[k]=='\n') | (bf[k]=='\r');
  if (!need) return in;
  out.clear();
  for (int k = 0; k < len; k++) {
    if      (bf[k]=='&')  out+="&amp;";
    else if (bf[k]=='<')  out+="&lt;";
    else if (bf[k]=='>')  out+="&gt;";
    else if (bf[k]=='"')  out+="&quot;";
    else if (bf[k]=='\n') out+="&#10;";
    else if (bf[k]=='\r') out+="&#13;";
    else if (bf[k]==0)    out+=' ';
    else                  out+=bf[k];
  }
  return out;
}

const wstring &encodeXML(const wstring &in) {
  static wstring out;
  const wchar_t *bf = in.c_str();
  int len = (int)in.length();
  bool need = false;
  for (int k = 0; k < len; k++) need |= (bf[k]=='&') | (bf[k]=='>') | (bf[k]=='<') | (bf[k]=='"') | (bf[k]==0) | (bf[k]=='\n') | (bf[k]=='\r');
  if (!need) return in;
  out.clear();
  for (int k = 0; k < len; k++) {
    if      (bf[k]=='&')  out+=L"&amp;";
    else if (bf[k]=='<')  out+=L"&lt;";
    else if (bf[k]=='>')  out+=L"&gt;";
    else if (bf[k]=='"')  out+=L"&quot;";
    else if (bf[k]=='\n') out+=L"&#10;";
    else if (bf[k]=='\r') out+=L"&#13;";
    else if (bf[k]==0)    out+=L' ';
    else                  out+=bf[k];
  }
  return out;
}

const wstring &encodeHTML(const wstring &in) {
  static wstring out;
  const wchar_t *bf = in.c_str();
  int len = (int)in.length();
  bool need = false;
  for (int k = 0; k < len; k++) need |= (bf[k]==' ')|(bf[k]=='&')|(bf[k]=='>')|(bf[k]=='<')|(bf[k]=='"')|(bf[k]==0)|(bf[k]=='\n')|(bf[k]=='\r')|(bf[k]==0x2013);
  if (!need) return in;
  out.clear();
  for (int k = 0; k < len; k++) {
    if      (bf[k]=='&')    out+=L"&amp;";
    else if (bf[k]=='<')    out+=L"&lt;";
    else if (bf[k]=='>')    out+=L"&gt;";
    else if (bf[k]=='"')    out+=L"&quot;";
    else if (bf[k]=='\n')   out+=L"&#10;";
    else if (bf[k]=='\r')   out+=L"&#13;";
    else if (bf[k]==0)      out+=L' ';
    else if (bf[k]==0x2013) out+=L"&ndash;";
    else if (bf[k]==' ')    out+=L"&nbsp;";
    else                    out+=bf[k];
  }
  return out;
}

const string &decodeXML(const string &in) {
  static string out;
  const char *bf = in.c_str();
  int len = (int)in.length();
  bool need = false;
  for (int k = 0; k < len; k++) need |= (bf[k]=='&');
  if (!need) return in;
  out.clear();
  std::ostringstream str;
  for (int k = 0; k < len; k++) {
    if (bf[k]=='&') {
      if      (!memcmp(&bf[k],"&amp;",5))  { str<<'&';  k+=4; }
      else if (!memcmp(&bf[k],"&lt;",4))   { str<<'<';  k+=3; }
      else if (!memcmp(&bf[k],"&gt;",4))   { str<<'>';  k+=3; }
      else if (!memcmp(&bf[k],"&quot;",6)) { str<<'"';  k+=5; }
      else if (!memcmp(&bf[k],"&nbsp;",6)) { str<<' ';  k+=5; }
      else if (!memcmp(&bf[k],"&#10;",5))  { str<<'\n'; k+=4; }
      else if (!memcmp(&bf[k],"&#13;",5))  { str<<'\r'; k+=4; }
      else str << bf[k];
    } else str << bf[k];
  }
  out = str.str();
  return out;
}

const char *decodeXML(const char *in) {
  static string out;
  bool need = false;
  for (int k = 0; in[k]; k++) need |= (in[k]=='&');
  if (!need) return in;
  out.clear();
  for (int k = 0; in[k]; k++) {
    if (in[k]=='&') {
      if      (!memcmp(&in[k],"&amp;",5))  { out+='&';  k+=4; }
      else if (!memcmp(&in[k],"&lt;",4))   { out+='<';  k+=3; }
      else if (!memcmp(&in[k],"&gt;",4))   { out+='>';  k+=3; }
      else if (!memcmp(&in[k],"&quot;",6)) { out+='"';  k+=5; }
      else if (!memcmp(&in[k],"&#10;",5))  { out+='\n'; k+=4; }
      else if (!memcmp(&in[k],"&#13;",5))  { out+='\r'; k+=4; }
      else out += in[k];
    } else out += in[k];
  }
  return out.c_str();
}

// ---- Canonical name / string distance ---------------------------------------
const wchar_t *canonizeName(const wchar_t *name) {
  static wchar_t out[70];
  static wchar_t tbf[70];
  for (int i = 0; i < 63 && name[i]; i++) {
    if (name[i] == ',') {
      int tout = 0;
      for (int j = i+1; j < 63 && name[j]; j++) tbf[tout++] = name[j];
      tbf[tout++] = ' ';
      for (int j = 0; j < i; j++) tbf[tout++] = name[j];
      tbf[tout] = 0; name = tbf; break;
    }
  }
  int outp = 0, k = 0;
  for (; k < 63 && name[k] == ' '; k++) {}
  bool first = true;
  while (k < 63 && name[k]) {
    if (!first) out[outp++] = ' ';
    while (name[k] != ' ' && k < 63 && name[k]) {
      out[outp++] = (name[k] == '-') ? ' ' : (wchar_t)toLowerStripped(name[k]);
      k++;
    }
    first = false;
    while (name[k] == ' ' && k < 64) k++;
  }
  out[outp] = 0;
  return out;
}

static const int notFound = 1000000;
static int charDist(const wchar_t *b, int len, int origin, wchar_t c) {
  int bound = std::max(1, std::min(len/2, 4));
  for (int k = 0; k < bound; k++) {
    int i = origin - k;
    if (i > 0 && b[i] == c) return -k;
    i = origin + k;
    if (i < len && b[i] == c) return k;
  }
  return notFound;
}

static double stringDistanceImpl(const wchar_t *a, int al, const wchar_t *b, int bl) {
  al = std::min(al, 256);
  int d1[256], avg = 0, missing = 0, ndiff = 1;
  for (int k = 0; k < al; k++) {
    int d = charDist(b, bl, k, a[k]);
    if (d == notFound) { missing++; d1[k] = 0; }
    else { d1[k] = d; avg += d; ndiff++; }
  }
  if (missing > std::min(3, std::max(1, al/3))) return 1;
  double mfactor = missing * 0.8;
  double center = (double)avg / (double)ndiff;
  double dist = 0;
  for (int k = 0; k < al; k++) {
    double ld = std::min<double>(std::fabs(d1[k] - center), std::abs(d1[k]));
    dist += ld * ld;
  }
  return (std::sqrt(dist) + mfactor * mfactor) / (double)al;
}

double stringDistance(const wchar_t *a, const wchar_t *b) {
  int al = (int)wcslen(a), bl = (int)wcslen(b);
  double d1 = stringDistanceImpl(a, al, b, bl); if (d1 >= 1) return 1.0;
  double d2 = stringDistanceImpl(b, bl, a, al); if (d2 >= 1) return 1.0;
  return (std::max(d1, d2) + d1 + d2) / 3.0;
}

double stringDistanceAssymetric(const wstring &target, const wstring &sample) {
  double d = stringDistanceImpl(target.c_str(), (int)target.length(), sample.c_str(), (int)sample.length());
  return std::min(1.0, d);
}

// ---- Number helpers ---------------------------------------------------------
int getNumberSuffix(const string &str) {
  int pos = (int)str.length();
  while (pos > 1 && (str[pos-1] & (~127)) == 0 && (isspace((uint8_t)str[pos-1]) || isdigit((uint8_t)str[pos-1]))) pos--;
  if (pos == (int)str.length()) return 0;
  return atoi(str.c_str() + pos);
}

int getNumberSuffix(const wstring &str) {
  int pos = (int)str.length();
  while (pos > 1 && (str[pos-1] & (~127)) == 0 && (isspace((int)str[pos-1]) || isdigit((int)str[pos-1]))) pos--;
  if (pos == (int)str.length()) return 0;
  return wtoi(str.c_str() + pos);
}

int extractAnyNumber(const wstring &str, wstring &prefix, wstring &suffix) {
  for (size_t k = 0; k < str.length(); k++) {
    if (isdigit((int)str[k])) {
      prefix = str.substr(0, k);
      int num = wtoi(str.c_str() + k);
      while (k < str.length() && (str[++k] & ~0x7F) == 0 && isdigit((int)str[k]));
      suffix = str.substr(k);
      return num;
    }
  }
  return -1;
}

// ---- Class name comparison --------------------------------------------------
static void decomposeClassName(const wstring &name, vector<wstring> &dec) {
  if (name.empty()) return;
  dec.push_back(wstring());
  for (size_t i = 0; i < name.size(); i++) {
    int bchar = toLowerStripped(name[i]);
    if (myIsSpace((wchar_t)bchar) || bchar == '-' || bchar == 160) {
      if (!dec.back().empty()) dec.push_back(wstring());
      continue;
    }
    if (!dec.back().empty()) {
      int last = dec.back().back();
      bool lastNum = last >= '0' && last <= '9';
      bool isNum   = bchar >= '0' && bchar <= '9';
      if (lastNum ^ isNum) dec.push_back(wstring());
    }
    dec.back().push_back((wchar_t)bchar);
  }
  if (!dec.empty() && dec.back().empty()) dec.pop_back();
  std::sort(dec.begin(), dec.end());
}

bool compareClassName(const wstring &a, const wstring &b) {
  if (a.empty() && b.empty()) return true;
  if (a.empty() || b.empty()) return false;
  vector<wstring> da, db;
  decomposeClassName(a, da);
  decomposeClassName(b, db);
  if (da.empty() || db.empty()) return false;
  int match = 0, total = (int)std::max(da.size(), db.size());
  for (auto &pa : da) for (auto &pb : db) if (pb.length() > 0 && pa.find(pb) == 0) { match++; break; }
  return match >= total * 2 / 3;
}

// ---- Error message ----------------------------------------------------------
wstring getErrorMessage(int code) {
#ifdef _WIN32
  LPVOID msg = nullptr;
  FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                 nullptr, (DWORD)code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&msg, 0, nullptr);
  if (!msg) {
    wchar_t ch[128];
    swprintf(ch, sizeof(ch)/sizeof(wchar_t), L"Error code: %d", code);
    return ch;
  }
  wstring str = (LPWSTR)msg;
  LocalFree(msg);
  return str;
#else
  if (code == 0) return L"";
  const char *msg = strerror(code);
  if (!msg) { wchar_t ch[128]; swprintf(ch,sizeof(ch)/sizeof(wchar_t),L"Error code: %d",code); return ch; }
  return wstring(msg, msg + strlen(msg));
#endif
}

// ---- HLS color class --------------------------------------------------------
HLS &HLS::RGBtoHLS(uint32_t lRGBColor) {
  uint16_t R = GetRValue(lRGBColor);
  uint16_t G = GetGValue(lRGBColor);
  uint16_t B = GetBValue(lRGBColor);
  uint8_t cMax = (uint8_t)std::max({R, G, B});
  uint8_t cMin = (uint8_t)std::min({R, G, B});
  short &L = lightness, &H = hue, &S = saturation;

  L = (short)(((cMax + cMin) * HLSMAX + RGBMAX) / (2 * RGBMAX));
  if (cMax == cMin) { S = 0; H = UNDEFINED; }
  else {
    if (L <= HLSMAX/2)
      S = (short)(((cMax-cMin)*HLSMAX + (cMax+cMin)/2) / (cMax+cMin));
    else
      S = (short)(((cMax-cMin)*HLSMAX + (2*RGBMAX-cMax-cMin)/2) / (2*RGBMAX-cMax-cMin));
    uint16_t Rdelta = (uint16_t)(((cMax-R)*(HLSMAX/6) + (cMax-cMin)/2) / (cMax-cMin));
    uint16_t Gdelta = (uint16_t)(((cMax-G)*(HLSMAX/6) + (cMax-cMin)/2) / (cMax-cMin));
    uint16_t Bdelta = (uint16_t)(((cMax-B)*(HLSMAX/6) + (cMax-cMin)/2) / (cMax-cMin));
    if (R == cMax)      H = (short)(Bdelta - Gdelta);
    else if (G == cMax) H = (short)(HLSMAX/3 + Rdelta - Bdelta);
    else                H = (short)(2*HLSMAX/3 + Gdelta - Rdelta);
    if (H < 0) H += HLSMAX;
    if (H > HLSMAX) H -= HLSMAX;
  }
  return *this;
}

uint16_t HLS::HueToRGB(uint16_t n1, uint16_t n2, uint16_t hue) const {
  if ((int)hue < 0) hue += HLSMAX;
  if (hue > HLSMAX) hue -= HLSMAX;
  if (hue < HLSMAX/6)       return n1 + (((n2-n1)*hue + HLSMAX/12) / (HLSMAX/6));
  if (hue < HLSMAX/2)       return n2;
  if (hue < 2*HLSMAX/3)     return n1 + (((n2-n1)*(2*HLSMAX/3 - hue) + HLSMAX/12) / (HLSMAX/6));
  return n1;
}

uint32_t HLS::HLStoRGB() const {
  uint16_t lum = (uint16_t)lightness, sat = (uint16_t)saturation;
  uint16_t R, G, B;
  if (sat == 0) {
    R = G = B = (uint16_t)((lum * RGBMAX) / HLSMAX);
  } else {
    uint16_t Magic2 = (lum <= HLSMAX/2)
      ? (uint16_t)((lum*(HLSMAX+sat) + HLSMAX/2) / HLSMAX)
      : (uint16_t)(lum + sat - ((lum*sat + HLSMAX/2) / HLSMAX));
    uint16_t Magic1 = (uint16_t)(2*lum - Magic2);
    R = (uint16_t)((HueToRGB(Magic1, Magic2, (uint16_t)(hue + HLSMAX/3)) * RGBMAX + HLSMAX/2) / HLSMAX);
    G = (uint16_t)((HueToRGB(Magic1, Magic2, (uint16_t)hue)              * RGBMAX + HLSMAX/2) / HLSMAX);
    B = (uint16_t)((HueToRGB(Magic1, Magic2, (uint16_t)(hue - HLSMAX/3)) * RGBMAX + HLSMAX/2) / HLSMAX);
  }
  return RGB(R, G, B);
}

void HLS::lighten(double f)    { lightness  = (short)std::min<int>(HLSMAX, (int)(f * lightness)); }
void HLS::saturate(double s)   { saturation = (short)std::min<int>(HLSMAX, (int)(s * saturation)); }
void HLS::colorDegree(double) {}

// ---- Type checks ------------------------------------------------------------
bool isAscii(const string &s) {
  for (char c : s) if ((uint8_t)c > 127) return false;
  return true;
}
bool isAscii(const wstring &s) {
  for (wchar_t c : s) if ((uint32_t)c > 127) return false;
  return true;
}
bool isNumber(const string &s) {
  if (s.empty()) return false;
  for (char c : s) if (!isdigit((uint8_t)c)) return false;
  return true;
}
bool isNumber(const wstring &s) {
  if (s.empty()) return false;
  for (wchar_t c : s) if ((c & 127) != c || !isdigit((int)c)) return false;
  return true;
}

// ---- Dynamic base conversion ------------------------------------------------
int convertDynamicBase(const wstring &s, long long &out) {
  out = 0;
  if (s.empty()) return 0;
  bool alpha = false, general = false;
  for (wchar_t c : s) {
    unsigned uc = (unsigned)c;
    if (uc >= '0' && uc <= '9') continue;
    if ((uc >= 'A' && uc <= 'Z') || (uc >= 'a' && uc <= 'z')) { alpha = true; continue; }
    general = true;
    if (uc < 32) return 0;
  }
  int base = general ? 256-32 : (alpha ? 36 : 10);
  long long factor = 1;
  for (int k = (int)s.length()-1; k >= 0; k--) {
    unsigned c = (unsigned)s[k] & 0xFF;
    if (general) c -= 32;
    else if (c >= '0' && c <= '9') c -= '0';
    else if (c >= 'A' && c <= 'Z') c -= 'A'-10;
    else if (c >= 'a' && c <= 'z') c -= 'a'-10;
    out += factor * c; factor *= base;
  }
  return base;
}

void convertDynamicBase(long long val, int base, wchar_t out[16]) {
  int len = 0;
  while (val != 0) {
    unsigned int c = (unsigned int)(val % base);
    val /= base;
    char cc;
    if (base == 10) cc = '0' + c;
    else if (base == 36) cc = (c < 10) ? '0' + c : 'A' + c - 10;
    else cc = (char)(c + 32);
    out[len++] = cc;
  }
  out[len] = 0;
  std::reverse(out, out + len);
}

// ---- Directory enumeration --------------------------------------------------
bool expandDirectory(const wchar_t *dir, const wchar_t *pattern, vector<wstring> &res) {
  namespace fs = std::filesystem;
  fs::path basePath;
  if (dir[0] == L'.') basePath = fs::current_path() / (dir + 1);
  else                basePath = dir;

  wstring pat(pattern);
  wstring suffix;
  auto star = pat.find(L'*');
  if (star != wstring::npos) suffix = pat.substr(star + 1);

  std::error_code ec;
  bool found = false;
  for (const auto &entry : fs::directory_iterator(basePath, ec)) {
    if (entry.is_directory()) continue;
    wstring name = entry.path().filename().wstring();
    if (name.empty() || name[0] == L'.') continue;
    if (suffix.empty() || (name.size() >= suffix.size() && name.compare(name.size()-suffix.size(), suffix.size(), suffix) == 0)) {
      res.push_back(entry.path().wstring());
      found = true;
    }
  }
  return found;
}

// ---- Sex encoding -----------------------------------------------------------
wstring encodeSex(PersonSex sex) {
  if (sex == sFemale) return L"F";
  if (sex == sMale)   return L"M";
  if (sex == sBoth)   return L"B";
  return L"";
}

PersonSex interpretSex(const wstring &sex) {
  int s = sex.empty() ? 0 : sex[0];
  if (s=='F'||s=='K'||s=='W'||s=='f'||s=='k'||s=='w') return sFemale;
  if (s=='M'||s=='H'||s=='m'||s=='h') return sMale;
  if (s=='B'||s=='b') return sBoth;
  return sUnknown;
}

bool matchNumber(int a, const wchar_t *b) {
  if (a == 0 && b[0]) return false;
  wchar_t bf[32];
  swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%d", a);
  for (int k = 0; k < 12; k++) {
    if (b[k] == 0) return true;
    if (bf[k] != b[k]) return false;
  }
  return false;
}

// ---- File name sanitisation -------------------------------------------------
wstring makeValidFileName(const wstring &input, bool strict) {
  wstring out;
  out.reserve(input.size());
  if (strict) {
    for (wchar_t b : input) {
      if ((b>='0'&&b<='9')||(b>='a'&&b<='z')||(b>='A'&&b<='Z')||b=='_'||b=='.') { out.push_back(b); continue; }
      if (b==' '||b==',') { out.push_back('_'); continue; }
      b = (wchar_t)toLowerStripped(b);
      if (b>='a'&&b<='z') {}
      else if (b==L'ö') b='o';
      else if (b==L'ä'||b==L'å'||b==L'à'||b==L'á'||b==L'â'||b==L'ã'||b==L'æ') b='a';
      else if (b==L'ç') b='c';
      else if (b==L'è'||b==L'é'||b==L'ê'||b==L'ë') b='e';
      else if (b==L'ï'||b==L'ì'||b==L'î'||b==L'í') b='i';
      else if (b==L'ò'||b==L'ó'||b==L'ô'||b==L'õ'||b==L'ø') b='o';
      else if (b==L'ù'||b==L'ú'||b==L'û'||b==L'ü') b='u';
      else if (b==L'ý') b='y';
      else b='-';
      out.push_back(b);
    }
  } else {
    for (wchar_t b : input) {
      if (b < 32 || b=='*'||b=='?'||b==':'||b=='/'||b=='\\') b='_';
      out.push_back(b);
    }
  }
  return out;
}

string makeValidFileName(const string &input, bool strict) {
  string out;
  out.reserve(input.size());
  if (strict) {
    for (char b : input) {
      if ((b>='0'&&b<='9')||(b>='a'&&b<='z')||(b>='A'&&b<='Z')||b=='_'||b=='.') { out.push_back(b); continue; }
      if (b==' '||b==',') { out.push_back('_'); continue; }
      if (b>='a'&&b<='z') {}
      else if (b<0) b='-';
      else b='-';
      out.push_back(b);
    }
  } else {
    for (char b : input) {
      if (b<32||b=='*'||b=='?'||b==':'||b=='/'||b=='\\') b='_';
      out.push_back(b);
    }
  }
  return out;
}

// ---- Capitalization ---------------------------------------------------------
void capitalize(wstring &str) {
  if (!str.empty()) str[0] = (wchar_t)towupper(str[0]);
}

static bool noCapitalize(const wstring &str, size_t pos) {
  string word;
  while (pos < str.length() && !myIsSpace(str[pos])) word.push_back((char)str[pos++]);
  return word=="of"||word=="for"||word=="at"||word=="by"||word=="on"||
         word=="and"||word=="or"||word=="from"||word=="as"||word=="in"||
         word=="with"||word=="to"||word=="next"||word=="a"||word=="an"||
         word=="the"||word=="but";
}

void capitalizeWords(wstring &str) {
  bool init = true;
  for (size_t i = 0; i < str.length(); i++) {
    wchar_t c = str[i];
    if (init && c >= 'a' && c <= 'z' && !noCapitalize(str, i)) str[i] = c + ('A'-'a');
    init = iswspace(c) || c=='/' || c=='-';
  }
}

// ---- Bib comparison ---------------------------------------------------------
bool compareBib(const wstring &b1, const wstring &b2) {
  int l1 = (int)b1.length(), l2 = (int)b2.length();
  if (l1 != l2) return l1 < l2;
  wchar_t maxc = 0, minc = std::numeric_limits<wchar_t>::max();
  for (wchar_t b : b1) { maxc = std::max(maxc, b); minc = std::min(minc, b); }
  for (wchar_t b : b2) { maxc = std::max(maxc, b); minc = std::min(minc, b); }
  unsigned coeff = maxc - minc + 1;
  unsigned z1 = 0; for (wchar_t b : b1) z1 = coeff * z1 + (b - minc);
  unsigned z2 = 0; for (wchar_t b : b2) z2 = coeff * z2 + (b - minc);
  return z1 < z2;
}

// ---- Name splitting ---------------------------------------------------------
static int getNameCommaSplitPoint(const wstring &name) {
  for (size_t k = 1; k + 1 < name.size(); k++)
    if (name[k] == ',') return (int)k + 2;
  return -1;
}

static int getNameSplitPoint(const wstring &name) {
  int splits[10];
  int nSplit = 0;
  for (size_t k = 1; k + 1 < name.size(); k++) {
    if (iswspace(name[k])) {
      splits[nSplit++] = (int)k;
      if (nSplit >= 9) break;
    }
  }
  if (nSplit == 1) return splits[0] + 1;
  if (nSplit == 0) return -1;
  return splits[nSplit-1] + 1; // simplified: split at last space
}

wstring getGivenName(const wstring &name) {
  int sp = getNameCommaSplitPoint(name);
  if (sp != -1) return trim(name.substr(sp));
  sp = getNameSplitPoint(name);
  return (sp == -1) ? trim(name) : trim(name.substr(0, sp));
}

wstring getFamilyName(const wstring &name) {
  int sp = getNameCommaSplitPoint(name);
  if (sp != -1) return trim(name.substr(0, sp - 2));
  sp = getNameSplitPoint(name);
  if (sp == -1) return _EmptyWString;
  return trim(name.substr(sp));
}

// ---- File lock --------------------------------------------------------------
#ifdef _WIN32
void MeOSFileLock::unlockFile() {
  if (lockedFile != INVALID_HANDLE_VALUE) { CloseHandle(lockedFile); lockedFile = INVALID_HANDLE_VALUE; }
}
void MeOSFileLock::lockFile(const wstring &file) {
  unlockFile();
  lockedFile = CreateFileW(file.c_str(), GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (lockedFile == INVALID_HANDLE_VALUE) {
    DWORD err = GetLastError();
    if (err == ERROR_SHARING_VIOLATION) throw meosException("open_error_locked");
    throw meosException(L"open_error#" + file);
  }
}
#else
void MeOSFileLock::unlockFile() {
  if (fd >= 0) { close(fd); fd = -1; }
}
void MeOSFileLock::lockFile(const wstring &file) {
  unlockFile();
  string sfile = wstring_to_utf8(file);
  fd = open(sfile.c_str(), O_RDWR | O_CREAT, 0666);
  if (fd < 0) throw meosException(L"open_error#" + file);
  struct flock fl = {};
  fl.l_type = F_WRLCK; fl.l_whence = SEEK_SET;
  if (fcntl(fd, F_SETLK, &fl) < 0) {
    close(fd); fd = -1;
    if (errno == EAGAIN || errno == EACCES) throw meosException("open_error_locked");
    throw meosException(L"open_error#" + file);
  }
}
#endif

// ---- File operations --------------------------------------------------------
void checkWriteAccess(const wstring &file) {
  namespace fs = std::filesystem;
  fs::path p(file);
  int flags = fs::exists(p) ? O_WRONLY : (O_WRONLY | O_CREAT);
  string sfile = wstring_to_utf8(file);
#ifdef _WIN32
  HANDLE h = CreateFileW(file.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE|FILE_SHARE_READ, nullptr,
                         fs::exists(p) ? OPEN_EXISTING : CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) throw meosException(L"Behörighet saknas för att skriva till 'X'.#" + file);
  CloseHandle(h);
#else
  int fd2 = open(sfile.c_str(), flags, 0666);
  if (fd2 < 0) throw meosException(L"Behörighet saknas för att skriva till 'X'.#" + file);
  close(fd2);
#endif
}

void moveFile(const wstring &src, const wstring &dst) {
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::remove(fs::path(dst), ec);
  fs::copy_file(fs::path(src), fs::path(dst), fs::copy_options::overwrite_existing, ec);
  if (ec) throw meosException(L"Kunde inte skriva till 'X'.#" + dst);
  fs::remove(fs::path(src), ec);
}

// ---- String encoding conversions --------------------------------------------
void string2Wide(const string &in, wstring &out) {
  if (in.empty()) { out = L""; return; }
#ifdef _WIN32
  out.resize(in.size(), 0);
  int n = MultiByteToWideChar(defaultCodePage, MB_PRECOMPOSED, in.c_str(), (int)in.size(), &out[0], (int)out.size());
  out.resize(n);
#else
  out = utf8_to_wstring(in);
#endif
}

void wide2String(const wstring &in, string &out) {
  out.clear();
  out.insert(out.begin(), in.begin(), in.end());
}

const string &narrow(const wstring &input) {
  string &output = StringCache::getInstance().get();
  output.clear();
  output.insert(output.begin(), input.begin(), input.end());
  return output;
}

const wstring &widen(const string &input) {
  wstring &output = StringCache::getInstance().wget();
#ifdef _WIN32
  if (input.empty()) { output = L""; return output; }
  output.resize(input.size(), 0);
  int n = MultiByteToWideChar(1252, MB_PRECOMPOSED, input.c_str(), (int)input.size(), &output[0], (int)output.size());
  output.resize(n);
#else
  output.clear();
  for (uint8_t c : input) output.push_back((wchar_t)cp1252_to_cp(c));
#endif
  return output;
}

const string &toUTF8(const wstring &winput) {
  string &output = StringCache::getInstance().get();
#ifdef _WIN32
  output.resize(winput.length() * 4 + 32);
  WideCharToMultiByte(CP_UTF8, 0, winput.c_str(), (int)winput.length()+1, &output[0], (int)output.size(), 0, 0);
  output.resize(strlen(output.c_str()));
#else
  output = wstring_to_utf8(winput);
#endif
  return output;
}

const wstring &fromUTF8(const string &input) {
  wstring &output = StringCache::getInstance().wget();
#ifdef _WIN32
  output.resize(input.length() + 1);
  int wlen = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), (int)input.length(), &output[0], (int)output.size());
  output.resize(wlen);
#else
  output = utf8_to_wstring(input);
#endif
  return output;
}

// ---- Version functions (from meosversion.cpp) ------------------------------
int getMeosBuild() {
  return 174 + 1568; // Fixed value from $Rev: 1568 $
}

wstring getMeosDate() {
  return L"2026-02-19";
}

static wstring getBuildType() { return L"Beta 1"; }

wstring getMajorVersion() { return L"5.0"; }

wstring getMeosFullVersion() {
  wchar_t bf[256];
#if defined(_WIN64)
  const wchar_t *bits = L"64-bit";
#elif defined(_WIN32)
  const wchar_t *bits = L"32-bit";
#elif defined(__x86_64__)
  const wchar_t *bits = L"64-bit";
#else
  const wchar_t *bits = L"32-bit";
#endif
  wstring maj = getMajorVersion();
  wstring bt  = getBuildType();
  if (bt.empty())
    swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"Version X#%s.%d (%s), %s", maj.c_str(), getMeosBuild(), bits, getMeosDate().c_str());
  else
    swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"Version X#%s.%d (%s), %s, %s", maj.c_str(), getMeosBuild(), bits, bt.c_str(), getMeosDate().c_str());
  return bf;
}

wstring getMeosCompectVersion() {
  wstring bt = getBuildType();
  if (bt.empty()) return getMajorVersion() + L"." + itow(getMeosBuild());
  return getMajorVersion() + L"." + itow(getMeosBuild()) + L" (" + bt + L")";
}

void getSupporters(vector<wstring> &supp, vector<wstring> &developSupp) {
  supp.emplace_back(L"Zdenko Rohac, KOB ATU Košice");
  supp.emplace_back(L"Hans Carlstedt, Sävedalens AIK");
  supp.emplace_back(L"O-Liceo, Spain");
  developSupp.emplace_back(L"Västerviks OK");
  supp.emplace_back(L"Aarhus 1900 Orientering");
  supp.emplace_back(L"Ljusne Ala OK");
  supp.emplace_back(L"Sävedalens AIK");
  supp.emplace_back(L"Foothills Wanderers Orienteering Club");
  std::reverse(supp.begin(), supp.end());
}

// ---- Stub zip operations ----------------------------------------------------
void unzip(const wchar_t *, const char *, vector<wstring> &) {}
int  zip(const wchar_t *, const char *, const vector<wstring> &) { return 0; }

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

#include "TimeStamp.h"
#include "time_util.h"
#include "timeconstants.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <ctime>

// Seconds between the Windows FILETIME epoch (1601-01-01) and Unix epoch (1970-01-01)
static constexpr int64_t kFileTimeToUnix = 11644473600LL;

// Year offset used to keep Time as an unsigned 32-bit offset from a recent epoch.
// Matches the legacy constant: (2014 - 1601) years before the FILETIME epoch.
static constexpr int64_t kMinYearConstant = 2014 - 1601;

// Convert current wall-clock time to the internal unsigned representation.
static unsigned currentTimeInternal() {
  std::tm st = {};
  meos_localtime_now(&st);
#ifdef _WIN32
  time_t t = _mkgmtime(&st);
#else
  time_t t = timegm(&st);
#endif
  int64_t ft64 = (int64_t(t) + kFileTimeToUnix) * 10000000LL;
  return static_cast<unsigned>((ft64 / 10000000LL) - kMinYearConstant * 365 * 24 * timeConstSecPerHour);
}

// Decode an internal unsigned time value to std::tm (UTC).
static std::tm decodeTime(unsigned t) {
  int64_t ft64 = (int64_t(t) + kMinYearConstant * 365LL * 24 * timeConstSecPerHour) * 10000000LL;
  time_t unix_t = static_cast<time_t>(ft64 / 10000000LL) - kFileTimeToUnix;
  std::tm st = {};
#ifdef _WIN32
  gmtime_s(&st, &unix_t);
#else
  gmtime_r(&unix_t, &st);
#endif
  return st;
}

TimeStamp::TimeStamp() : Time(0) {}

TimeStamp::~TimeStamp() {}

void TimeStamp::update(TimeStamp& ts) {
  Time = std::max(Time, ts.Time);
}

void TimeStamp::update() {
  Time = currentTimeInternal();
}

int TimeStamp::getAge() const {
  unsigned cur = currentTimeInternal();
  return static_cast<int>(cur) - static_cast<int>(Time);
}

const std::string& TimeStamp::getStamp() const {
  if (stampCodeTime == static_cast<int>(Time))
    return stampCode;

  stampCodeTime = static_cast<int>(Time);
  std::tm st = decodeTime(Time);

  // Clamp year to [2009, thisYear]
  int y = st.tm_year + 1900;
  int maxY = meos::util::getThisYear();
  if (y < 2009 || y > maxY) {
    y = maxY;
    st.tm_year  = y - 1900;
    st.tm_mon   = 0;
    st.tm_mday  = 1;
    st.tm_hour  = 2;
    st.tm_min   = 0;
    st.tm_sec   = 0;
  }

  char bf[32];
  snprintf(bf, sizeof(bf), "%d%02d%02d%02d%02d%02d",
           st.tm_year + 1900, st.tm_mon + 1, st.tm_mday,
           st.tm_hour, st.tm_min, st.tm_sec);
  stampCode = bf;
  return stampCode;
}

const std::string& TimeStamp::getStamp(const std::string& sqlStampIn) const {
  stampCode.resize(8 + 6);
  int outIx = 0;
  for (char c : sqlStampIn) {
    if (c >= '0' && c <= '9' && outIx < 8 + 6)
      stampCode[outIx++] = c;
  }
  stampCode.resize(outIx);  // trim to actual digits written
  return stampCode;
}

const std::wstring TimeStamp::getUpdateTime() const {
  std::tm st = decodeTime(Time);
  wchar_t bf[32];
  swprintf(bf, sizeof(bf) / sizeof(wchar_t), L"%02d:%02d", st.tm_hour, st.tm_min);
  return bf;
}

std::wstring TimeStamp::getStampString() const {
  std::tm st = decodeTime(Time);
  wchar_t bf[32];
  swprintf(bf, sizeof(bf) / sizeof(wchar_t), L"%d-%02d-%02d %02d:%02d:%02d",
           st.tm_year + 1900, st.tm_mon + 1, st.tm_mday,
           st.tm_hour, st.tm_min, st.tm_sec);
  return bf;
}

std::string TimeStamp::getStampStringN() const {
  std::tm st = decodeTime(Time);
  int y = st.tm_year + 1900;
  int maxY = meos::util::getThisYear();
  if (y > maxY || y < 2009) {
    st.tm_year = maxY - 1900;
    st.tm_mday = 1;
    st.tm_mon  = 0;
    st.tm_hour = 2;
    st.tm_min  = 0;
    st.tm_sec  = 0;
  }
  char bf[32];
  snprintf(bf, sizeof(bf), "%d-%02d-%02d %02d:%02d:%02d",
           st.tm_year + 1900, st.tm_mon + 1, st.tm_mday,
           st.tm_hour, st.tm_min, st.tm_sec);
  return bf;
}

void TimeStamp::setStamp(const std::string& s) {
  if (s.size() < 14)
    return;

  std::tm st = {};

  auto parse = [](const char* data, int size, const char*& next) -> int {
    int ix = 0;
    int out = 0;
    while (ix < size && data[ix] >= '0' && data[ix] <= '9') {
      out = (out << 1) + (out << 3) + data[ix] - '0';
      ix++;
    }
    while (data[ix] && (data[ix] == ' ' || data[ix] == '-' || data[ix] == ':'))
      ix++;
    next = data + ix;
    return out;
  };

  const char* ptr = s.data();
  st.tm_year = parse(ptr, 4, ptr) - 1900;
  st.tm_mon  = parse(ptr, 2, ptr) - 1;
  st.tm_mday = parse(ptr, 2, ptr);
  st.tm_hour = parse(ptr, 2, ptr);
  st.tm_min  = parse(ptr, 2, ptr);
  st.tm_sec  = parse(ptr, 2, ptr);

#ifdef _WIN32
  time_t t = _mkgmtime(&st);
#else
  time_t t = timegm(&st);
#endif
  int64_t currenttime = (int64_t(t) + kFileTimeToUnix) * 10000000LL;
  Time = static_cast<unsigned>((currenttime / 10000000LL) - kMinYearConstant * 365LL * 24 * timeConstSecPerHour);
}

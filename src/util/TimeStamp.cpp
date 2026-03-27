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
#include "meos_util.h"
#include "timeconstants.hpp"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <cstdint>

constexpr int64_t minYearConstant = 2014 - 1601;

// Seconds between Windows FILETIME epoch (1601-01-01) and Unix epoch (1970-01-01)
constexpr uint64_t WIN_UNIX_EPOCH_OFFSET = 11644473600ULL;

// Encode: local time treated as UTC -> time_t -> sec_since_1601 -> Time
// (matches original GetLocalTime + SystemTimeToFileTime behavior)
static unsigned encodeLocalTimeAsTime(const std::tm &local_st) {
  std::tm st_copy = local_st;
#ifdef _WIN32
  std::time_t encoded = _mkgmtime(&st_copy);
#else
  std::time_t encoded = timegm(&st_copy);
#endif
  uint64_t sec_since_1601 = (uint64_t)encoded + WIN_UNIX_EPOCH_OFFSET;
  return unsigned(sec_since_1601 - (uint64_t)minYearConstant * 365 * 24 * timeConstSecPerHour);
}

// Decode: Time -> sec_since_1601 -> time_t -> std::tm (local components via gmtime)
static std::tm decodeTimeToTm(unsigned time) {
  uint64_t sec_since_1601 = (uint64_t)time + (uint64_t)minYearConstant * 365 * 24 * timeConstSecPerHour;
  std::time_t tt = (std::time_t)(sec_since_1601 - WIN_UNIX_EPOCH_OFFSET);
  std::tm st = {};
#ifdef _WIN32
  gmtime_s(&st, &tt);
#else
  gmtime_r(&tt, &st);
#endif
  return st;
}

TimeStamp::TimeStamp()
{
  Time = 0;
}

TimeStamp::~TimeStamp()
{
}

void TimeStamp::update(TimeStamp &ts)
{
  Time = std::max(Time, ts.Time);
}

void TimeStamp::update()
{
  std::tm local_st = {};
  meos_localtime_now(&local_st);
  Time = encodeLocalTimeAsTime(local_st);
}

int TimeStamp::getAge() const
{
  std::tm local_st = {};
  meos_localtime_now(&local_st);
  unsigned CTime = encodeLocalTimeAsTime(local_st);
  return int(CTime) - int(Time);
}

const std::string &TimeStamp::getStamp() const
{
  if (stampCodeTime == (int)Time)
    return stampCode;

  stampCodeTime = (int)Time;
  std::tm st = decodeTimeToTm(Time);
  char bf[64];
  snprintf(bf, sizeof(bf), "%d%02d%02d%02d%02d%02d",
           st.tm_year + 1900, st.tm_mon + 1, st.tm_mday,
           st.tm_hour, st.tm_min, st.tm_sec);
  stampCode = bf;

  return stampCode;
}

const std::string &TimeStamp::getStamp(const std::string &sqlStampIn) const {
  stampCode.resize(8 + 6 + 1);
  int outIx = 0;
  for (char c : sqlStampIn) {
    if (c >= '0' && c <= '9' && outIx < 8 + 6)
      stampCode[outIx++] = c;
  }
  stampCode.resize(outIx);
  return stampCode;
}

const std::wstring TimeStamp::getUpdateTime() const {
  std::tm st = decodeTimeToTm(Time);
  wchar_t bf[32];
  swprintf(bf, sizeof(bf) / sizeof(wchar_t), L"%02d:%02d", st.tm_hour, st.tm_min);
  return bf;
}

std::wstring TimeStamp::getStampString() const
{
  std::tm st = decodeTimeToTm(Time);
  wchar_t bf[32];
  swprintf(bf, sizeof(bf) / sizeof(wchar_t), L"%d-%02d-%02d %02d:%02d:%02d",
           st.tm_year + 1900, st.tm_mon + 1, st.tm_mday,
           st.tm_hour, st.tm_min, st.tm_sec);
  return bf;
}

std::string TimeStamp::getStampStringN() const
{
  std::tm st = decodeTimeToTm(Time);
  int y = getThisYear();
  if ((st.tm_year + 1900) > y || (st.tm_year + 1900) < 2009) {
    st.tm_year = y - 1900;
    st.tm_mday = 1;
    st.tm_mon = 0;
    st.tm_hour = 2;
    st.tm_min = 0;
    st.tm_sec = 0;
  }

  char bf[32];
  snprintf(bf, sizeof(bf), "%d-%02d-%02d %02d:%02d:%02d",
           st.tm_year + 1900, st.tm_mon + 1, st.tm_mday,
           st.tm_hour, st.tm_min, st.tm_sec);
  return bf;
}

void TimeStamp::setStamp(const std::string &s)
{
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
    while (data[ix] && (data[ix] == ' ' || data[ix] == '-' || data[ix] == ':')) {
      ix++;
    }
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

  Time = encodeLocalTimeAsTime(st);
}

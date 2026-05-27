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

// TimeStamp.cpp: implementation of the TimeStamp class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "meos.h"
#include "TimeStamp.h"
#include <algorithm>
#include "meos_util.h"
#include <chrono>
#include <ctime>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

constexpr __int64 minYearConstant = 2014 - 1601;

TimeStamp::TimeStamp()
{
  Time=0;
  //Update();
}

TimeStamp::~TimeStamp()
{

}

void TimeStamp::update(TimeStamp &ts)
{
  Time=max(Time, ts.Time);
}

void TimeStamp::update()
{
  std::tm st = {};
  meos_localtime_now(&st);
#ifdef _WIN32
  time_t t = _mkgmtime(&st);
#else
  time_t t = timegm(&st);
#endif
  __int64 currenttime = (__int64(t) + 11644473600LL) * 10000000LL;
  Time = unsigned((currenttime / 10000000L) - minYearConstant * 365 * 24 * timeConstSecPerHour);
}

int TimeStamp::getAge() const
{
  std::tm st = {};
  meos_localtime_now(&st);
#ifdef _WIN32
  time_t t = _mkgmtime(&st);
#else
  time_t t = timegm(&st);
#endif
  __int64 currenttime = (__int64(t) + 11644473600LL) * 10000000LL;
  int CTime = int((currenttime / 10000000) - minYearConstant * 365 * 24 * timeConstSecPerHour);

  return CTime - Time;
}

const string &TimeStamp::getStamp() const
{
  if (stampCodeTime == Time)
    return stampCode;
  
  stampCodeTime = Time;
  __int64 ft64 = (__int64(Time) + minYearConstant * 365 * 24 * timeConstSecPerHour) * 10000000;
  time_t t = static_cast<time_t>(ft64 / 10000000LL) - 11644473600LL;
  std::tm st = {};
#ifdef _WIN32
  gmtime_s(&st, &t);
#else
  gmtime_r(&t, &st);
#endif
  char bf[64];
  snprintf(bf, 32, "%d%02d%02d%02d%02d%02d", (st.tm_year + 1900), (st.tm_mon + 1), st.tm_mday, st.tm_hour, st.tm_min, st.tm_sec);
  stampCode = bf;

  return stampCode;
}

const string &TimeStamp::getStamp(const string &sqlStampIn) const {
  stampCode.resize(8 + 6 + 1);
  int outIx = 0;
  for (char c : sqlStampIn) {
    if (c >= '0' && c <= '9' && outIx < 8 + 6)
      stampCode[outIx++] = c;
  }
  return stampCode;
}

const wstring TimeStamp::getUpdateTime() const {
  __int64 ft64 = (__int64(Time) + minYearConstant * 365 * 24 * timeConstSecPerHour) * 10000000;
  time_t t = static_cast<time_t>(ft64 / 10000000LL) - 11644473600LL;
  std::tm st = {};
#ifdef _WIN32
  gmtime_s(&st, &t);
#else
  gmtime_r(&t, &st);
#endif
  wchar_t bf[32];
  swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%02d:%02d", st.tm_hour, st.tm_min);
  return bf;
}


wstring TimeStamp::getStampString() const
{
  __int64 ft64 = (__int64(Time) + minYearConstant * 365 * 24 * timeConstSecPerHour) * 10000000;
  time_t t = static_cast<time_t>(ft64 / 10000000LL) - 11644473600LL;
  std::tm st = {};
#ifdef _WIN32
  gmtime_s(&st, &t);
#else
  gmtime_r(&t, &st);
#endif

  wchar_t bf[32];
  swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%d-%02d-%02d %02d:%02d:%02d", (st.tm_year + 1900), (st.tm_mon + 1), st.tm_mday, st.tm_hour, st.tm_min, st.tm_sec);

  return bf;
}

string TimeStamp::getStampStringN() const
{
  __int64 ft64 = (__int64(Time) + minYearConstant * 365 * 24 * timeConstSecPerHour) * 10000000;
  time_t t = static_cast<time_t>(ft64 / 10000000LL) - 11644473600LL;
  std::tm st = {};
#ifdef _WIN32
  gmtime_s(&st, &t);
#else
  gmtime_r(&t, &st);
#endif
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
  snprintf(bf, sizeof(bf), "%d-%02d-%02d %02d:%02d:%02d", (st.tm_year + 1900), (st.tm_mon + 1), st.tm_mday, st.tm_hour, st.tm_min, st.tm_sec);

  return bf;
}

void TimeStamp::setStamp(const string &s)
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
  st.tm_mon = parse(ptr, 2, ptr) - 1;
  st.tm_mday = parse(ptr, 2, ptr);
  st.tm_hour = parse(ptr, 2, ptr);
  st.tm_min = parse(ptr, 2, ptr);
  st.tm_sec = parse(ptr, 2, ptr);
#ifdef _WIN32
  time_t t = _mkgmtime(&st);
#else
  time_t t = timegm(&st);
#endif
  __int64 currenttime = (__int64(t) + 11644473600LL) * 10000000LL;

  Time = unsigned((currenttime / 10000000) - minYearConstant * 365 * 24 * timeConstSecPerHour);
}

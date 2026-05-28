#include <gtest/gtest.h>
#include "TimeStamp.h"
#include "time_util.h"
#include "timeconstants.hpp"

#include <cstring>
#include <thread>
#include <chrono>

// -----------------------------------------------------------------------
// timeconstants.hpp
// -----------------------------------------------------------------------
TEST(TimeConstants, Values) {
  EXPECT_EQ(timeUnitsPerSecond, 10);
  EXPECT_EQ(timeConstSecond, 10);
  EXPECT_EQ(timeConstMinute, 600);
  EXPECT_EQ(timeConstHour, 36000);
  EXPECT_EQ(timeConstSecPerMin, 60);
  EXPECT_EQ(timeConstMinPerHour, 60);
  EXPECT_EQ(timeConstSecPerHour, 3600);
}

// -----------------------------------------------------------------------
// meos::util::getThisYear
// -----------------------------------------------------------------------
TEST(TimeUtil, GetThisYearReasonable) {
  int y = meos::util::getThisYear();
  EXPECT_GE(y, 2024);
  EXPECT_LE(y, 2100);
}

TEST(TimeUtil, GetThisYearStable) {
  int y1 = meos::util::getThisYear();
  int y2 = meos::util::getThisYear();
  EXPECT_EQ(y1, y2);
}

// -----------------------------------------------------------------------
// TimeStamp construction
// -----------------------------------------------------------------------
TEST(TimeStamp, DefaultTimeIsZero) {
  TimeStamp ts;
  EXPECT_EQ(ts.getModificationTime(), 0u);
}

// -----------------------------------------------------------------------
// TimeStamp::update() / getAge()
// -----------------------------------------------------------------------
TEST(TimeStamp, UpdateSetsNonZeroTime) {
  TimeStamp ts;
  ts.update();
  EXPECT_GT(ts.getModificationTime(), 0u);
}

TEST(TimeStamp, AgeIsNonNegativeAfterUpdate) {
  TimeStamp ts;
  ts.update();
  EXPECT_GE(ts.getAge(), 0);
}

TEST(TimeStamp, AgeSmallerAfterLaterUpdate) {
  TimeStamp ts;
  ts.update();
  int age1 = ts.getAge();
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  int age2 = ts.getAge();
  EXPECT_GE(age2, age1);
}

// -----------------------------------------------------------------------
// TimeStamp::update(ts) — merges to max
// -----------------------------------------------------------------------
TEST(TimeStamp, UpdateFromOtherTakesMax) {
  TimeStamp a, b;
  a.update();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  b.update();

  TimeStamp c;
  c.update(a);  // c = a
  EXPECT_EQ(c.getModificationTime(), a.getModificationTime());

  c.update(b);  // c should be max(a,b) = b
  EXPECT_EQ(c.getModificationTime(), b.getModificationTime());

  // Merging an older stamp should not decrease
  c.update(a);
  EXPECT_EQ(c.getModificationTime(), b.getModificationTime());
}

// -----------------------------------------------------------------------
// TimeStamp::setStamp / getStamp roundtrip
// -----------------------------------------------------------------------
TEST(TimeStamp, SetGetStampRoundtrip) {
  TimeStamp ts;
  ts.setStamp("20231015143000");
  std::string s = ts.getStamp();
  EXPECT_EQ(s, "20231015143000");
}

TEST(TimeStamp, SetStampWithDashes) {
  TimeStamp ts;
  ts.setStamp("2023-10-15 14:30:00");
  std::string s = ts.getStamp();
  EXPECT_EQ(s, "20231015143000");
}

TEST(TimeStamp, SetStampTooShortIgnored) {
  TimeStamp ts;
  ts.setStamp("2023101");  // less than 14 chars
  EXPECT_EQ(ts.getModificationTime(), 0u);
}

// -----------------------------------------------------------------------
// TimeStamp::getStamp() — year clamping
// -----------------------------------------------------------------------
TEST(TimeStamp, GetStampClampsOldYear) {
  // Time == 0 corresponds to epoch 2014-01-01 minus offset, which may decode
  // to a very old year. getStamp() must clamp it to [2009, thisYear].
  TimeStamp ts;  // Time = 0
  std::string s = ts.getStamp();
  ASSERT_EQ(s.size(), 14u);
  int year = std::stoi(s.substr(0, 4));
  int maxY = meos::util::getThisYear();
  // Since time=0 → decoded year is before 2009, clamp should produce thisYear.
  EXPECT_GE(year, 2009);
  EXPECT_LE(year, maxY);
}

TEST(TimeStamp, GetStampReasonableYear) {
  TimeStamp ts;
  ts.update();
  std::string s = ts.getStamp();
  ASSERT_EQ(s.size(), 14u);
  int year = std::stoi(s.substr(0, 4));
  int maxY = meos::util::getThisYear();
  EXPECT_GE(year, 2009);
  EXPECT_LE(year, maxY);
}

// -----------------------------------------------------------------------
// TimeStamp::getStamp(sql) — strips non-digits, resizes to actual count
// -----------------------------------------------------------------------
TEST(TimeStamp, GetStampSqlStripsNonDigits) {
  TimeStamp ts;
  std::string result = ts.getStamp("2023-10-15 14:30:00");
  EXPECT_EQ(result, "20231015143000");
}

TEST(TimeStamp, GetStampSqlExactDigits) {
  TimeStamp ts;
  std::string result = ts.getStamp("20231015143000");
  EXPECT_EQ(result, "20231015143000");
  EXPECT_EQ(result.size(), 14u);
}

TEST(TimeStamp, GetStampSqlShortInput) {
  TimeStamp ts;
  std::string result = ts.getStamp("2023");
  EXPECT_EQ(result, "2023");
  EXPECT_EQ(result.size(), 4u);
}

TEST(TimeStamp, GetStampSqlEmptyInput) {
  TimeStamp ts;
  std::string result = ts.getStamp("");
  EXPECT_EQ(result, "");
  EXPECT_EQ(result.size(), 0u);
}

TEST(TimeStamp, GetStampSqlNoTrailingGarbage) {
  TimeStamp ts;
  // Only 6 digits in input — result must be exactly 6 chars (no trailing garbage)
  std::string result = ts.getStamp("abc123def");
  EXPECT_EQ(result, "123");
  EXPECT_EQ(result.size(), 3u);
}

// -----------------------------------------------------------------------
// TimeStamp::getStampString / getStampStringN
// -----------------------------------------------------------------------
TEST(TimeStamp, GetStampStringFormat) {
  TimeStamp ts;
  ts.setStamp("20231015143000");
  std::wstring ws = ts.getStampString();
  // Expected: L"2023-10-15 14:30:00"
  EXPECT_EQ(ws, L"2023-10-15 14:30:00");
}

TEST(TimeStamp, GetStampStringNFormat) {
  TimeStamp ts;
  ts.setStamp("20231015143000");
  std::string s = ts.getStampStringN();
  EXPECT_EQ(s, "2023-10-15 14:30:00");
}

TEST(TimeStamp, GetStampStringNClampsOldYear) {
  TimeStamp ts;  // time=0 → very old year
  std::string s = ts.getStampStringN();
  int year = std::stoi(s.substr(0, 4));
  int maxY = meos::util::getThisYear();
  EXPECT_GE(year, 2009);
  EXPECT_LE(year, maxY);
}

// -----------------------------------------------------------------------
// TimeStamp::getUpdateTime
// -----------------------------------------------------------------------
TEST(TimeStamp, GetUpdateTimeFormat) {
  TimeStamp ts;
  ts.setStamp("20231015143000");
  std::wstring wt = ts.getUpdateTime();
  EXPECT_EQ(wt, L"14:30");
}

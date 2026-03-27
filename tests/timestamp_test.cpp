#include <gtest/gtest.h>
#include "TimeStamp.h"
#include "timeconstants.hpp"
#include <string>
#include <thread>
#include <chrono>

// timeconstants.hpp
TEST(TimeConstants, Values) {
  EXPECT_EQ(timeUnitsPerSecond, 10);
  EXPECT_EQ(timeConstSecond, 10);
  EXPECT_EQ(timeConstMinute, 600);
  EXPECT_EQ(timeConstHour, 36000);
  EXPECT_EQ(timeConstSecPerMin, 60);
  EXPECT_EQ(timeConstMinPerHour, 60);
  EXPECT_EQ(timeConstSecPerHour, 3600);
}

// Default constructed TimeStamp has Time=0
TEST(TimeStamp, DefaultConstruct) {
  TimeStamp ts;
  EXPECT_EQ(ts.getModificationTime(), 0u);
}

// update() sets a non-zero Time
TEST(TimeStamp, UpdateSetsTime) {
  TimeStamp ts;
  ts.update();
  EXPECT_GT(ts.getModificationTime(), 0u);
}

// setStamp / getStamp round-trip (compact 14-digit format)
TEST(TimeStamp, SetStampGetStamp_Compact) {
  TimeStamp ts;
  ts.setStamp("20240315143022");
  const std::string &s = ts.getStamp();
  EXPECT_EQ(s, "20240315143022");
}

// setStamp with ISO-style format (dashes / colons / spaces)
TEST(TimeStamp, SetStampGetStamp_ISO) {
  TimeStamp ts;
  ts.setStamp("2024-03-15 14:30:22");
  const std::string &s = ts.getStamp();
  EXPECT_EQ(s, "20240315143022");
}

// setStamp with too-short input must not crash (no-op)
TEST(TimeStamp, SetStamp_TooShort) {
  TimeStamp ts;
  ts.setStamp("2024");
  EXPECT_EQ(ts.getModificationTime(), 0u);
}

// getStamp(sqlStamp) strips non-digit characters
TEST(TimeStamp, GetStampFromSQL) {
  TimeStamp ts;
  const std::string sql = "2024-03-15 14:30:22";
  const std::string &s = ts.getStamp(sql);
  EXPECT_EQ(s, "20240315143022");
}

// getStampString returns wstring in "YYYY-MM-DD HH:MM:SS" format
TEST(TimeStamp, GetStampString) {
  TimeStamp ts;
  ts.setStamp("20240315143022");
  std::wstring ws = ts.getStampString();
  EXPECT_EQ(ws, L"2024-03-15 14:30:22");
}

// getStampStringN returns narrow string in same format
TEST(TimeStamp, GetStampStringN) {
  TimeStamp ts;
  ts.setStamp("20240315143022");
  std::string s = ts.getStampStringN();
  EXPECT_EQ(s, "2024-03-15 14:30:22");
}

// getUpdateTime returns HH:MM
TEST(TimeStamp, GetUpdateTime) {
  TimeStamp ts;
  ts.setStamp("20240315143022");
  std::wstring wt = ts.getUpdateTime();
  EXPECT_EQ(wt, L"14:30");
}

// update(other) takes max of the two timestamps
TEST(TimeStamp, UpdateMax) {
  TimeStamp ts1, ts2;
  ts1.setStamp("20230101000000");
  ts2.setStamp("20240101000000");
  ts1.update(ts2);
  EXPECT_EQ(ts1.getStamp(), "20240101000000");

  TimeStamp ts3, ts4;
  ts3.setStamp("20250101000000");
  ts4.setStamp("20240101000000");
  ts3.update(ts4);
  EXPECT_EQ(ts3.getStamp(), "20250101000000");
}

// getAge returns seconds since the stamp was set (approximately)
TEST(TimeStamp, GetAgeApprox) {
  TimeStamp ts;
  ts.update();
  int age = ts.getAge();
  EXPECT_GE(age, 0);
  EXPECT_LT(age, 5); // should be less than 5 seconds
}

// Stamp caching: calling getStamp() twice returns same reference without recomputing
TEST(TimeStamp, StampCaching) {
  TimeStamp ts;
  ts.setStamp("20240315143022");
  const std::string *p1 = &ts.getStamp();
  const std::string *p2 = &ts.getStamp();
  EXPECT_EQ(p1, p2); // same address — cached
}

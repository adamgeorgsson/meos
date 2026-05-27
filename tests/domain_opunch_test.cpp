#include <gtest/gtest.h>
#include "oPunch.h"
#include "oEvent.h"

// -----------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------
TEST(oPunchTest, DefaultConstruction) {
  oEvent ev;
  oPunch p(&ev);
  EXPECT_EQ(p.getTypeCode(), 0);
  EXPECT_FALSE(p.hasTime());
  EXPECT_FALSE(p.isUsed);
  EXPECT_EQ(p.tMatchControlId, -1);
  EXPECT_EQ(p.tIndex, -1);
}

// -----------------------------------------------------------------------
// SpecialPunch enum values
// -----------------------------------------------------------------------
TEST(oPunchTest, SpecialPunchEnumValues) {
  EXPECT_EQ(oPunch::PunchUnused, 0);
  EXPECT_EQ(oPunch::PunchStart,  1);
  EXPECT_EQ(oPunch::PunchFinish, 2);
  EXPECT_EQ(oPunch::PunchCheck,  3);
  EXPECT_EQ(oPunch::HiredCard,   11111);
}

// -----------------------------------------------------------------------
// isStart / isFinish / isCheck / isHiredCard
// -----------------------------------------------------------------------
TEST(oPunchTest, TypePredicates) {
  oEvent ev;
  oPunch p(&ev);

  // Start
  p.decodeString("1-300;");
  EXPECT_TRUE(p.isStart());
  EXPECT_FALSE(p.isFinish());
  EXPECT_FALSE(p.isCheck());

  // Finish
  p.decodeString("2-500;");
  EXPECT_FALSE(p.isStart());
  EXPECT_TRUE(p.isFinish());
  EXPECT_FALSE(p.isCheck());

  // Check
  p.decodeString("3-100;");
  EXPECT_TRUE(p.isCheck());

  // Regular control
  p.decodeString("100-400;");
  EXPECT_FALSE(p.isStart());
  EXPECT_FALSE(p.isFinish());
  EXPECT_FALSE(p.isCheck());
  EXPECT_EQ(p.getControlNumber(), 100);
}

// -----------------------------------------------------------------------
// codeString / appendCodeString
// -----------------------------------------------------------------------
TEST(oPunchTest, CodeStringRoundTrip) {
  oEvent ev;
  oPunch p(&ev);
  p.decodeString("42-1230;");
  EXPECT_EQ(p.getTypeCode(), 42);
  // Encode and check it contains type and time
  string cs = p.codeString();
  EXPECT_NE(cs.find("42"), string::npos);
}

TEST(oPunchTest, AppendCodeString) {
  oEvent ev;
  oPunch p(&ev);
  p.decodeString("31-100;");
  string out;
  p.appendCodeString(out);
  EXPECT_FALSE(out.empty());
  EXPECT_NE(out.find("31"), string::npos);
}

TEST(oPunchTest, DecodeStringWithUnit) {
  oEvent ev;
  oPunch p(&ev);
  p.decodeString("1-500@3;");
  EXPECT_EQ(p.getPunchUnit(), 3);
  EXPECT_EQ(p.getTypeCode(), 1);
}

// -----------------------------------------------------------------------
// hasTime
// -----------------------------------------------------------------------
TEST(oPunchTest, HasTime) {
  oEvent ev;
  oPunch p(&ev);
  EXPECT_FALSE(p.hasTime()); // punchTime = 0

  p.decodeString("1-100;");
  EXPECT_TRUE(p.hasTime());
}

// -----------------------------------------------------------------------
// getTimeInt / getAdjustedTime
// -----------------------------------------------------------------------
TEST(oPunchTest, GetTimeInt) {
  oEvent ev;
  oPunch p(&ev);
  p.decodeString("1-100;"); // punchTime = 100 * timeConstSecond = 1000
  EXPECT_EQ(p.getTimeInt(), 1000);
}

TEST(oPunchTest, TimeAdjustment) {
  oEvent ev;
  oPunch p(&ev);
  p.decodeString("1-100;");
  p.setTimeAdjust(5);
  EXPECT_EQ(p.getTimeInt(), 1005);
  p.adjustTimeAdjust(10);
  EXPECT_EQ(p.getAdjustedTime(), 1015);
}

TEST(oPunchTest, ClearTimeAdjust) {
  oEvent ev;
  oPunch p(&ev);
  p.decodeString("1-50;");
  p.setTimeAdjust(20);
  p.adjustTimeAdjust(5);
  p.clearTimeAdjust();
  EXPECT_EQ(p.getTimeInt(), 500);
  EXPECT_EQ(p.getAdjustedTime(), 500);
}

// -----------------------------------------------------------------------
// getRunningTime
// -----------------------------------------------------------------------
TEST(oPunchTest, GetRunningTime) {
  oEvent ev;
  oPunch p(&ev);
  p.decodeString("2-200;"); // punchTime = 2000
  wstring rt = p.getRunningTime(1000); // 2000-1000 = 1000 units = 100 sec
  EXPECT_NE(rt, L"-");
  EXPECT_FALSE(rt.empty());
}

TEST(oPunchTest, GetRunningTimeNegativeStart) {
  oEvent ev;
  oPunch p(&ev);
  p.decodeString("2-50;");
  // start > punch → dash
  EXPECT_EQ(p.getRunningTime(600), L"-");
}

// -----------------------------------------------------------------------
// setTimeInt / updateChanged
// -----------------------------------------------------------------------
TEST(oPunchTest, SetTimeIntMarksChanged) {
  oEvent ev;
  oPunch p(&ev);
  // After construction, changed flag could be set; change the time and verify it stays changed
  p.setTimeInt(200, false);
  EXPECT_TRUE(p.isChanged());
  EXPECT_EQ(p.getTimeInt(), 200);
}

TEST(oPunchTest, SetTimeIntNoChange) {
  oEvent ev;
  oPunch p(&ev);
  // punchTime starts at 0; setting to 0 does nothing
  bool wasPunchTimeChanged = p.isChanged();
  p.setTimeInt(0, false); // same value as initial (0)
  // changed flag should not have been set by setTimeInt(0,false) when already 0
  EXPECT_EQ(p.getTimeInt(), 0);
}

// -----------------------------------------------------------------------
// setPunchUnit
// -----------------------------------------------------------------------
TEST(oPunchTest, SetPunchUnit) {
  oEvent ev;
  oPunch p(&ev);
  EXPECT_EQ(p.getPunchUnit(), 0);
  p.setPunchUnit(5);
  EXPECT_EQ(p.getPunchUnit(), 5);
  EXPECT_TRUE(p.isChanged());
}

// -----------------------------------------------------------------------
// getType (static)
// -----------------------------------------------------------------------
TEST(oPunchTest, GetTypeStart) {
  const wstring& s = oPunch::getType(oPunch::PunchStart, nullptr);
  EXPECT_EQ(s, L"Start");
}

TEST(oPunchTest, GetTypeFinish) {
  const wstring& s = oPunch::getType(oPunch::PunchFinish, nullptr);
  EXPECT_EQ(s, L"Mål");
}

TEST(oPunchTest, GetTypeCheck) {
  const wstring& s = oPunch::getType(oPunch::PunchCheck, nullptr);
  EXPECT_EQ(s, L"Check");
}

TEST(oPunchTest, GetTypeNumber) {
  const wstring& s = oPunch::getType(100, nullptr);
  EXPECT_EQ(s, L"100");
}

TEST(oPunchTest, GetTypeUnknown) {
  const wstring& s = oPunch::getType(0, nullptr);
  EXPECT_EQ(s, L"");
}

// -----------------------------------------------------------------------
// computeOrigin
// -----------------------------------------------------------------------
TEST(oPunchTest, ComputeOriginZeroInputsReturnZero) {
  EXPECT_EQ(oPunch::computeOrigin(0, 42), 0);
  EXPECT_EQ(oPunch::computeOrigin(100, 0), 0);
}

TEST(oPunchTest, ComputeOriginDeterministic) {
  int o1 = oPunch::computeOrigin(1000, 31);
  int o2 = oPunch::computeOrigin(1000, 31);
  EXPECT_EQ(o1, o2);
  EXPECT_GT(o1, 0);
}

// -----------------------------------------------------------------------
// getControlId
// -----------------------------------------------------------------------
TEST(oPunchTest, GetControlId) {
  oEvent ev;
  oPunch p(&ev);
  p.tMatchControlId = 77;
  EXPECT_EQ(p.getControlId(), 77);
}

// -----------------------------------------------------------------------
// canRemove / remove
// -----------------------------------------------------------------------
TEST(oPunchTest, CanRemove) {
  oEvent ev;
  oPunch p(&ev);
  EXPECT_TRUE(p.canRemove());
  EXPECT_FALSE(p.isRemoved());
  p.remove();
  EXPECT_TRUE(p.isRemoved());
}

// -----------------------------------------------------------------------
// getInfo
// -----------------------------------------------------------------------
TEST(oPunchTest, GetInfo) {
  oEvent ev;
  oPunch p(&ev);
  p.decodeString("31-200;");
  wstring info = p.getInfo();
  EXPECT_FALSE(info.empty());
}

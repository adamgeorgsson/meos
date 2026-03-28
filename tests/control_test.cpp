// control_test.cpp — Unit tests for oControl and oPunch (US-003b).
#include <gtest/gtest.h>
#include <locale>
#include <clocale>

#define protected public
#define private   public

#include "oControl.h"
#include "oPunch.h"
#include "oEvent.h"

using std::string;
using std::wstring;
using std::vector;

// ── Test fixture ──────────────────────────────────────────────────────────────

class ControlTest : public ::testing::Test {
protected:
  oEvent oe;

  void SetUp() override {
    std::setlocale(LC_ALL, "C.UTF-8");
  }
};

// ─────────────────────────── oControl basics ─────────────────────────────────

TEST_F(ControlTest, Control_DefaultState)
{
  oControl c(&oe);
  EXPECT_EQ(c.getId(), 0);
  EXPECT_EQ(c.getNNumbers(), 0);
  EXPECT_EQ(c.getStatus(), oControl::ControlStatus::StatusOK);
  EXPECT_FALSE(c.hasName());
}

TEST_F(ControlTest, Control_ConstructWithId)
{
  oControl c(&oe, 42);
  EXPECT_EQ(c.getId(), 42);
}

TEST_F(ControlTest, Control_SetNumbers)
{
  oControl c(&oe, 31);
  c.set(31, 100, L"");
  EXPECT_EQ(c.getNNumbers(), 1);
  EXPECT_EQ(c.getFirstNumber(), 100);
}

TEST_F(ControlTest, Control_SetNumbersFromWstring)
{
  oControl c(&oe, 1);
  c.set(1, 0, L"");
  bool ok = c.setNumbers(L"101;102;103");
  EXPECT_TRUE(ok);
  EXPECT_EQ(c.getNNumbers(), 3);
  EXPECT_EQ(c.getFirstNumber(), 101);
}

TEST_F(ControlTest, Control_SetNumbersInvalid)
{
  oControl c(&oe, 7);
  c.set(7, 7, L"");
  // Empty string should fail and leave numbers unchanged
  bool ok = c.setNumbers(L"");
  EXPECT_FALSE(ok);
  EXPECT_EQ(c.getNNumbers(), 1);
  EXPECT_EQ(c.getFirstNumber(), 7);
}

TEST_F(ControlTest, Control_CodeNumbers_Separator)
{
  oControl c(&oe, 1);
  c.setNumbers(L"50;60;70");
  wstring s = c.codeNumbers(';');
  EXPECT_EQ(s, L"50;60;70");
}

TEST_F(ControlTest, Control_GetNumbers_Vector)
{
  oControl c(&oe, 1);
  c.setNumbers(L"10;20;30");
  vector<int> nums;
  c.getNumbers(nums);
  ASSERT_EQ(nums.size(), 3u);
  EXPECT_EQ(nums[0], 10);
  EXPECT_EQ(nums[1], 20);
  EXPECT_EQ(nums[2], 30);
}

TEST_F(ControlTest, Control_SetGetStatus)
{
  oControl c(&oe);
  c.setStatus(oControl::ControlStatus::StatusBad);
  EXPECT_EQ(c.getStatus(), oControl::ControlStatus::StatusBad);
}

TEST_F(ControlTest, Control_StatusNotChanged_NoUpdate)
{
  oControl c(&oe);
  c.setStatus(oControl::ControlStatus::StatusOK);
  bool wasDirty = c.isChanged();
  EXPECT_FALSE(wasDirty);  // setting same status doesn't mark as changed
}

TEST_F(ControlTest, Control_SetGetName)
{
  oControl c(&oe, 5);
  c.setName(L"TestControl");
  EXPECT_EQ(c.getName(), L"TestControl");
  EXPECT_TRUE(c.hasName());
}

TEST_F(ControlTest, Control_DefaultName_WhenEmpty)
{
  oControl c(&oe, 42);
  // When Name is empty, getName returns "[id]"
  EXPECT_EQ(c.getName(), L"[42]");
  EXPECT_FALSE(c.hasName());
}

TEST_F(ControlTest, Control_IdS_WithName)
{
  oControl c(&oe, 99);
  c.setName(L"MyCtrl");
  EXPECT_EQ(c.getIdS(), L"MyCtrl");
}

TEST_F(ControlTest, Control_IdS_WithoutName)
{
  oControl c(&oe, 99);
  EXPECT_EQ(c.getIdS(), L"99");
}

// ── Punch-checking logic ──────────────────────────────────────────────────────

TEST_F(ControlTest, Control_CheckSingleNumber)
{
  oControl c(&oe, 1);
  c.setNumbers(L"131");
  c.startCheckControl();
  EXPECT_FALSE(c.controlCompleted(false));  // not yet checked
  EXPECT_TRUE(c.hasNumber(131));
  EXPECT_TRUE(c.controlCompleted(false));   // now checked
}

TEST_F(ControlTest, Control_WrongNumber_NotChecked)
{
  oControl c(&oe, 1);
  c.setNumbers(L"131");
  c.startCheckControl();
  EXPECT_FALSE(c.hasNumber(999));
  EXPECT_FALSE(c.controlCompleted(false));
}

TEST_F(ControlTest, Control_MultipleStatus_AllRequired)
{
  oControl c(&oe, 1);
  c.setNumbers(L"131;132");
  c.setStatus(oControl::ControlStatus::StatusMultiple);
  c.startCheckControl();
  EXPECT_FALSE(c.controlCompleted(false));
  c.hasNumber(131);
  EXPECT_FALSE(c.controlCompleted(false));  // still need 132
  c.hasNumber(132);
  EXPECT_TRUE(c.controlCompleted(false));
}

TEST_F(ControlTest, Control_HasNumberUnchecked)
{
  oControl c(&oe, 1);
  c.setNumbers(L"50");
  c.startCheckControl();
  EXPECT_TRUE(c.hasNumberUnchecked(50));   // first call: unchecked → checks it
  EXPECT_FALSE(c.hasNumberUnchecked(50));  // second call: already checked
}

TEST_F(ControlTest, Control_UncheckNumber)
{
  oControl c(&oe, 1);
  c.setNumbers(L"50");
  c.startCheckControl();
  c.hasNumber(50);
  EXPECT_TRUE(c.controlCompleted(false));
  c.uncheckNumber(50);
  EXPECT_FALSE(c.controlCompleted(false));
}

TEST_F(ControlTest, Control_GetMissingNumber)
{
  oControl c(&oe, 1);
  c.setNumbers(L"77");
  c.startCheckControl();
  EXPECT_EQ(c.getMissingNumber(), 77);
}

TEST_F(ControlTest, Control_AddUncheckedPunches)
{
  oControl c(&oe, 1);
  c.setNumbers(L"77;88");
  c.setStatus(oControl::ControlStatus::StatusMultiple);
  c.startCheckControl();
  vector<pair<int, pControl>> mp;
  c.addUncheckedPunches(mp, false);
  ASSERT_EQ(mp.size(), 2u);
  EXPECT_EQ(mp[0].first, 77);
  EXPECT_EQ(mp[1].first, 88);
}

// ── Course-control ID encoding ────────────────────────────────────────────────

TEST_F(ControlTest, Control_CourseControlId_RoundTrip)
{
  int cc = oControl::getCourseControlIdFromIdIndex(123, 2);
  auto [id, idx] = oControl::getIdIndexFromCourseControlId(cc);
  EXPECT_EQ(id, 123);
  EXPECT_EQ(idx, 2);
}

TEST_F(ControlTest, Control_CourseControlId_Index0)
{
  int cc = oControl::getCourseControlIdFromIdIndex(50, 0);
  EXPECT_EQ(cc, 50);
}

// ── DI field access ───────────────────────────────────────────────────────────

TEST_F(ControlTest, Control_TimeAdjust_DefaultZero)
{
  oControl c(&oe, 1);
  EXPECT_EQ(c.getTimeAdjust(), 0);
}

TEST_F(ControlTest, Control_SetTimeAdjust)
{
  oControl c(&oe, 1);
  c.setTimeAdjust(600);
  EXPECT_EQ(c.getTimeAdjust(), 600);
}

TEST_F(ControlTest, Control_MinTime_NoTiming_AlwaysZero)
{
  oControl c(&oe, 1);
  c.setMinTime(300);
  c.setStatus(oControl::ControlStatus::StatusNoTiming);
  EXPECT_EQ(c.getMinTime(), 0);  // overridden by NoTiming status
}

TEST_F(ControlTest, Control_SetMinTime)
{
  oControl c(&oe, 1);
  c.setMinTime(120);
  EXPECT_EQ(c.getMinTime(), 120);
}

TEST_F(ControlTest, Control_RogainingPoints)
{
  oControl c(&oe, 1);
  c.setRogainingPoints(42);
  EXPECT_EQ(c.getRogainingPoints(), 42);
}

// ── isSpecialControl / isRogaining ────────────────────────────────────────────

TEST_F(ControlTest, Control_IsSpecial)
{
  EXPECT_TRUE(oControl::isSpecialControl(oControl::ControlStatus::StatusStart));
  EXPECT_TRUE(oControl::isSpecialControl(oControl::ControlStatus::StatusFinish));
  EXPECT_TRUE(oControl::isSpecialControl(oControl::ControlStatus::StatusCheck));
  EXPECT_FALSE(oControl::isSpecialControl(oControl::ControlStatus::StatusOK));
}

TEST_F(ControlTest, Control_IsRogaining)
{
  oControl c(&oe, 1);
  c.setStatus(oControl::ControlStatus::StatusRogaining);
  EXPECT_TRUE(c.isRogaining(true));
  EXPECT_FALSE(c.isRogaining(false));  // not rogaining when support disabled
}

// ── Remove / canRemove ────────────────────────────────────────────────────────

TEST_F(ControlTest, Control_CanRemove)
{
  oControl c(&oe, 1);
  // stub always returns false for isControlUsed
  EXPECT_TRUE(c.canRemove());
}

// ── NumMulti ─────────────────────────────────────────────────────────────────

TEST_F(ControlTest, Control_NumMulti_StatusOK)
{
  oControl c(&oe, 1);
  c.setNumbers(L"10;20");
  EXPECT_EQ(c.getNumMulti(), 1);  // only 1 for non-Multiple
}

TEST_F(ControlTest, Control_NumMulti_StatusMultiple)
{
  oControl c(&oe, 1);
  c.setNumbers(L"10;20");
  c.setStatus(oControl::ControlStatus::StatusMultiple);
  EXPECT_EQ(c.getNumMulti(), 2);
}

// ─────────────────────────── oPunch basics ───────────────────────────────────

class PunchTest : public ::testing::Test {
protected:
  oEvent oe;

  void SetUp() override {
    std::setlocale(LC_ALL, "C.UTF-8");
  }
};

TEST_F(PunchTest, Punch_DefaultState)
{
  oPunch p(&oe);
  EXPECT_EQ(p.getTypeCode(), 0);
  EXPECT_FALSE(p.isStart());
  EXPECT_FALSE(p.isFinish());
  EXPECT_FALSE(p.isCheck());
  EXPECT_FALSE(p.hasTime());
  EXPECT_FALSE(p.isUsedInCourse());
}

TEST_F(PunchTest, Punch_AppendCodeDecode_RoundTrip)
{
  // appendCodeString formats in decimal seconds (e.g. "131-3600.0;")
  // decodeString then reconstructs punchTime in time-units (deciseconds)
  oPunch p1(&oe), p2(&oe);
  p1.type = 131;
  p1.punchTime = 36000;  // 3600 seconds in decisecond units
  p1.punchUnit = 0;
  p1.origin = 0;

  string cs;
  p1.appendCodeString(cs);
  p2.decodeString(cs.c_str());
  EXPECT_EQ(p2.getTypeCode(), 131);
  EXPECT_EQ(p2.punchTime, 36000);
}

TEST_F(PunchTest, Punch_AppendCodeString_NoUnit)
{
  oPunch p(&oe);
  p.type = 50;
  p.punchTime = 720;
  string s;
  p.appendCodeString(s);
  // format: "type-time;" for timeConstSecond == 1
  EXPECT_NE(s.find("50-"), string::npos);
  EXPECT_NE(s.find(";"), string::npos);
}

TEST_F(PunchTest, Punch_TypePredicates)
{
  oPunch start(&oe), finish(&oe), check(&oe);
  start.type  = oPunch::PunchStart;
  finish.type = oPunch::PunchFinish;
  check.type  = oPunch::PunchCheck;

  EXPECT_TRUE(start.isStart());
  EXPECT_FALSE(start.isFinish());
  EXPECT_TRUE(finish.isFinish());
  EXPECT_FALSE(finish.isCheck());
  EXPECT_TRUE(check.isCheck());
}

TEST_F(PunchTest, Punch_GetControlNumber_Regular)
{
  oPunch p(&oe);
  p.type = 135;
  EXPECT_EQ(p.getControlNumber(), 135);
}

TEST_F(PunchTest, Punch_GetControlNumber_SpecialReturnsZero)
{
  oPunch p(&oe);
  p.type = oPunch::PunchStart;
  EXPECT_EQ(p.getControlNumber(), 0);
}

TEST_F(PunchTest, Punch_ComputeOrigin_Zero)
{
  // time <= 0 or code <= 0 → 0
  EXPECT_EQ(oPunch::computeOrigin(0, 131), 0);
  EXPECT_EQ(oPunch::computeOrigin(100, 0), 0);
}

TEST_F(PunchTest, Punch_ComputeOrigin_Nonzero)
{
  int o = oPunch::computeOrigin(36000, 131);
  EXPECT_GT(o, 0);
}

TEST_F(PunchTest, Punch_IsOriginal_NoOrigin)
{
  oPunch p(&oe);
  p.type = 131;
  p.punchTime = 36000;
  p.origin = 0;
  EXPECT_FALSE(p.isOriginal());
}

TEST_F(PunchTest, Punch_AdjustedTime_NegativePunchTime)
{
  oPunch p(&oe);
  p.punchTime = -1;
  EXPECT_EQ(p.getAdjustedTime(), -1);
}

TEST_F(PunchTest, Punch_CodeDecodeWithUnit)
{
  oPunch p1(&oe), p2(&oe);
  p1.type = oPunch::PunchFinish;
  p1.punchTime = 1000;
  p1.punchUnit = 3;
  p1.origin = 0;

  string s;
  p1.appendCodeString(s);
  EXPECT_NE(s.find("@3"), string::npos);

  p2.decodeString(s.c_str());
  EXPECT_EQ(p2.getTypeCode(), oPunch::PunchFinish);
  EXPECT_EQ(p2.getPunchUnit(), 3);
}

TEST_F(PunchTest, Punch_CanRemove)
{
  oPunch p(&oe);
  EXPECT_TRUE(p.canRemove());
}

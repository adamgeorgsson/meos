#include <gtest/gtest.h>
#include "oControl.h"
#include "oEvent.h"

// -----------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------

TEST(oControlTest, DefaultConstruction) {
  oEvent ev;
  oControl ctrl(&ev);
  EXPECT_EQ(ctrl.getId(), 0);
  EXPECT_EQ(ctrl.getStatus(), oControl::ControlStatus::StatusOK);
  EXPECT_EQ(ctrl.getNNumbers(), 0);
  EXPECT_FALSE(ctrl.isRemoved());
}

TEST(oControlTest, ConstructionWithId) {
  oEvent ev;
  oControl ctrl(&ev, 42);
  EXPECT_EQ(ctrl.getId(), 42);
}

// -----------------------------------------------------------------------
// SpecialPunch enum accessible via oControl.h
// -----------------------------------------------------------------------

TEST(oControlTest, SpecialPunchEnumAvailable) {
  EXPECT_EQ(SpecialPunch::PunchStart,  oPunch::PunchStart);
  EXPECT_EQ(SpecialPunch::PunchFinish, oPunch::PunchFinish);
  EXPECT_EQ(SpecialPunch::PunchCheck,  oPunch::PunchCheck);
}

// -----------------------------------------------------------------------
// isSpecialControl
// -----------------------------------------------------------------------

TEST(oControlTest, IsSpecialControl) {
  EXPECT_TRUE(oControl::isSpecialControl(oControl::ControlStatus::StatusStart));
  EXPECT_TRUE(oControl::isSpecialControl(oControl::ControlStatus::StatusFinish));
  EXPECT_TRUE(oControl::isSpecialControl(oControl::ControlStatus::StatusCheck));
  EXPECT_FALSE(oControl::isSpecialControl(oControl::ControlStatus::StatusOK));
  EXPECT_FALSE(oControl::isSpecialControl(oControl::ControlStatus::StatusBad));
}

// -----------------------------------------------------------------------
// Status
// -----------------------------------------------------------------------

TEST(oControlTest, SetStatus) {
  oEvent ev;
  oControl ctrl(&ev, 1);
  ctrl.setStatus(oControl::ControlStatus::StatusBad);
  EXPECT_EQ(ctrl.getStatus(), oControl::ControlStatus::StatusBad);
  EXPECT_TRUE(ctrl.isChanged());
}

TEST(oControlTest, SetSameStatusNoChange) {
  oEvent ev;
  oControl ctrl(&ev, 1);
  // Status is already StatusOK; setting again should not set changed
  ctrl.setStatus(oControl::ControlStatus::StatusOK);
  EXPECT_FALSE(ctrl.isChanged());
}

TEST(oControlTest, GetStatusS) {
  oEvent ev;
  oControl ctrl(&ev);
  ctrl.setStatus(oControl::ControlStatus::StatusOK);
  EXPECT_EQ(ctrl.getStatusS(), L"OK");
  ctrl.setStatus(oControl::ControlStatus::StatusBad);
  EXPECT_EQ(ctrl.getStatusS(), L"Trasig");
  ctrl.setStatus(oControl::ControlStatus::StatusFinish);
  EXPECT_EQ(ctrl.getStatusS(), L"Mal");
}

// -----------------------------------------------------------------------
// Name
// -----------------------------------------------------------------------

TEST(oControlTest, DefaultNameIsIdInBrackets) {
  oEvent ev;
  oControl ctrl(&ev, 5);
  EXPECT_EQ(ctrl.getDefaultName(), L"[5]");
  EXPECT_EQ(ctrl.getName(), L"[5]");
}

TEST(oControlTest, SetName) {
  oEvent ev;
  oControl ctrl(&ev, 10);
  ctrl.setName(L"Forest");
  EXPECT_EQ(ctrl.getName(), L"Forest");
  EXPECT_TRUE(ctrl.hasName());
}

TEST(oControlTest, SetNameToDefaultClearsName) {
  oEvent ev;
  oControl ctrl(&ev, 7);
  ctrl.setName(L"Hill");
  EXPECT_TRUE(ctrl.hasName());
  ctrl.setName(ctrl.getDefaultName());
  EXPECT_FALSE(ctrl.hasName());
}

TEST(oControlTest, GetIdSWithName) {
  oEvent ev;
  oControl ctrl(&ev, 3);
  ctrl.setName(L"Pond");
  EXPECT_EQ(ctrl.getIdS(), L"Pond");
}

TEST(oControlTest, GetIdSWithoutName) {
  oEvent ev;
  oControl ctrl(&ev, 99);
  EXPECT_EQ(ctrl.getIdS(), L"99");
}

// -----------------------------------------------------------------------
// Number management
// -----------------------------------------------------------------------

TEST(oControlTest, SetMethod) {
  oEvent ev;
  oControl ctrl(&ev);
  ctrl.set(5, 101, L"North");
  EXPECT_EQ(ctrl.getId(), 5);
  EXPECT_EQ(ctrl.getFirstNumber(), 101);
  EXPECT_EQ(ctrl.getNNumbers(), 1);
  EXPECT_EQ(ctrl.getName(), L"North");
}

TEST(oControlTest, SetNumbers) {
  oEvent ev;
  oControl ctrl(&ev, 1);
  bool ok = ctrl.setNumbers(L"31;32;33");
  EXPECT_TRUE(ok);
  EXPECT_EQ(ctrl.getNNumbers(), 3);
  EXPECT_EQ(ctrl.getFirstNumber(), 31);
  EXPECT_EQ(ctrl.minNumber(), 31);
  EXPECT_EQ(ctrl.maxNumber(), 33);
}

TEST(oControlTest, CodeNumbers) {
  oEvent ev;
  oControl ctrl(&ev, 1);
  ctrl.setNumbers(L"50;60");
  wstring coded = ctrl.codeNumbers(';');
  EXPECT_EQ(coded, L"50;60");
}

TEST(oControlTest, HasNumber) {
  oEvent ev;
  oControl ctrl(&ev, 1);
  ctrl.setNumbers(L"100;200");
  EXPECT_TRUE(ctrl.hasNumber(100));
  EXPECT_FALSE(ctrl.hasNumber(300));
}

TEST(oControlTest, GetNumbers) {
  oEvent ev;
  oControl ctrl(&ev, 1);
  ctrl.setNumbers(L"10;20;30");
  vector<int> nums;
  ctrl.getNumbers(nums);
  ASSERT_EQ(nums.size(), 3u);
  EXPECT_EQ(nums[0], 10);
  EXPECT_EQ(nums[1], 20);
  EXPECT_EQ(nums[2], 30);
}

// -----------------------------------------------------------------------
// Punch checking logic
// -----------------------------------------------------------------------

TEST(oControlTest, StartCheckAndControlCompleted) {
  oEvent ev;
  oControl ctrl(&ev, 1);
  ctrl.setNumbers(L"55");
  ctrl.startCheckControl();
  EXPECT_FALSE(ctrl.controlCompleted(false));
  ctrl.hasNumber(55);
  EXPECT_TRUE(ctrl.controlCompleted(false));
}

TEST(oControlTest, AddUncheckedPunches) {
  oEvent ev;
  oControl ctrl(&ev, 1);
  ctrl.setNumbers(L"77");
  ctrl.startCheckControl();
  vector<pair<int, oControl*>> punches;
  ctrl.addUncheckedPunches(punches, false);
  ASSERT_EQ(punches.size(), 1u);
  EXPECT_EQ(punches[0].first, 77);
}

TEST(oControlTest, MultipleControlNeedsAllPunches) {
  oEvent ev;
  oControl ctrl(&ev, 1);
  ctrl.setNumbers(L"11;22");
  ctrl.setStatus(oControl::ControlStatus::StatusMultiple);
  ctrl.startCheckControl();
  ctrl.hasNumber(11);
  EXPECT_FALSE(ctrl.controlCompleted(false)); // 22 still missing
  ctrl.hasNumber(22);
  EXPECT_TRUE(ctrl.controlCompleted(false));
}

TEST(oControlTest, UncheckedNumberAndMissingNumber) {
  oEvent ev;
  oControl ctrl(&ev, 1);
  ctrl.setNumbers(L"33;44");
  ctrl.setStatus(oControl::ControlStatus::StatusMultiple);
  ctrl.startCheckControl();
  ctrl.hasNumber(33);
  EXPECT_EQ(ctrl.getMissingNumber(), 44);
}

// -----------------------------------------------------------------------
// Time adjust / min time / rogaining
// -----------------------------------------------------------------------

TEST(oControlTest, TimeAdjustRoundTrip) {
  oEvent ev;
  oControl ctrl(&ev, 1);
  ctrl.setTimeAdjust(30000);  // 30 seconds in ms
  EXPECT_EQ(ctrl.getTimeAdjust(), 30000);
}

TEST(oControlTest, MinTimeRoundTrip) {
  oEvent ev;
  oControl ctrl(&ev, 1);
  ctrl.setMinTime(60000);  // 60 seconds
  EXPECT_EQ(ctrl.getMinTime(), 60000);
}

TEST(oControlTest, RogainingPoints) {
  oEvent ev;
  oControl ctrl(&ev, 1);
  ctrl.setRogainingPoints(10);
  EXPECT_EQ(ctrl.getRogainingPoints(), 10);
}

// -----------------------------------------------------------------------
// CourseControlId encoding
// -----------------------------------------------------------------------

TEST(oControlTest, CourseControlIdEncoding) {
  int encoded = oControl::getCourseControlIdFromIdIndex(42, 3);
  auto [id, idx] = oControl::getIdIndexFromCourseControlId(encoded);
  EXPECT_EQ(id, 42);
  EXPECT_EQ(idx, 3);
}

// -----------------------------------------------------------------------
// Radio flag
// -----------------------------------------------------------------------

TEST(oControlTest, RadioFlag) {
  oEvent ev;
  oControl ctrl(&ev, 1);
  ctrl.setRadio(true);
  EXPECT_TRUE(ctrl.isValidRadio());
  ctrl.setRadio(false);
  EXPECT_FALSE(ctrl.isValidRadio());
}

// -----------------------------------------------------------------------
// getString / getLongString
// -----------------------------------------------------------------------

TEST(oControlTest, GetStringBasic) {
  oEvent ev;
  oControl ctrl(&ev, 1);
  ctrl.setNumbers(L"31");
  wstring s = ctrl.getString();
  EXPECT_FALSE(s.empty());
  EXPECT_NE(s.find(L"31"), wstring::npos);
}

TEST(oControlTest, GetNumMulti) {
  oEvent ev;
  oControl ctrl(&ev, 1);
  ctrl.setNumbers(L"10;20;30");
  ctrl.setStatus(oControl::ControlStatus::StatusMultiple);
  EXPECT_EQ(ctrl.getNumMulti(), 3);
  ctrl.setStatus(oControl::ControlStatus::StatusOK);
  EXPECT_EQ(ctrl.getNumMulti(), 1);
}

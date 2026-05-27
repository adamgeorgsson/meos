#include <gtest/gtest.h>
#include "oCourse.h"
#include "oControl.h"
#include "oEvent.h"

// Helper to add a control with a code number to the event and return it.
static oControl* makeControl(oEvent& oe, int id, int code) {
  oControl* c = oe.getControl(id, true);
  c->set(id, code, L"");
  return c;
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST(oCourse, Construction_DefaultId) {
  oEvent oe;
  oCourse c(&oe);
  EXPECT_GT(c.getId(), 0);
  EXPECT_FALSE(c.isRemoved());
  EXPECT_FALSE(c.isChanged());
}

TEST(oCourse, Construction_ExplicitId) {
  oEvent oe;
  oCourse c(&oe, 42);
  EXPECT_EQ(42, c.getId());
}

TEST(oCourse, Construction_EmptyName) {
  oEvent oe;
  oCourse c(&oe, 1);
  EXPECT_EQ(L"", c.getName());
  EXPECT_EQ(0, c.getLength());
}

TEST(oCourse, GetInfo) {
  oEvent oe;
  oCourse c(&oe, 1);
  c.setName(L"Röd");
  EXPECT_EQ(L"Bana Röd", c.getInfo());
}

// ---------------------------------------------------------------------------
// Name / length
// ---------------------------------------------------------------------------

TEST(oCourse, SetName_ChangesFlag) {
  oEvent oe;
  oCourse c(&oe, 1);
  c.setName(L"Blå");
  EXPECT_EQ(L"Blå", c.getName());
  EXPECT_TRUE(c.isChanged());
}

TEST(oCourse, SetName_SameNoChange) {
  oEvent oe;
  oCourse c1(&oe, 1);
  c1.setName(L"Test");
  // fresh course — not yet changed:
  oCourse c2(&oe, 2);
  c2.setName(L"Test");
  bool wasChanged = c2.isChanged();
  c2.setName(L"Test"); // same again
  EXPECT_EQ(L"Test", c2.getName());
  EXPECT_EQ(wasChanged, c2.isChanged()); // flag stays the same
}

TEST(oCourse, SetLength) {
  oEvent oe;
  oCourse c(&oe, 1);
  c.setLength(5000);
  EXPECT_EQ(5000, c.getLength());
}

TEST(oCourse, SetLength_Clamped) {
  oEvent oe;
  oCourse c(&oe, 1);
  c.setLength(-1);
  EXPECT_EQ(0, c.getLength());
  c.setLength(2000000);
  EXPECT_EQ(0, c.getLength());
}

TEST(oCourse, GetLengthS) {
  oEvent oe;
  oCourse c(&oe, 1);
  c.setLength(3400);
  EXPECT_EQ(L"3400", c.getLengthS());
}

// ---------------------------------------------------------------------------
// operator<
// ---------------------------------------------------------------------------

TEST(oCourse, OperatorLess) {
  oEvent oe;
  oCourse a(&oe, 1);
  oCourse b(&oe, 2);
  a.setName(L"Alpha");
  b.setName(L"Beta");
  EXPECT_TRUE(a < b);
  EXPECT_FALSE(b < a);
}

// ---------------------------------------------------------------------------
// Control management
// ---------------------------------------------------------------------------

TEST(oCourse, AddControl_Count) {
  oEvent oe;
  oCourse c(&oe, 1);
  makeControl(oe, 10, 31);
  makeControl(oe, 11, 32);
  c.addControl(10);
  c.addControl(11);
  EXPECT_EQ(2, c.nControls());
  EXPECT_EQ(2, c.getNumControls());
}

TEST(oCourse, GetControl_ByIndex) {
  oEvent oe;
  oCourse c(&oe, 1);
  auto* ctrl = makeControl(oe, 10, 31);
  c.addControl(10);
  EXPECT_EQ(ctrl, c.getControl(0));
  EXPECT_EQ(nullptr, c.getControl(1));
  EXPECT_EQ(nullptr, c.getControl(-1));
}

TEST(oCourse, GetControls_Vector) {
  oEvent oe;
  oCourse c(&oe, 1);
  auto* ctrl1 = makeControl(oe, 10, 31);
  auto* ctrl2 = makeControl(oe, 11, 32);
  c.addControl(10);
  c.addControl(11);
  std::vector<oControl*> v;
  c.getControls(v);
  ASSERT_EQ(2u, v.size());
  EXPECT_EQ(ctrl1, v[0]);
  EXPECT_EQ(ctrl2, v[1]);
}

TEST(oCourse, GetControlNumbers) {
  oEvent oe;
  oCourse c(&oe, 1);
  makeControl(oe, 10, 31);
  makeControl(oe, 11, 32);
  c.addControl(10);
  c.addControl(11);
  auto nums = c.getControlNumbers();
  ASSERT_EQ(2u, nums.size());
  EXPECT_EQ(31, nums[0]);
  EXPECT_EQ(32, nums[1]);
}

TEST(oCourse, GetControlsString) {
  oEvent oe;
  oCourse c(&oe, 1);
  makeControl(oe, 10, 31);
  makeControl(oe, 11, 32);
  c.addControl(10);
  c.addControl(11);
  std::string s = c.getControls();
  EXPECT_EQ("10;11;", s);
}

TEST(oCourse, SplitControls) {
  std::vector<int> v;
  oCourse::splitControls("10;11;12", v);
  ASSERT_EQ(3u, v.size());
  EXPECT_EQ(10, v[0]);
  EXPECT_EQ(11, v[1]);
  EXPECT_EQ(12, v[2]);
}

TEST(oCourse, SplitControls_Commas) {
  std::vector<int> v;
  oCourse::splitControls("10,20,30", v);
  ASSERT_EQ(3u, v.size());
}

TEST(oCourse, ImportControls_Basic) {
  oEvent oe;
  oCourse c(&oe, 1);
  makeControl(oe, 10, 31);
  makeControl(oe, 11, 32);
  bool changed = c.importControls("10;11;", true, false);
  EXPECT_TRUE(changed);
  EXPECT_EQ(2, c.nControls());
}

TEST(oCourse, ImportControls_NoChangeIfSame) {
  oEvent oe;
  oCourse c(&oe, 1);
  makeControl(oe, 10, 31);
  c.importControls("10;", false, false);
  bool changed = c.importControls("10;", false, false);
  EXPECT_FALSE(changed);
}

TEST(oCourse, HasControl) {
  oEvent oe;
  oCourse c(&oe, 1);
  auto* ctrl1 = makeControl(oe, 10, 31);
  auto* ctrl2 = makeControl(oe, 11, 32);
  c.addControl(10);
  EXPECT_TRUE(c.hasControl(ctrl1));
  EXPECT_FALSE(c.hasControl(ctrl2));
}

TEST(oCourse, HasControlCode) {
  oEvent oe;
  oCourse c(&oe, 1);
  makeControl(oe, 10, 31);
  c.addControl(10);
  EXPECT_TRUE(c.hasControlCode(31));
  EXPECT_FALSE(c.hasControlCode(99));
}

// ---------------------------------------------------------------------------
// Leg lengths
// ---------------------------------------------------------------------------

TEST(oCourse, LegLengths_SetGet) {
  oEvent oe;
  oCourse c(&oe, 1);
  makeControl(oe, 10, 31);
  c.addControl(10);
  c.setLegLengths({200, 300}); // start-ctrl1, ctrl1-finish
  EXPECT_EQ(200, c.getLegLength(0));
  EXPECT_EQ(300, c.getLegLength(1));
  EXPECT_EQ(0,   c.getLegLength(5));  // out of range
}

TEST(oCourse, LegLengths_InvalidSize_Throws) {
  oEvent oe;
  oCourse c(&oe, 1);
  makeControl(oe, 10, 31);
  c.addControl(10);
  EXPECT_THROW(c.setLegLengths({100}), std::runtime_error); // wrong size (need 2)
}

TEST(oCourse, LegLengths_StringRoundTrip) {
  oEvent oe;
  oCourse c(&oe, 1);
  makeControl(oe, 10, 31);
  c.addControl(10);
  c.setLegLengths({200, 300});
  std::string s = c.getLegLengths();
  EXPECT_EQ("200;300", s);
}

TEST(oCourse, ImportLegLengths) {
  oEvent oe;
  oCourse c(&oe, 1);
  makeControl(oe, 10, 31);
  c.addControl(10);
  c.importLegLengths("200;300", false);
  EXPECT_EQ(200, c.getLegLength(0));
  EXPECT_EQ(300, c.getLegLength(1));
}

// ---------------------------------------------------------------------------
// First/last as start/finish
// ---------------------------------------------------------------------------

TEST(oCourse, FirstAsStart_Default) {
  oEvent oe;
  oCourse c(&oe, 1);
  EXPECT_FALSE(c.useFirstAsStart());
}

TEST(oCourse, FirstAsStart_Set) {
  oEvent oe;
  oCourse c(&oe, 1);
  c.firstAsStart(true);
  EXPECT_TRUE(c.useFirstAsStart());
  c.firstAsStart(false);
  EXPECT_FALSE(c.useFirstAsStart());
}

TEST(oCourse, LastAsFinish_Set) {
  oEvent oe;
  oCourse c(&oe, 1);
  c.lastAsFinish(true);
  EXPECT_TRUE(c.useLastAsFinish());
}

TEST(oCourse, StartPunchType_Default) {
  oEvent oe;
  oCourse c(&oe, 1);
  EXPECT_EQ(oPunch::PunchStart, c.getStartPunchType());
}

TEST(oCourse, StartPunchType_FirstAsStart) {
  oEvent oe;
  oCourse c(&oe, 1);
  makeControl(oe, 10, 31);
  c.addControl(10);
  c.firstAsStart(true);
  EXPECT_EQ(31, c.getStartPunchType());
}

TEST(oCourse, FinishPunchType_Default) {
  oEvent oe;
  oCourse c(&oe, 1);
  EXPECT_EQ(oPunch::PunchFinish, c.getFinishPunchType());
}

TEST(oCourse, FinishPunchType_LastAsFinish) {
  oEvent oe;
  oCourse c(&oe, 1);
  makeControl(oe, 10, 31);
  makeControl(oe, 11, 99);
  c.addControl(10);
  c.addControl(11);
  c.lastAsFinish(true);
  EXPECT_EQ(99, c.getFinishPunchType());
}

// ---------------------------------------------------------------------------
// Start/finish control objects
// ---------------------------------------------------------------------------

TEST(oCourse, SetStartFinish) {
  oEvent oe;
  oCourse c(&oe, 1);
  auto* s = makeControl(oe, 100, oPunch::PunchStart);
  auto* f = makeControl(oe, 101, oPunch::PunchFinish);
  bool changed = c.setStartFinish(s, f, false);
  EXPECT_TRUE(changed);
  EXPECT_EQ(100, c.getStartId());
  EXPECT_EQ(101, c.getFinishId());
}

TEST(oCourse, SetStartFinish_NoChangeIfSame) {
  oEvent oe;
  oCourse c(&oe, 1);
  auto* s = makeControl(oe, 100, oPunch::PunchStart);
  c.setStartFinish(s, nullptr, false);
  bool changed = c.setStartFinish(s, nullptr, false);
  EXPECT_FALSE(changed);
}

// ---------------------------------------------------------------------------
// Common control / loops
// ---------------------------------------------------------------------------

TEST(oCourse, CommonControl_SetGet) {
  oEvent oe;
  oCourse c(&oe, 1);
  makeControl(oe, 10, 31);
  c.addControl(10);
  c.setCommonControl(10);
  EXPECT_EQ(10, c.getCommonControl());
}

TEST(oCourse, CommonControl_NotOnCourse_Throws) {
  oEvent oe;
  oCourse c(&oe, 1);
  makeControl(oe, 10, 31);
  c.addControl(10);
  EXPECT_THROW(c.setCommonControl(99), std::runtime_error);
}

TEST(oCourse, NumLoops_WithCommonControl) {
  oEvent oe;
  oCourse c(&oe, 1);
  makeControl(oe, 10, 31); // common control
  makeControl(oe, 11, 32);
  makeControl(oe, 12, 33);
  c.addControl(10); // start common
  c.addControl(11); // loop 1
  c.addControl(10); // middle common
  c.addControl(12); // loop 2
  c.setCommonControl(10);
  EXPECT_EQ(2, c.getNumLoops());
}

// ---------------------------------------------------------------------------
// Rogaining
// ---------------------------------------------------------------------------

TEST(oCourse, HasRogaining_Default) {
  oEvent oe;
  oCourse c(&oe, 1);
  EXPECT_FALSE(c.hasRogaining());
}

TEST(oCourse, HasRogaining_WithTimeLimit) {
  oEvent oe;
  oCourse c(&oe, 1);
  c.setMaximumRogainingTime(90 * 60 * 10); // 90 min
  EXPECT_TRUE(c.hasRogaining());
}

TEST(oCourse, HasRogaining_WithPointLimit) {
  oEvent oe;
  oCourse c(&oe, 1);
  c.setMinimumRogainingPoints(50);
  EXPECT_TRUE(c.hasRogaining());
}

TEST(oCourse, RogainingTime_SetGet) {
  oEvent oe;
  oCourse c(&oe, 1);
  c.setMaximumRogainingTime(3600 * 10);
  EXPECT_EQ(3600 * 10, c.getMaximumRogainingTime());
}

TEST(oCourse, RogainingPoints_SetGet) {
  oEvent oe;
  oCourse c(&oe, 1);
  c.setMinimumRogainingPoints(100);
  EXPECT_EQ(100, c.getMinimumRogainingPoints());
}

TEST(oCourse, RogainingPointsPerMinute) {
  oEvent oe;
  oCourse c(&oe, 1);
  c.setRogainingPointsPerMinute(5);
  EXPECT_EQ(5, c.getRogainingPointsPerMinute());
}

TEST(oCourse, CalculateReduction_Linear) {
  oEvent oe;
  oCourse c(&oe, 1);
  c.setRogainingPointsPerMinute(1); // 1 pt/min reduction
  // overTime = 1 min (in tenths-of-sec = 600)
  int r = c.calculateReduction(timeConstMinute);
  EXPECT_GT(r, 0);
}

TEST(oCourse, CalculateReduction_Zero) {
  oEvent oe;
  oCourse c(&oe, 1);
  EXPECT_EQ(0, c.calculateReduction(0));
  EXPECT_EQ(0, c.calculateReduction(-100));
}

// ---------------------------------------------------------------------------
// Number maps
// ---------------------------------------------------------------------------

TEST(oCourse, NumberMaps_SetGet) {
  oEvent oe;
  oCourse c(&oe, 1);
  c.setNumberMaps(50);
  EXPECT_EQ(50, c.getNumberMaps());
}

TEST(oCourse, NumberMaps_Default) {
  oEvent oe;
  oCourse c(&oe, 1);
  EXPECT_EQ(0, c.getNumberMaps());
}

// ---------------------------------------------------------------------------
// getCourseControlId
// ---------------------------------------------------------------------------

TEST(oCourse, GetCourseControlId_Single) {
  oEvent oe;
  oCourse c(&oe, 1);
  makeControl(oe, 10, 31);
  c.addControl(10);
  int ccid = c.getCourseControlId(0);
  EXPECT_GT(ccid, 0);
}

TEST(oCourse, GetCourseControlId_Duplicate) {
  oEvent oe;
  oCourse c(&oe, 1);
  makeControl(oe, 10, 31);
  c.addControl(10);
  c.addControl(10); // same control twice
  int ccid0 = c.getCourseControlId(0);
  int ccid1 = c.getCourseControlId(1);
  EXPECT_NE(ccid0, ccid1); // different course-control IDs
}

// ---------------------------------------------------------------------------
// getShorterVersion / setShorterVersion
// ---------------------------------------------------------------------------

TEST(oCourse, ShorterVersion_Default) {
  oEvent oe;
  oCourse c(&oe, 1);
  auto [active, ptr] = c.getShorterVersion();
  EXPECT_FALSE(active);
  EXPECT_EQ(nullptr, ptr);
}

TEST(oCourse, ShorterVersion_SetActiveSelf) {
  oEvent oe;
  oCourse c(&oe, 1);
  c.setShorterVersion(true, nullptr); // active self-shortening
  auto [active, ptr] = c.getShorterVersion();
  EXPECT_TRUE(active);
}

// ---------------------------------------------------------------------------
// isAdapted / getAdaptionId
// ---------------------------------------------------------------------------

TEST(oCourse, IsAdapted_Default) {
  oEvent oe;
  oCourse c(&oe, 1);
  EXPECT_FALSE(c.isAdapted());
}

TEST(oCourse, GetAdaptionId_Empty) {
  oEvent oe;
  oCourse c(&oe, 1);
  EXPECT_EQ(0, c.getAdaptionId());
}

// ---------------------------------------------------------------------------
// getIdSum
// ---------------------------------------------------------------------------

TEST(oCourse, GetIdSum) {
  oEvent oe;
  oCourse c(&oe, 1);
  makeControl(oe, 10, 31);
  makeControl(oe, 11, 32);
  c.addControl(10);
  c.addControl(11);
  int sum = c.getIdSum(2);
  EXPECT_NE(0, sum);
}

TEST(oCourse, GetIdSum_Empty_ReturnsId) {
  oEvent oe;
  oCourse c(&oe, 5);
  int sum = c.getIdSum(0);
  EXPECT_EQ(c.getId(), sum);
}

// ---------------------------------------------------------------------------
// getCourseProblems
// ---------------------------------------------------------------------------

TEST(oCourse, GetCourseProblems_NoRogaining) {
  oEvent oe;
  oCourse c(&oe, 1);
  EXPECT_EQ(L"", c.getCourseProblems());
}

TEST(oCourse, GetCourseProblems_RogainingNoControls) {
  oEvent oe;
  oCourse c(&oe, 1);
  c.setMaximumRogainingTime(3600 * 10);
  // no rogaining controls on course
  EXPECT_NE(L"", c.getCourseProblems());
}

// ---------------------------------------------------------------------------
// remove / canRemove
// ---------------------------------------------------------------------------

TEST(oCourse, Remove) {
  oEvent oe;
  oCourse c(&oe, 1);
  EXPECT_FALSE(c.isRemoved());
  c.remove();
  EXPECT_TRUE(c.isRemoved());
}

TEST(oCourse, CanRemove) {
  oEvent oe;
  oCourse c(&oe, 1);
  EXPECT_TRUE(c.canRemove());
}

// ---------------------------------------------------------------------------
// getControlOrdinal
// ---------------------------------------------------------------------------

TEST(oCourse, GetControlOrdinal_Normal) {
  oEvent oe;
  oCourse c(&oe, 1);
  makeControl(oe, 10, 31);
  makeControl(oe, 11, 32);
  makeControl(oe, 12, 33);
  c.addControl(10);
  c.addControl(11);
  c.addControl(12);
  EXPECT_EQ(L"1", c.getControlOrdinal(0));
  EXPECT_EQ(L"2", c.getControlOrdinal(1));
  EXPECT_EQ(L"3", c.getControlOrdinal(2));
}

TEST(oCourse, GetControlOrdinal_LastIsFinish) {
  oEvent oe;
  oCourse c(&oe, 1);
  makeControl(oe, 10, 31);
  c.addControl(10);
  c.lastAsFinish(true);
  // index == nControls() returns finish label
  const wstring& fin = c.getControlOrdinal(c.nControls());
  EXPECT_EQ(L"Mål", fin);
}

// ---------------------------------------------------------------------------
// oEvent::getControl stub
// ---------------------------------------------------------------------------

TEST(oEvent, GetControl_Create) {
  oEvent oe;
  oControl* c = oe.getControl(42, true);
  ASSERT_NE(nullptr, c);
  EXPECT_EQ(42, c->getId());
}

TEST(oEvent, GetControl_FindExisting) {
  oEvent oe;
  oControl* c1 = oe.getControl(42, true);
  oControl* c2 = oe.getControl(42, false);
  EXPECT_EQ(c1, c2);
}

TEST(oEvent, GetControl_NoCreate_ReturnsNull) {
  oEvent oe;
  EXPECT_EQ(nullptr, oe.getControl(99, false));
}

TEST(oEvent, GetFreeCourseId_Increments) {
  oEvent oe;
  int id1 = oe.getFreeCourseId();
  int id2 = oe.getFreeCourseId();
  EXPECT_GT(id2, id1);
}

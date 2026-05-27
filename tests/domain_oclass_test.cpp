// domain_oclass_test.cpp — Comprehensive unit tests for oClass (US-003e3)
// Covers: class-course assignment, leg info / start methods, timing flags,
//         fee fields, class type, operator<, codeLegMethod/importLegMethod,
//         parseCourses, importCourses, multi-course management.

#include <gtest/gtest.h>
#include "oClass.h"
#include "oCourse.h"
#include "oControl.h"
#include "oEvent.h"

// ===========================================================================
// Construction
// ===========================================================================

TEST(oClass, Construction_DefaultId) {
  oEvent oe;
  oClass c(&oe);
  EXPECT_GT(c.getId(), 0);
  EXPECT_FALSE(c.isRemoved());
  EXPECT_FALSE(c.isChanged());
}

TEST(oClass, Construction_ExplicitId) {
  oEvent oe;
  oClass c(&oe, 42);
  EXPECT_EQ(42, c.getId());
}

TEST(oClass, Construction_IdZeroGenerates) {
  oEvent oe;
  oClass c(&oe, 0);
  EXPECT_GT(c.getId(), 0);
}

TEST(oClass, GetInfo) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setName(L"H21", false);
  EXPECT_EQ(L"Klass H21", c.getInfo());
}

// ===========================================================================
// Name
// ===========================================================================

TEST(oClass, SetName_ChangesFlag) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setName(L"D21", false);
  EXPECT_EQ(L"D21", c.getName());
  EXPECT_TRUE(c.isChanged());
}

TEST(oClass, SetName_SameNoChange) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setName(L"H21", false);
  c.synchronize(); // resets changed flag
  c.setName(L"H21", false);
  EXPECT_FALSE(c.isChanged());
}

TEST(oClass, LongName_FallbackToName) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setName(L"H21", false);
  // LongName not set → falls back to Name
  EXPECT_EQ(L"H21", c.getLongName());
}

TEST(oClass, LongName_ExplicitOverride) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setName(L"H21", false);
  c.setLongName(L"Herrar 21");
  EXPECT_EQ(L"Herrar 21", c.getLongName());
}

// ===========================================================================
// Single-course assignment
// ===========================================================================

TEST(oClass, SetCourse_NullDefault) {
  oEvent oe;
  oClass c(&oe, 1);
  EXPECT_EQ(nullptr, c.getCourse());
  EXPECT_EQ(0, c.getCourseId());
}

TEST(oClass, SetCourse_Store) {
  oEvent oe;
  oCourse course(&oe, 10);
  oClass c(&oe, 1);
  c.setCourse(&course);
  EXPECT_EQ(&course, c.getCourse());
  EXPECT_EQ(10, c.getCourseId());
  EXPECT_TRUE(c.isChanged());
}

TEST(oClass, SetCourse_SameNoChange) {
  oEvent oe;
  oCourse course(&oe, 10);
  oClass c(&oe, 1);
  c.setCourse(&course);
  c.synchronize(); // resets changed flag
  c.setCourse(&course);
  EXPECT_FALSE(c.isChanged());
}

TEST(oClass, IsCourseUsed_Single) {
  oEvent oe;
  oCourse course(&oe, 10);
  oClass c(&oe, 1);
  EXPECT_FALSE(c.isCourseUsed(10));
  c.setCourse(&course);
  EXPECT_TRUE(c.isCourseUsed(10));
  EXPECT_FALSE(c.isCourseUsed(99));
}

// ===========================================================================
// Multi-course management
// ===========================================================================

TEST(oClass, SetNumStages_GrowsAndDefaultsToSTTime) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setNumStages(3);
  EXPECT_EQ(3u, c.getNumStages());
  EXPECT_TRUE(c.hasMultiCourse());
  EXPECT_EQ(STTime, c.getStartType(0));
  EXPECT_EQ(STTime, c.getStartType(1));
  EXPECT_EQ(STTime, c.getStartType(2));
}

TEST(oClass, SetNumStages_Shrink) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setNumStages(4);
  c.setNumStages(2);
  EXPECT_EQ(2u, c.getNumStages());
}

TEST(oClass, SetNumStages_SameIsNoOp) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setNumStages(2);
  c.synchronize(); // resets changed flag
  c.setNumStages(2);
  EXPECT_FALSE(c.isChanged());
}

TEST(oClass, AddStageCourse_AppendAndGet) {
  oEvent oe;
  oCourse ca(&oe, 10);
  oCourse cb(&oe, 20);
  oClass c(&oe, 1);
  c.setNumStages(2);
  EXPECT_TRUE(c.addStageCourse(0, &ca, -1));
  EXPECT_TRUE(c.addStageCourse(1, &cb, -1));

  std::vector<pCourse> courses;
  c.getCourses(0, courses);
  ASSERT_EQ(1u, courses.size());
  EXPECT_EQ(&ca, courses[0]);

  c.getCourses(1, courses);
  ASSERT_EQ(1u, courses.size());
  EXPECT_EQ(&cb, courses[0]);
}

TEST(oClass, AddStageCourse_ForkedStage) {
  oEvent oe;
  oCourse ca(&oe, 10);
  oCourse cb(&oe, 11);
  oClass c(&oe, 1);
  c.setNumStages(1);
  c.addStageCourse(0, &ca, -1);
  c.addStageCourse(0, &cb, -1);
  EXPECT_TRUE(c.isForked(0));
  EXPECT_TRUE(c.hasTrueMultiCourse());
  EXPECT_EQ(2u, [&]() { std::vector<pCourse> v; c.getCourses(0, v); return v.size(); }());
}

TEST(oClass, GetCourse_MultiStageByLegAndFork) {
  oEvent oe;
  oCourse ca(&oe, 10);
  oCourse cb(&oe, 11);
  oClass c(&oe, 1);
  c.setNumStages(1);
  c.addStageCourse(0, &ca, -1);
  c.addStageCourse(0, &cb, -1);
  EXPECT_EQ(&ca, c.getCourse(0, 0));
  EXPECT_EQ(&cb, c.getCourse(0, 1));
  EXPECT_EQ(&ca, c.getCourse(0, 2)); // wraps around
}

TEST(oClass, RemoveStageCourse) {
  oEvent oe;
  oCourse ca(&oe, 10);
  oCourse cb(&oe, 11);
  oClass c(&oe, 1);
  c.setNumStages(1);
  c.addStageCourse(0, &ca, -1);
  c.addStageCourse(0, &cb, -1);
  EXPECT_TRUE(c.removeStageCourse(0, 10, -1));
  std::vector<pCourse> v;
  c.getCourses(0, v);
  ASSERT_EQ(1u, v.size());
  EXPECT_EQ(&cb, v[0]);
}

TEST(oClass, MoveStageCourse) {
  oEvent oe;
  oCourse ca(&oe, 10);
  oCourse cb(&oe, 11);
  oCourse cc(&oe, 12);
  oClass c(&oe, 1);
  c.setNumStages(1);
  c.addStageCourse(0, &ca, -1);
  c.addStageCourse(0, &cb, -1);
  c.addStageCourse(0, &cc, -1);
  // Move index 0 to +1 (→ position 1)
  EXPECT_TRUE(c.moveStageCourse(0, 0, 1));
  std::vector<pCourse> v;
  c.getCourses(0, v);
  ASSERT_EQ(3u, v.size());
  EXPECT_EQ(&cb, v[0]);
  EXPECT_EQ(&ca, v[1]);
}

TEST(oClass, ClearStageCourses) {
  oEvent oe;
  oCourse ca(&oe, 10);
  oClass c(&oe, 1);
  c.setNumStages(1);
  c.addStageCourse(0, &ca, -1);
  c.clearStageCourses(0);
  std::vector<pCourse> v;
  c.getCourses(0, v);
  EXPECT_TRUE(v.empty());
}

TEST(oClass, IsCourseUsed_MultiStage) {
  oEvent oe;
  oCourse ca(&oe, 10);
  oClass c(&oe, 1);
  c.setNumStages(1);
  c.addStageCourse(0, &ca, -1);
  EXPECT_TRUE(c.isCourseUsed(10));
  EXPECT_FALSE(c.isCourseUsed(99));
}

// ===========================================================================
// parseCourses / importCourses
// ===========================================================================

TEST(oClass, ParseCourses_SingleStage) {
  std::vector<std::vector<int>> multi;
  std::set<int> ids;
  oClass::parseCourses("10 20 30", multi, ids);
  ASSERT_EQ(1u, multi.size());
  EXPECT_EQ((std::vector<int>{10, 20, 30}), multi[0]);
  EXPECT_EQ((std::set<int>{10, 20, 30}), ids);
}

TEST(oClass, ParseCourses_MultiStage) {
  std::vector<std::vector<int>> multi;
  std::set<int> ids;
  oClass::parseCourses("10 20;30", multi, ids);
  ASSERT_EQ(2u, multi.size());
  EXPECT_EQ((std::vector<int>{10, 20}), multi[0]);
  EXPECT_EQ((std::vector<int>{30}), multi[1]);
}

TEST(oClass, ParseCourses_Empty) {
  std::vector<std::vector<int>> multi;
  std::set<int> ids;
  oClass::parseCourses("", multi, ids);
  EXPECT_TRUE(multi.empty());
  EXPECT_TRUE(ids.empty());
}

// ===========================================================================
// Leg info — start methods
// ===========================================================================

TEST(oClass, LegInfo_DefaultSTTime) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setNumStages(2);
  EXPECT_EQ(STTime, c.getStartType(0));
  EXPECT_EQ(STTime, c.getStartType(1));
}

TEST(oClass, SetStartType) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setNumStages(3);
  c.setStartType(0, STTime, false);
  c.setStartType(1, STChange, false);
  c.setStartType(2, STPursuit, false);
  EXPECT_EQ(STTime,    c.getStartType(0));
  EXPECT_EQ(STChange,  c.getStartType(1));
  EXPECT_EQ(STPursuit, c.getStartType(2));
}

TEST(oClass, SetLegType) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setNumStages(3);
  c.setLegType(0, LTNormal);
  c.setLegType(1, LTParallel);
  c.setLegType(2, LTExtra);
  EXPECT_EQ(LTNormal,   c.getLegType(0));
  EXPECT_EQ(LTParallel, c.getLegType(1));
  EXPECT_EQ(LTExtra,    c.getLegType(2));
}

TEST(oClass, IsParallel_IsOptional) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setNumStages(3);
  c.setLegType(0, LTNormal);
  c.setLegType(1, LTParallel);
  c.setLegType(2, LTParallelOptional);
  EXPECT_FALSE(c.isParallel(0));
  EXPECT_TRUE(c.isParallel(1));
  EXPECT_TRUE(c.isParallel(2));
  EXPECT_FALSE(c.isOptional(1));
  EXPECT_TRUE(c.isOptional(2));
}

TEST(oClass, SetStartData) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setNumStages(2);
  EXPECT_TRUE(c.setStartData(0, 42));
  EXPECT_EQ(42, c.getStartData(0));
  EXPECT_EQ(0,  c.getStartData(1));
}

TEST(oClass, SetRestartTime_ParsesHMS) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setNumStages(1);
  c.setRestartTime(0, L"1:00:00");
  EXPECT_EQ(3600 * 10, c.getRestartTime(0)); // 1h in tenths-of-sec
}

TEST(oClass, SetRopeTime_ParsesHMS) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setNumStages(1);
  c.setRopeTime(0, L"0:30:00");
  EXPECT_EQ(1800 * 10, c.getRopeTime(0));
}

TEST(oClass, GetNumLegs) {
  oEvent oe;
  oClass c(&oe, 1);
  EXPECT_EQ(0, c.getNumLegs());
  c.setNumStages(4);
  EXPECT_EQ(4, c.getNumLegs());
}

// ===========================================================================
// Leg runner / duplicate runner
// ===========================================================================

TEST(oClass, LegRunner_NoParallel) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setNumStages(3);
  // No duplicate runner set — each leg maps to itself
  EXPECT_EQ(0, c.getLegRunner(0));
  EXPECT_EQ(1, c.getLegRunner(1));
  EXPECT_EQ(2, c.getLegRunner(2));
}

TEST(oClass, SetLegRunner_Duplicate) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setNumStages(3);
  c.setLegRunner(2, 0); // leg 2 is runner 0
  EXPECT_EQ(0, c.getLegRunner(2));
  EXPECT_EQ(2, c.getNumMultiRunners(0)); // runner 0 appears on legs 0 and 2
}

TEST(oClass, GetNumDistinctRunners) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setNumStages(4);
  // legs 0,1,2,3 — no duplicates
  EXPECT_EQ(4, c.getNumDistinctRunners());
  c.setLegRunner(3, 0); // leg 3 shares runner 0
  EXPECT_EQ(3, c.getNumDistinctRunners());
}

TEST(oClass, IsSingleRunnerMultiStage) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setNumStages(3);
  EXPECT_FALSE(c.isSingleRunnerMultiStage()); // 3 distinct runners
  c.setLegRunner(1, 0);
  c.setLegRunner(2, 0);
  EXPECT_TRUE(c.isSingleRunnerMultiStage());
}

// ===========================================================================
// Parallel leg navigation
// ===========================================================================

TEST(oClass, GetNextBaseLeg) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setNumStages(4);
  c.setLegType(1, LTParallel);
  c.setLegType(2, LTParallel);
  // Leg 0 is base; next base after 0 is 3
  EXPECT_EQ(3, c.getNextBaseLeg(0));
  EXPECT_EQ(3, c.getNextBaseLeg(1));
  EXPECT_EQ(3, c.getNextBaseLeg(2));
  EXPECT_EQ(4, c.getNextBaseLeg(3)); // past end
}

TEST(oClass, GetPreceedingLeg) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setNumStages(4);
  c.setLegType(1, LTParallel);
  c.setLegType(2, LTParallel);
  EXPECT_EQ(-1, c.getPreceedingLeg(0));
  EXPECT_EQ(0,  c.getPreceedingLeg(1));
  EXPECT_EQ(0,  c.getPreceedingLeg(2));
  EXPECT_EQ(0,  c.getPreceedingLeg(3));
}

TEST(oClass, SplitLegNumberParallel) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setNumStages(4);
  c.setLegType(1, LTParallel);
  c.setLegType(2, LTParallel);
  int legNum, legOrder;
  EXPECT_FALSE(c.splitLegNumberParallel(0, legNum, legOrder));
  EXPECT_EQ(0, legNum); EXPECT_EQ(0, legOrder);
  EXPECT_TRUE(c.splitLegNumberParallel(1, legNum, legOrder));
  EXPECT_EQ(0, legNum); EXPECT_EQ(1, legOrder);
  EXPECT_TRUE(c.splitLegNumberParallel(2, legNum, legOrder));
  EXPECT_EQ(0, legNum); EXPECT_EQ(2, legOrder);
  EXPECT_FALSE(c.splitLegNumberParallel(3, legNum, legOrder));
  EXPECT_EQ(3, legNum); EXPECT_EQ(0, legOrder);
}

TEST(oClass, GetLegNumber_MultiLeg) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setNumStages(3);
  c.setLegType(1, LTParallel);
  EXPECT_EQ(L"1",  c.getLegNumber(0));
  EXPECT_EQ(L"1a", c.getLegNumber(1));
  EXPECT_EQ(L"3",  c.getLegNumber(2));
}

TEST(oClass, GetNumLegNoParallel) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setNumStages(4);
  c.setLegType(1, LTParallel);
  c.setLegType(2, LTParallel);
  EXPECT_EQ(2, c.getNumLegNoParallel()); // legs 0 and 3
}

// ===========================================================================
// Timing flags
// ===========================================================================

TEST(oClass, NoTiming_DefaultFalse) {
  oEvent oe;
  oClass c(&oe, 1);
  EXPECT_FALSE(c.getNoTiming());
}

TEST(oClass, SetNoTiming) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setNoTiming(true);
  EXPECT_TRUE(c.getNoTiming());
  c.setNoTiming(false);
  EXPECT_FALSE(c.getNoTiming());
}

TEST(oClass, IgnoreStartPunch) {
  oEvent oe;
  oClass c(&oe, 1);
  EXPECT_FALSE(c.ignoreStartPunch());
  c.setIgnoreStartPunch(true);
  EXPECT_TRUE(c.ignoreStartPunch());
}

TEST(oClass, FreeStart) {
  oEvent oe;
  oClass c(&oe, 1);
  EXPECT_FALSE(c.hasFreeStart());
  c.setFreeStart(true);
  EXPECT_TRUE(c.hasFreeStart());
}

TEST(oClass, RequestStart) {
  oEvent oe;
  oClass c(&oe, 1);
  EXPECT_FALSE(c.hasRequestStart());
  c.setRequestStart(true);
  EXPECT_TRUE(c.hasRequestStart());
}

TEST(oClass, DirectResult) {
  oEvent oe;
  oClass c(&oe, 1);
  EXPECT_FALSE(c.hasDirectResult());
  c.setDirectResult(true);
  EXPECT_TRUE(c.hasDirectResult());
}

// ===========================================================================
// Fee fields (via DataInterface)
// ===========================================================================

TEST(oClass, FeeFields_DefaultZero) {
  oEvent oe;
  oClass c(&oe, 1);
  EXPECT_EQ(0, c.getDCI().getInt("ClassFee"));
  EXPECT_EQ(0, c.getDCI().getInt("HighClassFee"));
  EXPECT_EQ(0, c.getDCI().getInt("ClassFeeRed"));
  EXPECT_EQ(0, c.getDCI().getInt("HighClassFeeRed"));
}

TEST(oClass, FeeFields_SetAndGet) {
  oEvent oe;
  oClass c(&oe, 1);
  c.getDI().setInt("ClassFee",        150);
  c.getDI().setInt("HighClassFee",    250);
  c.getDI().setInt("ClassFeeRed",     100);
  c.getDI().setInt("HighClassFeeRed", 180);
  EXPECT_EQ(150, c.getDCI().getInt("ClassFee"));
  EXPECT_EQ(250, c.getDCI().getInt("HighClassFee"));
  EXPECT_EQ(100, c.getDCI().getInt("ClassFeeRed"));
  EXPECT_EQ(180, c.getDCI().getInt("HighClassFeeRed"));
}

TEST(oClass, NumberMaps_SetAndGet) {
  oEvent oe;
  oClass c(&oe, 1);
  EXPECT_EQ(0, c.getDCI().getInt("NumberMaps"));
  c.getDI().setInt("NumberMaps", 5);
  EXPECT_EQ(5, c.getDCI().getInt("NumberMaps"));
}

// ===========================================================================
// ClassType
// ===========================================================================

TEST(oClass, ClassType_Individual_NoStages) {
  oEvent oe;
  oClass c(&oe, 1);
  EXPECT_EQ(oClassIndividual, c.getClassType());
}

TEST(oClass, ClassType_IndividualRelay_OneRunner_MultiStage) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setNumStages(3);
  c.setLegRunner(1, 0);
  c.setLegRunner(2, 0);
  EXPECT_EQ(oClassIndividRelay, c.getClassType());
}

TEST(oClass, ClassType_Relay_MultipleRunners) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setNumStages(3);
  EXPECT_EQ(oClassRelay, c.getClassType());
}

TEST(oClass, IsTeamClass_True) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setNumStages(3);
  EXPECT_TRUE(c.isTeamClass());
}

TEST(oClass, IsTeamClass_False_SingleRunner) {
  oEvent oe;
  oClass c(&oe, 1);
  EXPECT_FALSE(c.isTeamClass());
  c.setNumStages(3);
  c.setLegRunner(1, 0);
  c.setLegRunner(2, 0);
  EXPECT_FALSE(c.isTeamClass());
}

// ===========================================================================
// Sort index / operator<
// ===========================================================================

TEST(oClass, SortIndex_DefaultZero) {
  oEvent oe;
  oClass c(&oe, 1);
  EXPECT_EQ(0, c.getSortIndex());
}

TEST(oClass, OperatorLess_BySortIndex) {
  oEvent oe;
  oClass a(&oe, 1);
  oClass b(&oe, 2);
  a.tSortIndex = 1;
  b.tSortIndex = 2;
  EXPECT_TRUE(a < b);
  EXPECT_FALSE(b < a);
}

TEST(oClass, OperatorLess_TiebreakerById) {
  oEvent oe;
  oClass a(&oe, 1);
  oClass b(&oe, 2);
  a.tSortIndex = 0;
  b.tSortIndex = 0;
  EXPECT_TRUE(a < b);
  EXPECT_FALSE(b < a);
}

// ===========================================================================
// remove / canRemove
// ===========================================================================

TEST(oClass, RemoveAndCanRemove) {
  oEvent oe;
  oClass c(&oe, 1);
  EXPECT_TRUE(c.canRemove());
  EXPECT_FALSE(c.isRemoved());
  c.remove();
  EXPECT_TRUE(c.isRemoved());
}

// ===========================================================================
// changedObject sets oe->sqlClasses.changed
// ===========================================================================

TEST(oClass, ChangedObject_SetsSqlClassesChanged) {
  oEvent oe;
  oe.sqlClasses.changed = false;
  oClass c(&oe, 1);
  c.changedObject();
  EXPECT_TRUE(oe.sqlClasses.changed);
  EXPECT_TRUE(oe.globalModification);
}

// ===========================================================================
// codeLegMethod / importLegMethod round-trip
// ===========================================================================

TEST(oClass, oLegInfo_CodeLegMethod_Default) {
  oLegInfo li;
  std::string code = li.codeLegMethod();
  EXPECT_FALSE(code.empty());
  // Parse back
  oLegInfo li2;
  li2.importLegMethod(code);
  EXPECT_EQ(STTime,   li2.startMethod);
  EXPECT_EQ(LTNormal, li2.legMethod);
  EXPECT_EQ(0,        li2.legStartData);
}

TEST(oClass, oLegInfo_CodeLegMethod_Pursuit) {
  oLegInfo li;
  li.startMethod  = STPursuit;
  li.legMethod    = LTNormal;
  li.legStartData = 60 * 10; // 60 sec = 600 tenths
  std::string code = li.codeLegMethod();

  oLegInfo li2;
  li2.importLegMethod(code);
  EXPECT_EQ(STPursuit, li2.startMethod);
  EXPECT_EQ(LTNormal,  li2.legMethod);
  EXPECT_EQ(600,       li2.legStartData);
}

TEST(oClass, oLegInfo_CodeLegMethod_Parallel) {
  oLegInfo li;
  li.startMethod = STChange;
  li.legMethod   = LTParallel;
  std::string code = li.codeLegMethod();

  oLegInfo li2;
  li2.importLegMethod(code);
  EXPECT_EQ(STChange,   li2.startMethod);
  EXPECT_EQ(LTParallel, li2.legMethod);
}

TEST(oClass, ImportLegMethod_MultipleLegs) {
  oEvent oe;
  oClass c(&oe, 1);
  // Encode a 3-leg relay: ST*CH*HU
  // (STTime, LTNormal) * (STChange, LTNormal) * (STPursuit, LTNormal)
  oLegInfo l0, l1, l2;
  l0.startMethod = STTime;
  l1.startMethod = STChange;
  l2.startMethod = STPursuit;
  std::string code = l0.codeLegMethod() + "*" +
                     l1.codeLegMethod() + "*" +
                     l2.codeLegMethod();
  c.importLegMethod(code);
  EXPECT_EQ(3, c.getNumLegs());
  EXPECT_EQ(STTime,    c.getStartType(0));
  EXPECT_EQ(STChange,  c.getStartType(1));
  EXPECT_EQ(STPursuit, c.getStartType(2));
}

TEST(oClass, CodeLegMethod_RoundTrip) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setNumStages(3);
  c.setStartType(0, STTime,    false);
  c.setStartType(1, STChange,  false);
  c.setStartType(2, STPursuit, false);
  c.setLegType(0, LTNormal);
  c.setLegType(1, LTParallel);
  c.setLegType(2, LTNormal);

  std::string coded = c.codeLegMethod();

  oEvent oe2;
  oClass c2(&oe2, 1);
  c2.importLegMethod(coded);

  EXPECT_EQ(3, c2.getNumLegs());
  EXPECT_EQ(STTime,    c2.getStartType(0));
  EXPECT_EQ(STChange,  c2.getStartType(1));
  EXPECT_EQ(STPursuit, c2.getStartType(2));
  EXPECT_EQ(LTNormal,   c2.getLegType(0));
  EXPECT_EQ(LTParallel, c2.getLegType(1));
  EXPECT_EQ(LTNormal,   c2.getLegType(2));
}

// ===========================================================================
// hasCoursePool / isRogaining
// ===========================================================================

TEST(oClass, HasCoursePool_DefaultFalse) {
  oEvent oe;
  oClass c(&oe, 1);
  EXPECT_FALSE(c.hasCoursePool());
}

TEST(oClass, HasCoursePool_SetTrue) {
  oEvent oe;
  oClass c(&oe, 1);
  c.getDI().setInt("HasPool", 1);
  EXPECT_TRUE(c.hasCoursePool());
}

TEST(oClass, IsRogaining_NoCourse) {
  oEvent oe;
  oClass c(&oe, 1);
  EXPECT_FALSE(c.isRogaining());
}

TEST(oClass, IsRogaining_CourseWithRogainingTime) {
  oEvent oe;
  oCourse course(&oe, 10);
  course.setMaximumRogainingTime(3600 * 10); // 1h
  oClass c(&oe, 1);
  c.setCourse(&course);
  EXPECT_TRUE(c.isRogaining());
}

// ===========================================================================
// Last-stage index helper
// ===========================================================================

TEST(oClass, GetLastStageIndex_NoStages) {
  oEvent oe;
  oClass c(&oe, 1);
  EXPECT_EQ(0u, c.getLastStageIndex());
}

TEST(oClass, GetLastStageIndex_WithStages) {
  oEvent oe;
  oClass c(&oe, 1);
  c.setNumStages(4);
  EXPECT_EQ(3u, c.getLastStageIndex());
}

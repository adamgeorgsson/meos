// class_test.cpp — Unit tests for oClass (US-003e).
#include <gtest/gtest.h>
#include <locale>
#include <clocale>

#define protected public
#define private   public

#include "oClass.h"
#include "oCourse.h"
#include "oControl.h"
#include "oEvent.h"
#include "classconfiginfo.h"
#include "../util/meosexception.h"

using std::string;
using std::wstring;
using std::vector;

// ── Test fixture ──────────────────────────────────────────────────────────────

class ClassTest : public ::testing::Test {
protected:
  oEvent oe;

  pCourse makeCourse(const wstring &name, int id = 0) {
    return oe.addCourse(name, 0, id ? id : 0);
  }

  void SetUp() override {
    std::setlocale(LC_ALL, "C.UTF-8");
  }
};

// ─────────────────────────── Construction ────────────────────────────────────

TEST_F(ClassTest, Class_DefaultState) {
  oClass c(&oe);
  EXPECT_TRUE(c.getName().empty());
  EXPECT_FALSE(c.isRemoved());
  EXPECT_EQ(c.getCourse(), nullptr);
  EXPECT_EQ(c.getNumStages(), 0u);
  EXPECT_EQ(c.getClassType(), oClassIndividual);
}

TEST_F(ClassTest, Class_WithId) {
  oClass c(&oe, 42);
  EXPECT_EQ(c.getId(), 42);
  EXPECT_EQ(c.getClassType(), oClassIndividual);
}

// ─────────────────────────── Name ────────────────────────────────────────────

TEST_F(ClassTest, Class_SetGetName) {
  oClass c(&oe);
  c.setName(L"Herrar 21", false);
  EXPECT_EQ(c.getName(), L"Herrar 21");
}

TEST_F(ClassTest, Class_ManualNameSetsFlag) {
  oClass c(&oe);
  c.setName(L"H21", true);
  EXPECT_TRUE(c.hasFlag(oClass::TransferFlags::FlagManualName));
}

// ─────────────────────────── Course assignment ───────────────────────────────

TEST_F(ClassTest, Class_SetGetCourse) {
  pCourse pc = makeCourse(L"Bana A");
  oClass c(&oe);
  c.setCourse(pc);
  EXPECT_EQ(c.getCourse(), pc);
}

TEST_F(ClassTest, Class_ClearCourse) {
  pCourse pc = makeCourse(L"Bana B");
  oClass c(&oe);
  c.setCourse(pc);
  c.setCourse(nullptr);
  EXPECT_EQ(c.getCourse(), nullptr);
}

TEST_F(ClassTest, Class_GetCourseId) {
  pCourse pc = makeCourse(L"Bana C");
  ASSERT_NE(pc, nullptr);
  oClass c(&oe);
  c.setCourse(pc);
  EXPECT_EQ(c.getCourseId(), pc->getId());
}

// ─────────────────────────── Multi-stage ─────────────────────────────────────

TEST_F(ClassTest, Class_SetNumStages) {
  oClass c(&oe);
  c.setNumStages(3);
  EXPECT_EQ(c.getNumStages(), 3u);
}

TEST_F(ClassTest, Class_AddStageCourse) {
  pCourse pc1 = makeCourse(L"Stage1");
  pCourse pc2 = makeCourse(L"Stage2");
  oClass c(&oe);
  c.setNumStages(2);
  EXPECT_TRUE(c.addStageCourse(0, pc1, -1));
  EXPECT_TRUE(c.addStageCourse(1, pc2, -1));
  EXPECT_EQ(c.getCourse(0, 0, false), pc1);
  EXPECT_EQ(c.getCourse(1, 0, false), pc2);
}

TEST_F(ClassTest, Class_AddStageCourseOutOfBounds) {
  pCourse pc = makeCourse(L"StageBig");
  oClass c(&oe);
  c.setNumStages(1);
  EXPECT_FALSE(c.addStageCourse(5, pc, -1));
}

TEST_F(ClassTest, Class_ClearStageCourses) {
  pCourse pc = makeCourse(L"ToBeCleared");
  oClass c(&oe);
  c.setNumStages(1);
  c.addStageCourse(0, pc, -1);
  c.clearStageCourses(0);
  EXPECT_TRUE(c.MultiCourse[0].empty());
}

// ─────────────────────────── Leg / start types ───────────────────────────────

TEST_F(ClassTest, Class_DefaultStartType) {
  oClass c(&oe);
  c.setNumStages(1);
  EXPECT_EQ(c.getStartType(0), STTime);  // oLegInfo default ctor sets startMethod=STTime
}

TEST_F(ClassTest, Class_SetGetStartType) {
  oClass c(&oe);
  c.setNumStages(2);
  c.setStartType(0, STTime, false);
  EXPECT_EQ(c.getStartType(0), STTime);
}

TEST_F(ClassTest, Class_SetGetLegType) {
  oClass c(&oe);
  c.setNumStages(2);
  c.setLegType(1, LTParallel);
  EXPECT_EQ(c.getLegType(1), LTParallel);
}

TEST_F(ClassTest, Class_StartDataIgnored_Parallel) {
  oClass c(&oe);
  c.setNumStages(2);
  c.setStartType(0, STTime, false);
  c.setLegType(1, LTParallel);
  EXPECT_TRUE(c.startdataIgnored(1));
}

TEST_F(ClassTest, Class_SetStartData) {
  oClass c(&oe);
  c.setNumStages(1);
  c.setStartType(0, STDrawn, false);
  c.setStartData(0, 42);
  EXPECT_EQ(c.getStartData(0), 42);
}

// ─────────────────────────── ClassType ───────────────────────────────────────

TEST_F(ClassTest, Class_IndividualType) {
  oClass c(&oe);
  EXPECT_EQ(c.getClassType(), oClassIndividual);
}

TEST_F(ClassTest, Class_PatrolType) {
  oClass c(&oe);
  c.setNumStages(2);
  c.setLegType(1, LTParallel);
  EXPECT_EQ(c.getClassType(), oClassPatrol);
}

TEST_F(ClassTest, Class_RelayType) {
  oClass c(&oe);
  c.setNumStages(3);
  // All duplicateRunner == -1 by default -> oClassIndividRelay (all unique runners)
  // Setting duplicateRunner != 0 on leg 1 -> oClassRelay
  c.legInfo[1].duplicateRunner = 2;
  EXPECT_EQ(c.getClassType(), oClassRelay);
}

// ─────────────────────────── DI fields ───────────────────────────────────────

TEST_F(ClassTest, Class_NoTiming) {
  oClass c(&oe);
  EXPECT_FALSE(c.getNoTiming());
  c.setNoTiming(true);
  EXPECT_TRUE(c.getNoTiming());
}

TEST_F(ClassTest, Class_FreeStart) {
  oClass c(&oe);
  EXPECT_FALSE(c.hasFreeStart());
  c.setFreeStart(true);
  EXPECT_TRUE(c.hasFreeStart());
}

TEST_F(ClassTest, Class_DirectResult) {
  oClass c(&oe);
  EXPECT_FALSE(c.hasDirectResult());
  c.setDirectResult(true);
  EXPECT_TRUE(c.hasDirectResult());
}

TEST_F(ClassTest, Class_SexSetGet) {
  oClass c(&oe);
  c.setSex(sMale);
  EXPECT_EQ(c.getSex(), sMale);
  c.setSex(sFemale);
  EXPECT_EQ(c.getSex(), sFemale);
}

TEST_F(ClassTest, Class_BlockSetGet) {
  oClass c(&oe);
  c.setBlock(5);
  EXPECT_EQ(c.getBlock(), 5);
}

TEST_F(ClassTest, Class_AgeLimit) {
  oClass c(&oe);
  c.setAgeLimit(18, 35);
  int low, high;
  c.getAgeLimit(low, high);
  EXPECT_EQ(low, 18);
  EXPECT_EQ(high, 35);
}

TEST_F(ClassTest, Class_Type) {
  oClass c(&oe);
  c.setType(L"Elite");
  EXPECT_EQ(c.getType(), L"Elite");
}

TEST_F(ClassTest, Class_LongName) {
  oClass c(&oe);
  c.setLongName(L"Herrar 21 l\u00e5ng");
  wstring name = c.getLongName();
  EXPECT_EQ(name, wstring(L"Herrar 21 l\u00e5ng"));
}

TEST_F(ClassTest, Class_SortIndex) {
  oClass c(&oe);
  c.reinitialize(true);
  // After reinitialize, a sort index should be assigned (non-negative)
  EXPECT_GE(c.getSortIndex(), 0);
}

TEST_F(ClassTest, Class_BibMode) {
  oClass c(&oe);
  EXPECT_EQ(c.getBibMode(), BibSame);
  c.setBibMode(BibAdd);
  EXPECT_EQ(c.getBibMode(), BibAdd);
  c.setBibMode(BibFree);
  EXPECT_EQ(c.getBibMode(), BibFree);
}

TEST_F(ClassTest, Class_DrawParameters) {
  oClass c(&oe);
  c.setDrawFirstStart(3600);
  EXPECT_EQ(c.getDrawFirstStart(), 3600);
  c.setDrawInterval(120);
  EXPECT_EQ(c.getDrawInterval(), 120);
  c.setDrawVacant(3);
  EXPECT_EQ(c.getDrawVacant(), 3);
  c.setDrawNumReserved(2);
  EXPECT_EQ(c.getDrawNumReserved(), 2);
}

TEST_F(ClassTest, Class_LockedForking) {
  oClass c(&oe);
  EXPECT_FALSE(c.lockedForking());
  c.lockedForking(true);
  EXPECT_TRUE(c.lockedForking());
  c.lockedForking(false);
  EXPECT_FALSE(c.lockedForking());
}

// ─────────────────────────── oEvent class management ─────────────────────────

TEST_F(ClassTest, Event_AddGetClass) {
  pClass pc = oe.addClass(L"Damer 21", 0, 0);
  ASSERT_NE(pc, nullptr);
  EXPECT_EQ(pc->getName(), L"Damer 21");

  pClass found = oe.getClass(pc->getId());
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->getId(), pc->getId());
}

TEST_F(ClassTest, Event_GetClassByName) {
  oe.addClass(L"H21E", 0, 0);
  pClass pc = oe.getClass(L"H21E");
  ASSERT_NE(pc, nullptr);
  EXPECT_EQ(pc->getName(), L"H21E");
}

TEST_F(ClassTest, Event_GetClassesSorted) {
  pClass pc1 = oe.addClass(L"Class Z", 0, 10);
  pClass pc2 = oe.addClass(L"Class A", 0, 20);
  ASSERT_NE(pc1, nullptr);
  ASSERT_NE(pc2, nullptr);

  vector<pClass> cls;
  oe.getClasses(cls, false);
  EXPECT_GE(cls.size(), 2u);
}

TEST_F(ClassTest, Event_RemoveClass) {
  pClass pc = oe.addClass(L"ToRemove", 0, 0);
  ASSERT_NE(pc, nullptr);
  int id = pc->getId();
  oe.removeClass(id);
  EXPECT_EQ(oe.getClass(id), nullptr);
}

// ─────────────────────────── Leg helper methods ───────────────────────────────

TEST_F(ClassTest, Class_NumDistinctRunners_Single) {
  oClass c(&oe);
  EXPECT_EQ(c.getNumDistinctRunners(), 1);
}

TEST_F(ClassTest, Class_NumDistinctRunners_Relay) {
  oClass c(&oe);
  c.setNumStages(3);
  // Default: all duplicateRunner == -1 -> 3 distinct
  EXPECT_EQ(c.getNumDistinctRunners(), 3);
}

TEST_F(ClassTest, Class_LegNumber) {
  oClass c(&oe);
  c.setNumStages(3);
  EXPECT_EQ(c.getLegNumber(0), L"1");
  EXPECT_EQ(c.getLegNumber(1), L"2");
  EXPECT_EQ(c.getLegNumber(2), L"3");
}

TEST_F(ClassTest, Class_GetNextBaseLeg) {
  oClass c(&oe);
  c.setNumStages(3);
  c.setLegType(1, LTParallel);
  // Leg 0 -> next base is leg 2 (leg 1 is parallel)
  EXPECT_EQ(c.getNextBaseLeg(0), 2);
}

TEST_F(ClassTest, Class_GetPreceedingLeg) {
  oClass c(&oe);
  c.setNumStages(3);
  c.setLegType(1, LTParallel);
  // Leg 2: getPreceedingLeg returns the index before the first non-parallel
  // leg <= leg, i.e. leg2 itself is non-parallel so returns leg2-1=1.
  EXPECT_EQ(c.getPreceedingLeg(2), 1);
}

TEST_F(ClassTest, Class_HasCoursePool) {
  oClass c(&oe);
  EXPECT_FALSE(c.hasCoursePool());
  c.setCoursePool(true);
  EXPECT_TRUE(c.hasCoursePool());
}

TEST_F(ClassTest, Class_SingleStageOnly) {
  oClass c(&oe);
  EXPECT_FALSE(c.isSingleStageOnly());
  c.setSingleStageOnly(true);
  EXPECT_TRUE(c.isSingleStageOnly());
}

// ─────────────────────────── ClassConfigInfo ─────────────────────────────────

TEST_F(ClassTest, ClassConfigInfo_EmptyDefault) {
  ClassConfigInfo ci;
  EXPECT_TRUE(ci.empty());
}

TEST_F(ClassTest, ClassConfigInfo_ClearFill) {
  ClassConfigInfo ci;
  ci.individual.push_back(1);
  ci.relay.push_back(2);
  EXPECT_FALSE(ci.empty());
  ci.clear();
  EXPECT_TRUE(ci.empty());
}

TEST_F(ClassTest, ClassConfigInfo_GetIndividual) {
  ClassConfigInfo ci;
  ci.individual.push_back(10);
  ci.individual.push_back(20);
  ci.rogainingClasses.push_back(30);

  set<int> sel;
  ci.getIndividual(sel, false);
  EXPECT_EQ(sel.size(), 2u);
  EXPECT_EQ(sel.count(10), 1u);
  EXPECT_EQ(sel.count(20), 1u);
  EXPECT_EQ(sel.count(30), 0u);

  set<int> sel2;
  ci.getIndividual(sel2, true);
  EXPECT_EQ(sel2.size(), 3u);
  EXPECT_EQ(sel2.count(30), 1u);
}

TEST_F(ClassTest, ClassConfigInfo_HasTeamClass) {
  ClassConfigInfo ci;
  EXPECT_FALSE(ci.hasTeamClass());
  ci.relay.push_back(1);
  EXPECT_TRUE(ci.hasTeamClass());
}

// course_test.cpp — Unit tests for oCourse (US-003d).
#include <gtest/gtest.h>
#include <locale>
#include <clocale>

#define protected public
#define private   public

#include "oCourse.h"
#include "oControl.h"
#include "oEvent.h"
#include "SICard.h"
#include "../util/meosexception.h"

using std::string;
using std::wstring;
using std::vector;

// ── Test fixture ──────────────────────────────────────────────────────────────

class CourseTest : public ::testing::Test {
protected:
  oEvent oe;

  // Helper: add a control with a given punch code and return it.
  pControl makeControl(int id, int code) {
    oControl c(&oe, id);
    c.set(id, code, L"");
    return oe.addControl(c);
  }

  // Helper: build a course with given control codes.
  // Returns the addCourse()'d pointer.
  pCourse makeCourse(const wstring& name, vector<int> codes) {
    oCourse crs(&oe);
    crs.name = name;
    pCourse pc = oe.addCourse(crs);
    for (int code : codes) {
      // Ensure control with that ID exists (ID == code for simplicity).
      if (oe.getControl(code) == nullptr)
        makeControl(code, code);
      pc->addControl(code);
    }
    return pc;
  }

  void SetUp() override {
    std::setlocale(LC_ALL, "C.UTF-8");
  }
};

// ─────────────────────────── Construction ────────────────────────────────────

TEST_F(CourseTest, Course_DefaultState) {
  oCourse c(&oe);
  EXPECT_EQ(c.getLength(), 0);
  EXPECT_EQ(c.nControls(), 0);
  EXPECT_TRUE(c.getName().empty());
  EXPECT_FALSE(c.isRemoved());
}

TEST_F(CourseTest, Course_ConstructWithId) {
  oCourse c(&oe, 42);
  EXPECT_EQ(c.getId(), 42);
}

// ─────────────────────────── Name / Length ───────────────────────────────────

TEST_F(CourseTest, Course_SetName) {
  oCourse c(&oe, 1);
  c.setName(L"Lång");
  EXPECT_EQ(c.getName(), L"Lång");
}

TEST_F(CourseTest, Course_SetLength) {
  oCourse c(&oe, 1);
  c.setLength(4500);
  EXPECT_EQ(c.getLength(), 4500);
  EXPECT_EQ(c.getLengthS(), L"4500");
}

TEST_F(CourseTest, Course_SetLengthClampNegative) {
  oCourse c(&oe, 1);
  c.setLength(-100);
  EXPECT_EQ(c.getLength(), 0);
}

TEST_F(CourseTest, Course_SetLengthClampTooLarge) {
  oCourse c(&oe, 1);
  c.setLength(2000000);
  EXPECT_EQ(c.getLength(), 0);
}

// ─────────────────────────── splitControls ───────────────────────────────────

TEST_F(CourseTest, Course_SplitControls_Semicolon) {
  vector<int> nr;
  oCourse::splitControls("101;102;103", nr);
  ASSERT_EQ(nr.size(), 3u);
  EXPECT_EQ(nr[0], 101);
  EXPECT_EQ(nr[1], 102);
  EXPECT_EQ(nr[2], 103);
}

TEST_F(CourseTest, Course_SplitControls_Comma) {
  vector<int> nr;
  oCourse::splitControls("31,32,33", nr);
  ASSERT_EQ(nr.size(), 3u);
  EXPECT_EQ(nr[0], 31);
  EXPECT_EQ(nr[1], 32);
  EXPECT_EQ(nr[2], 33);
}

TEST_F(CourseTest, Course_SplitControls_Empty) {
  vector<int> nr;
  oCourse::splitControls("", nr);
  EXPECT_TRUE(nr.empty());
}

TEST_F(CourseTest, Course_SplitControls_SkipsZero) {
  vector<int> nr;
  oCourse::splitControls("0;101;0;102", nr);
  ASSERT_EQ(nr.size(), 2u);
  EXPECT_EQ(nr[0], 101);
  EXPECT_EQ(nr[1], 102);
}

// ─────────────────────────── addControl / importControls ─────────────────────

TEST_F(CourseTest, Course_AddControl) {
  makeControl(31, 31);
  oCourse c(&oe, 1);
  c.setName(L"Test");
  pCourse pc = oe.addCourse(c);

  pc->addControl(31);
  EXPECT_EQ(pc->nControls(), 1);
  EXPECT_EQ(pc->getControl(0)->getFirstNumber(), 31);
}

TEST_F(CourseTest, Course_ImportControls) {
  makeControl(101, 101);
  makeControl(102, 102);
  makeControl(103, 103);

  oCourse c(&oe, 2);
  pCourse pc = oe.addCourse(c);
  bool changed = pc->importControls("101;102;103", true, false);
  EXPECT_TRUE(changed);
  EXPECT_EQ(pc->nControls(), 3);
  EXPECT_EQ(pc->getControl(0)->getFirstNumber(), 101);
  EXPECT_EQ(pc->getControl(2)->getFirstNumber(), 103);
}

TEST_F(CourseTest, Course_ImportControls_NoChange) {
  makeControl(50, 50);
  oCourse c(&oe, 3);
  pCourse pc = oe.addCourse(c);
  pc->importControls("50", true, false);
  bool changed = pc->importControls("50", true, false);
  EXPECT_FALSE(changed);
  EXPECT_EQ(pc->nControls(), 1);
}

TEST_F(CourseTest, Course_GetControls_StringRoundtrip) {
  makeControl(10, 10);
  makeControl(20, 20);
  oCourse c(&oe, 5);
  pCourse pc = oe.addCourse(c);
  pc->importControls("10;20", false, false);
  string s = pc->getControls();
  // Should be "10;20;"
  EXPECT_NE(s.find("10"), string::npos);
  EXPECT_NE(s.find("20"), string::npos);
}

// ─────────────────────────── oEvent management ───────────────────────────────

TEST_F(CourseTest, oEvent_AddAndGetCourse) {
  oCourse c(&oe, 99);
  c.setName(L"Sprint");
  pCourse pc = oe.addCourse(c);
  ASSERT_NE(pc, nullptr);
  EXPECT_EQ(pc->getName(), L"Sprint");

  pCourse found = oe.getCourse(99);
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->getId(), 99);
}

TEST_F(CourseTest, oEvent_GetCourseByName) {
  oCourse c(&oe, 10);
  c.setName(L"Lång");
  oe.addCourse(c);

  pCourse found = oe.getCourse(L"Lång");
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->getId(), 10);
}

TEST_F(CourseTest, oEvent_GetCourses_Vector) {
  oCourse c1(&oe, 1); c1.setName(L"A"); oe.addCourse(c1);
  oCourse c2(&oe, 2); c2.setName(L"B"); oe.addCourse(c2);

  vector<pCourse> crs;
  oe.getCourses(crs);
  EXPECT_EQ(crs.size(), 2u);
}

TEST_F(CourseTest, oEvent_AddCourseByName) {
  pCourse pc = oe.addCourse(L"Forest", 3200, 7);
  ASSERT_NE(pc, nullptr);
  EXPECT_EQ(pc->getName(), L"Forest");
  EXPECT_EQ(pc->getLength(), 3200);
  EXPECT_EQ(pc->getId(), 7);
}

TEST_F(CourseTest, oEvent_NoDuplicateCourseId) {
  oCourse c(&oe, 5);
  oe.addCourse(c);
  oCourse c2(&oe, 5);
  pCourse pc = oe.addCourse(c2);
  EXPECT_EQ(pc, nullptr);
}

TEST_F(CourseTest, oEvent_RemoveCourse) {
  oCourse c(&oe, 3);
  c.setName(L"ToRemove");
  oe.addCourse(c);
  ASSERT_NE(oe.getCourse(3), nullptr);

  oe.removeCourse(3);
  EXPECT_EQ(oe.getCourse(3), nullptr);
}

// ─────────────────────────── Control management via oEvent ───────────────────

TEST_F(CourseTest, oEvent_GetControlCreate) {
  pControl pc = oe.getControl(77, true, false);
  ASSERT_NE(pc, nullptr);
  EXPECT_EQ(pc->getId(), 77);

  // Second call should return same pointer.
  pControl pc2 = oe.getControl(77);
  EXPECT_EQ(pc, pc2);
}

// ─────────────────────────── hasControl ─────────────────────────────────────

TEST_F(CourseTest, Course_HasControl) {
  pControl ctrl = makeControl(55, 55);
  oCourse crs(&oe, 20);
  pCourse pc = oe.addCourse(crs);
  pc->addControl(55);
  EXPECT_TRUE(pc->hasControl(ctrl));

  oControl other(&oe, 99);
  EXPECT_FALSE(pc->hasControl(&other));
}

TEST_F(CourseTest, Course_HasControlCode) {
  makeControl(66, 66);
  oCourse crs(&oe, 21);
  pCourse pc = oe.addCourse(crs);
  pc->addControl(66);
  EXPECT_TRUE(pc->hasControlCode(66));
  EXPECT_FALSE(pc->hasControlCode(999));
}

// ─────────────────────────── getControlNumbers ───────────────────────────────

TEST_F(CourseTest, Course_GetControlNumbers) {
  makeControl(31, 31);
  makeControl(32, 32);
  makeControl(33, 33);
  oCourse crs(&oe, 30);
  pCourse pc = oe.addCourse(crs);
  pc->importControls("31;32;33", false, false);
  vector<int> nums = pc->getControlNumbers();
  ASSERT_EQ(nums.size(), 3u);
  EXPECT_EQ(nums[0], 31);
  EXPECT_EQ(nums[1], 32);
  EXPECT_EQ(nums[2], 33);
}

// ─────────────────────────── LegLengths ─────────────────────────────────────

TEST_F(CourseTest, Course_LegLengths_Import) {
  oCourse crs(&oe, 40);
  pCourse pc = oe.addCourse(crs);
  pc->importLegLengths("500;600;700;200", false);
  EXPECT_EQ(pc->getLegLength(0), 500);
  EXPECT_EQ(pc->getLegLength(2), 700);
  EXPECT_EQ(pc->getLegLength(3), 200);
  EXPECT_EQ(pc->getLegLength(4), 0); // out of range
}

TEST_F(CourseTest, Course_LegLengths_Set) {
  makeControl(10, 10);
  oCourse crs(&oe, 41);
  pCourse pc = oe.addCourse(crs);
  pc->addControl(10);
  vector<int> legs = {300, 400};
  pc->setLegLengths(legs);
  EXPECT_EQ(pc->getLegLength(0), 300);
  EXPECT_EQ(pc->getLegLength(1), 400);
}

TEST_F(CourseTest, Course_LegLengths_Roundtrip) {
  oCourse crs(&oe, 42);
  pCourse pc = oe.addCourse(crs);
  pc->importLegLengths("100;200;300", false);
  string s = pc->getLegLengths();
  EXPECT_EQ(s, "100;200;300");
}

// ─────────────────────────── Start / Finish ──────────────────────────────────

TEST_F(CourseTest, Course_StartFinish_Default) {
  oCourse crs(&oe, 50);
  pCourse pc = oe.addCourse(crs);
  EXPECT_FALSE(pc->useFirstAsStart());
  EXPECT_FALSE(pc->useLastAsFinish());
  EXPECT_EQ(pc->getFinishPunchType(), oPunch::PunchFinish);
  EXPECT_EQ(pc->getStartPunchType(),  oPunch::PunchStart);
}

TEST_F(CourseTest, Course_FirstAsStart) {
  makeControl(31, 31);
  oCourse crs(&oe, 51);
  pCourse pc = oe.addCourse(crs);
  pc->addControl(31);
  pc->firstAsStart(true);
  EXPECT_TRUE(pc->useFirstAsStart());
  EXPECT_EQ(pc->getStartPunchType(), 31);
}

TEST_F(CourseTest, Course_LastAsFinish) {
  makeControl(190, 190);
  oCourse crs(&oe, 52);
  pCourse pc = oe.addCourse(crs);
  pc->addControl(190);
  pc->lastAsFinish(true);
  EXPECT_TRUE(pc->useLastAsFinish());
  EXPECT_EQ(pc->getFinishPunchType(), 190);
}

TEST_F(CourseTest, Course_SetStartFinishId) {
  makeControl(31, 31);
  makeControl(190, 190);
  oCourse crs(&oe, 53);
  pCourse pc = oe.addCourse(crs);
  bool changed = pc->setStartFinishId(31, 190);
  EXPECT_TRUE(changed);
  EXPECT_EQ(pc->getStartId(),  31);
  EXPECT_EQ(pc->getFinishId(), 190);
}

// ─────────────────────────── distance (SICard) ───────────────────────────────

TEST_F(CourseTest, Course_Distance_ExactMatch) {
  makeControl(31, 31);
  makeControl(32, 32);
  makeControl(33, 33);
  oCourse crs(&oe, 60);
  pCourse pc = oe.addCourse(crs);
  pc->importControls("31;32;33", false, false);

  SICard card;
  card.nPunch = 3;
  card.Punch[0].Code = 31;
  card.Punch[1].Code = 32;
  card.Punch[2].Code = 33;
  EXPECT_EQ(pc->distance(card), 0);
}

TEST_F(CourseTest, Course_Distance_ExtraControl) {
  makeControl(31, 31);
  makeControl(32, 32);
  oCourse crs(&oe, 61);
  pCourse pc = oe.addCourse(crs);
  pc->importControls("31;32", false, false);

  SICard card;
  card.nPunch = 3;
  card.Punch[0].Code = 31;
  card.Punch[1].Code = 99; // extra
  card.Punch[2].Code = 32;
  EXPECT_GT(pc->distance(card), 0);
}

TEST_F(CourseTest, Course_Distance_MissingControl) {
  makeControl(31, 31);
  makeControl(32, 32);
  makeControl(33, 33);
  oCourse crs(&oe, 62);
  pCourse pc = oe.addCourse(crs);
  pc->importControls("31;32;33", false, false);

  SICard card;
  card.nPunch = 2;
  card.Punch[0].Code = 31;
  card.Punch[1].Code = 33; // 32 missing
  EXPECT_LT(pc->distance(card), 0);
}

// ─────────────────────────── Rogaining ───────────────────────────────────────

TEST_F(CourseTest, Course_NoRogaining_Default) {
  oCourse crs(&oe, 70);
  pCourse pc = oe.addCourse(crs);
  EXPECT_FALSE(pc->hasRogaining());
}

TEST_F(CourseTest, Course_Rogaining_MaxTime) {
  oCourse crs(&oe, 71);
  pCourse pc = oe.addCourse(crs);
  pc->setMaximumRogainingTime(3600);
  EXPECT_TRUE(pc->hasRogaining());
  EXPECT_EQ(pc->getMaximumRogainingTime(), 3600);
}

TEST_F(CourseTest, Course_Rogaining_PointLimit) {
  oCourse crs(&oe, 72);
  pCourse pc = oe.addCourse(crs);
  pc->setMinimumRogainingPoints(100);
  EXPECT_TRUE(pc->hasRogaining());
  EXPECT_EQ(pc->getMinimumRogainingPoints(), 100);
}

TEST_F(CourseTest, Course_Rogaining_PointsPerMinute) {
  oCourse crs(&oe, 73);
  pCourse pc = oe.addCourse(crs);
  pc->setRogainingPointsPerMinute(2);
  EXPECT_EQ(pc->getRogainingPointsPerMinute(), 2);
}

// ─────────────────────────── idSum ───────────────────────────────────────────

TEST_F(CourseTest, Course_IdSum) {
  makeControl(31, 31);
  makeControl(32, 32);
  oCourse crs(&oe, 80);
  pCourse pc = oe.addCourse(crs);
  pc->importControls("31;32", false, false);
  int s = pc->getIdSum(2);
  EXPECT_NE(s, 0);
}

// ─────────────────────────── getNameAndFamily ────────────────────────────────

TEST_F(CourseTest, Course_GetNameAndFamily_WithFamily) {
  oCourse crs(&oe, 90);
  crs.name = L"Sprint:Lång";
  wstring n, f;
  crs.getNameAndFamily(n, f);
  EXPECT_EQ(n, L"Lång");
  EXPECT_EQ(f, L"Sprint");
}

TEST_F(CourseTest, Course_GetNameAndFamily_NoFamily) {
  oCourse crs(&oe, 91);
  crs.name = L"Lång";
  wstring n, f;
  crs.getNameAndFamily(n, f);
  EXPECT_EQ(n, L"Lång");
  EXPECT_TRUE(f.empty());
}

// ─────────────────────────── getControlsUI ───────────────────────────────────

TEST_F(CourseTest, Course_GetControlsUI) {
  makeControl(31, 31);
  makeControl(32, 32);
  makeControl(33, 33);
  oCourse crs(&oe, 100);
  pCourse pc = oe.addCourse(crs);
  pc->importControls("31;32;33", false, false);
  wstring ui = pc->getControlsUI();
  EXPECT_NE(ui.find(L"31"), wstring::npos);
  EXPECT_NE(ui.find(L"33"), wstring::npos);
}

// ─────────────────────────── CommonControl ───────────────────────────────────

TEST_F(CourseTest, Course_CommonControl) {
  makeControl(31, 31);
  makeControl(32, 32);
  makeControl(33, 33);
  oCourse crs(&oe, 110);
  pCourse pc = oe.addCourse(crs);
  pc->importControls("31;32;33;31", false, false);
  pc->setCommonControl(31);
  EXPECT_EQ(pc->getCommonControl(), 31);
  EXPECT_GT(pc->getNumLoops(), 0);
}

TEST_F(CourseTest, Course_CommonControl_InvalidThrows) {
  oCourse crs(&oe, 111);
  pCourse pc = oe.addCourse(crs);
  EXPECT_THROW(pc->setCommonControl(99), meosException);
}

// ─────────────────────────── NumberMaps ──────────────────────────────────────

TEST_F(CourseTest, Course_NumberMaps) {
  oCourse crs(&oe, 120);
  pCourse pc = oe.addCourse(crs);
  pc->setNumberMaps(150);
  EXPECT_EQ(pc->getNumberMaps(), 150);
}

// ─────────────────────────── ShorterVersion ──────────────────────────────────

TEST_F(CourseTest, Course_ShorterVersion_None) {
  oCourse crs(&oe, 130);
  pCourse pc = oe.addCourse(crs);
  auto [active, shorter] = pc->getShorterVersion();
  EXPECT_FALSE(active);
  EXPECT_EQ(shorter, nullptr);
}

TEST_F(CourseTest, Course_ShorterVersion_Set) {
  oCourse c1(&oe, 131); pCourse pc1 = oe.addCourse(c1);
  oCourse c2(&oe, 132); pCourse pc2 = oe.addCourse(c2);
  pc1->setShorterVersion(true, pc2);
  auto [active, shorter] = pc1->getShorterVersion();
  EXPECT_TRUE(active);
  EXPECT_EQ(shorter, pc2);
}

// ─────────────────────────── isAdapted ───────────────────────────────────────

TEST_F(CourseTest, Course_IsAdapted_Default) {
  oCourse crs(&oe, 140);
  pCourse pc = oe.addCourse(crs);
  EXPECT_FALSE(pc->isAdapted());
}

// ─────────────────────────── getInfo ─────────────────────────────────────────

TEST_F(CourseTest, Course_GetInfo) {
  oCourse crs(&oe, 150);
  crs.name = L"Forest";
  pCourse pc = oe.addCourse(crs);
  wstring info = pc->getInfo();
  EXPECT_NE(info.find(L"Forest"), wstring::npos);
}

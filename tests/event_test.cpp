// event_test.cpp — Integration tests for US-003i (oEvent aggregate root)
#include <gtest/gtest.h>
#include "oEvent.h"
#include "oRunner.h"
#include "oCourse.h"
#include "oClass.h"
#include "oControl.h"
#include "oCard.h"
#include "oFreePunch.h"
#include "oPunch.h"
#include "SICard.h"
#include "oBase.h"

using std::wstring;
using std::string;
using CT = oBase::ChangeType;

class EventTest : public ::testing::Test {
protected:
  oEvent oe;
  void SetUp() override { setlocale(LC_ALL, "C.UTF-8"); }
};

// ── Competition name/date/zero-time round-trip ─────────────────────────────

TEST_F(EventTest, NameRoundTrip) {
  oe.setName(L"Sprint SM 2025");
  EXPECT_EQ(oe.getName(), L"Sprint SM 2025");
}

TEST_F(EventTest, NameTrimmed) {
  oe.setName(L"  My Race  ");
  EXPECT_EQ(oe.getName(), L"My Race");
}

TEST_F(EventTest, DateRoundTrip) {
  oe.setDate(L"2025-06-15");
  EXPECT_EQ(oe.getDate(), L"2025-06-15");
}

TEST_F(EventTest, ZeroTimeRoundTrip) {
  oe.setZeroTime(L"10:00:00");
  EXPECT_EQ(oe.getZeroTime(), L"10:00:00");
}

TEST_F(EventTest, ZeroTimeNumMatchesSet) {
  oe.setZeroTime(L"09:30:00");
  EXPECT_EQ(oe.getZeroTimeNum(), 342000); // timeUnitsPerSecond=10
}

TEST_F(EventTest, ZeroTimeEmptyReturnsDefault) {
  EXPECT_EQ(oe.getZeroTime(), L"00:00:00");
}

// ── Property store ─────────────────────────────────────────────────────────

TEST_F(EventTest, PropertyIntSetGet) {
  oe.setProperty("MaxAge", 99);
  EXPECT_EQ(oe.getPropertyInt("MaxAge", 0), 99);
}

TEST_F(EventTest, PropertyIntDefaultWhenMissing) {
  EXPECT_EQ(oe.getPropertyInt("nonexistent", 42), 42);
}

TEST_F(EventTest, PropertyStringSetGet) {
  oe.setProperty("Venue", wstring(L"Stockholm"));
  EXPECT_EQ(oe.getPropertyString("Venue", L""), L"Stockholm");
}

TEST_F(EventTest, PropertyStringDefaultWhenMissing) {
  EXPECT_EQ(oe.getPropertyString("nope", L"default"), L"default");
}

// ── newCompetition clears entities ─────────────────────────────────────────

TEST_F(EventTest, NewCompetitionClearsRunners) {
  oRunner r(&oe);
  r.setName(L"Anna", false);
  oe.addRunner(r);
  EXPECT_FALSE(oe.Runners.empty());

  oe.newCompetition(L"New Race");
  EXPECT_TRUE(oe.Runners.empty());
  EXPECT_EQ(oe.getName(), L"New Race");
}

TEST_F(EventTest, NewCompetitionClearsControls) {
  oControl ctrl(&oe, 1);
  oe.addControl(ctrl);
  EXPECT_FALSE(oe.Controls.empty());

  oe.newCompetition(L"New Race");
  EXPECT_TRUE(oe.Controls.empty());
}

TEST_F(EventTest, NewCompetitionClearsProperties) {
  oe.setProperty("key", 1);
  oe.newCompetition(L"Fresh");
  EXPECT_EQ(oe.getPropertyInt("key", -1), -1);
}

// ── getControlByNumber ─────────────────────────────────────────────────────

TEST_F(EventTest, GetControlByNumber_Found) {
  oControl ctrl(&oe, 10);
  ctrl.setNumbers(L"31");
  oe.addControl(ctrl);

  pControl found = oe.getControlByNumber(31);
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->getId(), 10);
}

TEST_F(EventTest, GetControlByNumber_NotFound) {
  pControl found = oe.getControlByNumber(999);
  EXPECT_EQ(found, nullptr);
}

// ── getControlIdFromPunch ──────────────────────────────────────────────────

TEST_F(EventTest, GetControlIdFromPunch_NoRunner_FallsBack) {
  oFreePunch fp(&oe, 0, 3600, 31, 0);
  int hash = oe.getControlIdFromPunch(3600, 31, 12345, false, fp);
  int expected = oFreePunch::getControlHash(31, 0);
  EXPECT_EQ(hash, expected);
  EXPECT_EQ(fp.getTiedRunnerId(), -1);
}

TEST_F(EventTest, GetControlIdFromPunch_WithRunnerAndCourse) {
  oControl ctrl(&oe, 1);
  ctrl.setNumbers(L"31");
  pControl pc = oe.addControl(ctrl);
  ASSERT_NE(pc, nullptr);

  pCourse course = oe.addCourse(L"TestCourse");
  ASSERT_NE(course, nullptr);
  course->addControl(pc->getId());

  pClass cls = oe.addClass(L"D21", course->getId());
  ASSERT_NE(cls, nullptr);

  oRunner r(&oe);
  r.setName(L"Test Runner", false);
  pRunner pr = oe.addRunner(r);
  ASSERT_NE(pr, nullptr);
  pr->setCardNo(12345, false, false);
  pr->setClassId(cls->getId(), false);
  pr->setCourseId(course->getId());

  oFreePunch fp(&oe, 0, 3600, 31, 0);
  oe.getControlIdFromPunch(3600, 31, 12345, false, fp);

  EXPECT_EQ(fp.getTiedRunnerId(), pr->getId());
}

// ── getRunnerByCardNo ──────────────────────────────────────────────────────

TEST_F(EventTest, GetRunnerByCardNo_Found) {
  oRunner r(&oe);
  r.setName(L"Card Runner", false);
  pRunner pr = oe.addRunner(r);
  pr->setCardNo(9999, false, false);

  pRunner found = oe.getRunnerByCardNo(9999, 0, oEvent::CardLookupProperty::Any);
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->getId(), pr->getId());
}

TEST_F(EventTest, GetRunnerByCardNo_NotFound) {
  pRunner found = oe.getRunnerByCardNo(7777, 0, oEvent::CardLookupProperty::Any);
  EXPECT_EQ(found, nullptr);
}

// ── Draw ──────────────────────────────────────────────────────────────────

TEST_F(EventTest, DrawAllClasses_AssignsStartTimes) {
  pClass cls = oe.addClass(L"H21");
  ASSERT_NE(cls, nullptr);

  for (int i = 0; i < 3; i++) {
    oRunner r(&oe);
    r.setName(L"Runner " + itow(i), false);
    pRunner pr = oe.addRunner(r);
    pr->setClassId(cls->getId(), false);
    pr->setStartNo(i + 1, CT::Update);
  }

  oe.drawAllClasses(3600, 60);

  vector<pRunner> runners;
  oe.getRunners(cls->getId(), 0, runners, false);
  ASSERT_EQ(runners.size(), 3u);

  bool hasPositive = false;
  for (pRunner r : runners)
    if (r->getStartTime() > 0) hasPositive = true;
  EXPECT_TRUE(hasPositive);
}

// ── calculateResults ───────────────────────────────────────────────────────

TEST_F(EventTest, CalculateResults_DoesNotCrash) {
  pClass cls = oe.addClass(L"H21");
  ASSERT_NE(cls, nullptr);

  struct RunnerInfo { const wchar_t* name; int start; int finish; };
  RunnerInfo info[] = {
    { L"Runner A", 3600, 4800 },
    { L"Runner B", 3600, 4600 },
    { L"Runner C", 3600, 5200 },
  };

  for (auto& ri : info) {
    oRunner r(&oe);
    r.setName(ri.name, false);
    pRunner pr = oe.addRunner(r);
    pr->setClassId(cls->getId(), false);
    pr->setStartTime(ri.start, true, CT::Update);
    pr->setFinishTime(ri.finish);
    pr->setStatus(StatusOK, true, CT::Update);
  }

  // Should not crash and should produce some output
  EXPECT_NO_THROW(oe.calculateResults(cls->getId()));
  EXPECT_NO_THROW(oe.sortRunnersByResult(cls->getId()));
}

// ── Full pipeline: evaluateCard ────────────────────────────────────────────

TEST_F(EventTest, EvaluateCard_FullPipeline) {
  oControl ctrl(&oe, 1);
  ctrl.setNumbers(L"31");
  pControl pc = oe.addControl(ctrl);

  pCourse course = oe.addCourse(L"Sprint");
  course->addControl(pc->getId());

  pClass cls = oe.addClass(L"D21", course->getId());

  oRunner r(&oe);
  r.setName(L"Test Runner", false);
  pRunner pr = oe.addRunner(r);
  pr->setClassId(cls->getId(), false);
  pr->setStartTime(3600, true, CT::Update);

  oCard card(&oe);
  card.setCardNo(54321);
  card.addPunch(oPunch::PunchStart, 3600, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(31, 4200, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(oPunch::PunchFinish, 4500, 0, 0, oCard::PunchOrigin::Unknown);

  pr->setCardNo(54321, false, false);
  pCard pc2 = oe.allocateCard(pr);
  *pc2 = card;

  vector<pair<int, pControl>> missing;
  pr->addCard(pc2, missing);
  pr->evaluateCard(true, missing, 0, CT::Update);

  EXPECT_EQ(pr->getStatus(), StatusOK);
  EXPECT_GT(pr->getRunningTime(false), 0);
  EXPECT_TRUE(missing.empty());
}

// Integration tests for oEvent draw and result computation (US-003i2).
//
// Tests exercise cross-entity relationships through oEvent:
//   - calculateResults assigns tPlace per runner within each class.
//   - calculateTeamResults assigns places to relay teams.
//   - drawStartList assigns sequential start times.

#include <gtest/gtest.h>
#include "oEvent.h"
#include "oRunner.h"
#include "oTeam.h"
#include "oClass.h"
#include "oBase.h"

using CT = oBase::ChangeType;
static constexpr int T = timeConstSecond; // 10 tenths-of-second = 1 second

// -----------------------------------------------------------------------
// Accessor to set protected runner fields (same friend class as in result tests)
// -----------------------------------------------------------------------
class RunnerResultTestAccessor {
public:
  static void setStartTime(oRunner* r, int t)       { r->startTime = t; r->tStartTime = t; }
  static void setStatus(oRunner* r, RunnerStatus s) { r->status = s; r->tStatus = s; }
  static void setFinishTime(oRunner* r, int t)      { r->FinishTime = t; }
  static void setClass(oRunner* r, oClass* cls)     { r->Class = cls; }
};

// -----------------------------------------------------------------------
// Fixture
// -----------------------------------------------------------------------
struct IntegrationTest : ::testing::Test {
  using A = RunnerResultTestAccessor;

  oEvent oe;

  // Helpers to set up runners with results.
  void makeRunnerResult(oRunner* r, oClass* cls, int startT, int finishT,
                        RunnerStatus status = StatusOK) {
    A::setClass(r, cls);
    A::setStartTime(r, startT);
    A::setFinishTime(r, finishT);
    A::setStatus(r, status);
  }
};

// -----------------------------------------------------------------------
// calculateResults — individual runners, ClassResult
// -----------------------------------------------------------------------

TEST_F(IntegrationTest, CalcResults_SingleClass_PlacesAssigned) {
  oClass* cls = oe.addClass(1);

  oRunner* r1 = oe.addRunner(1);
  oRunner* r2 = oe.addRunner(2);
  oRunner* r3 = oe.addRunner(3);

  // r2 fastest (200 s), r1 second (300 s), r3 DNF.
  int start = 10 * 60 * T;
  makeRunnerResult(r1, cls, start, start + 300 * T, StatusOK);
  makeRunnerResult(r2, cls, start, start + 200 * T, StatusOK);
  makeRunnerResult(r3, cls, start, start + 400 * T, StatusDNF);

  oe.calculateResults({}, oEvent::ResultType::ClassResult);

  EXPECT_EQ(r2->getPlace(), 1);
  EXPECT_EQ(r1->getPlace(), 2);
  EXPECT_EQ(r3->getPlace(), 0);  // DNF → no place
}

TEST_F(IntegrationTest, CalcResults_TiedRunners_SamePlace) {
  oClass* cls = oe.addClass(1);

  oRunner* r1 = oe.addRunner(1);
  oRunner* r2 = oe.addRunner(2);
  oRunner* r3 = oe.addRunner(3);

  int start = 10 * 60 * T;
  // r1 and r2 tied at 300 s, r3 at 400 s.
  makeRunnerResult(r1, cls, start, start + 300 * T, StatusOK);
  makeRunnerResult(r2, cls, start, start + 300 * T, StatusOK);
  makeRunnerResult(r3, cls, start, start + 400 * T, StatusOK);

  oe.calculateResults({}, oEvent::ResultType::ClassResult);

  EXPECT_EQ(r1->getPlace(), 1);
  EXPECT_EQ(r2->getPlace(), 1);
  EXPECT_EQ(r3->getPlace(), 3);
}

TEST_F(IntegrationTest, CalcResults_TwoClasses_SeparatePlaces) {
  oClass* cls1 = oe.addClass(1);
  oClass* cls2 = oe.addClass(2);

  oRunner* r1 = oe.addRunner(1);
  oRunner* r2 = oe.addRunner(2);
  oRunner* r3 = oe.addRunner(3);
  oRunner* r4 = oe.addRunner(4);

  int start = 10 * 60 * T;
  makeRunnerResult(r1, cls1, start, start + 100 * T, StatusOK);
  makeRunnerResult(r2, cls1, start, start + 200 * T, StatusOK);
  makeRunnerResult(r3, cls2, start, start + 150 * T, StatusOK);
  makeRunnerResult(r4, cls2, start, start + 250 * T, StatusOK);

  oe.calculateResults({}, oEvent::ResultType::ClassResult);

  // cls1: r1 wins, r2 second
  EXPECT_EQ(r1->getPlace(), 1);
  EXPECT_EQ(r2->getPlace(), 2);
  // cls2: r3 wins, r4 second (independent)
  EXPECT_EQ(r3->getPlace(), 1);
  EXPECT_EQ(r4->getPlace(), 2);
}

TEST_F(IntegrationTest, CalcResults_FilterByClass_OnlySelected) {
  oClass* cls1 = oe.addClass(1);
  oClass* cls2 = oe.addClass(2);

  oRunner* r1 = oe.addRunner(1);
  oRunner* r2 = oe.addRunner(2);

  int start = 10 * 60 * T;
  makeRunnerResult(r1, cls1, start, start + 100 * T, StatusOK);
  makeRunnerResult(r2, cls2, start, start + 150 * T, StatusOK);

  // Only calculate for cls1.
  oe.calculateResults({1}, oEvent::ResultType::ClassResult);

  EXPECT_EQ(r1->getPlace(), 1);
  // r2 in cls2 was not processed → place not set (default -1)
  EXPECT_EQ(r2->getPlace(), -1);
}

TEST_F(IntegrationTest, CalcResults_AllDNF_NoPlacesAssigned) {
  oClass* cls = oe.addClass(1);

  oRunner* r1 = oe.addRunner(1);
  oRunner* r2 = oe.addRunner(2);

  int start = 10 * 60 * T;
  makeRunnerResult(r1, cls, start, start + 100 * T, StatusDNF);
  makeRunnerResult(r2, cls, start, start + 200 * T, StatusDNF);

  oe.calculateResults({}, oEvent::ResultType::ClassResult);

  EXPECT_EQ(r1->getPlace(), 0);
  EXPECT_EQ(r2->getPlace(), 0);
}

TEST_F(IntegrationTest, CalcResults_EmptyRunnerList_NoCrash) {
  oe.calculateResults({}, oEvent::ResultType::ClassResult);
  SUCCEED();
}

// -----------------------------------------------------------------------
// calculateResults — TotalResult (includes inputTime)
// -----------------------------------------------------------------------

TEST_F(IntegrationTest, CalcTotalResults_SingleClass) {
  oClass* cls = oe.addClass(1);

  oRunner* r1 = oe.addRunner(1);
  oRunner* r2 = oe.addRunner(2);

  int start = 10 * 60 * T;
  makeRunnerResult(r1, cls, start, start + 300 * T, StatusOK);
  makeRunnerResult(r2, cls, start, start + 200 * T, StatusOK);

  oe.calculateResults({}, oEvent::ResultType::TotalResult);

  // TotalResult uses getTotalRunningTime (which falls through to getRunningTime
  // when inputTime=0 and inputStatus=OK for these simple runners).
  EXPECT_EQ(r2->getTotalPlace(), 1);
  EXPECT_EQ(r1->getTotalPlace(), 2);
}

// -----------------------------------------------------------------------
// calculateTeamResults
// -----------------------------------------------------------------------

// Local accessor for oTeam protected fields.
class TeamTestAccessor {
public:
  static void setClass(oTeam* t, oClass* cls) { t->Class = cls; }
};

TEST_F(IntegrationTest, CalcTeamResults_TwoTeams_PlacesAssigned) {
  oClass* cls = oe.addClass(1);

  oTeam* t1 = oe.addTeam(1);
  oTeam* t2 = oe.addTeam(2);

  TeamTestAccessor::setClass(t1, cls);
  TeamTestAccessor::setClass(t2, cls);

  // Assign a runner to each team so we can set the relay finish time.
  oRunner* r1 = oe.addRunner(1);
  oRunner* r2 = oe.addRunner(2);
  A::setClass(r1, cls);
  A::setClass(r2, cls);
  int start = 10 * 60 * T;
  A::setStartTime(r1, start);
  A::setStartTime(r2, start);
  A::setFinishTime(r1, start + 300 * T);
  A::setFinishTime(r2, start + 200 * T);
  A::setStatus(r1, StatusOK);
  A::setStatus(r2, StatusOK);

  r1->tInTeam = t1; r1->tLeg = 0; t1->getRunnersRef() = {r1};
  r2->tInTeam = t2; r2->tLeg = 0; t2->getRunnersRef() = {r2};

  oe.calculateTeamResults({1}, oEvent::ResultType::ClassResult);

  EXPECT_EQ(t2->getPlace(), 1);
  EXPECT_EQ(t1->getPlace(), 2);
}

// -----------------------------------------------------------------------
// drawStartList
// -----------------------------------------------------------------------

TEST_F(IntegrationTest, DrawStartList_AssignsSequentialTimes) {
  oClass* cls = oe.addClass(1);

  oRunner* r1 = oe.addRunner(1);
  oRunner* r2 = oe.addRunner(2);
  oRunner* r3 = oe.addRunner(3);

  A::setClass(r1, cls);
  A::setClass(r2, cls);
  A::setClass(r3, cls);

  int first = 100 * 60 * T;  // 100 minutes
  int interval = 2 * 60 * T; // 2 minutes

  oEvent::ClassDrawSpecification spec(1, -1, first, interval);
  oe.drawStartList({spec});

  // Each runner should get first + i * interval.
  EXPECT_EQ(r1->getStartTime(), first);
  EXPECT_EQ(r2->getStartTime(), first + interval);
  EXPECT_EQ(r3->getStartTime(), first + 2 * interval);
}

TEST_F(IntegrationTest, DrawStartList_MultipleClasses_Independent) {
  oClass* cls1 = oe.addClass(1);
  oClass* cls2 = oe.addClass(2);

  oRunner* r1 = oe.addRunner(1);
  oRunner* r2 = oe.addRunner(2);

  A::setClass(r1, cls1);
  A::setClass(r2, cls2);

  oEvent::ClassDrawSpecification s1(1, -1, 60 * T,  60 * T);
  oEvent::ClassDrawSpecification s2(2, -1, 120 * T, 90 * T);

  oe.drawStartList({s1, s2});

  EXPECT_EQ(r1->getStartTime(), 60 * T);
  EXPECT_EQ(r2->getStartTime(), 120 * T);
}

TEST_F(IntegrationTest, DrawStartList_EmptySpec_NoCrash) {
  oe.drawStartList({});
  SUCCEED();
}

// -----------------------------------------------------------------------
// Integration: draw then evaluate results
// -----------------------------------------------------------------------

TEST_F(IntegrationTest, DrawThenCalculate_EndToEnd) {
  oClass* cls = oe.addClass(1);

  oRunner* r1 = oe.addRunner(1);
  oRunner* r2 = oe.addRunner(2);
  oRunner* r3 = oe.addRunner(3);

  A::setClass(r1, cls);
  A::setClass(r2, cls);
  A::setClass(r3, cls);

  // Draw start times.
  int first    = 10 * 60 * T;  // 10 min
  int interval = 2  * 60 * T;  // 2 min
  oe.drawStartList({oEvent::ClassDrawSpecification(1, -1, first, interval)});

  EXPECT_EQ(r1->getStartTime(), first);
  EXPECT_EQ(r2->getStartTime(), first + interval);
  EXPECT_EQ(r3->getStartTime(), first + 2 * interval);

  // Simulate finish: r3 fastest overall (ran 300 s), r1 next (400 s), r2 DNF.
  A::setFinishTime(r1, r1->getStartTime() + 400 * T);
  A::setFinishTime(r2, 0);
  A::setFinishTime(r3, r3->getStartTime() + 300 * T);

  A::setStatus(r1, StatusOK);
  A::setStatus(r2, StatusDNF);
  A::setStatus(r3, StatusOK);

  oe.calculateResults({}, oEvent::ResultType::ClassResult);

  EXPECT_EQ(r3->getPlace(), 1);
  EXPECT_EQ(r1->getPlace(), 2);
  EXPECT_EQ(r2->getPlace(), 0);
}

// -----------------------------------------------------------------------
// newCompetition clears results
// -----------------------------------------------------------------------

TEST_F(IntegrationTest, NewCompetition_ClearsAll) {
  oe.addRunner(1);
  oe.addTeam(1);
  oe.addClass(1);

  oe.newCompetition(L"Fresh");

  EXPECT_TRUE(oe.Runners.empty());
  EXPECT_TRUE(oe.Teams.empty());
  EXPECT_TRUE(oe.Classes.empty());
}

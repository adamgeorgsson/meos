// Unit tests for oTeam relay leg computation (US-003h2).
//
// Covers: relay leg status propagation, relay leg running times,
// start-time propagation via apply(STChange), getLegFinishTime,
// and the bugfix for getLegStatus/deduceComputedStatus returning
// incorrect StatusUnknown when the team has tStatus=DNS/CANCEL
// but a runner exists with StatusUnknown.

#include <gtest/gtest.h>
#include "oTeam.h"
#include "oRunner.h"
#include "oClass.h"
#include "oCourse.h"
#include "oEvent.h"

static constexpr int T = timeConstSecond; // 10 tenths = 1 second

// Forward declaration for friend access in oTeam.h
class RelayTestAccessor {
public:
  static void setClass(oTeam* t, oClass* cls) { t->Class = cls; }
};

// -----------------------------------------------------------------------
// Fixtures / helpers
// -----------------------------------------------------------------------

struct RelayTest : ::testing::Test {
  oEvent ev;

  // Destruction order is REVERSE of declaration order.
  // runners must outlive teams (oRunner::~oRunner accesses tInTeam->Runners).
  // So declare teamStore AFTER runnerStore → teams are destroyed first.
  std::list<oClass>   classStore;
  std::list<oCourse>  courseStore;
  std::list<oRunner>  runnerStore;
  std::list<oTeam>    teamStore;

  // Build an oClass with `numLegs` legs.
  // All legs except leg 0 use `interLegStart` (default STChange).
  oClass* makeClass(int numLegs, StartTypes interLegStart = STChange,
                    int leg0StartTime = 0) {
    classStore.emplace_back(&ev);
    oClass* cls = &classStore.back();
    cls->setNumStages(numLegs);
    for (int k = 0; k < numLegs; k++) {
      cls->setLegType(k, LTNormal);
      cls->setStartType(k, k == 0 ? STTime : interLegStart, false);
    }
    if (leg0StartTime > 0)
      cls->setStartData(0, leg0StartTime);
    return cls;
  }

  // Make a runner with a given start + finish time and status.
  oRunner* makeRunner(int startTime = 0, int finishTime = 0,
                      RunnerStatus st = StatusUnknown) {
    runnerStore.emplace_back(&ev);
    oRunner* r = &runnerStore.back();
    if (startTime  > 0) r->setStartTime(startTime,  true, oBase::ChangeType::Update);
    if (finishTime > 0) r->setFinishTime(finishTime);
    if (st != StatusUnknown) r->setStatus(st, true, oBase::ChangeType::Update);
    return r;
  }

  // Build a team for a class with given runners.
  // Runners vector size must match class stage count.
  oTeam* makeTeam(oClass* cls, std::vector<oRunner*> runners) {
    teamStore.emplace_back(&ev);
    oTeam* t = &teamStore.back();
    RelayTestAccessor::setClass(t, cls);
    std::vector<pRunner> rns(runners.begin(), runners.end());
    t->importRunners(rns);
    return t;
  }
};

// -----------------------------------------------------------------------
// getLegStatus — basic 2-leg relay
// -----------------------------------------------------------------------

TEST_F(RelayTest, LegStatus_BothLegsOK) {
  oClass* cls = makeClass(2);
  // leg0: start=600, finish=1200, RT=600 → OK
  // leg1: start=1200, finish=1900, RT=700 → OK
  oRunner* r0 = makeRunner(600,  1200, StatusOK);
  oRunner* r1 = makeRunner(1200, 1900, StatusOK);
  oTeam*   t  = makeTeam(cls, {r0, r1});

  EXPECT_EQ(t->getLegStatus(0, false, false), StatusOK);
  EXPECT_EQ(t->getLegStatus(1, false, false), StatusOK);
  EXPECT_EQ(t->getLegStatus(-1, false, false), StatusOK);
}

TEST_F(RelayTest, LegStatus_FirstLegDNF_SecondLegUnknown) {
  oClass* cls = makeClass(2);
  oRunner* r0 = makeRunner(600, 0, StatusDNF);
  oRunner* r1 = makeRunner(0,   0, StatusUnknown);
  oTeam*   t  = makeTeam(cls, {r0, r1});

  // Leg 0: DNF
  EXPECT_EQ(t->getLegStatus(0, false, false), StatusDNF);
  // Leg 1: accumulated s=DNF; r1 has status=Unknown (st==0):
  //   since s != StatusOK, returns RunnerStatus(s=DNF) = DNF
  EXPECT_EQ(t->getLegStatus(1, false, false), StatusDNF);
}

TEST_F(RelayTest, LegStatus_FirstLegOK_SecondLegStillRunning) {
  oClass* cls = makeClass(2);
  oRunner* r0 = makeRunner(600, 1200, StatusOK);
  oRunner* r1 = makeRunner(1200, 0,   StatusUnknown);
  oTeam*   t  = makeTeam(cls, {r0, r1});

  EXPECT_EQ(t->getLegStatus(0, false, false), StatusOK);
  // Leg 1: accumulated s=OK; r1 has st==0:
  //   returns RunnerStatus(1 == 1 ? 0 : 1) = StatusUnknown (still running)
  EXPECT_EQ(t->getLegStatus(1, false, false), StatusUnknown);
}

TEST_F(RelayTest, LegStatus_FirstLegOK_SecondLegDNF) {
  oClass* cls = makeClass(2);
  oRunner* r0 = makeRunner(600, 1200, StatusOK);
  oRunner* r1 = makeRunner(1200, 0,  StatusDNF);
  oTeam*   t  = makeTeam(cls, {r0, r1});

  EXPECT_EQ(t->getLegStatus(0, false, false), StatusOK);
  EXPECT_EQ(t->getLegStatus(1, false, false), StatusDNF);
}

TEST_F(RelayTest, LegStatus_ThreeLeg_MiddleLegDNF) {
  oClass* cls = makeClass(3);
  oRunner* r0 = makeRunner(600, 1200, StatusOK);
  oRunner* r1 = makeRunner(1200, 0,  StatusDNF);
  oRunner* r2 = makeRunner(0,    0,  StatusUnknown);
  oTeam*   t  = makeTeam(cls, {r0, r1, r2});

  EXPECT_EQ(t->getLegStatus(0, false, false), StatusOK);
  EXPECT_EQ(t->getLegStatus(1, false, false), StatusDNF);
  // Leg 2: s=DNF after leg1; r2 has Unknown → returns DNF
  EXPECT_EQ(t->getLegStatus(2, false, false), StatusDNF);
}

TEST_F(RelayTest, LegStatus_NullRunners_TreatedAsDNS) {
  oClass* cls = makeClass(2);
  oTeam*  t   = makeTeam(cls, {nullptr, nullptr});

  // null runner treated as StatusDNS by getLegStatus
  EXPECT_EQ(t->getLegStatus(0, false, false), StatusDNS);
}

// -----------------------------------------------------------------------
// Bug-fix tests: getLegStatus must propagate tStatus DNS/CANCEL when
// all runners have StatusUnknown (i.e., the early-exit path must check
// the team's tStatus before returning StatusUnknown).
// -----------------------------------------------------------------------

TEST_F(RelayTest, LegStatus_TeamDNS_RunnerHasUnknown_ReturnsDNS) {
  oClass* cls = makeClass(2);
  // Runners exist but have no result (StatusUnknown = 0).
  oRunner* r0 = makeRunner();
  oRunner* r1 = makeRunner();
  oTeam*   t  = makeTeam(cls, {r0, r1});

  // Explicitly set the team's status to DNS (e.g. via import/API).
  t->setStatus(StatusDNS, true, oBase::ChangeType::Update);

  // getLegStatus must not return StatusUnknown for any leg.
  EXPECT_EQ(t->getLegStatus(0, false, false), StatusDNS);
  EXPECT_EQ(t->getLegStatus(-1, false, false), StatusDNS);
}

TEST_F(RelayTest, LegStatus_TeamCANCEL_RunnerHasUnknown_ReturnsCANCEL) {
  oClass* cls = makeClass(2);
  oRunner* r0 = makeRunner();
  oRunner* r1 = makeRunner();
  oTeam*   t  = makeTeam(cls, {r0, r1});

  t->setStatus(StatusCANCEL, true, oBase::ChangeType::Update);

  EXPECT_EQ(t->getLegStatus(0, false, false), StatusCANCEL);
  EXPECT_EQ(t->getLegStatus(-1, false, false), StatusCANCEL);
}

TEST_F(RelayTest, LegStatus_TeamDNS_NullRunners_ReturnsDNS) {
  // When Runners vector is empty, getLegStatus checks size boundary.
  // But with null runners in a class, the null-runner branch gives StatusDNS
  // immediately regardless of tStatus. This test verifies consistency.
  oClass* cls = makeClass(1);
  oTeam*  t   = makeTeam(cls, {nullptr});

  t->setStatus(StatusDNS, true, oBase::ChangeType::Update);
  // null runner → st = StatusDNS (line: int st = Runners[i] ? ... : StatusDNS)
  EXPECT_EQ(t->getLegStatus(0, false, false), StatusDNS);
}

// -----------------------------------------------------------------------
// getLegRunningTime
// -----------------------------------------------------------------------

TEST_F(RelayTest, LegRunningTime_FirstLeg_CorrectFromFinishMinusTeamStart) {
  // 1-leg relay (simple)
  oClass* cls = makeClass(1);
  // Team start at 600, runner finish at 1200 → RT = 600
  oRunner* r0 = makeRunner(600, 1200, StatusOK);
  oTeam*   t  = makeTeam(cls, {r0});
  t->setStartTime(600, true, oBase::ChangeType::Update);

  int rt = t->getLegRunningTime(0, false, false);
  EXPECT_EQ(rt, 600);   // 600 tenths = 60 s
}

TEST_F(RelayTest, LegRunningTime_TwoLegs_AccumulateCorrectly) {
  oClass* cls = makeClass(2);
  // Team start=600, r0 finish=1200 (RT=600), r1 start=1200 finish=2000 (RT=800)
  oRunner* r0 = makeRunner(600,  1200, StatusOK);
  oRunner* r1 = makeRunner(1200, 2000, StatusOK);
  oTeam*   t  = makeTeam(cls, {r0, r1});
  t->setStartTime(600, true, oBase::ChangeType::Update);

  // Leg 0 total time = r0.finish - team_start = 600
  EXPECT_EQ(t->getLegRunningTime(0, false, false), 600);
  // Leg 1 total time = r1.finish - team_start = 1400
  EXPECT_EQ(t->getLegRunningTime(1, false, false), 1400);
  EXPECT_EQ(t->getLegRunningTime(-1, false, false), 1400);
}

TEST_F(RelayTest, LegRunningTime_RunnerDNF_ReturnsZero) {
  oClass* cls = makeClass(2);
  oRunner* r0 = makeRunner(600, 0, StatusDNF);
  oRunner* r1 = makeRunner(0,   0, StatusUnknown);
  oTeam*   t  = makeTeam(cls, {r0, r1});
  t->setStartTime(600, true, oBase::ChangeType::Update);

  // prelStatusOK returns false for DNF → returns 0
  EXPECT_EQ(t->getLegRunningTime(0, false, false), 0);
  EXPECT_EQ(t->getLegRunningTime(1, false, false), 0);
}

// -----------------------------------------------------------------------
// getLegFinishTime
// -----------------------------------------------------------------------

TEST_F(RelayTest, LegFinishTime_OneLeg) {
  oClass* cls = makeClass(1);
  oRunner* r0 = makeRunner(600, 1800, StatusOK);
  oTeam*   t  = makeTeam(cls, {r0});

  EXPECT_EQ(t->getLegFinishTime(0),  1800);
  EXPECT_EQ(t->getLegFinishTime(-1), 1800);
}

TEST_F(RelayTest, LegFinishTime_TwoLegs) {
  oClass* cls = makeClass(2);
  oRunner* r0 = makeRunner(600,  1200, StatusOK);
  oRunner* r1 = makeRunner(1200, 2500, StatusOK);
  oTeam*   t  = makeTeam(cls, {r0, r1});

  EXPECT_EQ(t->getLegFinishTime(0),  1200);
  EXPECT_EQ(t->getLegFinishTime(1),  2500);
  EXPECT_EQ(t->getLegFinishTime(-1), 2500);
}

TEST_F(RelayTest, LegFinishTime_NoRunner_ReturnsZero) {
  oClass* cls = makeClass(1);
  oTeam*  t   = makeTeam(cls, {nullptr});
  EXPECT_EQ(t->getLegFinishTime(0), 0);
}

// -----------------------------------------------------------------------
// apply() — start-time propagation (STChange)
// -----------------------------------------------------------------------

TEST_F(RelayTest, Apply_STChange_PropagatesStartTimeToLeg1) {
  // Class: leg0=STTime(start=600), leg1=STChange
  oClass* cls = makeClass(2, STChange, 600);

  // r0: starts at 600 (class default), finishes at 1300
  oRunner* r0 = makeRunner(600, 1300, StatusOK);
  // r1: start not yet set
  oRunner* r1 = makeRunner(0,   2200, StatusOK);

  oTeam* t = makeTeam(cls, {r0, r1});
  t->apply(oBase::ChangeType::Update, nullptr);

  // After apply, r1.startTime should equal r0.finishTime = 1300
  EXPECT_EQ(r1->getStartTime(), 1300);
}

TEST_F(RelayTest, Apply_STChange_TeamStartEqualsFirstRunnerStart) {
  oClass* cls = makeClass(2, STChange, 600);
  oRunner* r0 = makeRunner(600, 1300, StatusOK);
  oRunner* r1 = makeRunner(0,   2200, StatusOK);
  oTeam*   t  = makeTeam(cls, {r0, r1});
  t->apply(oBase::ChangeType::Update, nullptr);

  EXPECT_EQ(t->getStartTime(), r0->getStartTime());
}

TEST_F(RelayTest, Apply_SetsTeamFinishTimeFromLastRunner) {
  oClass* cls = makeClass(2, STChange, 600);
  oRunner* r0 = makeRunner(600, 1300, StatusOK);
  oRunner* r1 = makeRunner(0,   2200, StatusOK);
  oTeam*   t  = makeTeam(cls, {r0, r1});
  t->apply(oBase::ChangeType::Update, nullptr);

  EXPECT_EQ(t->getFinishTime(), r1->getFinishTime());
}

TEST_F(RelayTest, Apply_SetsTeamStatusOKWhenAllLegsOK) {
  oClass* cls = makeClass(2, STChange, 600);
  oRunner* r0 = makeRunner(600, 1300, StatusOK);
  oRunner* r1 = makeRunner(0,   2200, StatusOK);
  oTeam*   t  = makeTeam(cls, {r0, r1});
  t->apply(oBase::ChangeType::Update, nullptr);

  EXPECT_EQ(t->getStatus(), StatusOK);
}

TEST_F(RelayTest, Apply_SetsTeamStatusDNFWhenFirstLegDNF) {
  oClass* cls = makeClass(2, STChange, 600);
  oRunner* r0 = makeRunner(600, 0, StatusDNF);
  oRunner* r1 = makeRunner(0,   0, StatusUnknown);
  oTeam*   t  = makeTeam(cls, {r0, r1});
  t->apply(oBase::ChangeType::Update, nullptr);

  // getLegStatus(-1) = DNF because r0 is DNF and r1 has Unknown but s=DNF
  EXPECT_EQ(t->getStatus(), StatusDNF);
}

TEST_F(RelayTest, Apply_STChange_DNFRunnerGivesZeroStartToNextLeg) {
  // When leg 0 DNFs (finish=0), STChange logic: ft = r0.getFinishTime() = 0,
  // no restart defined → next runner gets start 0 (no start propagation).
  oClass* cls = makeClass(2, STChange, 600);
  oRunner* r0 = makeRunner(600, 0, StatusDNF);
  oRunner* r1 = makeRunner(0,   0, StatusUnknown);
  oTeam*   t  = makeTeam(cls, {r0, r1});
  t->apply(oBase::ChangeType::Update, nullptr);

  // r1 gets start=0 because r0 didn't finish
  EXPECT_EQ(r1->getStartTime(), 0);
}

// -----------------------------------------------------------------------
// Three-leg relay (STChange chain)
// -----------------------------------------------------------------------

TEST_F(RelayTest, ThreeLeg_AllOK_TimeAccumulatesCorrectly) {
  oClass* cls = makeClass(3, STChange, 600);
  // r0: 600→1200 (RT=600), r1: 1200→1900 (RT=700), r2: 1900→2800 (RT=900)
  oRunner* r0 = makeRunner(600,  1200, StatusOK);
  oRunner* r1 = makeRunner(1200, 1900, StatusOK);
  oRunner* r2 = makeRunner(1900, 2800, StatusOK);
  oTeam*   t  = makeTeam(cls, {r0, r1, r2});
  t->setStartTime(600, true, oBase::ChangeType::Update);

  EXPECT_EQ(t->getLegRunningTime(0, false, false), 600);
  EXPECT_EQ(t->getLegRunningTime(1, false, false), 1300);
  EXPECT_EQ(t->getLegRunningTime(2, false, false), 2200);
  EXPECT_EQ(t->getLegStatus(0, false, false), StatusOK);
  EXPECT_EQ(t->getLegStatus(1, false, false), StatusOK);
  EXPECT_EQ(t->getLegStatus(2, false, false), StatusOK);
}

TEST_F(RelayTest, ThreeLeg_Apply_StartTimesPropagate) {
  oClass* cls = makeClass(3, STChange, 600);
  oRunner* r0 = makeRunner(600, 1200, StatusOK);
  oRunner* r1 = makeRunner(0,   1900, StatusOK);
  oRunner* r2 = makeRunner(0,   2800, StatusOK);
  oTeam*   t  = makeTeam(cls, {r0, r1, r2});
  t->apply(oBase::ChangeType::Update, nullptr);

  EXPECT_EQ(r1->getStartTime(), 1200);   // r0 finish
  EXPECT_EQ(r2->getStartTime(), 1900);   // r1 finish
  EXPECT_EQ(t->getFinishTime(), 2800);
}

// -----------------------------------------------------------------------
// deduceComputedStatus — same DNS/CANCEL propagation bug
// -----------------------------------------------------------------------

TEST_F(RelayTest, DeduceComputedStatus_TeamDNS_RunnerUnknown) {
  oClass* cls = makeClass(2);
  oRunner* r0 = makeRunner();
  oRunner* r1 = makeRunner();
  oTeam*   t  = makeTeam(cls, {r0, r1});
  t->setStatus(StatusDNS, true, oBase::ChangeType::Update);

  // deduceComputedStatus uses tStatus as fallback the same way
  RunnerStatus s = t->deduceComputedStatus();
  EXPECT_EQ(s, StatusDNS);
}

// -----------------------------------------------------------------------
// getLegStartTime
// -----------------------------------------------------------------------

TEST_F(RelayTest, LegStartTime_FirstAndSecondLeg) {
  oClass* cls = makeClass(2, STChange, 600);
  oRunner* r0 = makeRunner(600, 1300, StatusOK);
  oRunner* r1 = makeRunner(1300, 2100, StatusOK);
  oTeam*   t  = makeTeam(cls, {r0, r1});

  EXPECT_EQ(t->getLegStartTime(0), 600);
  EXPECT_EQ(t->getLegStartTime(1), 1300);
}

// -----------------------------------------------------------------------
// getNumRunners / getNumAssignedRunners after import
// -----------------------------------------------------------------------

TEST_F(RelayTest, NumRunners_AfterImport) {
  oClass* cls = makeClass(3);
  oRunner* r0 = makeRunner();
  oRunner* r2 = makeRunner();
  // middle leg is null
  oTeam* t = makeTeam(cls, {r0, nullptr, r2});

  EXPECT_EQ(t->getNumRunners(), 3);
  EXPECT_EQ(t->getNumAssignedRunners(), 2);
}

// -----------------------------------------------------------------------
// STTime start (non-relay): legs get class start time
// -----------------------------------------------------------------------

TEST_F(RelayTest, STTime_LegGetsClassStartData) {
  oClass* cls = makeClass(2, STTime /*both legs STTime*/, 600);
  cls->setStartData(1, 1200);   // leg 1 starts at 1200

  oRunner* r0 = makeRunner();
  oRunner* r1 = makeRunner();
  oTeam*   t  = makeTeam(cls, {r0, r1});
  t->apply(oBase::ChangeType::Update, nullptr);

  EXPECT_EQ(r0->getStartTime(), 600);
  EXPECT_EQ(r1->getStartTime(), 1200);
}


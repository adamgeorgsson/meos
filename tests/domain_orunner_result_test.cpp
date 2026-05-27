// Unit tests for oRunner::evaluateCard — status computation and split times (US-003g2).
//
// Access to protected members is granted via friend class RunnerResultTestAccessor
// declared in oAbstractRunner.h, oRunner.h, oCard.h, and oCourse.h.

#include <gtest/gtest.h>
#include "oRunner.h"
#include "oCard.h"
#include "oCourse.h"
#include "oControl.h"
#include "oEvent.h"
#include "oBase.h"

static constexpr int T = timeConstSecond; // tenths-of-seconds

// -----------------------------------------------------------------------
// Test accessor (declared friend in oAbstractRunner, oRunner, oCard, oCourse)
// -----------------------------------------------------------------------
class RunnerResultTestAccessor {
public:
  static void setStartTime(oRunner* r, int t)       { r->startTime = t; r->tStartTime = t; }
  static void setStatus(oRunner* r, RunnerStatus s) { r->status = s; r->tStatus = s; }
  static void setCard(oRunner* r, oCard* c)         { r->Card = c; }
  static void setCourse(oRunner* r, oCourse* crs)   { r->Course = crs; }
  static void setClass(oRunner* r, oClass* cls)     { r->Class = cls; }
  static void setFinishTime(oRunner* r, int t)      { r->FinishTime = t; }
  static void setUseStartPunch(oRunner* r, bool v)  { r->tUseStartPunch = v; }
  static RunnerStatus getTStatus(const oRunner* r)  { return r->tStatus; }
  static void setCardOwner(oCard* c, oRunner* r)    { c->tOwner = r; }
  static oPunchList& getPunches(oCard* c)           { return c->punches; }
  static void insertControl(oCourse* crs, int idx, oControl* ctrl) {
    crs->controls.insert(crs->controls.begin() + idx, ctrl);
  }
};

// -----------------------------------------------------------------------
// Fixture
// -----------------------------------------------------------------------
struct ResultTest : ::testing::Test {
  using A = RunnerResultTestAccessor;

  oEvent      ev;
  oRunner*    r    = nullptr;
  oCard*      card = nullptr;
  oClass*     cls  = nullptr;

  std::list<oCourse>  courses;
  std::list<oClass>   classStore;

  void SetUp() override {
    r    = new oRunner(&ev);
    card = new oCard(&ev);
    cls  = makeClass();
    A::setClass(r, cls);
  }

  void TearDown() override {
    delete card;
    delete r;
  }

  oClass* makeClass() {
    classStore.emplace_back(&ev);
    return &classStore.back();
  }

  oCourse* makeCourse(const std::vector<int>& codes) {
    courses.emplace_back(&ev);
    oCourse* crs = &courses.back();
    for (int code : codes)
      crs->addControl(code);
    return crs;
  }

  void addPunch(int type, int time) {
    card->addPunch(type, time, 0, 0, oCard::PunchOrigin::Original);
  }

  RunnerStatus evaluate(oCourse* crs = nullptr) {
    A::setCard(r, card);
    A::setCardOwner(card, r);
    if (crs) A::setCourse(r, crs);
    std::vector<std::pair<int, oControl*>> mp;
    r->evaluateCard(true, mp, 0, oBase::ChangeType::Update);
    return A::getTStatus(r);
  }

  RunnerStatus evaluateGetMP(oCourse* crs,
                              std::vector<std::pair<int, oControl*>>& mp) {
    A::setCard(r, card);
    A::setCardOwner(card, r);
    if (crs) A::setCourse(r, crs);
    r->evaluateCard(true, mp, 0, oBase::ChangeType::Update);
    return A::getTStatus(r);
  }
};

// -----------------------------------------------------------------------
// No card → returns false, status unchanged
// -----------------------------------------------------------------------
TEST_F(ResultTest, NoCard_ReturnsFalse_StatusUnchanged) {
  A::setStatus(r, StatusUnknown);
  std::vector<std::pair<int,oControl*>> mp;
  bool res = r->evaluateCard(true, mp, 0, oBase::ChangeType::Update);
  EXPECT_FALSE(res);
  EXPECT_EQ(A::getTStatus(r), StatusUnknown);
}

// -----------------------------------------------------------------------
// No course → status from finish punch
// -----------------------------------------------------------------------
TEST_F(ResultTest, NoCourse_WithFinish_StatusOK) {
  A::setStartTime(r, 100 * T);
  addPunch(oPunch::PunchStart,  100 * T);
  addPunch(oPunch::PunchFinish, 160 * T);
  EXPECT_EQ(evaluate(nullptr), StatusOK);
  EXPECT_EQ(r->getRunningTime(false), 60 * T);
}

TEST_F(ResultTest, NoCourse_NoFinish_StatusDNF) {
  A::setStartTime(r, 100 * T);
  addPunch(oPunch::PunchStart, 100 * T);
  EXPECT_EQ(evaluate(nullptr), StatusDNF);
}

// -----------------------------------------------------------------------
// With course — OK: all controls punched + finish
// -----------------------------------------------------------------------
TEST_F(ResultTest, Course_AllPunched_OK) {
  oCourse* crs = makeCourse({31, 32, 33});
  A::setStartTime(r, 100 * T);
  addPunch(oPunch::PunchStart,  100 * T);
  addPunch(31,                   110 * T);
  addPunch(32,                   120 * T);
  addPunch(33,                   130 * T);
  addPunch(oPunch::PunchFinish,  140 * T);
  EXPECT_EQ(evaluate(crs), StatusOK);
  EXPECT_EQ(r->getRunningTime(false), 40 * T);
}

// -----------------------------------------------------------------------
// MP: one control missing
// -----------------------------------------------------------------------
TEST_F(ResultTest, Course_MissingPunch_MP) {
  oCourse* crs = makeCourse({31, 32, 33});
  A::setStartTime(r, 100 * T);
  addPunch(oPunch::PunchStart,  100 * T);
  addPunch(31,                   110 * T);
  addPunch(33,                   130 * T);  // 32 missing
  addPunch(oPunch::PunchFinish,  140 * T);
  std::vector<std::pair<int, oControl*>> mp;
  EXPECT_EQ(evaluateGetMP(crs, mp), StatusMP);
  EXPECT_FALSE(mp.empty());
  EXPECT_EQ(mp[0].first, 32);
}

// -----------------------------------------------------------------------
// DNF: no finish punch
// -----------------------------------------------------------------------
TEST_F(ResultTest, Course_NoFinish_DNF) {
  oCourse* crs = makeCourse({31, 32});
  A::setStartTime(r, 100 * T);
  addPunch(oPunch::PunchStart,  100 * T);
  addPunch(31,                   110 * T);
  addPunch(32,                   120 * T);
  EXPECT_EQ(evaluate(crs), StatusDNF);
}

// -----------------------------------------------------------------------
// DQ: payBeforeResult flag
// -----------------------------------------------------------------------
TEST_F(ResultTest, PayBeforeResult_DQ) {
  oCourse* crs = makeCourse({31});
  r->setPayBeforeResult(true);
  A::setStartTime(r, 100 * T);
  addPunch(oPunch::PunchStart,  100 * T);
  addPunch(31,                   110 * T);
  addPunch(oPunch::PunchFinish,  120 * T);
  EXPECT_EQ(evaluate(crs), StatusDQ);
}

// -----------------------------------------------------------------------
// Split times recorded correctly
// -----------------------------------------------------------------------
TEST_F(ResultTest, SplitTimes_Populated) {
  oCourse* crs = makeCourse({31, 32, 33});
  A::setStartTime(r, 0);
  addPunch(31,                  110 * T);
  addPunch(32,                  120 * T);
  addPunch(33,                  130 * T);
  addPunch(oPunch::PunchFinish, 140 * T);

  A::setCard(r, card); A::setCardOwner(card, r); A::setCourse(r, crs);
  std::vector<std::pair<int,oControl*>> mp;
  r->evaluateCard(true, mp, 0, oBase::ChangeType::Update);

  const auto& splits = r->getSplitTimes(false);
  ASSERT_EQ((int)splits.size(), 3);
  EXPECT_TRUE(splits[0].hasTime());
  EXPECT_TRUE(splits[1].hasTime());
  EXPECT_TRUE(splits[2].hasTime());
  EXPECT_EQ(splits[0].getTime(false), 110 * T);
  EXPECT_EQ(splits[1].getTime(false), 120 * T);
  EXPECT_EQ(splits[2].getTime(false), 130 * T);
}

// -----------------------------------------------------------------------
// Split: missing control → SplitStatus::Missing
// -----------------------------------------------------------------------
TEST_F(ResultTest, SplitTimes_MissingControl_Missing) {
  oCourse* crs = makeCourse({31, 32, 33});
  A::setStartTime(r, 100 * T);
  addPunch(oPunch::PunchStart,  100 * T);
  addPunch(31,                   110 * T);
  // 32 skipped
  addPunch(33,                   130 * T);
  addPunch(oPunch::PunchFinish,  140 * T);

  A::setCard(r, card); A::setCardOwner(card, r); A::setCourse(r, crs);
  std::vector<std::pair<int,oControl*>> mp;
  r->evaluateCard(true, mp, 0, oBase::ChangeType::Update);

  const auto& splits = r->getSplitTimes(false);
  ASSERT_EQ((int)splits.size(), 3);
  EXPECT_TRUE(splits[0].hasTime());
  EXPECT_TRUE(splits[1].isMissing());  // control 32
  EXPECT_TRUE(splits[2].hasTime());
}

// -----------------------------------------------------------------------
// Status: DNS preserved when no card
// -----------------------------------------------------------------------
TEST_F(ResultTest, DNS_NoCard_Preserved) {
  A::setStatus(r, StatusDNS);
  std::vector<std::pair<int,oControl*>> mp;
  r->evaluateCard(true, mp, 0, oBase::ChangeType::Update);
  EXPECT_EQ(A::getTStatus(r), StatusDNS);
}

// -----------------------------------------------------------------------
// Running time computed correctly
// -----------------------------------------------------------------------
TEST_F(ResultTest, RunningTime_AfterEvaluate) {
  oCourse* crs = makeCourse({31});
  A::setStartTime(r, 100 * T);
  addPunch(oPunch::PunchStart,  100 * T);
  addPunch(31,                   115 * T);
  addPunch(oPunch::PunchFinish,  130 * T);
  evaluate(crs);
  EXPECT_EQ(r->getRunningTime(false), 30 * T);
}

// -----------------------------------------------------------------------
// Start time updated from start punch
// -----------------------------------------------------------------------
TEST_F(ResultTest, StartTime_FromStartPunch) {
  oCourse* crs = makeCourse({31});
  A::setStartTime(r, 100 * T);
  r->tStartTime = 100 * T;
  A::setUseStartPunch(r, true);
  addPunch(oPunch::PunchStart,  105 * T);
  addPunch(31,                   115 * T);
  addPunch(oPunch::PunchFinish,  125 * T);
  evaluate(crs);
  EXPECT_EQ(r->tStartTime, 105 * T);
}

// -----------------------------------------------------------------------
// MP when only start/finish punched (all controls missing)
// -----------------------------------------------------------------------
TEST_F(ResultTest, Course_OnlyStartFinish_MP) {
  oCourse* crs = makeCourse({31, 32});
  A::setStartTime(r, 100 * T);
  addPunch(oPunch::PunchStart,  100 * T);
  addPunch(oPunch::PunchFinish, 140 * T);
  std::vector<std::pair<int,oControl*>> mp;
  EXPECT_EQ(evaluateGetMP(crs, mp), StatusMP);
  EXPECT_EQ((int)mp.size(), 2);
}

// -----------------------------------------------------------------------
// evaluateCard returns true when card matched
// -----------------------------------------------------------------------
TEST_F(ResultTest, EvaluateReturnsTrue_WithCourse) {
  oCourse* crs = makeCourse({31});
  A::setStartTime(r, 100 * T);
  addPunch(31,                   110 * T);
  addPunch(oPunch::PunchFinish,  120 * T);
  A::setCard(r, card); A::setCardOwner(card, r); A::setCourse(r, crs);
  std::vector<std::pair<int,oControl*>> mp;
  EXPECT_TRUE(r->evaluateCard(true, mp, 0, oBase::ChangeType::Update));
}

// -----------------------------------------------------------------------
// OC flag: OutOfCompetition overrides OK
// -----------------------------------------------------------------------
TEST_F(ResultTest, OutsideCompetition_Flag) {
  oCourse* crs = makeCourse({31});
  A::setStartTime(r, 100 * T);
  r->setFlag(oAbstractRunner::TransferFlags::FlagOutsideCompetition, true);
  addPunch(oPunch::PunchStart,  100 * T);
  addPunch(31,                   110 * T);
  addPunch(oPunch::PunchFinish,  120 * T);
  EXPECT_EQ(evaluate(crs), StatusOutOfCompetition);
}

// -----------------------------------------------------------------------
// NoTiming flag overrides OK
// -----------------------------------------------------------------------
TEST_F(ResultTest, NoTiming_Flag) {
  oCourse* crs = makeCourse({31});
  A::setStartTime(r, 100 * T);
  r->setFlag(oAbstractRunner::TransferFlags::FlagNoTiming, true);
  addPunch(oPunch::PunchStart,  100 * T);
  addPunch(31,                   110 * T);
  addPunch(oPunch::PunchFinish,  120 * T);
  EXPECT_EQ(evaluate(crs), StatusNoTiming);
}

// -----------------------------------------------------------------------
// NoTiming via class setting
// -----------------------------------------------------------------------
TEST_F(ResultTest, NoTiming_ViaClass) {
  oCourse* crs = makeCourse({31});
  A::setStartTime(r, 100 * T);
  cls->setNoTiming(true);
  addPunch(oPunch::PunchStart,  100 * T);
  addPunch(31,                   110 * T);
  addPunch(oPunch::PunchFinish,  120 * T);
  EXPECT_EQ(evaluate(crs), StatusNoTiming);
}

// -----------------------------------------------------------------------
// Re-evaluate: clears old split times on second call
// -----------------------------------------------------------------------
TEST_F(ResultTest, ReEvaluate_ClearsSplits) {
  oCourse* crs = makeCourse({31, 32});
  A::setStartTime(r, 100 * T);
  addPunch(oPunch::PunchStart,  100 * T);
  addPunch(31,                   110 * T);
  addPunch(32,                   120 * T);
  addPunch(oPunch::PunchFinish,  130 * T);
  evaluate(crs);
  EXPECT_EQ(A::getTStatus(r), StatusOK);

  // Second evaluate without finish → DNF
  A::getPunches(card).clear();
  addPunch(oPunch::PunchStart,  100 * T);
  addPunch(31,                   110 * T);
  addPunch(32,                   120 * T);
  A::setFinishTime(r, 0);
  std::vector<std::pair<int,oControl*>> mp;
  r->evaluateCard(true, mp, 0, oBase::ChangeType::Update);
  EXPECT_EQ(A::getTStatus(r), StatusDNF);
}

// -----------------------------------------------------------------------
// Multiple controls (StatusMultiple): both codes must be punched
// -----------------------------------------------------------------------
TEST_F(ResultTest, MultipleControl_BothPunched_OK) {
  oControl* ctrl = ev.getControl(41, true);
  ctrl->set(41, 41, L"");
  ctrl->setNumbers(L"41;42");
  ctrl->setStatus(oControl::ControlStatus::StatusMultiple);

  courses.emplace_back(&ev);
  oCourse* crs = &courses.back();
  A::insertControl(crs, 0, ctrl);

  A::setStartTime(r, 100 * T);
  addPunch(oPunch::PunchStart,  100 * T);
  addPunch(41,                   110 * T);
  addPunch(42,                   112 * T);
  addPunch(oPunch::PunchFinish,  120 * T);

  std::vector<std::pair<int,oControl*>> mp;
  EXPECT_EQ(evaluateGetMP(crs, mp), StatusOK);
  EXPECT_TRUE(mp.empty());
}

TEST_F(ResultTest, MultipleControl_OneMissing_MP) {
  oControl* ctrl = ev.getControl(43, true);
  ctrl->set(43, 43, L"");
  ctrl->setNumbers(L"43;44");
  ctrl->setStatus(oControl::ControlStatus::StatusMultiple);

  courses.emplace_back(&ev);
  oCourse* crs = &courses.back();
  A::insertControl(crs, 0, ctrl);

  A::setStartTime(r, 100 * T);
  addPunch(oPunch::PunchStart, 100 * T);
  addPunch(43,                  110 * T);
  addPunch(oPunch::PunchFinish, 120 * T);

  std::vector<std::pair<int,oControl*>> mp;
  EXPECT_EQ(evaluateGetMP(crs, mp), StatusMP);
  EXPECT_FALSE(mp.empty());
}

// -----------------------------------------------------------------------
// Bad control (StatusBad) — completed even if not punched
// -----------------------------------------------------------------------
TEST_F(ResultTest, BadControl_CompletedEvenIfNotPunched) {
  oControl* ctrl = ev.getControl(51, true);
  ctrl->set(51, 51, L"");
  ctrl->setStatus(oControl::ControlStatus::StatusBad);

  oCourse* crs = makeCourse({31});
  A::insertControl(crs, 0, ctrl);

  A::setStartTime(r, 100 * T);
  addPunch(oPunch::PunchStart,  100 * T);
  addPunch(31,                   110 * T);
  addPunch(oPunch::PunchFinish,  120 * T);

  std::vector<std::pair<int,oControl*>> mp;
  EXPECT_EQ(evaluateGetMP(crs, mp), StatusOK) << "Bad control must not cause MP";
  EXPECT_TRUE(mp.empty());
}

// -----------------------------------------------------------------------
// Max running time exceeded → StatusMAX
// -----------------------------------------------------------------------
TEST_F(ResultTest, MaxTime_Exceeded_StatusMAX) {
  oCourse* crs = makeCourse({31});
  cls->tMaxTime = 20 * T;
  A::setStartTime(r, 100 * T);
  addPunch(oPunch::PunchStart,  100 * T);
  addPunch(31,                   110 * T);
  addPunch(oPunch::PunchFinish,  125 * T);  // RT=25 > 20 → MAX
  EXPECT_EQ(evaluate(crs), StatusMAX);
}

// -----------------------------------------------------------------------
// Empty course (0 controls): OK if finish present
// -----------------------------------------------------------------------
TEST_F(ResultTest, EmptyCourse_FinishPresent_OK) {
  oCourse* crs = makeCourse({});
  A::setStartTime(r, 100 * T);
  addPunch(oPunch::PunchStart,  100 * T);
  addPunch(oPunch::PunchFinish, 110 * T);
  EXPECT_EQ(evaluate(crs), StatusOK);
}

// -----------------------------------------------------------------------
// doApply=false: evaluateCard still computes result status
// -----------------------------------------------------------------------
TEST_F(ResultTest, DoApplyFalse_ComputesResult) {
  oCourse* crs = makeCourse({31});
  A::setStartTime(r, 100 * T);
  r->tStartTime = 100 * T;
  addPunch(31,                   110 * T);
  addPunch(oPunch::PunchFinish,  120 * T);

  A::setCard(r, card); A::setCardOwner(card, r); A::setCourse(r, crs);
  std::vector<std::pair<int,oControl*>> mp;
  r->evaluateCard(false, mp, 0, oBase::ChangeType::Update);
  EXPECT_EQ(A::getTStatus(r), StatusOK);
}

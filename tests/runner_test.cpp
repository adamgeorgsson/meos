// runner_test.cpp — Unit tests for oRunner and oAbstractRunner (US-003g)
#include <gtest/gtest.h>
#include "oRunner.h"
#include "oEvent.h"
#include "oCourse.h"
#include "oClass.h"
#include "oClub.h"
#include "oCard.h"
#include "oPunch.h"

using std::string;
using std::wstring;

class RunnerTest : public ::testing::Test {
protected:
  oEvent oe;
  void SetUp() override { setlocale(LC_ALL, "C.UTF-8"); }
};

// ── Construction ──────────────────────────────────────────────────────────────

TEST_F(RunnerTest, DefaultConstruction) {
  oRunner r(&oe);
  EXPECT_GT(r.getId(), 0);
  EXPECT_EQ(r.getStatus(), StatusUnknown);
  EXPECT_EQ(r.getStartTime(), 0);
  EXPECT_EQ(r.getFinishTime(), 0);
}

TEST_F(RunnerTest, ConstructionWithId) {
  oRunner r(&oe, 42);
  EXPECT_EQ(r.getId(), 42);
}

// ── Name ──────────────────────────────────────────────────────────────────────

TEST_F(RunnerTest, SetGetName) {
  oRunner r(&oe);
  r.setName(L"Anna Johansson", false);
  EXPECT_EQ(r.getName(), L"Anna Johansson");
}

TEST_F(RunnerTest, SetNameNormalizesWhitespace) {
  oRunner r(&oe);
  r.setName(L"Anna  Johansson", false);
  wstring n = r.getName();
  EXPECT_EQ(n.find(L"  "), wstring::npos); // No double spaces
}

TEST_F(RunnerTest, SetNameTrimsLeadingWhitespace) {
  oRunner r(&oe);
  r.setName(L"  Anna", false);
  EXPECT_EQ(r.getName(), L"Anna");
}

TEST_F(RunnerTest, SetNameTrimsTrailingWhitespace) {
  oRunner r(&oe);
  r.setName(L"Anna  ", false);
  EXPECT_EQ(r.getName(), L"Anna");
}

// ── Status ────────────────────────────────────────────────────────────────────

TEST_F(RunnerTest, SetGetStatus) {
  oRunner r(&oe);
  r.setStatus(StatusOK, true, oBase::ChangeType::Update, false);
  EXPECT_EQ(r.getStatus(), StatusOK);
}

TEST_F(RunnerTest, SetStatusDNS) {
  oRunner r(&oe);
  r.setStatus(StatusDNS, true, oBase::ChangeType::Update, false);
  EXPECT_EQ(r.getStatus(), StatusDNS);
}

TEST_F(RunnerTest, SetStatusMP) {
  oRunner r(&oe);
  r.setStatus(StatusMP, true, oBase::ChangeType::Update, false);
  EXPECT_EQ(r.getStatus(), StatusMP);
}

TEST_F(RunnerTest, StatusEncodeDecodeRoundTrip) {
  for (auto st : getAllRunnerStatus()) {
    wstring encoded = oAbstractRunner::encodeStatus(st);
    EXPECT_FALSE(encoded.empty());
    EXPECT_EQ(oAbstractRunner::decodeStatus(encoded), st);
  }
}

// ── Club ──────────────────────────────────────────────────────────────────────

TEST_F(RunnerTest, SetClubIdZero) {
  oRunner r(&oe);
  r.setClubId(0);
  EXPECT_EQ(r.getClubId(), 0);
  EXPECT_EQ(r.getClub(), L"");
}

TEST_F(RunnerTest, SetGetClubById) {
  oClub club(&oe, 10);
  club.setName(L"IK Hakarp");
  oe.addClub(club);

  oRunner r(&oe);
  r.setClubId(10);
  EXPECT_EQ(r.getClubId(), 10);
  EXPECT_EQ(r.getClub(), L"IK Hakarp");
}

TEST_F(RunnerTest, SetGetClubByName) {
  oRunner r(&oe);
  r.setClub(L"Testklubben");
  EXPECT_EQ(r.getClub(), L"Testklubben");
}

// ── Class ─────────────────────────────────────────────────────────────────────

TEST_F(RunnerTest, SetGetClass) {
  oClass cls(&oe, 5);
  cls.setName(L"H21", false);
  oe.addClass(cls);

  oRunner r(&oe);
  r.setClassId(5, false);
  EXPECT_EQ(r.getClassId(false), 5);
  EXPECT_EQ(r.getClass(false), L"H21");
}

TEST_F(RunnerTest, ClassIdZeroLeavesNoClass) {
  oRunner r(&oe);
  r.setClassId(0, false);
  EXPECT_EQ(r.getClassId(false), 0);
}

// ── Card ──────────────────────────────────────────────────────────────────────

TEST_F(RunnerTest, SetGetCardNo) {
  oRunner r(&oe);
  r.setCardNo(12345, false, false);
  EXPECT_EQ(r.getCardNo(), 12345);
}

TEST_F(RunnerTest, SetCardNoZero) {
  oRunner r(&oe);
  r.setCardNo(0, false, false);
  EXPECT_EQ(r.getCardNo(), 0);
}

// ── Start / Finish Time ───────────────────────────────────────────────────────

TEST_F(RunnerTest, SetGetStartTime) {
  oRunner r(&oe);
  r.setStartTime(3600, true, oBase::ChangeType::Update);
  EXPECT_EQ(r.getStartTime(), 3600);
}

TEST_F(RunnerTest, SetGetFinishTime) {
  oRunner r(&oe);
  r.setStartTime(3600, true, oBase::ChangeType::Update);
  r.setFinishTime(4800);
  EXPECT_EQ(r.getFinishTime(), 4800);
}

// ── Running time ──────────────────────────────────────────────────────────────

TEST_F(RunnerTest, GetRunningTimeNoFinish) {
  oRunner r(&oe);
  r.setStartTime(3600, true, oBase::ChangeType::Update);
  EXPECT_EQ(r.getRunningTime(false), 0);
}

TEST_F(RunnerTest, GetRunningTimeOK) {
  oRunner r(&oe);
  r.setStartTime(3600, true, oBase::ChangeType::Update);
  r.setStatus(StatusOK, true, oBase::ChangeType::Update, false);
  r.setFinishTime(4800);
  EXPECT_EQ(r.getRunningTime(false), 1200);
}

TEST_F(RunnerTest, GetRunningTimeDNS) {
  oRunner r(&oe);
  r.setStartTime(3600, true, oBase::ChangeType::Update);
  r.setStatus(StatusDNS, true, oBase::ChangeType::Update, false);
  r.setFinishTime(4800);
  EXPECT_EQ(r.getRunningTime(false), 0); // DNS => no running time
}

TEST_F(RunnerTest, GetRunningTimeDNF) {
  oRunner r(&oe);
  r.setStartTime(3600, true, oBase::ChangeType::Update);
  r.setStatus(StatusDNF, true, oBase::ChangeType::Update, false);
  r.setFinishTime(4800);
  EXPECT_EQ(r.getRunningTime(false), 0); // DNF => no running time
}

TEST_F(RunnerTest, GetRunningTimeCANCEL) {
  oRunner r(&oe);
  r.setStartTime(3600, true, oBase::ChangeType::Update);
  r.setStatus(StatusCANCEL, true, oBase::ChangeType::Update, false);
  r.setFinishTime(4800);
  EXPECT_EQ(r.getRunningTime(false), 0); // CANCEL => no running time
}

// ── evaluateCard — simple sequential course ───────────────────────────────────

TEST_F(RunnerTest, EvaluateCardOK) {
  pCourse crs = oe.addCourse(L"Short", 1000);
  pControl c1 = oe.getControl(oe.getFreeControlId(), true, false);
  pControl c2 = oe.getControl(oe.getFreeControlId(), true, false);
  c1->setNumbers(L"31");
  c2->setNumbers(L"32");
  crs->addControl(c1->getId());
  crs->addControl(c2->getId());

  oClass cls(&oe, 1);
  cls.setName(L"H21", false);
  oe.addClass(cls);
  pClass pc = oe.getClass(1);
  pc->setCourse(crs);

  oRunner r(&oe);
  r.setName(L"Test Runner", false);
  r.setClassId(1, false);
  r.setStartTime(3600, true, oBase::ChangeType::Update);

  oCard card(&oe);
  card.addPunch(oPunch::PunchStart,  3600, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(31,                  4000, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(32,                  4500, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(oPunch::PunchFinish, 5000, 0, 0, oCard::PunchOrigin::Unknown);

  pCard addedCard = oe.addCard(card);
  vector<pair<int, pControl>> mp;
  r.addCard(addedCard, mp);

  EXPECT_TRUE(mp.empty());  // No missing punches
  EXPECT_EQ(r.getStatus(), StatusOK);
  EXPECT_EQ(r.getFinishTime(), 5000);
  EXPECT_EQ(r.getRunningTime(false), 1400); // 5000 - 3600
}

TEST_F(RunnerTest, EvaluateCardMissingPunch) {
  pCourse crs = oe.addCourse(L"Short", 1000);
  pControl c1 = oe.getControl(oe.getFreeControlId(), true, false);
  pControl c2 = oe.getControl(oe.getFreeControlId(), true, false);
  c1->setNumbers(L"31");
  c2->setNumbers(L"32");
  crs->addControl(c1->getId());
  crs->addControl(c2->getId());

  oClass cls(&oe, 2);
  cls.setName(L"D21", false);
  oe.addClass(cls);
  pClass pc = oe.getClass(2);
  pc->setCourse(crs);

  oRunner r(&oe);
  r.setClassId(2, false);
  r.setStartTime(3600, true, oBase::ChangeType::Update);

  oCard card(&oe);
  card.addPunch(oPunch::PunchStart,  3600, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(31,                  4000, 0, 0, oCard::PunchOrigin::Unknown);
  // 32 is missing
  card.addPunch(oPunch::PunchFinish, 5000, 0, 0, oCard::PunchOrigin::Unknown);

  pCard addedCard = oe.addCard(card);
  vector<pair<int, pControl>> mp;
  r.addCard(addedCard, mp);

  EXPECT_FALSE(mp.empty());  // Missing control 32
  EXPECT_EQ(r.getStatus(), StatusMP);
}

TEST_F(RunnerTest, SplitTimesAfterEvaluateCard) {
  pCourse crs = oe.addCourse(L"Short", 1000);
  pControl c1 = oe.getControl(oe.getFreeControlId(), true, false);
  pControl c2 = oe.getControl(oe.getFreeControlId(), true, false);
  c1->setNumbers(L"31");
  c2->setNumbers(L"32");
  crs->addControl(c1->getId());
  crs->addControl(c2->getId());

  oClass cls(&oe, 3);
  cls.setName(L"H35", false);
  oe.addClass(cls);
  pClass pc = oe.getClass(3);
  pc->setCourse(crs);

  oRunner r(&oe);
  r.setClassId(3, false);
  r.setStartTime(3600, true, oBase::ChangeType::Update);

  oCard card(&oe);
  card.addPunch(oPunch::PunchStart,  3600, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(31,                  4000, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(32,                  4500, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(oPunch::PunchFinish, 5000, 0, 0, oCard::PunchOrigin::Unknown);

  pCard addedCard = oe.addCard(card);
  vector<pair<int, pControl>> mp;
  r.addCard(addedCard, mp);

  const auto& splits = r.getSplitTimes(false);
  ASSERT_EQ(splits.size(), 2u);
  EXPECT_TRUE(splits[0].hasTime());
  EXPECT_EQ(splits[0].getTime(false), 4000);
  EXPECT_TRUE(splits[1].hasTime());
  EXPECT_EQ(splits[1].getTime(false), 4500);
}

// ── EvaluateCard without course — sets finish from card ───────────────────────

TEST_F(RunnerTest, EvaluateCardNoCourse) {
  oClass cls(&oe, 10);
  cls.setName(L"NoMap", false);
  oe.addClass(cls);

  oRunner r(&oe);
  r.setClassId(10, false);
  r.setStartTime(1000, true, oBase::ChangeType::Update);

  oCard card(&oe);
  card.addPunch(oPunch::PunchStart,  1000, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(oPunch::PunchFinish, 2500, 0, 0, oCard::PunchOrigin::Unknown);

  pCard addedCard = oe.addCard(card);
  vector<pair<int, pControl>> mp;
  r.addCard(addedCard, mp);

  EXPECT_TRUE(mp.empty());
  EXPECT_EQ(r.getFinishTime(), 2500);
}

// ── DynamicStatus ─────────────────────────────────────────────────────────────

TEST_F(RunnerTest, DynamicStatusInactiveForDNS) {
  oRunner r(&oe);
  r.setStatus(StatusDNS, true, oBase::ChangeType::Update, false);
  EXPECT_EQ(r.getDynamicStatus(), StatusInactive);
}

TEST_F(RunnerTest, DynamicStatusFinished) {
  oRunner r(&oe);
  r.setStartTime(1000, true, oBase::ChangeType::Update);
  r.setStatus(StatusOK, true, oBase::ChangeType::Update, false);
  r.setFinishTime(2000);
  EXPECT_EQ(r.getDynamicStatus(), StatusFinished);
}

// ── canRemove ─────────────────────────────────────────────────────────────────

TEST_F(RunnerTest, CanRemoveWhenNoCard) {
  oRunner r(&oe);
  EXPECT_TRUE(r.canRemove());
}

// ── StartNo ───────────────────────────────────────────────────────────────────

TEST_F(RunnerTest, SetGetStartNo) {
  oRunner r(&oe);
  r.setStartNo(42, oBase::ChangeType::Update);
  EXPECT_EQ(r.getStartNo(), 42);
}

// ── RaceNo ────────────────────────────────────────────────────────────────────

TEST_F(RunnerTest, RaceNoDefaultZero) {
  oRunner r(&oe);
  EXPECT_EQ(r.getRaceNo(), 0);
}

// ── classInstance ─────────────────────────────────────────────────────────────

TEST_F(RunnerTest, ClassInstanceDefaultZero) {
  oRunner r(&oe);
  EXPECT_EQ(r.classInstance(), 0);
}

// ── matchAbstractRunner ───────────────────────────────────────────────────────

TEST_F(RunnerTest, MatchAbstractRunnerSelf) {
  oRunner r(&oe);
  EXPECT_TRUE(r.matchAbstractRunner(&r));
}

TEST_F(RunnerTest, MatchAbstractRunnerOther) {
  oRunner r1(&oe);
  oRunner r2(&oe);
  EXPECT_FALSE(r1.matchAbstractRunner(&r2));
}

// ── SplitData ─────────────────────────────────────────────────────────────────

TEST_F(RunnerTest, SplitDataDefault) {
  SplitData sd;
  EXPECT_TRUE(sd.isMissing());
  EXPECT_FALSE(sd.hasTime());
}

TEST_F(RunnerTest, SplitDataOK) {
  SplitData sd(4000, SplitData::SplitStatus::OK);
  EXPECT_FALSE(sd.isMissing());
  EXPECT_TRUE(sd.hasTime());
  EXPECT_EQ(sd.getTime(false), 4000);
}

// ── oEvent::addRunner / getRunner ─────────────────────────────────────────────

TEST_F(RunnerTest, AddAndGetRunner) {
  oRunner r(&oe, 77);
  r.setName(L"Bo Ek", false);
  pRunner pr = oe.addRunner(r);
  ASSERT_NE(pr, nullptr);
  EXPECT_EQ(pr->getId(), 77);
  EXPECT_EQ(pr->getName(), L"Bo Ek");

  pRunner found = oe.getRunner(77, 0);
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->getId(), 77);
}

TEST_F(RunnerTest, GetRunnerNotFound) {
  EXPECT_EQ(oe.getRunner(9999, 0), nullptr);
}

TEST_F(RunnerTest, AddRunnerIdZeroReturnsNull) {
  oRunner r(&oe, 0);
  pRunner pr = oe.addRunner(r);
  EXPECT_EQ(pr, nullptr);
}

// ── oEvent::getRunners ────────────────────────────────────────────────────────

TEST_F(RunnerTest, GetRunnersByClass) {
  oClass cls(&oe, 20);
  cls.setName(L"H40", false);
  oe.addClass(cls);

  oRunner r1(&oe, 101);
  r1.setClassId(20, false);
  oe.addRunner(r1);

  oRunner r2(&oe, 102);
  r2.setClassId(20, false);
  oe.addRunner(r2);

  oRunner r3(&oe, 103);
  // different class
  oe.addRunner(r3);

  vector<pRunner> result;
  oe.getRunners(20, 0, result, false);
  EXPECT_EQ(result.size(), 2u);
}

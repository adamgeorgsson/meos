// team_test.cpp — Unit tests for oTeam (US-003h)
#include <gtest/gtest.h>
#include "oTeam.h"
#include "oRunner.h"
#include "oEvent.h"
#include "oClass.h"
#include "oClub.h"

using std::wstring;
using std::vector;

class TeamTest : public ::testing::Test {
protected:
  oEvent oe;
  void SetUp() override { setlocale(LC_ALL, "C.UTF-8"); }
};

// ── Construction ──────────────────────────────────────────────────────────────

TEST_F(TeamTest, DefaultConstruction) {
  oTeam t(&oe);
  EXPECT_GT(t.getId(), 0);
  EXPECT_EQ(t.getStatus(), StatusUnknown);
  EXPECT_EQ(t.getStartTime(), 0);
  EXPECT_EQ(t.getFinishTime(), 0);
  EXPECT_EQ(t.getNumRunners(), 0);
}

TEST_F(TeamTest, ConstructionWithId) {
  oTeam t(&oe, 55);
  EXPECT_EQ(t.getId(), 55);
}

TEST_F(TeamTest, ConstructionIncrementsQFreeTeamId) {
  int before = oe.qFreeTeamId;
  oTeam t(&oe, before + 10);
  EXPECT_GE(oe.qFreeTeamId, before + 10);
}

// ── Name ──────────────────────────────────────────────────────────────────────

TEST_F(TeamTest, SetGetName) {
  oTeam t(&oe);
  t.setName(L"Team Alpha", false);
  EXPECT_EQ(t.getName(), L"Team Alpha");
}

TEST_F(TeamTest, SetNameEmpty) {
  oTeam t(&oe);
  t.setName(L"", false);
  EXPECT_EQ(t.getName(), L"");
}

TEST_F(TeamTest, GetNameAndRaceNoRace) {
  oTeam t(&oe);
  t.setName(L"Relätlaget", false);
  EXPECT_EQ(t.getNameAndRace(false), L"Relätlaget");
  EXPECT_EQ(t.getNameAndRace(true), L"Relätlaget");
}

// ── Status ────────────────────────────────────────────────────────────────────

TEST_F(TeamTest, SetGetStatus) {
  oTeam t(&oe);
  t.setStatus(StatusDNS, true, oBase::ChangeType::Update);
  EXPECT_EQ(t.getStatus(), StatusDNS);
}

TEST_F(TeamTest, StatusDefaultUnknown) {
  oTeam t(&oe);
  EXPECT_EQ(t.getStatus(), StatusUnknown);
}

TEST_F(TeamTest, IsTeam) {
  oTeam t(&oe);
  EXPECT_TRUE(t.isTeam());
}

// ── RaceNo ────────────────────────────────────────────────────────────────────

TEST_F(TeamTest, RaceNoAlwaysZero) {
  oTeam t(&oe);
  EXPECT_EQ(t.getRaceNo(), 0);
}

// ── StartNo ───────────────────────────────────────────────────────────────────

TEST_F(TeamTest, SetGetStartNo) {
  oTeam t(&oe);
  t.setStartNo(7, oBase::ChangeType::Update);
  EXPECT_EQ(t.getStartNo(), 7);
}

// ── Club ──────────────────────────────────────────────────────────────────────

TEST_F(TeamTest, SetClubIdZero) {
  oTeam t(&oe);
  t.setClubId(0);
  EXPECT_EQ(t.getClubId(), 0);
  EXPECT_EQ(t.getClub(), L"");
}

TEST_F(TeamTest, SetGetClubById) {
  oClub club(&oe, 10);
  club.setName(L"OK Orion");
  oe.addClub(club);

  oTeam t(&oe);
  t.setClubId(10);
  EXPECT_EQ(t.getClubId(), 10);
  EXPECT_EQ(t.getClub(), L"OK Orion");
}

TEST_F(TeamTest, SetGetClubByName) {
  oTeam t(&oe);
  t.setClub(L"Testklubben");
  EXPECT_EQ(t.getClub(), L"Testklubben");
}

// ── Class ─────────────────────────────────────────────────────────────────────

TEST_F(TeamTest, SetGetClass) {
  oClass cls(&oe, 3);
  cls.setName(L"Relay Open", false);
  oe.addClass(cls);

  oTeam t(&oe);
  t.setClassId(3, false);
  EXPECT_EQ(t.getClassId(false), 3);
  EXPECT_EQ(t.getClass(false), L"Relay Open");
}

TEST_F(TeamTest, ClassIdZeroLeavesNoClass) {
  oTeam t(&oe);
  t.setClassId(0, false);
  EXPECT_EQ(t.getClassId(false), 0);
}

// ── Runner management ─────────────────────────────────────────────────────────

TEST_F(TeamTest, AddRunnerToTeam) {
  oTeam t(&oe);
  oRunner r1(&oe);
  t.setRunner(0, &r1, false);
  EXPECT_EQ(t.getNumRunners(), 1);
  EXPECT_EQ(t.getRunner(0), &r1);
}

TEST_F(TeamTest, AddMultipleRunners) {
  oTeam t(&oe);
  oRunner r1(&oe);
  oRunner r2(&oe);
  t.setRunner(0, &r1, false);
  t.setRunner(1, &r2, false);
  EXPECT_EQ(t.getNumRunners(), 2);
  EXPECT_EQ(t.getRunner(0), &r1);
  EXPECT_EQ(t.getRunner(1), &r2);
}

TEST_F(TeamTest, GetRunnerOutOfRangeReturnsNull) {
  oTeam t(&oe);
  EXPECT_EQ(t.getRunner(99), nullptr);
}

TEST_F(TeamTest, IsRunnerUsed) {
  oTeam t(&oe);
  oRunner r1(&oe, 17);
  t.setRunner(0, &r1, false);
  EXPECT_TRUE(t.isRunnerUsed(17));
  EXPECT_FALSE(t.isRunnerUsed(99));
}

TEST_F(TeamTest, SetRunnerNullClearsSlot) {
  oTeam t(&oe);
  oRunner r1(&oe);
  t.setRunner(0, &r1, false);
  EXPECT_EQ(t.getRunner(0), &r1);
  // Clear using importRunners with a nullptr slot
  vector<pRunner> empty = {nullptr};
  t.importRunners(empty);
  EXPECT_EQ(t.getRunner(0), nullptr);
}

TEST_F(TeamTest, GetNumAssignedRunners) {
  oTeam t(&oe);
  oRunner r1(&oe);
  t.setRunner(0, &r1, false);
  t.setRunner(1, nullptr, false); // null
  // resize to 2 happens in setRunner, but nullptr isn't assigned
  EXPECT_EQ(t.getNumAssignedRunners(), 1);
}

TEST_F(TeamTest, RunnerLinkedToTeam) {
  oTeam t(&oe);
  oRunner r1(&oe);
  t.setRunner(0, &r1, false);
  EXPECT_EQ(r1.tInTeam, &t);
  EXPECT_EQ(r1.tLeg, 0);
}

// ── decodeRunners ─────────────────────────────────────────────────────────────

TEST_F(TeamTest, DecodeRunnersBasic) {
  oTeam t(&oe);
  vector<int> rid;
  t.decodeRunners("1;2;3;", rid);
  ASSERT_EQ(rid.size(), 3u);
  EXPECT_EQ(rid[0], 1);
  EXPECT_EQ(rid[1], 2);
  EXPECT_EQ(rid[2], 3);
}

TEST_F(TeamTest, DecodeRunnersEmpty) {
  oTeam t(&oe);
  vector<int> rid;
  t.decodeRunners("", rid);
  EXPECT_TRUE(rid.empty());
}

TEST_F(TeamTest, DecodeRunnersZeroSlots) {
  oTeam t(&oe);
  vector<int> rid;
  t.decodeRunners("0;0;", rid);
  ASSERT_EQ(rid.size(), 2u);
  EXPECT_EQ(rid[0], 0);
  EXPECT_EQ(rid[1], 0);
}

// ── getTeam / isVacant ────────────────────────────────────────────────────────

TEST_F(TeamTest, GetTeamSelf) {
  oTeam t(&oe);
  EXPECT_EQ(t.getTeam(), &t);
}

TEST_F(TeamTest, NotVacantByDefault) {
  oTeam t(&oe);
  EXPECT_FALSE(t.isVacant());
}

// ── matchAbstractRunner ───────────────────────────────────────────────────────

TEST_F(TeamTest, MatchAbstractRunnerSelf) {
  oTeam t(&oe);
  EXPECT_TRUE(t.matchAbstractRunner(&t));
}

TEST_F(TeamTest, MatchAbstractRunnerOther) {
  oTeam t1(&oe);
  oTeam t2(&oe);
  EXPECT_FALSE(t1.matchAbstractRunner(&t2));
}

// ── Input result ──────────────────────────────────────────────────────────────

TEST_F(TeamTest, SetGetInputTime) {
  oTeam t(&oe);
  t.setInputTime(1800);
  EXPECT_EQ(t.getInputTime(), 1800);
}

TEST_F(TeamTest, SetGetInputStatus) {
  oTeam t(&oe);
  t.setInputStatus(StatusOK);
  EXPECT_EQ(t.getInputStatus(), StatusOK);
}

TEST_F(TeamTest, SetGetInputPoints) {
  oTeam t(&oe);
  t.setInputPoints(42);
  EXPECT_EQ(t.getInputPoints(), 42);
}

TEST_F(TeamTest, SetGetInputPlace) {
  oTeam t(&oe);
  t.setInputPlace(3);
  EXPECT_EQ(t.getInputPlace(), 3);
}

// ── DI fields ─────────────────────────────────────────────────────────────────

TEST_F(TeamTest, DIBibFieldRoundTrip) {
  oTeam t(&oe);
  t.getDI().setString("Bib", L"42");
  EXPECT_EQ(t.getDCI().getString("Bib"), L"42");
}

TEST_F(TeamTest, DIHeatFieldRoundTrip) {
  oTeam t(&oe);
  t.getDI().setInt("Heat", 3);
  EXPECT_EQ(t.getDCI().getInt("Heat"), 3);
}

// ── oEvent team management ────────────────────────────────────────────────────

TEST_F(TeamTest, AddAndGetTeam) {
  oTeam t(&oe, 100);
  t.setName(L"Lag 1", false);
  pTeam pt = oe.addTeam(t);
  ASSERT_NE(pt, nullptr);
  EXPECT_EQ(pt->getId(), 100);
  EXPECT_EQ(pt->getName(), L"Lag 1");

  pTeam found = oe.getTeam(100);
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->getId(), 100);
}

TEST_F(TeamTest, GetTeamNotFound) {
  EXPECT_EQ(oe.getTeam(9999), nullptr);
}

TEST_F(TeamTest, AddTeamByNameAndClub) {
  pTeam pt = oe.addTeam(L"Lag Snabb", 0, 0);
  ASSERT_NE(pt, nullptr);
  EXPECT_EQ(pt->getName(), L"Lag Snabb");
  EXPECT_GT(pt->getId(), 0);
}

TEST_F(TeamTest, GetTeamsByClass) {
  oClass cls(&oe, 5);
  cls.setName(L"Stafett H", false);
  oe.addClass(cls);

  oTeam t1(&oe, 200);
  t1.setClassId(5, false);
  oe.addTeam(t1);

  oTeam t2(&oe, 201);
  t2.setClassId(5, false);
  oe.addTeam(t2);

  oTeam t3(&oe, 202);
  // no class
  oe.addTeam(t3);

  vector<pTeam> result;
  oe.getTeams(5, result, false);
  EXPECT_EQ(result.size(), 2u);
}

TEST_F(TeamTest, GetTeamsByClassZeroGetsAll) {
  oTeam t1(&oe, 301);
  oe.addTeam(t1);
  oTeam t2(&oe, 302);
  oe.addTeam(t2);

  vector<pTeam> result;
  oe.getTeams(0, result, false);
  EXPECT_GE(result.size(), 2u);
}

TEST_F(TeamTest, AddTeamIdZeroReturnsNull) {
  oTeam t(&oe, 0);
  pTeam pt = oe.addTeam(t);
  EXPECT_EQ(pt, nullptr);
}

// ── formatStatus ─────────────────────────────────────────────────────────────

TEST_F(TeamTest, FormatStatusOK) {
  const wstring &s = oEvent::formatStatus(StatusOK, false);
  EXPECT_FALSE(s.empty());
  EXPECT_NE(s, L"?");
}

TEST_F(TeamTest, FormatStatusDNS) {
  const wstring &s = oEvent::formatStatus(StatusDNS, false);
  EXPECT_FALSE(s.empty());
}

TEST_F(TeamTest, FormatStatusMP) {
  const wstring &s = oEvent::formatStatus(StatusMP, false);
  EXPECT_FALSE(s.empty());
}

TEST_F(TeamTest, FormatStatusNotCompetingPrint) {
  const wstring &print  = oEvent::formatStatus(StatusNotCompeting, true);
  const wstring &screen = oEvent::formatStatus(StatusNotCompeting, false);
  // forPrint should return em dash, screen should return text
  EXPECT_NE(print, screen);
}

TEST_F(TeamTest, FormatStatusUnknownPrint) {
  const wstring &s = oEvent::formatStatus(StatusUnknown, true);
  EXPECT_EQ(s, L"-");
}

TEST_F(TeamTest, FormatStatusNoTimingPrint) {
  // forPrint=true should return same as OK (Godkänd)
  const wstring &printOK    = oEvent::formatStatus(StatusOK, true);
  const wstring &printNT    = oEvent::formatStatus(StatusNoTiming, true);
  EXPECT_EQ(printOK, printNT);
}

// ── getLegRunningTime — simple 2-runner team without class ────────────────────

TEST_F(TeamTest, LegRunningTimeNoRunners) {
  oTeam t(&oe);
  EXPECT_EQ(t.getLegRunningTime(0, false, false), 0);
}

TEST_F(TeamTest, LegRunningTimeWithRunnerNoFinish) {
  oTeam t(&oe);
  t.setStartTime(3600, true, oBase::ChangeType::Update);

  oRunner r1(&oe);
  r1.setStartTime(3600, true, oBase::ChangeType::Update);
  t.setRunner(0, &r1, false);

  EXPECT_EQ(t.getLegRunningTime(0, false, false), 0);
}

TEST_F(TeamTest, LegRunningTimeWithRunner) {
  oTeam t(&oe);
  t.setStartTime(3600, true, oBase::ChangeType::Update);
  t.tStartTime = 3600;

  oRunner r1(&oe);
  r1.setStartTime(3600, true, oBase::ChangeType::Update);
  r1.setStatus(StatusOK, true, oBase::ChangeType::Update, false);
  r1.setFinishTime(4800);
  t.setRunner(0, &r1, false);

  // Without class, uses max(finish - tStartTime, 0) logic
  int rt = t.getLegRunningTime(0, false, false);
  EXPECT_GT(rt, 0);
}

// ── getLegStatus ──────────────────────────────────────────────────────────────

TEST_F(TeamTest, LegStatusNoRunnersUnknown) {
  oTeam t(&oe);
  EXPECT_EQ(t.getLegStatus(0, false, false), StatusUnknown);
}

TEST_F(TeamTest, LegStatusDNSWhenTeamSetDNS) {
  oTeam t(&oe);
  t.setStatus(StatusDNS, true, oBase::ChangeType::Update);
  // No runners, no class → fallback to tStatus for DNS check
  EXPECT_EQ(t.getLegStatus(0, false, false), StatusDNS);
}

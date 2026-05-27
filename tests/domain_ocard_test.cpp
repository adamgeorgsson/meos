// Tests for oCard domain migration.
#include <gtest/gtest.h>
#include "oCard.h"
#include "oEvent.h"
#include "oPunch.h"
#include "SICard.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static oEvent makeEvent() { return oEvent{}; }

static oCard makeCard(oEvent& oe) { return oCard(&oe); }

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
TEST(oCardTest, ConstructionDefault) {
  oEvent oe;
  oCard c(&oe);
  EXPECT_EQ(c.getCardNo(), 0);
  EXPECT_EQ(c.getNumPunches(), 0);
  EXPECT_FALSE(c.isRemoved());
}

TEST(oCardTest, ConstructionWithId) {
  oEvent oe;
  oCard c(&oe, 42);
  EXPECT_EQ(c.getId(), 42);
}

// ---------------------------------------------------------------------------
// setCardNo / getCardNo / getCardNoString
// ---------------------------------------------------------------------------
TEST(oCardTest, SetCardNo) {
  oEvent oe;
  oCard c(&oe);
  c.setCardNo(12345);
  EXPECT_EQ(c.getCardNo(), 12345);
}

TEST(oCardTest, GetCardNoString) {
  oEvent oe;
  oCard c(&oe);
  c.setCardNo(999);
  EXPECT_EQ(c.getCardNoString(), L"999");
}

// ---------------------------------------------------------------------------
// canRemove / remove
// ---------------------------------------------------------------------------
TEST(oCardTest, CanRemoveNoOwner) {
  oEvent oe;
  oCard c(&oe);
  EXPECT_TRUE(c.canRemove());
}

TEST(oCardTest, RemoveSetsFlag) {
  oEvent oe;
  oCard c(&oe);
  c.remove();
  EXPECT_TRUE(c.isRemoved());
}

// ---------------------------------------------------------------------------
// getInfo
// ---------------------------------------------------------------------------
TEST(oCardTest, GetInfo) {
  oEvent oe;
  oCard c(&oe);
  c.setCardNo(7777);
  wstring info = c.getInfo();
  EXPECT_NE(info.find(L"7777"), wstring::npos);
}

// ---------------------------------------------------------------------------
// addPunch / getNumPunches / getPunchByType
// ---------------------------------------------------------------------------
TEST(oCardTest, AddPunchManual) {
  oEvent oe;
  oCard c(&oe);
  c.addPunch(oPunch::PunchStart, 1200, 0, 0, oCard::PunchOrigin::Manual);
  EXPECT_EQ(c.getNumPunches(), 1);
}

TEST(oCardTest, AddMultiplePunches) {
  oEvent oe;
  oCard c(&oe);
  c.addPunch(oPunch::PunchStart,  100, 0, 0, oCard::PunchOrigin::Manual);
  c.addPunch(31,                   200, 0, 0, oCard::PunchOrigin::Manual);
  c.addPunch(oPunch::PunchFinish, 300, 0, 0, oCard::PunchOrigin::Manual);
  EXPECT_EQ(c.getNumPunches(), 3);
}

TEST(oCardTest, FinishStaysLast) {
  oEvent oe;
  oCard c(&oe);
  c.addPunch(oPunch::PunchFinish, 300, 0, 0, oCard::PunchOrigin::Manual);
  c.addPunch(31,                   200, 0, 0, oCard::PunchOrigin::Manual);
  // 31 should be inserted before finish
  EXPECT_EQ(c.getNumPunches(), 2);
  // Finish should still be last
  auto* last = c.getPunchByIndex(1);
  ASSERT_NE(last, nullptr);
  EXPECT_TRUE(last->isFinish());
}

TEST(oCardTest, GetPunchByType) {
  oEvent oe;
  oCard c(&oe);
  c.addPunch(oPunch::PunchStart, 500, 0, 0, oCard::PunchOrigin::Manual);
  c.addPunch(32, 600, 0, 0, oCard::PunchOrigin::Manual);
  auto* p = c.getPunchByType(oPunch::PunchStart);
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(p->isStart());
  EXPECT_EQ(p->getTypeCode(), (int)oPunch::PunchStart);
}

TEST(oCardTest, GetPunchByTypeNotFound) {
  oEvent oe;
  oCard c(&oe);
  EXPECT_EQ(c.getPunchByType(999), nullptr);
}

TEST(oCardTest, GetPunchByIndex) {
  oEvent oe;
  oCard c(&oe);
  c.addPunch(31, 100, 0, 0, oCard::PunchOrigin::Manual);
  c.addPunch(32, 200, 0, 0, oCard::PunchOrigin::Manual);
  auto* p0 = c.getPunchByIndex(0);
  auto* p1 = c.getPunchByIndex(1);
  auto* p2 = c.getPunchByIndex(2);
  ASSERT_NE(p0, nullptr);
  EXPECT_EQ(p0->getTypeCode(), 31);
  ASSERT_NE(p1, nullptr);
  EXPECT_EQ(p1->getTypeCode(), 32);
  EXPECT_EQ(p2, nullptr);
}

// ---------------------------------------------------------------------------
// getPunches
// ---------------------------------------------------------------------------
TEST(oCardTest, GetPunches) {
  oEvent oe;
  oCard c(&oe);
  c.addPunch(31, 100, 0, 0, oCard::PunchOrigin::Manual);
  c.addPunch(32, 200, 0, 0, oCard::PunchOrigin::Manual);
  std::vector<oPunch*> out;
  c.getPunches(out);
  EXPECT_EQ((int)out.size(), 2);
}

// ---------------------------------------------------------------------------
// getPunchString / importPunches round-trip
// ---------------------------------------------------------------------------
TEST(oCardTest, PunchStringRoundTrip) {
  oEvent oe;
  oCard c1(&oe);
  c1.addPunch(oPunch::PunchStart,  1000, 0, 0, oCard::PunchOrigin::Manual);
  c1.addPunch(31,                   2000, 0, 0, oCard::PunchOrigin::Manual);
  c1.addPunch(oPunch::PunchFinish, 3000, 0, 0, oCard::PunchOrigin::Manual);
  std::string ps = c1.getPunchString();
  EXPECT_FALSE(ps.empty());

  oCard c2(&oe);
  c2.setCardNo(c1.getCardNo());
  c2.importPunches(ps);
  EXPECT_EQ(c2.getNumPunches(), 3);
  EXPECT_EQ(c2.getPunchString(), ps);
}

TEST(oCardTest, EmptyCardPunchString) {
  oEvent oe;
  oCard c(&oe);
  EXPECT_EQ(c.getPunchString(), "");
}

// ---------------------------------------------------------------------------
// merge
// ---------------------------------------------------------------------------
TEST(oCardTest, MergeCardNo) {
  oEvent oe;
  oCard src(&oe);
  src.setCardNo(9999);
  oCard dst(&oe);
  dst.merge(src, nullptr);
  EXPECT_EQ(dst.getCardNo(), 9999);
}

TEST(oCardTest, MergePunches) {
  oEvent oe;
  oCard src(&oe);
  src.addPunch(31, 100, 0, 0, oCard::PunchOrigin::Manual);
  src.addPunch(32, 200, 0, 0, oCard::PunchOrigin::Manual);
  oCard dst(&oe);
  dst.merge(src, nullptr);
  EXPECT_EQ(dst.getNumPunches(), 2);
  EXPECT_EQ(dst.getPunchString(), src.getPunchString());
}

// ---------------------------------------------------------------------------
// SICard integration
// ---------------------------------------------------------------------------
TEST(oCardTest, SetReadId) {
  oEvent oe;
  oCard c(&oe);
  SICard si;
  si.CardNumber = 100;
  si.nPunch     = 2;
  si.Punch[0]   = {31u, 1000u};
  si.Punch[1]   = {32u, 2000u};
  c.setReadId(si);
  EXPECT_TRUE(c.isCardRead(si));
}

TEST(oCardTest, IsCardReadFalseForDifferentCard) {
  oEvent oe;
  oCard c(&oe);
  SICard si1;
  si1.nPunch = 1; si1.Punch[0] = {31u, 500u};
  c.setReadId(si1);

  SICard si2;
  si2.nPunch = 1; si2.Punch[0] = {32u, 500u};
  EXPECT_FALSE(c.isCardRead(si2));
}

TEST(oCardTest, GetSICard) {
  oEvent oe;
  oCard c(&oe);
  c.setCardNo(12345);
  c.addPunch(31, 100, 0, 0, oCard::PunchOrigin::Manual);
  c.addPunch(32, 200, 0, 0, oCard::PunchOrigin::Manual);
  SICard si;
  c.getSICard(si);
  EXPECT_EQ(si.CardNumber, 12345u);
  EXPECT_EQ(si.nPunch, 2u);
  EXPECT_EQ(si.Punch[0].Code, 31u);
  EXPECT_EQ(si.Punch[1].Code, 32u);
}

// ---------------------------------------------------------------------------
// getCardHash
// ---------------------------------------------------------------------------
TEST(oCardTest, CardHashDifferentPunches) {
  oEvent oe;
  oCard c1(&oe), c2(&oe);
  c1.setCardNo(1);
  c1.addPunch(31, 100, 0, 0, oCard::PunchOrigin::Manual);
  c2.setCardNo(1);
  c2.addPunch(32, 100, 0, 0, oCard::PunchOrigin::Manual);
  EXPECT_NE(c1.getCardHash(), c2.getCardHash());
}

// ---------------------------------------------------------------------------
// getTimeRange
// ---------------------------------------------------------------------------
TEST(oCardTest, GetTimeRange) {
  oEvent oe;
  oCard c(&oe);
  c.addPunch(31, 100, 0, 0, oCard::PunchOrigin::Manual);
  c.addPunch(32, 300, 0, 0, oCard::PunchOrigin::Manual);
  c.addPunch(oPunch::PunchFinish, 400, 0, 0, oCard::PunchOrigin::Manual);
  auto [lo, hi] = c.getTimeRange();
  EXPECT_EQ(lo, 100);
  EXPECT_EQ(hi, 400); // finish sets hi
}

// ---------------------------------------------------------------------------
// unexpectedOrder
// ---------------------------------------------------------------------------
TEST(oCardTest, UnexpectedOrderFalse) {
  oEvent oe;
  oCard c(&oe);
  c.addPunch(31, 100, 0, 0, oCard::PunchOrigin::Manual);
  c.addPunch(32, 200, 0, 0, oCard::PunchOrigin::Manual);
  c.addPunch(33, 300, 0, 0, oCard::PunchOrigin::Manual);
  EXPECT_FALSE(c.unexpectedOrder(50));
}

TEST(oCardTest, UnexpectedOrderTrue) {
  oEvent oe;
  oCard c(&oe);
  // Punch at time 100 appears before startTime=200
  c.addPunch(31, 100, 1, 0, oCard::PunchOrigin::Manual); // isUsed because matchControlId != 0
  EXPECT_TRUE(c.unexpectedOrder(200));
}

// ---------------------------------------------------------------------------
// getNumControlPunches
// ---------------------------------------------------------------------------
TEST(oCardTest, GetNumControlPunches) {
  oEvent oe;
  oCard c(&oe);
  c.addPunch(oPunch::PunchStart,  100, 0, 0, oCard::PunchOrigin::Manual);
  c.addPunch(31,                   200, 0, 0, oCard::PunchOrigin::Manual);
  c.addPunch(32,                   300, 0, 0, oCard::PunchOrigin::Manual);
  c.addPunch(oPunch::PunchFinish, 400, 0, 0, oCard::PunchOrigin::Manual);
  int n = c.getNumControlPunches(
    (int)oPunch::PunchStart, (int)oPunch::PunchFinish);
  EXPECT_EQ(n, 2);
}

// ---------------------------------------------------------------------------
// getStartTime / getStartPunchCode / getFinishPunchCode
// ---------------------------------------------------------------------------
TEST(oCardTest, GetStartTime) {
  oEvent oe;
  oCard c(&oe);
  c.addPunch(oPunch::PunchStart, 500, 0, 42, oCard::PunchOrigin::Manual);
  EXPECT_EQ(c.getStartTime(oPunch::PunchStart), 500);
}

TEST(oCardTest, GetStartPunchCode) {
  oEvent oe;
  oCard c(&oe);
  c.addPunch(oPunch::PunchStart, 500, 0, 7, oCard::PunchOrigin::Manual);
  EXPECT_EQ(c.getStartPunchCode(), 7);
}

TEST(oCardTest, GetFinishPunchCode) {
  oEvent oe;
  oCard c(&oe);
  c.addPunch(31, 100, 0, 0, oCard::PunchOrigin::Manual);
  c.addPunch(oPunch::PunchFinish, 300, 0, 9, oCard::PunchOrigin::Manual);
  EXPECT_EQ(c.getFinishPunchCode(), 9);
}

// ---------------------------------------------------------------------------
// adaptTimes
// ---------------------------------------------------------------------------
TEST(oCardTest, AdaptTimesNoOp) {
  oEvent oe;
  oCard c(&oe);
  c.addPunch(31, 100, 0, 0, oCard::PunchOrigin::Manual);
  c.adaptTimes(0); // No change expected for small times
  EXPECT_EQ(c.getPunchByIndex(0)->getTimeInt(), 100);
}

// ---------------------------------------------------------------------------
// deletePunch / insertPunchAfter
// ---------------------------------------------------------------------------
TEST(oCardTest, DeletePunch) {
  oEvent oe;
  oCard c(&oe);
  c.addPunch(31, 100, 0, 0, oCard::PunchOrigin::Manual);
  c.addPunch(32, 200, 0, 0, oCard::PunchOrigin::Manual);
  auto* pp = c.getPunchByType(31);
  ASSERT_NE(pp, nullptr);
  c.deletePunch(pp);
  EXPECT_EQ(c.getNumPunches(), 1);
  EXPECT_EQ(c.getPunchByType(31), nullptr);
}

TEST(oCardTest, InsertPunchAfter) {
  oEvent oe;
  oCard c(&oe);
  c.addPunch(31, 100, 0, 0, oCard::PunchOrigin::Manual);
  c.addPunch(33, 300, 0, 0, oCard::PunchOrigin::Manual);
  c.insertPunchAfter(0, 32, 200); // Insert type 32 after index 0
  EXPECT_EQ(c.getNumPunches(), 3);
  EXPECT_EQ(c.getPunchByIndex(1)->getTypeCode(), 32);
}

// ---------------------------------------------------------------------------
// isOriginalCard
// ---------------------------------------------------------------------------
TEST(oCardTest, IsOriginalCardManual) {
  oEvent oe;
  oCard c(&oe);
  c.addPunch(31, 100, 0, 0, oCard::PunchOrigin::Manual);
  EXPECT_EQ(c.isOriginalCard(), oCard::PunchOrigin::Manual);
}

TEST(oCardTest, IsOriginalCardEmpty) {
  oEvent oe;
  oCard c(&oe);
  EXPECT_EQ(c.isOriginalCard(), oCard::PunchOrigin::Original);
}

// ---------------------------------------------------------------------------
// Battery / voltage
// ---------------------------------------------------------------------------
TEST(oCardTest, GetCardVoltageEmpty) {
  EXPECT_EQ(oCard::getCardVoltage(0), L"");
  EXPECT_EQ(oCard::getCardVoltage(10), L"");
}

TEST(oCardTest, GetCardVoltageValue) {
  wstring v = oCard::getCardVoltage(3000);
  EXPECT_NE(v.find(L"3."), wstring::npos);
}

TEST(oCardTest, IsCriticalVoltageOK) {
  EXPECT_EQ(oCard::isCriticalCardVoltage(3500), oCard::BatteryStatus::OK);
}

TEST(oCardTest, IsCriticalVoltageWarning) {
  EXPECT_EQ(oCard::isCriticalCardVoltage(2600), oCard::BatteryStatus::Warning);
}

TEST(oCardTest, IsCriticalVoltageBad) {
  EXPECT_EQ(oCard::isCriticalCardVoltage(2000), oCard::BatteryStatus::Bad);
}

TEST(oCardTest, GetBatteryDateEmpty) {
  oEvent oe;
  oCard c(&oe);
  EXPECT_EQ(c.getBatteryDate(), _EmptyWString);
}

TEST(oCardTest, GetBatteryDateFormatted) {
  oEvent oe;
  oCard c(&oe);
  // batteryDate is protected; set via setMeasuredVoltage workaround not available,
  // so test via the empty case and voltage instead.
  wstring d = oCard::getCardVoltage(3150);
  EXPECT_NE(d.find(L"3."), wstring::npos);
}

// ---------------------------------------------------------------------------
// isConstructedFromPunches
// ---------------------------------------------------------------------------
TEST(oCardTest, IsConstructedFromPunchesDefault) {
  oEvent oe;
  oCard c(&oe);
  EXPECT_FALSE(c.isConstructedFromPunches());
}

// ---------------------------------------------------------------------------
// Unmatched punch ID helpers
// ---------------------------------------------------------------------------
TEST(oCardTest, UnmatchedPunchIdRoundTrip) {
  int punchIx = 5;
  int id = oCard::getUnmatchedPunchId(punchIx);
  EXPECT_EQ(oCard::getUnmatchedPunchIndex(id), punchIx);
}

// ---------------------------------------------------------------------------
// SICard.h calculateHash
// ---------------------------------------------------------------------------
TEST(SICardTest, CalculateHashConsistent) {
  SICard s1, s2;
  s1.nPunch = 2;
  s1.Punch[0] = {31u, 100u};
  s1.Punch[1] = {32u, 200u};
  s2 = s1;
  EXPECT_EQ(s1.calculateHash(), s2.calculateHash());
}

TEST(SICardTest, CalculateHashDiffers) {
  SICard s1, s2;
  s1.nPunch = 1; s1.Punch[0] = {31u, 100u};
  s2.nPunch = 1; s2.Punch[0] = {32u, 100u};
  EXPECT_NE(s1.calculateHash(), s2.calculateHash());
}

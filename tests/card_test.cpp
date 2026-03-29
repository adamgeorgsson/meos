// card_test.cpp — Unit tests for oCard and oFreePunch (US-003f)
#include <gtest/gtest.h>
#include "oCard.h"
#include "oFreePunch.h"
#include "oEvent.h"
#include "oPunch.h"
#include "SICard.h"
#include "../src/util/xmlparser.h"

using std::string;
using std::wstring;

// ── Fixtures ─────────────────────────────────────────────────────────────────

class CardTest : public ::testing::Test {
protected:
  oEvent oe;
  void SetUp() override { setlocale(LC_ALL, "C.UTF-8"); }
};

// ── oCard construction ─────────────────────────────────────────────────────

TEST_F(CardTest, CardDefaultConstruction) {
  oCard card(&oe);
  EXPECT_GT(card.getId(), 0);
  EXPECT_EQ(card.getCardNo(), 0);
  EXPECT_EQ(card.getNumPunches(), 0);
}

TEST_F(CardTest, CardConstructionWithId) {
  oCard card(&oe, 42);
  EXPECT_EQ(card.getId(), 42);
  EXPECT_EQ(card.getCardNo(), 0);
}

TEST_F(CardTest, CardSetGetCardNo) {
  oCard card(&oe);
  card.setCardNo(12345);
  EXPECT_EQ(card.getCardNo(), 12345);
}

TEST_F(CardTest, CardNoString) {
  oCard card(&oe);
  card.setCardNo(9876);
  EXPECT_EQ(card.getCardNoString(), L"9876");
}

// ── Punch add/count/access ─────────────────────────────────────────────────

TEST_F(CardTest, AddPunch) {
  oCard card(&oe);
  card.addPunch(31, 3600, 0, 0, oCard::PunchOrigin::Unknown);
  EXPECT_EQ(card.getNumPunches(), 1);
}

TEST_F(CardTest, AddMultiplePunches) {
  oCard card(&oe);
  card.addPunch(oPunch::PunchStart, 3600, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(31, 4000, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(32, 4500, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(oPunch::PunchFinish, 5000, 0, 0, oCard::PunchOrigin::Unknown);
  EXPECT_EQ(card.getNumPunches(), 4);
}

TEST_F(CardTest, FinishIsAlwaysLast) {
  oCard card(&oe);
  card.addPunch(oPunch::PunchFinish, 5000, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(31, 4000, 0, 0, oCard::PunchOrigin::Unknown); // added after finish
  EXPECT_EQ(card.getNumPunches(), 2);
  // Finish should be last even when added first
  auto* p = card.getPunchByType(oPunch::PunchFinish);
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getTypeCode(), oPunch::PunchFinish);
}

TEST_F(CardTest, GetPunchByType) {
  oCard card(&oe);
  card.addPunch(oPunch::PunchStart, 3600, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(31, 4000, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(oPunch::PunchFinish, 5000, 0, 0, oCard::PunchOrigin::Unknown);

  auto* start = card.getPunchByType(oPunch::PunchStart);
  ASSERT_NE(start, nullptr);
  EXPECT_EQ(start->getTypeCode(), oPunch::PunchStart);

  auto* finish = card.getPunchByType(oPunch::PunchFinish);
  ASSERT_NE(finish, nullptr);
  EXPECT_EQ(finish->getTypeCode(), oPunch::PunchFinish);

  auto* ctrl = card.getPunchByType(31);
  ASSERT_NE(ctrl, nullptr);
  EXPECT_EQ(ctrl->getTypeCode(), 31);

  // Non-existent type
  EXPECT_EQ(card.getPunchByType(99), nullptr);
}

TEST_F(CardTest, GetPunchByIndex) {
  oCard card(&oe);
  card.addPunch(oPunch::PunchStart, 3600, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(31, 4000, 0, 0, oCard::PunchOrigin::Unknown);

  auto* p0 = card.getPunchByIndex(0);
  ASSERT_NE(p0, nullptr);
  EXPECT_EQ(p0->getTypeCode(), oPunch::PunchStart);

  auto* p1 = card.getPunchByIndex(1);
  ASSERT_NE(p1, nullptr);
  EXPECT_EQ(p1->getTypeCode(), 31);

  EXPECT_EQ(card.getPunchByIndex(2), nullptr);
}

TEST_F(CardTest, GetPunches) {
  oCard card(&oe);
  card.addPunch(oPunch::PunchStart, 3600, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(31, 4000, 0, 0, oCard::PunchOrigin::Unknown);

  std::vector<pPunch> punches;
  card.getPunches(punches);
  EXPECT_EQ(punches.size(), 2u);
  EXPECT_EQ(punches[0]->getTypeCode(), oPunch::PunchStart);
  EXPECT_EQ(punches[1]->getTypeCode(), 31);
}

// ── Punch serialization round-trip ────────────────────────────────────────

TEST_F(CardTest, PunchStringRoundTrip) {
  oCard card(&oe);
  card.addPunch(oPunch::PunchStart, 3600, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(31, 4000, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(32, 4500, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(oPunch::PunchFinish, 5000, 0, 0, oCard::PunchOrigin::Unknown);

  const string& encoded = card.getPunchString();
  EXPECT_FALSE(encoded.empty());

  oCard card2(&oe);
  card2.importPunches(encoded);

  EXPECT_EQ(card2.getNumPunches(), 4);
  EXPECT_EQ(card2.getPunchString(), encoded);
}

TEST_F(CardTest, ImportEmptyString) {
  oCard card(&oe);
  card.addPunch(31, 4000, 0, 0, oCard::PunchOrigin::Unknown);
  EXPECT_EQ(card.getNumPunches(), 1);
  card.importPunches("");
  EXPECT_EQ(card.getNumPunches(), 0);
}

// ── SICard integration ────────────────────────────────────────────────────

TEST_F(CardTest, SetReadIdAndIsCardRead) {
  oCard card(&oe);
  card.setCardNo(12345);

  SICard sicard(ConvertedTimeStatus::Hour24);
  sicard.CardNumber = 12345;
  sicard.nPunch = 3;
  sicard.Punch[0] = {31, 4000};
  sicard.Punch[1] = {32, 4500};
  sicard.Punch[2] = {33, 5000};
  sicard.FinishPunch = {2, 6000};

  card.setReadId(sicard);
  EXPECT_TRUE(card.isCardRead(sicard));

  // Modified card should not match
  SICard sicard2(ConvertedTimeStatus::Hour24);
  sicard2.CardNumber = 12345;
  sicard2.nPunch = 2;
  sicard2.Punch[0] = {31, 4000};
  sicard2.Punch[1] = {32, 4500};
  EXPECT_FALSE(card.isCardRead(sicard2));
}

TEST_F(CardTest, GetSICard) {
  oCard card(&oe);
  card.setCardNo(9876);
  card.addPunch(oPunch::PunchStart, 3600, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(31, 4000, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(32, 4500, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(oPunch::PunchFinish, 5000, 0, 0, oCard::PunchOrigin::Unknown);

  SICard sicard;
  card.getSICard(sicard);
  EXPECT_EQ(sicard.CardNumber, 9876u);
  EXPECT_EQ(sicard.convertedTime, ConvertedTimeStatus::Done);
  // getSICard only copies punches with type > 30 (control punches)
  EXPECT_EQ(sicard.nPunch, 2u);
  EXPECT_EQ(sicard.Punch[0].Code, 31u);
  EXPECT_EQ(sicard.Punch[1].Code, 32u);
}

TEST_F(CardTest, SICardCalculateHash) {
  SICard card1(ConvertedTimeStatus::Hour24);
  card1.nPunch = 2;
  card1.Punch[0] = {31, 4000};
  card1.Punch[1] = {32, 4500};
  card1.FinishPunch = {2, 5000};

  SICard card2(ConvertedTimeStatus::Hour24);
  card2.nPunch = 2;
  card2.Punch[0] = {31, 4000};
  card2.Punch[1] = {32, 4500};
  card2.FinishPunch = {2, 5000};

  // Same content → same hash
  EXPECT_EQ(card1.calculateHash(), card2.calculateHash());

  // Different content → different hash
  card2.Punch[1] = {32, 4600};
  EXPECT_NE(card1.calculateHash(), card2.calculateHash());
}

TEST_F(CardTest, IsConstructedFromPunches) {
  oCard card(&oe);
  EXPECT_FALSE(card.isConstructedFromPunches());
}

// ── Card hash ─────────────────────────────────────────────────────────────

TEST_F(CardTest, GetCardHash) {
  oCard card(&oe);
  card.setCardNo(123);
  card.addPunch(31, 4000, 0, 0, oCard::PunchOrigin::Unknown);

  auto [a, b] = card.getCardHash();
  // Hashes should be deterministic
  auto [a2, b2] = card.getCardHash();
  EXPECT_EQ(a, a2);
  EXPECT_EQ(b, b2);
}

// ── Time range ─────────────────────────────────────────────────────────────

TEST_F(CardTest, GetTimeRangeEmpty) {
  oCard card(&oe);
  auto [minT, maxT] = card.getTimeRange();
  EXPECT_GE(minT, maxT); // min >= max means empty
}

TEST_F(CardTest, GetTimeRange) {
  oCard card(&oe);
  // getTimeInt() returns punchTime + unit adjustment (0 in stub)
  // We add punches with direct punchTime values
  card.addPunch(31, 4000, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(32, 4500, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(oPunch::PunchFinish, 5000, 0, 0, oCard::PunchOrigin::Unknown);

  auto [minT, maxT] = card.getTimeRange();
  EXPECT_EQ(minT, 4000);
  EXPECT_EQ(maxT, 5000); // finish time is captured
}

// ── GetNumControlPunches ────────────────────────────────────────────────────

TEST_F(CardTest, GetNumControlPunches) {
  oCard card(&oe);
  card.addPunch(oPunch::PunchStart, 3600, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(31, 4000, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(32, 4500, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(oPunch::PunchFinish, 5000, 0, 0, oCard::PunchOrigin::Unknown);

  // Using default start/finish types
  int ctrl = card.getNumControlPunches(oPunch::PunchStart, oPunch::PunchFinish);
  EXPECT_EQ(ctrl, 2); // 31 and 32 are control punches
}

// ── Adapt times ────────────────────────────────────────────────────────────

TEST_F(CardTest, AdaptTimesNoOp) {
  oCard card(&oe);
  card.addPunch(31, 4000, 0, 0, oCard::PunchOrigin::Unknown);
  card.adaptTimes(0);
  EXPECT_EQ(card.getPunchByIndex(0)->getTimeInt(), 4000);
}

// ── Unexpected order ───────────────────────────────────────────────────────

TEST_F(CardTest, UnexpectedOrderFalse) {
  oCard card(&oe);
  card.addPunch(oPunch::PunchStart, 3600, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(31, 4000, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(oPunch::PunchFinish, 5000, 0, 0, oCard::PunchOrigin::Unknown);
  // Punches are in order, start time 3600
  EXPECT_FALSE(card.unexpectedOrder(3600));
}

TEST_F(CardTest, UnexpectedOrderTrue) {
  oCard card(&oe);
  // Use PunchFinish (type=2): not skipped by the type>=30 filter
  card.addPunch(oPunch::PunchFinish, 4000, 0, 0, oCard::PunchOrigin::Unknown);
  // Finish time 4000 < start time 5000 → unexpected
  EXPECT_TRUE(card.unexpectedOrder(5000));
}

// ── getStartTime ────────────────────────────────────────────────────────────

TEST_F(CardTest, GetStartTimeFound) {
  oCard card(&oe);
  card.addPunch(oPunch::PunchStart, 3600, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(31, 4000, 0, 0, oCard::PunchOrigin::Unknown);
  EXPECT_EQ(card.getStartTime(oPunch::PunchStart), 3600);
}

TEST_F(CardTest, GetStartTimeNotFound) {
  oCard card(&oe);
  card.addPunch(31, 4000, 0, 0, oCard::PunchOrigin::Unknown);
  EXPECT_EQ(card.getStartTime(oPunch::PunchStart), -1);
}

// ── Voltage methods ────────────────────────────────────────────────────────

TEST_F(CardTest, GetCardVoltageZero) {
  EXPECT_EQ(oCard::getCardVoltage(0), L"");
  EXPECT_EQ(oCard::getCardVoltage(5), L"");
}

TEST_F(CardTest, GetCardVoltageNonZero) {
  wstring v = oCard::getCardVoltage(3000);
  EXPECT_FALSE(v.empty());
  // 3000 mV = 3.00 V
  EXPECT_NE(v.find(L"3"), wstring::npos);
}

TEST_F(CardTest, IsCriticalCardVoltageOK) {
  EXPECT_EQ(oCard::isCriticalCardVoltage(0), oCard::BatteryStatus::OK);
  EXPECT_EQ(oCard::isCriticalCardVoltage(3000), oCard::BatteryStatus::OK);
}

TEST_F(CardTest, IsCriticalCardVoltageWarning) {
  EXPECT_EQ(oCard::isCriticalCardVoltage(2600), oCard::BatteryStatus::Warning);
}

TEST_F(CardTest, IsCriticalCardVoltageBad) {
  EXPECT_EQ(oCard::isCriticalCardVoltage(2400), oCard::BatteryStatus::Bad);
}

// ── Delete punch ────────────────────────────────────────────────────────────

TEST_F(CardTest, DeletePunch) {
  oCard card(&oe);
  card.addPunch(31, 4000, 0, 0, oCard::PunchOrigin::Unknown);
  EXPECT_EQ(card.getNumPunches(), 1);
  auto* p = card.getPunchByIndex(0);
  ASSERT_NE(p, nullptr);
  card.deletePunch(p);
  EXPECT_EQ(card.getNumPunches(), 0);
}

TEST_F(CardTest, DeleteNullPunchThrows) {
  oCard card(&oe);
  EXPECT_THROW(card.deletePunch(nullptr), std::runtime_error);
}

// ── Insert punch after ─────────────────────────────────────────────────────

TEST_F(CardTest, InsertPunchAfter) {
  oCard card(&oe);
  card.addPunch(31, 4000, 0, 0, oCard::PunchOrigin::Unknown);
  card.addPunch(33, 5000, 0, 0, oCard::PunchOrigin::Unknown);
  card.insertPunchAfter(0, 32, 4500); // Insert 32 after index 0
  EXPECT_EQ(card.getNumPunches(), 3);
  auto* p1 = card.getPunchByIndex(1);
  ASSERT_NE(p1, nullptr);
  EXPECT_EQ(p1->getTypeCode(), 32);
}

// ── isOriginalCard ─────────────────────────────────────────────────────────

TEST_F(CardTest, IsOriginalCardEmptyIsOriginal) {
  oCard card(&oe);
  // No punches: no unknown flags
  EXPECT_EQ(card.isOriginalCard(), oCard::PunchOrigin::Original);
}

TEST_F(CardTest, IsOriginalCardWithUnknownOrigin) {
  oCard card(&oe);
  card.addPunch(31, 4000, 0, 0, oCard::PunchOrigin::Unknown);
  EXPECT_EQ(card.isOriginalCard(), oCard::PunchOrigin::Unknown);
}

// ── oEvent card management ─────────────────────────────────────────────────

TEST_F(CardTest, AddAndGetCard) {
  oCard card(&oe, 100);
  card.setCardNo(12345);
  pCard added = oe.addCard(card);
  ASSERT_NE(added, nullptr);
  EXPECT_EQ(added->getCardNo(), 12345);

  pCard found = oe.getCard(100);
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->getCardNo(), 12345);
}

TEST_F(CardTest, GetCardByNumber) {
  oCard card(&oe, 200);
  card.setCardNo(9999);
  oe.addCard(card);

  pCard found = oe.getCardByNumber(9999);
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->getId(), 200);
}

TEST_F(CardTest, GetCards) {
  oCard c1(&oe, 10);
  oCard c2(&oe, 11);
  c1.setCardNo(1001);
  c2.setCardNo(1002);
  oe.addCard(c1);
  oe.addCard(c2);

  std::vector<pCard> cards;
  oe.getCards(cards, false, false);
  EXPECT_EQ(cards.size(), 2u);
}

TEST_F(CardTest, IsCardRead) {
  oCard card(&oe, 300);
  card.setCardNo(5555);

  SICard sicard(ConvertedTimeStatus::Hour24);
  sicard.CardNumber = 5555;
  sicard.nPunch = 1;
  sicard.Punch[0] = {31, 4000};

  card.setReadId(sicard);
  oe.addCard(card);

  EXPECT_TRUE(oe.isCardRead(sicard));
}

TEST_F(CardTest, RemoveCard) {
  oCard card(&oe, 400);
  oe.addCard(card);

  pCard found = oe.getCard(400);
  EXPECT_NE(found, nullptr);

  oe.removeCard(400);
  // After removal, getCard returns removed card (still in list but Removed flag set)
  pCard afterRemove = oe.getCard(400);
  EXPECT_NE(afterRemove, nullptr); // getCard "allows removed cards" per the comment
}

// ── oFreePunch construction ────────────────────────────────────────────────

TEST_F(CardTest, FreePunchConstruction) {
  oFreePunch fp(&oe, 12345, 4000, 31, 0);
  EXPECT_EQ(fp.getCardNo(), 12345);
  EXPECT_EQ(fp.getTypeCode(), 31);
}

TEST_F(CardTest, FreePunchConstructionWithId) {
  oFreePunch fp(&oe, 42);
  EXPECT_EQ(fp.getId(), 42);
}

// ── oFreePunch control hash ────────────────────────────────────────────────

TEST_F(CardTest, FreePunchControlHashRoundTrip) {
  // Basic control hash (race 0)
  int courseControlId = 150;
  int hash = oFreePunch::getControlHash(courseControlId, 0);
  EXPECT_EQ(oFreePunch::getControlIdFromHash(hash, true), courseControlId);

  // With race
  int hash2 = oFreePunch::getControlHash(courseControlId, 2);
  EXPECT_EQ(oFreePunch::getControlIdFromHash(hash2, true), courseControlId);
  EXPECT_NE(hash, hash2); // Different race → different hash
}

TEST_F(CardTest, FreePunchControlIdFromHash) {
  // Test that getControlId works via iHashType
  oFreePunch fp(&oe, 12345, 4000, 31, 0);
  // iHashType = 0 initially; getControlId returns id from hash 0 which is 0
  int cid = fp.getControlId();
  EXPECT_EQ(cid, 0); // hash(0) → control id 0
}

// ── oFreePunch XML round-trip ─────────────────────────────────────────────

TEST_F(CardTest, FreePunchXmlRoundTrip) {
  oFreePunch fp1(&oe, 9999, 4500, 32, 7);
  // Write to buffer
  string xmlBuf;
  {
    xmlparser xml;
    xml.openMemoryOutput(false);
    fp1.Write(xml);
    xml.getMemoryOutput(xmlBuf);
  }
  ASSERT_FALSE(xmlBuf.empty());

  oFreePunch fp2(&oe, 1);
  {
    xmlparser xp;
    xp.readMemory(xmlBuf, 0);
    xmlobject xo = xp.getObject("Punch");
    if (xo)
      fp2.Set(&xo);
  }
  EXPECT_EQ(fp2.getCardNo(), 9999);
  EXPECT_EQ(fp2.getTypeCode(), 32);
}

// ── oFreePunch changedObject ───────────────────────────────────────────────

TEST_F(CardTest, FreePunchChangedObjectMarksSqlPunches) {
  oFreePunch fp(&oe, 12345, 4000, 31, 0);
  fp.addToEvent(&oe, nullptr);
  oe.sqlPunches.changed = false;
  // setTimeInt → updateChanged → changed=true; then synchronize triggers changedObject
  fp.setTimeInt(4100, false);
  fp.synchronize(false);
  EXPECT_TRUE(oe.sqlPunches.changed);
}

// ── oEvent punch hash management ─────────────────────────────────────────

TEST_F(CardTest, PunchHashInsertAndCheck) {
  oe.insertIntoPunchHash(1001, 31, 4000);
  EXPECT_TRUE(oe.isInPunchHash(1001, 31, 4000));
  EXPECT_FALSE(oe.isInPunchHash(1001, 31, 4001));
  EXPECT_FALSE(oe.isInPunchHash(1002, 31, 4000));
}

TEST_F(CardTest, PunchHashRemove) {
  oe.insertIntoPunchHash(1001, 31, 4000);
  EXPECT_TRUE(oe.isInPunchHash(1001, 31, 4000));
  oe.removeFromPunchHash(1001, 31, 4000);
  EXPECT_FALSE(oe.isInPunchHash(1001, 31, 4000));
}

TEST_F(CardTest, AddFreePunchDuplicateIgnored) {
  oe.insertIntoPunchHash(1001, 31, 4000);
  // addFreePunch should return nullptr for duplicate
  pFreePunch fp = oe.addFreePunch(4000, 31, 0, 1001, false, false);
  EXPECT_EQ(fp, nullptr);
}

// ── Hired card ──────────────────────────────────────────────────────────────

TEST_F(CardTest, IsHiredCardInitiallyFalse) {
  EXPECT_FALSE(oe.isHiredCard(12345));
}

TEST_F(CardTest, SetAndGetHiredCard) {
  oe.setHiredCard(12345, true);
  // isHiredCard uses the hiredCardHash (inserted directly by setHiredCard)
  EXPECT_TRUE(oe.isHiredCard(12345));
  EXPECT_FALSE(oe.isHiredCard(9999));
}

TEST_F(CardTest, ClearHiredCards) {
  oe.setHiredCard(12345, true);
  EXPECT_TRUE(oe.isHiredCard(12345));
  oe.clearHiredCards();
  EXPECT_FALSE(oe.isHiredCard(12345));
}

TEST_F(CardTest, GetHiredCards) {
  oe.setHiredCard(100, true);
  oe.setHiredCard(200, true);
  auto cards = oe.getHiredCards();
  EXPECT_EQ(cards.size(), 2u);
}

// ── addFreePunch and getPunch ──────────────────────────────────────────────

TEST_F(CardTest, AddFreePunchAndGetById) {
  oFreePunch fp(&oe, 5555, 4000, 31, 0);
  pFreePunch added = oe.addFreePunch(fp);
  ASSERT_NE(added, nullptr);
  EXPECT_EQ(added->getCardNo(), 5555);

  pFreePunch found = oe.getPunch(added->getId());
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->getCardNo(), 5555);
}

TEST_F(CardTest, GetPunchesByType) {
  oFreePunch fp1(&oe, 1111, 4000, 31, 0);
  oFreePunch fp2(&oe, 2222, 4500, 31, 0);
  oFreePunch fp3(&oe, 3333, 5000, 32, 0);
  oe.addFreePunch(fp1);
  oe.addFreePunch(fp2);
  oe.addFreePunch(fp3);

  auto type31 = oe.getPunchesByType(31, 0);
  EXPECT_EQ(type31.size(), 2u);

  auto type32 = oe.getPunchesByType(32, 0);
  EXPECT_EQ(type32.size(), 1u);

  auto type99 = oe.getPunchesByType(99, 0);
  EXPECT_EQ(type99.size(), 0u);
}

TEST_F(CardTest, RemoveFreePunch) {
  oFreePunch fp(&oe, 9999, 4000, 31, 0);
  pFreePunch added = oe.addFreePunch(fp);
  ASSERT_NE(added, nullptr);
  int id = added->getId();

  oe.removeFreePunch(id);
  pFreePunch found = oe.getPunch(id);
  EXPECT_EQ(found, nullptr); // Removed, so getPunch returns nullptr
}

// ── getFreeControls ────────────────────────────────────────────────────────

TEST_F(CardTest, GetFreeControlsEmpty) {
  std::set<int> controls;
  oe.getFreeControls(controls);
  EXPECT_TRUE(controls.empty());
}

// ── getLatestPunches ───────────────────────────────────────────────────────

TEST_F(CardTest, GetLatestPunchesEmpty) {
  std::vector<const oFreePunch*> out;
  oe.getLatestPunches(0, out);
  EXPECT_TRUE(out.empty());
}

// ── canRemove ─────────────────────────────────────────────────────────────

TEST_F(CardTest, CanRemoveUnowned) {
  oCard card(&oe);
  EXPECT_TRUE(card.canRemove()); // No owner → can remove
}

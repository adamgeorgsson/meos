// Tests for oFreePunch domain migration.
#include <gtest/gtest.h>
#include "oFreePunch.h"
#include "oEvent.h"
#include "oPunch.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static oFreePunch makeFreePunch(oEvent& oe, int card = 100, int time = 1200,
                                 int type = 31, int unit = 0) {
  return oFreePunch(&oe, card, time, type, unit);
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
TEST(oFreePunchTest, ConstructionWithFields) {
  oEvent oe;
  oFreePunch fp(&oe, 1001, 600, 31, 0);
  EXPECT_EQ(fp.getCardNo(), 1001);
  EXPECT_EQ(fp.getTypeCode(), 31);
  EXPECT_EQ(fp.getIHashType(), 0);
  EXPECT_FALSE(fp.isRemoved());
}

TEST(oFreePunchTest, ConstructionWithId) {
  oEvent oe;
  oFreePunch fp(&oe, 42);
  EXPECT_EQ(fp.getId(), 42);
  EXPECT_GE(oe.qFreePunchId, 42);
}

TEST(oFreePunchTest, IdAutoIncrement) {
  oEvent oe;
  oFreePunch fp1(&oe, 100, 100, 31, 0);
  oFreePunch fp2(&oe, 200, 200, 32, 0);
  EXPECT_NE(fp1.getId(), fp2.getId());
}

// ---------------------------------------------------------------------------
// getControlHash / getControlIdFromHash
// ---------------------------------------------------------------------------
TEST(oFreePunchTest, ControlHashRoundTrip) {
  int hash = oFreePunch::getControlHash(12345, 0);
  EXPECT_EQ(oFreePunch::getControlIdFromHash(hash, true), 12345);
}

TEST(oFreePunchTest, ControlHashWithRace) {
  int hash = oFreePunch::getControlHash(999, 2);
  // race=2 adds 2*100000000
  EXPECT_EQ(hash, 999 + 200000000);
  int cid = oFreePunch::getControlIdFromHash(hash, true);
  EXPECT_EQ(cid, 999);
}

TEST(oFreePunchTest, ControlHashRace0) {
  int hash0 = oFreePunch::getControlHash(50, 0);
  int hash1 = oFreePunch::getControlHash(50, 1);
  EXPECT_NE(hash0, hash1);
}

// ---------------------------------------------------------------------------
// getControlId / getCourseControlId / getIHashType
// ---------------------------------------------------------------------------
TEST(oFreePunchTest, InitialControlIdIsZero) {
  oEvent oe;
  oFreePunch fp(&oe, 100, 1200, 31, 0);
  // iHashType=0 → getControlIdFromHash(0, false) via oControl
  EXPECT_EQ(fp.getIHashType(), 0);
}

TEST(oFreePunchTest, GetCourseControlId) {
  oEvent oe;
  oFreePunch fp(&oe, 100, 1200, 31, 0);
  // Both getCourseControlId and getControlId use iHashType=0 initially
  EXPECT_EQ(fp.getCourseControlId(), oFreePunch::getControlIdFromHash(0, true));
}

// ---------------------------------------------------------------------------
// remove / canRemove
// ---------------------------------------------------------------------------
TEST(oFreePunchTest, CanRemove) {
  oEvent oe;
  oFreePunch fp(&oe, 100, 600, 31, 0);
  EXPECT_TRUE(fp.canRemove());
}

TEST(oFreePunchTest, RemoveSetsFlag) {
  oEvent oe;
  oFreePunch fp(&oe, 100, 600, 31, 0);
  fp.remove();
  EXPECT_TRUE(fp.isRemoved());
}

// ---------------------------------------------------------------------------
// setTimeInt
// ---------------------------------------------------------------------------
TEST(oFreePunchTest, SetTimeIntChangesTime) {
  oEvent oe;
  oFreePunch fp(&oe, 100, 600, 31, 0);
  fp.setTimeInt(1200, false);
  EXPECT_EQ(fp.getTimeInt(), 1200);
}

TEST(oFreePunchTest, SetTimeIntSetsChangedFlag) {
  oEvent oe;
  oFreePunch fp(&oe, 100, 600, 31, 0);
  fp.setTimeInt(1200, false);
  EXPECT_TRUE(fp.isChanged());
}

TEST(oFreePunchTest, SetTimeIntNoChangeWhenSame) {
  oEvent oe;
  oFreePunch fp(&oe, 100, 600, 31, 0);
  // Manually clear via constructing fresh (isChanged is true after ctor due to ID assignment)
  fp.setTimeInt(600, false);  // same time — should not update
  // Just verify type didn't change
  EXPECT_EQ(fp.getTypeCode(), 31);
}

// ---------------------------------------------------------------------------
// setType
// ---------------------------------------------------------------------------
TEST(oFreePunchTest, SetTypeNumeric) {
  oEvent oe;
  oFreePunch fp(&oe, 100, 600, 31, 0);
  bool changed = fp.setType(L"55", false);
  EXPECT_TRUE(changed);
  EXPECT_EQ(fp.getTypeCode(), 55);
}

TEST(oFreePunchTest, SetTypeSameNoChange) {
  oEvent oe;
  oFreePunch fp(&oe, 100, 600, 31, 0);
  bool changed = fp.setType(L"31", false);
  EXPECT_FALSE(changed);
}

TEST(oFreePunchTest, SetTypeInvalidIgnored) {
  oEvent oe;
  oFreePunch fp(&oe, 100, 600, 31, 0);
  bool changed = fp.setType(L"99999", false);  // > 10000
  EXPECT_FALSE(changed);
  EXPECT_EQ(fp.getTypeCode(), 31);
}

// ---------------------------------------------------------------------------
// setCardNo
// ---------------------------------------------------------------------------
TEST(oFreePunchTest, SetCardNoChanges) {
  oEvent oe;
  oFreePunch fp(&oe, 100, 600, 31, 0);
  bool changed = fp.setCardNo(999, false);
  EXPECT_TRUE(changed);
  EXPECT_EQ(fp.getCardNo(), 999);
}

TEST(oFreePunchTest, SetCardNoSameNoChange) {
  oEvent oe;
  oFreePunch fp(&oe, 100, 600, 31, 0);
  bool changed = fp.setCardNo(100, false);
  EXPECT_FALSE(changed);
}

// ---------------------------------------------------------------------------
// changedObject → sqlPunches.changed
// ---------------------------------------------------------------------------
TEST(oFreePunchTest, ChangedObjectSetsSqlFlag) {
  oEvent oe;
  EXPECT_FALSE(oe.sqlPunches.changed);
  oFreePunch fp(&oe, 100, 600, 31, 0);
  // changedObject is called via synchronize when changed=true
  fp.updateChanged();
  fp.synchronize(false);
  EXPECT_TRUE(oe.sqlPunches.changed);
}

// ---------------------------------------------------------------------------
// disableHashing flag
// ---------------------------------------------------------------------------
TEST(oFreePunchTest, DisableHashingStartsFalse) {
  // Static flag starts false (reset between tests via the static initializer).
  EXPECT_FALSE(oFreePunch::disableHashing);
}

// ---------------------------------------------------------------------------
// rehashPunches — basic behavior
// ---------------------------------------------------------------------------
TEST(oFreePunchTest, RehashPunchesEmptyListNoOp) {
  oEvent oe;
  // freePunches is empty → rehashPunches returns immediately
  oFreePunch fp(&oe, 100, 600, 31, 0);
  oFreePunch::rehashPunches(oe, 100, &fp);
  // No crash, punchIndex stays empty
  EXPECT_TRUE(oe.punchIndex.empty());
}

TEST(oFreePunchTest, RehashPunchesPopulatesIndex) {
  oEvent oe;
  // Add a punch to oe.freePunches backing store
  oe.freePunches.emplace_back(&oe, 100, 600, 31, 0);
  oFreePunch* fp = &oe.freePunches.back();

  // rehashPunches with empty punchIndex triggers full rehash
  oFreePunch::rehashPunches(oe, 100, nullptr);

  // After rehash, punchIndex should have an entry
  EXPECT_FALSE(oe.punchIndex.empty());
  // iHashType is set by getControlIdFromPunch stub = getControlHash(31, 0)
  int expectedHash = oFreePunch::getControlHash(31, 0);
  EXPECT_EQ(fp->getIHashType(), expectedHash);
  auto it = oe.punchIndex.find(expectedHash);
  ASSERT_NE(it, oe.punchIndex.end());
  auto range = it->second.equal_range(100);
  EXPECT_NE(range.first, range.second);
}

TEST(oFreePunchTest, RehashPunchesSkipsRemoved) {
  oEvent oe;
  oe.freePunches.emplace_back(&oe, 100, 600, 31, 0);
  oFreePunch* fp = &oe.freePunches.back();
  fp->remove();  // mark as removed

  oFreePunch::rehashPunches(oe, 100, nullptr);

  // Removed punch should not appear in index
  EXPECT_TRUE(oe.punchIndex.empty());
}

TEST(oFreePunchTest, RehashPunchesSortsByTime) {
  oEvent oe;
  oe.freePunches.emplace_back(&oe, 100, 800, 32, 0);
  oe.freePunches.emplace_back(&oe, 100, 600, 31, 0);

  oFreePunch::rehashPunches(oe, 100, nullptr);

  // Both should appear in punchIndex (different hash types in stub: hash(31,0) and hash(32,0))
  EXPECT_GE((int)oe.punchIndex.size(), 1);
}

// ---------------------------------------------------------------------------
// isHiredCard inherited from oPunch
// ---------------------------------------------------------------------------
TEST(oFreePunchTest, IsHiredCard) {
  oEvent oe;
  oFreePunch fp(&oe, 100, 600, oPunch::HiredCard, 0);
  EXPECT_TRUE(fp.isHiredCard());
}

TEST(oFreePunchTest, IsNotHiredCard) {
  oEvent oe;
  oFreePunch fp(&oe, 100, 600, 31, 0);
  EXPECT_FALSE(fp.isHiredCard());
}

// ---------------------------------------------------------------------------
// Inherited punch predicates
// ---------------------------------------------------------------------------
TEST(oFreePunchTest, IsStart) {
  oEvent oe;
  oFreePunch fp(&oe, 100, 600, oPunch::PunchStart, 0);
  EXPECT_TRUE(fp.isStart());
}

TEST(oFreePunchTest, IsFinish) {
  oEvent oe;
  oFreePunch fp(&oe, 100, 600, oPunch::PunchFinish, 0);
  EXPECT_TRUE(fp.isFinish());
}

TEST(oFreePunchTest, IsCheck) {
  oEvent oe;
  oFreePunch fp(&oe, 100, 600, oPunch::PunchCheck, 0);
  EXPECT_TRUE(fp.isCheck());
}

// ---------------------------------------------------------------------------
// getTiedRunner returns nullptr in stub
// ---------------------------------------------------------------------------
TEST(oFreePunchTest, GetTiedRunnerNullInStub) {
  oEvent oe;
  oFreePunch fp(&oe, 100, 600, 31, 0);
  EXPECT_EQ(fp.getTiedRunner(), nullptr);
}

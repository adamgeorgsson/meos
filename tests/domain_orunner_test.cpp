// Unit tests for oRunner and oAbstractRunner (US-003g1).

#include <gtest/gtest.h>
#include "oRunner.h"
#include "oEvent.h"

namespace {

// -----------------------------------------------------------------------
// Fixture
// -----------------------------------------------------------------------
struct RunnerTest : ::testing::Test {
  oEvent ev;
  oRunner* r = nullptr;

  void SetUp() override { r = new oRunner(&ev); }
  void TearDown() override { delete r; }
};

// -----------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------
TEST_F(RunnerTest, DefaultConstructorGivesId) {
  EXPECT_GT(r->getId(), 0);
}

TEST_F(RunnerTest, LoadingConstructorSetsId) {
  oRunner r2(&ev, 42);
  EXPECT_EQ(r2.getId(), 42);
}

TEST_F(RunnerTest, LoadingConstructorAdvancesFreeId) {
  int prev = ev.qFreeRunnerId;
  oRunner r2(&ev, prev + 100);
  EXPECT_EQ(ev.qFreeRunnerId, prev + 100);
}

// -----------------------------------------------------------------------
// Name
// -----------------------------------------------------------------------
TEST_F(RunnerTest, SetGetName) {
  r->setName(L"Alice", false);
  EXPECT_EQ(r->getName(), L"Alice");
}

TEST_F(RunnerTest, NameDefaultEmpty) {
  EXPECT_EQ(r->getName(), L"");
}

TEST_F(RunnerTest, MatchName) {
  r->setName(L"Bob", false);
  EXPECT_TRUE(r->matchName(L"Bob"));
  EXPECT_FALSE(r->matchName(L"Alice"));
}

TEST_F(RunnerTest, GivenAndFamilyName) {
  r->setName(L"Alice Smith", false);
  EXPECT_EQ(r->getGivenName(), L"Alice");
  EXPECT_EQ(r->getFamilyName(), L"Smith");
}

// -----------------------------------------------------------------------
// Club / class / course — initially 0/null
// -----------------------------------------------------------------------
TEST_F(RunnerTest, ClubIdInitiallyZero) {
  EXPECT_EQ(r->getClubId(), 0);
}

TEST_F(RunnerTest, ClassIdInitiallyZero) {
  EXPECT_EQ(r->getClassId(false), 0);
}

TEST_F(RunnerTest, CourseIdInitiallyZero) {
  EXPECT_EQ(r->getCourseId(), 0);
}

TEST_F(RunnerTest, SetClubIdNullStubDoesNotCrash) {
  r->setClubId(5);  // stub returns nullptr, club stays null
  EXPECT_EQ(r->getClubId(), 0);
}

TEST_F(RunnerTest, SetClassIdNullStubDoesNotCrash) {
  r->setClassId(3, false);  // stub returns nullptr
  EXPECT_EQ(r->getClassId(false), 0);
}

TEST_F(RunnerTest, SetCourseIdNullStubDoesNotCrash) {
  r->setCourseId(7);  // stub returns nullptr
  EXPECT_EQ(r->getCourseId(), 0);
}

// -----------------------------------------------------------------------
// Card
// -----------------------------------------------------------------------
TEST_F(RunnerTest, CardNoInitiallyZero) {
  EXPECT_EQ(r->getCardNo(), 0);
}

TEST_F(RunnerTest, SetCardNo) {
  r->setCardNo(12345, false);
  EXPECT_EQ(r->getCardNo(), 12345);
}

// -----------------------------------------------------------------------
// Time / status
// -----------------------------------------------------------------------
TEST_F(RunnerTest, StartTimeInitiallyZero) {
  EXPECT_EQ(r->getStartTime(), 0);
}

TEST_F(RunnerTest, SetStartTime) {
  r->setStartTime(3600, true, oBase::ChangeType::Update);
  EXPECT_EQ(r->getStartTime(), 3600);
}

TEST_F(RunnerTest, FinishTimeInitiallyZero) {
  EXPECT_EQ(r->getFinishTime(), 0);
}

TEST_F(RunnerTest, SetFinishTime) {
  r->setFinishTime(7200);
  EXPECT_EQ(r->getFinishTime(), 7200);
}

TEST_F(RunnerTest, RunningTimeCalculated) {
  r->setStartTime(1000, true, oBase::ChangeType::Update);
  r->setFinishTime(5000);
  EXPECT_EQ(r->getRunningTime(false), 4000);
}

TEST_F(RunnerTest, StatusInitiallyUnknown) {
  EXPECT_EQ(r->getStatus(), StatusUnknown);
}

TEST_F(RunnerTest, SetStatus) {
  r->setStatus(StatusOK, true, oBase::ChangeType::Update);
  EXPECT_EQ(r->getStatus(), StatusOK);
}

// -----------------------------------------------------------------------
// Status encoding/decoding
// -----------------------------------------------------------------------
TEST_F(RunnerTest, EncodeDecodeStatusOK) {
  EXPECT_EQ(oAbstractRunner::encodeStatus(StatusOK), L"OK");
  EXPECT_EQ(oAbstractRunner::decodeStatus(L"OK"), StatusOK);
}

TEST_F(RunnerTest, EncodeDecodeStatusDNS) {
  EXPECT_EQ(oAbstractRunner::encodeStatus(StatusDNS), L"NS");
  EXPECT_EQ(oAbstractRunner::decodeStatus(L"NS"), StatusDNS);
}

TEST_F(RunnerTest, EncodeDecodeStatusMP) {
  EXPECT_EQ(oAbstractRunner::encodeStatus(StatusMP), L"MP");
  EXPECT_EQ(oAbstractRunner::decodeStatus(L"MP"), StatusMP);
}

TEST_F(RunnerTest, DecodeUnknownStatus) {
  EXPECT_EQ(oAbstractRunner::decodeStatus(L"GARBAGE"), StatusUnknown);
}

// -----------------------------------------------------------------------
// Changed tracking
// -----------------------------------------------------------------------
TEST_F(RunnerTest, NewRunnerIsChanged) {
  EXPECT_TRUE(r->isChanged());
}

TEST_F(RunnerTest, ChangedObjectBumpsDataRevision) {
  int rev = ev.dataRevision;
  // setStatus with updateSource=false triggers changedObject() directly
  r->setStatus(StatusOK, false, oBase::ChangeType::Update);
  EXPECT_GT(ev.dataRevision, rev);
}

// -----------------------------------------------------------------------
// canRemove / remove
// -----------------------------------------------------------------------
TEST_F(RunnerTest, CanRemoveTrue) {
  EXPECT_TRUE(r->canRemove());
}

TEST_F(RunnerTest, RemoveSetsRemovedFlag) {
  r->remove();
  EXPECT_TRUE(r->isRemoved());
}

// -----------------------------------------------------------------------
// Multi-runner
// -----------------------------------------------------------------------
TEST_F(RunnerTest, GetMultiRunnerZeroReturnsSelf) {
  EXPECT_EQ(r->getMultiRunner(0), r);
}

TEST_F(RunnerTest, GetMultiRunnerNonZeroReturnsNull) {
  EXPECT_EQ(r->getMultiRunner(1), nullptr);
}

TEST_F(RunnerTest, NumMultiInitiallyZero) {
  EXPECT_EQ(r->getNumMulti(), 0);
}

// -----------------------------------------------------------------------
// Sex / birth
// -----------------------------------------------------------------------
TEST_F(RunnerTest, SexDefaultUnknown) {
  EXPECT_EQ(r->getSex(), sUnknown);
}

TEST_F(RunnerTest, SetSexMale) {
  r->setSex(sMale);
  EXPECT_EQ(r->getSex(), sMale);
}

TEST_F(RunnerTest, SetSexFemale) {
  r->setSex(sFemale);
  EXPECT_EQ(r->getSex(), sFemale);
}

TEST_F(RunnerTest, BirthYearDefault) {
  EXPECT_EQ(r->getBirthYear(), 0);
}

TEST_F(RunnerTest, SetBirthYear) {
  r->setBirthYear(1990);
  EXPECT_EQ(r->getBirthYear(), 1990);
}

// -----------------------------------------------------------------------
// DynamicValue
// -----------------------------------------------------------------------
TEST_F(RunnerTest, DynamicValueIsOldAfterReset) {
  oAbstractRunner::DynamicValue dv;
  dv.reset();
  EXPECT_TRUE(dv.isOld(ev));
}

TEST_F(RunnerTest, DynamicValueUpdateAndGet) {
  oAbstractRunner::DynamicValue dv;
  dv.update(ev, 0, 42, false);
  EXPECT_EQ(dv.get(false), 42);
}

// -----------------------------------------------------------------------
// Bib / start number
// -----------------------------------------------------------------------
TEST_F(RunnerTest, BibDefaultEmpty) {
  EXPECT_TRUE(r->getBib().empty());
}

TEST_F(RunnerTest, StartNoDefault) {
  EXPECT_EQ(r->getStartNo(), 0);
}

TEST_F(RunnerTest, SetStartNo) {
  r->setStartNo(5, oBase::ChangeType::Update);
  EXPECT_EQ(r->getStartNo(), 5);
}

// -----------------------------------------------------------------------
// TempResult
// -----------------------------------------------------------------------
TEST_F(RunnerTest, TempResultInitialStatusUnknown) {
  EXPECT_EQ(r->getTempResult().getStatus(), StatusUnknown);
}

} // namespace

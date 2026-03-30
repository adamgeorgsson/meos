// generalresult_test.cpp — Unit tests for US-003j (GeneralResult + result engine)
#include <gtest/gtest.h>
#include "oEvent.h"
#include "oRunner.h"
#include "oClass.h"
#include "oTeam.h"
#include "generalresult.h"
#include "oListInfo.h"

using std::wstring;
using std::vector;
using std::set;
using CT = oBase::ChangeType;

class GeneralResultTest : public ::testing::Test {
protected:
  oEvent oe;
  void SetUp() override { setlocale(LC_ALL, "C.UTF-8"); }
};

// ── GeneralResult construction ─────────────────────────────────────────────

TEST_F(GeneralResultTest, DefaultConstruct) {
  GeneralResult gr;
  EXPECT_FALSE(gr.isDynamic());
  EXPECT_FALSE(gr.isRogaining());
}

TEST_F(GeneralResultTest, ResultAtControlConstruct) {
  ResultAtControl rac;
  EXPECT_FALSE(rac.isDynamic());
}

// ── DynamicResult construction ─────────────────────────────────────────────

TEST_F(GeneralResultTest, DynamicResultConstruct) {
  DynamicResult dr;
  EXPECT_TRUE(dr.isDynamic());
  EXPECT_FALSE(dr.isRogaining());
}

TEST_F(GeneralResultTest, DynamicResultSetGetTag) {
  DynamicResult dr;
  dr.setTag("myTag");
  EXPECT_EQ(dr.getTag(), "myTag");
}

TEST_F(GeneralResultTest, DynamicResultSetGetName) {
  DynamicResult dr;
  dr.setName(L"My Custom Result");
  EXPECT_EQ(dr.getName(false), L"My Custom Result");
}

TEST_F(GeneralResultTest, DynamicResultCopyConstruct) {
  DynamicResult dr;
  dr.setTag("tag1");
  dr.setName(L"Name1");
  DynamicResult dr2(dr);
  EXPECT_EQ(dr2.getTag(), "tag1");
  EXPECT_EQ(dr2.getName(false), L"Name1");
}

TEST_F(GeneralResultTest, DynamicResultAssign) {
  DynamicResult dr;
  dr.setTag("orig");
  DynamicResult dr2;
  dr2 = dr;
  EXPECT_EQ(dr2.getTag(), "orig");
}

// ── GeneralResultCtr ───────────────────────────────────────────────────────

TEST_F(GeneralResultTest, GeneralResultCtrTagName) {
  auto ptr = std::make_shared<GeneralResult>();
  GeneralResultCtr ctr("testTag", L"Test Result", ptr);
  EXPECT_EQ(ctr.tag, "testTag");
  EXPECT_EQ(ctr.getName(), L"Test Result");
  EXPECT_FALSE(ctr.isDynamic());
}

TEST_F(GeneralResultTest, GeneralResultCtrDynamic) {
  auto ptr = std::make_shared<DynamicResult>();
  ptr->setName(L"DynRes");
  // GeneralResultCtr::isDynamic() checks fileSource, not ptr type.
  // Use the file-constructor to get isDynamic()==true.
  GeneralResultCtr ctr(wstring(L"myfile.xml"), ptr);
  EXPECT_TRUE(ctr.isDynamic());
  // The ptr itself reports isDynamic via GeneralResult::isDynamic()
  EXPECT_TRUE(ptr->isDynamic());
}

// ── TempResult fields ──────────────────────────────────────────────────────

TEST_F(GeneralResultTest, TempResultDefaultValues) {
  oAbstractRunner::TempResult tr;
  EXPECT_EQ(tr.runningTime, 0);
  EXPECT_EQ(tr.status, StatusUnknown);
  EXPECT_EQ(tr.points, 0);
  EXPECT_EQ(tr.place, 0);
  EXPECT_EQ(tr.startTime, 0);
  EXPECT_EQ(tr.timeAfter, 0);
}

TEST_F(GeneralResultTest, TempResultGetters) {
  oAbstractRunner::TempResult tr(1200, StatusOK, 5, 3);
  tr.startTime = 3600;
  EXPECT_EQ(tr.getRunningTime(), 1200);
  EXPECT_EQ(tr.getStatus(), StatusOK);
  EXPECT_EQ(tr.getPoints(), 5);
  EXPECT_EQ(tr.getPlace(), 3);
  EXPECT_EQ(tr.getStartTime(), 3600);
  EXPECT_EQ(tr.getFinishTime(), 3600 + 1200);
}

TEST_F(GeneralResultTest, TempResultReset) {
  oAbstractRunner::TempResult tr(999, StatusOK, 10, 2);
  tr.startTime = 3600;
  tr.timeAfter = 60;
  tr.outputTimes = {1, 2, 3};
  tr.reset();
  EXPECT_EQ(tr.runningTime, 0);
  EXPECT_EQ(tr.status, StatusUnknown);
  EXPECT_EQ(tr.startTime, 0);
  EXPECT_EQ(tr.timeAfter, 0);
  EXPECT_TRUE(tr.outputTimes.empty());
}

// ── Individual result calculation ──────────────────────────────────────────

TEST_F(GeneralResultTest, IndividualResults_BasicSorting) {
  pClass cls = oe.addClass(L"H21");
  ASSERT_NE(cls, nullptr);

  // Three runners with different times
  struct RInfo { const wchar_t* name; int start; int finish; };
  RInfo info[] = {
    { L"Slow",   3600, 5400 },  // 30 min
    { L"Fast",   3600, 4800 },  // 20 min
    { L"Medium", 3600, 5100 },  // 25 min
  };

  vector<pRunner> runners;
  for (auto& ri : info) {
    oRunner r(&oe);
    r.setName(ri.name, false);
    pRunner pr = oe.addRunner(r);
    pr->setClassId(cls->getId(), false);
    pr->setStartTime(ri.start, true, CT::Update);
    pr->setFinishTime(ri.finish);
    pr->setStatus(StatusOK, true, CT::Update);
    runners.push_back(pr);
  }

  vector<GeneralResult::GeneralResultInfo> results;
  auto controlId = std::make_pair(oPunch::PunchStart, oPunch::PunchFinish);
  GeneralResult::calculateIndividualResults(
    runners, controlId, false, true, true,
    "", oListInfo::ResultType::Classwise, 0, oe, results
  );

  ASSERT_EQ(results.size(), 3u);

  // After sorting, Fast should be place 1
  bool foundFast = false;
  for (const auto& ri : results) {
    if (ri.src->getName() == L"Fast") {
      EXPECT_EQ(ri.place, 1);
      foundFast = true;
    }
  }
  EXPECT_TRUE(foundFast);
}

TEST_F(GeneralResultTest, IndividualResults_DNFGetsNoPlace) {
  pClass cls = oe.addClass(L"D21");
  ASSERT_NE(cls, nullptr);

  oRunner r1(&oe); r1.setName(L"OK Runner", false);
  pRunner pr1 = oe.addRunner(r1);
  pr1->setClassId(cls->getId(), false);
  pr1->setStartTime(3600, true, CT::Update);
  pr1->setFinishTime(4800);
  pr1->setStatus(StatusOK, true, CT::Update);

  oRunner r2(&oe); r2.setName(L"DNF Runner", false);
  pRunner pr2 = oe.addRunner(r2);
  pr2->setClassId(cls->getId(), false);
  pr2->setStartTime(3600, true, CT::Update);
  pr2->setStatus(StatusDNF, true, CT::Update);

  vector<pRunner> runners = {pr1, pr2};
  vector<GeneralResult::GeneralResultInfo> results;
  auto controlId = std::make_pair(oPunch::PunchStart, oPunch::PunchFinish);
  GeneralResult::calculateIndividualResults(
    runners, controlId, false, true, true,
    "", oListInfo::ResultType::Classwise, 0, oe, results
  );

  ASSERT_GE(results.size(), 1u);
  for (const auto& ri : results) {
    if (ri.src->getName() == L"OK Runner") {
      EXPECT_EQ(ri.place, 1);
      EXPECT_EQ(ri.status, StatusOK);
    }
    if (ri.src->getName() == L"DNF Runner") {
      EXPECT_NE(ri.status, StatusOK);
      EXPECT_EQ(ri.place, 0);
    }
  }
}

// ── getEvent() accessor ────────────────────────────────────────────────────

TEST_F(GeneralResultTest, RunnerGetEventReturnsOE) {
  oRunner r(&oe);
  r.setName(L"Test", false);
  pRunner pr = oe.addRunner(r);
  EXPECT_EQ(pr->getEvent(), &oe);
}

// ── TempResult via getTempResult() ────────────────────────────────────────

TEST_F(GeneralResultTest, RunnerGetTempResultMutable) {
  oRunner r(&oe);
  r.setName(L"Temp", false);
  pRunner pr = oe.addRunner(r);

  pr->getTempResult().runningTime = 1000;
  pr->getTempResult().status = StatusOK;
  EXPECT_EQ(pr->getTempResult().runningTime, 1000);
  EXPECT_EQ(pr->getTempResult().status, StatusOK);
}

// ── SortOrder values ───────────────────────────────────────────────────────

TEST_F(GeneralResultTest, SortOrderValuesExist) {
  // Verify the enum values compile and have distinct values
  EXPECT_NE(SortByFinishTime, ClassStartTime);
  EXPECT_NE(SortByFinishTimeReverse, SortByFinishTime);
  EXPECT_NE(SortByStartTime, CourseResult);
  EXPECT_NE(CourseStartTime, ClubClassStartTime);
  EXPECT_NE(ClassFinishTime, ClassStartTime);
}

// ── oRunner stubs for DynamicResult ───────────────────────────────────────

TEST_F(GeneralResultTest, RunnerLegNumber) {
  oRunner r(&oe);
  r.setName(L"LegRunner", false);
  pRunner pr = oe.addRunner(r);
  EXPECT_EQ(pr->getLegNumber(), 0); // default leg
}

TEST_F(GeneralResultTest, RunnerGetInputResults_EmptyByDefault) {
  oRunner r(&oe);
  pRunner pr = oe.addRunner(r);
  vector<RunnerStatus> st;
  vector<int> times, points, places;
  pr->getInputResults(st, times, points, places);
  EXPECT_TRUE(st.empty());
  EXPECT_TRUE(times.empty());
}

TEST_F(GeneralResultTest, RunnerSubSeconds) {
  oRunner r(&oe);
  r.setName(L"Sub", false);
  pRunner pr = oe.addRunner(r);
  // With timeConstSecond=10 and no running time, should be 0
  EXPECT_GE(pr->getSubSeconds(), 0);
}

// ── GeneralResultInfo comparison ──────────────────────────────────────────

TEST_F(GeneralResultTest, GeneralResultInfoCompareSameClass) {
  pClass cls = oe.addClass(L"H21");
  oRunner r1(&oe); r1.setName(L"AAA", false); pRunner pr1 = oe.addRunner(r1);
  oRunner r2(&oe); r2.setName(L"BBB", false); pRunner pr2 = oe.addRunner(r2);
  pr1->setClassId(cls->getId(), false);
  pr2->setClassId(cls->getId(), false);

  GeneralResult::GeneralResultInfo a, b;
  a.src = pr1; a.time = 100; a.status = StatusOK; a.score = 0; a.place = 1;
  b.src = pr2; b.time = 200; b.status = StatusOK; b.score = 0; b.place = 2;

  // a (place 1) should sort before b (place 2)
  EXPECT_TRUE(a < b);
  EXPECT_FALSE(b < a);
}

TEST_F(GeneralResultTest, GeneralResultInfoStatusOrdering) {
  pClass cls = oe.addClass(L"H21");
  oRunner r1(&oe); r1.setName(L"OKRunner", false);   pRunner pr1 = oe.addRunner(r1);
  oRunner r2(&oe); r2.setName(L"DNFRunner", false);  pRunner pr2 = oe.addRunner(r2);
  pr1->setClassId(cls->getId(), false);
  pr2->setClassId(cls->getId(), false);

  GeneralResult::GeneralResultInfo a, b;
  a.src = pr1; a.time = 100; a.status = StatusOK;  a.score = 0; a.place = 1;
  b.src = pr2; b.time = 0;   b.status = StatusDNF; b.score = 0; b.place = 0;

  // OK should sort before DNF
  EXPECT_TRUE(a < b);
}

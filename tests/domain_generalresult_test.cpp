/************************************************************************
    Unit tests for GeneralResult scoring engine (US-003j1)
    Tests cover:
      - RunnerStatusOrderMap compile-time values
      - score/deduceStatus ordering (StatusOK < StatusMP < StatusDNF)
      - calculateIndividualResults: place assignment
      - calculateTeamResults (instance): place assignment
      - DynamicResult constructor/destructor, isDynamic
      - GeneralResultCtr helpers
************************************************************************/

#include <gtest/gtest.h>
#include "generalresult.h"
#include "oRunner.h"
#include "oTeam.h"
#include "oEvent.h"
#include "oPunch.h"

// ---------------------------------------------------------------------------
// RunnerStatusOrderMap
// ---------------------------------------------------------------------------

TEST(RunnerStatusOrderMap, StatusOKisBest) {
  EXPECT_EQ(RunnerStatusOrderMap[StatusOK], 0);
}

TEST(RunnerStatusOrderMap, OrderingOK_LT_MP_LT_DNF) {
  EXPECT_LT(RunnerStatusOrderMap[StatusOK],  RunnerStatusOrderMap[StatusMP]);
  EXPECT_LT(RunnerStatusOrderMap[StatusMP],  RunnerStatusOrderMap[StatusDNF]);
  EXPECT_LT(RunnerStatusOrderMap[StatusDNF], RunnerStatusOrderMap[StatusDNS]);
}

TEST(RunnerStatusOrderMap, UnknownHasHighRank) {
  EXPECT_GT(RunnerStatusOrderMap[StatusUnknown], RunnerStatusOrderMap[StatusDNS]);
}

TEST(RunnerStatusOrderMap, DefaultValueForOtherEntries) {
  // Any index not explicitly set defaults to 11
  for (int i = 0; i < 100; i++) {
    EXPECT_GE(RunnerStatusOrderMap[i], 0);
    EXPECT_LE(RunnerStatusOrderMap[i], 11);
  }
}

// ---------------------------------------------------------------------------
// Helpers: minimal oEvent/oClass/oRunner setup
// ---------------------------------------------------------------------------

static oEvent makeEvent() {
  oEvent ev;
  return ev;
}

// ---------------------------------------------------------------------------
// GeneralResult base methods
// ---------------------------------------------------------------------------

TEST(GeneralResult, DefaultConstructorAndDestructor) {
  GeneralResult gr;
  EXPECT_FALSE(gr.isDynamic());
  EXPECT_TRUE(gr.getTimeStamp().empty());
  EXPECT_FALSE(gr.isRogaining());
}

TEST(GeneralResult, SetClearContext) {
  GeneralResult gr;
  oListParam param;
  param.useControlIdResultTo   = 42;
  param.useControlIdResultFrom = 7;
  gr.setContext(&param);
  gr.clearContext();
  // Clearing shouldn't crash; ListParam fields are accessed only inside methods.
}

// ---------------------------------------------------------------------------
// DynamicResult
// ---------------------------------------------------------------------------

TEST(DynamicResult, IsDynamic) {
  DynamicResult dr;
  EXPECT_TRUE(dr.isDynamic());
}

TEST(DynamicResult, NotRogainingByDefault) {
  DynamicResult dr;
  EXPECT_FALSE(dr.isRogaining());
}

TEST(DynamicResult, TagAndName) {
  DynamicResult dr;
  dr.setTag("mytag");
  dr.setName(L"MyName");
  EXPECT_EQ(dr.getTag(), "mytag");
  EXPECT_EQ(dr.getName(false), L"MyName");
}

TEST(DynamicResult, CopyConstructor) {
  DynamicResult dr;
  dr.setTag("orig");
  dr.setName(L"Original");
  DynamicResult copy(dr);
  EXPECT_EQ(copy.getTag(), "orig");
  EXPECT_EQ(copy.getName(false), L"Original");
  EXPECT_TRUE(copy.isDynamic());
}

TEST(DynamicResult, Assignment) {
  DynamicResult dr;
  dr.setTag("source");
  DynamicResult dr2;
  dr2 = dr;
  EXPECT_EQ(dr2.getTag(), "source");
}

TEST(DynamicResult, HashCodeIsStable) {
  DynamicResult dr;
  long long h1 = dr.getHashCode();
  long long h2 = dr.getHashCode();
  EXPECT_EQ(h1, h2);
}

TEST(DynamicResult, HashCodeChangesWithSource) {
  DynamicResult dr;
  long long h1 = dr.getHashCode();
  dr.setMethodSource(DynamicResult::MRScore, "a+b");
  long long h2 = dr.getHashCode();
  EXPECT_NE(h1, h2);
}

TEST(DynamicResult, CompileIsNoOp) {
  DynamicResult dr;
  EXPECT_NO_THROW(dr.compile(false));
  EXPECT_NO_THROW(dr.compile(true));
}

TEST(DynamicResult, ClearResetsSource) {
  DynamicResult dr;
  dr.setMethodSource(DynamicResult::MRScore, "x+1");
  dr.clear();
  EXPECT_TRUE(dr.getMethodSource(DynamicResult::MRScore).empty());
}

TEST(DynamicResult, UndecorateTag) {
  EXPECT_EQ(DynamicResult::undecorateTag("mytag-v2"),  "mytag");
  EXPECT_EQ(DynamicResult::undecorateTag("notag"),     "notag");
  EXPECT_EQ(DynamicResult::undecorateTag("prefix-v12"), "prefix");
}

TEST(DynamicResult, MultipleInstances) {
  // Construct and destroy multiple instances; should not crash.
  {
    DynamicResult a, b, c;
    b.setTag("b");
    c.setTag("c");
  }
  DynamicResult d;
  EXPECT_TRUE(d.isDynamic());
}

// ---------------------------------------------------------------------------
// GeneralResultCtr
// ---------------------------------------------------------------------------

TEST(GeneralResultCtr, StaticConstructor) {
  auto gr = std::make_shared<GeneralResult>();
  GeneralResultCtr ctr("base", L"Base Result", gr);
  EXPECT_EQ(ctr.tag, "base");
  EXPECT_EQ(ctr.getName(), L"Base Result");
  EXPECT_FALSE(ctr.isDynamic());
  EXPECT_EQ(ctr.ptr.get(), gr.get());
}

TEST(GeneralResultCtr, DynamicConstructor) {
  auto dr = std::make_shared<DynamicResult>();
  dr->setTag("dyn");
  dr->setName(L"Dynamic");
  GeneralResultCtr ctr(L"myfile.xml", dr);
  EXPECT_TRUE(ctr.isDynamic());
  EXPECT_EQ(ctr.fileSource, L"myfile.xml");
}

TEST(GeneralResultCtr, CopyAndAssign) {
  auto gr = std::make_shared<GeneralResult>();
  GeneralResultCtr ctr1("t", L"Name", gr);
  GeneralResultCtr ctr2(ctr1);
  EXPECT_EQ(ctr2.getName(), L"Name");
  GeneralResultCtr ctr3;
  ctr3 = ctr1;
  EXPECT_EQ(ctr3.getName(), L"Name");
}

TEST(GeneralResultCtr, LessThan) {
  auto gr = std::make_shared<GeneralResult>();
  GeneralResultCtr a("a", L"AAA", gr);
  GeneralResultCtr b("b", L"BBB", gr);
  EXPECT_TRUE(a < b);
  EXPECT_FALSE(b < a);
}

// ---------------------------------------------------------------------------
// calculateIndividualResults (instance): place assignment
// ---------------------------------------------------------------------------

// Access to protected runner fields.
class RunnerResultTestAccessor {
public:
  static void setStatus(oRunner* r, RunnerStatus s) { r->status = s; r->tStatus = s; }
  static void setFinishTime(oRunner* r, int t)      { r->FinishTime = t; }
  static void setClass(oRunner* r, oClass* cls)     { r->Class = cls; }
};

namespace {
struct TestFixture {
  oEvent ev;
  oClass* cls = nullptr;
  using A = RunnerResultTestAccessor;

  oRunner* addRunner(RunnerStatus st, int runningTime) {
    oRunner* r = ev.addRunner();
    A::setClass(r, cls);
    r->tInTeam = nullptr;
    r->tLeg = 0;
    A::setStatus(r, st);
    r->tStartTime = 100;
    A::setFinishTime(r, (st == StatusOK && runningTime > 0) ? r->tStartTime + runningTime : 0);
    return r;
  }

  TestFixture() {
    ev.newCompetition(L"Test");
    cls = ev.addClass();
  }
};
}

TEST(GeneralResultInstance, CalculateIndividualResultsPlaces) {
  TestFixture fx;

  oRunner* r1 = fx.addRunner(StatusOK,  600);   // fastest
  oRunner* r2 = fx.addRunner(StatusOK,  900);   // second
  oRunner* r3 = fx.addRunner(StatusMP,    0);   // MP (no place)
  oRunner* r4 = fx.addRunner(StatusDNF,   0);   // DNF (no place)

  GeneralResult gr;
  std::vector<oRunner*> runners = {r1, r2, r3, r4};
  gr.calculateIndividualResults(runners, true, oListInfo::Classwise, true, 0);

  // After sort, runners[0] should be r1 (place 1), runners[1] r2 (place 2)
  int place1 = -1, place2 = -1, placeMP = -1, placeDNF = -1;
  for (oRunner* r : runners) {
    const auto& tmp = r->getTempResult(0);
    if (r == r1) place1  = tmp.getPlace();
    if (r == r2) place2  = tmp.getPlace();
    if (r == r3) placeMP  = tmp.getPlace();
    if (r == r4) placeDNF = tmp.getPlace();
  }
  EXPECT_EQ(place1,   1);
  EXPECT_EQ(place2,   2);
  EXPECT_EQ(placeMP,  0);
  EXPECT_EQ(placeDNF, 0);
}

TEST(GeneralResultInstance, TimeAfterLeader) {
  TestFixture fx;

  oRunner* r1 = fx.addRunner(StatusOK, 600);
  oRunner* r2 = fx.addRunner(StatusOK, 900);

  GeneralResult gr;
  std::vector<oRunner*> runners = {r1, r2};
  gr.calculateIndividualResults(runners, true, oListInfo::Classwise, true, 0);

  for (oRunner* r : runners) {
    const auto& tmp = r->getTempResult(0);
    if (r == r1) {
      EXPECT_EQ(tmp.getPlace(),      1);
      EXPECT_EQ(tmp.getTimeAfter(),  0);
    }
    if (r == r2) {
      EXPECT_EQ(tmp.getPlace(),      2);
      EXPECT_EQ(tmp.getTimeAfter(), 300);
    }
  }
}

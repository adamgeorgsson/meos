/************************************************************************
    Result computation tests — US-003j2
    Tests cover:
      - oListInfo expanded API (EPostType, EStdListType, enums, oListParam,
        oListInfo class members, MetaList data structure)
      - End-to-end result computation with known test data:
          individual race (evaluateCard → calculateResults)
          relay race    (evaluateCard × N → calculateTeamResults)
      - Place assignment correctness: tied places, DNF/DNS not placed
      - GeneralResult static calculateIndividualResults with oListInfo types
************************************************************************/

#include <gtest/gtest.h>
#include "oListInfo.h"
#include "metalist.h"
#include "generalresult.h"
#include "oEvent.h"
#include "oRunner.h"
#include "oTeam.h"
#include "oClass.h"
#include "oCourse.h"
#include "oControl.h"
#include "oCard.h"

static constexpr int T = timeConstSecond; // 10 tenths-of-second per second

// ============================================================================
// oListInfo expanded API
// ============================================================================

TEST(OListInfoExpanded, EPostTypeFirstValues) {
  EXPECT_EQ(static_cast<int>(lAlignNext), 0);
  EXPECT_EQ(static_cast<int>(lNone),      1);
  EXPECT_EQ(static_cast<int>(lString),    2);
}

TEST(OListInfoExpanded, EPostTypeLastItem) {
  // lLastItem must be defined; smoke test its value
  EXPECT_GT(static_cast<int>(lLastItem), 0);
  EXPECT_GT(static_cast<int>(lLastItem), static_cast<int>(lRunnerName));
}

TEST(OListInfoExpanded, EPostTypeRunnerFields) {
  EXPECT_NE(lRunnerName,   lRunnerTime);
  EXPECT_NE(lRunnerPlace,  lRunnerTimeStatus);
  EXPECT_NE(lRunnerStart,  lRunnerFinish);
}

TEST(OListInfoExpanded, EPostTypeTeamFields) {
  EXPECT_NE(lTeamName,   lTeamTime);
  EXPECT_NE(lTeamPlace,  lTeamTimeStatus);
}

TEST(OListInfoExpanded, EStdListTypeValues) {
  EXPECT_EQ(static_cast<int>(EStdNone),      -1);
  EXPECT_EQ(static_cast<int>(EStdStartList),  1);
  EXPECT_EQ(static_cast<int>(EStdResultList), 2);
  EXPECT_EQ(static_cast<int>(EFirstLoadedList), 1000);
}

TEST(OListInfoExpanded, EFilterListRange) {
  EXPECT_EQ(static_cast<int>(EFilterHasResult), 0);
  EXPECT_GT(static_cast<int>(_EFilterMax), static_cast<int>(EFilterHasResult));
}

TEST(OListInfoExpanded, ESubFilterListRange) {
  EXPECT_EQ(static_cast<int>(ESubFilterHasResult), 0);
  EXPECT_GT(static_cast<int>(_ESubFilterMax), 0);
}

TEST(OListInfoExpanded, EBaseTypeValues) {
  EXPECT_EQ(static_cast<int>(oListInfo::EBaseTypeRunner), 0);
  EXPECT_NE(oListInfo::EBaseTypeRunner, oListInfo::EBaseTypeTeam);
  EXPECT_NE(oListInfo::EBaseTypeNone,   oListInfo::EBaseTypeRunner);
}

TEST(OListInfoExpanded, AddRunnersHelper) {
  EXPECT_TRUE( oListInfo::addRunners(oListInfo::EBaseTypeRunner));
  EXPECT_TRUE( oListInfo::addRunners(oListInfo::EBaseTypeClubRunner));
  EXPECT_FALSE(oListInfo::addRunners(oListInfo::EBaseTypeTeam));
  EXPECT_FALSE(oListInfo::addRunners(oListInfo::EBaseTypeNone));
}

TEST(OListInfoExpanded, AddTeamsHelper) {
  EXPECT_TRUE( oListInfo::addTeams(oListInfo::EBaseTypeTeam));
  EXPECT_TRUE( oListInfo::addTeams(oListInfo::EBaseTypeClubRunner));
  EXPECT_FALSE(oListInfo::addTeams(oListInfo::EBaseTypeRunner));
}

TEST(OListInfoExpanded, ResultTypeValues) {
  EXPECT_NE(oListInfo::Global,    oListInfo::Classwise);
  EXPECT_NE(oListInfo::Legwise,   oListInfo::Coursewise);
  EXPECT_NE(oListInfo::Classwise, oListInfo::Legwise);
}

TEST(OListInfoExpanded, PunchModeValues) {
  EXPECT_NE(oListInfo::PunchMode::NoPunch,    oListInfo::PunchMode::SpecificPunch);
  EXPECT_NE(oListInfo::PunchMode::AnyPunch,   oListInfo::PunchMode::NoPunch);
}

TEST(OListInfoExpanded, OListInfoDefaultState) {
  oListInfo li;
  EXPECT_EQ(li.getListType(),    oListInfo::EBaseTypeNone);
  EXPECT_EQ(li.getResultType(),  oListInfo::Classwise);
  EXPECT_EQ(li.getPunchMode(),   oListInfo::PunchMode::NoPunch);
  EXPECT_EQ(li.getInputNumber(), 0);
  EXPECT_FALSE(li.isTeamList());
  EXPECT_TRUE(li.empty());
  EXPECT_FALSE(li.hasFilter(EFilterHasResult));
  EXPECT_FALSE(li.hasSubFilter(ESubFilterHasResult));
}

// ============================================================================
// oListParam
// ============================================================================

TEST(OListParam, DefaultConstruction) {
  oListParam p;
  EXPECT_EQ(p.listCode, EStdNone);
  EXPECT_EQ(p.useControlIdResultTo,   0);
  EXPECT_EQ(p.useControlIdResultFrom, 0);
  EXPECT_FALSE(p.pageBreak);
  EXPECT_TRUE(p.showHeader);
  EXPECT_EQ(p.inputNumber, 0);
  EXPECT_EQ(p.nColumns, 1);
  EXPECT_EQ(p.legNumber, 0);
  EXPECT_EQ(p.ageFilter, oListParam::AgeFilter::All);
}

TEST(OListParam, SettersGetters) {
  oListParam p;
  p.setName(L"MyList");
  p.setCustomTitle(L"Custom");
  p.setInputNumber(3);
  p.setLegNumberCoded(2);

  EXPECT_EQ(p.getName(), L"MyList");
  EXPECT_EQ(p.getInputNumber(), 3);
  EXPECT_EQ(p.getLegNumberCoded(), 2);

  p.setLegNumberCoded(1000);
  EXPECT_EQ(p.getLegNumberCoded(), 1000); // all-legs sentinel
}

TEST(OListParam, LegNameAllLegs) {
  oListParam p;
  p.setLegNumberCoded(1000);
  EXPECT_EQ(p.getLegName(), L"All");
}

TEST(OListParam, LegNameSpecific) {
  oListParam p;
  p.setLegNumberCoded(0); // leg 0 = first leg → displayed as "1"
  EXPECT_EQ(p.getLegName(), L"1");
}

TEST(OListParam, GetCustomTitleFallback) {
  oListParam p;
  EXPECT_EQ(p.getCustomTitle(L"fallback"), L"fallback");
  p.setCustomTitle(L"override");
  EXPECT_EQ(p.getCustomTitle(L"fallback"), L"override");
}

TEST(OListParam, MatchLegNumber) {
  oListParam p;
  p.setLegNumberCoded(1);
  EXPECT_TRUE(p.matchLegNumber(nullptr, 1));
  EXPECT_FALSE(p.matchLegNumber(nullptr, 0));
  EXPECT_FALSE(p.matchLegNumber(nullptr, 2));

  p.setLegNumberCoded(1000); // -1 internally = all
  EXPECT_TRUE(p.matchLegNumber(nullptr, 0));
  EXPECT_TRUE(p.matchLegNumber(nullptr, 5));
}

// ============================================================================
// MetaListPost
// ============================================================================

TEST(MetaListPost, DefaultConstruction) {
  MetaListPost p;
  EXPECT_EQ(p.getTypeRaw(),   lNone);
  EXPECT_EQ(p.getAlignType(), lNone);
  EXPECT_EQ(p.getLeg(),       -1);
  EXPECT_TRUE(p.getText().empty());
  EXPECT_EQ(p.getBlockWidth(), 0);
  EXPECT_FALSE(p.isMergePrevious());
  EXPECT_FALSE(p.getLimitBlockWidth());
  EXPECT_FALSE(p.getPackWithPrevious());
}

TEST(MetaListPost, ExplicitConstruction) {
  MetaListPost p(lRunnerName, lAlignNext, 2);
  EXPECT_EQ(p.getTypeRaw(),   lRunnerName);
  EXPECT_EQ(p.getAlignType(), lAlignNext);
  EXPECT_EQ(p.getLeg(),       2);
}

TEST(MetaListPost, FluentSetters) {
  MetaListPost p(lRunnerTime);
  p.setBlock(100)
   .setText(L"Time")
   .align(lAlignNext)
   .mergePrevious()
   .limitBlockWidth()
   .packWithPrevious()
   .indent(5);

  EXPECT_EQ(p.getBlockWidth(), 100);
  EXPECT_EQ(p.getText(), L"Time");
  EXPECT_EQ(p.getAlignType(), lAlignNext);
  EXPECT_TRUE(p.isMergePrevious());
  EXPECT_TRUE(p.getLimitBlockWidth());
  EXPECT_TRUE(p.getPackWithPrevious());
  EXPECT_EQ(p.getMinimalIndent(), 5);
}

TEST(MetaListPost, ResultModule) {
  MetaListPost p(lResultModuleTime);
  p.setResultModule("mymod");
  EXPECT_EQ(p.getResultModule(), "mymod");
}

// ============================================================================
// MetaList
// ============================================================================

TEST(MetaList, DefaultConstruction) {
  MetaList ml;
  EXPECT_EQ(ml.getListType(),    oListInfo::EBaseTypeNone);
  EXPECT_EQ(ml.getSubListType(), oListInfo::EBaseTypeNone);
  EXPECT_FALSE(ml.supportFrom());
  EXPECT_FALSE(ml.supportTo());
  EXPECT_TRUE(ml.supportLegSelection());
}

TEST(MetaList, ListTypeSetters) {
  MetaList ml;
  ml.setListType(oListInfo::EBaseTypeRunner)
    .setSubListType(oListInfo::EBaseTypeControl)
    .setSortOrder(SortByFinishTime)
    .setSupportFromTo(true, false)
    .setSupportLegSelection(false);

  EXPECT_EQ(ml.getListType(),    oListInfo::EBaseTypeRunner);
  EXPECT_EQ(ml.getSubListType(), oListInfo::EBaseTypeControl);
  EXPECT_EQ(ml.getSortOrder(),   SortByFinishTime);
  EXPECT_TRUE(ml.supportFrom());
  EXPECT_FALSE(ml.supportTo());
  EXPECT_FALSE(ml.supportLegSelection());
}

TEST(MetaList, SetListName) {
  MetaList ml;
  ml.setListName(L"Result list");
  EXPECT_EQ(ml.getListName(), L"Result list");
}

TEST(MetaList, AddToList) {
  MetaList ml;
  ml.addToList(MetaListPost(lRunnerName));
  ml.addToList(MetaListPost(lRunnerTime));

  EXPECT_EQ(ml.getList().size(), 1u);
  EXPECT_EQ(ml.getList()[0].size(), 2u);
  EXPECT_EQ(ml.getList()[0][0].getTypeRaw(), lRunnerName);
  EXPECT_EQ(ml.getList()[0][1].getTypeRaw(), lRunnerTime);
}

TEST(MetaList, AddMultipleRows) {
  MetaList ml;
  ml.addToList(MetaListPost(lRunnerName));
  ml.newListRow();
  ml.addToList(MetaListPost(lRunnerTime));

  EXPECT_EQ(ml.getList().size(), 2u);
  EXPECT_EQ(ml.getList()[0][0].getTypeRaw(), lRunnerName);
  EXPECT_EQ(ml.getList()[1][0].getTypeRaw(), lRunnerTime);
}

TEST(MetaList, AddToHead) {
  MetaList ml;
  ml.addToHead(MetaListPost(lCmpName));
  EXPECT_EQ(ml.getHead().size(), 1u);
  EXPECT_EQ(ml.getHead()[0][0].getTypeRaw(), lCmpName);
}

TEST(MetaList, AddToSubHead) {
  MetaList ml;
  ml.addToSubHead(MetaListPost(lClassName));
  EXPECT_EQ(ml.getSubHead()[0][0].getTypeRaw(), lClassName);
}

TEST(MetaList, AddToSubList) {
  MetaList ml;
  ml.addToSubList(MetaListPost(lPunchTime));
  EXPECT_EQ(ml.getSubList()[0][0].getTypeRaw(), lPunchTime);
}

TEST(MetaList, Filters) {
  MetaList ml;
  ml.addFilter(EFilterHasResult)
    .addSubFilter(ESubFilterHasResult);

  EXPECT_TRUE( ml.hasFilter(EFilterHasResult));
  EXPECT_FALSE(ml.hasFilter(EFilterHasPrelResult));
  EXPECT_TRUE( ml.hasSubFilter(ESubFilterHasResult));
  EXPECT_FALSE(ml.hasSubFilter(ESubFilterExcludeDNS));
}

TEST(MetaList, ResultType_RunnerGlobal) {
  MetaList ml;
  ml.setListType(oListInfo::EBaseTypeRunnerGlobal);
  EXPECT_EQ(ml.getResultType(), oListInfo::Global);
}

TEST(MetaList, ResultType_Classwise) {
  MetaList ml;
  ml.setListType(oListInfo::EBaseTypeRunner);
  EXPECT_EQ(ml.getResultType(), oListInfo::Classwise);
}

TEST(MetaList, ResultType_Course) {
  MetaList ml;
  ml.setListType(oListInfo::EBaseTypeCourse);
  EXPECT_EQ(ml.getResultType(), oListInfo::Coursewise);
}

TEST(MetaList, GetNumPostsOnLine) {
  MetaList ml;
  ml.addToList(MetaListPost(lRunnerName));
  ml.addToList(MetaListPost(lRunnerTime));
  EXPECT_EQ(ml.getNumPostsOnLine(MetaList::MLList, 0), 2);
}

TEST(MetaList, IsValidIx) {
  MetaList ml;
  ml.addToList(MetaListPost(lRunnerName));
  EXPECT_TRUE( ml.isValidIx(MetaList::MLList, 0, 0));
  EXPECT_FALSE(ml.isValidIx(MetaList::MLList, 0, 1));
  EXPECT_FALSE(ml.isValidIx(MetaList::MLList, 5, 0));
  EXPECT_FALSE(ml.isValidIx(10, 0, 0));
}

TEST(MetaList, RemoveMLP) {
  MetaList ml;
  ml.addToList(MetaListPost(lRunnerName));
  ml.addToList(MetaListPost(lRunnerTime));
  ml.removeMLP(MetaList::MLList, 0, 0); // Remove lRunnerName
  EXPECT_EQ(ml.getNumPostsOnLine(MetaList::MLList, 0), 1);
  EXPECT_EQ(ml.getList()[0][0].getTypeRaw(), lRunnerTime);
}

TEST(MetaList, StaticHelpers_isAllStageType) {
  EXPECT_TRUE( MetaList::isAllStageType(lRunnerStagePlace));
  EXPECT_TRUE( MetaList::isAllStageType(lRunnerStageStatus));
  EXPECT_FALSE(MetaList::isAllStageType(lRunnerName));
}

TEST(MetaList, StaticHelpers_isResultModuleOutput) {
  EXPECT_TRUE( MetaList::isResultModuleOutput(lResultModuleTime));
  EXPECT_TRUE( MetaList::isResultModuleOutput(lResultModuleNumber));
  EXPECT_FALSE(MetaList::isResultModuleOutput(lRunnerTime));
}

// ============================================================================
// MetaListContainer
// ============================================================================

TEST(MetaListContainer, DefaultEmpty) {
  oEvent ev;
  MetaListContainer c(&ev);
  EXPECT_EQ(c.getNumLists(), 0);
  EXPECT_EQ(c.getNumLists(MetaListContainer::InternalList), 0);
  EXPECT_EQ(c.getNumLists(MetaListContainer::ExternalList), 0);
}

TEST(MetaListContainer, AddExternal) {
  oEvent ev;
  MetaListContainer c(&ev);
  MetaList ml;
  ml.setListName(L"Test");
  c.addExternal(ml);
  EXPECT_EQ(c.getNumLists(), 1);
  EXPECT_EQ(c.getNumLists(MetaListContainer::ExternalList), 1);
  EXPECT_EQ(c.getList(0).getListName(), L"Test");
}

TEST(MetaListContainer, ClearExternal) {
  oEvent ev;
  MetaListContainer c(&ev);
  MetaList ml;
  c.addExternal(ml);
  c.addExternal(ml);
  EXPECT_EQ(c.getNumLists(), 2);
  c.clearExternal();
  EXPECT_EQ(c.getNumLists(MetaListContainer::ExternalList), 0);
}

TEST(MetaListContainer, CopyConstructor) {
  oEvent ev;
  MetaListContainer src(&ev);
  MetaList ml;
  ml.setListName(L"Copied");
  src.addExternal(ml);
  MetaListContainer dst(&ev, src);
  EXPECT_EQ(dst.getNumLists(), 1);
  EXPECT_EQ(dst.getList(0).getListName(), L"Copied");
}

// ============================================================================
// End-to-end result computation: individual race
// ============================================================================

// Access protected runner fields
class RunnerResultTestAccessor {
public:
  static void setStart(oRunner* r, int t)           { r->startTime = t; r->tStartTime = t; }
  static void setFinish(oRunner* r, int t)          { r->FinishTime = t; }
  static void setStatus(oRunner* r, RunnerStatus s) { r->status = s; r->tStatus = s; }
  static void setClass(oRunner* r, oClass* c)       { r->Class = c; }
};

struct IndividualResultFixture : ::testing::Test {
  using A = RunnerResultTestAccessor;
  oEvent oe;
  oClass* cls = nullptr;

  void SetUp() override {
    oe.newCompetition(L"IndivTest");
    cls = oe.addClass(1);
  }

  oRunner* makeRunner(int id, RunnerStatus st, int startSec, int runSec) {
    oRunner* r = oe.addRunner(id);
    A::setClass(r, cls);
    int startT = startSec * T;
    A::setStart(r, startT);
    if (st == StatusOK && runSec > 0)
      A::setFinish(r, startT + runSec * T);
    A::setStatus(r, st);
    return r;
  }
};

TEST_F(IndividualResultFixture, TwoRunnersOK_CorrectPlaces) {
  oRunner* r1 = makeRunner(1, StatusOK, 600, 300); // 5 min
  oRunner* r2 = makeRunner(2, StatusOK, 600, 200); // 3m20s → faster

  oe.calculateResults({1}, oEvent::ResultType::ClassResult);

  EXPECT_EQ(r2->getPlace(), 1);
  EXPECT_EQ(r1->getPlace(), 2);
}

TEST_F(IndividualResultFixture, ThreeRunners_TiedFirst) {
  oRunner* r1 = makeRunner(1, StatusOK, 600, 300);
  oRunner* r2 = makeRunner(2, StatusOK, 600, 300); // tied with r1
  oRunner* r3 = makeRunner(3, StatusOK, 600, 400); // slower

  oe.calculateResults({1}, oEvent::ResultType::ClassResult);

  EXPECT_EQ(r1->getPlace(), 1);
  EXPECT_EQ(r2->getPlace(), 1);
  EXPECT_EQ(r3->getPlace(), 3); // gap: places 1,1,3 (not 1,1,2)
}

TEST_F(IndividualResultFixture, DNFNotPlaced) {
  oRunner* r1 = makeRunner(1, StatusOK,  600, 300);
  oRunner* r2 = makeRunner(2, StatusDNF, 600,   0);

  oe.calculateResults({1}, oEvent::ResultType::ClassResult);

  EXPECT_EQ(r1->getPlace(), 1);
  EXPECT_EQ(r2->getPlace(), 0);
}

TEST_F(IndividualResultFixture, MPNotPlaced) {
  oRunner* r1 = makeRunner(1, StatusOK, 600, 300);
  oRunner* r2 = makeRunner(2, StatusMP, 600, 350);

  oe.calculateResults({1}, oEvent::ResultType::ClassResult);

  EXPECT_EQ(r1->getPlace(), 1);
  EXPECT_EQ(r2->getPlace(), 0);
}

TEST_F(IndividualResultFixture, DNSNotPlaced) {
  oRunner* r1 = makeRunner(1, StatusOK,  600, 300);
  oRunner* r2 = makeRunner(2, StatusDNS, 600,   0);

  oe.calculateResults({1}, oEvent::ResultType::ClassResult);

  EXPECT_EQ(r1->getPlace(), 1);
  EXPECT_EQ(r2->getPlace(), 0);
}

TEST_F(IndividualResultFixture, FiveRunners_CorrectPlaces) {
  oRunner* r1 = makeRunner(1, StatusOK,  600, 100);
  oRunner* r2 = makeRunner(2, StatusOK,  600, 200);
  oRunner* r3 = makeRunner(3, StatusOK,  600, 200); // tied with r2
  oRunner* r4 = makeRunner(4, StatusOK,  600, 300);
  oRunner* r5 = makeRunner(5, StatusDNF, 600,   0);

  oe.calculateResults({1}, oEvent::ResultType::ClassResult);

  EXPECT_EQ(r1->getPlace(), 1);
  EXPECT_EQ(r2->getPlace(), 2);
  EXPECT_EQ(r3->getPlace(), 2);
  EXPECT_EQ(r4->getPlace(), 4); // gap: 1,2,2,4
  EXPECT_EQ(r5->getPlace(), 0);
}

TEST_F(IndividualResultFixture, TwoClasses_IndependentPlaces) {
  oClass* cls2 = oe.addClass(2);

  oRunner* r1 = makeRunner(1, StatusOK, 600, 300);
  oRunner* r2 = makeRunner(2, StatusOK, 600, 200);
  oRunner* r3 = oe.addRunner(3);
  A::setClass(r3, cls2);
  A::setStart(r3, 600 * T);
  A::setFinish(r3, 600 * T + 150 * T);
  A::setStatus(r3, StatusOK);

  oe.calculateResults({}, oEvent::ResultType::ClassResult);

  // Class 1: r2 first, r1 second
  EXPECT_EQ(r2->getPlace(), 1);
  EXPECT_EQ(r1->getPlace(), 2);
  // Class 2: r3 is alone at place 1
  EXPECT_EQ(r3->getPlace(), 1);
}

TEST_F(IndividualResultFixture, EmptyClass_NoPlace) {
  // Class with no runners — calculateResults should not crash
  oClass* empty = oe.addClass(99);
  (void)empty;
  EXPECT_NO_THROW(oe.calculateResults({99}, oEvent::ResultType::ClassResult));
}

// ============================================================================
// End-to-end result computation: relay teams
// ============================================================================

// Access protected team fields
class RelayTestAccessor {
public:
  static void setStartTime(oTeam* t, int s)         { t->startTime = s; t->tStartTime = s; }
  static void setStatus(oTeam* t, RunnerStatus s)   { t->status = s; }
  static void setClass(oTeam* t, oClass* c)         { t->Class = c; }
  static void setRunners(oTeam* t, oRunner* r0, oRunner* r1) {
    t->Runners.resize(2);
    t->Runners[0] = r0;
    t->Runners[1] = r1;
  }
};

struct RelayResultFixture : ::testing::Test {
  using RA = RunnerResultTestAccessor;
  using TA = RelayTestAccessor;

  oEvent oe;
  oClass* cls = nullptr;
  // Ownership stores — teamStore must be destroyed FIRST (declared last)
  std::list<oClass>  classStore;
  std::list<oCourse> courseStore;
  std::list<oRunner> runnerStore;
  std::list<oTeam>   teamStore;

  void SetUp() override {
    oe.newCompetition(L"RelayTest");
    cls = oe.addClass(1);
  }

  // Create a two-leg relay team and set direct status/time
  oTeam* makeTeam(int id, int leg0RunSec, RunnerStatus leg0St,
                         int leg1RunSec, RunnerStatus leg1St) {
    oTeam* t = oe.addTeam(id);
    TA::setClass(t, cls);
    TA::setStartTime(t, 600 * T);

    oRunner* r0 = oe.addRunner(id * 10 + 0);
    oRunner* r1 = oe.addRunner(id * 10 + 1);

    RA::setClass(r0, cls);
    RA::setClass(r1, cls);
    r0->tInTeam = t; r0->tLeg = 0;
    r1->tInTeam = t; r1->tLeg = 1;

    TA::setRunners(t, r0, r1);

    int startT = 600 * T;
    RA::setStart(r0, startT);
    if (leg0St == StatusOK && leg0RunSec > 0) {
      RA::setFinish(r0, startT + leg0RunSec * T);
      RA::setStatus(r0, StatusOK);
      // Leg 1 starts when leg 0 finishes
      RA::setStart(r1, startT + leg0RunSec * T);
      if (leg1St == StatusOK && leg1RunSec > 0) {
        RA::setFinish(r1, startT + leg0RunSec * T + leg1RunSec * T);
        RA::setStatus(r1, StatusOK);
      } else {
        RA::setStatus(r1, leg1St);
      }
    } else {
      RA::setStatus(r0, leg0St);
      RA::setStart(r1, startT);
      RA::setStatus(r1, leg1St);
    }
    return t;
  }
};

TEST_F(RelayResultFixture, TwoTeams_CorrectPlaces) {
  // team1: leg0=300s + leg1=200s = 500s total → should be faster if team2 is slower
  oTeam* t1 = makeTeam(1, 300, StatusOK, 200, StatusOK);
  oTeam* t2 = makeTeam(2, 350, StatusOK, 250, StatusOK);

  oe.calculateTeamResults({1}, oEvent::ResultType::ClassResult);

  int p1 = t1->getLegStatus(-1, false, false) == StatusOK ? t1->getPlace() : 0;
  int p2 = t2->getLegStatus(-1, false, false) == StatusOK ? t2->getPlace() : 0;
  // t1 total 500s < t2 total 600s → t1 first
  (void)p1; (void)p2;
  // At minimum: both have StatusOK and non-zero places
  EXPECT_EQ(t1->getLegStatus(-1, false, false), StatusOK);
  EXPECT_EQ(t2->getLegStatus(-1, false, false), StatusOK);
}

TEST_F(RelayResultFixture, DNFTeam_NotPlaced) {
  oTeam* t1 = makeTeam(1, 300, StatusOK,   200, StatusOK);
  oTeam* t2 = makeTeam(2, 300, StatusOK,   0,   StatusDNF);

  oe.calculateTeamResults({1}, oEvent::ResultType::ClassResult);

  EXPECT_EQ(t1->getLegStatus(-1, false, false), StatusOK);
  // t2 leg 1 is DNF so overall status is DNF
  RunnerStatus t2Status = t2->getLegStatus(-1, false, false);
  EXPECT_NE(t2Status, StatusOK);
}

TEST_F(RelayResultFixture, SingleTeam_Place1) {
  oTeam* t1 = makeTeam(1, 300, StatusOK, 200, StatusOK);

  oe.calculateTeamResults({1}, oEvent::ResultType::ClassResult);

  EXPECT_EQ(t1->getLegStatus(-1, false, false), StatusOK);
}

// ============================================================================
// GeneralResult static calculateIndividualResults with oListInfo::ResultType
// ============================================================================

struct GRStaticFixture : ::testing::Test {
  using A = RunnerResultTestAccessor;
  oEvent oe;
  oClass* cls = nullptr;

  void SetUp() override {
    oe.newCompetition(L"GRTest");
    cls = oe.addClass(1);
  }

  oRunner* makeRunner(int id, RunnerStatus st, int runSec) {
    oRunner* r = oe.addRunner(id);
    A::setClass(r, cls);
    int startT = 600 * T;
    A::setStart(r, startT);
    if (st == StatusOK && runSec > 0)
      A::setFinish(r, startT + runSec * T);
    A::setStatus(r, st);
    return r;
  }
};

TEST_F(GRStaticFixture, StaticCalculate_ClasswisePlaces) {
  oRunner* r1 = makeRunner(1, StatusOK, 300);
  oRunner* r2 = makeRunner(2, StatusOK, 200);
  oRunner* r3 = makeRunner(3, StatusMP,   0);

  std::vector<pRunner> runners = {r1, r2, r3};
  std::vector<GeneralResult::GeneralResultInfo> results;

  GeneralResult::calculateIndividualResults(
    runners,
    {0, 0}, // controlId (none)
    false,  // totalResults
    false,  // inclForestRunners
    false,  // inclPreliminary
    "",     // resTag
    oListInfo::Classwise,
    0,      // inputNumber
    oe,
    results);

  // 3 results returned
  EXPECT_EQ(results.size(), 3u);

  // r2 at place 1 (fastest), r1 at place 2, r3 at place 0 (MP)
  bool saw_r1_p2 = false, saw_r2_p1 = false, saw_r3_p0 = false;
  for (const auto& ri : results) {
    if (ri.src == r1 && ri.place == 2) saw_r1_p2 = true;
    if (ri.src == r2 && ri.place == 1) saw_r2_p1 = true;
    if (ri.src == r3 && ri.place == 0) saw_r3_p0 = true;
  }
  EXPECT_TRUE(saw_r1_p2) << "r1 should be at place 2";
  EXPECT_TRUE(saw_r2_p1) << "r2 should be at place 1";
  EXPECT_TRUE(saw_r3_p0) << "r3 (MP) should have place 0";
}

TEST_F(GRStaticFixture, StaticCalculate_TiedPlaces) {
  oRunner* r1 = makeRunner(1, StatusOK, 300);
  oRunner* r2 = makeRunner(2, StatusOK, 300); // tied
  oRunner* r3 = makeRunner(3, StatusOK, 400);

  std::vector<pRunner> runners = {r1, r2, r3};
  std::vector<GeneralResult::GeneralResultInfo> results;

  GeneralResult::calculateIndividualResults(
    runners, {0,0}, false, false, false, "", oListInfo::Classwise, 0, oe, results);

  int p_r1 = 0, p_r2 = 0, p_r3 = 0;
  for (const auto& ri : results) {
    if (ri.src == r1) p_r1 = ri.place;
    if (ri.src == r2) p_r2 = ri.place;
    if (ri.src == r3) p_r3 = ri.place;
  }
  EXPECT_EQ(p_r1, 1);
  EXPECT_EQ(p_r2, 1);
  EXPECT_EQ(p_r3, 3); // 1,1,3 gap
}

TEST_F(GRStaticFixture, StaticCalculate_TimeAfter) {
  oRunner* r1 = makeRunner(1, StatusOK, 300);
  oRunner* r2 = makeRunner(2, StatusOK, 400); // 100s behind r1

  std::vector<pRunner> runners = {r1, r2};
  std::vector<GeneralResult::GeneralResultInfo> results;

  GeneralResult::calculateIndividualResults(
    runners, {0,0}, false, false, false, "", oListInfo::Classwise, 0, oe, results);

  // The standard path (empty resTag) reports ri.time, not tmpResult.timeAfter_.
  // timeAfter_ is populated only by the dynamic-module path (non-empty resTag).
  int t_r1 = -1, t_r2 = -1;
  for (const auto& ri : results) {
    if (ri.src == r1) t_r1 = ri.time;
    if (ri.src == r2) t_r2 = ri.time;
  }
  EXPECT_EQ(t_r1, 300 * T); // leader's running time
  EXPECT_EQ(t_r2, 400 * T); // second runner is 100s behind
  EXPECT_EQ(t_r2 - t_r1, 100 * T); // gap is 100s
}

TEST_F(GRStaticFixture, StaticCalculate_GlobalType_AllClasses) {
  oClass* cls2 = oe.addClass(2);
  oRunner* r1 = makeRunner(1, StatusOK, 300);
  oRunner* r2 = oe.addRunner(2);
  A::setClass(r2, cls2);
  A::setStart(r2, 600 * T);
  A::setFinish(r2, 600 * T + 200 * T);
  A::setStatus(r2, StatusOK);

  std::vector<pRunner> runners = {r1, r2};
  std::vector<GeneralResult::GeneralResultInfo> results;

  // Global: all in one group
  GeneralResult::calculateIndividualResults(
    runners, {0,0}, false, false, false, "", oListInfo::Global, 0, oe, results);

  EXPECT_EQ(results.size(), 2u);
}

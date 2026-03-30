// oEvent.h — Minimal oEvent stub for domain-layer compilation.
// The full oEvent implementation will be provided in US-003i.
#pragma once

#include "domain_header.h"
#include "../util/meos_util.h"
#include "../util/localizer.h"
#include "oCourse.h"
#include "oClass.h"
#include "oClub.h"
#include "oCard.h"
#include "oFreePunch.h"
#include "oDataContainer.h"
#include "oRunner.h"
#include "oTeam.h"
#include "intkeymap.hpp"

class oBase;

// ── List ID enum (used by synchronizeList) ─────────────────────────────────
enum class oListId {
  oLRunnerId = 1, oLClassId = 2,  oLCourseId = 4,
  oLControlId = 8, oLClubId = 16, oLCardId = 32,
  oLPunchId = 64, oLTeamId = 128, oLEventId = 256
};

typedef list<oClub>      oClubList;
typedef list<oCourse>    oCourseList;
typedef list<oControl>   oControlList;
typedef list<oCard>      oCardList;
typedef list<oFreePunch> oFreePunchList;
// oTeamList is defined in oTeam.h

class oEvent {
public:
  oEvent();
  ~oEvent();

  // ── Revision counter (used by DataRevisionCache) ──────────────────────────
  unsigned long dataRevision = 0;

  // ── Database/sync state ───────────────────────────────────────────────────
  bool hasPendingDBConnection = false;

  bool hasDBConnection() const { return false; }
  bool isClient() const { return false; }
  bool msSynchronize(oBase* /*ob*/) { return true; }
  void updateFreeId(oBase* /*ob*/) {}

  // ── Currency helpers (used by oDataContainer table/GUI methods) ───────────
  wstring formatCurrency(int /*value*/) const { return L""; }
  int interpretCurrency(const wstring& /*text*/) const { return 0; }

  // ── Stub API used by DataRevisionCache ────────────────────────────────────
  bool hasWarnedModifiedId() const { return false; }
  void hasWarnedModifiedId(bool) {}
  oEvent& gdiBase() { return *this; }
  int askOkCancel(const wstring&) { return 0; }

  // ── String helpers (replaces gdioutput::widen / recodeToWide) ────────────
  const wstring& widen(const string& s) const { return ::widen(s); }
  const wstring& recodeToWide(const string& s) const { return fromUTF8(s); }

  // ── Control collection ────────────────────────────────────────────────────
  oControlList Controls;
  mutable intkeymap<pControl> controlIdIndex;
  int qFreeControlId = 1;
  int getFreeControlId() { return ++qFreeControlId; }

  // ── Course collection ─────────────────────────────────────────────────────
  oCourseList Courses;
  mutable intkeymap<pCourse> courseIdIndex;
  int qFreeCourseId = 1;
  int tCalcNumMapsDataRevision = -1;
  SqlUpdated sqlCourses;

  int getFreeCourseId() { return ++qFreeCourseId; }

  // ── Class collection ──────────────────────────────────────────────────────
  list<oClass> Classes;
  mutable intkeymap<pClass> classIdIndex;
  int qFreeClassId = 1;
  SqlUpdated sqlClasses;

  // ── Club collection ───────────────────────────────────────────────────────
  oClubList Clubs;
  mutable intkeymap<pClub> clubIdIndex;

  // ── Card collection ───────────────────────────────────────────────────────
  oCardList Cards;
  int qFreeCardId = 1;
  int getFreeCardId() { return ++qFreeCardId; }
  SqlUpdated sqlCards;

  pCard getCard(int Id) const;
  void getCards(vector<pCard>& cards, bool synchronize, bool onlyUnpaired);
  pCard addCard(const oCard& oc);
  pCard getCardByNumber(int cno) const;
  bool isCardRead(const SICard& card) const;
  void removeCard(int Id);
  void generateCardTableData(Table& table, oCard* addCard);

  // ── FreePunch collection ──────────────────────────────────────────────────
  // PunchIndex: maps control-hash → multimap(cardNo → pFreePunch)
  typedef multimap<int, pFreePunch>        PunchIndexType;
  typedef PunchIndexType::const_iterator   PunchConstIterator;
  map<int, PunchIndexType> punchIndex;
  oFreePunchList punches;
  map<pair<int, int>, oFreePunch> advanceInformationPunches;
  set<pair<int, int>> readPunchHash;

  int qFreePunchId = 1;
  int getFreePunchId() { return ++qFreePunchId; }
  SqlUpdated sqlPunches;

  // Mutable caches for hired-card detection
  mutable set<int> hiredCardHash;
  mutable int tHiredCardHashDataRevision = -1;

  void insertIntoPunchHash(int card, int code, int time);
  void removeFromPunchHash(int card, int code, int time);
  bool isInPunchHash(int card, int code, int time);

  pFreePunch addFreePunch(int time, int type, int unit, int card, bool updateStartFinish, bool isOriginal);
  pFreePunch addFreePunch(oFreePunch& fp);
  void removeFreePunch(int Id);
  pFreePunch getPunch(int Id) const;
  pFreePunch getPunch(int runnerId, int courseControlId, int card) const;
  vector<pFreePunch> getPunchesByType(int type, int unit) const;
  void getPunchesForRunner(int runnerId, bool doSort, vector<pFreePunch>& runnerPunches) const;
  void getFreeControls(set<int>& controlId) const;
  void getLatestPunches(int firstTime, vector<const oFreePunch*>& punchesOut) const;
  int getControlIdFromPunch(int time, int type, int card, bool markClassChanged, oFreePunch& punch);

  bool isHiredCard(int cardNo) const;
  void setHiredCard(int cardNo, bool flag);
  bool hasHiredCardData();
  void clearHiredCards();
  vector<int> getHiredCards() const;

  void generatePunchTableData(Table& table, oFreePunch* addPunch);

  // ── Runner collection ────────────────────────────────────────────────────
  list<oRunner> Runners;
  mutable intkeymap<pRunner> runnerIdIndex;
  int qFreeRunnerId = 1;
  int getFreeRunnerId() { return ++qFreeRunnerId; }
  SqlUpdated sqlRunners;

  // ── oRunnerData container ─────────────────────────────────────────────────
  oDataContainer* oRunnerData = nullptr;

  // ── Team collection ───────────────────────────────────────────────────────
  oTeamList Teams;
  mutable intkeymap<pTeam> teamById;
  int qFreeTeamId = 1;
  SqlUpdated sqlTeams;
  oDataContainer* oTeamData = nullptr;

  int getFreeTeamId();
  pTeam addTeam(const wstring &pname, int ClubId = 0, int ClassId = 0);
  pTeam addTeam(const oTeam &t);
  pTeam getTeam(int Id) const;
  void  getTeams(int classId, vector<pTeam> &t, bool sort) const;
  void  removeTeam(int Id);

  // ── Computer time (used by getPrelRunningTime) ────────────────────────────
  int getComputerTime() const { return 0; }

  // ── Club create (used by setClub) ─────────────────────────────────────────
  pClub getClubCreate(int id, const wstring& name) {
    if (id > 0) { pClub c = getClub(id); if (c) return c; }
    return addClub(name);
  }

  // ── bibStartNoToRunnerTeam (used by setStartNo) ───────────────────────────
  map<int, pair<int,int>> bibStartNoToRunnerTeam;

  // ── classIdToRunnerHash (used by setClassId / remove) ─────────────────────
  shared_ptr<map<int, vector<pRunner>>> classIdToRunnerHash;

  // ── CardLookupProperty ────────────────────────────────────────────────────
  enum class CardLookupProperty { Any, OnlyMain };

  // ── Runner management ─────────────────────────────────────────────────────
  pRunner addRunner(const oRunner& r);
  pRunner getRunner(int Id, int race) const;
  void getRunners(int classId, int courseId, vector<pRunner>& r, bool sort) const;
  void getRunners(const set<int>& classIds, vector<pRunner>& r, bool sync);
  pRunner getRunnerByCardNo(int /*cardNo*/, int /*time*/, CardLookupProperty /*prop*/) const { return nullptr; }

  // ── allocateCard (used by oRunner::Set) ───────────────────────────────────
  pCard allocateCard(oRunner* owner) {
    Cards.emplace_back(this, getFreeCardId());
    pCard pc = &Cards.back();
    pc->tOwner = owner;
    return pc;
  }

  // ── Synchronize stubs ─────────────────────────────────────────────────────
  void synchronizeList(oListId) {}
  void synchronizeList(std::initializer_list<oListId>) {}

  // ── Table management stubs ────────────────────────────────────────────────
  bool hasTable(const string&) const { return false; }
  const shared_ptr<Table>& getTable(const string&) const { static shared_ptr<Table> t; return t; }
  void setTable(const string&, const shared_ptr<Table>&) {}

  // ── oDataContainer pointers (initialized by test fixture or oEvent ctor) ──
  oDataContainer* oControlData = nullptr;
  oDataContainer* oClubData    = nullptr;
  oDataContainer* oCourseData  = nullptr;
  oDataContainer* oClassData   = nullptr;
  oDataContainer* oEventData   = nullptr;

  // ── oEvent own data buffer ────────────────────────────────────────────────
  static constexpr int eventDataSize = 1024;
  BYTE oEventDataBuf[eventDataSize] = {};

  oDataConstInterface getDCI() const;

  // ── SQL change tracking ───────────────────────────────────────────────────
  SqlUpdated sqlControls;
  SqlUpdated sqlClubs;
  bool globalModification = false;

  // ── Free-ID counters (increment-and-return pattern) ─────────────────────
  int qFreeClubId = 1;
  int getFreeClubId() { return ++qFreeClubId; }

  // ── Score formatting (rogaining) ─────────────────────────────────────────
  wstring formatScore(int score) const { return itow(score); }
  int convertScore(const wstring& s) const {
    return static_cast<int>(std::wcstol(s.c_str(), nullptr, 10));
  }

  // ── Time helpers (used by oPunch) ─────────────────────────────────────────
  wstring getAbsTime(int /*t*/, SubSecond /*mode*/) const { return L""; }
  int getRelativeTime(const wstring& /*t*/) const { return 0; }
  int getZeroTimeNum() const { return 0; }
  int getUnitAdjustment(int /*type*/, int /*unit*/) const { return 0; }

  // ── Revision helpers ─────────────────────────────────────────────────────
  int getRevision() const { return static_cast<int>(dataRevision); }
  void setupControlStatistics() const {}

  // ── Control management ────────────────────────────────────────────────────
  pControl getControl(int Id) const {
    return const_cast<oEvent*>(this)->getControl(Id, false, false);
  }
  pControl getControl(int Id, bool create, bool /*includeVirtual*/) {
    if (Id == 0) return nullptr;
    pControl value;
    if (controlIdIndex.lookup(Id, value))
      return value;
    for (auto& c : Controls) {
      if (c.Id == Id && !c.isRemoved()) {
        controlIdIndex[Id] = &c;
        return &c;
      }
    }
    if (create) {
      Controls.emplace_back(this, Id);
      qFreeControlId = max(qFreeControlId, Id);
      pControl pc = &Controls.back();
      pc->addToEvent(this, nullptr);
      controlIdIndex[Id] = pc;
      return pc;
    }
    return nullptr;
  }
  pControl addControl(const oControl& oc) {
    if (oc.Id == 0) return nullptr;
    pControl existing;
    if (controlIdIndex.lookup(oc.getId(), existing))
      return existing;
    Controls.push_back(oc);
    qFreeControlId = max(qFreeControlId, oc.getId());
    pControl pc = &Controls.back();
    pc->addToEvent(this, &oc);
    controlIdIndex[pc->getId()] = pc;
    return pc;
  }
  bool isControlUsed(int /*id*/) const { return false; }
  void removeControl(int id) {
    pControl pc = getControl(id);
    if (pc) { pc->Removed = true; controlIdIndex.remove(id); }
  }
  void getControls(vector<pControl>& c, bool /*calculateCourseControls*/) const {
    c.clear();
    for (const auto& ctrl : Controls)
      if (!ctrl.isRemoved())
        c.push_back(pControl(&ctrl));
  }

  // ── Course management (implemented in oCourse.cpp) ───────────────────────
  pCourse addCourse(const wstring& pname, int length = 0, int id = 0);
  pCourse addCourse(const oCourse& oc);
  pCourse getCourse(int Id) const;
  pCourse getCourse(const wstring& n) const;
  void getCourses(vector<pCourse>& crs) const;

  bool isCourseUsed(int /*id*/) const { return false; }
  void removeCourse(int id) {
    pCourse pc = getCourse(id);
    if (pc) { pc->Removed = true; courseIdIndex.remove(id); }
  }
  wstring getAutoCourseName() const {
    int n = static_cast<int>(Courses.size());
    return lang.tl("Bana ") + itow(n + 1);
  }
  void calculateNumRemainingMaps(bool /*forceRecalculate*/) {
    for (auto& c : Courses) {
      c.tMapsUsed = 0;
      c.tMapsUsedNoVacant = 0;
    }
    tCalcNumMapsDataRevision = static_cast<int>(dataRevision);
  }
  void reEvaluateCourse(int /*id*/, bool /*sync*/) {}

  // ── Club management (implemented in oClub.cpp) ───────────────────────────
  pClub getClub(int Id) const;
  pClub getClub(const wstring& pname) const;
  pClub addClub(const wstring& pname, int createId = 0);
  pClub addClub(const oClub& oc);
  void  getClubs(vector<pClub>& c, bool sort);

  // ── Class management (implemented in oClass.cpp) ─────────────────────────
  int getFreeClassId();
  pClass getClass(int Id) const;
  pClass getClass(const wstring& cname) const;
  pClass addClass(const wstring& pname, int CourseId = 0, int classId = 0);
  pClass addClass(const oClass& c);
  void getClasses(vector<pClass>& cls, bool synchronize) const;
  void removeClass(int id);
  bool isClassUsed(int id) const;
  void reinitializeClasses() const;
  void classChanged(oClass* cls, bool doSync);
  void updateTabs();
  int getMaximalTime() const;
  void reCalculateLeaderTimes(int classId);
  void reEvaluateAll(const set<int>& classIds, bool sync);
  bool hasPrevStage() const;
  bool hasNextStage() const;
  void getPredefinedClassTypes(map<wstring, ClassMetaType>& types) const;

  // ── Vacant / no-club stubs (full impl in US-003i) ────────────────────────
  int getVacantClub(bool /*returnNoClubClub*/) { return 0; }
  int getVacantClubIfExist(bool /*returnNoClubClub*/) const { return 0; }

  // ── Club membership stubs ────────────────────────────────────────────────
  bool isClubUsed(int /*id*/) const { return false; }
  void removeClub(int id) {
    pClub pc = getClub(id);
    if (pc) { pc->Removed = true; clubIdIndex.remove(id); }
  }
  bool useRunnerDb() const { return false; }

  // ── Property store stubs (full impl in US-003i) ──────────────────────────
  int getPropertyInt(const char* /*name*/, int def) const { return def; }
  void setProperty(const char* /*name*/, int /*val*/) {}
  void setProperty(const char* /*name*/, const wstring& /*val*/) {}

  // ── Direct change notification (stub) ─────────────────────────────────────
  void pushDirectChange() {}

  // ── Stage / multi-day (stub) ──────────────────────────────────────────────
  int getStageNumber() const { return 1; }

  // ── Team result calculation (stub — full impl in US-003i) ─────────────────
  enum class ResultType { ClassResult, TotalResult };
  void calculateTeamResults(const std::set<int> &/*classIds*/, ResultType /*type*/) {}

  // ── Status formatting ─────────────────────────────────────────────────────
  static const wstring &formatStatus(RunnerStatus status, bool /*forPrint*/);

  // ── isRunnerUsed stub ─────────────────────────────────────────────────────
  bool isRunnerUsed(int /*id*/) const { return false; }

  // ── useStartSeconds stub ──────────────────────────────────────────────────
  bool useStartSeconds() const { return false; }

  // ── getAbsTime overload convenience ───────────────────────────────────────
  wstring getAbsTime(int t) const { return getAbsTime(t, SubSecond::Auto); }
};

// oEvent.h — Minimal oEvent stub for domain-layer compilation.
// The full oEvent implementation will be provided in US-003i.
#pragma once

#include "domain_header.h"
#include "../util/meos_util.h"
#include "../util/localizer.h"
#include "oCourse.h"
#include "oClass.h"
#include "oClub.h"
#include "intkeymap.hpp"

class oBase;
class oDataContainer;

typedef list<oClub>   oClubList;
typedef list<oCourse> oCourseList;
typedef list<oControl> oControlList;

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

  // ── Club collection ───────────────────────────────────────────────────────
  oClubList Clubs;
  mutable intkeymap<pClub> clubIdIndex;

  // ── oDataContainer pointers (initialized by test fixture or oEvent ctor) ──
  oDataContainer* oControlData = nullptr;
  oDataContainer* oClubData    = nullptr;
  oDataContainer* oCourseData  = nullptr;

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

  // ── Punch index (cleared when control numbers change) ────────────────────
  struct PunchIndexStub { void clear() {} } punchIndex;

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
    // Stub: set all maps-used counters to 0 (no Runners/Teams in stub).
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

  // ── Class management stubs (full impl in US-003e) ────────────────────────
  void getClasses(vector<pClass>& cls, bool /*synchronize*/) const { cls.clear(); }

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
};

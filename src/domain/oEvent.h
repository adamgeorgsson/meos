// oEvent.h — Minimal oEvent stub for domain-layer compilation.
// The full oEvent implementation will be provided in US-003i.
#pragma once

#include "domain_header.h"
#include "../util/meos_util.h"
#include "oCourse.h"
#include "oClass.h"
#include "oClub.h"
#include "intkeymap.hpp"

class oBase;
class oDataContainer;
typedef list<oClub> oClubList;

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

  // ── Collections (needed by oControl::getCourses / getClasses) ────────────
  list<oCourse> Courses;
  list<oClass>  Classes;

  // ── Club collection ───────────────────────────────────────────────────────
  oClubList Clubs;
  mutable intkeymap<pClub> clubIdIndex;

  // ── oDataContainer pointers (initialized by test fixture or oEvent ctor) ──
  oDataContainer* oControlData = nullptr;
  oDataContainer* oClubData    = nullptr;

  // ── SQL change tracking ───────────────────────────────────────────────────
  SqlUpdated sqlControls;
  SqlUpdated sqlClubs;
  bool globalModification = false;

  // ── Free-ID counters (increment-and-return pattern) ─────────────────────
  int qFreeClubId = 1;
  int getFreeClubId() { return ++qFreeClubId; }

  // ── Club management (implemented in oClub.cpp) ───────────────────────────
  pClub getClub(int Id) const;
  pClub getClub(const wstring &pname) const;
  pClub addClub(const wstring &pname, int createId = 0);
  pClub addClub(const oClub &oc);
  void  getClubs(vector<pClub> &c, bool sort);

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

  // ── Punch index (cleared when control numbers change) ────────────────────
  struct PunchIndexStub { void clear() {} } punchIndex;

  // ── Revision helpers ─────────────────────────────────────────────────────
  int getRevision() const { return static_cast<int>(dataRevision); }
  void setupControlStatistics() const {}

  // ── Score formatting (rogaining) ─────────────────────────────────────────
  wstring formatScore(int score) const { return itow(score); }
  int convertScore(const wstring& s) const {
    return static_cast<int>(std::wcstol(s.c_str(), nullptr, 10));
  }

  // ── Control management ────────────────────────────────────────────────────
  bool isControlUsed(int /*id*/) const { return false; }
  void removeControl(int /*id*/) {}
  void getControls(vector<pControl>& c, bool /*calculateCourseControls*/) const { c.clear(); }

  // ── Time helpers (used by oPunch) ─────────────────────────────────────────
  wstring getAbsTime(int /*t*/, SubSecond /*mode*/) const { return L""; }
  int getRelativeTime(const wstring& /*t*/) const { return 0; }
  int getZeroTimeNum() const { return 0; }
  int getUnitAdjustment(int /*type*/, int /*unit*/) const { return 0; }
};

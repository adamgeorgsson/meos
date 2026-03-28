// oEvent.h — Minimal oEvent stub for domain-layer compilation.
// The full oEvent implementation will be provided in US-003i.
#pragma once

#include "domain_header.h"
#include "../util/meos_util.h"
#include "oCourse.h"
#include "oClass.h"

class oBase;
class oDataContainer;

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

  // ── oDataContainer pointers (initialized by test fixture or oEvent ctor) ──
  oDataContainer* oControlData = nullptr;

  // ── SQL change tracking ───────────────────────────────────────────────────
  SqlUpdated sqlControls;
  bool globalModification = false;

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

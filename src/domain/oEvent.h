#pragma once

#include <list>
#include <map>
#include <string>
#include <vector>
#include "domain_header.h"
#include "oControl.h"

class oBase;
class oClub;
class oCourse;

// Minimal stub oEvent — provides just enough interface for oBase, oDataContainer,
// oPunch, oControl, and oClub. Full competition management logic lives in the legacy
// code/oEvent.h.
class oEvent {
public:
  int dataRevision = 0;
  bool hasPendingDBConnection = false;

  // Simplified club list — used by oClub::assignInvoiceNumber and friends.
  std::vector<oClub*> Clubs;
  // Simplified invoice date store (avoids pulling in full DataContainer on oEvent).
  mutable std::wstring eventInvoiceDate_;
  mutable int qFreeClubId_ = 0;

  // Control store — backing storage for getControl() / getFreeCourseId().
  mutable std::list<oControl> controlList_;
  mutable std::map<int, oControl*> controlIndex_;
  mutable int qFreeCourseId_ = 0;

  // Dirty flags set by changedObject() of course/control entities.
  bool globalModification = false;
  int tCalcNumMapsDataRevision = -1;
  struct SqlState { bool changed = false; };
  SqlState sqlCourses;

  bool isClient() const { return false; }
  bool hasDBConnection() const { return false; }

  // Called when an object's Id is assigned or changes.
  void updateFreeId(oBase*) {}

  // Returns the current data revision counter.
  int getRevision() const { return dataRevision; }

  // Stub: compute control visit/mistake statistics (full impl needs runner lists).
  void setupControlStatistics() const {}

  // Format a rogaining score as a string (stub: decimal).
  std::wstring formatScore(int v) const { return std::to_wstring(v); }

  // Parse a rogaining score string (stub: decimal).
  int convertScore(const std::wstring& s) const {
    try { return std::stoi(s); } catch (...) { return 0; }
  }

  // Punch-code index (cleared when control numbers change). Stub.
  struct PunchIndex { void clear() {} } punchIndex;

  // -----------------------------------------------------------------------
  // oPunch stubs
  // -----------------------------------------------------------------------

  // Format absolute time (tenths-of-sec from midnight) as "HH:MM:SS".
  std::wstring getAbsTime(int t, SubSecond /*mode*/) const {
    if (t <= 0) return L"-";
    int sec  = t / timeConstSecond;
    int hour = sec / 3600; sec %= 3600;
    int min  = sec / 60;   sec %= 60;
    wchar_t buf[16];
    swprintf(buf, 16, L"%d:%02d:%02d", hour, min, sec);
    return buf;
  }

  // Adjustment for a given punch unit (stub: 0).
  int getUnitAdjustment(int /*specialPunchType*/, int /*unit*/) const { return 0; }

  // Parse an absolute time string to tenths-of-sec units (stub: simple parse).
  int getRelativeTime(const std::wstring& t) const {
    return convertAbsoluteTimeHMS(t, -1);
  }

  // Zero-time number in tenths-of-sec units (stub: 0).
  int getZeroTimeNum() const { return 0; }

  // -----------------------------------------------------------------------
  // oClub stubs
  // -----------------------------------------------------------------------

  // Allocate a fresh club ID (stub: auto-increment from 1).
  int getFreeClubId() const { return ++qFreeClubId_; }

  // Returns the ID of the "vacant" club if it exists (stub: 0 = none).
  int getVacantClubIfExist(bool /*create*/) const { return 0; }

  // Returns true if any runner/team belongs to this club (stub: false).
  bool isClubUsed(int /*clubId*/) const { return false; }

  // Remove a club by ID (stub: no-op).
  void removeClub(int /*clubId*/) {}

  // Synchronize a list identified by integer ID (stub: no-op).
  void synchronizeList(int /*listId*/) {}

  // Property accessors used by assignInvoiceNumber (stub: always return default).
  int getPropertyInt(const char* /*name*/, int def) const { return def; }
  void setProperty(const char* /*name*/, int /*val*/) {}

  // -----------------------------------------------------------------------
  // oCourse stubs
  // -----------------------------------------------------------------------

  // Find or optionally create a control by id.
  oControl* getControl(int id, bool create = false,
                       bool /*includeVirtual*/ = false) const {
    auto it = controlIndex_.find(id);
    if (it != controlIndex_.end()) return it->second;
    if (!create) return nullptr;
    controlList_.emplace_back(const_cast<oEvent*>(this), id);
    oControl* ctrl = &controlList_.back();
    controlIndex_[id] = ctrl;
    return ctrl;
  }

  // Allocate a fresh course ID (stub: auto-increment from 1).
  int getFreeCourseId() const { return ++qFreeCourseId_; }
};

#pragma once

#include <list>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "domain_header.h"
#include "oControl.h"
#include "oFreePunch.h"
#include "oAbstractRunner.h"

class oBase;
class oCard;
class oCourse;

// Minimal stub oEvent — provides just enough interface for oBase, oDataContainer,
// oPunch, oControl, oClub, oCourse, and oClass. Full competition management logic lives in the legacy
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

  // Class store — backing storage for getFreeClassId().
  mutable int qFreeClassId = 0;

  // Runner store
  mutable int qFreeRunnerId = 0;

  // Dirty flags set by changedObject() of course/control/class entities.
  bool globalModification = false;
  int tCalcNumMapsDataRevision = -1;
  struct SqlState { bool changed = false; };
  SqlState sqlCourses;
  SqlState sqlClasses;
  mutable SqlState sqlRunners;

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

  // Punch-code index (cleared when control numbers change). Stub used by oControl.
  // Full implementation lives in oFreePunch stubs section below.

  // -----------------------------------------------------------------------
  // oRunner stubs
  // -----------------------------------------------------------------------

  // Runner lookup hashes (cleared on change)
  mutable std::multimap<int, oAbstractRunner*> bibStartNoToRunnerTeam;
  mutable std::shared_ptr<std::unordered_multimap<int, oRunner*>> cardToRunnerHash;
  mutable std::shared_ptr<std::map<int, std::vector<oRunner*>>> classIdToRunnerHash;

  int getFreeRunnerId() const { return ++qFreeRunnerId; }

  // Club lookups (stubs: no storage)
  pClub getClub(int /*id*/) const { return nullptr; }
  pClub getClub(const std::wstring& /*name*/) const { return nullptr; }
  pClub getClubCreate(int /*id*/, const std::wstring& /*name*/) const { return nullptr; }

  // Card allocation (stub)
  oCard* allocateCard(oRunner* /*owner*/) const { return nullptr; }
  oCard* getCard(int /*id*/) const { return nullptr; }

  // Course lookup (stub)
  oCourse* getCourseById(int /*id*/) const { return nullptr; }

  // Class lookup (stub)
  oClass* getClass(int /*id*/) const { return nullptr; }

  bool hasPrevStage() const { return false; }

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
    ctrl->set(id, id, L"");  // initialize control number so hasNumberUnchecked works
    controlIndex_[id] = ctrl;
    return ctrl;
  }

  // Allocate a fresh course ID (stub: auto-increment from 1).
  int getFreeCourseId() const { return ++qFreeCourseId_; }

  // -----------------------------------------------------------------------
  // oClass stubs
  // -----------------------------------------------------------------------

  // Course store for oClass::importCourses (stub: no course list, returns nullptr).
  oCourse* getCourse(int /*id*/) const { return nullptr; }

  // Allocate a fresh class ID (stub: auto-increment from 1).
  int getFreeClassId() const { return ++qFreeClassId; }

  // -----------------------------------------------------------------------
  // oFreePunch stubs
  // -----------------------------------------------------------------------
  using PunchIndexType    = std::multimap<int, oFreePunch*>;
  using PunchConstIterator = PunchIndexType::const_iterator;
  using oFreePunchList    = std::list<oFreePunch>;

  mutable oFreePunchList freePunches;
  mutable std::map<int, PunchIndexType> punchIndex;
  mutable int qFreePunchId = 0;
  mutable SqlState sqlPunches;

  int getFreePunchId() const { return ++qFreePunchId; }

  void insertIntoPunchHash(int /*card*/, int /*code*/, int /*time*/) {}
  void removeFromPunchHash(int /*card*/, int /*code*/, int /*time*/) {}

  // Declared here; defined in oFreePunch.cpp (needs full oFreePunch type).
  int getControlIdFromPunch(int time, int type, int card,
                            bool markClassChanged, oFreePunch& punch);

  oRunner* getRunner(int /*id*/, int /*race*/) const { return nullptr; }

  // Re-evaluate all runners in a class (stub: no-op in domain layer)
  void reEvaluateAll(const std::vector<int>& /*classIds*/, bool /*sync*/) {}

  // -----------------------------------------------------------------------
  // oCard stubs
  // -----------------------------------------------------------------------
  mutable int qFreeCardId = 0;
  mutable SqlState sqlCards;

  int getFreeCardId() const { return ++qFreeCardId; }
};

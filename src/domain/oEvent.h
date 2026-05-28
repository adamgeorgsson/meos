#pragma once

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "domain_header.h"
#include "oControl.h"
#include "oFreePunch.h"
#include "oAbstractRunner.h"
#include "oClub.h"
#include "oCard.h"
#include "oRunner.h"
#include "oTeam.h"

class oBase;

// Collection type aliases matching legacy naming.
using oControlList  = std::list<oControl>;
using oCourseList   = std::list<oCourse>;
using oClassList    = std::list<oClass>;
using oClubList     = std::list<oClub>;
using oRunnerList   = std::list<oRunner>;
using oCardList     = std::list<oCard>;
using oTeamList     = std::list<oTeam>;
using oFreePunchList = std::list<oFreePunch>;

class oEvent {
public:
  // -----------------------------------------------------------------------
  // Competition metadata
  // -----------------------------------------------------------------------
  int dataRevision = 0;
  bool hasPendingDBConnection = false;
  std::wstring Name;
  int ZeroTime = 0;           // competition zero time in tenths-of-second

  // -----------------------------------------------------------------------
  // Entity collections (oEvent is the owner/backing store)
  // -----------------------------------------------------------------------
  mutable oControlList  Controls;   // mutable: getControl(id, create=true) is const
  oCourseList   Courses;
  oClassList    Classes;
  oClubList     Clubs;
  oRunnerList   Runners;
  oTeamList     Teams;
  oCardList     Cards;

  // Simplified invoice date store.
  mutable std::wstring eventInvoiceDate_;

  // -----------------------------------------------------------------------
  // Free-ID counters
  // -----------------------------------------------------------------------
  mutable int qFreeControlId_  = 0;   // dedicated counter for addControl
  mutable int qFreeClubId_    = 0;
  mutable int qFreeCourseId_  = 0;
  mutable int qFreeClassId    = 0;
  mutable int qFreeRunnerId   = 0;
  mutable int qFreeCardId     = 0;
  mutable int qFreeTeamId     = 0;
  mutable int qFreePunchId    = 0;

  // -----------------------------------------------------------------------
  // Lookup indices (populated by add* methods / getControl create=true)
  // -----------------------------------------------------------------------
  mutable std::map<int, oControl*>              controlIndex_;
  mutable std::map<int, oCourse*>               courseByIdIndex;
  mutable std::map<int, oClass*>                classByIdIndex;
  mutable std::map<int, oClub*>                 clubByIdIndex;
  mutable std::map<int, oRunner*>               runnerById;
  mutable std::map<int, oCard*>                 cardByIdIndex;

  // Dirty flags
  bool globalModification        = false;
  mutable int tCalcNumMapsDataRevision = -1;
  struct SqlState { bool changed = false; };
  SqlState sqlCourses;
  SqlState sqlClasses;
  mutable SqlState sqlRunners;
  mutable SqlState sqlPunches;
  mutable SqlState sqlCards;
  mutable SqlState sqlTeams;
  SqlState sqlControls;

  // Runner lookup hashes (cleared on change)
  mutable std::multimap<int, oAbstractRunner*>                       bibStartNoToRunnerTeam;
  mutable std::shared_ptr<std::unordered_multimap<int, oRunner*>>    cardToRunnerHash;
  mutable std::shared_ptr<std::map<int, std::vector<oRunner*>>>      classIdToRunnerHash;

  // -----------------------------------------------------------------------
  // Punch store
  // -----------------------------------------------------------------------
  using PunchIndexType     = std::multimap<int, oFreePunch*>;
  using PunchConstIterator = PunchIndexType::const_iterator;

  mutable oFreePunchList               freePunches;
  mutable std::map<int, PunchIndexType> punchIndex;

  // -----------------------------------------------------------------------
  // Lifecycle
  // -----------------------------------------------------------------------

  /** Clear all entity collections and reset indices/counters. */
  void newCompetition(const std::wstring& name);

  // -----------------------------------------------------------------------
  // Entity management — add* methods create an entity, register it in the
  // appropriate backing list and index, and return a stable pointer.
  // -----------------------------------------------------------------------
  oControl* addControl(int id = 0);
  oCourse*  addCourse(int id = 0);
  oClass*   addClass(int id = 0);
  oClub*    addClub(int id = 0);
  oRunner*  addRunner(int id = 0);
  oTeam*    addTeam(int id = 0);
  oCard*    addCard(int id = 0);

  // -----------------------------------------------------------------------
  // Lookup methods
  // -----------------------------------------------------------------------

  /** Selector for getRunnerByCardNo. */
  enum class CardLookupProperty {
    Any,               ///< Any runner (excludes NotCompeting)
    ForReadout,        ///< Runners with no card, even if DNS
    IncludeNotCompeting,
    CardInUse,         ///< Runners with no card, ignoring DNS
    SkipNoStart,       ///< Skip DNS/CANCEL
    OnlyMainInstance
  };

  /** Find a control whose Numbers[] contains num. */
  oControl* getControlByNumber(int num) const;

  /** Find a runner by card number. Simple linear scan (prop ignored for now). */
  oRunner* getRunnerByCardNo(int cardNo, int time, CardLookupProperty prop) const;

  // -----------------------------------------------------------------------
  // Core accessors used by entities
  // -----------------------------------------------------------------------

  bool isClient() const { return false; }
  bool hasDBConnection() const { return false; }
  void updateFreeId(oBase*) {}
  int  getRevision() const { return dataRevision; }

  void setupControlStatistics() const {}

  std::wstring formatScore(int v) const { return std::to_wstring(v); }
  int convertScore(const std::wstring& s) const {
    try { return std::stoi(s); } catch (...) { return 0; }
  }

  // -----------------------------------------------------------------------
  // Free-ID allocators
  // -----------------------------------------------------------------------
  int getFreeClubId()   const { return ++qFreeClubId_; }
  int getFreeCourseId() const { return ++qFreeCourseId_; }
  int getFreeClassId()  const { return ++qFreeClassId; }
  int getFreeRunnerId() const { return ++qFreeRunnerId; }
  int getFreeCardId()   const { return ++qFreeCardId; }
  int getFreeTeamId()   const { return ++qFreeTeamId; }
  int getFreePunchId()  const { return ++qFreePunchId; }

  // -----------------------------------------------------------------------
  // Entity lookups (return nullptr when not found)
  // -----------------------------------------------------------------------

  oControl* getControl(int id, bool create = false,
                       bool /*includeVirtual*/ = false) const {
    auto it = controlIndex_.find(id);
    if (it != controlIndex_.end()) return it->second;
    if (!create) return nullptr;
    Controls.emplace_back(const_cast<oEvent*>(this), id);
    oControl* ctrl = &Controls.back();
    ctrl->set(id, id, L"");  // initialize number so hasNumberUnchecked works
    controlIndex_[id] = ctrl;
    return ctrl;
  }

  oCourse* getCourse(int id) const {
    auto it = courseByIdIndex.find(id);
    return it != courseByIdIndex.end() ? it->second : nullptr;
  }
  oCourse* getCourseById(int id) const { return getCourse(id); }

  oClass* getClass(int id) const {
    auto it = classByIdIndex.find(id);
    return it != classByIdIndex.end() ? it->second : nullptr;
  }

  pClub getClub(int id) const {
    auto it = clubByIdIndex.find(id);
    return it != clubByIdIndex.end() ? it->second : nullptr;
  }
  pClub getClub(const std::wstring& /*name*/) const { return nullptr; }
  pClub getClubCreate(int /*id*/, const std::wstring& /*name*/) const { return nullptr; }

  oCard* getCard(int id) const {
    auto it = cardByIdIndex.find(id);
    return it != cardByIdIndex.end() ? it->second : nullptr;
  }

  oRunner* getRunner(int id, int /*race*/) const {
    auto it = runnerById.find(id);
    return it != runnerById.end() ? it->second : nullptr;
  }

  oCard* allocateCard(oRunner* /*owner*/) const { return nullptr; }
  bool hasPrevStage() const { return false; }

  // -----------------------------------------------------------------------
  // oPunch helpers
  // -----------------------------------------------------------------------

  std::wstring getAbsTime(int t, SubSecond /*mode*/) const {
    if (t <= 0) return L"-";
    int sec  = t / timeConstSecond;
    int hour = sec / 3600; sec %= 3600;
    int min  = sec / 60;   sec %= 60;
    wchar_t buf[16];
    swprintf(buf, 16, L"%d:%02d:%02d", hour, min, sec);
    return buf;
  }

  int getUnitAdjustment(int /*specialPunchType*/, int /*unit*/) const { return 0; }

  int getRelativeTime(const std::wstring& t) const {
    return convertAbsoluteTimeHMS(t, -1);
  }

  int getZeroTimeNum() const { return ZeroTime; }

  // -----------------------------------------------------------------------
  // oClub helpers
  // -----------------------------------------------------------------------

  int getVacantClubIfExist(bool /*create*/) const { return 0; }
  bool isClubUsed(int /*clubId*/) const { return false; }
  void removeClub(int /*clubId*/) {}
  void synchronizeList(int /*listId*/) {}
  int  getPropertyInt(const char* /*name*/, int def) const { return def; }
  void setProperty(const char* /*name*/, int /*val*/) {}

  // -----------------------------------------------------------------------
  // oFreePunch helpers
  // -----------------------------------------------------------------------

  void insertIntoPunchHash(int /*card*/, int /*code*/, int /*time*/) {}
  void removeFromPunchHash(int /*card*/, int /*code*/, int /*time*/) {}

  // Declared here; defined in oFreePunch.cpp (needs full oFreePunch type).
  int getControlIdFromPunch(int time, int type, int card,
                            bool markClassChanged, oFreePunch& punch);

  // -----------------------------------------------------------------------
  // Result computation
  // -----------------------------------------------------------------------

  enum class ResultType {
    ClassResult,
    ClassResultDefault,
    TotalResult,
    TotalResultDefault,
    CourseResult,
    ClassCourseResult,
    PreliminarySplitResults
  };

  /** Populate out with non-removed runners, optionally filtered to a set of class IDs. */
  void getRunners(const std::set<int>& classes, std::vector<pRunner>& out) const;

  /**
   * Assign tPlace for each runner within their class group.
   * ClassResult/ClassResultDefault: group by (classId, duplicateLeg, legEquClass),
   *   sort by running time; StatusOK runners get sequential places 1..N.
   * TotalResult/TotalResultDefault: group by classId, sort by getTotalRunningTime();
   *   StatusOK total gets sequential places.
   * Other types: no-op in this simplified implementation.
   */
  void calculateResults(const std::set<int>& classes, ResultType resultType,
                        bool includePreliminary = false) const;

  /**
   * Assign tPlace for each team within their class.
   * Sorts teams by getTotalRunningTime() where getTotalStatus() == StatusOK.
   */
  void calculateTeamResults(const std::set<int>& classIds, ResultType resultType) const;

  // -----------------------------------------------------------------------
  // Start list drawing
  // -----------------------------------------------------------------------

  enum class VacantPosition { Mixed, Last, First };

  struct ClassDrawSpecification {
    int classID    = -1;
    int leg        = 0;
    int firstStart = 0;   ///< tenths-of-second from midnight
    int interval   = 600; ///< tenths-of-second between runners (default 60 s)
    int vacances   = 0;

    ClassDrawSpecification() = default;
    ClassDrawSpecification(int cls, int l, int first, int inter, int vac = 0)
      : classID(cls), leg(l), firstStart(first), interval(inter), vacances(vac) {}
  };

  /**
   * Assign start times to runners in the specified classes.
   * Runners are taken in their current list order (no shuffle).
   * Each runner's tStartTime is set to firstStart + i * interval.
   */
  void drawStartList(const std::vector<ClassDrawSpecification>& spec);

  // -----------------------------------------------------------------------
  // Miscellaneous stubs
  // -----------------------------------------------------------------------
  void reEvaluateAll(const std::vector<int>& /*classIds*/, bool /*sync*/) {}
};

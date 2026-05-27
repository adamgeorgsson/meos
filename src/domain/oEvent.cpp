#include "oEvent.h"
#include <ctime>

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void oEvent::newCompetition(const std::wstring& name) {
  // Clear all entity collections (destructors run in reverse-push order).
  Runners.clear();
  Teams.clear();
  Cards.clear();
  freePunches.clear();
  punchIndex.clear();
  Controls.clear();
  Courses.clear();
  Classes.clear();
  Clubs.clear();

  // Clear all lookup indices.
  controlIndex_.clear();
  courseByIdIndex.clear();
  classByIdIndex.clear();
  clubByIdIndex.clear();
  runnerById.clear();
  cardByIdIndex.clear();
  bibStartNoToRunnerTeam.clear();
  cardToRunnerHash.reset();
  classIdToRunnerHash.reset();

  // Reset free-ID counters.
  qFreeControlId_ = 0;
  qFreeClubId_   = 0;
  qFreeCourseId_ = 0;
  qFreeClassId   = 0;
  qFreeRunnerId  = 0;
  qFreeCardId    = 0;
  qFreeTeamId    = 0;
  qFreePunchId   = 0;

  // Reset dirty flags.
  sqlCourses  = {};
  sqlClasses  = {};
  sqlRunners  = {};
  sqlPunches  = {};
  sqlCards    = {};
  sqlTeams    = {};
  sqlControls = {};
  globalModification = false;
  dataRevision = 0;

  // Set ZeroTime to the current local hour (matches legacy behaviour).
  std::time_t now = std::time(nullptr);
  std::tm tm{};
#ifdef _WIN32
  localtime_s(&tm, &now);
#else
  localtime_r(&now, &tm);
#endif
  ZeroTime = tm.tm_hour * timeConstHour;

  Name = name;
}

// ---------------------------------------------------------------------------
// Entity management — add* methods
// ---------------------------------------------------------------------------

oControl* oEvent::addControl(int id) {
  if (id == 0) id = ++qFreeControlId_;
  Controls.emplace_back(this, id);
  oControl* p = &Controls.back();
  p->set(id, id, L"");
  controlIndex_[id] = p;
  return p;
}

oCourse* oEvent::addCourse(int id) {
  if (id == 0) id = ++qFreeCourseId_;
  Courses.emplace_back(this, id);
  oCourse* p = &Courses.back();
  courseByIdIndex[id] = p;
  return p;
}

oClass* oEvent::addClass(int id) {
  if (id == 0) id = ++qFreeClassId;
  Classes.emplace_back(this, id);
  oClass* p = &Classes.back();
  classByIdIndex[id] = p;
  return p;
}

oClub* oEvent::addClub(int id) {
  if (id == 0) id = ++qFreeClubId_;
  Clubs.emplace_back(this, id);
  oClub* p = &Clubs.back();
  clubByIdIndex[id] = p;
  return p;
}

oRunner* oEvent::addRunner(int id) {
  if (id == 0) id = ++qFreeRunnerId;
  Runners.emplace_back(this, id);
  oRunner* p = &Runners.back();
  runnerById[id] = p;
  if (p->getCardNo() != 0) {
    if (!cardToRunnerHash)
      cardToRunnerHash = std::make_shared<std::unordered_multimap<int, oRunner*>>();
    cardToRunnerHash->emplace(p->getCardNo(), p);
  }
  return p;
}

oTeam* oEvent::addTeam(int id) {
  if (id == 0) id = ++qFreeTeamId;
  Teams.emplace_back(this, id);
  oTeam* p = &Teams.back();
  return p;
}

oCard* oEvent::addCard(int id) {
  if (id == 0) id = ++qFreeCardId;
  Cards.emplace_back(this, id);
  oCard* p = &Cards.back();
  cardByIdIndex[id] = p;
  return p;
}

// ---------------------------------------------------------------------------
// Lookup methods
// ---------------------------------------------------------------------------

oControl* oEvent::getControlByNumber(int num) const {
  for (auto& c : Controls) {
    if (c.isRemoved()) continue;
    std::vector<int> nums;
    c.getNumbers(nums);
    for (int n : nums) {
      if (n == num) return const_cast<oControl*>(&c);
    }
  }
  return nullptr;
}

oRunner* oEvent::getRunnerByCardNo(int cardNo, int /*time*/,
                                   oEvent::CardLookupProperty prop) const {
  if (cardToRunnerHash) {
    auto range = cardToRunnerHash->equal_range(cardNo);
    for (auto it = range.first; it != range.second; ++it) {
      oRunner* r = it->second;
      if (r->isRemoved() || r->getCardNo() != cardNo) continue;
      if (prop == oEvent::CardLookupProperty::SkipNoStart &&
          (r->getStatus() == StatusDNS || r->getStatus() == StatusCANCEL))
        continue;
      return r;
    }
    return nullptr;
  }
  // Fallback: linear scan (before cardToRunnerHash is populated).
  for (auto& r : Runners) {
    if (r.isRemoved() || r.getCardNo() != cardNo) continue;
    if (prop == oEvent::CardLookupProperty::SkipNoStart &&
        (r.getStatus() == StatusDNS || r.getStatus() == StatusCANCEL))
      continue;
    return const_cast<oRunner*>(&r);
  }
  return nullptr;
}

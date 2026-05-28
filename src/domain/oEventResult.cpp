#include "oEvent.h"
#include "oRunner.h"
#include "oTeam.h"
#include "oClass.h"

#include <algorithm>
#include <map>
#include <vector>

// ---------------------------------------------------------------------------
// Helper: collect non-removed runners, filtered to a set of class IDs.
// ---------------------------------------------------------------------------

void oEvent::getRunners(const std::set<int>& classes, std::vector<pRunner>& out) const {
  out.clear();
  for (auto& r : Runners) {
    if (r.isRemoved()) continue;
    if (!classes.empty() && !classes.count(r.getClassId(true))) continue;
    out.push_back(const_cast<oRunner*>(&r));
  }
}

// ---------------------------------------------------------------------------
// calculateResults — assign tPlace per runner within each class group.
//
// Group key: (classId * 100 + duplicateLeg + 10 * legEquClass)
// Score: running time if StatusOK (+ preliminary check), else -1 (not placed).
// Sort: ascending score (ties get same place).
// ---------------------------------------------------------------------------

void oEvent::calculateResults(const std::set<int>& classes,
                               ResultType resultType,
                               bool includePreliminary) const {
  const bool totalResults = (resultType == ResultType::TotalResult ||
                              resultType == ResultType::TotalResultDefault);

  // Unsupported special result types — skip silently.
  if (resultType == ResultType::CourseResult ||
      resultType == ResultType::ClassCourseResult ||
      resultType == ResultType::PreliminarySplitResults)
    return;

  // Collect runners.
  std::vector<pRunner> runners;
  getRunners(classes, runners);
  if (runners.empty()) return;

  // Build (groupId, score, runner) triples.
  struct Entry {
    int groupId;
    int score;   // -1 = not placed
    pRunner r;
  };
  std::vector<Entry> entries;
  entries.reserve(runners.size());

  for (pRunner r : runners) {
    int clsId = r->getClassId(true);
    int groupId;
    if (totalResults) {
      groupId = clsId;
    } else {
      groupId = clsId * 100 + (r->tDuplicateLeg + 10 * r->tLegEquClass);
    }

    int score = -1;
    if (totalResults) {
      RunnerStatus totSt = r->getTotalStatus(false);
      if (totSt == StatusOK) {
        score = r->getTotalRunningTime(r->FinishTime, false, true);
        if (score <= 0) score = -1;
      } else if (includePreliminary && totSt == StatusUnknown &&
                 r->inputStatus == StatusOK) {
        int tt = r->getTotalRunningTime(r->FinishTime, false, true);
        if (tt > 0) score = tt;
      }
    } else {
      RunnerStatus st = r->getStatus();
      if (st == StatusOK) {
        score = r->getRunningTime(false);
        if (score <= 0) score = -1;
      } else if (includePreliminary && st == StatusUnknown && r->FinishTime > 0) {
        score = r->getRunningTime(false);
        if (score <= 0) score = -1;
      }
    }

    entries.push_back({groupId, score, r});
  }

  // Sort: by groupId first, then by score ascending (-1 = last).
  std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
    if (a.groupId != b.groupId) return a.groupId < b.groupId;
    if (a.score < 0 && b.score >= 0) return false;
    if (b.score < 0 && a.score >= 0) return true;
    if (a.score < 0 && b.score < 0) return false;
    return a.score < b.score;
  });

  // Assign places within each group.
  int curGroup = -1;
  int cPlace = 0, vPlace = 0, cScore = -1;

  for (auto& e : entries) {
    if (e.groupId != curGroup) {
      curGroup = e.groupId;
      cPlace = 0; vPlace = 0; cScore = -1;
    }

    if (e.score > 0) {
      cPlace++;
      if (e.score > cScore) {
        vPlace = cPlace;
        cScore = e.score;
      }
      // Update tPlace (or tTotalPlace).
      if (totalResults)
        e.r->tTotalPlace.update(*this, 0, vPlace, false);
      else
        e.r->tPlace.update(*this, 0, vPlace, false);
    } else {
      if (totalResults)
        e.r->tTotalPlace.update(*this, 0, 0, false);
      else
        e.r->tPlace.update(*this, 0, 0, false);
    }
  }
}

// ---------------------------------------------------------------------------
// calculateTeamResults — assign place to each team in its class.
// ---------------------------------------------------------------------------

void oEvent::calculateTeamResults(const std::set<int>& classIds,
                                   ResultType /*resultType*/) const {
  // Collect teams per class.
  struct Entry {
    int classId;
    int score;  // -1 = not placed
    pTeam t;
  };
  std::vector<Entry> entries;

  for (auto& team : Teams) {
    if (team.isRemoved()) continue;
    int cid = team.getClassId(true);
    if (!classIds.empty() && !classIds.count(cid)) continue;

    int score = -1;
    RunnerStatus st = team.getLegStatus(-1, false, false);
    if (st == StatusOK) {
      score = team.getLegRunningTime(-1, false, false);
      if (score <= 0) score = -1;
    }
    entries.push_back({cid, score, const_cast<oTeam*>(&team)});
  }

  if (entries.empty()) return;

  // Sort by (classId, score ascending, -1 last).
  std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
    if (a.classId != b.classId) return a.classId < b.classId;
    if (a.score < 0 && b.score >= 0) return false;
    if (b.score < 0 && a.score >= 0) return true;
    if (a.score < 0 && b.score < 0) return false;
    return a.score < b.score;
  });

  int curClass = -1;
  int cPlace = 0, vPlace = 0, cScore = -1;

  for (auto& e : entries) {
    if (e.classId != curClass) {
      curClass = e.classId;
      cPlace = 0; vPlace = 0; cScore = -1;
    }
    if (e.score > 0) {
      cPlace++;
      if (e.score > cScore) { vPlace = cPlace; cScore = e.score; }
      int lastLeg = std::max(0, (int)e.t->Runners.size() - 1);
      e.t->getTeamPlace(lastLeg).p.update(*this, 0, vPlace, false);
    } else {
      int lastLeg = std::max(0, (int)e.t->Runners.size() - 1);
      e.t->getTeamPlace(lastLeg).p.update(*this, 0, 0, false);
    }
  }
}

// ---------------------------------------------------------------------------
// getTeams — collect non-removed teams, optionally filtered by classId.
// ---------------------------------------------------------------------------

void oEvent::getTeams(int classId, std::vector<pTeam>& out, bool /*removeVacant*/) const {
  out.clear();
  for (auto& t : Teams) {
    if (t.isRemoved()) continue;
    if (classId != 0 && t.getClassId(true) != classId) continue;
    out.push_back(const_cast<oTeam*>(&t));
  }
}

// ---------------------------------------------------------------------------
// calculateTeamResults(vector<pTeam>&, ResultType) — vector overload used by
// GeneralResult modules.
// ---------------------------------------------------------------------------

void oEvent::calculateTeamResults(std::vector<pTeam>& teams, ResultType resultType) const {
  if (teams.empty()) return;
  std::set<int> classIds;
  for (pTeam t : teams)
    classIds.insert(t->getClassId(true));
  calculateTeamResults(classIds, resultType);
}

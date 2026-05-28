/************************************************************************
    MeOS — GeneralResult scoring engine
    Migrated to src/domain/.
    XML/GDI/file I/O methods excluded (belong in io/net layers).
    DynamicResult expression evaluation is stubbed (Parser is a stub).
************************************************************************/

#include "generalresult.h"
#include "oListInfo.h"
#include "oRunner.h"
#include "oTeam.h"
#include "oEvent.h"
#include "oPunch.h"
#include "common_enums.h"

#include <algorithm>
#include <cassert>

using namespace std;

// ===========================================================================
// GeneralResultCtr
// ===========================================================================

GeneralResultCtr::GeneralResultCtr(const char* tagIn, const wstring& nameIn,
                                   const shared_ptr<GeneralResult>& ptrIn) {
  name = nameIn;
  tag  = tagIn;
  ptr  = ptrIn;
}

GeneralResultCtr::GeneralResultCtr(const wstring& file,
                                   const shared_ptr<DynamicResult>& ptrIn) {
  ptr        = ptrIn;
  name       = ptrIn->getName(false);
  tag        = ptrIn->getTag();
  fileSource = file;
}

bool GeneralResultCtr::operator<(const GeneralResultCtr& c) const {
  return name < c.name;
}

const wstring& GeneralResultCtr::getName() const {
  // In the migrated layer there is no localizer; return the raw name.
  return name;
}

GeneralResultCtr::GeneralResultCtr(const GeneralResultCtr& ctr) {
  ptr        = ctr.ptr;
  name       = ctr.name;
  tag        = ctr.tag;
  fileSource = ctr.fileSource;
}

void GeneralResultCtr::operator=(const GeneralResultCtr& ctr) {
  if (this == &ctr) return;
  name       = ctr.name;
  ptr        = ctr.ptr;
  tag        = ctr.tag;
  fileSource = ctr.fileSource;
}

bool GeneralResultCtr::isDynamic() const {
  return !fileSource.empty();
}

// ===========================================================================
// GeneralResult
// ===========================================================================

GeneralResult::GeneralResult()  { context = nullptr; }
GeneralResult::~GeneralResult() {}

const string& GeneralResult::getTimeStamp() const {
  static const string empty;
  return empty;
}

bool GeneralResult::isRogaining() const { return false; }

void GeneralResult::setContext(const oListParam* ctx) { context = ctx; }
void GeneralResult::clearContext()                    { context = nullptr; }

int GeneralResult::getListParamTimeToControl() const {
  return context ? context->useControlIdResultTo : 0;
}

int GeneralResult::getListParamTimeFromControl() const {
  return context ? context->useControlIdResultFrom : 0;
}

// ---------------------------------------------------------------------------
// Internal sort helper
// ---------------------------------------------------------------------------

struct GRSortInfo {
  int principalSort = 0;
  pair<int,int> score = {0,0};
  oAbstractRunner* tr = nullptr;

  bool operator<(const GRSortInfo& other) const {
    if (principalSort != other.principalSort)
      return principalSort < other.principalSort;
    if (score != other.score)
      return score < other.score;
    const wstring& as = tr->getBib();
    const wstring& bs = other.tr->getBib();
    if (as != bs) return compareBib(as, bs);
    return tr->getName() < other.tr->getName();
  }
};

// ---------------------------------------------------------------------------
// calculateTeamResults (instance method)
// ---------------------------------------------------------------------------

void GeneralResult::calculateTeamResults(vector<oTeam*>& teams,
                                          bool classResult,
                                          oListInfo::ResultType resType,
                                          bool sortTeams,
                                          int inputNumber) const {
  if (teams.empty()) return;
  if (lockPrepare > 2) throw meosException("Bad cyclic call");
  lockPrepare++;
  try {
    set<int> clsId;
    vector<pRunner> runners;
    for (pTeam t : teams) {
      clsId.insert(t->getClassId(true));
      for (int j = 0; j < t->getNumRunners(); j++) {
        pRunner r = t->getRunner(j);
        if (r) { runners.push_back(r); clsId.insert(r->getClassId(true)); }
      }
    }
    prepareCalculations(*teams[0]->oe, classResult, clsId, runners, teams, inputNumber);

    vector<GRSortInfo> teamScore(teams.size());
    for (size_t k = 0; k < teams.size(); k++) {
      if (resType == oListInfo::Classwise) {
        teamScore[k].principalSort =
          teams[k]->Class ? teams[k]->Class->getSortIndex() * 50000 + teams[k]->Class->getId() : 0;
      }
      prepareCalculations(*teams[k], classResult);
      teams[k]->tmpResult.runningTime_ = deduceTime(*teams[k]);
      teams[k]->tmpResult.status_      = deduceStatus(*teams[k]);
      teams[k]->tmpResult.points_      = deducePoints(*teams[k]);
      teamScore[k].score = score(*teams[k], teams[k]->tmpResult.status_,
                                  teams[k]->tmpResult.runningTime_,
                                  teams[k]->tmpResult.points_);
      storeOutput(teams[k]->tmpResult.outputTimes_, teams[k]->tmpResult.outputNumbers_);
      teamScore[k].tr = teams[k];
    }

    ::sort(teamScore.begin(), teamScore.end());
    int place = 1, iPlace = 1, leadtime = 0;
    for (size_t k = 0; k < teamScore.size(); k++) {
      if (k > 0 && teamScore[k-1].principalSort != teamScore[k].principalSort) {
        place = 1; iPlace = 1;
      } else if (k > 0 && teamScore[k-1].score != teamScore[k].score) {
        place = iPlace;
      }
      if (teamScore[k].tr->tmpResult.status_ == StatusOK) {
        teamScore[k].tr->tmpResult.place_     = place;
        iPlace++;
        if (place == 1) {
          leadtime = teamScore[k].tr->tmpResult.runningTime_;
          teamScore[k].tr->tmpResult.timeAfter_ = 0;
        } else {
          teamScore[k].tr->tmpResult.timeAfter_ =
            teamScore[k].tr->tmpResult.runningTime_ - leadtime;
        }
      } else {
        teamScore[k].tr->tmpResult.place_     = 0;
        teamScore[k].tr->tmpResult.timeAfter_ = 0;
      }
    }
    if (sortTeams) {
      for (size_t k = 0; k < teamScore.size(); k++)
        teams[k] = (oTeam*)teamScore[k].tr;
    }
    lockPrepare--;
  } catch (...) {
    lockPrepare--;
    throw;
  }
}

// ---------------------------------------------------------------------------
// sortTeamMembers
// ---------------------------------------------------------------------------

void GeneralResult::sortTeamMembers(vector<oRunner*>& runners) const {
  vector<GRSortInfo> rs(runners.size());
  for (size_t k = 0; k < runners.size(); k++) {
    rs[k].score = runners[k]->tmpResult.internalScore_;
    rs[k].tr    = runners[k];
  }
  ::sort(rs.begin(), rs.end());
  for (size_t k = 0; k < runners.size(); k++)
    runners[k] = (oRunner*)rs[k].tr;
}

// ---------------------------------------------------------------------------
// sort template
// ---------------------------------------------------------------------------

template<class T> void GeneralResult::sort(vector<T*>& rt, SortOrder so) const {
  PrincipalSort ps = ClassWise;
  if (so == CourseResult || so == CourseStartTime)
    ps = CourseWise;
  else if (so == SortByName || so == SortByFinishTimeReverse ||
           so == SortByFinishTime || so == SortByStartTime)
    ps = None;

  const int maxT = timeConstHour * 100;
  vector<pair<int, oAbstractRunner*>> arr(rt.size());
  for (size_t k = 0; k < rt.size(); k++) {
    arr[k].first = 0;
    if (ps == ClassWise) {
      pClass cls = rt[k]->getClassRef(true);
      arr[k].first = cls ? cls->getSortIndex() : 0;
    } else if (ps == CourseWise) {
      oRunner* r = dynamic_cast<oRunner*>(rt[k]);
      arr[k].first = (r && r->getCourse(false)) ? r->getCourse(false)->getId() : 0;
    }
    arr[k].second = rt[k];
    int ord = 0;
    const oAbstractRunner::TempResult& tr = rt[k]->getTempResult(0);
    if (so == SortByFinishTime || so == ClassFinishTime) {
      ord = tr.getFinishTime();
      if (ord == 0 || tr.getStatus() > 1) ord = maxT;
    } else if (so == SortByFinishTimeReverse) {
      ord = tr.getFinishTime();
      if (ord == 0 || tr.getStatus() > 1) ord = maxT;
      else ord = maxT - ord;
    } else if (so == SortByStartTime || so == ClassStartTime ||
               so == ClassStartTimeClub || so == ClubClassStartTime) {
      ord = tr.getStartTime();
    }
    arr[k].first = arr[k].first * maxT * 10 + ord;
  }
  stable_sort(arr.begin(), arr.end());
  for (size_t k = 0; k < rt.size(); k++)
    rt[k] = (T*)arr[k].second;
}

// Explicit instantiations
template void GeneralResult::sort(vector<oRunner*>& rt, SortOrder so) const;
template void GeneralResult::sort(vector<oTeam*>&   rt, SortOrder so) const;

// ---------------------------------------------------------------------------
// calculateIndividualResults (instance method)
// ---------------------------------------------------------------------------

void GeneralResult::calculateIndividualResults(vector<oRunner*>& runners,
                                                bool classResult,
                                                oListInfo::ResultType resType,
                                                bool sortRunners,
                                                int inputNumber) const {
  if (runners.empty()) return;
  if (lockPrepare > 2) throw meosException("Bad cyclic call");
  lockPrepare++;
  try {
    set<int> clsId;
    for (pRunner r : runners) clsId.insert(r->getClassId(true));
    vector<pTeam> noTeams;
    prepareCalculations(*runners[0]->oe, classResult, clsId, runners, noTeams, inputNumber);

    vector<GRSortInfo> rs(runners.size());
    for (size_t k = 0; k < runners.size(); k++) {
      const oRunner* r = runners[k];
      if (resType == oListInfo::Classwise) {
        rs[k].principalSort = r->Class ?
          r->Class->getSortIndex() * 50000 + r->Class->getId() : 0;
      } else if (resType == oListInfo::Legwise) {
        rs[k].principalSort = r->Class ?
          r->Class->getSortIndex() * 50000 + r->Class->getId() : 0;
        int ln = r->getLegNumber();
        const oTeam* pt = r->getTeam();
        if (pt) {
          const oClass* tcls = pt->getClassRef(false);
          if (tcls && tcls->getClassType() == oClassRelay) {
            int dummy;
            tcls->splitLegNumberParallel(r->getLegNumber(), ln, dummy);
          }
        }
        rs[k].principalSort = rs[k].principalSort * 50 + ln;
      } else if (resType == oListInfo::Coursewise) {
        pCourse crs = r->getCourse(false);
        rs[k].principalSort = crs ? crs->getId() : 0;
      }

      int from = getListParamTimeFromControl();
      if (from <= 0) {
        runners[k]->tmpResult.startTime_ = r->getStartTime();
      } else {
        int rt; RunnerStatus stat;
        runners[k]->getSplitTime(from, stat, rt);
        runners[k]->tmpResult.startTime_ =
          (stat == StatusOK) ? runners[k]->getStartTime() + rt : runners[k]->getStartTime();
      }

      prepareCalculations(*runners[k], classResult);
      runners[k]->tmpResult.runningTime_ =
        deduceTime(*runners[k], runners[k]->tmpResult.startTime_);
      runners[k]->tmpResult.status_  = deduceStatus(*runners[k]);
      runners[k]->tmpResult.points_  = deducePoints(*runners[k]);
      rs[k].score = score(*runners[k], runners[k]->tmpResult.status_,
                           runners[k]->tmpResult.runningTime_,
                           runners[k]->tmpResult.points_, false);
      storeOutput(runners[k]->tmpResult.outputTimes_, runners[k]->tmpResult.outputNumbers_);
      rs[k].tr = runners[k];
    }

    ::sort(rs.begin(), rs.end());
    int place = 1, iPlace = 1, leadtime = 0;
    for (size_t k = 0; k < rs.size(); k++) {
      if (k > 0 && rs[k-1].principalSort != rs[k].principalSort) {
        place = 1; iPlace = 1;
      } else if (k > 0 && rs[k-1].score != rs[k].score) {
        place = iPlace;
      }
      if (rs[k].tr->tmpResult.status_ == StatusOK) {
        rs[k].tr->tmpResult.place_     = place;
        iPlace++;
        if (place == 1) {
          leadtime = rs[k].tr->tmpResult.runningTime_;
          rs[k].tr->tmpResult.timeAfter_ = 0;
        } else {
          rs[k].tr->tmpResult.timeAfter_ =
            rs[k].tr->tmpResult.runningTime_ - leadtime;
        }
      } else {
        rs[k].tr->tmpResult.place_     = 0;
        rs[k].tr->tmpResult.timeAfter_ = 0;
      }
    }
    if (sortRunners) {
      for (size_t k = 0; k < rs.size(); k++)
        runners[k] = (oRunner*)rs[k].tr;
    }
  } catch (...) {
    lockPrepare--;
    throw;
  }
  lockPrepare--;
}

// ---------------------------------------------------------------------------
// prepareCalculations (base implementations)
// ---------------------------------------------------------------------------

void GeneralResult::prepareCalculations(oEvent& /*oe*/, bool /*classResult*/,
                                         const set<int>& /*cls*/,
                                         vector<pRunner>& /*runners*/,
                                         vector<pTeam>& /*teams*/,
                                         int /*inputNumber*/) const {}

void GeneralResult::prepareCalculations(oTeam& team, bool classResult) const {
  int nr = team.getNumRunners();
  for (int j = 0; j < nr; j++) {
    pRunner r = team.getRunner(j);
    if (r) {
      prepareCalculations(*r, classResult);
      r->tmpResult.runningTime_    = deduceTime(*r, r->getStartTime());
      r->tmpResult.status_         = deduceStatus(*r);
      r->tmpResult.place_          = 0;
      r->tmpResult.timeAfter_      = 0;
      r->tmpResult.points_         = deducePoints(*r);
      r->tmpResult.internalScore_  = score(*r, r->tmpResult.status_,
                                            r->tmpResult.runningTime_,
                                            r->tmpResult.points_, true);
      storeOutput(r->tmpResult.outputTimes_, r->tmpResult.outputNumbers_);
    }
  }
}

void GeneralResult::prepareCalculations(oRunner& runner, bool /*classResult*/) const {
  int from = getListParamTimeFromControl();
  runner.tmpResult.startTime_ = runner.getStartTime();
  if (from > 0) {
    int rt; RunnerStatus stat;
    runner.getSplitTime(from, stat, rt);
    if (stat == StatusOK) runner.tmpResult.startTime_ += rt;
  }
}

void GeneralResult::storeOutput(vector<int>& /*times*/, vector<int>& /*numbers*/) const {}

// ---------------------------------------------------------------------------
// score / deduce* (base)
// ---------------------------------------------------------------------------

pair<int,int> GeneralResult::score(oTeam& team, RunnerStatus st, int rt, int points) const {
  return make_pair((100 * RunnerStatusOrderMap[st] + team.getNumShortening()) * 100000 - points,
                   rt);
}

RunnerStatus GeneralResult::deduceStatus(oTeam& team) const {
  return team.deduceComputedStatus();
}

int GeneralResult::deduceTime(oTeam& team) const {
  return team.deduceComputedRunningTime();
}

int GeneralResult::deducePoints(oTeam& team) const {
  return team.deduceComputedPoints();
}

pair<int,int> GeneralResult::score(oRunner& runner, RunnerStatus st, int time, int points,
                                    bool asTeamMember) const {
  if (asTeamMember)
    return make_pair(runner.getLegNumber(), 0);
  return make_pair((100 * RunnerStatusOrderMap[st] + runner.getNumShortening()) * 100000 - points,
                   time);
}

RunnerStatus GeneralResult::deduceStatus(oRunner& runner) const {
  return runner.getStatus();
}

int GeneralResult::deduceTime(oRunner& runner, int /*startTime*/) const {
  return runner.getRunningTime(false);
}

int GeneralResult::deducePoints(oRunner& runner) const {
  return runner.getRogainingPoints(false, false);
}

// ---------------------------------------------------------------------------
// Static calculateIndividualResults / calculateTeamResults
// ---------------------------------------------------------------------------

void GeneralResult::calculateIndividualResults(
    vector<pRunner>& runners,
    const pair<int,int>& controlId,
    bool totalResults,
    bool inclForestRunners,
    bool /*inclPreliminary*/,
    const string& resTag,
    oListInfo::ResultType resType,
    int inputNumber,
    oEvent& oe,
    vector<GeneralResultInfo>& results) {

  results.reserve(runners.size());

  // Simplified: only the standard full-course result path is implemented.
  // The control-split and dynamic-module paths will be completed in a later story.
  if (resTag.empty()) {
    GeneralResultInfo ri;
    if (!totalResults) {
      set<int> clsId;
      for (pRunner r : runners) clsId.insert(r->getClassId(true));
      oe.calculateResults(clsId, oEvent::ResultType::ClassResult);
      for (pRunner r : runners) {
        ri.status = r->getStatus();
        if (ri.status == StatusUnknown) {
          if (r->getFinishTime() == 0) {
            if (!inclForestRunners) continue;
            if (r->getClubId() == cVacantId) continue;
          } else {
            ri.status = StatusOK;
          }
        }
        ri.place = (ri.status == StatusOK) ? r->getPlace() : 0;
        ri.score = r->getRogainingPoints(true, false);
        ri.time  = r->getRunningTime(true);
        ri.src   = r;
        results.push_back(ri);
      }
    } else {
      oe.calculateResults(set<int>(), oEvent::ResultType::TotalResult);
      for (pRunner r : runners) {
        ri.status = r->getTotalStatus(false);
        if (ri.status == StatusUnknown) {
          if (r->getFinishTime() == 0) {
            if (!inclForestRunners) continue;
            if (r->getClubId() == cVacantId) continue;
          } else {
            ri.status = StatusOK;
          }
        }
        ri.place = (ri.status == StatusOK) ? r->getTotalPlace() : 0;
        ri.score = r->getRogainingPoints(true, true);
        ri.time  = r->getTotalRunningTime();
        ri.src   = r;
        results.push_back(ri);
      }
    }
  } else {
    // Dynamic-result module path (stub — no resTag lookup yet)
    oListParam param;
    param.useControlIdResultTo   = controlId.second;
    param.useControlIdResultFrom = controlId.first;

    shared_ptr<GeneralResult> specialInstance;
    if (controlId.second == oPunch::PunchFinish && controlId.first == oPunch::PunchStart
        && !totalResults)
      specialInstance = make_shared<GeneralResult>();
    else if (!totalResults)
      specialInstance = make_shared<ResultAtControl>();
    else
      specialInstance = make_shared<TotalResultAtControl>();

    specialInstance->setContext(&param);
    specialInstance->calculateIndividualResults(runners, false, resType, true, inputNumber);
    specialInstance->clearContext();

    GeneralResultInfo ri;
    for (pRunner r : runners) {
      const auto& tmp = r->getTempResult(0);
      ri.status = tmp.getStatus();
      if (ri.status == StatusUnknown) {
        if (r->getFinishTime() == 0) {
          if (!inclForestRunners) continue;
        } else {
          ri.status = StatusOK;
        }
      }
      ri.place  = (ri.status == StatusOK) ? tmp.getPlace() : 0;
      ri.score  = tmp.getPoints();
      ri.time   = tmp.getRunningTime();
      ri.src    = r;
      results.push_back(ri);
    }
  }
}

shared_ptr<GeneralResult::BaseResultContext>
GeneralResult::calculateTeamResults(
    vector<pTeam>& teams,
    int leg,
    const pair<int,int>& controlId,
    bool totalResults,
    const string& /*resTag*/,
    oListInfo::ResultType resType,
    int inputNumber,
    oEvent& oe,
    vector<GeneralResultInfo>& results) {

  auto out = make_shared<BaseResultContext>();
  out->leg          = leg;
  out->controlId    = controlId;
  out->totalResults = totalResults;
  out->useModule    = false;
  results.reserve(teams.size());

  if (controlId.second == oPunch::PunchFinish) {
    oe.calculateTeamResults(teams, oEvent::ResultType::ClassResult);
    GeneralResultInfo ri;
    for (pTeam t : teams) {
      ri.status = t->getStatus();
      if (ri.status == StatusUnknown) {
        if (t->getFinishTime() == 0) continue;
        ri.status = StatusOK;
      }
      ri.place = (ri.status == StatusOK) ? t->getPlace() : 0;
      ri.score = t->getRogainingPoints(true, false);
      ri.time  = t->getRunningTime(true);
      ri.src   = t;
      results.push_back(ri);
    }
  }
  return out;
}

// ---------------------------------------------------------------------------
// GeneralResultInfo::getNumSubresult / getSubResult
// ---------------------------------------------------------------------------

int GeneralResult::GeneralResultInfo::getNumSubresult(const BaseResultContext& context) const {
  int cid = src->getClassId(false);
  if (cid == 0) return 0;

  if (context.resIntervalCache.count(cid)) {
    const auto& cached = context.resIntervalCache[cid];
    return cached.second - cached.first;
  }
  const pClass cls = src->getClassRef(false);
  if (!cls) return 0;
  int leg = (context.leg == -1) ? cls->getNumStages() : context.leg;
  int nb  = cls->getNextBaseLeg(leg);
  if (nb == -1) nb = cls->getNumStages();
  int pb  = cls->getPreceedingLeg(leg) + 1;
  context.resIntervalCache[cid] = make_pair(pb, nb);
  return nb - pb;
}

bool GeneralResult::GeneralResultInfo::getSubResult(const BaseResultContext& context,
                                                      int ix,
                                                      GeneralResultInfo& out) const {
  if (getNumSubresult(context) == 0) return false;
  int base    = context.resIntervalCache[src->getClassId(false)].first;
  pRunner r   = pTeam(src)->getRunner(base + ix);
  if (!r) return false;
  out.place  = 0;
  out.src    = r;
  if (context.useModule) {
    out.score  = r->tmpResult.getPoints();
    out.status = r->tmpResult.getStatus();
    out.time   = r->tmpResult.getRunningTime();
  } else {
    if (context.controlId.second == oPunch::PunchFinish) {
      out.score  = r->getRogainingPoints(true, false);
      out.status = r->getStatus();
      out.time   = r->getRunningTime(true);
    } else {
      out.score = 0;
      r->getSplitTime(context.controlId.second, out.status, out.time);
    }
  }
  if (out.status == StatusUnknown && out.time > 0) out.status = StatusOK;
  return out.status != StatusUnknown;
}

// ===========================================================================
// ResultAtControl
// ===========================================================================

pair<int,int> ResultAtControl::score(oTeam& t, RunnerStatus st, int time, int points) const {
  return GeneralResult::score(t, st, time, points);
}

RunnerStatus ResultAtControl::deduceStatus(oTeam& t) const {
  return GeneralResult::deduceStatus(t);
}

int ResultAtControl::deduceTime(oTeam& t) const {
  return GeneralResult::deduceTime(t);
}

int ResultAtControl::deducePoints(oTeam& t) const {
  return GeneralResult::deducePoints(t);
}

pair<int,int> ResultAtControl::score(oRunner& runner, RunnerStatus st, int time, int points,
                                      bool asTeamMember) const {
  if (asTeamMember) return make_pair(runner.getLegNumber(), 0);
  return make_pair(RunnerStatusOrderMap[st], time);
}

RunnerStatus ResultAtControl::deduceStatus(oRunner& runner) const {
  int fc = getListParamTimeToControl();
  if (fc > 0) {
    RunnerStatus stat; int rt;
    runner.getSplitTime(fc, stat, rt);
    return stat;
  }
  RunnerStatus st = runner.getStatus();
  if (st == StatusUnknown && runner.getRunningTime(false) > 0) return StatusOK;
  return st;
}

int ResultAtControl::deduceTime(oRunner& runner, int startTime) const {
  int fc = getListParamTimeToControl();
  if (fc > 0) {
    RunnerStatus stat; int rt;
    runner.getSplitTime(fc, stat, rt);
    if (stat == StatusOK) return runner.getStartTime() + rt - startTime;
  } else if (runner.getFinishTime() > 0) {
    return runner.getFinishTime() - startTime;
  }
  return 0;
}

int ResultAtControl::deducePoints(oRunner& /*runner*/) const { return 0; }

// ===========================================================================
// TotalResultAtControl
// ===========================================================================

RunnerStatus TotalResultAtControl::deduceStatus(oRunner& runner) const {
  RunnerStatus singleStat = ResultAtControl::deduceStatus(runner);
  if (singleStat != StatusOK) return singleStat;

  RunnerStatus inputStatus = StatusOK;
  if (runner.getTeam() && getListParamTimeFromControl() <= 0) {
    const pTeam t = runner.getTeam();
    if (runner.getLegNumber() > 0 && t->getClassRef(false)) {
      int legIx = runner.getLegNumber();
      const pClass cls = t->getClassRef(false);
      while (legIx > 0 && (cls->isParallel(legIx) || cls->isOptional(legIx))) legIx--;
      if (legIx > 0)
        inputStatus = t->getLegStatus(legIx - 1, false, true);
      else
        inputStatus = t->getInputStatus();
    } else {
      inputStatus = t->getInputStatus();
    }
  } else {
    inputStatus = runner.getInputStatus();
  }
  return inputStatus;
}

int TotalResultAtControl::deduceTime(oRunner& runner, int startTime) const {
  int singleTime = ResultAtControl::deduceTime(runner, startTime);
  if (singleTime == 0) return 0;

  int inputTime = 0;
  if (runner.getTeam() && getListParamTimeFromControl() <= 0) {
    const pTeam t = runner.getTeam();
    if (runner.getLegNumber() > 0 && t->getClassRef(false)) {
      int legIx = runner.getLegNumber();
      const pClass cls = t->getClassRef(false);
      while (legIx > 0 && (cls->isParallel(legIx) || cls->isOptional(legIx))) legIx--;
      if (legIx > 0)
        inputTime = t->getLegRunningTime(legIx - 1, false, true);
      else
        inputTime = t->getInputTime();
    } else {
      inputTime = t->getInputTime();
    }
  } else {
    inputTime = runner.getInputTime();
  }
  return singleTime + inputTime;
}

pair<int,int> TotalResultAtControl::score(oRunner& runner, RunnerStatus st, int time, int points,
                                           bool asTeamMember) const {
  if (asTeamMember) return make_pair(runner.getLegNumber(), 0);
  const int TK = timeConstHour * 24 * 7;
  RunnerStatus inputStatus = StatusOK;
  if (runner.getTeam()) {
    const pTeam t = runner.getTeam();
    if (runner.getLegNumber() > 0) {
      inputStatus = t->getLegStatus(runner.getLegNumber() - 1, false, true);
    } else {
      inputStatus = t->getInputStatus();
    }
  } else {
    inputStatus = runner.getInputStatus();
  }
  if (st != StatusUnknown)
    st = (RunnerStatus)std::max((int)inputStatus, (int)st);
  return make_pair(RunnerStatusOrderMap[st], time);
}

// ===========================================================================
// DynamicResult — stub implementation (Parser is not yet implemented)
// ===========================================================================

int DynamicResult::instanceCount = 0;
map<string, DynamicResult::DynamicMethods> DynamicResult::symb2Method;
map<DynamicResult::DynamicMethods, pair<string,string>> DynamicResult::method2SymbName;

DynamicResult::DynamicResult() {
  builtIn    = false;
  readOnly   = false;
  isCompiled = false;
  methods.resize(_Mlast);
  instanceCount++;
  if (method2SymbName.empty()) {
    addSymbol(MDeduceRStatus, "RunnerStatus",  "Status calculation for runner");
    addSymbol(MDeduceRTime,   "RunnerTime",    "Time calculation for runner");
    addSymbol(MDeduceRPoints, "RunnerPoints",  "Point calculation for runner");
    addSymbol(MRScore,        "RunnerScore",   "Result score calculation for runner");
    addSymbol(MDeduceTStatus, "TeamStatus",    "Status calculation for team");
    addSymbol(MDeduceTTime,   "TeamTime",      "Time calculation for team");
    addSymbol(MDeduceTPoints, "TeamPoints",    "Point calculation for team");
    addSymbol(MTScore,        "TeamScore",     "Result score calculation for team");
  }
}

DynamicResult::DynamicResult(const DynamicResult& src) {
  instanceCount++;
  isCompiled  = false;
  name        = src.name;
  tag         = src.tag;
  readOnly    = false;
  description = src.description;
  origin      = src.origin;
  timeStamp   = src.timeStamp;
  annotation  = src.annotation;
  builtIn     = src.builtIn;
  methods.resize(_Mlast);
  for (size_t k = 0; k < methods.size(); k++) {
    methods[k].source      = src.methods[k].source;
    methods[k].description = src.methods[k].description;
  }
}

void DynamicResult::operator=(const DynamicResult& src) {
  clear();
  isCompiled  = false;
  name        = src.name;
  tag         = src.tag;
  description = src.description;
  origin      = src.origin;
  timeStamp   = src.timeStamp;
  annotation  = src.annotation;
  readOnly    = src.readOnly;
  builtIn     = src.builtIn;
  methods.resize(_Mlast);
  for (size_t k = 0; k < methods.size(); k++) {
    methods[k].source      = src.methods[k].source;
    methods[k].description = src.methods[k].description;
    methods[k].pn          = nullptr;
  }
}

void DynamicResult::addSymbol(DynamicMethods method, const char* symb, const char* nm) {
  if (method2SymbName.count(method) || symb2Method.count(symb))
    throw meosException("Method symbol used");
  method2SymbName[method] = make_pair(symb, nm);
  symb2Method[symb]       = method;
}

DynamicResult::~DynamicResult() {
  instanceCount--;
  if (instanceCount == 0) {
    method2SymbName.clear();
    symb2Method.clear();
  }
}

const ParseNode* DynamicResult::getMethod(DynamicMethods method) const {
  return methods[method].pn;
}

const string& DynamicResult::getMethodSource(DynamicMethods method) const {
  return methods[method].source;
}

void DynamicResult::setMethodSource(DynamicMethods method, const string& source) {
  methods[method].source = source;
  methods[method].pn     = nullptr;
  // With stub Parser, parse() always returns nullptr.
  methods[method].pn = parser.parse(source);
}

RunnerStatus DynamicResult::toStatus(int status) const {
  switch (status) {
  case StatusUnknown:        return StatusUnknown;
  case StatusOK:             return StatusOK;
  case StatusMP:             return StatusMP;
  case StatusDNF:            return StatusDNF;
  case StatusDNS:            return StatusDNS;
  case StatusCANCEL:         return StatusCANCEL;
  case StatusNotCompeting:   return StatusNotCompeting;
  case StatusDQ:             return StatusDQ;
  case StatusMAX:            return StatusMAX;
  case StatusOutOfCompetition: return StatusOutOfCompetition;
  case StatusNoTiming:       return StatusNoTiming;
  default:
    throw meosException("Unknown status code X#" + itos(status));
  }
}

pair<int,int> DynamicResult::score(oTeam& team, RunnerStatus st, int time, int points) const {
  if (getMethod(MTScore)) {
    // Stub: parser not implemented yet
  }
  if (getMethodSource(MTScore).empty())
    return GeneralResult::score(team, st, time, points);
  throw meosException("Syntax error");
}

RunnerStatus DynamicResult::deduceStatus(oTeam& team) const {
  if (getMethod(MDeduceTStatus)) { }
  if (getMethodSource(MDeduceTStatus).empty())
    return GeneralResult::deduceStatus(team);
  throw meosException("Syntax error");
}

int DynamicResult::deduceTime(oTeam& team) const {
  if (getMethod(MDeduceTTime)) { }
  if (getMethodSource(MDeduceTTime).empty())
    return GeneralResult::deduceTime(team);
  throw meosException("Syntax error");
}

int DynamicResult::deducePoints(oTeam& team) const {
  if (getMethod(MDeduceTPoints)) { }
  if (getMethodSource(MDeduceTPoints).empty())
    return GeneralResult::deducePoints(team);
  throw meosException("Syntax error");
}

pair<int,int> DynamicResult::score(oRunner& runner, RunnerStatus st, int time, int points,
                                    bool asTeamMember) const {
  if (getMethod(MRScore)) { }
  if (getMethodSource(MRScore).empty())
    return GeneralResult::score(runner, st, time, points, asTeamMember);
  throw meosException("Syntax error");
}

RunnerStatus DynamicResult::deduceStatus(oRunner& runner) const {
  if (getMethod(MDeduceRStatus)) { }
  if (getMethodSource(MDeduceRStatus).empty())
    return GeneralResult::deduceStatus(runner);
  throw meosException("Syntax error");
}

int DynamicResult::deduceTime(oRunner& runner, int startTime) const {
  if (getMethod(MDeduceRTime)) { }
  if (getMethodSource(MDeduceRTime).empty())
    return GeneralResult::deduceTime(runner, startTime);
  throw meosException("Syntax error");
}

int DynamicResult::deducePoints(oRunner& runner) const {
  if (getMethod(MDeduceRPoints)) { }
  if (getMethodSource(MDeduceRPoints).empty())
    return GeneralResult::deducePoints(runner);
  throw meosException("Syntax error");
}

void DynamicResult::clear() {
  parser.clear();
  for (auto& m : methods) {
    m.pn = nullptr;
    m.source.clear();
    m.description.clear();
  }
}

void DynamicResult::compile(bool /*forceRecompile*/) const {
  // Stub: no-op until full Parser is migrated.
  isCompiled = true;
}

void DynamicResult::prepareCommon(oAbstractRunner& runner, bool classResult) const {
  bool useComputed = !classResult;
  parser.clearVariables();
  parser.addSymbol("Status",   (int)runner.getStatus());
  parser.addSymbol("Start",    runner.getStartTime() / timeConstSecond);
  parser.addSymbol("Finish",   runner.getFinishTime() / timeConstSecond);
  parser.addSymbol("Time",     runner.getRunningTime(useComputed) / timeConstSecond);
  parser.addSymbol("Place",    runner.getPlace(false));
  parser.addSymbol("Points",   runner.getRogainingPoints(useComputed, false));
  parser.addSymbol("Shorten",  runner.getNumShortening());
  // Additional symbols omitted (stub)
}

void DynamicResult::prepareCalculations(oEvent& oe, bool classResult,
                                         const set<int>& clsSelection,
                                         vector<pRunner>& runners,
                                         vector<pTeam>& teams,
                                         int inputNumber) const {
  compile(false);
  declareSymbols(MRScore, true);
  if (!teams.empty()) {
    declareSymbols(MTScore, false);
    for (pTeam t : teams) t->resetResultCalcCache();
  }
}

void DynamicResult::prepareCalculations(oTeam& team, bool classResult) const {
  GeneralResult::prepareCalculations(team, classResult);
  prepareCommon(team, classResult);

  int nr = team.getNumRunners();
  vector<int> status(nr), time(nr), start(nr), finish(nr), points(nr);

  for (int k = 0; k < nr; k++) {
    pRunner r = team.getRunner(k);
    if (r) {
      const oAbstractRunner::TempResult& res = r->getTempResult();
      status[k] = res.getStatus();
      time[k]   = res.getRunningTime() / timeConstSecond;
      start[k]  = res.getStartTime()   / timeConstSecond;
      finish[k] = res.getFinishTime()  / timeConstSecond;
      points[k] = res.getPoints();
      if (classResult) r->updateComputedResultFromTemp();
      // outputTimes_/outputNumbers_ are private and unused with stub parser
    }
  }
  parser.addSymbol("RunnerStatus", status);
  parser.addSymbol("RunnerTime",   time);
  parser.addSymbol("RunnerStart",  start);
  parser.addSymbol("RunnerFinish", finish);
  parser.addSymbol("RunnerPoints", points);
}

void DynamicResult::prepareCalculations(oRunner& runner, bool classResult) const {
  GeneralResult::prepareCalculations(runner, classResult);
  prepareCommon(runner, classResult);

  vector<int> delta, place, after;
  runner.getSplitAnalysis(delta);
  runner.getLegTimeAfter(after);
  runner.getLegPlaces(place);
  parser.addSymbol("LegTimeDeviation", delta);
  parser.addSymbol("LegTimeAfter",     after);
  parser.addSymbol("LegPlace",         place);
  parser.addSymbol("Leg",              runner.getLegNumber());
  parser.addSymbol("BirthYear",        runner.getBirthYear());
  parser.addSymbol("Age",              runner.getBirthAge());
  parser.addSymbol("CheckTime",        runner.getCheckTime() / timeConstSecond);
}

void DynamicResult::storeOutput(vector<int>& times, vector<int>& numbers) const {
  parser.takeVariable("OutputTimes",   times);
  parser.takeVariable("OutputNumbers", numbers);
}

void DynamicResult::declareSymbols(DynamicMethods /*m*/, bool clear) const {
  if (clear) parser.clearSymbols();
  // Stub: symbol declarations omitted until Parser is fully migrated.
}

long long DynamicResult::getHashCode() const {
  long long hc = 1;
  for (const auto& m : methods) {
    long long cs = 0;
    for (char c : m.source) cs = cs * 31 + c;
    hc = hc * 997 + cs;
  }
  return hc;
}

string DynamicResult::undecorateTag(const string& inputTag) {
  size_t ix = inputTag.rfind("-v");
  size_t limit = inputTag.size() >= 4 ? inputTag.size() - 4 : 0;
  if (ix != string::npos && ix > 0 && ix >= limit)
    return inputTag.substr(0, ix);
  return inputTag;
}

bool DynamicResult::isRogaining() const {
  return !methods[MDeduceRPoints].source.empty();
}

wstring DynamicResult::getName(bool withAnnotation) const {
  if (annotation.empty() || !withAnnotation) return name;
  return name + L" (" + annotation + L")";
}

void DynamicResult::getMethodTypes(vector<pair<DynamicMethods,string>>& mt) const {
  mt.clear();
  for (const auto& kv : method2SymbName)
    mt.push_back(make_pair(kv.first, kv.second.second));
}

void DynamicResult::getSymbols(vector<pair<wstring, size_t>>& symb) const {
  parser.getSymbols(symb);
}

void DynamicResult::getSymbolInfo(int ix, wstring& nm, wstring& desc) const {
  parser.getSymbolInfo(ix, nm, desc);
}

#include "oTeam.h"
#include "oEvent.h"
#include "oClass.h"
#include "oRunner.h"
#include "oDataContainer.h"
#include <set>
#include <algorithm>
#include <cassert>
#include <limits>
#include <sstream>

using namespace std;

// ---------------------------------------------------------------------------
// Static DataContainer
// ---------------------------------------------------------------------------

oDataContainer& oTeam::container() {
  static oDataContainer dc(256);
  static bool init = false;
  if (!init) {
    init = true;
    dc.addVariableInt("Fee",          oDataContainer::oIS32,         "Anm. avgift");
    dc.addVariableInt("Paid",         oDataContainer::oIS32,         "Betalat");
    dc.addVariableInt("PayMode",      oDataContainer::oIS8,          "Betalsätt");
    dc.addVariableInt("Taxable",      oDataContainer::oIS32,         "Skattad avgift");
    dc.addVariableInt("EntryDate",    oDataContainer::oIS32,         "Anm. datum");
    dc.addVariableInt("EntryTime",    oDataContainer::oISTime,       "Anm. tid");
    dc.addVariableString("Nationality", 3,                            "Nationalitet");
    dc.addVariableString("Country",   23,                             "Land");
    dc.addVariableString("Bib",       8,                              "Nummerlapp");
    dc.addVariableInt("ExtId",        oDataContainer::oIS64,         "Externt Id");
    dc.addVariableInt("Priority",     oDataContainer::oIS8,          "Prioritering");
    dc.addVariableInt("SortIndex",    oDataContainer::oIS16,         "Sortering");
    dc.addVariableInt("TimeAdjust",   oDataContainer::oISTimeAdjust, "Tidsjustering");
    dc.addVariableInt("PointAdjust",  oDataContainer::oIS32,         "Poängjustering");
    dc.addVariableInt("TransferFlags",oDataContainer::oIS32,         "Överföring");
    dc.addVariableInt("EntrySource",  oDataContainer::oIS32,         "Källa");
    dc.addVariableInt("Heat",         oDataContainer::oIS8,          "Heat");
    dc.addVariableInt("NoRestart",    oDataContainer::oIS8,          "Ej omstart");
    dc.addVariableString("InputResult", "Tidigare resultat");
  }
  return dc;
}

oDataContainer& oTeam::getDataBuffers(pvoid& data, pvoid& olddata, pvectorstr& strData) const {
  data    = &dataMap_;
  olddata = &oldDataMap_;
  strData = nullptr;
  return container();
}

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------

oTeam::oTeam(oEvent* poe) : oAbstractRunner(poe, false) {
  Id = oe->getFreeTeamId();
  getDI().initData();
  StartNo = 0;
}

oTeam::oTeam(oEvent* poe, int id) : oAbstractRunner(poe, true) {
  Id = id;
  getDI().initData();
  StartNo = 0;
}

// ---------------------------------------------------------------------------
// changedObject
// ---------------------------------------------------------------------------

void oTeam::changedObject() {
  if (oe) oe->sqlTeams.changed = true;
}

// ---------------------------------------------------------------------------
// remove / canRemove / merge
// ---------------------------------------------------------------------------

void oTeam::remove() {
  Removed = true;
  changedObject();
}

bool oTeam::canRemove() const { return true; }

void oTeam::merge(const oBase& /*input*/, const oBase* /*base*/) {}

// ---------------------------------------------------------------------------
// Club
// ---------------------------------------------------------------------------

void oTeam::setClub(const wstring& name) {
  pClub c = oe->getClub(name);
  if (c != Club) {
    Club = c;
    updateChanged();
  }
}

pClub oTeam::setClubId(int clubId) {
  pClub c = oe->getClub(clubId);
  if (c != Club) {
    Club = c;
    propagateClub();
    updateChanged();
  }
  return Club;
}

void oTeam::propagateClub() {
  for (auto r : Runners) {
    if (r && r->getClubId() == 0)
      r->setClubId(Club ? Club->getId() : 0);
  }
}

// ---------------------------------------------------------------------------
// Runner management
// ---------------------------------------------------------------------------

pRunner oTeam::getRunner(unsigned leg) const {
  if (leg < Runners.size()) return Runners[leg];
  return nullptr;
}

int oTeam::getNumDistinctRunners() const {
  std::set<int> seen;
  for (auto r : Runners)
    if (r) seen.insert(r->getId());
  return (int)seen.size();
}

bool oTeam::isRunnerUsed(int rId) const {
  for (auto r : Runners)
    if (r && r->getId() == rId) return true;
  return false;
}

bool oTeam::isTeamMemberFor(const pRunner r) const {
  for (auto rr : Runners)
    if (rr == r) return true;
  return false;
}

void oTeam::correctRemove(pRunner r) {
  for (size_t i = 0; i < Runners.size(); i++) {
    if (r && Runners[i] == r) {
      Runners[i] = nullptr;
      r->tInTeam = nullptr;
      r->tLeg = 0;
      correctionNeeded = true;
      r->correctionNeeded = true;
    }
  }
}

void oTeam::setRunnerInternal(int k, pRunner r) {
  if (r == Runners[k]) {
    if (r) { r->tInTeam = this; r->tLeg = k; }
    return;
  }
  pRunner rOld = Runners[k];
  if (rOld) {
    assert(rOld->tInTeam == nullptr || rOld->tInTeam == this);
    rOld->tInTeam = nullptr;
    rOld->tLeg = 0;
  }
  if (r && r->tInTeam) {
    size_t leg = (size_t)r->tLeg;
    if (r->tInTeam->Runners.size() > leg && r->tInTeam->Runners[leg] == r) {
      r->tInTeam->Runners[leg] = nullptr;
      r->tInTeam->updateChanged();
    }
  }
  Runners[k] = r;
  if (r) {
    r->tInTeam = this;
    r->tLeg = k;
    if (Class && Class->getLegType(k) != LTGroup)
      r->setClassId(getClassId(false), false);
  }
  updateChanged();
}

void oTeam::setRunner(unsigned i, pRunner r, bool sync) {
  if (i >= Runners.size()) {
    if (i < 100)
      Runners.resize(i + 1);
    else
      throw std::runtime_error("Bad runner index");
  }
  if (Runners[i] == r) return;

  int oldRaceId = 0;
  pRunner tr = Runners[i];
  if (tr) {
    oldRaceId = tr->getDCI().getInt("RaceId");
    tr->getDI().setInt("RaceId", 0);
  }
  setRunnerInternal((int)i, r);

  if (r) {
    if (tStatus == StatusDNS)
      setStatus(StatusUnknown, true, ChangeType::Update);
    r->getDI().setInt("RaceId", oldRaceId);
    r->tInTeam = this;
    r->tLeg = i;
    r->createMultiRunner(true, sync);
  }

  if (Class) {
    int index = 1;
    for (unsigned k = i + 1; k < (unsigned)Class->getNumStages(); k++) {
      if ((unsigned)Class->getLegRunner(k) == i) {
        if (r) {
          pRunner mr = r->getMultiRunner(index++);
          if (mr) {
            mr->setName(r->getName(), true);
            setRunnerInternal(k, mr);
          }
        } else {
          setRunnerInternal(k, nullptr);
        }
      }
    }
  }
}

void oTeam::importRunners(const vector<int>& rids) {
  Runners.resize(rids.size());
  for (size_t n = 0; n < rids.size(); n++) {
    Runners[n] = rids[n] > 0 ? oe->getRunner(rids[n], 0) : nullptr;
    if (Runners[n]) {
      Runners[n]->tInTeam = this;
      Runners[n]->tLeg = (int)n;
    }
  }
}

void oTeam::importRunners(const vector<pRunner>& rns) {
  for (size_t k = 0; k < Runners.size(); k++) {
    pRunner r = Runners[k];
    if (r && r->tInTeam == this) { r->tInTeam = nullptr; r->tLeg = 0; }
  }
  Runners.resize(rns.size());
  for (size_t n = 0; n < rns.size(); n++) {
    Runners[n] = rns[n];
    if (rns[n] && isAddedToEvent()) {
      rns[n]->tInTeam = this;
      rns[n]->tLeg = (int)n;
    }
  }
}

string oTeam::getRunners() const {
  string str;
  char bf[16];
  for (size_t m = 0; m < Runners.size(); m++) {
    if (Runners[m])
      snprintf(bf, 16, "%d;", Runners[m]->getId());
    else
      snprintf(bf, 16, "0;");
    str += bf;
  }
  return str;
}

wstring oTeam::getRunnerIdString() const {
  string s = getRunners();
  return wstring(s.begin(), s.end());
}

void oTeam::decodeRunners(const string& rns, vector<int>& rid) const {
  rid.clear();
  const char* str = rns.c_str();
  while (*str) {
    int cid = atoi(str);
    while (*str && *str != ';' && *str != ',') str++;
    if (*str == ';' || *str == ',') str++;
    rid.push_back(cid);
  }
}

// ---------------------------------------------------------------------------
// quickApply — link runners to team, no start time calculation
// ---------------------------------------------------------------------------

void oTeam::quickApply() {
  if ((unsigned)status >= 100) status = StatusUnknown;

  if (Class && Runners.size() != (size_t)Class->getNumStages()) {
    for (size_t k = (size_t)Class->getNumStages(); k < Runners.size(); k++) {
      pRunner tr = Runners[k];
      if (tr && tr->tInTeam) {
        tr->tInTeam = nullptr; tr->tLeg = 0; tr->tLegEquClass = 0;
        if (tr->Class == Class) tr->Class = nullptr;
        oe->classIdToRunnerHash.reset();
        tr->updateChanged();
      }
    }
    Runners.resize(Class->getNumStages());
  }

  for (size_t i = 0; i < Runners.size(); i++) {
    if (Runners[i]) {
      if (Runners[i]->isRemoved()) {
        Runners[i]->tInTeam = nullptr; Runners[i]->tLeg = 0; Runners[i] = nullptr; continue;
      }
      if (Runners[i]->tInTeam && Runners[i]->tInTeam != this)
        Runners[i]->tInTeam->correctRemove(Runners[i]);
      Runners[i]->tInTeam = this;
      Runners[i]->tLeg = (int)i;
    }
  }
}

// ---------------------------------------------------------------------------
// apply — propagate start times and link runners
// ---------------------------------------------------------------------------

namespace {
static void compressStartTimes(vector<int>& v, int ft) {
  for (auto& t : v) ft = max(ft, t);
  v = {ft};
}
static void addStartTime(vector<int>& v, int ft) {
  for (size_t k = 0; k < v.size(); k++) {
    if (ft >= v[k]) { v.insert(v.begin() + k, ft); return; }
  }
  v.push_back(ft);
}
static int getBestStartTime(vector<int>& v) {
  if (v.empty()) return 0;
  int t = v.back(); v.pop_back(); return t;
}
} // anonymous

void oTeam::apply(ChangeType changeType, pRunner source) {
  if ((unsigned)status >= 100) status = StatusUnknown;

  int lastStartTime = 0;
  RunnerStatus lastStatus = StatusUnknown;
  bool freeStart = Class ? Class->hasFreeStart() : false;
  int extraFinishTime = -1;

  if (Class && Runners.size() != (size_t)Class->getNumStages()) {
    for (size_t k = (size_t)Class->getNumStages(); k < Runners.size(); k++) {
      auto tr = Runners[k];
      if (tr && tr->tInTeam) {
        tr->tInTeam = nullptr; tr->tLeg = 0; tr->tLegEquClass = 0;
        if (tr->Class == Class) tr->Class = nullptr;
        oe->classIdToRunnerHash.reset();
        if (changeType == ChangeType::Update) tr->updateChanged();
      }
    }
    Runners.resize(Class->getNumStages());
  }

  tNumRestarts = 0;
  vector<int> availableStartTimes;

  for (size_t i = 0; i < Runners.size(); i++) {
    if (Runners[i] && Runners[i]->isRemoved()) {
      Runners[i]->tInTeam = nullptr; Runners[i]->tLeg = 0; Runners[i] = nullptr;
    }
    if (changeType == ChangeType::Quiet && i > 0 && source && Runners[i - 1] == source)
      return;

    if (!Runners[i] && Class) {
      unsigned lr = (unsigned)Class->getLegRunner((int)i);
      if (lr < i && Runners[lr]) {
        Runners[lr]->createMultiRunner(false, false);
        int dup = Class->getLegRunnerIndex((int)i);
        Runners[i] = Runners[lr]->getMultiRunner(dup);
      }
    }

    if (changeType == ChangeType::Update && Runners[i] && Class) {
      unsigned lr = (unsigned)Class->getLegRunner((int)i);
      if (lr == i && Runners[i]->tParentRunner) {
        pRunner parent = Runners[i]->tParentRunner;
        for (size_t kk = 0; kk < parent->multiRunner.size(); ++kk) {
          if (Runners[i] == parent->multiRunner[kk]) {
            pRunner tr = Runners[i];
            parent->multiRunner.erase(parent->multiRunner.begin() + kk);
            tr->tParentRunner = nullptr; tr->tDuplicateLeg = 0;
            parent->markForCorrection(); parent->updateChanged();
            tr->markForCorrection(); tr->updateChanged();
            break;
          }
        }
      }
    }

    // Dedup: remove if same runner appears in earlier leg
    if (Runners[i]) {
      for (size_t k = 0; k < i; k++)
        if (Runners[i] == Runners[k]) { Runners[i] = nullptr; break; }
    }

    if (!Runners[i]) {
      if (Class) {
        LegTypes lt = Class->getLegType((int)i);
        StartTypes st = Class->getStartType((int)i);
        if (lt == LTIgnore || lt == LTExtra) {
          // keep lastStartTime unchanged
        } else if (st == STChange || st == STPursuit) {
          // Can't compute without runner; carry lastStartTime
        } else if (st == STTime) {
          lastStartTime = Class->getStartData((int)i);
        }
      }
      continue;
    }

    pRunner tr = Runners[i];
    pClass actualClass = tr->getClassRef(true);
    if (!actualClass) actualClass = Class;

    if (tr->tInTeam && tr->tInTeam != this)
      tr->tInTeam->correctRemove(tr);
    tr->tInTeam = this;
    tr->tLeg = (int)i;

    if (Class) {
      int unused;
      Class->splitLegNumberParallel((int)i, tr->tLegEquClass, unused);
    } else {
      tr->tLegEquClass = (int)i;
    }

    if (actualClass == Class)
      tr->setStartNo(StartNo, changeType);

    LegTypes legType = Class ? Class->getLegType((int)i) : LTIgnore;
    if (tr->Class != Class && legType != LTGroup) {
      tr->Class = Class;
      oe->classIdToRunnerHash.reset();
      tr->updateChanged();
    }

    tr->tNeedNoCard = false;
    if (!Class) {
      int dt = max(tr->getFinishTime() - tStartTime, 0);
      if (i > 0)
        dt = max(dt, getLegRunningTimeUnadjusted((int)i - 1, false, false) +
                     tr->getRunningTime(false));
      lastStartTime = tr->getStartTime();
      continue;
    }

    pClass pc = Class;

    if (legType == LTIgnore) {
      tr->tNeedNoCard = true;
      if (lastStatus != StatusUnknown)
        tr->setStatus(max(tr->tStatus, lastStatus), false, changeType);
    } else {
      lastStatus = tr->getStatus();
    }

    StartTypes st = (actualClass == pc) ? pc->getStartType((int)i) : actualClass->getStartType(0);
    LegTypes lt = legType;

    if (lt == LTIgnore || lt == LTExtra) {
      if (st != STDrawn) tr->setStartTime(lastStartTime, false, changeType);
      tr->tUseStartPunch = (st == STDrawn);
    } else {
      switch (st) {
        case STDrawn:
          if (lt == LTParallel || lt == LTParallelOptional) {
            tr->setStartTime(lastStartTime, false, changeType);
            tr->tUseStartPunch = false;
          } else {
            lastStartTime = tr->getStartTime();
          }
          break;

        case STTime: {
          bool prs = false;
          if (tr->Card && freeStart) {
            pCourse crs = tr->getCourse(false);
            int startType = crs ? crs->getStartPunchType() : oPunch::PunchStart;
            oPunch* pnc = tr->Card->getPunchByType(startType);
            if (pnc && pnc->getAdjustedTime() > 0) {
              prs = true; lastStartTime = pnc->getAdjustedTime();
            }
          }
          if (!prs) {
            if (lt == LTNormal || lt == LTSum || lt == LTGroup) {
              lastStartTime = (actualClass == pc) ? pc->getStartData((int)i) : actualClass->getStartData(0);
            }
            tr->setStartTime(lastStartTime, false, changeType);
            tr->tUseStartPunch = false;
          }
          break;
        }

        case STChange: {
          int probeIndex = 1;
          int startData = pc->getStartData((int)i);
          if (startData < 0) {
            probeIndex = -startData;
          } else {
            while ((int)i - probeIndex >= 0 && !Runners[i - probeIndex]) {
              LegTypes tlt = pc->getLegType((int)i - probeIndex);
              if (tlt == LTIgnore || tlt == LTExtra || tlt == LTGroup) probeIndex++;
              else break;
            }
          }

          if ((int)i - probeIndex >= 0 && Runners[i - probeIndex]) {
            int z = (int)i - probeIndex;
            LegTypes tlt = pc->getLegType(z);
            int ft = 0;
            if (availableStartTimes.empty() || startData < 0) {
              if (!availableStartTimes.empty()) ft = getBestStartTime(availableStartTimes);
              ft = (tlt != LTIgnore) ? Runners[z]->getFinishTime() : 0;
              while (z > 0 && (tlt == LTExtra || tlt == LTIgnore)) {
                tlt = pc->getLegType(--z);
                if (Runners[z]) {
                  int tft = Runners[z]->getFinishTime();
                  if (tft > 0 && tlt != LTIgnore) ft = ft > 0 ? min(tft, ft) : tft;
                }
              }
            } else {
              ft = getBestStartTime(availableStartTimes);
            }
            if (ft <= 0) ft = 0;
            int restart = pc->getRestartTime((int)i);
            int rope    = pc->getRopeTime((int)i);
            if (((restart > 0 && rope > 0 && (ft == 0 || ft > rope)) || (ft == 0 && restart > 0)) &&
                !preventRestart() && !tr->preventRestart()) {
              ft = restart; tNumRestarts++;
            }
            if (ft >= 0) tr->setStartTime(ft, false, changeType);
            tr->tUseStartPunch = false;
            lastStartTime = ft;
          } else {
            tr->setStartTime(Class->getRestartTime((int)i), false, changeType);
            tr->tUseStartPunch = false;
          }
          break;
        }

        case STPursuit: {
          bool setStart = false;
          if (i > 0 && Runners[i - 1]) {
            if (lt == LTNormal || lt == LTSum || availableStartTimes.empty()) {
              int rt = getLegRunningTimeUnadjusted((int)i - 1, false, false);
              if (rt > 0) setStart = true;
              int leaderTime = pc->getTotalLegLeaderTime(oClass::AllowRecompute::NoUseOld, (int)i - 1, false, false);
              int timeAfter = leaderTime > 0 ? rt - leaderTime : 0;
              if (rt > 0 && timeAfter >= 0)
                lastStartTime = pc->getStartData((int)i) + timeAfter;
              int restart = pc->getRestartTime((int)i);
              int rope    = pc->getRopeTime((int)i);
              RunnerStatus hst = getLegStatus((int)i - 1, false, false);
              if (hst != StatusUnknown && hst != StatusOK) {
                setStart = true; lastStartTime = restart;
              }
              if (restart > 0 && rope > 0 && lastStartTime > rope &&
                  !preventRestart() && !tr->preventRestart()) {
                lastStartTime = restart; tNumRestarts++;
              }
              if (!availableStartTimes.empty()) {
                if (setStart) fill(availableStartTimes.begin(), availableStartTimes.end(), lastStartTime);
                else          fill(availableStartTimes.begin(), availableStartTimes.end(), 0);
                availableStartTimes.pop_back();
              }
            } else if (lt == LTParallel || lt == LTParallelOptional) {
              lastStartTime = getBestStartTime(availableStartTimes);
              setStart = true;
            }
            if (tr->getFinishTime() > 0) {
              setStart = true;
              if (lastStartTime == 0) lastStartTime = pc->getRestartTime((int)i);
            }
            if (!setStart) lastStartTime = 0;
          } else {
            lastStartTime = 0;
          }
          tr->tUseStartPunch = false;
          tr->setStartTime(lastStartTime, false, changeType);
          break;
        }
      } // switch st
    }

    // Build available start times for next parallel block
    size_t nextNonPar = i + 1;
    while (nextNonPar < Runners.size() && pc->isOptional(nextNonPar) && !Runners[nextNonPar])
      nextNonPar++;
    size_t nextBaseLeg = nextNonPar;
    while (nextNonPar < Runners.size() && pc->isParallel(nextNonPar))
      nextNonPar++;

    if (lt == LTExtra || (i + 1 < Runners.size() && pc->getLegType((int)(i + 1)) == LTExtra)) {
      if (lt != LTExtra) extraFinishTime = -1;
      if (tr->getFinishTime() > 0) {
        if (extraFinishTime <= 0) extraFinishTime = tr->getFinishTime();
        else extraFinishTime = min(extraFinishTime, tr->getFinishTime());
      }
    } else {
      extraFinishTime = -1;
    }

    if (nextNonPar < Runners.size()) {
      StartTypes nextSt = pc->getStartType((int)nextNonPar);
      int finishTime = tr->getFinishTime();
      if (lt == LTExtra) finishTime = extraFinishTime;
      if (nextSt == STDrawn || nextSt == STTime) {
        availableStartTimes.clear();
      } else if (finishTime > 0) {
        int nRCurrent = pc->getNumParallel((int)i);
        int nRNext    = pc->getNumParallel((int)nextBaseLeg);
        if (nRCurrent > 1 || nRNext > 1) {
          if (nRCurrent < nRNext) {
            for (int j = 0; j < nRNext / nRCurrent; j++)
              availableStartTimes.push_back(finishTime);
          } else if (nRNext == 1) {
            compressStartTimes(availableStartTimes, finishTime);
          } else {
            addStartTime(availableStartTimes, finishTime);
          }
        } else {
          availableStartTimes.clear();
        }
      }
    }
  } // for i

  if (!Runners.empty() && Runners[0]) {
    setStartTime(Runners[0]->getStartTime(), false, changeType);
  } else if (Class && Class->getStartType(0) != STDrawn) {
    setStartTime(Class->getStartData(0), false, changeType);
  }

  setFinishTime(getLegFinishTime(-1));
  setStatus(getLegStatus(-1, false, false), false, changeType);
}

// ---------------------------------------------------------------------------
// getLegToUse — skip optional/extra/ignore runners to find the effective leg
// ---------------------------------------------------------------------------

int oTeam::getLegToUse(int leg) const {
  if (Runners.empty()) return 0;
  if (leg == -1) leg = (int)Runners.size() - 1;
  int oleg = leg;
  if (Class && (size_t)leg < Runners.size() && !Runners[leg]) {
    LegTypes lt = Class->getLegType(leg);
    while (leg >= 0 && (lt == LTParallelOptional || lt == LTExtra || lt == LTIgnore) && !Runners[leg]) {
      if (leg == 0) return oleg;
      lt = Class->getLegType(--leg);
    }
  }
  return leg;
}

// ---------------------------------------------------------------------------
// getLegRestingTime
// ---------------------------------------------------------------------------

int oTeam::getLegRestingTime(int leg, bool useComputedRunnerTime) const {
  if (!Class) return 0;
  int rest = 0;
  int R = min((int)Runners.size(), leg + 1);
  for (int k = 1; k < R; k++) {
    if (Class->getStartType(k) == STPursuit && !Class->isParallel(k) && Runners[k] && Runners[k-1]) {
      int ft = getLegRunningTimeUnadjusted(k - 1, false, useComputedRunnerTime) + tStartTime;
      int st = Runners[k]->getStartTime();
      if (ft > 0 && st > 0) rest += st - ft;
    }
  }
  return rest;
}

// ---------------------------------------------------------------------------
// getLegRunningTimeUnadjusted — core relay time calculation
// ---------------------------------------------------------------------------

int oTeam::getLegRunningTimeUnadjusted(int leg, bool multidayTotal, bool useComputed) const {
  leg = getLegToUse(leg);
  int addon = multidayTotal ? max(0, inputTime) : 0;

  if ((size_t)leg >= Runners.size() || !Runners[leg]) return 0;

  if (!Class) {
    int dt = addon + max(Runners[leg]->getFinishTime() - tStartTime, 0);
    int dt2 = 0;
    if (leg > 0)
      dt2 = getLegRunningTimeUnadjusted(leg - 1, multidayTotal, useComputed) +
            Runners[leg]->getRunningTime(useComputed);
    return max(dt, dt2);
  }

  pClass pc = Class;
  LegTypes lt = pc->getLegType(leg);
  LegTypes ltNext = pc->getLegType(leg + 1);
  if (ltNext == LTParallel || ltNext == LTParallelOptional || ltNext == LTExtra) lt = ltNext;

  switch (lt) {
    case LTNormal:
      if (Runners[leg]->prelStatusOK(useComputed, true, false)) {
        int dt = leg > 0
          ? getLegRunningTimeUnadjusted(leg - 1, false, useComputed) + Runners[leg]->getRunningTime(useComputed)
          : 0;
        return addon + max(Runners[leg]->getFinishTimeAdjusted(true) -
                           (tStartTime + getLegRestingTime(leg, useComputed)), dt);
      }
      return 0;

    case LTParallelOptional:
    case LTParallel:
      if (Runners[leg]->prelStatusOK(useComputed, false, false)) {
        int pt   = leg > 0 ? getLegRunningTimeUnadjusted(leg - 1, false, useComputed) : 0;
        int rest = getLegRestingTime(leg, useComputed);
        int finT = Runners[leg]->getFinishTimeAdjusted(true);
        return addon + max(finT - (tStartTime + rest), pt);
      }
      return 0;

    case LTExtra: {
      if (leg == 0) return addon + max(Runners[leg]->getFinishTime() - tStartTime, 0);
      int baseLeg = leg;
      while (baseLeg > 0 && pc->getLegType(baseLeg) == LTExtra) baseLeg--;
      int baseTime = baseLeg > 0
        ? getLegRunningTimeUnadjusted(baseLeg - 1, multidayTotal, useComputed)
        : addon;
      int cLeg = baseLeg;
      int legTime = 0;
      bool bad = false;
      do {
        if (Runners[cLeg] && Runners[cLeg]->getFinishTime() > 0) {
          int rt = Runners[cLeg]->getRunningTime(useComputed);
          if (legTime == 0 || rt < legTime) {
            bad = !Runners[cLeg]->prelStatusOK(useComputed, false, false);
            legTime = rt;
          }
        }
        cLeg++;
      } while ((size_t)cLeg < Runners.size() && pc->getLegType(cLeg) == LTExtra);
      if (bad || legTime == 0) return 0;
      return baseTime + legTime;
    }

    case LTSum:
      if (Runners[leg]->prelStatusOK(useComputed, false, false)) {
        if (leg == 0) return addon + Runners[leg]->getRunningTime(useComputed);
        int prev = getLegRunningTimeUnadjusted(leg - 1, multidayTotal, useComputed);
        return prev == 0 ? 0 : Runners[leg]->getRunningTime(useComputed) + prev;
      }
      return 0;

    case LTIgnore:
      if (leg == 0) return 0;
      return getLegRunningTimeUnadjusted(leg - 1, multidayTotal, useComputed);

    case LTGroup: {
      if (Class->getResultModuleTag().empty()) return 0;
      int dt = Runners[leg]->getRunningTime(useComputed);
      if (leg > 0) dt += getLegRunningTimeUnadjusted(leg - 1, multidayTotal, useComputed);
      return dt;
    }

    default:
      return 0;
  }
}

// ---------------------------------------------------------------------------
// getLegRunningTime / getRunningTime / getTotalRunningTime
// ---------------------------------------------------------------------------

int oTeam::getLegRunningTime(int leg, bool computedTime, bool multidayTotal) const {
  if (computedTime) {
    leg = getLegToUse(leg);
    auto& cr = getComputedResult(leg);
    int addon = multidayTotal ? inputTime : 0;
    if (cr.version == oe->dataRevision)
      return cr.time > 0 ? cr.time + addon : 0;
  }
  bool isLastLeg = (leg == -1 || leg + 1 == (int)Runners.size());
  return getLegRunningTimeUnadjusted(leg, multidayTotal, false) +
         (isLastLeg ? getTimeAdjustment(false) : 0);
}

int oTeam::getRunningTime(bool computedTime) const {
  return getLegRunningTime(-1, computedTime, false);
}

int oTeam::getTotalRunningTime() const {
  return getLegRunningTime(-1, false, true);
}

wstring oTeam::getLegRunningTimeS(int leg, bool computed, bool multidayTotal, SubSecond mode) const {
  if (leg == -1) leg = (int)Runners.size() - 1;
  int rt = getLegRunningTime(leg, computed, multidayTotal);
  const wstring& bf = formatTime(rt, mode);
  if (rt > 0) {
    if ((size_t)leg < Runners.size() && Runners[leg] && Class &&
        Runners[leg]->getStartTime() == Class->getRestartTime(leg))
      return L"*" + bf;
    if (getNumShortening(leg) > 0) return L"*" + bf;
  }
  return bf;
}

int oTeam::getTotalRunningTimeAtLegStart(int leg, bool multidayTotal) const {
  int off = multidayTotal ? max(0, getInputTime()) : 0;
  if (!Class || leg == 0) return off;
  int pleg = Class->getPreceedingLeg(leg);
  if (pleg < 0) return off;
  return getLegRunningTime(pleg, false, multidayTotal);
}

// ---------------------------------------------------------------------------
// getLegFinishTime / getLegFinishTimeS
// ---------------------------------------------------------------------------

int oTeam::getLegFinishTime(int leg) const {
  leg = getLegToUse(leg);

  if (Class) {
    pClass pc = Class;
    LegTypes lt = pc->getLegType(leg);
    while (leg > 0 && (lt == LTIgnore ||
           (lt == LTExtra && (!Runners[leg] || Runners[leg]->getFinishTime() <= 0)))) {
      lt = pc->getLegType(--leg);
    }
  }

  if ((size_t)leg >= Runners.size() || !Runners[leg]) return 0;

  int ft = Runners[leg]->getFinishTime();
  if (!Class) return ft;

  bool extra = (Class->getLegType(leg) == LTExtra) ||
               ((size_t)(leg + 1) < Runners.size() && Class->getLegType(leg + 1) == LTExtra);
  bool par   = Class->isParallel(leg) ||
               ((size_t)(leg + 1) < Runners.size() && Class->isParallel(leg + 1));

  if (extra) {
    int ileg = leg;
    while (ileg > 0 && Class->getLegType(ileg) == LTExtra) ileg--;
    ft = 0;
    while ((size_t)ileg < Class->getNumStages()) {
      int ift = 0;
      if ((size_t)ileg < Runners.size() && Runners[ileg])
        ift = Runners[ileg]->getFinishTimeAdjusted(true);
      if (ift > 0) ft = (ft == 0) ? ift : min(ft, ift);
      if ((size_t)(++ileg) < Class->getNumStages() && Class->getLegType(ileg) != LTExtra) break;
    }
  } else if (par) {
    int ileg = leg;
    while (ileg > 0 && Class->isParallel(ileg)) ileg--;
    ft = 0;
    while ((size_t)ileg < Class->getNumStages()) {
      int ift = 0;
      if ((size_t)ileg < Runners.size() && Runners[ileg])
        ift = Runners[ileg]->getFinishTimeAdjusted(true);
      if (ift > 0) ft = (ft == 0) ? ift : max(ft, ift);
      if ((size_t)(++ileg) < Class->getNumStages() && !Class->isParallel(ileg)) break;
    }
  }
  return ft;
}

int oTeam::getLegStartTime(int leg) const {
  if (leg == -1) leg = (int)Runners.size() - 1;
  if ((size_t)leg < Runners.size() && Runners[leg]) return Runners[leg]->getStartTime();
  return 0;
}

wstring oTeam::getLegStartTimeS(int leg) const {
  return formatTime(getLegStartTime(leg));
}

wstring oTeam::getLegFinishTimeS(int leg, SubSecond mode) const {
  if (leg == -1) leg = (int)Runners.size() - 1;
  if ((size_t)leg < Runners.size() && Runners[leg])
    return Runners[leg]->getFinishTimeS(true, mode);
  return L"-";
}

// ---------------------------------------------------------------------------
// getLegStatus
// ---------------------------------------------------------------------------

RunnerStatus oTeam::getLegStatus(int leg, bool computed, bool multidayTotal) const {
  if (leg == -1) leg = (int)Runners.size() - 1;
  if ((size_t)leg >= Runners.size()) return StatusUnknown;

  if (multidayTotal) {
    RunnerStatus s = getLegStatus(leg, computed, false);
    if (s == StatusUnknown && inputStatus != StatusNotCompeting) return StatusUnknown;
    if (inputStatus == StatusUnknown) return StatusDNS;
    return max(inputStatus, s);
  }

  if (leg == (int)Runners.size() - 1 && tStatus == StatusDQ) return tStatus;

  leg = getLegToUse(leg);
  if (!Class) return StatusUnknown;

  while (leg > 0 && Class->getLegType(leg) == LTIgnore) leg--;

  if (computed) {
    auto& cr = getComputedResult(leg);
    if (cr.version == oe->dataRevision) return cr.status;
  }

  int s = 0;
  for (int i = 0; i <= leg; i++) {
    while (i < leg && Class->getLegType(i) == LTIgnore) i++;
    int st       = Runners[i] ? Runners[i]->getStatus() : StatusDNS;
    int bestTime = Runners[i] ? Runners[i]->getFinishTime() : 0;

    if (Class) {
      while (i + 1 < (int)Runners.size() && Class->getLegType(i + 1) == LTExtra) {
        i++;
        if (Runners[i]) {
          if (bestTime == 0 || (Runners[i]->getFinishTime() > 0 && Runners[i]->getFinishTime() < bestTime)) {
            st = Runners[i]->getStatus();
            bestTime = Runners[i]->getFinishTime();
          }
        }
      }
    }
    if (st == 0) return RunnerStatus(s == StatusOK ? 0 : s);
    s = max(s, st);
  }
  if (s == StatusUnknown && tStatus == StatusDNS) return tStatus;
  return RunnerStatus(s);
}

const wstring& oTeam::getLegStatusS(int leg, bool computed, bool multidayTotal) const {
  thread_local wstring tl;
  tl = encodeStatus(getLegStatus(leg, computed, multidayTotal));
  return tl;
}

// ---------------------------------------------------------------------------
// Deduced computed values
// ---------------------------------------------------------------------------

RunnerStatus oTeam::deduceComputedStatus() const {
  int leg = (int)Runners.size() - 1;
  leg = getLegToUse(leg);
  if (!Class) return StatusUnknown;
  while (leg > 0 && Class->getLegType(leg) == LTIgnore) leg--;
  int s = 0;
  for (int i = 0; i <= leg; i++) {
    while (i < leg && Class->getLegType(i) == LTIgnore) i++;
    int st       = Runners[i] ? Runners[i]->getStatusComputed(false) : StatusDNS;
    int bestTime = Runners[i] ? Runners[i]->getFinishTime() : 0;
    if (Class) {
      while (i + 1 < (int)Runners.size() && Class->getLegType(i + 1) == LTExtra) {
        i++;
        if (Runners[i]) {
          if (bestTime == 0 || (Runners[i]->getFinishTime() > 0 && Runners[i]->getFinishTime() < bestTime)) {
            st = Runners[i]->getStatusComputed(false);
            bestTime = Runners[i]->getFinishTime();
          }
        }
      }
    }
    if (st == 0) return RunnerStatus(s == StatusOK ? 0 : s);
    s = max(s, st);
  }
  if (s == StatusUnknown && tStatus == StatusDNS) return tStatus;
  return RunnerStatus(s);
}

int oTeam::deduceComputedRunningTime() const {
  return getLegRunningTimeUnadjusted((int)Runners.size() - 1, false, true) + getTimeAdjustment(false);
}

int oTeam::deduceComputedPoints() const {
  int pt = 0;
  for (auto r : Runners)
    if (r) pt += r->getRogainingPoints(true, false);
  return max(0, pt + getPointAdjustment());
}

// ---------------------------------------------------------------------------
// getStatusComputed / getDynamicStatus
// ---------------------------------------------------------------------------

RunnerStatus oTeam::getStatusComputed(bool /*allowUpdate*/) const {
  return tComputedStatus != StatusUnknown ? tComputedStatus : tStatus;
}

DynamicRunnerStatus oTeam::getDynamicStatus() const {
  if (getFinishTime() > 0) return DynamicRunnerStatus::StatusFinished;
  if (tStartTime > 0) return DynamicRunnerStatus::StatusActive;
  return DynamicRunnerStatus::StatusInactive;
}

// ---------------------------------------------------------------------------
// Place
// ---------------------------------------------------------------------------

oTeam::TeamPlace& oTeam::getTeamPlace(int leg) const {
  if ((int)tPlace.size() <= leg) tPlace.resize(leg + 1);
  return tPlace[leg];
}

int oTeam::getLegPlace(int leg, bool multidayTotal, bool allowUpdate) const {
  if (leg == -1) leg = (int)Runners.size() - 1;
  if (Class) {
    while ((size_t)leg < Class->legInfo.size() && Class->legInfo[leg].legMethod == LTIgnore) leg--;
  }
  auto& p = getTeamPlace(leg);
  if (!multidayTotal) return p.p.get(!allowUpdate);
  return p.totalP.get(!allowUpdate);
}

wstring oTeam::getLegPlaceS(int leg, bool multidayTotal) const {
  int p = getLegPlace(leg, multidayTotal);
  if (p > 0 && p < 10000) return itow(p);
  return _EmptyWString;
}

wstring oTeam::getLegPrintPlaceS(int leg, bool multidayTotal, bool withDot) const {
  int p = getLegPlace(leg, multidayTotal);
  if (p > 0 && p < 10000) {
    if (withDot) { wchar_t bf[16]; swprintf(bf, 16, L"%d.", p); return bf; }
    return itow(p);
  }
  return _EmptyWString;
}

// ---------------------------------------------------------------------------
// Computed result cache
// ---------------------------------------------------------------------------

const oTeam::ComputedLegResult& oTeam::getComputedResult(int leg) const {
  if ((int)tComputedResults.size() <= leg) tComputedResults.resize(leg + 1);
  return tComputedResults[leg];
}

void oTeam::setComputedResult(int leg, ComputedLegResult& comp) const {
  if ((int)tComputedResults.size() <= leg) tComputedResults.resize(leg + 1);
  tComputedResults[leg] = comp;
}

// ---------------------------------------------------------------------------
// Result calculation cache
// ---------------------------------------------------------------------------

void oTeam::resetResultCalcCache() const {
  resultCalculationCache.clear();
}

vector<vector<int>>& oTeam::getResultCache(ResultCalcCacheSymbol symb) const {
  if ((int)resultCalculationCache.size() <= (int)symb)
    resultCalculationCache.resize((int)symb + 1);
  return resultCalculationCache[symb];
}

void oTeam::setResultCache(ResultCalcCacheSymbol symb, int leg, vector<int>& data) const {
  auto& c = getResultCache(symb);
  if ((int)c.size() <= leg) c.resize(leg + 1);
  c[leg] = data;
}

// ---------------------------------------------------------------------------
// Rogaining
// ---------------------------------------------------------------------------

int oTeam::getRogainingPoints(bool computed, bool multidayTotal) const {
  if (computed) {
    if (tComputedPoints >= 0) return tComputedPoints;
  }
  int pts = 0;
  for (auto r : Runners) if (r) pts += r->getRogainingPoints(computed, multidayTotal);
  return max(0, pts + getPointAdjustment());
}

int oTeam::getRogainingReduction(bool /*computed*/) const {
  int red = 0;
  for (auto r : Runners) if (r) red += r->getRogainingReduction(false);
  return red;
}

int oTeam::getRogainingOvertime(bool /*computed*/) const {
  int ov = 0;
  for (auto r : Runners) if (r) ov += r->getRogainingOvertime(false);
  return ov;
}

int oTeam::getRogainingPointsGross(bool /*computed*/) const {
  int pts = 0;
  for (auto r : Runners) if (r) pts += r->getRogainingPointsGross(false);
  return pts;
}

// ---------------------------------------------------------------------------
// setBib
// ---------------------------------------------------------------------------

void oTeam::setBib(const wstring& bib, int numericalBib, bool updateStartNo) {
  getDI().setString("Bib", bib);
  if (updateStartNo && numericalBib > 0) StartNo = numericalBib;
  updateChanged();
}

// ---------------------------------------------------------------------------
// applyBibs / evaluate
// ---------------------------------------------------------------------------

void oTeam::applyBibs() {
  wstring bib = getBib();
  BibMode bibMode = BibUndefined;
  for (size_t i = 0; i < Runners.size(); i++) {
    pRunner tr = Runners[i];
    if (!tr) continue;
    pClass actualClass = tr->getClassRef(true);
    if (!actualClass) actualClass = Class;
    if (actualClass == Class) tr->setStartNo(StartNo, ChangeType::Update);
    if (bibMode == BibMode::BibUndefined && Class) bibMode = Class->getBibMode();
    if (!bib.empty()) {
      if (bibMode == BibSame) {
        tr->setBib(bib, 0, false);
      } else if (bibMode == BibAdd) {
        wchar_t pattern[32], bf[32];
        int ibib = oClass::extractBibPattern(bib, pattern) + (int)i;
        swprintf(bf, 32, pattern, ibib);
        tr->setBib(bf, 0, false);
      } else if (bibMode == BibLeg) {
        wstring rbib = bib + L"-" + Class->getLegNumber((int)i);
        tr->setBib(rbib, 0, false);
      }
    } else {
      if (bibMode == BibSame || bibMode == BibAdd || bibMode == BibLeg)
        tr->setBib(bib, 0, false);
    }
  }
}

void oTeam::evaluate(ChangeType changeType) {
  apply(ChangeType::Quiet, nullptr);
  vector<pair<int, pControl>> mp;
  for (auto r : Runners)
    if (r) r->evaluateCard(false, mp, 0, changeType);
  apply(changeType, nullptr);
}

// ---------------------------------------------------------------------------
// setTeamMemberStatus
// ---------------------------------------------------------------------------

void oTeam::setTeamMemberStatus(RunnerStatus dnsStatus) {
  setStatus(dnsStatus, true, ChangeType::Update);
  for (auto r : Runners) {
    if (r && (!isResultStatus(r->getStatus()) ||
              ((dnsStatus == StatusOutOfCompetition || dnsStatus == StatusNoTiming) && r->statusOK(false, false)))) {
      r->setStatus(dnsStatus, true, ChangeType::Update);
    }
  }
}

// ---------------------------------------------------------------------------
// getNumShortening
// ---------------------------------------------------------------------------

int oTeam::getNumShortening() const { return 0; }
int oTeam::getNumShortening(int /*leg*/) const { return 0; }

// ---------------------------------------------------------------------------
// markClassChanged
// ---------------------------------------------------------------------------

void oTeam::markClassChanged(int /*controlId*/) {}

// ---------------------------------------------------------------------------
// getTimeAfter
// ---------------------------------------------------------------------------

int oTeam::getTimeAfter(int /*leg*/, bool /*allowUpdate*/) const { return 0; }

// ---------------------------------------------------------------------------
// isResultUpdated / matchAbstractRunner / getRanking / getEntryDate / getRaceInfo
// ---------------------------------------------------------------------------

bool oTeam::isResultUpdated(bool /*total*/) const { return false; }

bool oTeam::matchAbstractRunner(const oAbstractRunner* target) const {
  return target == this;
}

int oTeam::getRanking() const { return 0; }

wstring oTeam::getEntryDate(bool /*useTeam*/) const {
  int d = getDCI().getInt("EntryDate");
  if (d <= 0) return _EmptyWString;
  return formatDate(d, true);
}

const pair<wstring, int> oTeam::getRaceInfo() {
  return { L"", 0 };
}

// ---------------------------------------------------------------------------
// setInputData
// ---------------------------------------------------------------------------

void oTeam::setInputData(const oTeam& t) {
  inputTime   = t.inputTime;
  inputStatus = t.inputStatus;
  inputPoints = t.inputPoints;
  inputPlace  = t.inputPlace;
}

// ---------------------------------------------------------------------------
// Static comparators
// ---------------------------------------------------------------------------

bool oTeam::compareResult(const oTeam& a, const oTeam& b) {
  if (a.Class != b.Class) {
    if (a.Class) { if (b.Class) return *a.Class < *b.Class; return true; }
    return false;
  }
  if (a.tmpSortStatus != b.tmpSortStatus) return a.tmpSortStatus < b.tmpSortStatus;
  if (a.tmpSortTime   != b.tmpSortTime)   return a.tmpSortTime   < b.tmpSortTime;
  const wstring& as = a.getBib();
  const wstring& bs = b.getBib();
  if (as != bs) return compareBib(as, bs);
  int aix = a.getDCI().getInt("SortIndex");
  int bix = b.getDCI().getInt("SortIndex");
  if (aix != bix) {
    if (aix == 0) aix = std::numeric_limits<int>::max();
    if (bix == 0) bix = std::numeric_limits<int>::max();
    return aix < bix;
  }
  return a.sName < b.sName;
}

bool oTeam::compareResultNoSno(const oTeam& a, const oTeam& b) {
  if (a.Class != b.Class) {
    if (a.Class) { if (b.Class) return *a.Class < *b.Class; return true; }
    return false;
  }
  if (a.tmpSortStatus != b.tmpSortStatus) return a.tmpSortStatus < b.tmpSortStatus;
  if (a.tmpSortTime   != b.tmpSortTime)   return a.tmpSortTime   < b.tmpSortTime;
  int aix = a.getDCI().getInt("SortIndex");
  int bix = b.getDCI().getInt("SortIndex");
  if (aix != bix) {
    if (aix == 0) aix = std::numeric_limits<int>::max();
    if (bix == 0) bix = std::numeric_limits<int>::max();
    return aix < bix;
  }
  pClub ca = a.getClubRef();
  pClub cb = b.getClubRef();
  if (ca != cb) {
    int cres = compareClubs(ca, cb);
    if (cres != 2) return cres != 0;
  }
  return a.sName < b.sName;
}

bool oTeam::compareResultClub(const oTeam& a, const oTeam& b) {
  pClub ca = a.getClubRef();
  pClub cb = b.getClubRef();
  if (ca != cb) {
    int cres = compareClubs(ca, cb);
    if (cres != 2) return cres != 0;
  }
  return compareResult(a, b);
}

bool oTeam::compareSNO(const oTeam& a, const oTeam& b) {
  const wstring& as = a.getBib();
  const wstring& bs = b.getBib();
  if (as != bs) return compareBib(as, bs);
  if (a.Class != b.Class) {
    if (a.Class) { if (b.Class) return *a.Class < *b.Class; return true; }
    return false;
  }
  return a.sName < b.sName;
}

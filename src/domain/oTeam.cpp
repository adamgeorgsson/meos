// oTeam.cpp — oTeam implementation (US-003h).
// Ported from code/oTeam.cpp + code/oTeamEvent.cpp.
// Cross-platform, no Win32 / GUI dependencies.

#include "../util/gdioutput.h"
#include "../util/Table.h"
#include "oEvent.h"
#include "oTeam.h"
#include "oDataContainer.h"
#include "xmlparser.h"
#include "../util/localizer.h"
#include "../util/meos_util.h"
#include <algorithm>
#include <cassert>
#include <limits>
#include <set>

using ChangeType = oBase::ChangeType;

// ── Constructors / Destructor ─────────────────────────────────────────────────

oTeam::oTeam(oEvent *poe) : oAbstractRunner(poe, false) {
  Id = oe->getFreeTeamId();
  getDI().initData();
  correctionNeeded = false;
  StartNo = 0;
  tTeamPatrolRogainingAndVersion.first = -1;
}

oTeam::oTeam(oEvent *poe, int id) : oAbstractRunner(poe, true) {
  Id = id;
  oe->qFreeTeamId = max(id, oe->qFreeTeamId);
  getDI().initData();
  correctionNeeded = false;
  StartNo = 0;
  tTeamPatrolRogainingAndVersion.first = -1;
}

oTeam::~oTeam() {}

void oTeam::prepareRemove() {
  for (unsigned i = 0; i < Runners.size(); i++)
    setRunnerInternal(i, nullptr);
}

// ── DI buffer access ──────────────────────────────────────────────────────────

oDataContainer &oTeam::getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const {
  data    = (pvoid)oData;
  olddata = (pvoid)oDataOld;
  strData = const_cast<pvectorstr>(&oDataStr);
  return *oe->oTeamData;
}

// ── Changed notification ──────────────────────────────────────────────────────

void oTeam::changedObject() {
  markClassChanged(-1);
  sqlChanged = true;
  oe->sqlTeams.changed = true;
}

void oTeam::markClassChanged(int controlId) {
  if (Class)
    Class->markSQLChanged(-1, controlId);
}

// ── Name ──────────────────────────────────────────────────────────────────────

void oTeam::setName(const wstring &n, bool manualUpdate) {
  if (sName != n) {
    sName = n;
    updateChanged();
  }
}

// ── Club ──────────────────────────────────────────────────────────────────────

void oTeam::setClub(const wstring &clubName) {
  oAbstractRunner::setClub(clubName);
  propagateClub();
}

pClub oTeam::setClubId(int clubId) {
  oAbstractRunner::setClubId(clubId);
  propagateClub();
  return Club;
}

void oTeam::propagateClub() {
  if (Class && Class->getNumDistinctRunners() == 1) {
    for (pRunner r : Runners) {
      if (r && r->Club != Club) {
        r->Club = Club;
        r->updateChanged();
      }
    }
  }
}

// ── Class ─────────────────────────────────────────────────────────────────────

void oTeam::setClassId(int id, bool isManualUpdate) {
  oAbstractRunner::setClassId(id, isManualUpdate);
}

// ── StartNo ───────────────────────────────────────────────────────────────────

void oTeam::setStartNo(int no, ChangeType ct) {
  if (StartNo != no) {
    StartNo = no;
    updateChanged(ct);
  }
}

// ── XML Serialization ─────────────────────────────────────────────────────────

bool oTeam::write(xmlparser &xml) {
  if (Removed) return true;

  xml.startTag("Team");
  xml.write("Id", Id);
  xml.write("StartNo", StartNo);
  xml.write("Updated", getStamp());
  xml.write("Name", sName);
  xml.writeTime("Start", startTime);
  xml.writeTime("Finish", FinishTime);
  xml.write("Status", status);
  xml.write("Runners", getRunners());

  if (Club)  xml.write("Club",  Club->Id);
  if (Class) xml.write("Class", Class->Id);

  xml.write("InputPoint",  inputPoints);
  if (inputStatus != StatusOK)
    xml.write("InputStatus", itos(inputStatus));
  xml.writeTime("InputTime",  inputTime);
  xml.write("InputPlace", inputPlace);

  getDI().write(xml);
  xml.endTag();
  return true;
}

void oTeam::set(const xmlobject &xo) {
  xmlList xl;
  xo.getObjects(xl);

  for (auto &it : xl) {
    if (it.is("Id"))
      Id = it.getInt();
    else if (it.is("Name"))
      sName = it.getWStr();
    else if (it.is("StartNo"))
      StartNo = it.getInt();
    else if (it.is("Start"))
      tStartTime = startTime = it.getRelativeTime();
    else if (it.is("Finish"))
      FinishTime = it.getRelativeTime();
    else if (it.is("Status")) {
      unsigned rawStatus = it.getInt();
      tStatus = status = RunnerStatus(rawStatus < 100u ? rawStatus : 0);
    }
    else if (it.is("Class"))
      Class = oe->getClass(it.getInt());
    else if (it.is("Club"))
      Club = oe->getClub(it.getInt());
    else if (it.is("Runners")) {
      vector<int> r;
      decodeRunners(it.getRawStr(), r);
      importRunners(r);
    }
    else if (it.is("InputTime"))
      inputTime = it.getRelativeTime();
    else if (it.is("InputStatus")) {
      unsigned rawStatus = it.getInt();
      inputStatus = RunnerStatus(rawStatus < 100u ? rawStatus : 0);
    }
    else if (it.is("InputPoint"))
      inputPoints = it.getInt();
    else if (it.is("InputPlace"))
      inputPlace = it.getInt();
    else if (it.is("Updated"))
      Modified.setStamp(it.getRawStr());
    else if (it.is("oData"))
      getDI().set(it);
  }
}

// ── Runner string encoding ────────────────────────────────────────────────────

string oTeam::getRunners() const {
  string str;
  char bf[16];
  for (unsigned m = 0; m < Runners.size(); m++) {
    if (Runners[m]) {
      snprintf(bf, sizeof(bf), "%d;", Runners[m]->getId());
      str += bf;
    }
    else
      str += "0;";
  }
  return str;
}

void oTeam::decodeRunners(const string &rns, vector<int> &rid) {
  const char *str = rns.c_str();
  rid.clear();
  while (*str) {
    rid.push_back(atoi(str));
    while (*str && *str != ';' && *str != ',') str++;
    if (*str == ';' || *str == ',') str++;
  }
}

void oTeam::importRunners(const vector<int> &rns) {
  Runners.resize(rns.size());
  for (size_t n = 0; n < rns.size(); n++) {
    if (rns[n] > 0)
      Runners[n] = oe->getRunner(rns[n], 0);
    else
      Runners[n] = nullptr;

    if (Runners[n]) {
      Runners[n]->tInTeam = this;
      Runners[n]->tLeg    = (int)n;
    }
  }
}

void oTeam::importRunners(const vector<pRunner> &rns) {
  // Unlink old runners
  for (size_t k = 0; k < Runners.size(); k++) {
    pRunner r = Runners[k];
    if (r && r->tInTeam == this) {
      r->tInTeam = nullptr;
      r->tLeg    = 0;
    }
  }
  Runners.resize(rns.size());
  for (size_t n = 0; n < rns.size(); n++) {
    Runners[n] = rns[n];
    if (rns[n] && isAddedToEvent()) {
      rns[n]->tInTeam = this;
      rns[n]->tLeg    = (int)n;
    }
  }
}

// ── Runner assignment ─────────────────────────────────────────────────────────

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
  setRunnerInternal(i, r);

  if (r) {
    if (tStatus == StatusDNS)
      setStatus(StatusUnknown, true, ChangeType::Update);
    r->getDI().setInt("RaceId", oldRaceId);
    r->tInTeam = this;
    r->tLeg    = i;
    r->createMultiRunner(true, sync);
  }

  if (Class) {
    int index = 1;
    for (unsigned k = i + 1; k < Class->getNumStages(); k++) {
      if (Class->getLegRunner(k) == (int)i) {
        if (r) {
          pRunner mr = r->getMultiRunner(index++);
          if (mr) {
            mr->setName(r->getName(), true);
            mr->synchronize();
            setRunnerInternal(k, mr);
          }
        }
        else
          setRunnerInternal(k, nullptr);
      }
    }
  }
}

void oTeam::setRunnerInternal(int k, pRunner r) {
  if (r == Runners[k]) {
    if (r) {
      r->tInTeam = this;
      r->tLeg    = k;
    }
    return;
  }

  pRunner rOld = Runners[k];
  if (rOld) {
    assert(rOld->tInTeam == nullptr || rOld->tInTeam == this);
    rOld->tInTeam = nullptr;
    rOld->tLeg    = 0;
  }

  // Detach from any other team
  if (r && r->tInTeam) {
    if (r->tInTeam->Runners[r->tLeg]) {
      r->tInTeam->Runners[r->tLeg] = nullptr;
      r->tInTeam->updateChanged();
      if (r->tInTeam != this)
        r->tInTeam->synchronize(true);
    }
  }

  Runners[k] = r;
  if (r) {
    r->tInTeam = this;
    r->tLeg    = k;
    if (Class && (r->Class == nullptr || Class->getLegType(k) != LTGroup))
      r->setClassId(getClassId(false), false);
  }
  updateChanged();
}

pRunner oTeam::getRunner(unsigned leg) const {
  if (leg == unsigned(-1)) leg = (unsigned)Runners.size() - 1;
  return leg < Runners.size() ? Runners[leg] : nullptr;
}

bool oTeam::isRunnerUsed(int rId) const {
  for (unsigned i = 0; i < Runners.size(); i++)
    if (Runners[i] && Runners[i]->getId() == rId)
      return true;
  return false;
}

// ── Leg selection helper ──────────────────────────────────────────────────────

int oTeam::getLegToUse(int leg) const {
  if (Runners.empty()) return 0;
  if (leg == -1) leg = (int)Runners.size() - 1;
  int oleg = leg;
  if (Class && !Runners[leg]) {
    LegTypes lt = Class->getLegType(leg);
    while (leg >= 0 && (lt == LTParallelOptional || lt == LTExtra || lt == LTIgnore) && !Runners[leg]) {
      if (leg == 0) return oleg;
      leg--;
      lt = Class->getLegType(leg);
    }
  }
  return leg;
}

// ── Finish time ───────────────────────────────────────────────────────────────

wstring oTeam::getLegFinishTimeS(int leg, SubSecond mode) const {
  if (leg == -1) leg = (int)Runners.size() - 1;
  if (unsigned(leg) < Runners.size() && Runners[leg])
    return Runners[leg]->getFinishTimeS(true, mode);
  return L"-";
}

int oTeam::getLegFinishTime(int leg) const {
  leg = getLegToUse(leg);

  if (Class) {
    pClass pc = Class;
    LegTypes lt = pc->getLegType(leg);
    while (leg > 0 && (lt == LTIgnore ||
           (lt == LTExtra && (!Runners[leg] || Runners[leg]->getFinishTime() <= 0)))) {
      leg--;
      lt = pc->getLegType(leg);
    }
  }

  if (unsigned(leg) < Runners.size() && Runners[leg]) {
    int ft = Runners[leg]->getFinishTime();
    if (Class) {
      bool extra = Class->getLegType(leg) == LTExtra ||
                   (leg + 1 < (int)Class->getNumStages() && Class->getLegType(leg + 1) == LTExtra);
      bool par   = Class->isParallel(leg) ||
                   (leg + 1 < (int)Class->getNumStages() && Class->isParallel(leg + 1));

      if (extra) {
        ft = 0;
        int ileg = leg;
        while (ileg > 0 && Class->getLegType(ileg) == LTExtra) ileg--;
        while (size_t(ileg) < Class->getNumStages()) {
          int ift = Runners[ileg] ? Runners[ileg]->getFinishTimeAdjusted(true) : 0;
          if (ift > 0) ft = (ft == 0 ? ift : min(ft, ift));
          ileg++;
          if (size_t(ileg) < Class->getNumStages() && Class->getLegType(ileg) != LTExtra) break;
        }
      }
      else if (par) {
        ft = 0;
        int ileg = leg;
        while (ileg > 0 && Class->isParallel(ileg)) ileg--;
        while (size_t(ileg) < Class->getNumStages()) {
          int ift = Runners[ileg] ? Runners[ileg]->getFinishTimeAdjusted(true) : 0;
          if (ift > 0) ft = (ft == 0 ? ift : max(ft, ift));
          ileg++;
          if (size_t(ileg) < Class->getNumStages() && !Class->isParallel(ileg)) break;
        }
      }
    }
    return ft;
  }
  return 0;
}

// ── Running time ──────────────────────────────────────────────────────────────

int oTeam::getTotalRunningTimeAtLegStart(int leg, bool multidayTotal) const {
  int off = multidayTotal ? max(0, getInputTime()) : 0;
  if (!Class || leg == 0) return off;
  int pleg = Class->getPreceedingLeg(leg);
  if (pleg < 0) return off;
  return getLegRunningTime(pleg, false, multidayTotal);
}

int oTeam::getRunningTime(bool computedTime) const {
  return getLegRunningTime(-1, computedTime, false);
}

int oTeam::getLegRunningTime(int leg, bool computedTime, bool multidayTotal) const {
  if (computedTime) {
    leg = getLegToUse(leg);
    auto &cr = getComputedResult(leg);
    int addon = multidayTotal ? inputTime : 0;
    if (cr.version == (int)oe->dataRevision) {
      return cr.time > 0 ? cr.time + addon : 0;
    }
  }
  bool isLastLeg = (leg == -1 || leg + 1 == (int)Runners.size());
  return getLegRunningTimeUnadjusted(leg, multidayTotal, false) +
         (isLastLeg ? getTimeAdjustment(false) : 0);
}

int oTeam::getLegRestingTime(int leg, bool useComputedRunnerTime) const {
  if (!Class) return 0;
  int rest = 0;
  int R = min<int>((int)Runners.size(), leg + 1);
  for (int k = 1; k < R; k++) {
    if (Class->getStartType(k) == STPursuit && !Class->isParallel(k) &&
        Runners[k] && Runners[k - 1]) {
      int ft = getLegRunningTimeUnadjusted(k - 1, false, useComputedRunnerTime) + tStartTime;
      int st = Runners[k]->getStartTime();
      if (ft > 0 && st > 0) rest += st - ft;
    }
  }
  return rest;
}

int oTeam::getLegRunningTimeUnadjusted(int leg, bool multidayTotal, bool useComputedRunnerTime) const {
  leg = getLegToUse(leg);
  int addon = multidayTotal ? inputTime : 0;

  if (unsigned(leg) < Runners.size() && Runners[leg]) {
    if (Class) {
      pClass pc = Class;
      LegTypes lt = pc->getLegType(leg);
      LegTypes ltNext = (leg + 1 < (int)pc->getNumStages()) ? pc->getLegType(leg + 1) : LTNormal;
      if (ltNext == LTParallel || ltNext == LTParallelOptional || ltNext == LTExtra)
        lt = ltNext;

      switch (lt) {
        case LTNormal:
          if (Runners[leg]->prelStatusOK(useComputedRunnerTime, true, false)) {
            int dt = leg > 0 ? getLegRunningTimeUnadjusted(leg - 1, false, useComputedRunnerTime) +
                               Runners[leg]->getRunningTime(useComputedRunnerTime) : 0;
            return addon + max(Runners[leg]->getFinishTimeAdjusted(true) -
                              (tStartTime + getLegRestingTime(leg, useComputedRunnerTime)), dt);
          }
          return 0;

        case LTParallelOptional:
        case LTParallel:
          if (Runners[leg]->prelStatusOK(useComputedRunnerTime, false, false)) {
            int pt   = leg > 0 ? getLegRunningTimeUnadjusted(leg - 1, false, useComputedRunnerTime) : 0;
            int rest = getLegRestingTime(leg, useComputedRunnerTime);
            int ft   = Runners[leg]->getFinishTimeAdjusted(true);
            return addon + max(ft - (tStartTime + rest), pt);
          }
          return 0;

        case LTExtra:
          if (leg == 0)
            return addon + max(Runners[leg]->getFinishTime() - tStartTime, 0);
          else {
            int baseLeg = leg;
            while (baseLeg > 0 && pc->getLegType(baseLeg) == LTExtra) baseLeg--;
            int baseTime = baseLeg > 0 ?
              getLegRunningTimeUnadjusted(baseLeg - 1, multidayTotal, useComputedRunnerTime) : addon;

            int cLeg = baseLeg, legTime = 0;
            bool bad = false;
            do {
              if (Runners[cLeg] && Runners[cLeg]->getFinishTime() > 0) {
                int rt = Runners[cLeg]->getRunningTime(useComputedRunnerTime);
                if (legTime == 0 || rt < legTime) {
                  bad = !Runners[cLeg]->prelStatusOK(useComputedRunnerTime, false, false);
                  legTime = rt;
                }
              }
              cLeg++;
            } while (size_t(cLeg) < pc->getNumStages() && pc->getLegType(cLeg) == LTExtra);

            if (bad || legTime == 0) return 0;
            return baseTime + legTime;
          }

        case LTSum:
          if (Runners[leg]->prelStatusOK(useComputedRunnerTime, false, false)) {
            if (leg == 0)
              return addon + Runners[leg]->getRunningTime(useComputedRunnerTime);
            else {
              int prev = getLegRunningTimeUnadjusted(leg - 1, multidayTotal, useComputedRunnerTime);
              if (prev == 0) return 0;
              return Runners[leg]->getRunningTime(useComputedRunnerTime) + prev;
            }
          }
          return 0;

        case LTIgnore:
          if (leg == 0) return 0;
          return getLegRunningTimeUnadjusted(leg - 1, multidayTotal, useComputedRunnerTime);

        case LTGroup:
          if (Class->getResultModuleTag().empty()) return 0;
          else {
            int dt = Runners[leg]->getRunningTime(useComputedRunnerTime);
            if (leg > 0) dt += getLegRunningTimeUnadjusted(leg - 1, multidayTotal, useComputedRunnerTime);
            return dt;
          }

        default:
          return 0;
      }
    }
    else {
      int dt  = addon + max(Runners[leg]->getFinishTime() - tStartTime, 0);
      int dt2 = 0;
      if (leg > 0)
        dt2 = getLegRunningTimeUnadjusted(leg - 1, multidayTotal, useComputedRunnerTime) +
              Runners[leg]->getRunningTime(useComputedRunnerTime);
      return max(dt, dt2);
    }
  }
  return 0;
}

wstring oTeam::getLegRunningTimeS(int leg, bool computedTime, bool multidayTotal, SubSecond mode) const {
  if (leg == -1) leg = (int)Runners.size() - 1;
  int rt = getLegRunningTime(leg, computedTime, multidayTotal);
  const wstring &bf = formatTime(rt, mode);
  if (rt > 0) {
    if ((unsigned(leg) < Runners.size() && Runners[leg] && Class &&
         Runners[leg]->getStartTime() == Class->getRestartTime(leg)) || getNumShortening(leg) > 0)
      return L"*" + bf;
  }
  return bf;
}

// ── Status ────────────────────────────────────────────────────────────────────

RunnerStatus oTeam::getLegStatus(int leg, bool computed, bool multidayTotal) const {
  if (leg == -1) leg = (int)Runners.size() - 1;
  if (unsigned(leg) >= Runners.size()) {
    if (tStatus != StatusUnknown) return tStatus;
    return StatusUnknown;
  }

  if (multidayTotal) {
    RunnerStatus s = getLegStatus(leg, computed, false);
    if (s == StatusUnknown && inputStatus != StatusNotCompeting)
      return StatusUnknown;
    if (inputStatus == StatusUnknown) return StatusDNS;
    return max(inputStatus, s);
  }

  if (leg == (int)(Runners.size() - 1) && tStatus == StatusDQ)
    return tStatus;

  leg = getLegToUse(leg);

  if (!Class) return StatusUnknown;

  while (leg > 0 && Class->getLegType(leg) == LTIgnore) leg--;

  if (computed) {
    auto &cr = getComputedResult(leg);
    if (cr.version == (int)oe->dataRevision) return cr.status;
  }

  int s = 0;
  for (int i = 0; i <= leg; i++) {
    while (i < leg && Class->getLegType(i) == LTIgnore) i++;

    int st       = Runners[i] ? Runners[i]->getStatus() : StatusDNS;
    int bestTime = Runners[i] ? Runners[i]->getFinishTime() : 0;

    if (Class) {
      while ((i + 1) < (int)Runners.size() && Class->getLegType(i + 1) == LTExtra) {
        i++;
        if (Runners[i]) {
          if (bestTime == 0 || (Runners[i]->getFinishTime() > 0 &&
              Runners[i]->getFinishTime() < bestTime)) {
            st       = Runners[i]->getStatus();
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

const wstring &oTeam::getLegStatusS(int leg, bool computed, bool multidayTotal) const {
  return oe->formatStatus(getLegStatus(leg, computed, multidayTotal), true);
}

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
      while ((i + 1) < (int)Runners.size() && Class->getLegType(i + 1) == LTExtra) {
        i++;
        if (Runners[i] && (bestTime == 0 || (Runners[i]->getFinishTime() > 0 &&
            Runners[i]->getFinishTime() < bestTime))) {
          st       = Runners[i]->getStatusComputed(false);
          bestTime = Runners[i]->getFinishTime();
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
  for (size_t k = 0; k < Runners.size(); k++)
    if (Runners[k]) pt += Runners[k]->getRogainingPoints(true, false);
  return max(0, pt + getPointAdjustment());
}

RunnerStatus oTeam::getStatusComputed(bool allowUpdate) const {
  auto &p = getTeamPlace((int)Runners.size() - 1);
  if (Class && allowUpdate && p.p.isOld(*oe)) {
    std::set<int> ids;
    ids.insert(getClassId(true));
    oe->calculateTeamResults(ids, oEvent::ResultType::ClassResult);
  }
  return tComputedStatus != StatusUnknown ? tComputedStatus : tStatus;
}

// ── Place ─────────────────────────────────────────────────────────────────────

int oTeam::getLegPlace(int leg, bool multidayTotal, bool allowUpdate) const {
  if (leg == -1) leg = (int)Runners.size() - 1;
  if (Class) {
    while (size_t(leg) < Class->legInfo.size() && Class->legInfo[leg].legMethod == LTIgnore)
      leg--;
  }
  auto &p = getTeamPlace(leg);
  if (!multidayTotal) {
    if (Class && allowUpdate && p.p.isOld(*oe)) {
      std::set<int> ids; ids.insert(getClassId(true));
      oe->calculateTeamResults(ids, oEvent::ResultType::ClassResult);
    }
    return p.p.get(!allowUpdate);
  }
  else {
    if (Class && allowUpdate && p.totalP.isOld(*oe)) {
      std::set<int> ids; ids.insert(getClassId(true));
      oe->calculateTeamResults(ids, oEvent::ResultType::TotalResult);
    }
    return p.totalP.get(!allowUpdate);
  }
}

wstring oTeam::getLegPlaceS(int leg, bool multidayTotal) const {
  int p = getLegPlace(leg, multidayTotal);
  if (p > 0 && p < 10000) {
    wchar_t bf[16];
    swprintf(bf, sizeof(bf) / sizeof(wchar_t), L"%d", p);
    return bf;
  }
  return _EmptyWString;
}

wstring oTeam::getLegPrintPlaceS(int leg, bool multidayTotal, bool withDot) const {
  int p = getLegPlace(leg, multidayTotal);
  if (p > 0 && p < 10000) {
    if (withDot) {
      wchar_t bf[16];
      swprintf(bf, sizeof(bf) / sizeof(wchar_t), L"%d.", p);
      return bf;
    }
    else
      return itow(p);
  }
  return _EmptyWString;
}

bool oTeam::isResultUpdated(bool totalResult) const {
  auto &p = getTeamPlace((int)Runners.size() - 1);
  return totalResult ? !p.totalP.isOld(*oe) : !p.p.isOld(*oe);
}

// ── Computed result cache ─────────────────────────────────────────────────────

const oTeam::ComputedLegResult &oTeam::getComputedResult(int leg) const {
  if (size_t(leg) < tComputedResults.size()) return tComputedResults[leg];
  if (tComputedResults.empty()) tComputedResults.resize(1);
  return tComputedResults[0];
}

void oTeam::setComputedResult(int leg, ComputedLegResult &comp) const {
  if (tComputedResults.size() < Runners.size())
    tComputedResults.resize(Runners.size());
  if (size_t(leg) < tComputedResults.size())
    tComputedResults[leg] = comp;
}

oTeam::TeamPlace &oTeam::getTeamPlace(int leg) const {
  if (tPlace.size() != Runners.size()) tPlace.resize(max(Runners.size(), size_t(1)));
  if (size_t(leg) < tPlace.size()) return tPlace[leg];
  if (tPlace.empty()) tPlace.resize(1);
  return tPlace[0];
}

// ── Result calc cache ─────────────────────────────────────────────────────────

void oTeam::resetResultCalcCache() const {
  resultCalculationCache.resize(RCCLast);
  for (int k = 0; k < RCCLast; k++)
    resultCalculationCache[k].resize(Runners.size());
}

vector< vector<int> > &oTeam::getResultCache(ResultCalcCacheSymbol symb) const {
  return resultCalculationCache[symb];
}

void oTeam::setResultCache(ResultCalcCacheSymbol symb, int leg, vector<int> &data) const {
  if (!resultCalculationCache.empty() && size_t(leg) < resultCalculationCache[symb].size())
    resultCalculationCache[symb][leg].swap(data);
}

// ── Status helpers ────────────────────────────────────────────────────────────

void oTeam::setTeamMemberStatus(RunnerStatus dnsStatus) {
  assert(!isResultStatus(dnsStatus) || dnsStatus == StatusOK);
  setStatus(dnsStatus, true, ChangeType::Update);
  for (unsigned i = 0; i < Runners.size(); i++) {
    if (Runners[i] && (!isResultStatus(Runners[i]->getStatus()) ||
        ((dnsStatus == StatusOutOfCompetition || dnsStatus == StatusNoTiming) &&
          Runners[i]->statusOK(false, false)))) {
      Runners[i]->setStatus(dnsStatus, true, ChangeType::Update);
    }
  }
}

// ── Apply ─────────────────────────────────────────────────────────────────────

static void compressStartTimes(vector<int> &av, int ft) {
  for (size_t j = 0; j < av.size(); j++) ft = max(ft, av[j]);
  av.resize(1); av[0] = ft;
}

static void addStartTime(vector<int> &av, int ft) {
  for (size_t k = 0; k < av.size(); k++) {
    if (ft >= av[k]) { av.insert(av.begin() + k, ft); return; }
  }
  av.push_back(ft);
}

static int getBestStartTime(vector<int> &av) {
  if (av.empty()) return 0;
  int t = av.back(); av.pop_back(); return t;
}

void oTeam::quickApply() {
  if (unsigned(status) >= 100) status = StatusUnknown;

  if (Class && Runners.size() != size_t(Class->getNumStages())) {
    for (size_t k = Class->getNumStages(); k < Runners.size(); k++) {
      pRunner tr = Runners[k];
      if (tr && tr->tInTeam) {
        tr->tInTeam = nullptr; tr->tLeg = 0; tr->tLegEquClass = 0;
        if (tr->Class == Class) { tr->Class = nullptr; oe->classIdToRunnerHash.reset(); }
        tr->updateChanged();
      }
    }
    Runners.resize(Class->getNumStages());
  }

  for (size_t i = 0; i < Runners.size(); i++) {
    if (Runners[i]) {
      if (Runners[i]->isRemoved()) {
        Runners[i]->tInTeam = nullptr; Runners[i]->tLeg = 0; Runners[i] = nullptr;
      }
      auto tit = Runners[i] ? Runners[i]->tInTeam : nullptr;
      if (tit && tit != this) tit->correctRemove(Runners[i]);
      if (Runners[i]) { Runners[i]->tInTeam = this; Runners[i]->tLeg = (int)i; }
    }
  }
}

void oTeam::apply(ChangeType changeType, pRunner source) {
  if (unsigned(status) >= 100) status = StatusUnknown;

  int lastStartTime = 0;
  RunnerStatus lastStatus = StatusUnknown;
  bool freeStart = Class ? Class->hasFreeStart() : false;
  int extraFinishTime = -1;

  if (Class && Runners.size() != size_t(Class->getNumStages())) {
    for (size_t k = Class->getNumStages(); k < Runners.size(); k++) {
      auto tr = Runners[k];
      if (tr && tr->tInTeam) {
        tr->tInTeam = nullptr; tr->tLeg = 0; tr->tLegEquClass = 0;
        if (tr->Class == Class) { tr->Class = nullptr; oe->classIdToRunnerHash.reset(); }
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

    if (changeType == ChangeType::Quiet && i > 0 && source != nullptr && Runners[i - 1] == source)
      return;

    if (!Runners[i] && Class) {
      unsigned lr = Class->getLegRunner(i);
      if (lr < i && Runners[lr]) {
        Runners[lr]->createMultiRunner(false, false);
        int dup = Class->getLegRunnerIndex(i);
        Runners[i] = Runners[lr]->getMultiRunner(dup);
      }
    }

    if (Runners[i]) {
      pRunner tr = Runners[i];
      // Check for duplicate runner in earlier legs
      for (size_t k = 0; k < i; k++)
        if (Runners[i] == Runners[k]) { Runners[i] = nullptr; break; }
      if (!Runners[i]) continue;

      pClass actualClass = tr->getClassRef(true);
      if (!actualClass) actualClass = Class;

      if (tr->tInTeam && tr->tInTeam != this) tr->tInTeam->correctRemove(tr);
      tr->tInTeam = this;
      tr->tLeg    = (int)i;

      if (Class) {
        int unused;
        Class->splitLegNumberParallel((int)i, tr->tLegEquClass, unused);
      }
      else {
        tr->tLegEquClass = (int)i;
      }

      if (actualClass == Class) tr->setStartNo(StartNo, changeType);

      LegTypes legType = Class ? Class->getLegType((int)i) : LTIgnore;

      if (tr->Class != Class && legType != LTGroup) {
        tr->Class = Class;
        oe->classIdToRunnerHash.reset();
        tr->updateChanged();
      }

      tr->tNeedNoCard = false;

      if (Class) {
        pClass pc = Class;

        if (legType == LTIgnore) {
          tr->tNeedNoCard = true;
          if (lastStatus != StatusUnknown)
            tr->setStatus(max(tr->tStatus, lastStatus), false, changeType);
        }
        else
          lastStatus = tr->getStatus();

        StartTypes st = (actualClass == pc) ? pc->getStartType((int)i) : actualClass->getStartType(0);
        LegTypes lt = legType;

        if ((lt == LTParallel || lt == LTParallelOptional) && i == 0) {
          pc->setLegType(0, LTNormal);
          throw std::runtime_error("First leg cannot be parallel.");
        }

        if (lt == LTIgnore || lt == LTExtra) {
          if (st != STDrawn) tr->setStartTime(lastStartTime, false, changeType);
          tr->tUseStartPunch = (st == STDrawn);
        }
        else {
          switch (st) {
            case STDrawn:
              if (lt == LTParallel || lt == LTParallelOptional) {
                tr->setStartTime(lastStartTime, false, changeType);
                tr->tUseStartPunch = false;
              }
              else
                lastStartTime = tr->getStartTime();
              break;

            case STTime: {
              bool prs = false;
              if (tr->Card && freeStart) {
                pCourse crs = tr->getCourse(false);
                int startType = crs ? crs->getStartPunchType() : oPunch::PunchStart;
                oPunch *pnc = tr->Card->getPunchByType(startType);
                if (pnc && pnc->getAdjustedTime() > 0) {
                  prs = true;
                  lastStartTime = pnc->getAdjustedTime();
                }
              }
              if (!prs) {
                if (lt == LTNormal || lt == LTSum || lt == LTGroup) {
                  lastStartTime = (actualClass == pc) ? pc->getStartData((int)i) :
                                                        actualClass->getStartData(0);
                }
                tr->setStartTime(lastStartTime, false, changeType);
                tr->tUseStartPunch = false;
              }
            } break;

            case STChange: {
              int probeIndex = 1;
              int startData  = pc->getStartData((int)i);
              if (startData < 0) {
                probeIndex = -startData;
              }
              else {
                while ((int(i) - probeIndex) >= 0 && !Runners[i - probeIndex]) {
                  LegTypes tlt = pc->getLegType((int)(i - probeIndex));
                  if (tlt == LTIgnore || tlt == LTExtra || tlt == LTGroup) probeIndex++;
                  else break;
                }
              }

              if ((int(i) - probeIndex) >= 0 && Runners[i - probeIndex]) {
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
                }
                else {
                  ft = getBestStartTime(availableStartTimes);
                }

                if (ft <= 0) ft = 0;
                int restart = pc->getRestartTime((int)i);
                int rope    = pc->getRopeTime((int)i);
                if (((restart > 0 && rope > 0 && (ft == 0 || ft > rope)) ||
                     (ft == 0 && restart > 0)) &&
                    !preventRestart() && !tr->preventRestart()) {
                  ft = restart; tNumRestarts++;
                }
                if (ft >= 0) tr->setStartTime(ft, false, changeType);
                tr->tUseStartPunch = false;
                lastStartTime = ft;
              }
              else {
                tr->setStartTime(Class->getRestartTime((int)i), false, changeType);
                tr->tUseStartPunch = false;
              }
            } break;

            case STPursuit: {
              bool setStart = false;
              if (i > 0 && Runners[i - 1]) {
                if (lt == LTNormal || lt == LTSum || availableStartTimes.empty()) {
                  int rt = getLegRunningTimeUnadjusted((int)(i - 1), false, false);
                  if (rt > 0) setStart = true;
                  int leaderTime = pc->getTotalLegLeaderTime(oClass::AllowRecompute::NoUseOld, (int)(i - 1), false, false);
                  int timeAfter  = leaderTime > 0 ? rt - leaderTime : 0;
                  if (rt > 0 && timeAfter >= 0)
                    lastStartTime = pc->getStartData((int)i) + timeAfter;

                  int restart = pc->getRestartTime((int)i);
                  int rope    = pc->getRopeTime((int)i);
                  RunnerStatus hst = getLegStatus((int)(i - 1), false, false);
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
                }
                else if (lt == LTParallel || lt == LTParallelOptional) {
                  lastStartTime = getBestStartTime(availableStartTimes);
                  setStart = true;
                }

                if (tr->getFinishTime() > 0) {
                  setStart = true;
                  if (lastStartTime == 0) lastStartTime = pc->getRestartTime((int)i);
                }
                if (!setStart) lastStartTime = 0;
              }
              else
                lastStartTime = 0;

              tr->tUseStartPunch = false;
              tr->setStartTime(lastStartTime, false, changeType);
            } break;

            default: break;
          }
        }

        size_t nextNonPar = i + 1;
        while (nextNonPar < Runners.size() && pc->isOptional(nextNonPar) && !Runners[nextNonPar])
          nextNonPar++;

        int nextBaseLeg = (int)nextNonPar;
        while (nextNonPar < Runners.size() && pc->isParallel(nextNonPar))
          nextNonPar++;

        if (lt == LTExtra || (i + 1 < Runners.size() && pc->getLegType((int)(i + 1)) == LTExtra)) {
          if (lt != LTExtra) extraFinishTime = -1;
          if (tr->getFinishTime() > 0) {
            extraFinishTime = (extraFinishTime <= 0) ? tr->getFinishTime() :
                               min(extraFinishTime, tr->getFinishTime());
          }
        }
        else
          extraFinishTime = -1;

        if (nextNonPar < Runners.size()) {
          StartTypes nst = pc->getStartType((int)nextNonPar);
          int finishTime = tr->getFinishTime();
          if (lt == LTExtra) finishTime = extraFinishTime;

          if (nst == STDrawn || nst == STTime)
            availableStartTimes.clear();
          else if (finishTime > 0) {
            int nRCurrent = pc->getNumParallel((int)i);
            int nRNext    = pc->getNumParallel(nextBaseLeg);
            if (nRCurrent > 1 || nRNext > 1) {
              if (nRCurrent < nRNext) {
                for (int j = 0; j < nRNext / nRCurrent; j++)
                  availableStartTimes.push_back(finishTime);
              }
              else if (nRNext == 1)
                compressStartTimes(availableStartTimes, finishTime);
              else
                addStartTime(availableStartTimes, finishTime);
            }
            else
              availableStartTimes.clear();
          }
        }
      }
    }
  }

  if (!Runners.empty() && Runners[0])
    setStartTime(Runners[0]->getStartTime(), false, changeType);
  else if (Class && Class->getStartType(0) != STDrawn)
    setStartTime(Class->getStartData(0), false, changeType);

  setFinishTime(getLegFinishTime(-1));
  setStatus(getLegStatus(-1, false, false), false, changeType);
}

void oTeam::evaluate(ChangeType changeType) {
  apply(ChangeType::Quiet, nullptr);
  vector<pair<int, pControl>> mp;
  for (unsigned i = 0; i < Runners.size(); i++)
    if (Runners[i]) Runners[i]->evaluateCard(false, mp, 0, changeType);
  apply(changeType, nullptr);
  if (changeType == ChangeType::Update) {
    makeQuietChangePermanent();
    for (unsigned i = 0; i < Runners.size(); i++)
      if (Runners[i]) Runners[i]->synchronize(true);
    synchronize(true);
  }
}

void oTeam::adjustMultiRunners() {
  if (!Class) return;

  for (size_t k = Class->getNumStages(); k < Runners.size(); k++)
    setRunnerInternal((int)k, nullptr);

  if (Runners.size() != size_t(Class->getNumStages())) {
    Runners.resize(Class->getNumStages());
    updateChanged();
  }

  for (size_t i = 0; i < Runners.size(); i++) {
    if (!Class) continue;
    if (!Runners[i]) {
      unsigned lr = Class->getLegRunner((int)i);
      if (lr < i && Runners[lr]) {
        Runners[lr]->createMultiRunner(true, true);
        int dup = Class->getLegRunnerIndex((int)i);
        Runners[i] = Runners[lr]->getMultiRunner(dup);
      }
    }
    else if (Runners[i]->tParentRunner == nullptr && !Runners[i]->multiRunner.empty()) {
      Runners[i]->createMultiRunner(true, true);
    }
  }
  evaluate(ChangeType::Update);
}

void oTeam::correctRemove(pRunner r) {
  for (unsigned i = 0; i < Runners.size(); i++) {
    if (r && Runners[i] == r) {
      Runners[i] = nullptr;
      r->tInTeam = nullptr;
      r->tLeg    = 0;
      r->tLegEquClass = 0;
      correctionNeeded = true;
      r->correctionNeeded = true;
    }
  }
}

// ── Bibs ─────────────────────────────────────────────────────────────────────

void oTeam::setBib(const wstring &bib, int bibnumerical, bool updateStartNo) {
  if (updateStartNo)
    updateStartNo = !Class || !Class->lockedForking();

  if (getDI().setString("Bib", bib)) {
    if (oe) oe->bibStartNoToRunnerTeam.clear();
  }

  if (updateStartNo)
    setStartNo(bibnumerical, ChangeType::Update);
}

void oTeam::applyBibs() {
  BibMode bibMode = BibUndefined;
  wstring bib = getBib();

  for (size_t i = 0; i < Runners.size(); i++) {
    pRunner tr = Runners[i];
    if (tr) {
      pClass actualClass = tr->getClassRef(true);
      if (!actualClass) actualClass = Class;

      if (actualClass == Class) tr->setStartNo(StartNo, ChangeType::Update);

      if (bibMode == BibUndefined && Class) bibMode = Class->getBibMode();

      if (!bib.empty()) {
        if (bibMode == BibSame)
          tr->setBib(bib, 0, false);
        else if (bibMode == BibAdd) {
          wchar_t pattern[32], bf[32];
          int ibib = oClass::extractBibPattern(bib, pattern) + (int)i;
          swprintf(bf, sizeof(bf) / sizeof(wchar_t), pattern, ibib);
          tr->setBib(bf, 0, false);
        }
        else if (bibMode == BibLeg) {
          wstring rbib = bib + L"-" + Class->getLegNumber((int)i);
          tr->setBib(rbib, 0, false);
        }
      }
      else {
        if (bibMode == BibSame || bibMode == BibAdd || bibMode == BibLeg)
          tr->setBib(bib, 0, false);
      }
    }
  }
}

// ── Times ─────────────────────────────────────────────────────────────────────

int oTeam::getLegStartTime(int leg) const {
  if (leg == 0) return tStartTime;
  if (unsigned(leg) < Runners.size() && Runners[leg])
    return Runners[leg]->getStartTime();
  return 0;
}

wstring oTeam::getLegStartTimeS(int leg) const {
  int s = getLegStartTime(leg);
  if (s > 0) return oe->getAbsTime(s, SubSecond::Auto);
  return makeDash(L"-");
}

wstring oTeam::getLegStartTimeCompact(int leg) const {
  int s = getLegStartTime(leg);
  if (s > 0) return oe->getAbsTime(s, SubSecond::Auto);
  return makeDash(L"-");
}

int oTeam::getTimeAfter(int leg, bool allowUpdate) const {
  if (leg == -1) leg = (int)Runners.size() - 1;
  if (!Class || Class->tLeaderTime.size() <= unsigned(leg)) return -1;
  int t = getLegRunningTime(leg, true, false);
  if (t <= 0) return -1;
  return t - Class->getTotalLegLeaderTime(oClass::AllowRecompute::Yes, leg, true, false);
}

// ── Rogaining ─────────────────────────────────────────────────────────────────

int oTeam::getRogainingPoints(bool computed, bool multidayTotal) const {
  if (computed && tComputedPoints > 0) return tComputedPoints;

  int pt = 0;
  std::set<int> rogainingControls;
  for (size_t k = 0; k < Runners.size(); k++) {
    if (Runners[k]) {
      pCard c   = Runners[k]->getCard();
      pCourse crs = Runners[k]->getCourse(true);
      if (c && crs) {
        for (auto &p : c->punches) {
          if (rogainingControls.count(p.tMatchControlId) == 0) {
            rogainingControls.insert(p.tMatchControlId);
            pt += p.tRogainingPoints;
          }
        }
      }
    }
  }
  pt = max(pt - getRogainingReduction(true), 0);
  pt = max(0, pt + getPointAdjustment());
  if (multidayTotal) return pt + inputPoints;
  return pt;
}

int oTeam::getRogainingOvertime(bool computed) const {
  pCourse sampleCourse = nullptr;
  int overTime = 0;
  for (size_t k = 0; k < Runners.size(); k++) {
    if (Runners[k]) {
      if (!sampleCourse && (Runners[k]->tRogainingPoints > 0 || Runners[k]->tReduction > 0))
        sampleCourse = Runners[k]->getCourse(false);
      overTime += Runners[k]->tRogainingOvertime;
    }
  }
  if (sampleCourse && computed && tComputedTime > 0)
    overTime = max(0, tComputedTime - sampleCourse->getMaximumRogainingTime());
  return overTime;
}

int oTeam::getRogainingPointsGross(bool computed) const {
  int gross = 0;
  for (size_t k = 0; k < Runners.size(); k++)
    if (Runners[k]) gross += Runners[k]->tRogainingPointsGross;
  return gross;
}

int oTeam::getRogainingReduction(bool computed) const {
  pCourse sampleCourse = nullptr;
  int overTime = 0;
  for (size_t k = 0; k < Runners.size(); k++) {
    if (Runners[k]) {
      if (!sampleCourse && (Runners[k]->tRogainingPoints > 0 || Runners[k]->tReduction > 0))
        sampleCourse = Runners[k]->getCourse(false);
      overTime += Runners[k]->tRogainingOvertime;
    }
  }
  if (sampleCourse && computed && tComputedTime > 0) {
    overTime = max(0, tComputedTime - sampleCourse->getMaximumRogainingTime());
    return sampleCourse->calculateReduction(overTime);
  }
  return 0;
}

int oTeam::getRogainingPatrolPoints(bool multidayTotal) const {
  int madj = multidayTotal ? getInputPoints() : 0;
  if (tTeamPatrolRogainingAndVersion.first == (int)oe->dataRevision)
    return tTeamPatrolRogainingAndVersion.second.points + madj;

  tTeamPatrolRogainingAndVersion.first = (int)oe->dataRevision;
  tTeamPatrolRogainingAndVersion.second.reset();

  // Simplified: just return same as getRogainingPoints(false, false)
  int pts = getRogainingPoints(false, false);
  tTeamPatrolRogainingAndVersion.second.points = pts;
  return pts + madj;
}

int oTeam::getRogainingPatrolReduction() const {
  getRogainingPatrolPoints(false);
  return tTeamPatrolRogainingAndVersion.second.reduction;
}

int oTeam::getRogainingPatrolOvertime() const {
  getRogainingPatrolPoints(false);
  return tTeamPatrolRogainingAndVersion.second.overtime;
}

// ── Remove ─────────────────────────────────────────────────────────────────────

void oTeam::remove() {
  if (oe) oe->removeTeam(Id);
}

bool oTeam::canRemove() const { return true; }

// ── Display name helpers ──────────────────────────────────────────────────────

wstring oTeam::getDisplayName() const {
  if (!Class) return sName;
  ClassType ct = Class->getClassType();
  if (ct == oClassIndividRelay || ct == oClassPatrol) {
    if (Club) {
      wstring cname = getDisplayClub();
      if (!cname.empty()) return cname;
    }
  }
  return sName;
}

wstring oTeam::getDisplayClub() const {
  vector<pClub> clubs;
  if (Club) clubs.push_back(Club);
  for (size_t k = 0; k < Runners.size(); k++) {
    if (Runners[k] && Runners[k]->Club)
      if (count(clubs.begin(), clubs.end(), Runners[k]->Club) == 0)
        clubs.push_back(Runners[k]->Club);
  }
  if (clubs.empty()) return L"";
  if (clubs.size() == 1) return clubs[0]->getDisplayName();
  wstring res;
  for (size_t k = 0; k < clubs.size(); k++) {
    if (k == 0) res = clubs[k]->getDisplayName();
    else        res += L" / " + clubs[k]->getDisplayName();
  }
  return res;
}

// ── Input data ────────────────────────────────────────────────────────────────

void oTeam::setInputData(const oTeam &t) {
  inputTime   = t.getTotalRunningTime();
  inputStatus = t.getTotalStatus();
  inputPoints = t.getRogainingPoints(true, true);
  inputPlace  = t.getTotalPlace(true);

  oDataInterface dest = getDI();
  oDataConstInterface src = t.getDCI();
  dest.setInt("TransferFlags", src.getInt("TransferFlags"));
  dest.setString("Nationality", src.getString("Nationality"));
  dest.setString("Country",     src.getString("Country"));
  dest.setInt("Fee",            src.getInt("Fee"));
  dest.setInt("Paid",           src.getInt("Paid"));
  dest.setInt("Taxable",        src.getInt("Taxable"));
}

// ── Entry date ────────────────────────────────────────────────────────────────

wstring oTeam::getEntryDate(bool /*dummy*/) const {
  oDataConstInterface dci = getDCI();
  int date = dci.getInt("EntryDate");
  if (date == 0 && !isVacant()) {
    auto di = const_cast<oTeam*>(this)->getDI();
    di.setDate("EntryDate", getLocalDate());
    di.setInt("EntryTime", getLocalAbsTime());
  }
  return dci.getDate("EntryDate");
}

// ── Misc ──────────────────────────────────────────────────────────────────────

int oTeam::getTeamFee() const {
  int f = getDCI().getInt("Fee");
  for (size_t k = 0; k < Runners.size(); k++)
    if (Runners[k]) f += Runners[k]->getDCI().getInt("Fee");
  return f;
}

int oTeam::getRanking() const {
  for (size_t k = 0; k < Runners.size(); k++)
    if (Runners[k]) return Runners[k]->getRanking();
  return MaxRankingConstant;
}

int oTeam::getNumShortening() const { return getNumShortening(-1); }

int oTeam::getNumShortening(int leg) const {
  int ns = 0;
  if (Class) {
    for (size_t k = 0; k < Runners.size() && (leg == -1 || int(k) <= leg); k++)
      if (Runners[k] && !Class->isOptional(k)) ns += Runners[k]->getNumShortening();
  }
  else {
    for (size_t k = 0; k < Runners.size() && (leg == -1 || int(k) <= leg); k++)
      if (Runners[k]) ns += Runners[k]->getNumShortening();
  }
  return ns;
}

bool oTeam::checkValdParSetup() {
  if (!Class) return false;
  bool cor = false;
  for (size_t k = 0; k < Runners.size(); k++) {
    if (!Class->isOptional(k) && !Class->isParallel(k) && !Runners[k]) {
      int m = 1;
      while ((m + (int)k) < (int)Runners.size() &&
             (Class->isOptional(k + m) || Class->isParallel(k + m))) {
        if (Runners[k + m]) {
          Runners[k] = Runners[k + m];
          Runners[k]->tLeg = (int)k;
          Runners[k + m] = nullptr;
          updateChanged();
          cor = true;
          k += m; break;
        }
        else m++;
      }
    }
  }
  return cor;
}

// ── Comparison ────────────────────────────────────────────────────────────────

bool oTeam::compareResult(const oTeam &a, const oTeam &b) {
  if (a.Class != b.Class) {
    if (a.Class) {
      if (b.Class) return a.Class->tSortIndex < b.Class->tSortIndex ||
                          (a.Class->tSortIndex == b.Class->tSortIndex && a.Class->Id < b.Class->Id);
      else return true;
    }
    else return false;
  }
  else if (a.tmpSortStatus != b.tmpSortStatus)
    return a.tmpSortStatus < b.tmpSortStatus;
  else if (a.tmpSortTime != b.tmpSortTime)
    return a.tmpSortTime < b.tmpSortTime;

  const wstring &as = a.getBib();
  const wstring &bs = b.getBib();
  if (as != bs) return compareBib(as, bs);

  int aix = a.getDCI().getInt("SortIndex");
  int bix = b.getDCI().getInt("SortIndex");
  if (aix != bix) {
    if (aix == 0) aix = std::numeric_limits<int>::max();
    if (bix == 0) bix = std::numeric_limits<int>::max();
    return aix < bix;
  }
  return compareStringIgnoreCase(a.sName, b.sName) < 0;
}

bool oTeam::compareResultNoSno(const oTeam &a, const oTeam &b) {
  if (a.Class != b.Class) {
    if (a.Class) {
      if (b.Class) return a.Class->tSortIndex < b.Class->tSortIndex ||
                          (a.Class->tSortIndex == b.Class->tSortIndex && a.Class->Id < b.Class->Id);
      else return true;
    }
    else return false;
  }
  else if (a.tmpSortStatus != b.tmpSortStatus)
    return a.tmpSortStatus < b.tmpSortStatus;
  else if (a.tmpSortTime != b.tmpSortTime)
    return a.tmpSortTime < b.tmpSortTime;

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
  return compareStringIgnoreCase(a.sName, b.sName) < 0;
}

bool oTeam::compareResultClub(const oTeam &a, const oTeam &b) {
  pClub ca = a.getClubRef();
  pClub cb = b.getClubRef();
  if (ca != cb) {
    int cres = compareClubs(ca, cb);
    if (cres != 2) return cres != 0;
  }
  return compareResult(a, b);
}

bool oTeam::compareSNO(const oTeam &a, const oTeam &b) {
  const wstring &as = a.getBib();
  const wstring &bs = b.getBib();
  if (as != bs) return compareBib(as, bs);
  if (a.Class != b.Class) {
    if (a.Class) {
      if (b.Class) return a.Class->tSortIndex < b.Class->tSortIndex ||
                          (a.Class->tSortIndex == b.Class->tSortIndex && a.Class->Id < b.Class->Id);
      else return true;
    }
    else return false;
  }
  return compareStringIgnoreCase(a.sName, b.sName) < 0;
}

// ── Dynamic status ────────────────────────────────────────────────────────────

DynamicRunnerStatus oTeam::getDynamicStatus() const {
  if (tStatus == StatusNotCompeting || tStatus == StatusCANCEL)
    return DynamicRunnerStatus::StatusInactive;

  bool allFinish = true, someActive = false;
  for (pRunner r : Runners) {
    if (r) {
      DynamicRunnerStatus st = r->getDynamicStatus();
      if (st == DynamicRunnerStatus::StatusActive) {
        someActive = true; allFinish = false; break;
      }
      else if (st != DynamicRunnerStatus::StatusFinished)
        allFinish = false;
    }
  }
  if (someActive) return DynamicRunnerStatus::StatusActive;
  if (allFinish && tStatus != StatusDNS) return DynamicRunnerStatus::StatusFinished;
  return DynamicRunnerStatus::StatusInactive;
}

// ── Match ─────────────────────────────────────────────────────────────────────

bool oTeam::matchAbstractRunner(const oAbstractRunner *target) const {
  if (!target) return false;
  if (target == this) return true;
  const oRunner *r = dynamic_cast<const oRunner*>(target);
  return r && r->getTeam() == this;
}

// ── Race info ─────────────────────────────────────────────────────────────────

const pair<wstring, int> oTeam::getRaceInfo() {
  pair<wstring, int> res;
  RunnerStatus baseStatus = getStatus();
  int rtActual = getRunningTime(false);
  if (isResultStatus(baseStatus) || (isPossibleResultStatus(baseStatus) && rtActual > 0)) {
    int p          = getPlace(true);
    int rtComp     = getRunningTime(true);
    int pointsComp = getRogainingPoints(true, false);
    RunnerStatus compStatus = getStatusComputed(true);
    bool ok = compStatus == StatusOK || compStatus == StatusOutOfCompetition || compStatus == StatusNoTiming;
    res.second = ok ? 1 : -1;
    if (compStatus == baseStatus && rtComp == rtActual) {
      if (ok && p > 0) res.first = lang.tl("Placering: ") + itow(p) + L".";
    }
    else {
      if (ok) {
        res.first = lang.tl("Resultat: ");
        if (compStatus != baseStatus) res.first += oe->formatStatus(compStatus, true) + L", ";
        res.first += formatTime(rtComp);
        if (p > 0) res.first += L" (" + itow(p) + L")";
      }
      else if (compStatus != baseStatus)
        res.first = lang.tl("Resultat: ") + oe->formatStatus(compStatus, true);
    }
  }
  return res;
}

// ── Merge ─────────────────────────────────────────────────────────────────────

void oTeam::merge(const oBase &input, const oBase *baseIn) {
  oAbstractRunner::merge(input, baseIn);

  const oTeam &src  = dynamic_cast<const oTeam&>(input);
  const oTeam *base = dynamic_cast<const oTeam*>(baseIn);

  auto getRId = [](const oTeam &t, int ix) {
    pRunner r = t.getRunner(ix);
    return r ? r->getId() : 0;
  };

  bool chR;
  if (base) {
    chR = base->Runners.size() != src.Runners.size();
    if (!chR) {
      for (size_t i = 0; i < src.Runners.size(); i++)
        if (getRId(src, (int)i) != getRId(*base, (int)i)) { chR = true; break; }
    }
  }
  else chR = true;

  if (chR) {
    bool same = src.Runners.size() == Runners.size();
    vector<int> r(src.Runners.size());
    for (size_t i = 0; i < src.Runners.size(); i++) {
      if (src.Runners[i]) {
        r[i] = src.Runners[i]->Id;
        src.Runners[i]->tInTeam = nullptr;
      }
      if (same) {
        int rc = Runners[i] ? Runners[i]->Id : 0;
        if (rc != r[i]) same = false;
      }
    }
    if (!same) { importRunners(r); updateChanged(); }
  }

  if (getDI().merge(input, base)) updateChanged();
  synchronize(true);
}

// ── removeRunner (GUI helper — simplified) ────────────────────────────────────

void oTeam::removeRunner(gdioutput &/*gdi*/, bool /*askRemoveRunner*/, int i) {
  setRunner(i, nullptr, true);
  if (Class) {
    for (unsigned k = i + 1; k < Class->getNumStages(); k++)
      if (Class->getLegRunner(k) == i) setRunner(k, nullptr, true);
  }
}

// ── changeId ──────────────────────────────────────────────────────────────────

void oTeam::changeId(int newId) {
  pTeam old;
  if (oe->teamById.lookup(Id, old) && old == this)
    oe->teamById.remove(Id);
  oBase::changeId(newId);
  oe->teamById[newId] = this;
}

// ── Static helpers ────────────────────────────────────────────────────────────

void oTeam::checkClassesWithReferences(oEvent &oe, std::set<int> &clsWithRef) {
  vector<pRunner> r;
  oe.getRunners(-1, -1, r, false);
  map<int, pair<int, int>> pairedUnpaired;
  for (size_t k = 0; k < r.size(); k++) {
    if (r[k]->getReference())
      ++pairedUnpaired[r[k]->getClassId(false)].first;
    else
      ++pairedUnpaired[r[k]->getClassId(false)].second;
  }
  for (auto &it : pairedUnpaired)
    if (it.second.first > it.second.second) clsWithRef.insert(it.first);
}

void oTeam::convertClassWithReferenceToPatrol(oEvent &/*oe*/, const std::set<int> &/*clsWithRef*/) {}

// ── Table stubs ───────────────────────────────────────────────────────────────

const shared_ptr<Table> &oTeam::getTable(oEvent *oe) {
  if (!oe->hasTable("team")) {
    auto table = make_shared<Table>(oe, 20, L"Teams", "teams");
    oe->setTable("team", table);
  }
  return oe->getTable("team");
}

void oTeam::addTableRow(Table &/*table*/) const {}

pair<int, bool> oTeam::inputData(int /*id*/, const wstring &/*input*/,
                                 int /*inputId*/, wstring &/*output*/, bool /*noUpdate*/) {
  return {0, false};
}

void oTeam::fillInput(int /*id*/, vector<pair<wstring, size_t>> &/*out*/, size_t &/*selected*/) {}

// ── Speaker stub ─────────────────────────────────────────────────────────────

void oTeam::fillSpeakerObject(int /*leg*/, int /*previousControlCourseId*/,
                               const vector<int> &/*courseControlIds*/,
                               bool /*totalResult*/, oSpeakerObject &/*spk*/) const {}

// ── oEvent team management ────────────────────────────────────────────────────

pTeam oEvent::addTeam(const wstring &pname, int ClubId, int ClassId) {
  oTeam t(this);
  t.sName = pname;
  if (ClubId > 0) t.Club = getClub(ClubId);
  if (ClassId > 0) t.Class = getClass(ClassId);
  bibStartNoToRunnerTeam.clear();
  Teams.push_back(t);
  pTeam pt = &Teams.back();
  pt->addToEvent(this, &t);
  teamById[t.Id] = pt;
  pt->apply(ChangeType::Quiet, nullptr);
  pt->makeQuietChangePermanent();
  pt->updateChanged();
  return pt;
}

pTeam oEvent::addTeam(const oTeam &t) {
  if (t.Id == 0) return nullptr;
  if (getTeam(t.Id)) return nullptr;
  bibStartNoToRunnerTeam.clear();
  Teams.push_back(t);
  pTeam pt = &Teams.back();
  pt->addToEvent(this, &t);
  for (size_t i = 0; i < pt->Runners.size(); i++) {
    if (pt->Runners[i]) {
      pt->Runners[i]->tInTeam = pt;
      pt->Runners[i]->tLeg    = (int)i;
    }
  }
  teamById[t.Id] = pt;
  return pt;
}

pTeam oEvent::getTeam(int Id) const {
  pTeam value;
  if (teamById.lookup(Id, value) && value) {
    if (value->isRemoved()) return nullptr;
    return value;
  }
  return nullptr;
}

void oEvent::getTeams(int classId, vector<pTeam> &t, bool /*sort*/) const {
  t.clear();
  for (auto &tm : Teams) {
    if (tm.isRemoved()) continue;
    if (classId <= 0 || tm.getClassId(false) == classId)
      t.push_back(const_cast<pTeam>(&tm));
  }
}

void oEvent::removeTeam(int Id) {
  for (auto it = Teams.begin(); it != Teams.end(); ++it) {
    if (it->getId() == Id) {
      dataRevision++;
      it->prepareRemove();
      Teams.erase(it);
      teamById.remove(Id);
      return;
    }
  }
}

int oEvent::getFreeTeamId() {
  return ++qFreeTeamId;
}

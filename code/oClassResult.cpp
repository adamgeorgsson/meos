/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2026 Melin Software HB

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Melin Software HB - software@melin.nu - www.melin.nu
    Eksoppsvägen 16, SE-75646 UPPSALA, Sweden

************************************************************************/

// oClassResult.cpp: oClass leader time, split analysis, and configuration split from oClass.cpp
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <iostream>
#include <cassert>
#include <limits>
#include "oClass.h"
#include "oEvent.h"
#include "Table.h"
#include "meos_util.h"
#include "localizer.h"
#include <algorithm>
#include "inthashmap.h"
#include "intkeymapimpl.hpp"
#include "MeOSFeatures.h"
#include "gdioutput.h"
#include "gdistructures.h"
#include "meosexception.h"
#include "random.h"
#include "qualification_final.h"

oClass::LeaderInfo &oClass::getLeaderInfo(AllowRecompute recompute, int leg) const {
  if (recompute == AllowRecompute::Yes && leaderTimeVersion != oe->dataRevision)
    updateLeaderTimes();

  leg = mapLeg(leg);
  if (leg < 0)
    throw meosException();

  if (recompute == AllowRecompute::NoUseOld && size_t(leg) < tLeaderTimeOld.size())
    return tLeaderTimeOld[leg];

  if (size_t(leg) >= tLeaderTime.size())
    tLeaderTime.resize(leg + 1);

  return tLeaderTime[leg];
}

bool oClass::LeaderInfo::updateComputed(int rt, Type t) {
  if (rt <= 0)
    return false;
  bool update = false;

  switch (t) {
  case Type::Leg:
    if (bestTimeOnLegComputed <= 0 || bestTimeOnLegComputed > rt)
      bestTimeOnLegComputed = rt, update = true;
    break;

  case Type::Total:
    if (totalLeaderTimeComputed <= 0 || totalLeaderTimeComputed > rt)
      totalLeaderTimeComputed = rt, update = true;
    break;

  case Type::TotalInput:
    if (totalLeaderTimeInputComputed <= 0 || totalLeaderTimeInputComputed > rt)
      totalLeaderTimeInputComputed = rt, update = true;
    break;
  default:
    assert(false);
  }
  return update;
}

bool oClass::LeaderInfo::update(int rt, Type t) {
  if (rt <= 0)
    return false;
  bool update = false;
  switch (t) {
  case Type::Leg:
    if (rt >= 0 && (bestTimeOnLeg < 0 || bestTimeOnLeg > rt))
      bestTimeOnLeg = rt, update = true;
    break;

  case Type::Total:
    if (rt >= 0 && (totalLeaderTime < 0 || totalLeaderTime > rt))
      totalLeaderTime = rt, update = true;
    break;

  case Type::TotalInput:
    if (rt >= 0 && (totalLeaderTimeInput < 0 || totalLeaderTimeInput > rt))
      totalLeaderTimeInput = rt, update = true;
    break;
  
  case Type::Input:
    if (rt >= 0 && (inputTime < 0 || inputTime > rt))
      inputTime = rt, update = true;
    break;
  default:
    assert(false);
  }
  return update;
}

void oClass::updateLeaderTimes() const {
  resetLeaderTime();
  vector<pRunner> runners;
  oe->getRunners(Id, 0, runners, false);
  bool needupdate = true;
  int leg = 0;
  while (needupdate) {
    needupdate = false;
    for (auto r : runners) {
      int rLeg = r->tLeg;
      if (r->Class != this)
        rLeg = mapLeg(rLeg);
      if (rLeg == leg)
        r->storeTimes();
      else if (rLeg > leg)
        needupdate = true;
    }
    if (leg >= tLeaderTime.size())
      break;
    tLeaderTime[leg].setComplete();
    leg++;
  }
  leaderTimeVersion = oe->dataRevision;
}

void oClass::LeaderInfo::resetComputed(Type t) {
  switch (t) {
  case Type::Leg:
      bestTimeOnLegComputed = 0;
    break;

  case Type::Total:
      totalLeaderTimeComputed = 0;
    break;

  case Type::TotalInput:
      totalLeaderTimeInputComputed = 0;
    break;
  }
}

int oClass::LeaderInfo::getLeader(Type t, bool computed) const {
  switch (t) {
  case Type::Leg:
    if (computed && bestTimeOnLegComputed > 0)
      return bestTimeOnLegComputed;
    else
      return bestTimeOnLeg;

  case Type::Total:
    if (computed && totalLeaderTimeComputed > 0)
      return totalLeaderTimeComputed;
    else
      return totalLeaderTime;
   
  case Type::TotalInput:
    if (computed && totalLeaderTimeInputComputed > 0)
      return totalLeaderTimeInputComputed;
    else if (totalLeaderTimeInput > 0)
      return totalLeaderTimeInput;
    else
      return inputTime;
  }

  return 0;
}

int oClass::getBestLegTime(AllowRecompute recompute, int leg,  bool computedTime) const {
  leg = mapLeg(leg);
  if (unsigned(leg) >= tLeaderTime.size())
    return 0;
  int bt = getLeaderInfo(recompute, leg).getLeader(LeaderInfo::Type::Leg, computedTime);
  if (bt == -1 && recompute == AllowRecompute::Yes) {
    updateLeaderTimes();
    bt = tLeaderTime[leg].getInputTime();
  }
  return bt;
}

int oClass::getBestTimeCourse(AllowRecompute recompute, int courseId) const {
  if (recompute == AllowRecompute::Yes && leaderTimeVersion != oe->dataRevision)
    updateLeaderTimes();

  map<int, int>::const_iterator res = tBestTimePerCourse.find(courseId);
  if (res == tBestTimePerCourse.end())
    return 0;
  else
    return res->second;
}

int oClass::getBestInputTime(AllowRecompute recompute, int leg) const {
  leg = mapLeg(leg);

  if (unsigned(leg)>=tLeaderTime.size())
    return 0;
  else {
    int it = getLeaderInfo(recompute, leg).getInputTime();
    if (it == -1 && recompute == AllowRecompute::Yes) {
      updateLeaderTimes();
      it = getLeaderInfo(AllowRecompute::No, leg).getInputTime();
    }
    return -1;
  }
}

int oClass::getTotalLegLeaderTime(AllowRecompute recompute, int leg, bool computedTime, bool includeInput) const {
  leg = mapLeg(leg);
  if (unsigned(leg) >= tLeaderTime.size())
    return 0;

  int res = -1;
  int iter = -1;
  bool mayUseOld = recompute == AllowRecompute::NoUseOld;
  if (mayUseOld)
    recompute = AllowRecompute::No;

  while (res == -1 && ++iter<2) {
    if (includeInput)
      res = getLeaderInfo(recompute, leg).getLeader(LeaderInfo::Type::TotalInput, computedTime);
    else
      res = getLeaderInfo(recompute, leg).getLeader(LeaderInfo::Type::Total, computedTime);

    if (res == -1 && recompute == AllowRecompute::Yes) {
      recompute = AllowRecompute::No;
      updateLeaderTimes();
    }
    else if (res == -1 && mayUseOld)
      recompute = AllowRecompute::NoUseOld;
  }
  return res;
}

void oClass::mergeClass(int classIdSec) {
  vector<pTeam> t;
  vector<pRunner> r;
  vector<pRunner> rThis;

  oe->getRunners(classIdSec, 0, rThis, true);

  // Update teams
  oe->getTeams(classIdSec, t, true);
  oe->getRunners(classIdSec, 0, r, false);

  for (size_t k = 0; k < t.size(); k++) {
    pTeam it = t[k];
    it->Class = this;
    it->updateChanged();
    for (size_t k=0;k<it->Runners.size();k++)  {
      if (it->Runners[k]) {
        it->Runners[k]->Class = this;
        it->Runners[k]->updateChanged();
      }
    }
    it->synchronize(); //Synchronizes runners also  
  }

  // Update runners
  for (size_t k = 0; k < r.size(); k++) {
    pRunner it = r[k];
    it->Class = this;
    it->updateChanged();
    it->synchronize();
  }
  oe->classIdToRunnerHash.reset();
  // Check heats
  
  int maxHeatThis = 0;
  bool missingHeatThis = false, uniqueHeatThis = true;
  for (size_t k = 0; k < rThis.size(); k++) {
    int heat = rThis[k]->getDCI().getInt("Heat");
    if (heat == 0)
      missingHeatThis = true;
    if (maxHeatThis != 0 && heat != maxHeatThis)
      uniqueHeatThis = false;
    maxHeatThis = max(maxHeatThis, heat);
  }

  int maxHeatOther = 0;
  bool missingHeatOther = false, uniqueHeatOther = true;
  for (size_t k = 0; k < r.size(); k++) {
    int heat = r[k]->getDCI().getInt("Heat");
    if (heat == 0)
      missingHeatOther = true;
    if (maxHeatOther != 0 && heat != maxHeatOther)
      uniqueHeatOther = false;
    maxHeatOther = max(maxHeatOther, heat);
  }
  int heatForNext = 1;
  if (missingHeatThis) {
    for (size_t k = 0; k < rThis.size(); k++) {
      int heat = rThis[k]->getDCI().getInt("Heat");
      if (heat == 0) {
        if (uniqueHeatThis && maxHeatThis > 0)
          heat = maxHeatThis; // Some runners are missing the heat info. Fill in.
        else {
          // If maxHeatthis> 0, data somehow corrupted:
          // Some runners have heat, but not unqiue, 
          // others are missing. Heats not well defined.
          heat = maxHeatThis + 1; 
        }
      }
      heatForNext = max(heatForNext, heat+1);
      rThis[k]->getDI().setInt("Heat", heat);
    }
  }

  if (missingHeatOther) {
    for (size_t k = 0; k < r.size(); k++) {
      int heat = r[k]->getDCI().getInt("Heat");
      if (heat == 0) {
        if (maxHeatOther == 0)
          heat = heatForNext; // No runner had a heat, set to next heat
        else if (uniqueHeatOther)
          heat = maxHeatOther; // Some runner missing the heat. Use the defined heat.
        else
          heat = maxHeatOther + 1; // Data corrupted, see above. Make a unique heat.
      }
      r[k]->getDI().setInt("Heat", heat);
    }
  }
  // Write back
  for (size_t k = 0; k < t.size(); k++) {
    t[k]->synchronize(true); //Synchronizes runners also  
  }
  for (size_t k = 0; k < r.size(); k++) {
    r[k]->synchronize(true);
  }
  for (size_t k = 0; k < rThis.size(); k++) {
    rThis[k]->synchronize(true);
  }

  oe->removeClass(classIdSec);
}

void oClass::getSplitMethods(vector< pair<wstring, size_t> > &methods) {
  methods.clear();
  methods.push_back(make_pair(lang.tl("Dela klubbvis"), SplitClub));
  methods.push_back(make_pair(lang.tl("Dela slumpmässigt"), SplitRandom));
  methods.push_back(make_pair(lang.tl("Dela efter ranking"), SplitRank));
  methods.push_back(make_pair(lang.tl("Dela efter placering"), SplitResult));
  methods.push_back(make_pair(lang.tl("Dela efter tid"), SplitTime));
  methods.push_back(make_pair(lang.tl("Jämna klasser (ranking)"), SplitRankEven));
  methods.push_back(make_pair(lang.tl("Jämna klasser (placering)"), SplitResultEven));
  methods.push_back(make_pair(lang.tl("Jämna klasser (tid)"), SplitTimeEven));
}

class ClassSplit {
private:
  map<int, int> clubSize;
  map<int, int> idSplit;
  map<int, int> clubSplit;
  vector<const oAbstractRunner*> runners;
  void splitClubs(const vector<int> &parts);
  void valueSplit(const vector<int> &parts, vector< pair<int, int> > &valueId);
  void valueEvenSplit(const vector<int> &parts, vector< pair<int, int> > &valueId);

public: 
  static int evaluateTime(const oAbstractRunner &r) {
    if (r.getInputStatus() == StatusOK) {
      int t = r.getInputTime();
      if (t > 0)
        return t;
      else
        return timeConstHour * 24 * 8;
    }
    else {
      return timeConstHour * 24 * 8 + r.getId();
    }
  }

  static int evaluateResult(const oAbstractRunner &r) {
    int baseRes;
    if (r.getInputStatus() == StatusOK) {
      int t = r.getInputPlace();
      
      if (t == 0) {
        const oRunner *rr = dynamic_cast<const oRunner *>(&r);
        if (rr && rr->getTeam() && rr->getLegNumber() > 0) {
          const pRunner rPrev = rr->getTeam()->getRunner(rr->getLegNumber() - 1);
          if (rPrev && rPrev->getStatus() == StatusOK)
            t = rPrev->getPlace();
        }
      }
      
      if (t > 0)
        baseRes = t;
      else
        baseRes = 99999;
    }
    else {
      baseRes = 99999 + r.getInputStatus();
    }
    return r.getDCI().getInt("Heat") + 1000 * baseRes;
  }

  static int evaluatePoints(const oAbstractRunner &r) {
    if (r.getInputStatus() == StatusOK) {
      int p = r.getInputPoints();
      if (p > 0)
        return 1000*1000*1000 - p;
      else
        return 1000*1000*1000;
    }
    else {
      return 1000*1000*1000 + r.getInputStatus();
    }
  }

private:
  int evaluate(const oAbstractRunner &r, ClassSplitMethod method) {
    switch (method) {
      case SplitRank:
      case SplitRankEven:
        return r.getRanking();
      case SplitTime:
      case SplitTimeEven:
        return evaluateTime(r);
      case SplitResult:
      case SplitResultEven:
        return evaluateResult(r);
      default:
       throw meosException("Not yet implemented");
    }
  }
public:
  void addMember(const oAbstractRunner &r) {
    ++clubSize[r.getClubId()];
    runners.push_back(&r);
  }

  void split(const vector<int> &parts, ClassSplitMethod method);

  int getClassIndex(const oAbstractRunner &r) {
    if (clubSplit.count(r.getClubId()))
      return clubSplit[r.getClubId()];
    else if (idSplit.count(r.getId()))
      return idSplit[r.getId()];
    throw meosException("Internal split error");
  }
};

void ClassSplit::split(const vector<int> &parts, ClassSplitMethod method) {
  switch (method) {
    case SplitClub:
      splitClubs(parts);
    break;

    case SplitRank:
    case SplitTime:
    case SplitResult: {
      vector< pair<int, int> > v(runners.size());
      for (size_t k = 0; k < v.size(); k++) {
        v[k].second = runners[k]->getId();
        v[k].first = evaluate(*runners[k], method);
      }
      valueSplit(parts, v);
    } break;

    case SplitRankEven:
    case SplitTimeEven:
    case SplitResultEven: {
      vector< pair<int, int> > v(runners.size());
      for (size_t k = 0; k < v.size(); k++) {
        v[k].second = runners[k]->getId();
        v[k].first = evaluate(*runners[k], method);
      }
      valueEvenSplit(parts, v);
    } break;


    case SplitRandom: {
      vector<int> r(runners.size());
      for (size_t k = 0; k < r.size(); k++) {
        r[k] = k;
      }
      permute(r);
      vector< pair<int, int> > v(runners.size());
      for (size_t k = 0; k < v.size(); k++) {
        v[k].second = runners[k]->getId();
        v[k].first = r[k];
      }
      valueEvenSplit(parts, v);
      break;
    }
    default:
      throw meosException("Not yet implemented");
  }
}

void ClassSplit::splitClubs(const vector<int> &parts) {
  vector<int> classSize(parts);
  while ( !clubSize.empty() ) {
    // Find largest club
    int club=0;
    int size=0;
    for (map<int, int>::iterator it=clubSize.begin(); it!=clubSize.end(); ++it) {
      if (it->second>size) {
        club = it->first;
        size = it->second;
      }
    }
    clubSize.erase(club);
    // Find smallest class (e.g. highest number of remaining)
    int nrunner = -1000000;
    int cid = 0;

    for(size_t k = 0; k < parts.size(); k++) {
      if (classSize[k]>nrunner) {
        nrunner = classSize[k];
        cid = k;
      }
    }

    //Store result
    clubSplit[club] = cid;
    classSize[cid] -= size;
  }
}

void ClassSplit::valueSplit(const vector<int> &parts, vector< pair<int, int> > &valueId) {
  sort(valueId.begin(), valueId.end());

  int partIx = 0;
  int partCount = 0;
  for (size_t k = 0; k < valueId.size(); ) {
    int refValue = valueId[k].first;
    for (; k < valueId.size() && valueId[k].first == refValue; k++) {
      idSplit[valueId[k].second] = partIx;
      partCount++;
    } 

    if (k < valueId.size() && partCount >= parts[partIx] && size_t(partIx + 1) < parts.size()) {
      partIx++;
      partCount = 0;
    }
  }

  if (partIx == 0) {
    throw meosException("error:invalidmethod");
  }
}

void ClassSplit::valueEvenSplit(const vector<int> &parts, vector< pair<int, int> > &valueId) {
  sort(valueId.begin(), valueId.end());
  if (valueId.empty() || valueId.front().first == valueId.back().first) {
    throw meosException("error:invalidmethod");
  }

  vector<int> count(parts.size());
  bool odd = true;
  bool useRandomAssign = false;

  for (size_t k = 0; k < valueId.size(); ) {
    vector<int> distr;

    for (size_t j = 0; k < valueId.size() && j < parts.size(); j++) {
      if (count[j] < parts[j]) {
        distr.push_back(valueId[k++].second);
      }
    }
    if (distr.empty()) {
      idSplit[valueId[k++].second] = parts.size()-1; // Out of space, use last for rest
    }
    else {
      if (useRandomAssign) {
        permute(distr); //Random assignment to groups
      }
      else {
        // Use reverse/forward distribution. Swedish SM rules
        if (odd) 
          reverse(distr.begin(), distr.end());
        odd = !odd;
      }

      for (size_t j = 0; j < parts.size(); j++) {
        if (count[j] < parts[j]) {
          ++count[j];
          idSplit[distr.back()] = j;
          distr.pop_back();
        }
      }
    }
  }
}


void oClass::splitClass(ClassSplitMethod method, const vector<int> &parts, vector<int> &outClassId) {
  if (parts.size() <= 1)
    return;
  bool qf = false;
  set<int> clsIdSrc;
  clsIdSrc.insert(getId());

  if (getQualificationFinal()) {
    // Works for base classes
    set<int> base;
    getQualificationFinal()->getBaseClassInstances(base);
    assert(base.size() == parts.size());
    qf = true;
    for (int inst : base)
      clsIdSrc.insert(getVirtualClass(inst)->getId());
  }

  bool defineHeats = method == SplitRankEven || method == SplitResultEven;
  
  ClassSplit cc;
  vector<pTeam> t;
  vector<pRunner> r;
  
  if (!qf && oe->classHasTeams(getId()) ) {
    for (int clsId : clsIdSrc) {
      vector<pTeam> tTmp;
      oe->getTeams(clsId, tTmp, true);
      for (auto tk : tTmp) {
        t.push_back(tk);
        cc.addMember(*tk);
      }
    }
  }
  else {
    for (int clsId : clsIdSrc) {
      vector<pRunner> rTmp;
      oe->getRunners(clsId, 0, rTmp, true);
      for (auto rk : rTmp) {
        if (qf && rk->getLegNumber() != 0)
          continue;

        r.push_back(rk);
        cc.addMember(*rk);
      }
    }
  }
  
  // Split teams.
  cc.split(parts, method);

  vector<pClass> pcv(parts.size());
  outClassId.resize(parts.size());
  if (qf) {
    set<int> base;
    getQualificationFinal()->getBaseClassInstances(base);
    int ix = 0;
    for (int inst : base) {
      pcv[ix] = getVirtualClass(inst);
      outClassId[ix] = pcv[ix]->getId();
      ix++;
    }
  }
  else {
    pcv[0] = this;
    outClassId[0] = getId();

    pcv[0]->getDI().setInt("Heat", defineHeats ? 1 : 0);
    pcv[0]->synchronize(true);

    int lastSI = getDI().getInt("SortIndex");
    for (size_t k = 1; k < parts.size(); k++) {
      pcv[k] = oe->addClass(getName() + makeDash(L"-") + itow(k + 1), getCourseId());
      if (pcv[k]) {
        // Find suitable sort index
        lastSI = pcv[k]->getSortIndex(lastSI + 1);

        memcpy(pcv[k]->oData, oData, sizeof(oData));

        pcv[k]->getDI().setInt("SortIndex", lastSI);
        pcv[k]->getDI().setInt("Heat", defineHeats ? k + 1 : 0);
        pcv[k]->synchronize();
      }

      outClassId[k] = pcv[k]->getId();
    }

    setName(getName() + makeDash(L"-1"), false);
    synchronize();
  }

  for (size_t k = 0; k < t.size(); k++) {
    pTeam it = t[k];
    int clsIx = cc.getClassIndex(*it);
    it->Class = pcv[clsIx];
    it->updateChanged();
    for (size_t k=0;k<it->Runners.size();k++) {
      if (it->Runners[k]) {
        if (defineHeats)
          it->getDI().setInt("Heat", clsIx+1);
        it->Runners[k]->Class = it->Class;
        it->Runners[k]->updateChanged();
      }
    }
    it->synchronize(); //Synchronizes runners also
  }

  for (size_t k = 0; k < r.size(); k++) {
    pRunner it = r[k];
    int clsIx = cc.getClassIndex(*it);
    if (qf) {
      it->getDI().setInt("Heat", clsIx + 1);
    }
    else {
      it->Class = pcv[clsIx];
      if (defineHeats)
        it->getDI().setInt("Heat", clsIx + 1);
    }
    it->updateChanged();
    it->synchronize();
  }
  oe->classIdToRunnerHash.reset();
}

void oClass::getAgeLimit(int &low, int &high) const
{
  low = getDCI().getInt("LowAge");
  high = getDCI().getInt("HighAge");
}

void oClass::setAgeLimit(int low, int high)
{
  getDI().setInt("LowAge", low);
  getDI().setInt("HighAge", high);
}

int oClass::getExpectedAge() const
{
  int low, high;
  getAgeLimit(low, high);

  if (low>0 && high>0)
    return (low+high)/2;

  if (low==0 && high>0)
    return high-3;

  if (low>0 && high==0)
    return low + 1;


  // Try to guess age from class name
  for (size_t k=0; k<Name.length(); k++) {
    if (Name[k]>='0' && Name[k]<='9') {
      int age = wtoi(&Name[k]);
      if (age>=10 && age<100) {
        if (age>=10 && age<=20)
          return age - 1;
        else if (age==21)
          return 28;
        else if (age>=35)
          return age + 2;
      }
    }
  }

  return 0;
}

void oClass::setSex(PersonSex sex) {
  getDI().setString("Sex", encodeSex(sex));
}

PersonSex oClass::getSex() const {
  return interpretSex(getDCI().getString("Sex"));
}

void oClass::setStart(const wstring &start) {
  getDI().setString("StartName", start);
}

const wstring &oClass::getStart() const {
  return getDCI().getString("StartName");
}

void oClass::setBlock(int block) {
  getDI().setInt("StartBlock", block);
}

int oClass::getBlock() const {
  return getDCI().getInt("StartBlock");
}

void oClass::setAllowQuickEntry(bool quick)
{
  getDI().setInt("AllowQuickEntry", quick);
}

bool oClass::getAllowQuickEntry() const
{
  return getDCI().getInt("AllowQuickEntry")!=0;
}

void oClass::setNoTiming(bool quick)
{
  tNoTiming = quick ? 1 : 0;
  getDI().setInt("NoTiming", quick);
}

BibMode oClass::getBibMode() const {
  const wstring &bm = getDCI().getString("BibMode");
  wchar_t b = bm.c_str()[0];
  if (b == 'A')
    return BibAdd;
  else if (b == 'F')
    return BibFree;
  else if (b == 'L')
    return BibLeg;
  else
    return BibSame;
}

void oClass::setBibMode(BibMode bibMode) {
  wstring res;
  switch (bibMode) {
  case BibAdd:
    res = L"A";
    break;
  case BibFree:
    res = L"F";
    break;
  case BibLeg:
    res = L"L";
    break;
  case BibSame:
    res = L"";
    break;
  default:
    throw meosException("Invalid bib mode");
  }

  getDI().setString("BibMode", res);
}

bool oClass::getNoTiming() const {
  if (tNoTiming!=0 && tNoTiming!=1)
    tNoTiming = getDCI().getInt("NoTiming")!=0 ? 1 : 0;
  return tNoTiming!=0;
}

void oClass::setIgnoreStartPunch(bool ignoreStartPunch) { 
  tIgnoreStartPunch = ignoreStartPunch;
  getDI().setInt("IgnoreStart", ignoreStartPunch); 
}

void oClass::updatedIgnoreStartPunch() {
  updateChanged();
  synchronize();

  bool updated = false;
  bool ignoreSP = ignoreStartPunch();
  vector<pRunner> rr;
  oe->getRunners(getId(), -1, rr, false);
  for (pRunner r : rr) {
    if (ignoreSP && r->getStartTime() > 0) {
      if (r->getCard()) {
        int st = r->getCard()->getStartTime(oPunch::SpecialPunch::PunchStart);
        if (st > 0 && st == r->getStartTime()) {
          r->restoreDefaultStartTime(false);
          r->synchronize();
          updated = true;
        }
      }
      else {
        vector<pFreePunch> fp;
        oe->getPunchesForRunner(r->getId(), false, fp);
        for (pFreePunch p : fp) {
          if (p->getTypeCode() == oPunch::SpecialPunch::PunchStart && r->getStartTime() == p->getTimeInt()) {
            r->restoreDefaultStartTime(false);
            r->synchronize();
            updated = true;
          }
        }
      }
    }
    else if (!ignoreSP && !r->getCard()) {
      vector<pFreePunch> fp;
      auto crs = r->getCourse(false);
      int stCd = crs && crs->useFirstAsStart() && crs->getControl(0) ? crs->getControl(0)->getFirstNumber() : oPunch::SpecialPunch::PunchStart;
      oe->getPunchesForRunner(r->getId(), false, fp);
      for (pFreePunch p : fp) {
        if (p->getTypeCode() == stCd) {
          r->setStartTime(p->getTimeInt(), true, ChangeType::Update, false);
          r->synchronize();
          updated = true;
        }
      }
    }
  }

  if (updated) {
    oe->reEvaluateAll({ getId() }, true);
  }
}

bool oClass::ignoreStartPunch() const {
  if (tIgnoreStartPunch != 0 && tIgnoreStartPunch != 1)
    tIgnoreStartPunch = getDCI().getInt("IgnoreStart") != 0 ? 1 : 0;
  return tIgnoreStartPunch != 0;
}

void oClass::setFreeStart(bool quick) {
  getDI().setInt("FreeStart", quick);
}

bool oClass::hasFreeStart() const {
  bool fs = getDCI().getInt("FreeStart") != 0;
  return fs;
}

void oClass::setRequestStart(bool quick)
{
  getDI().setInt("RequestStart", quick);
}

bool oClass::hasRequestStart() const
{
  bool fs = getDCI().getInt("RequestStart") != 0;
  return fs;
}

void oClass::setDirectResult(bool quick)
{
  getDI().setInt("DirectResult", quick);
}

bool oClass::hasDirectResult() const
{
  return getDCI().getInt("DirectResult") != 0;
}


void oClass::setType(const wstring &start)
{
  getDI().setString("ClassType", start);
}

wstring oClass::getType() const
{
  return getDCI().getString("ClassType");
}

void oEvent::fillStarts(gdioutput &gdi, const string &id)
{
  vector< pair<wstring, size_t> > d;
  oe->fillStarts(d);
  gdi.setItems(id, d);
}

const vector< pair<wstring, size_t> > &oEvent::fillStarts(vector< pair<wstring, size_t> > &out)
{
  out.clear();
  set<wstring> starts;
  for (oClassList::iterator it = Classes.begin(); it!=Classes.end(); ++it) {
    if (!it->getStart().empty())
      starts.insert(it->getStart());
  }

  if (starts.empty())
    starts.insert(lang.tl(L"Start") + L" 1");

  for (set<wstring>::iterator it = starts.begin(); it!=starts.end(); ++it) {
    //gdi.addItem(id, *it);
    out.push_back(make_pair(*it, 0));
  }
  return out;
}

void oEvent::fillClassTypes(gdioutput &gdi, const string &id)
{
  vector< pair<wstring, size_t> > d;
  oe->fillClassTypes(d);
  gdi.setItems(id, d);
}

ClassMetaType oClass::interpretClassType() const {
  int lowAge;
  int highAge;
  getAgeLimit(lowAge, highAge);

  if (highAge>0 && highAge <= 16)
    return ctYouth;

  map<wstring, ClassMetaType> types;
  oe->getPredefinedClassTypes(types);

  wstring type = getType();

  for (map<wstring, ClassMetaType>::iterator it = types.begin(); it != types.end(); ++it) {
    if (type == it->first || type == lang.tl(it->first))
      return it->second;
  }

  if (oe->classTypeNameToType.empty()) {
    // Lazy readout of baseclasstypes
    wchar_t path[260];
    getUserFile(path, L"baseclass.xml");
    xmlparser xml;
    xml.read(path);
    xmlobject cType = xml.getObject("BaseClassTypes");
    xmlList xtypes;
    cType.getObjects("Type", xtypes);
    for (size_t k = 0; k<xtypes.size(); k++) {
      wstring name = xtypes[k].getAttrib("name").getWStr();
      wstring typeS = xtypes[k].getAttrib("class").getWStr();
      ClassMetaType mtype = ctUnknown;
      if (stringMatch(typeS, L"normal"))
        mtype = ctNormal;
      else if (stringMatch(typeS, L"elite"))
        mtype = ctElite;
      else if (stringMatch(typeS, L"youth"))
        mtype = ctYouth;
      else if (stringMatch(typeS, L"open"))
        mtype = ctOpen;
      else if (stringMatch(typeS, L"exercise"))
        mtype = ctExercise;
      else if (stringMatch(typeS, L"training"))
        mtype = ctTraining;
      else {
        wstring err = L"Unknown type X#" + typeS;
        throw meosException(err);
      }
      oe->classTypeNameToType[name] = mtype;
    }
  }

  if (oe->classTypeNameToType.count(type) == 1)
    return oe->classTypeNameToType[type];

  return ctUnknown;
}

void oClass::assignTypeFromName(){
  wstring type = getType();
  if (type.empty()) {
    wstring prefix, suffix;
    extractAnyNumber(Name, prefix, suffix);
    int age = getExpectedAge();

    ClassMetaType mt = ctUnknown;
    if (age>=18) {
      if (stringMatch(suffix, lang.tl(L"Elit")) || wcschr(suffix.c_str(), 'E'))
        mt = ctElite;
      else if (stringMatch(suffix, lang.tl(L"Motion")) || wcschr(suffix.c_str(), 'M'))
        mt = ctExercise;
      else
        mt = ctNormal;
    }
    else if (age>=10 && age<=16) {
      mt = ctYouth;
    }
    else if (age<10) {
      if (stringMatch(prefix, lang.tl(L"Ungdom")) || wcschr(prefix.c_str(), 'U')
          || stringMatch(prefix, L"insk") || stringMatch(prefix, lang.tl(L"Inskolning")))
        mt = ctYouth;
      else if (stringMatch(suffix, lang.tl(L"Motion")) || wcschr(suffix.c_str(), 'M'))
        mt = ctExercise;
      else
        mt = ctOpen;
    }

    map<wstring, ClassMetaType> types;
    oe->getPredefinedClassTypes(types);

    for (map<wstring, ClassMetaType>::iterator it = types.begin(); it != types.end(); ++it) {
      if (it->second == mt) {
        setType(lang.tl(it->first));
        return;
      }
    }
  }
}

void oEvent::getPredefinedClassTypes(map<wstring, ClassMetaType> &types) const {
  types.clear();
  types[L"Elit"] = ctElite;
  types[L"Vuxen"] = ctNormal;
  types[L"Ungdom"] = ctYouth;
  types[L"Motion"] = ctExercise;
  types[L"Öppen"] = ctOpen;
  types[L"Träning"] = ctTraining;
}

const vector< pair<wstring, size_t> > &oEvent::fillClassTypes(vector< pair<wstring, size_t> > &out)
{
  out.clear();
  set<wstring> cls;
  bool allHasType = !Classes.empty();
  for (oClassList::iterator it = Classes.begin(); it!=Classes.end(); ++it) {
    if (it->isRemoved())
      continue;

    if (!it->getType().empty())
      cls.insert(it->getType());
    else
      allHasType = false;
  }

  if (!allHasType) {
    map<wstring, ClassMetaType> types;
    getPredefinedClassTypes(types);

    for (map<wstring, ClassMetaType>::iterator it = types.begin(); it != types.end(); ++it)
      cls.insert(lang.tl(it->first));
  }

  for (set<wstring>::iterator it = cls.begin(); it!=cls.end(); ++it) {
    //gdi.addItem(id, *it);
    out.push_back(make_pair(*it, 0));
  }
  return out;
}

int oClass::getNumRemainingMaps(bool forceRecalculate) const {
  oe->calculateNumRemainingMaps(forceRecalculate);

  int numMaps = tMapsRemaining;

  if (Course && Course->tMapsRemaining != numeric_limits<int>::min()) {
    if (numMaps == numeric_limits<int>::min())
      numMaps = Course->tMapsRemaining;
    else
      numMaps = min(numMaps, Course->tMapsRemaining);
  }  
  return numMaps;
}

void oClass::setNumberMaps(int nm) {
  getDI().setInt("NumberMaps", nm);
}

int oClass::getNumberMaps(bool rawAttribute) const {
  int nm = getDCI().getInt("NumberMaps");

  if (rawAttribute)
    return nm;

  if (nm == 0 && Course)
    nm = Course->getNumberMaps();

  return nm;
}

void oEvent::getStartBlocks(vector<int> &blocks, vector<wstring> &starts) const
{
  oClassList::const_iterator it;
  set<pair<wstring, int>> bs;
  for (it = Classes.begin(); it != Classes.end(); ++it) {
    if (it->isRemoved())
      continue;
    
    bs.emplace(it->getStart(), it->getBlock());
  }
  blocks.clear();
  starts.clear();

  for (auto &v : bs) {
    blocks.push_back(v.second);
    starts.push_back(v.first);
  }
}

const shared_ptr<Table> &oClass::getTable(oEvent *oe) {
  if (!oe->hasTable("class")) {
    auto table =  make_shared<Table>(oe, 20, L"Klasser", "classes");

    table->addColumn("Id", 70, true, true);
    table->addColumn("Ändrad", 70, false);

    table->addColumn("Namn", 200, false);

    table->addColumn("Bana", 200, false);
    table->addColumn("Anmälda", 70, true);

    oe->oClassData->buildTableCol(table.get());
    oe->setTable("class", table);
  }

  return oe->getTable("class");
}

void oEvent::generateClassTableData(Table &table, oClass *addClass)
{
  if (addClass) {
    addClass->addTableRow(table);
    return;
  }

  synchronizeList(oListId::oLClassId);
  oClassList::iterator it;

  for (it=Classes.begin(); it != Classes.end(); ++it){
    if (!it->isRemoved())
      it->addTableRow(table);
  }
}

void oClass::addTableRow(Table &table) const {
  pClass it = pClass(this);
  table.addRow(getId(), it);
  
  int row = 0;
  table.set(row++, *it, TID_ID, itow(getId()), false);
  table.set(row++, *it, TID_MODIFIED, getTimeStamp(), false);

  table.set(row++, *it, TID_CLASSNAME, getName(), true);

  wstring crsStr;
  pCourse crs = getCourse(false);
  bool canEdit = false;
  if (crs) {
    crsStr = crs->getName();
    canEdit = true;
  }
  else {
    if (getNumStages() <= 1 && MultiCourse.empty()) {
      crsStr = lang.tl(L"Ingen bana");
      canEdit = true;
    }
    else {
      for (size_t leg = 0; leg < MultiCourse.size(); leg++) {
        for (size_t j = 0; j < MultiCourse[leg].size(); j++) {
          if (j >= 3) {
            crsStr += L"…";
            break;
          }
          if (MultiCourse[leg][j]) {
            if (!crsStr.empty())
              crsStr += L", ";
            crsStr += MultiCourse[leg][j]->getName();
          }
        }
        if (!crsStr.empty())
          break;
      }
    }
  }
  table.set(row++, *it, TID_COURSE, crsStr, canEdit, canEdit ? CellType::cellSelection : CellType::cellEdit);
  int numR = getNumRunners(true, false, false);
  table.set(row++, *it, TID_NUM, itow(numR), false);

  oe->oClassData->fillTableCol(*this, table, true);
}

pair<int, bool> oClass::inputData(int id, const wstring &input,
                       int inputId, wstring &output, bool noUpdate)
{
  synchronize(false);

  if (id>1000) {
    return oe->oClassData->inputData(this, id, input,
                                       inputId, output, noUpdate);
  }
  switch(id) {
    case TID_CLASSNAME:
      setName(input, true);
      synchronize();
      output=getName();
    break;
    case TID_COURSE: {
      pCourse c = nullptr;
      if (inputId != 0)
        c = oe->getCourse(inputId);
      setCourse(c);
      synchronize();
      output = input;
    }
  }

  return make_pair(0, false);
}

void oClass::fillInput(int id, vector< pair<wstring, size_t> > &out, size_t &selected)
{
  if (id>1000) {
    oe->oClassData->fillInput(this, id, 0, out, selected);
    return;
  }

  if (id==TID_COURSE) {
    out.clear();
    oe->getCourses(out, L"", true);
    out.push_back(make_pair(lang.tl(L"Ingen bana"), 0));
    pCourse c = getCourse(false);
    selected = c ? c->getId() : 0;
  }
}

void oClass::getStatistics(const set<int> &feeLock, int &entries, int &started) const
{
  oRunnerList::const_iterator it;
  entries = 0;
  started = 0;
  for (it = oe->Runners.begin(); it != oe->Runners.end(); ++it) {
    if (it->skip() || it->isVacant())
      continue;
    if (it->getStatus() == StatusNotCompeting)
      continue;

    if (it->getClassId(false)==Id) {
      if (feeLock.empty() || feeLock.count(it->getDCI().getInt("Fee"))) {
        entries++;
        if (it->getStatus()!= StatusUnknown && it->getStatus()!= StatusDNS && it->tStatus != StatusCANCEL)
          started++;
      }
    }
  }
}

bool oClass::isSingleRunnerMultiStage() const
{
  return getNumStages()>1 && getNumDistinctRunnersMinimal()==1;
}

int oClass::getEntryFee(const wstring &date, int age) const
{
  oDataConstInterface odc = oe->getDCI();
  wstring ordEntry = odc.getDate("OrdinaryEntry");
  wstring lateEntry = odc.getDate("SecondEntryDate");
  bool late = date > ordEntry && ordEntry>=L"2010-01-01";
  bool late2 = date >= lateEntry && lateEntry >= L"2010-01-01";

  bool reduced = false;

  if (age > 0) {
    int low = odc.getInt("YouthAge");
    int high = odc.getInt("SeniorAge");
    reduced = age <= low || (high > 0 && age >= high);
  }

  if (reduced) {
    int veryHigh = getDCI().getInt("SecondHighClassFeeRed");
    int high = getDCI().getInt("HighClassFeeRed");
    int normal = getDCI().getInt("ClassFeeRed");

    // Only return these fees if set
    if (late2 && veryHigh > 0)
      return veryHigh;
    else if (late && high>0)
      return high;
    else if (normal>0)
      return normal;
  }

  int veryHigh = getDCI().getInt("SecondHighClassFee");
  int high = getDCI().getInt("HighClassFee");
  int normal = getDCI().getInt("ClassFee");

  if (late2 && veryHigh > 0)
    return veryHigh;
  if (late && high > 0)
    return high;
  else
    return normal;
}

void oClass::addClassDefaultFee(bool resetFee) {
  int fee = getDCI().getInt("ClassFee");

  if (fee == 0 || resetFee) {
    assignTypeFromName();
    ClassMetaType type = interpretClassType();
    switch (type) {
      case ctElite:
        fee = oe->getDCI().getInt("EliteFee");
      break;
      case ctYouth:
        fee = oe->getDCI().getInt("YouthFee");
      break;
      default:
        fee = oe->getDCI().getInt("EntryFee");
    }

    const int reducedFee = oe->getDCI().getInt("YouthFee");

    double factor = 1.0 + 0.01 * _wtof(oe->getDCI().getString("LateEntryFactor").c_str());
    int lateFee = fee;
    int lateReducedFee = reducedFee;
    if (factor > 1) {
      lateFee = int(fee*factor + 0.5);
      lateReducedFee = int(reducedFee*factor + 0.5);
    }
    getDI().setInt("ClassFee", fee);
    getDI().setInt("HighClassFee", lateFee);
    getDI().setInt("ClassFeeRed", reducedFee);
    getDI().setInt("HighClassFeeRed", lateReducedFee);

    double factor2 = 1.0 + 0.01 * _wtof(oe->getDCI().getString("SecondEntryFactor").c_str());
    int lateFee2 = 0;
    int lateReducedFee2 = 0;
    if (factor > 1) {
      lateFee2 = int(fee * factor2 + 0.5);
      lateReducedFee2 = int(reducedFee * factor2 + 0.5);
    }

    getDI().setInt("SecondHighClassFee", lateFee2);
    getDI().setInt("SecondHighClassFeeRed", lateReducedFee2);
  }
}

void oClass::reinitialize(bool force) const {
  if (!force && isInitialized)
    return;
  isInitialized = true; // Prevent recursion

  int ix = getDCI().getInt("SortIndex");
  if (ix == 0) {
    ix = getSortIndex(getId()*10);
    const_cast<oClass*>(this)->getDI().setInt("SortIndex", ix);
  }
  tSortIndex = ix;

  tMaxTime = getDCI().getInt("MaxTime");
  if (tMaxTime == 0 && oe) {
    tMaxTime = oe->getMaximalTime();
  }

  wstring wInfo = getDCI().getString("Qualification");
  if (!wInfo.empty()) {
    if (qualificatonFinal && !qualificatonFinal->matchSerialization(wInfo))
      clearQualificationFinal();

    if (!qualificatonFinal)
      qualificatonFinal = make_shared<QualificationFinal>(MaxClassId, Id);

    qualificatonFinal->init(wInfo);
    virtualClasses.resize(getNumQualificationFinalClasses());

    int nc = qualificatonFinal->getNumClasses();
    for (int i = 1; i <= nc; i++)
      getVirtualClass(i);
  }
  else {
    clearQualificationFinal();
  }

  tNoTiming = -1;
  tIgnoreStartPunch = -1;
}

void oClass::clearQualificationFinal() const {
  if (!qualificatonFinal)
    return;

  int nc = qualificatonFinal->getNumClasses();
  for (pClass pc : virtualClasses) {
    if (pc)
      pc->parentClass = nullptr;
  }

  virtualClasses.clear();
  qualificatonFinal.reset(); 
}

void oEvent::reinitializeClasses() const {
  for (auto &c : Classes)
    c.reinitialize(true);
}

int oClass::getSortIndex(int candidate) const {
  int major = numeric_limits<int>::max();
  int minor = 0;

  for (oClassList::iterator it = oe->Classes.begin(); it != oe->Classes.end(); ++it) {
    int ix = it->getDCI().getInt("SortIndex");
    if (ix>0) {
      if (ix>candidate && ix<major)
        major = ix;

      if (ix<candidate && ix>minor)
        minor = ix;
    }
  }

  // If the gap is less than 10 (which is the default), optimize
  if (major < numeric_limits<int>::max() && minor>0 && ((major-candidate)<10 || (candidate-minor)<10))
    return (major+minor)/2;
  else
    return candidate;
}

void oClass::apply() {
  int trueLeg = 0;
  int trueSubLeg = 0;

  for (size_t k = 0; k<legInfo.size(); k++) {
    oLegInfo &li = legInfo[k];
    LegTypes lt = li.legMethod;
    if (lt == LTNormal || lt == LTSum || lt == LTGroup) {
      trueLeg++;
      trueSubLeg = 0;
    }
    else
      trueSubLeg++;

    if (trueSubLeg == 0 && (k+1) < legInfo.size()) {
      LegTypes nt = legInfo[k+1].legMethod;
      if (nt == LTParallel || nt == LTParallelOptional || nt == LTExtra || nt == LTIgnore)
        trueSubLeg = 1;
    }
    li.trueLeg = trueLeg;
    li.trueSubLeg = trueSubLeg;
    if (trueSubLeg == 0)
      li.displayLeg = itos(trueLeg);
    else
      li.displayLeg = itos(trueLeg) + "." + itos(trueSubLeg);
  }
}

class LegResult
{
private:
  inthashmap rmap;
public:
  void addTime(int from, int to, int time);
  int getTime(int from, int to) const;
};

void LegResult::addTime(int from, int to, int time)
{
  int key = from + (to<<15);
  int value;
  if (rmap.lookup(key, value))
    time = min(value, time);
  rmap[key] = time;
}

int LegResult::getTime(int from, int to) const
{
  int key = from + (to<<15);
  int value;
  if (rmap.lookup(key, value))
    return value;
  else
    return 0;
}

void oClass::clearSplitAnalysis()
{
#ifdef _DEBUG
  if (!tSplitAnalysisData.empty())
    std::wcerr << L"Clear splits " << Name << L"\n";
#endif
  tFirstStart.clear();
  tLastStart.clear();

  tSplitAnalysisData.clear();
  tCourseLegLeaderTime.clear();
  tCourseAccLegLeaderTime.clear();

  if (tLegLeaderTime)
    delete tLegTimeToPlace;
  tLegTimeToPlace = 0;

  if (tLegAccTimeToPlace)
    delete tLegAccTimeToPlace;
  tLegAccTimeToPlace = 0;

  tSplitRevision++;

  oe->classChanged(this, false);
}

void oClass::insertLegPlace(int from, int to, int time, int place)
{
  if (tLegTimeToPlace) {
    int key = time + (to + from*256)*8013;
    tLegTimeToPlace->insert(key, place);
  }
}

int oClass::getLegPlace(int ifrom, int ito, int time) const
{
  if (tLegTimeToPlace) {
    int key = time + (ito + ifrom*256)*8013;
    int place;
    if (tLegTimeToPlace->lookup(key, place))
      return place;
  }
  return 0;
}

int oClass::getAccLegControlLeader(int teamLeg, int courseControlId) const {
  if (teamLeg < teamLegCourseControlToLeaderPlace.size()) {
    auto res = teamLegCourseControlToLeaderPlace[teamLeg].find(courseControlId);
    if (res != teamLegCourseControlToLeaderPlace[teamLeg].end()) {
      return res->second.leader;
    }
  }
  return 0;
}

int oClass::getAccLegControlPlace(int teamLeg, int courseControlId, int time) const {
  if (teamLeg < teamLegCourseControlToLeaderPlace.size()) {
    auto res = teamLegCourseControlToLeaderPlace[teamLeg].find(courseControlId);
    if (res != teamLegCourseControlToLeaderPlace[teamLeg].end()) {
      auto& timeToPlace = res->second.timeToPlace;
      auto v = timeToPlace.find(time);
      if (v != timeToPlace.end())
        return v->second;
    }
  }
  return 0;
}

void oClass::insertAccLegPlace(int courseId, int controlNo, int time, int place)
{ /*
  char bf[256];
  snprintf(bf, sizeof(bf), "Insert to %d, %d, time %d\n", courseId, controlNo, time);
  OutputDebugString(bf);
  */
  if (tLegAccTimeToPlace) {
    int key = time + (controlNo + courseId*128)*16013;
    tLegAccTimeToPlace->insert(key, place);
  }
}

void oClass::getStartRange(int leg, int &firstStart, int &lastStart) const {
  leg = mapLeg(leg);

  if (tFirstStart.empty()) {
    size_t s = getLastStageIndex() + 1;
    assert(s>0);
    vector<int> lFirstStart, lLastStart;
    lFirstStart.resize(s, timeConstHour * 24 * 100);
    lLastStart.resize(s, 0);
    for (oRunnerList::iterator it = oe->Runners.begin(); it != oe->Runners.end(); ++it) {
      if (it->isRemoved() || it->getClassRef(true) != this)
        continue;
      if (it->needNoCard())
        continue;
      size_t tleg = mapLeg(it->tLeg);
      if (tleg < s) {
        lFirstStart[tleg] = min<unsigned>(lFirstStart[tleg], it->tStartTime);
        lLastStart[tleg] = max<signed>(lLastStart[tleg], it->tStartTime);
      }
    }
    swap(tLastStart, lLastStart);
    swap(tFirstStart, lFirstStart);
  }
  if (size_t(leg) < tFirstStart.size()) {
    firstStart = tFirstStart[leg];
    lastStart = tLastStart[leg];
  }
  else if (!tFirstStart.empty()) {
    firstStart = tFirstStart[0];
    lastStart = tLastStart[0];
    for (size_t k = 1; k < tFirstStart.size(); k++) {
      if (lastStart > 0) {
        firstStart = min(tFirstStart[k], firstStart);
        lastStart = max(tLastStart[k], lastStart);
      }
    }
  }
  else {
    firstStart = 0;
    lastStart = 0;
  }
}

int oClass::getAccLegPlace(int courseId, int controlNo, int time) const
{/*
  char bf[256];
  snprintf(bf, sizeof(bf), "Get from %d,  %d, time %d\n", courseId, controlNo, time);
  OutputDebugString(bf);
  */
  if (tLegAccTimeToPlace) {
    int key = time + (controlNo + courseId*128)*16013;
    int place;
    if (tLegAccTimeToPlace->lookup(key, place))
      return place;
  }
  return 0;
}

void oClass::calculateSplits() {
  clearSplitAnalysis();
  set<pCourse> cSet;
  map<int, vector<int> > legToTime;

  for (size_t k=0;k<MultiCourse.size();k++) {
    for (size_t j=0; j<MultiCourse[k].size(); j++) {
      if (MultiCourse[k][j])
        cSet.insert(MultiCourse[k][j]);
    }
  }
  if (getCourse())
    cSet.insert(getCourse());

  LegResult legRes;
  LegResult legBestTime;
  vector<pRunner> rCls;
  oe->getRunners(Id, -1, rCls, false);

  for (pRunner it : rCls) {
    pCourse tpc = it->getCourse(false);
    if (tpc == nullptr)
      continue;
    cSet.insert(tpc);
  }

  map<int, vector<pRunner>> rClsCrs;
  if (cSet.size() > 1) {
    for (pRunner it : rCls) {
      pCourse tpc = it->getCourse(false);
      if (tpc)
        rClsCrs[tpc->getId()].push_back(it);
    }
  }

  bool multiLeg = getNumStages() > 1; // Perhaps ignore parallell legs...

  if (multiLeg) {
    teamLegCourseControlToLeaderPlace.resize(getNumStages());
    for (auto& lp : teamLegCourseControlToLeaderPlace)
      lp.clear();
  }
  else {
    teamLegCourseControlToLeaderPlace.clear();
  }

  for (pCourse pc : cSet) {
    // Store all split times in a matrix
    const unsigned nc = pc->getNumControls();
    if (nc == 0)
      return;

    vector<vector<int>> splits(nc+1);
    vector<vector<int>> splitsAcc(nc+1);
    vector<int8_t> acceptMissingPunch(nc+1, true);
    vector<pRunner>* rList;
    if (rClsCrs.empty())
      rList = &rCls;
    else
      rList = &rClsCrs[pc->getId()];

    for (pRunner it : *rList) {
      pCourse tpc = it->getCourse(false);
      if (tpc != pc || tpc == 0)
        continue;

      const vector<SplitData> &sp = it->getSplitTimes(true);
      const int s = min<int>(nc, sp.size());

      for (int k = 0; k < s; k++) {
        if (sp[k].getTime(true) > 0 && acceptMissingPunch[k]) {
          pControl ctrl = tpc->getControl(k);
          // If there is a
          if (ctrl && ctrl->getStatus() != oControl::ControlStatus::StatusBad && ctrl->getStatus() != oControl::ControlStatus::StatusOptional)
            acceptMissingPunch[k] = false;
        }
      }
    }

    for (pRunner it : *rList) {
      pCourse tpc = it->getCourse(false);

      if (tpc != pc)
        continue;

      const vector<SplitData> &sp = it->getSplitTimes(true);
      const int s = min<int>(nc, sp.size());

      int off = -1;
      unordered_map<int, PlaceTime>* teamAccTimes = nullptr;
      if (multiLeg && it->tInTeam) 
        off = it->tInTeam->getTotalRunningTimeAtLegStart(it->tLeg, false);

      if (off >= 0 && it->tLeg < teamLegCourseControlToLeaderPlace.size()) 
        teamAccTimes = &teamLegCourseControlToLeaderPlace[it->tLeg];

      vector<int> &tLegTimes = it->tLegTimes;
      tLegTimes.resize(nc + 1);
      bool ok = true;

      // Acc team finish time
      if (teamAccTimes && it->FinishTime > 0 && (it->tStatus == StatusOK || it->tStatus == StatusUnknown)) {
        int ccId = oPunch::PunchFinish;
        int t = it->getRunningTime(false);
        auto& res = (*teamAccTimes)[ccId];
        if (res.leader <= 0 || res.leader > t + off)
          res.leader = t + off;
        // Count times
        ++res.timeToPlace[t + off];
      }

      for (int k = 0; k < s; k++) {
        if (sp[k].getTime(true) > 0) {
          if (ok) {
            // Store accumulated times
            int t = sp[k].getTime(true) - it->tStartTime;
            if (it->tStartTime > 0 && t > 0) {
              splitsAcc[k].push_back(t);
              if (teamAccTimes) {
                int ccId = pc->getCourseControlId(k);
                auto& res = (*teamAccTimes)[ccId];
                if (res.leader <= 0 || res.leader > t + off)
                  res.leader = t + off;

                // Count times
                ++res.timeToPlace[t + off];
              }
            }
          }

          if (k == 0) { // start -> first
            int t = sp[0].getTime(true) - it->tStartTime;
            if (it->tStartTime>0 && t>0) {
              splits[k].push_back(t);
              tLegTimes[k] = t;
            }
            else
              tLegTimes[k] = 0;
          }
          else { // control -> control
            int t = sp[k].getTime(true) - sp[k-1].getTime(true);
            if (sp[k-1].getTime(true)>0 && t>0) {
              splits[k].push_back(t);
              tLegTimes[k] = t;
            }
            else
              tLegTimes[k] = 0;
          }
        }
        else
          ok = acceptMissingPunch[k] != 0;
      }

      // last -> finish
      if (sp.size() == nc && sp[nc-1].getTime(true)>0 && it->FinishTime > 0) {
        int t = it->FinishTime - sp[nc-1].getTime(true);
        if (t>0) {
          splits[nc].push_back(t);
          tLegTimes[nc] = t;
          if (it->statusOK(true, false) && (it->FinishTime - it->tStartTime) > 0) {
            splitsAcc[nc].push_back(it->FinishTime - it->tStartTime);
          }
        }
        else
          tLegTimes[nc] = 0;
      }
    }

    if (splits.size()>0 && tLegTimeToPlace == 0) {
      tLegTimeToPlace = new inthashmap(splits.size() * splits[0].size());
      tLegAccTimeToPlace = new inthashmap(splits.size() * splits[0].size());
    }

    vector<int> &accLeaderTime = tCourseAccLegLeaderTime[pc->getId()];

    for (size_t k = 0; k < splits.size(); k++) {

      // Calculate accumulated best times and places
      if (!splitsAcc[k].empty()) {
        sort(splitsAcc[k].begin(), splitsAcc[k].end());
        accLeaderTime.push_back(splitsAcc[k].front()); // Store best time

        int place = 1;
        for (size_t j = 0; j < splitsAcc[k].size(); j++) {
          if (j>0 && splitsAcc[k][j-1]<splitsAcc[k][j])
            place = j+1;
          insertAccLegPlace(pc->getId(), k, splitsAcc[k][j], place);
        }
      }
      else {
        // Bad control / missing times
        int t = 0;
        if (!accLeaderTime.empty())
          t = accLeaderTime.back();
        accLeaderTime.push_back(t); // Store time from previous leg
      }

      sort(splits[k].begin(), splits[k].end());
      const size_t ntimes = splits[k].size();
      if (ntimes == 0)
        continue;

      int from = pc->getCommonControl(), to = pc->getCommonControl(); // Represents start/finish
      if (k < nc && pc->getControl(k))
        to = pc->getControl(k)->getId();
      if (k>0 && pc->getControl(k-1))
        from = pc->getControl(k-1)->getId();

      for (size_t j = 0; j < ntimes; j++)
        legToTime[256*from + to].push_back(splits[k][j]);

      int time = 0;
      if (ntimes < 5)
        time = splits[k][0]; // Best time
      else if (ntimes < 12)
        time = (splits[k][0]+splits[k][1]) / 2; //Average best time
      else {
        int nval = ntimes/6;
        for (int r = 1; r <= nval; r++)// "Best fraction", skip winner
          time += splits[k][r];
        time /= nval;
      }

      legRes.addTime(from, to, time);
      legBestTime.addTime(from, to, splits[k][0]); // Add leader time
    }
  }

  // Loop and sort times for each leg run in this class
  for (map<int, vector<int> >::iterator cit = legToTime.begin(); cit != legToTime.end(); ++cit) {
    int key = cit->first;
    vector<int> &times = cit->second;
    sort(times.begin(), times.end());
    int jsiz = times.size();
    for (int j = 0; j<jsiz; ++j) {
      if (j==0 || times[j-1]<times[j])
        insertLegPlace(0, key, times[j], j+1);
    }
  }

  for (pCourse pc : cSet)  {
    const unsigned nc = pc->getNumControls();
    vector<int> normRes(nc+1);
    vector<int> bestRes(nc+1);
    int cc = pc->getCommonControl();
    for (size_t k = 0; k <= nc; k++) {
      int from = cc, to = cc; // Represents start/finish
      if (k < nc && pc->getControl(k))
        to = pc->getControl(k)->getId();
      if (k>0 && pc->getControl(k-1))
        from = pc->getControl(k-1)->getId();
      normRes[k] = legRes.getTime(from, to);
      bestRes[k] = legBestTime.getTime(from, to);
    }

    swap(tSplitAnalysisData[pc->getId()], normRes);
    swap(tCourseLegLeaderTime[pc->getId()], bestRes);
  }

  // Convert number of competitors with time to place
  for (auto& courseControlLeaderPlace : teamLegCourseControlToLeaderPlace) {
    for (auto& leaderPlace : courseControlLeaderPlace) {
      int place = 1;
      for (auto& numTimes : leaderPlace.second.timeToPlace) {
        int num = numTimes.second;
        numTimes.second = place;
        place += num;
      }
    }
  }
}

bool oClass::isRogaining() const {
  if (Course &&  Course->getMaximumRogainingTime() > 0)
    return true;

  if (MultiCourse.size() > 0 && MultiCourse[0].size() > 0 && 
      MultiCourse[0][0] && MultiCourse[0][0]->getMaximumRogainingTime() > 0)
    return true;
  GeneralResult *gr = getResultModule();

  // Return true if result module makes point calculations and
  // controls define rogaining points. Otherwize point calculations
  // is assumed to be just a side effect.
  if (gr && gr->isRogaining() && Course) {
    for (int n = 0; n < Course->getNumControls(); n++) {
      pControl ctrl = Course->getControl(n);
      if (ctrl && ctrl->isRogaining(true))
        return true;
    }
  }
  return false;
}


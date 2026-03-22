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


// oClass.cpp: implementation of the oClass class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#define DODECLARETYPESYMBOLS

#include <cassert>
#include "oClass.h"
#include "oEvent.h"
#include "Table.h"
#include "meos_util.h"
#include <limits>
#include "localizer.h"
#include <algorithm>
#include "inthashmap.h"
#include "intkeymapimpl.hpp"
#include "SportIdent.h"
#include "MeOSFeatures.h"
#include "gdioutput.h"
#include "gdistructures.h"
#include "meosexception.h"
#include "random.h"
#include "qualification_final.h"
#include "generalresult.h"
#include "metalist.h"
#include "xmlparser.h"
#include <cstdint>
#include <iostream>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


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
    std::cerr << narrow((L"Clear splits " + Name + L"\n"));
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
  std::cerr << bf;
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
  std::cerr << bf;
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

void oClass::remove()
{
  if (oe)
    oe->removeClass(Id);
}

bool oClass::canRemove() const
{
  return !oe->isClassUsed(Id);
}

int oClass::getMaximumRunnerTime() const {
  reinitialize(false);
  return tMaxTime;
}

int oClass::getNumLegNoParallel() const {
  int nl = 1;
  for (size_t k = 1; k < legInfo.size(); k++) {
    if (!legInfo[k].isParallel())
      nl++;
  }
  return nl;
}

bool oClass::splitLegNumberParallel(int leg, int &legNumber, int &legOrder) const {
  legNumber = 0;
  legOrder = 0;
  if (legInfo.empty())
    return false;

  int stop = min<int>(leg, legInfo.size() - 1);
  int k;
  for (k = 0; k < stop; k++) {
    if (legInfo[k+1].isParallel() || legInfo[k+1].isOptional())
      legOrder++;
    else {
      legOrder = 0;
      legNumber++;
    }
  }
  if (legOrder == 0) {
    if (k+1 < int(legInfo.size()) && (legInfo[k+1].isParallel() || legInfo[k+1].isOptional()))
      return true;
    if (!(legInfo[k].isParallel() || legInfo[k].isOptional()))
      return false;
  }
  return true;
}

int oClass::getLegNumberLinear(int legNumberIn, int legOrderIn) const {
  if (legNumberIn == 0 && legOrderIn == 0)
    return 0;

  int legNumber = 0;
  int legOrder = 0;
  for (size_t k = 1; k < legInfo.size(); k++) {
    if (legInfo[k].isParallel() || legInfo[k].isOptional())
      legOrder++;
    else {
      legOrder = 0;
      legNumber++;
    }
    if (legNumberIn == legNumber && legOrderIn == legOrder)
      return k;
  }

  return -1;
}

/** Return an actual linear index for this class. */
int oClass::getLinearIndex(int index, bool isLinear) const {
  if (legInfo.empty())
    return 0;
  if (size_t(index) >= legInfo.size())
    return legInfo.size() - 1; // -1 to last leg

  return isLinear ? index : getLegNumberLinear(index, 0);
}

wstring oClass::getLegNumber(int leg) const {
  int legNumber, legOrder;
  bool par = splitLegNumberParallel(leg, legNumber, legOrder);
  wchar_t bf[16];
  if (par) {
    char symb = 'a' + legOrder;
    swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%d%c", legNumber + 1, symb);
  }
  else {
    swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%d", legNumber + 1);
  }
  return bf;
}

oClass::ClassStatus oClass::getClassStatus() const {
  if (tStatusRevision != oe->dataRevision) {
    wstring s = getDCI().getString("Status");
    if (s == L"I")
      tStatus =  ClassStatus::Invalid;
    else if (s == L"IR")
      tStatus = ClassStatus::InvalidRefund;
    else
      tStatus = ClassStatus::Normal;

    tStatusRevision = oe->dataRevision;
  }
  return tStatus;
}

void oClass::fillClassStatus(vector<pair<wstring, wstring>> &statusClass) {
  statusClass.push_back(make_pair(L"", L"OK"));
  statusClass.push_back(make_pair(L"IR", L"Struken med återbetalning"));
  statusClass.push_back(make_pair(L"I", L"Struken utan återbetalning"));
}
void oClass::clearCache(bool recalculate) {
  if (recalculate)
    oe->reCalculateLeaderTimes(getId());
  clearSplitAnalysis();
  tResultInfo.clear();//Do on competitor remove!
}


bool oClass::wasSQLChanged(int leg, int control) const {
  if (oe->globalModification)
    return true;

  map<int, set<int> >::const_iterator res = sqlChangedControlLeg.find(-1);
  if (res != sqlChangedControlLeg.end()) {
    if (leg == -1 || res->second.count(-1) || res->second.count(leg))
      return true;
  }

  if (control != -1) {
    if (control == -2) // Any control
      return sqlChangedControlLeg.size() > 0;
    res = sqlChangedControlLeg.find(control);
    if (res != sqlChangedControlLeg.end()) {
      if (leg == -1 || res->second.count(-1) || res->second.count(leg))
        return true;
    }
  }

  res = sqlChangedLegControl.find(leg);
  if (res != sqlChangedLegControl.end()) {
    if (control == -1 || res->second.count(-1) || res->second.count(control))
      return true;
  }

  return false;
}

void oClass::markSQLChanged(int leg, int control) {
  sqlChangedControlLeg[control].insert(leg);
  sqlChangedLegControl[leg].insert(control);
  oe->classChanged(this, false);
}

void oClass::changedObject() {
  markSQLChanged(-1,-1);
  tNoTiming = -1;
  tIgnoreStartPunch = -1;
  oe->sqlClasses.changed = true;
}

static void checkMissing(const map< pair<int, int>, int > &master,
                         const map< pair<int, int>, int > &check,
                         set< pair<int, int> > &controlProblems) {
  assert(master.size() >= check.size());

  for ( map< pair<int, int>, int >::const_iterator it = master.begin();
        it != master.end(); ++it) {
    map< pair<int, int>, int >::const_iterator res = check.find(it->first);
    if (res == check.end() || res->second != it->second)
      controlProblems.insert(it->first);
  }
}

// Check if forking is fair
bool oClass::checkForking(vector< vector<int> > &legOrder,
                          vector< vector<int> > &forks,
                          set< pair<int, int> > &unfairLegs) const {
  legOrder.clear();
  forks.clear();
  map<long long, int> hashes;
  int max = 1;
  set<int> factors;
  for (size_t k = 0; k < MultiCourse.size(); k++) {
    if (MultiCourse[k].size() > 1 && factors.count(MultiCourse[k].size()) == 0) {
      max *= MultiCourse[k].size();
      factors.insert(MultiCourse[k].size()); // This is an over estimate. Should consider prime factors to get it exact
    }
  }

  legOrder.reserve(max);
  for (int k = 0; k < max; k++) {
    vector<int> order;
    long long hash = 0;
    for (size_t j = 0; j< MultiCourse.size(); j++) {
      if (getLegType(j) == LTExtra || getLegType(j) == LTIgnore)
        continue;
      if (!MultiCourse[j].empty()) {
        int ix = k % MultiCourse[j].size();
        int cid = MultiCourse[j][ix]->getId();
        order.push_back(cid);
        hash = hash * 997 + cid;
      }
    }
    if (order.empty())
      continue;

    if (hashes.count(hash) == 0) {
      hashes[hash] = legOrder.size();
      legOrder.push_back(order);
    }
    else {
      int test = hashes[hash];
      if (legOrder[test] != order) {
        // Test for hash collision. Will not happen...
        bool exist = false;
        for (size_t i = 0; i < legOrder.size(); i++) {
          if (legOrder[i] == order) {
            exist = true;
            break;
          }
        }
        if (!exist) {
          legOrder.push_back(order);
        }
      }
    }
  }


  for (oTeamList::const_iterator it = oe->Teams.begin(); it != oe->Teams.end(); ++it) {
    if (it->skip() || it->Class != this)
      continue;
    vector<int> order;
    bool valid = true;
    long long hash = 0;
    for (size_t j = 0; j < it->Runners.size(); j++) {
      if (getLegType(j) == LTExtra || getLegType(j) == LTIgnore)
        continue;
      pCourse crs;
      if (it->Runners[j] && (crs = it->Runners[j]->getCourse(false)) != 0) {
        if (it->Runners[j]->getNumShortening() > 0) {
          valid = false;
          break;
        }
        int cid = crs->getId();
        order.push_back(cid);
        hash = hash * 997 + cid;
      }
      else {
        valid = false;
        break;
      }
    }
    if (!valid) 
      continue;
    if (hashes.count(hash) == 0) {
      hashes[hash] = legOrder.size();
      legOrder.push_back(order);
    }
    else {
      int test = hashes[hash];
      if (legOrder[test] != order) {
        // Test for hash collision. Will not happen...
        bool exist = false;
        for (size_t i = 0; i < legOrder.size(); i++) {
          if (legOrder[i] == order) {
            exist = true;
            break;
          }
        }
        if (!exist) {
          legOrder.push_back(order);
        }
      }
    }
  }

  vector< vector<int> > controlOrder(legOrder.size());
  vector< map< pair<int, int>, int > > countLegsPerFork(legOrder.size());

  for(size_t k = 0; k < legOrder.size(); k++) {
    for (size_t j = 0; j < legOrder[k].size(); j++) {
      pCourse pc = oe->getCourse(legOrder[k][j]);
      if (pc) {
        controlOrder[k].push_back(-1); // Finish/start
        for (int i = 0; i < pc->nControls(); i++) {
          int id = pc->controls[i]->nNumbers == 1 ? pc->controls[i]->Numbers[0] : pc->controls[i]->getId();
          controlOrder[k].push_back(id);
        }
      }
    }
    if (controlOrder[k].size() > 0)
      controlOrder[k].push_back(-1); // Finish

    for (size_t j = 1; j < controlOrder[k].size(); j++) {
      int s = controlOrder[k][j-1];
      int e = controlOrder[k][j];
      ++countLegsPerFork[k][make_pair(s, e)];
    }
  }

  unfairLegs.clear();
  for (size_t k = 1; k < countLegsPerFork.size(); k++) {
    if (countLegsPerFork[0].size() >= countLegsPerFork[k].size())
      checkMissing(countLegsPerFork[0], countLegsPerFork[k], unfairLegs);
    else
      checkMissing(countLegsPerFork[k], countLegsPerFork[0], unfairLegs);
  }

  forks = controlOrder;

  return unfairLegs.empty();
}

long long oClass::setupForkKey(const vector<int> indices, const vector< vector< vector<int> > > &courseKeys, vector<int> &legs) {
  size_t sr = 0;
  int mergeIx[maxRunnersTeam];
  const vector<int> *pCrs[maxRunnersTeam];
  int npar = indices.size();
  for (int k = 0; k < npar; k++) {
    if (indices[k]>=0) {
      pCrs[k] = &courseKeys[k][indices[k]];
      sr += pCrs[k]->size();
    }
    else
      pCrs[k] = 0;

    mergeIx[k] = 0;
  }
  if (legs.size() <= sr)
    legs.resize(sr+1);

 
  size_t nleg = 0;
  while (nleg < sr) {
    int best = -1;
    for (int i = 0; i < npar; i++) {
      if (!pCrs[i])
        continue;
      int s = pCrs[i]->size();
      if (mergeIx[i] < s && (best == -1 || (*pCrs[i])[mergeIx[i]] < (*pCrs[best])[mergeIx[best]]) )
        best = i;
    }
    if (best == -1)
      break;
    legs[nleg++] = (*pCrs[best])[mergeIx[best]];
    ++mergeIx[best];
  }

  /*for (size_t k = 0; k < indices.size(); k++) {
    if (indices[k]>=0) {
      const vector<int> &crs = courseKeys[k][indices[k]];
      for (size_t j = 0; j < crs.size(); j++)
        legs[nleg++] = crs[j];
      //legs.insert(crs.begin(), crs.end());
      //vector<int> tl;
      //tl.resize(legs.size() + crs.size());
      //merge(legs.begin(), legs.end(), crs.begin(), crs.end(), tl.begin());
      //tl.swap(legs);
    }
  }*/

  /*for (size_t k = 0; k < indices.size(); k++) {
    if (indices[k]>=0) {
      const vector<int> &crs = courseKeys[k][indices[k]];
      //legs.insert(crs.begin(), crs.end());
      vector<int> tl;
      tl.resize(legs.size() + crs.size());
      merge(legs.begin(), legs.end(), crs.begin(), crs.end(), tl.begin());
      tl.swap(legs);
    }
  }*/
  unsigned long long key = 0;
  for (size_t k = 0; k < nleg; k++)
    key = key * 1057 + legs[k];

  return key;
}

void maximizeSpread(const vector<pCourse> &coursesFirst, const vector<pCourse>& coursesLast, vector<int>& order, int numToGenerate) {
  
  struct Node {
    map<int, Node> children;
    vector<int> courses;
    int readIx = 0;

    void insert(int ix, const pCourse& course, int position) {
      if (course->getNumControls() == position)
        courses.push_back(ix);
      else {
        children[course->getControl(position)->getId()].insert(ix, course, position + 1);
      }
    }

    void insertBack(int ix, const pCourse& course, int position) {
      if (course->getNumControls() == position)
        courses.push_back(ix);
      else {
        children[course->getControl(course->getNumControls() - 1 - position)->getId()].insertBack(ix, course, position + 1);
      }
    }

    void shuffleOrder() {
      if (!courses.empty()) {
        permute(courses);
      }
      for (auto c : children)
        c.second.shuffleOrder();
    }

    int getNextForking(int inKey, unordered_map<int, int>& controlUsage) {
      if (readIx <courses.size()) {
        int res = courses[readIx++];
        return res;
      }
      int mKey = inKey * 197;
      while (!children.empty()) {
        auto it = children.begin();
        int key;
        if (children.size() > 1)
          key = mKey + it->first;
        else
          key = inKey;

        int best = controlUsage[it->first] + controlUsage[key];
        auto next = it;
        while (++it != children.end()) {
          key = mKey + it->first;
          int c = controlUsage[it->first] + controlUsage[key];
          if (c < best) {
            best = c;
            next = it;
          }
        }
        
        if (children.size() > 1)
          key = mKey + next->first;
        else
          key = inKey;

        int res = next->second.getNextForking(key, controlUsage);
        if (res != -1) {
          if (children.size() > 1)
            ++controlUsage[key];
          ++controlUsage[next->first];
          return res;
        }

        // Empty
        children.erase(next);
      }
      return -1;
    }
  };

  Node courseOrderLast;
  for (size_t i = 0; i < coursesLast.size(); i++)
    courseOrderLast.insertBack(i, coursesLast[i], 0);

  courseOrderLast.shuffleOrder();
  unordered_map<int, int> controlUsage;
  order.resize(coursesLast.size());
  for (int i = 0; i < order.size(); i++)
    order[i] = courseOrderLast.getNextForking(0, controlUsage);

  Node courseOrderFirst;
  for (size_t i : order)
    courseOrderFirst.insert(i, coursesFirst[i], 0);

  controlUsage.clear();
  int ns = min<int>(coursesFirst.size(), numToGenerate);
  order.resize(ns);
  for (int i = 0; i < ns; i++)
    order[i] = courseOrderFirst.getNextForking(0, controlUsage);
}

pair<int, int> oClass::autoForking(const vector<vector<int>> &inputCourses, int numToGenerate) {
  if (inputCourses.size() != getNumStages())
    throw meosException("Internal error");
  
  int legs = inputCourses.size();
  vector<int> nf(legs);
  vector<unsigned long long> prod(legs);
  vector<int> ix(legs);
  vector< vector< vector<int> > > courseKeys(legs);
  vector<vector<pCourse>> pCourses(legs);

  unsigned long long N = 1;
  for (int k = 0; k < legs; k++) {
    prod[k] = N;
    if (!inputCourses[k].empty()) {
      N *= inputCourses[k].size();
      nf[k] = inputCourses[k].size();
    }

    // Setup course keys
    courseKeys[k].resize(inputCourses[k].size());
    for (size_t j = 0; j < inputCourses[k].size(); j++) {
      pCourse pc = oe->getCourse(inputCourses[k][j]);
      pCourses[k].push_back(pc);
      if (pc) {
        for (int c = 0; c <= pc->getNumControls(); c++) {
          int from = c == 0 ? 1 : pc->getControl(c-1)->getId();
          int to = c == pc->getNumControls() ? 2 : pc->getControl(c)->getId();
          int key = from + to*997;
          courseKeys[k][j].push_back(key);
        }
        sort(courseKeys[k][j].begin(), courseKeys[k][j].end());
      }
    }
  }

  // Sample if there are very many combinations.
  uint64_t sampleFactor = 1;
  while(N > 10000000) {
    sampleFactor *= 13;
    N /= 13;
  }
  size_t Ns = size_t(N);
  map<long long, int> count;
  vector<int> ws;
  for (size_t k = 0; k < Ns; k++) {
    uint64_t D = uint64_t(k) * sampleFactor;
    for (int j = 0; j < legs; j++) {
      if (nf[j]>0) {
        ix[j] = int((D/prod[j] + j) % nf[j]);
      }
      else ix[j] = -1;
    }
    unsigned long long key = setupForkKey(ix, courseKeys, ws);
    ++count[key];
  }

  // Select the key generating best forking
  long long keyToUse = -1;
  int mv = 0;
  for (map<long long, int>::iterator it = count.begin(); it != count.end(); ++it) {
    if (it->second > mv) {
      keyToUse = it->first;
      mv = it->second;
    }
  }
  count.clear();

  // Clear old forking
  for (int j = 0; j < legs; j++) {
    if (nf[j]>0) {
      clearStageCourses(j);
    }
  }
  set<long long> generatedForkKeys;
  int genLimit = max(numToGenerate * 2, numToGenerate + 100);

  vector<vector<pCourse>> courseMatrix(legs);
  for (size_t k = 0; k < Ns; k++) {
    long long forkKey = 0;
    for (int j = 0; j < legs; j++) {
      uint64_t D = uint64_t((k * 997)%Ns) * sampleFactor;
      if (nf[j]>0) {
        ix[j] = int((D/prod[j] + j) % nf[j]);
        forkKey = forkKey * 997 + ix[j];
      }
      else ix[j] = -1;
    }
    unsigned long long key = setupForkKey(ix, courseKeys, ws);
    if (key == keyToUse && generatedForkKeys.count(forkKey) == 0) {
      generatedForkKeys.insert(forkKey);
      for (int j = 0; j < legs; j++) {
        if (nf[j] > 0) {
   //       coursesUsed.insert(pCourses[j][ix[j]]->getId());
          courseMatrix[j].push_back(pCourses[j][ix[j]]);
        }
      }
    }
    if (generatedForkKeys.size() >= genLimit)
      break;
  }

  vector<int> fperm;
  for (size_t j = 0; j < courseMatrix.size(); j++) {
    if (courseMatrix[j].empty())
      continue;

    int jj = courseMatrix.size() - 1;
    while (courseMatrix[jj].empty())
      jj--;

    maximizeSpread(courseMatrix[j], courseMatrix[jj], fperm, numToGenerate);
    // Take the first used course.
    /*fperm.resize(courseMatrix[j].size());
    for (size_t i = 0; i < fperm.size(); i++)
      fperm[i] = i;*/
    break;
  }

  // Determine first bib in class (if defined)
  wstring bibInfo = getDCI().getString("Bib");
  wchar_t pattern[32];
  int firstNumber = extractBibPattern(bibInfo, pattern);
  if (firstNumber == 0) {
    // Not explicitly defined. Look at any teams
    vector<pTeam> tl;
    oe->getTeams(getId(), tl);
    int minBib = 10000000;
    int minSN = 10000000;
    for (pTeam t : tl) {
      t->getBib();
      int n = extractBibPattern(bibInfo, pattern);
      if (n > 0)
        minBib = std::min(minBib, n);

      int no = t->getStartNo();
      if (no > 0)
        minSN = std::min(minSN, no);
    }
    if (minBib > 0)
      firstNumber = minBib;
    else if (minSN > 0)
      firstNumber = minSN;
  }

  unsigned int off = 0;
  if (firstNumber > 0) {
    // index = (index-1) % courses.size();
    unsigned firstCourse = unsigned(firstNumber - 1) % fperm.size();
    off = fperm.size() - firstCourse;
  }

  set<int> coursesUsed;
  int lastSet = -1;
  for (int j = 0; j < legs; j++) {
    if (nf[j] > 0) {
      lastSet = j;
      for (size_t k = 0; k < fperm.size(); k++) {
        int kk = unsigned(k + off) % fperm.size();
        coursesUsed.insert(courseMatrix[j][fperm[kk]]->getId());
        addStageCourse(j, courseMatrix[j][fperm[kk]], -1);
      }
    }
    else if (lastSet >= 0 && getLegType(j) == LTExtra) {
      MultiCourse[j] = MultiCourse[lastSet];
    }
    else {
      lastSet = -1;
    }
  }

  return make_pair<int, int>(fperm.size(), coursesUsed.size());
}

int oClass::extractBibPattern(const wstring &bibInfo, wchar_t pattern[32]) {
  int number = 0;

  if (bibInfo.empty())
    pattern[0] = 0;
  else {
    number = 0;
    int pIndex = 0;
    bool hasNC = false;

    for (size_t j = 0; j < bibInfo.size() && j < 10; j++) {
      if (bibInfo[j]>='0' && bibInfo[j]<='9') {
        if (!hasNC) {
          pattern[pIndex++] = '%';
          pattern[pIndex++] = 'd';
          hasNC = true;
        }
        number = 10 * number + bibInfo[j]-'0';
      }
      else if (bibInfo[j] != '%' && bibInfo[j] > 32)
        pattern[pIndex++] = bibInfo[j]; 
    }
    if (!hasNC) {
      pattern[pIndex++] = '%';
      pattern[pIndex++] = 'd';
      number = 1;
    }
    pattern[pIndex] = 0;
  }
  return number;
}

AutoBibType oClass::getAutoBibType() const {
  const wstring &bib = getDCI().getString("Bib");
  if (bib.empty()) // Manual
    return AutoBibManual;
  else if (bib == L"*") // Consecutive
    return AutoBibConsecutive;
  else if (bib == L"-") // No bib
    return AutoBibNone;
  else
    return AutoBibExplicit;
}

bool oClass::hasAnyCourse(const set<int> &crsId) const {
  if (Course && crsId.count(Course->getId()))
    return true;

  for (size_t j = 0; j < MultiCourse.size(); j++) {
    for (size_t k = 0; k < MultiCourse[j].size(); k++) {
      if (MultiCourse[j][k] && crsId.count(MultiCourse[j][k]->getId()))
        return true;
    }
  }

  return false;
}

bool oClass::usesCourse(const oCourse &crs) const {
  if (Course == &crs)
    return true;

  for (size_t j = 0; j < MultiCourse.size(); j++) {
    for (size_t k = 0; k < MultiCourse[j].size(); k++) {
      if (MultiCourse[j][k] == &crs)
        return true;
    }
  }

  return false;
}

void oClass::extractBibPatterns(oEvent &oe, map<int, pair<wstring, int> > &patterns) {
  vector<pTeam> t;
  oe.getTeams(0, t, true);
  vector<pRunner> r;
  oe.getRunners(0, 0, r, true);
  patterns.clear();
  wchar_t pattern[32];
  for (size_t k = t.size(); k > 0; k--) {
    int cls = t[k-1]->getClassId(true);
    if (cls == 0)
      continue;
    const wstring &bib = t[k-1]->getBib();
    if (!bib.empty()) {
      int num = extractBibPattern(bib, pattern);
      if (num > 0) {
        pair<wstring, int> &val = patterns[cls];
        if (num > val.second) {
          val.second = num;
          val.first = pattern;
        }
      }
    }
  }

  for (size_t k = r.size(); k > 0; k--) {
    if (r[k-1]->getTeam() != 0)
      continue;
    int cls = r[k-1]->getClassId(true);
    if (cls == 0)
      continue;
    const wstring &bib = r[k-1]->getBib();
    if (!bib.empty()) {
      int num = extractBibPattern(bib, pattern);
      if (num > 0) {
        pair<wstring, int> &val = patterns[cls];
        if (num < val.second) {
          val.second = num;
          val.first = pattern;
        }
      }
    }
  }
}

pair<int, wstring> oClass::getNextBib(map<int, pair<wstring, int> > &patterns) {
  map<int, pair<wstring, int> >::iterator it = patterns.find(Id);
  if (it != patterns.end() && it->second.second > 0) {
    wchar_t bib[32];
    swprintf(bib, sizeof(bib)/sizeof(wchar_t), it->second.first.c_str(), ++it->second.second);
    return make_pair(it->second.second, bib);  
  }
  return make_pair(0, _EmptyWString);
}

pair<int, wstring> oClass::getNextBib() {
  vector<pTeam> t;
  oe->getTeams(Id, t, true);
  set<int> bibs;
  wchar_t pattern[32];

  if (!t.empty()) {
    for (size_t k = 0; k < t.size(); k++) {
      const wstring &bib = t[k]->getBib();
      if (!bib.empty()) {
        int num = extractBibPattern(bib, pattern);
        if (num > 0) {
          bibs.insert(num);
        }
      }
    }
  }
  else {
    vector<pRunner> r;
    oe->getRunners(Id, 0, r, true);
 
    for (size_t k = 0; k < r.size(); k++) {
      if (r[k]->getTeam() != 0)
        continue;
      const wstring &bib = r[k]->getBib();
      if (!bib.empty()) {
        int num = extractBibPattern(bib, pattern);
        if (num > 0) {
          bibs.insert(num);
        }
      }
    }
  }

  if (bibs.empty()) {
    wstring bibInfo = getDCI().getString("Bib");
    int firstNumber = extractBibPattern(bibInfo, pattern);
    if (firstNumber > 0)
      return make_pair(firstNumber, bibInfo);

    return make_pair(0, _EmptyWString);
  }
  int candidate = -1;
  for (set<int>::iterator it = bibs.begin(); it != bibs.end(); ++it) {
    if (candidate > 0 && *it != candidate) {
      break;
    }
    candidate = *it + 1;
  }

  wchar_t bib[32];
  swprintf(bib, sizeof(bib)/sizeof(wchar_t), pattern, candidate);
  return make_pair(candidate, bib);
}

int oClass::getNumForks() const {
  set<int> factors;
  for (size_t k = 0; k < MultiCourse.size(); k++) {
    int f = MultiCourse[k].size();
    if (f <= 1)
      continue;
    bool skip = false;
    for (set<int>::iterator it = factors.begin(); it != factors.end(); ++it) {
      int of = *it;
      if (of == f) {
        skip = true;
        break;
      }
      else if (f < of && (of % f) == 0) {
        skip = true;
        break;
      }
      else if (of < f && (f % of) == 0) {
        factors.erase(it);
        it = factors.begin();
        continue;
      }
    }
    if (!skip)
      factors.insert(f);
  }
  int res = 1;
  for (set<int>::iterator it = factors.begin(); it != factors.end(); ++it) {
    res *= *it;
  }
  return res;
}

void oClass::getSeedingMethods(vector< pair<wstring, size_t> > &methods) {
  methods.clear();
  methods.push_back(make_pair(lang.tl("Resultat"), SeedResult));
  methods.push_back(make_pair(lang.tl("Tid"), SeedTime));
  methods.push_back(make_pair(lang.tl("Ranking"), SeedRank));
  methods.push_back(make_pair(lang.tl("Poäng"), SeedPoints));
}

void oClass::drawSeeded(ClassSeedMethod seed, int leg, int firstStart, 
                        int interval, const vector<int> &groups,
                        bool noClubNb, bool reverseOrder, int pairSize) {
  vector<pRunner> r;
  oe->getRunners(Id, 0, r, true);
  vector< pair<int, int> > seedIx;
  if (seed == SeedResult) {
    oe->reEvaluateAll(set<int>(), true);
    oe->calculateResults({}, oEvent::ResultType::ClassResult, false);
  }
  for (size_t k = 0; k < r.size(); k++) {
    if (r[k]->tLeg != leg && leg != -1)
      continue;

    pair<int,int> sx;
    sx.second = k;
    if (seed == SeedRank)
      sx.first = r[k]->getRanking();
    else if (seed == SeedResult)
      sx.first = evaluateSeedResult(*r[k]);
    else if (seed == SeedTime)
      sx.first = evaluateSeedTime(*r[k]);
    else if (seed == SeedPoints)
      sx.first = evaluateSeedPoints(*r[k]);
    else
      throw meosException("Not yet implemented");

    seedIx.push_back(sx);
  }

  sort(seedIx.begin(), seedIx.end());

  if (seedIx.empty() || seedIx.front().first == seedIx.back().first) {
    throw meosException("error:invalidmethod");
  }

  vector<size_t> seedSpec;
  if (groups.size() == 1) {
    size_t added = 0;
    if (groups[0] <= 0)
      throw meosException("Internal error");
    while (added < seedIx.size()) {
      seedSpec.push_back(groups[0]);
      added += groups[0];
    }
  }
  else {
    size_t added = 0;
    for (size_t k = 0; k < groups.size(); k++) {
      if (groups[k] <= 0)
        throw meosException("Internal error");
     
      seedSpec.push_back(groups[k]);
      added += groups[k];
    }
    if (added < seedIx.size())
      seedSpec.push_back(seedIx.size() - added);
  }

  list< vector<int> > seedGroups(1);
  size_t groupIx = 0;
  for (size_t k = 0; k < seedIx.size(); ) {
    if (groupIx < seedSpec.size() && seedGroups.back().size() >= seedSpec[groupIx]) {
      groupIx++;
      seedGroups.push_back(vector<int>());
    }

    int value = seedIx[k].first;
    while (k < seedIx.size() && seedIx[k].first == value) {
      seedGroups.back().push_back(seedIx[k].second);
      k++;
    }
  }

  vector<pRunner> startOrder;

  for (list< vector<int> >::iterator it = seedGroups.begin();
       it != seedGroups.end(); ++it) {
    vector<int> &g = *it;
    permute(g);
    for (size_t k = 0; k < g.size(); k++) {
      startOrder.push_back(r[g[k]]);
    }
  }
  
  if (noClubNb) {
    set<int> pushed_back;
    for (int k = 1; k < startOrder.size(); k++) {
      int idMe = startOrder[k]->getClubId();
      if (idMe != 0 && idMe == startOrder[k-1]->getClubId()) {
        // Make sure the runner with worst ranking is moved back. (Swedish SM rules)
        bool skipRank = pushed_back.count(startOrder[k - 1]->getId()) != 0;
        if (!skipRank &&  startOrder[k-1]->getRanking() > startOrder[k]->getRanking())
          swap(startOrder[k-1], startOrder[k]);
        pushed_back.insert(startOrder[k]->getId());
        vector<pair<int, pRunner> > rqueue;
        rqueue.push_back(make_pair(k, startOrder[k]));
        for (int j = k + 1; j < startOrder.size(); j++) {
          if (idMe != startOrder[j]->getClubId()) {
            pushed_back.insert(startOrder[j]->getId());
            swap(startOrder[j], startOrder[k]); // k-1 now has a non-club nb behind
            rqueue.push_back(make_pair(j, nullptr));
            // Shift the queue
            for (size_t q = 1; q < rqueue.size(); q++) {
              startOrder[rqueue[q].first] = rqueue[q-1].second;
            }
            break;
          }
          else {
            rqueue.push_back(make_pair(j, startOrder[j]));
          }

        }
        /*for (size_t j = k + 1; j < startOrder.size(); j++) {
          swap(startOrder[j], startOrder[j-1]);
          if (idMe != startOrder[j]->getClubId() && j+1 < startOrder.size() &&
              idMe != startOrder[j+1]->getClubId()) {
            break;
          }
        }*/
      }
    }
    // Handle special case where the two last have same club.
    int last = startOrder.size() - 1;
    if (last >= 3) {
      int lastClub = startOrder[last]->getClubId();
      if ( lastClub == startOrder[last-1]->getClubId() &&
           lastClub != startOrder[last-2]->getClubId() &&
           lastClub != startOrder[last-3]->getClubId() ) {
        swap(startOrder[last-1], startOrder[last-2]);
      }
    }
  }

  if (!reverseOrder)
    reverse(startOrder.begin(), startOrder.end());
  
  for (size_t k = 0; k < startOrder.size(); k++) {
    int kx = k/pairSize;
    startOrder[k]->setStartTime(firstStart + interval * kx, true, oBase::ChangeType::Update, false);
    startOrder[k]->storeDefaultStartTime();
    startOrder[k]->synchronize(true);
  }
}

bool oClass::hasClassGlobalDependence() const {
  for (size_t k = 0; k < legInfo.size(); k++) { 
    if (legInfo[k].startMethod == STPursuit)
      return true;
  }
  return false;
}

int oClass::getDrawFirstStart() const {
  return getDCI().getInt("FirstStart");    
}

void oClass::setDrawFirstStart(int st) {
  getDI().setInt("FirstStart", st);
}

int oClass::getDrawInterval() const {
  return getDCI().getInt("StartInterval");    
}

void oClass::setDrawInterval(int st) {
  getDI().setInt("StartInterval", st);
}

int oClass::getDrawVacant() const {
  return getDCI().getInt("Vacant");    
}

void oClass::setDrawVacant(int st) {
  getDI().setInt("Vacant", st);
}

int oClass::getDrawNumReserved() const {
  return getDCI().getInt("Reserved") & 0xFF;    
}

void oClass::setDrawNumReserved(int st) {
  int v = getDCI().getInt("Reserved") & 0xFF00;
  getDI().setInt("Reserved", v|st);
}

void oClass::setDrawSpecification(const vector<DrawSpecified> &spec) {
  int flag = 0;
  for (auto ds : spec) {
    flag |= int(ds);
  }
  int v = getDrawNumReserved();
  getDI().setInt("Reserved", v | (flag<<8));
}

set<oClass::DrawSpecified> oClass::getDrawSpecification() const {
  int v = (getDCI().getInt("Reserved") & 0xFF00) >> 8;
  set<DrawSpecified> res;
 
  for (auto dk : DrawKeys) {
    if (int(dk) & v)
      res.insert(dk);
  }
  return res;
}

void oClass::initClassId(oEvent &oe, const set<int>& classes) {
  vector<pClass> cls;
  oe.getClasses(cls, true);
  map<long long, wstring> id2Cls;
  for (size_t k = 0; k < cls.size(); k++) {
    if (!classes.empty() && !classes.count(cls[k]->getId()))
      continue;

    long long extId = cls[k]->getExtIdentifier();
    if (extId > 0) {
      if (id2Cls.count(extId)) {
        throw meosException(L"Klasserna X och Y har samma externa id. Använd tabelläget för att ändra id.#" +
                            id2Cls[extId] + L"#" + cls[k]->getName()); 
      }
      id2Cls[extId] = cls[k]->getName();
    } 
  }
  // Generate external identifiers when not set
  for (size_t k = 0; k < cls.size(); k++) {
    if (!classes.empty() && !classes.count(cls[k]->getId()))
      continue;

    long long extId = cls[k]->getExtIdentifier();
    if (extId == 0) {
      long long id = cls[k]->getId();
      while (id2Cls.count(id)) {
        id += 100000;
      }
      id2Cls[id] = cls[k]->getName();
      cls[k]->setExtIdentifier(id);
    }
  }
}

int oClass::getNextBaseLeg(int leg) const {
  for (size_t k = leg + 1; k  < legInfo.size(); k++) {
    if (!(legInfo[k].isParallel() || legInfo[k].isOptional()))
      return k;
  }
  return -1; // No next leg
}

int oClass::getPreceedingLeg(int leg) const {
  if (size_t(leg) >= legInfo.size())
    leg = legInfo.size() - 1;
  for (int k = leg; k > 0; k--) {
    if (!(legInfo[k].isParallel() || legInfo[k].isOptional()))
      return k-1;
  }
  return -1;
}

int oClass::getResultDefining(int leg) const {
  int res = leg;
  while (size_t(res+1) < legInfo.size() &&
         (legInfo[res+1].isParallel() || legInfo[res+1].isOptional()))
    res++;

  if (size_t(res) >= legInfo.size())
    res = legInfo.size() - 1;

  return res;
}

bool oClass::lockedForking() const {
  return (getDCI().getInt("Locked") & 1) == 1;
}

void oClass::lockedForking(bool locked) {
  int current = getDCI().getInt("Locked");
  getDI().setInt("Locked", locked ? (current | 1) : (current & ~1));
}

bool oClass::lockedClassAssignment() const {
  return (getDCI().getInt("Locked") & 2) == 2;
}

void oClass::lockedClassAssignment(bool locked) {
  int current = getDCI().getInt("Locked");
  getDI().setInt("Locked", locked ? (current | 2) : (current & ~2));
}

oClass *oClass::getVirtualClass(int instance, bool allowCreation) {
  if (instance == 0)
    return this;
  if (parentClass)
    return parentClass->getVirtualClass(instance, allowCreation);

  if (size_t(instance) < virtualClasses.size() && virtualClasses[instance])
    return virtualClasses[instance];

  if (instance >= getNumQualificationFinalClasses())
    return this; // Invalid
  virtualClasses.resize(getNumQualificationFinalClasses());
  int virtId = Id + instance * MaxClassId;
  virtualClasses[instance] = oe->getClass(virtId);
  if (virtualClasses[instance]) {
    virtualClasses[instance]->parentClass = pClass(this);
    return virtualClasses[instance];
  }
  configureInstance(instance, allowCreation);
  if (virtualClasses[instance])
    return virtualClasses[instance];
  return this; // Fallback
}

const pClass oClass::getVirtualClass(int instance) const {
  if (instance == 0)
    return pClass(this);
  if (parentClass)
    return parentClass->getVirtualClass(instance);

  if (size_t(instance) < virtualClasses.size() && virtualClasses[instance]) {
    virtualClasses[instance]->parentClass = pClass(this);
    return virtualClasses[instance];
  }

  if (instance >= getNumQualificationFinalClasses())
    return pClass(this); // Invalid
  virtualClasses.resize(getNumQualificationFinalClasses());

  int virtId = Id + instance * MaxClassId;
  virtualClasses[instance] = oe->getClass(virtId);
  if (virtualClasses[instance]) {
    virtualClasses[instance]->parentClass = pClass(this);
    return virtualClasses[instance];
  }
  configureInstance(instance, false);
  if (virtualClasses[instance])
    return virtualClasses[instance];
  return pClass(this); // Fallback
}

void oClass::configureInstance(int instance, bool allowCreation) const {
  int virtId = Id + instance * MaxClassId;
  virtualClasses[instance] = oe->getClass(virtId);
  if (virtualClasses[instance]) {
    virtualClasses[instance]->parentClass = pClass(this);
    return;
  }
  if (!allowCreation)
    return;

  oClass copy(*this);
  copy.Id = Id + instance * MaxClassId;
  copy.setExtIdentifier(copy.Id);
  copy.Name += makeDash(L"-") + qualificatonFinal->getInstanceName(instance);
  copy.sqlUpdated.clear();
  copy.parentClass = pClass(this);
  copy.tSortIndex += instance;
  copy.getDI().setInt("SortIndex", copy.tSortIndex);
  copy.legInfo.clear();
  copy.MultiCourse.clear();
  copy.getDI().setString("Qualification", L"");
  virtualClasses[instance] = oe->addClass(copy);
}

int oClass::getNumQualificationFinalClasses() const {
  reinitialize(false);
  if (qualificatonFinal)
    return qualificatonFinal->getNumClasses()+1;
  return 0;
}

void oClass::loadQualificationFinalScheme(const QualificationFinal& scheme) {
  auto qf = make_shared<QualificationFinal>(MaxClassId, Id);
  qf->setClasses(scheme.getClasses());
  wstring enc;
  qf->encode(enc);
  
  const int oldNS = getNumStages();
  const int ns = qf->getNumStages();

  setNumStages(ns);
  for (int i = 1; i < ns; i++) {
    setStartType(i, StartTypes::STDrawn, true);
    setLegType(i, LegTypes::LTNormal);
    setLegRunner(i, 0);
  }

  // Clear any old scheme
  clearQualificationFinal();
  qualificatonFinal = qf;
  getDI().setString("Qualification", enc);
  for (int i = 0; i < qualificatonFinal->getNumClasses(); i++) {
    pClass inst = getVirtualClass(i+1, true);
    inst->synchronize();
  }
  synchronize();
  set<int> base;
  qf->getBaseClassInstances(base);
  for (oRunner &r : oe->Runners) {
    if (r.getClassRef(false) == this) {
      if (r.getLegNumber() == 0 && !base.count(r.getDCI().getInt("Heat")))
        r.getDI().setInt("Heat", 0);
      pTeam t = r.getTeam();
      if (t == nullptr) {
        t = oe->addTeam(r.getName(), r.getClubId(), getId());
        t->setStartNo(r.getStartNo(), oBase::ChangeType::Update);
        t->setRunner(0, &r, true);
      }
      r.synchronizeAll();
    }
  }

  if (oldNS != ns) {
    for (oRunner& r : oe->Runners) {
      r.createMultiRunner(true, true);
    }
  }
}

void oClass::updateFinalClasses(oRunner* causingResult, bool updateStartNumbers) {
  if (!qualificatonFinal)
    return;
  assert(!causingResult || causingResult->Class == this);

  int causingLevel = causingResult ? causingResult->tLeg : 0;

  //oe->gdibase.addStringUT(0, L"UF:" + getName() + L" for " + (causingResult ? causingResult->getName() : L"-"));
  auto computeInstance = [this](const pRunner causingResult) {
    if (causingResult) {
      int instance = causingResult->classInstance();
      pClass currentInst = getVirtualClass(instance, false);
      if (qualificatonFinal->isFinalClass(instance))
        return -1; // Final class
      if (instance > 0) {
        int baseLevel = qualificatonFinal->getLevel(instance);
        instance = qualificatonFinal->getMinInstance(baseLevel);
      }
      return instance;
    }
    return 0;
  };
  const int baseInstance = computeInstance(causingResult);
  if (baseInstance < 0)
    return; // Final class
  int instance = baseInstance;

  const int maxDepth = getNumStages();
  bool needIter = true;
  const int limit = virtualClasses.size() - 1;
  bool wasReset = false;

  // Store all runners for the class (and runners explicitly set to final class)
  // Second becomes true for a runner that is qualified  
  vector<pair<pRunner, bool>> clsRunners;
  for (oRunner& r : oe->Runners) {
    if (r.isRemoved() || !r.Class)
      continue;

    if (r.Class != this && (r.Class->getId() % MaxClassId) != getId())
      continue;

    clsRunners.emplace_back(&r, r.Class != this);
  }

  unordered_set<int> qualifiedRunners;
  unordered_set<int> unQualifiedRunners;
  unordered_set<int> superQualifiedNext;
  vector<vector<pRunner>> classSplit(virtualClasses.size());
  vector<vector<pRunner>> nonQualifiedLevel(getNumStages());

  int maxIter = maxDepth;
  while (needIter && --maxIter > 0) {
    needIter = false;
    if (size_t(instance) >= virtualClasses.size())
      break; // Final class

    for (auto& c : classSplit)
      c.clear();
    for (auto& nq : nonQualifiedLevel)
      nq.clear();

    for (size_t ix = 0; ix < clsRunners.size(); ix++) {
      oRunner &r = *clsRunners[ix].first;
      int inst = r.Class == this ? r.classInstance() : (r.Class->getId() - getId()) / MaxClassId;
      if (r.Class == this && unQualifiedRunners.count(r.getId()))
        inst = 0;

      if (inst == 0 && r.tLeg > 0) {
        if (r.tLeg < getNumStages() && !clsRunners[ix].second)
          nonQualifiedLevel[r.tLeg].push_back(&r);
        continue; // Only allow base class for leg 0.
      }
      if (inst < instance || inst >= limit)
        continue;

      classSplit[inst].push_back(&r);
    }

    bool hasRemaining = qualificatonFinal->hasRemainingClass();
    GeneralResult gr;
    qualificatonFinal->prepareCalculations();
    
    struct TotalLevelRes {
      pRunner r;
      int place;
      int instance;
      int orderPlace;
      int numEqual;

      TotalLevelRes(int instance, pRunner r, int place,  int orderPlace, int numEqual) : r(r), instance(instance),
        orderPlace(orderPlace), numEqual(numEqual), place(place) {}

      TotalLevelRes(int instance, pRunner r) : r(r), instance(instance),
        orderPlace(numeric_limits<int>::max()), numEqual(0), 
        place(numeric_limits<int>::max()) {}

      bool operator<(const TotalLevelRes& other) const {
        return orderPlace < other.orderPlace;
      }
    };

    vector<vector<TotalLevelRes>> levelRes(maxDepth+1);

    int maxLevel = 0;
    for (int i = instance; i < limit; i++) {
      if (classSplit[i].empty())
        continue;
      const int thisLevel = qualificatonFinal->getLevel(i);
      maxLevel = max(maxLevel, thisLevel);

      if (i == 0 && qualificatonFinal->noQualification(i)) {
        set<int> allowed;
        qualificatonFinal->getBaseClassInstances(allowed);
        // Place all in this group
        for (int rix = classSplit[0].size() - 1; rix >= 0; rix--) {
          pRunner r = classSplit[0][rix];
          auto di = r->getDI();
          int oldHeat = di.getInt("Heat");
          if (allowed.count(oldHeat) || classSplit.size() < 2)
            continue;
          // Take the smallest group. User can set heat explicitly of other distribution is wanted.
          int heat = 1;
          for (int i : allowed) {
            if (size_t(i) < classSplit.size() &&
                classSplit[heat].size() > classSplit[i].size())
              heat = i;
          }
          if (heat != oldHeat) {
            bool lockedStartList = getVirtualClass(heat)->lockedClassAssignment() ||
              getVirtualClass(oldHeat)->lockedClassAssignment();

            if (!lockedStartList) {
              classSplit[heat].push_back(r);
              classSplit[0][rix] = classSplit[0].back();
              classSplit[0].pop_back();
              //classSplit[0].erase(classSplit[0].begin() + rix);
              pClass oldClass = r->getClassRef(true);
              oldClass->markSQLChanged(-1, 0);
              di.setInt("Heat", heat);
              r->classInstanceRev.first = -1;
              r->synchronize();
              oe->classIdToRunnerHash.reset();
            }
          }
        }
      }

      gr.calculateIndividualResults(classSplit[i], false, oListInfo::Classwise, true, 0);

      int lastPlace = 0, orderPlace = 1;
      int numEqual = 0;
      
      for (size_t k = 0; k < classSplit[i].size(); k++) {
        const auto &res = classSplit[i][k]->getTempResult();
        if (res.getStatus() == StatusOK) {
          int place = res.getPlace();
          if (lastPlace == place)
            numEqual++;
          else
            numEqual = 0;

          levelRes[thisLevel].emplace_back(i, classSplit[i][k], place, orderPlace, numEqual);
      //    qualificatonFinal->provideQualificationResult(classSplit[i][k], i, orderPlace, numEqual);
          lastPlace = place;
        }
        else if (hasRemaining && res.getStatus() != StatusUnknown) {
          levelRes[thisLevel].emplace_back(i, classSplit[i][k]);
          //qualificatonFinal->provideUnqualified(thisLevel, classSplit[i][k]);
        }
        orderPlace++;
      }
    }

    for (int level = 0; level < levelRes.size(); level++) {
      if (levelRes[level].empty())
        continue;
      stable_sort(levelRes[level].begin(), levelRes[level].end());

      for (auto& res : levelRes[level]) {
        if (res.orderPlace < numeric_limits<int>::max())
          qualificatonFinal->provideQualificationResult(res.r, res.instance, res.orderPlace, res.numEqual);
        else
          qualificatonFinal->provideUnqualified(level, res.r);
      }
    }

    if (hasRemaining) {
      for (int level = 0; level < maxLevel; level++) {
        vector<pair<int, pRunner>> sortedByResult;
        sortedByResult.reserve(nonQualifiedLevel[level].size());
        for (pRunner r : nonQualifiedLevel[level]) {
          pRunner finalR = r;
          int okCount = 0;
          for (int i = 0; i <= r->tLeg; i++) {
            if (r->getTeam() && r->getTeam()->getRunner(i)) {
              pRunner rx = r->getTeam()->getRunner(i);
              if (rx->getStatus() == StatusOK) {
                finalR = rx;
                okCount++;
              }
            }
          }
          int score = okCount * 10000;
          if (finalR->getStatus() == StatusOK)
            score -= finalR->getPlace();
          sortedByResult.emplace_back(-score, r);
        }
        sort(sortedByResult.begin(), sortedByResult.end());

        for (auto rr : sortedByResult)
          qualificatonFinal->provideUnqualified(level, rr.second);
      }
    }

    qualificatonFinal->computeFinals();

    auto qualifyNext = [&](const oRunner& thisRunner, int thisLevel) {
      auto res = qualificatonFinal->getNextFinal(thisRunner.getId());
      int heat = res.first;
      int nextLevel = qualificatonFinal->getLevel(heat);
      int levelInc = max(1, nextLevel - thisLevel);
      if (levelInc > 1) {
        pRunner runnerBefore = thisRunner.getMultiRunner(thisRunner.getRaceNo() + levelInc - 1);
        if (runnerBefore)
          superQualifiedNext.insert(runnerBefore->getId());
      }

      pRunner runnerToChange = thisRunner.getMultiRunner(thisRunner.getRaceNo() + levelInc);

      if (runnerToChange) {
        auto di = runnerToChange->getDI();
        int oldHeat = di.getInt("Heat");
        if (heat != oldHeat) {
          bool lockedStartList = (heat != 0 && getVirtualClass(heat)->lockedClassAssignment()) ||
            (oldHeat != 0 && getVirtualClass(oldHeat)->lockedClassAssignment());

          if (!lockedStartList) {
            if (heat > 0)
              qualifiedRunners.insert(runnerToChange->getId());
            else if (oldHeat > 0)
              unQualifiedRunners.insert(runnerToChange->getId());

            pClass oldClass = runnerToChange->getClassRef(true);
            oldClass->markSQLChanged(-1, 0);
            di.setInt("Heat", heat);
            runnerToChange->classInstanceRev.first = -1;
            oe->classIdToRunnerHash.reset();

            //oe->gdibase.addStringUT(0, L"HU:" + thisRunner.getName() + L" " + itow(oldHeat) + L"->" + itow(heat));
            runnerToChange->apply(ChangeType::Quiet, nullptr);
            runnerToChange->synchronize();
            if (runnerToChange->getFinishTime() > 0)
              needIter = true;
          }
        }
        else {
          if (heat > 0)
            qualifiedRunners.insert(runnerToChange->getId());
        }
      }
    };

    for (int i = instance; i < limit; i++) {      
      const int thisLevel = qualificatonFinal->getLevel(i);
      const int nextLevel = qualificatonFinal->getLevel(i + 1);

      for (size_t k = 0; k < classSplit[i].size(); k++) {
        qualifyNext(*classSplit[i][k], thisLevel);
      }
      
      if (hasRemaining) {
        if (thisLevel != nextLevel) {
          for (pRunner r : nonQualifiedLevel[thisLevel]) {
            if (!superQualifiedNext.count(r->getId()))
              qualifyNext(*r, thisLevel);
          }
        }
      }

      if (thisLevel != nextLevel && needIter) {
        instance = i+1; // Need not process last class again
        break;
      }
    }
  }

  int nextLevelInstance = baseInstance;
  int baseLevel = qualificatonFinal->getLevel(baseInstance);
  while (nextLevelInstance < limit && qualificatonFinal->getLevel(nextLevelInstance) == baseLevel)
    nextLevelInstance++;

  // Set runners that became unqualified
  for (auto& cc : clsRunners) {
    pRunner r = cc.first;
    int inst = r->Class == this ? r->classInstance() : (r->Class->getId() - getId()) / MaxClassId;
    
    if (inst < nextLevelInstance)
      continue;
       
    if (!cc.second && cc.first->tLeg > causingLevel && !qualifiedRunners.count(cc.first->getId())) {
      auto di = cc.first->getDI();
      int oldHeat = di.getInt("Heat");
      if (oldHeat != 0 && !getVirtualClass(oldHeat)->lockedClassAssignment()) {
        pClass oldClass = cc.first->getClassRef(true);
        oldClass->markSQLChanged(-1, 0);
        di.setInt("Heat", 0);
        cc.first->classInstanceRev.first = -1;
        oe->classIdToRunnerHash.reset();
        cc.first->apply(ChangeType::Quiet, nullptr);
        cc.first->synchronize();
      }
    }
  }
}

vector<pair<wstring, size_t>> oClass::getAllFees() const {
  set<int> fees;
  int f = getDCI().getInt("ClassFee");
  if (f > 0)
    fees.insert(f);

  f = getDCI().getInt("ClassFeeRed");
  if (f > 0)
    fees.insert(f);

  f = getDCI().getInt("HighClassFee");
  if (f > 0)
    fees.insert(f);

  f = getDCI().getInt("HighClassFeeRed");
  if (f > 0)
    fees.insert(f);

  if (fees.empty()) {
    f = oe->getDCI().getInt("EliteFee");
    if (f > 0)
      fees.insert(f);

    f = oe->getDCI().getInt("EntryFee");
    if (f > 0)
      fees.insert(f);

    f = oe->getDCI().getInt("YouthFee");
    if (f > 0)
      fees.insert(f);
  }
  vector< pair<wstring, size_t> > ff;
  for (set<int>::iterator it = fees.begin(); it != fees.end(); ++it)
    ff.emplace_back(oe->formatCurrency(*it), *it);

  return ff;
}

bool oEvent::hasAnyRestartTime() const {
  for (auto &c : Classes) {
    if (c.isRemoved())
      continue;

    for (auto &leg : c.legInfo) {
      if (leg.legRopeTime > 0 && leg.legRestartTime > 0)
        return true;
    }
  }

  return false;
}

GeneralResult *oClass::getResultModule() const {
  const string &tag = getResultModuleTag();
  if (tag.empty())
    return nullptr;
  wstring sf;
  return oe->getGeneralResult(tag, sf).get();
}

void oClass::setResultModule(const string &tag) {
  string oldTag = getResultModuleTag();
  if (tag == oldTag)
    return;

  vector<pClass> cls;
  oe->getClasses(cls, false);
  bool inUse = false;
  bool oldInUse = false;

  for (pClass c : cls) {
    if (c == this)
      continue;

    if (c->getResultModuleTag() == tag) {
      inUse = true;
    }
    if (c->getResultModuleTag() == oldTag) {
      oldInUse = true;
    }
  }

  if (!tag.empty()) {
    wstring fn;
    auto &ptr = oe->getGeneralResult(tag, fn);
    if (ptr->isDynamic()) {
      auto dptr = dynamic_pointer_cast<DynamicResult>(ptr);
      oe->getListContainer().updateGeneralResult(tag, dptr);
    }
  }
  if (!oldTag.empty() && !oldInUse) {
    oe->getListContainer().updateGeneralResult(oldTag, nullptr);
  }
  oe->synchronize(true);
  wstring wtag(tag.begin(), tag.end());
  getDI().setString("Result", wtag);
}

const string &oClass::getResultModuleTag() const {
  auto ws = getDCI().getString("Result");
  string &s = StringCache::getInstance().get();
  s.clear();
  s.insert(s.begin(), ws.begin(), ws.end());
  return s;
}

bool oClass::hasFlag(TransferFlags flag) const {
  return (getDCI().getInt("TransferFlags") & flag) != 0;
}

void oClass::setFlag(TransferFlags flag, bool onoff) {
  int cf = getDCI().getInt("TransferFlags");
  cf = onoff ? (cf | flag) : (cf & (~flag));
  getDI().setInt("TransferFlags", cf);
}

void oClass::adjustNumVacant(int leg, int numVacant) {
  const int vacantClubId = oe->getVacantClub(false);
  const bool multiDay = oe->hasPrevStage();
  
  if (numVacant < 0)
    throw meosException("Internal error");

  if (numVacant > 0 && getClassType() == oClassRelay)
    throw meosException("Vakanser stöds ej i stafett.");

  if (numVacant > 0 && (leg > 0 || getParentClass()))
    throw meosException("Det går endast att sätta in vakanser på sträcka 1.");

  if (size_t(leg) < legInfo.size()) {
    setStartType(leg, STDrawn, true); //Automatically change start method
  }
  else if (leg == -1) {
    for (size_t j = 0; j < legInfo.size(); j++)
      setStartType(j, STDrawn, true); //Automatically change start method
  }
  
  vector<int> currentVacant;
  vector<pRunner> rList;
  oe->getRunners({ Id }, rList);

  for (pRunner r : rList) {
    if (r->tInTeam)
      continue; // Cannot remove team runners
    if (r->getClubId() == vacantClubId)
      currentVacant.push_back(r->getId());
  }

  vector<int> toRemove;
  while (currentVacant.size() > numVacant) {
    toRemove.push_back(currentVacant.back());
    currentVacant.pop_back();
  }

  while (currentVacant.size() < numVacant) {
    pRunner r = oe->addRunnerVacant(Id);
    currentVacant.push_back(r->getId());
  }
  oe->removeRunner(toRemove);
}

/** Get best rogaining time for leg, and expected time given base speed*/
oClass::RogainingAnalysis oClass::getRogainingAnalysis(int from, int to, double baseSpeed) const {
  RogainingAnalysis out;
  if (!isRogaining())
    return out;

  if (rogainingStatistics.needsUpdate(*oe))
    oe->computeRogainingStatistics();

  auto &rgMap = rogainingStatistics.get();
  auto res = rgMap.find(make_pair(from, to));
  if (res != rgMap.end()) {
    out.bestTime = res->second.bestTime;
    out.lostTime = int(res->second.bestTime * baseSpeed);
    out.numLegRunners = res->second.numCompetitors;
  }

  return out;
}

/** Get class statistics rogaining legs */
vector<oClass::RogainingLeg> oClass::getRogainingLegs() const {
  vector<oClass::RogainingLeg> out;

  if (!isRogaining())
    return out;

  if (rogainingStatistics.needsUpdate(*oe))
    oe->computeRogainingStatistics();

  for (auto &[key, stat] : rogainingStatistics.get()) {
    RogainingLeg s;
    s.from = key.first;
    s.to = key.second;
    s.numCompetitors = stat.numCompetitors;
    s.bestTime = stat.bestTime;
    out.push_back(s);
  }

  sort(out.begin(), out.end(), [](const RogainingLeg &a, const RogainingLeg &b) {return a.numCompetitors > b.numCompetitors; });

  return out;
}

bool oClass::isSingleStageOnly() const {
  return getDCI().getInt("NoTotalResult") != 0;
}

void oClass::setSingleStageOnly(bool singleStageOnly) {
  getDI().setInt("NoTotalResult", singleStageOnly);
}

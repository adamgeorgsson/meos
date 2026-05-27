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

// oRunner.cpp: implementation of the oRunner class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "oRunner.h"

#include "oEvent.h"
#include "gdioutput.h"
#include "gdifonts.h"
#include "Table.h"
#include "meos_util.h"
#include <cassert>
#include "localizer.h"
#include "SportIdent.h"
#include <cmath>
#include "intkeymapimpl.hpp"
#include "RunnerDB.h"
#include "meosexception.h"
#include <algorithm>
#include "socket.h"
#include "MeOSFeatures.h"
#include "oListInfo.h"
#include "qualification_final.h"
#include "metalist.h"
#include "cardsystem.h"
#include "datadefiners.h"
#include "xmlparser.h"
#include <unordered_map>

char RunnerStatusOrderMap[100];

bool oAbstractRunner::DynamicValue::isOld(const oEvent &oe, int key) const {
  return oe.dataRevision != dataRevision || forKey != key;
}

bool oAbstractRunner::DynamicValue::isOld(const oEvent& oe) const {
  return oe.dataRevision != dataRevision;
}

oAbstractRunner::DynamicValue &oAbstractRunner::DynamicValue::update(const oEvent &oe, int key, int v, bool preferStd) {
  if (preferStd) {
    valueStd = v; // A temporary result for "default" when computing with result modules (internal calculation)
    forKey = 0;
  }
  else {
    value = v;
    forKey = key;
    dataRevision = oe.dataRevision;
  }
  return *this;
}

int oAbstractRunner::DynamicValue::get(bool preferStd) const {
  if (preferStd && valueStd >= 0)
    return valueStd;

  return value;
}

void oAbstractRunner::DynamicValue::reset() {
  value = -1;
  valueStd = -1;
  dataRevision = -1;
}

const wstring &oAbstractRunner::encodeStatus(RunnerStatus st, bool allowError) {
   wstring &res = StringCache::getInstance().wget();
   switch (st) {
   case StatusOK:
     res = L"OK";
     break;
   case StatusUnknown:
     res = L"UN";
     break;
   case StatusDNS: 
     res = L"NS";
     break;
   case StatusCANCEL: 
     res = L"CC";
     break;
   case StatusOutOfCompetition:
     res = L"OC";
     break;
   case StatusNoTiming:
     res = L"NT";
     break;
   case StatusMP:
     res = L"MP";
     break;
   case StatusDNF:
     res = L"NF";
     break;
   case StatusDQ:
     res = L"DQ";
     break;
   case StatusMAX:
     res = L"MX";
     break;
   case StatusNotCompeting:
     res = L"NC";
     break;
   default:
     if (allowError)
       res = L"ERROR";
     else
      throw std::runtime_error("Unknown status");
   }

   return res;
}

RunnerStatus oAbstractRunner::decodeStatus(const wstring &stat) {
  wstring ustat = stat;
  for (wchar_t &t : ustat) {
    t = toupper(t);
  }
  for (RunnerStatus st : getAllRunnerStatus())
    if (encodeStatus(st) == stat)
      return st;

  return StatusUnknown;
}

const wstring &oRunner::RaceIdFormatter::formatData(const oBase *ob, int index) const {
  return itow(dynamic_cast<const oRunner &>(*ob).getRaceIdentifier());
}

pair<int, bool> oRunner::RaceIdFormatter::setData(oBase *ob, int index, const wstring &input, wstring &output, int inputId) const {
  int rid = wtoi(input.c_str());
  if (input == L"0")
    ob->getDI().setInt("RaceId", 0);
  else if (rid>0 && rid != dynamic_cast<oRunner *>(ob)->getRaceIdentifier())
    ob->getDI().setInt("RaceId", rid);
  output = formatData(ob, index);
  return make_pair(0, false);
}

TableColSpec oRunner::RaceIdFormatter::addTableColumn(Table *table, const string &description, int minWidth) const {
  return table->addColumn(description, max(minWidth, 90), true, true);
}

const wstring &oRunner::RunnerReference::formatData(const oBase *obj, int index) const {
  int id = obj->getDCI().getInt("Reference");
  if (id > 0) {
    pRunner r = obj->getEvent()->getRunner(id, 0);
    if (r)
      return r->getUIName();
    else {
      return lang.tl("Okänd");
    }
  }
  return _EmptyWString;
 }


pair<int, bool> oRunner::RunnerReference::setData(oBase *obj, int index, const wstring &input, wstring &output, int inputId) const {
  int oldRef = obj->getDCI().getInt("Reference"); 
  obj->getDI().setInt("Reference", inputId);
  bool clearAll = false;
  if (inputId != oldRef) {
    if (oldRef != 0) {
      pRunner oldRefR = obj->getEvent()->getRunner(oldRef, 0);
      if (oldRefR) {
        oldRefR->setReference(0);
        clearAll = true;
      }
    }

    if (inputId != 0) {
      pRunner newRefR = obj->getEvent()->getRunner(inputId, 0);
      if (newRefR)
        newRefR->setReference(obj->getId());
    }
  }

  output = formatData(obj, index);
  return make_pair(inputId, clearAll);
}

void oRunner::RunnerReference::fillInput(const oBase *obj, int index, vector<pair<wstring, size_t>> &out, size_t &selected) const {
  const oRunner *r = static_cast<const oRunner *>(obj);
  int cls = r->getClassId(true);
  vector<pRunner> runners;
  r->oe->getRunners(cls, 0, runners, true);
  int id = obj->getDCI().getInt("Reference");
  selected = id;
  out.reserve(runners.size() + 2);
  out.emplace_back(lang.tl("Ingen"), 0);
  for (auto rr : runners) {
    if (rr->Id == id)
      id = 0;
    if (rr->Id == r->Id)
      continue; // No self reference

    out.emplace_back(rr->getUIName(), rr->Id);
  }

  if (id != 0) {
    pRunner rr = obj->getEvent()->getRunner(id, 0);
    if (rr)
      out.emplace_back(rr->getUIName(), id);
    else 
      out.emplace_back(lang.tl("Okänd"), id);
  }
}

TableColSpec oRunner::RunnerReference::addTableColumn(Table *table, const string &description, int minWidth) const {
  return table->addColumn(description, max(minWidth, 200), true, true);
}

CellType oRunner::RunnerReference::getCellType(int index) const {
  return CellType::cellSelection; 
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
oAbstractRunner::oAbstractRunner(oEvent* poe, bool loading) :oBase(poe) {
  Class = nullptr;
  Club = nullptr;
  startTime = 0;
  tStartTime = 0;

  FinishTime = 0;
  tStatus = status = StatusUnknown;
  tEntryTouched = true;
  sqlChanged = true;
  StartNo = 0;
  inputPoints = 0;
  if (loading || !oe->hasPrevStage())
    inputStatus = StatusOK;
  else
    inputStatus = StatusNotCompeting;

  inputTime = 0;
  inputPlace = 0;

  tTimeAdjustment = 0;
  tPointAdjustment = 0;
  tAdjustDataRevision = -1;
}

wstring oAbstractRunner::getInfo() const {
  return getName();
}

void oAbstractRunner::setFinishTimeS(const wstring &t) {
  setFinishTime(oe->getRelativeTime(t));
}

void oAbstractRunner::setStartTimeS(const wstring &t) {
  setStartTime(oe->getRelativeTime(t), true, ChangeType::Update);
}

oRunner::oRunner(oEvent *poe) :oAbstractRunner(poe, false) {
  isTemporaryObject = false;
  tTimeAfter = 0;
  tInitialTimeAfter = 0;
  tempRT = 0;
  tempStatus = StatusUnknown;
  Id = oe->getFreeRunnerId();
  Course = nullptr;
  StartNo = 0;
  cardNumber = 0;

  tInTeam = nullptr;
  tLeg = 0;
  tLegEquClass = 0;
  tNeedNoCard = false;
  tUseStartPunch = true;
  getDI().initData();
  correctionNeeded = false;

  tDuplicateLeg = 0;
  tParentRunner = nullptr;

  Card = nullptr;
  
  tCachedRunningTime = 0;
  tSplitRevision = -1;

  tRogainingPoints = 0;
  tRogainingOvertime = 0;
  tReduction = 0;
  tRogainingPointsGross = 0;
  tAdaptedCourse = 0;
  tAdaptedCourseRevision = -1;

  tShortenDataRevision = -1;
  tNumShortening = 0;
}

oRunner::oRunner(oEvent *poe, int id) :oAbstractRunner(poe, true) {
  isTemporaryObject = false;
  Id = id;
  oe->qFreeRunnerId = max(id, oe->qFreeRunnerId);
  Course = nullptr;
  StartNo = 0;
  cardNumber = 0;

  tInTeam = nullptr;
  tLeg = 0;
  tLegEquClass = 0;
  tNeedNoCard = false;
  tUseStartPunch = true;
  getDI().initData();
  correctionNeeded = false;

  tDuplicateLeg = 0;
  tParentRunner = nullptr;

  Card = nullptr;
  tCachedRunningTime = 0;
  tSplitRevision = -1;

  tRogainingPoints = 0;
  tRogainingOvertime = 0;
  tReduction = 0;
  tRogainingPointsGross = 0;
  tAdaptedCourse = 0;
  tAdaptedCourseRevision = -1;
}

oRunner::~oRunner() {
  if (tInTeam){
    for(unsigned i=0;i<tInTeam->Runners.size(); i++)
      if (tInTeam->Runners[i] && tInTeam->Runners[i]->getId() == Id)
        tInTeam->Runners[i] = nullptr;

    tInTeam=0;
  }

  for (size_t k=0;k<multiRunner.size(); k++) {
    if (multiRunner[k] && multiRunner[k]->tParentRunner == this)
      multiRunner[k]->tParentRunner = nullptr;
  }

  if (tParentRunner) {
    for (size_t k=0;k<tParentRunner->multiRunner.size(); k++)
      if (tParentRunner->multiRunner[k] == this)
        tParentRunner->multiRunner[k] = nullptr;
  }

  delete tAdaptedCourse;
  tAdaptedCourse = nullptr;
}

bool oRunner::Write(xmlparser &xml)
{
  if (Removed) return true;
  
  xml.startTag("Runner");
  xml.write("Id", Id);
  xml.write("Updated", getStamp());
  xml.write("Name", sName);
  xml.writeTime("Start", startTime);
  xml.writeTime("Finish", FinishTime);
  xml.write("Status", status);
  xml.write("CardNo", cardNumber);
  xml.write("StartNo", StartNo);

  xml.write("InputPoint", inputPoints);
  if (inputStatus != StatusOK)
    xml.write("InputStatus", itos(inputStatus)); //Force write of 0
  xml.writeTime("InputTime", inputTime);
  xml.write("InputPlace", inputPlace);

  if (Club) xml.write("Club", Club->Id);
  if (Class) xml.write("Class", Class->Id);
  if (Course) xml.write("Course", Course->Id);

  if (multiRunner.size()>0)
    xml.write("MultiR", codeMultiR());

  if (Card) {
    assert(Card->tOwner==this);
    Card->Write(xml);
  }
  getDI().write(xml);

  xml.endTag();

  for (size_t k=0;k<multiRunner.size();k++)
    if (multiRunner[k])
      multiRunner[k]->Write(xml);

  return true;
}

void oRunner::Set(const xmlobject &xo)
{
  xmlList xl;
  xo.getObjects(xl);
  xmlList::const_iterator it;

  for (it = xl.begin(); it != xl.end(); ++it) {
    if (it->is("Id")) {
      Id = it->getInt();
    }
    else if (it->is("Name")) {
      sName = it->getWStr();
      getRealName(sName, tRealName);
    }
    else if (it->is("Start")) {
      tStartTime = startTime = it->getRelativeTime();
    }
    else if (it->is("Finish")) {
      FinishTime = it->getRelativeTime();
    }
    else if (it->is("Status")) {
      unsigned rawStat = it->getInt();
      tStatus = status = RunnerStatus(rawStat < 100u ? rawStat : 0);
    }
    else if (it->is("CardNo")) {
      cardNumber = it->getInt();
    }
    else if (it->is("StartNo") || it->is("OrderId"))
      StartNo = it->getInt();
    else if (it->is("Club"))
      Club = oe->getClub(it->getInt());
    else if (it->is("Class"))
      Class = oe->getClass(it->getInt());
    else if (it->is("Course"))
      Course = oe->getCourse(it->getInt());
    else if (it->is("Card")) {
      Card = oe->allocateCard(this);
      Card->Set(*it);
      assert(Card->getId() != 0);
    }
    else if (it->is("oData"))
      getDI().set(*it);
    else if (it->is("Updated"))
      Modified.setStamp(it->getRawStr());
    else if (it->is("MultiR"))
      decodeMultiR(it->getRawStr());
    else if (it->is("InputTime")) {
      inputTime = it->getRelativeTime();
    }
    else if (it->is("InputStatus")) {
      unsigned rawStat = it->getInt();
      inputStatus = RunnerStatus(rawStat < 100u ? rawStat : 0);
    }
    else if (it->is("InputPoint")) {
      inputPoints = it->getInt();
    }
    else if (it->is("InputPlace")) {
      inputPlace = it->getInt();
    }
  }
}

int oAbstractRunner::getBirthAge() const {
  return 0;
}

int oRunner::getBirthAge() const {
  int y = getBirthYear();
  if (y > 0)
    return getThisYear() - y;
  return 0;
}

int oAbstractRunner::getDefaultFee() const {
  int age = getBirthAge();
  wstring date = getEntryDate();
  if (Class) {
    int fee = Class->getEntryFee(date, age);
    return fee;
  }
  return 0;
}

int oAbstractRunner::getEntryFee() const {
  return getDCI().getInt("Fee");
}

void oAbstractRunner::addClassDefaultFee(bool resetFees) {
  if (Class) {
    oDataInterface di = getDI();

    if (isVacant()) {
      di.setInt("Fee", 0);
      di.setInt("EntryDate", 0);
      di.setInt("EntryTime", 0);
      di.setInt("Paid", 0);
      if (typeid(*this)==typeid(oRunner))
        di.setInt("CardFee", 0);
      return;
    }
    wstring date = getEntryDate();
    int currentFee = di.getInt("Fee");

    pTeam t = getTeam();
    if (t && t != this) {
      // Thus us a runner in a team
      // Check if the team has a fee.
      // Don't assign personal fee if so.
      if (t->getDCI().getInt("Fee") > 0)
        return;
    }

    if ((currentFee == 0 && !hasFlag(FlagFeeSpecified)) || resetFees) {
      int fee = getDefaultFee();
      di.setInt("Fee", fee);
    }
  }
}

// Get entry date of runner (or its team)
wstring oRunner::getEntryDate(bool useTeamEntryDate) const {
  if (useTeamEntryDate && tInTeam) {
    wstring date = tInTeam->getEntryDate(false);
    if (!date.empty())
      return date;
  }
  oDataConstInterface dci = getDCI();
  int date = dci.getInt("EntryDate");
  if (date == 0) {
    auto di = (const_cast<oRunner *>(this)->getDI());
    di.setDate("EntryDate", getLocalDate());
    di.setInt("EntryTime", convertAbsoluteTimeHMS(getLocalTimeOnly(), -1));

  }
  return dci.getDate("EntryDate");
}

string oRunner::codeMultiR() const
{
  char bf[32];
  string r;

  for (size_t k=0;k<multiRunner.size() && multiRunner[k];k++) {
    if (!r.empty())
      r+=":";
    snprintf(bf, sizeof(bf), "%d", multiRunner[k]->getId());
    r+=bf;
  }
  return r;
}

void oRunner::decodeMultiR(const string &r)
{
  vector<string> sv;
  split(r, ":", sv);
  multiRunnerId.clear();

  for (size_t k=0;k<sv.size();k++) {
    int d = atoi(sv[k].c_str());
    if (d>0)
      multiRunnerId.push_back(d);
  }
  multiRunnerId.push_back(0); // Mark as containing something
}

void oAbstractRunner::setClassId(int id, bool isManualUpdate) {
  pClass pc = Class;
  Class = id ? oe->getClass(id) : nullptr;

  if (Class!=pc) {
    apply(ChangeType::Update, nullptr);
    if (Class) {
      Class->clearCache(true);
    }
    if (pc) {
      pc->clearCache(true);
      if (isManualUpdate) {
        setFlag(FlagUpdateClass, true);
        // Update heat data
        int heat = pc->getDCI().getInt("Heat");
        if (heat != 0)
          getDI().setInt("Heat", heat);
      }
    }
    updateChanged();
  }
}

// Update all classes (for multirunner)
void oRunner::setClassId(int id, bool isManualUpdate) {
  pClass nPc = id>0 ? oe->getClass(id) : 0;
  if (Class == nPc)
    return;
  oe->classIdToRunnerHash.reset();

  if (Class && Class->getQualificationFinal() && isManualUpdate && nPc && nPc->parentClass == Class) {
    int heat = Class->getQualificationFinal()->getHeatFromClass(id, Class->getId());
    if (heat >= 0) {
      int oldHeat = getDI().getInt("Heat");

      if (heat != oldHeat) {
        pClass oldHeatClass = getClassRef(true);
        getDI().setInt("Heat", heat);
        pClass newHeatClass = getClassRef(true);
        oldHeatClass->clearCache(true);
        newHeatClass->clearCache(true);
        tSplitRevision = 0;
        apply(ChangeType::Quiet, nullptr);
      }
    }
    return;
  }

  if (nPc && isManualUpdate && nPc->isQualificationFinalBaseClass() && nPc != Class) {
    int h = getDI().getInt("Heat"); // Clear heat if not a base class
    if (h != 0) {
      set<int> base;
      nPc->getQualificationFinal()->getBaseClassInstances(base);
      if (!base.count(h))
        getDI().setInt("Heat", 0);
    }
  }


  if (tParentRunner) { 
    assert(!isManualUpdate); // Do not support! This may be destroyed if calling tParentRunner->setClass
    return;
  }
  else {
    pClass pc = Class;
  
    if (pc && pc->isSingleRunnerMultiStage() && nPc!=pc && tInTeam) {
      if (!isTemporaryObject) {
        oe->autoRemoveTeam(this);

        if (nPc) {
          int newNR = max(nPc->getNumMultiRunners(0), 1);
          for (size_t k = newNR - 1; k<multiRunner.size(); k++) {
            if (multiRunner[k]) {
              assert(multiRunner[k]->tParentRunner == this);
              multiRunner[k]->tParentRunner = 0;
              vector<int> toRemove;
              toRemove.push_back(multiRunner[k]->Id);
              oe->removeRunner(toRemove);
            }
          }
          multiRunner.resize(newNR-1);
        }
      }
    }

    Class = nPc;

    if (Class != 0 && Class != pc && tInTeam==0 &&
                      Class->isSingleRunnerMultiStage()) {
      if (!isTemporaryObject) {
        pTeam t = oe->addTeam(getName(), getClubId(), getClassId(false));
        t->setStartNo(StartNo, ChangeType::Update);
        t->setRunner(0, this, true);
      }
    }

    apply(ChangeType::Quiet, nullptr); //We may get old class back from team.

    for (size_t k=0;k<multiRunner.size();k++) {
      if (multiRunner[k] && Class!=multiRunner[k]->Class) {
        multiRunner[k]->Class=Class;
        multiRunner[k]->updateChanged();
      }
    }

    if (Class!=pc && !isTemporaryObject) {
      if (Class) {
        Class->clearCache(true);
      }
      if (pc) {
        pc->clearCache(true);
      }
      tSplitRevision = 0;
      updateChanged();
      if (isManualUpdate && pc) {
        setFlag(FlagUpdateClass, true);
        // Update heat data
        int heat = pc->getDCI().getInt("Heat");
        if (heat != 0)
          getDI().setInt("Heat", heat);

      }
    }
  }
}

void oRunner::setCourseId(int id) {
  pCourse pc = Course;

  if (id > 0)
    Course = oe->getCourse(id);
  else
    Course = nullptr;

  if (Course != pc) {
    updateChanged();
    if (Class)
      getClassRef(true)->clearSplitAnalysis();
    tSplitRevision = 0;
  }
}

bool oAbstractRunner::setStartTime(int t, bool updateSource, ChangeType changeType, bool recalculate) {

  int tOST=tStartTime;
  if (t>0)
    tStartTime=t;
  else tStartTime=0;

  if (updateSource) {
    int OST=startTime;
    startTime = tStartTime;

    if (OST!=startTime) {
      updateChanged(changeType);
    }
  }

  if (tOST != tStartTime) {
    changedObject();
    if (Class) {
      Class->clearCache(false);
    }
  }

  if (tOST<tStartTime && Class && recalculate)
    oe->reCalculateLeaderTimes(Class->getId());

  return tOST != tStartTime;
}

void oAbstractRunner::setFinishTime(int t)
{
  int OFT=FinishTime;

  if (t>tStartTime)
    FinishTime=t;
  else //Beeb
    FinishTime=0;

  if (OFT != FinishTime) {
    updateChanged();
    if (Class) {
      Class->clearCache(false);
    }
  }

  if (OFT>FinishTime && Class)
    oe->reCalculateLeaderTimes(Class->getId());
}

void oRunner::setFinishTime(int t)
{
  bool update=false;
  if (Class && (getTimeAfter(tDuplicateLeg, false)==0 || getTimeAfter()==0))
    update=true;

  oAbstractRunner::setFinishTime(t);

  tSplitRevision = 0;

  if (update && t!=FinishTime)
    oe->reCalculateLeaderTimes(Class->getId());
}

const wstring &oAbstractRunner::getStartTimeS() const {
  if (tStartTime>0)
    return oe->getAbsTime(tStartTime);
  else if (Class && Class->hasFreeStart())
    return _EmptyWString;
  else  
    return makeDash(L"-");
}

const wstring &oAbstractRunner::getStartTimeCompact() const {
  if (tStartTime>0) {
    if (oe->useStartSeconds())
      return oe->getAbsTime(tStartTime);
    else
      return oe->getAbsTimeHM(tStartTime);
  }
  else if (Class && Class->hasFreeStart())
    return _EmptyWString;
  else 
    return makeDash(L"-");
}

const wstring &oAbstractRunner::getFinishTimeS(bool adjusted, SubSecond mode) const
{
  if (FinishTime > 0) {
    if (adjusted)
      return oe->getAbsTime(FinishTime, mode);
    else
      return oe->getAbsTime(FinishTime - getBuiltinAdjustment(), mode);
  }
  else return makeDash(L"-");
}

int oAbstractRunner::getRunningTime(bool computedTime) const {
  if (!computedTime || tComputedTime == 0) {
    int rt = FinishTime - tStartTime;
    if (rt > 0)
      return getTimeAdjustment(false) + rt;
    else
      return 0;
  }
  else
    return tComputedTime;
}

const wstring &oAbstractRunner::getRunningTimeS(bool computedTime, SubSecond mode) const
{
  return formatTime(getRunningTime(computedTime), mode);
}

const wstring &oAbstractRunner::getTotalRunningTimeS(SubSecond mode) const
{
  return formatTime(getTotalRunningTime(), mode);
}

int oAbstractRunner::getTotalRunningTime() const {
  int t = getRunningTime(true);
  if (t > 0 && inputTime>=0)
    return t + inputTime;
  else
    return 0;
}

int oRunner::getTotalRunningTime() const {
  return getTotalRunningTime(getFinishTime(), true, true);
}

const wstring &oAbstractRunner::getStatusS(bool formatForPrint, bool computedStatus) const
{
  if (computedStatus)
    return oEvent::formatStatus(getStatusComputed(true), formatForPrint);
  else
    return oEvent::formatStatus(tStatus, formatForPrint);
}

const wstring &oAbstractRunner::getTotalStatusS(bool formatForPrint) const
{
  auto ts = getTotalStatus();  
  return oEvent::formatStatus(ts, formatForPrint);
}

/*
 - Inactive		: Has not yet started
   - DidNotStart	: Did Not Start (in this race)
   - Active		: Currently on course
   - Finished		: Finished but not validated
   - OK			: Finished and validated
   - MisPunch		: Missing Punch
   - DidNotFinish	: Did Not Finish
   - Disqualified	: Disqualified
   - NotCompeting	: Not Competing (running outside the competition)
   - SportWithdr	: Sporting Withdrawal (e.g. helping injured)
   - OverTime 	: Overtime, i.e. did not finish within max time
   - Moved		: Moved to another class
   - MovedUp		: Moved to a "better" class, in case of entry
            restrictions
   - Cancelled
*/
const wchar_t *formatIOFStatus(RunnerStatus s, bool hasTime) {
  switch(s) {
  case StatusNoTiming:
    if (!hasTime)
      break;
  case StatusOK:
    return L"OK";
  case StatusDNS:
    return L"DidNotStart";
  case StatusCANCEL:
    return L"Cancelled";
  case StatusMP:
    return L"MisPunch";
  case StatusDNF:
    return L"DidNotFinish";
  case StatusDQ:
    return L"Disqualified";
  case StatusMAX:
    return L"OverTime";
  case StatusOutOfCompetition:
    if (!hasTime)
      break;
  case StatusNotCompeting:
    return L"NotCompeting";
  }
  return L"Inactive";
}

wstring oAbstractRunner::getIOFStatusS() const
{
  return formatIOFStatus(getStatusComputed(true), getFinishTime()> 0);
}

wstring oAbstractRunner::getIOFTotalStatusS() const
{
  return formatIOFStatus(getTotalStatus(), getFinishTime()> 0);
}

void oRunner::addCard(pCard card, vector<pair<int, pControl>> &missingPunches) {
  RunnerStatus oldStatus = getStatus();
  int oldFinishTime = getFinishTime();
  pCard oldCard = Card;

  if (Card && card != Card) {
    Card->tOwner = nullptr;
  }

  Card = card;
  card->adaptTimes(getStartTime());
  updateChanged();

  if (card) {
    if (card->cardNo > 0)
      setCardNo(card->cardNo, false, true);
    assert(card->tOwner==0 || card->tOwner==this);
  }
  // Auto-select shortening
  pCourse mainCourse = getCourse(false);
  int shortenLevel = 0;

  if (mainCourse && Card) {
    pCourse shortVersion = mainCourse->getShorterVersion().second;
    if (shortVersion) {
      //int s = mainCourse->getStartPunchType();
      //int f = mainCourse->getFinishPunchType();
      const int numCtrl = Card->getNumControlPunches(-1,-1);
      int numCtrlLong = mainCourse->getNumControls();
      int numCtrlShort = shortVersion->getNumControls();

      SICard sic(ConvertedTimeStatus::Unknown);
      Card->getSICard(sic);
      while (mainCourse->distance(sic) < 0 && abs(numCtrl-numCtrlShort) < abs(numCtrl-numCtrlLong)) {
        shortenLevel++;
        if (shortVersion->distance(sic) >= 0) {
          setNumShortening(shortenLevel); // We passed at some level
          break;
        }
        mainCourse = shortVersion;
        shortVersion = mainCourse->getShorterVersion().second;
        numCtrlLong = numCtrlShort;
        if (!shortVersion) {
          break;
        }
        numCtrlShort = shortVersion->getNumControls();
      }
    }
  }
  if (mainCourse && mainCourse->getCommonControl() != 0 && mainCourse->getShorterVersion().first) {
    oCourse tmpCourse(oe);
    int numShorten;
    mainCourse->getAdapetedCourse(*Card, tmpCourse, numShorten);
    setNumShortening(shortenLevel + numShorten);
  }


  if (Card)
    Card->tOwner=this;

  evaluateCard(true, missingPunches, 0, ChangeType::Update);

  cardWasSet = true;
  synchronizeAll(true);
  
  if (Card != card) {
    Card = card;
    updateChanged();
    evaluateCard(true, missingPunches, 0, ChangeType::Update);
    
    cardWasSet = true;
    synchronizeAll(true);
  }

  if (oe->isClient() && oe->getPropertyInt("UseDirectSocket", true)!=0) {
    if (oldStatus != getStatus() || oldFinishTime != getFinishTime()) {
      SocketPunchInfo pi;
      pi.runnerId = getId();
      pi.time = getFinishTime();
      pi.status = getStatus();
      pi.iHashType = oPunch::PunchFinish;
      oe->getDirectSocket().sendPunch(pi);
    }
  }

  oe->pushDirectChange();
  if (oldCard && Card && oldCard != Card && oldCard->isConstructedFromPunches())
    oldCard->remove(); // Remove card constructed from punches
}

pCourse oRunner::getCourse(bool useAdaptedCourse) const {
  pCourse tCrs = 0;
  if (Course)
    tCrs = Course;
  else if (Class) {
    const oClass *cls = getClassRef(true);

    if (cls->hasMultiCourse()) {
      if (tInTeam) {
        if (size_t(tLeg) >= tInTeam->Runners.size() || tInTeam->Runners[tLeg] != this) {
          tInTeam->quickApply();
        }
      }
      
      if (Class == cls) {
        if (tInTeam && Class->hasUnorderedLegs()) {
          vector< pair<int, pCourse> > group;
          Class->getParallelCourseGroup(tLeg, StartNo, group);

          if (group.size() == 1) {
            tCrs = group[0].second;
          }
          else {
            // Remove used courses
            int myStart = 0;

            for (size_t k = 0; k < group.size(); k++) {
              if (group[k].first == tLeg)
                myStart = k;

              pRunner tr = tInTeam->getRunner(group[k].first);
              if (tr && tr->Course) {
                // The course is assigned. Remove from group
                for (size_t j = 0; j < group.size(); j++) {
                  if (group[j].second == tr->Course) {
                    group[j].second = 0;
                    break;
                  }
                }
              }
            }

            // Clear out already preliminary assigned courses 
            for (int k = 0; k < myStart; k++) {
              pRunner r = tInTeam->getRunner(group[k].first);
              if (r && !r->Course) {
                size_t j = k;
                while (j < group.size()) {
                  if (group[j].second) {
                    group[j].second = 0;
                    break;
                  }
                  else j++;
                }
              }
            }

            for (size_t j = 0; j < group.size(); j++) {
              int ix = (j + myStart) % group.size();
              pCourse gcrs = group[ix].second;
              if (gcrs) {
                tCrs = gcrs;
                break;
              }
            }
          }
        }
        else if (tInTeam) {
          unsigned leg = legToRun();
          tCrs = Class->getCourse(leg, StartNo);
        }
        else {
          if (unsigned(tDuplicateLeg) < Class->MultiCourse.size()) {
            vector<pCourse> &courses = Class->MultiCourse[tDuplicateLeg];
            if (courses.size() > 0) {
              int index = StartNo % courses.size();
              tCrs = courses[index];
            }
          }
        }
      }
      else {
        // Final / qualification classes
        tCrs = cls->getCourse(0, StartNo);
      }
    }
    else
      tCrs = cls->Course;
  }

  if (tCrs && useAdaptedCourse) {
    // Find shortened version of course
    int ns = getNumShortening();
    pCourse shortCrs = tCrs;
    while (ns > 0 && shortCrs) {
      shortCrs = shortCrs->getShorterVersion().second;
      if (shortCrs)
        tCrs = shortCrs;
      ns--;
    }
  }

  if (tCrs && useAdaptedCourse && Card && tCrs->getCommonControl() != 0) {
    if (tAdaptedCourse && tAdaptedCourseRevision == oe->dataRevision) {
      return tAdaptedCourse;
    }
    if (!tAdaptedCourse)
      tAdaptedCourse = new oCourse(oe, -1);

    int numShorten;
    tCrs = tCrs->getAdapetedCourse(*Card, *tAdaptedCourse, numShorten);
    tAdaptedCourseRevision = oe->dataRevision;
    return tCrs;
  }

  return tCrs;
}

const wstring &oRunner::getCourseName() const
{
  pCourse oc=getCourse(false);
  if (oc) return oc->getName();
  return makeDash(L"-");
}

#define NOTATIME 0xF0000000
/*void oAbstractRunner::resetTmpStore() {
  tmpStore.startTime = startTime;
  tmpStore.status = status;
  tmpStore.startNo = StartNo;
  tmpStore.bib = getBib();
}
*/
/*
bool oAbstractRunner::setTmpStore() {
  bool res = false;
  setStartNo(tmpStore.startNo, false);
  res |= setStartTime(tmpStore.startTime, false, false, false);
  res |= setStatus(tmpStore.status, false, false, false);
  setBib(tmpStore.bib, 0, false, false);
  return res;
}*/

bool oRunner::evaluateCard(bool doApply, vector<pair<int, pControl>>& missingPunches,
  int addpunch, ChangeType changeType) {
  if (unsigned(status) >= 100u)
    status = StatusUnknown; //Reset bad input
  pClass clz = getClassRef(true);
  missingPunches.clear();
  const int oldFT = FinishTime;
  int oldStartTime;
  RunnerStatus oldStatus;
  int* refStartTime;
  RunnerStatus* refStatus;

  if (doApply) {
    oldStartTime = tStartTime;
    tStartTime = startTime;
    oldStatus = tStatus;
    tStatus = status;
    refStartTime = &tStartTime;
    refStatus = &tStatus;

    apply(changeType, nullptr);
  }
  else {
    // tmp initialized from outside. Do not change tStatus, tStartTime. Work with tmpStore instead!
    oldStartTime = tStartTime;
    oldStatus = tStatus;
    refStartTime = &tStartTime;
    refStatus = &tStatus;

    createMultiRunner(false, changeType == ChangeType::Update);
  }

  // Reset card data
  oPunchList::iterator p_it;
  if (Card) {
    for (p_it = Card->punches.begin(); p_it != Card->punches.end(); ++p_it) {
      p_it->tRogainingIndex = -1;
      p_it->anyRogainingMatchControlId = -1;
      p_it->tRogainingPoints = 0;
      p_it->isUsed = false;
      p_it->tIndex = -1;
      p_it->tMatchControlId = -1;
      p_it->clearTimeAdjust();
    }
  }

  bool inTeam = tInTeam != 0;
  tProblemDescription.clear();
  tReduction = 0;
  tRogainingPointsGross = 0;
  tRogainingOvertime = 0;

  vector<SplitData> oldTimes;
  swap(splitTimes, oldTimes);

  if (!Card) {
    if ((inTeam || !tUseStartPunch) && doApply)
      apply(changeType, nullptr); //Post apply. Update start times.

    if (storeTimes() && clz && changeType == ChangeType::Update) {
      oe->reEvaluateAll({ clz->getId() }, true);
    }
    normalizedSplitTimes.clear();
    if (oldTimes.size() > 0 && Class)
      clz->clearSplitAnalysis();
    return false;
  }
  //Try to match class?!
  if (!clz)
    return false;

  if (clz->ignoreStartPunch() && tStartTime > 0)
    tUseStartPunch = false;

  const pCourse course = getCourse(true);

  if (!course) {
    // Reset rogaining. Store start/finish
    for (p_it = Card->punches.begin(); p_it != Card->punches.end(); ++p_it) {
      if (p_it->isStart() && tUseStartPunch)
        *refStartTime = p_it->getTimeInt();
      else if (p_it->isFinish()) {
        setFinishTime(p_it->getTimeInt());
      }
    }
    if ((inTeam || !tUseStartPunch) && doApply)
      apply(changeType, nullptr); //Post apply. Update start times.

    storeTimes();
    // No course mode
    int maxTimeStatus = 0;
    if (getFinishTime() <= 0)
      *refStatus = StatusDNF;
    else {
      if (clz) {
        int mt = clz->getMaximumRunnerTime();
        if (mt > 0) {
          if (getRunningTime(false) > mt)
            maxTimeStatus = 1;
          else
            maxTimeStatus = 2;
        }
        else
          maxTimeStatus = 2;
      }

      if (*refStatus == StatusMAX && maxTimeStatus == 2)
        *refStatus = StatusUnknown;
    }
    if (payBeforeResult(false))
      *refStatus = StatusDQ;
    else if (*refStatus == StatusUnknown || *refStatus == StatusCANCEL || *refStatus == StatusDNS || *refStatus == StatusMAX) {
      if (maxTimeStatus == 1)
        *refStatus = StatusMAX;
      else
        *refStatus = StatusOK;
    }

    return false;
  }

  int startPunchCode = course->getStartPunchType();
  int finishPunchCode = course->getFinishPunchType();

  bool hasRogaining = course->hasRogaining();

  // Pairs: <control index, point>
  intkeymap<pair<int, int>> rogaining(course->getNumControls());
  unordered_set<int> requiredRG;
  for (int k = 0; k < course->nControls(); k++) {
    if (course->controls[k] && course->controls[k]->isRogaining(hasRogaining)) {
      int pt = course->controls[k]->getRogainingPoints();
      for (int j = 0; j < course->controls[k]->nNumbers; j++) {
        rogaining.insert(course->controls[k]->Numbers[j], make_pair(k, pt));
      }
      if (course->controls[k]->getStatus() == oControl::ControlStatus::StatusRogainingRequired)
        requiredRG.insert(k);
    }
  }

  if (addpunch && Card->punches.empty()) {
    Card->addPunch(addpunch, -1, course->controls[0] ? course->controls[0]->getId() : 0, 0, oCard::PunchOrigin::Manual);
  }

  if (Card->punches.empty()) {
    for (int k = 0; k < course->nControls(); k++) {
      if (course->controls[k]) {
        course->controls[k]->startCheckControl();
        course->controls[k]->addUncheckedPunches(missingPunches, hasRogaining);
      }
    }
    if ((inTeam || !tUseStartPunch) && doApply)
      apply(changeType, nullptr); //Post apply. Update start times.

    if (storeTimes() && clz && changeType == ChangeType::Update) {
      oe->reEvaluateAll({ clz->getId() }, true);
    }

    normalizedSplitTimes.clear();
    if (oldTimes.size() > 0 && clz)
      clz->clearSplitAnalysis();
    tRogainingPoints = max(0, getPointAdjustment());
    return false;
  }

  // Reset rogaining
  for (p_it = Card->punches.begin(); p_it != Card->punches.end(); ++p_it) {
    p_it->tRogainingIndex = -1;
    p_it->anyRogainingMatchControlId = -1;
    p_it->tRogainingPoints = 0;
  }

  bool clearSplitAnalysis = false;


  //Search for start and update start time.
  p_it = Card->punches.begin();
  while (p_it != Card->punches.end()) {
    if (p_it->type == startPunchCode) {
      if (tUseStartPunch && p_it->getAdjustedTime() != *refStartTime) {
        p_it->clearTimeAdjust();
        *refStartTime = p_it->getAdjustedTime();
        if (*refStartTime != oldStartTime)
          clearSplitAnalysis = true;
        //updateChanged();
      }
      break;
    }
    ++p_it;
  }

  //inthashmap expectedPunchCount(course->nControls);
  //inthashmap punchCount(Card->punches.size());
  map<int, int> punchCount;
  map<int, int> expectedPunchCount;
  map<int, int> controlToBase; // For multiple controls. Maps control code to base (or -1 for invalid/inconsistent)
  
  auto addBaseControl = [&controlToBase](int code, int base) {
    auto res = controlToBase.emplace(code, base);
    if (res.second)
      return base;
    else {
      if (base != code && base != res.first->second)
        res.first->second = -1; // Mark as invalid; control code used in multiple situations
      return res.first->second;
    }
  };

  auto getBaseControl = [&controlToBase](int code) {
    auto res = controlToBase.find(code);
    if (res != controlToBase.end())
      return res->second;
    else
      return -1;
  };

  for (int k = 0; k < course->nControls(); k++) {
    pControl ctrl = course->controls[k];
    if (ctrl && !ctrl->isRogaining(hasRogaining)) {
      if (ctrl->Status == oControl::ControlStatus::StatusMultiple) {
        for (int j = 0; j < ctrl->nNumbers; j++)
          ++expectedPunchCount[addBaseControl(ctrl->Numbers[j], ctrl->Numbers[j])];
      }
      else {
        constexpr int LargeCode = 1000000;
        int bc = LargeCode;
        for (int j = 0; j < ctrl->nNumbers; j++) // Use primary control code as base
          bc = min(bc, addBaseControl(ctrl->Numbers[j], ctrl->Numbers[0]));

        if (bc > 0 && bc < LargeCode)
          ++expectedPunchCount[bc];
      }
    }
  }

  for (p_it = Card->punches.begin(); p_it != Card->punches.end(); ++p_it) {
    if (p_it->type >= 10 && p_it->type <= 1024) {
      int baseCode = getBaseControl(p_it->type);
      if (baseCode > 0)
        ++punchCount[baseCode];
    }
  }

  p_it = Card->punches.begin();
  splitTimes.resize(course->nControls(), SplitData(NOTATIME, SplitData::SplitStatus::Missing));
  int k = 0;


  for (k = 0; k < course->nControls(); k++) {
    //Skip start finish check
    while (p_it != Card->punches.end() &&
      (p_it->isCheck() || p_it->isFinish() || p_it->isStart())) {
      p_it->clearTimeAdjust();
      ++p_it;
    }

    if (p_it == Card->punches.end())
      break;

    oPunchList::iterator tp_it = p_it;
    pControl ctrl = course->controls[k];
    int skippedPunches = 0;

    if (ctrl) {
      const int timeAdjustCtrl = ctrl->getTimeAdjust();
      ctrl->startCheckControl();

      // Add rogaining punches
      if (addpunch && ctrl->isRogaining(hasRogaining) && ctrl->getFirstNumber() == addpunch) {
        if (Card->getPunchByType(addpunch) == 0) {
          oPunch op(oe);
          op.type = addpunch;
          op.punchTime = -1;
          op.isUsed = true;
          op.tIndex = k;
          op.tMatchControlId = ctrl->getId();
          Card->punches.insert(tp_it, op);
          Card->updateChanged();
        }
      }

      if (ctrl->getStatus() == oControl::ControlStatus::StatusBad ||
        ctrl->getStatus() == oControl::ControlStatus::StatusOptional ||
        ctrl->getStatus() == oControl::ControlStatus::StatusBadNoTiming) {
        // The control is marked "bad" but we found it anyway in the card. Mark it as used.
        if (tp_it != Card->punches.end() && ctrl->hasNumberUnchecked(tp_it->type)) {
          tp_it->isUsed = true; //Show that this is used when splittimes are calculated.
                            // Adjust if the time of this control was incorrectly set.
          tp_it->setTimeAdjust(timeAdjustCtrl);
          tp_it->tMatchControlId = ctrl->getId();
          tp_it->tIndex = k;
          splitTimes[k].setPunchTime(tp_it->getAdjustedTime());
          ++tp_it;
          p_it = tp_it;
        }
      }
      else {
        while (!ctrl->controlCompleted(hasRogaining) && tp_it != Card->punches.end()) {
          if (ctrl->hasNumberUnchecked(tp_it->type)) {

            if (skippedPunches > 0) {
              if (ctrl->Status == oControl::ControlStatus::StatusOK) {
                // Avoid consuming forward for missing controls that occurres several times
                int code = tp_it->type;
                int baseCode = getBaseControl(tp_it->type);
                if (baseCode != -1 && expectedPunchCount[baseCode] > 1 && punchCount[baseCode] < expectedPunchCount[baseCode]) {
                  tp_it = Card->punches.end();
                  ctrl->uncheckNumber(code);
                  break;
                }
              }
            }
            tp_it->isUsed = true; //Show that this is used when splittimes are calculated.
            // Adjust if the time of this control was incorrectly set.
            tp_it->setTimeAdjust(timeAdjustCtrl);
            tp_it->tMatchControlId = ctrl->getId();
            tp_it->tIndex = k;
            if (ctrl->controlCompleted(hasRogaining))
              splitTimes[k].setPunchTime(tp_it->getAdjustedTime());
            ++tp_it;
            p_it = tp_it;
          }
          else {
            if (ctrl->hasNumberUnchecked(addpunch)) {
              //Add this punch.
              oPunch op(oe);
              op.type = addpunch;
              op.punchTime = -1;
              op.isUsed = true;
              op.origin = -1;

              op.tMatchControlId = ctrl->getId();
              op.tIndex = k;
              Card->punches.insert(tp_it, op);
              Card->updateChanged();
              if (ctrl->controlCompleted(hasRogaining))
                splitTimes[k].setPunched();
            }
            else {
              skippedPunches++;
              tp_it->isUsed = false;
              ++tp_it;
            }
          }
        }
      }

      if (tp_it == Card->punches.end() && !ctrl->controlCompleted(hasRogaining)
        && ctrl->hasNumberUnchecked(addpunch)) {
        Card->addPunch(addpunch, -1, ctrl->getId(), 0, oCard::PunchOrigin::Manual);
        if (ctrl->controlCompleted(hasRogaining))
          splitTimes[k].setPunched();
        Card->punches.back().isUsed = true;
        Card->punches.back().tMatchControlId = ctrl->getId();
        Card->punches.back().tIndex = k;
      }

      if (ctrl->controlCompleted(hasRogaining) && splitTimes[k].getTime(false) == NOTATIME)
        splitTimes[k].setPunched();
    }
    else //if (ctrl && ctrl->Status==oControl::StatusBad){
      splitTimes[k].setNotPunched();

    //Add missing punches
    if (ctrl && !ctrl->controlCompleted(hasRogaining))
      ctrl->addUncheckedPunches(missingPunches, hasRogaining);
  }

  //Add missing punches for remaining controls
  while (k < course->nControls()) {
    if (course->controls[k]) {
      pControl ctrl = course->controls[k];
      ctrl->startCheckControl();

      if (ctrl->hasNumberUnchecked(addpunch)) {
        Card->addPunch(addpunch, -1, ctrl->getId(), 0, oCard::PunchOrigin::Manual);
        Card->updateChanged();
        if (ctrl->controlCompleted(hasRogaining))
          splitTimes[k].setNotPunched();
      }
      ctrl->addUncheckedPunches(missingPunches, hasRogaining);
    }
    k++;
  }

  //Set the rest (if exist -- probably not) to "not used"
  while (p_it != Card->punches.end()) {
    p_it->isUsed = false;
    p_it->tIndex = -1;
    p_it->clearTimeAdjust();
    ++p_it;
  }

  int OK = missingPunches.empty();

  tRogaining.clear();
  tRogainingPoints = 0;
  int time_limit = 0;

  // Rogaining logic
  if (rogaining.size() > 0) {

    // Check this later
    time_limit = course->getMaximumRogainingTime();
    bool countAllControls = course->getDCI().getInt("NoLatePoints") == 0;

    unordered_set<int> visitedControls;
    for (p_it = Card->punches.begin(); p_it != Card->punches.end(); ++p_it) {
      pair<int, int> pt;
      if (rogaining.lookup(p_it->type, pt)) {
        p_it->anyRogainingMatchControlId = course->controls[pt.first]->getId();
        int timeAdjustCtrl = course->controls[pt.first]->getTimeAdjust();
        p_it->setTimeAdjust(timeAdjustCtrl);        
        if (visitedControls.insert(pt.first).second) {
          requiredRG.erase(pt.first);
          // May noy be revisited
          p_it->isUsed = true;
          p_it->tRogainingIndex = pt.first;
          p_it->tMatchControlId = p_it->anyRogainingMatchControlId;
          p_it->tRogainingPoints = pt.second;
          tRogaining.push_back(make_pair(course->controls[pt.first], p_it->getAdjustedTime()));
          splitTimes[pt.first].setPunchTime(p_it->getAdjustedTime());

          int rtHere = p_it->getAdjustedTime() - *refStartTime;
          if (countAllControls || rtHere <= 0 || time_limit <= 0 || rtHere <= time_limit)
            tRogainingPoints += pt.second;
        }
      }
    }

    for (int mp : requiredRG) {
      missingPunches.emplace_back(course->controls[mp]->getFirstNumber(), course->controls[mp]);
    }

    OK = missingPunches.empty();

    // Manual point adjustment
    tRogainingPoints = max(0, tRogainingPoints + getPointAdjustment());

    int point_limit = course->getMinimumRogainingPoints();
    if (point_limit > 0 && tRogainingPoints < point_limit) {
      tProblemDescription = L"X poäng fattas.#" + itow(point_limit - tRogainingPoints);
      OK = false;
    }

    for (int k = 0; k < course->nControls(); k++) {
      if (course->controls[k] && course->controls[k]->isRogaining(hasRogaining)) {
        if (!visitedControls.count(k))
          splitTimes[k].setNotPunched();
      }
    }
  }

  int maxTimeStatus = 0;
  if (clz && FinishTime > 0) {
    int mt = clz->getMaximumRunnerTime();
    if (mt > 0) {
      if (getRunningTime(false) > mt)
        maxTimeStatus = 1;
      else
        maxTimeStatus = 2;
    }
    else
      maxTimeStatus = 2;
  }

  if ((*refStatus == StatusMAX && maxTimeStatus == 2) ||
    *refStatus == StatusOutOfCompetition ||
    *refStatus == StatusNoTiming)
    *refStatus = StatusUnknown;

  if (payBeforeResult(false))
    *refStatus = StatusDQ;
  else if (OK && (*refStatus == 0 || *refStatus == StatusDNS || *refStatus == StatusCANCEL || *refStatus == StatusMP || *refStatus == StatusOK || *refStatus == StatusDNF))
    *refStatus = StatusOK;
  else	*refStatus = RunnerStatus(max(int(StatusMP), int(*refStatus)));

  oPunchList::reverse_iterator backIter = Card->punches.rbegin();

  if (finishPunchCode != oPunch::PunchFinish) {
    while (backIter != Card->punches.rend()) {
      if (backIter->type == finishPunchCode)
        break;
      ++backIter;
    }
  }

  if (backIter != Card->punches.rend() && backIter->type == finishPunchCode) {
    FinishTime = backIter->getTimeInt();
    if (finishPunchCode == oPunch::PunchFinish)
      backIter->tMatchControlId = oPunch::PunchFinish;
  }
  else if (FinishTime <= 0) {
    *refStatus = RunnerStatus(max(int(StatusDNF), int(tStatus)));
    tProblemDescription = L"Måltid saknas.";
    FinishTime = 0;
  }

  if (*refStatus == StatusOK && maxTimeStatus == 1)
    *refStatus = StatusMAX; //Maxtime

  if (!missingPunches.empty()) {
    tProblemDescription = L"Stämplingar saknas: X#" + itow(missingPunches[0].first);
    for (unsigned j = 1; j < 3; j++) {
      if (missingPunches.size() > j)
        tProblemDescription += L", " + itow(missingPunches[j].first);
    }
    if (missingPunches.size() > 3)
      tProblemDescription += L"...";
    else
      tProblemDescription += L".";
  }

  if (*refStatus == StatusOK) {
    if (hasFlag(TransferFlags::FlagOutsideCompetition))
      *refStatus = StatusOutOfCompetition;
    else if (hasFlag(TransferFlags::FlagNoTiming))
      *refStatus = StatusNoTiming;
    else if (clz && clz->getNoTiming())
      *refStatus = StatusNoTiming;
  }
  // Adjust times on course, including finish time
  doAdjustTimes(course);

  tRogainingPointsGross = tRogainingPoints;

  if (oldStatus != *refStatus || oldFT != FinishTime) {
    clearSplitAnalysis = true;
  }

  if (oldFT != FinishTime)
    updateChanged(changeType);

  if ((inTeam || !tUseStartPunch) && doApply)
    apply(changeType, nullptr); //Post apply. Update start times.

  if (tCachedRunningTime != FinishTime - *refStartTime) {
    tCachedRunningTime = FinishTime - *refStartTime;
    clearSplitAnalysis = true;
  }

  if (time_limit > 0) {
    int rt = getRunningTime(false);
    if (rt > 0) {
      int overTime = rt - time_limit;
      if (overTime > 0) {
        tRogainingOvertime = overTime;
        tReduction = course->calculateReduction(overTime);
        tProblemDescription = L"Tidsavdrag: X poäng.#" + itow(tReduction);
        tRogainingPoints = max(0, tRogainingPoints - tReduction);
      }
    }
  }

  // Clear split analysis data if necessary
  bool clear = splitTimes.size() != oldTimes.size() || clearSplitAnalysis;
  for (size_t k = 0; !clear && k < oldTimes.size(); k++) {
    if (splitTimes[k].time != oldTimes[k].time)
      clear = true;
  }

  if (clear) {
    normalizedSplitTimes.clear();
    if (clz)
      clz->clearSplitAnalysis();
  }

  if (doApply)
    storeTimes();
  if (clz && changeType == ChangeType::Update) {
    bool update = false;
    if (tInTeam) {
      int t1 = clz->getTotalLegLeaderTime(oClass::AllowRecompute::No, tLeg, false, false);
      int t2 = tInTeam->getLegRunningTime(tLeg, false, false);
      if (t2 <= t1 && t2 > 0)
        update = true;

      int t3 = clz->getTotalLegLeaderTime(oClass::AllowRecompute::No, tLeg, false, true);
      int t4 = tInTeam->getLegRunningTime(tLeg, false, true);
      if (t4 <= t3 && t4 > 0)
        update = true;
    }

    if (!update) {
      int t1 = clz->getBestLegTime(oClass::AllowRecompute::No, tLeg, false);
      int t2 = getRunningTime(false);
      if (t2 <= t1 && t2 > 0)
        update = true;
    }
    if (update) {
      oe->reEvaluateAll({ clz->getId() }, true);
    }
  }
  return true;
}

void oRunner::clearOnChangedRunningTime() {
  if (tCachedRunningTime != FinishTime - tStartTime) {
    tCachedRunningTime = FinishTime - tStartTime;
    normalizedSplitTimes.clear();
    if (Class)
      getClassRef(true)->clearSplitAnalysis();
  }
}

void oRunner::doAdjustTimes(pCourse course) {
  if (!Card)
    return;

  assert(course->nControls() == splitTimes.size());
  int adjustment = 0;
  oPunchList::iterator it = Card->punches.begin();

  adjustTimes.resize(splitTimes.size());
  for (int n = 0; n < course->nControls(); n++) {
    pControl ctrl = course->controls[n];
    if (!ctrl)
      continue;

    pControl ctrlPrev = n > 0 ? course->controls[n - 1] : nullptr;

    while (it != Card->punches.end() && !it->isUsed) {
      it->adjustTimeAdjust(adjustment);
      ++it;
    }

    int minTime = ctrl->getMinTime();
    int pN = n -1;
    
    while (pN >= 0 && (course->controls[pN]->getStatus() == oControl::ControlStatus::StatusBad ||
                       course->controls[pN]->getStatus() == oControl::ControlStatus::StatusBadNoTiming)) {
      pN--; // Skip bad controls
    }

    if (ctrl->getStatus() == oControl::ControlStatus::StatusNoTiming || (ctrlPrev && ctrlPrev->getStatus() == oControl::ControlStatus::StatusBadNoTiming)) {
      int t = 0;
      if (n>0 && pN>=0 && splitTimes[n].time>0 && splitTimes[pN].time>0) {
        t = splitTimes[n].time + adjustment - splitTimes[pN].time;
      }
      else if (pN < 0 && splitTimes[n].time>0) {
        t = splitTimes[n].time - tStartTime;
      }
      adjustment -= t;
    }
    else if (minTime > 0) {
      int t = 0;
      if (n > 0 && pN >= 0 && splitTimes[n].time > 0 && splitTimes[pN].time > 0) {
        t = splitTimes[n].time + adjustment - splitTimes[pN].time;
      }
      else if (pN < 0 && splitTimes[n].time>0) {
        t = splitTimes[n].time - tStartTime;
      }
      int maxadjust = max(minTime - t, 0);
      adjustment += maxadjust;
    }

    if (it != Card->punches.end() && it->tMatchControlId == ctrl->getId()) {
      it->adjustTimeAdjust(adjustment);
      ++it;
    }

    adjustTimes[n] = adjustment;
    splitTimes[n].setAdjustment(adjustment);
  }

  // Adjust remaining
  while (it != Card->punches.end()) {
    it->adjustTimeAdjust(adjustment);
    ++it;
  }

  FinishTime += adjustment;
}

bool oRunner::storeTimes() {
  bool updated = storeTimesAux(Class);
  if (tInTeam && tInTeam->Class && tInTeam->Class != Class)
    updated |= storeTimesAux(tInTeam->Class);
  else if (Class && Class->getQualificationFinal()) {
    updated |= storeTimesAux(getClassRef(true));
  }
  return updated;
}

bool oRunner::storeTimesAux(pClass targetClass) {
  if (!targetClass)
    return false;
  if (tInTeam) {
    if (tInTeam->getNumShortening(tLeg) > 0)
      return false;
  }
  else {
    if (getNumShortening() > 0)
      return false;
  }
  bool updated = false;
  //Store best time in class
  if (tInTeam && tInTeam->Class == targetClass) {
    if (targetClass && unsigned(tLeg)<targetClass->tLeaderTime.size()) {
      // Update for extra/optional legs
      int firstLeg = tLeg;
      int lastLeg = tLeg + 1;
      while(firstLeg>0 && targetClass->legInfo[firstLeg].isOptional())
        firstLeg--;
      int nleg = targetClass->legInfo.size();
      while(lastLeg<nleg && targetClass->legInfo[lastLeg].isOptional())
        lastLeg++;

      for (int leg = firstLeg; leg<lastLeg; leg++) {
        if (tStatus==StatusOK) {
          //int &bt=targetClass->tLeaderTime[leg].bestTimeOnLeg;
          int rt=getRunningTime(false);
          if (targetClass->tLeaderTime[leg].update(rt, oClass::LeaderInfo::Type::Leg))
            updated = true;
          /*if (rt > 0 && (bt == 0 || rt < bt)) {
            bt=rt;
            updated = true;
          }*/
        }

        if (getStatusComputed(false) == StatusOK) {
          int rt = getRunningTime(true);
          if (targetClass->tLeaderTime[leg].updateComputed(rt, oClass::LeaderInfo::Type::Leg))
            updated = true;
        }
      }

      bool updateTotal = true;
      bool updateTotalInput = true;
      bool updateTotalC = true;
      bool updateTotalInputC = true;

      int basePLeg = firstLeg;
      while (basePLeg > 0 && targetClass->legInfo[basePLeg].isParallel())
        basePLeg--;

      int ix = basePLeg;
      while (ix < nleg && (ix == basePLeg || targetClass->legInfo[ix].isParallel()) ) {
        updateTotal = updateTotal && tInTeam->getLegStatus(ix, false, false)==StatusOK;
        updateTotalInput = updateTotalInput && tInTeam->getLegStatus(ix, false, true)==StatusOK;

        updateTotalC = updateTotalC && tInTeam->getLegStatus(ix, true, false) == StatusOK;
        updateTotalInputC = updateTotalInputC && tInTeam->getLegStatus(ix, true, true) == StatusOK;
        ix++;
      }

      if (updateTotal) {
        int rt = 0;
        int ix = basePLeg;
        while (ix < nleg && (ix == basePLeg || targetClass->legInfo[ix].isParallel()) ) {
          rt = max(rt, tInTeam->getLegRunningTime(ix, false, false));
          ix++;
        }

        for (int leg = firstLeg; leg<lastLeg; leg++) {
          /*int &bt=targetClass->tLeaderTime[leg].totalLeaderTime;
          if (rt > 0 && (bt == 0 || rt < bt)) {
            bt=rt;
            updated = true;
          }*/
          if (targetClass->tLeaderTime[leg].update(rt, oClass::LeaderInfo::Type::Total))
            updated = true;
        }
      }
      if (updateTotalC) {
        int rt = 0;
        int ix = basePLeg;
        while (ix < nleg && (ix == basePLeg || targetClass->legInfo[ix].isParallel())) {
          rt = max(rt, tInTeam->getLegRunningTime(ix, true, false));
          ix++;
        }
        for (int leg = firstLeg; leg<lastLeg; leg++) {
          if (targetClass->tLeaderTime[leg].updateComputed(rt, oClass::LeaderInfo::Type::Total))
            updated = true;
        }
      }
      if (updateTotalInput) {
        //int rt=tInTeam->getLegRunningTime(tLeg, true);
        int rt = 0;
        int ix = basePLeg;
        while (ix < nleg && (ix == basePLeg || targetClass->legInfo[ix].isParallel()) ) {
          rt = max(rt, tInTeam->getLegRunningTime(ix, false, true));
          ix++;
        }
        for (int leg = firstLeg; leg<lastLeg; leg++) {
          /*int &bt=targetClass->tLeaderTime[leg].totalLeaderTimeInput;
          if (rt > 0 && (bt <= 0 || rt < bt)) {
            bt=rt;
            updated = true;
          }*/
          if (targetClass->tLeaderTime[leg].update(rt, oClass::LeaderInfo::Type::TotalInput))
            updated = true;
        }
      }
      if (updateTotalInputC) {
        int rt = 0;
        int ix = basePLeg;
        while (ix < nleg && (ix == basePLeg || targetClass->legInfo[ix].isParallel())) {
          rt = max(rt, tInTeam->getLegRunningTime(ix, true, true));
          ix++;
        }
        for (int leg = firstLeg; leg<lastLeg; leg++) {
          if (targetClass->tLeaderTime[leg].updateComputed(rt, oClass::LeaderInfo::Type::TotalInput))
            updated = true;
        }
      }
    }
  }
  else {
    size_t dupLeg = targetClass->mapLeg(tDuplicateLeg);
    if (targetClass && dupLeg < targetClass->tLeaderTime.size()) {
      if (tStatus == StatusOK) {
        int rt = getRunningTime(false);
        if (targetClass->tLeaderTime[dupLeg].update(rt, oClass::LeaderInfo::Type::Leg))
          updated = true;
      }

      if (getStatusComputed(false) == StatusOK) {
        int rt = getRunningTime(true);
        if (targetClass->tLeaderTime[dupLeg].updateComputed(rt, oClass::LeaderInfo::Type::Leg))
          updated = true;
      }

      int rt = getRaceRunningTime(false, dupLeg, false);
      if (targetClass->tLeaderTime[dupLeg].update(rt, oClass::LeaderInfo::Type::Total))
        updated = true;

      rt = getRaceRunningTime(true, dupLeg, false);
      if (targetClass->tLeaderTime[dupLeg].updateComputed(rt, oClass::LeaderInfo::Type::Total))
        updated = true;

      if (getTotalStatus(false) == StatusOK) {
        rt = getTotalRunningTime(getFinishTime(), false, true);
        if (targetClass->tLeaderTime[dupLeg].update(rt, oClass::LeaderInfo::Type::TotalInput))
          updated = true;

        rt = getTotalRunningTime(getFinishTime(), true, true);
        if (targetClass->tLeaderTime[dupLeg].updateComputed(rt, oClass::LeaderInfo::Type::TotalInput))
          updated = true;
      }

      /*int &bt = targetClass->tLeaderTime[dupLeg].totalLeaderTime;
      if (rt > 0 && (bt <= 0 || rt < bt)) {
        bt = rt;
        updated = true;
        targetClass->tLeaderTime[dupLeg].totalLeaderTimeInput = rt;
      }*/
    }
  }

  size_t mappedLeg = targetClass->mapLeg(tLeg);
  // Best input time
  if (mappedLeg<targetClass->tLeaderTime.size()) {
    if (inputStatus == StatusOK) {
      if (targetClass->tLeaderTime[mappedLeg].update(inputTime, oClass::LeaderInfo::Type::Input)) {
        updated = true;
      }
    }
  }

  if (targetClass && tStatus==StatusOK) {
    int rt = getRunningTime(true);
    pCourse pCrs = getCourse(false);
    if (pCrs && rt > 0) {
      map<int, int>::iterator res = targetClass->tBestTimePerCourse.find(pCrs->getId());
      if (res == targetClass->tBestTimePerCourse.end()) {
        targetClass->tBestTimePerCourse[pCrs->getId()] = rt;
        updated = true;
      }
      else if (rt < res->second) {
        res->second = rt;
        updated = true;
      }
    }
  }
  return updated;
}

int oRunner::getRaceRunningTime(bool computedTime, int leg, bool allowUpdate) const {
  if (tParentRunner)
    return tParentRunner->getRaceRunningTime(computedTime, leg, allowUpdate);

  if (leg == -1)
    leg = multiRunner.size() - 1;

  if (leg == 0) { /// XXX This code is buggy
    if (getTotalStatus(allowUpdate) == StatusOK)
      return getRunningTime(computedTime) + inputTime;
    else return 0;
  }
  leg--;

  if (unsigned(leg) < multiRunner.size() && multiRunner[leg]) {
    if (Class) {
      pClass pc=Class;
      LegTypes lt=pc->getLegType(leg);
      pRunner r=multiRunner[leg];

      switch(lt) {
        case LTNormal:
          if (r->statusOK(computedTime, allowUpdate)) {
            int dt=leg>0 ? r->getRaceRunningTime(computedTime, leg, allowUpdate)+r->getRunningTime(computedTime):0;
            return max(r->getFinishTime()-tStartTime, dt); // ### Luckor, jaktstart???
          }
          else return 0;
        break;

        case LTSum:
          if (r->statusOK(computedTime, allowUpdate))
            return r->getRunningTime(computedTime)+getRaceRunningTime(computedTime, leg, allowUpdate);
          else return 0;

        default:
          return 0;
      }
    }
    else 
      return getRunningTime(computedTime);
  }
  return 0;
}

bool oRunner::sortSplit(const oRunner &a, const oRunner &b)
{
  int acid=a.getClassId(true);
  int bcid=b.getClassId(true);
  if (acid!=bcid)
    return acid<bcid;
  else if (a.tempStatus != b.tempStatus)
    return a.tempStatus<b.tempStatus;
  else {
    if (a.tempStatus==StatusOK) {
      if (a.tempRT!=b.tempRT)
        return a.tempRT<b.tempRT;
    }
    return a.tRealName < b.tRealName;
  }
}

bool oRunner::operator<(const oRunner &c) const {
  if (oe->CurrentSortOrder == ClubClassStartTime) {
    pClub cl = getClubRef();
    pClub ocl = c.getClubRef();
    if (cl != ocl) {
      int cres = compareClubs(cl, ocl);
      if (cres != 2)
        return cres != 0;
    }
  }

  const oClass * myClass = getClassRef(true);
  const oClass * cClass = c.getClassRef(true);
  if (!myClass || !cClass)
    return size_t(myClass) < size_t(cClass);
  else if (Class == cClass && Class->getClassStatus() != oClass::ClassStatus::Normal)
    return tRealName < c.tRealName;

  if (oe->CurrentSortOrder == ClassStartTime || oe->CurrentSortOrder == ClubClassStartTime) {
    if (myClass->Id != cClass->Id) {
      if (myClass->tSortIndex != cClass->tSortIndex)
        return myClass->tSortIndex < cClass->tSortIndex;
      else
        return myClass->Id < cClass->Id;
    }
    else if (tStartTime != c.tStartTime) {
      if (tStartTime <= 0 && c.tStartTime > 0)
        return false;
      else if (c.tStartTime <= 0 && tStartTime > 0)
        return true;
      else return tStartTime < c.tStartTime;
    }
    else {
      //if (StartNo != c.StartNo && !(getBib().empty() && c.getBib().empty()))
      //  return StartNo < c.StartNo;
      const wstring &b1 = getBib();
      const wstring &b2 = c.getBib();
      if (b1 != b2) {
        return compareBib(b1, b2);
      }
    }
  }
  if (oe->CurrentSortOrder == ClassStartTime) {
    if (myClass->Id != cClass->Id) {
      if (myClass->tSortIndex != cClass->tSortIndex)
        return myClass->tSortIndex < cClass->tSortIndex;
      else
        return myClass->Id < cClass->Id;
    }
    else if (tStartTime != c.tStartTime) {
      if (tStartTime <= 0 && c.tStartTime > 0)
        return false;
      else if (c.tStartTime <= 0 && tStartTime > 0)
        return true;
      else return tStartTime < c.tStartTime;
    }
    else {
      //if (StartNo != c.StartNo && !(getBib().empty() && c.getBib().empty()))
      //  return StartNo < c.StartNo;
      const wstring& b1 = getBib();
      const wstring& b2 = c.getBib();
      if (b1 != b2) {
        return compareBib(b1, b2);
      }
    }
  }
  else if (oe->CurrentSortOrder == ClassDefaultResult) {
    RunnerStatus stat = tStatus == StatusUnknown ? StatusOK : tStatus;
    RunnerStatus cstat = c.tStatus == StatusUnknown ? StatusOK : c.tStatus;

    if (myClass != cClass)
      return myClass->tSortIndex < cClass->tSortIndex || (myClass->tSortIndex == cClass->tSortIndex && myClass->Id < cClass->Id);
    else if (tLegEquClass != c.tLegEquClass)
      return tLegEquClass < c.tLegEquClass;
    else if (tDuplicateLeg != c.tDuplicateLeg)
      return tDuplicateLeg < c.tDuplicateLeg;
    else if (stat != cstat)
      return RunnerStatusOrderMap[stat] < RunnerStatusOrderMap[cstat];
    else {
      if (stat == StatusOK) {
        if (Class->getNoTiming()) {
          return tRealName < c.tRealName;
        }
        int s = getNumShortening();
        int cs = c.getNumShortening();
        if (s != cs)
          return s < cs;

        int t = getRunningTime(false);
        if (t <= 0)
          t = timeConstHour * 1000;
        int ct = c.getRunningTime(false);
        if (ct <= 0)
          ct = timeConstHour * 1000;

        if (t != ct)
          return t < ct;
      }
    }
  }
  else if (oe->CurrentSortOrder == ClassResult) {
    
    RunnerStatus stat = getStatusComputed(false);
    RunnerStatus cstat = c.getStatusComputed(false);

    stat = stat == StatusUnknown ? StatusOK : stat;
    cstat = cstat == StatusUnknown ? StatusOK : cstat;

    if (myClass != cClass)
      return myClass->tSortIndex < cClass->tSortIndex || (myClass->tSortIndex == cClass->tSortIndex && myClass->Id < cClass->Id);
    else if (tLegEquClass != c.tLegEquClass)
      return tLegEquClass < c.tLegEquClass;
    else if (tDuplicateLeg != c.tDuplicateLeg)
      return tDuplicateLeg < c.tDuplicateLeg;
    else if (stat != cstat)
      return RunnerStatusOrderMap[stat] < RunnerStatusOrderMap[cstat];
    else {
      if (stat == StatusOK) {
        if (Class->getNoTiming()) {
          return tRealName < c.tRealName;
        }
        int s = getNumShortening();
        int cs = c.getNumShortening();
        if (s != cs)
          return s < cs;

        int t = getRunningTime(true);
        if (t <= 0)
          t = timeConstHour * 1000;
        int ct = c.getRunningTime(true);
        if (ct <= 0)
          ct = timeConstHour * 1000;

        if (t != ct)
          return t < ct;
      }
    }
  }
  else if (oe->CurrentSortOrder == ClassCourseResult) {
    if (myClass != cClass)
      return myClass->tSortIndex < cClass->tSortIndex;

    const pCourse crs1 = getCourse(false);
    const pCourse crs2 = c.getCourse(false);
    RunnerStatus stat = getStatusComputed(false);
    RunnerStatus cstat = c.getStatusComputed(false);

    if (crs1 != crs2) {
      int id1 = crs1 ? crs1->getId() : 0;
      int id2 = crs2 ? crs2->getId() : 0;
      return id1 < id2;
    }
    else if (tDuplicateLeg != c.tDuplicateLeg)
      return tDuplicateLeg < c.tDuplicateLeg;
    else if (stat != cstat)
      return RunnerStatusOrderMap[stat] < RunnerStatusOrderMap[cstat];
    else {
      if (stat == StatusOK) {
        if (Class->getNoTiming()) {
          return tRealName < c.tRealName;
        }
        int s = getNumShortening();
        int cs = c.getNumShortening();
        if (s != cs)
          return s < cs;

        int t = getRunningTime(true);
        int ct = c.getRunningTime(true);
        if (t != ct)
          return t < ct;
      }
    }
  }
  else if (oe->CurrentSortOrder == SortByName) {
    return tRealName < c.tRealName;
  }
  else if (oe->CurrentSortOrder == SortByLastName) {
    wstring a = getFamilyName();
    wstring b = c.getFamilyName();
    if (a.empty() && !b.empty())
      return false;
    else if (b.empty() && !a.empty())
      return true;
    else if (a != b) {
      return a < b;
    }
    a = getGivenName();
    b = c.getGivenName();
    if (a != b) {
      return a < b;
    }
  }
  else if (oe->CurrentSortOrder == SortByFinishTime) {
    RunnerStatus stat = getStatusComputed(false);
    RunnerStatus cstat = c.getStatusComputed(false);

    if (stat != cstat)
      return RunnerStatusOrderMap[stat] < RunnerStatusOrderMap[cstat];
    else {
      int ft = getFinishTimeAdjusted(true);
      int cft = c.getFinishTimeAdjusted(true);
      if (stat == StatusOK && ft != cft)
        return ft < cft;
    }
  }
  else if (oe->CurrentSortOrder == SortByFinishTimeReverse) {
    int ft = getFinishTimeAdjusted(true);
    int cft = c.getFinishTimeAdjusted(true);
    if (ft != cft)
      return ft > cft;
  }
  else if (oe->CurrentSortOrder == ClassFinishTime) {
    if (myClass != cClass)
      return myClass->tSortIndex < cClass->tSortIndex || (myClass->tSortIndex == cClass->tSortIndex && myClass->Id < cClass->Id);

    RunnerStatus stat = getStatusComputed(false);
    RunnerStatus cstat = c.getStatusComputed(false);

    if (stat != cstat)
      return RunnerStatusOrderMap[stat] < RunnerStatusOrderMap[cstat];
    else {
      int ft = getFinishTimeAdjusted(true);
      int cft = c.getFinishTimeAdjusted(true);
      if (stat == StatusOK && ft != cft)
        return ft < cft;
    }
  }
  else if (oe->CurrentSortOrder == SortByStartTime) {
    if (tStartTime < c.tStartTime)
      return true;
    else  if (tStartTime > c.tStartTime)
      return false;

    const wstring &b1 = getBib();
    const wstring &b2 = c.getBib();
    if (b1 != b2) {
      return compareBib(b1, b2);
    }
  }
  else if (oe->CurrentSortOrder == SortByStartTimeClass) {
    if (tStartTime < c.tStartTime)
      return true;
    else  if (tStartTime > c.tStartTime)
      return false;

    if (myClass != cClass)
      return myClass->tSortIndex < cClass->tSortIndex || (myClass->tSortIndex == cClass->tSortIndex && myClass->Id < cClass->Id);
  }
  else if (oe->CurrentSortOrder == SortByEntryTime) {
    auto dci = getDCI(), cdci = c.getDCI();
    int ed = dci.getInt("EntryDate");
    int ced = cdci.getInt("EntryDate");
    if (ed != ced)
      return ed > ced;
    int et = dci.getInt("EntryTime");
    int cet = cdci.getInt("EntryTime");
    if (et != cet)
      return et > cet;
  }
  else if (oe->CurrentSortOrder == SortByBib) {
    const wstring &b = getBib();
    const wstring &bc = c.getBib();
    if (b != bc) {
      int bn = wtoi(b.c_str());
      int bcn = wtoi(bc.c_str());
      if (bn != 0 && bcn != 0 && bn != bcn)
        return bn < bcn;
      else
        return b < bc;
    }
    if (StartNo != c.StartNo)
      return StartNo < c.StartNo;
    else
      return Id < c.Id;
  }
  else if (oe->CurrentSortOrder == ClassPoints) {
    if (myClass != cClass)
      return myClass->tSortIndex < cClass->tSortIndex || (myClass->tSortIndex == cClass->tSortIndex && myClass->Id < cClass->Id);
    else if (tDuplicateLeg != c.tDuplicateLeg)
      return tDuplicateLeg < c.tDuplicateLeg;
    else if (tStatus != c.tStatus)
      return RunnerStatusOrderMap[tStatus] < RunnerStatusOrderMap[c.tStatus];
    else {
      if (tStatus == StatusOK) {
        int myP = getRogainingPoints(true, false);
        int otherP = c.getRogainingPoints(true, false);

        if (myP != otherP)
          return myP > otherP;
        int t = getRunningTime(true);
        int ct = c.getRunningTime(true);
        if (t != ct)
          return t < ct;
      }
    }
  }
  else if (oe->CurrentSortOrder == ClassTotalResult) {
    if (myClass != cClass)
      return myClass->tSortIndex < cClass->tSortIndex || (myClass->tSortIndex == cClass->tSortIndex && myClass->Id < cClass->Id);
    else if (tDuplicateLeg != c.tDuplicateLeg)
      return tDuplicateLeg < c.tDuplicateLeg;
    else {
      RunnerStatus s1, s2;
      s1 = getTotalStatus();
      s2 = c.getTotalStatus();
      if (s1 != s2)
        return s1 < s2;
      else if (s1 == StatusOK) {
        if (Class->getNoTiming()) {
          return tRealName < c.tRealName;
        }
        int t = getTotalRunningTime(FinishTime, true, true);
        int ct = c.getTotalRunningTime(c.FinishTime, true, true);
        if (t != ct)
          return t < ct;
      }
    }
  }
  else if (oe->CurrentSortOrder == CourseResult) {
    const pCourse crs1 = getCourse(false);
    const pCourse crs2 = c.getCourse(false);
    RunnerStatus stat = getStatusComputed(false);
    RunnerStatus cstat = c.getStatusComputed(false);

    if (crs1 != crs2) {
      int id1 = crs1 ? crs1->getId() : 0;
      int id2 = crs2 ? crs2->getId() : 0;
      return id1 < id2;
    }
    else if (stat != cstat)
      return RunnerStatusOrderMap[stat] < RunnerStatusOrderMap[cstat];
    else {
      if (stat == StatusOK) {

        int s = getNumShortening();
        int cs = c.getNumShortening();
        if (s != cs)
          return s < cs;

        int t = getRunningTime(true);
        int ct = c.getRunningTime(true);
        if (t != ct) {
          return t < ct;
        }
      }
    }
  }
  else if (oe->CurrentSortOrder == CourseStartTime) {
    const pCourse crs1 = getCourse(false);
    const pCourse crs2 = c.getCourse(false);
    if (crs1 != crs2) {
      int id1 = crs1 ? crs1->getId() : 0;
      int id2 = crs2 ? crs2->getId() : 0;
      return id1 < id2;
    }
    else if (tStartTime != c.tStartTime)
      return tStartTime < c.tStartTime;
  }
  else if (oe->CurrentSortOrder == ClassStartTimeClub) {
    if (myClass != cClass)
      return myClass->tSortIndex < cClass->tSortIndex || (myClass->tSortIndex == cClass->tSortIndex && myClass->Id < cClass->Id);
    else if (tStartTime != c.tStartTime) {
      if (tStartTime <= 0 && c.tStartTime > 0)
        return false;
      else if (c.tStartTime <= 0 && tStartTime > 0)
        return true;
      else return tStartTime < c.tStartTime;
    }
    else if (Club != c.Club) {
      int cres = compareClubs(Club, c.Club);
      if (cres != 2)
        return cres != 0;
    }
  }
  else if (oe->CurrentSortOrder == ClassTeamLeg) {
    if (myClass->Id != cClass->Id)
      return myClass->tSortIndex < cClass->tSortIndex || (myClass->tSortIndex == cClass->tSortIndex && myClass->Id < cClass->Id);
    else if (tInTeam != c.tInTeam) {
      if (tInTeam == 0)
        return true;
      else if (c.tInTeam == 0)
        return false;
      if (tInTeam->StartNo != c.tInTeam->StartNo)
        return tInTeam->StartNo < c.tInTeam->StartNo;
      else
        return tInTeam->sName < c.tInTeam->sName;
    }
    else if (tInTeam && tLeg != c.tLeg)
      return tLeg < c.tLeg;
    else if (tStartTime != c.tStartTime) {
      if (tStartTime <= 0 && c.tStartTime > 0)
        return false;
      else if (c.tStartTime <= 0 && tStartTime > 0)
        return true;
      else return tStartTime < c.tStartTime;
    }
    else {
      const wstring &b1 = getBib();
      const wstring &b2 = c.getBib();
      if (StartNo != c.StartNo && b1 != b2)
        return StartNo < c.StartNo;
    }
  }
  else if (oe->CurrentSortOrder == ClassLiveResult) {
    if (myClass->Id != cClass->Id)
      return myClass->tSortIndex < cClass->tSortIndex || (myClass->tSortIndex == cClass->tSortIndex && myClass->Id < cClass->Id);
    
    if (currentControlTime != c.currentControlTime)
      return currentControlTime < c.currentControlTime;
  }
  return tRealName < c.tRealName;

}

void oAbstractRunner::setClub(const wstring &clubName)
{
  pClub pc=Club;
  Club = clubName.empty() ? 0 : oe->getClubCreate(0, clubName);
  if (pc != Club) {
    updateChanged();
    if (Class) {
      // Vacant clubs have special logic
      getClassRef(true)->tResultInfo.clear();
    }
    if (Club && Club->isVacant()) { // Clear entry date/time for vacant
      getDI().setInt("EntryDate", 0);
      getDI().setInt("EntryTime", 0);
    }
  }
}




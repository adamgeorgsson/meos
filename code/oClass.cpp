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

oClass::oClass(oEvent *poe): oBase(poe)
{
  getDI().initData();
  Course=0;
  Id=oe->getFreeClassId();
  tLeaderTime.resize(1);
  tNoTiming = -1;
  tIgnoreStartPunch = -1;
  tLegTimeToPlace = 0;
  tLegAccTimeToPlace = 0;
  tSplitRevision = 0;
  tSortIndex = 0;
  tMaxTime = 0;
  tCoursesChanged = false;
  tStatusRevision = 0;
  tShowMultiDialog = false;
  parentClass = 0;
}

oClass::oClass(oEvent *poe, int id): oBase(poe)
{
  getDI().initData();
  Course=0;
  if (id == 0)
    id = oe->getFreeClassId();
  Id=id;
  oe->qFreeClassId = max(id % MaxClassId, oe->qFreeClassId);
  tLeaderTime.resize(1);
  tNoTiming = -1;
  tIgnoreStartPunch = -1;
  tLegTimeToPlace = 0;
  tLegAccTimeToPlace = 0;
  tSplitRevision = 0;
  tSortIndex = 0;
  tMaxTime = 0;
  tCoursesChanged = false;
  tStatusRevision = 0;
  tShowMultiDialog = false;

  parentClass = 0;
}

void oClass::clearDuplicate() {
  int id = oe->getFreeClassId();
  clearDuplicateBase(id);
  oe->qFreeClassId = max(id % MaxClassId, oe->qFreeClassId);
  getDI().setInt("SortIndex", tSortIndex);
}

oClass::~oClass()
{
  if (tLegTimeToPlace)
    delete tLegTimeToPlace;
  if (tLegAccTimeToPlace)
    delete tLegAccTimeToPlace;
}

bool oClass::Write(xmlparser &xml)
{
  if (Removed) return true;
  xml.startTag("Class");

  xml.write("Id", Id);
  xml.write("Updated", getStamp());
  xml.write("Name", Name);

  if (Course)
    xml.write("Course", Course->Id);

  if (MultiCourse.size()>0)
    xml.write("MultiCourse", codeMultiCourse());

  if (legInfo.size()>0)
    xml.write("LegMethod", codeLegMethod());

  getDI().write(xml);
  xml.endTag();

  return true;
}


void oClass::Set(const xmlobject *xo)
{
  xmlList xl;
  xo->getObjects(xl);

  xmlList::const_iterator it;

  for(it=xl.begin(); it != xl.end(); ++it){
    if (it->is("Id")){
      Id = it->getInt();
    }
    else if (it->is("Name")){
      Name = it->getWStr();
      if (Name.size() > 1 && Name.at(0) == '%') {
        Name = lang.tl(Name.substr(1));
      }
    }
    else if (it->is("Course")){
      Course = oe->getCourse(it->getInt());
    }
    else if (it->is("MultiCourse")){
      set<int> cid;
      vector< vector<int> > multi;
      parseCourses(it->getRawStr(), multi, cid);
      importCourses(multi);
    }
    else if (it->is("LegMethod")){
      importLegMethod(it->getRawStr());
    }
    else if (it->is("oData")){
      getDI().set(*it);
    }
    else if (it->is("Updated")){
      Modified.setStamp(it->getRawStr());
    }
  }

  // Reinit temporary data
  getNoTiming();
}

void oClass::importCourses(const vector< vector<int> > &multi)
{
  MultiCourse.resize(multi.size());

  for (size_t k=0;k<multi.size();k++) {
    MultiCourse[k].resize(multi[k].size());
    for (size_t j=0; j<multi[k].size(); j++) {
      MultiCourse[k][j] = oe->getCourse(multi[k][j]);
    }
  }
  setNumStages(MultiCourse.size());
}

set<int> &oClass::getMCourseIdSet(set<int> &in) const
{
  in.clear();
  for (size_t k=0;k<MultiCourse.size();k++) {
    for (size_t j=0; j<MultiCourse[k].size(); j++) {
      if (MultiCourse[k][j])
        in.insert(MultiCourse[k][j]->getId());
    }
  }
  return in;
}

string oClass::codeMultiCourse() const
{
  vector< vector<pCourse> >::const_iterator stage_it;
  string str;
  char bf[16];

  for (stage_it=MultiCourse.begin();stage_it!=MultiCourse.end(); ++stage_it) {
    vector<pCourse>::const_iterator it;
    for (it=stage_it->begin();it!=stage_it->end(); ++it) {
      if (*it){
        snprintf(bf, 16, " %d", (*it)->getId());
        str+=bf;
      }
      else str+=" 0";
    }
    str += ";";
  }

  if (str.length() == 1)
    return "@"; // Special code for the case of one stage and no course
  else if (str.length()>0) {
    return trim(str.substr(0, str.length()-1));
  }
  //if (str.length()>0)
 //   return trim(str);
  else return "";
}

void oClass::parseCourses(const string &courses,
                          vector< vector<int> > &multi,
                          set<int> &courseId)
{
  courseId.clear();
  multi.clear();
  if (courses.empty())
    return;

  const char *str=courses.c_str();

  vector<int> empty;
  multi.push_back(empty);
  int n_stage=0;

  while (*str && isspace(*str))
    str++;

  while (*str) {
    int cid=atoi(str);

    if (cid) {
      multi[n_stage].push_back(cid);
      courseId.insert(cid);
    }

    while (*str && (*str!=';' && *str!=' ')) str++;

    if (*str==';') {
      str++;
      while (*str && *str==' ') str++;
      n_stage++;
      multi.push_back(empty);
    }
    else {
      if (*str) str++;
    }
  }
}

string oLegInfo::codeLegMethod() const {
  char bsd[16], bret[16], brot[16];

  auto codeTime = [](int t, char *b) -> const char * {
    if (timeConstSecond == 1 || t <= 0)
      snprintf(b, 16, "%d", t);
    else
      snprintf(b, 16, "%d.%d", (t / timeConstSecond),(t % timeConstSecond));

    return b;
  };

  char bf[256];

  if (isStartDataTime()) {
    snprintf(bf, sizeof(bf), "(%s:%s:%s:%s:%s:%d)", StartTypeNames[startMethod],
              LegTypeNames[legMethod],
              codeTime(legStartData, bsd),
              codeTime(legRestartTime, bret),
              codeTime(legRopeTime, brot),
              duplicateRunner);
  }
  else {
    snprintf(bf, sizeof(bf), "(%s:%s:%d:%s:%s:%d)", StartTypeNames[startMethod],
              LegTypeNames[legMethod],
              legStartData, 
              codeTime(legRestartTime, bret),
              codeTime(legRopeTime, brot),
              duplicateRunner);
  }
  return bf;
}

void oLegInfo::importLegMethod(const string &leg) {
  //Defaults
  startMethod = STTime;
  legMethod = LTNormal;
  legStartData = 0;
  legRestartTime = 0;

  size_t begin = leg.find_first_of('(');

  if (begin == string::npos)
    return;
  begin++;

  string coreLeg = leg.substr(begin, leg.find_first_of(')') - begin);

  vector<string> legsplit;
  split(coreLeg, ":", legsplit);

  if (legsplit.size() >= 1) {
    for (int st = 0; st < nStartTypes; ++st) {
      if (legsplit[0] == StartTypeNames[st]) {
        startMethod = (StartTypes)st;
        break;
      }
    }
  }
  if (legsplit.size() >= 2) {
    for (int t = 0; t < nLegTypes; ++t) {
      if (legsplit[1] == LegTypeNames[t]) {
        legMethod = (LegTypes)t;
        break;
      }
    }
  }

  if (legsplit.size() >= 3) {
    if (isStartDataTime())
      legStartData = parseRelativeTime(legsplit[2].c_str());
    else
      legStartData = atoi(legsplit[2].c_str());
  }

  if (legsplit.size() >= 4)
    legRestartTime = parseRelativeTime(legsplit[3].c_str());

  if (legsplit.size() >= 5)
    legRopeTime = parseRelativeTime(legsplit[4].c_str());

  if (legsplit.size() >= 6)
    duplicateRunner = atoi(legsplit[5].c_str());
}

string oClass::codeLegMethod() const
{
  string code;
  for(size_t k=0;k<legInfo.size();k++) {
    if (k>0) code+="*";
    code+=legInfo[k].codeLegMethod();
  }
  return code;
}

wstring oClass::getInfo() const
{
  return L"Klass " + Name;
}

void oClass::importLegMethod(const string &legMethods)
{
  vector< string > legsplit;
  split(legMethods, "*", legsplit);

  legInfo.clear();
  for (size_t k=0;k<legsplit.size();k++) {
    oLegInfo oli;
    oli.importLegMethod(legsplit[k]);
    legInfo.push_back(oli);
  }

  // Ensure we got valid data
  for (size_t k=0;k<legsplit.size();k++) {
    if (legInfo[k].duplicateRunner!=-1) {
      if ( unsigned(legInfo[k].duplicateRunner)<legInfo.size() )
        legInfo[legInfo[k].duplicateRunner].duplicateRunner=-1;
      else
        legInfo[k].duplicateRunner=-1;
    }
  }
  setNumStages(legInfo.size());
  apply();
}

string oClass::getCountTypeKey(int leg, CountKeyType type, bool countVacant) {
  return itos(leg) + ":" + itos(type) + (countVacant ? "V" : "");
}

int oClass::getNumRunners(bool checkFirstLeg, bool noCountVacant, bool noCountNotCompeting) const {
  if (tTypeKeyToRunnerCount.first != oe->dataRevision) {
    for (auto &c : oe->Classes) {
      c.tTypeKeyToRunnerCount.second.clear();
      c.tTypeKeyToRunnerCount.first = oe->dataRevision;
    }
  }
  string key = getCountTypeKey(checkFirstLeg ? 0 : -1, 
                               noCountNotCompeting ? CountKeyType::AllCompeting : CountKeyType::IncludeNotCompeting,
                               !noCountVacant);

  auto res = tTypeKeyToRunnerCount.second.find(key);
  if (res != tTypeKeyToRunnerCount.second.end())
    return res->second;

  unordered_map<int, int> nRunners;
  for (auto &r : oe->Runners) {
    if (r.isRemoved() || !r.Class)
      continue;
    if (checkFirstLeg && (r.tLeg > 0 && !r.Class->isQualificationFinalBaseClass()))
      continue;
    if (noCountVacant && r.isVacant())
      continue;
    if (noCountNotCompeting && (r.getStatus() == StatusNotCompeting || r.getStatus() == StatusCANCEL))
      continue;

    int id = r.getClassId(true);
    ++nRunners[id];
  }
  
  for (auto &c : oe->Classes) {
    if (!c.isRemoved())
      c.tTypeKeyToRunnerCount.second[key] = nRunners[c.Id];
  }
  return nRunners[Id];
}

void oClass::getNumResults(int leg, int &total, int &finished, int &dns) const {
  if (tTypeKeyToRunnerCount.first != oe->dataRevision) {
    for (auto &c : oe->Classes) {
      c.tTypeKeyToRunnerCount.second.clear();
      c.tTypeKeyToRunnerCount.first = oe->dataRevision;
    }
  }
  string keyTot = getCountTypeKey(leg, CountKeyType::ExpectedStarting, false);
  string keyFinished = getCountTypeKey(leg, CountKeyType::Finished, false);
  string keyDNS = getCountTypeKey(leg, CountKeyType::DNS, false);

  auto rTot = tTypeKeyToRunnerCount.second.find(keyTot);
  auto rFinished = tTypeKeyToRunnerCount.second.find(keyFinished);
  auto rDNS = tTypeKeyToRunnerCount.second.find(keyDNS);

  if (rTot != tTypeKeyToRunnerCount.second.end() &&
      rFinished != tTypeKeyToRunnerCount.second.end() &&
      rDNS != tTypeKeyToRunnerCount.second.end()) {
    total = rTot->second;
    finished = rFinished->second;
    dns = rDNS->second;
    return;
  }

  struct Cnt {
    bool team = false;
    bool singleClass = false;
    int maxleg = 0;
    int total = 0;
    int finished = 0;
    int dns = 0;
  };

  //Search runners
  unordered_map<int, Cnt> cnt;

  for (auto &c : oe->Classes) {
    if (c.isRemoved())
      continue;

    ClassType ct = c.getClassType();
    auto &cc = cnt[c.Id];
    cc.maxleg = c.getLastStageIndex();
    if (ct == oClassKnockout || cc.maxleg == 0)
      cc.singleClass = true;

    if (!(ct == oClassIndividual || ct == oClassIndividRelay || ct == oClassKnockout))
      cnt[c.Id].team = true;
    else if (ct == oClassKnockout && c.isQualificationFinalBaseClass())
      cnt[c.Id].team = true; // Count teams in the base class
  }

  for (auto &r : oe->Runners) {
    if (r.isRemoved() || !r.Class || r.tStatus == StatusNotCompeting || r.tStatus == StatusCANCEL)
      continue;

    auto &c = cnt[r.getClassId(true)];
    if (c.team)
      continue;

    int tleg = leg >= 0 ? leg : c.maxleg;

    if (r.tLeg == tleg || c.singleClass) {
      c.total++;

      if (!r.isStatusUnknown(false, false) && r.tStatus != StatusDNS)
        c.finished++;
      
      if (r.tStatus == StatusDNS)
        c.dns++;
    }
  }

  for (auto &t : oe->Teams) {
    if (t.isRemoved() || !t.Class || t.tStatus == StatusNotCompeting || t.tStatus == StatusCANCEL)
      continue;

    auto &c = cnt[t.getClassId(true)];
    if (!c.team)
      continue;

    c.total++;

    if (t.tStatus != StatusUnknown || t.getLegStatus(leg, false, false) != StatusUnknown)
      c.finished++;
  }

  for (auto &c : oe->Classes) {
    auto &cc = cnt[c.Id];

    c.tTypeKeyToRunnerCount.second[keyDNS] = cc.dns;
    c.tTypeKeyToRunnerCount.second[keyFinished] = cc.finished;
    c.tTypeKeyToRunnerCount.second[keyTot] = cc.total;
  }
  auto &cc = cnt[Id];

  dns = cc.dns;
  total = cc.total;
  finished = cc.finished;
}

void oClass::setCourse(pCourse c)
{
  if (Course!=c){
    if (MultiCourse.size() == 1) {
      // MultiCourse wich is in fact only one course, (e.g. for fixed start time in class). Keep in synch.
      if (c != 0) {
        if (MultiCourse[0].size() == 1)
          MultiCourse[0][0] = c;
        else if (MultiCourse[0].size() == 0)
          MultiCourse[0].push_back(c);
      }
      else {
        if (MultiCourse[0].size() == 1)
          MultiCourse[0].pop_back();
      }
    }
    Course=c;
    tCoursesChanged = true;
    updateChanged();
    // Update start from course
    if (Course && !Course->getStart().empty()) {
      setStart(Course->getStart());
    }
  }
}

void oClass::setName(const wstring &name, bool manualSet) {
  if (getName() != name) {
    Name = name;
    if (manualSet)
      setFlag(TransferFlags::FlagManualName, true);
    updateChanged();
  }
}

const wstring& oClass::getLongName() const {
return getDCI().getString("LongName");
}

void oClass::setLongName(const wstring& name) {
getDI().setString("LongName", name);
}

oDataContainer &oClass::getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const {
  data = (pvoid)oData;
  olddata = (pvoid)oDataOld;
  strData = const_cast< vector <vector<wstring> >* >(&oDataStr);
  return *oe->oClassData;
}

int oEvent::getNumClasses() const {
  int nc = 0;
  for (auto& c : Classes) {
    if (!c.isRemoved())
      nc++;
  }
  return nc;
}

pClass oEvent::getClassCreate(int Id, const wstring& createName, set<wstring>& exactMatch) {
  if (Id > 0) {
    for (auto it = Classes.begin(); it != Classes.end(); ++it) {
      if (it->Id == Id && !it->isRemoved()) {
        if (compareClassName(createName, it->getName()) || compareClassName(createName, it->getLongName())) {
          if (it != Classes.begin())
            Classes.splice(Classes.begin(), Classes, it, Classes.end());
          return &Classes.front();
        }
        else {
          Id = 0; //Bad Id
          break;
        }
      }
    }
  }

  if (createName.empty() && Id > 0) {
    oClass c(this, Id);
    c.setName(getAutoClassName(), false);
    return addClass(c);
  }
  else {
    bool exact = exactMatch.count(createName) > 0;

    //Check if class exist under different id
    for (auto& c : Classes) {
      if (c.isRemoved())
        continue;

      if (!exact && exactMatch.count(c.Name) == 0) {
        bool matchName = compareClassName(c.Name, createName);
        if (!matchName) {
          const wstring &longName = c.getLongName();
          matchName = !longName.empty() && compareClassName(longName, createName); 
        }
        if (matchName)
          return &c;
      }

      if (exact && c.Name == createName) {
        return &c;
      }
    }

    if (Id <= 0)
      Id = getFreeClassId();

    oClass c(this, Id);
    c.Name = createName;
    exactMatch.insert(createName);
    return addClass(c);
  }
}

bool oEvent::getClassesFromBirthYear(int year, PersonSex sex, vector<int> &classes) const {
  classes.clear();

  int age = year>0 ? getThisYear() - year : 0;

  int bestMatchClass = -1;
  int bestMatchDist = 1000;

  for (oClassList::const_iterator it=Classes.begin(); it != Classes.end(); ++it) {
    if (it->isRemoved())
      continue;

    if (it->getClassType() == ClassType::oClassRelay)
      continue;

    PersonSex clsSex = it->getSex();
    if (clsSex == sFemale && sex == sMale)
      continue;
    if (clsSex == sMale && sex == sFemale)
      continue;

    int distance = 1000;
    if (age>0) {
      int high, low;
      it->getAgeLimit(low, high);

      if (high>0 && age>high)
        continue;

      if (low>0 && age<low)
        continue;

      if (high>0)
        distance = high - age;

      if (low>0)
        distance = min(distance, age-low);

      if (distance < bestMatchDist) {
        if (bestMatchClass != -1)
          classes.push_back(bestMatchClass);
        // Add best class last
        bestMatchClass = it->getId();
        bestMatchDist = distance;
      }
      else
        classes.push_back(it->getId());
    }
    else
      classes.push_back(it->getId());
  }

  // Add best class last
  if (bestMatchClass != -1) {
    classes.push_back(bestMatchClass);
    return true;
  }
  return false;
}



static bool clsSortFunction (pClass i, pClass j) {
  return (*i < *j);
}

void oEvent::getClasses(vector<pClass> &classes, bool sync) const {
  if (sync) {
    oe->synchronizeList(oListId::oLCourseId);
    oe->reinitializeClasses();
  }
  
  classes.clear();
  for (oClassList::const_iterator it = Classes.begin(); it != Classes.end(); ++it) {
    if (it->isRemoved())
      continue;

    classes.push_back(pClass(&*it));
  }

  sort(classes.begin(), classes.end(), clsSortFunction);
}


pClass oEvent::getBestClassMatch(const wstring &cname) const {
  return getClass(cname);
}

pClass oEvent::getClass(const wstring &cname) const
{
  for (oClassList::const_iterator it=Classes.begin(); it != Classes.end(); ++it) {
    if (!it->isRemoved() && compareClassName(cname, it->Name))
      return pClass(&*it);
  }
  return 0;
}

pClass oEvent::getClass(int Id) const {
  if (Id<=0)
    return nullptr;

  oClassList::const_iterator it;

  for (it=Classes.begin(); it != Classes.end(); ++it){
    if (it->Id==Id && !it->isRemoved()) {
      return pClass(&*it);
    }
  }
  return nullptr;
}

pClass oEvent::addClass(const wstring &pname, int CourseId, int classId)
{
  if (classId > 0){
    pClass pOld=getClass(classId);
    if (pOld)
      return 0;
  }

  oClass c(this, classId);
  c.Name=pname;

  if (CourseId>0)
    c.Course=getCourse(CourseId);

  Classes.push_back(c);
  Classes.back().addToEvent(this, &c);
  Classes.back().synchronize();
  updateTabs();
  return &Classes.back();
}

pClass oEvent::addClass(const oClass &c)
{
  if (c.Id==0)
    return 0;
  else {
    pClass pOld=getClass(c.getId());
    if (pOld)
      return 0;
  }

  Classes.push_back(c);
  Classes.back().addToEvent(this, &c);

  if (hasDBConnection() && !Classes.back().existInDB() && !c.isImplicitlyCreated()) {
    Classes.back().changed = true;
    Classes.back().synchronize();
  }
  return &Classes.back();
}

bool oClass::fillStageCourses(gdioutput &gdi, int stage,
                              const string &name) const
{
  if (unsigned(stage)>=MultiCourse.size())
    return false;

  gdi.clearList(name);
  const vector<pCourse> &Stage=MultiCourse[stage];
  vector<pCourse>::const_iterator it;
  string out;
  string str="";
  wchar_t bf[128];
  int m=0;

  for (it=Stage.begin(); it!=Stage.end(); ++it) {
    swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%d: %s", ++m, (*it)->getName().c_str());
    gdi.addItem(name, bf, (*it)->getId());
  }

  return true;
}

bool oClass::addStageCourse(int iStage, int courseId, int index)
{
  return addStageCourse(iStage, oe->getCourse(courseId), index);
}

bool oClass::addStageCourse(int iStage, pCourse pc, int index)
{
  if (unsigned(iStage)>=MultiCourse.size())
    return false;

  vector<pCourse> &stage=MultiCourse[iStage];

  if (pc) {
    tCoursesChanged = true;
    if (index == -1 || size_t(index) >= stage.size())
      stage.push_back(pc);
    else {
      stage.insert(stage.begin() + index, pc);
    }
    updateChanged();
    return true;
  }
  return false;
}

bool oClass::moveStageCourse(int stage, int index, int offset) {
  if (unsigned(stage) >= MultiCourse.size())
    return false;

  vector<pCourse> &stages = MultiCourse[stage];

  if (offset == -1 && size_t(index) < stages.size() && index > 0) {
    swap(stages[index - 1], stages[index]);
    updateChanged();
    return true;
  }
  else if (offset == 1 && size_t(index + 1) < stages.size() && index >= 0) {
    swap(stages[index + 1], stages[index]);
    updateChanged();
    return true;
  }
  return false;
}


void oClass::clearStageCourses(int stage) {
  if (size_t(stage) < MultiCourse.size())
    MultiCourse[stage].clear();
}

bool oClass::removeStageCourse(int iStage, int CourseId, int position)
{
  if (unsigned(iStage)>=MultiCourse.size())
    return false;

  vector<pCourse> &Stage=MultiCourse[iStage];

  if ( !(uint32_t(position)<Stage.size()))
    return false;

  if (Stage[position]->getId()==CourseId){
    tCoursesChanged = true;
    Stage.erase(Stage.begin()+position);
    updateChanged();
    return true;
  }

  return false;
}

void oClass::setNumStages(int no)
{
  if (no>=0) {
    if (MultiCourse.size() != no)
      updateChanged();
    MultiCourse.resize(no);
    legInfo.resize(no);
    tLeaderTime.resize(max(no, 1));
  }
  oe->updateTabs();
}

void oClass::getTrueStages(vector<oClass::TrueLegInfo > &stages) const
{
  stages.clear();
  if (!legInfo.empty()) {
    for (size_t k = 0; k+1 < legInfo.size(); k++) {
      if (legInfo[k].trueLeg != legInfo[k+1].trueLeg) {
        stages.push_back(TrueLegInfo(k, legInfo[k].trueLeg));
      }
    }
    stages.push_back(TrueLegInfo(legInfo.size()-1, legInfo.back().trueLeg));

    for (size_t k = 0; k <stages.size(); k++) {
      stages[k].nonOptional = k > 0 ? stages[k-1].first + 1: 0;
      while(stages[k].nonOptional <= stages[k].first) {
        if (!legInfo[stages[k].nonOptional].isOptional())
          break;
        else
          stages[k].nonOptional++;
      }
    }
  }
  else {
    stages.push_back(TrueLegInfo(0,1));
    stages.back().nonOptional = -1;
  }

}

bool oClass::startdataIgnored(int i) const
{
  StartTypes st=getStartType(i);
  LegTypes lt=getLegType(i);

  if (lt==LTIgnore || lt==LTExtra || lt==LTParallel || lt == LTParallelOptional)
    return true;

  if (st==STChange || st==STDrawn)
    return true;

  return false;
}

bool oClass::restartIgnored(int i) const
{
  StartTypes st=getStartType(i);
  LegTypes lt=getLegType(i);

  if (lt==LTIgnore || lt==LTExtra || lt==LTParallel || lt == LTParallelOptional || lt == LTGroup)
    return true;

  if (st==STTime || st==STDrawn)
    return true;

  return false;
}

void oClass::fillStartTypes(gdioutput &gdi, const string &name, bool firstLeg) {
  vector<pair<wstring, size_t>> d;  
  d.emplace_back(lang.tl("Starttid"), STTime);
  if (!firstLeg)
    d.emplace_back(lang.tl("Växling"), STChange);
  d.emplace_back(lang.tl("Tilldelad"), STDrawn);
  if (!firstLeg)
    d.emplace_back(lang.tl("Jaktstart"), STPursuit);

  gdi.setItems(name, d);
}

StartTypes oClass::getStartType(int leg) const
{
  leg = mapLeg(leg);
  if (unsigned(leg)<legInfo.size())
    return legInfo[leg].startMethod;
  else return STDrawn;
}

LegTypes oClass::getLegType(int leg) const
{
  leg = mapLeg(leg);
  if (unsigned(leg)<legInfo.size())
    return legInfo[leg].legMethod;
  else return LTNormal;
}

int oClass::getStartData(int leg) const
{
  leg = mapLeg(leg);

  if (unsigned(leg)<legInfo.size())
    return legInfo[leg].legStartData;
  else return 0;
}

int oClass::getRestartTime(int leg) const
{
  leg = mapLeg(leg);

  if (leg > 0 && (isParallel(leg) || isOptional(leg)) )
    return getRestartTime(leg-1);

  if (unsigned(leg)<legInfo.size())
    return legInfo[leg].legRestartTime;
  else return 0;
}


int oClass::getRopeTime(int leg) const {
  leg = mapLeg(leg);

  if (leg > 0 && (isParallel(leg) || isOptional(leg)) )
    return getRopeTime(leg-1);
  if (unsigned(leg)<legInfo.size()) {
    return legInfo[leg].legRopeTime;
  }
  else return 0;
}


wstring oClass::getStartDataS(int leg) const
{
  leg = mapLeg(leg);

  int s=getStartData(leg);
  StartTypes t=getStartType(leg);

  if (t==STTime || t==STPursuit) {
    if (s>0)
      return oe->getAbsTime(s);
    else return makeDash(L"-");
  }
  else if (t==STChange || t==STDrawn)
    return makeDash(L"-");

  return L"?";
}

wstring oClass::getRestartTimeS(int leg) const
{
  leg = mapLeg(leg);

  int s=getRestartTime(leg);
  StartTypes t=getStartType(leg);

  if (t==STChange || t==STPursuit) {
    if (s>0)
      return oe->getAbsTime(s);
    else return makeDash(L"-");
  }
  else if (t==STTime || t==STDrawn)
    return makeDash(L"-");

  return L"?";
}

wstring oClass::getRopeTimeS(int leg) const
{
  leg = mapLeg(leg);

  int s=getRopeTime(leg);
  StartTypes t=getStartType(leg);

  if (t==STChange || t==STPursuit) {
    if (s>0)
      return oe->getAbsTime(s);
    else return makeDash(L"-");
  }
  else if (t==STTime || t==STDrawn)
    return makeDash(L"-");

  return L"?";
}

int oClass::getLegRunner(int leg) const {
  leg = mapLeg(leg);

  if (unsigned(leg)<legInfo.size())
    if (legInfo[leg].duplicateRunner==-1)
      return leg;
    else
      return legInfo[leg].duplicateRunner;

  return leg;
}

int oClass::getLegRunnerIndex(int leg) const {
  leg = mapLeg(leg);

  if (unsigned(leg)<legInfo.size())
    if (legInfo[leg].duplicateRunner==-1)
      return 0;
    else {
      int base=legInfo[leg].duplicateRunner;
      int index=1;
      for (int k=base+1;k<leg;k++)
        if (legInfo[k].duplicateRunner==base)
          index++;
      return index;
    }

  return leg;
}


void oClass::setLegRunner(int leg, int runnerNo)
{
  bool changed=false;
  if (leg==runnerNo)
    runnerNo=-1; //Default
  else {
    if (runnerNo<leg) {
      setLegRunner(runnerNo, runnerNo);
    }
    else {
      setLegRunner(runnerNo, leg);
      runnerNo=-1;
    }
  }

  if (unsigned(leg)<legInfo.size())
    changed=legInfo[leg].duplicateRunner!=runnerNo;
  else if (leg>=0) {
    changed=true;
    legInfo.resize(leg+1);
  }

  legInfo[leg].duplicateRunner=runnerNo;

  if (changed)
    updateChanged();
}

bool oClass::checkStartMethod() {
  StartTypes st = STTime;
  bool error = false;
  for (size_t j = 0; j < legInfo.size(); j++) {
    if (!legInfo[j].isParallel())
      st = legInfo[j].startMethod;
    else if ((legInfo[j].startMethod == STChange || legInfo[j].startMethod == STPursuit) && st != legInfo[j].startMethod) {
      legInfo[j].startMethod = STDrawn;
      error = true;
    }
  }
  return error;
}

void oClass::setStartType(int leg, StartTypes st, bool throwError)
{
  bool changed=false;

  if (unsigned(leg)<legInfo.size())
    changed=legInfo[leg].startMethod!=st;
  else if (leg>=0) {
    changed=true;
    legInfo.resize(leg+1);
  }

  legInfo[leg].startMethod=st;

  bool error = checkStartMethod();

  if (changed || error)
    updateChanged();

  if (error && throwError) {
    throw meosException("Ogiltig startmetod på sträcka X#" + itos(leg+1));
  }
}

void oClass::setLegType(int leg, LegTypes lt)
{
  bool changed=false;

  if (unsigned(leg)<legInfo.size())
    changed=legInfo[leg].legMethod!=lt;
  else if (leg>=0) {
    changed=true;
    legInfo.resize(leg+1);
  }

  legInfo[leg].legMethod=lt;

  bool error = checkStartMethod();

  if (changed || error) {
    apply();
    updateChanged();
  }

  if (error) {
    throw meosException("Ogiltig startmetod på sträcka X#" + itos(leg+1));
  }
}

bool oClass::setStartData(int leg, const wstring &s) {
  int rt;
  StartTypes styp=getStartType(leg);
  if (styp==STTime || styp==STPursuit)
    rt=oe->getRelativeTime(s);
  else
    rt=wtoi(s.c_str());

  return setStartData(leg, rt);
}

bool oClass::setStartData(int leg, int value) {
  bool changed = false;
  if (unsigned(leg) < legInfo.size())
    changed = legInfo[leg].legStartData != value;
  else if (leg >= 0) {
    changed = true;
    legInfo.resize(leg + 1);
  }
  legInfo[leg].legStartData = value;

  if (changed)
    updateChanged();
  return changed;
}

void oClass::setRestartTime(int leg, const wstring &t)
{
  int rt=oe->getRelativeTime(t);
  bool changed=false;

  if (unsigned(leg)<legInfo.size())
    changed=legInfo[leg].legRestartTime!=rt;
  else if (leg>=0) {
    changed=true;
    legInfo.resize(leg+1);
  }
  legInfo[leg].legRestartTime=rt;

  if (changed)
    updateChanged();
}

void oClass::setRopeTime(int leg, const wstring &t)
{
  int rt=oe->getRelativeTime(t);
  bool changed=false;

  if (unsigned(leg)<legInfo.size())
    changed=legInfo[leg].legRopeTime!=rt;
  else if (leg>=0) {
    changed=true;
    legInfo.resize(leg+1);
  }
  legInfo[leg].legRopeTime=rt;

  if (changed)
    updateChanged();
}


void oClass::fillLegTypes(gdioutput &gdi, const string &name)
{
  vector<pair<wstring, size_t>> types;
  types.push_back( make_pair(lang.tl("Normal"), LTNormal));
  types.push_back( make_pair(lang.tl("Parallell"), LTParallel));
  types.push_back( make_pair(lang.tl("Valbar"), LTParallelOptional));
  types.push_back( make_pair(lang.tl("Extra"), LTExtra));
  types.push_back( make_pair(lang.tl("Summera"), LTSum));
  types.push_back( make_pair(lang.tl("Medlöpare"), LTIgnore));
  types.push_back( make_pair(lang.tl("Gruppera"), LTGroup));

  gdi.setItems(name, types);
}

void oEvent::fillClasses(gdioutput &gdi, const string &id, const vector<pair<wstring, size_t>>& extraItems, ClassExtra extended, ClassFilter filter)
{
  vector<pair<wstring, size_t>> d;
  oe->fillClasses(d, extended, filter);
  for (auto& ex : extraItems)
    d.push_back(ex);
  gdi.setItems(id, d);
}

const vector< pair<wstring, size_t> > &oEvent::fillClasses(vector< pair<wstring, size_t> > &out,
                                                          ClassExtra extended, ClassFilter filter) {
  set<int> undrawn;
  set<int> hasRunner;
  out.clear();
  out.reserve(Classes.size() + 2);
  if (extended == extraDrawn) {
    oRunnerList::iterator rit;

    for (rit=Runners.begin(); rit != Runners.end(); ++rit) {
      bool needTime = true;
      if (rit->isRemoved())
        continue;

      pClass pc = rit->getClassRef(true);
      if (pc) {
        if (pc->getNumStages() > 0 && pc->getStartType(rit->tLeg) != STDrawn)
          needTime = false;
      }
      if (rit->tStartTime==0 && needTime)
        undrawn.insert(rit->getClassId(true));
      hasRunner.insert(rit->getClassId(true));
    }
  }
  else if (extended == extraNumMaps)
    calculateNumRemainingMaps(false);

  oClassList::iterator it;
  synchronizeList(oListId::oLClassId);

  reinitializeClasses();
  Classes.sort();//Sort by Id

  for (it=Classes.begin(); it != Classes.end(); ++it){
    if (!it->Removed) {

      if (filter==filterOnlyMulti && it->getNumStages()<=1)
        continue;
      else if (filter==filterOnlySingle && it->getNumStages()>1)
        continue;
      else if (filter==filterOnlyDirect && !it->getAllowQuickEntry())
        continue;

      if (extended == extraNone)
        out.emplace_back(it->Name, it->Id);
      else if (extended == extraDrawn) {
        wchar_t bf[256];

        if (it->MultiCourse.size() > 0 && it->getStartType(0) == STTime)
          swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%s\t%s", it->Name.c_str(), it->getStartDataS(0).c_str());
        else if (undrawn.count(it->getId()) || !hasRunner.count(it->getId()))
          swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%s", it->Name.c_str());
        else {
          swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%s\t[S]", it->Name.c_str());
        }
        out.emplace_back(bf, it->Id);
      }
      else if (extended == extraNumMaps) {
        wchar_t bf[256];
        int nmaps = it->getNumRemainingMaps(false);
        if (nmaps != numeric_limits<int>::min())
          swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%s (%d %s)", it->Name.c_str(), nmaps, lang.tl(L"kartor").c_str());
        else
          swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%s ( - %s)", it->Name.c_str(), lang.tl(L"kartor").c_str());

        out.emplace_back(bf, it->Id);
      }
    }
  }
  return out;
}

void oEvent::getNotDrawnClasses(set<int> &classes, bool someMissing)
{
  set<int> drawn;
  classes.clear();

  oRunnerList::iterator rit;

  synchronizeList(oListId::oLRunnerId);
  for (rit=Runners.begin(); rit != Runners.end(); ++rit) {
    if (rit->tStartTime>0)
      drawn.insert(rit->getClassId(true));
    else if (someMissing)
      classes.insert(rit->getClassId(true));
  }

  // Return all classe where some runner has no start time
  if (someMissing)
    return;

  oClassList::iterator it;
  synchronizeList(oListId::oLClassId);

  // Return classes where no runner has a start time
  for (it=Classes.begin(); it != Classes.end(); ++it) {
    if (drawn.count(it->getId())==0)
      classes.insert(it->getId());
  }
}


void oEvent::getAllClasses(set<int> &classes)
{
  classes.clear();

  oClassList::const_iterator it;
  synchronizeList(oListId::oLClassId);

  for (it=Classes.begin(); it != Classes.end(); ++it){
    if (!it->Removed){
      classes.insert(it->getId());
    }
  }
}

bool oEvent::fillClassesTB(gdioutput &gdi)//Table mode
{
  oClassList::iterator it;
  synchronizeList(oListId::oLClassId);

  reinitializeClasses();
  Classes.sort();//Sort by Id

  int dx[4]={100, 100, 50, 100};
  int y=gdi.getCY();
  int x=gdi.getCX();
  int lh=gdi.getLineHeight();

  y+=lh/2;

  int xp=x;
  gdi.addString("", y, xp, 0, "Klass"); xp+=dx[0];
  gdi.addString("", y, xp, 0, "Bana"); xp+=dx[1];
  gdi.addString("", y, xp, 0, "Deltagare"); xp+=dx[2];
  y+=(3*lh)/2;

  for (it = Classes.begin(); it != Classes.end(); ++it){
    if (!it->Removed){
      int xp=x;

      gdi.addString("", y, xp, 0, it->getName(), dx[0]); xp+=dx[0];

      pCourse pc=it->getCourse();
      if (pc) gdi.addString("", y, xp, 0, pc->getName(), dx[1]);
      else gdi.addString("", y, xp, 0, "-", dx[1]);
      xp+=dx[1];

      char num[10];
      _itoa_s(it->getNumRunners(false, false, false), num, 10);

      gdi.addString("", y, xp, 0, num, dx[2]);
      xp+=dx[2];

      y+=lh;
    }
  }
  return true;
}

bool oClass::isCourseUsed(int Id) const
{
  if (Course && Course->getId()==Id)
    return true;

  if (hasMultiCourse()){
    for(unsigned i=0;i<getNumStages(); i++) {
      const vector<pCourse> &pv=MultiCourse[i];
      for(unsigned j=0; j<pv.size(); j++)
        if (pv[j]->getId()==Id) return true;
    }
  }

  return false;
}

bool oClass::hasTrueMultiCourse() const {
  if (MultiCourse.empty())
    return false;
  return MultiCourse.size()>1 || hasCoursePool() || tShowMultiDialog ||
         (MultiCourse.size()==1 && MultiCourse[0].size()>1);
}


wstring oClass::getLength(int leg) const {
  leg = mapLeg(leg);

  wchar_t bf[64];
  if (hasMultiCourse()){
    int minlen=1000000;
    int maxlen=0;

    for(unsigned i=0;i<getNumStages(); i++) {
      if (i == leg || leg == -1) {
        const vector<pCourse> &pv=MultiCourse[i];
        for(unsigned j=0; j<pv.size(); j++) {
          int l=pv[j]->getLength();
          minlen=min(l, minlen);
          maxlen=max(l, maxlen);
        }
      }
    }

    if (maxlen==0)
      return _EmptyWString;
    else if (minlen==0)
      minlen=maxlen;

    if ( (maxlen-minlen)<100 )
      swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%d", maxlen);
    else
      swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%d - %d", minlen, maxlen);

    return makeDash(bf);
  }
  else if (Course && Course->getLength()>0) {
    return Course->getLengthS();
  }
  return _EmptyWString;
}

bool oClass::hasUnorderedLegs() const {
  return getDCI().getInt("Unordered") != 0;
}

void oClass::setUnorderedLegs(bool order) {
  getDI().setInt("Unordered", order);
}

void oClass::getParallelRange(int leg, int &parLegRangeMin, int &parLegRangeMax) const {
  parLegRangeMin = leg;
  while (parLegRangeMin > 0 && size_t(parLegRangeMin) < legInfo.size()) {
    if (legInfo[parLegRangeMin].isParallel())
      parLegRangeMin--;
    else 
      break;
  }
  parLegRangeMax = leg;
  while (size_t(parLegRangeMax+1) < legInfo.size()) {
    if (legInfo[parLegRangeMax+1].isParallel() || legInfo[parLegRangeMax+1].isOptional())
      parLegRangeMax++;
    else 
      break;
  }
}

void oClass::getParallelOptionalRange(int leg, int& parLegRangeMin, int& parLegRangeMax) const {
  parLegRangeMin = leg;
  while (parLegRangeMin > 0 && size_t(parLegRangeMin) < legInfo.size()) {
    if (legInfo[parLegRangeMin].isParallel() || legInfo[parLegRangeMin].isOptional())
      parLegRangeMin--;
    else
      break;
  }
  parLegRangeMax = leg;
  while (size_t(parLegRangeMax + 1) < legInfo.size()) {
    if (legInfo[parLegRangeMax + 1].isParallel() || legInfo[parLegRangeMax + 1].isOptional())
      parLegRangeMax++;
    else
      break;
  }
}


void oClass::getParallelCourseGroup(int leg, int startNo, vector< pair<int, pCourse> > &group) const {
  group.clear();
  // Assume hasUnorderedLegs
  /*if (!hasUnorderedLegs()) {
    pCourse crs = Course;
    if (leg < MultiCourse.size()) {
      int size = MultiCourse[leg].size();
      if (size > 0) {
        crs = MultiCourse[leg][startNo%leg];
      }
    }
    group.push_back(make_pair(leg, crs));
    return;
  }
  else*/ {
    // Find first leg in group
    while (leg > 0 && size_t(leg) < legInfo.size()) {
      if (legInfo[leg].isParallel())
        leg--;
      else 
        break;
    }
    if (startNo <= 0)
      startNo = 1; // Use first course

    // Fill in all legs in the group
    do {
      if (size_t(leg) < MultiCourse.size()) {
        int size = MultiCourse[leg].size();
        if (size > 0) {
          pCourse crs = MultiCourse[leg][(startNo-1)%size];
          group.push_back(make_pair(leg, crs));
        }
      }
      leg++;
    }
    while (size_t(leg) < legInfo.size() && 
           legInfo[leg].isParallel());
  }
}

pCourse oClass::selectParallelCourse(const oRunner &r, const SICard &sic) {
  synchronize();
  pCourse rc = 0; //Best match course
  vector< pair<int, pCourse> > group;
  getParallelCourseGroup(r.getLegNumber(), r.getStartNo(), group);
  cTeam t = r.getTeam();
  if (t && group.size() > 1) {
    for (size_t k = 0; k < group.size(); k++) {
      pRunner tr = t->getRunner(group[k].first);
      if (!tr) 
        continue;

      tr->synchronize();
      if (tr->Course) {
        // The course is assigned. Remove from group
        for (size_t j = 0; j < group.size(); j++) {
          if (group[j].second == tr->Course) {
            group[j].second = 0;
            break;
          }
        }
      }
    }

    // Select best match of available courses
    int distance=-1000;
    for (size_t k = 0; k < group.size(); k++) {
      if (group[k].second) {
        int d = group[k].second->distance(sic);
        if (d >= 0) {
          if (distance < 0) 
            distance = 1000;

          if (d<distance) {
            distance = d;
            rc = group[k].second;
          }
        }
        else if (distance < 0 && d > distance) {
          distance=d;
          rc = group[k].second;
        }
      }
    }
  }

  return rc;
}

pCourse oClass::getCourse(int leg, unsigned fork, bool getSampleFromRunner) const {
  leg = mapLeg(leg);

  if (size_t(leg) < MultiCourse.size()) {
    const vector<pCourse> &courses=MultiCourse[leg];
    if (courses.size()>0) {
      int index = fork;
      if (index>0)
        index = (index-1) % courses.size();

      return courses[index];
    }
  }

  if (!getSampleFromRunner)
    return nullptr;
  else {
    pCourse res = 0;
    for (oRunnerList::iterator it = oe->Runners.begin(); it != oe->Runners.end(); ++it) {
      if (it->getClassRef(true) == this && it->Course) {
        if (it->tLeg == leg)
          return it->Course;
        else
          res = it->Course; // Might find better candidate later
      }
    }
    return res;
  }
}

pCourse oClass::getCourse(bool getSampleFromRunner) const {
  pCourse res;
  if (MultiCourse.size() == 1 && MultiCourse[0].size() == 1)
    res = MultiCourse[0][0];
  else
    res = Course;

  if (!res && getSampleFromRunner)
    res = getCourse(0,0, true);

  return res;
}

bool oClass::isForked(int leg) const {
  leg = mapLeg(leg);
  if (leg < MultiCourse.size())
    return MultiCourse[leg].size() > 1;
  return false;
}

void oClass::getCourses(int leg, vector<pCourse> &courses) const {
  leg = mapLeg(leg);

  //leg == -1 -> all courses
  courses.clear();
  set<int> added;
  
  if (leg <= 0 && Course)
    courses.push_back(Course);

  for (size_t cl = 0; cl < MultiCourse.size(); cl++) {
    if (leg>= 0 && cl != leg)
      continue;
    const vector<pCourse> &mc = MultiCourse[cl];
    for (size_t k = 0; k < mc.size(); k++)
      if (added.insert(mc[k]->Id).second)
        courses.push_back(mc[k]);
  }

  // Add shortened versions
  for (size_t k = 0; k < courses.size(); k++) {
    pCourse sht = courses[k]->getShorterVersion().second;
    int maxIter = 10;
    while (sht && --maxIter >= 0 ) {
      if (added.insert(sht->Id).second)
        courses.push_back(sht);
      sht = sht->getShorterVersion().second;
    }
  }
}

ClassType oClass::getClassType() const
{
  if (legInfo.size()==2 && (legInfo[1].isParallel() ||
                           legInfo[1].legMethod==LTIgnore) )
    return oClassPatrol;
  else if (legInfo.size()>=2) {
    if (isQualificationFinalBaseClass())
      return oClassKnockout;

    for(size_t k=1;k<legInfo.size();k++)
      if (legInfo[k].duplicateRunner!=0)
        return oClassRelay;

    return oClassIndividRelay;
  }
  else
    return oClassIndividual;
}

int oClass::getNumMultiRunners(int leg) const
{
  int ndup=0;
  for (size_t k=0;k<legInfo.size();k++) {
    if (leg==legInfo[k].duplicateRunner || (legInfo[k].duplicateRunner==-1 && k==leg))
      ndup++;
  }
  if (legInfo.empty())
    ndup++; //If no multi-course, we run at least one race.

  return ndup;
}

int oClass::getNumParallel(int leg) const
{
  int nleg = legInfo.size();
  if (leg>=nleg)
    return 1;

  int nP = 1;
  int i = leg;
  while (++i<nleg && legInfo[i].isParallel())
    nP++;

  i = leg;
  while (i>=0 && legInfo[i--].isParallel())
    nP++;
  return nP;
}

int oClass::getNumDistinctRunners() const
{
  if (legInfo.empty())
    return 1;

  int ndist=0;
  for (size_t k=0;k<legInfo.size();k++) {
    if (legInfo[k].duplicateRunner==-1)
      ndist++;
  }
  return ndist;
}

int oClass::getNumDistinctRunnersMinimal() const
{
  if (legInfo.empty())
    return 1;

  int ndist=0;
  for (size_t k=0;k<legInfo.size();k++) {
    LegTypes lt = legInfo[k].legMethod;
    if (legInfo[k].duplicateRunner==-1 && (lt != LTExtra && lt != LTIgnore && lt != LTParallelOptional) )
      ndist++;
  }
  return max(ndist, 1);
}

void oClass::resetLeaderTime() const {
  tLeaderTimeOld.resize(tLeaderTime.size());

  for (size_t k = 0; k < tLeaderTime.size(); k++) {
    tLeaderTimeOld[k].updateFrom(tLeaderTime[k]); // During apply we may reset but still want to use the computed value (for pursuit)
    tLeaderTime[k].reset();
  }

  tBestTimePerCourse.clear();
  leaderTimeVersion = -1;
}

bool oClass::hasCoursePool() const
{
  return getDCI().getInt("HasPool")!=0;
}

void oClass::setCoursePool(bool p)
{
  if (hasCoursePool() != p) {
    getDI().setInt("HasPool", p);
    tCoursesChanged = true;
  }
}

pCourse oClass::selectCourseFromPool(int leg, const SICard &card) const {
  leg = mapLeg(leg);

  int Distance=-1000;
  const oCourse *rc=0; //Best match course

  if (MultiCourse.size()==0)
    return Course;

  if (unsigned(leg)>=MultiCourse.size())
    return Course;

  // First = course to check, second = course to assign. First could be a shortened version.
  vector<pair<pCourse, pCourse> > layer(MultiCourse[leg].size());

  for (size_t k = 0; k < layer.size(); k++) {
    layer[k].first = MultiCourse[leg][k];
    layer[k].second = MultiCourse[leg][k];
  }

  while (Distance < 0 && !layer.empty()) {

    for (size_t k=0;k < layer.size(); k++) {
      
      if (layer[k].first) {
        int d = layer[k].first->distance(card);

        if (d>=0) {
          if (Distance<0) Distance=1000;

          if (d<Distance) {
            Distance=d;
            rc = layer[k].second;
          }
        }
        else {
          if (Distance<0 && d>Distance) {
            Distance=d;
            rc = layer[k].second;
          }
        }
      }
    }

    if (Distance < 0) {
      // If we have found no acceptable match, try the shortened courses, if any
      vector< pair<pCourse, pCourse> > shortenedLayer;
      for (size_t k=0;k < layer.size(); k++) {
        if (layer[k].first) {
          pCourse sw = layer[k].first->getShorterVersion().second;
          if (sw)
            shortenedLayer.push_back(make_pair(sw, layer[k].second));
        }
      }
      swap(layer, shortenedLayer);
    }
  }

  return const_cast<pCourse>(rc);
}

void oClass::updateChangedCoursePool() {
  if (!tCoursesChanged)
    return;

  bool hasPool = hasCoursePool();
  vector< set<pCourse> > crs;
  for (size_t k = 0; k < MultiCourse.size(); k++) {
    crs.push_back(set<pCourse>());
    for (size_t j = 0; j < MultiCourse[k].size(); j++) {
      if (MultiCourse[k][j])
        crs.back().insert(MultiCourse[k][j]);
    }
  }

  SICard card(ConvertedTimeStatus::Unknown);
  oRunnerList::iterator it;
  for (it = oe->Runners.begin(); it != oe->Runners.end(); ++it) {
    if (it->isRemoved() || it->getClassRef(true) != this)
      continue;

    if (size_t(it->tLeg) >= crs.size() || crs[it->tLeg].empty())
      continue;

    if (!hasPool) {
      if (it->Course) {
        it->setCourseId(0);
        it->synchronize();
      }
    }
    else {
      bool correctCourse = crs[it->tLeg].count(it->Course) > 0;
      if ((!correctCourse || (correctCourse && it->tStatus == StatusMP)) && it->Card) {
        it->Card->getSICard(card);
        pCourse crs = selectCourseFromPool(it->tLeg, card);
        if (crs != it->Course) {
          it->setCourseId(crs->getId());
          it->synchronize();
        }
      }
    }
  }
  tCoursesChanged = false;
}

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

int evaluateSeedTime(const oAbstractRunner &r) {
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

int evaluateSeedResult(const oAbstractRunner &r) {
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

int evaluateSeedPoints(const oAbstractRunner &r) {
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

class ClassSplit {
private:
  map<int, int> clubSize;
  map<int, int> idSplit;
  map<int, int> clubSplit;
  vector<const oAbstractRunner*> runners;
  void splitClubs(const vector<int> &parts);
  void valueSplit(const vector<int> &parts, vector< pair<int, int> > &valueId);
  void valueEvenSplit(const vector<int> &parts, vector< pair<int, int> > &valueId);

private:
  int evaluate(const oAbstractRunner &r, ClassSplitMethod method) {
    switch (method) {
      case SplitRank:
      case SplitRankEven:
        return r.getRanking();
      case SplitTime:
      case SplitTimeEven:
        return evaluateSeedTime(r);
      case SplitResult:
      case SplitResultEven:
        return evaluateSeedResult(r);
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



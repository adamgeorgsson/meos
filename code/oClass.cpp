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
#include <iostream>
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

  if (position < 0 || size_t(position) >= Stage.size())
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


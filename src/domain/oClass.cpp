// oClass.cpp — Full oClass implementation.
// Ported from code/oClass.cpp + code/oClassConfig.cpp with Win32 and GUI removed.
// Methods that require oRunner/oTeam (not yet implemented) are stubbed.

#define DODECLARETYPESYMBOLS
#include "oClass.h"
#include "oEvent.h"
#include "../util/gdioutput.h"
#include "../util/Table.h"
#include "oDataContainer.h"
#include "../util/meos_util.h"
#include "../util/localizer.h"
#include "qualification_final.h"
#include "meosexception.h"
#include "intkeymapimpl.hpp"
#include "xmlparser.h"
#include "../util/timeconstants.hpp"
#include <cassert>
#include <algorithm>
#include <limits>
#include <cstdint>
#include <unordered_map>

using std::swap;

// ── oClass constructor / destructor ──────────────────────────────────────────

oClass::oClass(oEvent *poe) : oBase(poe)
{
  getDI().initData();
  Course = nullptr;
  Id = oe->getFreeClassId();
  tLeaderTime.resize(1);
  tNoTiming = -1;
  tIgnoreStartPunch = -1;
  tLegTimeToPlace = nullptr;
  tLegAccTimeToPlace = nullptr;
  tSplitRevision = 0;
  tSortIndex = 0;
  tMaxTime = 0;
  tCoursesChanged = false;
  tStatusRevision = 0;
  tShowMultiDialog = false;
  parentClass = nullptr;
  tMapsRemaining = std::numeric_limits<int>::min();
  tMapsUsed = 0;
  tMapsUsedNoVacant = 0;
}

oClass::oClass(oEvent *poe, int id) : oBase(poe)
{
  getDI().initData();
  Course = nullptr;
  if (id == 0)
    id = oe->getFreeClassId();
  Id = id;
  oe->qFreeClassId = max(id % MaxClassId, oe->qFreeClassId);
  tLeaderTime.resize(1);
  tNoTiming = -1;
  tIgnoreStartPunch = -1;
  tLegTimeToPlace = nullptr;
  tLegAccTimeToPlace = nullptr;
  tSplitRevision = 0;
  tSortIndex = 0;
  tMaxTime = 0;
  tCoursesChanged = false;
  tStatusRevision = 0;
  tShowMultiDialog = false;
  parentClass = nullptr;
  tMapsRemaining = std::numeric_limits<int>::min();
  tMapsUsed = 0;
  tMapsUsedNoVacant = 0;
}

oClass::~oClass()
{
  if (tLegTimeToPlace)
    delete tLegTimeToPlace;
  if (tLegAccTimeToPlace)
    delete tLegAccTimeToPlace;
}

void oClass::clearDuplicate() {
  int id = oe->getFreeClassId();
  clearDuplicateBase(id);
  oe->qFreeClassId = max(id % MaxClassId, oe->qFreeClassId);
  getDI().setInt("SortIndex", tSortIndex);
}

// ── XML serialization ─────────────────────────────────────────────────────────

bool oClass::Write(xmlparser &xml)
{
  if (Removed) return true;
  xml.startTag("Class");

  xml.write("Id", Id);
  xml.write("Updated", getStamp());
  xml.write("Name", Name);

  if (Course)
    xml.write("Course", Course->Id);

  if (!MultiCourse.empty())
    xml.write("MultiCourse", codeMultiCourse());

  if (!legInfo.empty())
    xml.write("LegMethod", codeLegMethod());

  getDI().write(xml);
  xml.endTag();

  return true;
}

void oClass::Set(const xmlobject *xo)
{
  xmlList xl;
  xo->getObjects(xl);

  for (auto it = xl.begin(); it != xl.end(); ++it) {
    if (it->is("Id")) {
      Id = it->getInt();
    }
    else if (it->is("Name")) {
      Name = it->getWStr();
      if (Name.size() > 1 && Name.at(0) == '%')
        Name = lang.tl(Name.substr(1));
    }
    else if (it->is("Course")) {
      Course = oe->getCourse(it->getInt());
    }
    else if (it->is("MultiCourse")) {
      set<int> cid;
      vector<vector<int>> multi;
      parseCourses(it->getRawStr(), multi, cid);
      importCourses(multi);
    }
    else if (it->is("LegMethod")) {
      importLegMethod(it->getRawStr());
    }
    else if (it->is("oData")) {
      getDI().set(*it);
    }
    else if (it->is("Updated")) {
      Modified.setStamp(it->getRawStr());
    }
  }

  getNoTiming();
}

// ── Multi-course helpers ──────────────────────────────────────────────────────

void oClass::importCourses(const vector<vector<int>> &multi)
{
  MultiCourse.resize(multi.size());

  for (size_t k = 0; k < multi.size(); k++) {
    MultiCourse[k].resize(multi[k].size());
    for (size_t j = 0; j < multi[k].size(); j++) {
      MultiCourse[k][j] = oe->getCourse(multi[k][j]);
    }
  }
  setNumStages(MultiCourse.size());
}

set<int> &oClass::getMCourseIdSet(set<int> &in) const
{
  in.clear();
  for (size_t k = 0; k < MultiCourse.size(); k++) {
    for (size_t j = 0; j < MultiCourse[k].size(); j++) {
      if (MultiCourse[k][j])
        in.insert(MultiCourse[k][j]->getId());
    }
  }
  return in;
}

string oClass::codeMultiCourse() const
{
  string str;
  char bf[16];

  for (const auto &stage : MultiCourse) {
    for (const auto &pc : stage) {
      if (pc) {
        snprintf(bf, sizeof(bf), " %d", pc->getId());
        str += bf;
      }
      else str += " 0";
    }
    str += ";";
  }

  if (str.length() == 1)
    return "@";
  else if (str.length() > 0)
    return trim(str.substr(0, str.length() - 1));
  else
    return "";
}

void oClass::parseCourses(const string &courses,
                           vector<vector<int>> &multi,
                           set<int> &courseId)
{
  courseId.clear();
  multi.clear();
  if (courses.empty())
    return;

  const char *str = courses.c_str();

  multi.push_back(vector<int>());
  int n_stage = 0;

  while (*str && isspace((unsigned char)*str))
    str++;

  while (*str) {
    int cid = atoi(str);
    if (cid) {
      multi[n_stage].push_back(cid);
      courseId.insert(cid);
    }

    while (*str && (*str != ';' && *str != ' ')) str++;

    if (*str == ';') {
      str++;
      while (*str && *str == ' ') str++;
      n_stage++;
      multi.push_back(vector<int>());
    }
    else {
      if (*str) str++;
    }
  }
}

// ── Leg method helpers ────────────────────────────────────────────────────────

string oLegInfo::codeLegMethod() const {
  char bsd[16], bret[16], brot[16];

  auto codeTime = [](int t, char *b) -> const char * {
    if (timeConstSecond == 1 || t <= 0)
      snprintf(b, 16, "%d", t);
    else
      snprintf(b, 16, "%d.%d", (t / timeConstSecond), (t % timeConstSecond));
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
  startMethod = STTime;
  legMethod = LTNormal;
  legStartData = 0;
  legRestartTime = 0;
  legRopeTime = 0;
  duplicateRunner = -1;

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
  for (size_t k = 0; k < legInfo.size(); k++) {
    if (k > 0) code += "*";
    code += legInfo[k].codeLegMethod();
  }
  return code;
}

void oClass::importLegMethod(const string &legMethods)
{
  vector<string> legsplit;
  split(legMethods, "*", legsplit);

  legInfo.clear();
  for (size_t k = 0; k < legsplit.size(); k++) {
    oLegInfo oli;
    oli.importLegMethod(legsplit[k]);
    legInfo.push_back(oli);
  }

  for (size_t k = 0; k < legsplit.size(); k++) {
    if (legInfo[k].duplicateRunner != -1) {
      if (unsigned(legInfo[k].duplicateRunner) < legInfo.size())
        legInfo[legInfo[k].duplicateRunner].duplicateRunner = -1;
      else
        legInfo[k].duplicateRunner = -1;
    }
  }
  setNumStages(legInfo.size());
  apply();
}

string oClass::getCountTypeKey(int leg, CountKeyType type, bool countVacant) {
  return itos(leg) + ":" + itos(type) + (countVacant ? "V" : "");
}

// ── Data buffers ──────────────────────────────────────────────────────────────

oDataContainer &oClass::getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const {
  data    = (pvoid)oData;
  olddata = (pvoid)oDataOld;
  strData = const_cast<vector<vector<wstring>>*>(&oDataStr);
  return *oe->oClassData;
}

// ── Changed object / SQL tracking ────────────────────────────────────────────

void oClass::changedObject() {
  markSQLChanged(-1, -1);
  tNoTiming = -1;
  tIgnoreStartPunch = -1;
  oe->sqlClasses.changed = true;
}

void oClass::markSQLChanged(int leg, int control) {
  sqlChangedControlLeg[control].insert(leg);
  sqlChangedLegControl[leg].insert(control);
  oe->classChanged(this, false);
}

bool oClass::wasSQLChanged(int leg, int control) const {
  if (oe->globalModification)
    return true;

  auto res = sqlChangedControlLeg.find(-1);
  if (res != sqlChangedControlLeg.end()) {
    if (leg == -1 || res->second.count(-1) || res->second.count(leg))
      return true;
  }

  if (control != -1) {
    if (control == -2)
      return !sqlChangedControlLeg.empty();
    res = sqlChangedControlLeg.find(control);
    if (res != sqlChangedControlLeg.end()) {
      if (leg == -1 || res->second.count(-1) || res->second.count(leg))
        return true;
    }
  }

  auto res2 = sqlChangedLegControl.find(leg);
  if (res2 != sqlChangedLegControl.end()) {
    if (control == -1 || res2->second.count(-1) || res2->second.count(control))
      return true;
  }

  return false;
}

// ── Name / info ───────────────────────────────────────────────────────────────

wstring oClass::getInfo() const {
  return L"Klass " + Name;
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

// ── Course management ─────────────────────────────────────────────────────────

void oClass::setCourse(pCourse c)
{
  if (Course != c) {
    if (MultiCourse.size() == 1) {
      if (c != nullptr) {
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
    Course = c;
    tCoursesChanged = true;
    updateChanged();
    if (Course && !Course->getStart().empty())
      setStart(Course->getStart());
  }
}

pCourse oClass::getCourse(bool getSampleFromRunner) const {
  pCourse res = nullptr;
  if (MultiCourse.size() == 1 && MultiCourse[0].size() == 1)
    res = MultiCourse[0][0];
  else
    res = Course;
  return res;
}

pCourse oClass::getCourse(int leg, unsigned fork, bool /*getSampleFromRunner*/) const {
  leg = mapLeg(leg);

  if (size_t(leg) < MultiCourse.size()) {
    const vector<pCourse> &courses = MultiCourse[leg];
    if (!courses.empty()) {
      int index = fork;
      if (index > 0)
        index = (index - 1) % courses.size();
      return courses[index];
    }
  }

  return nullptr;
}

bool oClass::isForked(int leg) const {
  leg = mapLeg(leg);
  if (size_t(leg) < MultiCourse.size())
    return MultiCourse[leg].size() > 1;
  return false;
}

void oClass::getCourses(int leg, vector<pCourse> &courses) const {
  leg = mapLeg(leg);

  courses.clear();
  set<int> added;

  if (leg <= 0 && Course)
    courses.push_back(Course);

  for (size_t cl = 0; cl < MultiCourse.size(); cl++) {
    if (leg >= 0 && int(cl) != leg)
      continue;
    const vector<pCourse> &mc = MultiCourse[cl];
    for (size_t k = 0; k < mc.size(); k++) {
      if (mc[k] && added.insert(mc[k]->Id).second)
        courses.push_back(mc[k]);
    }
  }

  for (size_t k = 0; k < courses.size(); k++) {
    pCourse sht = courses[k]->getShorterVersion().second;
    int maxIter = 10;
    while (sht && --maxIter >= 0) {
      if (added.insert(sht->Id).second)
        courses.push_back(sht);
      sht = sht->getShorterVersion().second;
    }
  }
}

bool oClass::isCourseUsed(int Id) const
{
  if (Course && Course->getId() == Id)
    return true;

  if (hasMultiCourse()) {
    for (unsigned i = 0; i < getNumStages(); i++) {
      const vector<pCourse> &pv = MultiCourse[i];
      for (unsigned j = 0; j < pv.size(); j++)
        if (pv[j]->getId() == Id) return true;
    }
  }

  return false;
}

bool oClass::hasTrueMultiCourse() const {
  if (MultiCourse.empty())
    return false;
  return MultiCourse.size() > 1 || hasCoursePool() || tShowMultiDialog ||
         (MultiCourse.size() == 1 && MultiCourse[0].size() > 1);
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

bool oClass::addStageCourse(int iStage, int courseId, int index)
{
  return addStageCourse(iStage, oe->getCourse(courseId), index);
}

bool oClass::addStageCourse(int iStage, pCourse pc, int index)
{
  if (unsigned(iStage) >= MultiCourse.size())
    return false;

  vector<pCourse> &stage = MultiCourse[iStage];

  if (pc) {
    tCoursesChanged = true;
    if (index == -1 || size_t(index) >= stage.size())
      stage.push_back(pc);
    else
      stage.insert(stage.begin() + index, pc);
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
  if (unsigned(iStage) >= MultiCourse.size())
    return false;

  vector<pCourse> &Stage = MultiCourse[iStage];

  if (!(uint32_t(position) < Stage.size()))
    return false;

  if (Stage[position]->getId() == CourseId) {
    tCoursesChanged = true;
    Stage.erase(Stage.begin() + position);
    updateChanged();
    return true;
  }

  return false;
}

// ── Stages / legs ─────────────────────────────────────────────────────────────

void oClass::setNumStages(int no)
{
  if (no >= 0) {
    if (MultiCourse.size() != size_t(no))
      updateChanged();
    MultiCourse.resize(no);
    legInfo.resize(no);
    tLeaderTime.resize(max(no, 1));
  }
  oe->updateTabs();
}

void oClass::getTrueStages(vector<oClass::TrueLegInfo> &stages) const
{
  stages.clear();
  if (!legInfo.empty()) {
    for (size_t k = 0; k + 1 < legInfo.size(); k++) {
      if (legInfo[k].trueLeg != legInfo[k + 1].trueLeg)
        stages.push_back(TrueLegInfo(k, legInfo[k].trueLeg));
    }
    stages.push_back(TrueLegInfo(legInfo.size() - 1, legInfo.back().trueLeg));

    for (size_t k = 0; k < stages.size(); k++) {
      stages[k].nonOptional = k > 0 ? stages[k - 1].first + 1 : 0;
      while (stages[k].nonOptional <= stages[k].first) {
        if (!legInfo[stages[k].nonOptional].isOptional())
          break;
        else
          stages[k].nonOptional++;
      }
    }
  }
  else {
    stages.push_back(TrueLegInfo(0, 1));
    stages.back().nonOptional = -1;
  }
}

// ── Leg type / start type ─────────────────────────────────────────────────────

bool oClass::startdataIgnored(int i) const
{
  StartTypes st = getStartType(i);
  LegTypes lt = getLegType(i);

  if (lt == LTIgnore || lt == LTExtra || lt == LTParallel || lt == LTParallelOptional)
    return true;
  if (st == STChange || st == STDrawn)
    return true;
  return false;
}

bool oClass::restartIgnored(int i) const
{
  StartTypes st = getStartType(i);
  LegTypes lt = getLegType(i);

  if (lt == LTIgnore || lt == LTExtra || lt == LTParallel || lt == LTParallelOptional || lt == LTGroup)
    return true;
  if (st == STTime || st == STDrawn)
    return true;
  return false;
}

StartTypes oClass::getStartType(int leg) const
{
  leg = mapLeg(leg);
  if (unsigned(leg) < legInfo.size())
    return legInfo[leg].startMethod;
  else return STDrawn;
}

LegTypes oClass::getLegType(int leg) const
{
  leg = mapLeg(leg);
  if (unsigned(leg) < legInfo.size())
    return legInfo[leg].legMethod;
  else return LTNormal;
}

int oClass::getStartData(int leg) const
{
  leg = mapLeg(leg);
  if (unsigned(leg) < legInfo.size())
    return legInfo[leg].legStartData;
  else return 0;
}

int oClass::getRestartTime(int leg) const
{
  leg = mapLeg(leg);
  if (leg > 0 && (isParallel(leg) || isOptional(leg)))
    return getRestartTime(leg - 1);
  if (unsigned(leg) < legInfo.size())
    return legInfo[leg].legRestartTime;
  else return 0;
}

int oClass::getRopeTime(int leg) const {
  leg = mapLeg(leg);
  if (leg > 0 && (isParallel(leg) || isOptional(leg)))
    return getRopeTime(leg - 1);
  if (unsigned(leg) < legInfo.size())
    return legInfo[leg].legRopeTime;
  else return 0;
}

wstring oClass::getStartDataS(int leg) const
{
  leg = mapLeg(leg);
  int s = getStartData(leg);
  StartTypes t = getStartType(leg);

  if (t == STTime || t == STPursuit) {
    if (s > 0)
      return oe->getAbsTime(s, SubSecond::Off);
    else return makeDash(L"-");
  }
  else if (t == STChange || t == STDrawn)
    return makeDash(L"-");
  return L"?";
}

wstring oClass::getRestartTimeS(int leg) const
{
  leg = mapLeg(leg);
  int s = getRestartTime(leg);
  StartTypes t = getStartType(leg);

  if (t == STChange || t == STPursuit) {
    if (s > 0)
      return oe->getAbsTime(s, SubSecond::Off);
    else return makeDash(L"-");
  }
  else if (t == STTime || t == STDrawn)
    return makeDash(L"-");
  return L"?";
}

wstring oClass::getRopeTimeS(int leg) const {
  leg = mapLeg(leg);
  int s = getRopeTime(leg);
  StartTypes t = getStartType(leg);

  if (t == STChange || t == STPursuit) {
    if (s > 0)
      return oe->getAbsTime(s, SubSecond::Off);
    else return makeDash(L"-");
  }
  else if (t == STTime || t == STDrawn)
    return makeDash(L"-");
  return L"?";
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
  bool changed = false;
  if (unsigned(leg) < legInfo.size())
    changed = legInfo[leg].startMethod != st;
  else if (leg >= 0) {
    changed = true;
    legInfo.resize(leg + 1);
  }

  legInfo[leg].startMethod = st;
  bool error = checkStartMethod();

  if (changed || error)
    updateChanged();

  if (error && throwError)
    throw meosException("Ogiltig startmetod på sträcka X#" + itos(leg + 1));
}

void oClass::setLegType(int leg, LegTypes lt)
{
  bool changed = false;
  if (unsigned(leg) < legInfo.size())
    changed = legInfo[leg].legMethod != lt;
  else if (leg >= 0) {
    changed = true;
    legInfo.resize(leg + 1);
  }

  legInfo[leg].legMethod = lt;
  bool error = checkStartMethod();

  if (changed || error) {
    apply();
    updateChanged();
  }

  if (error)
    throw meosException("Ogiltig startmetod på sträcka X#" + itos(leg + 1));
}

bool oClass::setStartData(int leg, const wstring &s) {
  int rt;
  StartTypes styp = getStartType(leg);
  if (styp == STTime || styp == STPursuit)
    rt = oe->getRelativeTime(s);
  else
    rt = wtoi(s.c_str());
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
  int rt = oe->getRelativeTime(t);
  bool changed = false;
  if (unsigned(leg) < legInfo.size())
    changed = legInfo[leg].legRestartTime != rt;
  else if (leg >= 0) {
    changed = true;
    legInfo.resize(leg + 1);
  }
  legInfo[leg].legRestartTime = rt;
  if (changed)
    updateChanged();
}

void oClass::setRopeTime(int leg, const wstring &t)
{
  int rt = oe->getRelativeTime(t);
  bool changed = false;
  if (unsigned(leg) < legInfo.size())
    changed = legInfo[leg].legRopeTime != rt;
  else if (leg >= 0) {
    changed = true;
    legInfo.resize(leg + 1);
  }
  legInfo[leg].legRopeTime = rt;
  if (changed)
    updateChanged();
}

// ── Runner distribution helpers ───────────────────────────────────────────────

int oClass::getLegRunner(int leg) const {
  leg = mapLeg(leg);
  if (unsigned(leg) < legInfo.size())
    return (legInfo[leg].duplicateRunner == -1) ? leg : legInfo[leg].duplicateRunner;
  return leg;
}

int oClass::getLegRunnerIndex(int leg) const {
  leg = mapLeg(leg);
  if (unsigned(leg) < legInfo.size()) {
    if (legInfo[leg].duplicateRunner == -1)
      return 0;
    int base = legInfo[leg].duplicateRunner;
    int index = 1;
    for (int k = base + 1; k < leg; k++)
      if (legInfo[k].duplicateRunner == base)
        index++;
    return index;
  }
  return leg;
}

void oClass::setLegRunner(int leg, int runnerNo)
{
  bool changed = false;
  if (leg == runnerNo)
    runnerNo = -1;
  else {
    if (runnerNo < leg)
      setLegRunner(runnerNo, runnerNo);
    else {
      setLegRunner(runnerNo, leg);
      runnerNo = -1;
    }
  }

  if (unsigned(leg) < legInfo.size())
    changed = legInfo[leg].duplicateRunner != runnerNo;
  else if (leg >= 0) {
    changed = true;
    legInfo.resize(leg + 1);
  }

  legInfo[leg].duplicateRunner = runnerNo;
  if (changed)
    updateChanged();
}

int oClass::getNumMultiRunners(int leg) const
{
  int ndup = 0;
  for (size_t k = 0; k < legInfo.size(); k++) {
    if (leg == legInfo[k].duplicateRunner || (legInfo[k].duplicateRunner == -1 && int(k) == leg))
      ndup++;
  }
  if (legInfo.empty())
    ndup++;
  return ndup;
}

int oClass::getNumParallel(int leg) const
{
  int nleg = legInfo.size();
  if (leg >= nleg)
    return 1;

  int nP = 1;
  int i = leg;
  while (++i < nleg && legInfo[i].isParallel())
    nP++;

  i = leg;
  while (i >= 0 && legInfo[i--].isParallel())
    nP++;
  return nP;
}

int oClass::getNumDistinctRunners() const
{
  if (legInfo.empty())
    return 1;

  int ndist = 0;
  for (size_t k = 0; k < legInfo.size(); k++) {
    if (legInfo[k].duplicateRunner == -1)
      ndist++;
  }
  return ndist;
}

int oClass::getNumDistinctRunnersMinimal() const
{
  if (legInfo.empty())
    return 1;

  int ndist = 0;
  for (size_t k = 0; k < legInfo.size(); k++) {
    LegTypes lt = legInfo[k].legMethod;
    if (legInfo[k].duplicateRunner == -1 && (lt != LTExtra && lt != LTIgnore && lt != LTParallelOptional))
      ndist++;
  }
  return max(ndist, 1);
}

// ── Class type ────────────────────────────────────────────────────────────────

ClassType oClass::getClassType() const
{
  if (legInfo.size() == 2 && (legInfo[1].isParallel() || legInfo[1].legMethod == LTIgnore))
    return oClassPatrol;
  else if (legInfo.size() >= 2) {
    if (isQualificationFinalBaseClass())
      return oClassKnockout;
    for (size_t k = 1; k < legInfo.size(); k++)
      if (legInfo[k].duplicateRunner != 0)
        return oClassRelay;
    return oClassIndividRelay;
  }
  else
    return oClassIndividual;
}

// ── Length ────────────────────────────────────────────────────────────────────

wstring oClass::getLength(int leg) const {
  leg = mapLeg(leg);

  wchar_t bf[64];
  if (hasMultiCourse()) {
    int minlen = 1000000;
    int maxlen = 0;

    for (unsigned i = 0; i < getNumStages(); i++) {
      if (int(i) == leg || leg == -1) {
        const vector<pCourse> &pv = MultiCourse[i];
        for (unsigned j = 0; j < pv.size(); j++) {
          int l = pv[j]->getLength();
          minlen = min(l, minlen);
          maxlen = max(l, maxlen);
        }
      }
    }

    if (maxlen == 0)
      return _EmptyWString;
    else if (minlen == 0)
      minlen = maxlen;

    if ((maxlen - minlen) < 100)
      swprintf(bf, sizeof(bf) / sizeof(wchar_t), L"%d", maxlen);
    else
      swprintf(bf, sizeof(bf) / sizeof(wchar_t), L"%d - %d", minlen, maxlen);
    return makeDash(bf);
  }
  else if (Course && Course->getLength() > 0) {
    return Course->getLengthS();
  }
  return _EmptyWString;
}

// ── Parallel / unordered legs ─────────────────────────────────────────────────

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
  while (size_t(parLegRangeMax + 1) < legInfo.size()) {
    if (legInfo[parLegRangeMax + 1].isParallel() || legInfo[parLegRangeMax + 1].isOptional())
      parLegRangeMax++;
    else
      break;
  }
}

void oClass::getParallelOptionalRange(int leg, int &parLegRangeMin, int &parLegRangeMax) const {
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

// ── Parallel course group — stub (requires oRunner) ──────────────────────────

void oClass::getParallelCourseGroup(int leg, int startNo,
                                     vector<pair<int, pCourse>> &group) const {
  group.clear();
  while (leg > 0 && size_t(leg) < legInfo.size()) {
    if (legInfo[leg].isParallel())
      leg--;
    else
      break;
  }
  if (startNo <= 0)
    startNo = 1;
  do {
    if (size_t(leg) < MultiCourse.size()) {
      int sz = MultiCourse[leg].size();
      if (sz > 0)
        group.push_back(make_pair(leg, MultiCourse[leg][(startNo - 1) % sz]));
    }
    leg++;
  } while (size_t(leg) < legInfo.size() && legInfo[leg].isParallel());
}

pCourse oClass::selectParallelCourse(const oRunner & /*r*/, const SICard & /*sic*/) {
  // Stub — requires oRunner; full implementation deferred to US-003g
  return nullptr;
}

// ── Course pool ───────────────────────────────────────────────────────────────

bool oClass::hasCoursePool() const {
  return getDCI().getInt("HasPool") != 0;
}

void oClass::setCoursePool(bool p) {
  if (hasCoursePool() != p) {
    getDI().setInt("HasPool", p);
    tCoursesChanged = true;
  }
}

pCourse oClass::selectCourseFromPool(int leg, const SICard &card) const {
  leg = mapLeg(leg);

  int Distance = -1000;
  const oCourse *rc = nullptr;

  if (MultiCourse.empty())
    return Course;

  if (unsigned(leg) >= MultiCourse.size())
    return Course;

  vector<pair<pCourse, pCourse>> layer(MultiCourse[leg].size());
  for (size_t k = 0; k < layer.size(); k++) {
    layer[k].first  = MultiCourse[leg][k];
    layer[k].second = MultiCourse[leg][k];
  }

  while (Distance < 0 && !layer.empty()) {
    for (size_t k = 0; k < layer.size(); k++) {
      if (layer[k].first) {
        int d = layer[k].first->distance(card);
        if (d >= 0) {
          if (Distance < 0) Distance = 1000;
          if (d < Distance) {
            Distance = d;
            rc = layer[k].second;
          }
        }
        else {
          if (Distance < 0 && d > Distance) {
            Distance = d;
            rc = layer[k].second;
          }
        }
      }
    }
    if (Distance < 0) {
      vector<pair<pCourse, pCourse>> shortenedLayer;
      for (size_t k = 0; k < layer.size(); k++) {
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
  // Stub — requires oRunner; deferred to US-003g
  tCoursesChanged = false;
}

// ── Leader time cache ─────────────────────────────────────────────────────────

void oClass::resetLeaderTime() const {
  tLeaderTimeOld.resize(tLeaderTime.size());
  for (size_t k = 0; k < tLeaderTime.size(); k++) {
    tLeaderTimeOld[k].updateFrom(tLeaderTime[k]);
    tLeaderTime[k].reset();
  }
  tBestTimePerCourse.clear();
  leaderTimeVersion = -1;
}

oClass::LeaderInfo &oClass::getLeaderInfo(AllowRecompute recompute, int leg) const {
  if (recompute == AllowRecompute::Yes && leaderTimeVersion != int(oe->dataRevision))
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
  bool upd = false;
  switch (t) {
  case Type::Leg:
    if (bestTimeOnLegComputed <= 0 || bestTimeOnLegComputed > rt)
      bestTimeOnLegComputed = rt, upd = true;
    break;
  case Type::Total:
    if (totalLeaderTimeComputed <= 0 || totalLeaderTimeComputed > rt)
      totalLeaderTimeComputed = rt, upd = true;
    break;
  case Type::TotalInput:
    if (totalLeaderTimeInputComputed <= 0 || totalLeaderTimeInputComputed > rt)
      totalLeaderTimeInputComputed = rt, upd = true;
    break;
  default:
    assert(false);
  }
  return upd;
}

bool oClass::LeaderInfo::update(int rt, Type t) {
  if (rt <= 0)
    return false;
  bool upd = false;
  switch (t) {
  case Type::Leg:
    if (rt >= 0 && (bestTimeOnLeg < 0 || bestTimeOnLeg > rt))
      bestTimeOnLeg = rt, upd = true;
    break;
  case Type::Total:
    if (rt >= 0 && (totalLeaderTime < 0 || totalLeaderTime > rt))
      totalLeaderTime = rt, upd = true;
    break;
  case Type::TotalInput:
    if (rt >= 0 && (totalLeaderTimeInput < 0 || totalLeaderTimeInput > rt))
      totalLeaderTimeInput = rt, upd = true;
    break;
  case Type::Input:
    if (rt >= 0 && (inputTime < 0 || inputTime > rt))
      inputTime = rt, upd = true;
    break;
  default:
    assert(false);
  }
  return upd;
}

void oClass::LeaderInfo::resetComputed(Type t) {
  switch (t) {
  case Type::Leg:        bestTimeOnLegComputed = 0;       break;
  case Type::Total:      totalLeaderTimeComputed = 0;     break;
  case Type::TotalInput: totalLeaderTimeInputComputed = 0; break;
  default: break;
  }
}

int oClass::LeaderInfo::getLeader(Type t, bool computed) const {
  switch (t) {
  case Type::Leg:
    if (computed && bestTimeOnLegComputed > 0)
      return bestTimeOnLegComputed;
    return bestTimeOnLeg;
  case Type::Total:
    if (computed && totalLeaderTimeComputed > 0)
      return totalLeaderTimeComputed;
    return totalLeaderTime;
  case Type::TotalInput:
    if (computed && totalLeaderTimeInputComputed > 0)
      return totalLeaderTimeInputComputed;
    if (totalLeaderTimeInput > 0)
      return totalLeaderTimeInput;
    return inputTime;
  default:
    return 0;
  }
}

void oClass::LeaderInfo::copyInputToTotalInput() {
  if (inputTime > 0 && (totalLeaderTimeInput < 0 || totalLeaderTimeInput > inputTime))
    totalLeaderTimeInput = inputTime;
}

void oClass::updateLeaderTimes() const {
  // Stub — requires oRunner; deferred to US-003g
  resetLeaderTime();
  leaderTimeVersion = oe->dataRevision;
}

int oClass::getBestLegTime(AllowRecompute recompute, int leg, bool computedTime) const {
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
  if (recompute == AllowRecompute::Yes && leaderTimeVersion != int(oe->dataRevision))
    updateLeaderTimes();
  auto res = tBestTimePerCourse.find(courseId);
  if (res == tBestTimePerCourse.end())
    return 0;
  return res->second;
}

int oClass::getBestInputTime(AllowRecompute recompute, int leg) const {
  leg = mapLeg(leg);
  if (unsigned(leg) >= tLeaderTime.size())
    return 0;
  int it = getLeaderInfo(recompute, leg).getInputTime();
  if (it == -1 && recompute == AllowRecompute::Yes) {
    updateLeaderTimes();
    it = getLeaderInfo(AllowRecompute::No, leg).getInputTime();
  }
  return it;
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

  while (res == -1 && ++iter < 2) {
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

// ── Split analysis ────────────────────────────────────────────────────────────

void oClass::clearSplitAnalysis()
{
  tFirstStart.clear();
  tLastStart.clear();
  tSplitAnalysisData.clear();
  tCourseLegLeaderTime.clear();
  tCourseAccLegLeaderTime.clear();

  if (tLegTimeToPlace)
    delete tLegTimeToPlace;
  tLegTimeToPlace = nullptr;

  if (tLegAccTimeToPlace)
    delete tLegAccTimeToPlace;
  tLegAccTimeToPlace = nullptr;

  tSplitRevision++;
  oe->classChanged(this, false);
}

void oClass::insertLegPlace(int from, int to, int time, int place)
{
  if (tLegTimeToPlace) {
    int key = time + (to + from * 256) * 8013;
    tLegTimeToPlace->insert(key, place);
  }
}

void oClass::insertAccLegPlace(int courseId, int controlNo, int time, int place)
{
  if (tLegAccTimeToPlace) {
    int key = time + (controlNo + courseId * 128) * 16013;
    tLegAccTimeToPlace->insert(key, place);
  }
}

int oClass::getLegPlace(int ifrom, int ito, int time) const
{
  if (tLegTimeToPlace) {
    int key = time + (ito + ifrom * 256) * 8013;
    int place;
    if (tLegTimeToPlace->lookup(key, place))
      return place;
  }
  return 0;
}

int oClass::getAccLegPlace(int courseId, int controlNo, int time) const
{
  if (tLegAccTimeToPlace) {
    int key = time + (controlNo + courseId * 128) * 16013;
    int place;
    if (tLegAccTimeToPlace->lookup(key, place))
      return place;
  }
  return 0;
}

int oClass::getAccLegControlLeader(int teamLeg, int courseControlId) const {
  if (size_t(teamLeg) < teamLegCourseControlToLeaderPlace.size()) {
    auto res = teamLegCourseControlToLeaderPlace[teamLeg].find(courseControlId);
    if (res != teamLegCourseControlToLeaderPlace[teamLeg].end())
      return res->second.leader;
  }
  return 0;
}

int oClass::getAccLegControlPlace(int teamLeg, int courseControlId, int time) const {
  if (size_t(teamLeg) < teamLegCourseControlToLeaderPlace.size()) {
    auto res = teamLegCourseControlToLeaderPlace[teamLeg].find(courseControlId);
    if (res != teamLegCourseControlToLeaderPlace[teamLeg].end()) {
      auto &ttp = res->second.timeToPlace;
      auto v = ttp.find(time);
      if (v != ttp.end())
        return v->second;
    }
  }
  return 0;
}

// ── Start range (stub — requires oRunner) ────────────────────────────────────

void oClass::getStartRange(int leg, int &firstStart, int &lastStart) const {
  // Stub — requires oRunner iteration; deferred to US-003g
  firstStart = 0;
  lastStart = 0;
}

// ── Reinitialize / apply / sort index ────────────────────────────────────────

void oClass::reinitialize(bool force) const {
  if (!force && isInitialized)
    return;
  isInitialized = true;

  int ix = getDCI().getInt("SortIndex");
  if (ix == 0) {
    ix = getSortIndex(getId() * 10);
    const_cast<oClass*>(this)->getDI().setInt("SortIndex", ix);
  }
  tSortIndex = ix;

  tMaxTime = getDCI().getInt("MaxTime");
  if (tMaxTime == 0 && oe)
    tMaxTime = oe->getMaximalTime();

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

  for (pClass pc : virtualClasses) {
    if (pc)
      pc->parentClass = nullptr;
  }
  virtualClasses.clear();
  qualificatonFinal.reset();
}

int oClass::getSortIndex(int candidate) const {
  int major = std::numeric_limits<int>::max();
  int minor = 0;

  for (const auto &cls : oe->Classes) {
    int ix = cls.getDCI().getInt("SortIndex");
    if (ix > 0) {
      if (ix > candidate && ix < major)
        major = ix;
      if (ix < candidate && ix > minor)
        minor = ix;
    }
  }

  if (major < std::numeric_limits<int>::max() && minor > 0 &&
      ((major - candidate) < 10 || (candidate - minor) < 10))
    return (major + minor) / 2;
  else
    return candidate;
}

void oClass::apply() {
  int trueLeg = 0;
  int trueSubLeg = 0;

  for (size_t k = 0; k < legInfo.size(); k++) {
    oLegInfo &li = legInfo[k];
    LegTypes lt = li.legMethod;
    if (lt == LTNormal || lt == LTSum || lt == LTGroup) {
      trueLeg++;
      trueSubLeg = 0;
    }
    else
      trueSubLeg++;

    if (trueSubLeg == 0 && (k + 1) < legInfo.size()) {
      LegTypes nt = legInfo[k + 1].legMethod;
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

// ── Leg number helpers ────────────────────────────────────────────────────────

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

  int stop = min<int>(leg, int(legInfo.size()) - 1);
  int k;
  for (k = 0; k < stop; k++) {
    if (legInfo[k + 1].isParallel() || legInfo[k + 1].isOptional())
      legOrder++;
    else {
      legOrder = 0;
      legNumber++;
    }
  }
  if (legOrder == 0) {
    if (k + 1 < int(legInfo.size()) && (legInfo[k + 1].isParallel() || legInfo[k + 1].isOptional()))
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
      return (int)k;
  }
  return -1;
}

int oClass::getLinearIndex(int index, bool isLinear) const {
  if (legInfo.empty())
    return 0;
  if (size_t(index) >= legInfo.size())
    return int(legInfo.size()) - 1;
  return isLinear ? index : getLegNumberLinear(index, 0);
}

wstring oClass::getLegNumber(int leg) const {
  int legNumber, legOrder;
  bool par = splitLegNumberParallel(leg, legNumber, legOrder);
  wchar_t bf[16];
  if (par) {
    char symb = 'a' + legOrder;
    swprintf(bf, sizeof(bf) / sizeof(wchar_t), L"%d%c", legNumber + 1, symb);
  }
  else {
    swprintf(bf, sizeof(bf) / sizeof(wchar_t), L"%d", legNumber + 1);
  }
  return bf;
}

int oClass::getNextBaseLeg(int leg) const {
  for (size_t k = leg + 1; k < legInfo.size(); k++) {
    if (!(legInfo[k].isParallel() || legInfo[k].isOptional()))
      return (int)k;
  }
  return -1;
}

int oClass::getPreceedingLeg(int leg) const {
  if (size_t(leg) >= legInfo.size())
    leg = int(legInfo.size()) - 1;
  for (int k = leg; k > 0; k--) {
    if (!(legInfo[k].isParallel() || legInfo[k].isOptional()))
      return k - 1;
  }
  return -1;
}

int oClass::getResultDefining(int leg) const {
  int res = leg;
  while (size_t(res + 1) < legInfo.size() &&
         (legInfo[res + 1].isParallel() || legInfo[res + 1].isOptional()))
    res++;
  if (size_t(res) >= legInfo.size())
    res = int(legInfo.size()) - 1;
  return res;
}

// ── Class status ──────────────────────────────────────────────────────────────

oClass::ClassStatus oClass::getClassStatus() const {
  if (tStatusRevision != int(oe->dataRevision)) {
    wstring s = getDCI().getString("Status");
    if (s == L"I")
      tStatus = ClassStatus::Invalid;
    else if (s == L"IR")
      tStatus = ClassStatus::InvalidRefund;
    else
      tStatus = ClassStatus::Normal;
    tStatusRevision = int(oe->dataRevision);
  }
  return tStatus;
}

void oClass::fillClassStatus(vector<pair<wstring, wstring>> &statusClass) {
  statusClass.push_back(make_pair(L"", L"OK"));
  statusClass.push_back(make_pair(L"IR", L"Struken med återbetalning"));
  statusClass.push_back(make_pair(L"I", L"Struken utan återbetalning"));
}

// ── Cache management ──────────────────────────────────────────────────────────

void oClass::clearCache(bool recalculate) {
  if (recalculate)
    oe->reCalculateLeaderTimes(getId());
  clearSplitAnalysis();
  tResultInfo.clear();
}

// ── Runner statistics — stubs (require oRunner) ───────────────────────────────

int oClass::getNumRunners(bool /*checkFirstLeg*/, bool /*noCountVacant*/, bool /*noCountNotCompeting*/) const {
  // Stub — requires oRunner; deferred to US-003g
  return 0;
}

void oClass::getNumResults(int /*leg*/, int &total, int &finished, int &dns) const {
  // Stub — requires oRunner; deferred to US-003g
  total = finished = dns = 0;
}

void oClass::getStatistics(const set<int> & /*feeLock*/, int &entries, int &started) const {
  // Stub — requires oRunner; deferred to US-003g
  entries = started = 0;
}

// ── Maps ──────────────────────────────────────────────────────────────────────

int oClass::getNumRemainingMaps(bool forceRecalculate) const {
  oe->calculateNumRemainingMaps(forceRecalculate);
  int numMaps = tMapsRemaining;
  if (Course && Course->tMapsRemaining != std::numeric_limits<int>::min()) {
    if (numMaps == std::numeric_limits<int>::min())
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

// ── Age limit ─────────────────────────────────────────────────────────────────

void oClass::getAgeLimit(int &low, int &high) const {
  low  = getDCI().getInt("LowAge");
  high = getDCI().getInt("HighAge");
}

void oClass::setAgeLimit(int low, int high) {
  getDI().setInt("LowAge", low);
  getDI().setInt("HighAge", high);
}

int oClass::getExpectedAge() const {
  int low, high;
  getAgeLimit(low, high);

  if (low > 0 && high > 0) return (low + high) / 2;
  if (low == 0 && high > 0) return high - 3;
  if (low > 0 && high == 0) return low + 1;

  for (size_t k = 0; k < Name.length(); k++) {
    if (Name[k] >= '0' && Name[k] <= '9') {
      int age = wtoi(&Name[k]);
      if (age >= 10 && age < 100) {
        if (age <= 20) return age - 1;
        else if (age == 21) return 28;
        else if (age >= 35) return age + 2;
      }
    }
  }
  return 0;
}

// ── Sex / start / block / quick entry / timing ────────────────────────────────

PersonSex oClass::getSex() const {
  return interpretSex(getDCI().getString("Sex"));
}

void oClass::setSex(PersonSex sex) {
  getDI().setString("Sex", encodeSex(sex));
}

const wstring &oClass::getStart() const {
  return getDCI().getString("StartName");
}

void oClass::setStart(const wstring &start) {
  getDI().setString("StartName", start);
}

int oClass::getBlock() const {
  return getDCI().getInt("StartBlock");
}

void oClass::setBlock(int block) {
  getDI().setInt("StartBlock", block);
}

bool oClass::getAllowQuickEntry() const {
  return getDCI().getInt("AllowQuickEntry") != 0;
}

void oClass::setAllowQuickEntry(bool quick) {
  getDI().setInt("AllowQuickEntry", quick);
}

bool oClass::getNoTiming() const {
  if (tNoTiming != 0 && tNoTiming != 1)
    tNoTiming = getDCI().getInt("NoTiming") != 0 ? 1 : 0;
  return tNoTiming != 0;
}

void oClass::setNoTiming(bool quick) {
  tNoTiming = quick ? 1 : 0;
  getDI().setInt("NoTiming", quick);
}

bool oClass::ignoreStartPunch() const {
  if (tIgnoreStartPunch != 0 && tIgnoreStartPunch != 1)
    tIgnoreStartPunch = getDCI().getInt("IgnoreStart") != 0 ? 1 : 0;
  return tIgnoreStartPunch != 0;
}

void oClass::setIgnoreStartPunch(bool v) {
  tIgnoreStartPunch = v;
  getDI().setInt("IgnoreStart", v);
}

void oClass::updatedIgnoreStartPunch() {
  // Stub — full implementation requires oRunner/oFreePunch; deferred to US-003g
  updateChanged();
  synchronize();
}

bool oClass::hasFreeStart() const {
  return getDCI().getInt("FreeStart") != 0;
}

void oClass::setFreeStart(bool quick) {
  getDI().setInt("FreeStart", quick);
}

bool oClass::hasRequestStart() const {
  return getDCI().getInt("RequestStart") != 0;
}

void oClass::setRequestStart(bool quick) {
  getDI().setInt("RequestStart", quick);
}

bool oClass::hasDirectResult() const {
  return getDCI().getInt("DirectResult") != 0;
}

void oClass::setDirectResult(bool quick) {
  getDI().setInt("DirectResult", quick);
}

// ── Type / class type ─────────────────────────────────────────────────────────

wstring oClass::getType() const {
  return getDCI().getString("ClassType");
}

void oClass::setType(const wstring &start) {
  getDI().setString("ClassType", start);
}

ClassMetaType oClass::interpretClassType() const {
  int lowAge, highAge;
  getAgeLimit(lowAge, highAge);

  if (highAge > 0 && highAge <= 16)
    return ctYouth;

  map<wstring, ClassMetaType> types;
  oe->getPredefinedClassTypes(types);
  wstring type = getType();

  for (auto it = types.begin(); it != types.end(); ++it) {
    if (type == it->first || type == lang.tl(it->first))
      return it->second;
  }
  return ctUnknown;
}

void oClass::assignTypeFromName() {
  wstring type = getType();
  if (type.empty()) {
    wstring prefix, suffix;
    extractAnyNumber(Name, prefix, suffix);
    int age = getExpectedAge();

    ClassMetaType mt = ctUnknown;
    if (age >= 18) {
      if (stringMatch(suffix, lang.tl(L"Elit")) || wcschr(suffix.c_str(), 'E'))
        mt = ctElite;
      else if (stringMatch(suffix, lang.tl(L"Motion")) || wcschr(suffix.c_str(), 'M'))
        mt = ctExercise;
      else
        mt = ctNormal;
    }
    else if (age >= 10 && age <= 16)
      mt = ctYouth;
    else if (age < 10) {
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

    for (auto it = types.begin(); it != types.end(); ++it) {
      if (it->second == mt) {
        setType(lang.tl(it->first));
        return;
      }
    }
  }
}

bool oClass::isSingleRunnerMultiStage() const {
  return getNumStages() > 1 && getNumDistinctRunnersMinimal() == 1;
}

bool oClass::isSingleStageOnly() const {
  return getDCI().getInt("NoTotalResult") != 0;
}

void oClass::setSingleStageOnly(bool singleStageOnly) {
  getDI().setInt("NoTotalResult", singleStageOnly);
}

// ── Fee management ────────────────────────────────────────────────────────────

int oClass::getEntryFee(const wstring &date, int age) const {
  oDataConstInterface odc = oe->getDCI();
  wstring ordEntry = odc.getDate("OrdinaryEntry");
  wstring lateEntry = odc.getDate("SecondEntryDate");
  bool late = date > ordEntry && ordEntry >= L"2010-01-01";
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
    if (late2 && veryHigh > 0) return veryHigh;
    else if (late && high > 0) return high;
    else if (normal > 0) return normal;
  }

  int veryHigh = getDCI().getInt("SecondHighClassFee");
  int high = getDCI().getInt("HighClassFee");
  int normal = getDCI().getInt("ClassFee");

  if (late2 && veryHigh > 0) return veryHigh;
  if (late && high > 0) return high;
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
      lateFee = int(fee * factor + 0.5);
      lateReducedFee = int(reducedFee * factor + 0.5);
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

vector<pair<wstring, size_t>> oClass::getAllFees() const {
  set<int> fees;
  int f = getDCI().getInt("ClassFee");    if (f > 0) fees.insert(f);
  f = getDCI().getInt("ClassFeeRed");     if (f > 0) fees.insert(f);
  f = getDCI().getInt("HighClassFee");    if (f > 0) fees.insert(f);
  f = getDCI().getInt("HighClassFeeRed"); if (f > 0) fees.insert(f);

  if (fees.empty()) {
    f = oe->getDCI().getInt("EliteFee");  if (f > 0) fees.insert(f);
    f = oe->getDCI().getInt("EntryFee");  if (f > 0) fees.insert(f);
    f = oe->getDCI().getInt("YouthFee");  if (f > 0) fees.insert(f);
  }
  vector<pair<wstring, size_t>> ff;
  for (int fee : fees)
    ff.emplace_back(oe->formatCurrency(fee), fee);
  return ff;
}

// ── Bib mode ──────────────────────────────────────────────────────────────────

AutoBibType oClass::getAutoBibType() const {
  const wstring &bib = getDCI().getString("Bib");
  if (bib.empty()) return AutoBibManual;
  else if (bib == L"*") return AutoBibConsecutive;
  else if (bib == L"-") return AutoBibNone;
  else return AutoBibExplicit;
}

BibMode oClass::getBibMode() const {
  const wstring &bm = getDCI().getString("BibMode");
  wchar_t b = bm.empty() ? 0 : bm[0];
  if (b == 'A') return BibAdd;
  else if (b == 'F') return BibFree;
  else if (b == 'L') return BibLeg;
  else return BibSame;
}

void oClass::setBibMode(BibMode bibMode) {
  wstring res;
  switch (bibMode) {
  case BibAdd:  res = L"A"; break;
  case BibFree: res = L"F"; break;
  case BibLeg:  res = L"L"; break;
  case BibSame: res = L"";  break;
  default: throw meosException("Invalid bib mode");
  }
  getDI().setString("BibMode", res);
}

int oClass::extractBibPattern(const wstring &bibInfo, wchar_t pattern[32]) {
  int number = 0;
  if (bibInfo.empty()) {
    pattern[0] = 0;
  }
  else {
    number = 0;
    int pIndex = 0;
    bool hasNC = false;

    for (size_t j = 0; j < bibInfo.size() && j < 10; j++) {
      if (bibInfo[j] >= '0' && bibInfo[j] <= '9') {
        if (!hasNC) {
          pattern[pIndex++] = '%';
          pattern[pIndex++] = 'd';
          hasNC = true;
        }
        number = 10 * number + (bibInfo[j] - '0');
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

pair<int, wstring> oClass::getNextBib(map<int, pair<wstring, int>> &patterns) {
  auto it = patterns.find(Id);
  if (it != patterns.end() && it->second.second > 0) {
    wchar_t bib[32];
    swprintf(bib, sizeof(bib) / sizeof(wchar_t), it->second.first.c_str(), ++it->second.second);
    return make_pair(it->second.second, wstring(bib));
  }
  return make_pair(0, _EmptyWString);
}

pair<int, wstring> oClass::getNextBib() {
  // Stub — requires oRunner/oTeam; deferred to US-003g
  return make_pair(0, _EmptyWString);
}

void oClass::extractBibPatterns(oEvent & /*oe*/, map<int, pair<wstring, int>> & /*patterns*/) {
  // Stub — requires oRunner/oTeam; deferred to US-003g
}

// ── Draw parameters ───────────────────────────────────────────────────────────

int oClass::getDrawFirstStart() const  { return getDCI().getInt("FirstStart"); }
void oClass::setDrawFirstStart(int st) { getDI().setInt("FirstStart", st); }
int oClass::getDrawInterval() const    { return getDCI().getInt("StartInterval"); }
void oClass::setDrawInterval(int st)   { getDI().setInt("StartInterval", st); }
int oClass::getDrawVacant() const      { return getDCI().getInt("Vacant"); }
void oClass::setDrawVacant(int st)     { getDI().setInt("Vacant", st); }
int oClass::getDrawNumReserved() const { return getDCI().getInt("Reserved") & 0xFF; }
void oClass::setDrawNumReserved(int st) {
  int v = getDCI().getInt("Reserved") & 0xFF00;
  getDI().setInt("Reserved", v | st);
}

void oClass::setDrawSpecification(const vector<DrawSpecified> &spec) {
  int flag = 0;
  for (auto ds : spec)
    flag |= int(ds);
  int v = getDrawNumReserved();
  getDI().setInt("Reserved", v | (flag << 8));
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

// ── Flags ─────────────────────────────────────────────────────────────────────

bool oClass::hasFlag(TransferFlags flag) const {
  return (getDCI().getInt("TransferFlags") & flag) != 0;
}

void oClass::setFlag(TransferFlags flag, bool onoff) {
  int cf = getDCI().getInt("TransferFlags");
  cf = onoff ? (cf | flag) : (cf & (~flag));
  getDI().setInt("TransferFlags", cf);
}

bool oClass::hasClassGlobalDependence() const {
  for (size_t k = 0; k < legInfo.size(); k++) {
    if (legInfo[k].startMethod == STPursuit)
      return true;
  }
  return false;
}

// ── Rogaining ─────────────────────────────────────────────────────────────────

bool oClass::isRogaining() const {
  if (Course && Course->getMaximumRogainingTime() > 0)
    return true;
  if (MultiCourse.size() > 0 && MultiCourse[0].size() > 0 &&
      MultiCourse[0][0] && MultiCourse[0][0]->getMaximumRogainingTime() > 0)
    return true;
  return false;
}

oClass::RogainingAnalysis oClass::getRogainingAnalysis(int /*from*/, int /*to*/, double /*baseSpeed*/) const {
  // Stub — requires rogainingStatistics with oRunner data; deferred to US-003g
  return RogainingAnalysis();
}

vector<oClass::RogainingLeg> oClass::getRogainingLegs() const {
  // Stub — requires rogainingStatistics with oRunner data; deferred to US-003g
  return {};
}

// ── Forking ───────────────────────────────────────────────────────────────────

bool oClass::checkForking(vector<vector<int>> &, vector<vector<int>> &, set<pair<int, int>> &) const {
  // Stub — complex forking analysis; deferred
  return true;
}

pair<int, int> oClass::autoForking(const vector<vector<int>> &, int) {
  // Stub — requires oRunner; deferred to US-003g
  return make_pair(0, 0);
}

int oClass::getNumForks() const {
  int maxForks = 1;
  for (size_t leg = 0; leg < MultiCourse.size(); leg++) {
    if (!MultiCourse[leg].empty())
      maxForks = max(maxForks, (int)MultiCourse[leg].size());
  }
  return maxForks;
}

long long oClass::setupForkKey(const vector<int> indices,
                                const vector<vector<vector<int>>> &courseKeys,
                                vector<int> &ws) {
  // Stub — complex algorithm
  return 0;
}

// ── Remove / canRemove ────────────────────────────────────────────────────────

void oClass::remove() {
  if (oe)
    oe->removeClass(Id);
}

bool oClass::canRemove() const {
  return !oe->isClassUsed(Id);
}

int oClass::getMaximumRunnerTime() const {
  reinitialize(false);
  return tMaxTime;
}

// ── Result module ─────────────────────────────────────────────────────────────

GeneralResult *oClass::getResultModule() const {
  // Stub — requires getGeneralResult; deferred to US-003i
  return nullptr;
}

void oClass::setResultModule(const string &tag) {
  wstring wtag(tag.begin(), tag.end());
  getDI().setString("Result", wtag);
}

const string &oClass::getResultModuleTag() const {
  auto ws = getDCI().getString("Result");
  string &s = StringCache::getInstance().get();
  s.assign(ws.begin(), ws.end());
  return s;
}

// ── Qualification / virtual classes ──────────────────────────────────────────

int oClass::getNumQualificationFinalClasses() const {
  reinitialize(false);
  if (qualificatonFinal)
    return qualificatonFinal->getNumClasses() + 1;
  return 0;
}

void oClass::loadQualificationFinalScheme(const QualificationFinal &scheme) {
  auto qf = make_shared<QualificationFinal>(MaxClassId, Id);
  qf->setClasses(scheme.getClasses());
  wstring enc;
  qf->encode(enc);

  const int ns = qf->getNumStages();
  setNumStages(ns);
  for (int i = 1; i < ns; i++) {
    setStartType(i, StartTypes::STDrawn, true);
    setLegType(i, LegTypes::LTNormal);
    setLegRunner(i, 0);
  }

  clearQualificationFinal();
  qualificatonFinal = qf;
  getDI().setString("Qualification", enc);
  for (int i = 0; i < qualificatonFinal->getNumClasses(); i++) {
    pClass inst = getVirtualClass(i + 1, true);
    if (inst)
      inst->synchronize();
  }
  synchronize();
  // Note: runner re-assignment deferred to US-003g
}

void oClass::updateFinalClasses(oRunner * /*causingResult*/, bool /*updateStartNumbers*/) {
  // Stub — requires oRunner; deferred to US-003g
}

oClass *oClass::getVirtualClass(int instance, bool allowCreation) {
  if (instance == 0)
    return this;
  if (parentClass)
    return parentClass->getVirtualClass(instance, allowCreation);

  if (size_t(instance) < virtualClasses.size() && virtualClasses[instance])
    return virtualClasses[instance];

  if (instance >= getNumQualificationFinalClasses())
    return this;
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
  return this;
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
    return pClass(this);
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
  return pClass(this);
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
  copy.Id = virtId;
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

// ── initClassId ───────────────────────────────────────────────────────────────

void oClass::initClassId(oEvent &oe, const set<int> &classes) {
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

  for (size_t k = 0; k < cls.size(); k++) {
    if (!classes.empty() && !classes.count(cls[k]->getId()))
      continue;
    long long extId = cls[k]->getExtIdentifier();
    if (extId == 0) {
      long long id = cls[k]->getId();
      while (id2Cls.count(id))
        id += 100000;
      id2Cls[id] = cls[k]->getName();
      cls[k]->setExtIdentifier(id);
    }
  }
}

// ── Adjust vacants — stub ─────────────────────────────────────────────────────

void oClass::adjustNumVacant(int /*leg*/, int /*numVacant*/) {
  // Stub — requires oRunner/oEvent; deferred to US-003g
}

// ── Static enum helpers ───────────────────────────────────────────────────────

void oClass::getSplitMethods(vector<pair<wstring, size_t>> &methods) {
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

void oClass::getSeedingMethods(vector<pair<wstring, size_t>> &methods) {
  methods.clear();
  methods.push_back(make_pair(lang.tl("Ranking"), SeedRank));
  methods.push_back(make_pair(lang.tl("Placering"), SeedResult));
  methods.push_back(make_pair(lang.tl("Tid"), SeedTime));
  methods.push_back(make_pair(lang.tl("Poäng"), SeedPoints));
}

// ── Split class / merge class — stubs (require oRunner/oTeam) ────────────────

void oClass::splitClass(ClassSplitMethod /*method*/, const vector<int> & /*parts*/,
                         vector<int> & /*outClassId*/) {
  // Stub — requires oRunner/oTeam; deferred to US-003g
}

void oClass::mergeClass(int /*classIdSec*/) {
  // Stub — requires oRunner/oTeam; deferred to US-003g
}

void oClass::drawSeeded(ClassSeedMethod /*seed*/, int /*leg*/, int /*firstStart*/,
                         int /*interval*/, const vector<int> & /*groups*/,
                         bool /*noClubNb*/, bool /*reverse*/, int /*pairSize*/) {
  // Stub — requires oRunner; deferred to US-003g
}

// ── GUI-coupled methods (stubbed) ─────────────────────────────────────────────

bool oClass::fillStageCourses(gdioutput &gdi, int stage, const string &name) const {
  if (unsigned(stage) >= MultiCourse.size())
    return false;
  return true;
}

void oClass::fillStartTypes(gdioutput & /*gdi*/, const string & /*name*/, bool /*firstLeg*/) {}
void oClass::fillLegTypes(gdioutput & /*gdi*/, const string & /*name*/) {}

void oClass::addTableRow(Table & /*table*/) const {}

pair<int, bool> oClass::inputData(int id, const wstring &input,
                                    int inputId, wstring &output, bool noUpdate) {
  synchronize(false);
  if (id > 1000)
    return oe->oClassData->inputData(this, id, input, inputId, output, noUpdate);
  switch (id) {
  case TID_CLASSNAME:
    setName(input, true);
    synchronize();
    output = getName();
    break;
  case TID_COURSE: {
    pCourse c = nullptr;
    if (inputId != 0)
      c = oe->getCourse(inputId);
    setCourse(c);
    synchronize();
    output = input;
    break;
  }
  }
  return make_pair(0, false);
}

void oClass::fillInput(int id, vector<pair<wstring, size_t>> &out, size_t &selected) {
  if (id > 1000) {
    oe->oClassData->fillInput(this, id, 0, out, selected);
    return;
  }
  if (id == TID_COURSE) {
    out.clear();
    vector<pCourse> crs;
    oe->getCourses(crs);
    for (auto pc : crs)
      out.push_back(make_pair(pc->getName(), pc->getId()));
    out.push_back(make_pair(lang.tl(L"Ingen bana"), 0));
    pCourse c = getCourse(false);
    selected = c ? c->getId() : 0;
  }
}

static shared_ptr<Table> classTable;
const shared_ptr<Table> &oClass::getTable(oEvent * /*oe*/) {
  if (!classTable)
    classTable = make_shared<Table>();
  return classTable;
}

// ── merge ─────────────────────────────────────────────────────────────────────

void oClass::merge(const oBase &input, const oBase *base) {
  const oClass &src = dynamic_cast<const oClass&>(input);

  if (!src.Name.empty())
    setName(src.Name, true);
  setCourse(oe->getCourse(src.getCourseId()));

  if (!src.MultiCourse.empty()) {
    vector<vector<pCourse>> mcCopy = MultiCourse;
    set<int> cid;
    vector<vector<int>> multi;
    parseCourses(src.codeMultiCourse(), multi, cid);
    importCourses(multi);
    if (mcCopy != MultiCourse)
      updateChanged();
  }
  else {
    setNumStages(0);
  }

  if (!src.legInfo.empty()) {
    if (codeLegMethod() != src.codeLegMethod()) {
      importLegMethod(src.codeLegMethod());
      updateChanged();
    }
  }

  if (getDI().merge(input, base))
    updateChanged();

  synchronize(true);
}

// ── oEvent class management (implemented here for domain layer) ───────────────

pClass oEvent::getClass(int Id) const {
  if (Id <= 0)
    return nullptr;
  for (const auto &cls : Classes) {
    if (cls.Id == Id && !cls.isRemoved())
      return pClass(&cls);
  }
  return nullptr;
}

pClass oEvent::getClass(const wstring &cname) const {
  for (const auto &cls : Classes) {
    if (!cls.isRemoved() && compareClassName(cname, cls.Name))
      return pClass(&cls);
  }
  return nullptr;
}

pClass oEvent::addClass(const wstring &pname, int CourseId, int classId) {
  if (classId > 0) {
    if (getClass(classId))
      return nullptr;
  }
  oClass c(this, classId);
  c.Name = pname;
  if (CourseId > 0)
    c.Course = getCourse(CourseId);
  Classes.push_back(c);
  Classes.back().addToEvent(this, &c);
  Classes.back().synchronize();
  updateTabs();
  return &Classes.back();
}

pClass oEvent::addClass(const oClass &c) {
  if (c.Id == 0)
    return nullptr;
  if (getClass(c.getId()))
    return nullptr;
  Classes.push_back(c);
  Classes.back().addToEvent(this, &c);
  return &Classes.back();
}

void oEvent::getClasses(vector<pClass> &classes, bool /*sync*/) const {
  classes.clear();
  for (const auto &cls : Classes) {
    if (!cls.isRemoved())
      classes.push_back(pClass(&cls));
  }
  sort(classes.begin(), classes.end(), [](pClass a, pClass b) {
    return a->getSortIndex() < b->getSortIndex() ||
           (a->getSortIndex() == b->getSortIndex() && a->getId() < b->getId());
  });
}

void oEvent::removeClass(int id) {
  pClass pc = getClass(id);
  if (pc) {
    pc->Removed = true;
    sqlClasses.changed = true;
  }
}

bool oEvent::isClassUsed(int /*id*/) const {
  // Stub — requires oRunner/oTeam; deferred to US-003g
  return false;
}

void oEvent::reinitializeClasses() const {
  for (auto &c : Classes)
    c.reinitialize(true);
}

void oEvent::classChanged(oClass * /*cls*/, bool /*doSync*/) {
  // Stub — notification hook; no-op in domain layer
}

int oEvent::getFreeClassId() {
  return ++qFreeClassId;
}

void oEvent::getPredefinedClassTypes(map<wstring, ClassMetaType> &types) const {
  types.clear();
  types[L"Elit"]     = ctElite;
  types[L"Vuxen"]    = ctNormal;
  types[L"Ungdom"]   = ctYouth;
  types[L"Motion"]   = ctExercise;
  types[L"Öppen"]    = ctOpen;
  types[L"Träning"]  = ctTraining;
}

void oEvent::updateTabs() {
  // Stub — no-op in domain layer
}

int oEvent::getMaximalTime() const {
  // Stub
  return 0;
}

void oEvent::reCalculateLeaderTimes(int /*classId*/) {
  // Stub — deferred to US-003g
}

void oEvent::reEvaluateAll(const set<int> & /*classIds*/, bool /*sync*/) {
  // Stub — deferred to US-003g
}

bool oEvent::hasPrevStage() const { return false; }
bool oEvent::hasNextStage() const { return false; }


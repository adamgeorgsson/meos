// oClass.cpp — Domain migration of oClass (US-003e1)
// UI, XML, SQL, result-calculation, and split-analysis methods excluded.

#include "oClass.h"
#include "oEvent.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <sstream>

using namespace std;

// ── StartType / LegType name tables ─────────────────────────────────────────

static const char* StartTypeNames[nStartTypes] = {"ST", "CH", "DR", "HU"};
static const char* LegTypeNames[nLegTypes]     = {"NO", "PA", "EX", "SM",
                                                    "IG", "PO", "GP"};

// ── Utility ──────────────────────────────────────────────────────────────────

// Parse a relative time string "M:SS" or "M:SS.t" into tenths-of-second units.
static int parseRelativeTime(const char* s) {
  if (!s || !*s) return 0;
  // Try M:SS or M:SS.t  (output by codeTime when time has minutes)
  int min = 0, sec = 0, tenths = 0;
  int n = sscanf(s, "%d:%d.%d", &min, &sec, &tenths);
  if (n >= 2) {
    return (min * 60 + sec) * timeConstSecond + tenths;
  }
  // Try S.t  (output by codeTime when time fits in seconds with tenths)
  n = sscanf(s, "%d.%d", &sec, &tenths);
  if (n == 2) {
    return sec * timeConstSecond + tenths;
  }
  // Plain integer: codeTime outputs whole seconds, so convert to tenths
  return atoi(s) * timeConstSecond;
}

// Split string by delimiter into vector<string>
static void splitStr(const string& str, const char* delim,
                     vector<string>& out) {
  out.clear();
  if (str.empty()) return;
  size_t start = 0;
  size_t dlen  = strlen(delim);
  while (true) {
    size_t pos = str.find(delim, start);
    out.push_back(str.substr(start, pos == string::npos ? string::npos : pos - start));
    if (pos == string::npos) break;
    start = pos + dlen;
  }
}

// ── Static DataContainer ─────────────────────────────────────────────────────

/*static*/ oDataContainer& oClass::container() {
  static oDataContainer dc(0);
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    dc.addVariableInt("ExtId",            oDataContainer::oIS64,  "External Id");
    dc.addVariableString("LongName",      32, "Long name");
    dc.addVariableInt("LowAge",           oDataContainer::oIS8,   "Low age");
    dc.addVariableInt("HighAge",          oDataContainer::oIS8,   "High age");
    dc.addVariableInt("HasPool",          oDataContainer::oIS8,   "Course pool");
    dc.addVariableInt("AllowQuickEntry",  oDataContainer::oIS8,   "Quick entry");
    dc.addVariableString("ClassType",     40, "Class type");
    dc.addVariableString("Sex",           4,  "Sex");
    dc.addVariableString("StartName",     16, "Start");
    dc.addVariableInt("StartBlock",       oDataContainer::oIS8,   "Block");
    dc.addVariableInt("NoTiming",         oDataContainer::oIS8,   "No timing");
    dc.addVariableInt("FreeStart",        oDataContainer::oIS8,   "Free start");
    dc.addVariableInt("RequestStart",     oDataContainer::oIS8,   "Request start");
    dc.addVariableInt("IgnoreStart",      oDataContainer::oIS8,   "Ignore start");
    dc.addVariableInt("FirstStart",       oDataContainer::oISTime,"First start");
    dc.addVariableInt("StartInterval",    oDataContainer::oISTime,"Start interval");
    dc.addVariableInt("Vacant",           oDataContainer::oIS8,   "Vacants");
    dc.addVariableInt("Reserved",         oDataContainer::oIS16,  "Reserved");
    dc.addVariableInt("ClassFee",         oDataContainer::oIS32,  "Fee");
    dc.addVariableInt("HighClassFee",     oDataContainer::oIS32,  "High fee");
    dc.addVariableInt("ClassFeeRed",      oDataContainer::oIS32,  "Reduced fee");
    dc.addVariableInt("HighClassFeeRed",  oDataContainer::oIS32,  "High reduced fee");
    dc.addVariableInt("SortIndex",        oDataContainer::oIS32,  "Sort index");
    dc.addVariableInt("MaxTime",          oDataContainer::oISTime,"Max time");
    dc.addVariableString("Status",        4,  "Status");
    dc.addVariableInt("DirectResult",     oDataContainer::oIS8,   "Direct result");
    dc.addVariableString("Bib",           8,  "Bib");
    dc.addVariableString("BibMode",       4,  "Bib mode");
    dc.addVariableInt("Unordered",        oDataContainer::oIS8,   "Unordered");
    dc.addVariableInt("Heat",             oDataContainer::oIS8,   "Heat");
    dc.addVariableInt("Locked",           oDataContainer::oIS8,   "Locked forking");
    dc.addVariableString("Qualification", "Qualification scheme");
    dc.addVariableInt("NumberMaps",       oDataContainer::oIS16,  "Number of maps");
    dc.addVariableString("Result",        24, "Result module");
    dc.addVariableInt("TransferFlags",    oDataContainer::oIS32,  "Transfer flags");
    dc.addVariableInt("DataA",            oDataContainer::oIS32,  "Data A");
    dc.addVariableInt("DataB",            oDataContainer::oIS32,  "Data B");
    dc.addVariableString("TextA",         40, "Text");
    dc.addVariableInt("NoTotalResult",    oDataContainer::oIS8,   "No total result");
  }
  return dc;
}

// ── Constructors ─────────────────────────────────────────────────────────────

oClass::oClass(oEvent* poe) : oBase(poe) {
  getDI().initData();
  Course = nullptr;
  Id = oe->getFreeClassId();
  tNoTiming = -1;
  tIgnoreStartPunch = -1;
  tSortIndex = 0;
  tMaxTime = 0;
}

oClass::oClass(oEvent* poe, int id) : oBase(poe) {
  getDI().initData();
  Course = nullptr;
  if (id == 0)
    id = oe->getFreeClassId();
  Id = id;
  oe->qFreeClassId = max(id % MaxClassId, oe->qFreeClassId);
  tNoTiming = -1;
  tIgnoreStartPunch = -1;
  tSortIndex = 0;
  tMaxTime = 0;
}

// ── oBase virtuals ───────────────────────────────────────────────────────────

oDataContainer& oClass::getDataBuffers(pvoid& data, pvoid& olddata,
                                        pvectorstr& strData) const {
  data    = &dataMap_;
  olddata = &oldDataMap_;
  strData = nullptr;
  return container();
}

wstring oClass::getInfo() const {
  return L"Klass " + Name;
}

void oClass::changedObject() {
  if (oe) {
    oe->globalModification = true;
    oe->sqlClasses.changed = true;
  }
}

void oClass::remove() {
  Removed = true;
}

bool oClass::canRemove() const {
  return true; // Full dependency check (runners/teams) deferred to later story.
}

void oClass::clearDuplicate() {
  int id = oe->getFreeClassId();
  clearDuplicateBase(id);
  oe->qFreeClassId = max(id % MaxClassId, oe->qFreeClassId);
  getDI().setInt("SortIndex", tSortIndex);
}

// ── Name ─────────────────────────────────────────────────────────────────────

void oClass::setName(const wstring& name, bool /*manualSet*/) {
  if (Name != name) {
    Name = name;
    updateChanged();
  }
}

const wstring& oClass::getLongName() const {
  const wstring& s = getDCI().getString("LongName");
  return s.empty() ? Name : s;
}

void oClass::setLongName(const wstring& name) {
  if (getDCI().getString("LongName") != name) {
    getDI().setString("LongName", name);
    updateChanged();
  }
}

// ── Course assignment ─────────────────────────────────────────────────────────

pCourse oClass::getCourse(bool /*getSampleFromRunner*/) const {
  return Course;
}

pCourse oClass::getCourse(int leg, unsigned fork,
                           bool /*getSampleFromRunner*/) const {
  if (MultiCourse.empty())
    return Course;
  if (leg < 0 || leg >= (int)MultiCourse.size())
    return Course;
  const auto& stageCourses = MultiCourse[leg];
  if (stageCourses.empty())
    return nullptr;
  unsigned idx = fork % (unsigned)stageCourses.size();
  return stageCourses[idx];
}

void oClass::setCourse(pCourse c) {
  if (Course != c) {
    Course = c;
    updateChanged();
  }
}

// ── Multi-course management ──────────────────────────────────────────────────

void oClass::setNumStages(int no) {
  int current = (int)MultiCourse.size();
  if (no == current && (int)legInfo.size() == no) return;
  MultiCourse.resize(no);
  // Only initialize legInfo slots that are genuinely new (beyond existing legInfo)
  int legCurrent = (int)legInfo.size();
  legInfo.resize(no);
  if (no > legCurrent) {
    for (int k = legCurrent; k < no; ++k)
      legInfo[k] = oLegInfo(); // default: STTime, LTNormal
  }
  updateChanged();
}

bool oClass::addStageCourse(int stage, int courseId, int index) {
  if (!oe) return false;
  pCourse pc = oe->getCourse(courseId);
  return addStageCourse(stage, pc, index);
}

bool oClass::addStageCourse(int stage, pCourse pc, int index) {
  if (stage < 0) return false;
  if (stage >= (int)MultiCourse.size())
    setNumStages(stage + 1);
  auto& stageCourses = MultiCourse[stage];
  if (index < 0 || index >= (int)stageCourses.size())
    stageCourses.push_back(pc);
  else
    stageCourses.insert(stageCourses.begin() + index, pc);
  updateChanged();
  return true;
}

void oClass::clearStageCourses(int stage) {
  if (stage >= 0 && stage < (int)MultiCourse.size()) {
    MultiCourse[stage].clear();
    updateChanged();
  }
}

bool oClass::removeStageCourse(int stage, int courseId, int position) {
  if (stage < 0 || stage >= (int)MultiCourse.size()) return false;
  auto& sc = MultiCourse[stage];
  for (int i = 0; i < (int)sc.size(); ++i) {
    if (sc[i] && sc[i]->getId() == courseId) {
      if (position < 0 || position == i) {
        sc.erase(sc.begin() + i);
        updateChanged();
        return true;
      }
    }
  }
  return false;
}

bool oClass::moveStageCourse(int stage, int index, int offset) {
  if (stage < 0 || stage >= (int)MultiCourse.size()) return false;
  auto& sc = MultiCourse[stage];
  int newPos = index + offset;
  if (index < 0 || index >= (int)sc.size()) return false;
  if (newPos < 0 || newPos >= (int)sc.size()) return false;
  auto pc = sc[index];
  sc.erase(sc.begin() + index);
  sc.insert(sc.begin() + newPos, pc);
  updateChanged();
  return true;
}

void oClass::getCourses(int leg, vector<pCourse>& courses) const {
  courses.clear();
  if (leg < 0 || leg >= (int)MultiCourse.size()) return;
  courses = MultiCourse[leg];
}

bool oClass::isForked(int leg) const {
  if (leg < 0 || leg >= (int)MultiCourse.size()) return false;
  return MultiCourse[leg].size() > 1;
}

bool oClass::isCourseUsed(int id) const {
  if (Course && Course->getId() == id) return true;
  for (auto& stage : MultiCourse)
    for (auto pc : stage)
      if (pc && pc->getId() == id) return true;
  return false;
}

bool oClass::hasTrueMultiCourse() const {
  if (MultiCourse.empty()) return false;
  for (auto& stage : MultiCourse)
    if (stage.size() > 1) return true;
  return false;
}

set<int>& oClass::getMCourseIdSet(set<int>& in) const {
  in.clear();
  for (auto& stage : MultiCourse)
    for (auto pc : stage)
      if (pc) in.insert(pc->getId());
  return in;
}

// ── Course coding / import ───────────────────────────────────────────────────

string oClass::codeMultiCourseStr() const {
  string str;
  char bf[16];
  for (const auto& stage : MultiCourse) {
    for (auto pc : stage) {
      if (pc)
        snprintf(bf, sizeof(bf), " %d", pc->getId());
      else
        strcpy(bf, " 0");
      str += bf;
    }
    str += ";";
  }
  if (str.size() == 1)
    return "@";
  if (!str.empty())
    return str.substr(0, str.size() - 1);
  return "";
}

string oClass::codeLegMethod() const {
  string code;
  for (size_t k = 0; k < legInfo.size(); ++k) {
    if (k > 0) code += "*";
    code += legInfo[k].codeLegMethod();
  }
  return code;
}

void oClass::importLegMethod(const string& legMethods) {
  vector<string> parts;
  splitStr(legMethods, "*", parts);
  legInfo.clear();
  for (auto& p : parts) {
    oLegInfo oli;
    oli.importLegMethod(p);
    legInfo.push_back(oli);
  }
  // Validate duplicateRunner references
  for (size_t k = 0; k < legInfo.size(); ++k) {
    if (legInfo[k].duplicateRunner != -1) {
      if ((unsigned)legInfo[k].duplicateRunner < legInfo.size())
        legInfo[legInfo[k].duplicateRunner].duplicateRunner = -1;
      else
        legInfo[k].duplicateRunner = -1;
    }
  }
  setNumStages((int)legInfo.size());
}

void oClass::importCourses(const vector<vector<int>>& multi) {
  MultiCourse.resize(multi.size());
  for (size_t k = 0; k < multi.size(); ++k) {
    MultiCourse[k].resize(multi[k].size());
    for (size_t j = 0; j < multi[k].size(); ++j)
      MultiCourse[k][j] = oe ? oe->getCourse(multi[k][j]) : nullptr;
  }
  setNumStages((int)MultiCourse.size());
}

/*static*/ void oClass::parseCourses(const string& courses,
                                      vector<vector<int>>& multi,
                                      set<int>& courseId) {
  courseId.clear();
  multi.clear();
  if (courses.empty()) return;

  const char* str = courses.c_str();
  while (*str && isspace((unsigned char)*str)) ++str;

  multi.push_back({});
  int n_stage = 0;

  while (*str) {
    int cid = atoi(str);
    if (cid) {
      multi[n_stage].push_back(cid);
      courseId.insert(cid);
    }
    while (*str && *str != ';' && *str != ' ') ++str;
    if (*str == ';') {
      ++str;
      while (*str && *str == ' ') ++str;
      ++n_stage;
      multi.push_back({});
    } else if (*str) {
      ++str;
    }
  }
}

// ── Leg info ─────────────────────────────────────────────────────────────────

StartTypes oClass::getStartType(int leg) const {
  if (leg < 0 || leg >= (int)legInfo.size()) return STTime;
  return legInfo[leg].startMethod;
}

LegTypes oClass::getLegType(int leg) const {
  if (leg < 0 || leg >= (int)legInfo.size()) return LTNormal;
  return legInfo[leg].legMethod;
}

int oClass::getStartData(int leg) const {
  if (leg < 0 || leg >= (int)legInfo.size()) return 0;
  return legInfo[leg].legStartData;
}

int oClass::getRestartTime(int leg) const {
  if (leg < 0 || leg >= (int)legInfo.size()) return 0;
  return legInfo[leg].legRestartTime;
}

int oClass::getRopeTime(int leg) const {
  if (leg < 0 || leg >= (int)legInfo.size()) return 0;
  return legInfo[leg].legRopeTime;
}

void oClass::setStartType(int leg, StartTypes st, bool /*noThrow*/) {
  if (leg < 0 || leg >= (int)legInfo.size()) return;
  legInfo[leg].startMethod = st;
  updateChanged();
}

void oClass::setLegType(int leg, LegTypes lt) {
  if (leg < 0 || leg >= (int)legInfo.size()) return;
  legInfo[leg].legMethod = lt;
  updateChanged();
}

bool oClass::setStartData(int leg, int value) {
  if (leg < 0 || leg >= (int)legInfo.size()) return false;
  legInfo[leg].legStartData = value;
  updateChanged();
  return true;
}

void oClass::setRestartTime(int leg, const wstring& t) {
  if (leg < 0 || leg >= (int)legInfo.size()) return;
  legInfo[leg].legRestartTime = convertAbsoluteTimeHMS(t, -1);
  updateChanged();
}

void oClass::setRopeTime(int leg, const wstring& t) {
  if (leg < 0 || leg >= (int)legInfo.size()) return;
  legInfo[leg].legRopeTime = convertAbsoluteTimeHMS(t, -1);
  updateChanged();
}

int oClass::getLegRunner(int leg) const {
  if (leg < 0 || leg >= (int)legInfo.size()) return leg;
  int dr = legInfo[leg].duplicateRunner;
  return (dr != -1) ? dr : leg;
}

int oClass::getLegRunnerIndex(int leg) const {
  if (leg < 0 || leg >= (int)legInfo.size()) return 0;
  if (legInfo[leg].duplicateRunner == -1) return 0;
  // Count how many times this runner appears before this leg
  int base = getLegRunner(leg);
  int idx = 0;
  for (int k = 0; k <= leg; ++k)
    if (getLegRunner(k) == base) ++idx;
  return idx - 1;
}

void oClass::setLegRunner(int leg, int runnerNo) {
  if (leg < 0 || leg >= (int)legInfo.size()) return;
  legInfo[leg].duplicateRunner = runnerNo;
  updateChanged();
}

int oClass::getNumMultiRunners(int leg) const {
  if (leg < 0 || leg >= (int)legInfo.size()) return 1;
  int base = getLegRunner(leg);
  int count = 0;
  for (int k = 0; k < (int)legInfo.size(); ++k)
    if (getLegRunner(k) == base) ++count;
  return count;
}

int oClass::getNumLegNoParallel() const {
  int count = 0;
  for (auto& li : legInfo)
    if (!li.isParallel()) ++count;
  return count;
}

int oClass::getNumParallel(int leg) const {
  if (leg < 0 || leg >= (int)legInfo.size()) return 1;
  // Count consecutive parallel legs around this leg
  // (simplified: count block of parallels including this leg)
  int start = leg;
  while (start > 0 && legInfo[start].isParallel()) --start;
  if (!legInfo[start].isParallel()) {}
  int count = 0;
  for (int k = start; k < (int)legInfo.size(); ++k) {
    if (k == start || legInfo[k].isParallel()) ++count;
    else break;
  }
  return count;
}

int oClass::getNextBaseLeg(int leg) const {
  for (int k = leg + 1; k < (int)legInfo.size(); ++k)
    if (!legInfo[k].isParallel()) return k;
  return (int)legInfo.size();
}

int oClass::getPreceedingLeg(int leg) const {
  for (int k = leg - 1; k >= 0; --k)
    if (!legInfo[k].isParallel()) return k;
  return -1;
}

int oClass::getResultDefining(int leg) const {
  int result = leg;
  for (int k = leg + 1; k < (int)legInfo.size(); ++k) {
    if (legInfo[k].isParallel()) result = k;
    else break;
  }
  return result;
}

bool oClass::splitLegNumberParallel(int leg, int& legNumber,
                                     int& legOrder) const {
  if (leg < 0 || leg >= (int)legInfo.size()) {
    legNumber = leg; legOrder = 0; return false;
  }
  // Walk back to find the base (non-parallel) leg
  int base = leg;
  while (base > 0 && legInfo[base].isParallel()) --base;
  legNumber = base;
  legOrder  = leg - base;
  return legInfo[leg].isParallel();
}

int oClass::getLegNumberLinear(int legNumber, int legOrder) const {
  if (legNumber < 0 || legNumber >= (int)legInfo.size()) return -1;
  int target = legNumber + legOrder;
  if (target < (int)legInfo.size()) return target;
  return -1;
}

wstring oClass::getLegNumber(int leg) const {
  if ((int)legInfo.size() <= 1) return L"";
  int legNumber, legOrder;
  splitLegNumberParallel(leg, legNumber, legOrder);
  wstring s = itow(legNumber + 1);
  if (legOrder > 0)
    s += wchar_t(L'a' + legOrder - 1);
  return s;
}

int oClass::getNumDistinctRunners() const {
  if (legInfo.empty()) return 1;
  set<int> runners;
  for (int k = 0; k < (int)legInfo.size(); ++k)
    runners.insert(getLegRunner(k));
  return (int)runners.size();
}

int oClass::getNumDistinctRunnersMinimal() const {
  return getNumDistinctRunners();
}

bool oClass::isSingleRunnerMultiStage() const {
  unsigned ns = getNumStages();
  if (ns < 2) return false;
  return getNumDistinctRunners() == 1;
}

// ── Timing flags ─────────────────────────────────────────────────────────────

void oClass::setNoTiming(bool noTiming) {
  getDI().setInt("NoTiming", noTiming ? 1 : 0);
  tNoTiming = noTiming ? 1 : 0;
  updateChanged();
}

bool oClass::getNoTiming() const {
  if (tNoTiming == -1)
    tNoTiming = getDCI().getInt("NoTiming");
  return tNoTiming != 0;
}

void oClass::setIgnoreStartPunch(bool ignore) {
  getDI().setInt("IgnoreStart", ignore ? 1 : 0);
  tIgnoreStartPunch = ignore ? 1 : 0;
  updateChanged();
}

bool oClass::ignoreStartPunch() const {
  if (tIgnoreStartPunch == -1)
    tIgnoreStartPunch = getDCI().getInt("IgnoreStart");
  return tIgnoreStartPunch != 0;
}

bool oClass::hasFreeStart() const {
  return getDCI().getInt("FreeStart") != 0;
}

void oClass::setFreeStart(bool freeStart) {
  getDI().setInt("FreeStart", freeStart ? 1 : 0);
  updateChanged();
}

bool oClass::hasRequestStart() const {
  return getDCI().getInt("RequestStart") != 0;
}

void oClass::setRequestStart(bool reqStart) {
  getDI().setInt("RequestStart", reqStart ? 1 : 0);
  updateChanged();
}

bool oClass::hasDirectResult() const {
  return getDCI().getInt("DirectResult") != 0;
}

void oClass::setDirectResult(bool directResult) {
  getDI().setInt("DirectResult", directResult ? 1 : 0);
  updateChanged();
}

// ── ClassType / status ───────────────────────────────────────────────────────

ClassType oClass::getClassType() const {
  unsigned ns = getNumStages();
  if (ns == 0) return oClassIndividual;
  if (getNumDistinctRunners() == 1) {
    if (ns > 1) return oClassIndividRelay;
    return oClassIndividual;
  }
  return oClassRelay;
}

oClass::ClassStatus oClass::getClassStatus() const {
  return ClassStatus::Normal; // Full implementation deferred.
}

bool oClass::isTeamClass() const {
  unsigned ns = getNumStages();
  return ns > 0 && getNumDistinctRunners() > 1;
}

bool oClass::isRogaining() const {
  // Simplified: check if the assigned course has rogaining time
  if (Course && Course->getMaximumRogainingTime() > 0)
    return true;
  if (!MultiCourse.empty() && !MultiCourse[0].empty() &&
      MultiCourse[0][0] && MultiCourse[0][0]->getMaximumRogainingTime() > 0)
    return true;
  return false;
}

// ── oLegInfo methods ─────────────────────────────────────────────────────────

string oLegInfo::codeLegMethod() const {
  char bsd[16], bret[16], brot[16];

  auto codeTime = [](int t, char* b) -> const char* {
    if (t <= 0) {
      snprintf(b, 16, "%d", t);
    } else {
      int sec    = t / timeConstSecond;
      int tenths = t % timeConstSecond;
      if (tenths)
        snprintf(b, 16, "%d.%d", sec, tenths);
      else
        snprintf(b, 16, "%d", sec);
    }
    return b;
  };

  char bf[256];
  if (isStartDataTime()) {
    snprintf(bf, sizeof(bf), "(%s:%s:%s:%s:%s:%d)",
             StartTypeNames[startMethod], LegTypeNames[legMethod],
             codeTime(legStartData, bsd),
             codeTime(legRestartTime, bret),
             codeTime(legRopeTime, brot),
             duplicateRunner);
  } else {
    snprintf(bf, sizeof(bf), "(%s:%s:%d:%s:%s:%d)",
             StartTypeNames[startMethod], LegTypeNames[legMethod],
             legStartData,
             codeTime(legRestartTime, bret),
             codeTime(legRopeTime, brot),
             duplicateRunner);
  }
  return bf;
}

void oLegInfo::importLegMethod(const string& leg) {
  startMethod    = STTime;
  legMethod      = LTNormal;
  legStartData   = 0;
  legRestartTime = 0;
  legRopeTime    = 0;

  size_t begin = leg.find_first_of('(');
  if (begin == string::npos) return;
  ++begin;
  size_t end = leg.find_first_of(')');
  if (end == string::npos || end < begin) return;

  string core = leg.substr(begin, end - begin);
  vector<string> parts;
  splitStr(core, ":", parts);

  if (parts.size() >= 1) {
    for (int st = 0; st < nStartTypes; ++st)
      if (parts[0] == StartTypeNames[st]) { startMethod = (StartTypes)st; break; }
  }
  if (parts.size() >= 2) {
    for (int t = 0; t < nLegTypes; ++t)
      if (parts[1] == LegTypeNames[t]) { legMethod = (LegTypes)t; break; }
  }
  if (parts.size() >= 3) {
    if (isStartDataTime())
      legStartData = parseRelativeTime(parts[2].c_str());
    else
      legStartData = atoi(parts[2].c_str());
  }
  if (parts.size() >= 4) legRestartTime = parseRelativeTime(parts[3].c_str());
  if (parts.size() >= 5) legRopeTime    = parseRelativeTime(parts[4].c_str());
  if (parts.size() >= 6) duplicateRunner = atoi(parts[5].c_str());
}

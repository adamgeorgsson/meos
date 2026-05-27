// oCourse.cpp — domain migration of oCourse (no UI/XML/oCard methods)

#include "oCourse.h"
#include "oEvent.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <set>
#include <stdexcept>

using namespace std;

// ---------------------------------------------------------------------------
// Data container
// ---------------------------------------------------------------------------

oDataContainer& oCourse::container() {
  static oDataContainer dc(128);
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    dc.addVariableInt("FirstAsStart",   oDataContainer::oIS8,  "FirstAsStart");
    dc.addVariableInt("LastAsFinish",   oDataContainer::oIS8,  "LastAsFinish");
    dc.addVariableInt("NumberMaps",     oDataContainer::oIS32, "NumberMaps");
    dc.addVariableInt("RPointLimit",    oDataContainer::oIS32, "RPointLimit");
    dc.addVariableInt("RTimeLimit",     oDataContainer::oISTime, "RTimeLimit");
    dc.addVariableInt("RReduction",     oDataContainer::oIS32, "RReduction");
    dc.addVariableInt("RReductionMethod", oDataContainer::oIS8, "RReductionMethod");
    dc.addVariableInt("CControl",       oDataContainer::oIS32, "CControl");
    dc.addVariableInt("Shorten",        oDataContainer::oIS32, "Shorten");
    dc.addVariableString("StartName",   "StartName");
  }
  return dc;
}

oDataContainer& oCourse::getDataBuffers(pvoid& data, pvoid& olddata,
                                         pvectorstr& strData) const {
  data = &dataMap_;
  olddata = &oldDataMap_;
  strData = nullptr;
  return container();
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

oCourse::oCourse(oEvent* poe) : oBase(poe) {
  getDI().initData();
  clearCache();
  Id = oe->getFreeCourseId();
}

oCourse::oCourse(oEvent* poe, int id) : oBase(poe) {
  getDI().initData();
  clearCache();
  if (id == 0)
    id = oe->getFreeCourseId();
  Id = id;
}

oCourse::~oCourse() = default;

// ---------------------------------------------------------------------------
// Info / changed
// ---------------------------------------------------------------------------

wstring oCourse::getInfo() const {
  return L"Bana " + name;
}

void oCourse::changedObject() {
  if (oe) {
    oe->globalModification = true;
    oe->sqlCourses.changed = true;
  }
}

void oCourse::clearCache() const {
  cachedHasRogaining = 0;
  cachedControlOrdinal.clear();
  cacheDataRevision = oe ? oe->dataRevision : -1;
  if (oe) {
    oe->tCalcNumMapsDataRevision = -1;
  }
  tMapsUsed = -1;
  tMapsUsedNoVacant = -1;
}

// ---------------------------------------------------------------------------
// Name / length
// ---------------------------------------------------------------------------

void oCourse::setName(const wstring& n) {
  if (name != n) {
    name = n;
    updateChanged();
  }
}

void oCourse::setLength(int le) {
  if (le < 0 || le > 1000000)
    le = 0;
  if (length != le) {
    length = le;
    updateChanged();
  }
}

wstring oCourse::getLengthS() const {
  return itow(length);
}

// ---------------------------------------------------------------------------
// Control management
// ---------------------------------------------------------------------------

oControl* oCourse::doAddControl(int id) {
  oControl* c = oe->getControl(id, true, false);
  if (!c)
    throw std::runtime_error("Invalid control id");
  controls.push_back(c);
  return c;
}

oControl* oCourse::addControl(int id) {
  oControl* c = doAddControl(id);
  updateChanged();
  return c;
}

oControl* oCourse::getControl(int index) const {
  if (index >= 0 && index < nControls())
    return controls[index];
  return nullptr;
}

void oCourse::getControls(vector<oControl*>& pc) const {
  pc = controls;
}

vector<int> oCourse::getControlNumbers() const {
  vector<int> ret;
  for (int k = 0; k < nControls(); k++)
    ret.push_back(controls[k]->getFirstNumber());
  return ret;
}

string oCourse::getControls() const {
  string str;
  char bf[16];
  for (int m = 0; m < nControls(); m++) {
    snprintf(bf, sizeof(bf), "%d;", controls[m]->getId());
    str += bf;
  }
  return str;
}

void oCourse::splitControls(const string& ctrls, vector<int>& nr) {
  const char* str = ctrls.c_str();
  nr.clear();
  while (*str) {
    int cid = atoi(str);
    while (*str && (*str != ';' && *str != ',' && *str != ' ')) str++;
    while (*str && (*str == ';' || *str == ',' || *str == ' ')) str++;
    if (cid > 0)
      nr.push_back(cid);
  }
}

bool oCourse::importControls(const string& ctrls, bool setChanged,
                              bool updateLegLengths) {
  vector<int> oldC;
  for (int k = 0; k < nControls(); k++)
    oldC.push_back(controls[k] ? controls[k]->getId() : 0);

  controls.clear();
  vector<int> newC;
  splitControls(ctrls, newC);
  for (size_t k = 0; k < newC.size(); k++)
    doAddControl(newC[k]);

  bool changed = nControls() != (int)oldC.size();

  if (changed && updateLegLengths && !legLengths.empty()) {
    int oldIndex = 0;
    int newIndex = 0;
    vector<int> newLen(nControls() + 1);
    bool lastOK = true;
    while (newIndex < nControls()) {
      if (oldIndex < (int)oldC.size()) {
        if (oldC[oldIndex] == newC[newIndex]) {
          if (lastOK && oldIndex < (int)legLengths.size())
            newLen[newIndex] = legLengths[oldIndex];
          lastOK = true;
          oldIndex++;
        } else {
          lastOK = false;
          int forward = oldIndex + 1;
          while (forward < (int)oldC.size()) {
            if (oldC[forward] == newC[newIndex]) {
              oldIndex = forward + 1;
              lastOK = true;
              break;
            }
            forward++;
          }
        }
      } else {
        lastOK = false;
      }
      newIndex++;
    }
    if (lastOK)
      newLen.back() = legLengths.back();
    swap(newLen, legLengths);
  }

  for (int k = 0; !changed && k < nControls(); k++)
    changed |= oldC[k] != controls[k]->getId();

  if (changed) {
    if (setChanged)
      updateChanged();
    oe->punchIndex.clear();
  }
  return changed;
}

void oCourse::importLegLengths(const string& legs, bool setChanged) {
  // Split on ";"
  vector<string> splits;
  const char* p = legs.c_str();
  while (*p) {
    const char* start = p;
    while (*p && *p != ';') p++;
    splits.emplace_back(start, p);
    if (*p == ';') p++;
  }

  bool changed = false;
  if (legLengths.size() != splits.size()) {
    legLengths.resize(splits.size());
    changed = true;
  }
  for (size_t k = 0; k < legLengths.size(); k++) {
    int val = atoi(splits[k].c_str());
    if (legLengths[k] != val)
      changed = true;
    legLengths[k] = val;
  }
  if (changed && setChanged)
    updateChanged();
}

// ---------------------------------------------------------------------------
// Leg lengths
// ---------------------------------------------------------------------------

int oCourse::getLegLength(int index) const {
  if ((size_t)index < legLengths.size())
    return legLengths[index];
  return 0;
}

void oCourse::setLegLengths(const vector<int>& legs) {
  if (legs.size() == (size_t)(nControls() + 1) || legs.empty()) {
    bool diff = legs.size() != legLengths.size();
    if (!diff) {
      for (size_t k = 0; k < legs.size(); k++)
        if (legs[k] != legLengths[k])
          diff = true;
    }
    if (diff) {
      updateChanged();
      legLengths = legs;
    }
  } else {
    throw std::runtime_error("Invalid parameter value");
  }
}

string oCourse::getLegLengths() const {
  string str;
  for (size_t m = 0; m < legLengths.size(); m++) {
    if (m > 0)
      str += ";";
    char bf[16];
    snprintf(bf, sizeof(bf), "%d", legLengths[m]);
    str += bf;
  }
  return str;
}

// ---------------------------------------------------------------------------
// Start/finish control
// ---------------------------------------------------------------------------

const wstring& oCourse::getStart() const {
  return getDCI().getString("StartName");
}

bool oCourse::setStartFinish(oControl* startC, oControl* finishC,
                              bool updateStatus) {
  if (startC != start || finishC != finish) {
    start = startC;
    finish = finishC;
    if (updateStatus)
      updateChanged();
    return true;
  }
  return false;
}

bool oCourse::setStartFinishId(int startId, int finishId, bool updateStatus) {
  oControl* s = startId ? oe->getControl(startId, false, false) : nullptr;
  oControl* f = finishId ? oe->getControl(finishId, false, false) : nullptr;
  return setStartFinish(s, f, updateStatus);
}

bool oCourse::useFirstAsStart() const {
  return getDCI().getInt("FirstAsStart") != 0;
}

bool oCourse::useLastAsFinish() const {
  return getDCI().getInt("LastAsFinish") != 0;
}

void oCourse::firstAsStart(bool f) {
  getDI().setInt("FirstAsStart", f ? 1 : 0);
}

void oCourse::lastAsFinish(bool f) {
  getDI().setInt("LastAsFinish", f ? 1 : 0);
}

int oCourse::getStartPunchType() const {
  if (useFirstAsStart() && nControls() > 0)
    return controls[0]->getFirstNumber();
  return oPunch::PunchStart;
}

int oCourse::getFinishPunchType() const {
  if (useLastAsFinish() && nControls() > 0)
    return controls.back()->getFirstNumber();
  return oPunch::PunchFinish;
}

// ---------------------------------------------------------------------------
// Common control / loops
// ---------------------------------------------------------------------------

int oCourse::getCommonControl() const {
  return getDCI().getInt("CControl");
}

void oCourse::setCommonControl(int ctrlId) {
  if (ctrlId != 0) {
    int found = 0;
    for (int k = 0; k < nControls(); k++)
      if (controls[k]->getId() == ctrlId)
        found++;
    if (found == 0)
      throw std::runtime_error("Control not on course");
  }
  getDI().setInt("CControl", ctrlId);
}

int oCourse::getNumLoops() const {
  int cc = getCommonControl();
  if (cc == 0) return 0;
  bool wasCC = true;
  int loopCount = 0;
  for (int i = 0; i < nControls(); i++) {
    if (controls[i]->getId() == cc)
      wasCC = true;
    else if (wasCC) {
      loopCount++;
      wasCC = false;
    }
  }
  return loopCount;
}

int oCourse::matchLoopKey(const vector<int>& punches,
                          const vector<oControl*>& key) {
  if (key.empty()) return 999;
  size_t ix = (size_t)-1;
  for (size_t k = 0; k < key.size(); k++) {
    int code = key[k]->getFirstNumber();
    while (++ix < punches.size()) {
      if (punches[ix] == code) {
        code = -1;
        break;
      }
    }
    if (code != -1) return 1000;
  }
  return (int)ix;
}

bool oCourse::constructLoopKeys(int cc,
                                 vector<vector<oControl*>>& loopKeys,
                                 vector<int>& ccIndex) const {
  bool firstAsStart = useFirstAsStart();
  if (firstAsStart) {
    for (int k = 1; k < nControls(); k++) {
      if (controls[k] == controls[0]) {
        firstAsStart = false;
        break;
      }
    }
  }
  if (nControls() > 0 && controls[0]->getId() == cc)
    firstAsStart = true;

  bool lastAsFinish = useLastAsFinish();
  if (lastAsFinish) {
    for (int k = 0; k < nControls() - 1; k++) {
      if (controls[k] == controls.back()) {
        lastAsFinish = false;
        break;
      }
    }
  }

  int startIx = firstAsStart ? 1 : 0;
  int endIx   = lastAsFinish ? nControls() : nControls() - 1;

  ccIndex.push_back(startIx - 1);
  for (int k = startIx; k < endIx; k++) {
    if (controls[k]->getId() == cc)
      ccIndex.push_back(k);
  }
  if ((int)ccIndex.size() <= 1) return false;

  loopKeys.clear();
  loopKeys.resize(ccIndex.size());

  int keyIndex = 1;
  bool changed = true;
  bool enough = false;
  while (changed && !enough) {
    changed = false;
    for (size_t k = 0; k < ccIndex.size(); k++) {
      int keyIx = ccIndex[k] + keyIndex;
      int nextIx = (k + 1 < ccIndex.size()) ? ccIndex[k + 1] : nControls();
      if (keyIx < nextIx && controls[keyIx]->isSingleStatusOK() &&
          controls[keyIx]->getNNumbers() == 1) {
        loopKeys[k].push_back(controls[keyIx]);
        changed = true;
      }
    }
    keyIndex++;
    if (changed) {
      enough = false;
      set<int64_t> hashes;
      for (size_t k = 0; k < loopKeys.size(); k++) {
        int64_t h = (int64_t)loopKeys[k].size();
        for (size_t j = 0; j < loopKeys[k].size(); j++) {
          int num = 0;
          vector<int> nums;
          loopKeys[k][j]->getNumbers(nums);
          if (!nums.empty()) num = nums[0];
          h = h * 997 + num;
        }
        hashes.insert(h);
      }
      enough = hashes.size() == loopKeys.size();
    }
  }
  return enough;
}

// ---------------------------------------------------------------------------
// Rogaining
// ---------------------------------------------------------------------------

void oCourse::setRogainingPointsPerMinute(int p) {
  getDI().setInt("RReduction", p);
}

int oCourse::getRogainingPointsPerMinute() const {
  return getDCI().getInt("RReduction");
}

int oCourse::calculateReduction(int overTime) const {
  if (overTime <= 0) return 0;
  int method = getDCI().getInt("RReductionMethod");
  if (method == 0)
    return (59 * timeConstSecond + overTime * getRogainingPointsPerMinute()) /
           timeConstMinute;
  else
    return ((59 * timeConstSecond + overTime) / timeConstMinute) *
           getRogainingPointsPerMinute();
}

void oCourse::setMinimumRogainingPoints(int p) {
  cachedHasRogaining = 0;
  getDI().setInt("RPointLimit", p);
}

int oCourse::getMinimumRogainingPoints() const {
  return getDCI().getInt("RPointLimit");
}

void oCourse::setMaximumRogainingTime(int t) {
  cachedHasRogaining = 0;
  if (t == NOTIME) t = 0;
  getDI().setInt("RTimeLimit", t);
}

int oCourse::getMaximumRogainingTime() const {
  return getDCI().getInt("RTimeLimit");
}

bool oCourse::hasRogaining() const {
  if (oe && oe->dataRevision != cacheDataRevision)
    clearCache();
  if (cachedHasRogaining > 0)
    return cachedHasRogaining == 2;
  bool r = getMaximumRogainingTime() > 0 || getMinimumRogainingPoints() > 0;
  cachedHasRogaining = r ? 2 : 1;
  return r;
}

// ---------------------------------------------------------------------------
// Control ordinal / part of course
// ---------------------------------------------------------------------------

const wstring& oCourse::getControlOrdinal(int controlIndex) const {
  static thread_local wstring tl_finish;
  tl_finish = L"Mål";
  if ((controlIndex + 1 == nControls() && useLastAsFinish()) ||
      controlIndex == nControls())
    return tl_finish;

  if (oe && oe->dataRevision != cacheDataRevision)
    clearCache();

  if ((size_t)controlIndex < cachedControlOrdinal.size() &&
      !cachedControlOrdinal[controlIndex].empty())
    return cachedControlOrdinal[controlIndex];

  if (controlIndex > nControls())
    throw std::runtime_error("Invalid index");
  cachedControlOrdinal.resize(nControls());

  int o = useFirstAsStart() ? 0 : 1;
  bool rogaining = hasRogaining();
  for (int k = 0; k < controlIndex && k < nControls(); k++) {
    if (controls[k] && !controls[k]->isRogaining(rogaining))
      o++;
  }
  cachedControlOrdinal[controlIndex] = itow(o);
  return cachedControlOrdinal[controlIndex];
}

double oCourse::getPartOfCourse(int start, int end) const {
  if (end == 0)
    end = nControls();
  if ((int)legLengths.size() != nControls() + 1 || start <= end ||
      (unsigned)start >= legLengths.size() ||
      (unsigned)end >= legLengths.size() || !(length > 0))
    return 0.0;
  int dist = 0;
  for (int k = start; k < end; k++)
    dist += legLengths[k];
  return std::max(1.0, double(dist) / double(length));
}

// ---------------------------------------------------------------------------
// Maps
// ---------------------------------------------------------------------------

void oCourse::setNumberMaps(int nm) {
  getDI().setInt("NumberMaps", nm);
}

int oCourse::getNumberMaps() const {
  return getDCI().getInt("NumberMaps");
}

int oCourse::getNumUsedMaps(bool noVacant) const {
  if (tMapsUsed < 0) return 0;
  return noVacant ? tMapsUsedNoVacant : tMapsUsed;
}

// ---------------------------------------------------------------------------
// Identity sum
// ---------------------------------------------------------------------------

int oCourse::getIdSum(int nC) {
  int id = 0;
  for (int k = 0; k < std::min(nC, nControls()); k++)
    id = 431 * id + (controls[k] ? controls[k]->getId() : 0);
  if (id == 0) return getId();
  return id;
}

// ---------------------------------------------------------------------------
// Control checks / ids
// ---------------------------------------------------------------------------

bool oCourse::hasControl(const oControl* ctrl) const {
  for (int i = 0; i < nControls(); i++)
    if (controls[i] == ctrl) return true;
  if (finish == ctrl) return true;
  if (start == ctrl) return true;
  return false;
}

bool oCourse::hasControlCode(int code) const {
  for (int i = 0; i < nControls(); i++)
    if (controls[i]->hasNumber(code)) return true;
  return false;
}

int oCourse::getCourseControlId(int controlIx) const {
  if (controlIx >= nControls()) return -1;
  int id = controls[controlIx] ? controls[controlIx]->getId() : 0;
  if (id == 0) return 0;
  int count = 0;
  for (int j = 0; j < controlIx; j++) {
    if (controls[j] && controls[j]->getId() == id)
      count++;
  }
  return oControl::getCourseControlIdFromIdIndex(id, count);
}

wstring oCourse::getRadioName(int courseControlId) const {
  pair<int, int> idix = oControl::getIdIndexFromCourseControlId(courseControlId);
  oControl* pc = nullptr;
  int numRadio = 0;
  int clsix = 1;
  for (int k = 0; k < nControls(); k++) {
    if (controls[k]) {
      if (controls[k]->isValidRadio())
        numRadio++;
      if (controls[k]->getId() == idix.first) {
        if (idix.second == 0) {
          pc = controls[k];
          break;
        } else {
          clsix++;
          idix.second--;
        }
      }
    }
  }
  if (!pc) return L"?";

  wstring nm;
  if (pc->hasName()) {
    nm = pc->getName();
    if (pc->getNumberDuplicates() > 1)
      nm += makeDash(L"-" + itow(clsix));
  } else {
    nm = L"Radio " + itow(numRadio);
  }
  return nm;
}

// ---------------------------------------------------------------------------
// Shorter version / adapted
// ---------------------------------------------------------------------------

pair<bool, oCourse*> oCourse::getShorterVersion() const {
  int ix = getDCI().getInt("Shorten");
  if (ix == -1)
    return {true, nullptr};
  return {ix != 0, nullptr}; // Full lookup requires oEvent course list (later story)
}

void oCourse::setShorterVersion(bool activeShortening, oCourse* shorter) {
  if (activeShortening)
    getDI().setInt("Shorten", shorter ? shorter->getId() : -1);
  else
    getDI().setInt("Shorten", 0);
}

bool oCourse::isAdapted() const {
  return !tMapToOriginalOrder.empty();
}

int oCourse::getAdaptionId() const {
  int key = 0;
  for (size_t j = 0; j < tMapToOriginalOrder.size(); j++)
    key = key * 97 + tMapToOriginalOrder[j];
  return key;
}

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

wstring oCourse::getCourseProblems() const {
  int max_time = getMaximumRogainingTime();
  int min_point = getMinimumRogainingPoints();
  if (max_time > 0) {
    for (int k = 0; k < nControls(); k++) {
      if (controls[k]->isRogaining(true))
        return L"";
    }
    return L"Banan saknar rogainingkontroller.";
  } else if (min_point > 0) {
    int max_p = 0;
    for (int k = 0; k < nControls(); k++) {
      if (controls[k]->isRogaining(true))
        max_p += controls[k]->getRogainingPoints();
    }
    if (max_p < min_point)
      return L"Banans kontroller ger för få poäng för att täcka poängkravet.";
  }
  return L"";
}

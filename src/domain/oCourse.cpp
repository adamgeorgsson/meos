// oCourse.cpp — Migrated from legacy code/oCourse.cpp (US-003d).
// Cross-platform, no Win32 / GUI dependencies.

#include "oCourse.h"
#include "oEvent.h"
#include "SICard.h"
#include "oDataContainer.h"
#include "../util/localizer.h"
#include "../util/meos_util.h"
#include "../util/meosexception.h"
#include "../util/xmlparser.h"
#include "../util/timeconstants.hpp"
#include <cassert>
#include <limits>
#include <algorithm>

// ── Constructors / destructor ─────────────────────────────────────────────────

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
  oe->qFreeCourseId = max(id, oe->qFreeCourseId);
}

oCourse::~oCourse() = default;

// ── Info / XML ────────────────────────────────────────────────────────────────

wstring oCourse::getInfo() const {
  return L"Bana " + name;
}

bool oCourse::Write(xmlparser& xml) {
  if (Removed) return true;

  xml.startTag("Course");
  xml.write("Id",       Id);
  xml.write("Updated",  getStamp());
  xml.write("Name",     name);
  xml.write("Length",   length);
  xml.write("Controls", getControls());
  if (start)  xml.write("Start",  start->getId());
  if (finish) xml.write("Finish", finish->getId());
  xml.write("Legs", getLegLengths());
  getDI().write(xml);
  xml.endTag();
  return true;
}

void oCourse::Set(const xmlobject* xo) {
  xmlList xl;
  xo->getObjects(xl);

  for (const auto& it : xl) {
    if (it.is("Id"))           Id = it.getInt();
    else if (it.is("Length"))  length = it.getInt();
    else if (it.is("Name"))    name = it.getWStr();
    else if (it.is("Controls")) importControls(it.getRawStr(), false, false);
    else if (it.is("Legs"))    importLegLengths(it.getRawStr(), false);
    else if (it.is("Start"))   start  = oe->getControl(it.getInt());
    else if (it.is("Finish"))  finish = oe->getControl(it.getInt());
    else if (it.is("oData"))   getDI().set(it);
    else if (it.is("Updated")) Modified.setStamp(it.getRawStr());
  }
}

// ── String serialisation ──────────────────────────────────────────────────────

string oCourse::getLegLengths() const {
  string str;
  for (size_t m = 0; m < legLengths.size(); m++) {
    if (m > 0) str += ";";
    str += itos(legLengths[m]);
  }
  return str;
}

string oCourse::getControls() const {
  string str;
  char bf[16];
  for (int m = 0; m < nControls(); m++) {
    snprintf(bf, sizeof(bf), "%d;", controls[m]->Id);
    str += bf;
  }
  return str;
}

wstring oCourse::getControlsUI() const {
  wstring str;
  wchar_t bf[16];
  for (int m = 0; m < nControls() - 1; m++) {
    swprintf(bf, 16, L"%d, ", controls[m]->Id);
    str += bf;
  }
  if (nControls() > 0) {
    swprintf(bf, 16, L"%d", controls[nControls()-1]->Id);
    str += bf;
  }
  return str;
}

vector<wstring> oCourse::getCourseReadable(int limit) const {
  vector<wstring> res;
  wstring str;
  if (!useFirstAsStart())
    str = lang.tl("Start").substr(0, 1);

  vector<pControl> rg;
  bool needFinish = false;
  bool rogaining = hasRogaining();
  for (int m = 0; m < nControls(); m++) {
    if (controls[m]->isRogaining(rogaining))
      rg.push_back(controls[m]);
    else {
      if (!str.empty()) str += L"-";
      str += controls[m]->getLongString();
      needFinish = true;
    }
    if (static_cast<int>(str.length()) >= limit) {
      res.push_back(str);
      str.clear();
    }
  }

  if (needFinish && !useLastAsFinish()) {
    if (!str.empty()) str += L"-";
    str += lang.tl("Mål").substr(0, 1);
  }
  if (!str.empty()) {
    if (str.length() < 5 && !res.empty())
      res.back().append(str);
    else
      res.push_back(str);
  }

  if (!rg.empty()) {
    str = lang.tl("Rogaining: ");
    for (size_t k = 0; k < rg.size(); k++) {
      if (k > 0) str += L", ";
      if (static_cast<int>(str.length()) >= limit) {
        res.push_back(str);
        str.clear();
      }
      str += rg[k]->getLongString();
    }
    if (!str.empty()) res.push_back(str);
  }
  return res;
}

// ── Control management ────────────────────────────────────────────────────────

pControl oCourse::addControl(int Id) {
  pControl pc = doAddControl(Id);
  updateChanged();
  return pc;
}

pControl oCourse::doAddControl(int Id) {
  pControl c = oe->getControl(Id, true, false);
  if (c == nullptr)
    throw meosException("Felaktig kontroll");
  controls.push_back(c);
  return c;
}

void oCourse::splitControls(const string& ctrls, vector<int>& nr) {
  const char* str = ctrls.c_str();
  nr.clear();
  while (*str) {
    int cid = atoi(str);
    while (*str && (*str != ';' && *str != ',' && *str != ' ')) str++;
    while (*str && (*str == ';' || *str == ',' || *str == ' ')) str++;
    if (cid > 0) nr.push_back(cid);
  }
}

bool oCourse::importControls(const string& ctrls, bool setChanged, bool updateLegLengths) {
  vector<int> oldC;
  for (int k = 0; k < nControls(); k++)
    oldC.push_back(controls[k] ? controls[k]->getId() : 0);

  controls.clear();

  vector<int> newC;
  splitControls(ctrls, newC);

  for (size_t k = 0; k < newC.size(); k++)
    doAddControl(newC[k]);

  bool changed = nControls() != static_cast<int>(oldC.size());

  if (changed && updateLegLengths && !legLengths.empty()) {
    int oldIndex = 0;
    int newIndex = 0;
    vector<int> newLen(nControls() + 1);
    bool lastOK = true;
    while (newIndex < nControls()) {
      if (oldIndex < static_cast<int>(oldC.size())) {
        if (oldC[oldIndex] == newC[newIndex]) {
          if (lastOK && oldIndex < static_cast<int>(legLengths.size()))
            newLen[newIndex] = legLengths[oldIndex];
          lastOK = true;
          oldIndex++;
        } else {
          lastOK = false;
          int forward = oldIndex + 1;
          while (forward < static_cast<int>(oldC.size())) {
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
    if (lastOK) newLen.back() = legLengths.back();
    swap(newLen, legLengths);
  }

  for (int k = 0; !changed && k < nControls(); k++)
    changed |= oldC[k] != controls[k]->getId();

  if (changed) {
    if (setChanged) updateChanged();
    oe->punchIndex.clear();
  }
  return changed;
}

void oCourse::importLegLengths(const string& legs, bool setChanged) {
  vector<string> splits;
  split(legs, ";", splits);

  bool changed = false;
  if (legLengths.size() != splits.size()) {
    legLengths.resize(splits.size());
    changed = true;
  }
  for (size_t k = 0; k < legLengths.size(); k++) {
    int val = atoi(splits[k].c_str());
    if (legLengths[k] != val) changed = true;
    legLengths[k] = val;
  }
  if (changed && setChanged) updateChanged();
}

oControl* oCourse::getControl(int index) const {
  if (index >= 0 && index < nControls()) return controls[index];
  return nullptr;
}

int oCourse::getLegLength(int index) const {
  if (static_cast<size_t>(index) < legLengths.size())
    return legLengths[index];
  return 0;
}

void oCourse::getControls(vector<pControl>& pc) const {
  pc = controls;
}

vector<int> oCourse::getControlNumbers() const {
  vector<int> ret;
  for (int k = 0; k < nControls(); k++)
    ret.push_back(controls[k]->getFirstNumber());
  return ret;
}

// ── Distance calculation ──────────────────────────────────────────────────────

int oCourse::distance(const SICard& card) const {
  if (card.nPunch >= 192) return -100;
  int punches[192];
  for (uint32_t i = 0; i < card.nPunch; i++)
    punches[i] = static_cast<int>(card.Punch[i].Code);
  return distance(punches, static_cast<int>(card.nPunch));
}

int oCourse::distance(const oCard& /*card*/) const {
  // oCard not fully implemented yet (US-003f); return 0 = exact match stub.
  return 0;
}

int oCourse::distance(int* punches, int numPunches) const {
  int matches = 0;

  std::set<int> rogaining;
  vector<map<int,int>> allowedControls;
  allowedControls.reserve(nControls());
  std::set<int> commonCode;
  if (hasRogaining()) {
    for (int k = 0; k < nControls(); k++) {
      if (controls[k]->isRogaining(true)) {
        for (int j = 0; j < controls[k]->nNumbers; j++)
          rogaining.insert(controls[k]->Numbers[j]);
      }
    }
  }

  int toMatch = 0;
  size_t orderIndex = 0;
  for (int k = 0; k < nControls(); k++) {
    using CS = oControl::ControlStatus;
    CS st = controls[k]->getStatus();
    if (controls[k]->isRogaining(hasRogaining()) ||
        st == CS::StatusBad || st == CS::StatusOptional ||
        st == CS::StatusBadNoTiming)
      continue;

    if (st == CS::StatusMultiple) {
      for (int j = 0; j < controls[k]->nNumbers; j++) {
        if (allowedControls.size() <= orderIndex)
          allowedControls.resize(orderIndex + 1);
        for (int i = 0; i < controls[k]->nNumbers; i++)
          ++allowedControls[orderIndex][controls[k]->Numbers[i]];
        orderIndex++;
        toMatch++;
      }
    } else {
      if (allowedControls.size() <= orderIndex)
        allowedControls.resize(orderIndex + 1);
      for (int j = 0; j < controls[k]->nNumbers; j++)
        ++allowedControls[orderIndex][controls[k]->Numbers[j]];
      orderIndex++;
      toMatch++;
    }

    if (getCommonControl() == controls[k]->getId()) {
      orderIndex = 0;
      commonCode.insert(controls[k]->Numbers,
                        controls[k]->Numbers + controls[k]->nNumbers);
    }
  }

  size_t matchIndex = 0;
  for (int k = 0; k < numPunches && matches < toMatch; k++) {
    for (int j = k; j < numPunches; j++) {
      if (matchIndex < allowedControls.size() &&
          allowedControls[matchIndex].count(punches[j]) &&
          allowedControls[matchIndex][punches[j]] > 0) {
        --allowedControls[matchIndex][punches[j]];
        k = j;
        matches++;
        break;
      }
    }
    matchIndex++;
    if (commonCode.count(punches[k]))
      matchIndex = 0;
  }

  if (matches == toMatch)
    return numPunches - toMatch; // positive = extra controls
  else
    return matches - toMatch;   // negative = missing controls
}

// ── Property accessors ────────────────────────────────────────────────────────

wstring oCourse::getLengthS() const {
  return itow(getLength());
}

void oCourse::setName(const wstring& n) {
  if (name != n) { name = n; updateChanged(); }
}

void oCourse::setLength(int le) {
  if (le < 0 || le > 1000000) le = 0;
  if (length != le) { length = le; updateChanged(); }
}

oDataContainer& oCourse::getDataBuffers(pvoid& data, pvoid& olddata, pvectorstr& strData) const {
  data    = (pvoid)oData;
  olddata = (pvoid)oDataOld;
  strData = nullptr;
  return *oe->oCourseData;
}

void oCourse::setNumberMaps(int block) {
  getDI().setInt("NumberMaps", block);
}

int oCourse::getNumberMaps() const {
  return getDCI().getInt("NumberMaps");
}

int oCourse::getNumUsedMaps(bool noVacant) const {
  if (tMapsUsed == -1)
    oe->calculateNumRemainingMaps(false);
  return noVacant ? tMapsUsedNoVacant : tMapsUsed;
}

bool oCourse::setStartFinish(pControl startC, pControl finishC, bool updateStatus) {
  if (startC != start || finishC != finish) {
    start  = startC;
    finish = finishC;
    if (updateStatus) updateChanged();
    return true;
  }
  return false;
}

bool oCourse::setStartFinishId(int startId, int finishId, bool updateStatus) {
  pControl s = startId ? oe->getControl(startId, false, false) : nullptr;
  pControl f = finishId ? oe->getControl(finishId, false, false) : nullptr;
  return setStartFinish(s, f, updateStatus);
}

void oCourse::setStart(const wstring& startName, bool sync) {
  if (getDI().setString("StartName", startName)) {
    if (sync) synchronize();
    for (auto& cls : oe->Classes) {
      if (cls.isRemoved()) continue;
      // Class-course linkage set on oClass; stub classes don't hold back-ref here.
    }
  }
}

const wstring& oCourse::getStart() const {
  return getDCI().getString("StartName");
}

// ── ID management ─────────────────────────────────────────────────────────────

void oCourse::changeId(int newId) {
  pCourse old = oe->courseIdIndex[Id];
  if (old == this) oe->courseIdIndex.remove(Id);
  oBase::changeId(newId);
  oe->courseIdIndex[newId] = this;
}

// ── Caching ───────────────────────────────────────────────────────────────────

void oCourse::clearCache() const {
  cachedHasRogaining = 0;
  cachedControlOrdinal.clear();
  cacheDataRevision = static_cast<int>(oe->dataRevision);
  oe->tCalcNumMapsDataRevision = -1;
  tMapsUsed = -1;
  tMapsUsedNoVacant = -1;
}

// ── Rogaining ─────────────────────────────────────────────────────────────────

void oCourse::setRogainingPointsPerMinute(int p) { getDI().setInt("RReduction", p); }
int  oCourse::getRogainingPointsPerMinute() const { return getDCI().getInt("RReduction"); }

int oCourse::calculateReduction(int overTime) const {
  int reduction = 0;
  if (overTime > 0) {
    int method = getDCI().getInt("RReductionMethod");
    if (method == 0)
      reduction = (59 * timeConstSecond + overTime * getRogainingPointsPerMinute()) / timeConstMinute;
    else
      reduction = ((59 * timeConstSecond + overTime) / timeConstMinute) * getRogainingPointsPerMinute();
  }
  return reduction;
}

void oCourse::setMinimumRogainingPoints(int p) {
  cachedHasRogaining = 0;
  getDI().setInt("RPointLimit", p);
}
int oCourse::getMinimumRogainingPoints() const { return getDCI().getInt("RPointLimit"); }

void oCourse::setMaximumRogainingTime(int p) {
  cachedHasRogaining = 0;
  if (p == NOTIME) p = 0;
  getDI().setInt("RTimeLimit", p);
}
int oCourse::getMaximumRogainingTime() const { return getDCI().getInt("RTimeLimit"); }

bool oCourse::hasRogaining() const {
  if (static_cast<int>(oe->dataRevision) != cacheDataRevision)
    clearCache();
  if (cachedHasRogaining > 0)
    return cachedHasRogaining == 2;
  bool r = getMaximumRogainingTime() > 0 || getMinimumRogainingPoints() > 0;
  cachedHasRogaining = r ? 2 : 1;
  return r;
}

// ── Control ordinal (printed number) ──────────────────────────────────────────

const wstring& oCourse::getControlOrdinal(int controlIndex) const {
  if ((controlIndex + 1 == nControls() && useLastAsFinish()) ||
       controlIndex == nControls())
    return lang.tl("Mål");

  if (static_cast<int>(oe->dataRevision) != cacheDataRevision)
    clearCache();

  if (static_cast<size_t>(controlIndex) < cachedControlOrdinal.size() &&
      !cachedControlOrdinal[controlIndex].empty())
    return cachedControlOrdinal[controlIndex];

  if (controlIndex > nControls())
    throw meosException("Invalid index");
  cachedControlOrdinal.resize(nControls());

  int o = useFirstAsStart() ? 0 : 1;
  bool rogaining = hasRogaining();
  for (int k = 0; k < controlIndex && k < nControls(); k++) {
    if (controls[k] && !controls[k]->isRogaining(rogaining)) o++;
  }
  cachedControlOrdinal[controlIndex] = itow(o);
  return cachedControlOrdinal[controlIndex];
}

// ── Course problems ───────────────────────────────────────────────────────────

wstring oCourse::getCourseProblems() const {
  int max_time  = getMaximumRogainingTime();
  int min_point = getMinimumRogainingPoints();

  if (max_time > 0) {
    for (int k = 0; k < nControls(); k++)
      if (controls[k]->isRogaining(true)) return L"";
    return L"Banan saknar rogainingkontroller.";
  } else if (min_point > 0) {
    int max_p = 0;
    for (int k = 0; k < nControls(); k++)
      if (controls[k]->isRogaining(true))
        max_p += controls[k]->getRogainingPoints();
    if (max_p < min_point)
      return L"Banans kontroller ger för få poäng för att täcka poängkravet.";
  }
  return L"";
}

// ── Remove / canRemove ────────────────────────────────────────────────────────

void oCourse::remove()         { if (oe) oe->removeCourse(Id); }
bool oCourse::canRemove() const{ return !oe->isCourseUsed(Id); }

// ── First-as-start / last-as-finish ──────────────────────────────────────────

bool oCourse::useFirstAsStart() const { return getDCI().getInt("FirstAsStart") != 0; }
bool oCourse::useLastAsFinish() const { return getDCI().getInt("LastAsFinish")  != 0; }
void oCourse::firstAsStart(bool f) { getDI().setInt("FirstAsStart", f ? 1 : 0); }
void oCourse::lastAsFinish(bool f) { getDI().setInt("LastAsFinish",  f ? 1 : 0); }

int oCourse::getFinishPunchType() const {
  if (useLastAsFinish() && nControls() > 0)
    return controls.back()->Numbers[0];
  return oPunch::PunchFinish;
}

int oCourse::getStartPunchType() const {
  if (useFirstAsStart() && nControls() > 0)
    return controls[0]->Numbers[0];
  return oPunch::PunchStart;
}

// ── Common control / loops ────────────────────────────────────────────────────

int  oCourse::getCommonControl() const { return getDCI().getInt("CControl"); }

int oCourse::getNumLoops() const {
  int cc = getCommonControl();
  if (cc == 0) return 0;
  bool wasCC = true;
  int loopCount = 0;
  for (int i = 0; i < nControls(); i++) {
    if (controls[i]->getId() == cc) wasCC = true;
    else if (wasCC) { loopCount++; wasCC = false; }
  }
  return loopCount;
}

void oCourse::setCommonControl(int ctrlId) {
  if (ctrlId != 0) {
    int found = 0;
    for (int k = 0; k < nControls(); k++)
      if (controls[k]->getId() == ctrlId) found++;
    if (found == 0)
      throw meosException("Kontroll X finns inte på banan#" + itos(ctrlId));
  }
  getDI().setInt("CControl", ctrlId);
}

// ── Adapted (loop) courses ────────────────────────────────────────────────────

pCourse oCourse::getAdapetedCourse(const oCard& /*card*/, oCourse& /*tmpCourse*/, int& numShorten) const {
  // oCard not fully implemented yet (US-003f).
  numShorten = 0;
  return pCourse(this);
}

bool oCourse::isAdapted() const { return !tMapToOriginalOrder.empty(); }

int oCourse::getAdaptionId() const {
  int key = 0;
  for (size_t j = 0; j < tMapToOriginalOrder.size(); j++)
    key = key * 97 + tMapToOriginalOrder[j];
  return key;
}

int oCourse::matchLoopKey(const vector<int>& punches, const vector<pControl>& key) {
  if (key.empty()) return 999;
  size_t ix = static_cast<size_t>(-1);
  for (size_t k = 0; k < key.size(); k++) {
    int code = key[k]->getFirstNumber();
    while (++ix < punches.size()) {
      if (punches[ix] == code) { code = -1; break; }
    }
    if (code != -1) return 1000;
  }
  return static_cast<int>(ix);
}

bool oCourse::constructLoopKeys(int cc,
                                 vector<vector<pControl>>& loopKeys,
                                 vector<int>& ccIndex) const {
  bool firstAsStart = useFirstAsStart();
  if (firstAsStart) {
    for (int k = 1; k < nControls(); k++) {
      if (controls[k] == controls[0]) { firstAsStart = false; break; }
    }
  }
  if (!controls.empty() && controls[0]->getId() == cc) firstAsStart = true;

  bool lastAsFinish = useLastAsFinish();
  if (lastAsFinish) {
    for (int k = 0; k < nControls() - 1; k++) {
      if (controls[k] == controls.back()) { lastAsFinish = false; break; }
    }
  }

  int startIx = firstAsStart ? 1 : 0;
  int endIx   = lastAsFinish ? nControls() : nControls() - 1;

  ccIndex.push_back(startIx - 1);
  for (int k = startIx; k < endIx; k++) {
    if (controls[k]->getId() == cc) ccIndex.push_back(k);
  }
  if (ccIndex.size() <= 1) return false;

  loopKeys.clear();
  loopKeys.resize(ccIndex.size());

  int keyIndex = 1;
  bool changed = true;
  bool enough  = false;
  while (changed && !enough) {
    changed = false;
    for (size_t k = 0; k < ccIndex.size(); k++) {
      int keyIx  = ccIndex[k] + keyIndex;
      int nextIx = (k + 1) < ccIndex.size() ? ccIndex[k+1] : nControls();
      if (keyIx < nextIx &&
          controls[keyIx]->isSingleStatusOK() &&
          controls[keyIx]->nNumbers == 1) {
        loopKeys[k].push_back(controls[keyIx]);
        changed = true;
      }
    }
    keyIndex++;
    if (changed) {
      enough = false;
      std::set<__int64> hashes;
      for (size_t k = 0; k < loopKeys.size(); k++) {
        __int64 h = static_cast<__int64>(loopKeys[k].size());
        for (size_t j = 0; j < loopKeys[k].size(); j++)
          h = h * 997 + loopKeys[k][j]->Numbers[0];
        hashes.insert(h);
      }
      enough = hashes.size() == loopKeys.size();
    }
  }
  return enough;
}

// ── Shorter / longer course variants ─────────────────────────────────────────

pair<bool, pCourse> oCourse::getShorterVersion() const {
  int ix = getDCI().getInt("Shorten");
  if (ix == -1) return make_pair(true, nullptr);
  auto c = oe->getCourse(ix);
  return make_pair(c != nullptr, c);
}

pCourse oCourse::getLongerVersion() const {
  for (auto it = oe->Courses.begin(); it != oe->Courses.end(); ++it) {
    int ix = it->getDCI().getInt("Shorten");
    if (ix == Id) return pCourse(&*it);
  }
  return nullptr;
}

void oCourse::setShorterVersion(bool activeShortening, pCourse shorten) {
  if (activeShortening)
    getDI().setInt("Shorten", shorten ? shorten->getId() : -1);
  else
    getDI().setInt("Shorten", 0);
}

// ── hasControl ────────────────────────────────────────────────────────────────

bool oCourse::hasControl(const oControl* ctrl) const {
  for (int i = 0; i < nControls(); i++)
    if (controls[i] == ctrl) return true;
  if (finish == ctrl) return true;
  if (start  == ctrl) return true;
  return false;
}

bool oCourse::hasControlCode(int code) const {
  for (int i = 0; i < nControls(); i++)
    if (controls[i]->hasNumber(code)) return true;
  return false;
}

// ── getClasses ────────────────────────────────────────────────────────────────

void oCourse::getClasses(vector<pClass>& usageClass) const {
  // Requires oEvent::getClasses with full oClass — stub until US-003e.
  usageClass.clear();
}

// ── Leg lengths ───────────────────────────────────────────────────────────────

void oCourse::setLegLengths(const vector<int>& legs) {
  if (legs.size() == static_cast<size_t>(nControls()) + 1 || legs.empty()) {
    bool diff = legs.size() != legLengths.size();
    if (!diff) {
      for (size_t k = 0; k < legs.size(); k++)
        if (legs[k] != legLengths[k]) diff = true;
    }
    if (diff) { updateChanged(); legLengths = legs; }
  } else {
    throw std::runtime_error("Invalid parameter value");
  }
}

// ── Part of course ────────────────────────────────────────────────────────────

double oCourse::getPartOfCourse(int startCtrl, int endCtrl) const {
  if (endCtrl == 0) endCtrl = nControls();
  if (legLengths.size() != static_cast<size_t>(nControls()) + 1 ||
      startCtrl <= endCtrl ||
      static_cast<size_t>(startCtrl) >= legLengths.size() ||
      static_cast<size_t>(endCtrl)   >= legLengths.size() ||
      !(length > 0))
    return 0.0;
  int dist = 0;
  for (int k = startCtrl; k < endCtrl; k++) dist += legLengths[k];
  return max(1.0, static_cast<double>(dist) / static_cast<double>(length));
}

// ── ID sum (for variant identification) ──────────────────────────────────────

int oCourse::getIdSum(int nC) {
  int id = 0;
  for (int k = 0; k < min<int>(nC, nControls()); k++)
    id = 431 * id + (controls[k] ? controls[k]->getId() : 0);
  return (id == 0) ? getId() : id;
}

// ── getCourseControlId ────────────────────────────────────────────────────────

int oCourse::getCourseControlId(int controlIx) const {
  if (controlIx >= nControls()) { assert(false); return -1; }
  int id = controls[controlIx] ? controls[controlIx]->getId() : 0;
  if (id == 0) return 0;
  int count = 0;
  for (int j = 0; j < controlIx; j++)
    if (controls[j] && controls[j]->Id == id) count++;
  return oControl::getCourseControlIdFromIdIndex(id, count);
}

// ── getRadioName ──────────────────────────────────────────────────────────────

wstring oCourse::getRadioName(int courseControlId) const {
  pair<int,int> idix = oControl::getIdIndexFromCourseControlId(courseControlId);
  pControl pc = nullptr;
  int numRadio = 0;
  int clsix = 1;
  for (int k = 0; k < nControls(); k++) {
    if (controls[k]) {
      if (controls[k]->isValidRadio()) numRadio++;
      if (controls[k]->Id == idix.first) {
        if (idix.second == 0) { pc = controls[k]; break; }
        else { clsix++; idix.second--; }
      }
    }
  }
  if (pc == nullptr) return L"?";

  wstring name;
  if (pc->hasName()) {
    name = pc->getName();
    if (pc->getNumberDuplicates() > 1)
      name += makeDash(L"-" + itow(clsix));
  } else {
    name = lang.tl("radio X#" + itos(numRadio));
    capitalize(name);
  }
  return name;
}

// ── changedObject ─────────────────────────────────────────────────────────────

void oCourse::changedObject() {
  if (oe) oe->globalModification = true;
  oe->sqlCourses.changed = true;
}

// ── merge ─────────────────────────────────────────────────────────────────────

void oCourse::merge(const oBase& input, const oBase* base) {
  const oCourse& src  = dynamic_cast<const oCourse&>(input);
  const oCourse* base2 = dynamic_cast<const oCourse*>(base);

  if ((base2 == nullptr || base2->name != src.name) && src.name.length() > 0)
    setName(src.name);
  if (!base2 || base2->length != src.length)
    setLength(src.length);

  importControls(src.getControls(), true, false);
  importLegLengths(src.getLegLengths(), true);
  setStartFinishId(src.getStartId(), src.getFinishId());
  if (getDI().merge(input, base)) updateChanged();
  synchronize(true);
}

// ── getNameAndFamily ──────────────────────────────────────────────────────────

void oCourse::getNameAndFamily(wstring& nameOut, wstring& family) const {
  vector<wstring> fn;
  split(this->name, L":", fn);
  if (fn.size() == 2) {
    family  = std::move(fn[0]);
    nameOut = std::move(fn[1]);
  } else {
    nameOut = this->name;
    family  = L"";
  }
}

// ── getBestTime ───────────────────────────────────────────────────────────────

int oCourse::getBestTime() const {
  // Stub: requires full oRunner; returns -1 until US-003g is done.
  return -1;
}

// ── inputData / fillInput ─────────────────────────────────────────────────────

pair<int, bool> oCourse::inputData(int id, const wstring& input,
                                    int inputId, wstring& output, bool noUpdate) {
  synchronize(false);
  if (id > 1000)
    return oe->oCourseData->inputData(this, id, input, inputId, output, noUpdate);
  switch (id) {
    case 1: setName(input); break;
    case 2: setLength(wtoi(input.c_str())); break;
    default: break;
  }
  return make_pair(0, false);
}

void oCourse::fillInput(int id, vector<pair<wstring,size_t>>& out, size_t& selected) {
  if (id > 1000) {
    oe->oCourseData->fillInput(this, id, 0, out, selected);
    return;
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
// oEvent course management methods (implemented here, declared in oEvent.h)
// ═══════════════════════════════════════════════════════════════════════════════

pCourse oEvent::addCourse(const wstring& pname, int plengh, int id) {
  oCourse c(this, id);
  c.length = plengh;
  c.name   = pname;
  return addCourse(c);
}

pCourse oEvent::addCourse(const oCourse& oc) {
  if (oc.Id == 0) return nullptr;
  pCourse pOld = getCourse(oc.getId());
  if (pOld) return nullptr;

  Courses.push_back(oc);
  qFreeCourseId = max(qFreeCourseId, oc.getId());

  pCourse pc = &Courses.back();
  pc->addToEvent(this, &oc);
  courseIdIndex[pc->getId()] = pc;
  return pc;
}

pCourse oEvent::getCourse(int Id) const {
  if (Id == 0) return nullptr;
  pCourse value;
  if (courseIdIndex.lookup(Id, value)) return value;
  return nullptr;
}

pCourse oEvent::getCourse(const wstring& n) const {
  for (auto it = Courses.begin(); it != Courses.end(); ++it) {
    if (!it->isRemoved() && it->name == n) return pCourse(&*it);
  }
  return nullptr;
}

void oEvent::getCourses(vector<pCourse>& crs) const {
  crs.clear();
  crs.reserve(Courses.size());
  for (auto it = Courses.begin(); it != Courses.end(); ++it) {
    if (!it->isRemoved()) crs.push_back(pCourse(&*it));
  }
}

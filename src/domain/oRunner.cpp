// oRunner.cpp — domain layer implementation of oRunner and oAbstractRunner.

#include "oRunner.h"
#include "oTeam.h"
#include "oEvent.h"
#include "oDataContainer.h"
#include "domain_header.h"
#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <unordered_set>

using namespace std;

// -----------------------------------------------------------------------
// DataContainer for oRunner (registered once per process)
// -----------------------------------------------------------------------
static oDataContainer& runnerContainer() {
  static oDataContainer dc(256);
  static bool init = false;
  if (!init) {
    init = true;
    // Financial/fee fields
    dc.addVariableInt("Fee",      oDataContainer::oISCurrency,  "Fee");
    dc.addVariableInt("CardFee",  oDataContainer::oISCurrency,  "CardFee");
    dc.addVariableInt("Paid",     oDataContainer::oISCurrency,  "Paid");
    dc.addVariableInt("PayMode",  oDataContainer::oIS8U,        "PayMode");
    dc.addVariableInt("Taxable",  oDataContainer::oISCurrency,  "Taxable");
    // Demographic
    dc.addVariableInt("BirthYear",oDataContainer::oISDateOrYear,"BirthYear");
    dc.addVariableString("Bib",   8, "Bib");
    dc.addVariableInt("Rank",     oDataContainer::oIS32,        "Rank");
    dc.addVariableInt("EntryDate",oDataContainer::oISDate,      "EntryDate");
    dc.addVariableInt("EntryTime",oDataContainer::oISTime,      "EntryTime");
    dc.addVariableString("Sex",   1, "Sex");
    dc.addVariableString("Nationality", 3, "Nationality");
    dc.addVariableString("Country",    23, "Country");
    dc.addVariableInt("ExtId",    oDataContainer::oIS64,        "ExtId");
    dc.addVariableInt("ExtId2",   oDataContainer::oIS64,        "ExtId2");
    dc.addVariableInt("Priority", oDataContainer::oIS8U,        "Priority");
    dc.addVariableString("Phone", 20, "Phone");
    dc.addVariableInt("RaceId",   oDataContainer::oIS32,        "RaceId");
    // Adjustments / flags
    dc.addVariableInt("TimeAdjust",    oDataContainer::oISTimeAdjust, "TimeAdjust");
    dc.addVariableInt("PointAdjust",   oDataContainer::oIS32,  "PointAdjust");
    dc.addVariableInt("TransferFlags", oDataContainer::oIS32,  "TransferFlags");
    dc.addVariableInt("Shorten",       oDataContainer::oIS8U,  "Shorten");
    dc.addVariableInt("EntrySource",   oDataContainer::oIS32,  "EntrySource");
    dc.addVariableInt("Heat",          oDataContainer::oIS8U,  "Heat");
    dc.addVariableInt("Reference",     oDataContainer::oIS32,  "Reference");
    dc.addVariableInt("NoRestart",     oDataContainer::oIS8U,  "NoRestart");
    dc.addVariableString("InputResult","InputResult");
    dc.addVariableInt("StartGroup",    oDataContainer::oIS32,  "StartGroup");
    dc.addVariableInt("Family",        oDataContainer::oIS32,  "Family");
    dc.addVariableInt("DrawnTime",     oDataContainer::oISTime,"DrawnTime");
    dc.addVariableInt("DataA",         oDataContainer::oIS32,  "DataA");
    dc.addVariableInt("DataB",         oDataContainer::oIS32,  "DataB");
    dc.addVariableString("TextA", 40, "TextA");
    dc.addVariableString("Annotation", "Annotation");
  }
  return dc;
}

// -----------------------------------------------------------------------
// oDataContainer& getDataBuffers
// -----------------------------------------------------------------------
oDataContainer& oRunner::getDataBuffers(pvoid& data, pvoid& olddata, pvectorstr& strData) const {
  data    = &dataMap_;
  olddata = &oldDataMap_;
  strData = nullptr;
  return runnerContainer();
}

// -----------------------------------------------------------------------
// oAbstractRunner::DynamicValue
// -----------------------------------------------------------------------
bool oAbstractRunner::DynamicValue::isOld(const oEvent& oe, int key) const {
  return oe.dataRevision != dataRevision || forKey != key;
}

bool oAbstractRunner::DynamicValue::isOld(const oEvent& oe) const {
  return oe.dataRevision != dataRevision;
}

oAbstractRunner::DynamicValue& oAbstractRunner::DynamicValue::update(
    const oEvent& oe, int key, int v, bool preferStd) {
  if (preferStd) {
    valueStd = v;
    forKey = 0;
  } else {
    value = v;
    forKey = key;
    dataRevision = oe.dataRevision;
  }
  return *this;
}

int oAbstractRunner::DynamicValue::get(bool preferStd) const {
  if (preferStd && valueStd >= 0) return valueStd;
  return value;
}

void oAbstractRunner::DynamicValue::reset() {
  value = -1; valueStd = -1; dataRevision = -1;
}

// -----------------------------------------------------------------------
// oAbstractRunner::TempResult
// -----------------------------------------------------------------------
oAbstractRunner::TempResult::TempResult(RunnerStatus statusIn, int startTime, int runningTime, int points)
  : startTime_(startTime), runningTime_(runningTime), points_(points), status_(statusIn) {}

void oAbstractRunner::TempResult::reset() {
  startTime_ = 0; runningTime_ = 0; timeAfter_ = 0;
  points_ = 0; place_ = 0; status_ = StatusUnknown;
  internalScore_ = {0, 0}; outputTimes_.clear(); outputNumbers_.clear();
}

const wstring& oAbstractRunner::TempResult::getStatusS(RunnerStatus inputStatus) const {
  return oAbstractRunner::encodeStatus(inputStatus);
}

const wstring& oAbstractRunner::TempResult::getPrintPlaceS(bool withDot) const {
  thread_local wstring buf;
  if (place_ > 0 && place_ < 10000) {
    buf = itow(place_);
    if (withDot) buf += L".";
  } else {
    buf = L"";
  }
  return buf;
}

const wstring& oAbstractRunner::TempResult::getRunningTimeS(int inputTime, SubSecond mode) const {
  thread_local wstring buf;
  buf = formatTimeMS(inputTime, mode != SubSecond::Off);
  return buf;
}

const wstring& oAbstractRunner::TempResult::getFinishTimeS(const oEvent* oe, SubSecond mode) const {
  thread_local wstring buf;
  int ft = getFinishTime();
  if (ft <= 0) { buf = L""; return buf; }
  // Simple HH:MM:SS format
  int sec = ft / timeConstSecond;
  int h = sec / 3600; sec %= 3600;
  int m = sec / 60;   sec %= 60;
  wchar_t tmp[16];
  swprintf(tmp, 16, L"%d:%02d:%02d", h, m, sec);
  buf = tmp;
  return buf;
}

const wstring& oAbstractRunner::TempResult::getStartTimeS(const oEvent* oe, SubSecond mode) const {
  thread_local wstring buf;
  int st = startTime_;
  if (st <= 0) { buf = L""; return buf; }
  int sec = st / timeConstSecond;
  int h = sec / 3600; sec %= 3600;
  int m = sec / 60;   sec %= 60;
  wchar_t tmp[16];
  swprintf(tmp, 16, L"%d:%02d:%02d", h, m, sec);
  buf = tmp;
  return buf;
}

const wstring& oAbstractRunner::TempResult::getOutputTime(int ix) const {
  static const wstring empty;
  if (ix < 0 || ix >= (int)outputTimes_.size()) return empty;
  thread_local wstring buf;
  buf = formatTimeMS(outputTimes_[ix], false);
  return buf;
}

int oAbstractRunner::TempResult::getOutputNumber(int ix) const {
  if (ix < 0 || ix >= (int)outputNumbers_.size()) return 0;
  return outputNumbers_[ix];
}

// -----------------------------------------------------------------------
// Status encoding/decoding
// -----------------------------------------------------------------------
const wstring& oAbstractRunner::encodeStatus(RunnerStatus st, bool allowError) {
  thread_local wstring res;
  switch (st) {
  case StatusOK:              res = L"OK"; break;
  case StatusUnknown:         res = L"UN"; break;
  case StatusDNS:             res = L"NS"; break;
  case StatusCANCEL:          res = L"CC"; break;
  case StatusOutOfCompetition:res = L"OC"; break;
  case StatusNoTiming:        res = L"NT"; break;
  case StatusMP:              res = L"MP"; break;
  case StatusDNF:             res = L"NF"; break;
  case StatusDQ:              res = L"DQ"; break;
  case StatusMAX:             res = L"MX"; break;
  case StatusNotCompeting:    res = L"NC"; break;
  default:
    if (allowError) res = L"ERROR";
    else throw runtime_error("Unknown status");
  }
  return res;
}

RunnerStatus oAbstractRunner::decodeStatus(const wstring& stat) {
  wstring ustat = stat;
  for (wchar_t& c : ustat) c = (wchar_t)towupper(c);
  for (RunnerStatus st : getAllRunnerStatus())
    if (encodeStatus(st) == ustat) return st;
  return StatusUnknown;
}

bool oAbstractRunner::isResultStatus(RunnerStatus rs) {
  switch (rs) {
  case StatusOK: case StatusMP: case StatusDNF: case StatusDQ:
  case StatusMAX: case StatusNoTiming: case StatusOutOfCompetition:
    return true;
  default:
    return false;
  }
}

// -----------------------------------------------------------------------
// oAbstractRunner constructors
// -----------------------------------------------------------------------
oAbstractRunner::oAbstractRunner(oEvent* poe, bool loading) : oBase(poe) {
  tStartTime = 0;
  startTime  = 0;
  FinishTime = 0;
  tStatus = status = StatusUnknown;
  tEntryTouched = true;
  sqlChanged    = true;
  StartNo       = 0;
  inputPoints   = 0;
  if (loading || !oe || !oe->hasPrevStage())
    inputStatus = StatusOK;
  else
    inputStatus = StatusNotCompeting;
  inputTime  = 0;
  inputPlace = 0;
  tTimeAdjustment    = 0;
  tPointAdjustment   = 0;
  tAdjustDataRevision = -1;
}

// -----------------------------------------------------------------------
// oAbstractRunner: info
// -----------------------------------------------------------------------
wstring oAbstractRunner::getInfo() const { return getName(); }

// -----------------------------------------------------------------------
// oAbstractRunner: name
// -----------------------------------------------------------------------
void oAbstractRunner::setName(const wstring& n, bool /*manualUpdate*/) {
  if (n != sName) {
    sName = n;
    updateChanged();
  }
}

// -----------------------------------------------------------------------
// oAbstractRunner: class
// -----------------------------------------------------------------------
void oAbstractRunner::setClassId(int id, bool /*isManualUpdate*/) {
  pClass newClass = id > 0 ? oe->getClass(id) : nullptr;
  if (newClass != Class) {
    Class = newClass;
    updateChanged();
  }
}

const wstring& oAbstractRunner::getClass(bool virtualClass) const {
  if (!Class) return _EmptyWString;
  if (virtualClass) {
    pClass vc = Class->getVirtualClass(classInstance());
    return vc ? vc->getName() : Class->getName();
  }
  return Class->getName();
}

// -----------------------------------------------------------------------
// oAbstractRunner: club
// -----------------------------------------------------------------------
void oAbstractRunner::setClub(const wstring& clubName) {
  pClub pc = oe->getClub(clubName);
  if (pc != Club) {
    Club = pc;
    updateChanged();
  }
}

pClub oAbstractRunner::setClubId(int clubId) {
  pClub pc = Club;
  Club = oe->getClub(clubId);
  if (pc != Club) {
    updateChanged();
    if (Class) Class->tResultInfo.clear();
  }
  return Club;
}

// -----------------------------------------------------------------------
// oAbstractRunner: start/finish time
// -----------------------------------------------------------------------
bool oAbstractRunner::setStartTime(int t, bool updateSource, ChangeType changeType, bool /*recalculate*/) {
  bool ch = false;
  if (t != tStartTime) {
    ch = true;
    tStartTime = t;
    if (!updateSource) return ch;
  }
  if (t != startTime) {
    ch = true;
    startTime = t;
    if (updateSource) updateChanged(changeType);
  }
  return ch;
}

void oAbstractRunner::setStartTimeS(const wstring& t) {
  setStartTime(oe->getRelativeTime(t), true, ChangeType::Update);
}

void oAbstractRunner::setFinishTime(int t) {
  if (t != FinishTime) {
    FinishTime = t;
    tStatus = (t > 0 && status == StatusUnknown) ? StatusUnknown : tStatus;
    updateChanged();
  }
}

void oAbstractRunner::setFinishTimeS(const wstring& t) {
  setFinishTime(oe->getRelativeTime(t));
}

// -----------------------------------------------------------------------
// oAbstractRunner: running time
// -----------------------------------------------------------------------
int oAbstractRunner::getRunningTime(bool computedTime) const {
  if (computedTime && tComputedTime > 0) return tComputedTime;
  if (tStatus == StatusDNS || tStatus == StatusCANCEL ||
      tStatus == StatusDNF || tStatus == StatusNotCompeting)
    return 0;
  if (FinishTime > 0 && tStartTime > 0 && FinishTime > tStartTime)
    return FinishTime - tStartTime;
  return 0;
}

int oAbstractRunner::getTotalRunningTime() const {
  return getRunningTime(false);
}

int oAbstractRunner::getPrelRunningTime() const {
  if (FinishTime > 0 && tStatus != StatusDNS && tStatus != StatusCANCEL &&
      tStatus != StatusDNF && tStatus != StatusNotCompeting)
    return getRunningTime(true);
  return 0;
}

// -----------------------------------------------------------------------
// oAbstractRunner: status
// -----------------------------------------------------------------------
bool oAbstractRunner::setStatus(RunnerStatus st, bool updateSource, ChangeType changeType, bool /*recalculate*/) {
  bool ch = false;
  if (st != tStatus) {
    ch = true;
    bool someOK = (st == StatusOK) || (tStatus == StatusOK);
    tStatus = st;
    if (Class && someOK) Class->clearCache(false);
    if (st == StatusUnknown) tComputedStatus = StatusUnknown;
  }
  if (st != status) {
    status = st;
    if (updateSource) {
      updateChanged(changeType);
      setFlag(TransferFlags::FlagOutsideCompetition, st == StatusOutOfCompetition);
      setFlag(TransferFlags::FlagNoTiming,            st == StatusNoTiming);
    } else {
      changedObject();
    }
    ch = true;
  }
  return ch;
}

RunnerStatus oAbstractRunner::getTotalStatus(bool /*allowUpdate*/) const {
  return tStatus;
}

bool oAbstractRunner::isStatusOK(bool computed, bool allowUpdate) const {
  RunnerStatus st = computed ? getStatusComputed(allowUpdate) : tStatus;
  return st == StatusOK;
}

bool oAbstractRunner::isStatusUnknown(bool computed, bool allowUpdate) const {
  RunnerStatus st = computed ? getStatusComputed(allowUpdate) : tStatus;
  return st == StatusUnknown;
}

bool oAbstractRunner::hasResult() const {
  RunnerStatus st = getStatusComputed(true);
  if (st == StatusUnknown || st == StatusNotCompeting) return false;
  if (isPossibleResultStatus(st)) return runnerHasResult();
  return true;
}

// -----------------------------------------------------------------------
// oAbstractRunner: bib
// -----------------------------------------------------------------------
const wstring& oAbstractRunner::getBib() const {
  return getDCI().getString("Bib");
}

int oAbstractRunner::getEncodedBib() const {
  const wstring& b = getBib();
  if (b.empty()) return 0;
  int n = 0;
  for (wchar_t c : b) {
    if (c >= L'0' && c <= L'9') n = n * 10 + (c - L'0');
    else return 0;
  }
  return n;
}

// -----------------------------------------------------------------------
// oAbstractRunner: start/bib number
// -----------------------------------------------------------------------
void oAbstractRunner::setStartNo(int no, ChangeType changeType) {
  if (no != StartNo) {
    if (oe) oe->bibStartNoToRunnerTeam.clear();
    StartNo = no;
    updateChanged(changeType);
  }
}

// -----------------------------------------------------------------------
// oAbstractRunner: transfer flags
// -----------------------------------------------------------------------
bool oAbstractRunner::hasFlag(TransferFlags flag) const {
  return (getDCI().getInt("TransferFlags") & flag) != 0;
}

void oAbstractRunner::setFlag(TransferFlags flag, bool state) {
  int flags = getDI().getInt("TransferFlags");
  int newFlags = state ? (flags | flag) : (flags & ~flag);
  if (newFlags != flags) getDI().setInt("TransferFlags", newFlags);
}

// -----------------------------------------------------------------------
// oAbstractRunner: time adjustment
// -----------------------------------------------------------------------
int oAbstractRunner::getTimeAdjustment(bool includeBuiltinAdjustment) const {
  int adj = getDCI().getInt("TimeAdjust");
  if (includeBuiltinAdjustment) adj += getBuiltinAdjustment();
  return adj;
}

int oAbstractRunner::getPointAdjustment() const {
  return getDCI().getInt("PointAdjust");
}

void oAbstractRunner::setTimeAdjustment(int adjust) {
  getDI().setInt("TimeAdjust", adjust);
}

void oAbstractRunner::setPointAdjustment(int adjust) {
  getDI().setInt("PointAdjust", adjust);
}

// -----------------------------------------------------------------------
// oAbstractRunner: time string formatting
// -----------------------------------------------------------------------
const wstring& oAbstractRunner::getStartTimeS() const {
  thread_local wstring buf;
  buf = formatTimeMS(tStartTime, false);
  return buf;
}

const wstring& oAbstractRunner::getStartTimeCompact() const {
  return getStartTimeS();
}

const wstring& oAbstractRunner::getFinishTimeS(bool adjusted, SubSecond mode) const {
  thread_local wstring buf;
  int ft = adjusted ? getFinishTimeAdjusted(true) : FinishTime;
  buf = formatTimeMS(ft, mode != SubSecond::Off);
  return buf;
}

const wstring& oAbstractRunner::getTotalRunningTimeS(SubSecond mode) const {
  thread_local wstring buf;
  buf = formatTimeMS(getTotalRunningTime(), mode != SubSecond::Off);
  return buf;
}

const wstring& oAbstractRunner::getRunningTimeS(bool computedTime, SubSecond mode) const {
  thread_local wstring buf;
  buf = formatTimeMS(getRunningTime(computedTime), mode != SubSecond::Off);
  return buf;
}

// -----------------------------------------------------------------------
// oAbstractRunner: status string formatting
// -----------------------------------------------------------------------
const wstring& oAbstractRunner::getStatusS(bool /*formatForPrint*/, bool computedStatus) const {
  RunnerStatus st = computedStatus ? getStatusComputed(true) : tStatus;
  return encodeStatus(st, true);
}

const wstring& oAbstractRunner::getTotalStatusS(bool formatForPrint) const {
  return getStatusS(formatForPrint, true);
}

wstring oAbstractRunner::getIOFStatusS() const {
  switch (tStatus) {
  case StatusOK:              return L"OK";
  case StatusDNS:             return L"DidNotStart";
  case StatusDNF:             return L"DidNotFinish";
  case StatusMP:              return L"MissingPunch";
  case StatusDQ:              return L"Disqualified";
  case StatusMAX:             return L"OverTime";
  case StatusOutOfCompetition:return L"NotCompeting";
  case StatusNoTiming:        return L"Unofficial";
  default:                    return L"Inactive";
  }
}

wstring oAbstractRunner::getIOFTotalStatusS() const {
  return getIOFStatusS();
}

// -----------------------------------------------------------------------
// oAbstractRunner: place strings
// -----------------------------------------------------------------------
wstring oAbstractRunner::getPlaceS() const {
  int p = getPlace();
  return (p > 0 && p < 10000) ? itow(p) : _EmptyWString;
}

wstring oAbstractRunner::getPrintPlaceS(bool withDot) const {
  int p = getPlace();
  if (p <= 0 || p >= 10000) return _EmptyWString;
  wstring s = itow(p);
  if (withDot) s += L".";
  return s;
}

wstring oAbstractRunner::getTotalPlaceS() const {
  int p = getTotalPlace();
  return (p > 0 && p < 10000) ? itow(p) : _EmptyWString;
}

wstring oAbstractRunner::getPrintTotalPlaceS(bool withDot) const {
  int p = getTotalPlace();
  if (p <= 0 || p >= 10000) return _EmptyWString;
  wstring s = itow(p);
  if (withDot) s += L".";
  return s;
}

// -----------------------------------------------------------------------
// oAbstractRunner: input data (multi-day)
// -----------------------------------------------------------------------
void oAbstractRunner::resetInputData() {
  inputTime   = 0;
  inputStatus = StatusNotCompeting;
  inputPoints = 0;
  inputPlace  = 0;
}

void oAbstractRunner::setInputTime(const wstring& t) {
  inputTime = oe->getRelativeTime(t);
}

wstring oAbstractRunner::getInputTimeS() const {
  return formatTimeMS(inputTime, false);
}

void oAbstractRunner::setInputStatus(RunnerStatus s) { inputStatus = s; }

wstring oAbstractRunner::getInputStatusS() const {
  return encodeStatus(inputStatus, true);
}

void oAbstractRunner::setInputPoints(int p)  { inputPoints = p; }
void oAbstractRunner::setInputPlace(int p)   { inputPlace  = p; }

bool oAbstractRunner::isVacant() const {
  return Club && Club->isVacant();
}

// -----------------------------------------------------------------------
// oAbstractRunner: TempResult access
// -----------------------------------------------------------------------
const oAbstractRunner::TempResult& oAbstractRunner::getTempResult(int /*idx*/) const {
  return tmpResult;
}
oAbstractRunner::TempResult& oAbstractRunner::getTempResult() {
  return tmpResult;
}
void oAbstractRunner::setTempResultZero(const TempResult& tr) {
  tmpResult = tr;
}
void oAbstractRunner::updateComputedResultFromTemp() {
  tComputedStatus = tmpResult.status_;
  tComputedTime   = tmpResult.runningTime_;
  tComputedPoints = tmpResult.points_;
}

// -----------------------------------------------------------------------
// oAbstractRunner: fee
// -----------------------------------------------------------------------
int oAbstractRunner::getDefaultFee() const {
  return Class ? Class->getDCI().getInt("ClassFee") : 0;
}

int oAbstractRunner::getEntryFee() const {
  return getDCI().getInt("Fee");
}

bool oAbstractRunner::hasLateEntryFee() const { return false; }

void oAbstractRunner::addClassDefaultFee(bool /*resetFees*/) {}

int oAbstractRunner::getPaymentMode() const {
  return getDCI().getInt("PayMode");
}

void oAbstractRunner::setPaymentMode(int mode) {
  getDI().setInt("PayMode", mode);
}

// -----------------------------------------------------------------------
// oAbstractRunner: entry tracking
// -----------------------------------------------------------------------
int oAbstractRunner::getEntrySource() const { return getDCI().getInt("EntrySource"); }
void oAbstractRunner::setEntrySource(int src) { getDI().setInt("EntrySource", src); }
void oAbstractRunner::flagEntryTouched(bool flag) { tEntryTouched = flag; }
bool oAbstractRunner::isEntryTouched() const { return tEntryTouched; }

// -----------------------------------------------------------------------
// oAbstractRunner: preventRestart
// -----------------------------------------------------------------------
bool oAbstractRunner::preventRestart() const {
  return getDCI().getInt("NoRestart") != 0;
}
void oAbstractRunner::preventRestart(bool state) {
  getDI().setInt("NoRestart", state ? 1 : 0);
}

// -----------------------------------------------------------------------
// oAbstractRunner: speaker priority
// -----------------------------------------------------------------------
void oAbstractRunner::setSpeakerPriority(int pri) {
  getDI().setInt("Priority", pri);
}

int oAbstractRunner::getSpeakerPriority() const {
  return getDCI().getInt("Priority");
}

// -----------------------------------------------------------------------
// oAbstractRunner: birth age
// -----------------------------------------------------------------------
int oAbstractRunner::getBirthAge() const { return 0; }

// -----------------------------------------------------------------------
// oAbstractRunner: stage result
// -----------------------------------------------------------------------
RunnerStatus oAbstractRunner::getStageResult(int /*stage*/, int& time, int& point, int& place) const {
  time = 0; point = 0; place = 0;
  return StatusUnknown;
}

void oAbstractRunner::getInputResults(vector<RunnerStatus>& st, vector<int>& times,
                                       vector<int>& points, vector<int>& places) const {
  st.clear(); times.clear(); points.clear(); places.clear();
}

void oAbstractRunner::addToInputResult(int /*stageNo*/, const oAbstractRunner* /*src*/) {}

void oAbstractRunner::hasManuallyUpdatedTimeStatus() {
  if (oe) oe->dataRevision++;
}

// -----------------------------------------------------------------------
// oAbstractRunner: merge
// -----------------------------------------------------------------------
void oAbstractRunner::merge(const oBase& input, const oBase* base) {
  const auto& src = dynamic_cast<const oAbstractRunner&>(input);
  if (src.startTime != startTime) {
    startTime = src.startTime;
    tStartTime = src.tStartTime;
  }
  if (src.FinishTime != FinishTime) FinishTime = src.FinishTime;
  if (src.status != status) { status = src.status; tStatus = src.tStatus; }
  if (src.sName != sName) sName = src.sName;
}

// -----------------------------------------------------------------------
// compareClubs (static helper)
// -----------------------------------------------------------------------
int oAbstractRunner::compareClubs(const oClub* a, const oClub* b) {
  if (!a && !b) return 2;
  if (!a) return 0;
  if (!b) return 1;
  return a->getName() < b->getName() ? 1 : (b->getName() < a->getName() ? 0 : 2);
}

// -----------------------------------------------------------------------
// oRunner constructors / destructor
// -----------------------------------------------------------------------
oRunner::oRunner(oEvent* poe) : oAbstractRunner(poe, false) {
  Id            = oe->getFreeRunnerId();
  isTemporaryObject = false;
  tTimeAfter    = 0;
  tInitialTimeAfter = 0;
  tempRT        = 0;
  tempStatus    = StatusUnknown;
  Course        = nullptr;
  Card          = nullptr;
  cardNumber    = 0;
  tInTeam       = nullptr;
  tLeg          = 0;
  tLegEquClass  = 0;
  tNeedNoCard   = false;
  tUseStartPunch = true;
  correctionNeeded = false;
  tDuplicateLeg = 0;
  tParentRunner = nullptr;
  tCachedRunningTime   = 0;
  tSplitRevision       = -1;
  tRogainingPoints     = 0;
  tRogainingOvertime   = 0;
  tReduction           = 0;
  tRogainingPointsGross = 0;
  tAdaptedCourse         = nullptr;
  tAdaptedCourseRevision = -1;
  tShortenDataRevision   = -1;
  tNumShortening         = 0;
  getDI().initData();
  updateChanged();
}

oRunner::oRunner(oEvent* poe, int id) : oAbstractRunner(poe, true) {
  Id            = id;
  oe->qFreeRunnerId = max(id, oe->qFreeRunnerId);
  isTemporaryObject = false;
  Course        = nullptr;
  Card          = nullptr;
  cardNumber    = 0;
  tInTeam       = nullptr;
  tLeg          = 0;
  tLegEquClass  = 0;
  tNeedNoCard   = false;
  tUseStartPunch = true;
  correctionNeeded = false;
  tDuplicateLeg = 0;
  tParentRunner = nullptr;
  tCachedRunningTime   = 0;
  tSplitRevision       = -1;
  tRogainingPoints     = 0;
  tRogainingOvertime   = 0;
  tReduction           = 0;
  tRogainingPointsGross = 0;
  tAdaptedCourse         = nullptr;
  tAdaptedCourseRevision = -1;
  getDI().initData();
}

oRunner::~oRunner() {
  if (tInTeam) {
    for (size_t i = 0; i < tInTeam->Runners.size(); i++)
      if (tInTeam->Runners[i] && tInTeam->Runners[i]->getId() == Id)
        tInTeam->Runners[i] = nullptr;
    tInTeam = nullptr;
  }
  for (size_t k = 0; k < multiRunner.size(); k++) {
    if (multiRunner[k] && multiRunner[k]->tParentRunner == this)
      multiRunner[k]->tParentRunner = nullptr;
  }
  if (tParentRunner) {
    for (size_t k = 0; k < tParentRunner->multiRunner.size(); k++)
      if (tParentRunner->multiRunner[k] == this)
        tParentRunner->multiRunner[k] = nullptr;
  }
  delete tAdaptedCourse;
  tAdaptedCourse = nullptr;
}

// -----------------------------------------------------------------------
// oRunner: changedObject
// -----------------------------------------------------------------------
void oRunner::changedObject() {
  if (oe) {
    oe->sqlRunners.changed = true;
    oe->dataRevision++;
    oe->bibStartNoToRunnerTeam.clear();
    if (oe->classIdToRunnerHash) oe->classIdToRunnerHash->clear();
  }
}

// -----------------------------------------------------------------------
// oRunner: info/remove
// -----------------------------------------------------------------------
wstring oRunner::getInfo() const { return sName; }

void oRunner::remove() {
  Removed = true;
  if (Card) { Card->tOwner = nullptr; Card = nullptr; }
}

bool oRunner::canRemove() const { return true; }

// -----------------------------------------------------------------------
// oRunner: merge
// -----------------------------------------------------------------------
void oRunner::merge(const oBase& input, const oBase* base) {
  oAbstractRunner::merge(input, base);
}

// -----------------------------------------------------------------------
// oRunner: name
// -----------------------------------------------------------------------
const wstring& oRunner::getName() const {
  if (!tRealName.empty()) return tRealName;
  return sName;
}

void oRunner::setName(const wstring& in, bool manualUpdate) {
  if (in == sName) return;
  wstring oldName = sName;
  sName = in;
  tRealName.clear();
  updateChanged();
  // Propagate name to parent multi-runner if applicable
  if (tParentRunner) {
    tParentRunner->setName(in, manualUpdate);
    return;
  }
  // Propagate name to team if applicable
  if (tInTeam && (tInTeam->getName() == oldName)) {
    tInTeam->setName(in, manualUpdate);
  }
  for (pRunner mr : multiRunner)
    if (mr) mr->sName = sName;
}

const wstring& oRunner::getNameLastFirst() const {
  // Simple stub: return sName
  return sName;
}

void oRunner::getRealName(const wstring& input, wstring& output) const {
  output = input;
}

wstring oRunner::getGivenName() const {
  size_t sp = sName.find(L' ');
  return sp != wstring::npos ? sName.substr(0, sp) : sName;
}

wstring oRunner::getFamilyName() const {
  size_t sp = sName.rfind(L' ');
  return sp != wstring::npos ? sName.substr(sp + 1) : L"";
}

wstring oRunner::getCompactName() const {
  wstring given = getGivenName();
  wstring family = getFamilyName();
  if (given.empty() || family.empty()) return sName;
  thread_local wstring buf;
  buf.clear();
  buf += given[0];
  buf += L". ";
  buf += family;
  return buf;
}

const wstring& oRunner::getUIName() const { return getName(); }

bool oRunner::matchName(const wstring& pname) const {
  return sName == pname || tRealName == pname;
}

// -----------------------------------------------------------------------
// oRunner: class / course / club
// -----------------------------------------------------------------------
void oRunner::setClassId(int id, bool isManualUpdate) {
  if (tParentRunner) { tParentRunner->setClassId(id, isManualUpdate); return; }
  oAbstractRunner::setClassId(id, isManualUpdate);
  for (pRunner mr : multiRunner)
    if (mr) mr->Class = Class;
}

void oRunner::setClub(const wstring& name) {
  if (tParentRunner) { tParentRunner->setClub(name); return; }
  oAbstractRunner::setClub(name);
  propagateClub();
}

pClub oRunner::setClubId(int clubId) {
  if (tParentRunner) return tParentRunner->setClubId(clubId);
  oAbstractRunner::setClubId(clubId);
  propagateClub();
  return Club;
}

void oRunner::propagateClub() {
  for (pRunner mr : multiRunner) {
    if (mr && mr->Club != Club) {
      mr->Club = Club;
      mr->updateChanged();
    }
  }
  if (tInTeam && tInTeam->getClubRef() != Club &&
      ((Class && Class->getNumDistinctRunners() == 1) || tInTeam->getNumAssignedRunners() <= 1)) {
    tInTeam->setClubId(Club ? Club->getId() : 0);
    tInTeam->updateChanged();
  }
}

pCourse oRunner::getCourse(bool /*getAdaptedCourse*/) const {
  return Course;
}

const wstring& oRunner::getCourseName() const {
  return Course ? Course->getName() : _EmptyWString;
}

void oRunner::setCourseId(int id) {
  pCourse nc = id > 0 ? oe->getCourseById(id) : nullptr;
  if (nc != Course) {
    Course = nc;
    updateChanged();
  }
}

bool oRunner::useCoursePool() const {
  return Class && (Class->hasCoursePool() || getClassRef(true)->hasCoursePool());
}

// -----------------------------------------------------------------------
// oRunner: card
// -----------------------------------------------------------------------
void oRunner::setCardNo(int cno, bool /*matchCard*/, bool /*updateFromDatabase*/) {
  if (cno != getCardNo()) {
    cardNumber = cno;
    updateChanged();
    if (oe && oe->cardToRunnerHash && cno != 0)
      oe->cardToRunnerHash->insert({cno, this});
  }
}

int oRunner::setCard(int cardId) {
  int oldCardId = Card ? Card->Id : 0;
  if (cardId == 0) {
    if (Card) { Card->tOwner = nullptr; Card = nullptr; }
    return oldCardId;
  }
  pCard newCard = oe->getCard(cardId);
  if (newCard == Card) return oldCardId;
  if (Card) Card->tOwner = nullptr;
  Card = newCard;
  if (Card) {
    Card->tOwner = this;
    cardNumber = Card->getCardNo();
  }
  updateChanged();
  return oldCardId;
}

bool oRunner::needNoCard() const { return tNeedNoCard; }

bool oRunner::isRentalCard() const { return isRentalCard(getCardNo()); }

bool oRunner::isRentalCard(int /*cno*/) const { return false; }

int oRunner::getRentalCardFee(bool /*forAllRunners*/) const { return 0; }

void oRunner::setRentalCard(bool /*rental*/) {}

bool oRunner::payBeforeResult(bool /*checkFlagOnly*/) const { return hasFlag(FlagPayBeforeResult); }

void oRunner::setPaid(int paid) { getDI().setInt("Paid", paid); }

void oRunner::setFee(int fee) { getDI().setInt("Fee", fee); }

void oRunner::setPayBeforeResult(bool flag) { setFlag(FlagPayBeforeResult, flag); }

// -----------------------------------------------------------------------
// oRunner: start/bib
// -----------------------------------------------------------------------
void oRunner::setStartNo(int no, ChangeType changeType) {
  bool individualSno = false;
  if (Class) {
    if (Class->isQualificationFinalBaseClass()) individualSno = true;
    else if (Class->getLegType(0) == LegTypes::LTGroup) individualSno = true;
  }
  if (tInTeam) {
    if (tInTeam->getStartNo() == 0) {
      tInTeam->setStartNo(no, changeType);
      individualSno = false;
    } else if (!individualSno) {
      no = tInTeam->getStartNo();
    }
  }
  if (individualSno)
    oAbstractRunner::setStartNo(no, changeType);
  else if (tParentRunner)
    tParentRunner->setStartNo(no, changeType);
  else {
    oAbstractRunner::setStartNo(no, changeType);
    for (pRunner mr : multiRunner)
      if (mr) mr->oAbstractRunner::setStartNo(no, changeType);
  }
}

void oRunner::updateStartNo(int no) {
  setStartNo(no, ChangeType::Update);
  synchronize(true);
}

void oRunner::setBib(const wstring& bib, int /*bibNumerical*/, bool updateStarNo) {
  if (bib != getBib()) {
    getDI().setString("Bib", bib);
    updateChanged();
    if (oe) oe->bibStartNoToRunnerTeam.clear();
  }
}

int oRunner::getStartGroup(bool useTmp) const {
  if (useTmp && tmpStartGroup != 0) return tmpStartGroup;
  return getDCI().getInt("StartGroup");
}

void oRunner::setStartGroup(int sg) {
  getDI().setInt("StartGroup", sg);
}

// -----------------------------------------------------------------------
// oRunner: finish time
// -----------------------------------------------------------------------
void oRunner::setFinishTime(int t) {
  oAbstractRunner::setFinishTime(t);
}

int oRunner::getTimeAfter() const { return tTimeAfter; }
int oRunner::getTimeAfterCourse(bool /*considerClass*/) const { return tTimeAfter; }

// -----------------------------------------------------------------------
// oRunner: status
// -----------------------------------------------------------------------
RunnerStatus oRunner::getStatusComputed(bool /*allowUpdate*/) const {
  return tComputedStatus != StatusUnknown ? tComputedStatus : tStatus;
}

DynamicRunnerStatus oRunner::getDynamicStatus() const {
  if (tStatus == StatusDNS || tStatus == StatusCANCEL || tStatus == StatusNotCompeting)
    return DynamicRunnerStatus::StatusInactive;
  if (FinishTime > 0 || isResultStatus(tStatus))
    return DynamicRunnerStatus::StatusFinished;
  return DynamicRunnerStatus::StatusActive;
}

bool oRunner::runnerHasResult() const {
  if (!Card && getCourse(false)) return false;
  return getRunningTime(false) > 0;
}

int oRunner::getPlace(bool /*allowUpdate*/) const { return tPlace.get(false); }

int oRunner::getCoursePlace(bool perClass) const {
  return perClass ? tCourseClassPlace.get(false) : tCoursePlace.get(false);
}

int oRunner::getTotalPlace(bool /*allowUpdate*/) const {
  return tInTeam ? 0 : tTotalPlace.get(false);
}

// -----------------------------------------------------------------------
// oRunner: total running time
// -----------------------------------------------------------------------
int oRunner::getTotalRunningTime() const {
  return getRunningTime(false) + inputTime;
}

int oRunner::getTotalRunningTime(int /*time*/, bool computedTime, bool includeInput) const {
  int rt = getRunningTime(computedTime);
  return includeInput ? rt + inputTime : rt;
}

int oRunner::getRaceRunningTime(bool computedTime, int /*leg*/, bool /*allowUpdate*/) const {
  return getRunningTime(computedTime);
}

RunnerStatus oRunner::getTotalStatus(bool /*allowUpdate*/) const {
  if (inputStatus != StatusOK && inputStatus != StatusNotCompeting) return inputStatus;
  return tStatus;
}

// -----------------------------------------------------------------------
// oRunner: rogaining
// -----------------------------------------------------------------------
int oRunner::getRogainingPoints(bool computed, bool /*multidayTotal*/) const {
  return computed ? tRogainingPoints : tRogainingPoints;
}
int oRunner::getRogainingReduction(bool /*computed*/) const { return tReduction; }
int oRunner::getRogainingOvertime(bool /*computed*/) const { return tRogainingOvertime; }
int oRunner::getRogainingPointsGross(bool /*computed*/) const { return tRogainingPointsGross; }

oClass::RogainingAnalysis oRunner::getRogainingAnalysis(int /*from*/, int /*to*/) const {
  return {};
}

// -----------------------------------------------------------------------
// oRunner: multi-runner
// -----------------------------------------------------------------------
pRunner oRunner::getMultiRunner(int race) const {
  if (race == 0) return const_cast<pRunner>(this);
  if (race > 0 && race <= (int)multiRunner.size())
    return multiRunner[race - 1];
  return nullptr;
}

void oRunner::createMultiRunner(bool /*createMaster*/, bool /*sync*/) {}

wstring oRunner::getNameAndRace(bool /*useUIName*/) const {
  if (tDuplicateLeg > 0) {
    return sName + L" (" + itow(tDuplicateLeg + 1) + L")";
  }
  return sName;
}

pRunner oRunner::getPredecessor() const { return nullptr; }

void oRunner::clearOnChangedRunningTime() {
  tSplitRevision = -1;
}

void oRunner::changeId(int newId) {
  if (oe) oe->bibStartNoToRunnerTeam.clear();
  Id = newId;
}

// -----------------------------------------------------------------------
// oRunner: match / race info
// -----------------------------------------------------------------------
bool oRunner::matchAbstractRunner(const oAbstractRunner* target) const {
  return target == this;
}

bool oRunner::isResultUpdated(bool /*totalResult*/) const { return true; }

bool oRunner::startTimeAvailable() const {
  return tStartTime > 0 || startTime > 0;
}

int oRunner::classInstance() const {
  return tDuplicateLeg;
}

int oRunner::getRanking() const {
  return getDCI().getInt("Rank");
}

wstring oRunner::getRankingScore() const {
  thread_local wstring buf;
  buf = to_wstring(getDCI().getInt("Rank"));
  return buf;
}

void oRunner::setRankingScore(double score) {
  getDI().setInt("Rank", (int)score);
}

wstring oRunner::getEntryDate(bool /*useTeamEntryDate*/) const {
  thread_local wstring buf;
  buf = getDCI().getString("EntryDate");
  return buf;
}

const pair<wstring, int> oRunner::getRaceInfo() {
  return { sName, Id };
}

int oRunner::getRaceIdentifier() const {
  return getDCI().getInt("RaceId");
}

void oRunner::markClassChanged(int /*controlId*/) {
  if (oe) oe->dataRevision++;
}

int oRunner::getParResultLeg() const { return tLeg; }

bool oRunner::isAnnonumousTeamMember() const {
  return tInTeam && sName.empty();
}

bool oRunner::hasFinished() const {
  if (Card || FinishTime > 0) return true;
  if (tStatus == StatusUnknown) return false;
  return !isStatusUnknown(false, false);
}

int oRunner::getCheckTime() const { return 0; }

pRunner oRunner::getReference() const {
  int refId = getDCI().getInt("Reference");
  return refId > 0 ? oe->getRunner(refId, 0) : nullptr;
}

void oRunner::setReference(int runnerId) { getDI().setInt("Reference", runnerId); }

bool oRunner::canShareCard(const pRunner /*other*/, int /*newCardNumber*/) const { return true; }

// -----------------------------------------------------------------------
// oRunner: sex / birth
// -----------------------------------------------------------------------
void oRunner::setSex(PersonSex sex) {
  wstring s;
  switch (sex) {
  case sMale:   s = L"M"; break;
  case sFemale: s = L"F"; break;
  default:      s = L"";  break;
  }
  getDI().setString("Sex", s);
}

PersonSex oRunner::getSex() const {
  const wstring& s = getDCI().getString("Sex");
  if (s == L"M") return sMale;
  if (s == L"F") return sFemale;
  return sUnknown;
}

void oRunner::setBirthYear(int year) { getDI().setInt("BirthYear", year); }

void oRunner::setBirthDate(const wstring& date) {
  getDI().setString("BirthYear", date);
}

int oRunner::getBirthYear() const { return getDCI().getInt("BirthYear"); }

const wstring& oRunner::getBirthDate() const {
  return getDCI().getString("BirthYear");
}

int oRunner::getBirthAge() const {
  int y = getBirthYear();
  return y > 0 ? 2024 - y : 0;
}

void oRunner::setNationality(const wstring& nat) { getDI().setString("Nationality", nat); }
wstring oRunner::getNationality() const { return getDCI().getString("Nationality"); }

// -----------------------------------------------------------------------
// oRunner: input data (multi-day)
// -----------------------------------------------------------------------
void oRunner::setInputData(const oRunner& src) {
  inputTime   = src.inputTime;
  inputStatus = src.inputStatus;
  inputPoints = src.inputPoints;
  inputPlace  = src.inputPlace;
}

bool oRunner::isTransferCardNoNextStage() const {
  return (getDCI().getInt("TransferFlags") & FlagUpdateCard) != 0;
}

void oRunner::setTransferCardNoNextStage(bool state) {
  setFlag(FlagUpdateCard, state);
}

int oRunner::getTotalTimeInput() const { return inputTime; }
RunnerStatus oRunner::getTotalStatusInput() const { return inputStatus; }

// -----------------------------------------------------------------------
// oRunner: misc
// -----------------------------------------------------------------------
int oRunner::getBuiltinAdjustment() const { return 0; }

void oRunner::apply(ChangeType /*changeType*/, pRunner /*src*/) {
  tStartTime = startTime;
}

int oRunner::getTimeAfter(int /*leg*/, bool /*allowUpdate*/) const { return tTimeAfter; }

void oRunner::fillSpeakerObject(int, int, const vector<int>&, bool, oSpeakerObject&) const {}

bool oRunner::synchronizeAll(bool writeOnly) {
  return synchronize(writeOnly);
}

void oRunner::cloneStartTime(const pRunner r) {
  if (r) setStartTime(r->startTime, true, ChangeType::Update);
}

void oRunner::cloneData(const pRunner r) {
  if (!r) return;
  sName = r->sName;
  Club  = r->Club;
  Class = r->Class;
}

void oRunner::resetPersonalData() {
  sName.clear();
  Club  = nullptr;
  Class = nullptr;
}

bool oRunner::operator<(const oRunner& c) const {
  if (Class != c.Class) {
    if (!Class) return false;
    if (!c.Class) return true;
    return Class->getName() < c.Class->getName();
  }
  return sName < c.sName;
}

// -----------------------------------------------------------------------
// oRunner: getNumMulti / getMultiIndex
// -----------------------------------------------------------------------
int oRunner::getMultiIndex() {
  if (!tParentRunner) return 0;
  for (int i = 0; i < (int)tParentRunner->multiRunner.size(); i++)
    if (tParentRunner->multiRunner[i] == this) return i + 1;
  return -1;
}

vector<pRunner> oRunner::getRunnersOrdered() const { return {}; }

string oRunner::codeMultiR() const { return ""; }

void oRunner::decodeMultiR(const string& /*r*/) {}

void oRunner::correctRemove(pRunner r) {
  for (auto& mr : multiRunner)
    if (mr == r) { mr = nullptr; break; }
}

// -----------------------------------------------------------------------
// oRunner: split/stat stubs (full impl in US-003g2)
// -----------------------------------------------------------------------
bool oRunner::evaluateCard(bool doApply, vector<pair<int, pControl>>& missingPunches,
                            int addPunch, ChangeType changeType) {
  // Reset bad status values
  if (unsigned(status) >= 100u)
    status = StatusUnknown;

  pClass clz = getClassRef(true);
  missingPunches.clear();
  const int oldFT = FinishTime;
  const RunnerStatus oldStatus = tStatus;

  if (doApply) {
    tStartTime = startTime;
    tStatus    = status;
    apply(changeType, nullptr);
  }

  // Reset card punch match state
  if (Card) {
    for (auto& p : Card->punches) {
      p.tRogainingIndex          = -1;
      p.anyRogainingMatchControlId = -1;
      p.tRogainingPoints         = 0;
      p.isUsed                   = false;
      p.tIndex                   = -1;
      p.tMatchControlId          = -1;
      p.clearTimeAdjust();
    }
  }

  bool inTeam = tInTeam != nullptr;
  tProblemDescription.clear();
  tReduction            = 0;
  tRogainingPointsGross = 0;
  tRogainingOvertime    = 0;

  vector<SplitData> oldTimes;
  swap(splitTimes, oldTimes);

  if (!Card) {
    if ((inTeam || !tUseStartPunch) && doApply)
      apply(changeType, nullptr);
    storeTimes();
    normalizedSplitTimes.clear();
    if (!oldTimes.empty() && clz)
      clz->clearSplitAnalysis();
    return false;
  }

  if (!clz)
    return false;

  if (clz->ignoreStartPunch() && tStartTime > 0)
    tUseStartPunch = false;

  const pCourse course = getCourse(true);

  if (!course) {
    // No-course mode: extract start/finish from card
    for (auto& p : Card->punches) {
      if (p.isStart() && tUseStartPunch)
        tStartTime = p.getTimeInt();
      else if (p.isFinish())
        setFinishTime(p.getTimeInt());
    }
    if ((inTeam || !tUseStartPunch) && doApply)
      apply(changeType, nullptr);
    storeTimes();

    int maxTimeStatus = 0;
    if (FinishTime <= 0) {
      tStatus = StatusDNF;
    } else {
      int mt = clz->getMaximumRunnerTime();
      if (mt > 0)
        maxTimeStatus = (getRunningTime(false) > mt) ? 1 : 2;
      else
        maxTimeStatus = 2;

      if (tStatus == StatusMAX && maxTimeStatus == 2)
        tStatus = StatusUnknown;
    }
    if (payBeforeResult(false))
      tStatus = StatusDQ;
    else if (tStatus == StatusUnknown || tStatus == StatusCANCEL ||
             tStatus == StatusDNS    || tStatus == StatusMAX)
      tStatus = (maxTimeStatus == 1) ? StatusMAX : StatusOK;
    return false;
  }

  // -----------------------------------------------------------------------
  // Full course-matching mode
  // -----------------------------------------------------------------------
  int startPunchCode  = course->getStartPunchType();
  int finishPunchCode = course->getFinishPunchType();
  bool hasRogaining   = course->hasRogaining();

  // Build rogaining map: control-code → (controlIndex, points)
  map<int, pair<int,int>> rogainingMap;
  unordered_set<int> requiredRG;
  for (int k = 0; k < course->nControls(); k++) {
    pControl ctrl = course->getControl(k);
    if (ctrl && ctrl->isRogaining(hasRogaining)) {
      int pts = ctrl->getRogainingPoints();
      vector<int> nums;
      ctrl->getNumbers(nums);
      for (int num : nums)
        rogainingMap[num] = {k, pts};
      if (ctrl->getStatus() == oControl::ControlStatus::StatusRogainingRequired)
        requiredRG.insert(k);
    }
  }

  // Find and apply start punch
  bool clearSplitAnalysis = false;
  for (auto& p : Card->punches) {
    if (p.type == startPunchCode) {
      if (tUseStartPunch && p.getAdjustedTime() != tStartTime) {
        p.clearTimeAdjust();
        int newST = p.getAdjustedTime();
        if (newST != tStartTime)
          clearSplitAnalysis = true;
        tStartTime = newST;
      }
      break;
    }
  }

  // Build punchCount / expectedPunchCount for multi-occurrence detection
  map<int,int> punchCount;
  map<int,int> expectedPunchCount;
  map<int,int> controlToBase;

  auto addBaseControl = [&](int code, int base) -> int {
    auto res = controlToBase.emplace(code, base);
    if (res.second)
      return base;
    if (base != code && base != res.first->second)
      res.first->second = -1;
    return res.first->second;
  };

  auto getBaseControl = [&](int code) -> int {
    auto it = controlToBase.find(code);
    return it != controlToBase.end() ? it->second : -1;
  };

  for (int k = 0; k < course->nControls(); k++) {
    pControl ctrl = course->getControl(k);
    if (ctrl && !ctrl->isRogaining(hasRogaining)) {
      vector<int> nums;
      ctrl->getNumbers(nums);
      if (ctrl->getStatus() == oControl::ControlStatus::StatusMultiple) {
        for (int num : nums)
          ++expectedPunchCount[addBaseControl(num, num)];
      } else {
        constexpr int LargeCode = 1000000;
        int bc = LargeCode;
        for (int num : nums)
          bc = min(bc, addBaseControl(num, nums.empty() ? num : nums[0]));
        if (bc > 0 && bc < LargeCode)
          ++expectedPunchCount[bc];
      }
    }
  }

  for (auto& p : Card->punches) {
    if (p.type >= 10 && p.type <= 1024) {
      int bc = getBaseControl(p.type);
      if (bc > 0)
        ++punchCount[bc];
    }
  }

  // Initialize split times with NOTATIME sentinel
  splitTimes.resize(course->nControls(),
                    SplitData(NOTATIME, SplitData::SplitStatus::Missing));

  auto p_it = Card->punches.begin();
  int k = 0;

  for (k = 0; k < course->nControls(); k++) {
    // Skip start/finish/check punches
    while (p_it != Card->punches.end() &&
           (p_it->isCheck() || p_it->isFinish() || p_it->isStart())) {
      p_it->clearTimeAdjust();
      ++p_it;
    }
    if (p_it == Card->punches.end())
      break;

    auto tp_it = p_it;
    pControl ctrl = course->getControl(k);
    int skippedPunches = 0;

    if (ctrl) {
      int timeAdjustCtrl = ctrl->getTimeAdjust();
      ctrl->startCheckControl();

      if (ctrl->getStatus() == oControl::ControlStatus::StatusBad    ||
          ctrl->getStatus() == oControl::ControlStatus::StatusOptional ||
          ctrl->getStatus() == oControl::ControlStatus::StatusBadNoTiming) {
        if (tp_it != Card->punches.end() && ctrl->hasNumberUnchecked(tp_it->type)) {
          tp_it->isUsed         = true;
          tp_it->setTimeAdjust(timeAdjustCtrl);
          tp_it->tMatchControlId = ctrl->getId();
          tp_it->tIndex          = k;
          splitTimes[k].setPunchTime(tp_it->getAdjustedTime());
          ++tp_it;
          p_it = tp_it;
        }
      } else {
        while (!ctrl->controlCompleted(hasRogaining) && tp_it != Card->punches.end()) {
          if (ctrl->hasNumberUnchecked(tp_it->type)) {
            if (skippedPunches > 0 &&
                ctrl->getStatus() == oControl::ControlStatus::StatusOK) {
              int bc = getBaseControl(tp_it->type);
              if (bc != -1 && expectedPunchCount[bc] > 1 &&
                  punchCount[bc] < expectedPunchCount[bc]) {
                int savedCode = tp_it->type;
                ctrl->uncheckNumber(savedCode);
                tp_it = Card->punches.end();
                break;
              }
            }
            tp_it->isUsed          = true;
            tp_it->setTimeAdjust(timeAdjustCtrl);
            tp_it->tMatchControlId  = ctrl->getId();
            tp_it->tIndex           = k;
            if (ctrl->controlCompleted(hasRogaining))
              splitTimes[k].setPunchTime(tp_it->getAdjustedTime());
            ++tp_it;
            p_it = tp_it;
          } else {
            skippedPunches++;
            tp_it->isUsed = false;
            ++tp_it;
          }
        }
      }

      if (ctrl->controlCompleted(hasRogaining) &&
          splitTimes[k].getTime(false) == NOTATIME)
        splitTimes[k].setPunched();
    } else {
      splitTimes[k].setNotPunched();
    }

    if (ctrl && !ctrl->controlCompleted(hasRogaining))
      ctrl->addUncheckedPunches(missingPunches, hasRogaining);
  }

  // Remaining controls get their missing punches added
  while (k < course->nControls()) {
    pControl ctrl = course->getControl(k);
    if (ctrl) {
      ctrl->startCheckControl();
      ctrl->addUncheckedPunches(missingPunches, hasRogaining);
    }
    k++;
  }

  // Mark remaining card punches as unused
  while (p_it != Card->punches.end()) {
    p_it->isUsed      = false;
    p_it->tIndex      = -1;
    p_it->clearTimeAdjust();
    ++p_it;
  }

  bool OK = missingPunches.empty();

  // Rogaining
  tRogaining.clear();
  tRogainingPoints = 0;
  int time_limit   = 0;

  if (!rogainingMap.empty()) {
    time_limit = course->getMaximumRogainingTime();
    bool countAllControls = (course->getDCI().getInt("NoLatePoints") == 0);

    unordered_set<int> visitedControls;
    for (auto& p : Card->punches) {
      auto it = rogainingMap.find(p.type);
      if (it != rogainingMap.end()) {
        int ctrlIdx = it->second.first;
        int pts     = it->second.second;
        pControl rc = course->getControl(ctrlIdx);
        if (!rc) continue;
        p.anyRogainingMatchControlId = rc->getId();
        p.setTimeAdjust(rc->getTimeAdjust());
        if (visitedControls.insert(ctrlIdx).second) {
          requiredRG.erase(ctrlIdx);
          p.isUsed          = true;
          p.tRogainingIndex = ctrlIdx;
          p.tMatchControlId = p.anyRogainingMatchControlId;
          p.tRogainingPoints = pts;
          tRogaining.push_back({rc, p.getAdjustedTime()});
          splitTimes[ctrlIdx].setPunchTime(p.getAdjustedTime());

          int rtHere = p.getAdjustedTime() - tStartTime;
          if (countAllControls || rtHere <= 0 || time_limit <= 0 || rtHere <= time_limit)
            tRogainingPoints += pts;
        }
      }
    }

    for (int mpIdx : requiredRG) {
      pControl rc = course->getControl(mpIdx);
      if (rc)
        missingPunches.emplace_back(rc->getFirstNumber(), rc);
    }

    OK = missingPunches.empty();
    tRogainingPoints = max(0, tRogainingPoints + getPointAdjustment());

    int point_limit = course->getMinimumRogainingPoints();
    if (point_limit > 0 && tRogainingPoints < point_limit) {
      tProblemDescription = L"X points missing.#" + itow(point_limit - tRogainingPoints);
      OK = false;
    }

    for (int ki = 0; ki < course->nControls(); ki++) {
      pControl ctrl = course->getControl(ki);
      if (ctrl && ctrl->isRogaining(hasRogaining))
        if (!visitedControls.count(ki))
          splitTimes[ki].setNotPunched();
    }
  }

  // Determine max-time status
  int maxTimeStatus = 0;
  // (FinishTime set from card below, so maxTimeStatus is finalized after)

  if ((tStatus == StatusMAX && maxTimeStatus == 2) ||
       tStatus == StatusOutOfCompetition ||
       tStatus == StatusNoTiming)
    tStatus = StatusUnknown;

  if (payBeforeResult(false))
    tStatus = StatusDQ;
  else if (OK && (tStatus == StatusUnknown || tStatus == StatusDNS ||
                  tStatus == StatusCANCEL  || tStatus == StatusMP  ||
                  tStatus == StatusOK      || tStatus == StatusDNF))
    tStatus = StatusOK;
  else
    tStatus = RunnerStatus(max(int(StatusMP), int(tStatus)));

  // Find finish punch
  auto backIter = Card->punches.rbegin();
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
  } else if (FinishTime <= 0) {
    tStatus     = RunnerStatus(max(int(StatusDNF), int(tStatus)));
    tProblemDescription = L"Finish time missing.";
    FinishTime  = 0;
  }

  // Finalize max-time status now that FinishTime is known
  if (clz && FinishTime > 0) {
    int mt = clz->getMaximumRunnerTime();
    if (mt > 0)
      maxTimeStatus = (getRunningTime(false) > mt) ? 1 : 2;
    else
      maxTimeStatus = 2;
  }

  if (tStatus == StatusOK && maxTimeStatus == 1)
    tStatus = StatusMAX;

  if (!missingPunches.empty()) {
    tProblemDescription = L"Missing punches: X#" + itow(missingPunches[0].first);
    for (unsigned j = 1; j < 3 && j < missingPunches.size(); j++)
      tProblemDescription += L", " + itow(missingPunches[j].first);
    if (missingPunches.size() > 3) tProblemDescription += L"...";
    else tProblemDescription += L".";
  }

  if (tStatus == StatusOK) {
    if (hasFlag(TransferFlags::FlagOutsideCompetition))
      tStatus = StatusOutOfCompetition;
    else if (hasFlag(TransferFlags::FlagNoTiming))
      tStatus = StatusNoTiming;
    else if (clz && clz->getNoTiming())
      tStatus = StatusNoTiming;
  }

  doAdjustTimes(course);
  tRogainingPointsGross = tRogainingPoints;

  if (oldStatus != tStatus || oldFT != FinishTime)
    clearSplitAnalysis = true;

  if (oldFT != FinishTime)
    updateChanged(changeType);

  if ((inTeam || !tUseStartPunch) && doApply)
    apply(changeType, nullptr);

  if (tCachedRunningTime != FinishTime - tStartTime) {
    tCachedRunningTime = FinishTime - tStartTime;
    clearSplitAnalysis = true;
  }

  if (time_limit > 0) {
    int rt = getRunningTime(false);
    if (rt > 0) {
      int overTime = rt - time_limit;
      if (overTime > 0) {
        tRogainingOvertime = overTime;
        tReduction         = course->calculateReduction(overTime);
        tProblemDescription = L"Time penalty: X points.#" + itow(tReduction);
        tRogainingPoints    = max(0, tRogainingPoints - tReduction);
      }
    }
  }

  // Cache invalidation
  bool doClear = (splitTimes.size() != oldTimes.size() || clearSplitAnalysis);
  for (size_t ki = 0; !doClear && ki < oldTimes.size(); ki++) {
    if (splitTimes[ki].getTime(false) != oldTimes[ki].getTime(false))
      doClear = true;
  }
  if (doClear) {
    normalizedSplitTimes.clear();
    if (clz) clz->clearSplitAnalysis();
  }

  if (doApply)
    storeTimes();

  if (clz && changeType == ChangeType::Update && getRunningTime(false) > 0)
    oe->reEvaluateAll({clz->getId()}, true);

  return true;
}

void oRunner::addCard(pCard /*card*/, vector<pair<int, pControl>>& /*missingPunches*/) {}

void oRunner::setupRunnerStatistics() const {}

bool oRunner::storeTimes() { return storeTimesAux(Class); }

bool oRunner::storeTimesAux(pClass /*targetClass*/) { return false; }

void oRunner::doAdjustTimes(pCourse /*course*/) {
  // Control-level time adjustments are already applied via oPunch::setTimeAdjust
  // during evaluateCard. Full adapted-course adjustment deferred to oEvent migration.
}

const vector<SplitData>& oRunner::getSplitTimes(bool /*normalized*/) const { return splitTimes; }
void oRunner::getSplitAnalysis(vector<int>& v) const { v.clear(); }
void oRunner::getLegPlaces(vector<int>& v) const { v.clear(); }
void oRunner::getLegTimeAfter(vector<int>& v) const { v.clear(); }
void oRunner::getLegPlacesAcc(vector<ResultData>& v) const { v.clear(); }
void oRunner::getLegTimeAfterAcc(vector<ResultData>& v) const { v.clear(); }

int oRunner::getSplitTime(int, bool) const { return 0; }
int oRunner::getTimeAdjust(int) const { return 0; }
int oRunner::getNamedSplit(int) const { return 0; }

const wstring& oRunner::getNamedSplitS(int, SubSecond) const { return _EmptyWString; }

int oRunner::getPunchTime(int, bool, bool, bool) const { return 0; }

const wstring& oRunner::getPunchTimeS(int, bool, bool, bool, SubSecond) const { return _EmptyWString; }

const wstring& oRunner::getSplitTimeS(int, bool, SubSecond) const { return _EmptyWString; }

void oRunner::getSplitTime(int, RunnerStatus& stat, int& rt) const { stat = StatusUnknown; rt = 0; }

int oRunner::getMissedTime() const { return 0; }
int oRunner::getMissedTime(int) const { return 0; }
wstring oRunner::getMissedTimeS() const { return _EmptyWString; }
wstring oRunner::getMissedTimeS(int) const { return _EmptyWString; }
int oRunner::getLegPlace(int) const { return 0; }
int oRunner::getLegTimeAfter(int) const { return 0; }
int oRunner::getLegPlaceAcc(int, bool) const { return 0; }
int oRunner::getLegTimeAfterAcc(int, bool) const { return 0; }
int oRunner::getTimeWhenPlaceFixed() const { return 0; }

// -----------------------------------------------------------------------
// oRunner: ext identifier 2
// -----------------------------------------------------------------------
int64_t oRunner::getExtIdentifier2() const { return getDCI().getInt64("ExtId2"); }
void oRunner::setExtIdentifier2(int64_t id) { getDI().setInt64("ExtId2", id); }
wstring oRunner::getExtIdentifierString2() const {
  return to_wstring(getExtIdentifier2());
}
void oRunner::setExtIdentifier2(const wstring& str) {
  try { setExtIdentifier2(stoll(str)); } catch (...) {}
}

// -----------------------------------------------------------------------
// oRunner: formatting helpers
// -----------------------------------------------------------------------
wstring oRunner::getCompleteIdentification(IDType, NameType) const { return sName; }

wstring oRunner::formatName(NameFormat style) const {
  switch (style) {
  case NameFormat::LastFirst: return getFamilyName() + L", " + getGivenName();
  default: return sName;
  }
}

void oRunner::getNameFormats(vector<pair<wstring, size_t>>& out) {
  out = { {L"Default", 0}, {L"First Last", 1}, {L"Last, First", 2} };
}

oRunner::BibAssignResult oRunner::autoAssignBib() { return BibAssignResult::NoBib; }

pRunner oRunner::nextNeedReadout() const { return nullptr; }

pRunner oRunner::getMatchedRunner(const SICard&) const { return nullptr; }

// -----------------------------------------------------------------------
// oRunner: setExtraPersonData
// -----------------------------------------------------------------------
void oRunner::setExtraPersonData(const wstring& sex, const wstring& nationality,
                                  const wstring& rank, wstring& /*phone*/,
                                  const wstring& bib, const wstring& /*text*/,
                                  int dataA, int dataB) {
  getDI().setString("Sex", sex);
  getDI().setString("Nationality", nationality);
  if (!rank.empty()) {
    try { getDI().setInt("Rank", stoi(rank)); } catch (...) {}
  }
  if (!bib.empty()) getDI().setString("Bib", bib);
  getDI().setInt("DataA", dataA);
  getDI().setInt("DataB", dataB);
}

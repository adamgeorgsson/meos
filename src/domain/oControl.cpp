#include "oControl.h"
#include "oDataContainer.h"
#include "oEvent.h"

#include <cassert>
#include <cstdio>
#include <cstring>

using namespace std;

// -----------------------------------------------------------------------
// Static DataContainer
// -----------------------------------------------------------------------

oDataContainer& oControl::container() {
  static oDataContainer dc(64);
  static bool initialized = false;
  if (!initialized) {
    dc.addVariableInt("MinTime", oDataContainer::oISTime, "MinTime");
    dc.addVariableInt("TimeAdjust", oDataContainer::oISTimeAdjust, "TimeAdjust");
    dc.addVariableInt("Rogaining", oDataContainer::oIS16, "Rogaining");
    dc.addVariableInt("Radio", oDataContainer::oIS8, "Radio");
    initialized = true;
  }
  return dc;
}

oDataContainer& oControl::getDataBuffers(pvoid& data, pvoid& olddata,
                                          pvectorstr& strData) const {
  data = &dataMap_;
  olddata = &oldDataMap_;
  strData = nullptr;
  return container();
}

// -----------------------------------------------------------------------
// Construction / Destruction
// -----------------------------------------------------------------------

oControl::oControl(oEvent* poe) : oBase(poe) {
  getDI().initData();
  nNumbers = 0;
  Status = ControlStatus::StatusOK;
  tMissedTimeMax = 0;
  tMissedTimeTotal = 0;
  tNumVisitorsActual = 0;
  tNumVisitorsExpected = 0;
  tMissedTimeMedian = 0;
  tMistakeQuotient = 0;
  tNumRunnersRemaining = 0;
  tStatDataRevision = -1;
  tHasFreePunchLabel = false;
  tNumberDuplicates = 0;
}

oControl::oControl(oEvent* poe, int id) : oBase(poe) {
  Id = id;
  getDI().initData();
  nNumbers = 0;
  Status = ControlStatus::StatusOK;
  tMissedTimeMax = 0;
  tMissedTimeTotal = 0;
  tNumVisitorsActual = 0;
  tNumVisitorsExpected = 0;
  tMistakeQuotient = 0;
  tMissedTimeMedian = 0;
  tNumRunnersRemaining = 0;
  tStatDataRevision = -1;
  tHasFreePunchLabel = false;
  tNumberDuplicates = 0;
}

oControl::~oControl() {}

// -----------------------------------------------------------------------
// Static helpers
// -----------------------------------------------------------------------

pair<int, int> oControl::getIdIndexFromCourseControlId(int courseControlId) {
  return make_pair(courseControlId % 100000, courseControlId / 100000);
}

int oControl::getCourseControlIdFromIdIndex(int controlId, int index) {
  assert(controlId < 100000);
  return controlId + index * 100000;
}

// -----------------------------------------------------------------------
// Core setters
// -----------------------------------------------------------------------

void oControl::set(int pId, int pNumber, wstring pName) {
  Id = pId;
  Numbers[0] = pNumber;
  nNumbers = 1;
  Name = pName;
  updateChanged();
}

void oControl::setStatus(ControlStatus st) {
  if (st != Status) {
    Status = st;
    updateChanged();
  }
}

void oControl::setName(const wstring& name) {
  if (name == getDefaultName()) {
    if (!Name.empty()) {
      Name = L"";
      updateChanged();
    }
  } else if (name != getName()) {
    Name = name;
    updateChanged();
  }
}

// -----------------------------------------------------------------------
// Name / id accessors
// -----------------------------------------------------------------------

const wstring& oControl::getName() const {
  if (!Name.empty())
    return Name;
  else
    return getDefaultName();
}

const wstring& oControl::getDefaultName() const {
  wchar_t bf[16];
  swprintf(bf, sizeof(bf) / sizeof(wchar_t), L"[%d]", Id);
  static thread_local wstring cache;
  cache = bf;
  return cache;
}

wstring oControl::getIdS() const {
  if (!Name.empty())
    return Name;
  else {
    wchar_t bf[16];
    swprintf(bf, sizeof(bf) / sizeof(wchar_t), L"%d", Id);
    return bf;
  }
}

// -----------------------------------------------------------------------
// Number management
// -----------------------------------------------------------------------

int oControl::getFirstNumber() const {
  return nNumbers > 0 ? Numbers[0] : 0;
}

void oControl::getNumbers(vector<int>& numbers) const {
  numbers.clear();
  for (int k = 0; k < nNumbers; k++)
    numbers.push_back(Numbers[k]);
}

wstring oControl::codeNumbers(char sep) const {
  wstring n;
  wchar_t bf[16];
  for (int i = 0; i < nNumbers; i++) {
    swprintf(bf, 16, L"%d", Numbers[i]);
    n += bf;
    if (i + 1 < nNumbers)
      n += wchar_t(sep);
  }
  return n;
}

bool oControl::decodeNumbers(string s) {
  const char* str = s.c_str();
  nNumbers = 0;
  while (*str) {
    int cid = atoi(str);
    while (*str && (*str != ';' && *str != ',' && *str != ' ')) str++;
    while (*str && (*str == ';' || *str == ',' || *str == ' ')) str++;
    if (cid > 0 && cid < 1024 && nNumbers < 32)
      Numbers[nNumbers++] = cid;
  }
  if (nNumbers == 0) {
    Numbers[0] = Id;
    nNumbers = 1;
    return false;
  }
  return true;
}

bool oControl::setNumbers(const wstring& numbers) {
  int nn = nNumbers;
  int bf[32];
  if (unsigned(nNumbers) < 32)
    memcpy(bf, Numbers, sizeof(int) * nNumbers);

  string nnumbers(numbers.begin(), numbers.end());
  bool success = decodeNumbers(nnumbers);

  if (!success) {
    memcpy(Numbers, bf, sizeof(int) * nn);
    nNumbers = nn;
  }

  if (nNumbers != nn || memcmp(bf, Numbers, sizeof(int) * nNumbers) != 0) {
    updateChanged();
    if (oe)
      oe->punchIndex.clear();
  }
  return success;
}

bool oControl::hasNumber(int i) {
  for (int n = 0; n < nNumbers; n++)
    if (Numbers[n] == i) {
      checkedNumbers[n] = true;
      return true;
    }
  return nNumbers == 0;
}

bool oControl::uncheckNumber(int i) {
  for (int n = 0; n < nNumbers; n++)
    if (Numbers[n] == i) {
      checkedNumbers[n] = false;
      return true;
    }
  return false;
}

bool oControl::hasNumberUnchecked(int i) {
  for (int n = 0; n < nNumbers; n++)
    if (Numbers[n] == i && !checkedNumbers[n]) {
      checkedNumbers[n] = true;
      return true;
    }
  return nNumbers == 0;
}

// -----------------------------------------------------------------------
// String representation
// -----------------------------------------------------------------------

wstring oControl::getString() {
  wstring num;
  if (Status == ControlStatus::StatusMultiple)
    num = codeNumbers('+');
  else if (Status == ControlStatus::StatusRogaining ||
           Status == ControlStatus::StatusRogainingRequired) {
    int pts = getRogainingPoints();
    wstring ptsStr = oe ? oe->formatScore(pts) : itow(pts);
    num = codeNumbers('|') + L" (" + ptsStr + L"p)";
  } else
    num = codeNumbers('|');

  if (Status == ControlStatus::StatusBad ||
      Status == ControlStatus::StatusBadNoTiming)
    return L"\u26A0" + num;
  if (Status == ControlStatus::StatusOptional)
    return L"\u2b59" + num;
  return num;
}

wstring oControl::getLongString() {
  if (Status == ControlStatus::StatusOK ||
      Status == ControlStatus::StatusNoTiming) {
    if (nNumbers == 1)
      return codeNumbers('|');
    else
      return L"VALFRI(" + codeNumbers(',') + L")";
  } else if (Status == ControlStatus::StatusMultiple) {
    return L"ALLA(" + codeNumbers(',') + L")";
  } else if (Status == ControlStatus::StatusRogaining ||
             Status == ControlStatus::StatusRogainingRequired) {
    int pts = getRogainingPoints();
    wstring ptsStr = oe ? oe->formatScore(pts) : itow(pts);
    return L"RG(" + codeNumbers(',') + L"|" + ptsStr + L"p)";
  } else {
    return L"TRASIG(" + codeNumbers(',') + L")";
  }
}

const wstring oControl::getStatusS() const {
  switch (getStatus()) {
    case ControlStatus::StatusOK:             return L"OK";
    case ControlStatus::StatusBad:            return L"Trasig";
    case ControlStatus::StatusOptional:       return L"Valfri";
    case ControlStatus::StatusMultiple:       return L"Multipel";
    case ControlStatus::StatusRogaining:      return L"Rogaining";
    case ControlStatus::StatusRogainingRequired: return L"Rogaining Obligatorisk";
    case ControlStatus::StatusStart:          return L"Start";
    case ControlStatus::StatusCheck:          return L"Check";
    case ControlStatus::StatusFinish:         return L"Mal";
    case ControlStatus::StatusNoTiming:       return L"Utan tidtagning";
    case ControlStatus::StatusBadNoTiming:    return L"Forsvunnen";
    default:                                  return L"Okand";
  }
}

// -----------------------------------------------------------------------
// Multi-punch
// -----------------------------------------------------------------------

int oControl::getNumMulti() {
  return Status == ControlStatus::StatusMultiple ? nNumbers : 1;
}

// -----------------------------------------------------------------------
// Punch checking
// -----------------------------------------------------------------------

void oControl::startCheckControl() {
  for (int k = 0; k < nNumbers; k++)
    checkedNumbers[k] = false;
}

int oControl::getMissingNumber() const {
  for (int k = 0; k < nNumbers; k++)
    if (!checkedNumbers[k])
      return Numbers[k];
  assert(false);
  return Numbers[0];
}

bool oControl::controlCompleted(bool supportRogaining) const {
  if (Status == ControlStatus::StatusOK ||
      Status == ControlStatus::StatusNoTiming ||
      ((Status == ControlStatus::StatusRogaining ||
        Status == ControlStatus::StatusRogainingRequired) &&
       !supportRogaining)) {
    for (int k = 0; k < nNumbers; k++)
      if (checkedNumbers[k]) return true;
    return nNumbers == 0;
  } else if (Status == ControlStatus::StatusMultiple) {
    for (int k = 0; k < nNumbers; k++)
      if (!checkedNumbers[k]) return false;
    return true;
  }
  return true;
}

void oControl::addUncheckedPunches(vector<pair<int, oControl*>>& mp,
                                    bool supportRogaining) const {
  if (controlCompleted(supportRogaining)) return;
  for (int k = 0; k < nNumbers; k++)
    if (!checkedNumbers[k]) {
      mp.emplace_back(Numbers[k], const_cast<oControl*>(this));
      if (Status != ControlStatus::StatusMultiple) return;
    }
}

// -----------------------------------------------------------------------
// Cache and DI accessors
// -----------------------------------------------------------------------

void oControl::setupCache() const {
  if (!oe || tCache.dataRevision != oe->dataRevision) {
    tCache.timeAdjust = getDCI().getInt("TimeAdjust");
    tCache.minTime = getDCI().getInt("MinTime");
    if (oe) tCache.dataRevision = oe->dataRevision;
  }
}

int oControl::getMinTime() const {
  if (Status == ControlStatus::StatusNoTiming ||
      Status == ControlStatus::StatusBadNoTiming)
    return 0;
  setupCache();
  return tCache.minTime;
}

int oControl::getTimeAdjust() const {
  setupCache();
  return tCache.timeAdjust;
}

wstring oControl::getTimeAdjustS() const {
  return formatTimeMS(getTimeAdjust(), false, SubSecond::Auto);
}

wstring oControl::getMinTimeS() const {
  if (getMinTime() > 0)
    return formatTimeMS(getMinTime(), false, SubSecond::Auto);
  else
    return makeDash(L"-");
}

int oControl::getRogainingPoints() const {
  return getDCI().getInt("Rogaining");
}

wstring oControl::getRogainingPointsS() const {
  return oe ? oe->formatScore(getRogainingPoints()) : itow(getRogainingPoints());
}

bool oControl::setTimeAdjust(int v) {
  if (v == NOTIME) v = 0;
  return getDI().setInt("TimeAdjust", v);
}

bool oControl::setTimeAdjust(const wstring& t) {
  return setTimeAdjust(convertAbsoluteTimeMS(t));
}

void oControl::setMinTime(int v) {
  if (v < 0 || v == NOTIME) v = 0;
  getDI().setInt("MinTime", v);
}

void oControl::setMinTime(const wstring& t) {
  setMinTime(convertAbsoluteTimeMS(t));
}

void oControl::setRogainingPoints(int v) {
  getDI().setInt("Rogaining", v);
}

void oControl::setRadio(bool r) {
  // 1 = radio, 2 = no radio, 0 = default
  getDI().setInt("Radio", r ? 1 : 2);
}

bool oControl::isValidRadio() const {
  int flag = getDCI().getInt("Radio");
  if (flag == 0)
    return (tHasFreePunchLabel || hasName()) &&
           getStatus() == oControl::ControlStatus::StatusOK;
  return flag == 1;
}

// -----------------------------------------------------------------------
// Statistics (require full oEvent with runner lists — return cached or 0)
// -----------------------------------------------------------------------

int oControl::getMissedTimeTotal() const {
  if (oe && tStatDataRevision != oe->getRevision())
    oe->setupControlStatistics();
  return tMissedTimeTotal;
}

int oControl::getMissedTimeMax() const {
  if (oe && tStatDataRevision != oe->getRevision())
    oe->setupControlStatistics();
  return tMissedTimeMax;
}

int oControl::getMissedTimeMedian() const {
  if (oe && tStatDataRevision != oe->getRevision())
    oe->setupControlStatistics();
  return tMissedTimeMedian;
}

int oControl::getMistakeQuotient() const {
  if (oe && tStatDataRevision != oe->getRevision())
    oe->setupControlStatistics();
  return tMistakeQuotient;
}

int oControl::getNumVisitors(bool actualVisits) const {
  if (oe && tStatDataRevision != oe->getRevision())
    oe->setupControlStatistics();
  return actualVisits ? tNumVisitorsActual : tNumVisitorsExpected;
}

int oControl::getNumRunnersRemaining() const {
  if (oe && tStatDataRevision != oe->getRevision())
    oe->setupControlStatistics();
  return tNumRunnersRemaining;
}

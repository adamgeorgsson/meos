// oControl.cpp — Migrated from legacy code/oControl.cpp (US-003b).
// Cross-platform, no Win32 dependencies.

#include "oControl.h"
#include "oDataContainer.h"  // needed for oDataInterface / oDataConstInterface full definitions
#include "oEvent.h"
#include "oCourse.h"
#include "../util/meos_util.h"
#include "../util/localizer.h"
#include "../util/xmlparser.h"
#include "../util/timeconstants.hpp"
#include <cassert>
#include <cstring>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

oControl::oControl(oEvent* poe) : oBase(poe)
{
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

oControl::oControl(oEvent* poe, int id) : oBase(poe)
{
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

oControl::~oControl() = default;

// ── Static helpers ──────────────────────────────────────────────────────────

pair<int, int> oControl::getIdIndexFromCourseControlId(int courseControlId) {
  return make_pair(courseControlId % 100000, courseControlId / 100000);
}

int oControl::getCourseControlIdFromIdIndex(int controlId, int index) {
  assert(controlId < 100000);
  return controlId + index * 100000;
}

// ── XML serialization ───────────────────────────────────────────────────────

bool oControl::write(xmlparser& xml)
{
  if (Removed) return true;

  xml.startTag("Control");
  xml.write("Id", Id);
  xml.write("Updated", getStamp());
  xml.write("Name", Name);
  xml.write("Numbers", codeNumbers());
  xml.write("Status", int(Status));
  getDI().write(xml);
  xml.endTag();
  return true;
}

void oControl::set(int pId, int pNumber, wstring pName)
{
  Id = pId;
  Numbers[0] = pNumber;
  nNumbers = 1;
  Name = pName;
  updateChanged();
}

void oControl::set(const xmlobject* xo)
{
  xmlList xl;
  xo->getObjects(xl);
  nNumbers = 0;
  Numbers[0] = 0;

  for (auto it = xl.begin(); it != xl.end(); ++it) {
    if (it->is("Id"))
      Id = it->getInt();
    else if (it->is("Number")) {
      Numbers[0] = it->getInt();
      nNumbers = 1;
    }
    else if (it->is("Numbers"))
      decodeNumbers(it->getRawStr());
    else if (it->is("Status"))
      Status = static_cast<ControlStatus>(it->getInt());
    else if (it->is("Name")) {
      Name = it->getWStr();
      if (Name.size() > 1 && Name.at(0) == L'%')
        Name = lang.tl(Name.substr(1));
    }
    else if (it->is("Updated"))
      Modified.setStamp(it->getRawStr());
    else if (it->is("oData"))
      getDI().set(*it);
  }
}

// ── Status / name setters ───────────────────────────────────────────────────

void oControl::setStatus(ControlStatus st)
{
  if (st != Status) {
    Status = st;
    updateChanged();
  }
}

void oControl::setName(const wstring& name)
{
  if (name == getDefaultName()) {
    if (!Name.empty()) {
      Name = L"";
      updateChanged();
    }
  }
  else if (name != getName()) {
    Name = name;
    updateChanged();
  }
}

// ── Number encoding / decoding ──────────────────────────────────────────────

wstring oControl::codeNumbers(char sep) const
{
  wstring n;
  wchar_t bf[16];
  for (int i = 0; i < nNumbers; i++) {
    swprintf(bf, 16, L"%d", Numbers[i]);
    n += bf;
    if (i + 1 < nNumbers)
      n += static_cast<wchar_t>(sep);
  }
  return n;
}

bool oControl::decodeNumbers(string s)
{
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

bool oControl::setNumbers(const wstring& numbers)
{
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
    oe->punchIndex.clear();
  }

  return success;
}

void oControl::getNumbers(vector<int>& numbers) const
{
  numbers.resize(nNumbers);
  for (int i = 0; i < nNumbers; i++)
    numbers[i] = Numbers[i];
}

int oControl::getFirstNumber() const
{
  return (nNumbers > 0) ? Numbers[0] : 0;
}

// ── String representations ──────────────────────────────────────────────────

wstring oControl::getString()
{
  wstring num;
  if (Status == ControlStatus::StatusMultiple)
    num = codeNumbers('+');
  else if (Status == ControlStatus::StatusRogaining ||
           Status == ControlStatus::StatusRogainingRequired)
    num = codeNumbers('|') + L" (" +
          (getRogainingPoints() != 0 ? oe->formatScore(getRogainingPoints()) : L"0") + L"p)";
  else
    num = codeNumbers('|');

  if (Status == ControlStatus::StatusBad || Status == ControlStatus::StatusBadNoTiming)
    return L"\u26A0" + num;
  if (Status == ControlStatus::StatusOptional)
    return L"\u2b59" + num;
  return num;
}

wstring oControl::getLongString()
{
  if (Status == ControlStatus::StatusOK || Status == ControlStatus::StatusNoTiming) {
    if (nNumbers == 1)
      return codeNumbers('|');
    return wstring(lang.tl("VALFRI(")) + codeNumbers(',') + L")";
  }
  if (Status == ControlStatus::StatusMultiple)
    return wstring(lang.tl("ALLA(")) + codeNumbers(',') + L")";
  if (Status == ControlStatus::StatusRogaining || Status == ControlStatus::StatusRogainingRequired)
    return wstring(lang.tl("RG(")) + codeNumbers(',') + L"|" +
           (getRogainingPoints() != 0 ? oe->formatScore(getRogainingPoints()) : L"0") + L"p)";
  return wstring(lang.tl("TRASIG(")) + codeNumbers(',') + L")";
}

const wstring oControl::getStatusS() const
{
  switch (getStatus()) {
    case ControlStatus::StatusOK:                  return lang.tl("OK");
    case ControlStatus::StatusBad:                 return lang.tl("Trasig");
    case ControlStatus::StatusOptional:            return lang.tl("Valfri");
    case ControlStatus::StatusMultiple:            return lang.tl("Multipel");
    case ControlStatus::StatusRogaining:           return lang.tl("Rogaining");
    case ControlStatus::StatusRogainingRequired:   return lang.tl("Rogaining Obligatorisk");
    case ControlStatus::StatusStart:               return lang.tl("Start");
    case ControlStatus::StatusCheck:               return lang.tl("Check");
    case ControlStatus::StatusFinish:              return lang.tl(L"M\u00e5l");
    case ControlStatus::StatusNoTiming:            return lang.tl("Utan tidtagning");
    case ControlStatus::StatusBadNoTiming:         return lang.tl("F\u00f6rsvunnen");
    default:                                       return lang.tl("Ok\u00e4nd");
  }
}

wstring oControl::getInfo() const
{
  return getName();
}

const wstring& oControl::getName() const
{
  if (!Name.empty())
    return Name;
  return getDefaultName();
}

const wstring& oControl::getDefaultName() const
{
  wchar_t bf[16];
  swprintf(bf, sizeof(bf) / sizeof(wchar_t), L"[%d]", Id);
  wstring& res = StringCache::getInstance().wget();
  res = bf;
  return res;
}

wstring oControl::getIdS() const
{
  if (!Name.empty())
    return Name;
  wchar_t bf[16];
  swprintf(bf, sizeof(bf) / sizeof(wchar_t), L"%d", Id);
  return bf;
}

// ── Data buffer interface ────────────────────────────────────────────────────

oDataContainer& oControl::getDataBuffers(pvoid& data, pvoid& olddata, pvectorstr& strData) const
{
  data = reinterpret_cast<pvoid>(const_cast<BYTE*>(oData));
  olddata = reinterpret_cast<pvoid>(const_cast<BYTE*>(oDataOld));
  strData = nullptr;
  return *oe->oControlData;
}

// ── Cache helpers ────────────────────────────────────────────────────────────

void oControl::setupCache() const
{
  if (tCache.dataRevision != static_cast<int>(oe->dataRevision)) {
    tCache.timeAdjust = getDCI().getInt("TimeAdjust");
    tCache.minTime    = getDCI().getInt("MinTime");
    tCache.dataRevision = static_cast<int>(oe->dataRevision);
  }
}

void oControl::clearCache()
{
  tCache.dataRevision = -1;
}

// ── Time adjustment / min-time ───────────────────────────────────────────────

int oControl::getTimeAdjust() const
{
  setupCache();
  return tCache.timeAdjust;
}

wstring oControl::getTimeAdjustS() const
{
  return formatTimeMS(getTimeAdjust(), false, SubSecond::Auto);
}

bool oControl::setTimeAdjust(int v)
{
  if (v == NOTIME)
    v = 0;
  return getDI().setInt("TimeAdjust", v);
}

bool oControl::setTimeAdjust(const wstring& s)
{
  return setTimeAdjust(convertAbsoluteTimeMS(s));
}

int oControl::getMinTime() const
{
  if (Status == ControlStatus::StatusNoTiming || Status == ControlStatus::StatusBadNoTiming)
    return 0;
  setupCache();
  return tCache.minTime;
}

wstring oControl::getMinTimeS() const
{
  if (getMinTime() > 0)
    return formatTimeMS(getMinTime(), false, SubSecond::Auto);
  return makeDash(L"-");
}

void oControl::setMinTime(int v)
{
  if (v < 0 || v == NOTIME)
    v = 0;
  getDI().setInt("MinTime", v);
}

void oControl::setMinTime(const wstring& s)
{
  setMinTime(convertAbsoluteTimeMS(s));
}

// ── Rogaining ───────────────────────────────────────────────────────────────

int oControl::getRogainingPoints() const
{
  return getDCI().getInt("Rogaining");
}

wstring oControl::getRogainingPointsS() const
{
  return oe->formatScore(getRogainingPoints());
}

void oControl::setRogainingPoints(int v)
{
  getDI().setInt("Rogaining", v);
}

void oControl::setRogainingPoints(const wstring& s)
{
  setRogainingPoints(oe->convertScore(s));
}

// ── Radio flag ───────────────────────────────────────────────────────────────

void oControl::setRadio(bool r)
{
  // 1 = radio, 2 = no radio, 0 = default
  getDI().setInt("Radio", r ? 1 : 2);
}

bool oControl::isValidRadio() const
{
  int flag = getDCI().getInt("Radio");
  if (flag == 0)
    return (tHasFreePunchLabel || hasName()) && getStatus() == ControlStatus::StatusOK;
  return flag == 1;
}

// ── Punch checking ───────────────────────────────────────────────────────────

void oControl::startCheckControl()
{
  for (int k = 0; k < nNumbers; k++)
    checkedNumbers[k] = false;
}

bool oControl::hasNumber(int i)
{
  for (int n = 0; n < nNumbers; n++)
    if (Numbers[n] == i) {
      checkedNumbers[n] = true;
      return true;
    }
  return nNumbers == 0;
}

bool oControl::hasNumberUnchecked(int i)
{
  for (int n = 0; n < nNumbers; n++)
    if (Numbers[n] == i && !checkedNumbers[n]) {
      checkedNumbers[n] = true;
      return true;
    }
  return nNumbers == 0;
}

bool oControl::uncheckNumber(int i)
{
  for (int n = 0; n < nNumbers; n++)
    if (Numbers[n] == i) {
      checkedNumbers[n] = false;
      return true;
    }
  return false;
}

int oControl::getNumMulti()
{
  return (Status == ControlStatus::StatusMultiple) ? nNumbers : 1;
}

void oControl::addUncheckedPunches(vector<pair<int, pControl>>& mp, bool supportRogaining) const
{
  if (controlCompleted(supportRogaining))
    return;

  for (int k = 0; k < nNumbers; k++)
    if (!checkedNumbers[k]) {
      mp.emplace_back(Numbers[k], pControl(this));
      if (Status != ControlStatus::StatusMultiple)
        return;
    }
}

int oControl::getMissingNumber() const
{
  for (int k = 0; k < nNumbers; k++)
    if (!checkedNumbers[k])
      return Numbers[k];
  assert(false);
  return Numbers[0];
}

bool oControl::controlCompleted(bool supportRogaining) const
{
  if (Status == ControlStatus::StatusOK || Status == ControlStatus::StatusNoTiming ||
      ((Status == ControlStatus::StatusRogaining ||
        Status == ControlStatus::StatusRogainingRequired) && !supportRogaining)) {
    for (int k = 0; k < nNumbers; k++)
      if (checkedNumbers[k])
        return true;
    return nNumbers == 0;
  }
  if (Status == ControlStatus::StatusMultiple) {
    for (int k = 0; k < nNumbers; k++)
      if (!checkedNumbers[k])
        return false;
    return true;
  }
  return true;
}

// ── Statistics (delegated to oEvent) ─────────────────────────────────────────

int oControl::getMissedTimeTotal() const
{
  if (tStatDataRevision != oe->getRevision())
    oe->setupControlStatistics();
  return tMissedTimeTotal;
}

int oControl::getMissedTimeMax() const
{
  if (tStatDataRevision != oe->getRevision())
    oe->setupControlStatistics();
  return tMissedTimeMax;
}

int oControl::getMissedTimeMedian() const
{
  if (tStatDataRevision != oe->getRevision())
    oe->setupControlStatistics();
  return tMissedTimeMedian;
}

int oControl::getMistakeQuotient() const
{
  if (tStatDataRevision != oe->getRevision())
    oe->setupControlStatistics();
  return tMistakeQuotient;
}

int oControl::getNumVisitors(bool actualVisits) const
{
  if (tStatDataRevision != oe->getRevision())
    oe->setupControlStatistics();
  return actualVisits ? tNumVisitorsActual : tNumVisitorsExpected;
}

int oControl::getNumRunnersRemaining() const
{
  if (tStatDataRevision != oe->getRevision())
    oe->setupControlStatistics();
  return tNumRunnersRemaining;
}

// ── Course-control duplicates ────────────────────────────────────────────────

int oControl::getNumberDuplicates() const
{
  return tNumberDuplicates;
}

void oControl::getCourseControls(vector<int>& cc) const
{
  cc.resize(tNumberDuplicates);
  for (int i = 0; i < tNumberDuplicates; i++)
    cc[i] = getCourseControlIdFromIdIndex(Id, i);
}

// ── Relationship queries ─────────────────────────────────────────────────────

void oControl::getCourses(vector<pCourse>& crs) const
{
  crs.clear();
  for (auto it = oe->Courses.begin(); it != oe->Courses.end(); ++it) {
    if (!it->isRemoved() && it->hasControl(this))
      crs.push_back(pCourse(&*it));
  }
}

void oControl::getClasses(vector<pClass>& cls) const
{
  vector<pCourse> crs;
  getCourses(crs);
  std::set<int> cid;
  for (auto& c : crs)
    cid.insert(c->getId());

  for (auto it = oe->Classes.begin(); it != oe->Classes.end(); ++it) {
    if (!it->isRemoved() && it->hasAnyCourse(cid))
      cls.push_back(pClass(&*it));
  }
}

int oControl::getControlIdByName(const oEvent& oe, const string& name)
{
  if (_stricmp(name.c_str(), "finish") == 0) return oPunch::PunchFinish;
  if (_stricmp(name.c_str(), "start")  == 0) return oPunch::PunchStart;

  vector<pControl> ac;
  oe.getControls(ac, true);
  wstring wname = oe.recodeToWide(name);
  for (pControl c : ac) {
    if (compareStringIgnoreCase(c->getName(), wname) == 0)
      return c->getId();
  }
  return 0;
}

// ── Unit-type helpers ────────────────────────────────────────────────────────

bool oControl::isUnit() const
{
  return isSpecialControl(getStatus()) && getUnitCode() > 0;
}

int oControl::getUnitCode() const
{
  return getDCI().getInt("Unit");
}

oPunch::SpecialPunch oControl::getUnitType() const
{
  switch (getStatus()) {
    case ControlStatus::StatusFinish: return oPunch::SpecialPunch::PunchFinish;
    case ControlStatus::StatusStart:  return oPunch::SpecialPunch::PunchStart;
    case ControlStatus::StatusCheck:  return oPunch::SpecialPunch::PunchCheck;
    default: break;
  }
  throw std::runtime_error("getUnitType: invalid control status");
}

// ── Lifecycle ────────────────────────────────────────────────────────────────

void oControl::remove()
{
  if (oe) oe->removeControl(Id);
}

bool oControl::canRemove() const
{
  return !oe->isControlUsed(Id);
}

void oControl::changedObject()
{
  if (oe) oe->globalModification = true;
  oe->sqlControls.changed = true;
}

// ── GUI stubs (table/input coupling removed) ─────────────────────────────────

void oControl::addTableRow(Table& /*table*/) const
{
  // No-op: GUI coupling removed
}

pair<int, bool> oControl::inputData(int /*id*/, const wstring& /*input*/,
                                     int /*inputId*/, wstring& /*output*/, bool /*noUpdate*/)
{
  return {0, false};
}

void oControl::fillInput(int /*id*/, vector<pair<wstring, size_t>>& /*elements*/,
                          size_t& /*selected*/)
{
  // No-op: GUI coupling removed
}

// ── Merge ────────────────────────────────────────────────────────────────────

void oControl::merge(const oBase& input, const oBase* base)
{
  const oControl& src = dynamic_cast<const oControl&>(input);
  if (src.Name.length() > 0)
    setName(src.Name);
  setNumbers(src.codeNumbers());
  setStatus(src.getStatus());
  if (getDI().merge(input, base))
    updateChanged();
  synchronize(true);
}

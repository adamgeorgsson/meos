// oRunner.cpp — oRunner and oAbstractRunner implementation (US-003g).
// Cross-platform, no Win32 / GUI dependencies.

#include "../util/Table.h"      // Table stub + TID_* constants
#include "oRunner.h"
#include "oEvent.h"
#include "oDataContainer.h"
#include "oCard.h"
#include "qualification_final.h"
#include "../util/xmlparser.h"

// ── Static data ───────────────────────────────────────────────────────────────
// Ordering: lower value = better result (0=OK, higher=worse/unfinished).
// Matches initialization from legacy meos.cpp.
char RunnerStatusOrderMap[100] = {
  // Index = RunnerStatus enum value; all others default to 0.
};

namespace {
  struct RunnerStatusOrderMapInit {
    RunnerStatusOrderMapInit() {
      for (int k = 0; k < 100; k++) RunnerStatusOrderMap[k] = 9; // default: unranked
      RunnerStatusOrderMap[StatusOK]              = 0;
      RunnerStatusOrderMap[StatusNoTiming]        = 1;
      RunnerStatusOrderMap[StatusOutOfCompetition]= 2;
      RunnerStatusOrderMap[StatusMAX]             = 3;
      RunnerStatusOrderMap[StatusMP]              = 4;
      RunnerStatusOrderMap[StatusDNF]             = 5;
      RunnerStatusOrderMap[StatusDQ]              = 6;
      RunnerStatusOrderMap[StatusCANCEL]          = 7;
      RunnerStatusOrderMap[StatusDNS]             = 8;
      RunnerStatusOrderMap[StatusUnknown]         = 9;
      RunnerStatusOrderMap[StatusNotCompeting]    = 10;
    }
  } runnerStatusOrderMapInitializer;
}

// ── oAbstractRunner::DynamicValue ─────────────────────────────────────────────

bool oAbstractRunner::DynamicValue::isOld(const oEvent& oe) const {
  return revision != oe.dataRevision;
}

void oAbstractRunner::DynamicValue::update(const oEvent& oe, int val) const {
  value    = val;
  revision = oe.dataRevision;
}

// ── oAbstractRunner constructor ───────────────────────────────────────────────

oAbstractRunner::oAbstractRunner(oEvent* poe, bool /*loading*/)
  : oBase(poe)
{}

// ── oAbstractRunner::merge ────────────────────────────────────────────────────

void oAbstractRunner::merge(const oBase& /*input*/, const oBase* /*base*/) {
  // Base merge — subclasses override as needed
}

// ── oAbstractRunner::setClub ──────────────────────────────────────────────────

void oAbstractRunner::setClub(const wstring& name) {
  if (!oe) return;
  if (name.empty()) { Club = nullptr; return; }
  Club = oe->getClubCreate(0, name);
}

// ── oAbstractRunner::setClubId ────────────────────────────────────────────────

pClub oAbstractRunner::setClubId(int id) {
  if (!oe || id == 0) { Club = nullptr; return nullptr; }
  Club = oe->getClub(id);
  return Club;
}

// ── oAbstractRunner::setClassId ───────────────────────────────────────────────

void oAbstractRunner::setClassId(int id, bool /*isManualUpdate*/) {
  if (!oe || id == 0) { Class = nullptr; return; }
  Class = oe->getClass(id);
}

// ── oAbstractRunner::setStatus ────────────────────────────────────────────────

bool oAbstractRunner::setStatus(RunnerStatus st, bool updatePermanent,
                                 ChangeType ct, bool /*recalculate*/) {
  if (tStatus != st) tStatus = st;
  if (updatePermanent && status != st) {
    status = st;
    updateChanged(ct);
    return true;
  }
  return false;
}

// ── oAbstractRunner::setStartTime ────────────────────────────────────────────

bool oAbstractRunner::setStartTime(int t, bool updatePermanent, ChangeType ct,
                                    bool /*recalculate*/) {
  if (tStartTime != t) tStartTime = t;
  if (updatePermanent && startTime != t) {
    startTime = t;
    updateChanged(ct);
    return true;
  }
  return false;
}

// ── oAbstractRunner::setFinishTime ────────────────────────────────────────────

void oAbstractRunner::setFinishTime(int t) {
  if (FinishTime != t) {
    FinishTime      = t;
    finishTimeWasSet = true;
    changedObject();
  }
}

// ── oAbstractRunner::getRunningTime ──────────────────────────────────────────

int oAbstractRunner::getRunningTime(bool /*computed*/) const {
  if (FinishTime > 0 &&
      tStatus != StatusDNS &&
      tStatus != StatusCANCEL &&
      tStatus != StatusDNF &&
      tStatus != StatusNotCompeting)
  {
    return FinishTime - tStartTime;
  }
  return 0;
}

// ── oAbstractRunner::getClass ────────────────────────────────────────────────

const wstring& oAbstractRunner::getClass(bool /*virtualClass*/) const {
  if (Class) return Class->getName();
  return _EmptyWString;
}

// ── oAbstractRunner::getBib ───────────────────────────────────────────────────

wstring oAbstractRunner::getBib() const {
  return getDCI().getString("Bib");
}

// ── oAbstractRunner::encodeStatus / decodeStatus ─────────────────────────────

wstring oAbstractRunner::encodeStatus(RunnerStatus s) {
  switch (s) {
    case StatusOK:              return L"OK";
    case StatusNoTiming:        return L"NoTiming";
    case StatusMP:              return L"MP";
    case StatusDNF:             return L"DNF";
    case StatusDQ:              return L"DQ";
    case StatusMAX:             return L"MAX";
    case StatusOutOfCompetition: return L"OutOfCompetition";
    case StatusDNS:             return L"DNS";
    case StatusCANCEL:          return L"CANCEL";
    case StatusNotCompeting:    return L"NotCompeting";
    case StatusUnknown:         return L"Unknown";
    default:                    return L"Unknown";
  }
}

RunnerStatus oAbstractRunner::decodeStatus(const wstring& s) {
  if (s == L"OK")               return StatusOK;
  if (s == L"NoTiming")         return StatusNoTiming;
  if (s == L"MP")               return StatusMP;
  if (s == L"DNF")              return StatusDNF;
  if (s == L"DQ")               return StatusDQ;
  if (s == L"MAX")              return StatusMAX;
  if (s == L"OutOfCompetition") return StatusOutOfCompetition;
  if (s == L"DNS")              return StatusDNS;
  if (s == L"CANCEL")           return StatusCANCEL;
  if (s == L"NotCompeting")     return StatusNotCompeting;
  return StatusUnknown;
}

// ── oAbstractRunner::getStatusS ──────────────────────────────────────────────

const wstring& oAbstractRunner::getStatusS(bool /*shortFormat*/, SubSecond /*mode*/) const {
  static const wstring sUnknown   = L"-";
  static const wstring sOK        = L"OK";
  static const wstring sNoTiming  = L"Ej tid";
  static const wstring sMP        = L"Felst.";
  static const wstring sDNF       = L"Utgått";
  static const wstring sDQ        = L"Disk.";
  static const wstring sMAX       = L"Maxtid";
  static const wstring sOOC       = L"Utom tävlan";
  static const wstring sDNS       = L"EJ start";
  static const wstring sCANCEL    = L"Struken";
  static const wstring sNotComp   = L"Deltar ej";

  switch (tStatus) {
    case StatusOK:               return sOK;
    case StatusNoTiming:         return sNoTiming;
    case StatusMP:               return sMP;
    case StatusDNF:              return sDNF;
    case StatusDQ:               return sDQ;
    case StatusMAX:              return sMAX;
    case StatusOutOfCompetition: return sOOC;
    case StatusDNS:              return sDNS;
    case StatusCANCEL:           return sCANCEL;
    case StatusNotCompeting:     return sNotComp;
    default:                     return sUnknown;
  }
}

// ── oRunner constructors ──────────────────────────────────────────────────────

oRunner::oRunner(oEvent* poe)
  : oAbstractRunner(poe, false)
{
  Id = poe ? poe->getFreeRunnerId() : 1;
  memset(oData,    0, sizeof(oData));
  memset(oDataOld, 0, sizeof(oDataOld));
  if (poe && poe->oRunnerData)
    poe->oRunnerData->initData(this, dataSize);
}

oRunner::oRunner(oEvent* poe, int id)
  : oAbstractRunner(poe, false)
{
  Id = id;
  memset(oData,    0, sizeof(oData));
  memset(oDataOld, 0, sizeof(oDataOld));
  if (poe && poe->oRunnerData)
    poe->oRunnerData->initData(this, dataSize);
}

oRunner::~oRunner() = default;

// ── oRunner::getDataBuffers ───────────────────────────────────────────────────

oDataContainer& oRunner::getDataBuffers(pvoid& data, pvoid& olddata,
                                         pvectorstr& strData) const {
  data    = const_cast<BYTE*>(oData);
  olddata = const_cast<BYTE*>(oDataOld);
  strData = const_cast<vector<vector<wstring>>*>(&oDataStr);
  return *oe->oRunnerData;
}

// ── oRunner::changedObject ────────────────────────────────────────────────────

void oRunner::changedObject() {
  if (Class) {
    // Invalidate class caches
    Class->clearCache(false);
  }
  if (oe) oe->dataRevision++;
}

// ── oRunner::setName ──────────────────────────────────────────────────────────

void oRunner::setName(const wstring& n, bool /*manualUpdate*/) {
  // Collapse runs of whitespace to a single space and trim
  wstring normalized;
  bool lastWasSpace = true;  // trim leading
  for (wchar_t c : n) {
    if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
      if (!lastWasSpace) { normalized += L' '; lastWasSpace = true; }
    } else {
      normalized += c;
      lastWasSpace = false;
    }
  }
  // trim trailing
  if (!normalized.empty() && normalized.back() == L' ')
    normalized.pop_back();

  if (sName != normalized || tRealName != normalized) {
    sName     = normalized;
    tRealName = normalized;
    changedObject();
  }
}

// ── oRunner::getName ─────────────────────────────────────────────────────────

const wstring& oRunner::getName() const {
  return tRealName;
}

// ── oRunner::getNameAndRace ───────────────────────────────────────────────────

wstring oRunner::getNameAndRace(bool /*withRace*/) const {
  return tRealName;
}

// ── oRunner::setClub ─────────────────────────────────────────────────────────

void oRunner::setClub(const wstring& name) {
  oAbstractRunner::setClub(name);
}

// ── oRunner::setClubId ────────────────────────────────────────────────────────

pClub oRunner::setClubId(int id) {
  return oAbstractRunner::setClubId(id);
}

// ── oRunner::setClassId ───────────────────────────────────────────────────────

void oRunner::setClassId(int id, bool isManualUpdate) {
  oAbstractRunner::setClassId(id, isManualUpdate);
}

// ── oRunner::getClass ─────────────────────────────────────────────────────────

const wstring& oRunner::getClass(bool virtualClass) const {
  return oAbstractRunner::getClass(virtualClass);
}

// ── oRunner::setCardNo ────────────────────────────────────────────────────────

void oRunner::setCardNo(int cno, bool /*matchCard*/, bool /*updateFromDatabase*/) {
  if (cardNumber != cno) {
    cardNumber = cno;
    changedObject();
    updateChanged(ChangeType::Update);
  }
}

// ── oRunner::setCard ──────────────────────────────────────────────────────────

int oRunner::setCard(int cardId) {
  if (!oe) return 0;
  pCard c = oe->getCard(cardId);
  if (!c) return 0;
  Card = c;
  Card->tOwner = this;
  changedObject();
  vector<pair<int, pControl>> mp;
  evaluateCard(true, mp, 0, ChangeType::Update);
  return cardId;
}

// ── oRunner::addCard ─────────────────────────────────────────────────────────

void oRunner::addCard(pCard card, vector<pair<int, pControl>>& missingPunches) {
  Card = card;
  if (Card) Card->tOwner = this;
  evaluateCard(true, missingPunches, 0, ChangeType::Update);
}

// ── oRunner::evaluateCard ─────────────────────────────────────────────────────

bool oRunner::evaluateCard(bool doApply, vector<pair<int, pControl>>& missingPunches,
                            int /*addPunch*/, ChangeType changeType) {
  if (unsigned(status) >= 100u) status = StatusUnknown;
  pClass clz = getClassRef(true);
  missingPunches.clear();

  if (doApply) {
    tStartTime = startTime;
    tStatus    = status;
    apply(changeType, nullptr);
  }

  // Reset card punch state
  if (Card) {
    for (auto& p : Card->punches) {
      p.tIndex          = -1;
      p.tMatchControlId = -1;
      p.isUsed          = false;
    }
  }

  tRogainingPoints      = 0;
  tRogainingPointsGross = 0;
  tRogainingOvertime    = 0;
  tReduction            = 0;
  tProblemDescription.clear();

  vector<SplitData> oldTimes;
  swap(splitTimes, oldTimes);

  if (!Card) {
    if (doApply) apply(changeType, nullptr);
    normalizedSplitTimes.clear();
    return false;
  }
  if (!clz) return false;

  const pCourse course = getCourse(true);
  if (!course) {
    // No course — read start/finish from card
    for (auto& p : Card->punches) {
      if (p.isStart() && tUseStartPunch)
        tStartTime = p.getTimeInt();
      else if (p.isFinish())
        setFinishTime(p.getTimeInt());
    }
    if (doApply) apply(changeType, nullptr);
    if (getFinishTime() <= 0)
      tStatus = StatusDNF;
    else if (tStatus == StatusUnknown || tStatus == StatusCANCEL || tStatus == StatusDNS)
      tStatus = StatusOK;
    return false;
  }

  int startPunchCode  = course->getStartPunchType();
  int finishPunchCode = course->getFinishPunchType();

  // Find start punch
  for (auto& p : Card->punches) {
    if (p.type == startPunchCode && tUseStartPunch) {
      tStartTime = p.getTimeInt();
      break;
    }
  }

  int nCtrl = course->nControls();
  splitTimes.resize(nCtrl, SplitData(-1, SplitData::SplitStatus::Missing));

  // Sequential matching
  int k = 0;
  for (auto& p : Card->punches) {
    if (k >= nCtrl) break;
    pControl ctrl = course->getControl(k);
    if (!ctrl) { k++; continue; }
    if (ctrl->hasNumber(p.type)) {
      int punchTime = p.getTimeInt();
      splitTimes[k] = SplitData(
        punchTime >= 0 ? punchTime : -1,
        punchTime >= 0 ? SplitData::SplitStatus::OK : SplitData::SplitStatus::NoTime
      );
      p.tIndex          = k;
      p.tMatchControlId = ctrl->getId();
      p.isUsed          = true;
      k++;
    }
  }

  // Remaining controls are missing
  for (; k < nCtrl; k++) {
    missingPunches.emplace_back(k, course->getControl(k));
  }

  // Find finish punch
  for (auto& p : Card->punches) {
    if (p.type == finishPunchCode || p.isFinish()) {
      setFinishTime(p.getTimeInt());
      break;
    }
  }

  if (doApply) apply(changeType, nullptr);

  // Determine status
  if (tStatus == StatusDNS || tStatus == StatusCANCEL ||
      tStatus == StatusNotCompeting) {
    // Keep DNS/CANCEL/NotCompeting
  } else if (getFinishTime() <= 0) {
    tStatus = StatusDNF;
  } else if (!missingPunches.empty()) {
    tStatus = StatusMP;
  } else if (tStatus == StatusUnknown || tStatus == StatusMAX) {
    tStatus = StatusOK;
  }

  normalizedSplitTimes.clear();
  return !missingPunches.empty();
}

// ── oRunner::apply ────────────────────────────────────────────────────────────

void oRunner::apply(ChangeType /*ct*/, pRunner /*src*/) {
  // Simplified: propagate permanent values to transient when no team/multirunner
  if (!tInTeam) {
    tStartTime = startTime;
    tStatus    = status;
  }
}

// ── oRunner::getCourse ────────────────────────────────────────────────────────

pCourse oRunner::getCourse(bool /*getAdapted*/) const {
  if (Course) return Course;
  if (Class) return Class->getCourse();
  return nullptr;
}

// ── oRunner::getSplitTimes ────────────────────────────────────────────────────

const vector<SplitData>& oRunner::getSplitTimes(bool normalized) const {
  if (normalized && !normalizedSplitTimes.empty())
    return normalizedSplitTimes;
  return splitTimes;
}

// ── oRunner::getStatusComputed ───────────────────────────────────────────────

RunnerStatus oRunner::getStatusComputed(bool /*allowUpdate*/) const {
  return tComputedStatus != StatusUnknown ? tComputedStatus : tStatus;
}

// ── oRunner::getTotalStatus ───────────────────────────────────────────────────

RunnerStatus oRunner::getTotalStatus(bool /*computed*/) const {
  return tStatus;
}

// ── oRunner::getPlace ─────────────────────────────────────────────────────────

int oRunner::getPlace(bool /*computed*/) const {
  return tmpResult.place;
}

// ── oRunner::getTotalPlace ────────────────────────────────────────────────────

int oRunner::getTotalPlace(bool /*computed*/) const {
  return 0;
}

// ── oRunner::getDynamicStatus ─────────────────────────────────────────────────

DynamicRunnerStatus oRunner::getDynamicStatus() const {
  if (tStatus == StatusDNS || tStatus == StatusCANCEL)
    return StatusInactive;
  if (FinishTime > 0)
    return StatusFinished;
  if (tStartTime > 0)
    return StatusActive;
  return StatusInactive;
}

// ── oRunner::getRogainingPoints ───────────────────────────────────────────────

int oRunner::getRogainingPoints(bool /*computed*/, bool /*total*/) const {
  return tRogainingPoints;
}

int oRunner::getRogainingPointsGross(bool /*computed*/) const {
  return tRogainingPointsGross;
}

int oRunner::getRogainingReduction(bool /*computed*/) const {
  return tReduction;
}

int oRunner::getRogainingOvertime(bool /*computed*/) const {
  return tRogainingOvertime;
}

// ── oRunner::getRanking ───────────────────────────────────────────────────────

int oRunner::getRanking() const {
  return 0;
}

// ── oRunner::getNumShortening ─────────────────────────────────────────────────

int oRunner::getNumShortening() const {
  return tNumShortening;
}

// ── oRunner::isResultUpdated ──────────────────────────────────────────────────

bool oRunner::isResultUpdated(bool /*onlyMain*/) const {
  return false;
}

// ── oRunner::classInstance ────────────────────────────────────────────────────

int oRunner::classInstance() const {
  return tDuplicateLeg;
}

// ── oRunner::startTimeAvailable ───────────────────────────────────────────────
// (already defined inline in oAbstractRunner.h via base — override not needed)

// ── oRunner::getRaceInfo ──────────────────────────────────────────────────────

const pair<wstring, int> oRunner::getRaceInfo() {
  return {L"", 0};
}

// ── oRunner::markClassChanged ─────────────────────────────────────────────────

void oRunner::markClassChanged(int /*controlId*/) {
  if (oe) oe->dataRevision++;
}

// ── oRunner::matchAbstractRunner ──────────────────────────────────────────────

bool oRunner::matchAbstractRunner(const oAbstractRunner* target) const {
  return this == target;
}

// ── oRunner::setStartNo ───────────────────────────────────────────────────────

void oRunner::setStartNo(int no, ChangeType ct) {
  oAbstractRunner::setStartNo(no, ct);
}

// ── oRunner::setBib ───────────────────────────────────────────────────────────

void oRunner::setBib(const wstring& /*bib*/, int /*numericalBib*/, bool /*updateRace*/) {
  // Stub — full impl in a later US
}

// ── oRunner::getStartTimeCompact / setCourseId ────────────────────────────────

wstring oRunner::getStartTimeCompact() const {
  return oe ? oe->getAbsTime(tStartTime, SubSecond::Auto) : L"";
}

void oRunner::setCourseId(int id) {
  if (oe) Course = oe->getCourse(id);
}

// ── oRunner::getTimeAfter ─────────────────────────────────────────────────────

int oRunner::getTimeAfter(int /*leg*/, bool /*allowUpdate*/) const {
  // Stub — full implementation in a later US (oRunnerData migration)
  return 0;
}

// ── oRunner::synchronize ──────────────────────────────────────────────────────

void oRunner::synchronize(bool /*writeOnly*/) {
  // Stub — update sqlChanged tracking
  sqlChanged = getTimeStampN();
}

// ── oRunner::synchronizeAll ───────────────────────────────────────────────────

bool oRunner::synchronizeAll(bool writeOnly) {
  synchronize(writeOnly);
  return true;
}

// ── oRunner::remove ───────────────────────────────────────────────────────────

void oRunner::remove() {
  Removed = true;
  if (oe) oe->classIdToRunnerHash.reset();
}

// ── oRunner::canRemove ────────────────────────────────────────────────────────

bool oRunner::canRemove() const {
  return Card == nullptr;
}

// ── oRunner::merge ────────────────────────────────────────────────────────────

void oRunner::merge(const oBase& /*input*/, const oBase* /*base*/) {
  // Stub — full impl in a later US
}

// ── oRunner::inputData ────────────────────────────────────────────────────────

pair<int, bool> oRunner::inputData(int /*id*/, const wstring& /*input*/,
                                    int /*inputId*/, wstring& /*output*/,
                                    bool /*noUpdate*/) {
  return {0, false};
}

// ── oRunner::fillInput ────────────────────────────────────────────────────────

void oRunner::fillInput(int /*id*/,
                         vector<pair<wstring, size_t>>& /*elements*/,
                         size_t& /*selected*/) {
  // Stub
}

// ── oRunner::Write ────────────────────────────────────────────────────────────

bool oRunner::Write(xmlparser& /*xml*/) {
  // Stub — full impl in a later US
  return false;
}

// ── oRunner::Set ─────────────────────────────────────────────────────────────

void oRunner::Set(const xmlobject& /*xo*/) {
  // Stub — full impl in a later US
}

// ── oRunner::fillSpeakerObject ────────────────────────────────────────────────

void oRunner::fillSpeakerObject(int /*leg*/, int /*controlCourseId*/,
                                  int /*expectedFinishTime*/, bool /*totalResult*/,
                                  oSpeakerObject& /*spk*/) const {
  // Stub
}

// ── oRunner::addTableRow ──────────────────────────────────────────────────────

void oRunner::addTableRow(Table& /*table*/) const {
  // Stub
}

// ── oRunner::getTable ─────────────────────────────────────────────────────────

const shared_ptr<Table>& oRunner::getTable(oEvent* /*oe*/) {
  static shared_ptr<Table> t;
  return t;
}

// ═════════════════════════════════════════════════════════════════════════════
// oEvent runner management (implemented here to avoid oEvent.h circular deps)
// ═════════════════════════════════════════════════════════════════════════════

pRunner oEvent::addRunner(const oRunner& r) {
  if (r.Id == 0) return nullptr;
  pRunner existing;
  if (runnerIdIndex.lookup(r.getId(), existing)) return existing;
  Runners.push_back(r);
  qFreeRunnerId = max(qFreeRunnerId, r.getId());
  pRunner pr = &Runners.back();
  pr->addToEvent(this, &r);
  runnerIdIndex[pr->getId()] = pr;
  return pr;
}

pRunner oEvent::getRunner(int Id, int /*race*/) const {
  pRunner value;
  if (runnerIdIndex.lookup(Id, value) && value && !value->isRemoved())
    return value;
  for (const auto& r : Runners) {
    if (r.Id == Id && !r.isRemoved())
      return const_cast<pRunner>(&r);
  }
  return nullptr;
}

void oEvent::getRunners(int classId, int /*courseId*/, vector<pRunner>& r,
                         bool /*sort*/) const {
  r.clear();
  for (const auto& runner : Runners) {
    if (runner.isRemoved()) continue;
    if (classId > 0 && runner.getClassId(true) != classId) continue;
    r.push_back(const_cast<pRunner>(&runner));
  }
}

void oEvent::getRunners(const set<int>& classIds, vector<pRunner>& r,
                         bool /*sync*/) {
  r.clear();
  for (auto& runner : Runners) {
    if (runner.isRemoved()) continue;
    if (!classIds.empty() &&
        classIds.find(runner.getClassId(true)) == classIds.end()) continue;
    r.push_back(&runner);
  }
}

// ── oAbstractRunner — missing helper implementations ──────────────────────────

wstring oAbstractRunner::getFinishTimeS(bool adjusted, SubSecond mode) const {
  if (FinishTime > 0) {
    if (adjusted)
      return oe->getAbsTime(FinishTime, mode);
    else
      return oe->getAbsTime(FinishTime - getBuiltinAdjustment(), mode);
  }
  return makeDash(L"-");
}

bool oAbstractRunner::compareBib(const wstring &b1, const wstring &b2) {
  int l1 = (int)b1.length();
  int l2 = (int)b2.length();
  if (l1 != l2)
    return l1 < l2;
  if (l1 == 0) return false;

  wchar_t maxc = 0, minc = std::numeric_limits<wchar_t>::max();
  for (int k = 0; k < l1; k++) { maxc = std::max(maxc, b1[k]); minc = std::min(minc, b1[k]); }
  for (int k = 0; k < l2; k++) { maxc = std::max(maxc, b2[k]); minc = std::min(minc, b2[k]); }

  unsigned coeff = maxc - minc + 1;
  unsigned z1 = 0, z2 = 0;
  for (int k = 0; k < l1; k++) z1 = coeff * z1 + (b1[k] - minc);
  for (int k = 0; k < l2; k++) z2 = coeff * z2 + (b2[k] - minc);
  return z1 < z2;
}

int oAbstractRunner::compareClubs(const oClub *ca, const oClub *cb) {
  if (ca == cb) return 0;
  if (!ca) return 1;
  if (!cb) return -1;
  const wstring an = ca->getName();
  const wstring bn = cb->getName();
  if (an < bn) return -1;
  if (an > bn) return  1;
  return 0;
}

int oRunner::getBirthYear() const {
  return getDCI().getInt("BirthYear");
}

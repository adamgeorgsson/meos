// oCard.cpp — Cross-platform migration of oCard domain entity.
// UI methods (fillPunches, addTableRow, getTable), XML (Write/Set),
// and setupFromRadioPunches (requires oRunner lists) are excluded.

#include "oCard.h"
#include "oControl.h"
#include "oCourse.h"
#include "oRunner.h"
#include "oEvent.h"
#include "domain_header.h"
#include "SICard.h"

#include <algorithm>
#include <stdexcept>
#include <cstdio>
#include <cwchar>

using namespace std;

// ---------------------------------------------------------------------------
// Static DataContainer (no oData fields — card data lives in punch list)
// ---------------------------------------------------------------------------
static oDataContainer& oCardContainer() {
  static oDataContainer dc(8);
  return dc;
}

oDataContainer& oCard::getDataBuffers(pvoid& data, pvoid& olddata,
                                       pvectorstr& strData) const {
  data    = &dataMap_;
  olddata = &oldDataMap_;
  strData = nullptr;
  return oCardContainer();
}

// ---------------------------------------------------------------------------
// Constructors / destructor
// ---------------------------------------------------------------------------
oCard::oCard(oEvent* poe) : oBase(poe) {
  Id      = oe->getFreeCardId();
  cardNo  = 0;
  readId  = 0;
  tOwner  = nullptr;
}

oCard::oCard(oEvent* poe, int id) : oBase(poe) {
  Id     = id;
  cardNo = 0;
  readId = 0;
  tOwner = nullptr;
  if (id > oe->qFreeCardId)
    oe->qFreeCardId = id;
}

oCard::~oCard() = default;

// ---------------------------------------------------------------------------
// changedObject
// ---------------------------------------------------------------------------
void oCard::changedObject() {
  if (tOwner)
    tOwner->changedObject();
  oe->sqlCards.changed = true;
}

// ---------------------------------------------------------------------------
// remove / canRemove
// ---------------------------------------------------------------------------
void oCard::remove() {
  Removed = true;
}

bool oCard::canRemove() const {
  return getOwner() == nullptr;
}

// ---------------------------------------------------------------------------
// getInfo
// ---------------------------------------------------------------------------
wstring oCard::getInfo() const {
  wchar_t bf[128];
  swprintf(bf, sizeof(bf) / sizeof(wchar_t), L"Card %d", cardNo);
  return bf;
}

// ---------------------------------------------------------------------------
// cardNo accessors
// ---------------------------------------------------------------------------
void oCard::setCardNo(int c) {
  if (cardNo != c)
    updateChanged();
  cardNo = c;
}

const wstring& oCard::getCardNoString() const {
  thread_local wstring tl_cardno;
  tl_cardno = itow(cardNo);
  return tl_cardno;
}

// ---------------------------------------------------------------------------
// Punch string serialization (core card-matching logic)
// ---------------------------------------------------------------------------
const string& oCard::getPunchString() const {
  punchString.clear();
  punchString.reserve(punches.size() * 16);
  for (auto& p : punches)
    p.appendCodeString(punchString);
  return punchString;
}

void oCard::importPunches(const string& s) {
  punches.clear();
  size_t startpos = 0;
  size_t endpos   = s.find(';', startpos);
  while (endpos != string::npos) {
    oPunch p(oe);
    p.decodeString(s.c_str() + startpos);
    punches.push_back(p);
    startpos = endpos + 1;
    endpos   = s.find(';', startpos);
  }
}

// ---------------------------------------------------------------------------
// addPunch
// ---------------------------------------------------------------------------
void oCard::addPunch(int type, int time, int matchControlId, int unit,
                     PunchOrigin origin) {
  oPunch p(oe);
  p.punchTime       = time;
  p.type            = type;
  p.tMatchControlId = matchControlId;
  p.isUsed          = (matchControlId != 0);
  p.punchUnit       = unit;

  if (origin == PunchOrigin::Original)
    p.origin = p.computeOrigin(oe->getZeroTimeNum() + time, type);
  else if (origin == PunchOrigin::Manual)
    p.origin = -1;

  if (punches.empty()) {
    punches.push_back(p);
  } else {
    oPunch oldBack = punches.back();
    if (oldBack.isFinish()) {
      punches.pop_back();
      punches.push_back(p);
      punches.push_back(oldBack);
    } else {
      punches.push_back(p);
    }
  }
  updateChanged();
}

// ---------------------------------------------------------------------------
// Punch lookup helpers
// ---------------------------------------------------------------------------
oPunch* oCard::getPunch(const oPunch* punch) {
  for (auto& p : punches)
    if (&p == punch) return &p;
  return nullptr;
}

oPunch* oCard::getPunchByType(int type) const {
  for (auto& p : punches)
    if (p.type == type) return const_cast<oPunch*>(&p);
  return nullptr;
}

oPunch* oCard::getPunchById(int courseControlId) const {
  auto idix = oControl::getIdIndexFromCourseControlId(courseControlId);
  oPunch* res = nullptr;
  for (auto& p : punches) {
    if (p.tMatchControlId == idix.first) {
      res = const_cast<oPunch*>(&p);
      if (idix.second == 0)
        return res;
      --idix.second;
    }
  }
  return nullptr;
}

oPunch* oCard::getPunchByIndex(int ix) const {
  for (auto& p : punches)
    if (0 == ix--) return const_cast<oPunch*>(&p);
  return nullptr;
}

void oCard::getPunches(vector<oPunch*>& out) const {
  out.clear();
  out.reserve(punches.size());
  for (auto& p : punches)
    out.push_back(const_cast<oPunch*>(&p));
}

// ---------------------------------------------------------------------------
// punch count
// ---------------------------------------------------------------------------
int oCard::getNumControlPunches(int startPunchType, int finishPunchType) const {
  int count = 0;
  for (auto& p : punches) {
    if (p.isFinish(finishPunchType) || p.isCheck() || p.isStart(startPunchType))
      continue;
    ++count;
  }
  return count;
}

// ---------------------------------------------------------------------------
// getOwner
// ---------------------------------------------------------------------------
oRunner* oCard::getOwner() const {
  if (tOwner && !static_cast<oBase*>(tOwner)->isRemoved())
    return tOwner;
  return nullptr;
}

// ---------------------------------------------------------------------------
// getTimeRange
// ---------------------------------------------------------------------------
pair<int, int> oCard::getTimeRange() const {
  pair<int, int> t(24 * timeConstHour, 0);
  bool finishLock = false;
  for (auto& p : punches) {
    if (p.hasTime()) {
      int pt = p.getTimeInt();
      if (pt < t.first) t.first = pt;
      if (p.isFinish()) {
        t.second   = p.getTimeInt();
        finishLock = true;
      }
      if (!finishLock && !p.isCheck() && !p.isStart())
        if (pt > t.second) t.second = pt;
    }
  }
  return t;
}

// ---------------------------------------------------------------------------
// SICard integration
// ---------------------------------------------------------------------------
void oCard::setReadId(const SICard& card) {
  updateChanged();
  readId = card.calculateHash();
}

bool oCard::isCardRead(const SICard& card) const {
  return readId == card.calculateHash();
}

void oCard::getSICard(SICard& card) const {
  card = SICard(ConvertedTimeStatus::Done);
  card.CardNumber = (uint32_t)cardNo;
  for (auto& p : punches) {
    if (p.type > 30)
      card.Punch[card.nPunch++].Code = (uint32_t)p.type;
  }
}

// ---------------------------------------------------------------------------
// getCardHash (used for merge de-duplication)
// ---------------------------------------------------------------------------
pair<int, int> oCard::getCardHash() const {
  int a = cardNo;
  int b = (int)readId;
  for (auto& p : punches) {
    a = a * 31 + p.punchTime * 997 + p.getTypeCode();
    b = b * 41 + p.punchTime * 97  + p.getTypeCode();
  }
  return make_pair(a, b);
}

// ---------------------------------------------------------------------------
// merge (from oevent_transfer.cpp)
// ---------------------------------------------------------------------------
void oCard::merge(const oBase& input, const oBase* /*base*/) {
  const oCard& src = dynamic_cast<const oCard&>(input);
  setCardNo(src.getCardNo());
  if (readId != src.readId) {
    readId = src.readId;
    updateChanged();
  }
  if (getPunchString() != src.getPunchString()) {
    importPunches(src.getPunchString());
    updateChanged();
  }
}

// ---------------------------------------------------------------------------
// adaptTimes
// ---------------------------------------------------------------------------
void oCard::adaptTimes(int startTime) {
  int st = -1;
  for (auto& p : punches) {
    if (p.hasTime()) { st = p.getTimeInt(); break; }
  }
  if (st == -1) return;

  const int h24 = 24 * timeConstHour;
  int offset = st / h24;
  if (offset > 0) {
    for (auto& p : punches)
      if (p.hasTime() && p.getTimeInt() < offset * h24) return;
    for (auto& p : punches)
      if (p.hasTime()) p.punchTime -= offset * h24;
    updateChanged();
  }

  if (startTime >= h24) {
    offset = startTime / h24;
    for (auto& p : punches)
      if (p.hasTime()) p.punchTime += offset * h24;
    updateChanged();
  }
}

// ---------------------------------------------------------------------------
// getSplitTime
// ---------------------------------------------------------------------------
int oCard::getSplitTime(int startTime, const oPunch* punch) const {
  for (auto& p : punches) {
    if (&p == punch) {
      int t = p.getAdjustedTime();
      if (t <= 0) return -1;
      return (startTime > 0) ? t - startTime : -1;
    } else if (p.isUsed) {
      startTime = p.getAdjustedTime();
    }
  }
  return -1;
}

// ---------------------------------------------------------------------------
// getRogainingSplit
// ---------------------------------------------------------------------------
wstring oCard::getRogainingSplit(int ix, int startTime) const {
  for (auto& p : punches) {
    int t = p.getAdjustedTime();
    if (0 == ix--) {
      if (t > 0 && t > startTime)
        return formatTime(t - startTime);
    }
    if (p.isUsed) startTime = t;
  }
  return makeDash(L"-");
}

// ---------------------------------------------------------------------------
// unexpectedOrder
// ---------------------------------------------------------------------------
bool oCard::unexpectedOrder(int startTime) const {
  for (auto& p : punches) {
    if (p.isCheck() || p.isStart() || p.getTimeInt() <= 0) continue;
    if (p.getTypeCode() >= 30 && !p.isUsedInCourse()) continue;
    if (p.getTimeInt() < startTime) return true;
    startTime = p.getTimeInt();
  }
  return false;
}

// ---------------------------------------------------------------------------
// setPunchTime
// ---------------------------------------------------------------------------
bool oCard::setPunchTime(const oPunch* punch, const wstring& time) {
  oPunch* op = getPunch(punch);
  if (!op) return false;
  int ot = op->punchTime;
  op->setTime(time);
  if (ot != op->punchTime) updateChanged();
  return true;
}

// ---------------------------------------------------------------------------
// deletePunch / insertPunchAfter
// ---------------------------------------------------------------------------
void oCard::deletePunch(oPunch* pp) {
  if (!pp) throw std::runtime_error("Punch not found");
  for (auto it = punches.begin(); it != punches.end(); ++it) {
    if (&*it == pp) {
      punches.erase(it);
      updateChanged();
      return;
    }
  }
}

void oCard::insertPunchAfter(int pos, int type, int time) {
  if (pos == 1023) return;
  oPunch punch(oe);
  punch.punchTime = time;
  punch.type      = type;
  int k = -1;
  for (auto it = punches.begin(); it != punches.end(); ++it) {
    if (k == pos) {
      punches.insert(it, punch);
      updateChanged();
      return;
    }
    ++k;
  }
  punches.push_back(punch);
  updateChanged();
}

// ---------------------------------------------------------------------------
// getStartTime / getStartPunchCode / getFinishPunchCode
// ---------------------------------------------------------------------------
int oCard::getStartTime(int ptype) const {
  if (ptype == oPunch::SpecialPunch::PunchStart) {
    auto it = punches.begin();
    if (it != punches.end()) {
      if (it->getTypeCode() == oPunch::SpecialPunch::PunchStart)
        return it->getTimeInt();
      ++it;
      if (it != punches.end())
        if (it->getTypeCode() == oPunch::SpecialPunch::PunchStart)
          return it->getTimeInt();
    }
  } else {
    for (auto& p : punches)
      if (p.getTypeCode() == ptype) return p.getTimeInt();
  }
  return -1;
}

int oCard::getStartPunchCode() const {
  auto it = punches.begin();
  if (it != punches.end()) {
    if (it->getTypeCode() == oPunch::SpecialPunch::PunchStart)
      return it->getPunchUnit();
    ++it;
    if (it != punches.end())
      if (it->getTypeCode() == oPunch::SpecialPunch::PunchStart)
        return it->getPunchUnit();
  }
  return 0;
}

int oCard::getFinishPunchCode() const {
  auto it = punches.rbegin();
  if (it != punches.rend())
    if (it->getTypeCode() == oPunch::SpecialPunch::PunchFinish)
      return it->getPunchUnit();
  return 0;
}

// ---------------------------------------------------------------------------
// isOriginalCard
// ---------------------------------------------------------------------------
oCard::PunchOrigin oCard::isOriginalCard() const {
  bool isUnknown = false;
  for (auto& p : punches) {
    if (p.origin == 0)
      isUnknown = true;
    else if (!p.isOriginal())
      return PunchOrigin::Manual;
  }
  return isUnknown ? PunchOrigin::Unknown : PunchOrigin::Original;
}

// ---------------------------------------------------------------------------
// Battery / voltage
// ---------------------------------------------------------------------------
wstring oCard::getCardVoltage() const {
  return getCardVoltage(miliVolt);
}

wstring oCard::getCardVoltage(int mv) {
  if (mv <= 10) return L"";
  int vi = mv / 1000;
  int vd = (mv % 1000) / 10;
  wchar_t bf[64];
  swprintf(bf, sizeof(bf) / sizeof(wchar_t), L"%d.%02d V", vi, vd);
  return bf;
}

oCard::BatteryStatus oCard::isCriticalCardVoltage() const {
  return isCriticalCardVoltage(miliVolt);
}

oCard::BatteryStatus oCard::isCriticalCardVoltage(int mv) {
  if (mv > 10 && mv < 2445)  return BatteryStatus::Bad;
  if (mv > 10 && mv <= 2710) return BatteryStatus::Warning;
  return BatteryStatus::OK;
}

wstring oCard::getBatteryDate() const {
  if (batteryDate > 0) return formatDate(batteryDate, false);
  return _EmptyWString;
}

// ---------------------------------------------------------------------------
// getWrongPunch — finds the most likely incorrect punch for a missing control
// ---------------------------------------------------------------------------
pair<int, oControl*> oCard::getWrongPunch(const oCourse& crs,
                                           const oControl& ctrl) {
  int ix = 0;
  map<int, oPunch*> indexToWrongPunches;
  vector<int> crsToPunch(crs.getNumControls(), -1);
  vector<oPunch*> controlPunches;

  for (oPunch& p : punches) {
    if (p.isStart() || p.isFinish() || p.isCheck()) { ++ix; continue; }
    if (!p.isUsedInCourse())
      indexToWrongPunches[ix] = &p;
    else if (p.tIndex >= 0 && p.tIndex < (int)crsToPunch.size())
      crsToPunch[p.tIndex] = ix;
    controlPunches.push_back(&p);
    ++ix;
  }

  oPunch* wrongPunch = nullptr;
  int seedPunchIx = -1;
  for (int j = 0; j < crs.getNumControls(); j++) {
    if (crs.getControl(j) == &ctrl) {
      if (seedPunchIx + 1 < (int)controlPunches.size() &&
          !controlPunches[seedPunchIx + 1]->isUsedInCourse()) {
        wrongPunch = controlPunches[seedPunchIx + 1];
        break;
      }
    } else {
      if (crsToPunch[j] >= 0) seedPunchIx = crsToPunch[j];
    }
  }

  if (wrongPunch) {
    for (auto& [id, pCtrl] : oe->controlIndex_) {
      if (pCtrl->isRemoved()) continue;
      if (pCtrl->getNNumbers() > 0 &&
          pCtrl->hasNumber(wrongPunch->getTypeCode()))
        return make_pair(wrongPunch->getTypeCode(), pCtrl);
    }
    return make_pair(wrongPunch->getTypeCode(), nullptr);
  }
  return make_pair(0, nullptr);
}

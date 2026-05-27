// oFreePunch.cpp — Cross-platform migration of oFreePunch domain entity.
// UI methods (addTableRow, fillInput, inputData, getTable), XML (Set/Write),
// socket/runner-live-update logic, and DB operations are excluded.

#include "oFreePunch.h"
#include "oEvent.h"
#include "domain_header.h"

#include <algorithm>
#include <cassert>
#include <vector>

using namespace std;

// ---------------------------------------------------------------------------
// Static flag
// ---------------------------------------------------------------------------
bool oFreePunch::disableHashing = false;

// ---------------------------------------------------------------------------
// Constructors / destructor
// ---------------------------------------------------------------------------
oFreePunch::oFreePunch(oEvent* poe, int card, int time, int inType, int unit)
    : oPunch(poe) {
  Id        = oe->getFreePunchId();
  CardNo    = card;
  punchTime = time;
  punchUnit = unit;
  type      = inType;
  iHashType = 0;
  tRunnerId = 0;
}

oFreePunch::oFreePunch(oEvent* poe, int id) : oPunch(poe) {
  Id        = id;
  if (id > oe->qFreePunchId)
    oe->qFreePunchId = id;
  iHashType = 0;
  tRunnerId = 0;
}

oFreePunch::~oFreePunch() = default;

// ---------------------------------------------------------------------------
// changedObject — sets dirty flag on the event's punch SQL state
// ---------------------------------------------------------------------------
void oFreePunch::changedObject() {
  // Full implementation would also call r->markClassChanged(tMatchControlId).
  // Stub: runner lookup deferred until oRunner is migrated.
  oe->sqlPunches.changed = true;
}

// ---------------------------------------------------------------------------
// getTiedRunner — stub until oRunner is migrated
// ---------------------------------------------------------------------------
oRunner* oFreePunch::getTiedRunner() const {
  return oe->getRunner(tRunnerId, 0);
}

// ---------------------------------------------------------------------------
// remove / canRemove
// ---------------------------------------------------------------------------
void oFreePunch::remove() {
  Removed = true;
}

bool oFreePunch::canRemove() const {
  return true;
}

// ---------------------------------------------------------------------------
// setCardNo
// ---------------------------------------------------------------------------
bool oFreePunch::setCardNo(int cno, bool databaseUpdate) {
  if (cno == CardNo) return false;

  // Remove this punch from punchIndex for its current card.
  auto it2 = oe->punchIndex.find(iHashType);
  if (it2 != oe->punchIndex.end()) {
    oEvent::PunchIndexType& pi = it2->second;
    auto it = pi.find(CardNo);
    while (it != pi.end() && it->first == CardNo) {
      if (it->second == this) {
        pi.erase(it);
        break;
      }
      ++it;
    }
  }

  oe->removeFromPunchHash(CardNo, type, punchTime);
  rehashPunches(*oe, CardNo, nullptr);

  CardNo = cno;
  oe->insertIntoPunchHash(CardNo, type, punchTime);
  rehashPunches(*oe, CardNo, this);

  if (!databaseUpdate)
    updateChanged();

  return true;
}

// ---------------------------------------------------------------------------
// setType — numeric type only (locale-based names are UI concern)
// ---------------------------------------------------------------------------
bool oFreePunch::setType(const wstring& t, bool databaseUpdate) {
  int inputType = wtoi(t.c_str());
  int ttype = 0;
  if (inputType > 0 && inputType < 10000)
    ttype = inputType;

  if (ttype > 0 && ttype != type) {
    oe->removeFromPunchHash(CardNo, type, punchTime);
    type = ttype;
    oe->insertIntoPunchHash(CardNo, type, punchTime);
    rehashPunches(*oe, CardNo, nullptr);

    if (!databaseUpdate)
      updateChanged();

    return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// setTimeInt — updates punch time and invalidates hash
// ---------------------------------------------------------------------------
void oFreePunch::setTimeInt(int t, bool databaseUpdate) {
  if (t != punchTime) {
    oe->removeFromPunchHash(CardNo, type, punchTime);
    punchTime = t;
    oe->insertIntoPunchHash(CardNo, type, punchTime);
    rehashPunches(*oe, CardNo, nullptr);

    if (!databaseUpdate)
      updateChanged();
  }
}

// ---------------------------------------------------------------------------
// rehashPunches — rebuild iHashType for all punches of a given card
// ---------------------------------------------------------------------------
void oFreePunch::rehashPunches(oEvent& oe, int cardNo, pFreePunch newPunch) {
  if (disableHashing ||
      (cardNo == 0 && !oe.punchIndex.empty()) ||
      oe.freePunches.empty())
    return;

  vector<pFreePunch> fp;

  if (oe.punchIndex.empty()) {
    // Full rehash — process every non-removed, non-hired-card punch.
    fp.reserve(oe.freePunches.size());
    for (auto& pit : oe.freePunches) {
      if (pit.isRemoved() || pit.isHiredCard()) continue;
      fp.push_back(&pit);
    }

    sort(fp.begin(), fp.end(), FreePunchComp());

    disableHashing = true;
    try {
      for (pFreePunch punch : fp) {
        punch->iHashType = oe.getControlIdFromPunch(
            punch->getTimeInt(), punch->type, punch->CardNo, true, *punch);
        oe.punchIndex[punch->iHashType].insert({punch->CardNo, punch});
      }
    } catch (...) {
      disableHashing = false;
      throw;
    }
    disableHashing = false;
    return;
  }

  // Collect all punches for the specified card across all hash buckets.
  fp.reserve(oe.punchIndex.size() + 1);
  for (auto& [hashType, index] : oe.punchIndex) {
    auto res = index.equal_range(cardNo);
    for (auto it = res.first; it != res.second; ++it) {
      if (!it->second->isRemoved())
        fp.push_back(it->second);
    }
    index.erase(res.first, res.second);
  }

  if (newPunch && !newPunch->isHiredCard())
    fp.push_back(newPunch);

  sort(fp.begin(), fp.end(), FreePunchComp());

  for (size_t j = 0; j < fp.size(); j++) {
    if (j > 0 && fp[j - 1] == fp[j]) continue;  // skip duplicates
    pFreePunch punch = fp[j];
    punch->iHashType = oe.getControlIdFromPunch(
        punch->getTimeInt(), punch->type, cardNo, true, *punch);
    oe.punchIndex[punch->iHashType].insert({punch->CardNo, punch});
  }
}

// ---------------------------------------------------------------------------
// merge
// ---------------------------------------------------------------------------
void oFreePunch::merge(const oBase& /*input*/, const oBase* /*base*/) {
  // Not yet implemented (same as legacy oevent_transfer.cpp stub).
}

// ---------------------------------------------------------------------------
// oEvent::getControlIdFromPunch — stub (full impl needs runner/course lists)
// ---------------------------------------------------------------------------
int oEvent::getControlIdFromPunch(int /*time*/, int type, int /*card*/,
                                  bool /*markClassChanged*/, oFreePunch& punch) {
  punch.tRunnerId      = -1;
  punch.tMatchControlId = type;
  return oFreePunch::getControlHash(type, 0);
}

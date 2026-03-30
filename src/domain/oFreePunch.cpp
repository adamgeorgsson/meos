// oFreePunch.cpp — Migrated from legacy code/oFreePunch.cpp (US-003f).
// Cross-platform, no Win32 dependencies.

/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2026 Melin Software HB

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
************************************************************************/

#include "oFreePunch.h"
#include "oEvent.h"
#include "oControl.h"
#include "oRunner.h"
#include "../util/meos_util.h"
#include "../util/localizer.h"
#include "../util/xmlparser.h"

#define MEOS_DOM_STUBS_IMPL
#include "meos_dom_stubs.h"

#include <algorithm>
#include <cassert>

bool oFreePunch::disableHashing = false;

oFreePunch::oFreePunch(oEvent* poe, int card, int time, int inType, int unit) : oPunch(poe) {
  Id = oe->getFreePunchId();
  CardNo = card;
  punchTime = time;
  punchUnit = unit;
  type = inType;
  iHashType = 0;
  tRunnerId = 0;
}

oFreePunch::oFreePunch(oEvent* poe, int id) : oPunch(poe) {
  Id = id;
  oe->qFreePunchId = max(id, oe->qFreePunchId);
  iHashType = 0;
  tRunnerId = 0;
  CardNo = 0;
}

oFreePunch::~oFreePunch() {
}

bool oFreePunch::Write(xmlparser& xml)
{
  if (Removed) return true;
  xml.startTag("Punch");
  xml.write("CardNo", CardNo);
  xml.writeTime("Time", punchTime);
  xml.write("Type", type);
  xml.write("Unit", punchUnit);
  xml.write("Origin", origin);
  xml.write("Id", Id);
  xml.write("Updated", getStamp());
  xml.endTag();

  return true;
}

void oFreePunch::Set(const xmlobject* xo)
{
  xmlList xl;
  xo->getObjects(xl);

  xmlList::const_iterator it;
  for (it = xl.begin(); it != xl.end(); ++it) {
    if (it->is("CardNo")) {
      CardNo = it->getInt();
    }
    else if (it->is("Type")) {
      type = it->getInt();
    }
    else if (it->is("Time")) {
      punchTime = it->getRelativeTime();
    }
    else if (it->is("Unit")) {
      punchUnit = it->getInt();
    }
    else if (it->is("Origin")) {
      origin = it->getInt();
    }
    else if (it->is("Id")) {
      Id = it->getInt();
    }
    else if (it->is("Updated")) {
      Modified.setStamp(it->getRawStr());
    }
  }
}

bool oFreePunch::setCardNo(int cno, bool databaseUpdate) {
  if (cno != CardNo) {
    pRunner r1 = oe->getRunner(tRunnerId, 0);
    int oldControlId = tMatchControlId;

    // Remove from index
    oEvent::PunchIndexType& pi = oe->punchIndex[iHashType];
    oEvent::PunchConstIterator it = pi.find(CardNo);
    while (it != pi.end() && it->first == CardNo) {
      if (it->second == this) {
        pi.erase(it);
        break;
      }
      ++it;
    }
    oe->removeFromPunchHash(CardNo, type, punchTime);
    rehashPunches(*oe, CardNo, nullptr);

    CardNo = cno;
    oe->insertIntoPunchHash(CardNo, type, punchTime);

    rehashPunches(*oe, CardNo, this);
    pRunner r2 = oe->getRunner(tRunnerId, 0);

    if (r1 && oldControlId > 0)
      r1->markClassChanged(oldControlId);
    if (r2 && iHashType > 0)
      r2->markClassChanged(tMatchControlId);

    if (!databaseUpdate)
      updateChanged();

    return true;
  }
  return false;
}

void oFreePunch::remove()
{
  if (oe)
    oe->removeFreePunch(Id);
}

bool oFreePunch::canRemove() const
{
  return true;
}

const shared_ptr<Table>& oFreePunch::getTable(oEvent* oe) {
  if (!oe->hasTable("punch")) {
    auto table = make_shared<Table>(oe, 20, L"Stamplingar", "punches");
    table->addColumn("Id", 70, true, true);
    table->addColumn("Andrad", 150, false);
    table->addColumn("Bricka", 70, true);
    table->addColumn("Kontroll", 70, true);
    table->addColumn("Enhet", 70, true);
    table->addColumn("Tid", 70, false);
    table->addColumn("Lopare", 170, false);
    table->addColumn("Lag", 170, false);
    table->addColumn("Klass", 170, false);
    oe->setTable("punch", table);
  }
  return oe->getTable("punch");
}

void oFreePunch::addTableRow(Table& table) const {
  oFreePunch& it = *pFreePunch(this);
  table.addRow(getId(), &it);
  int row = 0;
  table.set(row++, it, TID_ID, itow(getId()), false, cellEdit);
  table.set(row++, it, TID_MODIFIED, getTimeStamp(), false, cellEdit);
  table.set(row++, it, TID_CARD, itow(getCardNo()), true, cellEdit);
  table.set(row++, it, TID_CONTROL, getType(nullptr), true, cellEdit);
  table.set(row++, it, TID_UNIT, punchUnit > 0 ? itow(punchUnit) : _EmptyWString, true, cellEdit);
  table.set(row++, it, TID_TIME, getTime(false, SubSecond::Auto), true, cellEdit);

  pRunner r = nullptr;
  if (CardNo > 0)
    r = oe->getRunnerByCardNo(CardNo, getTimeInt(), oEvent::CardLookupProperty::Any);

  table.set(row++, it, TID_RUNNER, r ? r->getName() : L"?", false, cellEdit);

  if (r && r->getTeam())
    table.set(row++, it, TID_TEAM, L"", false, cellEdit);
  else
    table.set(row++, it, TID_TEAM, L"", false, cellEdit);

  table.set(row++, it, TID_CLASSNAME, r ? r->getClass(true) : L"", false, cellEdit);
}

pair<int, bool> oFreePunch::inputData(int id, const wstring& input,
                                      int /*inputId*/, wstring& output, bool /*noUpdate*/)
{
  synchronize(false);
  switch (id) {
    case TID_CARD:
      setCardNo((int)std::wcstol(input.c_str(), nullptr, 10));
      synchronize(true);
      output = itow(CardNo);
      break;

    case TID_TIME:
      setTime(input);
      synchronize(true);
      output = getTime(false, SubSecond::Auto);
      break;

    case TID_CONTROL:
      setType(input);
      synchronize(true);
      output = getType(nullptr);
      break;

    case TID_UNIT:
      setPunchUnit((int)std::wcstol(input.c_str(), nullptr, 10));
      synchronize(true);
      output = punchUnit > 0 ? itow(punchUnit) : _EmptyWString;
      break;
  }
  return make_pair(0, false);
}

void oFreePunch::fillInput(int /*id*/, vector<pair<wstring, size_t>>& /*out*/, size_t& /*selected*/)
{
  // No-op
}

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

bool oFreePunch::setType(const wstring& t, bool databaseUpdate) {
  int inputType = (int)std::wcstol(t.c_str(), nullptr, 10);
  int ttype = 0;
  if (inputType > 0 && inputType < 10000)
    ttype = inputType;
  else {
    if (t == lang.tl("Check"))
      ttype = oPunch::PunchCheck;
    else if (t == lang.tl(L"Mål"))
      ttype = oPunch::PunchFinish;
    if (t == lang.tl("Start"))
      ttype = oPunch::PunchStart;
  }
  if (ttype > 0 && ttype != type) {
    oe->removeFromPunchHash(CardNo, type, punchTime);
    type = ttype;
    oe->insertIntoPunchHash(CardNo, type, punchTime);
    int oldControlId = tMatchControlId;
    rehashPunches(*oe, CardNo, nullptr);

    pRunner r = oe->getRunner(tRunnerId, 0);
    if (r) {
      r->markClassChanged(tMatchControlId);
      if (oldControlId > 0)
        r->markClassChanged(oldControlId);
    }

    if (!databaseUpdate)
      updateChanged();

    return true;
  }
  return false;
}

void oFreePunch::rehashPunches(oEvent& oe, int cardNo, pFreePunch newPunch) {
  if (disableHashing || (cardNo == 0 && !oe.punchIndex.empty()) || oe.punches.empty())
    return;

  vector<pFreePunch> fp;

  if (oe.punchIndex.empty()) {
    // Rehash all punches
    fp.reserve(oe.punches.size());
    for (oFreePunchList::iterator pit = oe.punches.begin(); pit != oe.punches.end(); ++pit) {
      if (pit->isRemoved() || pit->isHiredCard())
        continue;
      fp.push_back(&(*pit));
    }

    sort(fp.begin(), fp.end(), FreePunchComp());
    disableHashing = true;
    try {
      for (size_t j = 0; j < fp.size(); j++) {
        pFreePunch punch = fp[j];
        punch->iHashType = oe.getControlIdFromPunch(punch->getTimeInt(), punch->type,
                                                    punch->CardNo, true, *punch);
        oEvent::PunchIndexType& card2Punch = oe.punchIndex[punch->iHashType];
        card2Punch.insert(make_pair(punch->CardNo, punch));
      }
    }
    catch (...) {
      disableHashing = false;
      throw;
    }
    disableHashing = false;
    return;
  }

  map<int, oEvent::PunchIndexType>::iterator it;
  fp.reserve(oe.punchIndex.size() + 1);

  // Get all punches for the specified card.
  for (it = oe.punchIndex.begin(); it != oe.punchIndex.end(); ++it) {
    pair<oEvent::PunchConstIterator, oEvent::PunchConstIterator> res = it->second.equal_range(cardNo);
    oEvent::PunchConstIterator pIter = res.first;
    while (pIter != res.second) {
      pFreePunch punch = pIter->second;
      assert(punch && punch->CardNo == cardNo);
      if (!punch->isRemoved()) {
        fp.push_back(punch);
      }
      ++pIter;
    }
    it->second.erase(res.first, res.second);
  }

  if (newPunch && !newPunch->isHiredCard())
    fp.push_back(newPunch);

  sort(fp.begin(), fp.end(), FreePunchComp());
  for (size_t j = 0; j < fp.size(); j++) {
    if (j > 0 && fp[j - 1] == fp[j])
      continue; // Skip duplicates
    pFreePunch punch = fp[j];
    punch->iHashType = oe.getControlIdFromPunch(punch->getTimeInt(), punch->type, cardNo, true, *punch);
    oEvent::PunchIndexType& card2Punch = oe.punchIndex[punch->iHashType];
    card2Punch.insert(make_pair(punch->CardNo, punch));
  }
}

int oFreePunch::getControlHash(int courseControlId, int race) {
  int newId = courseControlId + race * 100000000;
  return newId;
}

int oFreePunch::getControlIdFromHash(int hash, bool courseControlId) {
  int r = (hash % 100000000);
  if (courseControlId)
    return r;
  else
    return oControl::getIdIndexFromCourseControlId(r).first;
}

pRunner oFreePunch::getTiedRunner() const {
  return oe->getRunner(tRunnerId, 0);
}

void oFreePunch::changedObject() {
  pRunner r = getTiedRunner();
  if (r && tMatchControlId > 0)
    r->markClassChanged(tMatchControlId);
  oe->sqlPunches.changed = true;
}

void oFreePunch::merge(const oBase& /*input*/, const oBase* /*base*/) {
  // Not implemented
}

// ── oEvent punch management methods ───────────────────────────────────────────

void oEvent::insertIntoPunchHash(int card, int code, int time) {
  if (time > 0) {
    int p1 = time * 4096 + code;
    int p2 = card;
    readPunchHash.insert(make_pair(p1, p2));
  }
}

void oEvent::removeFromPunchHash(int card, int code, int time) {
  int p1 = time * 4096 + code;
  int p2 = card;
  readPunchHash.erase(make_pair(p1, p2));
}

bool oEvent::isInPunchHash(int card, int code, int time) {
  int p1 = time * 4096 + code;
  int p2 = card;
  return readPunchHash.count(make_pair(p1, p2)) > 0;
}

// Full getControlIdFromPunch — matches physical punch to course control.
int oEvent::getControlIdFromPunch(int time, int type, int card,
                                  bool markClassChanged, oFreePunch& punch) {
  pRunner r = getRunnerByCardNo(card, time, CardLookupProperty::Any);
  if (r) {
    pCourse course = r->getCourse(false);
    if (course) {
      int nCtrl = course->nControls();
      for (int k = 0; k < nCtrl; k++) {
        pControl ctrl = course->getControl(k);
        if (ctrl && ctrl->hasNumber(type)) {
          int courseControlId = course->getCourseControlId(k);
          punch.tRunnerId = r->getId();
          punch.tMatchControlId = ctrl->getId();
          int hash = oFreePunch::getControlHash(courseControlId, r->getRaceNo());
          if (markClassChanged)
            r->markClassChanged(ctrl->getId());
          return hash;
        }
      }
    }
    punch.tRunnerId = r->getId();
  } else {
    punch.tRunnerId = -1;
  }
  punch.tMatchControlId = type;
  return oFreePunch::getControlHash(type, 0);
}

pFreePunch oEvent::addFreePunch(oFreePunch& fp) {
  insertIntoPunchHash(fp.CardNo, fp.type, fp.punchTime);
  punches.push_back(fp);
  pFreePunch fpz = &punches.back();
  fpz->addToEvent(this, &fp);
  oFreePunch::rehashPunches(*this, fp.CardNo, fpz);

  if (!fpz->existInDB() && hasDBConnection()) {
    fpz->changed = true;
    fpz->synchronize();
  }
  return fpz;
}

// Simplified addFreePunch — skips socket/runner status updates
// (full impl requires US-003g oRunner migration).
pFreePunch oEvent::addFreePunch(int time, int type, int unit, int card,
                                bool /*updateStartFinish*/, bool isOriginal) {
  if (time > 0 && isInPunchHash(card, type, time))
    return nullptr;
  oFreePunch ofp(this, card, time, type, unit);
  if (isOriginal)
    ofp.origin = ofp.computeOrigin(time, type);

  punches.emplace_back(ofp);
  pFreePunch fp = &punches.back();
  fp->addToEvent(this, &ofp);
  oFreePunch::rehashPunches(*this, card, fp);
  insertIntoPunchHash(card, type, time);

  fp->updateChanged();
  fp->synchronize();
  return fp;
}

void oEvent::removeFreePunch(int Id) {
  oFreePunchList::iterator it;
  for (it = punches.begin(); it != punches.end(); ++it) {
    if (it->Id == Id) {
      pRunner r = getRunner(it->tRunnerId, 0);
      if (r && r->Class) {
        r->markClassChanged(it->tMatchControlId);
        classChanged(r->Class, true);
      }
      pFreePunch fp = &*it;
      PunchIndexType& ix = punchIndex[it->iHashType];
      pair<PunchConstIterator, PunchConstIterator> res = ix.equal_range(it->CardNo);
      while (res.first != res.second) {
        if (res.first->second == fp) {
          PunchConstIterator rm = res.first;
          ++res.first;
          ix.erase(rm);
        }
        else
          ++res.first;
      }

      int cardNo = fp->CardNo;
      removeFromPunchHash(cardNo, fp->type, fp->punchTime);
      punches.erase(it);
      oFreePunch::rehashPunches(*this, cardNo, nullptr);
      dataRevision++;
      return;
    }
  }
}

pFreePunch oEvent::getPunch(int Id) const
{
  for (oFreePunchList::const_iterator it = punches.begin(); it != punches.end(); ++it) {
    if (it->Id == Id) {
      if (it->isRemoved())
        return nullptr;
      return const_cast<pFreePunch>(&*it);
    }
  }
  return nullptr;
}

pFreePunch oEvent::getPunch(int runnerId, int courseControlId, int card) const
{
  // Lazy setup
  oFreePunch::rehashPunches(const_cast<oEvent&>(*this), 0, nullptr);

  pRunner r = getRunner(runnerId, 0);
  int runnerRace = r ? r->getRaceNo() : 0;

  int itype = oFreePunch::getControlHash(courseControlId, runnerRace);

  map<int, PunchIndexType>::const_iterator it1 = punchIndex.find(itype);
  if (it1 != punchIndex.end()) {
    const PunchIndexType& cIndex = it1->second;
    pair<PunchConstIterator, PunchConstIterator> res = cIndex.equal_range(card);
    PunchConstIterator pIter = res.first;
    while (pIter != res.second) {
      pFreePunch punch = pIter->second;
      if (!punch->isRemoved()) {
        assert(punch && punch->CardNo == card);
        if (punch->tRunnerId == runnerId || runnerId == 0)
          return punch;
      }
      ++pIter;
    }
  }

  auto res = advanceInformationPunches.find(make_pair(itype, card));
  if (res != advanceInformationPunches.end())
    return const_cast<pFreePunch>(&res->second);

  return nullptr;
}

vector<pFreePunch> oEvent::getPunchesByType(int type, int unit) const {
  vector<pFreePunch> out;
  for (auto& p : punches) {
    if (!p.isRemoved() && p.getTypeCode() == type) {
      if (unit == 0 || p.getPunchUnit() == unit)
        out.push_back(pFreePunch(&p));
    }
  }
  return out;
}

void oEvent::getPunchesForRunner(int runnerId, bool doSort, vector<pFreePunch>& runnerPunches) const {
  runnerPunches.clear();
  pRunner r = getRunner(runnerId, 0);
  if (r == nullptr)
    return;

  // Lazy setup
  oFreePunch::rehashPunches(const_cast<oEvent&>(*this), 0, nullptr);

  int card = r->getCardNo();
  if (card == 0)
    return;

  for (auto& it1 : punchIndex) {
    const PunchIndexType& cIndex = it1.second;
    pair<PunchConstIterator, PunchConstIterator> res = cIndex.equal_range(card);
    PunchConstIterator pIter = res.first;
    while (pIter != res.second) {
      pFreePunch punch = pIter->second;
      if (!punch->isRemoved()) {
        assert(punch && punch->CardNo == card);
        if (punch->tRunnerId == runnerId || runnerId == 0)
          runnerPunches.push_back(punch);
      }
      ++pIter;
    }
  }

  if (doSort) {
    sort(runnerPunches.begin(), runnerPunches.end(),
         [](const oPunch* p1, const oPunch* p2) { return p1->getTimeInt() < p2->getTimeInt(); });
  }
}

void oEvent::getFreeControls(set<int>& controlId) const {
  controlId.clear();
  for (auto& it : punchIndex) {
    int id = oFreePunch::getControlIdFromHash(it.first, false);
    controlId.insert(id);
  }
}

void oEvent::getLatestPunches(int firstTime, vector<const oFreePunch*>& punchesOut) const {
  for (auto& it : advanceInformationPunches) {
    int time = it.second.getModificationTime();
    if (time >= firstTime)
      punchesOut.push_back(&it.second);
  }
  for (auto& p : punches) {
    int time = p.getModificationTime();
    if (time >= firstTime)
      punchesOut.push_back(&p);
  }
}

bool oEvent::isHiredCard(int cardNo) const {
  if (tHiredCardHashDataRevision != (int)dataRevision) {
    hiredCardHash.clear();
    for (auto& p : punches) {
      if (!p.isRemoved() && p.isHiredCard())
        hiredCardHash.insert(p.getCardNo());
    }
    tHiredCardHashDataRevision = (int)dataRevision;
  }
  return hiredCardHash.count(cardNo) > 0;
}

void oEvent::setHiredCard(int cardNo, bool flag) {
  if (cardNo <= 0)
    return;

  if (isHiredCard(cardNo) != flag) {
    if (flag) {
      addFreePunch(0, oPunch::HiredCard, 0, cardNo, false, false);
      hiredCardHash.insert(cardNo);
      tHiredCardHashDataRevision = (int)dataRevision;
    }
    else {
      hiredCardHash.erase(cardNo);
      for (auto it = punches.begin(); it != punches.end();) {
        if (!it->isRemoved() && it->isHiredCard() && it->CardNo == cardNo) {
          auto toErase = it;
          ++it;
          punches.erase(toErase);
        }
        else {
          ++it;
        }
      }
      tHiredCardHashDataRevision = (int)dataRevision;
    }
  }
}

bool oEvent::hasHiredCardData() {
  isHiredCard(0);
  return !hiredCardHash.empty();
}

void oEvent::clearHiredCards() {
  for (auto it = punches.begin(); it != punches.end();) {
    if (!it->isRemoved() && it->isHiredCard()) {
      auto toErase = it;
      ++it;
      punches.erase(toErase);
    }
    else {
      ++it;
    }
  }
  hiredCardHash.clear();
}

vector<int> oEvent::getHiredCards() const {
  isHiredCard(0); // Update hash
  vector<int> r(hiredCardHash.begin(), hiredCardHash.end());
  return r;
}

void oEvent::generatePunchTableData(Table& /*table*/, oFreePunch* /*addPunch*/) {
  // GUI stub — not implemented in domain layer
}

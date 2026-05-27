/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2026 Melin Software HB

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Melin Software HB - software@melin.nu - www.melin.nu
    Eksoppsvägen 16, SE-75646 UPPSALA, Sweden

************************************************************************/


// oRunner.cpp: implementation of the oRunner class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "oRunner.h"
#include "oDataContainer.h"
#include "oEvent.h"
#include "gdioutput.h"
#include "gdifonts.h"
#include "Table.h"
#include "meos_util.h"
#include <cassert>
#include "localizer.h"
#include "SportIdent.h"
#include <cmath>
#include "intkeymapimpl.hpp"
#include "RunnerDB.h"
#include "meosexception.h"
#include <algorithm>
#include "socket.h"
#include "MeOSFeatures.h"
#include "oListInfo.h"
#include "qualification_final.h"
#include "metalist.h"
#include "cardsystem.h"
#include "datadefiners.h"
#include "xmlparser.h"
#include <unordered_map>
#include "oRunnerInternal.h"


pClub oAbstractRunner::setClubId(int clubId)
{
  pClub pc=Club;
  Club = oe->getClub(clubId);
  if (pc != Club) {
    updateChanged();
    if (Class) {
      // Vacant clubs have special logic
      Class->tResultInfo.clear();
    }
    if (Club && Club->isVacant()) { // Clear entry date/time for vacant
      getDI().setInt("EntryDate", 0);
      getDI().setInt("EntryTime", 0);
    }
  }
  return Club;
}

void oRunner::setClub(const wstring &clubName)
{
  if (tParentRunner)
    tParentRunner->setClub(clubName);
  else {
    oAbstractRunner::setClub(clubName);
    propagateClub();
  }
}

pClub oRunner::setClubId(int clubId) {
  if (tParentRunner)
    tParentRunner->setClubId(clubId);
  else {
    oAbstractRunner::setClubId(clubId);

    propagateClub();
  }
  return Club;
}

void oRunner::propagateClub() {
  for (size_t k = 0; k < multiRunner.size(); k++) {
    if (multiRunner[k] && multiRunner[k]->Club != Club) {
      multiRunner[k]->Club = Club;
      multiRunner[k]->updateChanged();
    }
  }
  if (tInTeam && tInTeam->getClubRef() != Club && ((Class && Class->getNumDistinctRunners() == 1) || tInTeam->getNumAssignedRunners() <= 1)) {
    tInTeam->Club = Club;
    tInTeam->updateChanged();
  }
}

void oAbstractRunner::setStartNo(int no, ChangeType changeType) {
  if (no!=StartNo) {
    if (oe)
      oe->bibStartNoToRunnerTeam.clear();
    StartNo=no;
    updateChanged(changeType);
  }
}

void oRunner::setStartNo(int no, ChangeType changeType) {
  bool individualSno = false;
    
  if (Class) {
    // Check if we can allow different forkings for different runners
    if (Class->isQualificationFinalBaseClass())
      individualSno = true;
    else if (Class->getLegType(0) == LegTypes::LTGroup) {
      individualSno = true; 
    }
  }
  if (tInTeam) {
    if (tInTeam->getStartNo() == 0) {
      tInTeam->setStartNo(no, changeType);
      individualSno = false; // Set all initially
    }
    else if (!individualSno) {
      // Do not allow different from team
      no = tInTeam->getStartNo();
    }
  }
  if (individualSno)
    oAbstractRunner::setStartNo(no, changeType);
  else if (tParentRunner)
    tParentRunner->setStartNo(no, changeType);
  else {
    oAbstractRunner::setStartNo(no, changeType);

    for (size_t k=0;k<multiRunner.size();k++)
      if (multiRunner[k])
        multiRunner[k]->oAbstractRunner::setStartNo(no, changeType);
  }
}

void oRunner::updateStartNo(int no) {
  if (tInTeam) {
    tInTeam->synchronize(false);
    for (pRunner r : tInTeam->Runners) {
      if (r) {
        r->synchronize(false);
      }
    }

    tInTeam->setStartNo(no, ChangeType::Update);
    for (pRunner r : tInTeam->Runners) {
      if (r) {
        r->setStartNo(no, ChangeType::Update);
      }
    }

    tInTeam->synchronize(true);
    for (pRunner r : tInTeam->Runners) {
      if (r)
        r->synchronize(true);
    }
  }
  else {
    setStartNo(no, ChangeType::Update);
    synchronize(true);
  }
}

int oRunner::getPlace(bool allowUpdate) const {
  if (allowUpdate && tPlace.isOld(*oe)) {
    if (Class) {
      oEvent::ResultType rt = oEvent::ResultType::ClassResult;
      oe->calculateResults({ getClassId(true) }, rt, false);
    }
  }
  return tPlace.get(!allowUpdate);
}

RunnerStatus oRunner::getStatusComputed(bool allowUpdate) const { 
  if (allowUpdate && tPlace.isOld(*oe)) {
    if (Class) {
      oEvent::ResultType rt = oEvent::ResultType::ClassResult;
      oe->calculateResults({ getClassId(true) }, rt, false);
    }
  }
  return tComputedStatus != StatusUnknown ? tComputedStatus : tStatus; 
}

int oRunner::getCoursePlace(bool perClass) const {
  if (perClass) {
    if (tCourseClassPlace.isOld(*oe) && Class) {
      oEvent::ResultType rt = oEvent::ResultType::ClassCourseResult;
      oe->calculateResults({ getClassId(true) }, rt, false);
    }
    return tCourseClassPlace.get(false);

  }
  else {
    if (tCoursePlace.isOld(*oe) && Class) {
      oEvent::ResultType rt = oEvent::ResultType::CourseResult;
      oe->calculateResults({ getClassId(true) }, rt, false);
    }
    return tCoursePlace.get(false);
  }
}

int oRunner::getTotalPlace(bool allowUpdate) const {
  if (tInTeam)
    return tInTeam->getLegPlace(getParResultLeg(), true, allowUpdate);
  else {
    if (allowUpdate && tTotalPlace.isOld(*oe) && Class) {
      oEvent::ResultType rt = oEvent::ResultType::TotalResult;
      oe->calculateResults({ getClassId(true) }, rt, false);
    }
    return tTotalPlace.get(!allowUpdate);
  }
}

wstring oAbstractRunner::getPlaceS() const
{
  wchar_t bf[16];
  int p=getPlace();
  if (p>0 && p<10000){
    swprintf(bf, 16, L"%d", p);
    return bf;
  }
  else return _EmptyWString;
}

wstring oAbstractRunner::getPrintPlaceS(bool withDot) const
{
  wchar_t bf[16];
  int p=getPlace();
  if (p>0 && p<10000){
    if (withDot) {
      swprintf(bf, 16, L"%d", p);
      return wstring(bf)+L".";
    }
    else
      return itow(p);
  }
  else return _EmptyWString;
}

wstring oAbstractRunner::getTotalPlaceS() const
{
  wchar_t bf[16];
  int p=getTotalPlace();
  if (p>0 && p<10000){
    swprintf(bf, 16, L"%d", p);
    return bf;
  }
  else return _EmptyWString;
}

wstring oAbstractRunner::getPrintTotalPlaceS(bool withDot) const
{
  wchar_t bf[16];
  int p=getTotalPlace();
  if (p>0 && p<10000){
    if (withDot) {
      swprintf(bf, 16, L"%d", p);
      return wstring(bf)+L".";
    }
    else
      return itow(p);
  }
  else return _EmptyWString;
}
wstring oRunner::getGivenName() const
{
  return ::getGivenName(sName);
}

wstring oRunner::getFamilyName() const
{
  return ::getFamilyName(sName);
}

void oRunner::setCardNo(int cno, bool matchCard, bool updateFromDatabase)
{
  if (cno != getCardNo()) {
    int oldNo = getCardNo();
    cardNumber = cno;

    if (oe->cardToRunnerHash && cno != 0 && isAddedToEvent() && !isTemporaryObject) {
      oe->cardToRunnerHash->emplace(cno, this);
    }

    if (isAddedToEvent()) {
      oFreePunch::rehashPunches(*oe, oldNo, 0);
      oFreePunch::rehashPunches(*oe, cardNumber, 0);
    }

    if (matchCard && !Card) {
      pCard c = oe->getCardByNumber(cno);

      if (c && !c->tOwner) {
        vector<pair<int, pControl>> mp;
        addCard(c, mp);
      }
    }

    if (!updateFromDatabase)
      updateChanged();
  }
}

int oRunner::getRentalCardFee(bool forAllRunners) const {
  const oRunner* parent = this;
  if (tParentRunner)
    parent = tParentRunner;
  
  if (forAllRunners) {
    // Count total card fee (case: several different rented cards or own card and then rented card)
    int fee = parent->getRentalCardFee(false);
    if (parent->multiRunner.size() > 0) {
      set<int> cards;
      for (int i = 0; i < parent->multiRunner.size(); i++) {
        pRunner r = parent->multiRunner[i];
        if (parent->cardNumber != r->cardNumber && r->getDCI().getInt("CardFee") > 0) {
          if (cards.insert(r->cardNumber).second)
            fee += r->getRentalCardFee(false);
        }
      }
    }
    return fee;
  }

  // Return the rental card fee.
  // The fee is only returned for the "first" runner
  // in a multi runner having the curren't runners card
  // to avoid duplicate fees
  if (!isRentalCard())
    return 0;
  
  bool okFirst = false;
  int fee = 0;
  if (parent->getCardNo() == getCardNo()) {
    if (parent != this)
      return 0;
    fee = max<int>(fee, parent->getDCI().getInt("CardFee"));
    okFirst = true;
  }

  for (pRunner r : parent->multiRunner) {
    if (r && r->getCardNo() == getCardNo()) {
      if (parent != this && !okFirst)
        return 0; // Was not first runner with this card

      fee = max<int>(fee, r->getDCI().getInt("CardFee"));
      okFirst = true;
    }
  }

  return fee;
}

/** Set rental card status (does not update fee)*/
void oRunner::setRentalCard(bool rental) {
  const bool rentalState = isRentalCard();
  if (rental && !rentalState) {
    getDI().setInt("CardFee", oe->getBaseCardFee());
  }
  else if (!rental && rentalState) {
    // Reset card fee
    oRunner* parent = this;
    if (tParentRunner)
      parent = tParentRunner;
    if (parent->getCardNo() == getCardNo())
      parent->getDI().setInt("CardFee", 0);
    for (pRunner r : parent->multiRunner) {
      if (r && r->getCardNo() == getCardNo()) {
        r->getDI().setInt("CardFee", 0);
      }
    }
  }
}

bool oRunner::isRentalCard() const {
  if (getDCI().getInt("CardFee") != 0)
    return true;
  if (tParentRunner && tParentRunner != this)
    return tParentRunner->isRentalCard(getCardNo());
  else
    return isRentalCard(cardNumber);
}

bool oRunner::isRentalCard(int cno) const {
  if (cno == getCardNo())
    return getDCI().getInt("CardFee") != 0;

  for (pRunner r : multiRunner) {
    if (r && r->getCardNo() == cno && r->getDCI().getInt("CardFee") != 0)
      return true;
  }
  return false;
}

int oRunner::setCard(int cardId) {
  pCard c = cardId ? oe->getCard(cardId) : nullptr;
  int oldId = 0;

  auto clearRG = [](pRunner r) {
    r->tRogaining.clear();
    r->tRogainingPoints = 0;
    r->tRogainingPointsGross = 0;
    r->tRogainingOvertime = 0;
  };

  if (Card != c) {
    if (Card) {
      oldId = Card->getId();
      Card->tOwner = nullptr;
    }
    if (c) {
      if (c->tOwner) {
        pRunner otherR = c->tOwner;
        assert(otherR != this);
        otherR->Card = nullptr;
        otherR->updateChanged();
        otherR->setStatus(StatusUnknown, true, ChangeType::Update);
        otherR->synchronize(true);
        clearRG(otherR);
      }
      c->tOwner = this;
      setCardNo(c->cardNo, false, true);
    }
    else {
      clearRG(this);
    }
    Card = c;
    vector<pair<int, pControl>> mp;
    evaluateCard(true, mp, 0, ChangeType::Update);
    updateChanged();
    synchronize(true);
  }
  return oldId;
}

void oAbstractRunner::setName(const wstring &n, bool manualUpdate)
{
  wstring tn = trim(n);
  if (tn.empty())
    throw std::runtime_error("Tomt namn är inte tillåtet.");
  if (tn != sName){
    sName.swap(tn);
    if (manualUpdate)
      setFlag(FlagUpdateName, true);
    updateChanged();
  }
}

void oRunner::setName(const wstring &in, bool manualUpdate)
{
  wstring n = trim(in);
  bool wasSpace = false;
  int kx = 0;
  for (size_t k = 0; k < n.length(); k++) {
    if (iswspace(n[k])) {
      if (!wasSpace) {
        n[kx++] = ' ';
        wasSpace = true;
      }
    }
    else {
      n[kx++] = n[k];
      wasSpace = false;
    }
  }
  if (wasSpace)
    kx = kx - 1;
  n.resize(kx);

  if (n.empty())
    throw std::runtime_error("Tomt namn är inte tillåtet.");

  if (n.length() <= 4 || n == lang.tl("N.N."))
    manualUpdate = false; // Never consider default names manual

  if (tParentRunner)
    tParentRunner->setName(n, manualUpdate);
  else {
    wstring oldName = sName;
    wstring oldRealName = tRealName;
    wstring newRealName;
    getRealName(n, newRealName);
    if (newRealName != tRealName || n != sName) {
      sName = n;
      tRealName = newRealName;

      if (manualUpdate)
        setFlag(FlagUpdateName, true);
      
      setFlag(TransferFlags::FlagUnnamed, false);

      updateChanged();
    }

    for (size_t k=0;k<multiRunner.size();k++) {
      if (multiRunner[k] && n!=multiRunner[k]->sName) {
        multiRunner[k]->sName = n;
        multiRunner[k]->tRealName = tRealName;
        multiRunner[k]->updateChanged();
      }
    }
    if (tInTeam && Class && Class->isSingleRunnerMultiStage()) {
      if (tInTeam->sName == oldName || tInTeam->sName == oldRealName)
        tInTeam->setName(tRealName, manualUpdate);
    }
  }
}

const wstring &oRunner::getName() const {
  return tRealName;
}

const wstring& getNameLastFirst(const wstring &sName) {
  if (sName.find_first_of(',') != sName.npos)
    return sName;  // Already "Fiske, Eric"
  if (sName.find_first_of(' ') == sName.npos)
    return sName; // No space "Vacant", "Eric"

  wstring& res = StringCache::getInstance().wget();
  res = getFamilyName(sName) + L", " + getGivenName(sName);
  return res;
}

const wstring &oRunner::getNameLastFirst() const {
  return ::getNameLastFirst(sName);
}

void oRunner::getRealName(const wstring &input, wstring &output) const {
  bool wasSpace = false;
  wstring n = input;
  int kx = 0;
  for (size_t k = 0; k < n.length(); k++) {
    if (iswspace(n[k])) {
      if (!wasSpace) {
        n[kx++] = ' ';
        wasSpace = true;
      }
    }
    else {
      if (n[k] == ',' && wasSpace)
        kx--; // Ignore space before comma

      n[kx++] = n[k];
      wasSpace = false;
    }
  }
  if (wasSpace)
    kx = kx - 1;
  n.resize(kx);

  size_t comma = n.find_first_of(',');
  if (oe->getNameMode() != oEvent::NameMode::LastFirst) {
    if (comma == string::npos)
      output = n;
    else
      output = trim(n.substr(comma + 1) + L" " + trim(n.substr(0, comma)));
  }
  else {
    if (comma != string::npos)
      output = n;
    else
      output = ::getNameLastFirst(n);
  }
}

bool oAbstractRunner::isResultStatus(RunnerStatus st) {
  switch (st) {
    case StatusDNS:
    case StatusCANCEL:
    case StatusOutOfCompetition:
    case StatusNotCompeting:
    case StatusUnknown:
    case StatusNoTiming:
      return false;
    default:
      return true;
  }
}

bool oAbstractRunner::setStatus(RunnerStatus st, bool updateSource, ChangeType changeType, bool recalculate) {
  assert(!(updateSource && changeType == ChangeType::Quiet));
  
  bool ch = false;
  if (tStatus != st) {
    ch = true;
    bool someOK = (st == StatusOK) || (tStatus == StatusOK);
    tStatus = st;

    if (Class && someOK) {
      Class->clearCache(recalculate);
    }
    if (st == StatusUnknown)
      tComputedStatus = StatusUnknown;
  }

  if (st != status) {
    status = st;
    if (updateSource) {
      updateChanged(changeType);
      if (st == StatusOutOfCompetition)
        setFlag(TransferFlags::FlagOutsideCompetition, true);
      else {
        setFlag(TransferFlags::FlagOutsideCompetition, false);
      }

      if (st == StatusNoTiming)
        setFlag(TransferFlags::FlagNoTiming, true);
      else {
        setFlag(TransferFlags::FlagNoTiming, false);
      }
    }
    else
      changedObject();
  }

  return ch;
}

int oAbstractRunner::getPrelRunningTime() const
{
  if (FinishTime>0 && tStatus!=StatusDNS && tStatus != StatusCANCEL && tStatus!=StatusDNF && tStatus!=StatusNotCompeting)
    return getRunningTime(true);
  else if (tStatus==StatusUnknown)
    return oe->getComputerTime()-tStartTime;
  else return 0;
}

oDataContainer &oRunner::getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const {
  data = (pvoid)oData;
  olddata = (pvoid)oDataOld;
  strData = const_cast<pvectorstr>(&dynamicData);
  return *oe->oRunnerData;
}

void oEvent::getRunners(int classId, int courseId, vector<pRunner> &r, bool sort) {
  if (sort) {
    synchronizeList(oListId::oLRunnerId);

    if (classId > 0 && classIdToRunnerHash) {
      sortRunners(SortByName, (*classIdToRunnerHash)[classId]);
    }
    else
     sortRunners(SortByName);
  }

  r.clear();

  if (classId > 0 && classIdToRunnerHash) {
    auto &rh = (*classIdToRunnerHash)[classId];
    r.reserve(rh.size());
    for (pRunner rr : rh) {
      if (!rr->isRemoved() && rr->getClassId(true) == classId) {
        
        bool skip = false;
        if (courseId > 0) {
          pCourse pc = rr->getCourse(false);
          if (pc == 0 || pc->getId() != courseId)
            skip = true;
        }

        if (!skip)
          r.push_back(rr);
      }
    }
    return;
  }

  if (classId <= 0)
    r.reserve(Runners.size());
  else if (Classes.size() > 0)
    r.reserve((Runners.size()*min<size_t>(Classes.size(), 4)) / Classes.size());

  bool hash = false;
  if (!classIdToRunnerHash) {
    classIdToRunnerHash = make_shared<map<int, vector<pRunner>>>();
    hash = true;
  }

  for (oRunnerList::iterator it = Runners.begin(); it != Runners.end(); ++it) {
    if (it->isRemoved())
      continue;

    bool skip = false;
    if (courseId > 0) {
      pCourse pc = it->getCourse(false);
      if (pc == 0 || pc->getId() != courseId)
        skip = true; // May still be used to setup hash
    }
    int clsId = 0;
    if (!skip && classId <= 0 || (clsId = it->getClassId(true)) == classId)
      r.push_back(&*it);

    if (hash) {
      if (clsId == 0)
        clsId = it->getClassId(true);

      if (clsId != 0)
        (*classIdToRunnerHash)[clsId].push_back(&*it);
    }
  }
}

void oEvent::getRunners(const set<int> &classId, vector<pRunner> &r, bool synchRunners) {
  if (classId.size() == Classes.size() || classId.size() == 0) {
    getRunners(0, 0, r, synchRunners);
    return;
  }

  if (synchRunners) {
    synchronizeList(oListId::oLRunnerId);
  }

  getRunners(classId, r);
}

void oEvent::getRunners(const set<int> &classId, vector<pRunner> &r) const {
  if (classId.size() == Classes.size() || classId.size() == 0) {
    const_cast<oEvent *>(this)->getRunners(0, 0, r, false);
    return;
  }

  r.clear();

  if (classIdToRunnerHash) {
    size_t s = 0;
    for (int cid : classId)
      s += (*classIdToRunnerHash)[cid].size();
    r.reserve(s);

    for (int cid : classId) {
      auto &rh = (*classIdToRunnerHash)[cid];
      for (pRunner rr : rh) {
        if (!rr->isRemoved() && rr->getClassId(true) == cid)
          r.push_back(rr);
      }
    }
    return;
  }

  r.reserve(Runners.size());
  classIdToRunnerHash = make_shared<map<int, vector<pRunner>>>();
  
  for (auto it = Runners.begin(); it != Runners.end(); ++it) {
    if (it->isRemoved())
      continue;
    int clsId = it->getClassId(true);
    pRunner rr = const_cast<pRunner>(&*it);
    if (classId.count(clsId))
      r.push_back(rr);

    if (clsId != 0)
      (*classIdToRunnerHash)[clsId].push_back(rr);
  }
}

pRunner oEvent::getRunner(int Id, int stage) const
{
  pRunner value;

  if (runnerById.lookup(Id, value) && value) {
    if (value->isRemoved())
      return 0;
    assert(value->Id == Id);
    if (stage==0)
      return value;
    else if (unsigned(stage)<=value->multiRunner.size())
      return value->multiRunner[stage-1];
  }
  return 0;
}

pRunner oRunner::nextNeedReadout() const {
  if (tInTeam) {
    bool isQF = Class && Class->isQualificationFinalBaseClass();

    // For a runner in a team, first the team for the card
    for (size_t k = 0; k < tInTeam->Runners.size(); k++) {
      pRunner tr = tInTeam->Runners[k];
      if (tr && k > 0 && isQF) {
        if (tr->getDCI().getInt("Heat") == 0)
         continue; // Not qualified. Maybe directly qualified for higher final.
      }
      if (tr && tr->getCardNo() == getCardNo() && !tr->Card && !tr->statusOK(false, false))
        return tr;
    }
  }

  if (!Card || Card->cardNo!=getCardNo() || Card->isConstructedFromPunches()) //-1 means card constructed from punches
    return pRunner(this);

  for (size_t k=0;k<multiRunner.size();k++) {
    if (multiRunner[k] && (!multiRunner[k]->Card ||
           multiRunner[k]->Card->cardNo!=getCardNo()))
      return multiRunner[k];
  }
  return nullptr;
}

vector<pRunner> oEvent::getCardToRunner(int cardNo) const {
  if (!cardToRunnerHash || cardToRunnerHash->size() > Runners.size() * 2) {
    cardToRunnerHash = make_shared<unordered_multimap<int, pRunner>>();
    for (auto &rc : Runners) {
      pRunner r = const_cast<pRunner>(&rc);
      int cno = r->getCardNo();
      if (cno == 0 || r->isRemoved())
        continue;

      cardToRunnerHash->emplace(cno, r); // The cache is "to large" -> filter is needed when looking into it.
    }
  }
  vector<pRunner> res;
  set<int> ids;
  auto rng = cardToRunnerHash->equal_range(cardNo);
  for (auto it = rng.first; it != rng.second; ++it) {
    pRunner r = it->second;
    if (!r->isRemoved() && r->getCardNo() == cardNo) {
      if (ids.insert(r->getId()).second)
        res.push_back(r);
      
      for (pRunner r2 : r->multiRunner) {
        if (r2 && r2->getCardNo() == cardNo) {
          if (ids.insert(r2->getId()).second)
            res.push_back(r2);
        }
      }
    }
  }
  return res;
}

pRunner oEvent::getRunnerByCardNo(int cardNo, int time, CardLookupProperty prop) const {
  auto range = getCardToRunner(cardNo);
  bool skipDNS = (prop == CardLookupProperty::SkipNoStart || prop == CardLookupProperty::CardInUse);

  if (range.size() == 1) {
    // Single hit
    pRunner r = range[0];
    if (r->isRemoved() || r->getCardNo() != cardNo)
      return nullptr;
    if (skipDNS && (r->getStatus() == StatusDNS || r->getStatus() == StatusCANCEL))
      return nullptr;
    if (prop != CardLookupProperty::IncludeNotCompeting && r->getStatus() == StatusNotCompeting)
      return nullptr;
    if (prop == CardLookupProperty::ForReadout || prop == CardLookupProperty::CardInUse)
      return r->nextNeedReadout();

    return r; // Only one runner with this card
  }
  vector<pRunner> cand;
  bool forceRet = false;

  for (auto r : range) {
    if (skipDNS && (r->getStatus() == StatusDNS || r->getStatus() == StatusCANCEL))
      continue;

    if (prop != CardLookupProperty::IncludeNotCompeting && r->getStatus() == StatusNotCompeting)
      continue;

    if (prop == CardLookupProperty::OnlyMainInstance && r->skip())
      continue;

    cand.push_back(r);
  }

  if (time <= 0) { //No time specified. Card readout search
    pRunner secondTry = nullptr;
    pRunner dnsR = nullptr;
    for (pRunner r : cand) {
      pRunner ret = r->nextNeedReadout();
      if (ret) {
        if (ret->getStatus() == StatusDNS || ret->getStatus() == StatusCANCEL || ret->getStatus() == StatusDNF)
          dnsR = ret; //Return a DNS runner if there is no better match.
        else if (!r->skip())
          return ret;
        else if (secondTry == 0 || secondTry->tLeg > ret->tLeg)
          secondTry = ret;
      }
    }
    if (secondTry)
      return secondTry;
    if (dnsR)
      return dnsR;
  }
  else {
    pRunner bestR = 0;
    const int K = timeConstHour * 24;
    int dist = 10 * K;
    for (size_t k = 0; k < cand.size(); k++) {
      pRunner r = cand[k];
      if (time <= 0)
        return r; // No time specified.
      //int start = r->getStartTime();
      //int finish = r->getFinishTime();
      int start = r->getStartTime();
      int finish = r->getFinishTime();
      if (r->getCard()) {
        pair<int, int> cc = r->getCard()->getTimeRange();
        if (cc.first > 0)
          start = min(start, cc.first);
        if (cc.second > 0)
          finish = max(finish, cc.second);
      }
      start = max(0, start - 3 * timeConstMinute); // Allow some extra time before start

      if (start > 0 && finish > 0 && time >= start && time <= finish)
        return r;
      int d = 3 * K;
      if (start > 0 && finish > 0 && start < finish) {
        if (time < start)
          d += K + (start - time);
        else if (time > finish)
          d += K + (time - finish);
      }
      else {
        if (start > 0) {
          if (time < start)
            d = K + start - time;
          else
            d = time - start;
        }
        if (finish > 0) {
          if (time > finish)
            d += K + time - finish;
        }
      }
      if (d < dist) {
        bestR = r;
        dist = d;
      }
    }

    if (bestR != 0 || forceRet)
      return bestR;
  }

  if (prop != CardLookupProperty::ForReadout && !skipDNS) 	{
    for (pRunner r : cand) {
      pRunner rx = r->nextNeedReadout();
      return rx ? rx : r;
    }
  }

  return nullptr;
}

void oEvent::getRunnersByCardNo(int cardNo, bool sortUpdate, CardLookupProperty prop, vector<pRunner> &out) const {
  out.clear();
  bool skipDNS = (prop == CardLookupProperty::SkipNoStart || prop == CardLookupProperty::CardInUse);

  if (sortUpdate)
    const_cast<oEvent *>(this)->synchronizeList(oListId::oLRunnerId);
    
  if (cardNo != 0) {
    auto range = getCardToRunner(cardNo);
    for (auto r : range) {
      if (skipDNS && (r->getStatus() == StatusDNS || r->getStatus() == StatusCANCEL))
        continue;
      if (prop == CardLookupProperty::OnlyMainInstance && r->getRaceNo() != 0)
        continue;
      if (prop != CardLookupProperty::IncludeNotCompeting && r->getStatus() == StatusNotCompeting)
        continue;
      if (prop == CardLookupProperty::ForReadout && r->getCard() && !r->getCard()->isConstructedFromPunches())
        continue;

      out.push_back(r);
    }
  }
  else {
    for (auto it=Runners.begin(); it != Runners.end(); ++it) {
      pRunner r = const_cast<pRunner>(&*it);
      if (r->isRemoved() || r->getCardNo() != cardNo)
        continue;
      if (skipDNS && (r->getStatus() == StatusDNS || r->getStatus() == StatusCANCEL))
        continue;
      if (prop == CardLookupProperty::OnlyMainInstance && r->getRaceNo() != 0)
        continue;
      if (prop != CardLookupProperty::IncludeNotCompeting && r->getStatus() == StatusNotCompeting)
        continue;
      if (prop == CardLookupProperty::ForReadout && r->getCard() && !r->getCard()->isConstructedFromPunches())
        continue;

      out.push_back(r);
    }
  }
  
  if (sortUpdate) {
    const_cast<oEvent *>(this)->CurrentSortOrder = SortByName;
    sort(out.begin(), out.end(), [](const pRunner &a, const pRunner &b) {return *a < *b; });
  }
}

int oRunner::getRaceIdentifier() const {
  if (tParentRunner)
    return tParentRunner->getRaceIdentifier();// A unique person has a unique race identifier, even if the race is "split" into several

  int stored = getDCI().getInt("RaceId");
  if (stored != 0)
    return stored;

  if (!tInTeam)
    return 1000000 + (Id&0xFFFFFFF) * 2;//Even
  else
    return 1000000 * (tLeg+1) + (tInTeam->Id & 0xFFFFFFF) * 2 + 1;//Odd
}

static int getEncodedBib(const wstring &bib) {
  int enc = 0;
  for (size_t j = 0; j < bib.length(); j++) { //WCS
    int x = toupper(bib[j])-32;
    if (x<0)
      return 0; // Not a valid bib
    enc = enc * 97 - x;
  }
  return enc;
}

int oAbstractRunner::getEncodedBib() const {
  return ::getEncodedBib(getBib());
}


typedef multimap<int, oAbstractRunner*>::iterator BSRTIterator;

pRunner oEvent::getRunnerByBibOrStartNo(const wstring &bib, bool findWithoutCardNo) const {
  if (bib.empty() || bib == L"0")
    return 0;

  if (bibStartNoToRunnerTeam.empty()) {
    for (oTeamList::const_iterator tit = Teams.begin(); tit != Teams.end(); ++tit) {
      const oTeam &t=*tit;
      if (t.skip())
        continue;

      int sno = t.getStartNo();
      if (sno != 0)
        bibStartNoToRunnerTeam.insert(make_pair(sno, (oAbstractRunner *)&t));
      int enc = t.getEncodedBib();
      if (enc != 0)
        bibStartNoToRunnerTeam.insert(make_pair(enc, (oAbstractRunner *)&t));
    }

    for (oRunnerList::const_iterator it=Runners.begin(); it != Runners.end(); ++it) {
      if (it->skip())
        continue;
       const oRunner &t=*it;

      int sno = t.getStartNo();
      if (sno != 0)
        bibStartNoToRunnerTeam.insert(make_pair(sno, (oAbstractRunner *)&t));
      int enc = t.getEncodedBib();
      if (enc != 0)
        bibStartNoToRunnerTeam.insert(make_pair(enc, (oAbstractRunner *)&t));
    }
  }

  int sno = wtoi(bib.c_str());

  pair<BSRTIterator, BSRTIterator> res;
  if (sno > 0) {
    // Require that a bib starts with numbers
    int bibenc = getEncodedBib(bib);
    res = bibStartNoToRunnerTeam.equal_range(bibenc);
    if (res.first == res.second)
      res = bibStartNoToRunnerTeam.equal_range(sno); // Try startno instead

    for(BSRTIterator it = res.first; it != res.second; ++it) {
      oAbstractRunner *pa = it->second;
      if (pa->isRemoved())
        continue;

      if (typeid(*pa)==typeid(oRunner)) {
        oRunner &r = dynamic_cast<oRunner &>(*pa);
        if (r.getStartNo()==sno || stringMatch(r.getBib(), bib)) {
          if (findWithoutCardNo) {
            if (r.getCardNo() == 0 && r.needNoCard() == false)
              return &r;
          }
          else {
            if (r.getNumMulti()==0 || r.tStatus == StatusUnknown)
              return &r;
            else {
              for(int race = 0; race < r.getNumMulti(); race++) {
                pRunner r2 = r.getMultiRunner(race);
                if (r2 && r2->tStatus == StatusUnknown)
                  return r2;
              }
              return &r;
            }
          }
        }
      }
      else {
        oTeam &t = dynamic_cast<oTeam &>(*pa);
        if (t.getStartNo()==sno || stringMatch(t.getBib(), bib)) {
          if (!findWithoutCardNo) {
            for (int leg=0; leg<t.getNumRunners(); leg++) {
              pRunner r = t.Runners[leg];
              if (r && r->getCardNo() > 0 && r->getStatus()==StatusUnknown)
                return r;
            }
          }
          else {
            for (int leg=0; leg<t.getNumRunners(); leg++) {
              pRunner r = t.Runners[leg];
              if (r && r->getCardNo() == 0 && r->needNoCard() == false)
                return r;
            }
          }
        }
      }
    }
  }
  return nullptr;
}

pRunner oEvent::getRunnerByName(const wstring &pname, const wstring &pclub, int classId) const {
  vector<pRunner> cnd;

  if (classId <= 0) {
    for (auto &r : Runners) {
      if (!r.skip() && r.matchName(pname)) {
        if (pclub.empty() || pclub == r.getClub())
          cnd.push_back(pRunner(&r));
      }
    }
  }
  else {
    vector<pRunner> rr;
    getRunners({ classId }, rr);
    for (auto r : rr) {
      if (!r->skip() && r->matchName(pname)) {
        if (pclub.empty() || pclub == r->getClub())
          cnd.push_back(r);
      }
    }
  }
  if (cnd.size() == 1)
    return cnd[0]; // Only return if uniquely defined.

  return 0;
}

void oEvent::fillRunners(gdioutput &gdi, const string &id, bool longName, int filter)
{
  vector< pair<wstring, size_t> > d;
  oe->fillRunners(d, longName, filter, unordered_set<int>());
  gdi.setItems(id, d);
}

const vector< pair<wstring, size_t> > &oEvent::fillRunners(vector< pair<wstring, size_t> > &out,
                                                           bool longName, int filter,
                                                           const unordered_set<int> &personFilter)
{
  const bool showAll = (filter & RunnerFilterShowAll) == RunnerFilterShowAll;
  const bool noResult = (filter & RunnerFilterOnlyNoResult) ==  RunnerFilterOnlyNoResult;
  const bool withResult = (filter & RunnerFilterWithResult) ==  RunnerFilterWithResult;
  const bool compact = (filter & RunnerCompactMode) == RunnerCompactMode;

  synchronizeList(oListId::oLRunnerId);
  oRunnerList::iterator it;
  int lVacId = getVacantClubIfExist(false);
  if (getNameMode() == LastFirst)
    CurrentSortOrder = SortByLastName;
  else
    CurrentSortOrder = SortByName;
  Runners.sort();
  out.clear();
  if (personFilter.empty())
    out.reserve(Runners.size());
  else
    out.reserve(personFilter.size());

  wchar_t bf[512];
  const bool usePersonFilter = !personFilter.empty();

  if (longName) {
    for (it=Runners.begin(); it != Runners.end(); ++it) {
      if (noResult && (it->Card || it->FinishTime>0))
        continue;
      if (withResult && !it->Card && it->FinishTime == 0)
        continue;
      if (usePersonFilter && personFilter.count(it->Id) == 0)
        continue;
      if (!it->skip() || (showAll && !it->isRemoved())) {
        if (compact) {
          const wstring &club = it->getClub();
          if (!club.empty()) {
            swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%s, %s (%s)", it->getNameAndRace(true).c_str(),
                       club.c_str(),
                       it->getClass(true).c_str());
          }
          else {
            swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%s (%s)", it->getNameAndRace(true).c_str(),
                       it->getClass(true).c_str());
          }

        } else {
          swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%s\t%s\t%s", it->getNameAndRace(true).c_str(),
                                        it->getClass(true).c_str(),
                                        it->getClub().c_str());
        }
        out.emplace_back(bf, it->Id);
      }
    }
  }
  else {
    for (it=Runners.begin(); it != Runners.end(); ++it) {
      if (noResult && (it->Card || it->FinishTime>0))
        continue;
      if (withResult && !it->Card && it->FinishTime == 0)
        continue;
      if (usePersonFilter && personFilter.count(it->Id) == 0)
        continue;

      if (!it->skip() || (showAll && !it->isRemoved())) {
        if ( it->getClubId() != lVacId || lVacId == 0)
          out.push_back(make_pair(it->getUIName(), it->Id));
        else {
          swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%s (%s)", it->getUIName().c_str(), it->getClass(true).c_str());
          out.emplace_back(bf, it->Id);
        }
      }
    }
  }
  return out;
}

void oRunner::resetPersonalData()
{
  oDataInterface di = getDI();
  di.setInt("BirthYear", 0);
  di.setString("Nationality", L"");
  di.setString("Country", L"");
  di.setInt64("ExtId", 0);
}

wstring oRunner::getNameAndRace(bool userInterface) const
{
  if (tDuplicateLeg>0 || multiRunner.size()>0) {
    wchar_t bf[16];
    swprintf(bf, sizeof(bf)/sizeof(wchar_t), L" (%d)", getRaceNo()+1);
    if (userInterface)
      return getUIName() + bf;
    return getName()+bf;
  }
  else if (userInterface)
    return getUIName();
  else return getName();
}

pRunner oRunner::getMultiRunner(int race) const
{
  if (race==0) {
    if (!tParentRunner)
      return pRunner(this);
    else return tParentRunner;
  }

  const vector<pRunner> &mr = tParentRunner ? tParentRunner->multiRunner : multiRunner;

  if (unsigned(race-1)>=mr.size()) {
    assert(tParentRunner);
    return 0;
  }

  return mr[race-1];
}

void oRunner::createMultiRunner(bool createMaster, bool sync)
{
  if (tDuplicateLeg)
    return; //Never allow chains.
  bool allowCreate = true;
  if (multiRunnerId.size()>0) {
    multiRunner.resize(multiRunnerId.size() - 1);
    for (size_t k=0;k<multiRunner.size();k++) {
      multiRunner[k]=oe->getRunner(multiRunnerId[k], 0);
      if (multiRunner[k]) {
        if (multiRunner[k]->multiRunnerId.size() > 1 || !multiRunner[k]->multiRunner.empty())
          multiRunner[k]->markForCorrection();

        multiRunner[k]->multiRunner.clear(); //Do not allow chains
        multiRunner[k]->multiRunnerId.clear();
        multiRunner[k]->tDuplicateLeg = k+1;
        multiRunner[k]->tParentRunner = this;
     
        if (multiRunner[k]->Id != multiRunnerId[k])
          markForCorrection();
      }
      else if (multiRunnerId[k] > 0) {
        markForCorrection();
        allowCreate = false;
      }

      assert(multiRunner[k]);
    }
    multiRunnerId.clear();
  }

  if (!Class || !createMaster)
    return;

  int ndup=0;

  if (!tInTeam)
    ndup=Class->getNumMultiRunners(0);
  else
    ndup=Class->getNumMultiRunners(tLeg);

  bool update = false;

  vector<int> toRemove;

  for (size_t k = ndup-1; k<multiRunner.size();k++) {
    if (multiRunner[k] && multiRunner[k]->getStatus()==StatusUnknown) {
      toRemove.push_back(multiRunner[k]->getId());
      multiRunner[k]->tParentRunner = 0;
      if (multiRunner[k]->tInTeam && size_t(multiRunner[k]->tLeg)<multiRunner[k]->tInTeam->Runners.size()) {
        if (multiRunner[k]->tInTeam->Runners[multiRunner[k]->tLeg] == multiRunner[k])
          multiRunner[k]->tInTeam->Runners[multiRunner[k]->tLeg] = nullptr;
      }
    }
  }

  multiRunner.resize(ndup-1);
  for (int k = 1; k < ndup; k++) {
	  if (!multiRunner[k - 1] && allowCreate) {
		  update = true;
		  multiRunner[k - 1] = oe->addRunner(sName, getClubId(),
											 getClassId(false), 0, getBirthDate(), false);
		  multiRunner[k - 1]->tDuplicateLeg = k;
		  multiRunner[k - 1]->tParentRunner = this;
		  multiRunner[k - 1]->cardNumber = 0;

		  if (sync)
			  multiRunner[k - 1]->synchronize();
	  }
  }
  if (update)
    updateChanged();

  if (sync) {
    synchronize(true);
    if (toRemove.size() > 0)
      oe->removeRunner(toRemove);
  }
}

pRunner oRunner::getPredecessor() const
{
  if (!tParentRunner || unsigned(tDuplicateLeg-1)>=16)
    return 0;

  if (tDuplicateLeg==1)
    return tParentRunner;
  else
    return tParentRunner->multiRunner[tDuplicateLeg-2];
}

void oRunner::apply(ChangeType changeType, pRunner src) {
  for (size_t k = 0; k < multiRunner.size(); k++) {
    if (multiRunner[k] && multiRunner[k]->isRemoved()) {
      multiRunner[k]->tParentRunner = nullptr;
      multiRunner[k] = nullptr;
    }
  }

  createMultiRunner(false, false);

  tLeg = -1;
  tLegEquClass = 0;
  tUseStartPunch = true;
  if (tInTeam) {
    tInTeam->apply(changeType, this);
    if (Class && Class->isQualificationFinalBaseClass()) {
      if (tLeg > 0 && Class == getClassRef(true))
        tNeedNoCard = true; // Not qualified
    }
  }
  else {
    if (Class && Class->hasMultiCourse()) {
      pClass pc = Class;
      StartTypes st = pc->getStartType(tDuplicateLeg);
      if (st == STTime) {
        pCourse crs = getCourse(false);
        int startType = crs ? crs->getStartPunchType() : oPunch::PunchStart;
        bool hasStartPunch = Card && Card->getPunchByType(startType) != nullptr;
        if (!hasStartPunch || pc->ignoreStartPunch()) {
          setStartTime(pc->getStartData(tDuplicateLeg), false, changeType);
          tUseStartPunch = false;
        }
      }
      else if (st == STChange) {
        pRunner r = getPredecessor();
        int lastStart = 0;
        if (r && r->FinishTime > 0)
          lastStart = r->FinishTime;

        int restart = pc->getRestartTime(tDuplicateLeg);
        int rope = pc->getRopeTime(tDuplicateLeg);

        if (restart && rope && (lastStart > rope || lastStart == 0))
          lastStart = restart; //Runner in restart

        setStartTime(lastStart, false, changeType);
        tUseStartPunch = false;
      }
      else if (st == STPursuit) {
        pRunner r = getPredecessor();
        int lastStart = 0;

        if (r && r->FinishTime > 0 && r->statusOK(false, false)) {
          int rt = r->getRaceRunningTime(false, tDuplicateLeg - 1, false);
          int timeAfter = rt - pc->getTotalLegLeaderTime(oClass::AllowRecompute::NoUseOld, r->tDuplicateLeg, false, true);
          if (rt > 0 && timeAfter >= 0)
            lastStart = pc->getStartData(tDuplicateLeg) + timeAfter;
        }
        int restart = pc->getRestartTime(tDuplicateLeg);
        int rope = pc->getRopeTime(tDuplicateLeg);

        if (restart && rope && (lastStart > rope || lastStart == 0))
          lastStart = restart; //Runner in restart

        setStartTime(lastStart, false, changeType);
        tUseStartPunch = false;
      }
    }
  }

  if (tLeg == -1) {
    tLeg = 0;
    tInTeam = nullptr;
  }
}

void oRunner::cloneStartTime(const pRunner r) {
  if (tParentRunner)
    tParentRunner->cloneStartTime(r);
  else {
    setStartTime(r->getStartTime(), true, ChangeType::Update);

    for (size_t k=0; k < min(multiRunner.size(), r->multiRunner.size()); k++) {
      if (multiRunner[k]!=0 && r->multiRunner[k]!=0)
        multiRunner[k]->setStartTime(r->multiRunner[k]->getStartTime(), true, ChangeType::Update);
    }
    apply(ChangeType::Update, nullptr);
  }
}

void oRunner::cloneData(const pRunner r) {
  if (tParentRunner)
    tParentRunner->cloneData(r);
  else {
    size_t t = sizeof(oData);
    memcpy(oData, r->oData, t);
  }
}

unsigned static nStageMaxStored = -1;

const shared_ptr<Table> &oRunner::getTable(oEvent *oe) {
  int sn = oe->getStageNumber();
  vector<pRunner> runners;
  oe->getRunners(0, 0, runners, false);
  for (pRunner r : runners) {
    const wstring &raw = r->getDCI().getString("InputResult");
    int ns = (int)count(raw.begin(), raw.end(), ';');
    sn = max(sn, (ns + 1) / 3);
  }
  sn = min(10, sn);

  if (nStageMaxStored != sn || !oe->hasTable("runner")) {
    nStageMaxStored = sn;
    auto table = make_shared<Table>(oe, 20, L"Deltagare", "runners");

    table->addColumn("Id", 70, true, true);
    table->addColumn("Ändrad", 70, false);

    table->addColumn("Namn", 200, false);
    table->addColumn("Klass", 120, false);
    table->addColumn("Bana", 120, false);

    table->addColumn("Klubb", 120, false);
    table->addColumn("Lag", 120, false);
    table->addColumn("Sträcka", 70, true);

    table->addColumn("Bricka", 90, true, false);

    table->addColumn("Start", 70, false, true);
    table->addColumn("Mål", 70, false, true);
    table->addColumn("Status", 70, false);
    table->addColumn("Tid", 70, false, true);
    table->addColumn("Poäng", 70, true, true);

    table->addColumn("Plac.", 70, true, true);
    table->addColumn("Start nr.", 70, true, false);

    oe->oRunnerData->buildTableCol(table.get());

    for (unsigned k = 1; k < nStageMaxStored; k++) {
      table->addColumn(lang.tl("Tid E[stageno]") + itow(k), 70, false, true);
      table->addColumn(lang.tl("Status E[stageno]") + itow(k), 70, false, true);
      table->addColumn(lang.tl("Poäng E[stageno]") + itow(k), 70, true);
      table->addColumn(lang.tl("Plac. E[stageno]") + itow(k), 70, true);
    }

    table->addColumn("Tid in", 70, false, true);
    table->addColumn("Status in", 70, false, true);
    table->addColumn("Poäng in", 70, true);
    table->addColumn("Placering in", 70, true);
    table->addColumn("Bricktyp", 70, false, false);
    table->addColumn("Startenhet", 70, true, false);
    table->addColumn("Målenhet", 70, true, false);

    oe->setTable("runner", table);
  }

  return oe->getTable("runner");
}

void oEvent::generateRunnerTableData(Table &table, oRunner *addRunner)
{
  oe->calculateResults({}, ResultType::ClassResult, false);

  if (addRunner) {
    addRunner->addTableRow(table);
    return;
  }

  synchronizeList(oListId::oLRunnerId);
  oRunnerList::iterator it;
  table.reserve(Runners.size());
  for (it=Runners.begin(); it != Runners.end(); ++it){
    if (!it->isRemoved()){
      it->addTableRow(table);
    }
  }
}

pRunner oRunner::getReference() const
{
  int rid = getDCI().getInt("Reference");
  if (rid != 0)
    return oe->getRunner(rid, 0);
  else 
    return 0;
}

void oRunner::setReference(int runnerId)
{
  getDI().setInt("Reference", runnerId);
}

const wstring &oRunner::getUIName() const {
  oEvent::NameMode nameMode = oe->getNameMode();
  
  switch (nameMode) {
  case oEvent::Raw: 
    return getNameRaw();
  case oEvent::LastFirst:
    return getNameLastFirst();
  default:
    return getName();
  }
}

void oRunner::addTableRow(Table &table) const
{
  oRunner &it = *pRunner(this);
  table.addRow(getId(), &it);

  int row = 0;
  table.set(row++, it, TID_ID, itow(getId()), false);
  table.set(row++, it, TID_MODIFIED, getTimeStamp(), false);

  if (tParentRunner == 0)
    table.set(row++, it, TID_RUNNER, getUIName(), true);
  else
    table.set(row++, it, TID_RUNNER, getUIName() + L" (" + itow(tDuplicateLeg+1) + L")", false);
  table.set(row++, it, TID_CLASSNAME, getClass(true), true, cellSelection);
  table.set(row++, it, TID_COURSE, getCourseName(), true, cellSelection);
  table.set(row++, it, TID_CLUB, getClub(), tParentRunner == 0, cellCombo);

  table.set(row++, it, TID_TEAM, tInTeam ? tInTeam->getName() : L"", false);
  table.set(row++, it, TID_LEG, tInTeam ? itow(tLeg+1) : L"" , false);

  int cno = getCardNo();
  table.set(row++, it, TID_CARD, cno>0 ? itow(cno) : L"", true);

  table.set(row++, it, TID_START, getStartTimeS(), true);
  table.set(row++, it, TID_FINISH, getFinishTimeS(false, SubSecond::Auto), true);
  table.set(row++, it, TID_STATUS, getStatusS(false, true), true, cellSelection);
  table.set(row++, it, TID_RUNNINGTIME, getRunningTimeS(true, SubSecond::Auto), false);
  int rp = getRogainingPoints(true, false);
  table.set(row++, it, TID_POINTS, oe->formatScore(rp), false);

  table.set(row++, it, TID_PLACE, getPlaceS(), false);
  table.set(row++, it, TID_STARTNO, itow(getStartNo()), true);

  row = oe->oRunnerData->fillTableCol(it, table, true);
  
  if (nStageMaxStored > 1) {
    const wstring &raw = getDCI().getString("InputResult");
    vector<wstring> spvec;
    split(raw, L";", spvec);

    for (unsigned j = 0; j + 1 < nStageMaxStored; j++) {
      size_t k = j * 4;
      int rawStat = StatusUnknown;
      int rawTime = 0;
      int rawPoints = 0;
      int place = 0;

      if (k + 3 < spvec.size()) {
        rawStat = wtoi(spvec[k].c_str());
        rawTime = parseRelativeTime(spvec[k + 1].c_str());
        rawPoints = wtoi(spvec[k + 2].c_str());
        place = wtoi(spvec[k + 3].c_str());
      }
      table.set(row++, it, 200 + j, formatTime(rawTime));
      table.set(row++, it, 300 + j, oEvent::formatStatus(RunnerStatus(rawStat), false), true, cellSelection);
      table.set(row++, it, 400 + j, oe->formatScore(rawPoints));
      table.set(row++, it, 500 + j, place > 0 ? itow(place) : _EmptyWString);
    }
  }
  table.set(row++, it, TID_INPUTTIME, getInputTimeS(), true);
  table.set(row++, it, TID_INPUTSTATUS, getInputStatusS(), true, cellSelection);
  table.set(row++, it, TID_INPUTPOINTS, oe->formatScore(inputPoints), true);
  table.set(row++, it, TID_INPUTPLACE, itow(inputPlace), true);

  table.set(row++, it, TID_CARDTYPE, cardNumber > 0 ? oe->getCardSystem().getType(cardNumber) : _EmptyWString, false);
  int finishId = 0, startId = 0;
  if (Card) {
    startId = Card->getStartPunchCode();
    finishId = Card->getFinishPunchCode();
  }
  table.set(row++, it, TID_STARTCONTROL, startId > 0 ? itow(startId) : _EmptyWString);
  table.set(row++, it, TID_FINISHCONTROL, finishId > 0 ? itow(finishId) : _EmptyWString);
}

pair<int, bool> oRunner::inputData(int id, const wstring &input,
                                   int inputId, wstring &output, bool noUpdate)
{
  int t,s;
  vector<pair<int, pControl>> mp;
  synchronize(false);

  if (id>1000) {
    return oe->oRunnerData->inputData(this, id, input,
                                        inputId, output, noUpdate);
  }
  else if (id >= 200 && id <= 600) {
    int type = id / 100;
    int stage = id % 100;

    const wstring &raw = getDCI().getString("InputResult");
    vector<wstring> spvec;
    split(raw, L";", spvec);

    int nStageNow = spvec.size() / 4;
    int numStage = max(nStageNow, stage + 1);
    spvec.resize(numStage * 4);
    
    switch (type) {
    case 2:
    {
      int time = ::convertAbsoluteTimeHMS(input, -1);
      spvec[4 * stage + 1] = codeRelativeTimeW(time);
      output = formatTimeHMS(time);
    }
    break;
    case 3: {
      if (inputId >= 0) {
        spvec[4 * stage + 0] = itow(inputId);
        output = oEvent::formatStatus(RunnerStatus(inputId), false);
      }
    }
    break;
    case 4:
    {
      int points = oe->convertScore(input);
      spvec[4 * stage + 2] = itow(points);
      output = oe->formatScore(points);
    }
    break;
    case 5:
    {
      int place = wtoi(input.c_str());
      output = spvec[4 * stage + 3] = itow(place);
    }
    break;
    }

    wstring out;
    unsplit<wstring>(spvec, L";", out);
    getDI().setString("InputResult", out);

    return make_pair(0, false);
  }

  switch(id) {
    case TID_CARD:
      setCardNo(wtoi(input.c_str()), true);
      synchronizeAll();
      output = itow(getCardNo());
      break;
    case TID_RUNNER:
      if (trim(input).empty())
        throw std::runtime_error("Tomt namn inte tillåtet.");

      if (sName != input && tRealName != input) {
        updateFromDB(input, getClubId(), getClassId(false), getCardNo(), getBirthYear(), false);
        setName(input, true);
        synchronizeAll();
      }
      output = getName();
      break;
    break;

    case TID_START:
      setStartTimeS(input);
      t=getStartTime();
      evaluateCard(true, mp, 0, ChangeType::Update);
      s=getStartTime();
      if (s!=t)
        throw std::runtime_error("Starttiden är definerad genom klassen eller löparens startstämpling.");
      synchronize(true);
      output = getStartTimeS();
      break;
    break;

    case TID_FINISH:
      setFinishTimeS(input);
      t=getFinishTime();
      evaluateCard(true, mp, 0, ChangeType::Update);
      s=getFinishTime();
      if (s!=t)
        throw std::runtime_error("För att ändra måltiden måste löparens målstämplingstid ändras.");
      synchronize(true);
      output = getStartTimeS();
      break;
    break;

    case TID_COURSE:
      if (inputId == -1) {
        pCourse c = oe->getCourse(input);
        if (c)
          inputId = c->getId();
      }
      setCourseId(inputId);
      synchronize(true);
      output = getCourseName();
      break;

    case TID_CLUB:
      {
        pClub pc = 0;
        if (inputId > 0)
          pc = oe->getClub(inputId);
        else
          pc = oe->getClubCreate(0, input);

        updateFromDB(getName(), pc ? pc->getId():0, getClassId(false), getCardNo(), getBirthYear(), false);

        setClub(pc ? pc->getName() : L"");
        synchronize(true);
        output = getClub();
      }
      break;

    case TID_CLASSNAME:
      if (inputId == -1) {
        pClass c = oe->getClass(input);
        if (c)
          inputId = c->getId();
      }

      setClassId(inputId, true);
      synchronize(true);
      output = getClass(true);
      break;

    case TID_STATUS: {
      if (inputId >= 0) 
        setStatus(RunnerStatus(inputId), true, ChangeType::Update);
      int s = getStatus();
      evaluateCard(true, mp, 0, ChangeType::Update);
      if (s!=getStatus())
        throw std::runtime_error("Status matchar inte data i löparbrickan.");
      synchronize(true);
      output = getStatusS(false, true);
    }
    break;

    case TID_STARTNO:
      setStartNo(wtoi(input.c_str()), ChangeType::Update);
      synchronize(true);
      output = itow(getStartNo());
      break;

    case TID_INPUTSTATUS:
      if (inputId >= 0)
        setInputStatus(RunnerStatus(inputId));
      synchronize(true);
      output = getInputStatusS();
      break;

    case TID_INPUTTIME:
      setInputTime(input);
      synchronize(true);
      output = getInputTimeS();
      break;

    case TID_INPUTPOINTS:
      setInputPoints(oe->convertScore(input));
      synchronize(true);
      output = oe->formatScore(getInputPoints());
      break;

    case TID_INPUTPLACE:
      setInputPlace(wtoi(input.c_str()));
      synchronize(true);
      output = itow(getInputPlace());
      break;
  }

  return make_pair(0,false);
}

void oRunner::fillInput(int id, vector< pair<wstring, size_t> > &out, size_t &selected)
{
  if (id>1000) {
    oe->oRunnerData->fillInput(this, id, 0, out, selected);
    return;
  }

  if (id==TID_COURSE) {
    oe->getCourses(out, L"", true);
    out.push_back(make_pair(lang.tl(L"Klassens bana"), 0));
    selected = getCourseId();
  }
  else if (id==TID_CLASSNAME) {
    oe->fillClasses(out, oEvent::extraNone, oEvent::filterNone);
    out.push_back(make_pair(lang.tl(L"Ingen klass"), 0));
    selected = getClassId(true);
  }
  else if (id==TID_CLUB) {
    oe->fillClubs(out);
    out.push_back(make_pair(lang.tl(L"Klubblös"), 0));
    selected = getClubId();
  }
  else if (id==TID_STATUS) {
    oe->fillStatus(out);
    selected = getStatus();
  }
  else if (id==TID_INPUTSTATUS) {
    oe->fillStatus(out);
    selected = inputStatus;
  }
  else if (id >= 300 && id < 400) {
    size_t sIndex = id - 300;
    vector<RunnerStatus> rs;
    vector<int> times, points, places;
    getInputResults(rs, times, points, places);
    oe->fillStatus(out);
    if (sIndex < rs.size())
      selected = rs[sIndex];
    else
      selected = StatusUnknown;
  }
}

int oRunner::getSplitTime(int controlNumber, bool normalized) const
{
  if (!Card) {
    if (controlNumber == 0)
      return getPunchTime(0, false, true, false);
    else {
      int ct = getPunchTime(controlNumber, false, true, false);
      if (ct > 0) {
        int dt = getPunchTime(controlNumber - 1, false, true, false);
        if (dt > 0 && ct > dt)
          return ct - dt;
      }
    }

    return -1;
  }
  const vector<SplitData> &st = getSplitTimes(normalized);
  if (controlNumber>0 && controlNumber == st.size() && FinishTime>0) {
    int t = st.back().time;
    if (t >0)
      return max(FinishTime - t, -1);
  }
  else if ( unsigned(controlNumber)<st.size() ) {
    if (controlNumber==0)
      return (tStartTime>0 && st[0].time>0) ? max(st[0].time-tStartTime, -1) : -1;
    else if (st[controlNumber].time>0 && st[controlNumber-1].time>0)
      return max(st[controlNumber].time - st[controlNumber-1].time, -1);
    else return -1;
  }
  return -1;
}

int oRunner::getTimeAdjust(int controlNumber) const
{
  if ( unsigned(controlNumber)<adjustTimes.size() ) {
    return adjustTimes[controlNumber];
  }
  return 0;
}

int oRunner::getNamedSplit(int controlNumber) const {
  pCourse crs=getCourse(true);
  if (!crs || unsigned(controlNumber)>=unsigned(crs->nControls()))
    return -1;

  pControl ctrl=crs->controls[controlNumber];
  if (!ctrl || !ctrl->hasName())
    return -1;

  int k=controlNumber-1;
  int ct = getPunchTime(controlNumber, false, true, false);
  if (ct <= 0)
    return -1;
 
  //Measure from previous named control
  while (k >= 0) {
    pControl c = crs->controls[k];

    if (c && c->hasName()) {
      int dt = getPunchTime(k, false, true, false);
      if (dt > 0 && ct > dt)
        return max(ct - dt, -1);
      else return -1;
    }
    k--;
  }

  //Measure from start time
  return ct;
}

const wstring &oRunner::getSplitTimeS(int controlNumber, bool normalized, SubSecond mode) const
{
  return formatTime(getSplitTime(controlNumber, normalized), mode);
}

const wstring &oRunner::getNamedSplitS(int controlNumber, SubSecond mode) const
{
  return formatTime(getNamedSplit(controlNumber), mode);
}

int oRunner::getPunchTime(int controlIndex, bool normalized, bool adjusted, bool teamTotal) const
{
  int off = teamTotal && tInTeam ? tInTeam->getTotalRunningTimeAtLegStart(getLegNumber(), false) : 0;

  if (!Card) {
    pCourse pc = getCourse(false);
    if (!pc || controlIndex > pc->getNumControls())
      return -1;

    if (controlIndex == pc->getNumControls())
      return getFinishTime() - tStartTime + off;

    int ccId = pc->getCourseControlId(controlIndex);
    pFreePunch fp = oe->getPunch(Id, ccId, getCardNo());
    if (fp)
      return fp->getTimeInt() - tStartTime + off;
    return -1;
  }
  const vector<SplitData> &st = getSplitTimes(normalized);

  if (unsigned(controlIndex) < st.size()) {
    if (st[controlIndex].hasTime())
      return st[controlIndex].getTime(adjusted) - tStartTime + off;
    else return -1;
  }
  else if (unsigned(controlIndex) == st.size())
    return FinishTime - tStartTime + off;

  return -1;
}

const wstring &oRunner::getPunchTimeS(int controlIndex, bool normalized, bool adjusted,
                                      bool teamTotal, SubSecond mode) const {
  return formatTime(getPunchTime(controlIndex, normalized, adjusted, teamTotal), mode);
}

bool oAbstractRunner::isVacant() const {
  int vacClub = oe->getVacantClubIfExist(false);
  return vacClub > 0 && getClubId()==vacClub;
}

bool oRunner::isAnnonumousTeamMember() const {
  wstring anon = lang.tl("N.N.");
  if (getNameRaw() == anon && getExtIdentifier() == 0)
    return true;

  return false;
}

bool oRunner::needNoCard() const {
  const_cast<oRunner*>(this)->apply(ChangeType::Quiet, nullptr);
  return tNeedNoCard;
}

void oRunner::getSplitTime(int courseControlId, RunnerStatus &stat, int &rt) const
{
  rt = 0;
  stat = StatusUnknown;
  int cardno = getCardNo();

  if (courseControlId==oPunch::PunchFinish && FinishTime>0) {
    stat = tStatus;
    rt = getFinishTimeAdjusted(true);
  }
  else if (Card) {
    oPunch *p=Card->getPunchById(courseControlId);
    if (p && p->hasTime()) {
      rt=p->getAdjustedTime();
      stat = StatusOK;
    }
    else if (p && p->punchTime == -1 && statusOK(true, false)) {
      rt = getFinishTimeAdjusted(true);
      if (rt > 0)
        stat = StatusOK;
      else
        stat = StatusMP;
    }
    else
      stat = courseControlId==oPunch::PunchFinish ? StatusDNF: StatusMP;
  }
  else if (cardno) {
    oFreePunch *fp=oe->getPunch(getId(), courseControlId, cardno);

    if (fp) {
      rt=fp->getAdjustedTime();
      stat=StatusOK;
    }
    if (courseControlId==oPunch::PunchFinish && tStatus!=StatusUnknown)
      stat = tStatus;
  }
  rt-=tStartTime;

  if (rt<0)
    rt=0;
}

void oRunner::fillSpeakerObject(int leg, int previousControlCourseIdX, const vector<int>& courseControlIds,
  bool totalResult, oSpeakerObject& spk) const {
  spk.owner = const_cast<oRunner*>(this);
  spk.result.clear();
  spk.timeSinceChange = -1;
  spk.bib = getBib();
  spk.names.push_back(getName());

  spk.club = getClub();
  spk.finishStatus = totalResult ? getTotalStatus() : getStatusComputed(true);

  spk.startTimeS = getStartTimeCompact();
  spk.missingStartTime = tStartTime <= 0;

  spk.priority = 0;

  spk.isRendered = false;

  for (int courseControlId : courseControlIds) {
    spk.result.emplace_back();
    auto& sres = spk.result.back();
    sres.status = StatusUnknown;

    getSplitTime(courseControlId, sres.status, sres.runningTime.time);

    // Compute time since change
    if (courseControlId == oPunch::PunchFinish) {
      if (FinishTime > 0)
        spk.timeSinceChange = oe->getComputerTime() - FinishTime;
    }
    else {
      if (sres.runningTime.time > timeConstSecond * 10)
        spk.timeSinceChange = oe->getComputerTime() - (sres.runningTime.time + tStartTime);
    }

    spk.priority = speakerPriority;
    sres.runningTime.preliminary = getPrelRunningTime();

    if (sres.status == StatusOK) {
      sres.runningTimeLeg = sres.runningTime;
      sres.runningTime.preliminary = sres.runningTime.time;
      sres.runningTimeLeg.preliminary = sres.runningTime.time;
    }
    else {
      sres.runningTimeLeg.time = sres.runningTime.preliminary;
      sres.runningTimeLeg.preliminary = sres.runningTime.preliminary;
    }

    if (totalResult) {
      if (sres.runningTime.preliminary > 0)
        sres.runningTime.preliminary += inputTime;
      if (sres.runningTime.time > 0)
        sres.runningTime.time += inputTime;

      if (inputStatus != StatusOK)
        sres.status = spk.finishStatus;
    }

    // Adjust status for No timic, not competing etc
    if (getStatus() == StatusNoTiming || getStatus() == StatusOutOfCompetition) {
      if (sres.status == StatusOK)
        sres.status = getStatus();
    }
  }
}

pRunner oEvent::findRunner(const wstring &s, int lastId, 
                           const unordered_set<int> &inputFilter,
                           unordered_set<int> &matchFilter) const {
  matchFilter.clear();
  wstring trm = trim(s);
  int len = trm.length();
  int sn = wtoi(trm.c_str());
  wchar_t s_lc[1024];
  wcscpy_s(s_lc, s.c_str());  
  prepareMatchString(s_lc, len);
  int score;
  pRunner res = 0;

  if (!inputFilter.empty() && inputFilter.size() < Runners.size() / 2) {
    for (unordered_set<int>::const_iterator it = inputFilter.begin(); it!= inputFilter.end(); ++it) {
      int id = *it;
      pRunner r = getRunner(id, 0);
      if (!r)
        continue;

      if (sn>0) {
        if (matchNumber(r->StartNo, s_lc) || matchNumber(r->getCardNo(), s_lc)) {
          matchFilter.insert(id);
          if (res == 0)
            res = r;
        }
      }
      else {
        if (filterMatchString(r->tRealName, s_lc, score)) {
          matchFilter.insert(id);
          if (res == 0)
            res = r;
        }
      }
    }
    return res;
  }

  oRunnerList::const_iterator itstart = Runners.begin();

  if (lastId) {
    for (; itstart != Runners.end(); ++itstart) {
      if (itstart->Id==lastId) {
        ++itstart;
        break;
      }
    }
  }

  oRunnerList::const_iterator it;
  for (it=itstart; it != Runners.end(); ++it) {
    pRunner r = pRunner(&(*it));
    if (r->skip())
       continue;

    if (sn>0) {
      if (matchNumber(r->StartNo, s_lc) || matchNumber(r->getCardNo(), s_lc)) {
        matchFilter.insert(r->Id);
        if (res == 0)
          res = r;
      }
    }
    else {
      if (filterMatchString(r->tRealName, s_lc, score)) {
        matchFilter.insert(r->Id);
        if (res == 0)
          res = r;
      }
    }
  }
  for (it=Runners.begin(); it != itstart; ++it) {
    pRunner r = pRunner(&(*it));
    if (r->skip())
       continue;

    if (sn>0) {
      if (matchNumber(r->StartNo, s_lc) || matchNumber(r->getCardNo(), s_lc)) {
        matchFilter.insert(r->Id);
        if (res == 0)
          res = r;
      }
    }
    else {
      if (filterMatchString(r->tRealName, s_lc, score)) {
        matchFilter.insert(r->Id);
        if (res == 0)
          res = r;
      }
    }
  }

  return res;
}

int oRunner::getTimeAfter(int leg, bool allowUpdate) const
{
  if (leg==-1)
    leg=tDuplicateLeg;

  if (!Class || Class->tLeaderTime.size()<=unsigned(leg))
    return -1;

  int t=getRaceRunningTime(true, leg, allowUpdate);

  if (t<=0)
    return -1;

  return t-Class->getTotalLegLeaderTime(oClass::AllowRecompute::Yes, leg, true, true);
}

int oRunner::getTimeAfter() const {
  int leg=0;
  if (tInTeam)
    leg=tLeg;
  else
    leg=tDuplicateLeg;

  if (!Class || Class->tLeaderTime.size()<=unsigned(leg))
    return -1;

  int t=getRunningTime(true);

  if (t<=0)
    return -1;

  return t - Class->getBestLegTime(oClass::AllowRecompute::Yes, leg, true);
}

int oRunner::getTimeAfterCourse(bool considerClass) const {
  if (considerClass && !Class)
    return -1;

  const pCourse crs = getCourse(false);
  if (!crs)
    return -1;

  int t = getRunningTime(true);

  if (t<=0)
    return -1;
  int bt;
  if (considerClass)
    bt = Class->getBestTimeCourse(oClass::AllowRecompute::Yes, crs->getId());
  else
    bt = crs->getBestTime();

  if (bt <= 0)
    return -1;

  return t - bt;
}

bool oRunner::synchronizeAll(bool writeOnly)
{
  if (tParentRunner)
    tParentRunner->synchronizeAll();
  else {
    synchronize(writeOnly);
    for (size_t k=0;k<multiRunner.size();k++) {
      if (multiRunner[k])
        multiRunner[k]->synchronize(writeOnly);
    }
    if (tInTeam)
      tInTeam->synchronize(writeOnly);
  }
  return true;
}

const wstring &oAbstractRunner::getBib() const
{
  return getDCI().getString("Bib");
}

void oRunner::setBib(const wstring &bib, int bibNumerical, bool updateStartNo) {
  if (getBib() == bib)
    return;

  const bool freeBib = !Class || Class->getBibMode() == BibMode::BibFree;

  if (tParentRunner && !freeBib)
    tParentRunner->setBib(bib, bibNumerical, updateStartNo);
  else {
    if (updateStartNo)
      setStartNo(bibNumerical, ChangeType::Update); // Updates multi too.

    if (getDI().setString("Bib", bib)) {
      if (oe)
        oe->bibStartNoToRunnerTeam.clear();
    }
    if (!freeBib) {
      for (size_t k = 0; k < multiRunner.size(); k++) {
        if (multiRunner[k]) {
          multiRunner[k]->getDI().setString("Bib", bib);
        }
      }
    }
  }
}

void oEvent::analyseDNS(vector<int> &unknown_dns, vector<int> &known_dns,
                        vector<int> &known, vector<int> &unknown, bool &hasSetDNS)
{
  autoSynchronizeLists(true);

  vector<pRunner> stUnknown;
  vector<pRunner> stDNS;

  for (oRunnerList::iterator it = Runners.begin(); it!=Runners.end();++it) {
    if (!it->isRemoved() && !it->needNoCard()) {
      if (!it->hasFinished())
        stUnknown.push_back(&*it);
      else if (it->getStatus() == StatusDNS) {
        stDNS.push_back(&*it);
        if (it->hasFlag(oAbstractRunner::FlagAutoDNS))
          hasSetDNS = true;
      }
    }
  }

  // Map cardNo -> punch
  multimap<int, pFreePunch> punchHash;
  map<int, int> cardCount;

  for (oRunnerList::const_iterator it = Runners.begin(); it != Runners.end(); ++it) {
    if (!it->isRemoved() && it->getCardNo() > 0)
      ++cardCount[it->getCardNo()];
  }

  typedef multimap<int, pFreePunch>::const_iterator TPunchIter;
  for (oFreePunchList::iterator it = punches.begin(); it != punches.end(); ++it) {
    if (!it->isRemoved() && !it->isHiredCard())
      punchHash.insert(make_pair(it->getCardNo(), &*it));
  }

  set<int> knownCards;
  for (oCardList::iterator it = Cards.begin(); it!=Cards.end(); ++it) {
    if (it->tOwner == 0)
      knownCards.insert(it->cardNo);
  }

  unknown.clear();
  known.clear();

  for (size_t k=0;k<stUnknown.size();k++) {
    int card = stUnknown[k]->getCardNo();
    if (card == 0)
      unknown.push_back(stUnknown[k]->getId());
    else {
      bool hitCard = knownCards.count(card)==1 && cardCount[card] == 1;
      if (!hitCard) {
        pair<TPunchIter, TPunchIter> res = punchHash.equal_range(card);
        while (res.first != res.second) {
          if (cardCount[card] == 1 || res.first->second->tRunnerId == stUnknown[k]->getId()) {
            hitCard = true;
            break;
          }
          ++res.first;
        }
      }
      if (hitCard)
        known.push_back(stUnknown[k]->getId());
      else
        unknown.push_back(stUnknown[k]->getId()); //These can be given "dns"
    }
  }

  unknown_dns.clear();
  known_dns.clear();

  for (size_t k=0;k<stDNS.size(); k++) {
    int card = stDNS[k]->getCardNo();
    if (card == 0)
      unknown_dns.push_back(stDNS[k]->getId());
    else {
      bool hitCard = knownCards.count(card)==1 && cardCount[card] == 1;
      if (!hitCard) {
        pair<TPunchIter, TPunchIter> res = punchHash.equal_range(card);
        while (res.first != res.second) {
          if (cardCount[card] == 1 || res.first->second->tRunnerId == stDNS[k]->getId()) {
            hitCard = true;
            break;
          }
          ++res.first;
        }
      }
      if (hitCard)
        known_dns.push_back(stDNS[k]->getId());
      else
        unknown_dns.push_back(stDNS[k]->getId());
    }
  }
}

void oRunner::printSplits(gdioutput& gdi) const {
  
  wstring wListId;
  pClass cls1 = getClassRef(true);
  if (cls1)
    wListId = cls1->getDCI().getString("SplitPrint");

  if (wListId.empty()) {
    // Make it possibe to define the list in the base class
    pClass cls2 = getClassRef(false);
    if (cls2 != cls1)
      wListId = cls2->getDCI().getString("SplitPrint");
  }

  if (wListId.empty()) {
    wListId = oe->getDCI().getString("SplitPrint");
  }

  string listId;
  if (wListId.empty()) {
    if (cls1) {
      if (cls1->getClassType() == ClassType::oClassIndividual) {
        if (cls1->isRogaining())
          listId = "Tsplit_result_rogaining";
        else
          listId = "Tsplit_result_individual";
      }
      else if (cls1->getClassType() == ClassType::oClassRelay) {
        if (cls1->isRogaining())
          listId = "Tsplit_result_team_rogaining";
        else
          listId = "Tsplit_result_team";
      }
    }
  }
  else if (wListId == L"*") { // Standarad, no list
  }
  else {
    listId = gdioutput::narrow(wListId);
  }

  const bool wideFormat = oe->getPropertyInt("WideSplitFormat", 0) == 1;
  if (!wideFormat) 
    gdi.setCX(10);

  if (listId.empty()) {
    printSplits(gdi, nullptr);
  }
  else {
    oListParam par;
    par.selection.insert(getClassId(true));
    oListInfo currentList;

    par.listCode = oe->getListContainer().getCodeFromUnqiueId(listId);
    par.showInterTimes = false;
    int legNo = getLegNumber(), legOrd;
    if (Class)
      Class->splitLegNumberParallel(getLegNumber(), legNo, legOrd);
    par.setLegNumberCoded(legNo);
    par.filterMaxPer = 3;
    par.alwaysInclude = this;
    par.showHeader = false;
    par.tightBoundingBox = true;

    try {
      oe->generateListInfo(gdi, par, currentList);
    }
    catch (const meosException&) {
      oe->gdiBase().addInfoBox("load_id_list", L"info:nosplitprint", L"", BoxStyle::Header, 10000);
      printSplits(gdi, nullptr);
      return;
    }

    if (currentList.isSplitPrintList()) {
      auto& sp = *currentList.getSplitPrintInfo();
      currentList.getParam().filterMaxPer = sp.numClassResults;
    }

    if (!wideFormat)
      currentList.shrinkSize();

    printSplits(gdi, &currentList);
  }
}


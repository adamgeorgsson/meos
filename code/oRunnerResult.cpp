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

// oRunnerResult.cpp: oRunner/oEvent runner query and result split from oRunner.cpp
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "oRunner.h"

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
        throw std::exception("Tomt namn inte tillåtet.");

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
        throw std::exception("Starttiden är definerad genom klassen eller löparens startstämpling.");
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
        throw std::exception("För att ändra måltiden måste löparens målstämplingstid ändras.");
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
        throw std::exception("Status matchar inte data i löparbrickan.");
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

static int findNextControl(const vector<pControl> &ctrl, int startIndex, int id, int &offset, bool supportRogaining)
{
  vector<pControl>::const_iterator it=ctrl.begin();
  int index=0;
  offset = 1;
  while(startIndex>0 && it!=ctrl.end()) {
    int multi = (*it)->getNumMulti();
    offset += multi-1;
    ++it, --startIndex, ++index;
    if (it!=ctrl.end() && (*it)->isRogaining(supportRogaining))
      index--;
  }

  while(it!=ctrl.end() && (*it) && (*it)->getId()!=id) {
    int multi = (*it)->getNumMulti();
    offset += multi-1;
    ++it, ++index;
    if (it!=ctrl.end() && (*it)->isRogaining(supportRogaining))
      index--;
  }

  if (it==ctrl.end())
    return -1;
  else
    return index;
}

static void gotoNextLine(gdioutput &gdi, int &xcol, int &cx, int &cy, int colDeltaX, int numCol, int baseCX) {
  if (++xcol < numCol) {
    cx += colDeltaX;
  }
  else {
    xcol = 0;
    cy += int(gdi.getLineHeight()*1.1);
    cx = baseCX;
  }
}

static void addMissingControl(bool wideFormat, gdioutput &gdi, 
                              int &xcol, int &cx, int &cy, 
                              int colDeltaX, int numCol, int baseCX) {
  int xx = cx;
  wstring str = makeDash(L"-");
  int posy = wideFormat ? cy : cy-int(gdi.getLineHeight()*0.4);
  const int endx = cx + colDeltaX - gdi.scaleLength(27/2);

  while (xx < endx) {
    gdi.addStringUT(posy, xx, fontSmall, str);
    xx += gdi.scaleLength(8);
  }

  // Make a thin line for list format, otherwise, take a full place
  if (wideFormat) {
    gotoNextLine(gdi, xcol, cx, cy, colDeltaX, numCol, baseCX);
  }
  else
    cy+=int(gdi.getLineHeight()*0.3);
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

void oRunner::printSplits(gdioutput& gdi, const oListInfo* li) const {
  bool withAnalysis = (oe->getDI().getInt("Analysis") & 1) == 0;
  bool withSpeed = (oe->getDI().getInt("Analysis") & 2) == 0;
  bool withResult = (oe->getDI().getInt("Analysis") & 4) == 0;
  
  bool includeStandardHeading = true;
  bool includeDefaultTitle = true;
  bool includeSplitTimes = true;
  bool includeAbsTime = true;
  bool includeControlCode = true;
  bool includeLegPlace = false;
  bool includeAccLegPlace = false;
  bool includeTimeLoss = false;

  if (li && li->isSplitPrintList()) {
    auto& sp = *li->getSplitPrintInfo();
    includeDefaultTitle = !li->hasHead();
    includeStandardHeading = sp.withStandardHeading;
    withSpeed = sp.withSpeed;
    withResult = sp.withResult;
    withAnalysis = sp.withAnalysis;
    includeSplitTimes = sp.includeSplitTimes;
    includeControlCode = sp.withControlCode;
    includeAbsTime = sp.withAbsTime;
    includeLegPlace = sp.withLegPlace;
    includeAccLegPlace = sp.withAccLegPlace;
    includeTimeLoss = sp.withTimeLoss;
  }
  
  const bool wideFormat = oe->getPropertyInt("WideSplitFormat", 0) == 1;
  const int numCol = 4;

  pClass cls = getClassRef(true);
  if (cls && cls->getNoTiming()) {
    withResult = false;
    withAnalysis = false;
  }

  gdiFonts head = boldText;
  gdiFonts normal = fontSmall;
  gdiFonts bnormal = boldSmall;
  if (wideFormat) {
    head = boldLarge;
    normal = normalText;
    bnormal = boldText;
  }
  
  gdi.fillDown();
  gdi.pushX();
  if (includeDefaultTitle) {
    gdi.addStringUT(head, oe->getName());
    gdi.addStringUT(normal, oe->getDate());
    gdi.dropLine(0.5);
  }
  else {
    oe->formatHeader(gdi, *li, pRunner(this));
    gdi.popX();
  }

  pCourse pc = getCourse(true);
  SubSecond mode = oe->useSubSecond() ? SubSecond::On : SubSecond::Auto;

  if (includeStandardHeading) {
    gdi.addStringUT(bnormal, getName() + L", " + getClass(true));
    gdi.addStringUT(normal, getClub());
    gdi.dropLine(0.5);
    gdi.addStringUT(normal, lang.tl("Start: ") + getStartTimeS() + lang.tl(", Mål: ") + getFinishTimeS(false, mode));
    if (cls && cls->isRogaining()) {
      gdi.addStringUT(normal, lang.tl("Poäng: ") +
        itow(getRogainingPoints(true, false)) +
        +L" (" + lang.tl("Avdrag: ") + itow(getRogainingReduction(true)) + L")");
    }

    wstring statInfo = lang.tl("Status: ") + getStatusS(true, true) + lang.tl(", Tid: ") + getRunningTimeS(true, mode);
    if (withSpeed && pc && pc->getLength() > 0) {
      int kmt = (getRunningTime(false) * 1000) / pc->getLength();
      statInfo += L" (" + formatTime(kmt, SubSecond::Off) + lang.tl(" min/km") + L")";
    }
    gdi.addStringUT(normal, statInfo);
  }

  if (pc && withSpeed) {
    if (pc->legLengths.empty() || *max_element(pc->legLengths.begin(), pc->legLengths.end()) <= 0)
      withSpeed = false; // No leg lenghts available
  }
 
  int cy = gdi.getCY() + gdi.scaleLength(4/2);
  int cx = gdi.getCX();

  int spMax = 0;
  int totMax = 0;
  if (pc) {
    for (int n = 0; n < pc->nControls(); n++) {
      spMax = max(spMax, getSplitTime(n, false));
      totMax = max(totMax, getPunchTime(n, false, false, false));
    }
  }
  bool moreThanHour = max(totMax, getRunningTime(true)) >= timeConstHour;
  bool moreThanHourSplit = spMax >= timeConstHour;

  const int c1 = gdi.scaleLength(17);
  const int spW = moreThanHourSplit ? 32 : 27;
  const int legPlaceW = includeLegPlace ? 18 : 0;
  // Time on leg, right formatted
  const int c2_legtime = c1 + gdi.scaleLength(30 + spW + legPlaceW) - (includeControlCode ? 0 : c1);
  
  // Time after on leg, right formatted
  const int c3_legafter = c2_legtime + (includeTimeLoss ? gdi.scaleLength(34) : 0);

  // Abs time left formatted
  const int c4_abstime = c3_legafter + gdi.scaleLength(5);
  
  const int accLegPlaceW = includeAccLegPlace ? 18 : 0;
  const int absTimeW = includeAbsTime ? 34 : 0;

  // Accumulated leg time, right formatted
  const int c5_accleg = c4_abstime + gdi.scaleLength(absTimeW + accLegPlaceW + (moreThanHour ? 42 : 32));
  
  const int c6_speed = withSpeed ? c5_accleg + gdi.scaleLength(40) : c5_accleg;
  const int baseCX = cx;
  const int colDeltaX = c6_speed + gdi.scaleLength(16);

  char bf[256];
  int lastIndex = -1;
  int adjust = 0;
  int offset = 1;

  vector<pControl> ctrl;

  int finishType = -1;
  int startType = -1, startOffset = 0;
  if (pc) {
    pc->getControls(ctrl);
    finishType = pc->getFinishPunchType();

    if (pc->useFirstAsStart()) {
      startType = pc->getStartPunchType();
      startOffset = -1;
    }
  }

  set<int> headerPos;
  set<int> checkedIndex;

  if (Card && includeSplitTimes) {
    bool hasRogaining = pc ? pc->hasRogaining() : false;

    const int cyHead = cy;
    cy += int(gdi.getLineHeight() * 0.9);
    int xcol = 0;
    int baseY = cy;

    if (pc) {
      oPunchList& p = Card->punches;
      for (oPunchList::iterator it = p.begin(); it != p.end(); ++it) {
        if (headerPos.count(cx) == 0) {
          headerPos.insert(cx);
          gdi.addString("", cyHead, cx, italicSmall, "Kontroll");
          gdi.addString("", cyHead, cx + c2_legtime, italicSmall | textRight, "Tid");
          if (withSpeed)
            gdi.addString("", cyHead, cx + c6_speed, italicSmall | textRight, "min/km");
        }

        bool any = false;
        if (it->tRogainingIndex >= 0) {
          const pControl c = pc->getControl(it->tRogainingIndex);
          string point = c ? itos(c->getRogainingPoints()) + "p." : "";

          gdi.addStringUT(cy, cx + c1 + gdi.scaleLength(10 / 2), fontSmall, point);
          any = true;

          snprintf(bf, sizeof(bf), "%d", it->type);
          gdi.addStringUT(cy, cx, fontSmall, bf);
          int st = Card->getSplitTime(getStartTime(), &*it);

          if (st > 0) 
            gdi.addStringUT(cy, cx + c2_legtime, fontSmall | textRight, formatTime(st, SubSecond::Off));
          
          if (includeAbsTime)
            gdi.addStringUT(cy, cx + c4_abstime, fontSmall, it->getTime(false, SubSecond::Off));

          int pt = it->getAdjustedTime();
          st = getStartTime();
          if (st > 0 && pt > 0 && pt > st) {
            wstring punchTime = formatTime(pt - st, SubSecond::Off);
            gdi.addStringUT(cy, cx + c5_accleg, fontSmall | textRight, punchTime);
          }

          cy += int(gdi.getLineHeight() * 0.9);
          continue;
        }

        int cid = it->tMatchControlId;
        wstring accLegTime;
        int sp;
        int controlLegIndex = -1;
        if (it->isFinish(finishType)) {
          // Check if the last normal control was missing, and indicate this
          for (int j = pc->getNumControls() - 1; j >= 0; j--) {
            pControl ctrl = pc->getControl(j);
            if (ctrl && ctrl->isSingleStatusOK()) {
              if (checkedIndex.count(j) == 0) {
                addMissingControl(wideFormat, gdi, xcol, cx, cy, colDeltaX, numCol, baseCX);
              }
              break;
            }
          }

          gdi.addString("", cy, cx, fontSmall, "Mål");
          sp = getSplitTime(splitTimes.size(), false);

          if (sp > 0) {
            wstring legTime = formatTime(sp, SubSecond::Off);
            if (includeLegPlace) {
              int pl = getLegPlace(splitTimes.size());
              if (pl > 0) {
                legTime += L" (" + itow(pl) + L".)";
              }
            }
            gdi.addStringUT(cy, cx + c2_legtime, fontSmall | textRight, legTime);
            accLegTime = formatTime(getRunningTime(true), SubSecond::Off);

            if (includeAccLegPlace) {
              int pl = getLegPlaceAcc(splitTimes.size(), tLeg > 0);
              if (pl > 0) {
                accLegTime += L" (" + itow(pl) + L".)";
              }
            }
          }
          if (includeAbsTime)
            gdi.addStringUT(cy, cx + c4_abstime, fontSmall, oe->getAbsTime(it->getTimeInt() + adjust, SubSecond::Off));
          any = true;
          if (sp > 0) {
            gdi.addStringUT(cy, cx + c5_accleg, fontSmall | textRight, accLegTime);
          }
          controlLegIndex = pc->getNumControls();
        }
        else if (it->type > 10) { //Filter away check and start
          int index = -1;
          bool isExtraPunch = false; // Control not in course
          if (cid > 0)
            index = findNextControl(ctrl, lastIndex + 1, cid, offset, hasRogaining);
          if (index >= 0) {
            if (index > lastIndex + 1) {
              addMissingControl(wideFormat, gdi, xcol, cx, cy, colDeltaX, numCol, baseCX);
            }
            lastIndex = index;

            if (it->type == startType && (index + offset) == 1)
              continue; // Skip start control

            snprintf(bf, sizeof(bf), "%d.", index + offset + startOffset);
            gdi.addStringUT(cy, cx, fontSmall, bf);
            
            if (includeControlCode) {
              snprintf(bf, sizeof(bf), "(%d)", it->type);
              gdi.addStringUT(cy, cx + c1, fontSmall, bf);
            }

            controlLegIndex = it->tIndex;
            checkedIndex.insert(controlLegIndex);
            adjust = getTimeAdjust(controlLegIndex);
            sp = getSplitTime(controlLegIndex, false);
            if (sp > 0) {
              accLegTime = getPunchTimeS(controlLegIndex, false, false, false, SubSecond::Off);
              wstring legTime = formatTime(sp, SubSecond::Off);

              if (includeAccLegPlace) {
                int pl = getLegPlaceAcc(controlLegIndex, tLeg > 0);
                if (pl > 0) {
                  accLegTime += L" (" + itow(pl) + L".)";
                }
              }

              if (includeLegPlace) {
                int pl = getLegPlace(controlLegIndex);
                if (pl > 0) {
                  legTime += L" (" + itow(pl) + L".)";
                }
              }
              
              gdi.addStringUT(cy, cx + c2_legtime, fontSmall | textRight, legTime);

              if (includeTimeLoss) {
                int loss = getLegTimeAfter(controlLegIndex);
                if (loss != 0) {
                  wstring lossS = L"+" + formatTime(loss, SubSecond::Off);
                  gdi.addStringUT(cy, cx + c3_legafter, fontSmall | textRight, lossS);
                }
              }
            }
          }
          else {
            isExtraPunch = true;
            if (!it->isUsed && includeControlCode) {
              gdi.addStringUT(cy, cx, fontSmall, makeDash(L"-"));
            }
            snprintf(bf, sizeof(bf), "(%d)", it->type);
            if (includeControlCode)
              gdi.addStringUT(cy, cx + c1, fontSmall, bf);
            else
              gdi.addStringUT(cy, cx, fontSmall, bf);

          }

          if (includeAbsTime || isExtraPunch) {
            if (it->punchTime > 0)
              gdi.addStringUT(cy, cx + c4_abstime, fontSmall, oe->getAbsTime(it->getAdjustedTime() + adjust, SubSecond::Off));
            else {
              wstring str = makeDash(L"-");
              gdi.addStringUT(cy, cx + c4_abstime, fontSmall, str);
            }
          }

          if (!accLegTime.empty()) {
            gdi.addStringUT(cy, cx + c5_accleg, fontSmall | textRight, accLegTime);
          }
          any = true;
        }

        if (withSpeed && controlLegIndex >= 0 && size_t(controlLegIndex) < pc->legLengths.size()) {
          int length = pc->legLengths[controlLegIndex];
          if (length > 0 && sp > 0) {
            int tempo = (sp * 1000) / length;
            gdi.addStringUT(cy, cx + c6_speed, fontSmall | textRight, formatTime(tempo, SubSecond::Off));
          }
        }

        if (any) {
          if (!wideFormat) {
            cy += int(gdi.getLineHeight() * 0.9);
          }
          else {
            gotoNextLine(gdi, xcol, cx, cy, colDeltaX, numCol, baseCX);
          }
        }
      }
      gdi.dropLine();
      if (wideFormat) {
        for (int i = 0; i < numCol - 1; i++) {
          RECT rc;
          rc.top = baseY;
          rc.bottom = cy;
          rc.left = baseCX + colDeltaX * (i + 1) - 10;
          rc.right = rc.left + 1;
          gdi.addRectangle(rc, colorBlack);
        }
      }

      if (withAnalysis) {
        vector<wstring> misses;
        if (cls && cls->isRogaining()) {
          int cnt = 0;
          for (auto &p : Card->punches) {
            const oControl *ctrl = p.getRogainingControl(*pc);
            if (p.isFinish(pc->getFinishId()) || ctrl) {
              int missed = getMissedTime(cnt);
              if (missed > 0) {
                wstring n = ctrl ? itow(ctrl->getFirstNumber()) : lang.tl("Mål").substr(0, 1);
                misses.push_back(n + L"/" + formatTime(missed, SubSecond::Off));
              }
              cnt++;
            }

          }
        }
        else {
          int last = ctrl.size();
          if (pc->useLastAsFinish())
            last--;

          for (int k = pc->useFirstAsStart() ? 1 : 0; k < last; k++) {
            int missed = getMissedTime(k);
            if (missed > 0) {
              misses.push_back(pc->getControlOrdinal(k) + L"/" + formatTime(missed, SubSecond::Off));
            }
          }
        }
        if (misses.size() == 0) {
          vector<pRunner> rOut;
          oe->getRunners(0, pc->getId(), rOut, false);
          int count = 0;
          for (size_t k = 0; k < rOut.size(); k++) {
            if (rOut[k]->getCard())
              count++;
          }

          if (count < 3)
            gdi.addString("", normal, "Underlag saknas för bomanalys.");
          else
            gdi.addString("", normal, "Inga bommar registrerade.");
        }
        else {
          wstring out = lang.tl("Tidsförluster (kontroll-tid): ");
          for (size_t k = 0; k < misses.size(); k++) {
            if (out.length() > (wideFormat ? 80u : (withSpeed ? 40u : 35u))) {
              gdi.addStringUT(normal, out);
              out.clear();
            }
            out += misses[k];
            if (k < misses.size() - 1)
              out += L", ";
            else
              out += L".";
          }
          gdi.addStringUT(fontSmall, out);
        }
      }
    }
    else {
      int index = 0;
      int lastTime = 0;

      for (auto& it : Card->punches) {
        if (headerPos.count(cx) == 0) {
          headerPos.insert(cx);
          gdi.addString("", cyHead, cx, italicSmall, "Kontroll");
          gdi.addString("", cyHead, cx + c2_legtime - gdi.scaleLength(55 / 2), italicSmall, "Tid");
        }

        bool any = false;
        wstring punchTime;
        if (it.isFinish(finishType)) {
          gdi.addString("", cy, cx, fontSmall, "Mål");
          int rt = it.getTimeInt() - tStartTime;
          if (rt > 0) {
            gdi.addStringUT(cy, cx + c2_legtime, fontSmall | textRight, formatTime(rt - lastTime, SubSecond::Off));
            punchTime = formatTime(getRunningTime(true), SubSecond::Off);
          }
          gdi.addStringUT(cy, cx + c4_abstime, fontSmall, oe->getAbsTime(it.getTimeInt(), SubSecond::Off));
          any = true;
          if (!punchTime.empty()) {
            gdi.addStringUT(cy, cx + c5_accleg, fontSmall | textRight, punchTime);
          }
        }
        else if (it.type > 10 && it.type != startType) { //Filter away check and start
          snprintf(bf, sizeof(bf), "%d.", ++index);
          gdi.addStringUT(cy, cx, fontSmall, bf);
          snprintf(bf, sizeof(bf), "(%d)", it.type);
          gdi.addStringUT(cy, cx + c1, fontSmall, bf);

          if (it.hasTime()) {
            int rt = it.getTimeInt() - tStartTime;
            punchTime = formatTime(rt, SubSecond::Off);
            gdi.addStringUT(cy, cx + c2_legtime, fontSmall | textRight, formatTime(rt - lastTime, SubSecond::Off));
            lastTime = rt;
          }

          if (it.hasTime())
            gdi.addStringUT(cy, cx + c4_abstime, fontSmall, oe->getAbsTime(it.getTimeInt(), SubSecond::Off));
          else {
            wstring str = makeDash(L"-");
            gdi.addStringUT(cy, cx + c4_abstime, fontSmall, str);
          }

          if (!punchTime.empty()) {
            gdi.addStringUT(cy, cx + c5_accleg, fontSmall | textRight, punchTime);
          }
          any = true;
        }

        if (any) {
          if (!wideFormat) {
            cy += int(gdi.getLineHeight() * 0.9);
          }
          else {
            gotoNextLine(gdi, xcol, cx, cy, colDeltaX, numCol, baseCX);
          }
        }
      }
    }

  }

  if (getStatus() != StatusUnknown && getFinishTime() > 0) {

    oe->calculateResults({ getClassId(true) }, oEvent::ResultType::ClassResult);
    if (hasInputData())
      oe->calculateResults({ getClassId(true) }, oEvent::ResultType::TotalResult);
    if (tInTeam)
      oe->calculateTeamResults(std::set<int>({ getClassId(true) }), oEvent::ResultType::ClassResult);

    if (withResult && statusOK(true, true)) {
      gdi.dropLine(0.5);
      wstring place = oe->formatListString(lRunnerGeneralPlace, pRunner(this), L"%s");
      wstring timestatus;
      if (tInTeam || hasInputData()) {
        timestatus = oe->formatListString(lRunnerGeneralTimeStatus, pRunner(this));
        if (!place.empty() && !timestatus.empty())
          timestatus = L", " + timestatus;
      }

      wstring after = oe->formatListString(lRunnerGeneralTimeAfter, pRunner(this));
      if (!after.empty() && !(place.empty() && timestatus.empty()))
        after = L", " + after;

      gdi.fillRight();
      gdi.pushX();
      if (!place.empty())
        gdi.addString("", bnormal, "Placering:");
      else
        gdi.addString("", bnormal, "Resultat:");
      gdi.fillDown();
      gdi.addString("", normal, place + timestatus + after);
      gdi.popX();
    }
  }

  if (Card && Card->miliVolt > 0) {
    gdi.dropLine(0.7);
    auto stat = Card->isCriticalCardVoltage();
    wstring warning;
    if (stat == oCard::BatteryStatus::Bad)
      warning = lang.tl("Replace[battery]");
    else if (stat == oCard::BatteryStatus::Warning)
      warning = lang.tl("Low");
    else
      warning = lang.tl("OK");
    gdi.fillRight();
    gdi.pushX();
    gdi.addString("", fontSmall, L"Batteristatus:");
    gdi.addStringUT(boldSmall, getCard()->getCardVoltage());
    gdi.fillDown();
    gdi.addStringUT(fontSmall, L"(" + warning + L")");
    gdi.popX();
  }

  if (li && !li->empty(false)) {
    oe->generateList(gdi, false, *li, false);
    gdi.dropLine();
  }
  else {
    gdi.dropLine(0.7);
  }

  vector< pair<wstring, int> > lines;
  oe->getExtraLines("SPExtra", lines);
  for (size_t k = 0; k < lines.size(); k++) {
    gdi.addStringUT(lines[k].second, formatExtraLine(pRunner(this), lines[k].first));
  }
  if (lines.size()>0)
    gdi.dropLine(0.5);

  gdi.addString("", fontSmall, "Av MeOS: www.melin.nu/meos");
}

void oRunner::printStartInfo(gdioutput &gdi, bool includeEconomy) const {
  gdi.setCX(10);
  gdi.fillDown();
  gdi.addString("", boldText, L"Startbevis X#" + oe->getName());
  gdi.addStringUT(fontSmall, oe->getDate());
  gdi.dropLine(0.5);

  wstring bib = getBib();
  if (!bib.empty())
    bib = bib + L": ";

  gdi.addStringUT(boldSmall, bib + getName() + L", " + getClass(true));
  gdi.addStringUT(fontSmall, getClub());
  gdi.dropLine(0.5);
  
  wstring startName;
  if (getCourse(false)) {
    startName = trim(getCourse(false)->getStart());
    if (!startName.empty())
      startName = L" (" + startName + L")";
  }    
  if (getStartTime() > 0)
    gdi.addStringUT(fontSmall, lang.tl(L"Start: ") + getStartTimeS() + startName);
  else
    gdi.addStringUT(fontSmall, lang.tl(L"Fri starttid") + startName);

  wstring borrowed = isRentalCard() ? L" (" + lang.tl(L"Hyrd") + L")" : L"";
      
  gdi.addStringUT(fontSmall, lang.tl(L"Bricka: ") + itow(getCardNo()) +  borrowed);
  
  int cardFee = getRentalCardFee(true);
  if (cardFee < 0)
    cardFee = 0;

  if (includeEconomy) {
    int fee = oe->getMeOSFeatures().hasFeature(MeOSFeatures::Economy) ? getDCI().getInt("Fee") + cardFee : 0;

    if (fee > 0) {
      wstring info;
      if (getDCI().getInt("Paid") == fee)
        info = lang.tl("Betalat");
      else
        info = lang.tl("Faktureras");

      gdi.addStringUT(fontSmall, lang.tl("Anmälningsavgift: ") + itow(fee) + L" (" + info + L")");
    }
  }

  vector< pair<wstring, int> > lines;
  oe->getExtraLines("EntryExtra", lines);

  if (!lines.empty())
    gdi.dropLine(0.5);

  for (size_t k = 0; k < lines.size(); k++) {
    gdi.addStringUT(lines[k].second, formatExtraLine(pRunner(this), lines[k].first));
  }
//  if (lines.size()>0)
//    gdi.dropLine(0.5);

//  gdi.addStringUT(fontSmall, L"Av MeOS " + getMeosCompectVersion() + L" / www.melin.nu/meos");
}


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
    GNU General Public License fro more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Melin Software HB - software@melin.nu - www.melin.nu
    Eksoppsvägen 16, SE-75646 UPPSALA, Sweden

************************************************************************/


#include "StdAfx.h"

#include <vector>
#include <set>
#include <cassert>
#include <algorithm>
#include <limits>

#include "oEvent.h"
#include "gdioutput.h"
#include "gdifonts.h"
#include "oDataContainer.h"
#include "metalist.h"
#include "cardsystem.h"

#include "SportIdent.h"

#include "meosexception.h"
#include "oFreeImport.h"
#include "TabBase.h"
#include "meos_util.h"
#include "RunnerDB.h"
#include "localizer.h"
#include "progress.h"
#include "intkeymapimpl.hpp"
#include "socket.h"

#include "machinecontainer.h"
#include "MeOSFeatures.h"
#include "generalresult.h"
#include "oEventDraw.h"
#include "MeosSQL.h"
#include "binencoder.h"
#include "image.h"
#include "datadefiners.h"
#include "maprenderer.h"
#include "xmlparser.h"

#include <chrono>
#include <random>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "Table.h"
#include <cstdint>
#include <iostream>
#include <ctime>
#include <filesystem>
#include <fstream>

namespace {
  void getNewFileName(wstring &fn, wstring &nameId) {
    std::tm st = {};
    meos_localtime_now(&st);

    wchar_t file[260];
    wchar_t filename[64];
    swprintf(filename, 64, L"meos_%d%02d%02d_%02d%02d%02d_%X.meos",
               (st.tm_year + 1900), (st.tm_mon + 1), st.tm_mday, st.tm_hour, st.tm_min, st.tm_sec, 0 /* TODO: std::tm has no milliseconds */);

    getUserFile(file, filename);

    wchar_t CurrentNameId[64];
    {
      wstring stemw = std::filesystem::path(file).stem().wstring();
      wcsncpy_s(CurrentNameId, 64, stemw.c_str(), 63);
    }

    fn = file;
    nameId = CurrentNameId;
  }
}

extern Image image;

//Version of database

void oEvent::removeRunner(const vector<int> &ids)
{
  cardToRunnerHash.reset();
  classIdToRunnerHash.reset();
  oRunnerList::iterator it;

  set<int> toRemove;
  for (size_t k = 0; k < ids.size(); k++) {
    int Id = ids[k];
    pRunner r=getRunner(Id, 0);

    if (r==0)
      continue;
    
    if (r->tInTeam) // XXX
      r = r->tParentRunner ? r->tParentRunner : r;
    else if (r->tParentRunner) {      
      r->tParentRunner->createMultiRunner(true, true);
      r = getRunner(Id, 0);
      if (r == nullptr)
        continue;
      else {
        auto &mlr = r->tParentRunner->multiRunner;
        mlr.erase(std::remove(mlr.begin(), mlr.end(), r), mlr.end());
      }
    }
    if (toRemove.count(r->getId()))
      continue; //Already found.

    //Remove a singe runner team
    for (size_t k = 0; k < r->multiRunner.size(); k++) {
      if (r->multiRunner[k])
        toRemove.insert(r->multiRunner[k]->getId());
    }
    autoRemoveTeam(r);
    toRemove.insert(r->Id);
  }

  if (toRemove.empty())
    return;

  dataRevision++;
  set<pClass> affectedCls;
  for (it=Runners.begin(); it != Runners.end();){
    oRunner &cr = *it;
    if (toRemove.count(cr.getId())> 0) {
      if (cr.Class)
        affectedCls.insert(cr.Class);
      if (hasDBConnection())
        sqlRemove(&cr);
      toRemove.erase(cr.getId());
      runnerById.erase(cr.getId());
      if (cr.Card) {
        assert( cr.Card->tOwner == &cr );
        cr.Card->tOwner = nullptr;
      }
      // Reset team runner (this should not happen)
      if (it->tInTeam) {
        if (it->tInTeam->Runners[it->tLeg]==&*it)
          it->tInTeam->Runners[it->tLeg] = nullptr;
      }

      oRunnerList::iterator next = it;
      ++next;

      Runners.erase(it);
      if (toRemove.empty()) {
        break;
      }
      else
      it = next;
    }
    else
      ++it;
  }

  for (set<pClass>::iterator it = affectedCls.begin(); it != affectedCls.end(); ++it) {
    (*it)->clearCache(true);
    (*it)->markSQLChanged(-1,-1);
  }

  oe->updateTabs();
}

void oEvent::removeCourse(int Id)
{
  oCourseList::iterator it;

  for (it=Courses.begin(); it != Courses.end(); ++it){
    if (it->Id==Id){
      if (hasDBConnection())
        sqlRemove(&*it);
      dataRevision++;
      Courses.erase(it);
      courseIdIndex.erase(Id);
      return;
    }
  }
}

void oEvent::removeClass(int Id)
{
  oClassList::iterator it;
  vector<int> subRemove;
  for (it = Classes.begin(); it != Classes.end(); ++it){
    if (it->Id==Id){
      if (it->getQualificationFinal()) {
        for (int n = 0; n < it->getNumQualificationFinalClasses(); n++) {
          const oClass *pc = it->getVirtualClass(n);
          if (pc && pc != &*it)
            subRemove.push_back(pc->getId());
        }
      }
      if (hasDBConnection())
        sqlRemove(&*it);
      Classes.erase(it);
      dataRevision++;
      updateTabs();
      break;
    }
  }
  for (int id : subRemove) {
    removeClass(id);
  }
}

void oEvent::removeControl(int Id)
{
  oControlList::iterator it;

  for (it=Controls.begin(); it != Controls.end(); ++it){
    if (it->Id==Id){
      if (hasDBConnection())
        sqlRemove(&*it);
      Controls.erase(it);
      dataRevision++;
      return;
    }
  }
}

void oEvent::removeClub(int Id)
{
  oClubList::iterator it;

  for (it=Clubs.begin(); it != Clubs.end(); ++it){
    if (it->Id==Id) {
      if (hasDBConnection())
        sqlRemove(&*it);
      Clubs.erase(it);
      clubIdIndex.erase(Id);
      dataRevision++;
      return;
    }
  }
  if (vacantId == Id)
    vacantId = 0; // Clear vacant id

  if (noClubId == Id)
    noClubId = 0;
}

void oEvent::removeCard(int Id) {
  for (auto it = Cards.begin(); it != Cards.end(); ++it) {
    if (it->getOwner() == 0 && it->Id == Id) {
      if (it->tOwner) {
        if (it->tOwner->Card == &*it)
          it->tOwner->Card = nullptr;
      }
      if (hasDBConnection())
        sqlRemove(&*it);
      Cards.erase(it);
      dataRevision++;
      return;
    }
  }
}

bool oEvent::isCourseUsed(int Id) const
{
  oClassList::const_iterator it;

  for (it=Classes.begin(); it != Classes.end(); ++it){
    if (it->isCourseUsed(Id))
      return true;
  }

  oRunnerList::const_iterator rit;

  for (rit=Runners.begin(); rit != Runners.end(); ++rit){
    pCourse pc=rit->getCourse(false);
    if (pc && pc->Id==Id)
      return true;
  }
  return false;
}

bool oEvent::isClassUsed(int Id) const
{
  pClass cl = getClass(Id);
  if (cl && cl->parentClass) {
    if (isClassUsed(cl->parentClass->Id))
      return true;
  }

  set<int> idToCheck;
  idToCheck.insert(Id);
  if (cl) {
    for (int i = 0; i < cl->getNumQualificationFinalClasses(); i++)
      idToCheck.insert(cl->getVirtualClass(i)->getId());
  }
  //Search runners
  for (auto it=Runners.begin(); it != Runners.end(); ++it){
    if (it->isRemoved())
      continue;
    if (idToCheck.count(it->getClassId(false)))
      return true;
  }

  //Search teams
  for (auto tit=Teams.begin(); tit != Teams.end(); ++tit){
    if (tit->isRemoved())
      continue;
    if (idToCheck.count(tit->getClassId(false)))
      return true;
  }
  return false;
}

bool oEvent::isClubUsed(int Id) const
{
  //Search runners
  oRunnerList::const_iterator it;
  for (it=Runners.begin(); it != Runners.end(); ++it){
    if (it->getClubId()==Id)
      return true;
  }

  //Search teams
  oTeamList::const_iterator tit;
  for (tit=Teams.begin(); tit != Teams.end(); ++tit){
    if (tit->getClubId()==Id)
      return true;
  }

  return false;
}

bool oEvent::isRunnerUsed(int Id) const
{
  //Search teams
  oTeamList::const_iterator tit;
  for (tit=Teams.begin(); tit != Teams.end(); ++tit){
    if (tit->isRunnerUsed(Id)) {
      if (tit->Class && tit->Class->isSingleRunnerMultiStage())
        //Don't report single-runner-teams as blocking
        continue;
      return true;
    }
  }

  return false;
}

bool oEvent::isControlUsed(int Id) const {
  for (auto& crs : Courses) {
    if (crs.isRemoved())
      continue;
    for (pControl ctrl : crs.controls) {
      if (ctrl && ctrl->Id == Id)
        return true;
    }

    if (crs.finish && crs.finish->Id == Id)
      return true;

    if (crs.start && crs.start->Id == Id)
      return true;
  }
  return false;
}

bool oEvent::classHasResults(int Id) const {
  oRunnerList::const_iterator it;

  for (it=Runners.begin(); it != Runners.end(); ++it) {
    if (it->isRemoved())
      continue;
    if ( (Id == 0 || it->getClassId(true) == Id) && (it->getCard() || it->FinishTime))
      return true;
  }

  return false;
}

bool oEvent::classHasTeams(int Id) const
{
  pClass pc = oe->getClass(Id);
  if (pc == 0)
    return false;

  if (pc->getQualificationFinal() != 0)
    return false;

  oTeamList::const_iterator it;
  for (it=Teams.begin(); it != Teams.end(); ++it)
    if (!it->isRemoved() && it->getClassId(false)==Id)
      return true;

  return false;
}

void oEvent::generateVacancyList(gdioutput &gdi, GUICALLBACK cb)
{
  sortRunners(ClassStartTime);
  oRunnerList::iterator it;

  // BIB, START, NAME, CLUB, SI
  int dx[5]={0, 0, gdi.scaleLength(70), gdi.scaleLength(150)};

  bool withbib=hasBib(true, false);
  int i;

  const int bibLen = gdi.scaleLength(40);
  if (withbib) for (i = 1; i < 4; i++) dx[i] += bibLen;

  int y=gdi.getCY();
  int x=gdi.getCX();
  int lh=gdi.getLineHeight();

  const int yStart = y;
  int nVac = 0;
  for (it=Runners.begin(); it != Runners.end(); ++it) {
    if (it->skip() || !it->isVacant())
      continue;
    nVac++;
  }

  int nCol = 1 + min(3, nVac/10);
  int RunnersPerCol = nVac / nCol;

  char bf[256];
  int nRunner = 0;
  y+=lh;

  int Id=0;
  for(it=Runners.begin(); it != Runners.end(); ++it){
    if (it->skip() || !it->isVacant())
      continue;

    if (it->getClassId(true) != Id) {
      Id=it->getClassId(true);
      y+=lh/2;

      if (nRunner>=RunnersPerCol) {
        y = yStart;
        x += dx[3]+gdi.scaleLength(5);
        nRunner = 0;
      }


      gdi.addStringUT(y, x+dx[0], 1, it->getClass(true));
      y+=lh+lh/3;
    }

    oDataInterface DI=it->getDI();

    if (withbib) {
      wstring bib=it->getBib();

      if (!bib.empty()) {
        gdi.addStringUT(y, x+dx[0], 0, bib);
      }
    }
    gdi.addStringUT(y, x+dx[1], 0, it->getStartTimeS(), 0,  cb).setExtra(it->getId());

    _itoa_s(it->Id, bf, 256, 10);
    gdi.addStringUT(y, x+dx[2], 0, it->getName(), dx[3]-dx[2]-4, cb).setExtra(it->getId());
    //gdi.addStringUT(y, x+dx[3], 0, it->getClub());

    y+=lh;
    nRunner++;
  }
  if (nVac==0)
    gdi.addString("", y, x, 0, "Inga vakanser tillgängliga. Vakanser skapas vanligen vid lottning.");
  gdi.updateScrollbars();
}

void oEvent::generateInForestList(gdioutput& gdi, GUICALLBACK cb, GUICALLBACK cb_nostart_unused) {
  //Lazy setup: tie runners and persons
  oFreePunch::rehashPunches(*oe, 0, 0);
  updateComputerTime(false);
  calcUseStartSeconds();
  const int ct = getComputerTime();

  DWORD filter = 0;
  bool hasFilter = gdi.getData("FilterSetting", filter);
  static DWORD lastFilter = 0;

  int y = gdi.getCY();
  int x = gdi.getCX();
  int lh = gdi.getLineHeight();

  if (!hasFilter) {
    gdi.addStringUT(2, lang.tl(L"Kvar-i-skogen") + makeDash(L" - ") + getName());
    y += lh / 2;

    gdi.addStringUT(1, getDate());

    gdi.dropLine();
    gdi.pushX();
    gdi.fillRight();
    vector<pair<wstring, size_t>> options;
    int mt = getMaximalTime();

    options.emplace_back(lang.tl("Alla"), 0);
    if (mt > 0)
      options.emplace_back(lang.tl("Maxtid[setting]"), mt);

    if (mt != timeConstHour)
      options.emplace_back(lang.tl("En timme"), timeConstHour);

    if (mt != (2 * timeConstHour))
      options.emplace_back(lang.tl("Två timmar"), 2*timeConstHour);

    class RefreshFilter final : public GuiHandler {
      oEvent& oe;
      GUICALLBACK cb;
    public: 
      RefreshFilter(oEvent& oe, GUICALLBACK cb) : oe(oe), cb(cb) {}

      void handle(gdioutput& gdi, BaseInfo& info, GuiEventType type) final {
        if (type == GuiEventType::GUI_LISTBOX) {
          ListBoxInfo lbi = dynamic_cast<ListBoxInfo &>(info);
          gdi.setData("FilterSetting", lbi.data);
          lastFilter = DWORD(lbi.data);
          oe.generateInForestList(gdi, cb, nullptr);
          if (oe.cbBaseButtons) oe.cbBaseButtons(gdi, 1, false);
          gdi.refreshFast();
        }
      }
    };

    gdi.addString("", 0, "Filter:");
    shared_ptr<GuiHandler> h = make_shared<RefreshFilter>(*this, cb);
    gdi.dropLine(-0.1);
    gdi.addSelection("FilterT", 100, 200).setHandler(h);
    gdi.setItems("FilterT", options);
    if (gdi.selectItemByData("FilterT", lastFilter))
      filter = lastFilter;
    else
      gdi.selectFirstItem("FilterT");

    gdi.setData("FilterSetting", filter);
    gdi.popX();
    gdi.fillDown();
    gdi.dropLine(2.5);
  }
  else {
    gdi.restoreNoUpdate("UpdateFilter");
  }

  gdi.setRestorePoint("UpdateFilter");
  
  int id = 0;
  int nr = 0;

  // Get a set with unknown runner id:s
  set<int> statUnknown;
  for (auto& itr : Runners) {
    if (!itr.hasFinished() && !itr.skip() && !itr.needNoCard()) {
      if (itr.tInTeam && itr.tLeg > 0 && !itr.startTimeAvailable() && itr.tInTeam->startTimeAvailable()) {
        continue;  // In a team and has not started yet. Skip
      }

      statUnknown.insert(itr.getId());
    }
  }

  gdi.addString("", fontMediumPlus, "Totalt antal: X#" + itos(statUnknown.size()));
  y = gdi.getCY() + lh;
  bool hasCheck = false;
  bool hasEntry = false;
  int withinTime = 0;

  int tnr = 0;
  id = 0;
  nr = 0;
  sortRunners(ClassStartTime);

  const int dx[5] = { 0, 
                      gdi.scaleLength(70), 
                      gdi.scaleLength(350), 
                      gdi.scaleLength(410),
                      gdi.scaleLength(540) };
  
  y = gdi.getCY();
  vector<pRunner> rr;
  vector<pFreePunch> runnerPunches;
  set<int> reported;

  for (auto it = Runners.begin(); it != Runners.end(); ++it) {
    if (it->skip() || it->needNoCard() || it->hasFinished())
      continue;
    if (!statUnknown.count(it->Id))
      continue;

    getPunchesForRunner(it->getId(), true, runnerPunches);
    wstring punches;
    wstring lastPunch;
    wstring otherRunners;
    wstring checkTime;
    for (auto rp : runnerPunches) {
      if (!punches.empty())
        punches.append(L", ");
      punches.append(rp->getSimpleString());
      if (rp->isCheck())
        checkTime = rp->getTime(false, SubSecond::Off);
    }

    getRunnersByCardNo(it->getCardNo(), false, CardLookupProperty::SkipNoStart, rr);
    for (size_t k = 0; k < rr.size(); k++) {
      if (!rr[k]->skip() && rr[k]->getId() != it->getId()) {
        if (otherRunners.empty()) {
          otherRunners = lang.tl("Bricka X används också av: #" + itos(it->getCardNo()));
        }
        else {
          otherRunners += L", ";
        }
        otherRunners += rr[k]->getName();
      }
    }

    int startTime = NOTIME;
    wstring timerTemplate;
    wstring sStart, sTooltip;

    if (it->startTimeAvailable()) {
      //gdi.addStringUT(y, x + dx[0], 0, it->getStartTimeCompact());
      sStart = it->getStartTimeCompact();
      startTime = it->getStartTime();
    }
    else if (!checkTime.empty()) {
      timerTemplate = L"@X\u00b9";
      if (!useStartSeconds()) {
        int pos = checkTime.find_last_of(':');
        sStart = checkTime.substr(0, pos) + L"\u00b9";
      }
      else {
        sStart = checkTime;
      }

      sTooltip = L"Checktid";
      hasCheck = true;
      startTime = oe->getRelativeTime(checkTime);
    }
    else if (it->getEntryDate() == getLocalDate() || it->getEntryDate() == oe->getDate()) {
      int entryTime = it->getDCI().getInt("EntryTime");
      if (entryTime > 0) {
        timerTemplate = L"@X\u00b2";
        wstring entryTimeS = formatTimeHMS(entryTime, SubSecond::Off).substr(0, 5);
        sStart = entryTimeS + L"\u00b2";
        sTooltip = L"Anmälningstid";
        startTime = oe->getRelativeTime(entryTimeS);
        hasEntry = true;
        if (startTime > timeConstHour * 23)
          startTime -= timeConstHour * 24;
      }
    }

    if (filter != 0 && startTime != NOTIME && int(ct - startTime) < int(filter)) {
      withinTime++;
      continue;
    }

    reported.insert(it->Id);

    if (id != it->getClassId(true)) {
      if (nr > 0) {
        gdi.addString("", y, x, 0, "Antal: X#" + itos(nr));
        y += lh;
        nr = 0;
      }
      y += lh;
      id = it->getClassId(true);
      gdi.addStringUT(y, x, 1, it->getClass(true));

      gdi.addString("", y, x + dx[2], 1 | textRight, "Tid", dx[3] - dx[2] - gdi.scaleLength(4));
      gdi.addString("", y, x + dx[3], 1, "Senast sedd", dx[4] - dx[3] - 4);
      y += lh;
    }

    if (!sStart.empty()) {
      RECT rc = gdi.addStringUT(y, x + dx[0], 0, sStart).textRect;
      if (!sTooltip.empty())
        gdi.addToolTip("", sTooltip, 0, &rc);
    }

    wstring club = it->getClub();
    if (!club.empty())
      club = L" (" + club + L")";

    gdi.addStringUT(y, x + dx[1], 0, it->getName() + club, dx[2] - dx[1] - 4, cb).setExtra(it->getId()).id = "R";
    nr++;
    tnr++;

    if (startTime != NOTIME) {
      int rt = ct - startTime;
      if (rt > 0)
        gdi.addTimer(y, x + dx[2], timeHHMM | textRight, rt / timeConstSecond, timerTemplate, dx[3] - dx[2] - gdi.scaleLength(4));
    }

    if (!punches.empty()) {
      wstring lastSeen = runnerPunches.back()->getSimpleString();
      capitalize(lastSeen);
      if (otherRunners.empty()) {
        RECT rc = gdi.addString("", y, x + dx[3], 0, L"#" + lastSeen, dx[4] - dx[3] - 4).textRect;
        capitalize(punches);
        gdi.addToolTip("", L"#" + punches, 0, &rc);
      }
      else {
        // Återanvänd bricka
        RECT rc = gdi.addString("", y, x + dx[3], 0, L"#" + lastSeen, dx[4] - dx[3] - 4).textRect;
        capitalize(punches);
        gdi.addToolTip("", L"#" + punches + L". " + otherRunners, 0, &rc);
      }
    }
    
    // gdi.addStringUT(y, x + dx[3], 0, it->getClass(true));
    y += lh;
  }
  if (nr > 0) {
    gdi.addString("", y, x, 0, "Antal: X#" + itos(nr));
    y += lh * 2;
  }

  // Team summary

  sortTeams(ClassStartTime, 0, true);

  nr = 0; 
  id = -1;
  for (auto it = Teams.begin(); it != Teams.end(); ++it) {
    if (it->isRemoved())
      continue;

    if (it->Class && it->Class->getClassType() != ClassType::oClassRelay)
      continue;

    bool unknown = false;
    for (int j = 0; j < it->getNumRunners(); j++) {
      pRunner lr = it->getRunner(j);
      if (lr && reported.count(lr->getId())) {
        unknown = true;
        break;
      }
    }

    if (unknown) {
      if (id != it->getClassId(false)) {
        if (nr > 0) {
          gdi.addString("", y, x, 0, "Antal: X#" + itos(nr));
          y += lh;
          nr = 0;
        }
        else {
          gdi.addString("", y, x, fontMediumPlus, "Lag(flera)");
          y += lh;
        }
        y += lh;
        id = it->getClassId(false);
        gdi.addStringUT(y, x, 1, it->getClass(false));
        y += lh;
      }
      gdi.addStringUT(y, x, 0, it->getStartTimeCompact());
      nr++;
      gdi.addStringUT(y, x + dx[1], 0, it->getName(), 0, cb).setExtra(it->getId()).id = "T";
      y += lh;
    }
  }

  if (nr > 0) {
    gdi.addString("", y, x, 0, "Antal: X#" + itos(nr));
    y += lh * 2;
  }


  map<int, vector<const oPunch*>> unknownPunches;

  for (auto& p : punches) {
    if (p.getTypeCode() == oPunch::SpecialPunch::HiredCard)
      continue;
    if (p.getTiedRunner() == nullptr)
      unknownPunches[p.CardNo].push_back(&p);
  }

  if (!unknownPunches.empty()) {
    y += lh;
    gdi.addString("", y, x, 1, "Okända brickor med registreringar");

    bool useRunnerDB = false;
    if (getMeOSFeatures().hasFeature(MeOSFeatures::RunnerDb)) {
      useRunnerDB = true;
      y += lh;
      gdi.addString("", y, x, gdiFonts::fontSmall, "Namn/klubb från löpardatabasen");
    }

    y += (lh*3)/2;
    int cnt = 0;
    for (auto& [card, pl] : unknownPunches) {
      if (cnt > 0 && (cnt % 5) == 0)
        y += lh / 2;

      gdi.addStringUT(y, x, 0, itos(card));
      if (useRunnerDB) {
        auto rdb = getRunnerDatabase().getRunnerByCard(card);
        if (rdb) {
          wstring name, club;
          rdb->getName(name);
          int cno = rdb->dbe().clubNo;
          if (getRunnerDatabase().getClub(cno, club))
            name += L" (" + club + L")?";
          else
            name += L"?";

          gdi.addStringUT(y, x + dx[1], 0, name).setColor(GDICOLOR::colorDarkRed);
        }
      }

      wstring ps;
      for (auto& p : pl) {
        if (!ps.empty())
          ps += L", ";
        ps += p->getType(nullptr) + L" " + p->getTime(true, SubSecond::Off);
      }
      gdi.addStringUT(y, x + dx[2], 0, ps);

      y += lh;
      ++cnt;
    }

    y += lh;
  }

  if (withinTime > 0) {
    y += lh;
    gdi.addString("", y, x, 0, "X deltagare i skogen visas inte#" + itos(withinTime)).setColor(GDICOLOR::colorDarkGreen);
  }
  else if (tnr == 0 && Runners.size() > 0) {
    gdi.addString("", y, x, 10, "inforestwarning");
  }

  if (hasCheck || hasEntry) {
    gdi.dropLine();
    gdi.pushX();
    gdi.fillRight();
    if (hasCheck)
      gdi.addStringUT(0, L"\u00b9 " + lang.tl("Checktid"));
    if (hasEntry)
      gdi.addStringUT(0, L"\u00b2 " + lang.tl("Anmälningstid"));
    gdi.fillDown();
    gdi.dropLine();
    gdi.popX();
  }

  gdi.updateScrollbars();
}

void oEvent::generateMinuteStartlist(gdioutput &gdi) {
  sortRunners(SortByStartTime);

  int dx[4]={0, gdi.scaleLength(70), gdi.scaleLength(340), gdi.scaleLength(510)};
  int y=gdi.getCY();
  int x=gdi.getCX();
  int lh=gdi.getLineHeight();

  vector<int> blocks;
  vector<wstring> starts;
  getStartBlocks(blocks, starts);
  bool compactClub = getPropertyBool("CompactClubName", false);

  wchar_t bf[256];
  for (size_t k=0;k<blocks.size();k++) {
    gdi.dropLine();
    if (k>0)
      gdi.addStringUT(gdi.getCY()-1, 0, pageNewChapter, "");

    gdi.addStringUT(boldLarge|Capitalize, lang.tl(L"Minutstartlista", true) +  makeDash(L" - ") + getName());
    if (!starts[k].empty()) {
      swprintf(bf, sizeof(bf)/sizeof(wchar_t), lang.tl("%s, block: %d").c_str(), starts[k].c_str(), blocks[k]);
      gdi.addStringUT(fontMedium, bf);
    }
    else if (blocks[k]!=0) {
      swprintf(bf, sizeof(bf)/sizeof(wchar_t), lang.tl("Startblock: %d").c_str(),  blocks[k]);
      gdi.addStringUT(fontMedium, bf);
    }

    vector<vector<vector<const oRunner *>>> sb;
    sb.reserve(Runners.size() / 5+10);
    int lastStartTime = -1;
    
    for (const auto &r : Runners) {
      if (r.isRemoved())
        continue;
      if (r.Class && r.Class->getBlock() != blocks[k])
        continue;
      if (r.Class && r.Class->getStart() != starts[k])
        continue;
      if (!r.Class && blocks[k] != 0)
        continue;
      if (r.getStatus() == StatusNotCompeting || r.getStatus() == StatusCANCEL)
        continue;

      if (lastStartTime != r.tStartTime) {
        sb.emplace_back();
        lastStartTime = r.tStartTime;
      }

      if (sb.empty())
        sb.resize(1);

      if (r.tInTeam == 0)
        sb.back().push_back(vector<const oRunner *>(1, &r));
      else {
        if (r.legToRun() > 0 && r.getStartTime() == 0)
          continue;
        int minIx = 10000;
        for (int j = 0; j < r.tInTeam->getNumRunners(); j++) {
          if (j != r.tLeg && r.tInTeam->Runners[j] && r.tInTeam->Runners[j]->tStartTime == r.tStartTime)
            minIx = min(minIx, j);
        }
        if (minIx == 10000)
          sb.back().push_back(vector<const oRunner *>(1, &r)); // Single runner on this start time
        else if (minIx > r.tLeg) {
          sb.back().emplace_back();
          for (int j = 0; j < r.tInTeam->getNumRunners(); j++) {
          if (r.tInTeam->Runners[j] && r.tInTeam->Runners[j]->tStartTime == r.tStartTime)
            sb.back().back().push_back(r.tInTeam->Runners[j]);
          }
        }
      }
    }

    lastStartTime = -1;
    map<int, int> startIntervalCount;
    int totalStartTimes = 0;

    for (size_t k = 0; k < sb.size(); k++) {
      if (!sb[k].empty()) {
        int st = sb[k][0][0]->getStartTime();
        if (lastStartTime != -1 && lastStartTime != st) {
          int startInterval = st - lastStartTime;
          ++startIntervalCount[startInterval];
          totalStartTimes++;
          lastStartTime = st;
        }
        lastStartTime = st;
      }
    }

    int typicalStartInterval = 0;
    int maxStartIntervalCount = 0;
    for (auto& sic : startIntervalCount) {
      if (sic.second > maxStartIntervalCount) {
        maxStartIntervalCount = sic.second;
        typicalStartInterval = sic.first;
      }
    }

    int startInterval = -1;
    if (maxStartIntervalCount > totalStartTimes / 4) {
      startInterval = typicalStartInterval;
    }

    y = gdi.getCY();
    lastStartTime = -1;

    for (size_t k = 0; k < sb.size(); k++) {
      if (sb[k].empty())
        continue;
    
      const int st = sb[k][0][0]->getStartTime();
      if (startInterval > 0 && lastStartTime > 0 && st - lastStartTime > startInterval) {
        int missingStartCount = (st - lastStartTime) / startInterval;
        if (st == lastStartTime + missingStartCount * startInterval) {
          lastStartTime += startInterval;
          int count = 0;
          while (lastStartTime < st) {
            if (++count < 2) {
              y += lh / 2;
              gdi.addStringUT(y, x + dx[0], boldText, getAbsTime(lastStartTime));
              y += lh;
              gdi.addStringUT(y, x + dx[1], fontMedium, L"\u2014");
              y += lh;
            }
            lastStartTime += startInterval;
          }
        }
      }
      lastStartTime = st;

      y+=lh/2;
      if (st > 0) {
        gdi.addStringUT(y, x + dx[0], boldText, sb[k][0][0]->getStartTimeS());
        y += lh;
      }

      for (size_t j = 0; j < sb[k].size(); j++) {
        const int src_y = y;
        int indent = 0;
        const auto &rList = sb[k][j];
        if (rList.size() == 1) {
          if (rList[0]->getCardNo()>0)
            gdi.addStringUT(y, x+dx[0], fontMedium, itos(rList[0]->getCardNo()));

          wstring name;
          if (rList[0]->getBib().empty())
            name = rList[0]->getName();
          else
            name = rList[0]->getName() + L" (" + rList[0]->getBib() + L")";
          gdi.addStringUT(y, x+dx[1], fontMedium, name, dx[2]-dx[1]-4);
        }
        else {
          wstring name;
          if (!rList[0]->tInTeam->getBib().empty())
            name = rList[0]->tInTeam->getBib() + L": ";

          int nnames = 0;
          for (size_t i = 0; i < rList.size(); i++) {
            if (nnames>0)
              name += L", ";
            nnames++;

            if (nnames > 2) {
              gdi.addStringUT(y, x+dx[0]+indent, fontMedium, name, dx[2]-dx[0]-4-indent);
              name.clear();
              nnames = 1;
              y+=lh;
              indent = gdi.scaleLength(20);
            }

            name += rList[i]->getName();
            if (rList[i]->getCardNo()>0) 
              name += L" (" + itow(rList[i]->getCardNo()) + L")";
          }
          gdi.addStringUT(y, x+dx[0]+indent, fontMedium, name, dx[2]-dx[0]-4-indent);
        }
        pClub club = rList[0]->getClubRef();
        if (club) {
          if (compactClub)
            gdi.addStringUT(src_y, x + dx[2], fontMedium, club->getCompactName(), dx[3] - dx[2] - 4);
          else
            gdi.addStringUT(src_y, x + dx[2], fontMedium, club->getDisplayName(), dx[3] - dx[2] - 4);
        }
        gdi.addStringUT(src_y, x + dx[3], fontMedium, rList[0]->getClass(true));
        y+=lh;
      }
    }
  }

  gdi.refresh();
}

const wstring &oEvent::getName() const {
  if (Name.size() > 1 && Name.at(0) == '%') {
    return lang.tl(Name.substr(1));
  }
  else
    return Name;
}

bool oEvent::empty() const
{
  return Name.empty();
}

void oEvent::clearListedCmp()
{
  cinfo.clear();
}

bool oEvent::enumerateCompetitions(const wchar_t *file, const wchar_t *filetype)
{
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::path dirPath(file);

  int id=1;
  cinfo.clear();

  for (const auto& entry : fs::directory_iterator(dirPath, ec)) {
    if (entry.is_regular_file() && glob_match(filetype, entry.path().filename().wstring()))
    {
      CompetitionInfo ci;

      ci.FullPath = entry.path().wstring();
      ci.Name=L"";
      ci.Date=L"2007-01-01";
      ci.Id=id++;

      {
        time_t t = meos_file_time_to_time_t(entry.last_write_time());
        std::tm st = {};
        meos_gmtime(&t, &st);
        ci.Modified=convertSystemTimeN(st);
      }
      xmlparser xp;

      try {
        xp.read(ci.FullPath, 30);

        const xmlobject date=xp.getObject("Date");

        if (date) ci.Date=date.getWStr();

        const xmlobject name=xp.getObject("Name");

        if (name) {
          ci.Name = name.getWStr();
          if (ci.Name.size() > 1 && ci.Name.at(0) == '%') {
            ci.Name = lang.tl(ci.Name.substr(1));
          }
        }
        const xmlobject annotation=xp.getObject("Annotation");

        if (annotation)
          ci.Annotation=annotation.getWStr();

        const xmlobject nameid = xp.getObject("NameId");
        if (nameid)
          ci.NameId = nameid.getWStr();

        auto oData = xp.getObject("oData");
        if (oData) {
          auto preEvent = oData.getObject("PreEvent");
          if (preEvent)
            ci.preEvent = preEvent.getWStr();

          auto postEvent = oData.getObject("PostEvent");
          if (postEvent)
            ci.postEvent = postEvent.getWStr();

          auto importStamp = oData.getObject("ImportStamp");
          if (importStamp)
            ci.importTimeStamp = importStamp.getWStr();
        }
        cinfo.push_front(ci);
      }
      catch (std::exception &) {
        // XXX Do what??
      }
    }
  }

  if (ec)
    return false;

  if (!getServerName().empty())
    sqlConnection->listCompetitions(this, true);

  for (list<CompetitionInfo>::iterator it=cinfo.begin(); it!=cinfo.end(); ++it) {
    if (it->Name.size() > 1 && it->Name[0] == '%')
      it->Name = lang.tl(it->Name.substr(1));
  }

/*
  vector<pair<wstring, wstring>> cc;
  for (auto &c : cinfo) {
    cc.emplace_back(c.NameId, c.Date + L": " + c.Name);
  }
  sort(cc.begin(), cc.end());
  for (auto &c : cc) {
    std::cerr << narrow(c.first);
    std::cerr << ", ";
    std::cerr << narrow(c.second);
    std::cerr << "" << '\n';
  }
*/
  return true;
}

bool oEvent::enumerateBackups(const wstring &file) {
  backupInfo.clear();

  enumerateBackups(file, L"*.meos.bu?", 1);
  enumerateBackups(file, L"*.removed", 1);
  enumerateBackups(file, L"*.dbmeos*", 2);
  backupInfo.sort();

  int id = 1;
  for (list<BackupInfo>::iterator it = backupInfo.begin(); it != backupInfo.end(); ++it) {
    it->backupId = id++;
  }
  return true;
}

const BackupInfo &oEvent::getBackup(int bid) const {
  for (list<BackupInfo>::const_iterator it = backupInfo.begin(); it != backupInfo.end(); ++it) {
    if (it->backupId == bid) {
      return *it;
    }
  }
  throw meosException("Internal error");
}

void oEvent::deleteBackups(const BackupInfo &bu) {
  wstring file = bu.fileName + bu.Name;
  list<wstring> toRemove;

  for (list<BackupInfo>::iterator it = backupInfo.begin(); it != backupInfo.end(); ++it) {
    if (file == it->fileName + it->Name)
      toRemove.push_back(it->FullPath);
  }
  if (!toRemove.empty()) {
    wstring dest = std::filesystem::path(toRemove.back()).parent_path().wstring();
    if (!dest.empty() && dest.back() != L'/' && dest.back() != L'\\')
      dest += L'/';
    toRemove.push_back(dest + bu.fileName + L".persons");
    toRemove.push_back(dest + bu.fileName + L".clubs");
    toRemove.push_back(dest + bu.fileName + L".wclubs");
    toRemove.push_back(dest + bu.fileName + L".wpersons");

    for (list<wstring>::iterator it = toRemove.begin(); it != toRemove.end(); ++it) {
      { std::error_code ec; std::filesystem::remove(it->c_str(), ec); }
    }
  }
}


bool oEvent::listBackups(gdioutput &gdi, GUICALLBACK cb)
{
  int y = gdi.getCY();
  int x = gdi.getCX();

  list<BackupInfo>::iterator it = backupInfo.begin();
  while (it != backupInfo.end()) {
    list<BackupInfo>::iterator sum_size = it;
    size_t s = 0;
    //string date = it->Modified;
    wstring file = it->fileName + it->Name;

    while(sum_size != backupInfo.end() && file == sum_size->fileName + sum_size->Name) {
      s += sum_size->fileSize;
      ++sum_size;
    }
    wstring type = lang.tl(it->type==1 ? L"backup" : L"serverbackup");
    string size;
    if (s < 1024) {
      size = itos(s) + " bytes";
    }
    else if (s < 1024*512) {
      size = itos(s/1024) + " kB";
    }
    else {
      size = itos(s/(1024*1024)) + "." + itos( ((10*(s/1024))/1024)%10) + " MB";
    }
    gdi.dropLine();
    gdi.addStringUT(gdi.getCY(), gdi.getCX(), boldText, it->Name + L" (" + it->Date + L") " + type, 400);
    
    gdi.pushX();
    gdi.fillRight();
    gdi.addString("", 0, "Utrymme: X#" + size);
    gdi.addString("EraseBackup", 0, "[Radera]", cb).setExtra(it->backupId);
    gdi.fillDown();
    gdi.popX();
    gdi.dropLine(1.5);
    y = gdi.getCY();
    while(it != backupInfo.end() && file == it->fileName + it->Name) {
      gdi.addStringUT(y, x+30, 0, it->Modified, 400, cb).setExtra(it->backupId);
      ++it;
      y += gdi.getLineHeight();
    }
  }

  return true;
}

bool BackupInfo::operator<(const BackupInfo &ci)
{
  if (Date!=ci.Date)
    return Date>ci.Date;

  if (fileName!=ci.fileName)
    return fileName<ci.fileName;

  return Modified>ci.Modified;
}


bool oEvent::enumerateBackups(const wstring &file, const wstring &filetype, int type)
{
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::path dirPath(file);

  for (const auto& entry : fs::directory_iterator(dirPath, ec)) {
    if (entry.is_regular_file() && glob_match(filetype, entry.path().filename().wstring()))
    {
      BackupInfo ci;

      ci.type = type;
      ci.FullPath = entry.path().wstring();
      ci.Name=L"";
      ci.Date=L"2007-01-01";
      ci.fileName = entry.path().filename().wstring();
      ci.fileSize = static_cast<size_t>(entry.file_size(ec));
      size_t pIndex = ci.fileName.find_first_of(L".");
      if (pIndex>0 && pIndex<ci.fileName.size())
        ci.fileName = ci.fileName.substr(0, pIndex);

      {
        time_t t = meos_file_time_to_time_t(entry.last_write_time());
        std::tm st = {};
        meos_localtime(&t, &st);
        ci.Modified=convertSystemTimeN(st);
      }
      xmlparser xp;

      try {
        xp.read(ci.FullPath, 5);
        //xmlobject *xo=xp.getObject("meosdata");
        const xmlobject date=xp.getObject("Date");

        if (date) ci.Date=date.getWStr();

        const xmlobject name=xp.getObject("Name");

        if (name) {
          ci.Name=name.getWStr();
          if (ci.Name.size() > 1 && ci.Name.at(0) == '%') {
            ci.Name = lang.tl(ci.Name.substr(1));
          }
        }

        backupInfo.push_front(ci);
      }
      catch (std::exception &) {
        //XXX Do what?
      }
    }
  }

  return !ec;
}

bool oEvent::fillCompetitions(gdioutput &gdi,
                              const string &name, int type,
                              const wstring &select,
                              bool doClear) {
  cinfo.sort();
  cinfo.reverse();
  list<CompetitionInfo>::iterator it;
  const CompetitionInfo *bestMatch = nullptr; 

  auto accept = [this, &bestMatch](const CompetitionInfo &ci) {
    if (bestMatch == nullptr)
      bestMatch = &ci;
    else {
      bool matchPrevNextId = bestMatch->preEvent == currentNameId || bestMatch->postEvent == currentNameId;
      bool ciMatchPrevNextId = ci.preEvent == currentNameId || ci.postEvent == currentNameId;
      if (matchPrevNextId != ciMatchPrevNextId) {
        if (ciMatchPrevNextId)
          bestMatch = &ci;
      }
      else {
        if (ci.Date > bestMatch->Date) {
          bestMatch = &ci;
        }
        else {
          if (ci.importTimeStamp > bestMatch->importTimeStamp)
            bestMatch = &ci;
        }
      }
    }
  };

  if (doClear)
    gdi.clearList(name);
  string b;
  //char bf[128];
  for (it=cinfo.begin(); it!=cinfo.end(); ++it) {
    wstring annotation;
    if (!it->Annotation.empty())
      annotation = L" (" + it->Annotation + L")";
    if (it->Server.length()==0) {
      if (type==0 || type==1) {
        if (it->NameId == select && !select.empty())
          accept(*it);
        wstring bf = L"[" + it->Date + L"] " + it->Name;
        gdi.addItem(name, bf + annotation, it->Id);
      }
    }
    else if (type==0 || type==2) {
      if (it->NameId == select && !select.empty())
        accept(*it);
      wstring bf;
      if (type==0)
        bf = lang.tl(L"Server: [X] Y#" + it->Date + L"#" + it->Name);
      else
         bf = L"[" + it->Date + L"] " + it->Name;

      gdi.addItem(name, bf + annotation, 10000000+it->Id);
    }
  }

  if (bestMatch)
    gdi.selectItemByData(name.c_str(), bestMatch->Id);

  return true;
}

void oEvent::checkDB()
{
  if (hasDBConnection()) {
    vector<wstring> err;
    int k=checkChanged(err);

#ifdef _DEBUG
    if (k>0) {
      wchar_t bf[256];
      swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"Databasen innehåller %d osynkroniserade ändringar.", k);
      wstring msg(bf);
      for(int i=0;i < min<int>(err.size(), 10);i++)
        msg+=wstring(L"\n")+err[i];

      std::cerr << narrow(msg) << '\n';
    }
#endif
  }
  updateTabs();
  gdibase.setWindowTitle(getTitleName());
}

void destroyExtraWindows();

void oEvent::clear()
{
  checkDB();

  if (hasDBConnection())
    sqlConnection->checkConnection(0);

  isConnectedToServer = false;
  hasPendingDBConnection = false;

  destroyExtraWindows();

  tables.clear();
  Table::resetTableIds();

  getRunnerDatabase().releaseTables();
  getMeOSFeatures().clear(*this);
  Id=0;
  dataRevision = 0;
  tClubDataRevision = -1;
  tCalcNumMapsDataRevision = -1;

  ZeroTime=0;
  Name.clear();
  Annotation.clear();

  //Make sure no daemon is hunting us.
  if (cbKillMachines) cbKillMachines();

  delete directSocket;
  directSocket = 0;

  tLongTimesCached = -1;

  //Order of destruction is extreamly important...
  cardToRunnerHash.reset();
  classIdToRunnerHash.reset();
  runnerById.clear();
  bibStartNoToRunnerTeam.clear();
  Runners.clear();
  Teams.clear();
  teamById.clear();

  Classes.clear();
  Courses.clear();
  courseIdIndex.clear();

  Controls.clear();

  Cards.clear();
  Clubs.clear();
  clubIdIndex.clear();

  punchIndex.clear();
  punches.clear();
  cachedFirstStart.clear();
  hiredCardHash.clear();

  updateFreeId();

  currentNameId.clear();
  wcscpy_s(CurrentFile, L"");

  sqlRunners.reset();
  sqlClasses.reset();
  sqlCourses.reset();
  sqlControls.reset();
  sqlClubs.reset();
  sqlCards.reset();
  sqlPunches.reset();
  sqlTeams.reset();
  
  vacantId = 0;
  noClubId = 0;
  oEventData->initData(this, sizeof(oData));
  timelineClasses.clear();
  timeLineEvents.clear();
  nextTimeLineEvent = 0;

  tCurrencyFactor = 1;
  tCurrencySymbol = L"kr";
  tCurrencySeparator = L",";
  tCurrencyPreSymbol = false;

  readPunchHash.clear();

  //Reset speaker data structures.
  listContainer->clearExternal();
  while(!generalResults.empty() && generalResults.back().isDynamic())
    generalResults.pop_back();

  // Cleanup user interface
  if (isMainEvent)
    gdibase.getTabs().clearCompetitionData();
  
  machineContainer.release();
  renderMaps.reset();

  MeOSUtil::useHourFormat = getPropertyInt("UseHourFormat", 1) != 0;

  currentNameMode = (NameMode) getPropertyInt("NameMode", FirstLast);

  hasWarnedModifiedExtId = false;

  useSubsecondsVersion = -1; 
}

const shared_ptr<Table> &oEvent::getTable(const string &key) const {
  if (tables.count(key)) {
    tables.find(key)->second->update();
    return tables.find(key)->second;
  }
  throw meosException("Unknown table " + key);
}

void oEvent::setTable(const string &key, const shared_ptr<Table> &table) {
  tables[key] = table;
}

bool oEvent::deleteCompetition()
{
  if (!empty() && !hasDBConnection()) {
    wstring removed = wstring(CurrentFile)+L".removed";
    ::_wremove(removed.c_str()); //Delete old removed file
    openFileLock->unlockFile();
    ::_wrename(CurrentFile, removed.c_str());
    return true;
  }
  else return false;
}

void oEvent::newCompetition(const wstring &name)
{
  openFileLock->unlockFile();
  clear();

  std::tm st = {};
  meos_localtime_now(&st);

  Date = convertSystemDate(st);
  ZeroTime = st.tm_hour*timeConstHour;

  Name = name;
  oEventData->initData(this, sizeof(oData));

  if (!name.empty() && name != L"-")
    getMergeTag();

  setCurrency(-1, L"", L"", 0);

  wstring file;
  getNewFileName(file, currentNameId);
  wcscpy_s(CurrentFile, 260, file.c_str());

  oe->updateTabs();
}

void oEvent::loadDefaults() {
  getDI().setString("Organizer", getPropertyString("Organizer", L""));
  getDI().setString("Street", getPropertyString("Street", L""));
  getDI().setString("Address", getPropertyString("Address", L""));
  getDI().setString("EMail", getPropertyString("EMail", L""));
  getDI().setString("Homepage", getPropertyString("Homepage", L""));

  getDI().setInt("CardFee", getPropertyInt("CardFee", 25));
  getDI().setInt("EliteFee", getPropertyInt("EliteFee", 130));
  getDI().setInt("EntryFee", getPropertyInt("EntryFee", 90));
  getDI().setInt("YouthFee", getPropertyInt("YouthFee", 50));

  getDI().setInt("SeniorAge", getPropertyInt("SeniorAge", 0));
  getDI().setInt("YouthAge", getPropertyInt("YouthAge", 16));

  getDI().setString("Account", getPropertyString("Account", L""));
  getDI().setString("LateEntryFactor", getPropertyString("LateEntryFactor", L"50 %"));

  getDI().setString("CurrencySymbol", getPropertyString("CurrencySymbol", L"kr"));
  getDI().setString("CurrencySeparator", getPropertyString("CurrencySeparator", L"."));
  getDI().setInt("CurrencyFactor", getPropertyInt("CurrencyFactor", 1));
  getDI().setInt("CurrencyPreSymbol", getPropertyInt("CurrencyPreSymbol", 0));
  getDI().setString("PayModes", getPropertyString("PayModes", L""));
  setCurrency(-1, L"", L"", 0);

  getDI().setInt("UTC", oe->getPropertyInt("UseEventorUTC", 0) != 0);
  getDI().setInt("OldCards", oe->getPropertyInt("OldCards", 0));
}

void oEvent::reEvaluateCourse(int CourseId, bool doSync)
{
  oRunnerList::iterator it;

  if (doSync)
    autoSynchronizeLists(false);

  vector<int> mp;
  set<int> classes;
  for(it=Runners.begin(); it != Runners.end(); ++it){
    if (it->getCourse(false) && it->getCourse(false)->getId()==CourseId){
      classes.insert(it->getClassId(true));
    }
  }

  reEvaluateAll(classes, false);
}

void oEvent::reEvaluateAll(const set<int> &cls, bool doSync)
{
  if (disableRecalculate)
    return;

  if (doSync)
    autoSynchronizeLists(false);

  for(oClassList::iterator it=Classes.begin();it!=Classes.end();++it) {
    if (cls.empty() || cls.count(it->Id)) {
      it->clearSplitAnalysis();
      it->resetLeaderTime();
      it->reinitialize(true);
    }
  }

  for(oTeamList::iterator tit=Teams.begin();tit!=Teams.end();++tit) {
    if (!cls.empty() && cls.count(tit->getClassId(false)) == 0)
      continue;

    if (!tit->isRemoved()) {
      tit->apply(ChangeType::Quiet, nullptr);
    }
  }
  oRunnerList::iterator it;

  if (cls.size() < 5) {
    vector<pRunner> runners;
    getRunners(cls, runners);
    for (pRunner it : runners) {
      if (!it->tInTeam || it->Class != it->tInTeam->Class || (it->Class && it->Class->isQualificationFinalBaseClass())) {
        it->apply(ChangeType::Quiet, nullptr);
      }
    }
  }
  else {
    for (it = Runners.begin(); it != Runners.end(); ++it) {
      if (!cls.empty() && cls.count(it->getClassId(true)) == 0)
        continue;

      if (!it->tInTeam || it->Class != it->tInTeam->Class || (it->Class && it->Class->isQualificationFinalBaseClass())) {
        it->apply(ChangeType::Quiet, nullptr);
      }
    }
  }

  vector<pair<int, pControl>> mp;
  bool needupdate = true;
  int leg = 0;
  while (needupdate) {
    needupdate = false;
    for (it=Runners.begin(); it != Runners.end(); ++it) {
      if (!cls.empty() && cls.count(it->getClassId(true)) == 0)
         continue;

      if (!it->isRemoved()) {
        if (it->tLeg == leg) {
          it->evaluateCard(false, mp, 0, ChangeType::Quiet); // Must not sync!
          it->storeTimes();
        }
        else if (it->tLeg>leg)
          needupdate = true;
      }
    }
    leg++;
  }

  // Mark info as complete
  for (auto& c : Classes) {
    if (!c.isRemoved() && (cls.empty() || cls.count(c.Id)))
      for (auto &i : c.tLeaderTime)
        i.setComplete();
  }

  // Update team start times etc.
  for(oTeamList::iterator tit=Teams.begin();tit!=Teams.end();++tit) {
    if (!tit->isRemoved()) {
      if (!cls.empty() && cls.count(tit->getClassId(true)) == 0)
        continue;

      tit->apply(ChangeType::Quiet, nullptr);
    }
  }
  for (it=Runners.begin(); it != Runners.end(); ++it) {
    if (!it->isRemoved()) {
      if (!cls.empty() && cls.count(it->getClassId(true)) == 0)
        continue;

      if (!it->tInTeam || it->Class != it->tInTeam->Class || (it->Class && (it->Class->isQualificationFinalBaseClass())))
        it->apply(ChangeType::Quiet, nullptr);
      it->storeTimes();
      it->clearOnChangedRunningTime();
    }
  }
  //reCalculateLeaderTimes(0);
}

void oEvent::reEvaluateChanged()
{
  if (sqlClasses.changed || sqlCourses.changed || sqlControls.changed) {
    reEvaluateAll(set<int>(), false);
    globalModification = true;
    return;
  }

  if (sqlClubs.changed)
    globalModification = true;


  if (!sqlCards.changed && !sqlRunners.changed && !sqlTeams.changed)
    return; // Nothing to do

  map<int, bool> resetClasses;
  for(oClassList::iterator it=Classes.begin();it!=Classes.end();++it)  {
    if (it->wasSQLChanged(-1, oPunch::PunchFinish)) {
      it->clearSplitAnalysis();
      it->resetLeaderTime();
      it->reinitialize(true);
      resetClasses[it->getId()] = it->hasClassGlobalDependence();
      it->updateLeaderTimes();
    }
  }

  unordered_set<int> addedTeams;

  for(oTeamList::iterator tit=Teams.begin();tit!=Teams.end();++tit) {
    if (tit->isRemoved() || !tit->wasSQLChanged())
      continue;

    addedTeams.insert(tit->getId());
    
    tit->apply(ChangeType::Quiet, nullptr);
  }

  oRunnerList::iterator it;
  vector< vector<pRunner> > legRunners(maxRunnersTeam);

  if (Teams.size() > 0) {
    for (it=Runners.begin(); it != Runners.end(); ++it) {
      if (it->isRemoved())
        continue;
      int clz = it->getClassId(true);
      //if (resetClasses.count(clz))
      //  it->storeTimes();

      if (!it->wasSQLChanged() && !resetClasses[clz])
        continue;

      pTeam t = it->tInTeam;
      if (t && !addedTeams.count(t->getId())) {
        addedTeams.insert(t->getId());
        t->apply(ChangeType::Quiet, nullptr);
      }
    }
  }

  for (it=Runners.begin(); it != Runners.end(); ++it) {
    pRunner r = &*it;
    if (r->isRemoved())
      continue;

    if (r->wasSQLChanged() || (r->tInTeam && addedTeams.count(r->tInTeam->getId()))) {
      unsigned leg = r->tLeg;
      if (leg <0 || leg >= maxRunnersTeam)
        leg = 0;

      if (legRunners[leg].empty())
        legRunners[leg].reserve(Runners.size() / (leg+1));

      legRunners[leg].push_back(r);
      if (!r->tInTeam) {
        r->apply(ChangeType::Quiet, nullptr);
      }
    }
    else {
      if (r->Class && r->Class->wasSQLChanged(-1, oPunch::PunchFinish)) {
        it->storeTimes();
      }
    }
  }

  vector<pair<int, pControl>> mp;

  // Reevaluate
  for (size_t leg = 0; leg < legRunners.size(); leg++) {
    const vector<pRunner> &lr = legRunners[leg];
    for (size_t k = 0; k < lr.size(); k++) {
      lr[k]->evaluateCard(false, mp, 0, ChangeType::Quiet); // Must not sync!
    }
  }

  for(oTeamList::iterator tit=Teams.begin();tit!=Teams.end();++tit) {
    if (addedTeams.count(tit->getId())) {
      tit->apply(ChangeType::Quiet, nullptr);
    }
  }

  for (size_t leg = 0; leg < legRunners.size(); leg++) {
    const vector<pRunner> &lr = legRunners[leg];
    for (size_t k = 0; k < lr.size(); k++) {
      if (!lr[k]->tInTeam)
        lr[k]->apply(ChangeType::Quiet, nullptr);
      lr[k]->clearOnChangedRunningTime();
    }
  }
}

void oEvent::reCalculateLeaderTimes(int classId)
{
  if (disableRecalculate)
    return;

  if (classId) {
    pClass cls = getClass(classId);
    if (cls)
      cls->resetLeaderTime();
  }
  else {
    for (auto &c : Classes) {
      if (!c.isRemoved())
        c.resetLeaderTime();
    }
  }
  
  /*
#ifdef _DEBUG
  wchar_t bf[128];
  swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"Calculate leader times %d\n", classId);
  std::cerr << bf;
#endif
  for (oClassList::iterator it=Classes.begin(); it != Classes.end(); ++it) {
    if (!it->isRemoved() && (classId==it->getId() || classId==0))
      it->resetLeaderTime();
  }
  bool needupdate = true;
  int leg = 0;
  while (needupdate) {
    needupdate = false;
    for (oRunnerList::iterator it=Runners.begin(); it != Runners.end(); ++it) {
      if (!it->isRemoved() && (classId==0 || classId==it->getClassId(true))) {
        if (it->tLeg == leg)
          it->storeTimes();
        else if (it->tLeg>leg)
          needupdate = true;
      }
    }
    leg++;
  }*/
}


wstring oEvent::getCurrentTimeS() const
{
  std::tm st = {};
  meos_localtime_now(&st);

  wchar_t bf[64];
  swprintf(bf, 64, L"%02d:%02d:%02d", st.tm_hour, st.tm_min, st.tm_sec);
  return bf;
}

int oEvent::findBestClass(const SICard &card, vector<pClass> &classes) const
{
  classes.clear();
  int Distance=-1000;
  oClassList::const_iterator it;

  for (it=Classes.begin(); it != Classes.end(); ++it) {
    vector<pCourse> courses;
    it->getCourses(0, courses);
    bool insertClass = false; // Make sure a class is only included once

    for (size_t k = 0; k<courses.size(); k++) {
      pCourse pc = courses[k];
      if (pc) {
        int d=pc->distance(card);

        if (d>=0) {
          if (Distance<0) Distance=1000;

          if (d<Distance) {
            Distance=d;
            classes.clear();
            insertClass = true;
            classes.push_back(pClass(&*it));
          }
          else if (d == Distance) {
            if (!insertClass) {
              insertClass = true;
              classes.push_back(pClass(&*it));
            }
          }
        }
        else {
          if (Distance<0 && d>Distance) {
            Distance = d;
            classes.clear();
            insertClass = true;
            classes.push_back(pClass(&*it));
          }
          else if (Distance == d) {
            if (!insertClass) {
              insertClass = true;
              classes.push_back(pClass(&*it));
            }
          }
        }
      }
    }
  }
  return Distance;
}

void oEvent::convertTimes(pRunner runner, SICard &sic) const
{
  assert(sic.convertedTime != ConvertedTimeStatus::Unknown);
  if (sic.convertedTime == ConvertedTimeStatus::Done)
    return;

  if (sic.convertedTime == ConvertedTimeStatus::Hour12) {

    int startTime = ZeroTime + 2*timeConstHour; //Add two hours. Subtracted below
    if (useLongTimes())
      startTime = 7 * timeConstHour; // Avoid midnight as default. Prefer morning

    int st = -1;
    if (runner) {
      st = runner->getStartTime();
      if (st > 0) {
        if (sic.StartPunch.Code == -1)
          startTime = (ZeroTime + st) % (timeConstHour * 24); // No start punch
        else {
          // Got start punch. If this is close to specified start time,
          // use specified start time
          const int stPunch = sic.StartPunch.Time; // 12 hour
          const int stStart = startTime = (ZeroTime + st) % (timeConstHour * 12); // 12 hour
          if (std::abs(stPunch - stStart) < timeConstHour / 2) {
            startTime = (ZeroTime + st) % (timeConstHour * 24); // Use specified start time (for conversion)
          }
          else {
            st = -1; // Ignore start time
          }
        }
      }
      else {
        st = -1;
      }
    }

    if (st <= -1) {
      // Fallback for no start time. Take from card. Will be wrong if more than 12 hour after ZeroTime
      if (sic.StartPunch.Code != -1) {
        st = sic.StartPunch.Time;
      }
      else if (sic.nPunch > 0 && sic.Punch[0].Time >= 0) {
        st = sic.Punch[0].Time;
      }

      if (st >= 0) { // Optimize local zero time w.r.t first punch
        int relT12 = (st - ZeroTime + timeConstHour * 24) % (timeConstHour * 12);
        startTime = (ZeroTime + relT12) % (timeConstHour * 24);
      }
    }
    int zt = (startTime + 22 * timeConstHour) % (24 * timeConstHour); // Subtract two hours from start time
    sic.analyseHour12Time(zt);
  }
  sic.convertedTime = ConvertedTimeStatus::Done;

  if (sic.CheckPunch.Code!=-1){
    if (sic.CheckPunch.Time<unsigned(ZeroTime))
      sic.CheckPunch.Time+=(24*timeConstHour);

    sic.CheckPunch.Time-=ZeroTime;
  }

   // Support times longer than 24 hours
  int maxLegTime = useLongTimes() ? 22 * timeConstHour : 0;
  
  if (maxLegTime > 0) {

    const int START = 1000;
    const int FINISH = 1001;
    vector<pair<int, int> > times;
    
    if (sic.StartPunch.Code!=-1) {
      if (sic.StartPunch.Time != -1)
        times.push_back(make_pair(sic.StartPunch.Time, START));
    }

    for (unsigned k=0; k <sic.nPunch; k++){
      if (sic.Punch[k].Code!=-1 && sic.Punch[k].Time != -1) {
        times.push_back(make_pair(sic.Punch[k].Time, k));
      }
    }

    if (sic.FinishPunch.Code!=-1 && sic.FinishPunch.Time != 1) {
      times.push_back(make_pair(sic.FinishPunch.Time, FINISH));

    if (!times.empty()) {
      int dayOffset = 0;
      if (times.front().first < int(ZeroTime)) {
        dayOffset = timeConstHour * 24;
        times.front().first += dayOffset;
      }
      for (size_t k = 1; k < times.size(); k++) {
        int delta = times[k].first - (times[k-1].first - dayOffset);
        if (delta < (maxLegTime - 24 * timeConstHour)) {
          dayOffset += 24 * timeConstHour;
        }
        times[k].first += dayOffset;
      }

      // Update card times
      for (size_t k = 0; k < times.size(); k++) {
        if (times[k].second == START)
          sic.StartPunch.Time = times[k].first;
        else if (times[k].second == FINISH)
          sic.FinishPunch.Time = times[k].first;
        else 
          sic.Punch[times[k].second].Time = times[k].first;
        }
      }
    }
  }

  if (sic.StartPunch.Code != -1) {
    if (sic.StartPunch.Time<unsigned(ZeroTime))
      sic.StartPunch.Time+=(24*timeConstHour);

    sic.StartPunch.Time-=ZeroTime;
  }

  for (unsigned k = 0; k < sic.nPunch; k++){
    if (sic.Punch[k].Code!=-1){
      if (sic.Punch[k].Time<unsigned(ZeroTime))
        sic.Punch[k].Time+=(24*timeConstHour);

      sic.Punch[k].Time-=ZeroTime;
    }
  }

  if (sic.FinishPunch.Code!=-1){
    if (sic.FinishPunch.Time<unsigned(ZeroTime))
      sic.FinishPunch.Time+=(24*timeConstHour);

    sic.FinishPunch.Time-=ZeroTime;
  }
}

int oEvent::getFirstStart(int classId, bool considerStartPunches) const {
  int key = (classId + 1) * (considerStartPunches ? -1 : 1);
  
  auto& [revision, firstStart] = cachedFirstStart[key];
  if (dataRevision == revision)
    return firstStart;

  int minTime = timeConstHour * 240;

  for (auto &r : Runners) {
    if (r.isRemoved() || !(classId == 0 || r.getClassId(true) == classId)) 
      continue;
      
    if (r.tStartTime > minTime || r.tStatus == StatusNotCompeting || r.tStartTime <= 0)
      continue;

    if (!considerStartPunches && r.Card && r.tUseStartPunch) {
      int startCode = oPunch::SpecialPunch::PunchStart;
      if (auto c = r.getCourse(false); c && c->useFirstAsStart() && c->getControl(0)) {
        startCode = c->getControl(0)->getFirstNumber();
      }
      bool skip = false;
      for (auto p : r.Card->punches) {
        if (p.getTypeCode() == startCode) {
          skip = true;
          break;
        }
      }
      if (skip)
        continue;
    }
        
    minTime = r.tStartTime;
  }

  if (minTime == timeConstHour * 240)
    minTime = 0;

  revision = dataRevision;
  firstStart = minTime;

  return minTime;
}

bool oEvent::hasRank() const {
  for (auto &r : Runners){
    if (!r.isRemoved()) {
      int rank = r.getDCI().getInt("Rank");
      if (rank > 0 && rank < MaxOrderRank)
        return true;
    }
  }
  return false;
}

void oEvent::setMaximalTime(const wstring &t)
{
  getDI().setInt("MaxTime", convertAbsoluteTime(t));
}

int oEvent::getMaximalTime() const
{
  return getDCI().getInt("MaxTime");
}

wstring oEvent::getMaximalTimeS() const
{
  return formatTime(getMaximalTime());
}


bool oEvent::hasBib(bool runnerBib, bool teamBib) const
{
  if (runnerBib) {
    oRunnerList::const_iterator it;
    for (it=Runners.begin(); it != Runners.end(); ++it){
      if (!it->getBib().empty())
        return true;
    }
  }
  if (teamBib) {
    oTeamList::const_iterator it;
    for (it=Teams.begin(); it != Teams.end(); ++it){
      if (!it->getBib().empty())
        return true;
    }
  }
  return false;
}

bool oEvent::hasTeam() const
{
  return Teams.size() > 0;
}

void oEvent::addBib(int ClassId, int leg, const wstring& firstNumber, int limit, bool assignToVacant) {
  if (!classHasTeams(ClassId)) {
    sortRunners(ClassStartTimeClub);

    pClass cls = getClass(ClassId);
    if (cls == 0)
      throw meosException("Class not found");

    if (cls->getParentClass()) {
      cls->getParentClass()->setBibMode(BibFree);
      cls->getParentClass()->synchronize(true);
    }
    if (!firstNumber.empty()) {
      cls->setBibMode(BibFree);
      cls->synchronize(true);
      wchar_t pattern[32];
      int num = oClass::extractBibPattern(firstNumber, pattern);
      int count = 0;
      for (auto& r : Runners) {
        if (r.isRemoved())
          continue;
        if ((ClassId == 0 || r.getClassId(true) == ClassId) && (r.legToRun() == leg || leg == -1)) {
          bool skip = !assignToVacant && r.isVacant();
          wchar_t bib[32];
          swprintf(bib, sizeof(bib)/sizeof(wchar_t), pattern, num);

          pClass pc = r.getClassRef(true);
          if ((limit == 0 || count < limit) && !skip) {
            r.setBib(bib, num, pc ? !pc->lockedForking() : true);
            count++;
            num++;
          }
          else {
            r.setBib(L"", 0, false);//Update only bib
          }

          r.synchronize(true);
        }
      }
    }
    else {
      for (auto r : Runners) {
        if (r.isRemoved())
          continue;
        if (ClassId == 0 || r.getClassId(true) == ClassId) {
          r.setBib(L"", 0, false);//Update only bib
          r.synchronize(true);
        }
      }
    }
  }
  else {
    map<int, int> teamStartNo;

    if (!firstNumber.empty()) {
      // Clear out start number temporarily, to not use it for sorting
      for (auto it = Teams.begin(); it != Teams.end(); ++it) {
        if (it->isRemoved())
          continue;
        if (!assignToVacant && it->isVacant())
          continue;
        if (ClassId == 0 || it->getClassId(false) == ClassId) {
          if (it->getClassRef(false) && it->getClassRef(false)->getBibMode() != BibFree) {
            for (size_t i = 0; i < it->Runners.size(); i++) {
              if (it->Runners[i]) {
                it->Runners[i]->setStartNo(0, ChangeType::Update);
                it->Runners[i]->setBib(L"", 0, false);
              }
            }
          }
          teamStartNo[it->getId()] = it->getStartNo();
          it->setStartNo(0, ChangeType::Update);
        }
      }
    }

    sortTeams(ClassStartTimeClub, 0, true); // Sort on first leg starttime and sortindex

    if (!firstNumber.empty()) {
      wchar_t pattern[32];
      int num = oClass::extractBibPattern(firstNumber, pattern);
      int count = 0;
      for (auto it = Teams.begin(); it != Teams.end(); ++it) {
        if (it->isRemoved())
          continue;

        if (ClassId == 0 || it->getClassId(false) == ClassId) {
          if ((!assignToVacant && it->isVacant()) || (limit > 0 && count >= limit)) {
            it->setBib(L"", 0, false); // Does nothing, already cleared
            it->applyBibs();
            it->evaluate(ChangeType::Update);
            continue;
          }

          count++;
          wchar_t bib[32];
          swprintf(bib, sizeof(bib)/sizeof(wchar_t), pattern, num);
          bool lockedStartNo = it->Class && it->Class->lockedForking();
          if (lockedStartNo) {
            it->setBib(bib, num, false);
            it->setStartNo(teamStartNo[it->getId()], ChangeType::Update);
          }
          else {
            it->setBib(bib, num, true);
          }
          num++;
          it->applyBibs();
          it->evaluate(ChangeType::Update);
        }
      }
    }
    else {
      for (auto it = Teams.begin(); it != Teams.end(); ++it) {
        if (ClassId == 0 || it->getClassId(false) == ClassId) {
          it->getDI().setString("Bib", L""); //Update only bib
          it->applyBibs();
          it->evaluate(ChangeType::Update);
        }
      }
    }
  }
}

void oEvent::addAutoBib() {
  bool noBibToVacant = oe->getDCI().getInt("NoVacantBib") != 0;

  sortRunners(ClassStartTimeClub);
  oRunnerList::iterator it;
  int clsId = -1;
  const int bibGap = oe->getBibClassGap();
  int numBibPerClass = oe->getDCI().getInt("BibsPerClass");
  if (numBibPerClass <= 0)
    numBibPerClass = numeric_limits<int>::max();

  int interval = 1;
  set<int> isTeamCls;
  wchar_t pattern[32] = {0};
  wchar_t storedPattern[32] = {0};
  wcscpy_s(storedPattern, L"%d");

  
  map<int, int> teamStartNo;
  // Clear out start number temporarily, to not use it for sorting
  for (oTeamList::iterator tit = Teams.begin(); tit != Teams.end(); ++tit) {
    if (tit->skip())
      continue;
    pClass cls = tit->getClassRef(false);
    if (cls == 0)
      continue;

    teamStartNo[tit->getId()] = tit->getStartNo();

    wstring bibInfo = cls->getDCI().getString("Bib");
  
    bool teamAssign = !bibInfo.empty() && cls->getNumStages() > 1;

    bool freeMode = cls->getBibMode()==BibFree;
    if (!teamAssign && freeMode)
      continue; // Manul or none
    isTeamCls.insert(cls->getId());

    bool addBib = bibInfo != L"-";

    if (addBib && teamAssign)
      tit->setStartNo(0, ChangeType::Update);

    if (tit->getClassRef(false) && tit->getClassRef(false)->getBibMode() != BibFree) {
      for (size_t i = 0; i < tit->Runners.size(); i++) {
        if (tit->Runners[i]) {
          if (addBib && teamAssign)
            tit->Runners[i]->setStartNo(0, ChangeType::Update);
          if (!freeMode)
            tit->Runners[i]->setBib(L"", 0, false);
        }
      }
    }
  }

  sortTeams(ClassStartTimeClub, 0, true); // Sort on first leg starttime and sortindex
  map<int, vector<pTeam> > cls2TeamList;

  for (oTeamList::iterator tit = Teams.begin(); tit != Teams.end(); ++tit) {
    if (tit->skip())
      continue;
    int clsId = tit->getClassId(false);
    cls2TeamList[clsId].push_back(&*tit);
  }

  map<int, vector<pRunner> > cls2RunnerList;
  for (it = Runners.begin(); it != Runners.end(); ++it) {
    if (it->isRemoved() || !it->getClassId(false))
      continue;
    int clsId = it->getClassId(true);
    cls2RunnerList[clsId].push_back(&*it);
  }

  Classes.sort();
  int number = 0;

  for (auto &cls : Classes) {
    if (cls.isRemoved())
      continue;
  
    clsId = cls.getId();

    wstring bibInfo = cls.getDCI().getString("Bib");
    if (bibInfo.empty()) {
      // Skip class
      continue;
    }
    else if (bibInfo == L"*") {
      if (number == 0)
        number = 1;
      else 
        number += bibGap;

      if (pattern[0] == 0) {
        wcscpy_s(pattern, storedPattern);
      }
    }
    else if (bibInfo == L"-") {
      if (pattern[0]) {
        wcscpy_s(storedPattern, pattern);
      }
      pattern[0] = 0; // Clear bibs in class

    }
    else {
      number = oClass::extractBibPattern(bibInfo, pattern); 
    }
      
    if (isTeamCls.count(clsId)) {
      vector<pTeam> &tl = cls2TeamList[clsId]; 

      if (cls.getBibMode() == BibAdd) {
        int ns = cls.getNumStages();
        if (ns <= 10)
          interval = 10;
        else
          interval = 100;

        if (bibInfo == L"*") {
          int add = interval - number % interval;
          number += add;
        }
      }
      else {
        interval = 1;
      }

      if (pattern[0] == 0) {
        // Remove bib
        for (size_t k = 0; k < tl.size(); k++) {
          tl[k]->getDI().setString("Bib", L""); //Update only bib
          tl[k]->applyBibs();
          tl[k]->evaluate(ChangeType::Update);
        }
      }
      else  {
        bool lockedForking = cls.lockedForking();
        
        for (size_t k = 0; k < tl.size(); k++) {
          if ( (noBibToVacant && tl[k]->isVacant()) || k >= numBibPerClass) {
            tl[k]->getDI().setString("Bib", L""); //Remove only bib
          }
          else {
            wchar_t buff[32];
            swprintf(buff, sizeof(buff)/sizeof(wchar_t), pattern, number);

            if (lockedForking) {
              tl[k]->setBib(buff, number, false);
              tl[k]->setStartNo(teamStartNo[tl[k]->getId()], ChangeType::Update);
            }
            else {
              tl[k]->setBib(buff, number, true);
            }
            number += interval;
          }
          tl[k]->applyBibs();
          tl[k]->evaluate(ChangeType::Update);
        }
      }

      continue;
    }
    else {
      interval = 1;
    
      vector<pRunner> &rl = cls2RunnerList[clsId]; 
      bool locked = cls.lockedForking();
      if (pattern[0] && cls.getParentClass()) {
        // Switch to free mode if bib set for subclass
        cls.getParentClass()->setBibMode(BibFree);
        cls.setBibMode(BibFree);
        cls.getParentClass()->synchronize(true);
        cls.synchronize(true);
      }
      for (size_t k = 0; k < rl.size(); k++) {
        if (pattern[0] && (!noBibToVacant || !rl[k]->isVacant()) && k < numBibPerClass) {
          wchar_t buff[32];
          swprintf(buff, sizeof(buff)/sizeof(wchar_t), pattern, number);
          rl[k]->setBib(buff, number, !locked);
          number += interval;
        }
        else {
          rl[k]->getDI().setString("Bib", L""); //Update only bib
        }
        rl[k]->synchronize(true);
      }
    }
  }
}

void oEvent::checkOrderIdMultipleCourses(int ClassId) {
  sortRunners(ClassStartTime);
  int order = 1;
  oRunnerList::iterator it;

  //Find first free order
  for (it = Runners.begin(); it != Runners.end(); ++it) {
    if (it->isRemoved())
      continue;
    if (ClassId == 0 || it->getClassId(false) == ClassId) {
      it->synchronize();//Ensure we are up-to-date
      order = max(order, it->StartNo);
    }
  }

  //Assign orders
  for (it = Runners.begin(); it != Runners.end(); ++it) {
    if (it->isRemoved())
      continue;
    if (it->getClassRef(true) && it->getClassRef(true)->lockedForking())
      continue;
    if (ClassId == 0 || it->getClassId(false) == ClassId)
      if (it->StartNo == 0) {
        if (it->getTeam()) {
          if (it->getTeam()->getStartNo() == 0) {
            it->updateStartNo(++order);
          }
          else {
            it->setStartNo(it->getTeam()->getStartNo(), ChangeType::Update);
            it->synchronize(true);
          }
        }
        else {
          it->updateStartNo(++order);
        }
      }
  }
}

void oEvent::fillStatus(gdioutput &gdi, const string& id)
{
  vector< pair<wstring, size_t> > d;
  fillStatus(d);
  gdi.setItems(id, d);
}

const vector< pair<wstring, size_t> > &oEvent::fillStatus(vector< pair<wstring, size_t> > &out) {
  out.clear();
  out.push_back(make_pair(lang.tl(L"-"), StatusUnknown));
  out.push_back(make_pair(lang.tl(L"Godkänd"), StatusOK));
  out.push_back(make_pair(lang.tl(L"Ej start"), StatusDNS));
  out.push_back(make_pair(lang.tl(L"Återbud[status]"), StatusCANCEL));
  out.push_back(make_pair(lang.tl(L"Felst."), StatusMP));
  out.push_back(make_pair(lang.tl(L"Utg."), StatusDNF));
  out.push_back(make_pair(lang.tl(L"Disk."), StatusDQ));
  out.push_back(make_pair(lang.tl(L"Maxtid"), StatusMAX));
  out.push_back(make_pair(lang.tl(L"Utom tävlan"), StatusOutOfCompetition));
  out.push_back(make_pair(lang.tl(L"Utan tidtagning"), StatusNoTiming));
  out.push_back(make_pair(lang.tl(L"Deltar ej"), StatusNotCompeting));
  return out;
}

int oEvent::getPropertyInt(const char *name, int def)
{
  if (eventProperties.count(name)==1)
    return wtoi(eventProperties[name].c_str());
  else {
    setProperty(name, def);
    return def;
  }
}

const wstring &oEvent::getPropertyString(const char *name, const wstring &def)
{
  if (eventProperties.count(name)==1) {
    return eventProperties[name];
  }
  else {
    eventProperties[name] = def;
    return eventProperties[name];
  }
}

const string &oEvent::getPropertyString(const char *name, const string &def)
{
  if (eventProperties.count(name)==1) {
    string &out = StringCache::getInstance().get();
    wide2String(eventProperties[name], out);
    return out;
  }
  else {
    string &out = StringCache::getInstance().get();
    string2Wide(def, eventProperties[name]);
    out = def;
    return out;
  }
}

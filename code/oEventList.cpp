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
#include <iostream>

#include <vector>
#include <set>
#include <cassert>
#include <algorithm>
#include <limits>

#include "oEvent.h"
#include "gdioutput.h"
#include <filesystem>
#include <fstream>
#include "gdifonts.h"
#include "oDataContainer.h"
#include "metalist.h"
#include "cardsystem.h"

#include "SportIdent.h"

#include "meosexception.h"
#include "oFreeImport.h"
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
#include <ctime>

#include <chrono>
#include <random>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "Table.h"

extern Image image;

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
          oe.callBaseButtons(gdi, 1, false);
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


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

extern Image image;

//Version of database


string oEvent::getPropertyStringDecrypt(const char *name, const string &def)
{
  wchar_t bf[MAX_COMPUTERNAME_LENGTH + 1];
  DWORD len = MAX_COMPUTERNAME_LENGTH + 1;
  GetComputerName(bf, &len);
  string prop = getPropertyString(name, def);
  string prop2;
  int code = 0;
  const int s = 337;

  for (size_t j = 0; j<prop.length(); j+=2) {
    for (size_t k = 0; k<len; k++)
      code = code * 31 + bf[k];
    unsigned int b1 = ((unsigned char *)prop.c_str())[j] - 33;
    unsigned int b2 = ((unsigned char *)prop.c_str())[j+1] - 33;
    unsigned int b = b1 | (b2<<4);
    unsigned kk = abs(code) % s;
    b = (b + s - kk) % s;
    code += b%5;
    prop2.push_back((unsigned char)b);
  }
  return prop2;
}

void oEvent::setPropertyEncrypt(const char *name, const string &prop) {
  wchar_t bf[MAX_COMPUTERNAME_LENGTH + 1];
  DWORD len = MAX_COMPUTERNAME_LENGTH + 1;
  GetComputerName(bf, &len);
  string prop2;
  int code = 0;
  const int s = 337;

  for (size_t j = 0; j<prop.length(); j++) {
    for (size_t k = 0; k<len; k++)
      code = code * 31 + bf[k];
    unsigned int b = ((unsigned char *)prop.c_str())[j];
    unsigned kk = abs(code) % s;
    code += b%5;
    b = (b + kk) % s;
    unsigned b1 = (b & 0x0F) + 33;
    unsigned b2 = (b>>4) + 33;
    prop2.push_back((unsigned char)b1);
    prop2.push_back((unsigned char)b2);
  }

  setProperty(name, gdibase.widen(prop2));
}

void oEvent::setProperty(const char *name, int prop) {
  eventProperties[name]=itow(prop);
}

void oEvent::setProperty(const char *name, const wstring &prop) {
  eventProperties[name] = prop;
}

void oEvent::saveProperties(const wchar_t *file) {
  map<string, wstring>::const_iterator it;
  xmlparser xml;
  xml.openOutputT(file, false, "MeOSPreference");

  for (it = eventProperties.begin(); it != eventProperties.end(); ++it) {
    xml.write(it->first.c_str(), it->second);
  }

  xml.closeOut();
}

void oEvent::loadProperties(const wchar_t *file) {
  eventProperties.clear();
  initProperties();
  try {
    xmlparser xml;
    xml.read(file);
    xmlobject xo = xml.getObject("MeOSPreference");
    if (xo) {
      xmlList list;
      xo.getObjects(list);
      for (size_t k = 0; k<list.size(); k++) {
        eventProperties[list[k].getName()] = list[k].getWStr();
      }
    }
  }
  catch (std::exception &) {
    // Failed to read. Continue.
  }
}

bool compareClubClassTeamName(const oRunner &a, const oRunner &b)
{
  if (a.Club==b.Club) {
    if (a.getClassId(true) == b.getClassId(true)) {
      if (a.tInTeam==b.tInTeam)
        return a.tRealName<b.tRealName;
      else if (a.tInTeam) {
        if (b.tInTeam)
          return a.tInTeam->getStartNo() < b.tInTeam->getStartNo();
        else return false;
      }
      return b.tInTeam!=0;
    }
    else
      return a.getClass(true)<b.getClass(true);
  }
  else
    return a.getClub()<b.getClub();
}

void oEvent::assignCardInteractive(gdioutput& gdi, GUICALLBACK cb, SortOrder& orderRunners)
{
  gdi.fillDown();
  gdi.dropLine(1);
  gdi.addString("", 2, "Tilldelning av hyrbrickor");

  class SortUpdate : public GuiHandler {
    SortOrder& orderRunners;
    oEvent* oe;
    GUICALLBACK cb;
  public:
    SortUpdate(oEvent *oe, GUICALLBACK cb, SortOrder& orderRunners) : 
      orderRunners(orderRunners), cb(cb), oe(oe) { }

    void handle(gdioutput& gdi, BaseInfo& info, GuiEventType type) final {
      ListBoxInfo& lb = dynamic_cast<ListBoxInfo&>(info);
      orderRunners = SortOrder(lb.data);
      oe->assignCardInteractive(gdi, cb, orderRunners);
    }
    ~SortUpdate() {
    }
  };

  if (gdi.hasData("AssignCardMark")) {
    gdi.restore("AssignCardRP", false);
  }
  else {
    auto h = make_shared<SortUpdate>(this, cb, orderRunners);
    gdi.dropLine(0.5);
    gdi.addSelection("Sorting", 200, 300, nullptr, L"Sortering:").setHandler(h);

    vector<pair<wstring, size_t> > orders;
    for (auto ord : MetaList::getOrderToSymbol()) {
      if (ord.first != SortOrder::Custom && ord.first != SortOrder::ClassDefaultResult)
        orders.push_back(make_pair(lang.tl(ord.second), ord.first));
    }
    sort(orders.begin(), orders.end());
    orders.insert(orders.begin(), make_pair(lang.tl("Standard"), SortOrder::Custom));

    gdi.setItems("Sorting", orders);
    gdi.selectItemByData("Sorting", orderRunners);

    gdi.dropLine();
    gdi.setData("AssignCardMark", 1);
    gdi.setRestorePoint("AssignCardRP");
  }
  
  if (orderRunners == SortOrder::Custom) {
    Runners.sort(compareClubClassTeamName);
  }
  else {
    CurrentSortOrder = orderRunners;
    Runners.sort();
  }

  oRunnerList::iterator it;
  pClub lastClub = nullptr;
  pClass lastClass = nullptr;

  const int px4 = gdi.scaleLength(4);
  const int px450 = gdi.scaleLength(450);

  int k = 0;
  bool groupByClub = orderRunners == SortOrder::Custom || orderRunners == ClubClassStartTime;
  bool groupByClass = orderByClass(orderRunners);

  for (it = Runners.begin(); it != Runners.end(); ++it) {

    if (it->skip() || it->getCardNo() || it->isVacant() || it->needNoCard())
      continue;

    if (it->getStatus() == StatusDNS || it->getStatus() == StatusCANCEL || it->getStatus() == StatusNotCompeting)
      continue;

    if (groupByClub && it->Club != lastClub) {
      lastClub = it->Club;
      gdi.dropLine(0.5);
      gdi.addStringUT(1, it->getClub());
    }
    else if (groupByClass && it->Class != lastClass) {
      lastClass = it->getClassRef(true);
      gdi.dropLine(0.5);
      gdi.addStringUT(1, it->getClass(true));
    }

    wstring r;
    if (!groupByClass && it->Class)
      r += it->getClass(false) + L", ";
    
    if (!groupByClub && it->Club)
      r += it->getClub() + L", ";

    if (it->tInTeam) {
      if (!it->tInTeam->getBib().empty())
        r += it->tInTeam->getBib() + L" ";

      r += it->tInTeam->getName() + L", ";
    }
    else {
      if (!it->getBib().empty())
        r += it->getBib() + L" ";
    }
    r += it->getName() + L":";

    gdi.fillRight();
    gdi.pushX();
    gdi.addStringUT(0, r);
    char id[24];
    snprintf(id, sizeof(id), "*%d", k++);

    gdi.addInput(max(gdi.getCX(), px450), gdi.getCY() - px4,
      id, L"", 10, cb).setExtra(it->getId());

    gdi.popX();
    gdi.dropLine(1.6);
    gdi.fillDown();
  }

  if (k == 0)
    gdi.addString("", 0, "Ingen löpare saknar bricka");

  gdi.refresh();
}

void oEvent::calcUseStartSeconds()
{
  tUseStartSeconds = false;
  oRunnerList::iterator it;
  for (it = Runners.begin(); it != Runners.end(); ++it) {
    if (it->getStartTime() > 0 &&
      (it->getStartTime() + ZeroTime) % timeConstMinute != 0) {
      tUseStartSeconds = true;
      return;
    }
  }
}

const wstring &oEvent::formatStatus(RunnerStatus status, bool forPrint)
{
  const static wstring stats[12] = { L"?", L"Godkänd", L"Ej start", L"Felst.", L"Utg.", L"Disk.",
                                 L"Maxtid", L"Deltar ej", L"Återbud[status]", L"Utom tävlan",
                                 L"Utan tidtagning", L"\u2014" };
  switch (status) {
  case StatusOK:
    return lang.tl(stats[1]);
  case StatusDNS:
    return lang.tl(stats[2]);
  case StatusCANCEL:
    return lang.tl(stats[8]);
  case StatusMP:
    return lang.tl(stats[3]);
  case StatusDNF:
    return lang.tl(stats[4]);
  case StatusDQ:
    return lang.tl(stats[5]);
  case StatusMAX:
    return lang.tl(stats[6]);
  case StatusNotCompeting:
    if (forPrint)
      return stats[11];
    else
      return lang.tl(stats[7]);
  case StatusOutOfCompetition:
    return lang.tl(stats[9]);
  case StatusUnknown: {
    if (forPrint)
      return formatTime(-1);
    else
      return stats[0];
  }
  case StatusNoTiming: {
    if (forPrint)
      return lang.tl(stats[1]);
    else
      return lang.tl(stats[10]);
  }
  default:
    return stats[0];
  }
}

#ifndef MEOSDB

void oEvent::analyzeClassResultStatus() const
{
  map<int, ClassResultInfo> res;
  for (oRunnerList::const_iterator it = Runners.begin(); it != Runners.end(); ++it) {
    if (it->isRemoved() || !it->Class)
      continue;

    int id = it->Class->Id * 31 + it->tLeg;
    ClassResultInfo &cri = res[id];

    if (it->getStatus() == StatusUnknown) {
      cri.nUnknown++;
      if (it->tStartTime > 0) {
        if (!it->isVacant()) {
          if (cri.lastStartTime>=0)
            cri.lastStartTime = max(cri.lastStartTime, it->tStartTime);
        }
      }
      else
        cri.lastStartTime = -1; // Cannot determine
    }
    else
      cri.nFinished++;

  }

  for (oClassList::const_iterator it = Classes.begin(); it != Classes.end(); ++it) {
    if (it->isRemoved())
      continue;

    if (!it->legInfo.empty()) {
      it->tResultInfo.resize(it->legInfo.size());
      for (size_t k = 0; k<it->legInfo.size(); k++) {
        int id = it->Id * 31 + k;
        it->tResultInfo[k] = res[id];
      }
    }
    else {
      it->tResultInfo.resize(1);
      it->tResultInfo[0] = res[it->Id * 31];
    }
  }
}

void oEvent::generateTestCard(SICard &sic) const {
  sic.clear(0);
  sic.convertedTime = ConvertedTimeStatus::Hour24;

  if (Runners.empty())
    return;

  unsigned seed = (unsigned)std::chrono::system_clock::now().time_since_epoch().count();
  auto rnd = std::default_random_engine(seed);
  std::uniform_int_distribution distr(0, 10000);

  analyzeClassResultStatus();

  oRunnerList::const_iterator it;


  vector<pRunner> candidates;
  it = Runners.begin();

  oRunner *r = nullptr;
  oRunner *parRunner = nullptr;
  int cardNo = 0;
  for (auto &it : Runners) {
    if (it.Card)
      continue;
    cardNo = it.getCardNo();

    if (it.Class && it.tLeg > 0) {
      StartTypes st = it.Class->getStartType(it.tLeg);
      if (st == STPursuit) {
        if (it.Class->tResultInfo[it.tLeg - 1].nUnknown > 0)
          cardNo = 0; // Wait with this leg
      }
    }

    // Make sure teams start in right order
    if (it.tInTeam && it.tLeg > 0) {
      if (it.Class) {
        StartTypes st = it.Class->getStartType(it.tLeg);
        if (st != STDrawn && st != STTime) {
          pRunner prev = it.tInTeam->Runners[it.tLeg - 1];
          if (prev && prev->getStatus() == StatusUnknown)
            cardNo = 0; // Wait with this runner
        }
      }
    }

    if (cardNo) {

      if (it.tInTeam && it.tLeg > 0 && (it.Class->getLegType(it.tLeg) == LegTypes::LTParallel
                                        || it.Class->getLegType(it.tLeg) == LegTypes::LTParallelOptional)) {
        pRunner prev = it.tInTeam->Runners[it.tLeg - 1];
        if (prev && prev->hasResult()) {
          candidates.clear();
          parRunner = prev;
          candidates.push_back(pRunner(&it));
          break;
        }
      }

      // For team runners, we require start time to get right order
      if (!it.tInTeam || it.tLeg == 0 || (it.tInTeam->getRunner(it.tLeg-1) && it.tInTeam->getRunner(it.tLeg - 1)->hasResult()))
        candidates.push_back(pRunner(&it));
    };
  }

  if (candidates.size() > 0)
    r = candidates[rnd() % candidates.size()];

  if (r) {
    r->synchronize();
    pCourse pc = r->getCourse(false);

    if (!pc) {
      pClass cls = r->Class;
      if (cls) {
        pc = const_cast<oEvent *>(this)->generateTestCourse(distr(rnd) % 15 + 7);
        pc->synchronize();
        cls->setCourse(pc);
        cls->synchronize();
      }
    }

    if (pc) {
      sic.CardNumber = r->getCardNo();

      if (distr(rnd) % 100 == 3)
        sic.CardNumber = 100000 + distr(rnd) % 99999;

      int s = r->tStartTime > 0 ? r->tStartTime + ZeroTime : ZeroTime + timeConstHour + distr(rnd) % (timeConstHour * 3);
      int tomiss = distr(rnd) % (60 * 10);
      if (tomiss > 60 * 9)
        tomiss = distr(rnd) % (30 * 10);
      else if (distr(rnd) % 20 == 3)
        tomiss *= distr(rnd) % 3;


      vector<int> rgain(pc->nControls());
      for (int i = 0; i < rgain.size(); i++)
        rgain[i] = i;

      int f; 
      if (pc->getMaximumRogainingTime() > 0) {
        int rt = pc->getMaximumRogainingTime();
        rt = max(rt / 2, rt + (distr(rnd) % 15 - 13) * timeConstSecond);
        f = s + rt;

        shuffle(rgain.begin(), rgain.end(), rnd);
        int nNot = (distr(rnd) % rgain.size())/3;
        for (int i = 0; i < nNot; i++)
          rgain[i] = -1;
      }
      else {
        f = sic.FinishPunch.Time = s + ((30 + pc->getLength() / 200) * 60 + tomiss) * timeUnitsPerSecond;
      }

      if (parRunner) {
        s = parRunner->getStartTime() + ZeroTime;
        f = s + parRunner->getRunningTime(false) + (distr(rnd) % 8 - 4) * timeConstSecond;
      }

      sic.StartPunch.Time = s;
      sic.FinishPunch.Time = f;

      if (distr(rnd) % 80 == 31 || r->tStartTime > 0)
        sic.StartPunch.Code = -1;

      if (distr(rnd) % 250 == 31)
        sic.FinishPunch.Code = -1;

      sic.nPunch = 0;
      double dt = 1. / double(pc->nControls() + 1);
      int missed = 0;

      for (int k = 0; k <= pc->nControls() + 1; k++) {
        if (parRunner && parRunner->getCard()) {
          auto p = parRunner->getCard()->getPunchByIndex(k);
          if (p && p->getTypeCode() > 30) {
            sic.Punch[sic.nPunch].Code = p->getTypeCode();
            sic.Punch[sic.nPunch].Time = s + p->getTimeInt() - parRunner->getStartTime() + (distr(rnd) % 8 - 4) * timeConstSecond;
            sic.nPunch++;
          }
        }
        else if (k < pc->nControls() && distr(rnd) % 930 != 50 && rgain[k] >= 0) {
          sic.Punch[sic.nPunch].Code = pc->getControl(rgain[k])->getFirstNumber();
          double cc = (k + 1) * dt;

          if (missed < tomiss) {
            int left = pc->nControls() - k;
            if (distr(rnd) % left == 1)
              missed += ((tomiss - missed) * (distr(rnd) % 4 + 1)) / 6;
            else if (left == 1)
              missed = tomiss;
          }

          sic.Punch[sic.nPunch].Time = int(0.1 * ((f - tomiss * timeUnitsPerSecond) * cc + s * (1. - cc) + missed * timeUnitsPerSecond)) * 10;
          sic.nPunch++;
        }
      }
    }
  }
}

pCourse oEvent::generateTestCourse(int nCtrl)
{
  wchar_t bf[64];
  static int sk=0;
  swprintf(bf, sizeof(bf)/sizeof(wchar_t), lang.tl("Bana %d").c_str(), ++sk);
  pCourse pc=addCourse(bf, 4000+(rand()%1000)*10);

  int i=0;
  for (;i<nCtrl/3;i++)
    pc->addControl(rand()%(99-32)+32);

  i++;
  pc->addControl(50)->setName(L"Radio 1");

  for (;i<(2*nCtrl)/3;i++)
    pc->addControl(rand()%(99-32)+32);

  i++;
  pc->addControl(150)->setName(L"Radio 2");

  for (;i<nCtrl-1;i++)
    pc->addControl(rand()%(99-32)+32);
  pc->addControl(100)->setName(L"Förvarning");

  return pc;
}


pClass oEvent::generateTestClass(int nlegs, int nrunners,
                                 wchar_t *name, const wstring &start)
{
  pClass cls=addClass(name);

  if (nlegs==1 && nrunners==1) {
    int nCtrl=rand()%15+5;
    if (rand()%10==1)
      nCtrl+=rand()%40;
    cls->setCourse(generateTestCourse(nCtrl));
  }
  else if (nlegs==1 && nrunners==2) {
    setupRelay(*cls, PPatrol, 2, start);
    int nCtrl=rand()%15+10;
    pCourse pc=generateTestCourse(nCtrl);
    cls->addStageCourse(0, pc->getId(), -1);
    cls->addStageCourse(1, pc->getId(), -1);
  }
  else if (nlegs>1 && nrunners==2) {
    setupRelay(*cls, PTwinRelay, nlegs, start);
    int nCtrl=rand()%8+10;
    int cid[64];
    for (int k=0;k<nlegs;k++)
      cid[k]=generateTestCourse(nCtrl)->getId();

    for (int k=0;k<nlegs;k++)
      for (int j=0;j<nlegs;j++)
        cls->addStageCourse(k, cid[(k+j)%nlegs], -1);
  }
  else if (nlegs>1 && nrunners==nlegs) {
    setupRelay(*cls, PRelay, nlegs, start);
    int nCtrl=rand()%8+10;
    int cid[64];
    for (int k=0;k<nlegs;k++)
      cid[k]=generateTestCourse(nCtrl)->getId();

    for (int k=0;k<nlegs;k++)
      for (int j=0;j<nlegs;j++)
        cls->addStageCourse(k, cid[(k+j)%nlegs], -1);
  }
  else if (nlegs>1 && nrunners==1) {
    setupRelay(*cls, PHunting, 2, start);
    cls->addStageCourse(0, generateTestCourse(rand()%8+10)->getId(), -1);
    cls->addStageCourse(1, generateTestCourse(rand()%8+10)->getId(), -1);
  }
  return cls;
}


void oEvent::generateTestCompetition(int nClasses, int nRunners,
                                     bool generateTeams) {
  if (nClasses > 0) {
    oe->newCompetition(L"!TESTTÄVLING");
    oe->loadDefaults();
    oe->setZeroTime(L"05:00:00", true);
    oe->getMeOSFeatures().useAll(*oe);
  }
  vector<wstring> gname;
  //gname.reserve(RunnerDatabase.size());
  vector<wstring> fname;
  //fname.reserve(RunnerDatabase.size());

  runnerDB->getAllNames(gname, fname);

  if (fname.empty())
    fname.push_back(L"Foo");

  if (gname.empty())
    gname.push_back(L"Bar");

/*  oRunnerList::iterator it;
  for(it=RunnerDatabase.begin(); it!=RunnerDatabase.end(); ++it){
    if (!it->getGivenName().empty())
      gname.push_back(it->getGivenName());

    if (!it->getFamilyName().empty())
      fname.push_back(it->getFamilyName());
  }
*/
  int nClubs=30;
  wchar_t bfw[128];
  
  int startno=1;
  const vector<oDBClubEntry> &oc = runnerDB->getClubDB(false);
  for(int k=0;k<nClubs;k++) {
    if (oc.empty()) {
      swprintf(bfw, sizeof(bfw)/sizeof(wchar_t), L"Klubb %d", k);
      addClub(bfw, k+1);
    }
    else {
      addClub(oc[(k*13)%oc.size()].getName(), k+1);
    }
  }

  int now=getRelativeTime(getCurrentTimeS());
  wstring start=getAbsTime(now+60*3-(now%60));

  for (int k=0;k<nClasses;k++) {
    pClass cls=0;

    if (!generateTeams) {
      int age=0;
      if (k<7)
        age=k+10;
      else if (k==7)
        age=18;
      else if (k==8)
        age=20;
      else if (k==9)
        age=21;
      else
        age=30+(k-9)*5;

      swprintf(bfw, sizeof(bfw)/sizeof(wchar_t), L"HD %d", age);
      cls=generateTestClass(1,1, bfw, L"");
    }
    else {
      swprintf(bfw, sizeof(bfw)/sizeof(wchar_t), L"Klass %d", k);
      int nleg=k%5+1;
      int nrunner=k%3+1;
      nrunner = nrunner == 3 ? nleg:nrunner;

      nleg=3;
      nrunner=3;
      cls=generateTestClass(nleg, nrunner, bfw, start);
    }
  }

  nClasses = Classes.size();
  int k = 0;

  for (oClassList::iterator it = Classes.begin(); it != Classes.end(); ++it, ++k) {
    pClass cls = &*it;
    int classesLeft=(nClasses-k);
    int nRInClass=nRunners/classesLeft;

    if (classesLeft>2 && nRInClass>3)
      nRInClass+=int(nRInClass*0.7)-rand()%int(nRInClass*1.5);

    if (cls->getNumDistinctRunners()==1) {
      for (int i=0;i<nRInClass;i++) {
        pRunner r=addRunner(gname[rand()%gname.size()]+L" "+fname[rand()%fname.size()],
          rand()%nClubs+1, cls->getId(), 0, L"", true);

        r->setStartNo(startno++, ChangeType::Update);
        r->setCardNo(500001+Runners.size()*97+rand()%97, false);
        r->apply(ChangeType::Update, nullptr);
      }
      nRunners-=nRInClass;
      if (k%5!=5) {
        vector<ClassDrawSpecification> spec;
        spec.emplace_back(cls->getId(), 0, getRelativeTime(start), 10, 3, VacantPosition::Mixed);
        drawList(spec, DrawMethod::MeOS, 1, oEvent::DrawType::DrawAll);
      }
      else
        cls->Name += L" Öppen";
    }
    else {
      int dr=cls->getNumDistinctRunners();
      for (int i=0;i<nRInClass;i++) {
        pTeam t=addTeam(L"Lag " + fname[rand()%fname.size()], rand()%nClubs+1, cls->getId());
        t->setStartNo(startno++, ChangeType::Update);

        for (int j=0;j<dr;j++) {
          pRunner r=addRunner(gname[rand()%gname.size()]+L" "+fname[rand()%fname.size()], 0, 0, 0, L"", true);
          r->setCardNo(500001+Runners.size()*97+rand()%97, false);
          t->setRunner(j, r, false);
        }
      }
      nRunners-=nRInClass;

      if ( cls->getStartType(0)==STDrawn ) {
        vector<ClassDrawSpecification> spec;
        spec.emplace_back(cls->getId(), 0, getRelativeTime(start), 20, 3, VacantPosition::Mixed);
        drawList(spec, DrawMethod::MeOS, 1, DrawType::DrawAll);
      }
    }
  }
}

#endif

void oEvent::getFreeImporter(oFreeImport &fi)
{
  if (!fi.isLoaded())
    fi.load();

  fi.init(Runners, Clubs, Classes);
}


void oEvent::fillFees(gdioutput &gdi, const string &name, bool onlyDirect, bool withAuto) const {
  gdi.clearList(name);

  set<int> fees;

  int f;
  for (oClassList::const_iterator it = Classes.begin(); it != Classes.end(); ++it) {
    if (it->isRemoved())
      continue;
    if (onlyDirect && !it->getAllowQuickEntry())
      continue;

    f = it->getDCI().getInt("ClassFee");
    if (f > 0)
      fees.insert(f);

    f = it->getDCI().getInt("ClassFeeRed");
    if (f > 0)
      fees.insert(f);

    f = it->getDCI().getInt("SecondHighClassFee");
    if (f > 0)
      fees.insert(f);

    if (withAuto) {
      f = it->getDCI().getInt("HighClassFee");
      if (f > 0)
        fees.insert(f);

      f = it->getDCI().getInt("HighClassFeeRed");
      if (f > 0)
        fees.insert(f);

      f = it->getDCI().getInt("SecondHighClassFeeRed");
      if (f > 0)
        fees.insert(f);
    }
  }
  
  if (fees.empty()) {
    if (!onlyDirect) {
      f = getDCI().getInt("EliteFee");
      if (f > 0)
        fees.insert(f);
    }

    f = getDCI().getInt("EntryFee");
    if (f > 0)
      fees.insert(f);

    f = getDCI().getInt("YouthFee");
    if (f > 0)
      fees.insert(f);
  }
  vector<pair<wstring, size_t>> ff;
  if (withAuto)
    ff.push_back(make_pair(lang.tl(L"Från klassen"), -1));
  for (set<int>::iterator it = fees.begin(); it != fees.end(); ++it)
    ff.push_back(make_pair(formatCurrency(*it), *it));

  gdi.setItems(name, ff);
}

void oEvent::fillLegNumbers(const set<int> &cls,
                            bool isTeamList, 
                            bool includeSubLegs, 
                            vector< pair<wstring, size_t> > &out) {
  oClassList::iterator it;
  synchronizeList(oListId::oLClassId);

  out.clear();
  set< pair<int, int> > legs;

  for (it=Classes.begin(); it != Classes.end(); ++it) {
    if (!it->Removed && (cls.empty() || cls.count(it->getId()))) {
      if (it->getNumStages() == 0)
        continue;

      for (size_t j = 0; j < it->getNumStages(); j++) {
        int number, order;
        if (it->splitLegNumberParallel(j, number, order)) {
          if (order == 0)
            legs.insert( make_pair(number, 0) );
          else {
            if (it->isOptional(j))
              continue;

            if (order == 1)
              legs.insert( make_pair(number, 1000));
            legs.insert(make_pair(number, 1000+order));
          }
        }
        else {
          legs.insert( make_pair(number, 0) );
        }
      }
    }
  }

  out.reserve(legs.size() + 1);
  for (set< pair<int, int> >::const_iterator it = legs.begin(); it != legs.end(); ++it) {
    if (it->second == 0) {
      out.push_back( make_pair(lang.tl("Sträcka X#" + itos(it->first + 1)), it->first));
    }
  }
  if (includeSubLegs) {
    for (set< pair<int, int> >::const_iterator it = legs.begin(); it != legs.end(); ++it) {
      if (it->second >= 1000) {
        int leg = it->first;
        int sub = it->second - 1000;
        char bf[64];
        char symb = 'a' + sub;
        snprintf(bf, sizeof(bf), "Sträcka X#%d%c", leg+1, symb);
        out.push_back( make_pair(lang.tl(bf), (leg + 1) * 10000 + sub));
      }
    }
  }
  
  if (isTeamList)
    out.push_back(make_pair(lang.tl("Sista sträckan"), 1000));
  else
    out.push_back(make_pair(lang.tl("Alla sträckor"), 1000));
}

void oEvent::generateTableData(const string &tname, Table &table, TableUpdateInfo &tui)
{
  if (tname == "runners") {
    if (tui.doRefresh && !tui.doAdd)
      return;
    pRunner r = pRunner(tui.object);
    if (tui.doAdd) {
      r = addRunner(getAutoRunnerName(), 0, 0, 0, L"", false);
      r->setFlag(oRunner::TransferFlags::FlagUnnamed, true);
    }
    generateRunnerTableData(table, r);
    return;
  }
  else if (tname == "classes") {
    if (tui.doRefresh && !tui.doAdd)
      return;
    pClass c = tui.doAdd ? addClass(getAutoClassName()) : pClass(tui.object);
    generateClassTableData(table, c);
    return;
  }
  else if (tname == "clubs") {
    if (tui.doRefresh && !tui.doAdd)
      return;
    pClub c = tui.doAdd ? addClub(L"Club", 0) : pClub(tui.object);
    generateClubTableData(table, c);
    return;
  }
  else if (tname == "teams") {
    if (tui.doRefresh && !tui.doAdd)
      return;
    pTeam t = tui.doAdd ? addTeam(getAutoTeamName()) : pTeam(tui.object);
    generateTeamTableData(table, t);
    return;
  }
  else if (tname == "cards") {
    if (tui.doRefresh && !tui.doAdd)
      return;
    generateCardTableData(table, pCard(tui.object));
    return;
  }
  else if (tname == "controls") {
    if (tui.doRefresh && !tui.doAdd)
      return;
    generateControlTableData(table, pControl(tui.object));
    return;
  }
  else if (tname == "punches") {
    if (tui.doRefresh && !tui.doAdd)
      return;

    pFreePunch c = tui.doAdd ? addFreePunch(0,0,0,0, false, false) : pFreePunch(tui.object);
    generatePunchTableData(table, c);
    return;
  }
  else if (tname == "courses") {
    if (tui.doRefresh && !tui.doAdd)
      return;

    pCourse c = tui.doAdd ? addCourse(getAutoCourseName()) : pCourse(tui.object);
    oCourse::generateTableData(oe, table, c);
    return;
  }
  else if (tname == "runnerdb") {
    if (tui.doAdd || !tui.doRefresh) {
      oDBRunnerEntry *entry = tui.doAdd ? getRunnerDatabase().addRunner() : (oDBRunnerEntry *)(tui.object);
      getRunnerDatabase().generateRunnerTableData(table, entry);
    }

    if (tui.doRefresh)
      getRunnerDatabase().refreshRunnerTableData(table);

    return;
  }
  else if (tname == "clubdb") {
    if (tui.doAdd || !tui.doRefresh) {
      pClub c = tui.doAdd ? getRunnerDatabase().addClub() : pClub(tui.object);
      getRunnerDatabase().generateClubTableData(table, c);
    }

    if (tui.doRefresh) {
      getRunnerDatabase().refreshClubTableData(table);
    }
    return;
  }
  throw std::exception("Wrong table name");
}

void oEvent::applyEventFees(bool updateClassFromEvent,
                            bool updateFees, bool updateCardFees,
                            const set<int> &classFilter) {
  synchronizeList({ oListId::oLClassId, oListId::oLRunnerId });
  bool allClass = classFilter.empty();

  if (updateClassFromEvent) {
    for (oClassList::iterator it = Classes.begin(); it != Classes.end(); ++it) {
      if (it->isRemoved())
        continue;
      if (allClass || classFilter.count(it->getId())) {
        it->addClassDefaultFee(true);
        it->synchronize(true);
      }
    }
  }

  if (updateFees) {
    for (oRunnerList::iterator it = Runners.begin(); it != Runners.end(); ++it) {
      if (it->skip())
        continue;

      if (allClass || classFilter.count(it->getClassId(true))) {
        it->addClassDefaultFee(true);
        it->synchronize(true);
      }
    }
  }

  if (updateCardFees) {
    int cf = getDCI().getInt("CardFee");

    for (oRunnerList::iterator it = Runners.begin(); it != Runners.end(); ++it) {
      if (it->skip())
        continue;

      if (it->getDI().getInt("CardFee") != 0) {
        it->getDI().setInt("CardFee", cf);
        it->synchronize(true);
      }
    }
  }
}

#ifndef MEOSDB
void hideTabs();
void createTabs(bool force, bool onlyMain, bool skipTeam, bool skipSpeaker,
                bool skipEconomy, bool skipLists, bool skipRunners, bool skipCourses, bool skipControls);

void oEvent::updateTabs(bool force, bool hide) const
{
  bool hasTeam = !Teams.empty();

  for (oClassList::const_iterator it = Classes.begin();
                  !hasTeam && it!=Classes.end(); ++it) {
    if (it->getNumStages()>1)
      hasTeam = true;
  }

  bool hasRunner = !Runners.empty() || !Classes.empty();
  bool hasLists = !empty();
  bool skipCourses = getMeOSFeatures().withoutCourses(*this);
  if (hide || isKiosk())
    hideTabs();
  else
    createTabs(force, empty(), !hasTeam, !getMeOSFeatures().hasFeature(MeOSFeatures::Speaker),
               !(getMeOSFeatures().hasFeature(MeOSFeatures::Economy)
               || getMeOSFeatures().hasFeature(MeOSFeatures::EditClub)),
               !hasLists, !hasRunner, skipCourses, Controls.empty() && !skipCourses);
}

#else
void oEvent::updateTabs(bool force) const
{
}
#endif

bool oEvent::useRunnerDb() const {
  return getMeOSFeatures().hasFeature(MeOSFeatures::RunnerDb);
}

int oEvent::getBaseCardFee() const {
  int baseCardFee = oe->getDI().getInt("CardFee");
  if (baseCardFee == 0)
    baseCardFee = -1;
  return baseCardFee;
}

bool oEvent::hasMultiRunner() const {
  for (oClassList::const_iterator it = Classes.begin(); it!=Classes.end(); ++it) {
    if (it->hasMultiCourse() && it->getNumDistinctRunners() != it->getNumStages())
      return true;
  }

  return false;
}

/** Return false if card is not used */
bool oEvent::checkCardUsed(gdioutput &gdi, oRunner &runnerToAssignCard, int cardNo) {

  if (deprecateOldCards() && oe->getCardSystem().isDeprecated(cardNo)) {
    gdi.alert(L"Brickan är av äldre typ och kan inte användas.");
    return true;
  }

  pRunner pold = 0;
  if (cardNo != 0) {
    vector<pRunner> allR;
    getRunnersByCardNo(cardNo, true, CardLookupProperty::OnlyMainInstance, allR);
    for (pRunner it : allR) {
      if (!runnerToAssignCard.canShareCard(it, cardNo)) {
        pold = &*it;
        break;
      }
    }
  }
  wchar_t bf[1024];

  if (pold) {
    swprintf(bf, sizeof(bf)/sizeof(wchar_t), (L"#" + lang.tl("Bricka %d används redan av %s och kan inte tilldelas.")).c_str(),
                  cardNo, pold->getCompleteIdentification(oRunner::IDType::OnlyThis).c_str());
    gdi.alert(bf);
    return true;
  }
  return false;
}

void oEvent::removeVacanies(int classId) {
  oRunnerList::iterator it;
  vector<int> toRemove;

  for (it=Runners.begin(); it != Runners.end(); ++it) {
    if (it->skip() || !it->isVacant())
      continue;

    if (classId!=0 && it->getClassId(false)!=classId)
      continue;

    if (!isRunnerUsed(it->Id))
      toRemove.push_back(it->Id);
  }

  removeRunner(toRemove);
}

void oEvent::sanityCheck(gdioutput &gdi, bool expectResult, int onlyThisClass) {
  bool hasResult = false;
  bool warnNoName = false;
  bool warnNoClass = false;
  bool warnNoTeam = false;
  bool warnNoPatrol = false;
  bool warnIndividualTeam = false;

  for (oRunnerList::iterator it = Runners.begin(); it!=Runners.end(); ++it) {
    if (it->isRemoved())
      continue;
    if (onlyThisClass > 0 && it->getClassId(false) != onlyThisClass)
      continue;
    if (it->sName.empty()) {
      if (!warnNoName) {
        warnNoName = true;
        gdi.alert("Varning: deltagare med blankt namn påträffad. MeOS "
                  "kräver att alla deltagare har ett namn, och tilldelar namnet 'N.N.'");
      }
      it->setName(lang.tl("N.N."), false);
      it->synchronize();
    }

    if (!it->Class) {
      if (!warnNoClass) {
        gdi.alert(L"Deltagaren 'X' saknar klass.#" + it->getName());
        warnNoClass = true;
      }
      continue;
    }

    if (!it->tInTeam) {
      ClassType type = it->Class->getClassType();
      int cid = it->Class->getId();
      if (type == oClassIndividRelay) {
        it->setClassId(0, true);
        it->setClassId(cid, true);
        it->synchronizeAll();
      }
      else if (type == oClassRelay) {
        if (!warnNoTeam) {
          gdi.alert(L"Deltagaren 'X' deltar i stafettklassen 'Y' men saknar lag. Klassens start- "
                    L"och resultatlistor kan därmed bli felaktiga.#" + it->getName() +
                     L"#" + it->getClass(false));
          warnNoTeam = true;
        }
      }
      else if (type == oClassPatrol) {
        if (!warnNoPatrol) {
          gdi.alert(L"Deltagaren 'X' deltar i patrullklassen 'Y' men saknar patrull. Klassens start- "
                    L"och resultatlistor kan därmed bli felaktiga.#" + it->getName() +
                     + L"#" + it->getClass(false));
          warnNoPatrol = true;
        }
      }
    }

    if (it->getFinishTime()>0)
      hasResult = true;
  }

  for (oTeamList::iterator it = Teams.begin(); it != Teams.end(); ++it) {
    if (it->isRemoved())
      continue;
    
    if (onlyThisClass > 0 && it->getClassId(false) != onlyThisClass)
      continue;

    if (it->sName.empty()) {
      if (!warnNoName) {
        warnNoName = true;
        gdi.alert("Varning: lag utan namn påträffat. "
                  "MeOS kräver att alla lag har ett namn, och tilldelar namnet 'N.N.'");
      }
      it->setName(lang.tl("N.N."), false);
      it->synchronize();
    }

    if (!it->Class) {
      if (!warnNoClass) {
        gdi.alert(L"Laget 'X' saknar klass.#" + it->getName());
        warnNoClass = true;
      }
      continue;
    }

    ClassType type = it->Class->getClassType();
    if (type == oClassIndividual) {
      if (!warnIndividualTeam) {
        gdi.alert(L"Laget 'X' deltar i individuella klassen 'Y'. Klassens start- och resultatlistor "
                  L"kan därmed bli felaktiga.#" + it->getName() + L"#" + it->getClass(true));
        warnIndividualTeam = true;
      }
    }
  }


  if (expectResult && !hasResult)
    gdi.alert("Tävlingen innehåller inga resultat.");


  bool warnBadStart = false;

  for (oClassList::iterator it = Classes.begin(); it != Classes.end(); ++it) {
    if (it->isRemoved())
      continue;
    if (it->getClassStatus() != oClass::ClassStatus::Normal)
      continue;

    if (onlyThisClass > 0 && it->getId() != onlyThisClass)
      continue;

    if (it->getQualificationFinal())
      continue;

    if (it->hasMultiCourse()) {
      for (unsigned k=0;k<it->getNumStages(); k++) {
        StartTypes st = it->getStartType(k);
        LegTypes lt = it->getLegType(k);
        if (k==0 && (st == STChange || st == STPursuit) && !warnBadStart) {
          warnBadStart = true;
          gdi.alert(L"Klassen 'X' har jaktstart/växling på första sträckan.#" + it->getName());
        }
        if (st == STTime && it->getStartData(k)<=0 && !warnBadStart &&
              (lt == LTNormal || lt == LTSum)) {
          warnBadStart = true;
          gdi.alert(L"Ogiltig starttid i 'X' på sträcka Y.#" + it->getName() + L"#" + itow(k+1));
        }
      }
    }
  }
}

oTimeLine::~oTimeLine() {

}

void oEvent::remove() {
  if (isClient())
   dropDatabase();
  else
   deleteCompetition();

  clearListedCmp();
  newCompetition(L"");
}

bool oEvent::canRemove() const {
  return true;
}

wstring oEvent::formatCurrency(int c, bool includeSymbol) const {
  if (tCurrencyFactor == 1)
    if (!includeSymbol)
      return itow(c);
    else if (tCurrencyPreSymbol)
      return tCurrencySymbol + itow(c);
    else
      return itow(c) + tCurrencySymbol;
  else {
    wchar_t bf[32];
    if (includeSymbol) {
      swprintf(bf, 32, L"%d%s%02d", c/tCurrencyFactor,
                 tCurrencySeparator.c_str(), c%tCurrencyFactor);

      if (tCurrencyPreSymbol)
        return tCurrencySymbol + bf;
      else
        return bf + tCurrencySymbol;
    }
    else {
      swprintf(bf, 32, L"%d.%02d", c/tCurrencyFactor, c%tCurrencyFactor);
      return bf;
    }
  }
}

int oEvent::interpretCurrency(const wstring &c) const {
  if (tCurrencyFactor == 1 && tCurrencyPreSymbol == false)
    return wtoi(c.c_str());

  size_t s = 0;
  while (s < c.length() && (c[s]<'0' || c[s]>'9'))
    s++;

  wstring cc = c.substr(s);

  for (size_t k = 0; k<cc.length(); k++) {
    if (cc[k] == ',' || cc[k] == tCurrencySeparator[0])
      cc[k] = '.';
  }

  return int(_wtof(cc.c_str())*tCurrencyFactor);
}

int oEvent::interpretCurrency(double val, const wstring &cur)  {
  if (_wcsicmp(L"sek", cur.c_str()) == 0)
    setCurrency(1, L"kr", L",", false);
  else if (_wcsicmp(L"eur", cur.c_str()) == 0)
    setCurrency(100, L"€", L".", false);//WCS

  return int(floor(val * tCurrencyFactor+0.5));
}

void oEvent::setCurrency(int factor, const wstring &symbol, const wstring &separator, bool preSymbol) {
  if (factor == -1) {
    // Load from data
    int cf = getDCI().getInt("CurrencyFactor");
    if (cf != 0)
      tCurrencyFactor = cf;

    wstring cs = getDCI().getString("CurrencySymbol");
    if (!cs.empty())
      tCurrencySymbol = cs;

    cs = getDCI().getString("CurrencySeparator");
    if (!cs.empty())
      tCurrencySeparator = cs;

    int ps = getDCI().getInt("CurrencyPreSymbol");
    tCurrencyPreSymbol = (ps != 0);

    if (tCurrencySymbol.size() > 0) {
      if (tCurrencyPreSymbol) {
        wchar_t end = *tCurrencySymbol.rbegin();
        if ((end>='a' && end <='z') || end>='A' && end <='Z')
          tCurrencySymbol += L" ";
      }
      else {
        wchar_t end = *tCurrencySymbol.begin();
        if ((end>='a' && end <='z') || end>='A' && end <='Z')
          tCurrencySymbol = L" " + tCurrencySymbol;
      }
    }
  }
  else {
    tCurrencyFactor = factor;
    tCurrencySymbol = symbol;
    tCurrencySeparator = separator;
    tCurrencyPreSymbol = preSymbol;
    getDI().setString("CurrencySymbol", symbol);
    getDI().setInt("CurrencyFactor", factor);
    getDI().setString("CurrencySeparator", separator);
    getDI().setInt("CurrencyPreSymbol", preSymbol ? 1 : 0);
  }
}

MetaListContainer &oEvent::getListContainer() const {
  if (!listContainer)
    throw std::exception("Nullpointer exception");
  return *listContainer;
}

void oEvent::updateListReferences(const string& oldId, const string& newId) {
  wstring oldIdW = gdioutput::widen(oldId);
  wstring newIdW = gdioutput::widen(newId);

  if (getDI().getString("SplitPrint") == oldIdW) {
    if (getDI().setString("SplitPrint", newIdW))
      synchronize();
  }

  for (auto& c : Classes) {
    if (!c.isRemoved()) {
      if (c.getDI().getString("SplitPrint") == oldIdW) {
        if (c.getDI().setString("SplitPrint", newIdW))
          c.synchronize();
      }
    }
  }
}

void oEvent::setExtraLines(const char *attrib, const vector< pair<wstring, int> > &lines) {
  wstring str;

  for(size_t k = 0; k < lines.size(); k++) {
    if (k>0)
      str.push_back('|');

    wstring msg = lines[k].first;
    for (size_t i = 0; i < msg.size(); i++) {
      if (msg[i] == '|')
        str.push_back(':'); // Encoding does not support |
      else
        str.push_back(msg[i]);
    }
    str.push_back('|');
    str.append(itow(lines[k].second));
  }
  getDI().setString(attrib, str);
}

void oEvent::getExtraLines(const char *attrib, vector< pair<wstring, int> > &lines) const {
  vector<wstring> splt;
  const wstring &splitPrintExtra = getDCI().getString(attrib);
  split(splitPrintExtra, L"|", splt);
  lines.clear();
  lines.reserve(splt.size() / 2);
  for (size_t k = 0; k + 1 < splt.size(); k+=2) {
    lines.push_back(make_pair(splt[k], wtoi(splt[k+1].c_str())));
  }

  while(!lines.empty()) {
    if (lines.back().first.length() == 0)
      lines.pop_back();
    else break;
  }
}

oEvent::MultiStageType oEvent::getMultiStageType() const {
  if (getDCI().getString("PreEvent").empty())
    return MultiStageNone;
  else
    return MultiStageSameEntry;
}

bool oEvent::hasNextStage() const {
  return !getDCI().getString("PostEvent").empty();
}

bool oEvent::hasPrevStage() const {
  return !getDCI().getString("PreEvent").empty() || getStageNumber() > 1;
}

int oEvent::getNumStages() const {
  int ns = getDCI().getInt("NumStages");
  if (ns>0)
    return ns;
  else
    return 1;
}

void oEvent::setNumStages(int numStages) {
  getDI().setInt("NumStages", numStages);
}

int oEvent::getStageNumber() const {
  return getDCI().getInt("EventNumber");
}

void oEvent::setStageNumber(int num) {
  getDI().setInt("EventNumber", num);
}

oDataContainer &oEvent::getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const {
  data = (pvoid)oData;
  olddata = (pvoid)oDataOld;
  strData = const_cast<pvectorstr>(&dynamicData);
  return *oEventData;
}

void oEvent::changedObject() {
  globalModification = true;
}

void oEvent::pushDirectChange() {
  PostMessage(gdibase.getHWNDMain(), WM_USER + 4, 0, 0);
}

int oEvent::getBibClassGap() const {
  int ns = getDCI().getInt("BibGap");
  return ns;
}

void oEvent::setBibClassGap(int numStages) {
  getDI().setInt("BibGap", numStages);
}

void oEvent::checkNecessaryFeatures() {
  bool hasMultiRace = false;
  bool hasRelay = false;
  bool hasPatrol = false;
  bool hasForkedIndividual = false;

  for (oClassList::iterator it = Classes.begin(); it != Classes.end(); ++it) {
    const oClass &c = *it;
    bool multiRace = false;
    bool relay = false;
    bool patrol = false;

    for (size_t j = 0; j < c.legInfo.size(); j++) {      

      if (c.legInfo[j].duplicateRunner != -1)
        multiRace = true;

      if (j > 0 && !c.legInfo[j].isParallel() && !c.legInfo[j].isOptional()) {
        relay = true;
        patrol = false;
      }

      if (j > 0 && (c.legInfo[j].isParallel() || c.legInfo[j].isOptional()) && !relay) {
        patrol = true;
      }
    }

    hasForkedIndividual |= c.legInfo.size() == 1;
    hasMultiRace |= multiRace;
    hasRelay |= relay;
    hasPatrol |= patrol;
  }

  if (hasForkedIndividual)
    oe->getMeOSFeatures().useFeature(MeOSFeatures::ForkedIndividual, true, *this);

  if (hasRelay)
    oe->getMeOSFeatures().useFeature(MeOSFeatures::Relay, true, *this);

  if (hasPatrol)
    oe->getMeOSFeatures().useFeature(MeOSFeatures::Patrol, true, *this);

  if (hasMultiRace)
    oe->getMeOSFeatures().useFeature(MeOSFeatures::MultipleRaces, true, *this);

  oe->synchronize(true);
}

bool oEvent::useLongTimes() const {
  if (tLongTimesCached != -1)
    return tLongTimesCached != 0;

  tLongTimesCached = getDCI().getInt("LongTimes");
  return tLongTimesCached != 0;
}

void oEvent::useLongTimes(bool use) {
  tLongTimesCached = use;
  getDI().setInt("LongTimes", use ? 1 : 0);
}

bool oEvent::supportSubSeconds() const {
  return getDCI().getInt("SubSeconds") != 0;
}

void oEvent::supportSubSeconds(bool use) {
  if (cbSetSubSecondMode) cbSetSubSecondMode(use);
  getDI().setInt("SubSeconds", use ? 1 : 0);
}

void oEvent::getPayModes(vector<pair<wstring, size_t>> &modes) {
  modes.clear();
  modes.reserve(10);
  vector< pair<wstring, int> > lines;
  getExtraLines("PayModes", lines);
  
  modes.push_back(make_pair(lang.tl(L"Kontant betalning"), 0));
  map<int,int> id2ix;
  id2ix[0] = 0;
  
  for (size_t k = 0; k < lines.size(); k++) {
    int id = lines[k].second;
    if (id2ix.count(id))
      modes[id2ix[id]].first = lines[k].first;
    else {
      id2ix[id] = k;
      modes.push_back(make_pair(lines[k].first, id));
    }
  }
}

void oEvent::setPayMode(int id, const wstring &mode) {
  vector< pair<wstring, int> > lines;
  getExtraLines("PayModes", lines);
  
  if (mode.empty()) {
    // Remove
    for (size_t k = 0; k < lines.size(); k++) {
      if (lines[k].second == id) {
        bool valid = id != 0;
        for (oRunnerList::const_iterator it = Runners.begin(); 
                               valid && it != Runners.end(); ++it) {
          if (it->getPaymentMode() == id)
            valid = false;
        }
        for (oTeamList::const_iterator it = Teams.begin(); 
                               valid && it != Teams.end(); ++it) {
          if (it->getPaymentMode() == id)
            valid = false;
        }

        if (!valid)
          throw meosException("Betalningsättet behövs och kan inte tas bort.");

        lines.erase(lines.begin() + k);
        k--;
      }
    }
  }
  else {
    // Add / update
    bool done = false;
    for (size_t k = 0; k < lines.size(); k++) {
      if (lines[k].second == id) {
        lines[k].first = mode;
        done = true;
        break;
      }
    }
    if (!done) {
      lines.push_back(make_pair(mode, id));
    }
  }

  setExtraLines("PayModes", lines);
}

void oEvent::useDefaultProperties(bool useDefault) {
  if (useDefault) {
    if (savedProperties.empty())
      savedProperties.swap(eventProperties);
  }
  else {
    if (!savedProperties.empty()) {
      savedProperties.swap(eventProperties);
      savedProperties.clear();
    }
  }
}

static void checkValid(oEvent &oe, int &time, int delta, const wstring &name) {
  int srcTime = time;
  time += delta;
  if (time <= 0)
    time += 24 * timeConstHour;
  if (time > 24 * timeConstHour)
    time -= 24 * timeConstHour;
  if (time < 0 || time > 22 * timeConstHour) {
    throw meosException(L"X har en tid (Y) som inte är kompatibel med förändringen.#" + name + L"#" + oe.getAbsTime(srcTime));
  }
}

void oEvent::updateStartTimes(int delta) {
  for (int pass = 0; pass <= 1; pass++) {
    for (oClass &c : Classes) {
      if (c.isRemoved())
        continue;
      for (unsigned i = 0; i < c.getNumStages(); i++) {
        int st = c.getStartData(i);
        if (st > 0) {
          checkValid(*oe, st, delta, c.getName());
          if (pass == 1) {
            c.setStartData(i, st);
            c.synchronize(true);
          }
        }
      }
    }

    if (pass == 1)
      reEvaluateAll(set<int>(), false);

    for (oRunner &r : Runners) {
      if (r.isRemoved())
        continue;
      if (r.Class  && r.Class->getStartType(r.getLegNumber()) == STDrawn) {
        int st = r.getStartTime();
        if (st > 0) {
          checkValid(*oe, st, delta, r.getName());
          if (pass == 1) {
            bool updateStored = r.getStartTime() == r.getDCI().getInt("DrawnTime");
            r.setStartTime(st, true, ChangeType::Update, false);
            if (updateStored)
              r.storeDefaultStartTime();
            r.synchronize(true);
          }
        }
      }
      int ft = r.getFinishTime();
      if (ft > 0) {
        checkValid(*oe, ft, delta, r.getName());
        if (pass == 1) {
          r.setFinishTime(ft);
          r.synchronize(true);
        }
      }
    }

    for (oCard &c : Cards) {
      if (c.isRemoved())
        continue;
      wstring desc = L"Bricka X#" + c.getCardNoString();
      for (oPunch &p : c.punches) {
        int t = p.punchTime;
        if (t > 0) {
          if (c.getOwner() != 0)
            checkValid(*oe, t, delta, desc);
          else {
            // Skip check
            t += delta;
            if (t <= 0)
              t += 24 * timeConstHour;
          }

          if (pass == 1) {
            p.setTimeInt(t, false);
          }
        }
      }
    }

    for (oTeam &t : Teams) {
      if (t.isRemoved())
        continue;
      if (t.Class  && t.Class->getStartType(0) == STDrawn) {
        int st = t.getStartTime();
        if (st > 0) {
          checkValid(*oe, st, delta, t.getName());
          if (pass == 1) {
            t.setStartTime(st, true, ChangeType::Update, false);
            t.synchronize(true);
          }
        }
      }
      int ft = t.getFinishTime();
      if (ft > 0) {
        checkValid(*oe, ft, delta, t.getName());
        if (pass == 1) {
          t.setFinishTime(ft);
          t.synchronize(true);
        }
      }
    }

    for (oFreePunch &p : punches) {
      int t = p.punchTime;
      if (t > 0) {
        if (pass == 1) {
          t += delta;
          if (t <= 0)
            t += 24 * timeConstHour;

          p.setTimeInt(t, false); // Skip check
        }
      }
    }
  }
}

bool oEvent::hasFlag(TransferFlags flag) const {
  return (getDCI().getInt("TransferFlags") & flag) != 0;
}

void oEvent::setFlag(TransferFlags flag, bool onoff) {
  int cf = getDCI().getInt("TransferFlags");
  cf = onoff ? (cf | flag) : (cf & (~flag));
  getDI().setInt("TransferFlags", cf);
}

string oEvent::encodeStartGroups() const {
  string ss;
  string tmp;
  for (auto &sg : startGroups) {
    tmp = itos(sg.first) + "," +
      itos(sg.second.firstStart) + "," + itos(sg.second.lastStart);
    if (!sg.second.name.empty()) {
      wstring name = sg.second.name;
      for (int j = 0; j < name.length(); j++) {
        if (name[j] == L',')
          name[j] = L'|';
        if (name[j] == L';')
          name[j] = L'^';
      }
      tmp += "," + gdioutput::toUTF8(name);
    }
    if (ss.empty())
      ss = tmp;
    else
      ss += ";" + tmp;
  }
  return ss;
}

void oEvent::decodeStartGroups(const string &enc) const {
  vector<string> g, sg;
  split(enc, ";", g);
  startGroups.clear();
  for (string &grp : g) {
    split(grp, ",", sg);
    if (sg.size() == 3 || sg.size() == 4) {
      int id = atoi(sg[0].c_str());
      int start = atoi(sg[1].c_str());
      int end = atoi(sg[2].c_str());
      wstring name;
      if (sg.size() == 4) {
        name = gdioutput::fromUTF8(sg[3]);
        for (int j = 0; j < name.length(); j++) {
          if (name[j] == L'|')
            name[j] = L',';
          if (name[j] == L'^')
            name[j] = L';';
        }
      }
      startGroups.emplace(id, StartGroupInfo(name, start, end));
    }
  }
}

void oEvent::setStartGroup(int id, int firstStart, int lastStart, const wstring &name) {
  if (firstStart < 0)
    startGroups.erase(id);
  else
    startGroups[id] = StartGroupInfo(name, firstStart, lastStart);
}

void oEvent::updateStartGroups() {
  getDI().setString("StartGroups", gdibase.widen(encodeStartGroups()));
}

void oEvent::readStartGroups() const {
  auto &sg = getDCI().getString("StartGroups");
  decodeStartGroups(gdibase.narrow(sg));
}

const map<int, StartGroupInfo> &oEvent::getStartGroups(bool reload) const {
  if (reload)
    readStartGroups();
  return startGroups;
}

StartGroupInfo oEvent::getStartGroup(int id) const {
  auto res = startGroups.find(id);
  if (res != startGroups.end())
    return res->second;
  else
    return StartGroupInfo(L"", -1, -1);
}

MachineContainer &oEvent::getMachineContainer() const {
  if (!machineContainer)
    machineContainer = make_unique<MachineContainer>();

  return *machineContainer;
}

void oEvent::setRunnerIdTypes(const pair<string, string>& preferredIdType) {
  wstring coded = gdioutput::widen(preferredIdType.first + ";" + preferredIdType.second);
  getDI().setString("RunnerIdTypes", coded);
}

pair<wstring, wstring> oEvent::getRunnerIdTypes() const {
  wstring raw = getDCI().getString("RunnerIdTypes");
  vector<wstring> out;
  split(raw, L";", out);
  pair<wstring, wstring> outp;
  if (out.size() > 0)
    outp.first = std::move(out[0]);
  if (out.size() > 1)
    outp.second = std::move(out[1]);
  return outp;
}

namespace {
  int encodeExtra(oEvent::ExtraFieldContext context, oEvent::ExtraFields field) {
    return int(context) * 100 + int(field);
  }

  pair<oEvent::ExtraFieldContext, oEvent::ExtraFields> decodeExtra(int coded) {
    int context = coded / 100;
    int field = coded % 100;
    if (context >= 0 && context < int(oEvent::ExtraFieldContext::MaxContext) &&
      field >= 0 && field < int(oEvent::ExtraFields::MaxField)) {
      return make_pair(oEvent::ExtraFieldContext(context), oEvent::ExtraFields(field));
    }
    else
      return make_pair(oEvent::ExtraFieldContext::MaxContext, oEvent::ExtraFields::MaxField);
  }
}

map<oEvent::ExtraFields, wstring> oEvent::getExtraFields(oEvent::ExtraFieldContext context) const {
  map<ExtraFields, wstring> extraFields;
  wstring ws = getDCI().getString("ExtraFields");
  vector<wstring> sp;
  split(ws, L"|", sp);

  for (const wstring& w : sp) {
    int code = wtoi(w.c_str());
    auto cc = decodeExtra(code);
    if (cc.first != context)
      continue;
    size_t descP = w.find_first_of(';');
    if (descP == wstring::npos)
      extraFields[cc.second] = L"";
    else
      extraFields[cc.second] = w.substr(descP+1);
  }
  return extraFields;
}

map<oEvent::ExtraFields, wstring> oEvent::getExtraFieldNames() const {
  map<ExtraFields, wstring> extraFields;
  wstring ws = getDCI().getString("ExtraFields");
  vector<wstring> sp;
  split(ws, L"|", sp);

  for (const wstring& w : sp) {
    int code = wtoi(w.c_str());
    auto cc = decodeExtra(code);
    if (cc.second != ExtraFields::DataA &&
      cc.second != ExtraFields::DataB &&
      cc.second != ExtraFields::TextA)
      continue;
    size_t descP = w.find_first_of(';');
    if (descP != wstring::npos)
      extraFields[cc.second] = w.substr(descP + 1);
  }
  return extraFields;
}

enum class ExtraFields {
  DataA = 0,
  DataB = 1,
  TextA = 2,
  Nationality = 3,
  Sex = 4,
  BirthDate = 5,
  Rank = 6,
  Phone = 7,
  StartTime = 8,
  Bib = 9,
  MaxField
};

string oEvent::extraFieldName(ExtraFields field) {
  switch (field) {
  case ExtraFields::DataA:
    return "DataA";
  case ExtraFields::DataB:
    return "DataB";
  case ExtraFields::TextA:
    return "TextA";
  case ExtraFields::Nationality:
    return "Nationality";
  case ExtraFields::Sex:
    return "Sex";
  case ExtraFields::BirthDate:
    return "BirthDate";
  case ExtraFields::Rank:
    return "Rank";
  case ExtraFields::Phone:
    return "Phone";
  case ExtraFields::StartTime:
    return "StartTime";
  case ExtraFields::Bib:
    return "Bib";
  }
  throw meosException("Unsupported type");
}

void oEvent::updateExtraFields(ExtraFieldContext context, const map<ExtraFields, wstring>& fields) {
  wstring ws = getDCI().getString("ExtraFields");
  vector<wstring> sp;
  split(ws, L"|", sp);
  vector<wstring> spOut;
  set<ExtraFields> used;
  for (const wstring& w : sp) {
    int code = wtoi(w.c_str());
    auto cc = decodeExtra(code);
    if (cc.first == ExtraFieldContext::MaxContext)
      continue;
    if (cc.first != context)
      spOut.push_back(w); // Not touched
    else if (auto res = fields.find(cc.second); res != fields.end()) {
      // Update definition
      wstring w2 = itow(encodeExtra(cc.first, cc.second));
      if (!res->second.empty())
        w2 += L";" + res->second;
      spOut.push_back(w2);
      used.insert(cc.second);
    }
  }

  // Add new
  for (auto& in : fields) {
    if (!used.count(in.first)) {
      wstring w2 = itow(encodeExtra(context, in.first));
      if (!in.second.empty())
        w2 += L";" + in.second;
      spOut.push_back(w2);
    }
  }

  wstring res;
  unsplit<wstring>(spOut, L"|", res);

  getDI().setString("ExtraFields", res);
}

const CardSystem& oEvent::getCardSystem() const {
  if (!cardSystem) {
    cardSystem = make_shared<CardSystem>();
    wstring path = getMeOSFile( L"sportident.cardsystem");

#ifdef _DEBUG
    if (!fileExists(path))
      path = L"./../Lists/sportident.cardsystem";
#endif
    cardSystem->load(path);
  }
  return *cardSystem;
}

bool oEvent::deprecateOldCards() const {
  return getDCI().getInt("OldCards") > 0;
}

void oEvent::deprecateOldCards(bool flag) {
  setProperty("OldCards", flag);
  getDI().setInt("OldCards", flag ? 1 : 0);
}

map<int, oPunch::SpecialPunch> oEvent::getPunchMapping() const {
  const wstring &cMap = getDCI().getString("ControlMap");
  map<int, oPunch::SpecialPunch> res;
  size_t pos = 0;
  while (pos < cMap.size()) {
    int code = wtoi(cMap.data() + pos);
    pos = cMap.find_first_of('-', pos);
    if (code <= 0 || pos == wstring::npos)
      break;
    pos++;
    int value = wtoi(cMap.data() + pos);
    
    if (value == oPunch::PunchCheck || 
        value == oPunch::PunchStart || 
        value == oPunch::PunchFinish) {

      res.emplace(code, oPunch::SpecialPunch(value));
    }

    pos = cMap.find_first_of(';', pos);
    if (pos == wstring::npos)
      break;
    pos++;
  }

  return res;
}

void oEvent::definePunchMapping(int code, oPunch::SpecialPunch value) {
  auto map = getPunchMapping();

  if (value == oPunch::SpecialPunch::PunchUnused)
    map.erase(code);
  else
    map.emplace(code, value);

  wstring data;
  for (auto& [key, val] : map) {
    data += itow(key) + L"-" + itow(val) + L";";
  }
  synchronize(false);
  getDI().setString("ControlMap", data);
  synchronize(true);

  setProperty("ControlMap", data);
}

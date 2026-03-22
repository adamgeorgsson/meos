/********************i****************************************************
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


#include "StdAfx.h"
#include "oListInfo.h"
#include "oEvent.h"
#include "gdioutput.h"
#include "meos_util.h"
#include <cassert>
#include <cmath>
#include "localizer.h"
#include "metalist.h"
#include <algorithm>
#include "gdifonts.h"
#include "generalresult.h"
#include "meosexception.h"
#include "gdiimpl.h"
#include "image.h"
#include "xmlparser.h"


bool oEvent::formatPrintPost(const list<oPrintPost> &ppli, PrintPostInfo &ppi,
                             const pTeam t, const pRunner r, const pClub c,
                             const pClass pc, const pCourse crs, const pControl ctrl,
                             const oPunch *punch, const RogainingLegInfo *rgLeg, int legIndex) {
  int y = ppi.gdi.getCY();
  int x = ppi.gdi.getCX();
  bool updated = false;
  int lineHeight = 0;
  int pdx = 0, pdy = 0;
  auto ppit = ppli.begin();
  while (ppit != ppli.end()) {
    const oPrintPost &pp = *ppit;

    if (pp.type == lLineBreak) {
      x -= ppi.gdi.scaleLength(pp.dx) - pdx;
      pdx = ppi.gdi.scaleLength(pp.dx);
      y += lineHeight;
      ++ppit;
      continue;
    }
    else if (pp.type == lImage) {
      pdy = ppi.gdi.scaleLength(pp.dy);
      pdx = ppi.gdi.scaleLength(pp.dx);
      int format = 0;
      if (pp.imageNoUpdatePos)
        format |= imageNoUpdatePos;
      ppi.gdi.addImage("", y + pdy, x + pdx, format, pp.text,
              ppi.gdi.scaleLength(pp.fixedWidth), ppi.gdi.scaleLength(pp.fixedHeight));
      ++ppit;
      continue;
    }

    int limit = ppit->xlimit;

    bool keepNext = false;
    //Skip merged entities
    while (ppit != ppli.end() && ppit->doMergeNext)
      ++ppit;

    // Main increment below
    if (++ppit != ppli.end() && ppit->dy == pp.dy)
      limit = ppit->dx - pp.dx;
    else
      keepNext = true;

    if (pp.useStrictWidth)
      limit = max(pp.fixedWidth - 5, 0); // Allow some space
    else
      limit = max(pp.fixedWidth, limit);

    assert(limit >= 0);
    pRunner rr = r;
    if (!rr && t) {
      if (pp.legIndex >= 0) {
        if (pc) {
          int lg = pp.legIndex;
          if (pp.legIndex < max<int>(1, pc->getNumStages()))
            lg = pc->getLinearIndex(pp.legIndex, pp.linearLegIndex);

          rr = t->getRunner(lg);
        }
      }
      else if (legIndex >= 0)
        rr = t->getRunner(legIndex);
      else {
        int lg = ppi.par.getLegNumber(pc);
        rr = t->getRunner(lg);
      }
    }
    const wstring &ttext = punch ?   formatPunchString(pp, ppi.par, t, rr, punch, ppi.counter) :
      ((legIndex == -1) ?           formatListString(pp, ppi.par, t, rr, c, pc, ppi.counter) :
                                    formatSpecialString(pp, ppi.par, t, legIndex, crs, ctrl, ppi.counter));

    const wstring *text = &ttext;
    if (rgLeg && ttext.empty())
      text = &formatRogainingString(pp, ppi.par, rgLeg);

    updated |= !text->empty();

    TextInfo *ti = 0;
    if (!text->empty()) {
      int tightBBFlag = ppi.par.tightBoundingBox ? 0 : skipBoundingBox;
      pdy = ppi.gdi.scaleLength(pp.dy);
      pdx = ppi.gdi.scaleLength(pp.dx);
      if ((pp.type == lRunnerName || pp.type == lRunnerCompleteName ||
          pp.type == lRunnerFamilyName || pp.type == lRunnerGivenName ||
          pp.type == lTeamRunner || (pp.type == lPatrolNameNames && !t)) && rr) {
        ti = &ppi.gdi.addStringUT(y + pdy, x + pdx, pp.format | tightBBFlag, *text,
                                  ppi.gdi.scaleLength(limit), ppi.par.cb, pp.fontFace.c_str());
        ti->setExtra(rr->getId());
        ti->id = "R";
      }
      else if ((pp.type == lTeamName || pp.type == lPatrolNameNames || pp.type == lTeamNameRaw) && t) {
        ti = &ppi.gdi.addStringUT(y + pdy, x + pdx, pp.format | tightBBFlag, *text,
                                  ppi.gdi.scaleLength(limit), ppi.par.cb, pp.fontFace.c_str());
        ti->setExtra(t->getId());
        ti->id = "T";
      }
      else {
        ti = &ppi.gdi.addStringUT(y + pdy, x + pdx,
                                  pp.format | tightBBFlag, *text, ppi.gdi.scaleLength(limit), 0, pp.fontFace.c_str());
      }
      if (ti && ppi.keepToghether)
        ti->lineBreakPrioity = -1;
      if (ti) {
        lineHeight = ti->getHeight();
      }
      if (pp.color != colorDefault)
        ti->setColor(pp.color);
    }
    ppi.keepToghether |= keepNext;
  }
  return updated;
}

void oEvent::calculatePrintPostKey(const list<oPrintPost> &ppli, gdioutput &gdi, const oListParam &par,
                                   const pTeam t, const pRunner r, const pClub c,
                                   const pClass pc, oCounter &counter, wstring &key)
{
  key.clear();
  list<oPrintPost>::const_iterator ppit;
  for (ppit=ppli.begin();ppit!=ppli.end(); ++ppit) {
    const oPrintPost &pp=*ppit;
    pRunner rr = r;
    if (!rr && t) {
      int linLeg = pp.legIndex;
      if (pc)
        linLeg = pc->getLinearIndex(pp.legIndex, pp.linearLegIndex);
      rr=t->getRunner(linLeg);
    }
    const wstring &text = formatListString(pp, par, t, rr, c, pc, counter);
    key += text;
    //Skip merged entities
    while (ppit != ppli.end() && ppit->doMergeNext)
      ++ppit;
  }
}

void oEvent::listGeneratePunches(const oListInfo &listInfo, gdioutput &gdi, 
                                 pTeam t, pRunner r, pClub club, pClass cls) {
  const list<oPrintPost> &ppli = listInfo.subListPost;
  const oListParam &par = listInfo.lp;
  auto type = listInfo.listSubType;
  if (!r || ppli.empty())
    return;
  bool filterNamed = listInfo.subFilter(ESubFilterNamedControl);
  const bool filterFinish = listInfo.subFilter(ESubFilterNotFinish);

  pCourse crs = r->getCourse(true);

  if (cls && cls->getNoTiming())
    return;
  if (r && (r->getStatusComputed(true) == StatusNoTiming || r->noTiming()))
    return;

  int h = gdi.getLineHeight();
  int w = 0;
  bool newLine = false;
  int haccum = 0;
  for (auto &pl : ppli) {
    if (pl.type == lPunchNamedTime)
      filterNamed = true;
    if (pl.type == lLineBreak) {
      haccum += h;
      h = 0;
      newLine = true;
      continue;
    }
    h = max(h, gdi.getLineHeight(pl.getFont(), pl.fontFace.c_str()) + gdi.scaleLength(pl.dy));
    w = max(w, gdi.scaleLength(pl.fixedWidth + pl.dx));
  }
  h += haccum;
  int xlimit = gdi.getCX() + gdi.scaleLength(600);
  par.lineBreakControlList = newLine; // Controls if controls names are printed even if the runner has not punched there yet.

  if (w > 0) {
    gdi.pushX();
    gdi.fillNone();
  }

  bool neednewline = false;
  bool updated = false;

  int limit = crs ? crs->nControls() + 1 : 1;

  if (r->Card && r->Card->getNumPunches() > limit)
    limit = r->Card->getNumPunches();

  vector<char> skip(limit, false);
  if (filterNamed && crs) {
    for (int k = 0; k < crs->nControls(); k++) {
      if (crs->getControl(k) && !crs->getControl(k)->hasName())
        skip[k] = true;
    }
    for (int k = crs->nControls() + 1; k < limit; k++) {
      skip[k] = true;
    }
    if (filterFinish)
      skip[crs->nControls()] = true;
  }
  bool filterRadioTimes = listInfo.sortOrder == SortOrder::ClassLiveResult || listInfo.filter(EFilterList::EFilterAnyResult);
  if (filterRadioTimes && crs) {
    for (int k = 0; k < crs->nControls(); k++) {
      if (crs->getControl(k) && !crs->getControl(k)->isValidRadio())
        skip[k] = true;
    }
    for (int k = crs->nControls() + 1; k < limit; k++) {
      skip[k] = true;
    }
    if (filterFinish)
      skip[crs->nControls()] = true;
  }
  PrintPostInfo ppi(gdi, par);
  if (type == oListInfo::EBaseType::EBaseTypeCoursePunches) {
    for (int k = 0; k < limit; k++) {
      if (w > 0 && updated) {
        updated = false;
        if (gdi.getCX() + w > xlimit || newLine) {
          neednewline = false;
          gdi.popX();
          gdi.setCY(gdi.getCY() + h);
        }
        else
          gdi.setCX(gdi.getCX() + w);
      }

      if (!skip[k]) {
        updated |= formatPrintPost(ppli, ppi, t, r, club, cls,
                                   nullptr, nullptr, nullptr, nullptr, -1);
        neednewline |= updated;
      }
      ppi.counter.level3++;
    }
  }
  else if(type == oListInfo::EBaseType::EBaseTypeAllPunches) {
    int startType = -1;
    int finishType = -1;
    const pCourse pcrs = r->getCourse(false);

    if (pcrs) {
      startType = pcrs->getStartPunchType();
      finishType = pcrs->getFinishPunchType();
    }
    int prevPunchTime = r->getStartTime();
    vector<pPunch> punches;
    if (r->Card) {
      for (auto &punch : r->Card->punches)
        punches.push_back(&punch);
    }
    else {
      vector<pFreePunch> fPunches;
      oe->getPunchesForRunner(r->getId(), true, fPunches);
      for (auto punch : fPunches)
        punches.push_back(punch);
    }

    for (auto &pPunch : punches) {
      const oPunch &punch = *pPunch;
      punch.previousPunchTime = prevPunchTime;

      if (punch.isCheck() || punch.isStart(startType))
        continue;
      if (filterFinish && punch.isFinish(finishType))
        continue;
      prevPunchTime = punch.getTimeInt();
      if (w > 0 && updated) {
        updated = false;
        if (gdi.getCX() + w > xlimit || newLine) {
          neednewline = false;
          gdi.popX();
          gdi.setCY(gdi.getCY() + h);
        }
        else
          gdi.setCX(gdi.getCX() + w);
      }

      updated |= formatPrintPost(ppli, ppi, t, r, club, cls,
                                  nullptr, nullptr, &punch, nullptr, -1);
      neednewline |= updated;
      ppi.counter.level3++;
    }
  }
  if (w > 0) {
    gdi.popX();
    gdi.fillDown();
    if (neednewline)
      gdi.setCY(gdi.getCY() + h);
  }
}

void oEvent::generateList(gdioutput &gdi, bool reEvaluate, const oListInfo &li, bool updateScrollBars) {
  if (reEvaluate)
    reEvaluateAll(set<int>(), false);

  oe->calcUseStartSeconds();
  oe->calculateNumRemainingMaps(false);
  oe->updateComputerTime(false);
  oe->setGeneralResultContext(&li.lp);
  
  wstring listname;
  if (!li.head.empty()) {
    oCounter counter;
    const wstring &name = formatListString(li.head.front(), li.lp, 0, 0, 0, 0, counter);
    listname = name;
    li.lp.updateDefaultName(name);
  }
  bool addHead = li.lp.showHeader && !li.lp.useLargeSize;
  size_t nClassesSelected = li.lp.selection.size();
  if (nClassesSelected!=0 && nClassesSelected < min(Classes.size(), Classes.size()/2+5) ) {
    // Non-trivial class selection:
    Classes.sort();
    wstring cls;
    for (oClassList::iterator it = Classes.begin(); it != Classes.end(); ++it) {
      if (li.lp.selection.count(it->getId())) {
        cls += makeDash(L" - ");
        cls += it->getName();
      }
    }
    listname += cls;
  }

  if (li.lp.getLegNumberCoded() != -1) {
    listname += lang.tl(L" Sträcka X#" + li.lp.getLegName());
  }

  generateListInternal(gdi, li, addHead);
  
  for (list<oListInfo>::const_iterator it = li.next.begin(); it != li.next.end(); ++it) {
    bool interHead = addHead && it->getParam().showInterTitle;
    if (li.lp.pageBreak || it->lp.pageBreak) {
      gdi.dropLine(1.0);
      gdi.addStringUT(gdi.getCY() - 1, 0, pageNewPage, "");
    }
    else if (interHead) {
      gdi.dropLine(1.5);
      gdi.addStringUT(gdi.getCY() - 1, 0, pageNewChapter, "");
    }
    else {
      gdi.addStringUT(gdi.getCY() - 1, 0, pageNewPage, "");
    }

    generateListInternal(gdi, *it, interHead);
  }
  // Reset context
  oe->setGeneralResultContext(nullptr);

  gdi.setListDescription(listname);
  if (updateScrollBars)
    gdi.updateScrollbars();
}

// Return true -> filtered away
bool oListInfo::filterRunner(const oRunner &r) const {
  if (r.isRemoved()) 
    return true;

  if (r.tStatus == StatusNotCompeting && !filter(EFilterIncludeNotParticipating))
    return true;

  if (!lp.selection.empty() && lp.selection.count(r.getClassId(true)) == 0)
    return true;

  if (!lp.matchLegNumber(r.getClassRef(false), r.legToRun()))
    return true;

  if (lp.ageFilter != oListParam::AgeFilter::All) {
    int age = r.getBirthAge();
    if (age > 0) {
      oDataConstInterface odc = r.getEvent()->getDCI();
      int lowAgeLimit = odc.getInt("YouthAge");
      //int highAgeLimit = odc.getInt("SeniorAge");


      if (lp.ageFilter == oListParam::AgeFilter::ExludeYouth &&
          age <= lowAgeLimit)
        return true;
        
      if (lp.ageFilter == oListParam::AgeFilter::OnlyYouth &&
          age > lowAgeLimit)
        return true;
    }
    else {
      // Consider "normal"
      if (lp.ageFilter != oListParam::AgeFilter::ExludeYouth)
        return true;
    }
  }

  if (filter(EFilterExcludeDNS)) {
    if (r.tStatus == StatusDNS)
      return true;
    if (r.Class && r.Class->isQualificationFinalBaseClass()) {
      if (r.getLegNumber() > 0 && r.getClassRef(true) == r.Class)
        return true; //Not qualified -> out
    }
  }
  if (filter(EFilterExcludeCANCEL) && r.tStatus == StatusCANCEL)
    return true;

  if (filter(EFilterVacant)) {
    if (r.isVacant())
      return true;
  }
  if (filter(EFilterOnlyVacant)) {
    if (!r.isVacant())
      return true;
  }

  if (filter(EFilterAnyResult)) {
    if (!r.hasOnCourseResult())
      return true;
  }

  if (filter(EFilterAPIEntry)) {
    if (!r.hasFlag(oRunner::FlagAddedViaAPI))
      return true;
  }

  if (filter(EFilterWrongFee)) {
    if (r.getEntryFee() == r.getDefaultFee())
      return true;
  }

  if (filter(EFilterModifiedCard)) {
    if (!r.getCard() || r.getCard()->isOriginalCard() != oCard::PunchOrigin::Manual)
      return true;
  }

  if (filter(EFilterTimeNoResult)) {
    if (r.getFinishTime() <= 0 || r.hasResult())
      return true;
  }

  if (filter(EFilterUnexpectedPunchOrder)) {
    if (!r.getCard() || !r.getCard()->unexpectedOrder(r.getStartTime()))
      return true;
  }

  if (filter(EFilterRentCard) && !r.isRentalCard())
    return true;

  if (filter(EFilterHasCard) && r.getCardNo() == 0)
    return true;

  if (filter(EFilterHasNoCard) && r.getCardNo() > 0)
    return true;

  return false;
}

bool oListInfo::filterRunnerResult(GeneralResult *gResult, const oRunner &r) const {
  if (gResult && r.getTempResult(0).getStatus() == StatusNotCompeting)
    return true;

  if (filter(EFilterHasResult)) {
    if (gResult == 0) {
      if (lp.useControlIdResultTo <= 0 && !r.hasResult())
        return true;
      else if ((lp.useControlIdResultTo > 0 || lp.useControlIdResultFrom > 0) && r.tempStatus != StatusOK)
        return true;
      else if (calcTotalResults && r.getTotalStatus() == StatusUnknown)
        return true;
    }
    else {
      auto &res = r.getTempResult(0);
      RunnerStatus st = res.getStatus();
      if (st == StatusUnknown || isPossibleResultStatus(st) && r.getRunningTime(false) <= 0)
        return true;
    }
  }
  else if (filter(EFilterHasPrelResult)) {
    if (gResult == 0) {
      if (lp.useControlIdResultTo <= 0 && (r.tStatus == StatusUnknown || isPossibleResultStatus(r.getStatusComputed(true))) && r.getRunningTime(false) <= 0)
        return true;
      else if ((lp.useControlIdResultTo > 0 || lp.useControlIdResultFrom > 0) && r.tempStatus != StatusOK)
        return true;
      else if (calcTotalResults && r.getTotalStatus() == StatusUnknown && r.getTotalRunningTime() <= 0)
        return true;
    }
    else {
      auto &res = r.getTempResult(0);
      int rt = res.getRunningTime();
      RunnerStatus st = res.getStatus();
      if ((st == StatusUnknown || isPossibleResultStatus(st)) && rt <= 0)
        return true;
    }
  }
  return false;
}

GeneralResult *oListInfo::applyResultModule(oEvent &oe, vector<pRunner> &rlist) const {
  GeneralResult *gResult = nullptr;
  if (!resultModule.empty()) {
    wstring src;
    oListInfo::ResultType resType = getResultType();
    gResult = oe.getGeneralResult(resultModule, src).get();
    gResult->calculateIndividualResults(rlist, false, resType, sortOrder == Custom, getParam().getInputNumber());

    if (sortOrder == SortByFinishTime || sortOrder == SortByFinishTimeReverse || sortOrder == SortByStartTime)
      gResult->sort(rlist, sortOrder);
  }
  return gResult;
}

void oEvent::formatHeader(gdioutput& gdi, const oListInfo& li, const pRunner rInput) {
  vector<tuple<EPostType, int, wstring>> v;
  int* xLimitForwardUpdate = nullptr;
  for (auto& lp : li.head) {
    bool strUpdate = lp.type == lCmpName || lp.type == lString;
    if (lp.xlimit == 0 && !strUpdate) {
      v.clear();
      v.emplace_back(lp.type, lp.legIndex, lp.text);
      gdiFonts font = lp.getFont();
      lp.xlimit = li.getMaxCharWidth(*this, gdi, li.getParam().selection, v, font, lp.fontFace.c_str());
    }
    else if (strUpdate) {
      v.clear();
      const_cast<wstring&>(lp.text) = li.lp.getCustomTitle(lp.text);
      v.emplace_back(lp.type, lp.legIndex, lp.text);
      gdiFonts font = lp.getFont();
      lp.xlimit = li.getMaxCharWidth(*this, gdi, li.getParam().selection, v, font, lp.fontFace.c_str());
    }
    if (xLimitForwardUpdate)
      (*xLimitForwardUpdate) += lp.xlimit;

    if (lp.doMergeNext && xLimitForwardUpdate == nullptr)
      xLimitForwardUpdate = &lp.xlimit;
    else
      xLimitForwardUpdate = nullptr;
  }

  pTeam team = nullptr;
  pClass cls = nullptr;
  pClub club = nullptr;
  pCourse crs = nullptr;
  int legIndex = -1;
  if (rInput) {
    team = rInput->getTeam();
    club = rInput->getClubRef();
    crs = rInput->getCourse(true);
    cls = rInput->getClassRef(true);
  }

  PrintPostInfo printPostInfo(gdi, li.lp);
  formatPrintPost(li.head, printPostInfo, team, rInput, 
                  club, cls, crs,
                  nullptr, nullptr, nullptr, -1);
}

void oEvent::generateListInternal(gdioutput &gdi, const oListInfo &li, bool formatHead) {
  li.setupLinks();
  li.transformTypes(*this);

  pClass sampleClass = 0;
  bool calculatedSplitResults = false;
  if (!li.lp.selection.empty())
    sampleClass = getClass(*li.lp.selection.begin());
  if (!sampleClass && !Classes.empty())
    sampleClass = &*Classes.begin();

  if (li.listType == li.EBaseTypeRunner) {
    if (li.calculateLiveResults || li.sortOrder == SortOrder::ClassLiveResult)
      calculateResults(li.lp.selection, ResultType::PreliminarySplitResults);

    if (li.calcCourseClassResults)
      calculateResults(li.lp.selection, ResultType::ClassCourseResult);

    if (li.calcCourseResults)
      calculateResults(li.lp.selection, ResultType::CourseResult);

    if (li.calcTotalResults) {
      calculateTeamResults(li.lp.selection, ResultType::TotalResult);
      calculateResults(li.lp.selection, ResultType::TotalResult);
    }

    if (li.calcResults) {
      if (li.lp.useControlIdResultTo > 0 || li.lp.useControlIdResultFrom > 0) {
        calculateSplitResults(li.lp.useControlIdResultFrom, li.lp.useControlIdResultTo);
        calculatedSplitResults = true;
      }
      else {
        calculateTeamResults(li.lp.selection, ResultType::ClassResult);
        calculateResults(li.lp.selection, ResultType::ClassResult);
      }
    }
  }
  else if (li.listType == li.EBaseTypeTeam || li.listType == li.EBaseTypeClubTeam) {
    if (li.calcResults)
      calculateTeamResults(li.lp.selection, ResultType::ClassResult);
    if (li.calcTotalResults)
      calculateTeamResults(li.lp.selection, ResultType::TotalResult);
    if (li.calcCourseResults)
      calculateTeamResults(li.lp.selection, ResultType::CourseResult);
    if (li.calculateLiveResults || li.sortOrder == SortOrder::ClassLiveResult)
      calculateResults(li.lp.selection, ResultType::PreliminarySplitResults);

    if (li.calcCourseClassResults)
      calculateResults(li.lp.selection, ResultType::ClassCourseResult);
  }
  else if (li.listType == li.EBaseTypeClubRunner) {
    if (li.calcResults) {
      calculateTeamResults(li.lp.selection, ResultType::TotalResult);
      calculateTeamResults(li.lp.selection, ResultType::ClassResult);
    }
    if (li.calcCourseClassResults)
      calculateResults(li.lp.selection, ResultType::ClassCourseResult);
    if (li.calcCourseResults)
      calculateResults(li.lp.selection, ResultType::CourseResult);

    if (li.calcResults) {
      if (li.lp.useControlIdResultTo > 0 || li.lp.useControlIdResultFrom > 0) {
        calculateSplitResults(li.lp.useControlIdResultFrom, li.lp.useControlIdResultTo);
        calculatedSplitResults = true;
      }
      else {
        calculateResults(li.lp.selection, ResultType::ClassResult);
      }
    }
  }

  PrintPostInfo printPostInfo(gdi, li.lp);
  //oCounter counter;
  //Render header
  vector<tuple<EPostType, int, wstring>> v;
  for (auto &listPostList : { &li.subHead, &li.listPost, &li.subListPost }) {
    for (auto &lp : *listPostList) {
      if (lp.xlimit == 0) {
        v.clear();
        v.emplace_back(lp.type, lp.legIndex, lp.text);
        gdiFonts font = lp.getFont();
        lp.xlimit = li.getMaxCharWidth(*this, gdi, li.getParam().selection, v, font, lp.fontFace.c_str());
      }
    }
  }

  if (formatHead && li.getParam().showHeader) 
    formatHeader(gdi, li, nullptr);

  if (li.fixedType) {
    generateFixedList(gdi, li);
    return;
  }
     
  // Apply for all teams (calculate start times etc.)
  
  vector<pTeam> tlist;
  tlist.reserve(Teams.size());
  const bool include_nc = li.filter(EFilterIncludeNotParticipating);
  for (oTeamList::iterator it = Teams.begin(); it != Teams.end(); ++it) {
    if (it->isRemoved() || (!include_nc && it->tStatus == StatusNotCompeting))
      continue;

    if (!li.lp.selection.empty() && li.lp.selection.count(it->getClassId(true)) == 0)
      continue;
    it->apply(oBase::ChangeType::Quiet, nullptr);
    tlist.push_back(&*it);
  }

  vector<pRunner> rlist;
  GeneralResult *gResult = nullptr;

  if (li.listType == li.EBaseTypeRunner || li.listType == li.EBaseTypeClubRunner) {
    vector<pRunner> rlistInput;
    getRunners(li.lp.selection, rlistInput, false);
    rlist.reserve(rlistInput.size());
    for (auto r : rlistInput) {
      if (!li.filterRunner(*r))
        rlist.push_back(r);
    }

    if (li.sortOrder != Custom && !calculatedSplitResults)
      sortRunners(li.sortOrder, rlist);

    gResult = li.applyResultModule(*this, rlist);
  }


  wstring oldKey;

  auto formatTeam = [this, &gdi, &li, &gResult, &printPostInfo, &oldKey](pTeam it,
    bool includeSubHead, int &parLegRangeMin, int &parLegRangeMax, pClass &parLegRangeClass) {
    int linearLegSpec = li.lp.getLegNumber(it->getClassRef(false));

    if (gResult && it->getTempResult(0).getStatus() == StatusNotCompeting && !li.filter(EFilterIncludeNotParticipating))
      return false;

    if (li.filter(EFilterExcludeDNS)) {
      if (it->tStatus == StatusDNS)
        return false;
    }

    if (li.filter(EFilterVacant)) {
      if (it->isVacant())
        return false;
    }

    if (li.filter(EFilterOnlyVacant)) {
      if (!it->isVacant())
        return false;
    }

    if (li.filter(EFilterTimeNoResult)) {
      if (it->getFinishTime() <= 0 || !it->hasResult())
        return false;
    }

    if (li.filter(EFilterModifiedCard)) {
      bool modified = false;
      for (pRunner r : it->Runners) {
        if (r && r->getCard() && r->getCard()->isOriginalCard() == oCard::PunchOrigin::Manual)
          modified = true;
      }
      if (modified)
        return false;
    }

    if (li.filter(EFilterHasResult)) {
      if (gResult) {
        RunnerStatus st = it->getTempResult(0).getStatus();
        if (st == StatusUnknown || (isPossibleResultStatus(st) && it->getTempResult(0).getRunningTime() <= 0))
          return false;
      }
      else {
        RunnerStatus st = it->getLegStatus(linearLegSpec, true, false);
        if (st == StatusUnknown || (isPossibleResultStatus(st) && it->getLegRunningTime(linearLegSpec, true, false) <= 0))
          return false;
        else if (li.calcTotalResults) {
          st = it->getLegStatus(linearLegSpec, true, true);
          if (st == StatusUnknown || (isPossibleResultStatus(st) && it->getLegRunningTime(linearLegSpec, true, true) <= 0))
            return false;
        }
      }
    }
    else if (li.filter(EFilterHasPrelResult)) {
      if (gResult) {
        RunnerStatus st = it->getTempResult(0).getStatus();
        if ((st == StatusUnknown || isPossibleResultStatus(st)) && it->getTempResult(0).getRunningTime() <= 0)
          return false;
      }
      else {
        RunnerStatus st = it->getLegStatus(linearLegSpec, true, false);
        if ((st == StatusUnknown || isPossibleResultStatus(st)) && it->getLegRunningTime(linearLegSpec, true, false) <= 0)
          return false;
        else if (li.calcTotalResults) {
          RunnerStatus st = it->getLegStatus(linearLegSpec, true, true);

          if ((st == StatusUnknown || isPossibleResultStatus(st)) && it->getLegRunningTime(linearLegSpec, true, true) <= 0)
            return false;
        }
      }
    }
    else if (li.filter(EFilterAnyResult)) {
      pRunner r = it->getRunner(linearLegSpec);
      if (!r || !r->hasOnCourseResult())
        return false;
    }

    const bool needParRange = li.subFilter(ESubFilterSameParallel)
                              || li.subFilter(ESubFilterSameParallelNotFirst);

    if (needParRange && it->Class != parLegRangeClass && it->Class) {
      parLegRangeClass = it->Class;
      parLegRangeClass->getParallelRange(linearLegSpec < 0 ? 0 : linearLegSpec, parLegRangeMin, parLegRangeMax);
      if (li.subFilter(ESubFilterSameParallelNotFirst))
        parLegRangeMin++;
    }

    if (includeSubHead) {
      wstring newKey;
      printPostInfo.par.relayLegIndex = linearLegSpec;
      calculatePrintPostKey(li.subHead, gdi, li.lp, &*it, 0, it->Club, it->Class, printPostInfo.counter, newKey);
      if (newKey != oldKey) {
        if (!oldKey.empty())
          gdi.addStringUT(gdi.getCY() - 1, 0, pageNewPage, "");

        wstring legInfo;
        if (linearLegSpec >= 0 && it->getClassRef(false)) {
          // Specified leg
          legInfo = lang.tl(L", Str. X#" + li.lp.getLegName());
        }

        gdi.addStringUT(pagePageInfo, it->getClass(true) + legInfo); // Teamlist

        oldKey.swap(newKey);
        printPostInfo.counter.level2 = 0;
        printPostInfo.counter.level3 = 0;
        printPostInfo.reset();
        printPostInfo.par.relayLegIndex = linearLegSpec;
        formatPrintPost(li.subHead, printPostInfo, &*it, 0, it->Club, it->Class,
          nullptr, nullptr, nullptr, nullptr, -1);
      }
    }
    ++printPostInfo.counter;
    if (li.lp.filterInclude(printPostInfo.counter.level2, it)) {
      printPostInfo.counter.level3 = 0;
      printPostInfo.reset();
      printPostInfo.par.relayLegIndex = linearLegSpec;
      formatPrintPost(li.listPost, printPostInfo, it, 0, it->Club, it->Class,
        nullptr, nullptr, nullptr, nullptr, -1);

      if (li.subListPost.empty())
        return true;

      if (li.listSubType == li.EBaseTypeRunner) {
        int nr = int(it->Runners.size());
        vector<pRunner> tr;
        tr.reserve(nr);
        vector<int> usedIx(nr, -1);

        for (int k = 0; k < nr; k++) {
          if (!it->Runners[k]) {
            if (li.filter(EFilterHasResult) || li.subFilter(ESubFilterHasResult) ||
              li.filter(EFilterHasPrelResult) || li.subFilter(ESubFilterHasPrelResult) ||
              li.filter(EFilterExcludeDNS) || li.filter(EFilterExcludeCANCEL) || li.subFilter(ESubFilterExcludeDNS) ||
              li.subFilter(ESubFilterVacant)) {
              usedIx[k] = -2; // Skip totally
            }
            continue;
          }
          else
            usedIx[k] = -2; // Skip unless allowed after filters
          bool noResult = false;
          bool noPrelResult = false;
          bool noStart = false;
          bool cancelled = false;
          if (gResult == 0) {
            noResult = it->Runners[k]->tStatus == StatusUnknown;
            noPrelResult = it->Runners[k]->tStatus == StatusUnknown && it->Runners[k]->getRunningTime(false) <= 0;
            noStart = it->Runners[k]->tStatus == StatusDNS || it->Runners[k]->tStatus == StatusCANCEL;
            if (it->Runners[k]->Class && it->Runners[k]->Class->isQualificationFinalBaseClass()) {
              if (k > 0 && it->Runners[k]->getClassRef(true) == it->Runners[k]->Class)
                noStart = true;
            }

            cancelled = it->Runners[k]->tStatus == StatusCANCEL;
            //XXX TODO Multiday
          }
          else {
            noResult = it->Runners[k]->tmpResult.status == StatusUnknown;
            noPrelResult = it->Runners[k]->tmpResult.status == StatusUnknown && it->Runners[k]->tmpResult.runningTime <= 0;
            noStart = it->Runners[k]->tmpResult.status == StatusDNS || it->Runners[k]->tmpResult.status == StatusCANCEL;
            cancelled = it->Runners[k]->tmpResult.status == StatusCANCEL;
          }

          if (noResult && (li.filter(EFilterHasResult) || li.subFilter(ESubFilterHasResult)))
            continue;

          if (noPrelResult && (li.filter(EFilterHasPrelResult) || li.subFilter(ESubFilterHasPrelResult)))
            continue;

          if (noStart && (li.filter(EFilterExcludeDNS) || li.subFilter(ESubFilterExcludeDNS)))
            continue;

          if (cancelled && li.filter(EFilterExcludeCANCEL))
            continue;

          if (it->Runners[k]->isVacant() && li.subFilter(ESubFilterVacant))
            continue;

          if ((it->Runners[k]->tLeg < parLegRangeMin || it->Runners[k]->tLeg > parLegRangeMax)
            && needParRange)
            continue;

          usedIx[k] = tr.size();
          tr.push_back(it->Runners[k]);
        }

        if (gResult) {
          gResult->sortTeamMembers(tr);

          for (size_t k = 0; k < tr.size(); k++) {
            bool suitableBreak = k < 2 || (k + 2) >= tr.size();
            printPostInfo.keepToghether = suitableBreak;
            printPostInfo.par.relayLegIndex = tr[k] ? tr[k]->tLeg : -1;
            formatPrintPost(li.subListPost, printPostInfo, &*it, tr[k],
              it->Club, tr[k]->getClassRef(true),
              nullptr, nullptr, nullptr, nullptr, -1);
            printPostInfo.counter.level3++;
          }
        }
        else {
          for (size_t k = 0; k < usedIx.size(); k++) {
            if (usedIx[k] == -2)
              continue; // Skip
            bool suitableBreak = k < 2 || (k + 2) >= usedIx.size();
            printPostInfo.keepToghether = suitableBreak;
            printPostInfo.par.relayLegIndex = k;
            if (usedIx[k] == -1) {
              pCourse crs = it->Class ? it->Class->getCourse(k, it->StartNo) : 0;
              formatPrintPost(li.subListPost, printPostInfo, &*it, 0,
                it->Club, it->Class, crs, nullptr, nullptr, nullptr, k);
            }
            else {
              formatPrintPost(li.subListPost, printPostInfo, &*it, tr[usedIx[k]],
                it->Club, tr[usedIx[k]]->getClassRef(true),
                nullptr, nullptr, nullptr, nullptr, -1);
            }
            printPostInfo.counter.level3++;
          }
        }
      }
      else if (li.listSubType == li.EBaseTypeCoursePunches ||
               li.listSubType == li.EBaseTypeAllPunches) {
        pRunner r = it->getRunner(linearLegSpec);
        if (!r) 
          return true;

        listGeneratePunches(li, gdi, &*it, r, it->Club, it->Class);
      }
    }
    return true;
  };

  if (li.listType == li.EBaseTypeRunner) {
    for (size_t k = 0; k < rlist.size(); k++) {
      pRunner it = rlist[k];
      if (li.filterRunnerResult(gResult, *it))
        continue;
 
      wstring newKey;
      printPostInfo.par.relayLegIndex = -1;
      calculatePrintPostKey(li.subHead, gdi, li.lp, it->tInTeam, &*it, it->Club, it->getClassRef(true), printPostInfo.counter, newKey);

      if (newKey != oldKey) {
        if (!oldKey.empty())
          gdi.addStringUT(gdi.getCY() - 1, 0, pageNewPage, "");
        
        gdi.addStringUT(pagePageInfo, it->getClass(true));

        oldKey.swap(newKey);
        printPostInfo.counter.level2 = 0;
        printPostInfo.counter.level3 = 0;
        printPostInfo.reset();
        printPostInfo.par.relayLegIndex = -1;
        formatPrintPost(li.subHead, printPostInfo, it->tInTeam, &*it, it->Club, it->getClassRef(true),
                        nullptr, nullptr, nullptr, nullptr, -1);
      }
      if (li.lp.filterInclude(printPostInfo.counter.level2 + 1, rlist[k])) {
        printPostInfo.reset();
        printPostInfo.par.relayLegIndex = it->tLeg;
        formatPrintPost(li.listPost, printPostInfo, it->tInTeam, &*it, it->Club, it->getClassRef(true),
                        nullptr, nullptr, nullptr, nullptr, -1);

        if (li.listSubType == li.EBaseTypeCoursePunches ||
            li.listSubType == li.EBaseTypeAllPunches) {
          listGeneratePunches(li, gdi, it->tInTeam, &*it, it->Club, it->getClassRef(true));
        }
      }
      ++printPostInfo.counter;
    }
  }
  else if (li.listType == li.EBaseTypeTeam) {
    if (li.sortOrder != SortOrder::Custom) {
      pair<int, bool> legInfo = li.lp.getLegInfo(sampleClass);
      sortTeams(li.sortOrder, legInfo.first, legInfo.second, tlist);
    }
    
    if (!li.resultModule.empty()) {
      wstring src;
      gResult = getGeneralResult(li.resultModule, src).get();
      oListInfo::ResultType resType = li.getResultType();
      gResult->calculateTeamResults(tlist, false, resType, li.sortOrder == Custom, li.getParam().getInputNumber());
    }
    // Range of runners to include
    int parLegRangeMin = 0, parLegRangeMax = 1000;
    pClass parLegRangeClass = nullptr;
    
    for (pTeam t : tlist) {
      formatTeam(t, true, parLegRangeMin, parLegRangeMax, parLegRangeClass);
    }
  }
  else if (li.listType == li.EBaseTypeClubRunner) {
    Clubs.sort();
    
    map<int, vector<pRunner>> clubToRunner;
    for (pRunner r : rlist) {
      int club = r->getClubId();
      if (club == 0)
        continue;
      clubToRunner[club].push_back(r);
    }

    bool first = true;
    for (auto it = Clubs.begin(); it != Clubs.end(); ++it) {
      if (it->isRemoved())
        continue;

      if (li.filter(EFilterVacant)) {
        if (it->getId() == getVacantClub(false))
          continue;
      }

      if (li.filter(EFilterOnlyVacant)) {
        if (it->getId() != getVacantClub(false))
          continue;
      }

      bool startClub = false;

      for (auto rit : clubToRunner[it->getId()]) {
        if (li.filterRunnerResult(gResult, *rit))
          continue;

        if (!startClub) {
          if (!first)
            gdi.addStringUT(gdi.getCY() - 1, 0, pageNewPage, "");
          else
            first = false;
          
          gdi.addStringUT(pagePageInfo, it->getName());
          printPostInfo.counter.level2 = 0;
          printPostInfo.counter.level3 = 0;
          printPostInfo.reset();
          printPostInfo.par.relayLegIndex = -1;
          formatPrintPost(li.subHead, printPostInfo, nullptr, nullptr, &*it,
                          nullptr, nullptr, nullptr, nullptr, nullptr, -1);
          startClub = true;
        }
        ++printPostInfo.counter;
        if (li.lp.filterInclude(printPostInfo.counter.level2, rit)) {
          printPostInfo.counter.level3 = 0;
          printPostInfo.reset();
          printPostInfo.par.relayLegIndex = rit->tLeg;
          formatPrintPost(li.listPost, printPostInfo, nullptr, rit, &*it, rit->getClassRef(true),
                          nullptr, nullptr, nullptr, nullptr, -1);

          if (li.listSubType == li.EBaseTypeCoursePunches ||
              li.listSubType == li.EBaseTypeAllPunches) {
            listGeneratePunches(li, gdi, rit->tInTeam, &*rit, rit->Club, rit->getClassRef(true));
          }
        }
      }//Runners
    }//Clubs
  }
  else if (li.listType == li.EBaseTypeClubTeam) {
    Clubs.sort();
    map<int, vector<pTeam>> clubToTeam;

    for (pTeam t : tlist) {
      int club = t->getClubId();
      if (club == 0)
        continue;
      clubToTeam[club].push_back(t);
    }

    bool first = true;
    int parLegRangeMin = 0, parLegRangeMax = 1000;
    pClass parLegRangeClass = nullptr;

    for (auto it = Clubs.begin(); it != Clubs.end(); ++it) {
      if (it->isRemoved())
        continue;

      if (li.filter(EFilterVacant)) {
        if (it->getId() == getVacantClub(false))
          continue;
      }

      if (li.filter(EFilterOnlyVacant)) {
        if (it->getId() != getVacantClub(false))
          continue;
      }

      bool startClub = false;
      for (auto rit : clubToTeam[it->getId()]) {
        if (!startClub) {
          if (!first)
            gdi.addStringUT(gdi.getCY() - 1, 0, pageNewPage, "");
          else
            first = false;

          gdi.addStringUT(pagePageInfo, it->getName());
          printPostInfo.counter.level2 = 0;
          printPostInfo.counter.level3 = 0;
          printPostInfo.reset();
          printPostInfo.par.relayLegIndex = -1;
          formatPrintPost(li.subHead, printPostInfo, nullptr, nullptr, &*it,
            nullptr, nullptr, nullptr, nullptr, nullptr, -1);
          startClub = true;
        }
        formatTeam(rit, false, parLegRangeMin, parLegRangeMax, parLegRangeClass);
      }//Teams
    }//Clubs
  }
  else if (li.listType == oListInfo::EBaseTypeCourse) {
    Courses.sort();
    oCourseList::iterator it;
    for (it = Courses.begin(); it != Courses.end(); ++it) {
      if (it->isRemoved())
        continue;

      if (!li.lp.selection.empty()) {
        vector<pClass> usageClass;
        it->getClasses(usageClass);

        bool used = false;
        while (!used && !usageClass.empty()) {
          used = li.lp.selection.count(usageClass.back()->getId()) > 0;
          usageClass.pop_back();
        }
        if (!used)
          continue;
      }

      wstring newKey;
      calculatePrintPostKey(li.subHead, gdi, li.lp, 0, 0, 0, 0, printPostInfo.counter, newKey);

      if (newKey != oldKey) {
        if (!oldKey.empty())
          gdi.addStringUT(gdi.getCY() - 1, 0, pageNewPage, "");
        
        oldKey.swap(newKey);
        printPostInfo.counter.level2 = 0;
        printPostInfo.counter.level3 = 0;
        printPostInfo.reset();
        formatPrintPost(li.subHead, printPostInfo, nullptr,
                        nullptr, nullptr, nullptr, &*it, nullptr, nullptr, nullptr, 0);
      }
      if (li.lp.filterInclude(printPostInfo.counter.level2 + 1, nullptr)) {
        printPostInfo.reset();
        formatPrintPost(li.listPost, printPostInfo, nullptr, nullptr, nullptr,
                        nullptr, &*it, nullptr, nullptr, nullptr, 0);

        if (li.listSubType == li.EBaseTypeControl) {
          //TODO: listGeneratePunches(li.subListPost, gdi, li.lp, it->tInTeam, &*it, it->Club, it->Class);
        }
      }
      ++printPostInfo.counter;
    }
  }
  else if (li.listType == oListInfo::EBaseTypeControl) {
    Controls.sort();
    oControlList::iterator it;
    for (it = Controls.begin(); it != Controls.end(); ++it) {
      if (it->isRemoved())
        continue;

      wstring newKey;
      calculatePrintPostKey(li.subHead, gdi, li.lp, 0, 0, 0, 0, printPostInfo.counter, newKey);

      if (newKey != oldKey) {
        if (!oldKey.empty())
          gdi.addStringUT(gdi.getCY() - 1, 0, pageNewPage, "");
        
        oldKey.swap(newKey);
        printPostInfo.counter.level2 = 0;
        printPostInfo.counter.level3 = 0;
        printPostInfo.reset();
        formatPrintPost(li.subHead, printPostInfo, nullptr, nullptr, nullptr,
                        nullptr, nullptr, &*it, nullptr, nullptr, 0);
      }
      if (li.lp.filterInclude(printPostInfo.counter.level2 + 1, nullptr)) {
        printPostInfo.reset();
        formatPrintPost(li.listPost, printPostInfo, nullptr, nullptr, nullptr,
                        nullptr, nullptr, &*it, nullptr, nullptr, 0);
      }
      ++printPostInfo.counter;
    }
  }
  else if (li.listType == oListInfo::EBaseTypeRGLeg || li.listType == oListInfo::EBaseTypeRGLegGlobal) {
    vector<pair<pClass, vector<oClass::RogainingLeg>>> rgLegs;
    bool globalList = li.listType == oListInfo::EBaseTypeRGLegGlobal;
    map<pair<int, int>, oClass::RogainingLeg> globalData;

    for (auto &c : Classes) {
      if (c.isRemoved())
        continue;

      auto res = c.getRogainingLegs();
      if (!res.empty()) {
        if (!globalList) {
          rgLegs.emplace_back();
          rgLegs.back().first = &c;
          rgLegs.back().second = std::move(res);
        }
        else {
          for (auto &leg : res) {
            auto gRes = globalData.emplace(make_pair(leg.from, leg.to), leg);
            if (!gRes.second) {
              auto &gData = gRes.first->second;
              gData.bestTime = min(gData.bestTime, leg.bestTime);
              gData.numCompetitors += leg.numCompetitors;
            }
          }
        }
      }
    }

    if (globalList && !globalData.empty()) {
      rgLegs.emplace_back();
      for (auto &[key, val] : globalData) {
        rgLegs.back().second.push_back(val);
      }
    }

    for (auto &rgCls : rgLegs) {
      printPostInfo.counter.level2 = 0;

      sort(rgCls.second.begin(), rgCls.second.end(), [](const oClass::RogainingLeg &a,
                                                        const oClass::RogainingLeg &b) -> bool {
        if (a.to == b.to)
          return a.from < b.from;
        else {
          if (a.to == oPunch::SpecialPunch::PunchFinish)
            return false;
          else if (b.to == oPunch::SpecialPunch::PunchFinish)
            return true;
          else
            return a.to < b.to;
        }
      });

      for (auto &leg : rgCls.second) {
        pControl ctrl = nullptr;
        RogainingLegInfo rgLeg;
        wstring newKey;
        calculatePrintPostKey(li.subHead, gdi, li.lp, nullptr, nullptr, nullptr, rgCls.first, printPostInfo.counter, newKey);

        rgLeg.bestTime = leg.bestTime;
        rgLeg.numCompetitors = leg.numCompetitors;
        if (leg.from == oPunch::SpecialPunch::PunchStart)
          rgLeg.from = lang.tl("Start");
        else
          rgLeg.from = itow(leg.from);

        if (leg.to == oPunch::SpecialPunch::PunchFinish)
          rgLeg.to = lang.tl("Mål");
        else
          rgLeg.to = itow(leg.to);

        pCourse crs = rgCls.first ? rgCls.first->getCourse(false) : nullptr;

        if (newKey != oldKey) {
          if (!oldKey.empty())
            gdi.addStringUT(gdi.getCY() - 1, 0, pageNewPage, "");

          oldKey.swap(newKey);
          printPostInfo.counter.level3 = 0;
          printPostInfo.reset();
          formatPrintPost(li.subHead, printPostInfo, nullptr, nullptr, nullptr,
                          rgCls.first, crs, ctrl, nullptr, &rgLeg, -1);
        }
        if (li.lp.filterInclude(printPostInfo.counter.level3 + 1, nullptr)) {
          printPostInfo.reset();
          formatPrintPost(li.listPost, printPostInfo, nullptr, nullptr, nullptr,
                          rgCls.first, crs, ctrl, nullptr, &rgLeg, -1);
        }
        ++printPostInfo.counter;
      }
    }
  }
}

void oEvent::fillListTypes(gdioutput &gdi, const string &name, int filter)
{
  map<EStdListType, oListInfo> listMap;
  getListTypes(listMap, filter);

  //gdi.clearList(name);
  map<EStdListType, oListInfo>::iterator it;

  vector< pair<wstring, size_t> > v;
  for (it=listMap.begin(); it!=listMap.end(); ++it) {
    v.push_back(make_pair(it->second.Name, it->first));
    //gdi.addItem(name, it->second.Name, it->first);
  }
  sort(v.begin(), v.end());
  gdi.setItems(name, v);
}

void oEvent::getListType(EStdListType type, oListInfo &li)
{
  map<EStdListType, oListInfo> listMap;
  getListTypes(listMap, false);
  li = listMap[type];
}


void oEvent::getListTypes(map<EStdListType, oListInfo> &listMap, int filterResults)
{
  listMap.clear();

  if (!filterResults) {
    oListInfo li;
    li.Name=lang.tl(L"Startlista, individuell");
    li.listType=li.EBaseTypeRunner;
    li.supportClasses = true;
    li.supportLegs = true;
    listMap[EStdStartList]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl(L"Resultat, individuell");
    li.listType=li.EBaseTypeRunner;
    li.supportClasses = true;
    li.supportLegs = true;
    li.supportFrom = true;
    li.supportTo = true;
    listMap[EStdResultList]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl(L"Resultat, generell");
    li.listType=li.EBaseTypeRunner;
    li.supportClasses = true;
    li.supportLegs = true;
    li.supportLarge = true;
    li.supportFrom = true;
    li.supportTo = true;
    li.calcTotalResults = true;
    listMap[EGeneralResultList]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl(L"Rogaining, individuell");
    li.listType=li.EBaseTypeRunner;
    li.supportClasses = true;
    li.supportLegs = true;
    listMap[ERogainingInd]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl(L"Avgjorda klasser (prisutdelningslista)");
    li.listType=li.EBaseTypeRunner;
    li.supportClasses = true;
    li.supportLegs = true;
    listMap[EIndPriceList]=li;
  }
  if (!filterResults) {
    oListInfo li;
    li.Name=lang.tl(L"Startlista, patrull");
    li.listType=li.EBaseTypeTeam;
    li.supportClasses = true;
    li.supportLegs = false;

    listMap[EStdPatrolStartList]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl(L"Resultat, patrull");
    li.listType=li.EBaseTypeTeam;
    li.supportClasses = true;
    li.supportLegs = false;

    listMap[EStdPatrolResultList]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl(L"Patrullresultat (STOR)");
    li.listType=li.EBaseTypeTeam;
    li.supportClasses = true;
    li.supportLegs = false;
    li.largeSize = true;
    listMap[EStdPatrolResultListLARGE]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl(L"Resultat (STOR)");
    li.supportClasses = true;
    li.supportLegs = true;
    li.supportFrom = true;
    li.supportTo = true;
    li.largeSize = true;
    li.listType=li.EBaseTypeRunner;
    listMap[EStdResultListLARGE]=li;
  }

  /*{
    oListInfo li;
    li.Name=lang.tl("Stafettresultat, sträcka (STOR)");
    li.supportClasses = true;
    li.supportLegs = true;
    li.largeSize = true;
    li.listType=li.EBaseTypeTeam;
    listMap[EStdTeamResultListLegLARGE]=li;
  }*/

  if (!filterResults) {
    oListInfo li;
    li.Name=lang.tl(L"Hyrbricksrapport");
    li.listType=li.EBaseTypeRunner;
    li.supportClasses = false;
    li.supportLegs = false;
    listMap[EStdRentedCard]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl(L"Stafettresultat, delsträckor");
    li.listType=li.EBaseTypeTeam;
    li.supportClasses = true;
    li.supportLegs = false;
    listMap[EStdTeamResultListAll]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl(L"Stafettresultat, lag");
    li.listType=li.EBaseTypeTeam;
    li.supportClasses = true;
    li.supportLegs = false;
    listMap[EStdTeamResultList]=li;
  }
  /*
  {
    oListInfo li;
    li.Name=lang.tl("Stafettresultat, sträcka");
    li.listType=li.EBaseTypeTeam;
    li.supportClasses = true;
    li.supportLegs = true;
    listMap[EStdTeamResultListLeg]=li;
  }*/

  if (!filterResults) {
    {
      oListInfo li;
      li.Name=lang.tl(L"Startlista, stafett (lag)");
      li.listType=li.EBaseTypeTeam;
      li.supportClasses = true;
      li.supportLegs = false;
      listMap[EStdTeamStartList]=li;
    }
    {
      oListInfo li;
      li.Name=lang.tl(L"Startlista, stafett (sträcka)");
      li.listType=li.EBaseTypeTeam;
      li.supportClasses = true;
      li.supportLegs = true;
      listMap[EStdTeamStartListLeg]=li;
    }
    {
      oListInfo li;
      li.Name=lang.tl(L"Bantilldelning, stafett");
      li.listType=li.EBaseTypeTeam;
      li.supportClasses = true;
      li.supportLegs = false;
      listMap[ETeamCourseList]=li;
    }
    {
      oListInfo li;
      li.Name=lang.tl(L"Bantilldelning, individuell");
      li.listType=li.EBaseTypeRunner;
      li.supportClasses = true;
      li.supportLegs = false;
      listMap[EIndCourseList]=li;
    }
    {
      oListInfo li;
      li.Name=lang.tl(L"Individuell startlista, visst lopp");
      li.listType=li.EBaseTypeTeam;
      li.supportClasses = true;
      li.supportLegs = true;
      listMap[EStdIndMultiStartListLeg]=li;
    }
  }

  {
    oListInfo li;
    li.Name=lang.tl(L"Individuell resultatlista, visst lopp");
    li.listType=li.EBaseTypeTeam;
    li.supportClasses = true;
    li.supportLegs = true;
    listMap[EStdIndMultiResultListLeg]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl(L"Individuell resultatlista, visst lopp (STOR)");
    li.listType=li.EBaseTypeTeam;
    li.supportClasses = true;
    li.supportLegs = true;
    li.largeSize = true;
    listMap[EStdIndMultiResultListLegLARGE]=li;
  }

  {
    oListInfo li;
    li.Name = lang.tl(L"Individuell resultatlista, alla lopp");
    li.listType = li.EBaseTypeTeam;
    li.supportClasses = true;
    li.supportLegs = false;
    listMap[EStdIndMultiResultListAll]=li;
  }

  if (!filterResults) {
    oListInfo li;
    li.Name = lang.tl(L"Klubbstartlista");
    li.listType = li.EBaseTypeClubRunner;
    li.supportClasses = true;
    li.supportLegs = false;
    listMap[EStdClubStartList]=li;
  }

  {
    oListInfo li;
    li.Name = lang.tl(L"Klubbresultatlista");
    li.listType = li.EBaseTypeClubRunner;
    li.supportClasses = true;
    li.supportLegs = false;
    listMap[EStdClubResultList]=li;
  }

  if (!filterResults) {
    {
      oListInfo li;
      li.Name=lang.tl(L"Tävlingsrapport");
      li.supportClasses = false;
      li.supportLegs = false;
      li.listType=li.EBaseTypeNone;
      listMap[EFixedReport]=li;
    }

    {
      oListInfo li;
      li.Name=lang.tl(L"Kontroll inför tävlingen");
      li.supportClasses = false;
      li.supportLegs = false;
      li.listType=li.EBaseTypeNone;
      listMap[EFixedPreReport]=li;
    }

    {
      oListInfo li;
      li.Name=lang.tl(L"Kvar-i-skogen");
      li.supportClasses = false;
      li.supportLegs = false;
      li.listType=li.EBaseTypeNone;
      listMap[EFixedInForest]=li;
    }

    {
      oListInfo li;
      li.Name=lang.tl(L"Fakturor");
      li.supportClasses = false;
      li.supportLegs = false;
      li.listType=li.EBaseTypeNone;
      listMap[EFixedInvoices]=li;
    }

    {
      oListInfo li;
      li.Name=lang.tl(L"Ekonomisk sammanställning");
      li.supportClasses = false;
      li.supportLegs = false;
      li.listType=li.EBaseTypeNone;
      listMap[EFixedEconomy]=li;
    }
  }
  /*
  {
    oListInfo li;
    li.Name=lang.tl("Först-i-mål, klassvis");
    li.supportClasses = false;
    li.supportLegs = false;
    li.listType=li.EBaseTypeNone;
    listMap[EFixedResultFinishPerClass]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl("Först-i-mål, gemensam");
    li.supportClasses = false;
    li.supportLegs = false;
    li.listType=li.EBaseTypeNone;
    listMap[EFixedResultFinish]=li;
  }*/

  if (!filterResults) {
    oListInfo li;
    li.Name=lang.tl(L"Minutstartlista");
    li.supportClasses = false;
    li.supportLegs = false;
    li.listType=li.EBaseTypeNone;
    listMap[EFixedMinuteStartlist]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl(L"Händelser - tidslinje");
    li.supportClasses = true;
    li.supportLegs = false;
    li.listType=li.EBaseTypeNone;
    listMap[EFixedTimeLine]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl(L"Liveresultat, deltagare");
    li.listType=li.EBaseTypeRunner;
    li.supportClasses = true;
    li.supportLegs = false;
    li.supportFrom = true;
    li.supportTo = true;
    li.supportSplitAnalysis = false;
    li.supportInterResults = false;
    li.supportPageBreak = false;
    li.supportClassLimit = false;
    li.supportCustomTitle = false;
  
    listMap[EFixedLiveResult]=li;
  }

  {
    oListInfo li;
    li.Name=lang.tl(L"Stafettresultat, sträcka (STOR)");
    li.supportClasses = true;
    li.supportLegs = false;
    li.largeSize = true;
    li.listType=li.EBaseTypeTeam;
    listMap[EStdTeamAllLegLARGE]=li;
  }

  getListContainer().setupListInfo(EFirstLoadedList, listMap, filterResults != 0);
}


void oEvent::generateListInfo(const gdioutput& target, EStdListType lt, int classId, oListInfo &li) {
  oListParam par;
  if (classId!=0)
    par.selection.insert(classId);

  par.listCode=lt;
  generateListInfo(target, par, li);
}

int openRunnerTeamCB(gdioutput *gdi, GuiEventType type, BaseInfo *data);

void oEvent::generateFixedList(gdioutput &gdi, const oListInfo &li)
{
  wstring dmy;
  switch (li.lp.listCode) {
    case EFixedPreReport:
      generatePreReport(gdi);
    break;

    case EFixedReport:
      generateCompetitionReport(gdi);
    break;

    case EFixedInForest:
      generateInForestList(gdi, openRunnerTeamCB, 0);
    break;

    case EFixedEconomy:
      printInvoices(gdi, IPTAllPrint, dmy, true);
    break;

    case EFixedInvoices:
      printInvoices(gdi, IPTAllPrint, dmy, false);
    break;

    case EFixedMinuteStartlist:
      generateMinuteStartlist(gdi);
    break;

    case EFixedLiveResult:


    break;

    case EFixedTimeLine:
      gdi.clearPage(false);
      gdi.addString("", boldLarge, makeDash(L"Tidslinje - X#") + getName());

      gdi.dropLine();
      set<__int64> stored;
      vector<oTimeLine> events;

      map<int, wstring> cName;
      for (oClassList::const_iterator it = Classes.begin(); it != Classes.end(); ++it) {
        if (!it->isRemoved())
          cName[it->getId()] = it->getName();
      }

      oe->getTimeLineEvents(li.lp.selection, events, stored, timeConstHour*24*7);
      gdi.fillDown();
      int yp = gdi.getCY();
      int xp = gdi.getCX();

      int w1 = gdi.scaleLength(60);
      int w = gdi.scaleLength(110);
      int w2 = w1+w;

      for (size_t k = 0; k<events.size(); k++) {
        const oTimeLine &ev = events[k];

        pRunner r = dynamic_cast<pRunner>(ev.getSource(*this));
        if (!r)
          continue;

        if (ev.getType() == oTimeLine::TLTFinish && r->getStatus() != StatusOK)
          continue;

        wstring name = L"";
        if (r)
          name = r->getCompleteIdentification(oRunner::IDType::ParallelLegExtra) + L" ";

        gdi.addStringUT(yp, xp, 0, oe->getAbsTime(ev.getTime()));
        gdi.addStringUT(yp, xp + w1, 0, cName[ev.getClassId()], w-10);
        gdi.addStringUT(yp, xp + w2, 0, name + lang.tl(ev.getMessage()));

        yp += gdi.getLineHeight();


        /*string detail = ev.getDetail();

        if (detail.size() > 0) {
          gdi.addStringUT(yp, xp + w, 0, detail);
          yp += gdi.getLineHeight();
        }*/

      }
      gdi.refresh();

    break;
  }
}

void oListInfo::setCallback(GUICALLBACK cb) {
  lp.cb=cb;
  for (list<oListInfo>::iterator it = next.begin(); it != next.end(); ++it) {
    it->setCallback(cb);
  }
}

void oEvent::generateListInfo(const gdioutput& target, oListParam &par, oListInfo &li) {
  vector<oListParam> parV(1, par);
  generateListInfo(target, parV, li);
}

void oEvent::generateListInfo(const gdioutput& target, vector<oListParam> &par, oListInfo &li) {
  li.getParam().sourceParam = -1;// Reset source
  loadGeneralResults(false, false);
  for (size_t k = 0; k < par.size(); k++) {
    par[k].cb = 0;
  }

  map<EStdListType, oListInfo> listMap;
  getListTypes(listMap, false);

  if (par.size() == 1) {
    generateListInfoAux(target, par[0], li, listMap[par[0].listCode].Name);
    set<int> used;
    // Add linked lists
    oListParam *cPar = &par[0];
    while (cPar->nextList>0) {
      if (used.count(cPar->nextList))
        break; // Circular definition
      used.insert(cPar->nextList);
      used.insert(cPar->previousList);
      oListParam &nextPar = oe->getListContainer().getParam(cPar->nextList-1);
      li.next.push_back(oListInfo());
      nextPar.cb = 0;
      generateListInfoAux(target, nextPar, li.next.back(), L"");
      cPar = &nextPar;
    }
  }
  else {
    for (size_t k = 0; k < par.size(); k++) {
      if (k > 0) {
        li.next.push_back(oListInfo());
      }
      generateListInfoAux(target, par[k], k == 0 ? li : li.next.back(), 
                          li.Name = listMap[par[0].listCode].Name);
    }
  }
}

extern Image image;

void oEvent::generateListInfoAux(const gdioutput& target, oListParam &par, oListInfo &li, const wstring &name) {
  const int lh=14;
  const int vspace=lh/2;
  int bib;
  pair<int, bool> ln;
  const double scale = 1.8;

  wstring ttl;
  Position pos;

  pClass sampleClass = 0;
  if (!par.selection.empty())
    sampleClass = getClass(*par.selection.begin());
  if (!sampleClass && !Classes.empty())
    sampleClass = &*Classes.begin();

  EStdListType lt=par.listCode;
  li=oListInfo();
  li.lp = par;
  li.Name = name;
  li.lp.defaultName = li.Name;
  if (par.defaultName.empty())
    par.defaultName = li.Name;
  if (par.name.empty())
    par.name = li.Name;
  if (li.lp.name.empty())
    li.lp.name = li.Name;

  auto getMaxCharWidth = [&li, this, &par](EPostType type, int minSize = 0) {
    return li.getMaxCharWidth(*this, par.selection, type, -1, L"", normalText, nullptr, false, minSize);
  };

  switch (lt) {
    case EStdStartList: {
      li.addHead(oPrintPost(lCmpName, makeDash(lang.tl(L"Startlista - %s", true)), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));
      
      int bib = 0;
      int rank = 0;
      if (hasBib(true, false)) {
        li.addListPost(oPrintPost(lRunnerBib, L"", normalText, 0, 0));
        bib=40;
      }
      li.addListPost(oPrintPost(lRunnerStart, L"", normalText, 0+bib, 0));
      li.addListPost(oPrintPost(lPatrolNameNames, L"", normalText, 70+bib, 0));
      li.addListPost(oPrintPost(lPatrolClubNameNames, L"", normalText, 300+bib, 0));
      if (hasRank()) {
        li.addListPost(oPrintPost(lRunnerRank, L"", normalText, 470+bib, 0));
        rank = 50;
      }
      li.addListPost(oPrintPost(lRunnerCard, L"", normalText, 470+bib+rank, 0));

      li.addSubHead(oPrintPost(lClassName, L"", boldText, 0, 12));
      li.addSubHead(oPrintPost(lClassLength, lang.tl(L"%s meter"), boldText, 300+bib, 12));
      li.addSubHead(oPrintPost(lClassStartName, L"", boldText, 470+bib+rank, 12));
      li.addSubHead(oPrintPost(lString, L"", boldText, 470+bib, 16));

      li.listType=li.EBaseTypeRunner;
      li.sortOrder=ClassStartTime;
      li.setFilter(EFilterExcludeDNS);
      break;
    }

    case EStdClubStartList: {
      li.addHead(oPrintPost(lCmpName, makeDash(lang.tl(L"Klubbstartlista - %s", true)), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));

      if (hasBib(true, true)) {
        pos.add("bib", 40);
        li.addListPost(oPrintPost(lRunnerBib, L"", normalText, 0, 0));
      }

      pos.add("start", getMaxCharWidth(lRunnerStart));
      li.addListPost(oPrintPost(lRunnerStart, L"", normalText, pos.get("start"), 0));

      if (hasRank()) {
        pos.add("rank", 50);
        li.addListPost(oPrintPost(lRunnerRank, L"", normalText, pos.get("rank"), 0));
      }
      pos.add("name", getMaxCharWidth(lRunnerName));
      li.addListPost(oPrintPost(lPatrolNameNames, L"", normalText, pos.get("name"), 0));
      pos.add("class", getMaxCharWidth(lClassName));
      li.addListPost(oPrintPost(lClassName, L"", normalText, pos.get("class"), 0));
      pos.add("length", li.getMaxCharWidth(*this, par.selection, lClassLength, -1, L"%s m", normalText));
      li.addListPost(oPrintPost(lClassLength, lang.tl(L"%s m"), normalText, pos.get("length"), 0));
      pos.add("sname", getMaxCharWidth(lClassStartName));
      li.addListPost(oPrintPost(lClassStartName, L"", normalText, pos.get("sname"), 0));

      pos.add("card", 70);
      li.addListPost(oPrintPost(lRunnerCard, L"", normalText, pos.get("card"), 0));

      li.addSubHead(oPrintPost(lClubName, L"", boldText, 0, 12));
      li.addSubHead(oPrintPost(lString, L"", boldText, 100, 16));

      li.listType=li.EBaseTypeClubRunner;
      li.sortOrder=ClassTeamLeg;
      li.setFilter(EFilterExcludeDNS);
      li.setFilter(EFilterVacant);
      break;
    }

    case EStdClubResultList: {
      li.addHead(oPrintPost(lCmpName, makeDash(lang.tl(L"Klubbresultatlista - %s", true)), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));

      pos.add("class", getMaxCharWidth(lClassName));
      li.addListPost(oPrintPost(lClassName, L"", normalText, pos.get("class"), 0));
       
      pos.add("place", 40);
      li.addListPost(oPrintPost(lRunnerPlace, L"", normalText, pos.get("place"), 0));

      pos.add("name", getMaxCharWidth(lPatrolNameNames));
      li.addListPost(oPrintPost(lRunnerName, L"", normalText, pos.get("name"), 0));

      pos.add("time", getMaxCharWidth(lRunnerTimeStatus));
      li.addListPost(oPrintPost(lRunnerTimeStatus, L"", normalText, pos.get("time"), 0));

      pos.add("after", getMaxCharWidth(lRunnerTimeAfter));
      li.addListPost(oPrintPost(lRunnerTimeAfter, L"", normalText, pos.get("after"), 0));

      li.addSubHead(oPrintPost(lClubName, L"", boldText, 0, 12));
      li.addSubHead(oPrintPost(lString, L"", boldText, 100, 16));

      li.listType=li.EBaseTypeClubRunner;
      li.sortOrder=ClassResult;
      li.calcResults = true;
      li.setFilter(EFilterVacant);
      break;
    }

    case EStdRentedCard:
    {
      li.addHead(oPrintPost(lCmpName, makeDash(lang.tl(L"Hyrbricksrapport - %s", true)), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));

      li.addListPost(oPrintPost(lTotalCounter, L"%s", normalText, 0, 0));
      li.addListPost(oPrintPost(lRunnerCard, L"", normalText, 30, 0));
      li.addListPost(oPrintPost(lRunnerName, L"", normalText, 130, 0));
      li.addSubHead(oPrintPost(lClassName, L"", boldText, 0, 10));

      li.setFilter(EFilterHasCard);
      li.setFilter(EFilterRentCard);
      li.setFilter(EFilterExcludeDNS);
      li.listType=li.EBaseTypeRunner;
      li.sortOrder=ClassStartTime;
      break;
    }

    case EStdResultList: {
      wstring stitle;
      getResultTitle(*this, li.lp, stitle);
      stitle = par.getCustomTitle(stitle);

      li.addHead(oPrintPost(lCmpName, makeDash(stitle), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      pos.add("place", 25);
      pos.add("name", getMaxCharWidth(lPatrolNameNames, 25));
      pos.add("club", getMaxCharWidth(lPatrolClubNameNames, 25));
      pos.add("status", getMaxCharWidth(lRunnerTimeStatus, 25));
      pos.add("after", getMaxCharWidth(lRunnerTimeAfter, 25));
      pos.add("missed", 50);

      li.addSubHead(oPrintPost(lClassName, L"", boldText, 0, 10));

      li.addListPost(oPrintPost(lRunnerPlace, L"", normalText, pos.get("place"), 0));
      li.addListPost(oPrintPost(lPatrolNameNames, L"", normalText, pos.get("name"), 0));
      li.addListPost(oPrintPost(lPatrolClubNameNames, L"", normalText, pos.get("club"), 0));

      if (li.lp.useControlIdResultTo<=0 && li.lp.useControlIdResultFrom<=0) {
        li.addSubHead(oPrintPost(lClassResultFraction, L"", boldText, pos.get("club"), 10));

        li.addListPost(oPrintPost(lRunnerTimeStatus, L"", normalText, pos.get("status"), 0));
        li.addListPost(oPrintPost(lRunnerTimeAfter, L"", normalText, pos.get("after"), 0));
        if (li.lp.showInterTimes) {
          li.addSubListPost(oPrintPost(lPunchNamedTime, L"", italicSmall, pos.get("name"), 0, make_pair(1, true)));
          li.subListPost.back().fixedWidth = 160;
          li.listSubType = li.EBaseTypeCoursePunches;
        }
        else if (li.lp.showSplitTimes) {
          li.addSubListPost(oPrintPost(lPunchTime, L"", italicSmall, pos.get("name"), 0, make_pair(1, true)));
          li.subListPost.back().fixedWidth = 95;
          li.listSubType = li.EBaseTypeCoursePunches;
        }
      }
      else {
        li.needPunches = oListInfo::PunchMode::SpecificPunch;
        li.addListPost(oPrintPost(lRunnerTempTimeStatus, L"", normalText, pos.get("status"), 0));
        li.addListPost(oPrintPost(lRunnerTempTimeAfter, L"", normalText, pos.get("after"), 0));
      }
      li.addSubHead(oPrintPost(lString, lang.tl(L"Tid"), boldText, pos.get("status"), 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Efter"), boldText, pos.get("after"), 10));

      if (li.lp.splitAnalysis)  {
        li.addListPost(oPrintPost(lRunnerLostTime, L"", normalText, pos.get("missed"), 0));
        li.addSubHead(oPrintPost(lString, lang.tl(L"Bomtid"), boldText, pos.get("missed"), 10));
      }

      li.calcResults = true;
      li.listType=li.EBaseTypeRunner;
      li.sortOrder=ClassResult;
      li.supportFrom = true;
      li.supportTo = true;
      li.setFilter(EFilterHasPrelResult);
      li.setFilter(EFilterExcludeCANCEL);
      break;
    }
    case EGeneralResultList: {
      wstring stitle;
      getResultTitle(*this, li.lp, stitle);
      stitle = par.getCustomTitle(stitle);

      gdiFonts normal, header, small;
      double s;

      if (par.useLargeSize) {
        s = scale;
        normal = fontLarge;
        header = boldLarge;
        small = normalText;
      }
      else {
        s = 1.0;
        normal = normalText;
        header = boldText;
        small = italicSmall;
      }

      li.addHead(oPrintPost(lCmpName, makeDash(stitle), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      pos.add("place", getMaxCharWidth(lRunnerGeneralPlace));
      pos.add("name", getMaxCharWidth(lRunnerCompleteName));
      pos.add("status", getMaxCharWidth(lRunnerGeneralTimeStatus));
      pos.add("after", getMaxCharWidth(lRunnerGeneralTimeAfter));
      pos.add("missed", 50);

      li.addSubHead(oPrintPost(lClassName, L"", header, 0, 10));

      li.addListPost(oPrintPost(lRunnerGeneralPlace, L"", normal, pos.get("place", s), 0));
      li.addListPost(oPrintPost(lRunnerCompleteName, L"", normal, pos.get("name", s), 0));

      if (li.lp.useControlIdResultTo<=0 && li.lp.useControlIdResultFrom<=0) {
        li.addListPost(oPrintPost(lRunnerGeneralTimeStatus, L"", normal, pos.get("status", s), 0));
        li.addListPost(oPrintPost(lRunnerGeneralTimeAfter, L"", normal, pos.get("after", s), 0));
        if (li.lp.showInterTimes) {
          li.addSubListPost(oPrintPost(lPunchNamedTime, L"", small, pos.get("name", s), 0, make_pair(1, true)));
          li.subListPost.back().fixedWidth = 160;
          li.listSubType = li.EBaseTypeCoursePunches;
        }
        else if (li.lp.showSplitTimes) {
          li.addSubListPost(oPrintPost(lPunchTime, L"", small, pos.get("name", s), 0, make_pair(1, true)));
          li.subListPost.back().fixedWidth = 95;
          li.listSubType = li.EBaseTypeCoursePunches;
        }
      }
      else {
        li.needPunches = oListInfo::PunchMode::SpecificPunch;
        li.addListPost(oPrintPost(lRunnerTempTimeStatus, L"", normal, pos.get("status", s), 0));
        li.addListPost(oPrintPost(lRunnerTempTimeAfter, L"", normal, pos.get("after", s), 0));
      }
      li.addSubHead(oPrintPost(lString, lang.tl(L"Tid"), header, pos.get("status", s), 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Efter"), header, pos.get("after", s), 10));

      li.calcResults = true;
      li.listType=li.EBaseTypeRunner;
      li.sortOrder=ClassResult;
      li.setFilter(EFilterHasPrelResult);
      li.setFilter(EFilterExcludeCANCEL);
      li.supportFrom = true;
      li.supportTo = true;
      li.calcTotalResults = true;
      break;
    }
    case EIndPriceList:

      li.addHead(oPrintPost(lCmpName, makeDash(lang.tl(L"Avgjorda placeringar - %s")), boldLarge, 0,0));
      li.addHead(oPrintPost(lCurrentTime, lang.tl(L"Genererad: ") + L"%s", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      pos.add("place", 25);
      pos.add("name", getMaxCharWidth(lPatrolNameNames));
      pos.add("club", getMaxCharWidth(lPatrolClubNameNames));
      pos.add("status", 80);
      pos.add("info", 80);

      li.addSubHead(oPrintPost(lClassName, L"", boldText, 0, 10));
      li.addSubHead(oPrintPost(lClassResultFraction, L"", boldText, pos.get("club"), 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Tid"), boldText, pos.get("status"), 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Avgörs kl"), boldText, pos.get("info"), 10));

      li.addListPost(oPrintPost(lRunnerPlace, L"", normalText, pos.get("place"), 0));
      li.addListPost(oPrintPost(lPatrolNameNames, L"", normalText, pos.get("name"), 0));
      li.addListPost(oPrintPost(lPatrolClubNameNames, L"", normalText, pos.get("club"), 0));
      li.addListPost(oPrintPost(lRunnerTimeStatus, L"", normalText, pos.get("status"), 0));
      li.addListPost(oPrintPost(lRunnerTimePlaceFixed, L"", normalText, pos.get("info"), 0));

      li.calcResults = true;
      li.listType = li.EBaseTypeRunner;
      li.sortOrder = ClassResult;
      li.setFilter(EFilterHasResult);
      li.setFilter(EFilterExcludeCANCEL);

      break;

    case EStdTeamResultList:
      li.addHead(oPrintPost(lCmpName, makeDash(lang.tl(L"Resultatsammanställning - %s")), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      li.addSubHead(oPrintPost(lClassName, L"", boldText, 0, 14));
      li.addSubHead(oPrintPost(lClassResultFraction, L"", normalText, 280, 14));

      //Use last leg for every team (index=-1)
      li.addListPost(oPrintPost(lTeamPlace, L"", normalText, 0, 5, make_pair(-1, true)));
      li.addListPost(oPrintPost(lTeamName, L"", normalText, 25, 5, make_pair(-1, true)));
      li.addListPost(oPrintPost(lTeamTimeStatus, L"", normalText, 280, 5, make_pair(-1, true)));
      li.addListPost(oPrintPost(lTeamTimeAfter, L"", normalText, 340, 5, make_pair(-1, true)));

      li.lp.setLegNumberCoded(-1);
      li.calcResults=true;
      li.listType=li.EBaseTypeTeam;
      li.listSubType=li.EBaseTypeRunner;
      li.sortOrder=ClassResult;
      li.setFilter(EFilterHasResult);
      li.setFilter(EFilterExcludeCANCEL);
      break;

    case EStdTeamResultListAll:
      li.addHead(oPrintPost(lCmpName, makeDash(lang.tl(L"Resultat - %s")), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      li.addSubHead(oPrintPost(lClassName, L"", boldText, 0, 14));
      li.addSubHead(oPrintPost(lClassResultFraction, L"", boldText, 280, 14));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Tid"), boldText, 400, 14));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Efter"), boldText, 460, 14));

      //Use last leg for every team (index=-1)
      li.addListPost(oPrintPost(lTeamPlace, L"", normalText, 0, 5, make_pair(-1, true)));
      li.addListPost(oPrintPost(lTeamName, L"", normalText, 25, 5, make_pair(-1, true)));
      li.addListPost(oPrintPost(lTeamTimeStatus, L"", normalText, 400, 5, make_pair(-1, true)));
      li.addListPost(oPrintPost(lTeamTimeAfter, L"", normalText, 460, 5, make_pair(-1, true)));

      li.addSubListPost(oPrintPost(lRunnerLegNumberAlpha, L"%s.", normalText, 25, 0, make_pair(-1, true)));
      li.addSubListPost(oPrintPost(lRunnerName, L"", normalText, 45, 0, make_pair(-1, true)));
      li.addSubListPost(oPrintPost(lRunnerTimeStatus, L"", normalText, 280, 0, make_pair(-1, true)));
      li.addSubListPost(oPrintPost(lTeamLegTimeStatus, L"", normalText, 400, 0, make_pair(-1, true)));
      li.addSubListPost(oPrintPost(lTeamLegTimeAfter, L"", normalText, 460, 0, make_pair(-1, true)));

      if (li.lp.splitAnalysis)  {
        li.addSubListPost(oPrintPost(lRunnerLostTime, L"", normalText, 510, 0));
        li.addSubHead(oPrintPost(lString, lang.tl(L"Bomtid"), boldText, 510, 14));
      }

      li.lp.setLegNumberCoded(-1);
      li.calcResults=true;
      li.listType=li.EBaseTypeTeam;
      li.listSubType=li.EBaseTypeRunner;
      li.sortOrder=ClassResult;
      li.setFilter(EFilterHasResult);
      li.setFilter(EFilterExcludeCANCEL);
      break;

    case unused_EStdTeamResultListLeg: {
      wchar_t title[256];
      if (li.lp.getLegNumberCoded() != 1000)
        swprintf(title, sizeof(title)/sizeof(wchar_t), (lang.tl(L"Resultat efter sträcka X#" + li.lp.getLegName())+L" - %%s").c_str());
      else
        swprintf(title, sizeof(title)/sizeof(wchar_t), (lang.tl(L"Resultat efter sträckan")+L" - %%s").c_str());

      pos.add("place", 25);
      pos.add("team", getMaxCharWidth(lTeamName));
      pos.add("name", getMaxCharWidth(lRunnerName));
      pos.add("status", 50);
      pos.add("teamstatus", 50);
      pos.add("after", 50);
      pos.add("missed", 50);

      li.addHead(oPrintPost(lCmpName, makeDash(title), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      li.addSubHead(oPrintPost(lClassName, L"", boldText, pos.get("place"), 14));
      li.addSubHead(oPrintPost(lClassResultFraction, L"", boldText, pos.get("name"), 14));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Tid"), boldText, pos.get("status"), 14));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Totalt"), boldText, pos.get("teamstatus"), 14));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Efter"), boldText, pos.get("after"), 14));

      ln = li.lp.getLegInfo(sampleClass);
      li.addListPost(oPrintPost(lTeamPlace, L"", normalText, pos.get("place"), 2, ln));
      li.addListPost(oPrintPost(lTeamName, L"", normalText, pos.get("team"), 2, ln));
      li.addListPost(oPrintPost(lRunnerName, L"", normalText, pos.get("name"), 2, ln));
      li.addListPost(oPrintPost(lRunnerTimeStatus, L"", normalText, pos.get("status"), 2, ln));
      li.addListPost(oPrintPost(lTeamTimeStatus, L"", normalText, pos.get("teamstatus"), 2, ln));
      li.addListPost(oPrintPost(lTeamTimeAfter, L"", normalText, pos.get("after"), 2, ln));

      if (li.lp.splitAnalysis)  {
        li.addListPost(oPrintPost(lRunnerLostTime, L"", normalText, pos.get("missed"), 2, ln));
        li.addSubHead(oPrintPost(lString, lang.tl(L"Bomtid"), boldText, pos.get("missed"), 14));
      }

      li.calcResults=true;
      li.listType=li.EBaseTypeTeam;
      li.sortOrder=ClassResult;
      li.setFilter(EFilterHasResult);
      break;
    }
    case unused_EStdTeamResultListLegLARGE: {
      wchar_t title[256];
      if (li.lp.getLegNumberCoded() != 1000)
        swprintf(title, sizeof(title)/sizeof(wchar_t), (L"%%s - "+lang.tl(L"sträcka X#" + li.lp.getLegName())).c_str());
      else
        swprintf(title, sizeof(title)/sizeof(wchar_t), (L"%%s - "+lang.tl(L"slutsträckan")).c_str());

      pos.add("place", 25);
      pos.add("team", min(120, getMaxCharWidth(lTeamName)));
      pos.add("name", min(120, getMaxCharWidth(lRunnerName)));
      pos.add("status", 50);
      pos.add("teamstatus", 50);
      pos.add("after", 50);
      pos.add("missed", 50);

      li.addSubHead(oPrintPost(lClassName, makeDash(title), boldLarge, pos.get("place", scale), 14));
      li.addSubHead(oPrintPost(lClassResultFraction, L"", boldLarge, pos.get("name", scale), 14));

      li.addSubHead(oPrintPost(lString, lang.tl(L"Tid"), boldLarge, pos.get("status", scale), 14));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Totalt"), boldLarge, pos.get("teamstatus", scale), 14));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Efter"), boldLarge, pos.get("after", scale), 14));

      ln = li.lp.getLegInfo(sampleClass);
      li.addListPost(oPrintPost(lTeamPlace, L"", fontLarge, pos.get("place", scale), 5, ln));
      li.addListPost(oPrintPost(lTeamName, L"", fontLarge, pos.get("team", scale), 5, ln));
      li.addListPost(oPrintPost(lRunnerName, L"", fontLarge, pos.get("name", scale), 5, ln));
      li.addListPost(oPrintPost(lRunnerTimeStatus, L"", fontLarge, pos.get("status", scale), 5, ln));
      li.addListPost(oPrintPost(lTeamTimeStatus, L"", fontLarge, pos.get("teamstatus", scale), 5, ln));
      li.addListPost(oPrintPost(lTeamTimeAfter, L"", fontLarge, pos.get("after", scale), 5, ln));

      if (li.lp.splitAnalysis)  {
        li.addListPost(oPrintPost(lRunnerLostTime, L"", fontLarge, pos.get("missed", scale), 5, ln));
        li.addSubHead(oPrintPost(lString, lang.tl(L"Bomtid"), boldLarge, pos.get("missed", scale), 14));
      }

      li.calcResults=true;
      li.listType=li.EBaseTypeTeam;
      li.sortOrder=ClassResult;
      li.setFilter(EFilterHasResult);
      break;
    }
    case EStdTeamStartList:
      {
        MetaList mList;
        mList.setListName(L"Startlista, stafett");
        mList.addToHead(lCmpName).setText(L"Startlista - X");;
        mList.newHead();
        mList.addToHead(lCmpDate);

        mList.addToSubHead(lClassName);
        mList.addToSubHead(MetaListPost(lClassStartTime, lNone));
        mList.addToSubHead(MetaListPost(lClassLength, lNone)).setText(L"X meter");
        mList.addToSubHead(MetaListPost(lClassStartName, lNone));

        mList.addToList(lTeamBib);
        mList.addToList(lTeamStartCond);
        mList.addToList(lTeamName);

        mList.addToSubList(lRunnerLegNumberAlpha).align(lTeamName, false).setText(L"X.");
        mList.addToSubList(lRunnerName);
        mList.addToSubList(lRunnerCard).align(lClassStartName);

        mList.interpret(this, target, par, li);
      }
      li.listType=li.EBaseTypeTeam;
      li.listSubType=li.EBaseTypeRunner;
      //li.setFilter(EFilterExcludeDNS);
      li.setSubFilter(ESubFilterVacant);
      li.sortOrder = ClassStartTime;
      break;

    case ETeamCourseList:
      li.addHead(oPrintPost(lCmpName, makeDash(lang.tl(L"Bantilldelningslista - %s")), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));
      li.listType=li.EBaseTypeTeam;

      bib=0;
      li.addListPost(oPrintPost(lTeamBib, L"", normalText, 0+bib, 4));
      li.addListPost(oPrintPost(lTeamName, L"", normalText, 50+bib, 4));
      li.addListPost(oPrintPost(lTeamClub, L"", normalText, 300+bib, 4));

      li.listSubType=li.EBaseTypeRunner;
      li.addSubListPost(oPrintPost(lRunnerLegNumberAlpha, L"%s.", normalText, 25+bib, 0, make_pair(-1, true)));
      li.addSubListPost(oPrintPost(lRunnerName, L"", normalText, 50+bib, 0, make_pair(-1, true)));
      li.addSubListPost(oPrintPost(lRunnerCard, L"", normalText, 300+bib, 0, make_pair(-1, true)));
      li.addSubListPost(oPrintPost(lRunnerCourse, L"", normalText, 400+bib, 0, make_pair(-1, true)));

      li.addSubHead(oPrintPost(lClassName, L"", boldText, 0, 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Bricka"), boldText, 300+bib, 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Bana"),   boldText, 400+bib, 10));

      li.sortOrder = ClassStartTime;
      break;

    case EIndCourseList: {
      li.addHead(oPrintPost(lCmpName, makeDash(lang.tl(L"Bantilldelningslista - %s")), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));
      li.listType=li.EBaseTypeRunner;

      bib=0;
      li.addListPost(oPrintPost(lRunnerBib, L"", normalText, 0+bib, 0));
      oPrintPost &rn = li.addListPost(oPrintPost(lRunnerName, L"", normalText, 50+bib, 0));
      rn.doMergeNext = true;
      li.addListPost(oPrintPost(lRunnerClub, L" (%s)", normalText, 50+bib, 0));
      li.addListPost(oPrintPost(lRunnerCard, L"", normalText, 300+bib, 0));
      li.addListPost(oPrintPost(lRunnerCourse, L"", normalText, 400+bib, 0));

      li.addSubHead(oPrintPost(lClassName, L"", boldText, 0, 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Bricka"), boldText, 300+bib, 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Bana"),   boldText, 400+bib, 10));

      li.sortOrder = ClassStartTime;
      break;
    }
    case EStdTeamStartListLeg: {
      if (li.lp.getLegNumberCoded() == 1000)
        throw std::exception("Ogiltigt val av sträcka");

      li.addHead(oPrintPost(lCmpName, makeDash(lang.tl(L"Startlista X - sträcka Y#%s#" + li.lp.getLegName(), true)), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));

      ln=li.lp.getLegInfo(sampleClass);
      li.listType=li.EBaseTypeTeam;
      bib=0;
      if (hasBib(false, true)) {
        li.addListPost(oPrintPost(lTeamBib, L"", normalText, 0, 0));
        bib=40;
      }
      li.addListPost(oPrintPost(lTeamStart, L"", normalText, 0+bib, 0, ln));
      li.addListPost(oPrintPost(lTeamName, L"", normalText, 70+bib, 0, ln));
      li.addListPost(oPrintPost(lTeamRunner, L"", normalText, 300+bib, 0, ln));
      li.addListPost(oPrintPost(lTeamRunnerCard, L"", normalText, 520+bib, 0, ln));

      li.addSubHead(oPrintPost(lClassName, L"", boldText, 0, 10));
      li.addSubHead(oPrintPost(lClassStartName, L"", normalText, 300+bib, 10));

      li.sortOrder=ClassStartTime;
      //li.setFilter(EFilterExcludeDNS);
      break;
    }
    case EStdIndMultiStartListLeg:
      if (li.lp.getLegNumberCoded() == 1000)
        throw std::exception("Ogiltigt val av sträcka");

      //sprintf_s(title, lang.tl("Startlista lopp %d - %%s").c_str(), li.lp.legNumber+1);
      ln=li.lp.getLegInfo(sampleClass);

      ttl = makeDash(lang.tl(L"Startlista lopp X - Y#" + li.lp.getLegName() + L"#%s", true));
      li.addHead(oPrintPost(lCmpName, ttl, boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));

      li.listType=li.EBaseTypeTeam;
      bib=0;
      if (hasBib(false, true)) {
        li.addListPost(oPrintPost(lTeamBib, L"", normalText, 0, 0));
        bib=40;
      }
      li.addListPost(oPrintPost(lTeamStart, L"", normalText, 0+bib, 0, ln));
      li.addListPost(oPrintPost(lTeamRunner, L"", normalText, 70+bib, 0, ln));
      li.addListPost(oPrintPost(lTeamClub, L"", normalText, 300+bib, 0, ln));
      li.addListPost(oPrintPost(lTeamRunnerCard, L"", normalText, 500+bib, 0, ln));

      li.addSubHead(oPrintPost(lClassName, L"", boldText, 0, 10));
      li.addSubHead(oPrintPost(lClassLength, lang.tl(L"%s meter"), boldText, 300+bib, 10, ln));
      li.addSubHead(oPrintPost(lClassStartName, L"", boldText, 500+bib, 10, ln));

      li.sortOrder=ClassStartTime;
      li.setFilter(EFilterExcludeDNS);
      break;

    case EStdIndMultiResultListLeg:
      ln=li.lp.getLegInfo(sampleClass);

      if (li.lp.getLegNumberCoded() != 1000)
        ttl = lang.tl(L"Resultat lopp X - Y#" + li.lp.getLegName() + L"#%s");
      else
        ttl = lang.tl(L"Resultat - X#%s");

      li.addHead(oPrintPost(lCmpName, makeDash(ttl), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      li.addSubHead(oPrintPost(lClassName, L"", boldText, 0, 14));
      li.addSubHead(oPrintPost(lClassResultFraction, L"", boldText, 260, 14));

      li.addSubHead(oPrintPost(lString, lang.tl(L"Tid"), boldText, 460, 14));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Totalt"), boldText, 510, 14));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Efter"), boldText, 560, 14));

      li.addListPost(oPrintPost(lTeamPlace, L"", normalText, 0, 0, ln));
      li.addListPost(oPrintPost(lRunnerName, L"", normalText, 40, 0, ln));
      li.addListPost(oPrintPost(lRunnerClub, L"", normalText, 260, 0, ln));

      li.addListPost(oPrintPost(lRunnerTimeStatus, L"", normalText, 460, 0, ln));
      li.addListPost(oPrintPost(lTeamTimeStatus, L"", normalText, 510, 0, ln));
      li.addListPost(oPrintPost(lTeamTimeAfter, L"", normalText, 560, 0, ln));

      if (li.lp.splitAnalysis)  {
        li.addListPost(oPrintPost(lRunnerLostTime, L"", normalText, 620, 0, ln));
        li.addSubHead(oPrintPost(lString, lang.tl(L"Bomtid"), boldText, 620, 14));
      }

      li.calcResults=true;
      li.listType=li.EBaseTypeTeam;
      li.sortOrder=ClassResult;
      li.setFilter(EFilterHasResult);
      li.setFilter(EFilterExcludeCANCEL);

      break;

    case EStdIndMultiResultListLegLARGE:
      if (li.lp.getLegNumberCoded() == 1000)
        throw std::exception("Ogiltigt val av sträcka");

      ln=li.lp.getLegInfo(sampleClass);

      pos.add("place", 25);
      pos.add("name", getMaxCharWidth(lRunnerName));
      pos.add("club", getMaxCharWidth(lRunnerClub));

      pos.add("status", 50);
      pos.add("teamstatus", 50);
      pos.add("after", 50);

      ttl = L"%s - " + lang.tl(L"Lopp ") + li.lp.getLegName();
      li.addSubHead(oPrintPost(lClassName, makeDash(ttl), boldLarge, pos.get("place", scale), 14));
      li.addSubHead(oPrintPost(lClassResultFraction, L"", boldLarge, pos.get("club", scale), 14));

      li.addSubHead(oPrintPost(lString, lang.tl(L"Tid"), boldLarge, pos.get("status", scale), 14));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Totalt"), boldLarge, pos.get("teamstatus", scale), 14));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Efter"), boldLarge, pos.get("after", scale), 14));

      li.addListPost(oPrintPost(lTeamPlace, L"", fontLarge, pos.get("place", scale), 0, ln));
      li.addListPost(oPrintPost(lRunnerName, L"", fontLarge, pos.get("name", scale), 0, ln));
      li.addListPost(oPrintPost(lRunnerClub, L"", fontLarge, pos.get("club", scale), 0, ln));

      li.addListPost(oPrintPost(lRunnerTimeStatus, L"", fontLarge, pos.get("status", scale), 0, ln));
      li.addListPost(oPrintPost(lTeamTimeStatus, L"", fontLarge, pos.get("teamstatus", scale), 0, ln));
      li.addListPost(oPrintPost(lTeamTimeAfter, L"", fontLarge, pos.get("after", scale), 0, ln));

      li.calcResults=true;
      li.listType=li.EBaseTypeTeam;
      li.sortOrder=ClassResult;
      li.setFilter(EFilterHasResult);
      li.setFilter(EFilterExcludeCANCEL);

      break;

    case EStdIndMultiResultListAll:
      li.addHead(oPrintPost(lCmpName, makeDash(lang.tl(L"Resultat - %s")), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      li.addSubHead(oPrintPost(lClassName, L"", boldText, 0, 14));
      li.addSubHead(oPrintPost(lClassResultFraction, L"", boldText, 280, 14));
      li.addSubHead(oPrintPost(lClassResultFraction, lang.tl(L"Tid"), boldText, 480, 14));
      li.addSubHead(oPrintPost(lClassResultFraction, lang.tl(L"Efter"), boldText, 540, 14));

      //Use last leg for every team (index=-1)
      li.addListPost(oPrintPost(lTeamPlace, L"", normalText, 0, 5, make_pair(-1, true)));
      li.addListPost(oPrintPost(lRunnerName, L"", normalText, 25, 5, make_pair(-1, true)));
      li.addListPost(oPrintPost(lRunnerClub, L"", normalText, 280, 5, make_pair(-1, true)));

      li.addListPost(oPrintPost(lTeamTimeStatus, L"", normalText, 480, 5, make_pair(-1, true)));
      li.addListPost(oPrintPost(lTeamTimeAfter, L"", normalText, 540, 5, make_pair(-1, true)));

      li.addSubListPost(oPrintPost(lSubSubCounter, lang.tl(L"Lopp %s"), normalText, 25, 0, make_pair(-1, true)));
      li.addSubListPost(oPrintPost(lRunnerTimeStatus, L"", normalText, 90, 0, make_pair(-1, true)));
      li.addSubListPost(oPrintPost(lTeamLegTimeStatus, L"", normalText, 150, 0, make_pair(-1, true)));
      li.addSubListPost(oPrintPost(lTeamLegTimeAfter, L"", normalText, 210, 0, make_pair(-1, true)));

      li.lp.setLegNumberCoded(-1);
      li.calcResults=true;
      li.listType=li.EBaseTypeTeam;
      li.listSubType=li.EBaseTypeRunner;
      li.sortOrder=ClassResult;
      li.setFilter(EFilterHasResult);
      li.setFilter(EFilterExcludeCANCEL);

      break;
    case EStdPatrolStartList:
    {
      MetaList mList;
      mList.setListName(L"Startlista, patrull");

      mList.addToHead(lCmpName).setText(L"Startlista - X").align(false);
      mList.newHead();
      mList.addToHead(lCmpDate).align(false);

      mList.addToSubHead(lClassName);
      mList.addToSubHead(lClassStartTime);
      mList.addToSubHead(lClassLength).setText(L"X meter");
      mList.addToSubHead(lClassStartName);

      mList.addToList(lTeamBib);
      mList.addToList(lTeamStartCond).setLeg(0);
      mList.addToList(lTeamName);

      mList.newListRow();

      mList.addToList(MetaListPost(lTeamRunner, lTeamName, 0));
      mList.addToList(MetaListPost(lTeamRunnerCard, lAlignNext, 0));
      mList.addToList(MetaListPost(lTeamRunner, lAlignNext, 1));
      mList.addToList(MetaListPost(lTeamRunnerCard, lAlignNext, 1));

      mList.setListType(li.EBaseTypeTeam);
      mList.setSortOrder(ClassStartTime);
      mList.addFilter(EFilterExcludeDNS);
      mList.interpret(this, target, par, li);
      break;
    }
    case EStdPatrolResultList:
      li.addHead(oPrintPost(lCmpName, makeDash(lang.tl(L"Resultatlista - %s")), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      li.addListPost(oPrintPost(lTeamPlace, L"", normalText, 0, vspace, make_pair(1, true)));
      li.addListPost(oPrintPost(lTeamName, L"", normalText, 70, vspace));
      //li.addListPost(oPrintPost(lTeamClub, "", normalText, 250, vspace));
      li.addListPost(oPrintPost(lTeamTimeStatus, L"", normalText, 400, vspace, make_pair(1, true)));
      li.addListPost(oPrintPost(lTeamTimeAfter, L"", normalText, 460, vspace, make_pair(1, true)));

      li.addListPost(oPrintPost(lTeamRunner, L"", normalText, 70, lh+vspace, make_pair(0, true)));
      li.addListPost(oPrintPost(lTeamRunner, L"", normalText, 250, lh+vspace, make_pair(1, true)));
      li.setFilter(EFilterHasResult);

      li.addSubHead(oPrintPost(lClassName, L"", boldText, 0, 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Tid"), boldText, 400, 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Efter"), boldText, 460, 10));

      if (li.lp.showInterTimes) {
        li.addSubListPost(oPrintPost(lPunchNamedTime, L"", normalText, 10, 0, make_pair(1, true)));
        li.subListPost.back().fixedWidth=160;
        li.listSubType=li.EBaseTypeCoursePunches;
      }

      if (li.lp.splitAnalysis)  {
        li.addListPost(oPrintPost(lRunnerLostTime, L"", normalText, 520, vspace, make_pair(1, true)));
        li.addSubHead(oPrintPost(lString, lang.tl(L"Bomtid"), boldText, 520, 10));
      }

      li.listType=li.EBaseTypeTeam;
      li.sortOrder=ClassResult;
      li.lp.setLegNumberCoded(-1);
      li.calcResults=true;
      li.setFilter(EFilterExcludeCANCEL);

      break;

    case EStdPatrolResultListLARGE:
      pos.add("place", 25);
      pos.add("team", int(0.7*max(getMaxCharWidth(lTeamName),
                        getMaxCharWidth(lPatrolNameNames))));

      pos.add("status", 50);
      pos.add("after", 50);
      pos.add("missed", 50);

      li.addListPost(oPrintPost(lTeamPlace, L"", fontLarge, pos.get("place", scale), vspace, make_pair(1, true)));
      li.addListPost(oPrintPost(lTeamName, L"", fontLarge, pos.get("team", scale), vspace));
      li.addListPost(oPrintPost(lTeamTimeStatus, L"", fontLarge, pos.get("status", scale), vspace, make_pair(1, true)));
      li.addListPost(oPrintPost(lTeamTimeAfter, L"", fontLarge, pos.get("after", scale), vspace, make_pair(1, true)));

      li.addListPost(oPrintPost(lPatrolNameNames, L"", fontLarge, pos.get("team", scale), 25+vspace, make_pair(0, true)));
      //li.addListPost(oPrintPost(lTeamRunner, "", fontLarge, pos.get("status", scale), 25+vspace, 1));

      li.addSubHead(oPrintPost(lClassName, L"", boldLarge, 0, 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Tid"), boldLarge, pos.get("status", scale), 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Efter"), boldLarge, pos.get("after", scale), 10));

      if (li.lp.showInterTimes) {
        li.addSubListPost(oPrintPost(lPunchNamedTime, L"", normalText, 10, 0, make_pair(1, true)));
        li.subListPost.back().fixedWidth=200;
        li.listSubType=li.EBaseTypeCoursePunches;
      }

      if (li.lp.splitAnalysis)  {
        li.addListPost(oPrintPost(lRunnerLostTime, L"", fontLarge, pos.get("missed", scale), vspace, make_pair(0, true)));
        li.addSubHead(oPrintPost(lString, lang.tl(L"Bomtid"), boldLarge, pos.get("missed", scale), 10));
      }

      li.setFilter(EFilterHasResult);
      li.setFilter(EFilterExcludeCANCEL);

      li.listType=li.EBaseTypeTeam;
      li.sortOrder=ClassResult;
      li.lp.setLegNumberCoded(-1);
      li.calcResults=true;
      break;

    case unused_EStdRaidResultListLARGE:
      li.addListPost(oPrintPost(lTeamPlace, L"", fontLarge, 0, vspace));
      li.addListPost(oPrintPost(lTeamName, L"", fontLarge, 40, vspace));
      li.addListPost(oPrintPost(lTeamTimeStatus, L"", fontLarge, 490, vspace));

      li.addListPost(oPrintPost(lTeamRunner, L"", fontLarge, 40, 25+vspace, make_pair(0, true)));
      li.addListPost(oPrintPost(lTeamRunner, L"", fontLarge, 300, 25+vspace, make_pair(1, true)));

      li.addSubListPost(oPrintPost(lPunchNamedTime, L"", fontMedium, 0, 2, make_pair(1, true)));
      li.subListPost.back().fixedWidth=200;
      li.setFilter(EFilterHasResult);

      li.addSubHead(oPrintPost(lClassName, L"", boldLarge, 0, 10));

      li.listType=li.EBaseTypeTeam;
      li.listSubType=li.EBaseTypeCoursePunches;
      li.sortOrder=ClassResult;
      li.calcResults=true;
      break;

   case EStdResultListLARGE:
      pos.add("place", 25);
      pos.add("name", li.getMaxCharWidth(*this, par.selection, lPatrolNameNames, -1, L"", normalText, 0, true));
      pos.add("club", li.getMaxCharWidth(*this, par.selection, lPatrolClubNameNames, -1, L"", normalText, 0, true));
      pos.add("status", 50);
      pos.add("missed", 50);

      li.addListPost(oPrintPost(lRunnerPlace, L"", fontLarge, pos.get("place", scale), vspace));
      li.addListPost(oPrintPost(lPatrolNameNames, L"", fontLarge, pos.get("name", scale), vspace));
      li.addListPost(oPrintPost(lPatrolClubNameNames, L"", fontLarge, pos.get("club", scale), vspace));

      li.addSubHead(oPrintPost(lClassName, L"", boldLarge, pos.get("place", scale), 10));

      if (li.lp.useControlIdResultTo<=0 && li.lp.useControlIdResultFrom<=0) {
        li.addSubHead(oPrintPost(lClassResultFraction, L"", boldLarge, pos.get("club", scale), 10));
        li.addListPost(oPrintPost(lRunnerTimeStatus, L"", fontLarge, pos.get("status", scale), vspace));
      }
      else {
        li.needPunches = oListInfo::PunchMode::SpecificPunch;
        li.addListPost(oPrintPost(lRunnerTempTimeStatus, L"", normalText, pos.get("status", scale), vspace));
      }
      if (li.lp.splitAnalysis) {
        li.addSubHead(oPrintPost(lString, lang.tl(L"Bomtid"), boldLarge, pos.get("missed", scale), 10));
        li.addListPost(oPrintPost(lRunnerLostTime, L"", fontLarge, pos.get("missed", scale), vspace));
      }

      if (li.lp.showInterTimes) {
        li.addSubListPost(oPrintPost(lPunchNamedTime, L"", normalText, 0, 0, make_pair(1, true)));
        li.subListPost.back().fixedWidth = 160;
        li.listSubType = li.EBaseTypeCoursePunches;
      }
      else if (li.lp.showSplitTimes) {
        li.addSubListPost(oPrintPost(lPunchTime, L"", normalText, 0, 0, make_pair(1, true)));
        li.subListPost.back().fixedWidth = 95;
        li.listSubType = li.EBaseTypeCoursePunches;
      }

      li.setFilter(EFilterHasResult);
      li.setFilter(EFilterExcludeCANCEL);

      li.lp.setLegNumberCoded(0);
      li.listType=li.EBaseTypeRunner;
      li.sortOrder=ClassResult;
      li.calcResults=true;

      li.supportFrom = true;
      li.supportTo = true;
      break;

   case EStdUM_Master:
      li.addListPost(oPrintPost(lRunnerPlace, L"", fontMedium, 0, 0));
      li.addListPost(oPrintPost(lRunnerName, L"", fontMedium, 40, 0));
      li.addListPost(oPrintPost(lRunnerClub, L"", fontMedium, 250, 0));
      li.addListPost(oPrintPost(lClassName, L"", fontMedium, 490, 0));
      li.addListPost(oPrintPost(lRunnerUMMasterPoint, L"", fontMedium, 580, 0));

      li.setFilter(EFilterHasResult);

      li.lp.setLegNumberCoded(0);
      li.listType=li.EBaseTypeRunner;
      li.sortOrder=ClassResult;
      li.calcResults=true;
      break;

   case ERogainingInd:
      pos.add("place", 25);
      pos.add("name", getMaxCharWidth(lRunnerCompleteName));
      pos.add("points", 50);
      pos.add("status", 50);
      li.addHead(oPrintPost(lCmpName, makeDash(par.getCustomTitle(lang.tl(L"Rogainingresultat - %s"))), boldLarge, 0,0));
      li.addHead(oPrintPost(lCmpDate, L"", normalText, 0, 25));
      generateNBestHead(par, li, 25+lh);

      li.addListPost(oPrintPost(lRunnerPlace, L"", normalText, pos.get("place"), vspace));
      li.addListPost(oPrintPost(lRunnerCompleteName, L"", normalText, pos.get("name"), vspace));
      li.addListPost(oPrintPost(lRunnerRogainingPoint, L"%sp", normalText, pos.get("points"), vspace));
      li.addListPost(oPrintPost(lRunnerTimeStatus, L"", normalText, pos.get("status"), vspace));

      li.setFilter(EFilterHasResult);
      li.setFilter(EFilterExcludeCANCEL);

      if (li.lp.splitAnalysis || li.lp.showInterTimes) {
        li.addSubListPost(oPrintPost(lRogainingPunch, L"", normalText, 10, 0, make_pair(1, true)));
        li.subListPost.back().fixedWidth=130;
        li.listSubType=li.EBaseTypeCoursePunches;
      }

      li.addSubHead(oPrintPost(lClassName, L"", boldText, pos.get("place"), 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Poäng"), boldText, pos.get("points"), 10));
      li.addSubHead(oPrintPost(lString, lang.tl(L"Tid"), boldText, pos.get("status"), 10));

      li.listType=li.EBaseTypeRunner;
      li.sortOrder = ClassPoints;
      li.lp.setLegNumberCoded(-1);
      li.calcResults = true;
      li.rogainingResults = true;
    break;

    case EStdTeamAllLegLARGE: {
      vector< pair<wstring, size_t> > out;
      fillLegNumbers(par.selection, false, false, out);
      par.listCode = oe->getListContainer().getType("legresult");
      par.useLargeSize = true;
      for (size_t k = 0; k < out.size(); k++) {
        if (out[k].second >= 1000)
          continue;
        par.setLegNumberCoded(out[k].second);
        if (k == 0)
          generateListInfo(target, par, li);
        else {
          li.next.push_back(oListInfo());
          generateListInfo(target, par, li.next.back());
        }
      }
    }
    break;

    case EFixedPreReport:
    case EFixedReport:
    case EFixedInForest:
    case EFixedEconomy:
    case EFixedInvoices:
    case EFixedMinuteStartlist:
    case EFixedTimeLine:
    case EFixedLiveResult:
      li.fixedType = true;
    break;

    default:
      if (!getListContainer().interpret(this, target, par, li))
        throw meosException("Could not load list 'X'#L" + itos(par.listCode));
  }
}

string oPrintPost::encodeFont(const string &face, int factor) {
  string out(face);
  if (factor > 0 && factor != 100) {
    out += ";" + itos(factor/100) + "." + itos(factor%100);
  }
  return out;
}

wstring oPrintPost::encodeFont(const wstring &face, int factor) {
  wstring out(face);
  if (factor > 0 && factor != 100) {
    out += L";" + itow(factor/100) + L"." + itow(factor%100);
  }
  return out;
}

void oListInfo::setupLinks(const list<oPrintPost> &lst) const {
  for (list<oPrintPost>::const_iterator it = lst.begin(); it != lst.end(); ++it) {
    list<oPrintPost>::const_iterator itNext = it;
    ++itNext;
    if (itNext != lst.end() && it->doMergeNext)
      it->mergeWithTmp = &*itNext;
    else
      it->mergeWithTmp = 0;
  }
}

void oListInfo::setupLinks() const {
  setupLinks(head);
  setupLinks(subHead);
  setupLinks(listPost);
  setupLinks(subListPost);
}

void oListInfo::shrinkSize() {

  auto& scale = [](int& format) -> double {

    int highFormat = format & ~0xFF;
    format &= 0xFF;

    double s = 1.0;
    double fs = GDIImplFontSet::baseSize(format, 1.0);
    if (format == boldLarge)
      format = boldText;
    else if (format == boldHuge)
      format = boldLarge;
    else if (format == boldText)
      format = boldSmall;
    else if (format == normalText || format == fontMedium)
      format = fontSmall;
    else if (format = fontMediumPlus || format == fontLarge)
      format = fontMedium;
    else if (format == italicText)
      format = italicSmall;
    else if (format == italicMediumPlus)
      format = italicText;
    double fsNew = GDIImplFontSet::baseSize(format, 1.0);
    format |= highFormat;
    return fsNew/fs;
  };

  auto scaleForward = [](oPrintPost& pp, double scale, 
                         list<oPrintPost> &pList, map<int, int> &yOffset) {
    if (pp.xlimit > 0)
      pp.xlimit = int(pp.xlimit*scale);
    
    int minDX = -1;
    for (auto& h : pList) {
      if (h.dy == pp.dy && h.dx > pp.dx) {
        int df = h.dx - pp.dx;
        //h.dx = pp.dx + int(df * scale);
        int dx = h.dx - (pp.dx + int(df * scale));
        assert(dx >= 0);
        if (minDX == -1 || dx < minDX)
          minDX = dx;
      }
    }

    for (auto& h : pList) {
      if (h.dy == pp.dy && h.dx > pp.dx) {
        h.dx -= minDX;
      }
    }

    int dy = 0;
    for (auto& h : pList) {
      if (h.dy > pp.dy) {
        dy = h.dy - int(pp.dy + (h.dy - pp.dy) * scale);
        auto res = yOffset.emplace(pp.dy, dy);
        if (!res.second) {
          if (dy > res.first->second) {
            // Already moved. Only do the extra offset and store the highest offset.
            int newDY = dy - res.first->second;
            res.first->second = dy;
            dy = newDY;
          }
        }
        break;
      }
    }
    if (dy > 0) {
      for (auto& h : pList) {
        if (h.dy > pp.dy) {
          h.dy -= dy;
        }
      }
    }
  };

  list<oPrintPost>* allList[4] = { &head, &subHead, &listPost, &subListPost };

  // Store original position in a map
  map<int, vector<pair<int, int>>> xOffsetToListPost;
  for (int j = 0; j < 4; j++) {
    list<oPrintPost>& lp = *allList[j];
    int ix = 0;
    for (auto& h : lp) {
      int off = h.dx;
      xOffsetToListPost[off].emplace_back(j, ix);
      ix++;
    }
  }

  for (int j = 0; j < 4; j++) {
    list<oPrintPost>& lp = *allList[j];
    map<int, int> yOffsets;
    for (auto& h : lp) {
      double s = scale(h.format);
      if (s < 1.0)
        scaleForward(h, s, lp, yOffsets);
    }
  }

  map<pair<int, int>, oPrintPost*> listPostToOffsetAfter;

  for (int j = 0; j < 4; j++) {
    list<oPrintPost>& lp = *allList[j];
    int ix = 0;
    for (auto& h : lp) {
      listPostToOffsetAfter[make_pair(j, ix)] = &h;
      ix++;
    }
  }

  for (auto& it : xOffsetToListPost) {
    int minNewPos = -1;
    for (pair<int, int>& lpIx : it.second) {
      oPrintPost &pp = *listPostToOffsetAfter[lpIx];
      if (minNewPos == -1 || minNewPos > pp.dx)
        minNewPos = pp.dx;
    }

    for (pair<int, int>& lpIx : it.second) {
      oPrintPost& pp = *listPostToOffsetAfter[lpIx];
      int off = pp.dx - minNewPos;

      if (off > 0) {
        pp.dx = minNewPos;
        // TODO: Offset forward
      }
    }
  }
    

  /*for (auto& h : Head) {
    scale(h.format);
  }
  
  for (auto& h : subHead) {
    scale(h.format);
  }

  for (auto& h : listPost) {
    double s = scale(h.format);
    if (s < 1.0)
      scaleForward(h, s, listPost);
  }

  for (auto& h : subListPost) {
    scale(h.format);
  }*/
}


oListInfo::ResultType oListInfo::getResultType() const {
  return resType;
}

bool oListParam::matchLegNumber(const pClass cls, int leg)  const {
  if (cls == 0 || legNumber == -1 || leg < 0)
    return true;
  int number, order;
  cls->splitLegNumberParallel(leg, number, order);
  if (number == legNumber)
    return true;
  int sub = legNumber % 10000;
  int maj = legNumber / 10000;
  return maj == number + 1 && sub == order;
}

int oListParam::getLegNumber(const pClass cls) const {
  if (legNumber < 0)
    return legNumber;

  int sub = legNumber % 10000;
  int maj = legNumber / 10000;
  
  if (cls) {
    if (legNumber < 10000)
      return cls->getLegNumberLinear(legNumber, 0);
    else
      return cls->getLegNumberLinear(maj-1, sub);
  }
  return sub;
}

pair<int, bool> oListParam::getLegInfo(const pClass cls) const {
  if (legNumber == -1)
    return make_pair(-1, true);
  else if (legNumber < 10000)
    return make_pair(legNumber, false);

  int sub = legNumber % 10000;
  int maj = legNumber / 10000;
  
  int lin = cls ? cls->getLegNumberLinear(maj-1, sub) : sub+maj;
  
  return make_pair(lin, true);
}

wstring oListParam::getLegName() const {
  if (legNumber == -1)
    return L"";

  if (legNumber < 1000)
    return itow(legNumber + 1);

  int sub = legNumber % 10000;
  int maj = legNumber / 10000;
  
  wchar_t bf[64];
  char symb = 'a' + sub;
  swprintf(bf, sizeof(bf)/sizeof(wchar_t), L"%d%c", maj, symb);
  return bf;
}


void SplitPrintListInfo::serialize(xmlparser& xml) const {
  xml.writeBool("Split", includeSplitTimes);
  xml.writeBool("Speed", withSpeed);
  xml.writeBool("Result", withResult);
  xml.writeBool("Analysis", withAnalysis);
  xml.writeBool("Heading", withStandardHeading);
  xml.writeBool("NoAbsTime", !withAbsTime);
  xml.writeBool("NoControlCode", !withControlCode);
  xml.writeBool("TimeLoss", withTimeLoss);
  xml.writeBool("LegPlace", withLegPlace);
  xml.writeBool("AccLegPlace", withAccLegPlace);

  xml.write("NumClsResult", numClassResults);
}

void SplitPrintListInfo::deserialize(const xmlobject& xml) {
  includeSplitTimes = xml.getObjectBool("Split");
  withSpeed = xml.getObjectBool("Speed");
  withResult = xml.getObjectBool("Result");
  withAnalysis = xml.getObjectBool("Analysis");
  withStandardHeading = xml.getObjectBool("Heading");
  if (xml.got("NumClsResult"))
    numClassResults = xml.getObjectInt("NumClsResult");
  
  withAbsTime = !xml.getObjectBool("NoAbsTime");
  withControlCode = !xml.getObjectBool("NoControlCode");

  withTimeLoss = xml.getObjectBool("TimeLoss");
  withLegPlace = xml.getObjectBool("LegPlace");
  withAccLegPlace = xml.getObjectBool("AccLegPlace");
 }

int64_t SplitPrintListInfo::checkSum() const {
  int64_t ch = 0;
  ch = 2 * ch + includeSplitTimes;
  ch = 2 * ch + withSpeed;
  ch = 2 * ch + withResult;
  ch = 2 * ch + withStandardHeading;
  ch = 2 * ch + withAbsTime;
  ch = 2 * ch + withControlCode;
  ch = 2 * ch + withLegPlace;
  ch = 2 * ch + withAccLegPlace;
  ch = 2 * ch + withTimeLoss;

  ch = 997 * ch + numClassResults;
  return ch;
}

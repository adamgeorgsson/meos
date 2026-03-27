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
#include "oRunnerInternal.h"
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

vector<pRunner> oRunner::getRunnersOrdered() const {
  if (tParentRunner)
    return tParentRunner->getRunnersOrdered();

  vector<pRunner> r(multiRunner.size()+1);
  r[0] = (pRunner)this;
  for (size_t k=0;k<multiRunner.size();k++)
    r[k+1] = (pRunner)multiRunner[k];

  return r;
}

int oRunner::getMultiIndex() {
  if (!tParentRunner)
    return 0;

  const vector<pRunner> &r = tParentRunner->multiRunner;

  for (size_t k=0;k<r.size(); k++)
    if (r[k]==this)
      return k+1;

  // Error
  tParentRunner = 0;
  markForCorrection();
  return -1;
}

void oRunner::correctRemove(pRunner r) {
  for(unsigned i=0;i<multiRunner.size(); i++)
    if (r!=0 && multiRunner[i]==r) {
      multiRunner[i] = 0;
      r->tParentRunner = 0;
      r->tLeg = 0;
      r->tLegEquClass = 0;
      if (i+1==multiRunner.size())
        multiRunner.pop_back();

      correctionNeeded = true;
      r->correctionNeeded = true;
    }
}

void oEvent::updateRunnersFromDB()
{
  oRunnerList::iterator it;
  if (!oe->useRunnerDb())
    return;

  for (it=Runners.begin(); it != Runners.end(); ++it) {
    if (!it->isVacant() && !it->isRemoved())
      it->updateFromDB(it->sName, it->getClubId(), it->getClassId(false), it->getCardNo(), it->getBirthYear(), true);
  }
}

bool oRunner::updateFromDB(const wstring &name, int clubId, int classId,
                           int cardNo, int birthYear, bool forceUpdate) {
  if (!oe->useRunnerDb())
    return false;
  uint64_t oldId = getExtIdentifier();
  if (oldId && !forceUpdate && oe->getRunnerDatabase().getRunnerById(oldId) == 0)
    return false; // Keep data if database is not installed

  pRunner db_r = 0;
  if (cardNo>0) {
    db_r = oe->dbLookUpByCard(cardNo);

    if (db_r && db_r->matchName(name)) {
      //setName(db_r->getName());
      //setClub(db_r->getClub()); Don't...
      setExtIdentifier(db_r->getExtIdentifier());
      setBirthDate(db_r->getBirthDate());
      setSex(db_r->getSex());
      setNationality(db_r->getNationality());
      return true;
    }
  }

  db_r = oe->dbLookUpByName(name, clubId, classId, birthYear);

  if (db_r) {
    setExtIdentifier(db_r->getExtIdentifier());
    setBirthDate(db_r->getBirthDate());
    setSex(db_r->getSex());
    setNationality(db_r->getNationality());
    return true;
  }
  else if (getExtIdentifier()>0) {
    db_r = oe->dbLookUpById(getExtIdentifier());
    if (db_r && db_r->matchName(name)) {
      setBirthDate(db_r->getBirthDate());
      setSex(db_r->getSex());
      setNationality(db_r->getNationality());
      return true;
    }
    // Reset external identifier
    setExtIdentifier(0);
    setBirthYear(0);
    // Do not reset nationality and sex,
    // since they are likely correct.
  }

  return false;
}

void oRunner::setSex(PersonSex sex)
{
  getDI().setString("Sex", encodeSex(sex));
}

PersonSex oRunner::getSex() const
{
  return interpretSex(getDCI().getString("Sex"));
}

void oRunner::setBirthYear(int year)
{
  getDI().setInt("BirthYear", year);
}

int oRunner::getBirthYear() const
{
  return getDCI().getYear("BirthYear");
}

void oRunner::setBirthDate(const wstring& date) {
  getDI().setDate("BirthYear", date);
}

const wstring &oRunner::getBirthDate() const {
  return getDCI().getDate("BirthYear");
}

void oAbstractRunner::setSpeakerPriority(int year)
{
  if (Class) {
    oe->classChanged(Class, false);
  }
  getDI().setInt("Priority", year);
}

int oAbstractRunner::getSpeakerPriority() const
{
  return getDCI().getInt("Priority");
}

int oRunner::getSpeakerPriority() const {
  int p = oAbstractRunner::getSpeakerPriority();

  if (tParentRunner)
    p = max(p, tParentRunner->getSpeakerPriority());
  else if (tInTeam) {
    p = max(p, tInTeam->getSpeakerPriority());
  }

  return p;
}

void oRunner::setNationality(const wstring &nat)
{
  getDI().setString("Nationality", nat);
}

wstring oRunner::getNationality() const
{
  return getDCI().getString("Nationality");
}

bool oRunner::matchName(const wstring &pname) const
{
  if (pname == sName || pname == tRealName)
    return true;

  vector<wstring> myNames, inNames;

  split(tRealName, L" ", myNames);
  split(pname, L" ", inNames);
  int numInNames = inNames.size();

  for (size_t k = 0; k < myNames.size(); k++)
    myNames[k] = canonizeName(myNames[k].c_str());

  int nMatched = 0;
  for (size_t j = 0; j < inNames.size(); j++) {
    wstring inName = canonizeName(inNames[j].c_str());
    for (size_t k = 0; k < myNames.size(); k++) {
      if (myNames[k] == inName) {
        nMatched++;

        // Suppert changed last name in the most common case
        /*if (j == 0 && k == 0 && inNames.size() == 2 && myNames.size() == 2) {
          return true;
        }*/
        break;
      }
    }
  }

  return nMatched >= min<int>(max<int>(numInNames, myNames.size()), 2);
}

oRunner::BibAssignResult oRunner::autoAssignBib() {
  if (Class == 0 || !getBib().empty())
    return BibAssignResult::NoBib;

  int maxbib = 0;
  wchar_t pattern[32];
  int noBib = 0;
  int withBib = 0;
  unordered_set<wstring> allBibs;
  allBibs.reserve(oe->Runners.size());

  for(oRunnerList::iterator it = oe->Runners.begin(); it !=oe->Runners.end();++it) {
    if (it->isRemoved())
      continue;

    const wstring &bib = it->getBib();
    allBibs.insert(bib);

    if (it->Class == Class) {
      if (!bib.empty()) {
        withBib++;
        int ibib = oClass::extractBibPattern(bib, pattern); 
        maxbib = max(ibib, maxbib);
      }
      else
        noBib++;
    }
  }

  if (maxbib>0 && withBib>noBib) {
    wchar_t bib[32];
    swprintf(bib, pattern, maxbib+1);
    wstring nBib = bib;
    if (allBibs.count(nBib))
      return BibAssignResult::Failed; // Bib already use. Do not allow duplicates.
    setBib(nBib, maxbib+1, true);
    return BibAssignResult::Assigned;
  }
  return BibAssignResult::NoBib;
}

void oRunner::getSplitAnalysis(vector<int>& deltaTimes) const {
  deltaTimes.clear();
  vector<int> mp;

  if (splitTimes.empty() || !Class)
    return;
  pClass cls = getClassRef(true);

  if (cls->tSplitRevision == tSplitRevision) {
    deltaTimes = tMissedTime;
    return;
  }

  pCourse pc = getCourse(true);
  if (!pc)
    return;
  vector<int> reorder;
  if (pc->isAdapted())
    reorder = pc->getMapToOriginalOrder();
  else {
    reorder.reserve(pc->nControls() + 1);
    for (int k = 0; k <= pc->nControls(); k++)
      reorder.push_back(k);
  }

  int id = pc->getId();
  if (cls->tSplitAnalysisData.count(id) == 0)
    cls->calculateSplits();

  const vector<int>& baseLine = cls->tSplitAnalysisData[id];
  const unsigned nc = pc->getNumControls();

  if (baseLine.size() != nc + 1)
    return;

  vector<double> res(nc + 1);

  double resSum = 0;
  double baseSum = 0;
  double bestTime = 0;
  for (size_t k = 0; k <= nc; k++) {
    res[k] = getSplitTime(k, false);
    if (res[k] > 0) {
      resSum += res[k];
      baseSum += baseLine[reorder[k]];
    }
    bestTime += baseLine[reorder[k]];
  }

  deltaTimes.resize(nc + 1);

  // Adjust expected time by removing mistakes
  for (size_t k = 0; k <= nc; k++) {
    if (res[k] > 0) {
      double part = res[k] * baseSum / (resSum * bestTime);
      double delta = part - baseLine[reorder[k]] / bestTime;
      int deltaAbs = int(floor(delta * resSum + 0.5));
      if (res[k] - deltaAbs < baseLine[reorder[k]])
        deltaAbs = int(res[k] - baseLine[reorder[k]]);

      if (deltaAbs > 0)
        resSum -= deltaAbs;
    }
  }
  vector<double> resOrig = res;

  for (size_t k = 0; k <= nc; k++) {
    if (res[k] > 0) {
      double part = res[k] * baseSum / (resSum * bestTime);
      double delta = part - baseLine[reorder[k]] / bestTime;

      int deltaAbs = int(floor(delta * resSum + 0.5));

      if (deltaAbs > 0) {
        if (fabs(delta) > 0.01 && deltaAbs > res[k]*0.1 && deltaAbs >= (20 * timeConstSecond))
          deltaTimes[k] = deltaAbs;

        res[k] -= deltaAbs;
        if (res[k] < baseLine[reorder[k]])
          res[k] = baseLine[reorder[k]];
      }
    }
  }

  resSum = 0;
  for (size_t k = 0; k <= nc; k++) {
    if (res[k] > 0) {
      resSum += res[k];
    }
  }

  for (size_t k = 0; k <= nc; k++) {
    if (res[k] > 0) {
      double part = resOrig[k] * baseSum / (resSum * bestTime);
      double delta = part - baseLine[reorder[k]] / bestTime;
      int deltaAbs = int(floor(delta * resSum + 0.5));

      //if (fabs(delta) > 1.0 / 100 && deltaAbs >= timeConstSecond * 20)
      if (fabs(delta) > 0.01 && deltaAbs > resOrig[k] * 0.1 && deltaAbs >= (20 * timeConstSecond))
        deltaTimes[k] = max(deltaAbs, deltaTimes[k]);
    }
  }
}

void oRunner::getLegPlaces(vector<int> &places) const {
  places.clear();
  pCourse pc = getCourse(true);
  if (!pc || !Class || splitTimes.empty())
    return;
  pClass cls = getClassRef(true);

  if (cls->tSplitRevision == tSplitRevision) {
    places = tPlaceLeg;
    return;
  }

  int id = pc->getId();

  if (cls->tSplitAnalysisData.count(id) == 0)
    cls->calculateSplits();

  const unsigned nc = pc->getNumControls();

  places.resize(nc+1);
  int cc = pc->getCommonControl();
  for (unsigned k = 0; k<=nc; k++) {
    int to = cc;
    if (k<nc)
      to = pc->getControl(k) ? pc->getControl(k)->getId() : 0;
    int from = cc;
    if (k>0)
      from = pc->getControl(k-1) ? pc->getControl(k-1)->getId() : 0;

    int time = getSplitTime(k, false);

    if (time>0)
      places[k] = cls->getLegPlace(from, to, time);
    else
      places[k] = 0;
  }
}

void oRunner::getLegTimeAfter(vector<int> &times) const
{
  times.clear();
  if (splitTimes.empty() || !Class)
    return;
  pClass cls = getClassRef(true);
  if (cls->tSplitRevision == tSplitRevision) {
    times = tAfterLeg;
    return;
  }

  pCourse pc = getCourse(false);
  if (!pc)
    return;

  int id = pc->getId();

  if (cls->tCourseLegLeaderTime.count(id) == 0)
    cls->calculateSplits();

  const unsigned nc = pc->getNumControls();

  const vector<int> leaders = cls->tCourseLegLeaderTime[id];

  if (leaders.size() != nc + 1)
    return;

  times.resize(nc+1);

  for (unsigned k = 0; k<=nc; k++) {
    int s = getSplitTime(k, true);

    if (s>0) {
      times[k] = s - leaders[k];
      if (times[k]<0)
        times[k] = -1;
    }
    else
      times[k] = -1;
  }
  // Normalized order
  const vector<int> &reorder = getCourse(true)->getMapToOriginalOrder();
  if (!reorder.empty()) {
    vector<int> orderedTimes(times.size());
    for (size_t k = 0; k < min(reorder.size(), times.size()); k++) {
      orderedTimes[k] = times[reorder[k]];
    }
    times.swap(orderedTimes);
  }
}

void oRunner::getLegTimeAfterAcc(vector<ResultData> &times) const
{
  times.clear();
  if (splitTimes.empty() || !Class || tStartTime<=0)
    return;
  pClass cls = getClassRef(true);
  if (cls->tSplitRevision == tSplitRevision) {
    times = tAfterLegAcc;
    return;
  }
  pCourse pc = getCourse(false); //XXX Does not work for loop courses
  if (!pc)
    return;

  int id = pc->getId();

  if (cls->tCourseAccLegLeaderTime.count(id) == 0)
    cls->calculateSplits();

  const unsigned nc = pc->getNumControls();

  const vector<int> leaders = cls->tCourseAccLegLeaderTime[id];
  const vector<SplitData> &sp = getSplitTimes(true);
  if (leaders.size() != nc + 1)
    return;
  //xxx reorder output
  times.resize(nc+1);

  bool isRelayTeam = tInTeam != nullptr;
  int off = tInTeam ? tInTeam->getTotalRunningTimeAtLegStart(tLeg, false) : 0;

  for (unsigned k = 0; k<=nc; k++) {
    int s = 0;
    if (k < sp.size())
      s = sp[k].getTime(true);
    else if (k==nc)
      s = FinishTime;

    if (s>0) {
      times[k].data = s - tStartTime - leaders[k];
      if (times[k].data < 0)
        times[k].data = -1;
    }
    else
      times[k].data = -1;

    if (!isRelayTeam || times[k].data < 0)
      times[k].teamTotalData = times[k].data;
    else {
      if (k < nc)
        times[k].teamTotalData = s - tStartTime + off - cls->getAccLegControlLeader(tLeg, pc->getCourseControlId(k));
      else
        times[k].teamTotalData = s - tStartTime + off - cls->getAccLegControlLeader(tLeg, oPunch::PunchFinish);
    }
  }

   // Normalized order
  const vector<int> &reorder = getCourse(true)->getMapToOriginalOrder();
  if (!reorder.empty()) {
    vector<ResultData> orderedTimes(times.size());
    for (size_t k = 0; k < min(reorder.size(), times.size()); k++) {
      orderedTimes[k] = times[reorder[k]];
    }
    times.swap(orderedTimes);
  }
}

void oRunner::getLegPlacesAcc(vector<ResultData> &places) const
{
  places.clear();
  pCourse pc = getCourse(false);
  if (!pc || !Class)
    return;
  if (splitTimes.empty() || tStartTime<=0)
    return;
  pClass cls = getClassRef(true);
  if (cls->tSplitRevision == tSplitRevision) {
    places = tPlaceLegAcc;
    return;
  }

  int id = pc->getId();
  const unsigned nc = pc->getNumControls();
  const vector<SplitData> &sp = getSplitTimes(true);
  places.resize(nc+1);

  bool isRelayTeam = tInTeam != nullptr;
  int off = tInTeam ? tInTeam->getTotalRunningTimeAtLegStart(tLeg, false) : 0;

  for (unsigned k = 0; k<=nc; k++) {
    int s = 0;
    if (k < sp.size())
      s = sp[k].getTime(true);
    else if (k==nc)
      s = FinishTime;

    if (s>0) {
      int time = s - tStartTime;

      if (time > 0) {
        places[k].data = cls->getAccLegPlace(id, k, time);
        if (k < nc)
          places[k].teamTotalData = cls->getAccLegControlPlace(tLeg, pc->getCourseControlId(k), time + off);
        else
          places[k].teamTotalData = cls->getAccLegControlPlace(tLeg, oPunch::PunchFinish, time + off);
      }
      else {
        places[k].data = 0;
        places[k].teamTotalData = 0;
      }
    }
  }

  // Normalized order
  const vector<int> &reorder = getCourse(true)->getMapToOriginalOrder();
  if (!reorder.empty()) {
    vector<ResultData> orderedPlaces(reorder.size());
    for (size_t k = 0; k < reorder.size(); k++) {
      orderedPlaces[k] = places[reorder[k]];
    }
    places.swap(orderedPlaces);
  }
}

void oRunner::setupRunnerStatistics() const
{
  if (!Class)
    return;
  pClass cls = getClassRef(true);

  if (cls->tSplitRevision == tSplitRevision)
    return;
  if (Card)
    tOnCourseResults.clear();

  getSplitAnalysis(tMissedTime);
  getLegPlaces(tPlaceLeg);
  getLegTimeAfter(tAfterLeg);
  getLegPlacesAcc(tPlaceLegAcc);
  getLegTimeAfterAcc(tAfterLegAcc);
  tSplitRevision = cls->tSplitRevision;
}

int oRunner::getMissedTime(int ctrlNo) const {
  if (Class && Class->isRogaining()) {
    pCourse crs = getCourse(false);
    if (Card && crs) {
      if (ctrlNo > Card->punches.size())
        return 0;
      int count = 0;
      int prevP = crs->getStartPunchType();
      int lastTime = getStartTime();
      for (oPunch &p : Card->punches) {
        if (p.getTypeCode() < 30 && !p.isFinish(crs->getFinishPunchType()))
          continue;
        if (p.isStart(crs->getStartPunchType()))
          continue;
        if (!p.getRogainingControl(*crs))
          continue;
        int nextP = p.getTypeCode();
        int thisTime = p.getAdjustedTime();

        if (count == ctrlNo) {
          auto res = getRogainingAnalysis(prevP, nextP);
          return res.lostTime;
        }
        lastTime = thisTime;
        prevP = nextP;
        count++;
      }
    }
    return 0;
  }
  else {
    setupRunnerStatistics();
    if (unsigned(ctrlNo) < tMissedTime.size())
      return tMissedTime[ctrlNo];
    else
      return -1;
  }
}

int oRunner::getMissedTime() const {
  if (Class && Class->isRogaining()) {
    pCourse crs = getCourse(false);
    if (Card && crs) {
      int prevP = crs->getStartPunchType();
      int lostT = 0;
      for (oPunch &p : Card->punches) {
        if (p.getTypeCode() < 30 && !p.isFinish(crs->getFinishPunchType()))
          continue;
        if (p.isStart(crs->getStartPunchType()))
          continue;
        if (!p.getRogainingControl(*crs))
          continue;
        int nextP = p.getTypeCode();
        auto res = getRogainingAnalysis(prevP, nextP);
        lostT += res.lostTime;
        prevP = nextP;
      }
      return lostT;
    }
    return 0;
  }
  else {
    setupRunnerStatistics();
    int t = 0;
    for (size_t k = 0; k < tMissedTime.size(); k++) {
      if (tMissedTime[k] > 0)
        t += tMissedTime[k];
    }
    return t;
  }
}

wstring oRunner::getMissedTimeS() const {
  return formatTimeMS(getMissedTime(), false, SubSecond::Off);
}

wstring oRunner::getMissedTimeS(int ctrlNo) const {
  int t = getMissedTime(ctrlNo);
  if (t>0)
    return formatTimeMS(t, false, SubSecond::Off);
  else
    return L"";
}

int oRunner::getLegPlace(int ctrlNo) const {
  setupRunnerStatistics();
  if (unsigned(ctrlNo) < tPlaceLeg.size())
    return tPlaceLeg[ctrlNo];
  else
    return 0;
}

int oRunner::getLegTimeAfter(int ctrlNo) const {
  setupRunnerStatistics();
  if (unsigned(ctrlNo) < tAfterLeg.size())
    return tAfterLeg[ctrlNo];
  else
    return -1;
}

int oRunner::getLegPlaceAcc(int ctrlNo, bool teamTotal) const {
  for (auto &res : tOnCourseResults.res) {
    if (res.controlIx == ctrlNo)
      return teamTotal ? res.teamTotalPlace : res.place;
  }
  if (!Card) {
    return 0;
  }
  setupRunnerStatistics();
  if (unsigned(ctrlNo) < tPlaceLegAcc.size())
    return tPlaceLegAcc[ctrlNo].get(teamTotal);
  else
    return 0;
}

int oRunner::getLegTimeAfterAcc(int ctrlNo, bool teamTotal) const {
  for (auto &res : tOnCourseResults.res) {
    if (res.controlIx == ctrlNo)
      return teamTotal ? res.teamTotalAfter : res.after;
  }
  if (!Card) 
    return -1;
  setupRunnerStatistics();
  if (unsigned(ctrlNo) < tAfterLegAcc.size())
    return tAfterLegAcc[ctrlNo].get(teamTotal);
  else
    return -1;
}

int oRunner::getTimeWhenPlaceFixed() const {
  if (!Class || !statusOK(true, true))
    return -1;
  if (unsigned(tLeg) >= Class->tResultInfo.size()) {
    oe->analyzeClassResultStatus();
    if (unsigned(tLeg) >= Class->tResultInfo.size())
      return -1;
  }

  int lst =  Class->tResultInfo[tLeg].lastStartTime;
  return lst > 0 ? lst + getRunningTime(false) : lst;
}


pRunner oRunner::getMatchedRunner(const SICard &sic) const {
  if (multiRunner.size() == 0 && tParentRunner == 0)
    return pRunner(this);
  if (!Class)
    return pRunner(this);
  
  const vector<pRunner> &multiV = tParentRunner ? tParentRunner->multiRunner : multiRunner;
  
  vector<pRunner> multiOrdered;
  multiOrdered.push_back( tParentRunner ? tParentRunner : pRunner(this));
  multiOrdered.insert(multiOrdered.end(), multiV.begin(), multiV.end());

  int Distance=-1000;
  pRunner r = 0; //Best runner
  
  for (size_t k = 0; k<multiOrdered.size(); k++) {
    if (!multiOrdered[k] || multiOrdered[k]->Card || multiOrdered[k]->getStatus() != StatusUnknown)
      continue;

    LegTypes lt = Class->getLegType(multiOrdered[k]->tLeg);
    StartTypes st = Class->getStartType(multiOrdered[k]->tLeg);
    
    if (lt == LTNormal || lt == LTParallel || st==STChange || st == STPursuit)
      return pRunner(this);

    vector<pCourse> crs;
    if (Class->hasCoursePool()) {
      Class->getCourses(multiOrdered[k]->tLeg, crs);
    }
    else {
      pCourse pc = multiOrdered[k]->getCourse(false);
      crs.push_back(pc);
    }

    for (size_t j = 0; j < crs.size(); j++) { 
      pCourse pc = crs[j];
      if (!pc)
        continue;

      int d = pc->distance(sic);

      if (d>=0) {
        if (Distance<0) Distance=1000;
        if (d<Distance) {
          Distance=d;
          r = multiOrdered[k];
        }
      }
      else {
        if (Distance<0 && d>Distance) {
          Distance=d;
          r = multiOrdered[k];
        }
      }
    }
  }

  if (r)
    return r;
  else
    return pRunner(this);
}

int oRunner::getTotalRunningTime(int time, bool computedTime, bool includeInput) const {
  if (tStartTime < 0)
    return 0;
  if (tInTeam == 0 || tLeg == 0) {
    if (time == FinishTime)
      return getRunningTime(computedTime) + (includeInput ? inputTime : 0);
    else
      return time-tStartTime + (includeInput ? inputTime : 0);
  }
  else {
    if (Class == 0 || unsigned(tLeg) >= Class->legInfo.size())
      return 0;

    if (time == FinishTime) {
      return tInTeam->getLegRunningTime(getParResultLeg(), computedTime, includeInput); // Use the official running time in this case (which works with parallel legs)
    }

    int baseleg = tLeg;
    while (baseleg>0 && (Class->legInfo[baseleg].isParallel() ||
                         Class->legInfo[baseleg].isOptional())) {
      baseleg--;
    }

    int leg = baseleg-1;
    while (leg>0 && (Class->legInfo[leg].legMethod == LTExtra || Class->legInfo[leg].legMethod == LTIgnore)) {
      leg--;
    }

    int pt = leg>=0 ? tInTeam->getLegRunningTime(leg, computedTime, includeInput) : 0;
    if (pt>0)
      return pt + time - tStartTime;
    else if (tInTeam->tStartTime > 0)
      return (time - tInTeam->tStartTime) + (includeInput ? tInTeam->inputTime : 0);
    else
      return 0;
  }
}

wstring oRunner::getCompactName() const {
  wstring given = getGivenName();
  wstring last = getFamilyName();
  if (!given.empty()) {
    wchar_t wbf[4];
    wbf[0] = given[0];
    wbf[1] = '.';
    wbf[2] = ' ';
    wbf[3] = 0;
    return wbf + last;
  }
  return last;
}

  // Get the complete name, including team and club.
wstring oRunner::getCompleteIdentification(IDType type, NameType nameType) const {
  bool onlyThisRunner = type == IDType::OnlyThis;
  bool includeExtra = type == IDType::ParallelLegExtra;

  if (onlyThisRunner || tInTeam == 0 || !Class || tInTeam->getName() == sName) {
    wstring rname = nameType == NameType::Compact ? getCompactName() : getName();
    wstring clubteam;
    if (tInTeam && tInTeam->getName() != sName) {
      clubteam = tInTeam->getName();
    }
    else if (Club)
      clubteam = nameType != NameType::Default ? Club->getCompactName() : Club->getName();
    
    if (!clubteam.empty()) 
      return rname + L" (" + clubteam + L")";
    else
      return rname;
  }
  else {
    wstring names;
    pClass clsToUse = tInTeam->Class != 0 ? tInTeam->Class : Class;
    // Get many names for paralell legs
    int firstLeg = tLeg;
    LegTypes lt=clsToUse->getLegType(firstLeg--);
    while(firstLeg>=0 && (lt==LTIgnore || lt==LTParallel || lt==LTParallelOptional || (lt==LTExtra && includeExtra)) )
      lt=clsToUse->getLegType(firstLeg--);

    for (size_t k = firstLeg+1; k < clsToUse->legInfo.size(); k++) {
      pRunner r = tInTeam->getRunner(k);
      if (r) {
        wstring rname = nameType == NameType::Compact ? getCompactName() : getName();
        if (names.empty())
          names = rname;
        else
          names += L"/" + rname;
      }
      lt = clsToUse->getLegType(k + 1);
      if ( !(lt==LTIgnore || lt==LTParallel || lt == LTParallelOptional || (lt==LTExtra && includeExtra)))
        break;
    }

    if (clsToUse->legInfo.size() <= 2)
      return names + L" (" + tInTeam->sName + L")";
    else
      return tInTeam->sName + L" (" + names + L")";
  }
}

RunnerStatus oAbstractRunner::getTotalStatus(bool allowUpdate) const {
  RunnerStatus st = getStatusComputed(allowUpdate);
  if (st == StatusUnknown && inputStatus != StatusNotCompeting)
    return StatusUnknown;
  else if (inputStatus == StatusUnknown)
    return StatusDNS;

  return max(st, inputStatus);
}

RunnerStatus oRunner::getTotalStatus(bool allowUpdate) const {
  RunnerStatus stm = getStatusComputed(allowUpdate);
  if (stm == StatusUnknown && inputStatus != StatusNotCompeting)
    return StatusUnknown;
  else if (inputStatus == StatusUnknown)
    return StatusDNS;
  int leg = getParResultLeg();

  if (tInTeam == 0 || leg == 0)
    return max(stm, inputStatus);
  else {
    RunnerStatus st = tInTeam->getLegStatus(leg-1, true, true);

    if (leg + 1 == tInTeam->getNumRunners())
      st = max(st, tInTeam->getStatusComputed(allowUpdate));

    if (st == StatusOK || st == StatusUnknown)
      return stm;
    else
      return max(max(stm, st), inputStatus);
  }
}

void oRunner::remove()
{
  if (oe) {
    vector<int> me;
    me.push_back(Id);
    oe->removeRunner(me);
  }
}

bool oRunner::canRemove() const
{
  return !oe->isRunnerUsed(Id);
}

void oAbstractRunner::setInputTime(const wstring &time) {
  int t = convertAbsoluteTimeMS(time);
  if (t != inputTime) {
    inputTime = t;
    updateChanged();
  }
}

wstring oAbstractRunner::getInputTimeS() const {
  if (inputTime > 0)
    return formatTime(inputTime);
  else
    return makeDash(L"-");
}

void oAbstractRunner::setInputStatus(RunnerStatus s) {
  if (inputStatus != s) {
    inputStatus = s;
    updateChanged();
  }
}

wstring oAbstractRunner::getInputStatusS() const {
  return oe->formatStatus(inputStatus, true);
}

void oAbstractRunner::setInputPoints(int p)
{
  if (p != inputPoints) {
    inputPoints = p;
    updateChanged();
  }
}

void oAbstractRunner::setInputPlace(int p)
{
  if (p != inputPlace) {
    inputPlace = p;
    updateChanged();
  }
}

void oRunner::setInputData(const oRunner &r) {
  if (r.getClassRef(false) && r.getClassRef(true)->isSingleStageOnly()) {
    resetInputData();
    return;
  }

  if (!r.multiRunner.empty() && r.multiRunner.back() && r.multiRunner.back() != &r)
    setInputData(*r.multiRunner.back());
  else {
    oDataInterface dest = getDI();
    oDataConstInterface src = r.getDCI();

    if (r.tStatus != StatusNotCompeting) {
      inputTime = r.getTotalRunningTime(r.FinishTime, true, true);
      inputStatus = r.getTotalStatus();
      if (r.tInTeam) { // If a team has not status ok, transfer this status to all team members.
        if (r.tInTeam->getTotalStatus() > StatusOK)
          inputStatus = r.tInTeam->getTotalStatus();
      }
      inputPoints = r.getRogainingPoints(true, true);
      inputPlace = r.tTotalPlace.get(false);
    }
    else {
      // Copy input
      inputTime = r.inputTime;
      inputStatus = r.inputStatus;
      inputPoints = r.inputPoints;
      inputPlace = r.inputPlace;
    }

    if (r.getClubRef())
      setClub(r.getClub());
      
    if (!Card && r.isTransferCardNoNextStage()) {
      setCardNo(r.getCardNo(), false);
      dest.setInt("CardFee", src.getInt("CardFee"));
      setTransferCardNoNextStage(true);
    }
    // Copy flags.
    // copy....
    
    dest.setInt("TransferFlags", src.getInt("TransferFlags"));
    dest.setString("Nationality", src.getString("Nationality"));
    dest.setString("Country", src.getString("Country"));

    dest.setInt("Fee", src.getInt("Fee"));
    dest.setInt("Paid", src.getInt("Paid"));
    dest.setInt("Taxable", src.getInt("Taxable"));

    int sn = r.getEvent()->getStageNumber();
    addToInputResult(sn-1, &r);
  }
}

void oEvent::getDBRunnersInEvent(intkeymap<int, __int64> &runners) const {
  runners.clear();
  for (oRunnerList::const_iterator it = Runners.begin(); it != Runners.end(); ++it) {
    if (it->isRemoved())
      continue;
    __int64 id = it->getExtIdentifier();
    if (id != 0)
      runners.insert(id, it->getId());
    else if (it->getCardNo() != 0) {
      // Lookup by card + constant
      id = it->getCardNo() + cardIdConstant;
      runners.insert(id, it->getId());
    }
  }
}

void oRunner::init(const RunnerWDBEntry &dbr, bool updateOnlyExt) {
  if (updateOnlyExt) {
    dbr.getName(sName);
    getRealName(sName, tRealName);
    getDI().setString("Nationality", dbr.getNationality());
    getDI().setInt("BirthYear", dbr.dbe().getBirthDateInt());
    getDI().setString("Sex", dbr.getSex());
    setExtIdentifier(dbr.getExtId());
  }
  else {
    setTemporary();
    dbr.getName(sName);
    getRealName(sName, tRealName);
    cardNumber = dbr.dbe().cardNo;
    Club = oe->getRunnerDatabase().getClub(dbr.dbe().clubNo);
    getDI().setString("Nationality", dbr.getNationality());
    getDI().setInt("BirthYear", dbr.dbe().getBirthDateInt());
    getDI().setString("Sex", dbr.getSex());
    setExtIdentifier(dbr.getExtId());
  }
}

void oEvent::selectRunners(const wstring &classType, int lowAge,
                           int highAge, const wstring &firstDate,
                           const wstring &lastDate, bool includeWithFee,
                           vector<pRunner> &output) const {
  oRunnerList::const_iterator it;
  int cid = 0;
  if (classType.length() > 2 && classType.substr(0,2) == L"::")
    cid = wtoi(classType.c_str() + 2);

  output.clear();

  int firstD = 0, lastD = 0;
  if (!firstDate.empty()) {
    firstD = convertDateYMD(firstDate, true);
    if (firstD <= 0)
      throw meosException(L"Felaktigt datumformat 'X' (Använd ÅÅÅÅ-MM-DD).#" + firstDate);
  }

  if (!lastDate.empty()) {
    lastD = convertDateYMD(lastDate, true);
    if (lastD <= 0)
      throw meosException(L"Felaktigt datumformat 'X' (Använd ÅÅÅÅ-MM-DD).#" + lastDate);
  }


  bool allClass = classType == L"*";
  for (it=Runners.begin(); it != Runners.end(); ++it) {
    if (it->skip())
      continue;

    const pClass pc = it->Class;
    if (cid > 0 && (pc == 0 || pc->getId() != cid))
      continue;

    if (cid == 0 && !allClass) {
      if ((pc && pc->getType()!=classType) || (pc==0 && !classType.empty()))
        continue;
    }

    int age = it->getBirthAge();
    if (age > 0 && (lowAge > 0 || highAge > 0)) {
      if (lowAge > highAge)
        throw meosException("Undre åldersgränsen är högre än den övre.");

      if (age < lowAge || age > highAge)
        continue;
      /*
      bool ageOK = false;
      if (lowAge > 0 && age <= lowAge)
        ageOK = true;
      else if (highAge > 0 && age >= highAge)
        ageOK = true;

      if (!ageOK)
        continue;*/
    }

    int date = it->getDCI().getInt("EntryDate");
    if (date > 0) {
      if (firstD > 0 && date < firstD)
        continue;
      if (lastD > 0 && date > lastD)
        continue;

    }

    if (!includeWithFee) {
      int fee = it->getDCI().getInt("Fee");
      if (fee != 0)
        continue;
    }
    //    string date = di.getDate("EntryDate");

    output.push_back(pRunner(&*it));
  }
}

void oRunner::changeId(int newId) {
  pRunner old = oe->runnerById[Id];
  if (old == this)
    oe->runnerById.remove(Id);

  oBase::changeId(newId);

  oe->runnerById[newId] = this;
}

const vector<SplitData> &oRunner::getSplitTimes(bool normalized) const {
  if (!normalized)
    return splitTimes;
  else {
    pCourse pc = getCourse(true);
    if (pc && pc->isAdapted() && splitTimes.size() == pc->nControls()
        && getCourse(false)->nControls() == pc->nControls()) {
      if (!normalizedSplitTimes.empty())
        return normalizedSplitTimes;
      const vector<int> &mapToOriginal = pc->getMapToOriginalOrder();
      normalizedSplitTimes.resize(splitTimes.size());
      vector<int> orderedSplits(splitTimes.size() + 1, -1);

      for (int k = 0; k < pc->nControls(); k++) {
        if (splitTimes[k].hasTime()) {
          int t = -1;
          int j = k - 1;
          while (j >= -1 && t == -1) {
            if (j == -1)
              t = getStartTime();
            else if (splitTimes[j].hasTime())
              t = splitTimes[j].getTime(true);
            j--;
          }
          orderedSplits[mapToOriginal[k]] = splitTimes[k].getTime(true) - t;
        }
      }

      // Last to finish
      {
        int t = -1;
        int j = pc->nControls() - 1;
        while (j >= -1 && t == -1) {
          if (j == -1)
            t = getStartTime();
          else if (splitTimes[j].hasTime())
            t = splitTimes[j].getTime(true);
          j--;
        }
        orderedSplits[mapToOriginal[pc->nControls()]] = FinishTime - t;
      }

      int accumulatedTime = getStartTime();
      for (int k = 0; k < pc->nControls(); k++) {
        if (orderedSplits[k] > 0) {
          accumulatedTime += orderedSplits[k];
          normalizedSplitTimes[k].setPunchTime(accumulatedTime);
        }
        else
          normalizedSplitTimes[k].setNotPunched();
      }

      return normalizedSplitTimes;
    }
    return splitTimes;
  }
}

void oRunner::markClassChanged(int controlId) {
  assert(controlId < 4096);
  if (Class) {
    Class->markSQLChanged(tLeg, controlId);
    pClass cls2 = getClassRef(true);
    if (cls2 != Class)
      cls2->markSQLChanged(-1, controlId);

    if (tInTeam && tInTeam->Class != Class && tInTeam->Class) {
      tInTeam->Class->markSQLChanged(tLeg, controlId);
    }
  }
  else if (oe)
    oe->globalModification = true;
}

void oRunner::changedObject() {
  markClassChanged(-1);
  sqlChanged = true;
  oe->sqlRunners.changed = true;
}

int oRunner::getBuiltinAdjustment() const { 
  if (adjustTimes.empty())
    return 0;

  return adjustTimes.back();
}

int oAbstractRunner::getTimeAdjustment(bool includeBuiltinAdjustment) const {
  if (oe->dataRevision != tAdjustDataRevision) {
    oDataConstInterface dci = getDCI();
    tTimeAdjustment = dci.getInt("TimeAdjust");
    
    tPointAdjustment = dci.getInt("PointAdjust");
    tAdjustDataRevision = oe->dataRevision;
  }
  if (!includeBuiltinAdjustment)
    return tTimeAdjustment;
  else
    return tTimeAdjustment + getBuiltinAdjustment();

}
 
int oAbstractRunner::getPointAdjustment() const {
  if (oe->dataRevision != tAdjustDataRevision) {
    getTimeAdjustment(false); //Setup cache
  }
  return tPointAdjustment;
}

void oAbstractRunner::setTimeAdjustment(int adjust) {
  tTimeAdjustment = adjust;
  getDI().setInt("TimeAdjust", adjust);
}

void oAbstractRunner::setPointAdjustment(int adjust) {
  tPointAdjustment = adjust;
  getDI().setInt("PointAdjust", adjust);
}

int oRunner::getRogainingPoints(bool computed, bool multidayTotal) const {
  int pb = tRogainingPoints;
  if (computed && tComputedPoints >= 0)
    pb = tComputedPoints;

  if (multidayTotal)
    return inputPoints + pb;
  else
    return pb;
}

int oRunner::getRogainingReduction(bool computed) const {
 // if (computed && tComputedPoints >= 0 && tRogainingPointsGross >= tComputedPoints)
 //   return tRogainingPointsGross - tComputedPoints;
  return tReduction;
}

int oRunner::getRogainingPointsGross(bool computed) const {
  if (computed) {
    int cmp = getRogainingPoints(computed, false);
    if (cmp > 0)
      return cmp + tReduction; // This formula only holds when something remains. If tRediction > collected points, the result is wrong
    else
      return tRogainingPointsGross;
  }
  else
    return tRogainingPointsGross;
}

int oRunner::getRogainingOvertime(bool computed) const {
  if (computed) {
    int rt = getRunningTime(true);
    pCourse pc = getCourse(false);
    if (pc && rt > 0 && pc->getMaximumRogainingTime() > 0) {
      return max(0, rt - pc->getMaximumRogainingTime());
    }
  }
  return tRogainingOvertime;
}

void oAbstractRunner::TempResult::reset() {
  runningTime = 0;
  timeAfter = 0;
  points = 0;
  place = 0;
  startTime = 0;
  status = StatusUnknown;
  internalScore.first = 0;
  internalScore.second = 0;
}

oAbstractRunner::TempResult::TempResult() {
  reset();
}

oAbstractRunner::TempResult::TempResult(RunnerStatus statusIn, 
                                        int startTimeIn, 
                                        int runningTimeIn,
                                        int pointsIn) :status(statusIn),
                                        startTime(startTimeIn), runningTime(runningTimeIn),
                                        timeAfter(0), points(0), place(0) {
}

const oAbstractRunner::TempResult &oAbstractRunner::getTempResult(int tempResultIndex) const {
  return tmpResult; //Ignore index for now...
  /*if (tempResultIndex == 0)
    return tmpResult;
  else
    throw meosException("Not implemented");*/
}

oAbstractRunner::TempResult &oAbstractRunner::getTempResult()  {
  return tmpResult; 
}

void oAbstractRunner::setTempResultZero(const TempResult &tr)  {
  tmpResult = tr;
}

void oAbstractRunner::updateComputedResultFromTemp() {
  tComputedTime = tmpResult.getRunningTime();
  tComputedPoints = tmpResult.getPoints();
  tComputedStatus = tmpResult.getStatus();
}

const wstring &oAbstractRunner::TempResult::getStatusS(RunnerStatus inputStatus) const {
  if (inputStatus == StatusOK)
    return oEvent::formatStatus(getStatus(), true);
  else if (inputStatus == StatusUnknown)
    return formatTime(-1);
  else
    return oEvent::formatStatus(max(inputStatus, getStatus()), true);
}

const wstring &oAbstractRunner::TempResult::getPrintPlaceS(bool withDot) const {
  int p=getPlace();
  if (p>0 && p<10000){
    if (withDot) {
      wstring &res = StringCache::getInstance().wget();
      res = itow(p);
      res += L".";
      return res;
    }
    else
      return itow(p);
  }
  return _EmptyWString;
}

const wstring &oAbstractRunner::TempResult::getRunningTimeS(int inputTime, SubSecond mode) const {
  return formatTime(getRunningTime() + inputTime, mode);
}

const wstring &oAbstractRunner::TempResult::getFinishTimeS(const oEvent *oe, SubSecond mode) const {
  return oe->getAbsTime(getFinishTime(), mode);
}

const wstring &oAbstractRunner::TempResult::getStartTimeS(const oEvent *oe, SubSecond mode) const {
  int st = getStartTime();
  if (st > 0)
      return oe->getAbsTime(st, mode);
  else return makeDash(L"-");
}

const wstring &oAbstractRunner::TempResult::getOutputTime(int ix) const {
  int t = size_t(ix) < outputTimes.size() ? outputTimes[ix] : 0;
  return formatTime(t * timeConstSecond);
}

int oAbstractRunner::TempResult::getOutputNumber(int ix) const {
  return size_t(ix) < outputNumbers.size() ? outputNumbers[ix] : 0;
}

void oAbstractRunner::resetInputData() {
  setInputPlace(0);
  if (0 != inputTime) {
    inputTime = 0;
    updateChanged();
  }
  setInputStatus(StatusNotCompeting);
  setInputPoints(0);
}

bool oRunner::isTransferCardNoNextStage() const {
  return hasFlag(FlagUpdateCard);
}

void oRunner::setTransferCardNoNextStage(bool state) {
  setFlag(FlagUpdateCard, state);
}

bool oAbstractRunner::hasFlag(TransferFlags flag) const {
  return (getDCI().getInt("TransferFlags") & flag) != 0;
}

void oAbstractRunner::setFlag(TransferFlags flag, bool onoff) {
  int cf = getDCI().getInt("TransferFlags");
  cf = onoff ? (cf | flag) : (cf & (~flag));
  getDI().setInt("TransferFlags", cf);
}

int oRunner::getNumShortening() const {
  if (oe->dataRevision != tShortenDataRevision) {
    oDataConstInterface dci = getDCI();
    tNumShortening = dci.getInt("Shorten");
    tShortenDataRevision = oe->dataRevision;
  }
  return tNumShortening;
}

void oRunner::setNumShortening(int numShorten) {
  tNumShortening = numShorten;
  tShortenDataRevision = oe->dataRevision;
  oDataInterface di = getDI();
  di.setInt("Shorten", numShorten);
}

int oAbstractRunner::getEntrySource() const {
  return getDCI().getInt("EntrySource");
}

void oAbstractRunner::setEntrySource(int src) {
  getDI().setInt("EntrySource", src);
}

void oAbstractRunner::flagEntryTouched(bool flag) {
  tEntryTouched = flag;
}

bool oAbstractRunner::isEntryTouched() const {
  return tEntryTouched;
}


// Get results from all previous stages
void oAbstractRunner::getInputResults(vector<RunnerStatus> &st,
                                      vector<int> &times,
                                      vector<int> &points,
                                      vector<int> &places) const {
  const wstring &raw = getDCI().getString("InputResult");
  vector<wstring> spvec;
  split(raw, L";", spvec);

  int nStageNow = spvec.size() / 4;
  st.resize(nStageNow);
  times.resize(nStageNow);
  points.resize(nStageNow);
  places.resize(nStageNow);
  for (int j = 0; j < nStageNow; j++) {
    st[j] = RunnerStatus(wtoi(spvec[j * 4 + 0].c_str()));
    times[j] = parseRelativeTime(spvec[j * 4 + 1].c_str());
    points[j] = wtoi(spvec[j * 4 + 2].c_str());
    places[j] = wtoi(spvec[j * 4 + 3].c_str());
  }
}

RunnerStatus  oAbstractRunner::getStageResult(int stage, int &time, int &point, int &place) const {  
  vector<RunnerStatus> st;
  vector<int> times;
  vector<int> points;
  vector<int> places;
  getInputResults(st, times, points, places);
  if (size_t(stage) >= st.size()) {
    time = 0;
    point = 0;
    place = 0;
    return StatusNotCompeting;
  }
  time = times[stage];
  point = points[stage];
  place = places[stage];
  return st[stage];
}


// Add current result to input result. Only use when transferring to next stage
void oAbstractRunner::addToInputResult(int thisStageNo, const oAbstractRunner *src) {
  thisStageNo = max(thisStageNo, 0);
  int p = src->getPlace();
  int rt = src->getRunningTime(true);
  RunnerStatus st = src->getStatusComputed(true);
  int pt = src->getRogainingPoints(true, false);

  const wstring &raw = src->getDCI().getString("InputResult");
  vector<wstring> spvec;
  split(raw, L";", spvec);

  int nStageNow = spvec.size() / 4;
  int numStage = max(nStageNow, thisStageNo + 1);
  spvec.resize(numStage * 4);
  spvec[4*thisStageNo] = itow(st);
  spvec[4*thisStageNo+1] = codeRelativeTimeW(rt);
  spvec[4*thisStageNo+2] = itow(pt);
  spvec[4*thisStageNo+3] = itow(p);

  wstring out;
  unsplit<wstring>(spvec, L";", out);
  getDI().setString("InputResult", out);
}

int oRunner::getTotalTimeInput() const {
  if (tInTeam) {
    if (getLegNumber()>0) { 
      return tInTeam->getLegRunningTime(getLegNumber()-1, true, true);
    }
    else {
      return tInTeam->getInputTime();
    }
  }
  else {
    return getInputTime();
  }
}


RunnerStatus oRunner::getTotalStatusInput() const {
  RunnerStatus inStatus = StatusOK;
  if (tInTeam) {
    const pTeam t = tInTeam;
    if (getLegNumber()>0) { 
      inStatus = t->getLegStatus(getLegNumber()-1, true, true);
    }
    else {
      inStatus = t->getInputStatus();
    }
  }
  else {
    inStatus = getInputStatus();
  }
  return inStatus;
}

bool oAbstractRunner::startTimeAvailable() const {
  if (getFinishTime() > 0)
    return true;

  return getStartTime() > 0;
}

bool oRunner::startTimeAvailable() const {
  if (getFinishTime() > 0)
    return true;

  int st = getStartTime();
  bool definedTime = st > 0;

  if (!definedTime)
    return false;

  if (!Class || !tInTeam || tLeg == 0)
    return definedTime; 
  
  // Check if time is restart time
  int restart = Class->getRestartTime(tLeg);
  if (st == restart && Class->getStartType(tLeg) == STChange) {
    int currentTime = oe->getComputerTime();
    int rope = Class->getRopeTime(tLeg);
    return rope != 0 && currentTime + 10 * timeConstMinute > rope;
  }

  return true;
}

int oRunner::getRanking() const {
  int rank = getDCI().getInt("Rank");
  if (rank == 0 && tParentRunner)
    rank = tParentRunner->getRanking();
  if (rank <= 0)
    return MaxRankingConstant;
  else
    return rank;
}

wstring oRunner::getRankingScore() const {
  int raw = getDCI().getInt("Rank");
  wchar_t wbf[32] = { 0 };
  if (raw > MaxOrderRank) {
    constexpr int TurnAround = MaxOrderRank * 100000;
    double score = double(TurnAround - raw)/100;
    if (score > 0 && score < 10000) {
      swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%.2f", score);
    }
  }
  return wbf;
}

void oRunner::setRankingScore(double score) {
  int rank = 0;
  if (score > -10 && score < 10000) {
    constexpr int TurnAround = MaxOrderRank * 100000;
    rank = TurnAround - int(score * 100);
  }
  getDI().setInt("Rank", rank);
}

void oAbstractRunner::hasManuallyUpdatedTimeStatus() {
  if (Class && Class->hasClassGlobalDependence()) {
    set<int> cls;
    oe->reEvaluateAll(cls, false);
  }
  if (Class) {
    Class->updateFinalClasses(dynamic_cast<oRunner *>(this), false);
  }
}

bool oRunner::canShareCard(const pRunner other, int newCardNo) const {
  if (!other || other->getCardNo() != newCardNo || newCardNo == 0)
    return true;

  if (getCard() && getCard()->getCardNo() == newCardNo)
    return true;


  if (other->getStatus() == StatusDNF || other->getStatus() == StatusCANCEL
      || other->getStatus() == StatusNotCompeting || other->getStatus() == StatusDNS)
    return true;

  if (other->skip() || other->getCard() || other == this ||
      other->getMultiRunner(0) == getMultiRunner(0))
    return true;

  if (!getTeam() || other->getTeam() != getTeam())
    return false;

  const oClass * tCls = getTeam()->getClassRef(false);
  if (!tCls || tCls != Class)
    return false;

  LegTypes lt1 = tCls->getLegType(tLeg);
  LegTypes lt2 = tCls->getLegType(other->tLeg);

  if (lt1 == LTGroup || lt2 == LTGroup)
    return false;

  int ln1, ln2, ord; 
  tCls->splitLegNumberParallel(tLeg, ln1, ord);
  tCls->splitLegNumberParallel(other->tLeg, ln2, ord);
  return ln1 != ln2;
}

int oAbstractRunner::getPaymentMode() const {
  return getDCI().getInt("PayMode");
}

void oAbstractRunner::setPaymentMode(int mode) {
  getDI().setInt("PayMode", mode);
}

bool oAbstractRunner::hasLateEntryFee() const {
  if (!Class)
    return false;
  
  int highFee = Class->getDCI().getInt("HighClassFee");
  int highFee2 = Class->getDCI().getInt("SecondHighClassFee");
  int normalFee = Class->getDCI().getInt("ClassFee");
  
  int fee = getDCI().getInt("Fee");
  if (fee == normalFee || fee == 0)
    return false;
  else if (fee == highFee && highFee > normalFee && normalFee > 0)
    return true;
  else if (fee == highFee2 && highFee2 > normalFee && normalFee > 0)
    return true;

  wstring date = getEntryDate(true);
  oDataConstInterface odc = oe->getDCI();
  wstring oentry = odc.getDate("OrdinaryEntry");
  bool late = date > oentry && oentry >= L"2010-01-01";

  return late;
}

bool oRunner::payBeforeResult(bool checkFlagOnly) const {
  if (!hasFlag(TransferFlags::FlagPayBeforeResult))
    return false;
  if (checkFlagOnly)
    return true;
  int paid = getDCI().getInt("Paid");
  return getEntryFee() > paid;
}

void oRunner::setPayBeforeResult(bool flag) {
  if (hasFlag(TransferFlags::FlagPayBeforeResult) == flag)
    return;
  setFlag(TransferFlags::FlagPayBeforeResult, flag);
  if (!flag && getStatus() == StatusDQ)
    setStatus(RunnerStatus::StatusUnknown, true, ChangeType::Update, false);
  vector<pair<int, pControl>> mp;
  evaluateCard(true, mp, 0, ChangeType::Update);
}

void oRunner::setPaid(int paid) {
  getDI().setInt("Paid", paid);
}

void oRunner::setFee(int fee) {
  bool needPay = payBeforeResult(false);
  bool paymentChanged = getDI().setInt("Fee", fee);
  if (paymentChanged && needPay) {
    if (getStatus() == StatusDQ)
      setStatus(RunnerStatus::StatusUnknown, true, ChangeType::Update, false);
  }
  if (payBeforeResult(true)) {
    vector<pair<int, pControl>> mp;
    evaluateCard(true, mp, 0, ChangeType::Update);
  }
}


int oRunner::classInstance() const {
  if (classInstanceRev.first == oe->dataRevision)
    return classInstanceRev.second;
  classInstanceRev.second = getDCI().getInt("Heat");
  if (Class)
    classInstanceRev.second = min(classInstanceRev.second, Class->getNumQualificationFinalClasses());
  classInstanceRev.first = oe->dataRevision;
  return classInstanceRev.second;
}

const wstring &oAbstractRunner::getClass(bool virtualClass) const {
  if (Class) {
    if (virtualClass)
      return Class->getVirtualClass(classInstance())->Name;
    else
      return Class->Name;
  }
  
  else return _EmptyWString; 
}

wstring oRunner::formatExtraLine(pRunner r, const wstring &input) {
  wstring ws = input, wsOut;
  size_t parS = ws.find_first_of('[');
  while (parS != wstring::npos) {
    size_t parE = ws.find_first_of(']');
    if (parE != wstring::npos && parE > parS) {
      wsOut += ws.substr(0, parS);
      wstring cmd = ws.substr(parS + 1, parE - parS - 1);
      ws = ws.substr(parE + 1);
      parS = ws.find_first_of('[');

      auto type = MetaList::getTypeFromSymbol(cmd);
      if (r) {
        if (type == EPostType::lNone)
          wsOut += L"{Error: " + cmd + L"}";
        else {
          wsOut += r->getEvent()->formatListString(type, r);
        }
      }
      else if (type == EPostType::lNone) {
        throw meosException(L"Unknown type: " + cmd);
      }
    }
    else {
      if (r == nullptr)
        throw meosException(L"Syntax error: " + input);
      break; // Error
    }
  }
  wsOut += ws;

  return wsOut;
}

bool oAbstractRunner::preventRestart() const {
  if (tPreventRestartCache.second == oe->dataRevision)
    return tPreventRestartCache.first;

  tPreventRestartCache.first = getDCI().getInt("NoRestart") != 0;
  tPreventRestartCache.second = oe->dataRevision;

  return tPreventRestartCache.first;
}

void oAbstractRunner::preventRestart(bool state) {
  getDI().setInt("NoRestart", state);
  tPreventRestartCache.first = state;
  tPreventRestartCache.second = oe->dataRevision;
}

int oRunner::getCheckTime() const {
  oPunch *p = nullptr;
  if (Card) {
    p = Card->getPunchByType(oPunch::PunchCheck);
  }
  else {
    p = oe->getPunch(Id, oPunch::PunchCheck, getCardNo());
  }
  if (p && p->hasTime())
    return p->getTimeInt();

  return 0;
}

const pair<wstring, int> oRunner::getRaceInfo() {
  pair<wstring, int> res;
  RunnerStatus baseStatus = getStatus();
  if (hasResult()) {
    int p = getPlace();
    int rtComp = getRunningTime(true);
    int rtActual = getRunningTime(false);
    int pointsActual = getRogainingPoints(false, false);
    int pointsComp = getRogainingPoints(true, false);
    RunnerStatus compStatus = getStatusComputed(true);
    bool ok = compStatus == StatusOK || compStatus == StatusOutOfCompetition
      || compStatus == StatusNoTiming;
    res.second = ok ? 1 : -1;
    if (compStatus == baseStatus && rtComp == rtActual && pointsComp == pointsActual) {
      res.first = lang.tl(getProblemDescription());
      if (ok && p > 0)
        res.first = lang.tl("Placering: ") + itow(p) + L".";
    }
    else {
      if (ok) {
        res.first += lang.tl("Resultat: ");
        if (compStatus != baseStatus)
          res.first = oe->formatStatus(compStatus, true) + L", ";
        if (pointsActual != pointsComp)
          res.first += itow(pointsComp) + L", ";

        res.first += formatTime(rtComp);
        
        if (p > 0)
          res.first += L" (" + itow(p) + L")";
      }
      else if (!ok && compStatus != baseStatus) {
        res.first = lang.tl("Resultat: ") + oe->formatStatus(compStatus, true);
      }

      if (ok && getRogainingReduction(true) > 0) {
        tProblemDescription = L"Tidsavdrag: X poäng.#" + itow(getRogainingReduction(true));
      }

      if (!getProblemDescription().empty()) {
        if (!res.first.empty()) {
          if (res.first.back() != ')')
            res.first += L", ";
          else
            res.first += L" ";
        }
        res.first += lang.tl(getProblemDescription());
      }
    }
  }
  else {
    vector<oFreePunch*> pl;
    oe->synchronizeList(oListId::oLPunchId);
    oe->getPunchesForRunner(Id, true, pl);
    if (!pl.empty()) {
      res.first = lang.tl(L"Senast sedd: X vid Y.#" +
                          oe->getAbsTime(pl.back()->getTimeInt()) +
                          L"#" + pl.back()->getType(getCourse(false)));
    }
  }

  return res;
}

int oRunner::getParResultLeg() const {
  if (!tInTeam || !Class)
    return 0;
  
  size_t leg = tLeg;
  while (leg < tInTeam->Runners.size()) {
    if (Class->isParallel(leg + 1) && tInTeam->getRunner(leg + 1))
      leg++;
    else
      break;
  }
  return leg;
}

bool oRunner::isResultUpdated(bool totalResult) const {
  if (totalResult)
    return !tPlace.isOld(*oe);
  else
    return !tTotalPlace.isOld(*oe);
}

int oRunner::getStartGroup(bool useTmpStartGroup) const {
  if (useTmpStartGroup && tmpStartGroup)
    return tmpStartGroup;
  int g = getDCI().getInt("StartGroup");
  if (g == 0 && Club)
    return Club->getStartGroup();
  return g;
}

void oRunner::setStartGroup(int sg) {
  getDI().setInt("StartGroup", sg);
}

bool oAbstractRunner::isStatusOK(bool computed, bool allowUpdate) const {
  RunnerStatus st = computed ? getStatusComputed(allowUpdate) : getStatus();
  if (st == StatusOK)
    return true;
  else if (st == StatusOutOfCompetition || st == StatusNoTiming) {
    int rt = getRunningTime(computed);
    return rt > 0;
  }
  return false;
}

bool oAbstractRunner::isStatusUnknown(bool computed, bool allowUpdate) const {
  RunnerStatus st = computed ? getStatusComputed(allowUpdate) : getStatus();
  if (st == StatusUnknown)
    return true;
  else if (st == StatusOutOfCompetition || st == StatusNoTiming) {
    int rt = getRunningTime(computed);
    return rt == 0;
  }
  return false;
}

bool oRunner::matchAbstractRunner(const oAbstractRunner* target) const {
  if (target == nullptr)
    return false;

  if (target == this)
    return true;

  const oTeam* t = dynamic_cast<const oTeam*>(target);
  if (t != nullptr)
    return getTeam() == t;

  return false;
}

/** Format the name according to the style. */
wstring oRunner::formatName(NameFormat style) const {
  switch (style) {
  case NameFormat::Default:
    return getName();
  case NameFormat::FirstLast: {
    size_t comma = sName.find_first_of(',');
    if (comma == string::npos)
      return sName;
    else
      return trim(sName.substr(comma + 1) + L" " + trim(sName.substr(0, comma)));
  }
  case NameFormat::LastFirst:
    return getNameLastFirst();
  case NameFormat::First:
    return getGivenName();
  case NameFormat::Last:
    return getFamilyName();
  case NameFormat::Init: {
    wstring given = getGivenName();
    wstring family = getFamilyName();
    wchar_t out[5];
    int ix = 0;
    auto append = [&out, &ix](wchar_t w) {
      out[ix++] = w;
    };
    if (!given.empty()) {
      append(given[0]);
      append('.');
    }
    if (!family.empty()) {
      append(family[0]);
      append('.');
    }
    append(0);
    return out;
  }
  case NameFormat::InitLast: {
    wstring given = getGivenName();
    if (!given.empty()) {
      given.resize(1);
      given.append(L". ");
    }
    given.append(getFamilyName());
    return given;
  }
  }
  throw meosException("Unknown name style");
}

/** Get available name styles. */
void oRunner::getNameFormats(vector<pair<wstring, size_t>> &  out) {
/*enum class NameFormat {
  Default,
  FirstLast,
  LastFirst,
  Last,
  First,
  Init,
  InitLast
};
*/
  out.clear();
  auto add = [&out](NameFormat f, const string& w) {
    out.emplace_back(lang.tl(w), size_t(f));
  };

  add(NameFormat::Default, "Standard");
  add(NameFormat::FirstLast, "Förnamn Efternamn");
  add(NameFormat::LastFirst, "Efternamn, Förnamn");
  add(NameFormat::Last, "Efternamn");
  add(NameFormat::First, "Förnamn");
  add(NameFormat::Init, "F.E.");
  add(NameFormat::InitLast, "F. Efternamn");
}

void oRunner::setExtIdentifier2(int64_t id) {
  getDI().setInt64("ExtId2", id);
}

int64_t oRunner::getExtIdentifier2() const {
  return getDCI().getInt64("ExtId2");
}

wstring oRunner::getExtIdentifierString2() const {
  int64_t raw = getExtIdentifier2();
  wchar_t res[16];
  if (raw == 0)
    return L"";
  if (raw & BaseGenStringFlag)
    convertDynamicBase(raw & ExtStringMask, 256 - 32, res);
  else if (raw & Base36StringFlag)
    convertDynamicBase(raw & ExtStringMask, 36, res);
  else
    convertDynamicBase(raw, 10, res);
  return res;
}

void oRunner::setExtIdentifier2(const wstring& str) {
  int64_t val = converExtIdentifierString(str);
  setExtIdentifier2(val);
}

void oRunner::setExtraPersonData(const wstring &sex, const wstring &nationality, 
                                 const wstring &rank, wstring &phone,
                                 const wstring &bib, const wstring &text, 
                                 int dataA, int dataB) {
  oDataInterface di = getDI();
  if (sex == L"F" || sex == L"f")
    setSex(PersonSex::sFemale);
  else if (sex == L"M" || sex == L"m")
    setSex(PersonSex::sMale);

  if (!rank.empty()) {
    wstring out;
    RankScoreFormatter rf;
    rf.setData(this, 0, rank, out, 0);
  }
  di.setString("Phone", phone);
  setBib(bib, wtoi(bib.c_str()), true);
  di.setString("TextA", text);
  di.setInt("DataA", dataA);
  di.setInt("DataB", dataB);
  setNationality(nationality);
}

DynamicRunnerStatus oRunner::getDynamicStatus() const  {
  if (tStatus == StatusNotCompeting || tStatus == StatusCANCEL || tStatus == StatusDNS)
    return DynamicRunnerStatus::StatusInactive;
  
  bool finishStat = tStatus != RunnerStatus::StatusUnknown &&
    tStatus != RunnerStatus::StatusOutOfCompetition &&
    tStatus != RunnerStatus::StatusNoTiming;

  if (getRunningTime(false) > 0 || Card != nullptr || finishStat)
    return DynamicRunnerStatus::StatusFinished;

  if (!startTimeAvailable())
    return DynamicRunnerStatus::StatusInactive;

  int ct = oe->getComputerTime();
  if (ct >= getStartTime())
    return DynamicRunnerStatus::StatusActive;

  return DynamicRunnerStatus::StatusInactive;
}

void oRunner::restoreDefaultStartTime(bool recalculate) {
  int st = getDCI().getInt("DrawnTime");
  if (st < 0)
    st = 0;
  if (setStartTime(st, true, ChangeType::Update, recalculate))
    updateChanged(); // Force an update 
}

void oRunner::storeDefaultStartTime() {
  getDI().setInt("DrawnTime", getStartTime());
}

/** Return best time in class and expected time on leg for this runner */
oClass::RogainingAnalysis oRunner::getRogainingAnalysis(int from, int to) const {
  if (!Class || !Class->isRogaining())
    return oClass::RogainingAnalysis();

  if (rogainingBaseSpeed.needsUpdate(*oe))
    oe->computeRogainingStatistics();

  oClass::RogainingAnalysis rg = Class->getRogainingAnalysis(from, to, rogainingBaseSpeed.get());

  int splitTime = 0;
  int expected = rg.lostTime; // Convert "expected" to "lost
  auto res = rogainingLegSplitPlace.find(make_pair(from, to));
  if (res != rogainingLegSplitPlace.end()) {
    rg.legPlace = res->second.second;
    splitTime = res->second.first;
  }
  int delta = expected > 0 && splitTime > 0 ? splitTime - expected : 0;

  if (delta < timeConstSecond * 5 || delta < int(rg.bestTime * 0.05))
    delta = 0;

  rg.lostTime = delta; 
  return rg;
}

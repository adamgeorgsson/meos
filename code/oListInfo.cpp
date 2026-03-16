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


oListInfo::oListInfo() {
  listType=EBaseTypeRunner;
  listSubType=EBaseTypeRunner;
  calcResults=false;
  calcTotalResults = false;
  rogainingResults = false;
  calculateLiveResults = false;
  calcCourseClassResults = false;
  calcCourseResults = false;
  listPostFilter.resize(_EFilterMax+1, 0);
  listPostSubFilter.resize(_ESubFilterMax+1, 0);
  fixedType = false;
  largeSize = false;
  supportClasses = true;
  supportLegs = false;
  supportParameter = false;
  supportLarge = false;
  supportTo = false;
  supportFrom = false;
  supportCustomTitle = true;
  
  resType = Classwise;

  supportSplitAnalysis = true;
  supportInterResults = true;
  supportPageBreak = true;
  supportClassLimit = true;
  
  needPunches = PunchMode::NoPunch;
}

oListInfo::~oListInfo(void) {
}

void oListInfo::replaceType(EPostType find, EPostType replace, bool onlyFirst) {
  for (auto blp : { &head, &subHead, &listPost, &subListPost }) {
    for (auto &pp : *blp) {
      if (pp.type == replace && onlyFirst)
        return;
      if (pp.type == find) {
        pp.type = replace;
        if (onlyFirst)
          return;
      }
    }
  }
}

wstring oListParam::getContentsDescriptor(const oEvent &oe) const {
  wstring cls;
  vector<pClass> classes;
  oe.getClasses(classes, false);
  if (classes.size() == selection.size() || selection.empty())
    cls = oe.getName(); // All classes
  else {
    for (pClass c : classes) {
      if (selection.count(c->getId())) {
        if (!cls.empty())
          cls += L", ";
        cls += c->getName();
      }
    }
  }
  if (legNumber != -1)
    return lang.tl(L"Sträcka X#" + getLegName()) + L": " + cls;
  else
    return cls;
}

oPrintPost::oPrintPost() : type(lString), dx(0), dy(0), format(0) {
  legIndex = -1;
  linearLegIndex = true;
}

oPrintPost::oPrintPost(EPostType type, const wstring& text,
                       int format, int dx, int dy,
                       pair<int, bool> index) : type(type), text(text),
                                                format(format), dx(dx), dy(dy) {
  legIndex = index.first;
  linearLegIndex = index.second;
}

oPrintPost::oPrintPost(const wstring& image,
                       int style, int dx, int dy,
                       int width, int height) : type(lImage), text(image),
                                                format(style), dx(dx), dy(dy),
                                                fixedWidth(width), fixedHeight(height) {
  legIndex = -1;
  linearLegIndex = true;
}


bool oListInfo::needRegenerate(const oEvent &oe) const {
  for(oClassList::const_iterator it = oe.Classes.begin(); it != oe.Classes.end(); ++it) {
    if (it->isRemoved())
      continue;

    if (!lp.selection.empty() && lp.selection.count(it->getId()) == 0 )
      continue; // Not our class

    int legToCheck = -1;
    if (needPunches == PunchMode::SpecificPunch) {
      int to = oControl::getIdIndexFromCourseControlId(lp.useControlIdResultTo).first;
      int from = oControl::getIdIndexFromCourseControlId(lp.useControlIdResultFrom).first;

      if (it->wasSQLChanged(legToCheck, to) ||
          it->wasSQLChanged(legToCheck, from) )
        return true;
    }
    else if (needPunches == PunchMode::AnyPunch) {
      if (it->wasSQLChanged(legToCheck, -2))
        return true;
    }
    else {
      if (it->wasSQLChanged(legToCheck, -1))
        return true;
    }
  }

  return false;
}

void generateNBestHead(const oListParam &par, oListInfo &li, int ypos) {
  if (par.filterMaxPer > 0)
    li.addHead(oPrintPost(lString, lang.tl(L"Visar de X bästa#" + itow(par.filterMaxPer)), normalText, 0, ypos));
}

extern gdioutput *gdi_main;

pair<wstring, bool> getControlName(const oEvent &oe, int courseContolId) {
  pair<int, int> idt = oControl::getIdIndexFromCourseControlId(courseContolId);
  pControl to = oe.getControl(idt.first);
  wstring toS;
  bool name = false;
  if (to) {
    if (to->hasName()) {
      toS = to->getName();
      name = true;
    }
    else if (to->getFirstNumber()>0)
      toS = itow(to->getFirstNumber());
    else
      toS = itow(idt.first);

    if (to->getNumberDuplicates() > 0)
      toS += L"-" + itow(idt.second + 1);
  }
  else
    toS = itow(idt.first);

  return make_pair(toS, name);
}

wstring getFullControlName(const oEvent &oe, int ctrl) {
  pair<wstring, bool> toS = getControlName(oe, ctrl);
  if (toS.second)
    return toS.first;
  else
    return lang.tl(L"Kontroll X#" + toS.first);
}

void getResultTitle(const oEvent &oe, const oListParam &lp, wstring &title) {
  if (lp.useControlIdResultTo <= 0 && lp.useControlIdResultFrom <= 0)
    title = lang.tl(L"Resultat - %s");
  else if (lp.useControlIdResultTo>0 && lp.useControlIdResultFrom<=0){
    pair<wstring, bool> toS = getControlName(oe, lp.useControlIdResultTo);
    if (toS.second)
      title = lang.tl(L"Resultat - %s") + L", " + toS.first;
    else
      title = lang.tl(L"Resultat - %s") + L", " + lang.tl(L"vid kontroll X#" + toS.first);
  }
  else {
    wstring fromS = lang.tl(L"Start"), toS = lang.tl(L"Mål");
    if (lp.useControlIdResultTo>0) {
      toS = getControlName(oe, lp.useControlIdResultTo).first;
    }
    if (lp.useControlIdResultFrom>0) {
      fromS = getControlName(oe, lp.useControlIdResultFrom).first;
    }
    title = lang.tl(L"Resultat mellan X och Y#" + fromS + L"#" + toS);
  }
}

double adjustmentFactor(double par, double target) {
  double k = (1.0 - target)/10;
  double val = max(target, 1.0 - (par * k));
  return val;
}


template<typename T, int size> class WordMeasure {
  multimap<int, T> words;
public:
  WordMeasure() {}

  void add(const T &word) {
    if (words.size() > size) {
      size_t blen = words.begin()->first;
      if (word.length() <= blen)
        return;
      else {
        words.erase(words.begin());
        words.insert(make_pair(word.length(), word));
      }
    }
    else {
      words.insert(make_pair(word.length(), word));
    }
  }

  int measure(const gdioutput &gdi, 
              gdiFonts font,
              const wchar_t *fontFace,
              T &longest) {   

    int w = 0;
    TextInfo ti;
    HDC hDC = GetDC(gdi.getHWNDTarget());
    
    for (auto it = words.begin(); it != words.end(); ++it) {
      ti.xp = 0;
      ti.yp = 0;
      ti.format = font;
      ti.text = it->second;
      ti.font = fontFace != 0 ? fontFace : L"";
      gdi.calcStringSize(ti, hDC);
      if (ti.textRect.right > w) {
        w = ti.textRect.right;
        longest = ti.text;
      }
    }

    ReleaseDC(gdi.getHWNDTarget(), hDC);
    return w;
  }

};

EPostType oListInfo::transformType(oEvent& oe, EPostType in) const {
  if (transformStatus == -1)
    transformStatus = oe.getPropertyBool("CompactClubName", false) ? 1 : 0;
  if (transformStatus) {
    switch (in) {
    case lClubName:
      return lClubNameShort;
    case lRunnerClub:
      return lRunnerClubShort;
    case lTeamClub:
      return lTeamClubShort;
    case lPatrolClubNameNames:
      return lPatrolClubNameNamesShort;
    case lRunnerCompleteName:
      return lRunnerCompleteNameCompactClub;
    }
  }
  return in;
}

void oListInfo::setNoTransform() {
  transformStatus = 0;
}

void oListInfo::transformTypes(oEvent& oe) const {
  for (auto& pp : head)
    pp.type = transformType(oe, pp.type);

  for (auto& pp : subHead)
    pp.type = transformType(oe, pp.type);

  for (auto& pp : listPost)
    pp.type = transformType(oe, pp.type);

  for (auto& pp : subListPost)
    pp.type = transformType(oe, pp.type);
}

int oListInfo::getMaxCharWidth(oEvent &oe,
                               const gdioutput &gdi,
                               const set<int> &clsSel,
                               const vector<tuple<EPostType, int, wstring>> &typeFormats,
                               gdiFonts font,
                               const wchar_t *fontFace,
                               bool large, int minSize) const {
  vector<oPrintPost> pps;
  for (size_t k = 0; k < typeFormats.size(); k++) {
    pps.emplace_back();
    pps.back().text = get<wstring>(typeFormats[k]);
    pps.back().type = get<EPostType>(typeFormats[k]);
    pps.back().legIndex = get<int>(typeFormats[k]);
  }

  oListParam par;
  par.setLegNumberCoded(0);
  oCounter c;
  vector<WordMeasure<wstring, 32>> extras(pps.size());

  for (size_t k = 0; k < pps.size(); k++) {
    wstring extra;
    EPostType type = transformType(oe, pps[k].type);

    switch (type) {
    case lResultModuleNumber:
    case lResultModuleNumberTeam:
      if (pps[k].text.length() > 1 && pps[k].text[0] == '@') {
        wstring tmp;
        int miLen = 0;
        for (int j = 0; j < 10; j++) {
          tmp = MetaList::fromResultModuleNumber(pps[k].text.substr(1), j, tmp);
          if (tmp.length() > miLen / 2) {
            miLen = tmp.length();
            extras[k].add(tmp);
          }
        }
      }
      else
        extra = L"999";
      break;
    case lRunnerCardVoltage:
      extra = L"3.00 V";
      break;
    case lPunchName:
    case lControlName:
    case lPunchNamedTime:
    case lPunchTeamTotalNamedTime: {
      wstring maxcn = lang.tl("Mål");
      vector<pControl> ctrl;
      oe.getControls(ctrl, false);
      for (pControl c : ctrl) {
        wstring cn = c->getName();
        if (cn.length() > maxcn.length())
          maxcn.swap(cn);
      }
      if (type == lPunchNamedTime)
        extra = maxcn + L": 50:50 (50:50)";
      if (type == lPunchTeamTotalNamedTime)
        extra = maxcn + L": 2:50:50 (50:50)";
      else
        maxcn.swap(extra);
    }
                                 break;
    case lRunnerFinish:
    case lRunnerCheck:
    case lPunchAbsTime:
    case lRunnerStart:
    case lTeamStart:
      extra = L"10:10:00";
      break;
    case lRunnerTotalTimeAfter:
    case lRunnerClassCourseTimeAfter:
    case lRunnerCourseTimeAfter:
    case lRunnerTimeAfterDiff:
    case lRunnerTempTimeAfter:
    case lRunnerTimeAfter:
    case lRunnerLostTime:
    case lTeamTimeAfter:
    case lTeamLegTimeAfter:
    case lTeamTotalTimeAfter:
    case lTeamTimeAdjustment:
    case lRunnerTimeAdjustment:
    case lRunnerGeneralTimeAfter:
    case lPunchTotalTimeAfter:
    case lPunchTeamTotalTimeAfter:
      extra = L"+10:00";
      break;
    case lTeamRogainingPointOvertime:
    case lRunnerRogainingPointOvertime:
    case lResultModuleTime:
    case lResultModuleTimeTeam:
    case lTeamTime:
    case lTeamGrossTime:
    case lTeamTotalTime:
    case lTeamTotalTimeStatus:
    case lTeamLegTimeStatus:
    case lTeamTimeStatus:
    case lRunnerTempTimeStatus:
    case lRunnerTotalTimeStatus:
    case lRunnerTotalTime:
    case lClassStartTime:
    case lRunnerTime:
    case lRunnerGrossTime:
    case lRunnerTimeStatus:
    case lRunnerStageTime:
    case lRunnerStageTimeStatus:
    case lRunnerStageStatus:
    case lRunnerTimePlaceFixed:
    case lPunchLostTime:
    case lPunchTimeSinceLast:
    case lPunchSplitTime:
    case lPunchNamedSplit:
    case lRogainingLegBestTime:
      extra = L"50:50";
      break;
    case lPunchTotalTime:
    case lPunchTeamTotalTime:
      extra = L"1:50:50";
      break;
    case lRunnerGeneralTimeStatus:
    case lClassStartTimeRange:
      extra = L"50:50 (50:50)";
      break;
    case lTeamRogainingPointReduction:
    case lRunnerRogainingPointReduction:
    case lTeamPointAdjustment:
    case lRunnerPointAdjustment:
    case lRunnerRogainingPointGross:
    case lRunnerPlace:
    case lRunnerPlaceDiff:
    case lTeamPlaceDiff:
    case lRunnerTotalPlace:
    case lRunnerClassCoursePlace:
    case lRunnerCoursePlace:
    case lTeamPlace:
    case lTeamTotalPlace:
    case lPunchControlPlace:
    case lPunchControlPlaceAcc:
    case lPunchControlPlaceTeamAcc:
    case lRunnerStagePlace:
    case lRunnerDataA:
    case lRunnerDataB:
    case lTeamDataA:
    case lTeamDataB:
    case lClassDataA:
    case lClassDataB:
    case lRogainingLegFrom:
    case lRogainingLegTo:
    case lRogainingLegNumCompetitors:

      extra = L"99.";
      break;
    case lRunnerGeneralPlace:
      extra = L"99. (99.)";
      break;
    }

    if (type == lClubName || type == lRunnerName || type == lTeamName || type == lTeamNameRaw
      || type == lTeamRunner || type == lTeamClub || type == lRunnerClub)
      extras[k].add(L"IK Friskus Varberg");

    if (type == lClubNameShort || type == lRunnerClubShort || type == lTeamClubShort)
      extras[k].add(L"Sundsvall");

    if (type == lRunnerGivenName || type == lRunnerFamilyName ||
      type == lRunnerNationality || type == lRunnerLegTeamLeaderName || type == lPatrolClubNameNamesShort)
      extras[k].add(L"Karl-Gunnar");

    if (type == lRunnerCompleteName || type == lRunnerCompleteNameCompact || type == lRunnerCompleteNameCompactClub ||
      type == lPatrolNameNames || type == lPatrolClubNameNames)
      extras[k].add(L"Karl-Gunnar Alexandersson");

    extras[k].add(extra);
  }

  bool hasRG = oe.hasRogaining();
  RogainingLegInfo rgLeg;
  rgLeg.from = lang.tl("Start");
  rgLeg.to = lang.tl("Mål");
  rgLeg.bestTime = timeConstHour;

  for (size_t k = 0; k < pps.size(); k++) {
    const oPrintPost &pp = pps[k];
   
    int cardSkip = 0;
    int skip = 0;
    wstring last;
    for (auto crd = oe.Cards.begin(); crd != oe.Cards.end(); ++crd) {
      if (--skip > 0)
        continue;
      skip = cardSkip++;

      for (auto &p : crd->punches) {
        pRunner r = crd->tOwner;
        p.previousPunchTime = 0;
        const wstring &out = oe.formatPunchString(pp, par, r ? r->getTeam() : nullptr, r, &p, c);
        if (last == out)
          break;
        else
          last = out;
        extras[k].add(out);
      }
    }

    for (auto &cls : oe.Classes) {
      if (cls.isRemoved())
        continue;
      if (!clsSel.empty() && clsSel.count(cls.getId()) == 0)
        continue;

      const wstring &out = oe.formatListString(pp, par, 0, 0, 0, pClass(&cls), c);
      extras[k].add(out);
    }

    for (auto it = oe.Courses.begin(); it != oe.Courses.end(); ++it) {
      if (it->isRemoved())
        continue;

      const wstring &out = oe.formatSpecialString(pp, par, 0, 0, pCourse(&*it), 0,  c);
      extras[k].add(out);
    }

    for (auto it = oe.Controls.begin(); it != oe.Controls.end(); ++it) {
      if (it->isRemoved())
        continue;

      const wstring &out = oe.formatSpecialString(pp, par, 0, 0, 0, pControl(&*it),  c);
      extras[k].add(out);
    }

    if (hasRG) {
      const wstring &out = oe.formatRogainingString(pp, par, &rgLeg);
      extras[k].add(out);
    }
  }

  vector<int> row(pps.size(), 0);
  vector<wstring> samples(pps.size());
  wstring totWord = L"";
  for (size_t k = 0; k < pps.size(); k++) {
    extras[k].measure(gdi, font, fontFace, samples[k]);
    totWord += samples[k];
  }
  
  WordMeasure<wstring, 64> totMeasure;
  totMeasure.add(totWord);
  for (auto it = oe.Runners.begin(); it != oe.Runners.end(); ++it) {
    if (it->isRemoved())
      continue;

    // Case when runner/team has different class
    bool teamOK = it->getTeam() && clsSel.count(it->getTeam()->getClassId(false));

    if (!clsSel.empty() && (!teamOK && clsSel.count(it->getClassId(true)) == 0))
        continue;

    totWord.clear();
    wstring rout;
    for (size_t k = 0; k < pps.size(); k++) {
      oPrintPost &pp = pps[k];
      
      if (MetaList::isLegBased(pp.type)) {
        pp.legIndex = it->tLeg;
        pp.linearLegIndex = true;
      }
      int numIter = 1;

      if (pp.type == lPunchNamedTime || pp.type == lPunchTime || pp.type == lPunchTeamTime) {
        row[k] = max(row[k], 10);
        pRunner r = pRunner(&*it);
        numIter = (r && r->getCard()) ? r->getCard()->getNumPunches() + 1 : 1;
      }
      rout.clear();
      while (numIter-- > 0) {
        const wstring &out = oe.formatListString(pp, par, it->tInTeam, pRunner(&*it), it->Club, pClass(it->getClassRef(true)), c);
        //row[k] = max(row[k], int(out.length()));
        if (out.length() > rout.length())
          rout = out;
        if (numIter>0)
          c.level3++;
      }

      if (rout.length() > samples[k].length())
        totWord.append(rout);
      else
        totWord.append(samples[k]);

    }
    totMeasure.add(totWord);
  }

  wstring dummy;
  int w = totMeasure.measure(gdi, font, fontFace, dummy);
  w = max(w, gdi.scaleLength(minSize));
  return int(0.5 + (w + (large ? 5 : 15))/gdi.getScale());
}

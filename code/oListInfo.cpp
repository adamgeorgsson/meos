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

static void generateNBestHead(const oListParam &par, oListInfo &li, int ypos) {
  if (par.filterMaxPer > 0)
    li.addHead(oPrintPost(lString, lang.tl(L"Visar de X bästa#" + itow(par.filterMaxPer)), normalText, 0, ypos));
}

extern gdioutput *gdi_main;

static pair<wstring, bool> getControlName(const oEvent &oe, int courseContolId) {
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

static wstring getFullControlName(const oEvent &oe, int ctrl) {
  pair<wstring, bool> toS = getControlName(oe, ctrl);
  if (toS.second)
    return toS.first;
  else
    return lang.tl(L"Kontroll X#" + toS.first);
}

static void getResultTitle(const oEvent &oe, const oListParam &lp, wstring &title) {
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

static double adjustmentFactor(double par, double target) {
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

const wstring & oEvent::formatListString(EPostType type, const pRunner r) const
{
  oPrintPost pp;
  oCounter ctr;
  oListParam par;
  par.setLegNumberCoded(r->tLeg);
  pp.type = type;
  return formatListString(pp, par, r->tInTeam, r, r->Club, r->getClassRef(true), ctr);
}

const wstring & oEvent::formatListString(EPostType type, const pRunner r, 
                                         const wstring &format) const {
  oPrintPost pp;
  oCounter ctr;
  oListParam par;
  par.setLegNumberCoded(r->tLeg);
  pp.type = type;
  pp.text = format;
  return formatListString(pp, par, r->tInTeam, r, r->Club, r->getClassRef(true), ctr);
}


const wstring &oEvent::formatListString(const oPrintPost &pp, const oListParam &par,
                                        const pTeam t, const pRunner r, const pClub c,
                                        const pClass pc, oCounter &counter) const {
  const oPrintPost *cpp = &pp;
  const wstring *tmp = 0;
  wstring *out = 0;
  while (cpp) {
    if (tmp) {
      if (!out) {
        out = &StringCache::getInstance().wget();
        *out = L"";
      }
      out->append(*tmp);
    }
    tmp = &formatListStringAux(*cpp, par, t, r, c, pc, counter);
    cpp = cpp->mergeWithTmp;
  }

  if (out) {
    out->append(*tmp);
    return *out;
  }
  else
    return *tmp;
}

const wstring &oEvent::formatPunchString(const oPrintPost &pp, const oListParam &par,
                                         const pTeam t, const pRunner r,
                                         const oPunch *punch, oCounter &counter) const {
  const oPrintPost *cpp = &pp;
  const wstring *tmp = 0;
  wstring *out = 0;
  while (cpp) {
    if (tmp) {
      if (!out) {
        out = &StringCache::getInstance().wget();
        *out = L"";
      }
      out->append(*tmp);
    }
    tmp = &formatPunchStringAux(*cpp, par, t, r, punch, counter);
    cpp = cpp->mergeWithTmp;
  }

  if (out) {
    out->append(*tmp);
    return *out;
  }
  else
    return *tmp;
}

const wstring &oEvent::formatRogainingString(const oPrintPost &pp, const oListParam &par,
                                             const RogainingLegInfo *rgLeg) const {
  const oPrintPost *cpp = &pp;
  const wstring *tmp = 0;
  wstring *out = 0;
  while (cpp) {
    if (tmp) {
      if (!out) {
        out = &StringCache::getInstance().wget();
        *out = L"";
      }
      out->append(*tmp);
    }
    tmp = &formatRogainingStringAux(*cpp, par, rgLeg);
    cpp = cpp->mergeWithTmp;
  }

  if (out) {
    out->append(*tmp);
    return *out;
  }
  else
    return *tmp;
}

const wstring &oEvent::formatRogainingStringAux(const oPrintPost &pp, const oListParam &par, const RogainingLegInfo *rgLeg) const {
  wchar_t bfw[128];
  const wstring *wsptr = 0;
  bfw[0] = 0;

  if (rgLeg) {
    switch (pp.type) {
    case lRogainingLeg:
    {
      wstring tmp = rgLeg->from + L" \u2192 " + rgLeg->to;
      wcscpy_s(bfw, tmp.c_str());
    }
      break;
    case lRogainingLegFrom:
      wsptr = &rgLeg->from;
      break;
    case lRogainingLegTo:
      wsptr = &rgLeg->to;
      break;
    case lRogainingLegBestTime:
      wsptr = &formatTime(rgLeg->bestTime, SubSecond::Off);
      break;
    case lRogainingLegNumCompetitors:
      wsptr = &itow(rgLeg->numCompetitors);
      break;
    }
  }
  if (pp.type != lString && (wsptr == 0 || wsptr->empty()) && bfw[0] == 0)
    return _EmptyWString;
  else if (wsptr) {
    if (pp.text.empty())
      return *wsptr;
    else {
      swprintf(bfw, sizeof(bfw)/sizeof(wchar_t), pp.text.c_str(), wsptr->c_str());
      wstring &res = StringCache::getInstance().wget();
      res = bfw;
      return res;
    }
  }
  else {
    if (pp.text.empty()) {
      wstring &res = StringCache::getInstance().wget();
      res = bfw;
      return res;
    }
    else {
      wchar_t bf2[512];
      swprintf(bf2, sizeof(bf2)/sizeof(wchar_t), pp.text.c_str(), bfw);
      wstring &res = StringCache::getInstance().wget();
      res = bf2;
      return res;
    }
  }
}

const wstring &oEvent::formatSpecialString(const oPrintPost &pp, const oListParam &par, const pTeam t, int legIndex,
                                           const pCourse crs, const pControl ctrl, oCounter &counter) const {
  const oPrintPost *cpp = &pp;
  const wstring *tmp = 0;
  wstring *out = 0;
  while (cpp) {
    if (tmp) {
      if (!out) {
        out = &StringCache::getInstance().wget();
        *out = L"";
      }
      out->append(*tmp);
    }
    tmp = &formatSpecialStringAux(*cpp, par, t, legIndex, crs, ctrl, counter);
    cpp = cpp->mergeWithTmp;
  }

  if (out) {
    out->append(*tmp);
    return *out;
  }
  else
    return *tmp;
}

const wstring &oEvent::formatPunchStringAux(const oPrintPost &pp, const oListParam &par,
                                            const pTeam t, const pRunner r,
                                            const oPunch *punch, oCounter &counterIn) const {
  wchar_t bfw[128];
  const wstring *wsptr = 0;
  bfw[0] = 0;
  pClass pc = r ? r->getClassRef(true) : 0;
  bool invalidClass = pc && pc->getClassStatus() != oClass::ClassStatus::Normal;
  oCounter counter(counterIn);

  static bool reentrantLock = false;
  if (reentrantLock == true) {
    reentrantLock = false;
    throw meosException("Internal list error");
  }
  bool doDefault = false;

  switch (pp.type) {
      case lControlName:
      case lPunchName:
        if (punch) {
          pCourse pc = r ? r->getCourse(false) : nullptr;
          if (punch->isFinish(pc ? pc->getFinishPunchType() : oPunch::PunchFinish)) {
            wsptr = &lang.tl("Mål");
          }
          else {
            pControl ctrl = getControl(punch->getControlId());
            if (!ctrl)
              ctrl = getControlByType(punch->type);

            if (ctrl && ctrl->hasName()) {
              swprintf(bfw, sizeof(bfw)/sizeof(wchar_t), L"%s", ctrl->getName().c_str());
            }
          }
        }
        break;
  case lPunchTimeSinceLast:
    if (punch && punch->previousPunchTime && r && !invalidClass) {
      int time = punch->getTimeInt();
      int pTime = punch->previousPunchTime;
      if (pTime > 0 && time > pTime) {
        int t = time - pTime;
        wsptr = &formatTime(t);
      }
    }
    break;
  case lPunchTime:
  case lPunchTeamTime:
  case lPunchControlNumber:
  case lPunchControlCode:
  case lPunchLostTime:
  case lPunchControlPlace:
  case lPunchControlPlaceAcc:
  case lPunchControlPlaceTeamAcc:
  case lPunchSplitTime:
  case lPunchTotalTime:
  case lPunchTeamTotalTime:
  case lPunchAbsTime:
    if (punch && r && !invalidClass) {
      if (punch->tIndex >= 0) {
        // Punch in course
        counter.level3 = punch->tIndex;
        doDefault = true;
        break;
      }
      switch (pp.type) {
        case lPunchTime:
        case lPunchTeamTime: {
          if (punch->hasTime()) {
            int off = 0;
            if (pp.type == lPunchTeamTime && r->getTeam())
              off = r->getTeam()->getTotalRunningTimeAtLegStart(r->getLegNumber(), false);

            swprintf(bfw, sizeof(bfw)/sizeof(wchar_t), L"\u2013 (%s)", formatTime(off + punch->getTimeInt() - r->getStartTime(), SubSecond::Off).c_str());
          }
          else {
            wsptr = &makeDash(L"- (-)");
          }
          break;
        }
        case lPunchSplitTime: {
          swprintf(bfw, sizeof(bfw)/sizeof(wchar_t), L"\u2013");
          break;
        }
        case lPunchControlNumber: {
          wcscpy_s(bfw, L"\u2013");
          break;
        }
        case lPunchControlCode: {
          swprintf(bfw, sizeof(bfw)/sizeof(wchar_t), L"%d", punch->getTypeCode());
          break;
        }
        case lPunchAbsTime: {
          if (punch->hasTime())
            wsptr = &getAbsTime(punch->getTimeInt(), SubSecond::Off);
          break;
        }
        case lPunchTotalTime: {
          if (punch->hasTime())
            wsptr = &formatTime(punch->getTimeInt() - r->getStartTime(), SubSecond::Off);
          break;
        }
        case lPunchTeamTotalTime: {
          if (punch->hasTime()) {
            pTeam t = r->getTeam();
            if (!t || r->getLegNumber() == 0)
              wsptr = &formatTime(punch->getTimeInt() - r->getStartTime(), SubSecond::Off);
            else {
              int input = t->getTotalRunningTimeAtLegStart(r->getLegNumber(), false);
              wsptr = &formatTime(input + punch->getTimeInt() - r->getStartTime(), SubSecond::Off);
            }
          }
          break;
        }
      }
    }
    break;

  default:
    doDefault = true;
  }

  if (doDefault) {
    reentrantLock = true;
    try {
      const wstring &res = formatListStringAux(pp, par, t, r,
                                                r ? r->getClubRef() : 0,
                                                r ? r->getClassRef(true) : 0, counter);
      reentrantLock = false;
      return res;
    }
    catch (...) {
      reentrantLock = false;
      throw;
    }
  }

  if (pp.type != lString && (wsptr == 0 || wsptr->empty()) && bfw[0] == 0)
    return _EmptyWString;
  else if (wsptr) {
    if (pp.text.empty())
      return *wsptr;
    else {
      swprintf(bfw, sizeof(bfw)/sizeof(wchar_t), pp.text.c_str(), wsptr->c_str());
      wstring &res = StringCache::getInstance().wget();
      res = bfw;
      return res;
    }
  }
  else {
    if (pp.text.empty()) {
      wstring &res = StringCache::getInstance().wget();
      res = bfw;
      return res;
    }
    else {
      wchar_t bf2[512];
      swprintf(bf2, sizeof(bf2)/sizeof(wchar_t), pp.text.c_str(), bfw);
      wstring &res = StringCache::getInstance().wget();
      res = bf2;
      return res;
    }
  }
}

const wstring &oEvent::formatSpecialStringAux(const oPrintPost &pp, const oListParam &par,
                                              const pTeam t, int legIndex,
                                              const pCourse pc, const pControl ctrl, 
                                              const oCounter &counter) const {

  wchar_t bfw[512];
  const wstring *wsptr=0;
  bfw[0] = 0;

  static bool reentrantLock = false;
  if (reentrantLock == true) {
    reentrantLock = false;
    throw meosException("Internal list error");
  }

  switch (pp.type) {
    case lCourseLength:
      if (pc) {
        int len = pc->getLength();
        if (len > 0)
          swprintf(bfw, sizeof(bfw)/sizeof(wchar_t), L"%d", len);
      }
    break;

    case lTeamCourseName:
    case lCourseName:
    case lRunnerCourse:
      if (pc) {
        wsptr = &pc->getName();
      }
    break;

    case lCourseNumber:
    case lTeamCourseNumber:
      if (pc) {
        wsptr = &itow(pc->getId());
      }
    break;

    case lRunnerLegNumberAlpha:
      if (t && t->getClassRef(false) && legIndex >= 0) {
        wstring legStr = t->getClassRef(false)->getLegNumber(legIndex);
        wcscpy_s(bfw, legStr.c_str());
      }
      break;

    case lRunnerLegNumber:
      if (t && t->getClassRef(false) && legIndex >= 0) {
         int legNumber, legOrder;
         t->getClassRef(false)->splitLegNumberParallel(legIndex, legNumber, legOrder);
         wsptr = &itow(legNumber+1);
      }
      break;

    case lCourseClimb: {
      int len = pc ? pc->getDCI().getInt("Climb") : 0;
      if (len > 0)
        swprintf(bfw, sizeof(bfw)/sizeof(wchar_t), L"%d", len);      
    }
    break;

    case lCourseUsage:
      if (pc) {
        wsptr = &itow(pc->getNumUsedMaps(false));
      }
    break;

    case lCourseUsageNoVacant:
      if (pc) {
        wsptr = &itow(pc->getNumUsedMaps(true));
      }
    break;

    case lCourseNumControls:
      if (pc)
        wsptr = &itow(pc->getNumControls());
    break;

    case lCourseClasses:
      if (pc) {
        vector<pClass> cls;
        pc->getClasses(cls);
        wstring tmp;
        for (size_t k = 0; k < cls.size(); k++) {
          if (k > 0)
            tmp += L", ";
          tmp += cls[k]->getName();
          if (tmp.length() > 100) {
            tmp += L", ...";
            break;
          }
        }
        wcsncpy_s(bfw, tmp.c_str(), 256);
      }
    break;

    case lControlName:
      if (ctrl)
        wcsncpy_s(bfw, ctrl->getName().c_str(), 128);
    break;

    case lControlCourses:
      if (ctrl) {
        vector<pCourse> crs;
        ctrl->getCourses(crs);
        if (crs.size() == Courses.size()) {
          wsptr = &lang.tl("Alla");
          break;
        }

        wstring tmp;
        for (size_t k = 0; k < crs.size(); k++) {
          if (k > 0)
            tmp += L", ";
          tmp += crs[k]->getName();
          if (tmp.length() > 100) {
            tmp += L", ...";
            break;
          }
        }
        wcsncpy_s(bfw, tmp.c_str(), 256);
      }
    break;
  
    case lControlClasses:
      if (ctrl) {
        vector<pClass> cls;
        ctrl->getClasses(cls);
        if (cls.size() == getNumClasses()) {
          wsptr = &lang.tl("Alla");
          break;
        }

        wstring tmp;
        for (size_t k = 0; k < cls.size(); k++) {
          if (k > 0)
            tmp += L", ";
          tmp += cls[k]->getName();
          if (tmp.length() > 100) {
            tmp += L", ...";
            break;
          }
        }
        wcsncpy_s(bfw, tmp.c_str(), 256);
      }
    break;

    case lControlVisitors:
      if (ctrl)
        wsptr = &itow(ctrl->getNumVisitors(false));
    break;

    case lControlPunches:
      if (ctrl)
        wsptr = &itow(ctrl->getNumVisitors(true));
      break;

    case lControlMedianLostTime:
      if (ctrl) 
        wsptr = &formatTime(ctrl->getMissedTimeMedian(), SubSecond::Off); 
    break;

    case lControlMaxLostTime:
      if (ctrl) 
        wsptr = &formatTime(ctrl->getMissedTimeMax(), SubSecond::Off);
      break;
    
    case lControlMistakeQuotient:
      if (ctrl) {
        wstring tmp = itow(ctrl->getMistakeQuotient()); 
        tmp += L"%";
        wcsncpy_s(bfw, tmp.c_str(), 20);
      }
      break;

    case lControlRunnersLeft:
      if (ctrl)
        wsptr = &itow(ctrl->getNumRunnersRemaining());
    break;

    case lControlCodes: 
      if (ctrl) {
        vector<int> numbers;
        ctrl->getNumbers(numbers);
        wstring tmp;
        for (size_t j = 0; j < numbers.size(); j++) {
          if (j > 0)
            tmp += L", ";
          tmp += itow(numbers[j]);
        }
        wcsncpy_s(bfw, tmp.c_str(), 256);
      }
      break;

    default: {
      reentrantLock = true;
      try {
        const wstring &res = formatListStringAux(pp, par, t, t ? t->getRunner(legIndex) : 0, 
                                                                 t ? t->getClubRef() : 0,
                                                                 t ? t->getClassRef(false) : 0, counter);
        reentrantLock = false;
        return res;
      }
      catch(...) {
        reentrantLock = false;
        throw;
      }
    }
  }

  if (pp.type!=lString && (wsptr==0 || wsptr->empty()) && bfw[0]==0)
    return _EmptyWString;
  else if (wsptr) {
    if (pp.text.empty())
      return *wsptr;
    else {
      swprintf(bfw, sizeof(bfw)/sizeof(wchar_t), pp.text.c_str(), wsptr->c_str());
      wstring &res = StringCache::getInstance().wget();
      res = bfw;
      return res;
    }
  }
  else {
    if (pp.text.empty()) {
      wstring &res = StringCache::getInstance().wget();
      res = bfw;
      return res;
    }
    else {
      wchar_t bf2[512];
      swprintf(bf2, sizeof(bf2)/sizeof(wchar_t), pp.text.c_str(), bfw);
      wstring &res = StringCache::getInstance().wget();
      res = bf2;
      return res;
    }
  }
}

const wstring* formatAnnotation(const wstring &wsptrIn, wchar_t* wbfOut, int rowNumber) {
  int numRow = 0;
  int atIx = 0;
  for (int j = 0; j < wsptrIn.length(); j++) {
    if (wsptrIn[j] == '@') {
      atIx = j + 1;
      break;
    }
  }
  int startIx = atIx;
  for (int j = startIx; j < wsptrIn.length(); j++) {
    if (wsptrIn[j] == '\n') {
      numRow++;
      if (rowNumber == numRow) {
        int x = 0;
        for (int i = startIx; i < j && x < 511; i++) {
          if (wsptrIn[i] != '\r')
            wbfOut[x++] = wsptrIn[i];
        }
        wbfOut[x] = 0;
        return nullptr;
      }
      startIx = j + 1;
    }
  }

  if (rowNumber > 0) {
    if (rowNumber == numRow + 1) {
      int x = 0;
      for (int i = startIx; i < wsptrIn.length() && x < 511; i++) {
        if (wsptrIn[i] != '\r')
          wbfOut[x++] = wsptrIn[i];
      }
      wbfOut[x] = 0;
      return nullptr;
    }

    wbfOut[0] = 0; // Empty output
    return nullptr;
  }
  else if (numRow == 0 && atIx == 0) {
    return &wsptrIn; // Entire input
  }
  else {
    // Everything. Remove new lines etc
    int x = 0;
    for (int i = atIx; i < wsptrIn.length() && x < 511; i++) {
      if (wsptrIn[i] == '\n' || wsptrIn[i] == '\t')
        wbfOut[x++] += ' ';
      else if (wsptrIn[i] != '\r')
        wbfOut[x++] = wsptrIn[i];
    }

    wbfOut[x] = 0; 
    return nullptr;
  }
}

const wstring &oEvent::formatListStringAux(const oPrintPost &pp, const oListParam &par,
                                          const pTeam t, const pRunner r, const pClub c,
                                          const pClass pc, const oCounter &counter) const {

  wchar_t wbf[512] = { 0 };
  const wstring *wsptr = nullptr;  
  SubSecond mode = useSubSecond() ? SubSecond::On : SubSecond::Auto;
  int textOffset = 0;
  auto type = pp.type;

  auto noTimingRunner = [&]() {
    return (pc ? pc->getNoTiming() : false) || (r ? (r->getStatusComputed(true) == StatusNoTiming || r->noTiming()) : false);
  };
  auto noTimingTeam = [&]() {
    return (pc ? pc->getNoTiming() : false) || (t ? (t->getStatusComputed(true) == StatusNoTiming || t->noTiming()): false);
  };
  bool invalidClass = pc && pc->getClassStatus() != oClass::ClassStatus::Normal;
  int legIndex = pp.legIndex;
  if(pc && MetaList::isAllLegType(type)) {
    if (legIndex == -1) {
      if (r)
        legIndex = r->getLegNumber();
      else
        legIndex = pc->getNumStages() - 1;
    }
    else if (legIndex < max<int>(1, pc->getNumStages()))
      legIndex = pc->getLinearIndex(pp.legIndex, pp.linearLegIndex);
  }    
  else if (pc && MetaList::isLegBased(type)) {
    if (legIndex == -1) {
      if (r)
        legIndex = r->getLegNumber();
      else
        legIndex = pc->getNumStages() - 1;
    }
    else
      legIndex = pc->getLinearIndex(pp.legIndex, pp.linearLegIndex);
  }
        
  switch (type) {
    case lClassName:
      if (invalidClass)
        swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%s (%s)", pc->getName().c_str(), lang.tl("Struken").c_str());
      else
        wsptr=pc ? &pc->getName() : 0;
      break;
    case lResultDescription: {
        wstring title;
        getResultTitle(*this, par, title);
        wcscpy_s(wbf, title.c_str());
      }
      break;
    case lTimingFromName:
      if (par.useControlIdResultFrom > 0)
        wcscpy_s(wbf, getFullControlName(*this, par.useControlIdResultFrom).c_str());
      else
        wsptr = &lang.tl(L"Start");
      break;
    case lTimingToName:
      if (par.useControlIdResultTo > 0)
        wcscpy_s(wbf, getFullControlName(*this, par.useControlIdResultTo).c_str());
      else
        wsptr = &lang.tl(L"Mål");
      break;
    case lClassLength:
      if (pc) {
        wcscpy_s(wbf, pc->getLength(par.relayLegIndex).c_str());
      }
      break;
    case lClassStartName:
      if (pc) wcscpy_s(wbf, pc->getDI().getString("StartName").c_str());
      break;
    case lClassStartTime:
    case lClassStartTimeRange:
      if (pc) {
        int first, last;
        pc->getStartRange(legIndex, first, last);
        if (pc->hasFreeStart() || pc->hasRequestStart())
          wsptr = &lang.tl("Fri starttid");
        else if (first > 0 && first == last) {
          if (oe->useStartSeconds())
            wsptr = &oe->getAbsTime(first);
          else
            wsptr = &oe->getAbsTimeHM(first);
        }
        else if (type == lClassStartTimeRange) {
          wstring range =  oe->getAbsTimeHM(first) + makeDash(L" - ") + oe->getAbsTimeHM(last);
          wcscpy_s(wbf, range.c_str());
        }
      }
      break;
    case lClassResultFraction:
      if (pc && !invalidClass) {
        int total, finished,  dns;
        pc->getNumResults(par.getLegNumber(pc), total, finished, dns);
        swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"(%d / %d)", finished, max(finished, total-dns));
      }
      break;
    case lClassRemainInForest:
      if (pc && !invalidClass) {
        int total, finished, dns;
        pc->getNumResults(par.getLegNumber(pc), total, finished, dns);
        swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%d", max(0, total - dns - finished));
      }
      break;
    case lCourseLength:
    case lCourseName:
    case lRunnerCourse:
    case lCourseNumber:
      if (r) {
        pCourse crs = r->getCourse(false);
        return formatSpecialStringAux(pp, par, t, 0, crs, 0, counter);
      }
    break;

    case lTeamCourseNumber:
    case lTeamCourseName:
      if (r && pc && legIndex < pc->getNumStages()) {
        pCourse crs = r->getCourse(false);
        return formatSpecialStringAux(pp, par, t, 0, crs, 0, counter);
      }
      else if (t && pc && legIndex < pc->getNumStages()) {
        pCourse crs = pc->getCourse(legIndex, t->getStartNo());
        return formatSpecialStringAux(pp, par, t, 0, crs, 0, counter);
      }
      break;

    case lTeamLegName: 
      if (pc) {
        if (legIndex < pc->getNumStages())
          wcscpy_s(wbf, pc->getLegNumber(legIndex).c_str());
      }
      break;

    case lClassAvailableMaps:
      if (pc) {
        int n = pc->getNumRemainingMaps(false);
        if (n != numeric_limits<int>::min())
          wsptr = &itow(n);
      }
      break;

    case lClassTotalMaps:
      if (pc) {
        int n = pc->getNumberMaps();
        if (n > 0)
          wsptr = &itow(n);
      }
      break;
    
    case lClassNumEntries:
      if (pc) {
        int n = pc->getNumRunners(true, true, true);
        wsptr = &itow(n);
      }
      break;

    case lClassDataA:
      if (pc)
        wsptr = &itow(pc->getDCI().getInt("DataA"));
      break;

    case lClassDataB:
      if (pc)
        wsptr = &itow(pc->getDCI().getInt("DataB"));
      break;

    case lClassTextA:
      if (pc)
        wsptr = &pc->getDCI().getString("TextA");
      break;

    case lCourseClimb:
    case lCourseUsageNoVacant:
    case lCourseUsage:
      if (r) {
        pCourse crs = r->getCourse(false);
         return formatSpecialStringAux(pp, par, t, 0, crs, 0, counter);
      }
    break;

    case lCourseNumControls:
      if (r) {
        pCourse crs = r->getCourse(true);
        return formatSpecialStringAux(pp, par, t, 0, crs, 0, counter);
      }
      break;

    case lCourseShortening:
      if (r) {
        int sh = r->getNumShortening();
        if (sh > 0)
          swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%d", sh);
      }
      break;
    case lCmpName:
      wsptr = &getName();
      break;
    case lCmpDate:
      wsptr = &getDate();
      break;
    case lCurrentTime:
      wcscpy_s(wbf, getCurrentTimeS().c_str());
      break;
    case lRunnerClub:
      wsptr = (r && r->Club) ? &r->Club->getDisplayName() : nullptr;
      break;
    case lRunnerClubShort:
      wsptr = (r && r->Club) ? &r->Club->getCompactName() : nullptr;
      break;
    case lRunnerFinish:
      if (r && !invalidClass) {
        if (pp.resultModuleIndex == -1)
          wsptr = &r->getFinishTimeS(false, mode);
        else
          wsptr = &r->getTempResult(pp.resultModuleIndex).getFinishTimeS(this, mode);
      }
      break;
    case lRunnerStart:
    case lRunnerStartCond:
    case lRunnerStartZero:
      if (r) {
        if ((type == lRunnerStartCond || type == lRunnerStartZero) && pc && !pc->hasFreeStart()) {
          int fs, ls;
          pc->getStartRange(legIndex, fs, ls);
          if (fs>0 && fs == ls) {
            break; // Common start time, skip
          }
        }
        if (r->getStatus() == StatusCANCEL) {
          wsptr = &oEvent::formatStatus(StatusCANCEL, true);
        }
        else if (r->startTimeAvailable()) {
          if (type != lRunnerStartZero) 
            wsptr = &r->getStartTimeCompact();
          else {
            int st = r->getStartTime();
            wsptr = &formatTimeMS(st-oe->getFirstStart(0, false), true);
          }
        }
        else
          wsptr = &makeDash(L"-");
      }
      break;
    case lRunnerCheck:
      if (r && !invalidClass && r->Card) {
        oPunch *punch = r->Card->getPunchByType(oPunch::PunchCheck);
        if (punch && punch->hasTime())
          wsptr = &getAbsTime(punch->getTimeInt());
        else
          wsptr = &makeDash(L"-"); 
      }
      break;

    case lRunnerName:
      wsptr = r ? &r->getName() : 0;
      break;
    case lRunnerNameCompact:
      if (r) wcscpy_s(wbf, r->getCompactName().c_str());
      break;
    case lRunnerGivenName:
      if (r) wcscpy_s(wbf, r->getGivenName().c_str());
      break;
    case lRunnerFamilyName:
      if (r) wcscpy_s(wbf, r->getFamilyName().c_str());
      break;
    case lRunnerCompleteName:
      if (r) wcscpy_s(wbf, r->getCompleteIdentification(oRunner::IDType::ParallelLegExtra, oRunner::NameType::Default).c_str());
      break;
    case lRunnerCompleteNameCompact:
      if (r) wcscpy_s(wbf, r->getCompleteIdentification(oRunner::IDType::ParallelLegExtra, oRunner::NameType::Compact).c_str());
      break;
    case lRunnerCompleteNameCompactClub:
      if (r) wcscpy_s(wbf, r->getCompleteIdentification(oRunner::IDType::ParallelLegExtra, oRunner::NameType::CompactClub).c_str());
      break;
    case lRunnerLegTeamLeaderName: {
      pRunner rr = r;
      if (t && r)
        rr = t->getRunnerBestTimePar(r->getLegNumber());
      else if (t)
        rr = t->getRunnerBestTimePar(legIndex);

      if (rr)
        swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%s", rr->formatName(oRunner::NameFormat::InitLast).c_str());
    }
    break;
    case lPatrolNameNames:
      if (t) {
        pRunner r1 = t->getRunner(0);
        pRunner r2 = t->getRunner(1);
        if (r1 && r2 && r2->tParentRunner != r1) {
          swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%s / %s", r1->getName().c_str(),r2->getName().c_str());
        }
        else if (r1) {
          wsptr = &r1->getName();
        }
        else if (r2) {
          wsptr = &r2->getName();
        }
      }
      else {
        wsptr = r ? &r->getName() : 0;
      }
      break;
    case lPatrolClubNameNames:
    case lPatrolClubNameNamesShort:
      if (t) {
        pRunner r1 = t->getRunner(0);
        pRunner r2 = t->getRunner(1);
        pClub c1 = r1 ? r1->Club : nullptr;
        pClub c2 = r2 ? r2->Club : nullptr;
        if (c1 == c2)
          c2 = nullptr;

        if (type == lPatrolClubNameNames) {
          if (c1 && c2) {
            swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%s / %s", c1->getDisplayName().c_str(), c2->getDisplayName().c_str());
          }
          else if (c1) {
            wsptr = &c1->getDisplayName();
          }
          else if (c2) {
            wsptr = &c2->getDisplayName();
          }
        }
        else {
          if (c1 && c2) {
            swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%s / %s", c1->getCompactName().c_str(), c2->getCompactName().c_str());
          }
          else if (c1) {
            wsptr = &c1->getCompactName();
          }
          else if (c2) {
            wsptr = &c2->getCompactName();
          }
        }
      }
      else {
        if (type == lPatrolClubNameNames)
          wsptr = r && r->Club ? &r->Club->getDisplayName() : nullptr;
        else
          wsptr = r && r->Club ? &r->Club->getCompactName() : nullptr;
      }
      break;

    case lRunnerTime:
      if (r && !invalidClass) {
        if (pp.resultModuleIndex == -1)
          wsptr = &r->getRunningTimeS(true, mode);
        else
          wsptr = &r->getTempResult(pp.resultModuleIndex).getRunningTimeS(0, mode);

        if (r->getNumShortening() > 0) {
          swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"*%s", wsptr->c_str());
          wsptr = 0;
        }
      }
      break;
    case lRunnerGrossTime:
      if (r && !invalidClass) {
        int tm = r->getRunningTime(true);
        if (tm > 0)
          tm -= r->getTimeAdjustment(true);

        wsptr = &formatTime(tm);
      }
      break;

    case lRunnerTimeStatus:
      if (r) {
        if (invalidClass)
          wsptr = &lang.tl("Struken");
        else if (pp.resultModuleIndex == -1) {
          bool ok = r->prelStatusOK(true, true, true);
          if (ok && !noTimingRunner()) {
            wsptr = &r->getRunningTimeS(true, mode);
            if (r->getNumShortening() > 0) {
              swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"*%s", wsptr->c_str());
              wsptr = 0;
            }
            else if (r->getStatusComputed(true) == StatusOutOfCompetition) {
              swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"(%s)", wsptr->c_str());
              wsptr = 0;
            }
          }
          else {
            if (ok)
              wsptr = &formatStatus(StatusOK, true);
            else
              wsptr = &r->getStatusS(true, true);
          }
        }
        else {
          const oAbstractRunner::TempResult &res = r->getTempResult(pp.resultModuleIndex);
          if (res.isStatusOK() && !noTimingRunner()) {
            wsptr = &res.getRunningTimeS(0, mode);
            if (r->getNumShortening() > 0) {
              swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"*%s", wsptr->c_str());
              wsptr = 0;
            }
            else if (res.getStatus() == StatusOutOfCompetition) {
              swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"(%s)", wsptr->c_str());
              wsptr = 0;
            }
          }
          else
            wsptr = &res.getStatusS(StatusOK);
        }
      }
      break;

    case lRunnerGeneralTimeStatus:
      if (r) {
        if (invalidClass)
          wsptr = &lang.tl("Struken");
        else if (pp.resultModuleIndex == -1) {
          if (r->prelStatusOK(true, true, true) && !noTimingRunner()) {
            wstring timeStatus = r->getRunningTimeS(true, mode);
            
            if (r->hasInputData() || (r->getLegNumber() > 0 && !r->isPatrolMember())) {
              RunnerStatus ts = r->getTotalStatus();
              int rt = r->getTotalRunningTime();
              if (ts == StatusOK || (ts == StatusUnknown && rt > 0)) {
                wstring vts = formatTime(rt) + L" (" + timeStatus + L")";
                swap(vts, timeStatus);
              }
              else {
                wstring vts = formatStatus(ts, true) + L" (" + timeStatus + L")";
                swap(vts, timeStatus);
              }
            }

            if (r->getNumShortening() > 0) {
              swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"*%s", timeStatus.c_str());
            }
            else {
              wcscpy_s(wbf, timeStatus.c_str());
            }
          }
          else
            wsptr = &r->getStatusS(true, true);
        }
        else {
          const oAbstractRunner::TempResult &res = r->getTempResult(pp.resultModuleIndex);
          if (res.isStatusOK() && !noTimingRunner()) {
            wsptr = &res.getRunningTimeS(0, mode);
            if (r->getNumShortening() > 0) {
              swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"*%s", wsptr->c_str());
              wsptr = 0;
            }
            else if (res.getStatus() == StatusOutOfCompetition) {
              swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"(%s)", wsptr->c_str());
              wsptr = 0;
            }
          }
          else
            wsptr = &res.getStatusS(StatusOK);
        }
      }
      break;

      break;

    case lRunnerGeneralTimeAfter:
      if (r && pc && !invalidClass && !noTimingRunner()) {
        if (pp.resultModuleIndex == -1) {

          if (r->hasInputData() || (r->getLegNumber() > 0 && !r->isPatrolMember())) {
            int tleg = r->tLeg >= 0 ? r->tLeg:0;
            if (r->getTotalStatus()==StatusOK) {
              if ( (t && t->getNumShortening(tleg) == 0) || (!t && r->getNumShortening() == 0)) { 
                int after = r->getTotalRunningTime() - pc->getTotalLegLeaderTime(oClass::AllowRecompute::Yes, tleg, true, true);
                if (after > 0)
                  swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"+%s", formatTimeMS(after, false, mode).c_str());
              }
              else {
                wsptr = &makeDash(L"-");
              }
            }
          }
          else {
            int tleg=r->tLeg>=0 ? r->tLeg:0;
            if (r->tStatus==StatusOK && pc && !noTimingRunner()) {
              if (r->getNumShortening() == 0) {
                int after = r->getRunningTime(true) - pc->getBestLegTime(oClass::AllowRecompute::Yes, tleg, true);
                if (after > 0)
                  swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"+%s", formatTimeMS(after, false, mode).c_str());
              }
              else {
                wsptr = &makeDash(L"-");
              }
            }
          }
        }
        else {
          int after = r->getTempResult(pp.resultModuleIndex).getTimeAfter();
          if (after > 0)
            swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"+%s", formatTimeMS(after, false, mode).c_str());
        }
      }
      break;

    case lRunnerTimePerKM:
      if (r && !invalidClass && r->prelStatusOK(true, true, true)) {
        const pCourse pc = r->getCourse(false);
        if (pc) {
          int t = r->getRunningTime(false);
          int len = pc->getLength();
          if (len > 0 && t > 0) {
            int sperkm = (1000 * t) / len;
            wsptr = &formatTime(sperkm, SubSecond::Off);
          }
        }
      }
      break;
    case lRunnerTotalTime:
      if (r && !invalidClass) {
        if (pp.resultModuleIndex == -1)
          wsptr = &r->getTotalRunningTimeS(mode);
        else
          wsptr = &r->getTempResult(pp.resultModuleIndex).getRunningTimeS(r->getTotalTimeInput(), mode);
      }
      break;
    case lRunnerTotalTimeStatus:
      if (invalidClass)
        wsptr = &lang.tl("Struken");
      else if (r) {
        if (pp.resultModuleIndex == -1) {
          if ((r->getTotalStatus()==StatusOK || (r->getTotalStatus()==StatusUnknown 
            && r->prelStatusOK(true, true, true) && r->getInputStatus() == StatusOK) ) && !noTimingRunner()) {
            wsptr = &r->getTotalRunningTimeS(mode);
            if (r->getNumShortening() > 0) {
              swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"*%s", wsptr->c_str());
              wsptr = 0;
            }
          }
          else
            wsptr = &r->getTotalStatusS(true);
        }
        else {
          const oAbstractRunner::TempResult &res = r->getTempResult(pp.resultModuleIndex);
          RunnerStatus input = r->getTotalStatusInput();
          if (input == StatusOK && res.getStatus() == StatusOK && !noTimingRunner()) {
            wsptr = &res.getRunningTimeS(r->getTotalTimeInput(), mode);
            if (r->getNumShortening() > 0) {
              swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"*%s", wsptr->c_str());
              wsptr = 0;
            }
          }
          else
            wsptr = &res.getStatusS(input);
        }
      }
      break;
    case lRunnerTempTimeStatus:
      if (invalidClass)
          wsptr = &lang.tl("Struken");
      else if (r) {
        if (showResultTime(r->tempStatus, r->tempRT) && !noTimingRunner())
          wcscpy_s(wbf, formatTime(r->tempRT).c_str());
        else
          wcscpy_s(wbf, formatStatus(r->tempStatus, true).c_str() );
      }
      break;

    case lRunnerCardVoltage:
      if (r && r->getCard()) {
        wcscpy_s(wbf, r->getCard()->getCardVoltage().c_str());
      }
      break;

    case lRunnerStageNumber:
      if (pp.legIndex >= 0)
        swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%d", pp.legIndex + 1);
      break;

    case lRunnerStageTimeStatus:
      if (invalidClass)
        wsptr = &lang.tl("Struken");
      else if (r) {
        wstring tmp;
        int time, d;
        RunnerStatus st = r->getStageResult(pp.legIndex, time, d, d);
        if (showResultTime(st, time) && !noTimingRunner())
          wcscpy_s(wbf, formatTime(time).c_str());
        else
          wcscpy_s(wbf, formatStatus(st, true).c_str());
      }
      break;

    case lRunnerStageTime:
      if (invalidClass)
        wsptr = &lang.tl("Struken");
      else if (r) {
        wstring tmp;
        int time, d;
        RunnerStatus st = r->getStageResult(pp.legIndex, time, d, d);
        if (time > 0 && !noTimingRunner())
          wcscpy_s(wbf, formatTime(time).c_str());
      }
      break;

    case lRunnerStageStatus:
      if (invalidClass)
        wsptr = &lang.tl("Struken");
      else if (r) {
        wstring tmp;
        int time, d;
        RunnerStatus st = r->getStageResult(pp.legIndex, time, d, d);
        wcscpy_s(wbf, formatStatus(st, true).c_str());
      }
      break;

    case lRunnerStagePoints:
      if (!invalidClass && r) {
        int points, d;
        RunnerStatus st = r->getStageResult(pp.legIndex, d, points, d);
        wsptr = &formatScore(points);
      }
      break;

    case lRunnerPlace:
      if (r && !invalidClass && !noTimingRunner()) {
        if (pp.resultModuleIndex == -1)
          wcscpy_s(wbf, r->getPrintPlaceS(pp.text.empty()).c_str() );
        else
          wsptr = &r->getTempResult(pp.resultModuleIndex).getPrintPlaceS(pp.text.empty());
      }
      break;
    case lRunnerTotalPlace:
      if (r && !invalidClass && !noTimingRunner())
        wcscpy_s(wbf, r->getPrintTotalPlaceS(pp.text.empty()).c_str() );
      break;
    case lRunnerStagePlace:
      if (r && !invalidClass && !noTimingRunner()) {
        int d, place;
        wstring tmp;
        r->getStageResult(pp.legIndex, d, d, place);
        if (place > 0) {
          tmp = itow(place);
          if (pp.text.empty())
            tmp += L".";
        }
        wcscpy_s(wbf, tmp.c_str());
      }
      break;
    case lRunnerGeneralPlace:
      if (r && !invalidClass && pc && !noTimingRunner()) {
        if (pp.resultModuleIndex == -1) {
          if (r->hasInputData() || (r->getLegNumber() > 0 && !r->isPatrolMember())) {
            wstring iPlace;
            if (pc->getClassType() != oClassPatrol)
              iPlace = r->getPrintPlaceS(true);

            wstring tPlace = r->getPrintTotalPlaceS(true); 
            wstring v;
            if (iPlace.empty())
              v = tPlace;
            else {
              if (tPlace.empty())
                tPlace = makeDash(L"-");
              v = tPlace + L" (" + iPlace + L")";
            }
            wcscpy_s(wbf, v.c_str());
          }
          else
            wcscpy_s(wbf, r->getPrintPlaceS(pp.text.empty()).c_str() );
        }
        else
          wsptr = &r->getTempResult(pp.resultModuleIndex).getPrintPlaceS(pp.text.empty());
      }
      break;

    case lRunnerClassCoursePlace:
      if (r && !invalidClass && !noTimingRunner()) {
        int p = r->getCoursePlace(true);
        if (p>0 && p<10000)
          swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%d.", p);
      }
      break;

    case lRunnerCoursePlace:
      if (r && !invalidClass && !noTimingRunner()) {
        int p = r->getCoursePlace(false);
        if (p>0 && p<10000)
          swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%d.", p);
      }
      break;
    case lRunnerPlaceDiff:
      if (r && !invalidClass && !noTimingRunner()) {
        int p = r->getTotalPlace();
        if (r->getTotalStatus() == StatusOK && p > 0 && r->inputPlace>0) {
          int pd = p - r->inputPlace;
          if (pd > 0)
            swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"+%d", pd);
          else if (pd < 0)
            swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"-%d", -pd);
        }
      }
      break;
    case lRunnerTimeAfterDiff:
      if (r && pc && !invalidClass) {
        int tleg = r->tLeg >= 0 ? r->tLeg:0;
        if (r->getTotalStatus() == StatusOK && pc && !noTimingRunner()) {
          int after = r->getTotalRunningTime() - pc->getTotalLegLeaderTime(oClass::AllowRecompute::Yes, tleg, true, true);
          int afterOld = r->inputTime - pc->getBestInputTime(oClass::AllowRecompute::Yes, tleg);
          int ad = after - afterOld;
          if (ad > 0)
            swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"+%s", formatTimeMS(ad, false, mode).c_str());
          if (ad < 0)
            wsptr = &formatTimeMS(ad, false, mode);
        }
      }
      break;
    case lRunnerRogainingPoint:
      if (r && !invalidClass) {
        if (pp.resultModuleIndex == -1) 
          wsptr = &oe->formatScore(r->getRogainingPoints(true, false));
        else
          wsptr = &oe->formatScore(r->getTempResult(pp.resultModuleIndex).getPoints());
      }
      break;

    case lRunnerRogainingPointTotal:
      if (r && !invalidClass) {
        if (pp.resultModuleIndex == -1) 
          wsptr = &oe->formatScore(r->getRogainingPoints(true, true));
        else
          wsptr = &oe->formatScore(r->getTempResult(pp.resultModuleIndex).getPoints() + r->getInputPoints());
      }
      break;

    case lRunnerRogainingPointReduction:
      if (r && !invalidClass) {
        int red = r->getRogainingReduction(true);
        if (red > 0)
          wsptr = &oe->formatScore(-red);
      }
      break;
    
    case lRunnerRogainingPointGross:
      if (r && !invalidClass) {
        int p = r->getRogainingPointsGross(true);
        wsptr = &oe->formatScore(p);
      }
      break;
    
    case lRunnerPointAdjustment:
      if (r && !invalidClass) {
        int a = r->getPointAdjustment();
        if (a<0)
          wsptr = &oe->formatScore(a);
        else if (a>0)
          swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"+%s", oe->formatScore(a).c_str());
      }
      break;
    case lRunnerTimeAdjustment:
      if (r && !invalidClass) {
        int a = r->getTimeAdjustment(true);
        if (a != 0)
          wsptr = &formatTimeMS(a, false);
      }
      break;
    case lRunnerRogainingPointOvertime:
      if (r && !invalidClass) {
        int over = r->getRogainingOvertime(true);
        if (over > 0)
          wsptr = &formatTime(over);
      }
      break;

    case lRunnerTimeAfter:
      if (r && pc && !invalidClass && !noTimingRunner()) {
        int after = 0;
        if (pp.resultModuleIndex == -1) {
          int tleg=r->tLeg>=0 ? r->tLeg:0;
          int brt = pc->getBestLegTime(oClass::AllowRecompute::Yes, tleg, true);
          if (r->prelStatusOK(true, true, true) && brt > 0) {
            after=r->getRunningTime(true) - brt;
          }
        }
        else {
          after = r->getTempResult(pp.resultModuleIndex).getTimeAfter();
        }
  
        if (r->getNumShortening() == 0) {
          if (after > 0)
            swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"+%s", formatTimeMS(after, false, mode).c_str());
        }
        else {
          wsptr = &makeDash(L"-");
        }
      }
      break;
    case lRunnerTotalTimeAfter:
      if (r && pc && !invalidClass) {
        int tleg = r->tLeg >= 0 ? r->tLeg:0;
        if (r->getTotalStatus()==StatusOK &&  pc && !noTimingRunner()) {
          if ( (t && t->getNumShortening(tleg) == 0) || (!t && r->getNumShortening() == 0)) { 
            int after = r->getTotalRunningTime() - pc->getTotalLegLeaderTime(oClass::AllowRecompute::Yes, tleg, true, true);
            if (after > 0)
              swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"+%s", formatTimeMS(after, false, mode).c_str());
          }
          else {
            wsptr = &makeDash(L"-");
          }
        }
      }
      break;
    case lRunnerClassCourseTimeAfter:
    case lRunnerCourseTimeAfter:
      if (r && pc && !invalidClass) {
        if (r->isStatusOK(true, true) && !noTimingRunner()) {
          int after = r->getTimeAfterCourse(type == lRunnerClassCourseTimeAfter);
          if (after > 0)
            swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"+%s", formatTimeMS(after, false, mode).c_str());
        }
      }
      break;
    case lRunnerTimePlaceFixed:
      if (r && !invalidClass) {
        int t = r->getTimeWhenPlaceFixed();
        if (t == 0 || (t > 0 && t < getComputerTime())) {
          wcscpy_s(wbf, lang.tl("klar").c_str());
        }
        else if (t == -1)
          wcscpy_s(wbf, L"-");
        else
          wcscpy_s(wbf, oe->getAbsTime(t).c_str());
      }
      break;
    case lRunnerLegNumberAlpha:
    case lRunnerLegNumber:
      if (r)
        return formatSpecialStringAux(pp, par, t, r->getLegNumber(), 0, 0, counter);
      else
        wcscpy_s(wbf, par.getLegName().c_str());
      break;
    case lRunnerLostTime:
      if (r && r->prelStatusOK(true, true, true) && !noTimingRunner() && !invalidClass) {
        wcscpy_s(wbf, r->getMissedTimeS().c_str());
      }
      break;
    case lRunnerTempTimeAfter:
      if (r && pc) {
        if (r->tempStatus == StatusOK && pc && !noTimingRunner()
            && r->tempRT > pc->tLegLeaderTime) {
          int after = r->tempRT - pc->tLegLeaderTime;
          swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"+%s", formatTimeMS(after, false, mode).c_str());
        }
      }
      break;

    case lRunnerCard:
      if (r && r->getCardNo() > 0)
        swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%d", r->getCardNo());
      break;
    case lRunnerRank:
      if (r) {
        int rank = r->getRanking(); 
        if (rank>0 && rank < MaxOrderRank)
          wcscpy_s(wbf, formatRank(rank).c_str());
      }
      break;
    case lRunnerRankScore:
      if (r) {
        wcscpy_s(wbf, r->getRankingScore().c_str());
      }
      break;
    case lRunnerBib:
      if (r) {
        const wstring &bib = r->getBib();
        if (!bib.empty())
          wsptr = &bib;
      }
      break;
    case lRunnerRentalCard:
      if (r && r->isRentalCard()) {
        wsptr = &lang.tl("Hyrd");
      }
      break;

    case lRunnerUMMasterPoint:
      if (r) {
        int total, finished, dns;
        pc->getNumResults(par.getLegNumber(pc), total, finished, dns);
        int percent = int(floor(0.5+double((100*(total-dns-r->getPlace()))/double(total-dns))));
        if (r->getStatus()==StatusOK)
          swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%d",  percent);
        else
          swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"0");
      }
      break;
    case lRunnerAge:
      if (r) {
        int y = r->getBirthAge();
        if (y > 0)
          swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%d", y);
      }
    break;
    case lRunnerBirthYear:
      if (r) {
        int y = r->getBirthYear();
        if (y > 0)
          swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%d", y);
      }
    break;
    case lRunnerBirthDate:
      if (r) {
        wsptr = &r->getBirthDate();
      }
      break;
    case lRunnerSex:
      if (r) {
        PersonSex s = r->getSex();
        if (s == sFemale)
          wsptr = &lang.tl("Kvinna");
        else if (s == sMale)
          wsptr = &lang.tl("Man");
      }
    break;
    case lRunnerPhone:
      if (r) {
        wsptr = &r->getDCI().getString("Phone");
      }
    break;
    case lRunnerFee:
      if (r) {
        wstring s = formatCurrency(r->getDCI().getInt("Fee"));
        wcscpy_s(wbf, s.c_str());
      }
    break;
    case lRunnerExpectedFee:
      if (r) {
        wstring s = formatCurrency(r->getDefaultFee());
        wcscpy_s(wbf, s.c_str());
      }
      break;
    case lRunnerPaid:
      if (r) {
        wstring s = formatCurrency(r->getDCI().getInt("Paid"));
        wcscpy_s(wbf, s.c_str());
      }
      break;
    case lRunnerId:
      if (r) {
        wstring s = r->getExtIdentifierString();
        wcscpy_s(wbf, s.c_str());
      }
      break;
    case lRunnerPayMethod:
      if (r) {
        wsptr = &r->getDCI().formatString(r, "PayMode");
      }
      break;
    case lRunnerEntryDate:
      if (r && r->getDCI().getInt("EntryDate") > 0) {
        wsptr = &r->getDCI().getDate("EntryDate");
      }
      break;
    case lRunnerEntryTime:
      if (r) {
        wsptr = &formatTime(r->getDCI().getInt("EntryTime"));
      }
      break;
    case lTeamFee:
      if (t) {
        wstring s = formatCurrency(t->getTeamFee());
        wcscpy_s(wbf, s.c_str());
      }
    break;
    case lRunnerNationality:
      if (r) {
        wstring nat = r->getNationality();
        wcscpy_s(wbf, nat.c_str());
      }
    break;
    case lTeamName:
      if (t) 
        wcscpy_s(wbf, t->getDisplayName().c_str());
      break;
    case lTeamNameRaw:
      if (t)
        wsptr = &t->getName();
      break;
    case lTeamStart:
    case lTeamStartCond:
    case lTeamStartZero:
      if (t) {
        if ((type == lTeamStartCond || type == lTeamStartZero) && pc && !pc->hasFreeStart()) {
          int fs, ls;
          pc->getStartRange(legIndex, fs, ls);
          if (fs>0 && fs == ls) {
            break; // Common start time, skip
          }
        }

        if (unsigned(legIndex)<t->Runners.size() && t->Runners[legIndex] && t->Runners[legIndex]->startTimeAvailable()) {
          if (type != lTeamStartZero) 
            wsptr = &t->Runners[legIndex]->getStartTimeCompact();
          else {
            int st = t->Runners[legIndex]->getStartTime();
            wsptr = &formatTimeMS(st-timeConstHour, true);
          }
        }
      }
      break;
    case lTeamStatus:
      if (t && !invalidClass) {
        if (pp.resultModuleIndex == -1)
          wsptr = &t->getLegStatusS(legIndex, true, false);
        else
          wsptr = &t->getTempResult(pp.resultModuleIndex).getStatusS(StatusOK);
      }
      break;
    case lTeamTime:
      if (t && !invalidClass) {
        if (pp.resultModuleIndex == -1)
          wcscpy_s(wbf, t->getLegRunningTimeS(legIndex, true, false, mode).c_str() );
        else
          wsptr = &t->getTempResult(pp.resultModuleIndex).getRunningTimeS(0, mode);
      }
      break;
    case lTeamGrossTime:
      if (t && !invalidClass) {
        int tm = t->getLegRunningTimeUnadjusted(legIndex, false, false);
        wsptr = &formatTime(tm);
      }
      break;
    case lTeamTimeStatus:
      if (invalidClass)
          wsptr = &lang.tl("Struken");
      else if (t) {
        if (pp.resultModuleIndex == -1) {
          RunnerStatus st = t->getLegStatus(legIndex, true, false);
          if (st == StatusOK || ((st == StatusUnknown || st == StatusOutOfCompetition) && t->getLegRunningTime(legIndex, true, false) > 0)) {
            if (st != StatusOutOfCompetition)
              wcscpy_s(wbf, t->getLegRunningTimeS(legIndex, true, false, mode).c_str());
            else 
              swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"(%s)", t->getLegRunningTimeS(legIndex, true, false, mode).c_str());
          }
          else
            wsptr = &t->getLegStatusS(legIndex, true, false);
        }
        else {
          RunnerStatus st = t->getTempResult(pp.resultModuleIndex).getStatus();
          if (st == StatusOK || st == StatusUnknown) {
            wsptr = &t->getTempResult(pp.resultModuleIndex).getRunningTimeS(0, mode);
            if (t->getNumShortening() > 0) {
              swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"*%s", wsptr->c_str());
              wsptr = 0;
            }
          }
          else
            wsptr = &t->getTempResult(pp.resultModuleIndex).getStatusS(StatusOK);
        }
      }
      break;
    case lTeamRogainingPoint:
      if (t && !invalidClass) {
        if (pp.resultModuleIndex == -1) 
          wsptr = &oe->formatScore(t->getRogainingPoints(true, false));
        else
          wsptr = &oe->formatScore(t->getTempResult(pp.resultModuleIndex).getPoints());
      }
      break;
    case lTeamRogainingPointTotal:
      if (t && !invalidClass) {
        if (pp.resultModuleIndex == -1) 
          wsptr = &oe->formatScore(t->getRogainingPoints(true, true));
        else
          wsptr = &oe->formatScore(t->getTempResult(pp.resultModuleIndex).getPoints() + t->getInputPoints());
      }
      break;

    case lTeamRogainingPointReduction:
      if (t && !invalidClass) {
        int red = t->getRogainingReduction(true);
        if (red > 0)
          wsptr = &oe->formatScore(-red);
      }
      break;

    case lTeamRogainingPointOvertime:
      if (t && !invalidClass) {
        int over = t->getRogainingOvertime(true);
        if (over > 0)
          wsptr = &formatTime(over);
      }
      break;

   case lTeamPointAdjustment:
      if (t && !invalidClass) {
        int a = t->getPointAdjustment();
        if (a<0)
          wsptr  = &oe->formatScore(a);
        else if (a>0)
          wprintf_s(wbf, "+%s", oe->formatScore(a).c_str());
      }
      break;

    case lTeamTimeAdjustment:
      if (t && !invalidClass) {
        int a = t->getTimeAdjustment(true);
        if (a != 0)
          wsptr = &formatTimeMS(a, false);
      }
      break;

    case lTeamTimeAfter:
      if (t && !invalidClass) {
        if (pp.resultModuleIndex == -1) {
          if (t->getLegStatus(legIndex, true, false)==StatusOK) {
            if (t->getNumShortening(legIndex) == 0) {
              int ta=t->getTimeAfter(legIndex, true);
              if (ta>0)
                swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"+%s", formatTimeMS(ta, false, mode).c_str());
            }
            else {
              wsptr = &makeDash(L"-");
            }
          }
        }
        else {
          if (t->getTempResult().getStatus() == StatusOK) {
            int after = t->getTempResult(pp.resultModuleIndex).getTimeAfter();
            if (after > 0)
              swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"+%s", formatTimeMS(after, false, mode).c_str());
          }
        }
      }
      break;
    case lTeamPlace:
      if (t && !invalidClass && !noTimingTeam() && pc && pc->isValidLeg(legIndex)) {
        if (pp.resultModuleIndex == -1) {
          wcscpy_s(wbf, t->getLegPrintPlaceS(legIndex, false, pp.text.empty()).c_str());
        }
        else
          wsptr = &t->getTempResult(pp.resultModuleIndex).getPrintPlaceS(pp.text.empty());
      }
      break;

    case lTeamLegTimeStatus:
      if (invalidClass)
        wsptr = &lang.tl("Struken");
      else if (t) {
        /*int ix = r ? r->getLegNumber() : counter.level3;
        if (pc)
          ix = pc->getResultDefining(ix);*/
        int ix = legIndex;
        if (pc) {
          if (!pc->isValidLeg(legIndex))
            break;
          ix = pc->getResultDefining(ix);
        }
        RunnerStatus st = t->getLegStatus(ix, true, false);
        if (st == StatusOK)
          wcscpy_s(wbf, t->getLegRunningTimeS(ix, true, false, mode).c_str() );
        else if (st == StatusOutOfCompetition && t->getLegRunningTime(ix, true, false) > 0)
          swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"(%s)", t->getLegRunningTimeS(ix, true, false, mode).c_str());
        else
          wcscpy_s(wbf, t->getLegStatusS(ix, true, false).c_str() );
      }
      break;
    case lTeamLegTimeAfter:
      if (t) {
        /*int ix = r ? r->getLegNumber() : counter.level3; xxx
        if (pc)
          ix = pc->getResultDefining(ix);
        */
        int ix = legIndex;
        if (pc) {
          if (!pc->isValidLeg(legIndex))
            break;
          ix = pc->getResultDefining(ix);
        }
        if (t->getLegStatus(ix, true, false)==StatusOK && !invalidClass) {
          if (t->getNumShortening(ix) == 0) {
            int ta=t->getTimeAfter(ix, true);
            if (ta>0)
              swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"+%s", formatTimeMS(ta, false, mode).c_str());
          }
          else {
            wsptr = &makeDash(L"-");
          }
        }
      }
      break;
    case lTeamClub:
      if (t) {
        wcscpy_s(wbf, t->getDisplayClub().c_str());
      }
      break;
    case lTeamClubShort:
      if (t && t->getClubRef()) {
        wsptr = &t->getClubRef()->getCompactName();
      }
      break;
    case lTeamRunner:
      if (t && unsigned(legIndex)<t->Runners.size() && t->Runners[legIndex])
        wsptr=&t->Runners[legIndex]->getName();
      break;
    case lTeamRunnerCard:
      if (t && unsigned(legIndex)<t->Runners.size() && t->Runners[legIndex]
      && t->Runners[legIndex]->getCardNo()>0)
        swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%d", t->Runners[legIndex]->getCardNo());
      break;
    case lTeamBib:
      if (t) {
        wsptr = &t->getBib();
      }
      break;
    case lTeamTotalTime:
      if (t && !invalidClass) wcscpy_s(wbf, t->getLegRunningTimeS(legIndex, true, true, mode).c_str() );
      break;
    case lTeamTotalTimeStatus:
      if (invalidClass)
          wsptr = &lang.tl("Struken");
      else if (t) {
        if (pp.resultModuleIndex == -1) {
          if (t->getLegStatus(legIndex, true, true)==StatusOK)
            wcscpy_s(wbf, t->getLegRunningTimeS(legIndex, true, true, mode).c_str() );
          else
            wcscpy_s(wbf, t->getLegStatusS(legIndex, true, true).c_str() );
        }
        else {
          RunnerStatus st = t->getTempResult(pp.resultModuleIndex).getStatus();
          RunnerStatus inp = t->getInputStatus();
          if (st == StatusOK && inp == StatusOK) {
            wsptr = &t->getTempResult(pp.resultModuleIndex).getRunningTimeS(0, mode);
            if (t->getNumShortening() > 0) {
              swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"*%s", wsptr->c_str());
              wsptr = 0;
            }
          }
          else
            wsptr = &t->getTempResult(pp.resultModuleIndex).getStatusS(StatusOK);
        }
      }
      break;
    case lTeamPlaceDiff:
      if (t && !invalidClass) {
        int p = t->getTotalPlace();
        if (t->getTotalStatus() == StatusOK && p > 0 && t->inputPlace>0) {
          int pd = p - t->inputPlace;
          if (pd > 0)
            swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"+%d", pd);
          else if (pd < 0)
            swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"-%d", -pd);
        }
      }
      break;
    case lTeamTotalPlace:
      if (t && !invalidClass && !noTimingTeam()) wcscpy_s(wbf, t->getPrintTotalPlaceS(pp.text.empty()).c_str() );
      break;
    case lTeamTotalTimeAfter:
      if (t && pc && !invalidClass) {
        int tleg = t->getNumRunners() - 1;
        if (t->getTotalStatus()==StatusOK &&  pc && !noTimingTeam()) {
          int after = t->getTotalRunningTime() - pc->getTotalLegLeaderTime(oClass::AllowRecompute::Yes, tleg, true, true);
          if (after > 0)
            swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"+%s", formatTimeMS(after, false, mode).c_str());
        }
      }
      break;
    case lTeamTotalTimeDiff:
      if (t && pc && !invalidClass) {
        int tleg = t->getNumRunners() - 1;
        if (t->getTotalStatus()==StatusOK &&  pc && !noTimingTeam()) {
          int after = t->getTotalRunningTime() - pc->getTotalLegLeaderTime(oClass::AllowRecompute::Yes, tleg, true, true);
          int afterOld = t->inputTime - pc->getBestInputTime(oClass::AllowRecompute::Yes, tleg);
          int ad = after - afterOld;
          if (ad > 0)
            swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"+%s", formatTimeMS(ad, false, mode).c_str()); 
          if (ad < 0)
            wsptr = &formatTimeMS(ad, false, mode);
        }
      }
      break;
    case lTeamStartNo:
      if (t)
        swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%d", t->getStartNo());
      break;
    case lRunnerStartNo:
      if (r)
        swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%d", r->getStartNo());
      break;
    case lRunnerDataA:
      if (r)
        wsptr = &itow(r->getDCI().getInt("DataA"));
      break;
    case lRunnerDataB:
      if (r)
        wsptr = &itow(r->getDCI().getInt("DataB"));
      break;
    case lRunnerTextA:
      if (r)
        wsptr = &r->getDCI().getString("TextA");
      break;
    case lRunnerAnnotation:
      if (r) {
        wsptr = &r->getDCI().getString("Annotation");
        if (!wsptr->empty()) {
          wsptr = formatAnnotation(*wsptr, wbf, legIndex + 1);
        }
      }
      break;

    case lTeamDataA:
      if (t)
        wsptr = &itow(t->getDCI().getInt("DataA"));
      break;
    case lTeamDataB:
      if (t)
        wsptr = &itow(t->getDCI().getInt("DataB"));
      break;
    case lTeamTextA:
      if (t)
        wsptr = &t->getDCI().getString("TextA");
      break;
    case lTeamAnnotation:
      if (t) {
        wsptr = &t->getDCI().getString("Annotation");
        if (!wsptr->empty()) {
          wsptr = formatAnnotation(*wsptr, wbf, legIndex + 1);
        }
      }
      break;

    case lNationality:
      if (r && !(wsptr = &r->getDCI().getString("Nationality"))->empty())
        break;
      else if (t && !(wsptr = &t->getDCI().getString("Nationality"))->empty())
        break;
      else if (c && !(wsptr = &c->getDCI().getString("Nationality"))->empty())
        break;

      break;

    case lCountry:
      if (r && !(wsptr = &r->getDCI().getString("Country"))->empty())
        break;
      else if (t && !(wsptr = &t->getDCI().getString("Country"))->empty())
        break;
      else if (c && !(wsptr = &c->getDCI().getString("Country"))->empty())
        break;

      break;
    case lControlName:
    case lPunchName:
    case lPunchNamedTime:
    case lPunchTeamTotalNamedTime:
    case lPunchNamedSplit:
    case lPunchTime:
    case lPunchTeamTime:
    case lPunchSplitTime:
    case lPunchTotalTime:
    case lPunchTeamTotalTime:
    case lPunchControlNumber:
    case lPunchControlCode:
    case lPunchLostTime:
    case lPunchControlPlace:
    case lPunchControlPlaceAcc:
    case lPunchControlPlaceTeamAcc:
    case lPunchAbsTime:
    case lPunchTotalTimeAfter:
    case lPunchTeamTotalTimeAfter:
      if (r && r->getCourse(false) && !invalidClass) {
        const pCourse crs=r->getCourse(true);
        const oControl *ctrl = nullptr;
        int nCtrl = crs->getNumControls();
        if (counter.level3 != nCtrl) { // Always allow finish
          ctrl=crs->getControl(counter.level3);
          if (!ctrl || ctrl->isRogaining(crs->hasRogaining()))
            break;
        }
        switch (type) {
          case lPunchNamedSplit:
            if (ctrl && ctrl->hasName() && r->getPunchTime(counter.level3, false, true, false) > 0) {
              swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%s", r->getNamedSplitS(counter.level3, SubSecond::Off).c_str());
            }
          break;

          case lPunchNamedTime:
          case lPunchTeamTotalNamedTime:
            if (ctrl && ctrl->hasName() && (!par.lineBreakControlList || r->getPunchTime(counter.level3, false, true, false) > 0)) {
              swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%s: %s (%s)", ctrl->getName().c_str(),
                         r->getPunchTimeS(counter.level3, false, true, type == lPunchTeamTotalNamedTime, SubSecond::Off).c_str(),
                         r->getNamedSplitS(counter.level3, SubSecond::Off).c_str());
            }
            break;
          case lControlName:
          case lPunchName:
            if (ctrl && ctrl->hasName() && (!par.lineBreakControlList || r->getPunchTime(counter.level3, false, true, false) > 0)) {
              swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%s", ctrl->getName().c_str());
            }
            else if (counter.level3 == nCtrl) {
              wsptr = &lang.tl(L"Mål");
            }
            break;

          case lPunchTime:
          case lPunchTeamTime: {
            swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%s (%s)",
                       r->getSplitTimeS(counter.level3, false, SubSecond::Off).c_str(),
                       r->getPunchTimeS(counter.level3, false, true, type == lPunchTeamTime, SubSecond::Off).c_str());
            break;
          }
          case lPunchSplitTime: {
            wcscpy_s(wbf, r->getSplitTimeS(counter.level3, false, SubSecond::Off).c_str());
            break;
          }
          case lPunchTotalTime:
          case lPunchTeamTotalTime: {
            int pt = r->getPunchTime(counter.level3, false, true, type == lPunchTeamTotalTime);
            if (pt > 0) 
              wsptr = &formatTime(pt, SubSecond::Off);
            break;
          }
          case lPunchTotalTimeAfter:
          case lPunchTeamTotalTimeAfter: {
            if (r->getPunchTime(counter.level3, false, true, false) > 0) {
              int rt = r->getLegTimeAfterAcc(counter.level3, type == lPunchTeamTotalTimeAfter);
              if (rt > 0)
                wcscpy_s(wbf, (L"+" + formatTime(rt, SubSecond::Off)).c_str());
            }
            break;
          }
          case lPunchControlNumber: {
            wcscpy_s(wbf, crs->getControlOrdinal(counter.level3).c_str());
            break;
          }
          case lPunchControlCode: {
            const oControl *ctrl = crs->getControl(counter.level3);
            if (ctrl) {
              if (ctrl->getStatus() == oControl::ControlStatus::StatusMultiple) {
                wstring str = ctrl->getStatusS();
                swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%s.", str.substr(0, 1).c_str());
              }
              else
                swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%d", ctrl->getFirstNumber());
            }
            break;
          }
          case lPunchControlPlace: {
            int p = r->getLegPlace(counter.level3);
            if (p > 0)
              swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%d", p);
            break;
          }
          case lPunchControlPlaceAcc:
          case lPunchControlPlaceTeamAcc: {
            int p = r->getLegPlaceAcc(counter.level3, type == lPunchControlPlaceTeamAcc);
            if (p > 0)
              swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%d", p);
            break;
          }
          case lPunchLostTime: {
            wcscpy_s(wbf, r->getMissedTimeS(counter.level3).c_str());
            break;
          }
          case lPunchAbsTime: {
            int t = r->getPunchTime(counter.level3, false, true, false);
            if (t > 0)
              wsptr = &getAbsTime(r->tStartTime + t);
            break;
          }
        }
      }
      break;
    case lSubSubCounter:
      if (pp.text.empty())
        swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%d.", counter.level3+1);
      else
        swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%d", counter.level3+1);
      break;
    case lSubCounter:
      if (pp.text.empty())
        swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%d.", counter.level2+1);
      else
        swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%d", counter.level2+1);
      break;
    case lTotalCounter:
      if (pp.text.empty())
        swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%d.", counter.level1+1);
      else
        swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%d", counter.level1+1);
      break;

    case lTotalRunLength: {
      int sum = 0;
      for (auto& r : Runners) {
        if (!r.isRemoved() && r.getCourse(false) && r.isStatusOK(false, false))
          sum += r.getCourse(false)->getLength();
      }
      wsptr = &itow(sum/1000);
      break;
    }
    case lTotalRunTime: {
      int sum = 0;
      for (auto& r : Runners) {
        if (!r.isRemoved())
          sum += r.getRunningTime(false);
      }
      wsptr = &formatTimeHMS(sum);
      break;
    }
    case lNumEntries: {
      int sum = 0;
      for (auto& r : Runners) {
        if (!r.isRemoved() && r.tParentRunner == nullptr)
          sum++;
      }
      wsptr = &itow(sum);
      break;
    }
    case lNumStarts: {
      int sum = 0;
      for (auto& r : Runners) {
        if (!r.isRemoved() && r.tParentRunner == nullptr && (oAbstractRunner::isResultStatus(r.getStatus()) || r.isStatusOK(false, false)))
          sum++;
      }
      wsptr = &itow(sum);
      break;
    }
    case lClubName:
      wsptr = c != nullptr ? &c->getDisplayName() : nullptr;
      break;

    case lClubNameShort:
      wsptr = c != nullptr ? &c->getCompactName() : nullptr;
      break;

    case lResultModuleTime:
    case lResultModuleTimeTeam:

      if (pp.resultModuleIndex != -1) {
        if (t && type == lResultModuleTimeTeam)
          wsptr = &t->getTempResult(pp.resultModuleIndex).getOutputTime(legIndex);
        else if (r)
          wsptr = &r->getTempResult(pp.resultModuleIndex).getOutputTime(legIndex);
      }
      break;

    case lResultModuleNumber:
    case lResultModuleNumberTeam:
    
      if (pp.resultModuleIndex != -1) {
        int nr = 0;
        if (t && type == lResultModuleNumberTeam)
          nr = t->getTempResult(pp.resultModuleIndex).getOutputNumber(legIndex);
        else if (r)
          nr = r->getTempResult(pp.resultModuleIndex).getOutputNumber(legIndex);

        if (pp.text.empty() || pp.text[0] != '@') {
          if (!pp.text.empty() && pp.text[0] == '&') {
            wsptr = &itow(nr);
            textOffset = 1;
          }
          else
            wsptr = &oe->formatScore(nr);
        }
        else {
          wstring &res = StringCache::getInstance().wget();
          MetaList::fromResultModuleNumber(pp.text.substr(1), nr, res);
          return res;
        }
      }
      break;

    case lRogainingPunch:
      if (r && r->Card && r->getCourse(false)) {
        const pCourse crs = r->getCourse(false);
        const pPunch punch = r->Card->getPunchByIndex(counter.level3);
        if (punch && punch->tRogainingIndex>=0) {
          const pControl ctrl = crs->getControl(punch->tRogainingIndex);
          if (ctrl) {
            swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), L"%d, %sp, %s (%s)",
                      punch->type, ctrl->getRogainingPointsS().c_str(),
                      r->Card->getRogainingSplit(counter.level3, r->tStartTime).c_str(),
                      punch->getRunningTime(r->tStartTime).c_str());
          }
        }
      }
      break;

    case lLineBreak:
      break;
  }

  if (type != lString && (wsptr == 0 || wsptr->empty()) && wbf[0] == 0)
    return _EmptyWString;
  else if (wsptr) {
    if (pp.text.empty())
      return *wsptr;
    else {
      swprintf(wbf, sizeof(wbf)/sizeof(wchar_t), pp.text.c_str() + textOffset, wsptr->c_str());
      wstring &res = StringCache::getInstance().wget();
      res = wbf;
      return res;
    }
  }
  else {
    if (pp.text.empty()) {
      wstring &res = StringCache::getInstance().wget();
      res = wbf;
      return res;
    }
    else {
      wchar_t bf2[512];
      swprintf(bf2, sizeof(bf2)/sizeof(wchar_t), pp.text.c_str() + textOffset, wbf);
      wstring &res = StringCache::getInstance().wget();
      res = bf2;
      return res;
    }
  }
}



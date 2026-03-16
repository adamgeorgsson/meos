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

// oListInfoUtil.cpp: oPrintPost::encodeFont, oListInfo::setupLinks, shrinkSize,
// oListParam leg helpers, SplitPrintListInfo split from oListInfo.cpp
//
//////////////////////////////////////////////////////////////////////

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

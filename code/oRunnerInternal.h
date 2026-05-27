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

// oRunnerInternal.h: file-local helpers shared between oRunnerData.cpp and oRunnerResult.cpp
#pragma once
#include "oRunner.h"
#include "gdioutput.h"

static inline int findNextControl(const vector<pControl> &ctrl, int startIndex, int id, int &offset, bool supportRogaining)
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

static inline void gotoNextLine(gdioutput &gdi, int &xcol, int &cx, int &cy, int colDeltaX, int numCol, int baseCX) {
  if (++xcol < numCol) {
    cx += colDeltaX;
  }
  else {
    xcol = 0;
    cy += int(gdi.getLineHeight()*1.1);
    cx = baseCX;
  }
}

static inline void addMissingControl(bool wideFormat, gdioutput &gdi,
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

  if (wideFormat) {
    gotoNextLine(gdi, xcol, cx, cy, colDeltaX, numCol, baseCX);
  }
  else
    cy+=int(gdi.getLineHeight()*0.3);
}

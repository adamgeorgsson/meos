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


// gdioutput.cpp: implementation of the gdioutput class.
//
//////////////////////////////////////////////////////////////////////
#define _USE_MATH_DEFINES

#include "StdAfx.h"
#include "gdioutput.h"
#include "gdiconstants.h"
#include "meosexception.h"

#include "process.h"

#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <objbase.h>
#include <shlobj.h>
#include <cassert>

#include <cmath>

#include <sstream>

#include "meos_util.h"
#include "Table.h"

#include "localizer.h"

#include "TabBase.h"
#include "toolbar.h"
#include "gdiimpl.h"
#include "Printer.h"
#include "recorder.h"
#include "animationdata.h"
#include "image.h"
#include "autocomplete.h"
#include "maprenderer.h"

extern Image image;

//#define DEBUGRENDER


bool gdioutput::removeWidget(const string &id)
{
  {
    auto it = BI.begin();
    int cnt = 0;
    while (it != BI.end()) {
      if (it->id == id) {
        DestroyWindow(it->hWnd);
        biByHwnd.erase(it->hWnd);

        if (it->isCheckbox)
          removeString("T" + id);
        BI.erase(it);
        // Update restorepoints
        for (auto& rp : restorePoints) {
          if (rp.second.nBI > cnt)
            rp.second.nBI--;
        }
        return true;
      }
      ++it;
      ++cnt;
    }
  }
  {
    auto lit = LBI.begin();
    int cnt = 0;
    while (lit != LBI.end()) {
      if (lit->id == id) {
        DestroyWindow(lit->hWnd);
        lbiByHwnd.erase(lit->hWnd);
        removeString(id + "_label");
        if (lit->writeLock)
          hasCleared = true;
        LBI.erase(lit);
        // Update restorepoints
        for (auto& rp : restorePoints) {
          if (rp.second.nLBI > cnt)
            rp.second.nLBI--;
        }
        return true;
      }
      ++lit;
      cnt++;
    }
  }

  {
    auto iit = II.begin();
    int cnt = 0;
    while (iit != II.end()) {
      if (iit->id == id) {
        DestroyWindow(iit->hWnd);
        iiByHwnd.erase(iit->hWnd);
        II.erase(iit);
        removeString(id + "_label");
        // Update restorepoints
        for (auto& rp : restorePoints) {
          if (rp.second.nII > cnt)
            rp.second.nII--;
        }
        return true;
      }
      ++iit;
      cnt++;
    }
  }
  removeString(id);
  return false;
}

bool gdioutput::hideWidget(const string &id, bool hide) {
  list<ButtonInfo>::iterator it=BI.begin();

  while (it!=BI.end()) {
    if (it->id == id) {
      ShowWindow(it->hWnd, hide ? SW_HIDE : SW_SHOW);
      if (it->isCheckbox) {
        hideWidget("T" + id, hide);
      }
      return true;
    }
    ++it;
  }

  list<ListBoxInfo>::iterator lit=LBI.begin();

  while (lit!=LBI.end()) {
    if (lit->id==id) {
      ShowWindow(lit->hWnd, hide ? SW_HIDE : SW_SHOW);
      return true;
    }
    ++lit;
  }

  list<InputInfo>::iterator iit=II.begin();

  while (iit!=II.end()) {
    if (iit->id==id) {
      ShowWindow(iit->hWnd, hide ? SW_HIDE : SW_SHOW);
      return true;
    }
    ++iit;
  }

  for (auto &ti : TL) {
    if (ti.id == id) {
      if (hide)
        ti.format |= hiddenText;
      else
        ti.format &= ~hiddenText;

      return true;
    }
  }
  return false;
}

void gdioutput::setRestorePoint() {
  setRestorePoint("");
}


void gdioutput::setRestorePoint(const string &id) {
  RestoreInfo ri;

  ri.id = id;

  ri.nLBI = LBI.size();
  ri.nBI = BI.size();
  ri.nII = II.size();
  ri.nTL = TL.size();
  ri.nRect = Rectangles.size();
  ri.nTooltip = toolTips.size();
  ri.nTables = Tables.size();
  ri.nHWND = FocusList.size();
  ri.nData = DataInfo.size();
  
  for (auto& rp : restorePoints)
    ri.restorePoints.insert(rp.first);

  ri.sCX=CurrentX;
  ri.sCY=CurrentY;
  ri.sMX=MaxX;
  ri.sMY=MaxY;
  ri.sOX=OffsetX;
  ri.sOY=OffsetY;

  ri.onClear = onClear;
  ri.postClear = postClear;
  restorePoints[id]=ri;
}


bool gdioutput::getWidgetRestorePoint(const string& id, string& restorePoint) const {
  int count;

  count = 0;
  for (auto& lbi : LBI) {
    if (lbi.id == id) {
      const RestoreInfo* bestRestoreInfo = nullptr;
      for (auto& rp : restorePoints) {
        if (count <= rp.second.nLBI && (bestRestoreInfo == nullptr || rp.second < *bestRestoreInfo)) {
          bestRestoreInfo = &rp.second;
          restorePoint = rp.first;
        }
      }
      return bestRestoreInfo != nullptr;
    }
    count++;
  }

  count = 0;
  for (auto& ii : II) {
    if (ii.id == id) {
      const RestoreInfo* bestRestoreInfo = nullptr;
      for (auto& rp : restorePoints) {
        if (count <= rp.second.nII && (bestRestoreInfo == nullptr || rp.second < *bestRestoreInfo)) {
          bestRestoreInfo = &rp.second;
          restorePoint = rp.first;
        }
      }
      return bestRestoreInfo != nullptr;
    }
    count++;
  }


  count = 0;
  for (auto& bi : BI) {
    if (bi.id == id) {
      const RestoreInfo* bestRestoreInfo = nullptr;
      for (auto& rp : restorePoints) {
        if (count <= rp.second.nBI && (bestRestoreInfo == nullptr || rp.second < *bestRestoreInfo)) {
          bestRestoreInfo = &rp.second;
          restorePoint = rp.first;
        }
      }
      return bestRestoreInfo != nullptr;
    }
    count++;
  }

  count = 0;
  for (auto& ti : TL) {
    if (ti.id == id) {
      const RestoreInfo* bestRestoreInfo = nullptr;
      for (auto& rp : restorePoints) {
        if (count <= rp.second.nTL && (bestRestoreInfo == nullptr || rp.second < *bestRestoreInfo)) {
          bestRestoreInfo = &rp.second;
          restorePoint = rp.first;
        }
      }
      return bestRestoreInfo != nullptr;
    }
    count++;
  }


  return false;
}

void gdioutput::setWidgetRestorePoint(const string& id, const string& restorePoint) {
  for (auto it = LBI.begin(); it != LBI.end(); ++it) {
    if (it->id == id) {
      auto& rpTarget = restorePoints[restorePoint];
      auto itNew = LBI.begin();
      int numNew = rpTarget.nLBI;
      advance(itNew, numNew);
      LBI.splice(itNew, LBI, it);
      for (auto& rp : restorePoints) {
        if (rp.second.nLBI >= numNew) {
          rp.second.nLBI++;
        }
      }
      return;
    }
  }

  for (auto it = II.begin(); it != II.end(); ++it) {
    if (it->id == id) {
      auto& rpTarget = restorePoints[restorePoint];
      auto itNew = II.begin();
      int numNew = rpTarget.nII;
      advance(itNew, numNew);
      II.splice(itNew, II, it);
      for (auto& rp : restorePoints) {
        if (rp.second.nII >= numNew) {
          rp.second.nII++;
        }
      }
      return;
    }
  }

  for (auto it = BI.begin(); it != BI.end(); ++it) {
    if (it->id == id) {
      auto& rpTarget = restorePoints[restorePoint];
      auto itNew = BI.begin();
      int numNew = rpTarget.nBI;
      advance(itNew, numNew);
      BI.splice(itNew, BI, it);
      for (auto& rp : restorePoints) {
        if (rp.second.nBI >= numNew) {
          rp.second.nBI++;
        }
      }
      return;
    }
  }

  for (auto it = TL.begin(); it != TL.end(); ++it) {
    if (it->id == id) {
      auto& rpTarget = restorePoints[restorePoint];
      auto itNew = TL.begin();
      int numNew = rpTarget.nTL;
      advance(itNew, numNew);
      TL.splice(itNew, TL, it);
      for (auto& rp : restorePoints) {
        if (rp.second.nTL >= numNew) {
          rp.second.nTL++;
        }
      }
      return;
    }
  }
}

void gdioutput::restoreInternal(const RestoreInfo &ri)
{
  int toolRemove=toolTips.size()-ri.nTooltip;
  while (toolRemove>0 && toolTips.size()>0) {
    ToolInfo &info=toolTips.back();
    if (hWndToolTip) {
      SendMessage(hWndToolTip, TTM_DELTOOL, 0, (LPARAM) &info.ti);
    }
    toolTips.pop_back();
    toolRemove--;
  }

  int lbiRemove=LBI.size()-ri.nLBI;
  while (lbiRemove>0 && LBI.size()>0) {
    ListBoxInfo &lbi=LBI.back();
    lbi.callBack = nullptr; // Avoid kill focus event here
    lbi.clearHandler();
    DestroyWindow(lbi.hWnd);
    if (lbi.writeLock)
      hasCleared = true;
    lbiByHwnd.erase(lbi.hWnd);
    LBI.pop_back();
    lbiRemove--;
  }
  int tlRemove=TL.size()-ri.nTL;

  while (tlRemove>0 && TL.size()>0) {
    TL.pop_back();
    tlRemove--;
  }
  itTL=TL.begin();
  updateImageReferences();

  // Clear cache of shown strings
  shownStrings.clear();

  int biRemove=BI.size()-ri.nBI;
  while (biRemove>0 && BI.size()>0) {
    ButtonInfo &bi=BI.back();
    bi.callBack = nullptr;
    bi.clearHandler();
    DestroyWindow(bi.hWnd);
    biByHwnd.erase(bi.hWnd);
    BI.pop_back();
    biRemove--;
  }

  int iiRemove=II.size()-ri.nII;

  while (iiRemove>0 && II.size()>0) {
    InputInfo &ii=II.back();
    ii.callBack = nullptr; // Avoid kill focus event here
    ii.clearHandler();
    DestroyWindow(ii.hWnd);
    iiByHwnd.erase(ii.hWnd);
    II.pop_back();
    iiRemove--;
  }

  int rectRemove=Rectangles.size()-ri.nRect;

  while (rectRemove>0 && Rectangles.size()>0) {
    Rectangles.pop_back();
    rectRemove--;
  }

  int hwndRemove=FocusList.size()-ri.nHWND;
  while(hwndRemove>0 && FocusList.size()>0) {
    FocusList.pop_back();
    hwndRemove--;
  }

  while(Tables.size() > unsigned(ri.nTables)){
    auto t=Tables.back().table;
    Tables.pop_back();
    t->hide(*this);
  }

  int dataRemove=DataInfo.size()-ri.nData;
  while(dataRemove>0 && DataInfo.size()>0) {
    DataInfo.pop_front();
    dataRemove--;
  }

  CurrentX=ri.sCX;
  CurrentY=ri.sCY;
  onClear = ri.onClear;
  postClear = ri.postClear;

  for (auto it = restorePoints.begin(); it != restorePoints.end(); ) {
    if (!ri.restorePoints.count(it->first) && (& it->second != &ri))
      it = restorePoints.erase(it);
    else
      ++it;
  }
}

void gdioutput::restore(const string &restorePointId, bool doRefresh) {
  auto rp = restorePoints.find(restorePointId);
  if (rp == restorePoints.end())
    return;
  const RestoreInfo& ri = rp->second;

  restoreInternal(ri);

  MaxX=ri.sMX;
  MaxY=ri.sMY;

  if (doRefresh)
    refresh();

  setOffset(ri.sOY, ri.sOY, false);
}

RECT gdioutput::getDimensionSince(const string& restorePointId) const {
  auto rp = restorePoints.find(restorePointId);
  if (rp == restorePoints.end())
    throw meosException("Internal error: " + restorePointId);
  
  const RestoreInfo& ri = rp->second;
  RECT out = {numeric_limits<int>::max(), numeric_limits<int>::max(), 0, 0};
  
  auto grow = [&out](int x, int y, int w, int h) {
    out.left = min<int>(out.left, x);
    out.right = max<int>(out.right, x + w);
    out.top = min<int>(out.top, y);
    out.bottom = max<int>(out.bottom, y + h);
  };

  int lbiRemove = LBI.size() - ri.nLBI;
  for (auto it = LBI.rbegin(); lbiRemove > 0; lbiRemove--, ++it) {
    grow(it->getX(), it->getY(), it->getWidth(), it->getHeight());
  }
  
  int tlRemove = TL.size() - ri.nTL;
  for (auto it = TL.rbegin(); tlRemove > 0; tlRemove--, ++it) {
    grow(it->getX(), it->getY(), it->getWidth(), it->getHeight());
  }

  int biRemove = BI.size() - ri.nBI;
  for (auto it = BI.rbegin(); biRemove > 0; biRemove--, ++it) {
    int w, h;
    it->getDimension(*this, w, h);
    grow(it->getX(), it->getY(), w, h);
  }

  int iiRemove = II.size() - ri.nII;
  for (auto it = II.rbegin(); iiRemove > 0; iiRemove--, ++it) {
    grow(it->getX(), it->getY(), it->getWidth(), it->getHeight());
  }

  int rectRemove = Rectangles.size() - ri.nRect;
  for (auto it = Rectangles.rbegin(); rectRemove > 0; rectRemove--, ++it) {
    auto& rc = it->getRect();
    grow(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
  }
  
  return out;
}

void gdioutput::restoreNoUpdate(const string &restorePointId) {
  auto rp = restorePoints.find(restorePointId);
  if (rp == restorePoints.end())
    return;

  const RestoreInfo& ri = rp->second;

  MaxX=ri.sMX;
  MaxY=ri.sMY;

  restoreInternal(ri);
}

bool gdioutput::canClear() {
  bool ok = true;
  auto clsCopy = onClear;
  for (auto& clr : clsCopy) {
    try {
      if (clr.makeEvent(*this, GUI_CLEAR) == 0)
        ok = false;
    }
    catch (const meosCancel&) {
      return false;
    }
    catch (meosException& ex) {
      if (isTestMode)
        throw ex;
      wstring msg = ex.wwhat();
      alert(msg);
      return true;
    }
    catch (const std::exception& ex) {
      if (isTestMode)
        throw ex;
      string msg(ex.what());
      alert(msg);
      return true;
    }
  }
  return ok;
}

int gdioutput::sendCtrlMessage(const string &id)
{
  for (list<ButtonInfo>::iterator it=BI.begin(); it != BI.end(); ++it) {
    if (id==it->id) {
      if (it->hasEventHandler())
        return it->handleEvent(*this, GUI_BUTTON);
      else if (it->callBack) 
        return it->callBack(this, GUI_BUTTON, &*it); //it may be destroyed here...
    }
  }
  for(list<EventInfo>::iterator it=Events.begin(); it != Events.end(); ++it){
    if (id==it->id) {
      if (it->hasEventHandler())
        return it->handleEvent(*this, GUI_EVENT);
      else if (it->callBack) 
        return it->callBack(this, GUI_EVENT, &*it); //it may be destroyed here...
    }
  }
#ifdef _DEBUG
  throw meosException("Unknown command " +id);
#endif
  return 0;
}

void gdioutput::unregisterEvent(const string &id)
{
  list<EventInfo>::iterator it;
  for (it = Events.begin(); it != Events.end(); ++it) {
    if ( id == it->id) {
      Events.erase(it);
      return;
    }
  }
}

EventInfo &gdioutput::registerEvent(const string &id, GUICALLBACK cb)
{
  list<EventInfo>::iterator it;
  for (it = Events.begin(); it != Events.end(); ++it) {
    if ( id == it->id) {
      Events.erase(it);
      break;
    }
  }

  EventInfo ei;
  ei.id=id;
  ei.callBack=cb;

  Events.push_front(ei);
  return Events.front();
}

void flushEvent(const string &id, const string &origin, DWORD data, int extraData);

DWORD gdioutput::makeEvent(const string &id, const string &origin,
                           DWORD data, int extraData, bool doflush)
{
  if (doflush) {
#ifndef MEOSDB
    ::flushEvent(id, origin, data, extraData);
#else
    throw std::exception("internal gdi/database error");
#endif
  }
  else {
    list<EventInfo>::iterator it;

    for(it=Events.begin(); it != Events.end(); ++it){
      if (id==it->id && (it->callBack || it->hasEventHandler()) ) {
        it->setData(origin, data);
        if (extraData) {
          it->setExtra(extraData);
        }
        if (it->handleEvent(*this, GUI_EVENT)) {
          return 1;
        }
        else
          return it->callBack(this, GUI_EVENT, &*it); //it may be destroyed here...
      }
    }
  }
  return -1;
}


RectangleInfo &RectangleInfo::changeDimension(gdioutput &gdi, int dx, int dy) {
  rc.right += dx;
  rc.bottom += dy;
  int ex = gdi.scaleLength(5);
  gdi.updatePos(rc.left, rc.top, rc.right-rc.left+ex, rc.bottom-rc.top+ex);
  return *this;
}

RectangleInfo &gdioutput::addRectangle(const RECT &rc, GDICOLOR color, bool drawBorder, bool addFirst) {
  RectangleInfo ri;

  ri.rc.left = min<int>(rc.left, rc.right);
  ri.rc.right = max<int>(rc.left, rc.right);
  ri.rc.top = min<int>(rc.top, rc.bottom);
  ri.rc.bottom = max<int>(rc.top, rc.bottom);

  if (color==colorDefault)
    ri.color = GetSysColor(COLOR_INFOBK);
  else if (color == colorWindowBar) {
    ri.color = GetSysColor(COLOR_3DFACE);
  }
  else ri.color = color;

  ri.color2 = ri.color;
  ri.drawBorder = drawBorder;

  if (hWndTarget && !manualUpdate) {
    HDC hDC=GetDC(hWndTarget);
    renderRectangle(hDC, 0, ri);
    ReleaseDC(hWndTarget, hDC);
  }

  int ex = scaleLength(5);
  updatePos(ri.rc.left, ri.rc.top, ri.rc.right-ri.rc.left+ex, ri.rc.bottom-ri.rc.top+ex);
  if (addFirst) {
    Rectangles.push_front(ri);
    return Rectangles.front();
  }
  else {
    Rectangles.push_back(ri);
    return Rectangles.back();
  }
}

void gdioutput::setMapRenderer(shared_ptr<MapDataRenderer>& rdr) {
  renderMap = rdr;
}

RectangleInfo &gdioutput::getRectangle(const char *id) {
  for (list<RectangleInfo>::iterator it = Rectangles.begin(); it != Rectangles.end(); ++it) {
    return *it;
  }
  string err = string("Internal Error, identifier not found: X#") + id;
  throw std::exception(err.c_str());
}
  
void gdioutput::setOffset(int x, int y, bool update)
{
  int h, w;
  getTargetDimension(w, h);

  int cdy = 0;
  int cdx = 0;

  if (y != OffsetY) {
    int oldY = OffsetY;
    OffsetY = y;
    if (OffsetY < 0)
      OffsetY = 0;
    else if (OffsetY > MaxY)
      OffsetY = MaxY;
    //cdy=(oldY!=OffsetY);
    cdy = oldY - OffsetY;
  }

  if (x != OffsetX) {
    int oldX = OffsetX;
    OffsetX = x;
    if (OffsetX < 0)
      OffsetX = 0;
    else if (OffsetX > MaxX)
      OffsetX = MaxX;

    //cdx=(oldX!=OffsetX);
    cdx = oldX - OffsetX;
  }

  if (cdx || cdy) {
    updateScrollbars();
    updateObjectPositions();
    if (cdy) {
      SCROLLINFO si;
      memset(&si, 0, sizeof(si));

      si.nPos = OffsetY;
      si.fMask = SIF_POS;
      SetScrollInfo(hWndTarget, SB_VERT, &si, true);
    }

    if (cdx) {
      SCROLLINFO si;
      memset(&si, 0, sizeof(si));

      si.nPos = OffsetX;
      si.fMask = SIF_POS;
      SetScrollInfo(hWndTarget, SB_HORZ, &si, true);
    }

    if (update) {
      //RECT ScrollArea, ClipArea;
      //GetClientRect(hWndTarget, &ScrollArea);
      //ClipArea = ScrollArea;

    /*  ScrollArea.top=-gdi->getHeight()-100;
      ScrollArea.bottom+=gdi->getHeight();
      ScrollArea.right=gdi->getWidth()-gdi->GetOffsetX()+15;
      ScrollArea.left = -2000;
  */
      ScrollWindowEx(hWndTarget, -cdx, cdy,
        NULL, NULL,
        (HRGN)NULL, (LPRECT)NULL, 0/*SW_INVALIDATE|SW_SMOOTHSCROLL|(1000*65536 )*/);
      UpdateWindow(hWndTarget);

    }
  }
}

void gdioutput::scrollTo(int x, int y) {
  int cx = x - OffsetX;
  int cy = y - OffsetY;

  int h, w;
  getTargetDimension(w, h);

  bool cdy = false;
  bool cdx = false;

  if (cy <= (h / 15) || cy >= (h - h / 10)) {
    int oldY = OffsetY;
    OffsetY = y - h / 2;
    if (OffsetY < 0)
      OffsetY = 0;
    else if (OffsetY > MaxY)
      OffsetY = MaxY;

    cdy = (oldY != OffsetY);
  }

  if (cx <= (w / 15) || cx >= (w - w / 8)) {
    int oldX = OffsetX;
    OffsetX = x - w / 2;
    if (OffsetX < 0)
      OffsetX = 0;
    else if (OffsetX > MaxX)
      OffsetX = MaxX;

    cdx = (oldX != OffsetX);
  }

  if (cdx || cdy) {
    updateScrollbars();
    updateObjectPositions();
    if (cdy) {
      SCROLLINFO si;
      memset(&si, 0, sizeof(si));

      si.nPos = OffsetY;
      si.fMask = SIF_POS;
      SetScrollInfo(hWndTarget, SB_VERT, &si, true);
    }

    if (cdx) {
      SCROLLINFO si;
      memset(&si, 0, sizeof(si));

      si.nPos = OffsetX;
      si.fMask = SIF_POS;
      SetScrollInfo(hWndTarget, SB_HORZ, &si, true);
    }
  }
}

void gdioutput::scrollToBottom() {
  OffsetY = MaxY;
  SCROLLINFO si;
  memset(&si, 0, sizeof(si));

  updateScrollbars();
  updateObjectPositions();
  si.nPos = OffsetY;
  si.fMask = SIF_POS;
  SetScrollInfo(hWndTarget, SB_VERT, &si, true);
}

bool gdioutput::clipOffset(int PageX, int PageY, int& MaxOffsetX, int& MaxOffsetY)
{
  if (animationData) {
    MaxOffsetX = 0;
    MaxOffsetY = 0;
    return false;
  }

  if (highContrast)
    setHighContrastMaxWidth();

  int oy = OffsetY;
  int ox = OffsetX;

  MaxOffsetY = max(getPageY() - PageY, 0);
  MaxOffsetX = max(getPageX() - PageX, 0);

  if (OffsetY < 0) OffsetY = 0;
  else if (OffsetY > MaxOffsetY)
    OffsetY = MaxOffsetY;

  if (OffsetX < 0) OffsetX = 0;
  else if (OffsetX > MaxOffsetX)
    OffsetX = MaxOffsetX;

  if (ox != OffsetX || oy != OffsetY) {
    updateObjectPositions();
    return true;

  }
  return false;
}

//bool ::GetSaveFile(string &file, char *filter)
wstring gdioutput::browseForSave(const vector< pair<wstring, wstring> > &filter,
                                const wstring &defext, int &filterIndex)
{
  if (isTestMode) {
    if (!cmdAnswers.empty()) {
      string ans = cmdAnswers.front();
      cmdAnswers.pop_front();
      if (ans.substr(0, 1) == "*")
        return widen(ans.substr(1));
    }
    throw meosException("Browse for file");
  }

  InitCommonControls();

  TCHAR FileName[260];
  FileName[0]=0;
  OPENFILENAME of;
  wstring sFilter;
  for (size_t k = 0; k< filter.size(); k++) {
    sFilter.append(lang.tl(filter[k].first)).push_back(0);
    sFilter.append(filter[k].second).push_back(0);
  }
  sFilter.push_back(0);

  of.lStructSize       = sizeof(of);
  of.hwndOwner         = hWndTarget;
  of.hInstance         = (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE);
  of.lpstrFilter       = sFilter.c_str();
  of.lpstrCustomFilter = NULL;
  of.nMaxCustFilter    = 0;
  of.nFilterIndex      = filterIndex;
  of.lpstrFile         = FileName;
  of.nMaxFile          = 260;
  of.lpstrFileTitle    = NULL;
  of.nMaxFileTitle     = 0;
  of.lpstrInitialDir   = NULL;
  of.lpstrTitle        = NULL;
  of.Flags             = OFN_OVERWRITEPROMPT|OFN_HIDEREADONLY;
  of.lpstrDefExt   	   = defext.c_str();
  of.lpfnHook		       = NULL;

  bool res;
  setCommandLock();
  try {
    res = GetSaveFileName(&of) != false;
    liftCommandLock();
  }
  catch (...) {
    liftCommandLock();
    throw;
  }

  if (res==false)
    return L"";

  filterIndex=of.nFilterIndex;

  return FileName;
}

wstring gdioutput::browseForOpen(const vector< pair<wstring, wstring> > &filter,
                                const wstring &defext)
{
  if (isTestMode) {
    if (!cmdAnswers.empty()) {
      string ans = cmdAnswers.front();
      cmdAnswers.pop_front();
      if (ans.substr(0, 1) == "*")
        return widen(ans.substr(1));
    }
    throw meosException("Browse for file");
  }

  InitCommonControls();

  wchar_t FileName[260];
  FileName[0]=0;
  OPENFILENAME of;

  wstring sFilter;
  for (size_t k = 0; k< filter.size(); k++) {
    sFilter.append(lang.tl(filter[k].first)).push_back(0);
    sFilter.append(filter[k].second).push_back(0);
  }
  sFilter.push_back(0);
  
  of.lStructSize       = sizeof(of);
  of.hwndOwner         = hWndTarget;
  of.hInstance         = (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE);
  of.lpstrFilter       = sFilter.c_str();
  of.lpstrCustomFilter = NULL;
  of.nMaxCustFilter    = 0;
  of.nFilterIndex      = 1;
  of.lpstrFile         = FileName;
  of.nMaxFile          = 260;
  of.lpstrFileTitle    = NULL;
  of.nMaxFileTitle     = 0;
  of.lpstrInitialDir   = NULL;
  of.lpstrTitle        = NULL;
  of.Flags             = OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST|OFN_HIDEREADONLY;
  of.lpstrDefExt   	   = defext.c_str();
  of.lpfnHook		       = NULL;

  bool res;
  setCommandLock();
  try {
    res = GetOpenFileName(&of) != false;
    liftCommandLock();
  }
  catch (...) {
    liftCommandLock();
    throw;
  }


  if (res == false)
    return L"";

  return FileName;
}

wstring gdioutput::browseForFolder(const wstring &folderStart, const wchar_t *descr)
{
  if (isTestMode) {
    if (!cmdAnswers.empty()) {
      string ans = cmdAnswers.front();
      cmdAnswers.pop_front();
      if (ans.substr(0, 1) == "*")
        return widen(ans.substr(1));
    }
    throw meosException("Browse for folder");
  }

  CoInitializeEx(0, COINIT_APARTMENTTHREADED);
  BROWSEINFO bi;

  wchar_t InstPath[260];
  wcscpy_s(InstPath, folderStart.c_str());

  memset(&bi, 0, sizeof(bi) );

  bi.hwndOwner=hWndAppMain;
  bi.pszDisplayName=InstPath;
  wstring title = descr ? lang.tl(descr) : L"";
  bi.lpszTitle = title.c_str();
  bi.ulFlags=BIF_RETURNONLYFSDIRS|BIF_EDITBOX|BIF_NEWDIALOGSTYLE|BIF_EDITBOX;

  LPITEMIDLIST  pidl_new;

  setCommandLock();
  try {
    pidl_new = SHBrowseForFolder(&bi);
    liftCommandLock();
  }
  catch (...) {
    liftCommandLock();
    throw;
  }

  if (pidl_new==NULL)
    return L"";

  // Convert the item ID list's binary
  // representation into a file system path
  //char szPath[_MAX_PATH];
  SHGetPathFromIDList(pidl_new, InstPath);

  // Allocate a pointer to an IMalloc interface
  LPMALLOC pMalloc;

  // Get the address of our task allocator's IMalloc interface
  SHGetMalloc(&pMalloc);

  // Free the item ID list allocated by SHGetSpecialFolderLocation
  pMalloc->Free(pidl_new);

  // Free our task allocator
  pMalloc->Release();

  return InstPath;
}


bool gdioutput::openDoc(const wstring &doc) {
  return (intptr_t)ShellExecute(hWndTarget, L"open", doc.c_str(), NULL, L"", SW_SHOWNORMAL) >32;
}

void gdioutput::init(HWND hWnd, HWND hMain, HWND hTab) {
  setWindow(hWnd);
  hWndAppMain=hMain;
  hWndTab=hTab;

  InitCommonControls();

  hWndToolTip = CreateWindow(TOOLTIPS_CLASS, (LPWSTR) NULL, TTS_ALWAYSTIP,
      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
      NULL, (HMENU) NULL, (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE), NULL);
}

ToolInfo &gdioutput::addToolTip(const string &tipId, const wstring &tip, HWND hWnd, RECT *rc) {
  static ToolInfo dummy;
  if (!hWndToolTip)
    return dummy;

  toolTips.emplace_back();
  ToolInfo &info = toolTips.back();
  TOOLINFOW &ti = info.ti;
  info.tip = lang.tl(tip);

  memset(&ti, 0, sizeof(ti));
  ti.cbSize = sizeof(TOOLINFO);

  if (hWnd != 0) {
    ti.uFlags = TTF_IDISHWND;
    info.id = uintptr_t(hWnd);
    ti.uId = (UINT_PTR) hWnd;
  }
  else {
    ti.uFlags = TTF_SUBCLASS;
    info.id = toolTips.size();
    ti.uId = info.id;
  }

  ti.hwnd = hWndTarget;
  ti.hinst = (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE);
  info.name = tipId;
  ti.lpszText = (LPWSTR)toolTips.back().tip.c_str();

  if (rc != nullptr) {
    ti.rect = *rc;
    info.rc = *rc;
    info.hasRect = true;
    ti.rect.top -= OffsetY;
    ti.rect.bottom -= OffsetY;
    ti.rect.right -= OffsetX;
    ti.rect.left -= OffsetX;
  }
  SendMessage(hWndToolTip, TTM_ADDTOOLW, 0, (LPARAM) &ti);

  if (tip.find('\n') != string::npos || tip.length()>40)
    SendMessage(hWndToolTip, TTM_SETMAXTIPWIDTH, 0, scaleLength(250));

  return info;
}

void gdioutput::removeToolTip(const string& id) {
  for (auto tt = toolTips.begin(); tt != toolTips.end(); ++tt) {
    if (tt->name == id) {
      if (hWndToolTip) {
        SendMessage(hWndToolTip, TTM_DELTOOL, 0, (LPARAM)&tt->ti);
      }
      toolTips.erase(tt);
      return;
    }
  }
}


ToolInfo *gdioutput::getToolTip(const string &id) {
  for (ToolList::reverse_iterator it = toolTips.rbegin(); it != toolTips.rend(); ++it) {
    if (it->name == id)
      return &*it;
  }
  return 0;
}

ToolInfo &gdioutput::updateToolTip(const string &id, const wstring &tip) {
  for (ToolList::reverse_iterator it = toolTips.rbegin(); it != toolTips.rend(); ++it) {
    if (it->name == id && hWndToolTip) {
      it->tip = lang.tl(tip);
      it->ti.lpszText = (LPWSTR)it->tip.c_str();
      SendMessage(hWndToolTip, TTM_UPDATETIPTEXTW, 0, (LPARAM) &it->ti);
      return *it;
    }
  }
  BaseInfo &bi = getBaseInfo(id.c_str());
  return addToolTip(id, tip, bi.getControlWindow());
}

void gdioutput::selectTab(int Id)
{
  if (hWndTab)
    TabCtrl_SetCurSel(hWndTab, Id);
}

void gdioutput::getTargetDimension(int &x, int &y) const
{
  if (hWndTarget){
    RECT rc;
    GetClientRect(hWndTarget, &rc);
    x=rc.right;
    y=rc.bottom;
  }
  else {
    x=0;
    y=0;
  }
}

Table &gdioutput::getTable() const {
  if (Tables.empty())
    throw std::exception("No table defined");

  return *const_cast<Table *>(Tables.back().table.get());
}

static int gdiTableCB(gdioutput *gdi, GuiEventType type, BaseInfo *data)
{
  if (type == GUI_BUTTON) {
    ButtonInfo bi = *static_cast<ButtonInfo *>(data);
    gdi->tableCB(bi, &gdi->getTable());
  }
  return 0;
}

void gdioutput::tableCB(ButtonInfo &bu, Table *t)
{
  if (bu.id=="tblPrint") {
    t->keyCommand(*this, KC_PRINT);
  }
  else if (bu.id=="tblColumns") {
    disableTables();
    if (Tables.empty())
      return;

    restore("tblRestore");
    int ybase =  Tables.back().yp;
    addString("", ybase, 20, boldLarge, "Välj kolumner");
    ybase += scaleLength(30);
    addString("", ybase, 20, 0, L"Välj kolumner för tabellen X.#"+ t->getTableName());
    ybase += getLineHeight()*2;

    addListBox(20, ybase, "tblColSel", 180, 450, 0, L"", L"", true);
    const int btnHeight = getButtonHeight()+scaleLength(5);
    vector<Table::ColSelection> cols = t->getColumns();
    set<int> sel;

    for (size_t k=0; k<cols.size(); k++) {
      addItem("tblColSel", cols[k].name, cols[k].index);
      if (cols[k].selected)
        sel.insert(cols[k].index);
    }
    setSelection("tblColSel", sel);
    int xp = scaleLength(220);
    addButton(xp, ybase+btnHeight*0, "tblAll", "Välj allt", gdiTableCB);
    addButton(xp, ybase+btnHeight*1, "tblNone", "Välj inget", gdiTableCB);
    addButton(xp, ybase+btnHeight*2, "tblAuto", "Välj automatiskt", gdiTableCB).setExtra(t->getTableId());

    addButton(xp, ybase+btnHeight*4, "tblOK", "OK", gdiTableCB).setExtra(t->getTableId());
    addButton(xp, ybase+btnHeight*5, "tblCancel", "Avbryt", gdiTableCB);

    if (toolbar)
      toolbar->hide();

    refresh();
  }
  else if (bu.id=="tblAll") {
    set<int> sel;
    sel.insert(-1);
    setSelection("tblColSel", sel);
  }
  else if (bu.id=="tblNone") {
    set<int> sel;
    setSelection("tblColSel", sel);
  }
  else if (bu.id=="tblAuto") {
    restore("tblRestore", false);
    t->autoSelectColumns();
    t->autoAdjust(*this);
    enableTables();
    refresh();
  }
  else if (bu.id=="tblOK") {
    set<int> sel;
    getSelection("tblColSel", sel);
    restore("tblRestore", false);
    t->clearCellSelection(this);
    t->selectColumns(sel);
    t->autoAdjust(*this);
    enableTables();
    refresh();
  }
  else if (bu.id=="tblReset") {
    t->clearCellSelection(this);
    t->resetColumns();
    t->autoAdjust(*this);
    t->updateDimension(*this);
    refresh();
  }
  else if (bu.id == "tblMarkAll") {
    t->keyCommand(*this, KC_MARKALL);
  }
  else if (bu.id == "tblClearAll") {
    t->keyCommand(*this, KC_CLEARALL);
  }
  else if (bu.id=="tblUpdate") {
    t->keyCommand(*this, KC_REFRESH);
  }
  else if (bu.id=="tblCancel") {
    restore("tblRestore", true);
    enableTables();
    refresh();
  }
  else if (bu.id == "tblCopy") {
    t->keyCommand(*this, KC_COPY);
  }
  else if (bu.id == "tblPaste") {
    t->keyCommand(*this, KC_PASTE);
  }
  else if (bu.id == "tblRemove") {
    t->keyCommand(*this, KC_DELETE);
  }
  else if (bu.id == "tblInsert") {
    t->keyCommand(*this, KC_INSERT);
  }
}

void gdioutput::enableTables()
{
  useTables=true;
  if (!Tables.empty()) {
    auto &t = Tables.front().table;
    if (toolbar == 0)
      toolbar = new Toolbar(*this);

    toolbar->setData(t);

    string tname = string("table") + itos(t->canDelete()) + itos(t->canInsert()) + itos(t->canPaste());
    if (!toolbar->isLoaded(tname)) {
      toolbar->reset();
      toolbar->addButton("tblColumns", 1, 2, "Välj vilka kolumner du vill visa");
      toolbar->addButton("tblPrint", 0, STD_PRINT, "Skriv ut tabellen (X)#Ctrl+P");
      toolbar->addButton("tblUpdate", 1, 0, "Uppdatera alla värden i tabellen (X)#F5");
      toolbar->addButton("tblReset", 1, 4, "Återställ tabeldesignen och visa allt");
      toolbar->addButton("tblMarkAll", 1, 5, "Markera allt (X)#Ctrl+A");
      toolbar->addButton("tblClearAll", 1, 6, "Markera inget (X)#Ctrl+D");
      toolbar->addButton("tblCopy", 0, STD_COPY, "Kopiera selektionen till urklipp (X)#Ctrl+C");
      if (t->canPaste())
        toolbar->addButton("tblPaste", 0, STD_PASTE, "Klistra in data från urklipp (X)#Ctrl+V");
      if (t->canDelete())
       toolbar->addButton("tblRemove", 1, 1, "Ta bort valda rader från tabellen (X)#Del");
      if (t->canInsert())
       toolbar->addButton("tblInsert", 1, 3, "Lägg till en ny rad i tabellen (X)#Ctrl+I");
      toolbar->createToolbar(tname, L"Tabellverktyg");
    }
    else {
      toolbar->show();
    }
  }
}

void gdioutput::processToolbarMessage(const string &id, Table *tbl) {
  if (hasCommandLock())
    return;
  wstring msg;
  string cmd;
  if (getRecorder().recording()) { 
    cmd = "tableCmd(\"" + id + "\"); //" + toUTF8(tbl->getTableName());
  }
  try {
    ButtonInfo bi;
    bi.id = id;
    tableCB(bi, tbl);
    getRecorder().record(cmd);
  }
  catch (const meosCancel&) {
  }
  catch (meosException &ex) {
    msg = ex.wwhat();
  }
  catch(std::exception &ex) {
    msg = widen(ex.what());
    if (msg.empty())
      msg = L"Ett okänt fel inträffade.";
  }
  catch(...) {
    msg = L"Ett okänt fel inträffade.";
  }

  if (!msg.empty())
    alert(msg);
}

HWND gdioutput::getToolbarWindow() const {
  if (!toolbar)
    return 0;
  return toolbar->getFloater();
}

bool gdioutput::hasToolbar() const {
  if (!toolbar)
    return false;
  return toolbar->isVisible();
}

void gdioutput::activateToolbar(bool active) {
  if (!toolbar)
    return;
  toolbar->activate(active);
}

void gdioutput::disableTables()
{
  useTables=false;

  for(list<ButtonInfo>::iterator bit=BI.begin(); bit != BI.end();) {
    if (bit->id.substr(0, 3)=="tbl" && bit->getExtra()!=0) {
      string id = bit->id;
      ++bit;
      removeWidget(id);
    }
    else
      ++bit;
  }

}

void gdioutput::addTable(const shared_ptr<Table> &t, int x, int y)
{
  TableInfo ti;
  ti.table = t;
  ti.xp = x;
  ti.yp = y;
  t->setPosition(x,y, MaxX, MaxY);

  if (t->hasAutoSelect())
    t->autoSelectColumns();
  t->autoAdjust(*this);

  Tables.push_back(ti);

  //updatePos(x, y, dx + TableXMargin, dy + TableYMargin);
  setRestorePoint("tblRestore");

  enableTables();
  updateScrollbars();
}

void gdioutput::pasteText(const char *id)
{
  list<InputInfo>::iterator it;
  for (it=II.begin(); it != II.end(); ++it) {
    if (it->id==id) {
      SendMessage(it->hWnd, WM_PASTE, 0,0);
      return;
    }
  }
}

wchar_t *gdioutput::getExtra(const char *id) const {
  return getBaseInfo(id).getExtra();
}

int gdioutput::getExtraInt(const char *id) const {
  return getBaseInfo(id).getExtraInt();
}

bool gdioutput::hasEditControl() const
{
  return !II.empty() || (Tables.size()>0 && Tables.front().table->hasEditControl());
}

void gdioutput::enableEditControls(bool enable, bool processAll)
{
  set<string> TCheckControls;
  for (list<ButtonInfo>::iterator it=BI.begin(); it != BI.end(); ++it) {
    if (it->isEditControl || processAll) {
      EnableWindow(it->hWnd, enable);
      if (it->isCheckbox) {
        TCheckControls.insert("T" + it->id);
      }
    }
  }

  for (list<TextInfo>::iterator it=TL.begin(); it != TL.end(); ++it) {
    if (TCheckControls.count(it->id)) {
      enableCheckBoxLink(*it, enable);
    }
  }


  for (list<InputInfo>::iterator it=II.begin(); it != II.end(); ++it) {
    if (it->isEditControl)
      EnableWindow(it->hWnd, enable);
  }

  for(  list<ListBoxInfo>::iterator it=LBI.begin(); it != LBI.end(); ++it) {
    if (it->isEditControl)
      EnableWindow(it->hWnd, enable);
  }
}

void gdioutput::closeWindow() {
  PostMessage(hWndTarget, WM_CLOSE, 0, 0);
}

InputInfo &InputInfo::setPassword(bool pwd) {
  LONG style = GetWindowLong(hWnd, GWL_STYLE);
  if (pwd)
    style |= ES_PASSWORD;
  else
    style &= ~ES_PASSWORD;
  SetWindowLong(hWnd, GWL_STYLE, style);
  SendMessage(hWnd, EM_SETPASSWORDCHAR, 183, 0);
  return *this;
}

int gdioutput::setHighContrastMaxWidth() {

  RECT rc;
  GetClientRect(hWndTarget, &rc);

  if (lockRefresh)
    return rc.bottom;

#ifdef DEBUGRENDER
  OutputDebugString("Set high contrast\n");
#endif

  double w = getPageX();
  double s = rc.right / w;
  if (!highContrast || (fabs(s-1.0) > 1e-3 && (s * scale) >= 1.0) ) {
    lockRefresh = true;
    try {
      highContrast = true;
      scaleSize(s);
      refresh();
      lockRefresh = false;
    }
    catch (...) {
      lockRefresh = false;
      throw;
    }
  }
  return rc.bottom;
}

double static acc = 0;

void gdioutput::setAutoScroll(double speed) {
  if (autoSpeed == 0 && speed != 0) {
    SetTimer(hWndTarget, 1001, 20, 0);
    autoPos = OffsetY;
  }
  else if (speed == 0 && autoSpeed != 0) {
    KillTimer(hWndTarget, 1001);
  }

  if (speed == -1)
    autoSpeed = -autoSpeed;
  else
    autoSpeed = speed;

  autoCounter = - M_PI_2;
  acc = 0;
}

void gdioutput::getAutoScroll(double &speed, double &pos) const {
  RECT rc;
  GetClientRect(hWndTarget, &rc);
  double height = rc.bottom;

  double s = autoSpeed * (1 + height/1000 + sin(autoCounter)/max(1.0, 500/height));

  autoCounter += M_PI/75.0;
  if (autoCounter > M_PI)
    autoCounter -= 2*M_PI;

  acc += 0.3/30;
  if (acc>0.8)
    acc = 0.8;

  speed = (lastSpeed * (1.0-acc) + s * acc);
  lastSpeed = speed;
  pos = autoPos;
}

void gdioutput::storeAutoPos(double pos) {
  autoPos = pos;
}

void gdioutput::setFullScreen(bool useFullScreen) {
  if (useFullScreen && !fullScreen) {
    SetWindowLong(hWndTarget, GWL_STYLE, WS_POPUP | WS_BORDER);
    ShowWindow(hWndTarget, SW_MAXIMIZE);
    UpdateWindow(hWndTarget);
  }
  else if (fullScreen) {
    SetWindowLong(hWndTarget, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
    ShowWindow(hWndTarget, SW_NORMAL);
    UpdateWindow(hWndTarget);
  }
  fullScreen = useFullScreen;
}

void gdioutput::setColorMode(DWORD bgColor1, DWORD bgColor2,
                             DWORD fgColor, const wstring &bgImage) {
  backgroundColor1 = bgColor1;
  backgroundColor2 = bgColor2;
  foregroundColor = fgColor;
  backgroundImage = bgImage;
}


bool gdioutput::hasFGColor() const {
  return foregroundColor != -1;
}
bool gdioutput::hasBGColor() const {
  return backgroundColor1 != -1;
}
bool gdioutput::hasBGColor2() const {
  return backgroundColor2 != -1;
}

DWORD gdioutput::getFGColor() const {
  return foregroundColor != -1 ? foregroundColor : 0;
}
DWORD gdioutput::getBGColor() const {
  return backgroundColor1 != -1 ? backgroundColor1 : RGB(255,255,255);
}
DWORD gdioutput::getBGColor2() const {
  return backgroundColor2;
}
const wstring &gdioutput::getBGImage() const {
  return backgroundImage;
}

bool gdioutput::hasCommandLock() const {
  if (commandLock)
    return true;

  if (commandUnlockTime > 0) {
    uint64_t t = GetTickCount64();
    if (commandUnlockTime < (commandUnlockTime + 500) &&
        t < (commandUnlockTime+500)) {
      commandUnlockTime = 0;
      return true;
    }
  }

  return false;
}

void gdioutput::setCommandLock() const {
  commandLock = true;
}

void gdioutput::liftCommandLock() const {
  commandUnlockTime = GetTickCount64();
  commandLock = false;
}

int gdioutput::getLineHeight(gdiFonts font, const wchar_t *face) const {
  int h;
  if (face == nullptr)
    h = getFontHeight(font, _EmptyWString);
  else
    h = getFontHeight(font, face);

  return (11*h)/10;
}

GDIImplFontSet::GDIImplFontSet() {
  Huge = 0;
  Large = 0;
  Medium = 0;
  Small = 0;
  pfLarge = 0;
  pfMedium = 0;
  pfMediumPlus = 0;
  pfSmall = 0;
  pfSmallItalic = 0;
  pfItalic = 0;
  pfItalicMediumPlus = 0;
  pfMono = 0;
}

GDIImplFontSet::~GDIImplFontSet() {
  deleteFonts();
}

void GDIImplFontSet::deleteFonts()
{
  if (Huge)
    DeleteObject(Huge);
  Huge = 0;

  if (Large)
    DeleteObject(Large);
  Large = 0;

  if (Medium)
    DeleteObject(Medium);
  Medium = 0;

  if (Small)
    DeleteObject(Small);
  Small = 0;

  if (pfLarge)
    DeleteObject(pfLarge);
  pfLarge = 0;

  if (pfMedium)
    DeleteObject(pfMedium);
  pfMedium = 0;

  if (pfMediumPlus)
    DeleteObject(pfMediumPlus);
  pfMediumPlus = 0;

  if (pfSmall)
    DeleteObject(pfSmall);
  pfSmall = 0;

  if (pfMono)
    DeleteObject(pfMono);
  pfMono = 0;


  if (pfSmallItalic)
    DeleteObject(pfSmallItalic);
  pfSmallItalic = 0;

  if (pfItalicMediumPlus)
    DeleteObject(pfItalicMediumPlus);
  pfItalicMediumPlus = 0;

  if (pfItalic)
    DeleteObject(pfItalic);
  pfItalic = 0;
}

float GDIImplFontSet::baseSize(int format, float scale)  {
  format &= 0xFF;
  if (format==0 || format==10) {
    return 14 * scale;
  }
  else if (format==fontMedium){
    return 14 * scale;
  }
  else if (format==1) {
    return 14.0001f * scale; //Bold
  }
  else if (format==boldLarge){
    return 24.0001f * scale;
  }
  else if (format==boldHuge){
    return 34.0001f * scale;
  }
  else if (format==boldSmall){
    return 11.0001f * scale;
  }
  else if (format==fontLarge){
    return 24 * scale;
  }
  else if (format==fontMediumPlus){
    return 18 * scale;
  }
  else if (format==fontSmall){
    return 11 * scale;
  }
  else if (format==italicSmall){
    return 11 * scale;
  }
  else if (format==italicText){
    return 14 * scale;
  }
  else if (format == monoText){
    return 14 * scale;
  }
  else if (format==italicMediumPlus){
    return 18 * scale;
  }
  else {
    return 10 * scale;
  }
}

void GDIImplFontSet::init(double scale, const wstring &font, const wstring &gdiName_)
{
  if (font == L"Segoe UI")
    scale = scale * 1.1;
  int charSet = DEFAULT_CHARSET;
  deleteFonts();
  gdiName = gdiName_;

  Huge=CreateFont(int(scale*34), 0, 0, 0, FW_BOLD, false,  false, false, charSet,
    OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH|FF_ROMAN, font.c_str());

  Large=CreateFont(int(scale*24), 0, 0, 0, FW_BOLD, false,  false, false, charSet,
    OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH|FF_ROMAN, font.c_str());

  Medium=CreateFont(int(scale*14), 0, 0, 0, FW_BOLD, false,  false, false, charSet,
    OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH|FF_ROMAN, font.c_str());

  Small=CreateFont(int(scale*11), 0, 0, 0, FW_BOLD, false,  false, false, charSet,
    OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH|FF_ROMAN, font.c_str());

  pfLarge=CreateFont(int(scale*24), 0, 0, 0, FW_NORMAL, false,  false, false, charSet,
    OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH|FF_ROMAN, font.c_str());

  pfMedium=CreateFont(int(scale*14), 0, 0, 0, FW_NORMAL, false,  false, false, charSet,
    OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH|FF_ROMAN, font.c_str());

  pfMediumPlus=CreateFont(int(scale*18), 0, 0, 0, FW_NORMAL, false,  false, false, charSet,
    OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH|FF_ROMAN, font.c_str());

  pfSmall=CreateFont(int(scale*11), 0, 0, 0, FW_NORMAL, false,  false, false, charSet,
    OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH|FF_ROMAN, font.c_str());

  pfSmallItalic = CreateFont(int(scale*11), 0, 0, 0, FW_NORMAL, true,  false, false, charSet,
    OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH|FF_ROMAN, font.c_str());

  pfItalic = CreateFont(int(scale*14), 0, 0, 0, FW_NORMAL, true,  false, false, charSet,
    OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH|FF_ROMAN, font.c_str());

  pfMono = CreateFont(int(scale*12), 0, 0, 0, FW_NORMAL, false, false, false, charSet,
    OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH|FF_MODERN, L"Lucida Console");

  pfItalicMediumPlus = CreateFont(int(scale*18), 0, 0, 0, FW_NORMAL, true,  false, false, charSet,
    OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH|FF_ROMAN, font.c_str());
}

void GDIImplFontSet::getInfo(FontInfo &fi) const {
  fi.normal = pfMedium;
  fi.bold = Medium;
  fi.italic = pfItalic;
}

void GDIImplFontSet::selectFont(HDC hDC, int format) const {
  if (format==0 || format==10) {
    SelectObject(hDC, pfMedium);
  }
  else if (format==fontMedium){
    SelectObject(hDC, pfMedium);
  }
  else if (format==1){
    SelectObject(hDC, Medium);
  }
  else if (format==boldLarge){
    SelectObject(hDC, Large);
  }
  else if (format==boldHuge){
    SelectObject(hDC, Huge);
  }
  else if (format==boldSmall){
    SelectObject(hDC, Small);
  }
  else if (format==fontLarge){
    SelectObject(hDC, pfLarge);
  }
  else if (format==fontMediumPlus){
    SelectObject(hDC, pfMediumPlus);
  }
  else if (format==fontSmall){
    SelectObject(hDC, pfSmall);
  }
  else if (format==italicSmall){
    SelectObject(hDC, pfSmallItalic);
  }
  else if (format==italicText){
    SelectObject(hDC, pfItalic);
  }
  else if (format==italicMediumPlus){
    SelectObject(hDC, pfItalicMediumPlus);
  }
  else if (format == monoText) {
    SelectObject(hDC, pfMono);
  }
  else {
    SelectObject(hDC, GetStockObject(DEFAULT_GUI_FONT));
  }
}


HFONT GDIImplFontSet::getFont(int format) const {
  format = format & 31;
  if (format==0 || format==10) {
    return pfMedium;
  }
  else if (format==fontMedium){
    return pfMedium;
  }
  else if (format==1){
    return Medium;
  }
  else if (format==boldLarge){
    return Large;
  }
  else if (format==boldHuge){
    return Huge;
  }
  else if (format==boldSmall){
    return Small;
  }
  else if (format==fontLarge){
    return pfLarge;
  }
  else if (format==fontMediumPlus){
    return pfMediumPlus;
  }
  else if (format==fontSmall){
    return pfSmall;
  }
  else if (format==italicSmall){
    return pfSmallItalic;
  }
  else if (format==italicText){
    return pfItalic;
  }
  else if (format==italicMediumPlus){
    return pfItalicMediumPlus;
  }
  else if (format == monoText) {
    return pfMono;
  }
  else {
    return (HFONT)GetStockObject(DEFAULT_GUI_FONT);
  }
}



const GDIImplFontSet &gdioutput::getCurrentFont() const {
  if (currentFontSet == 0) {
    map<wstring, GDIImplFontSet>::const_iterator res = fonts.find(currentFont);
    if (res == fonts.end())
      throw meosException(L"Font not defined: " + currentFont);
    currentFontSet = &res->second;
  }

  return *currentFontSet;
}

const GDIImplFontSet &gdioutput::getFont(const wstring &font) const {
  map<wstring, GDIImplFontSet>::const_iterator res = fonts.find(font);
  if (res == fonts.end()) {
    return const_cast<gdioutput *>(this)->loadFont(font);
    throw meosException(L"Font not defined: " + currentFont);
  }
  return res->second;
}

int CALLBACK enumFontProc(const LOGFONT* logFont, const TEXTMETRIC *metric, DWORD id, LPARAM lParam) {
  if (logFont->lfFaceName[0] == '@')
    return 1;

  if (metric->tmAveCharWidth <= 0)
    return 1;

  vector<GDIImplFontEnum> &enumFonts = *(vector<GDIImplFontEnum> *)(lParam);
  /*string we = "we: " + itos(logFont->lfWeight);
  string wi = "wi: " + itos(metric->tmAveCharWidth);
  string he = "he: " + itos(metric->tmHeight);
  string info = string(logFont->lfFaceName) + ", " + we + ", " + wi + ", " + he;*/
  enumFonts.push_back(GDIImplFontEnum());
  GDIImplFontEnum &f = enumFonts.back();
  f.face = logFont->lfFaceName;
  f.height = metric->tmHeight;
  f.width = metric->tmAveCharWidth;
  f.relScale = ((double(metric->tmHeight) / double(metric->tmAveCharWidth)) * 14.0/36.0);
  return 1;
}

void gdioutput::getEnumeratedFonts(vector< pair<wstring, size_t> > &output) const {
  if (enumeratedFonts.empty()) {
    HDC hDC = GetDC(hWndTarget);
//    EnumFontFamilies(hDC, NULL, enumFontProc, LPARAM(&enumeratedFonts));
    LOGFONT logFont;
    memset(&logFont, 0, sizeof(LOGFONT));
    logFont.lfCharSet = DEFAULT_CHARSET;
    EnumFontFamiliesEx(hDC, &logFont, enumFontProc, LPARAM(&enumeratedFonts), 0);
    ReleaseDC(hWndTarget, hDC);
  }
  output.resize(enumeratedFonts.size());
  for (size_t k = 0; k<output.size(); k++) {
    output[k].first = enumeratedFonts[k].getFace();
    output[k].second = k;
  }
}

double gdioutput::getRelativeFontScale(gdiFonts font, const wchar_t *fontFace) const {
  double sw = scale * 5.2381; //MeOS default assums this//getCurrentFont().getAvgFontWidth(*this, normalText);
  double other;
  if (fontFace == 0 || fontFace[0] == 0)
    other = getCurrentFont().getAvgFontWidth(*this, font);
  else
    other = getFont(fontFace).getAvgFontWidth(*this, font);
  return other/sw;
}

double GDIImplFontSet::getAvgFontWidth(const gdioutput &gdi, gdiFonts font) const {
  if (avgWidthCache.empty())
    avgWidthCache.resize(16, 0);

  if (size_t(font) > avgWidthCache.size())
    throw meosException("Internal font error");

  if (avgWidthCache[font] == 0) {
    TextInfo ti;
    ti.xp = 0;
    ti.yp = 0;
    ti.format = font;
    ti.text = L"Goliat Meze 1234:5678";
    ti.font = gdiName;
    gdi.calcStringSize(ti);
    avgWidthCache[font] = double(ti.textRect.right) / double(ti.text.length());

  }
  return avgWidthCache[font];
}

const wstring &gdioutput::getFontName(int id) {

  return _EmptyWString;
}

GDIImplFontEnum::GDIImplFontEnum() {
  relScale = 1.0;
}

GDIImplFontEnum::~GDIImplFontEnum() {
}

/*
FontEncoding interpetEncoding(const string &enc) {
  if (enc == "RUSSIAN")
    return Russian;
  else if (enc == "EASTEUROPE")
    return EastEurope;
  else if (enc == "HEBREW")
    return Hebrew;
  else
    return ANSI;
}*/


const wstring &gdioutput::recodeToWide(const string &input) {
  wstring &output = StringCache::getInstance().wget();
  int cp = defaultCodePage;
 // if (defaultCodePage > 0)
 //   cp = defaultCodePage;

  /*switch(getEncoding()) {
    case Russian:
      cp = 1251;
      break;
    case EastEurope:
      cp = 1250;
      break;
    case Hebrew:
      cp = 1255;
      break;
  }*/

  if (input.empty()) {
    output = L"";
    return output;
  }
  output.reserve(input.size()+1);
  output.resize(input.size(), 0);
  MultiByteToWideChar(cp, MB_PRECOMPOSED, input.c_str(), input.size(), &output[0], output.size() * sizeof(wchar_t));
  return output;
}

const string &gdioutput::recodeToNarrow(const wstring &input) {
  string &output = StringCache::getInstance().get();
  int cp = defaultCodePage;
 // if (defaultCodePage > 0)
 //   cp = defaultCodePage;

  /*switch(getEncoding()) {
    case Russian:
      cp = 1251;
      break;
    case EastEurope:
      cp = 1250;
      break;
    case Hebrew:
      cp = 1255;
      break;
  }*/

  if (input.empty()) {
    output = "";
    return output;
  }
  int res = input.size() * 3 + 2;
  output.reserve(res);
  output.resize(input.size(), 0);
  BOOL usedDef = false;
  int ok = WideCharToMultiByte(cp, 0, input.c_str(), input.size(), &output[0], res, "?", &usedDef);

  return output;
}

void gdioutput::setListDescription(const wstring &desc) {
  listDescription = desc;
}

InputInfo &InputInfo::setFont(gdioutput &gdi, gdiFonts font) {
  SendMessage(hWnd, WM_SETFONT, (WPARAM) gdi.getCurrentFont().getFont(font), 0);
  return *this;
}

void gdioutput::copyToClipboard(const string &html, const wstring &txt) const {

  if (OpenClipboard(getHWNDMain()) != false) {
    EmptyClipboard();

    size_t len = html.length() + 1;
    const char *output = html.c_str();
    
    const char cbd[]=
      "Version:0.9\n"
      "StartHTML:%08u\n"
      "EndHTML:%08u\n"
      "StartFragment:%08u\n"
      "EndFragment:%08u\n";

    char head[256];
    sprintf_s(head, cbd, 1,0,0,0);

    int offset=strlen(head);
 
    //Fill header with relevant information
    int ho_start = offset;
    int ho_end = offset + len;
    sprintf_s(head, cbd, offset,offset+len,ho_start,ho_end);

    HANDLE hMem=GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE, offset+len);
    LPVOID data=GlobalLock(hMem);

    memcpy(LPSTR(data), head, offset);
    memcpy(LPSTR(data)+offset, output, len);
    
    GlobalUnlock(hMem);

    // Text format
    //HANDLE hMemText = 0;
    HANDLE hMemTextWide = 0;

    if (txt.length() > 0) {
      size_t siz = txt.length();
      hMemTextWide = GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE, siz * sizeof(wchar_t));
      LPVOID dataText = GlobalLock(hMemTextWide);
      memcpy(LPSTR(dataText), txt.c_str(), siz * sizeof(wchar_t));
      GlobalUnlock(hMemTextWide);


      /*
      hMemText = GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE, txt.length()+1);
      LPVOID dataText=GlobalLock(hMemText);
      memcpy(LPSTR(dataText), txt.c_str() , txt.length()+1);
      GlobalUnlock(hMemText);*/
    }
    else {
      // HTML table to text
      std::ostringstream result;
      bool started = false;
      bool newline = false;
      bool dowrite = false;
      for (size_t k = 0; k + 3 < html.size(); k++) {
        if (html[k] == '<') {
          if (html[k+1] == 't') {
            if (html[k+2] == 'r') {
              newline = true;
              if (started)
               result << "\r\n";
            }
            else if (html[k+2] == 'd') {
              if (!newline)
                result << "\t";
              started = true;
              newline = false;
              dowrite = true;
            }
          }
          else if (html[k+1] == '/') {
            if (html[k+2] == 't' && html[k+3] == 'd') {
              dowrite = false;
            }
          }
          while (k < html.size() && html[k] != '>')
            k++;
        }
        else {
          if (dowrite)
            result << html[k];
        }
      }

      string atext = decodeXML(result.str());
/*      result.flush();

      for (size_t k = 0; k < atext.size(); k++) {
        if (atext[k] == '&') {
          size_t m = 0;
          while ((k+m) < atext.size() && atext[k+m] != ';')
            m++;

          if ((k+m) < atext.size() && atext[k+m] == ';') {
            string cmd = atext.substr(k, m-k);
            if (cmd == "nbsp")
              result << " ";
            else if (cmd == "amp")
              result << " ";
            else if (cmd == "lt")
              result << "<";
            else if (cmd == "gt")
              result << ">";
            else if (cmd == "quot")
              result << "\"";
            
            k += m;
          }
        }
        else 
          result << atext[k];

      }

      atext = result.str();
*/
      if (atext.size() > 0) {
        wstring atextw;
        int osize = atext.size();
        atextw.resize(osize + 1, 0);
        size_t siz = atextw.size();
        MultiByteToWideChar(CP_UTF8, 0, atext.c_str(), -1, &atextw[0], siz * sizeof(wchar_t));
        hMemTextWide = GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE, siz * sizeof(wchar_t));
        LPVOID dataText = GlobalLock(hMemTextWide);
        memcpy(LPSTR(dataText), atextw.c_str(), siz * sizeof(wchar_t));
        GlobalUnlock(hMemTextWide);
      }
    }
    UINT CF_HTML = RegisterClipboardFormat(L"HTML format");
    SetClipboardData(CF_HTML, hMem);
    
    if (hMemTextWide != 0) {
      SetClipboardData(CF_UNICODETEXT, hMemTextWide);
    }
    CloseClipboard();
  }
}

Recorder &gdioutput::getRecorder() {
  if (recorder.first == 0) {
    recorder.first = new Recorder();
    recorder.second = true;
  }
  return *recorder.first;
}

void gdioutput::initRecorder(Recorder *rec) {
  if (recorder.second)
    delete recorder.first;

  recorder.first = rec;
  recorder.second = false;
}

string gdioutput::dbPress(const string &id, int extra) {
  bool notEnabled = false;
  for (list<ButtonInfo>::iterator it=BI.begin(); it != BI.end(); ++it) {
    if (id==it->id && (extra == -65536 || extra == it->getExtraInt())) {
      
      if (!IsWindowEnabled(it->hWnd)) {
        notEnabled = true;
        continue;
      }
        
      if (it->isCheckbox) {
        check(id, !isChecked(id));
      }
      else if(!it->callBack && !it->hasEventHandler())
        throw meosException("Button " + id + " is not active.");

      wstring val = it->text;
      if (it->hasEventHandler())
        it->handleEvent(*this, GUI_BUTTON);
      else if (it->callBack)
        it->callBack(this, GUI_BUTTON, &*it); //it may be destroyed here...
      return toUTF8(val);
    }
  }
  if (notEnabled)
    throw meosException("Button " + id + " is not active.");
      
  throw meosException("Unknown command " + id + ".");
}

string gdioutput::dbPress(const string &id, const char *extra) {
  wstring eid = widen(extra ? extra : "");
  for (list<ButtonInfo>::iterator it=BI.begin(); it != BI.end(); ++it) {
    if (id==it->id && (!extra || (it->isExtraString() && eid == it->getExtra()))) {
      
      if (!IsWindowEnabled(it->hWnd))
        throw meosException("Button " + id + " is not active.");
      
      if (it->isCheckbox) {
        check(id, !isChecked(id));
      }
      else if(!it->callBack && !it->hasEventHandler())
        throw meosException("Button " + id + " is not active.");

      wstring val = it->text;
      if (it->hasEventHandler())
        it->handleEvent(*this, GUI_BUTTON);
      else if (it->callBack)
        it->callBack(this, GUI_BUTTON, &*it); //it may be destroyed here...
      return toUTF8(val);
    }
  }
  throw meosException(L"Unknown command " + widen(id) + L"/" + eid + L".");
}


string gdioutput::dbSelect(const string &id, int data) {

  for (list<ListBoxInfo>::iterator it = LBI.begin(); it != LBI.end(); ++it) {
    if (id==it->id) {
      if (!IsWindowEnabled(it->hWnd))
        throw meosException("Selection " + id + " is not active.");
      if (it->multipleSelection) {
        auto res = it->data2Index.find(data);
        if (res != it->data2Index.end())
          SendMessage(it->hWnd, LB_SETSEL, true, res->second);
        else
          throw meosException("List " + id + " does not contain value " + itos(data) + ".");
      }
      else {
        size_t origIdx = it->originalIdx;
        wstring orig = it->original;
        if (!selectItemByData(id, data))
          throw meosException("List " + id + " does not contain value " + itos(data) + ".");
        it->original = orig;
        it->originalIdx = origIdx;
      }
      UpdateWindow(it->hWnd);
      wstring res = it->text;
      internalSelect(*it);
      return toUTF8(res);
    }
  }
  throw meosException("Unknown selection " + id + ".");
}

void gdioutput::internalSelect(ListBoxInfo &bi) {
  bi.syncData();
  if (bi.callBack || bi.handler || bi.managedHandler) {
    setWaitCursor(true);
    hasCleared = false;
    try {
      bi.writeLock = true;
      if (bi.hasEventHandler())
        bi.handleEvent(*this, GUI_LISTBOX);
      else
        bi.callBack(this, GUI_LISTBOX, &bi); //it may be destroyed here... Then hasCleared is set.
    }
    catch(...) {
      if (!hasCleared)
        bi.writeLock = false;
      setWaitCursor(false);
      throw;
    }
    if (!hasCleared)
      bi.writeLock = false;
    setWaitCursor(false);
  }
}

void gdioutput::dbInput(const string &id, const string &text) {
  for (list<ListBoxInfo>::iterator it = LBI.begin(); it != LBI.end(); ++it) {
    if (id==it->id) {
      if (!IsWindowEnabled(it->hWnd) || !it->IsCombo)
        throw meosException("Selection " + id + " is not active.");

      SendMessage(it->hWnd, CB_SETCURSEL, -1, 0);
      SetWindowText(it->hWnd, widen(text).c_str());
      it->text = widen(text);
      it->data = -1;
      if (it->hasEventHandler())
        it->handleEvent(*this, GUI_COMBO);
      else if (it->callBack)
        it->callBack(this, GUI_COMBO, &*it); //it may be destroyed here...
      return;
    }
  }

  for (list<InputInfo>::iterator it = II.begin(); it != II.end(); ++it) {
    if (id == it->id) {
      if (!IsWindowEnabled(it->hWnd))
        throw meosException("Input " + id + " is not active.");

      it->text = widen(text);
      SetWindowText(it->hWnd, widen(text).c_str());
      if (it->hasEventHandler())
        it->handleEvent(*this, GUI_INPUT);
      else if (it->callBack)
        it->callBack(this, GUI_INPUT, &*it);
      return;
    }
  }

  throw meosException("Unknown input " + id + ".");
}

void gdioutput::dbCheck(const string &id, bool state) {

}

string gdioutput::dbClick(const string &id, int extra) {
  for (list<TextInfo>::iterator it = TL.begin(); it != TL.end(); ++it) {
    if (it->id == id && (extra == -65536 || it->getExtraInt() == extra)) {
      if (it->callBack || it->hasEventHandler()) {
        string res = toUTF8(it->text);
        if (!it->handleEvent(*this, GUI_LINK))
          it->callBack(this, GUI_LINK, &*it);
        return res;
      }
      else
        throw meosException("Link " + id + " is not active.");
    }
  }
  
  throw meosException("Unknown link " + id + ".");
}

void gdioutput::dbDblClick(const string &id, int data) {
  for (list<ListBoxInfo>::iterator it = LBI.begin(); it != LBI.end(); ++it) {
    if (id==it->id) {
      if (!IsWindowEnabled(it->hWnd))
        throw meosException("Selection " + id + " is not active.");
      selectItemByData(id, data);
      if (it->hasEventHandler())
        it->handleEvent(*this, GUI_LISTBOXSELECT);
      else if (it->callBack)
        it->callBack(this, GUI_LISTBOXSELECT, &*it); //it may be destroyed here...
      return;
    }
  }
  throw meosException("Unknown selection " + id + ".");
}

// Add the next answer for a dialog popup
void gdioutput::dbPushDialogAnswer(const string &answer) {
  cmdAnswers.push_back(answer);
}

void gdioutput::clearDialogAnswers(bool checkEmpty) {
  if (!cmdAnswers.empty()) {
    string front = cmdAnswers.front();
    cmdAnswers.clear();
    if (checkEmpty)
      throw meosException("Pending answer: X#" + front);
  }
}

int gdioutput::dbGetStringCount(const string &str, bool subString) const {
  int count = 0;
  wstring wstr = widen(str);
  for (list<TextInfo>::const_iterator it = TL.begin(); it != TL.end(); ++it) {
    if (subString == false) {
      if (it->text == wstr)
        count++;
    }
    else {
      size_t off = 0;
      while(off < it->text.size()) {
        off = it->text.find(wstr, off);
        if (off != string::npos) {
          count++;
          off++;
        }
        else
          break;
      }
    }
  }
  return count;
}

void gdioutput::dbRegisterSubCommand(const SubCommand *cmd, const string &action) {
  if (cmd == 0)
    subCommands.clear();
  else
    subCommands.push_back(make_pair(cmd, action));
}

void gdioutput::runSubCommand() {
  if (!subCommands.empty()) {
    auto cmd = subCommands.back();
    subCommands.pop_back();
    cmd.first->subCommand(cmd.second);
  }
}

void gdioutput::getWindowsPosition(RECT &rc) const {
  WINDOWPLACEMENT wpl;
  memset(&wpl, 0, sizeof(WINDOWPLACEMENT));
  wpl.length = sizeof(WINDOWPLACEMENT);
  GetWindowPlacement(hWndAppMain, &wpl);
  rc = wpl.rcNormalPosition;
}

void gdioutput::setWindowsPosition(const RECT &rc) {
  WINDOWPLACEMENT wpl;
  memset(&wpl, 0, sizeof(WINDOWPLACEMENT));
  wpl.length = sizeof(WINDOWPLACEMENT);
  wpl.rcNormalPosition = rc;
  wpl.showCmd = SW_SHOWNORMAL;
  SetWindowPlacement(hWndAppMain, &wpl);
}

void gdioutput::getVirtualScreenSize(RECT &rc) {
  int px = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  if (px < 10 || px > 100000)
    px = GetSystemMetrics(SM_CXSCREEN);

  int py = GetSystemMetrics(SM_CYVIRTUALSCREEN);
  if (py < 10 || py > 100000)
    py = GetSystemMetrics(SM_CYSCREEN);

  rc.left = 0;
  rc.right = px;
  rc.top = 0;
  rc.bottom = py;
}

DWORD gdioutput::selectColor(wstring &def, DWORD input) {
  CHOOSECOLOR cc;
  memset(&cc, 0, sizeof(cc));
  cc.lStructSize = sizeof(cc);
  cc.hwndOwner = getHWNDMain();
  cc.rgbResult = COLORREF(input);
  if (GDICOLOR(input) != colorDefault)
    cc.Flags |= CC_RGBINIT;

  COLORREF staticColor[16];
  memset(staticColor, 0, 16 * sizeof(COLORREF));

  const wchar_t *end = def.c_str() + def.length();
  const wchar_t * pEnd = def.c_str();
  int pix = 0;
  while (pEnd < end && pix < 16) {
    staticColor[pix++] = wcstol(pEnd, (wchar_t **)&pEnd, 16);
  }

  cc.lpCustColors = staticColor;
  int res = 0;
  setCommandLock();
  try {
    res = ChooseColor(&cc);
    liftCommandLock();
  }
  catch (...) {
    liftCommandLock();
    throw;
  }

  if (res) {
    wstring co;
    for (int ix = 0; ix < 16; ix++) {
      wchar_t bf[16];
      swprintf_s(bf, L"%x ", staticColor[ix]);
      co += bf;
    }
    swap(def,co);
    return cc.rgbResult;
  }
  return -1;
}

void gdioutput::setAnimationMode(const shared_ptr<AnimationData> &data) {
  if (animationData && animationData->takeOver(data))
    return;
  animationData = data;
}

namespace {
  BOOL CALLBACK enumMonitors(HMONITOR hMonitor, HDC hDC, LPRECT rect, LPARAM gdiObj) {
    gdioutput* gdi = reinterpret_cast<gdioutput*>(gdiObj);
    gdi->addMonitorRect(*rect);
    return true;
  }
}

void gdioutput::updateMonitorConfiguration() {
  monitorConfiguration.clear();
  EnumDisplayMonitors(NULL, NULL, enumMonitors, LPARAM(this));

  if (monitorConfiguration.size() > 0) {
    RECT rc;
    GetWindowRect(hWndAppMain, &rc);
    double showArea = 0;
    for (auto& mRC : monitorConfiguration) {
      RECT dst;
      IntersectRect(&dst, &mRC, &rc);
      double area = fabs(dst.right - dst.left) * fabs(dst.bottom - dst.top);
      showArea += area;
    }

    double totArea = fabs(rc.right - rc.left) * fabs(rc.bottom - rc.top);

    if (showArea < 0.33 * totArea) {
      HWND hDskTop = GetDesktopWindow();
      GetClientRect(hDskTop, &rc);

      // Out of bounds, just use default position and size
      int xp = 50;
      int yp = 20;
      int xs = max(850, min<int>(int(rc.right) - yp, (rc.right * 9) / 10));
      int ys = max(650, min<int>(int(rc.bottom) - yp - 40, (rc.bottom * 8) / 10));
      SetWindowPos(hWndAppMain, NULL, xp, yp, xs, ys, SWP_NOZORDER);
    }

  }
}

AutoCompleteInfo &gdioutput::addAutoComplete(const string &key) {
  BaseInfo &bi = getBaseInfo(key.c_str());
  RECT rc, rcMain;
  GetWindowRect(bi.getControlWindow(), &rc);
  GetWindowRect(hWndTarget, &rcMain);
  POINT pt;
  int height = scaleLength(200);
  pt.x = rc.right;
  //pt.y = min(rc.top, rcMain.bottom-height);
  pt.y = rc.bottom;
  if (pt.y + height > rcMain.bottom)
    pt.y = rc.top - height;
  
  ScreenToClient(hWndTarget, &pt);
  if (pt.y < 0) { //Fallback
    pt.x = rc.right;
    pt.y = min(rc.top, rcMain.bottom - height);
    ScreenToClient(hWndTarget, &pt);
  }

  if (autoCompleteInfo && autoCompleteInfo->matchKey(key)) {
    return *autoCompleteInfo;
  }

  autoCompleteInfo.reset();

  HWND hWnd = CreateWindowEx(WS_EX_CLIENTEDGE, L"AUTOCOMPLETE", L"", WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS| WS_BORDER ,
    pt.x, pt.y, scaleLength(350), height, hWndTarget, NULL,
    (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE), NULL);

  autoCompleteInfo.reset(new AutoCompleteInfo(hWnd, key, *this));
  
  //SendMessage(hWnd, WM_SETFONT, (WPARAM)getGUIFont(), 0);

  return *autoCompleteInfo;
}

void gdioutput::clearAutoComplete(const string &key) {
  autoCompleteInfo.reset();
}

int gdioutput::getPageY() const {
  if (hideBG || backgroundColor1 != -1)
    return max(MaxY, 100);
  else
    return max(MaxY, 100) + scaleLength(60); 
}

int gdioutput::getPageX() const { 
  int xlimit = 100;
  for (auto &b : BI)
    xlimit = max(b.xp + b.width, xlimit);

  if (hideBG || backgroundColor1 != -1 || xlimit >= MaxX)
    return max(MaxX, xlimit);
  else
    return max(MaxX, xlimit) + scaleLength(60); 
}

int gdioutput::popupMenu(int x, int y, const vector<pair<wstring, int>> &menuItems) const {
  POINT pt;
  pt.x = x;
  pt.y = y;
  ClientToScreen(getHWNDTarget(), &pt);
  HMENU hm = CreatePopupMenu();
  for (auto &me : menuItems) {
    if (me.first.empty())
      AppendMenu(hm, MF_SEPARATOR, me.second, L"");
    else
      AppendMenu(hm, MF_STRING, me.second, lang.tl(me.first).c_str());
  }
  int res = TrackPopupMenuEx(hm, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_NONOTIFY,
                             pt.x, pt.y, getHWNDTarget(), nullptr);

  DestroyMenu(hm);
  return res;
}

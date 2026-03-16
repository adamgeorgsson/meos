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

// gdioutputState.cpp: State management, scrolling, dialogs, toolbar
//
//////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "gdioutput.h"
#include "gdiconstants.h"
#include "meosexception.h"
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <objbase.h>
#include <shlobj.h>
#include "meos_util.h"
#include "Table.h"
#include "localizer.h"
#include "toolbar.h"
#include "gdiimpl.h"
#include "Printer.h"
#include "recorder.h"
#include "autocomplete.h"

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

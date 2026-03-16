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

// gdioutputDraw.cpp: Drawing, rendering, and list-box management
//
//////////////////////////////////////////////////////////////////////
#define _USE_MATH_DEFINES
#include "StdAfx.h"
#include "gdioutput.h"
#include "gdiconstants.h"
#include "meosexception.h"
#include "process.h"
#include <commctrl.h>
#include <cmath>
#include <sstream>
#include "meos_util.h"
#include "Table.h"
#include "localizer.h"
#include "TabBase.h"
#include "toolbar.h"
#include "gdiimpl.h"
#include "animationData.h"
#include "image.h"
#include "autocomplete.h"
#include "maprenderer.h"

extern Image image;

//#define DEBUGRENDER

#ifdef DEBUGRENDER
  static int counterRender = 0;
  static bool breakRender = false;
  static int debugDrawColor = 0;
#endif

void gdioutput::draw(HDC hDC, RECT& rc, RECT& drawArea) {
#ifdef DEBUGRENDER
  if (debugDrawColor) {
    string ds = "DebugDraw" + itos(drawArea.left) + "-" + itos(drawArea.right) + ", " + itos(drawArea.top) + "-" + itos(drawArea.bottom) + "\n";
    OutputDebugString(ds.c_str());
    SelectObject(hDC, GetStockObject(DC_BRUSH));
    SetDCBrushColor(hDC, debugDrawColor);
    Rectangle(hDC, rc.left, rc.top, rc.right, rc.bottom);
    return;
  }
#endif
  if (highContrast)
    drawBackground(hDC, drawArea);
  else
    drawBackground(hDC, rc);

  if (drawArea.left > MaxX - OffsetX + 15) {
    drawBoxes(hDC, rc);
    return;
  }

  if (animationData) {
    int page = 0;
    animationData->renderPage(hDC, *this, GetTickCount64());
    return;
  }

  SelectObject(hDC, GetStockObject(DC_BRUSH));

  for (auto& rit : Rectangles)
    renderRectangle(hDC, 0, rit);

  if (useTables)
    for (list<TableInfo>::iterator tit = Tables.begin(); tit != Tables.end(); ++tit) {
      tit->table->draw(*this, hDC, tit->xp, tit->yp, rc);
    }

  resetLast();
  TIList::iterator it;

  int BoundYup = OffsetY - maxTextBlockHeight - 2 + drawArea.top;
  int BoundYupTight = OffsetY - 2 + drawArea.top;
  int BoundYdown = OffsetY + drawArea.bottom + 2;

  for (auto imgTL : imageReferences) {
    RenderString(*imgTL, hDC);
  }

  if (renderMap)
    renderMap->renderDecoration(hDC, *this);

  if (!renderOptimize || itTL == TL.end()) {
#ifdef DEBUGRENDER
    //if (breakRender)
    //  DebugBreak();
    OutputDebugString(("Raw render" + itos(size_t(this)) + "\n").c_str());
#endif
    for (it = TL.begin(); it != TL.end(); ++it) {
      TextInfo& ti = *it;
      if ((ti.format & 0xFF) == textImage)
        continue;
      if ((ti.yp > BoundYup || ti.textRect.bottom > BoundYupTight) && ti.yp < BoundYdown)
        RenderString(*it, hDC);
    }
  }
  else {
#ifdef DEBUGRENDER
    OutputDebugString((itos(++counterRender) + " opt render " + itos(size_t(this)) + "\n").c_str());
#endif

    while (itTL != TL.end() && itTL->yp < BoundYup)
      ++itTL;

    if (itTL != TL.end())
      while (itTL != TL.begin() && itTL->yp > BoundYup)
        --itTL;

    it = itTL;
    while (it != TL.end() && it->yp < BoundYdown) {
      if ((it->format & 0xFF) != textImage)
        RenderString(*it, hDC);
      ++it;
    }
  }

  updateStringPosCache();
  drawBoxes(hDC, rc);
}

void gdioutput::renderRectangle(HDC hDC, RECT *clipRegion, const RectangleInfo &ri) {
  if (ri.drawBorder) {
    SelectObject(hDC, GetStockObject(DC_PEN));
    SetDCPenColor(hDC, RGB(40,40,60));
  }
  else
    SelectObject(hDC, GetStockObject(NULL_PEN));
  
  if (ri.color == colorTransparent) 
    SelectObject(hDC, GetStockObject(NULL_BRUSH));
  else {
    SetDCBrushColor(hDC, ri.color);
  }
  RECT rect_rc=ri.rc;
  OffsetRect(&rect_rc, -OffsetX, -OffsetY);
  if (rect_rc.left == rect_rc.right || rect_rc.top == rect_rc.bottom) {
    MoveToEx(hDC, rect_rc.left, rect_rc.top, nullptr);
    LineTo(hDC, rect_rc.right, rect_rc.bottom);
  }
  else {
    Rectangle(hDC, rect_rc.left, rect_rc.top, rect_rc.right, rect_rc.bottom);
  }
  if (ri.color == colorTransparent)
    SelectObject(hDC, GetStockObject(DC_BRUSH));
}

void gdioutput::updateStringPosCache() {
  RECT rc;
  GetClientRect(hWndTarget, &rc);
  int BoundYup = OffsetY-100;
  int BoundYdown = OffsetY+rc.bottom+10;
  shownStrings.clear();
  TIList::iterator it;

  if (!renderOptimize || itTL == TL.end()) {
    for (it=TL.begin();it!=TL.end(); ++it) {
      TextInfo &ti=*it;
      if ( ti.yp > BoundYup && ti.yp < BoundYdown) {
        if (ti.textRect.top != ti.yp - OffsetY) {
          int diff = it->textRect.top - (ti.yp - OffsetY);
          ti.textRect.top -= diff;
          ti.textRect.bottom -= diff;
        }
        shownStrings.push_back(&ti);
      }
    }
  }
  else {
    TIList::iterator itC = itTL;

    while( itC != TL.end() && itC->yp < BoundYup)
      ++itC;

    if (itC!=TL.end())
      while( itC != TL.begin() && itC->yp > BoundYup)
        --itC;

    it=itC;
    while( it != TL.end() && it->yp < BoundYdown) {
      shownStrings.push_back(&*it);
      if (it->textRect.top != it->yp - OffsetY) {
        int diff = it->textRect.top - (it->yp - OffsetY);
        it->textRect.top -= diff;
        it->textRect.bottom -= diff;
      }
      ++it;
    }
  }
}

TextInfo& gdioutput::addTimer(int yp, int xp, int format, int zeroTime, const wstring &textFormat, 
                              int xlimit, GUICALLBACK cb, int timeOut, const wchar_t* fontFace) {
  hasAnyTimer = true;
  int64_t signedTime = 1000 * zeroTime;
  uint64_t zt = GetTickCount64() - signedTime;
  wstring text = getTimerText(zeroTime, format, true, textFormat);

  addStringUT(yp, xp, format, text, xlimit, cb, fontFace);
  TextInfo& ti = TL.back();
  ti.hasTimer = true;
  ti.zeroTime = zt;
  ti.timerFormat = textFormat;
  if (timeOut != NOTIMEOUT)
    ti.timeOut = ti.zeroTime + timeOut * 1000;

  return ti;
}

TextInfo& gdioutput::addTimeout(int TimeOut, GUICALLBACK cb) {
  addStringUT(0, 0, 0, "", 0, cb);
  TextInfo& ti = TL.back();
  ti.hasTimer = true;
  ti.zeroTime = GetTickCount64();
  if (TimeOut != NOTIMEOUT)
    ti.timeOut = ti.zeroTime + TimeOut * 1000;
  return ti;
}

void CALLBACK gdiTimerProc(HWND hWnd, UINT a, UINT_PTR ptr, DWORD b) {
  wstring msg;
  KillTimer(hWnd, ptr);
  TimerInfo *it = (TimerInfo *)ptr;
  it->setWnd = 0;
  try {
    if (it->parent) {
      it->parent->timerProc(*it, b);
    }
  }
  catch (const meosCancel&) {
    return;
  }
  catch (meosException &ex) {
    msg = ex.wwhat();
  }
  catch(std::exception &ex) {
    string2Wide(ex.what(), msg);
    if (msg.empty())
      msg = L"Ett okänt fel inträffade.";
  }
  catch(...) {
    msg = L"Unexpected error";
  }

  if (!msg.empty()) {
    MessageBox(hWnd, msg.c_str(), L"MeOS", MB_OK|MB_ICONEXCLAMATION);
  }
}

int TimerInfo::globalTimerId = 0;

void gdioutput::timerProc(TimerInfo &timer, DWORD timeout) {
  int timerId = timer.timerId;
  if (timer.handler)
    timer.handler->handle(*this, timer, GUI_TIMER);
  if (timer.managedHandler)
    timer.managedHandler->handle(*this, timer, GUI_TIMER);
  else if (timer.callBack)
    timer.callBack(this, GUI_TIMER, &timer);

  for (auto it = timers.begin(); it != timers.end(); ++it) {
    if (it->getId() == timerId) {
      timers.erase(it);
      break;
    }
  }  

  //timers.erase(remove_if(timers.begin(), timers.end(), [timerId](TimerInfo &x) {return x.getId() == timerId; }), timers.end());
}

void gdioutput::removeHandler(GuiHandler *h) {
  for (auto &it : timers) {
    if (it.handler == h)
      it.handler = 0;
  }

  for (auto &it : BI) {
    if (it.handler == h)
      it.handler = 0;
  }


  for (auto &it : II) {
    if (it.handler == h)
      it.handler = 0;
  }
  
  for (auto &it : TL) {
    if (it.handler == h)
      it.handler = 0;
  }

  for (auto &it : LBI) {
    if (it.handler == h)
      it.handler = 0;
  }
}

void gdioutput::removeTimeoutMilli(const string &id) {
  for (list<TimerInfo>::iterator it = timers.begin(); it != timers.end(); ++it) {
    if (it->id == id) {
      timers.erase(it);
      return;
    }
  }
}

TimerInfo &gdioutput::addTimeoutMilli(int timeOut, const string &id, GUICALLBACK cb)
{
  removeTimeoutMilli(id);
  timers.emplace_back(this, cb);
  timers.back().id = id;
  SetTimer(hWndTarget, (UINT_PTR)&timers.back(), timeOut, gdiTimerProc);
  timers.back().setWnd = hWndTarget;
  return timers.back();
}

TimerInfo:: ~TimerInfo() {
  handler = 0;
  callBack = 0;
  if (setWnd)
    KillTimer(setWnd, (UINT_PTR)this);
}

TextInfo& gdioutput::addImage(const string& id, int yp, int xp, int format, 
                              const wstring& imageId, int width, int height,
                              int offsetX, int offsetY,
                              int srcWidth, int srcHeight,
                              GUICALLBACK cb) {
  bool skipBBCalc = (format & skipBoundingBox) == skipBoundingBox;
  format &= ~skipBoundingBox;

  int oldYP = TL.empty() ? -1 : TL.back().yp;
  TL.emplace_back();
  TextInfo& TI = TL.back();
  itTL = TL.begin();

  imageReferences.push_back(&TI);

  TI.id = id;
  TI.format = format | textImage;
  TI.text = L"L" + imageId;
  TI.callBack = cb;

  uint64_t imgId = _wcstoui64(imageId.c_str(), nullptr, 10);
  int rwidth = image.getWidth(imgId);
  int rheight = image.getHeight(imgId);
  
  if (width == 0 || height == 0) {
    if (width == 0 && height == 0) {
      width = rwidth;
      height = rheight;
    }
    else if (height == 0)
      height = (width * rheight) / rwidth;
      else
      width = (height * rwidth) / rheight;
  }
  
   double scaleX = double(height) / double(rheight), scaleY = double(width)/double(rwidth);

  TI.srcRect.left = offsetX;
  /*if (offsetX < 0) {
    TI.srcRect.left = 0;
    xp -= offsetX * scaleX;
    width += offsetX * scaleX;
  }*/

  TI.srcRect.top = offsetY;
  if(srcWidth < 0)
    TI.srcRect.right = offsetX + rwidth;
  else
    TI.srcRect.right = offsetX + srcWidth;

  if (srcHeight < 0)
    TI.srcRect.bottom = offsetY + rheight;
  else
    TI.srcRect.bottom = offsetY + srcHeight;


  /*if (offsetY < 0) {
    TI.srcRect.top = 0;
    yp -= offsetY * scaleY;
    height += offsetY * scaleY;
  }*/
  
/*  if (srcWidth < 0)
    TI.srcRect.right = rwidth;
  else {
    TI.srcRect.right = offsetX + srcWidth;
    if (TI.srcRect.right > rwidth) {
      int extraX = TI.srcRect.right - rwidth;
      TI.srcRect.right = rwidth;
      width = max<int>(0, width - scaleX*extraX);// int(width * (double(srcWidth - extraX) / double(srcWidth)));
    }
  }

  if (srcHeight < 0)
    TI.srcRect.bottom = rheight;
  else {
    TI.srcRect.bottom = offsetY + srcHeight;
    if (TI.srcRect.bottom > rheight) {
      int extraY = TI.srcRect.bottom - rheight;
      TI.srcRect.bottom = rheight;
      height = max<int>(0, width - scaleY * extraY);// int(height * (double(srcHeight - extraY) / double(srcHeight)));
    }
  }
  */
  TI.xp = xp;
  TI.yp = yp;
  TI.textRect.left = xp;
  TI.textRect.top = yp;
  TI.textRect.right = xp + width;
  TI.textRect.bottom = yp + height;
  TI.realWidth = width;

  FlowDirection oldDir = flowDirection;

  if (format & imageNoUpdatePos)
    flowDirection = FlowDirection::None;

  updatePos(TI.xp, TI.yp, width + scaleLength(10),
            height + scaleLength(2));
  
  flowDirection = oldDir;

  if (oldYP > TI.yp)
    renderOptimize = false;
  
  return TL.back();
}

TextInfo* gdioutput::setImage(const string& id, int imgId, bool update) {
  return (TextInfo *)setText(id.c_str(), L"L" + itow(imgId), update);
}

TextInfo &gdioutput::addStringUT(int yp, int xp, int format, const string &text,
                                 int xlimit, GUICALLBACK cb, const wchar_t *fontFace) {
  return addStringUT(yp, xp, format, widen(text), xlimit, cb, fontFace);
}

int gdioutput::getFontHeight(int format, const wstring &fontFace) const {
  format = format & 0xFF;
  auto res = fontHeightCache.find(make_pair(format, fontFace));

  if (res != fontHeightCache.end())
    return res->second;

  TextInfo TI;
  TI.format = format;
  TI.xp = 0;
  TI.yp = 0;
  TI.text = L"M1y|";
  TI.xlimit = 100;
  TI.callBack = 0;
  TI.font = fontFace;
  calcStringSize(TI);
  int h = TI.textRect.bottom - TI.textRect.top;
  fontHeightCache.emplace(make_pair(format, fontFace), h);
  return h;
}

TextInfo& gdioutput::addStringUT(int yp, int xp, int format, const wstring& text,
  int xlimit, GUICALLBACK cb, const wchar_t* fontFace)
{
  bool skipBBCalc = (format & skipBoundingBox) == skipBoundingBox;
  format &= ~skipBoundingBox;
  int oldYP = TL.empty() ? -1 : TL.back().yp;

  TL.emplace_back();
  TextInfo& TI = TL.back();
  itTL = TL.begin();

  if ((format & 0xFF) == textImage)
    imageReferences.push_back(&TI);

  TI.format = format;
  TI.xp = xp;
  TI.yp = yp;
  TI.text = text;
  TI.xlimit = xlimit;
  TI.callBack = cb;
  if (fontFace)
    TI.font = fontFace;
  if (!skipTextRender(format)) {

    if (skipBBCalc) {
      assert(xlimit > 0);
      int h = getFontHeight(format, fontFace);
      TI.textRect.left = xp;
      TI.textRect.top = yp;
      TI.textRect.right = xp + xlimit;
      TI.textRect.bottom = yp + h;
      TI.realWidth = xlimit;

      updatePos(TI.xp, TI.yp, TI.realWidth + scaleLength(10),
        TI.textRect.bottom - TI.textRect.top + scaleLength(2));

      maxTextBlockHeight = max(maxTextBlockHeight, h + 1);
    }
    else {
      HDC hDC = GetDC(hWndTarget);

      if (hWndTarget && !manualUpdate)
        RenderString(TI, hDC);
      else
        calcStringSize(TI, hDC);

      if (xlimit == 0 || (format & (textRight | textCenter)) == 0) {
        updatePosTight(TI.textRect.left, TI.yp,
          TI.realWidth, TI.textRect.bottom - TI.textRect.top,
          scaleLength(10), scaleLength(2));
      }
      else {
        updatePosTight(TI.xp, TI.yp,
          TI.realWidth, TI.textRect.bottom - TI.textRect.top,
          scaleLength(10), scaleLength(2));
      }
      ReleaseDC(hWndTarget, hDC);
      maxTextBlockHeight = max<int>(maxTextBlockHeight, 1 + TI.textRect.bottom - TI.textRect.top);
    }

    if (oldYP > TI.yp)
      renderOptimize = false;
  }
  else {
    TI.textRect.left = xp;
    TI.textRect.right = xp;
    TI.textRect.bottom = yp;
    TI.textRect.top = yp;
  }

  return TL.back();
}

TextInfo &gdioutput::addString(const char *id, int yp, int xp, int format, const string &text,
                               int xlimit, GUICALLBACK cb, const wchar_t *fontFace)
{
  return addString(id, yp, xp, format, widen(text), xlimit, cb, fontFace);
}

TextInfo& gdioutput::addString(const char* id, int yp, int xp, int format, const wstring& text,
  int xlimit, GUICALLBACK cb, const wchar_t* fontFace)
{
  int oldYP = TL.empty() ? -1 : TL.back().yp;

  TL.emplace_back();
  itTL = TL.begin();
  TextInfo& TI = TL.back();

  if ((format & 0xFF) == textImage)
    imageReferences.push_back(&TI);

  TI.format = format;
  TI.xp = xp;
  TI.yp = yp;
  if ((format & 0xFF) != textImage) {
    TI.text = lang.tl(text);
    if ((format & Capitalize) == Capitalize && lang.capitalizeWords())
      capitalizeWords(TI.text);
  }
  else {
    TI.text = text;
  }
  TI.id = id;
  TI.xlimit = xlimit;
  TI.callBack = cb;
  if (fontFace)
    TI.font = fontFace;

  if (!skipTextRender(format)) {
    HDC hDC = GetDC(hWndTarget);

    if (hWndTarget && !manualUpdate)
      RenderString(TI, hDC);
    else
      calcStringSize(TI, hDC);

    if (xlimit == 0 || (format & (textRight | textCenter)) == 0) {
      updatePos(TI.textRect.right + OffsetX, yp, scaleLength(10),
        TI.textRect.bottom - TI.textRect.top + scaleLength(2));
    }
    else {
      updatePos(TI.xp, TI.yp, TI.realWidth + scaleLength(10),
        TI.textRect.bottom - TI.textRect.top + scaleLength(2));
    }
    ReleaseDC(hWndTarget, hDC);

    maxTextBlockHeight = max<int>(maxTextBlockHeight, TI.textRect.bottom - TI.textRect.top + 1);

    if (oldYP > TI.yp)
      renderOptimize = false;
  }
  else {
    TI.textRect.left = xp;
    TI.textRect.right = xp;
    TI.textRect.bottom = yp;
    TI.textRect.top = yp;
  }

  return TL.back();
}

TextInfo &gdioutput::addString(const string &id, int format, const string &text, GUICALLBACK cb) {
  return addString(id.c_str(), CurrentY, CurrentX, format, text, 0, cb);
}

TextInfo &gdioutput::addString(const string &id, int format, const wstring &text, GUICALLBACK cb) {
  return addString(id.c_str(), CurrentY, CurrentX, format, text, 0, cb);
}

TextInfo &gdioutput::addString(const string &id, int yp, int xp, int format, const string &text,
                               int xlimit, GUICALLBACK cb, const wchar_t *fontFace) {
  return addString(id.c_str(), yp, xp, format, text, xlimit, cb, fontFace);
}

TextInfo &gdioutput::addString(const string &id, int yp, int xp, int format, const wstring &text,
                               int xlimit, GUICALLBACK cb, const wchar_t *fontFace) {
  return addString(id.c_str(), yp, xp, format, text, xlimit, cb, fontFace);
}

TextInfo &gdioutput::addString(const char *id, int format, const string &text, GUICALLBACK cb)
{
  return addString(id, CurrentY, CurrentX, format, text, 0, cb);
}

TextInfo &gdioutput::addString(const char *id, int format, const wstring &text, GUICALLBACK cb)
{
  return addString(id, CurrentY, CurrentX, format, text, 0, cb);
}

TextInfo &gdioutput::addStringUT(int format, const string &text, GUICALLBACK cb)
{
  return addStringUT(CurrentY, CurrentX, format, text, 0, cb);
}

TextInfo &gdioutput::addStringUT(int format, const wstring &text, GUICALLBACK cb)
{
  return addStringUT(CurrentY, CurrentX, format, text, 0, cb);
}

ButtonInfo &gdioutput::addButton(const string &id, const string &text, GUICALLBACK cb,
                                 const string &tooltip)
{
  return addButton(CurrentX,  CurrentY, id, text, cb, tooltip);
}

ButtonInfo &gdioutput::addButton(const string &id, const wstring &text, GUICALLBACK cb,
                                 const wstring &tooltip)
{
  return addButton(CurrentX,  CurrentY, id, text, cb, tooltip);
}

ButtonInfo &gdioutput::addButton(int x, int y, const string &id, const string &text, GUICALLBACK cb,
                                 const string &tooltip)
{
  return addButton(x,y, id, widen(text), cb, widen(tooltip));
}

ButtonInfo& gdioutput::addButton(int x, int y, const string& id, const wstring& text, GUICALLBACK cb,
  const wstring& tooltip)
{
  HANDLE bm = 0;
  int width = 0;
  if (text[0] == '@') {
    HINSTANCE hInst = GetModuleHandle(0);
    int ir = _wtoi(text.c_str() + 1);
    bm = LoadBitmap(hInst, MAKEINTRESOURCE(ir));

    SIZE size;
    size.cx = 24;
    width = size.cx + 4;
  }
  else {
    SIZE size;
    HDC hDC = GetDC(hWndTarget);
    SelectObject(hDC, getGUIFont());
    wstring ttext = lang.tl(text);
    int tts = ttext.size();
    if (tts > 2 && ttext[0] == '<' && ttext[1] == '<') {
      ttext = L"◀" + ttext.substr(2);
    }
    else if (tts > 2 && ttext[tts - 1] == '>' && ttext[tts - 2] == '>') {
      ttext = ttext.substr(0, tts - 2) + L"▶";
    }
    if (lang.capitalizeWords())
      capitalizeWords(ttext);
    GetTextExtentPoint32(hDC, ttext.c_str(), ttext.length(), &size);
    ReleaseDC(hWndTarget, hDC);
    width = size.cx + scaleLength(30);
    if (text != L"...")
      width = max<int>(width, scaleLength(75));
  }

  ButtonInfo& bi = addButton(x, y, width, id, text, cb, tooltip, false, false);

  if (bm != 0) {
    SendMessage(bi.hWnd, BM_SETIMAGE, IMAGE_BITMAP, LPARAM(bm));
  }

  return bi;
}

ButtonInfo &ButtonInfo::setDefault()
{
  flags |= 1;
  storedFlags |= 1;
  //SetWindowLong(hWnd, i, GetWindowLong(hWnd, i)|BS_DEFPUSHBUTTON);
  return *this;
}

void ButtonInfo::moveButton(gdioutput &gdi, int nxp, int nyp) {
  xp = nxp;
  yp = nyp;
  int w, h;
  getDimension(gdi, w, h);
  MoveWindow(hWnd, xp, yp, w, h, true);
  gdi.updatePos(xp, yp, w, h);
}

void ButtonInfo::getDimension(const gdioutput &gdi, int &w, int &h) const {
  RECT rc;
  GetWindowRect(hWnd, &rc);
  w = rc.right - rc.left + gdi.scaleLength(GDI_BUTTON_SPACING);
  h = rc.bottom - rc.top;
}

ButtonInfo &gdioutput::addButton(int x, int y, int w, const string &id,
                                 const string &text, GUICALLBACK cb, const string &tooltip,
                                 bool AbsPos, bool hasState) {
  return addButton(x, y, w, id, widen(text), cb, widen(tooltip), AbsPos, hasState);
}

ButtonInfo& gdioutput::addButton(int x, int y, int w, const string& id,
  const wstring& text, GUICALLBACK cb, const wstring& toolTip,
  bool absPos, bool hasState) {
  return addButton(x, y, w, getButtonHeight(), id, text,
    gdiFonts::normalText, cb, toolTip, absPos, hasState);
}

ButtonInfo& gdioutput::addButton(int x, int y, int width, int height,
  const string& id, const wstring& text,
  gdiFonts font, GUICALLBACK cb,
  const wstring& tooltip,
  bool absPos, bool hasState) {
  int style = hasState ? BS_CHECKBOX | BS_PUSHLIKE : BS_PUSHBUTTON;

  if (text[0] == '@')
    style |= BS_BITMAP;

  ButtonInfo bi;
  wstring ttext = lang.tl(text);
  int tts = ttext.size();
  if (tts > 2 && ttext[0] == '<' && ttext[1] == '<') {
    ttext = L"◀" + ttext.substr(2);
  }
  else if (tts > 2 && ttext[tts - 1] == '>' && ttext[tts - 2] == '>') {
    ttext = ttext.substr(0, tts - 2) + L"▶";
  }
  if (lang.capitalizeWords())
    capitalizeWords(ttext);
  if (absPos) {
    if (ttext.find_first_of('\n') != string::npos) { 
      style |= BS_MULTILINE;
      height *= 2;
    }
    bi.hWnd = CreateWindow(L"BUTTON", ttext.c_str(), WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS | style | BS_NOTIFY,
      x - OffsetX, y, width, height, hWndTarget, NULL,
      (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE), NULL);
  }
  else {
    bi.hWnd = CreateWindow(L"BUTTON", ttext.c_str(), WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS | style | BS_NOTIFY,
      x - OffsetX, y - OffsetY - 1, width, height, hWndTarget, NULL,
      (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE), NULL);
  }

  if (font == gdiFonts::normalText)
    SendMessage(bi.hWnd, WM_SETFONT, (WPARAM)getGUIFont(), 0);
  else
    SendMessage(bi.hWnd, WM_SETFONT, (WPARAM)getCurrentFont().getFont(font), 0);

  if (!absPos)
    updatePos(x, y, width + scaleLength(GDI_BUTTON_SPACING), height + 5);

  bi.xp = x;
  bi.yp = y - 1;
  bi.width = width;
  bi.text = ttext;
  bi.id = id;
  bi.callBack = cb;
  bi.AbsPos = absPos;

  if (tooltip.length() > 0)
    addToolTip(id, tooltip, bi.hWnd);

  BI.push_back(bi);
  biByHwnd[bi.hWnd] = &BI.back();

  FocusList.push_back(bi.hWnd);
  return BI.back();
}

ButtonInfo& gdioutput::addImageButton(int x, int y, int width, int height,
                                      const string &id, int imgId, GUICALLBACK cb,
                                      const wstring& tooltip,
                                      bool absPos, bool hasState) {
  int style = hasState ? BS_CHECKBOX | BS_PUSHLIKE : BS_PUSHBUTTON;
  style |= BS_BITMAP;

  ButtonInfo bi;
  if (absPos) {
    bi.hWnd = CreateWindow(L"BUTTON", L"...", WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS | style | BS_NOTIFY,
      x - OffsetX, y, width, height, hWndTarget, NULL,
      (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE), NULL);
  }
  else {
    bi.hWnd = CreateWindow(L"BUTTON", L"...", WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS | style | BS_NOTIFY,
      x - OffsetX, y - OffsetY - 1, width, height, hWndTarget, NULL,
      (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE), NULL);
  }

  if (!absPos)
    updatePos(x, y, width + scaleLength(GDI_BUTTON_SPACING), height + 5);

  bi.xp = x;
  bi.yp = y - 1;
  bi.width = width;
  bi.id = id;
  bi.callBack = cb;
  bi.AbsPos = absPos;

  image.loadImage(imgId, Image::ImageMethod::Default); 
  HBITMAP bm = image.getVersion(imgId, width - 5, height - 5);
  SendMessage(bi.hWnd, BM_SETIMAGE, IMAGE_BITMAP, LPARAM(bm));

  if (tooltip.length() > 0)
    addToolTip(id, tooltip, bi.hWnd);

  BI.push_back(bi);
  biByHwnd[bi.hWnd] = &BI.back();

  FocusList.push_back(bi.hWnd);
  return BI.back();
}

static int checkBoxCallback(gdioutput *gdi, GuiEventType type, BaseInfo *data) {
  if (type == GUI_LINK) {
    TextInfo *ti = (TextInfo *)data;
    string cid = ti->id.substr(1);
    gdi->check(cid, !gdi->isChecked(cid), true);
    ButtonInfo &bi = ((ButtonInfo &)gdi->getBaseInfo(cid.c_str()));
    if (bi.callBack || bi.hasEventHandler())
      gdi->sendCtrlMessage(cid);
    //gdi->getBaseInfo(cid);
  }
  return 0;
}

void gdioutput::enableCheckBoxLink(TextInfo &ti, bool enable) {
  bool needRefresh = false;
  if (enable) {
    needRefresh = ti.callBack == 0;
    ti.callBack = checkBoxCallback;
    ti.setColor(colorDefault);
  }
  else {
    needRefresh = ti.callBack != 0;
    ti.callBack = 0;
    DWORD c = GetSysColor(COLOR_GRAYTEXT);
    ti.setColor(GDICOLOR(c));
  }
  if (needRefresh)
    InvalidateRect(hWndTarget, &ti.textRect, true);
}

ButtonInfo &gdioutput::addCheckbox(const string &id, const string &text,
                                   GUICALLBACK cb, bool Checked, const string &tooltip)
{
  return addCheckbox(CurrentX,  CurrentY,  id, text, cb, Checked, tooltip);
}

ButtonInfo &gdioutput::addCheckbox(const string &id, const wstring &text,
                                   GUICALLBACK cb, bool Checked, const wstring &tooltip)
{
  return addCheckbox(CurrentX,  CurrentY,  id, text, cb, Checked, tooltip);
}

ButtonInfo &gdioutput::addCheckbox(int x, int y, const string &id, const string &text,
                                   GUICALLBACK cb, bool Checked, const string &tooltip, bool AbsPos)
{
  return addCheckbox(x,y,id, widen(text), cb, Checked, widen(tooltip), AbsPos);
}

ButtonInfo& gdioutput::addCheckbox(int x, int y, const string& id, const wstring& text,
  GUICALLBACK cb, bool Checked, const wstring& tooltip, bool AbsPos)
{
  ButtonInfo bi;
  SIZE size;

  wstring ttext = lang.tl(text);
  HDC hDC = GetDC(hWndTarget);
  SelectObject(hDC, GetStockObject(DEFAULT_GUI_FONT));
  GetTextExtentPoint32(hDC, L"M", 1, &size);

  int ox = OffsetX;
  int oy = OffsetY;

  if (AbsPos) {
    ox = 0;
    oy = 0;
  }

  int h = size.cy;
  SelectObject(hDC, getGUIFont());
  GetTextExtentPoint32(hDC, ttext.c_str(), ttext.length(), &size);
  ReleaseDC(hWndTarget, hDC);

  int cbY = y + (size.cy - h) / 2;
  bi.hWnd = CreateWindowEx(0, L"BUTTON", L"", WS_TABSTOP | WS_VISIBLE |
    WS_CHILD | WS_CLIPSIBLINGS | BS_AUTOCHECKBOX | BS_NOTIFY,
    x - ox, cbY - oy, h, h, hWndTarget, NULL,
    (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE), NULL);

  TextInfo& desc = addStringUT(y, x + (3 * h) / 2, 0, ttext, 0, checkBoxCallback);
  desc.id = "T" + id;

  SendMessage(bi.hWnd, WM_SETFONT, (WPARAM)getGUIFont(), 0);

  if (Checked)
    SendMessage(bi.hWnd, BM_SETCHECK, BST_CHECKED, 0);

  bi.checked = Checked;

  if (!AbsPos) {
    if (ttext.empty())
      updatePos(x, y, size.cx + int(30 * scale), size.cy + int(scale * 12) + 3);
    else
      updatePos(x, y, size.cx + int(30 * scale), desc.textRect.bottom - desc.textRect.top + scaleLength(4));
  }
  if (tooltip.length() > 0) {
    addToolTip(id, tooltip, bi.hWnd);
    addToolTip(desc.id, tooltip, 0, &desc.textRect);
  }
  bi.isCheckbox = true;
  bi.xp = x;
  bi.yp = cbY;
  bi.width = desc.textRect.right - (x - ox);
  bi.text = ttext;
  bi.id = id;
  bi.callBack = cb;
  bi.AbsPos = AbsPos;
  bi.originalState = Checked;
  bi.isEdit(true);
  BI.push_back(bi);
  biByHwnd[bi.hWnd] = &BI.back();

  FocusList.push_back(bi.hWnd);
  return BI.back();
}

bool gdioutput::isChecked(const string &id)
{
  list<ButtonInfo>::iterator it;
  for(it=BI.begin(); it != BI.end(); ++it)
    if (it->id==id)
      return SendMessage(it->hWnd, BM_GETCHECK, 0, 0)==BST_CHECKED;

  return false;
}

void gdioutput::check(const string &id, bool state, bool keepOriginalState){
  list<ButtonInfo>::iterator it;
  for(it=BI.begin(); it != BI.end(); ++it) {
    if (it->id==id){
      SendMessage(it->hWnd, BM_SETCHECK, state ? BST_CHECKED:BST_UNCHECKED, 0);
      it->checked = state;
      it->synchData();
      if (!keepOriginalState)
        it->originalState = state;
      return;
    }
  }

  #ifdef _DEBUG
    string err = string("Internal Error, identifier not found: X#") + id;
    throw std::exception(err.c_str());
  #endif
}

InputInfo &gdioutput::addInput(const string &id, const wstring &text, int length, 
                               GUICALLBACK cb, const wstring &explanation, const wstring &help)
{
  return addInput(CurrentX, CurrentY, id, text, length, cb, explanation, help);
}

HFONT gdioutput::getGUIFont() const
{
  if (scale==1)
    return (HFONT)GetStockObject(DEFAULT_GUI_FONT);
  else
    return getCurrentFont().getGUIFont();
}

pair<int, int> gdioutput::getInputDimension(int length) const {
  if (!guiMeasure) {
    HDC hDC = GetDC(hWndTarget);
    SelectObject(hDC, getGUIFont());
    SIZE size;
    GetTextExtentPoint32(hDC, L"M", 1, &size);

    SIZE sizeAvg;
    wstring avgText = L"123456789ABCDEFGHIJHKLMNOPQRSTUVXYZ abcdefghijklmnopqrstuvxyz";
    GetTextExtentPoint32(hDC, avgText.c_str(), avgText.length(), &sizeAvg);
    ReleaseDC(hWndTarget, hDC);

    int dy = GetSystemMetrics(SM_CYEDGE);
    int dx = GetSystemMetrics(SM_CXEDGE);
    guiMeasure = make_shared<GuiMeasure>();
    guiMeasure->letterWidth = size.cx;
    guiMeasure->extraX = 2 * dx;
    guiMeasure->height = 4 + dy * 2 + size.cy;
    guiMeasure->avgCharWidth = float(sizeAvg.cx) / float(avgText.length());
  }

  return make_pair(length * guiMeasure->letterWidth + guiMeasure->extraX, guiMeasure->height);
}

int gdioutput::getButtonHeight() const {
  return int(getInputDimension(0).second * 1.2);//int(scale * 24) + 0;
}


InputInfo &gdioutput::addInput(int x, int y, const string &id, const wstring &text,
                               int length, GUICALLBACK cb,
                               const wstring &explanation, const wstring &help) {
  if (explanation.length()>0) {
    addString(id + "_label", y, x, 0, explanation);
    y+=lineHeight;
  }

  InputInfo ii;
  
  auto dim = getInputDimension(length);
  int ox=OffsetX;
  int oy=OffsetY;

  ii.hWnd=CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", text.c_str(),
    WS_TABSTOP|WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS | ES_AUTOHSCROLL | WS_BORDER,
    x-ox, y-oy, dim.first, dim.second,
    hWndTarget, NULL, (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE), NULL);
  int mrg = scaleLength(4);
  updatePos(x, y, dim.first+mrg, dim.second+mrg);

  SendMessage(ii.hWnd, WM_SETFONT,
              (WPARAM) getGUIFont(), 0);

  ii.xp=x;
  ii.yp=y;
  ii.width = dim.first;
  ii.height = dim.second;
  ii.text = text;
  ii.original = text;
  ii.focusText = text;
  ii.id=id;
  ii.callBack=cb;

  II.push_back(ii);
  iiByHwnd[ii.hWnd] = &II.back();
  if (help.length() > 0)
    addToolTip(id, help, ii.hWnd);

  FocusList.push_back(ii.hWnd);

  if (II.size() == 1) {
    SetFocus(ii.hWnd);
    currentFocus = ii.hWnd;
  }

  return II.back();
}

InputInfo &gdioutput::addInputBox(const string &id, int width, int height, const wstring &text,
                                  GUICALLBACK cb, const wstring &explanation)
{
  return addInputBox(id, CurrentX, CurrentY, width, height, text, cb, explanation);
}

InputInfo &gdioutput::addInputBox(const string &id, int x, int y, int widthIn, int heightIn,
                                  const wstring &text, GUICALLBACK cb, const wstring &explanation)
{
  if (explanation.length()>0) {
    addString("", y, x, 0, explanation);
    y+=lineHeight;
  }
  int width = scaleLength(widthIn);
  int height = scaleLength(heightIn);
  InputInfo ii;

  int ox=OffsetX;
  int oy=OffsetY;

  ii.hWnd=CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", text.c_str(), WS_HSCROLL|WS_VSCROLL|
    WS_TABSTOP|WS_VISIBLE|WS_CHILD | WS_CLIPSIBLINGS |ES_AUTOHSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|WS_BORDER,
    x-ox, y-oy, width, height, hWndTarget, NULL,
    (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE), NULL);

  updatePos(x, y, width, height + scaleLength(5));

  SendMessage(ii.hWnd, WM_SETFONT, (WPARAM) getGUIFont(), 0);

  ii.xp=x;
  ii.yp=y;
  ii.width = width;
  ii.height = height;
  ii.text = text;
  ii.original = text;
  ii.focusText = text;
  ii.id=id;
  ii.callBack=cb;
  II.push_back(ii);

  iiByHwnd[ii.hWnd] = &II.back();
  
  FocusList.push_back(ii.hWnd);
  return II.back();
}

ListBoxInfo &gdioutput::addListBox(const string &id, int width, int height, GUICALLBACK cb, const wstring &explanation, const wstring &tooltip, bool multiple)
{
  return addListBox(CurrentX, CurrentY, id, width, height, cb, explanation, tooltip, multiple);
}

LRESULT CALLBACK GetMsgProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam) {
  ListBoxInfo *lbi = (ListBoxInfo *)(GetWindowLongPtr(hWnd, GWLP_USERDATA));
  if (!lbi) {
    throw std::exception("Internal GDI error");
  }

  LPARAM res = CallWindowProc(lbi->originalProc, hWnd, iMsg, wParam, lParam);
  if (iMsg == WM_VSCROLL || iMsg == WM_MOUSEWHEEL || iMsg == WM_KEYDOWN) {
    LRESULT topIndex = CallWindowProc(lbi->originalProc, hWnd, LB_GETTOPINDEX, 0, 0);
    if (lbi->lbiSync) {
      ListBoxInfo *other = lbi->lbiSync;
      CallWindowProc(other->originalProc, other->hWnd, LB_SETTOPINDEX, topIndex, 0);
    }
  }
  return res;
}

void gdioutput::synchronizeListScroll(const string &id1, const string &id2)
{
  ListBoxInfo *a = 0, *b = 0;
  list<ListBoxInfo>::iterator it;
  for (it = LBI.begin(); it != LBI.end(); ++it) {
    if (it->id == id1)
      a = &*it;
    else if (it->id == id2)
      b = &*it;
  }
  if (!a || !b)
    throw std::exception("Not found");

  a->lbiSync = b;
  b->lbiSync = a;
  SetWindowLongPtr(a->hWnd, GWLP_USERDATA, LONG_PTR(a));
  SetWindowLongPtr(b->hWnd, GWLP_USERDATA, LONG_PTR(b));

  a->originalProc = WNDPROC(GetWindowLongPtr(a->hWnd, GWLP_WNDPROC));
  b->originalProc = WNDPROC(GetWindowLongPtr(b->hWnd, GWLP_WNDPROC));

  SetWindowLongPtr(a->hWnd, GWLP_WNDPROC, LONG_PTR(GetMsgProc));
  SetWindowLongPtr(b->hWnd, GWLP_WNDPROC, LONG_PTR(GetMsgProc));
}

ListBoxInfo &gdioutput::addListBox(int x, int y, const string &id, int width, int height, GUICALLBACK cb, 
                                   const wstring &explanation, const wstring &tooltip, bool multiple) {
  if (explanation.length()>0) {
    addString(id+"_label", y, x, 0, explanation);
    y+=lineHeight;
  }
  ListBoxInfo lbi;
  int ox=OffsetX;
  int oy=OffsetY;

  DWORD style=WS_TABSTOP|WS_VISIBLE|WS_CHILD | WS_CLIPSIBLINGS |WS_BORDER|LBS_USETABSTOPS|LBS_NOTIFY|WS_VSCROLL;

  if (multiple)
    style|=LBS_MULTIPLESEL;

  lbi.hWnd=CreateWindowEx(WS_EX_CLIENTEDGE, L"LISTBOX", L"",  style,
    x-ox, y-oy, int(width*scale), int(height*scale), hWndTarget, NULL,
    (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE), NULL);

  updatePos(x, y, int(scale*(width+5)), int(scale * (height+2)));
  SendMessage(lbi.hWnd, WM_SETFONT, (WPARAM) getGUIFont(), 0);

  lbi.IsCombo=false;
  lbi.multipleSelection = multiple;
  lbi.xp=x;
  lbi.yp=y;
  lbi.width = scale*width;
  lbi.height = scale*height;
  lbi.id=id;
  lbi.callBack=cb;
  LBI.push_back(lbi);
  lbiByHwnd[lbi.hWnd] = &LBI.back();
  if (tooltip.length() > 0)
    addToolTip(id, tooltip, lbi.hWnd);

  FocusList.push_back(lbi.hWnd);
  return LBI.back();
}

void gdioutput::setSelection(const string &id, const set<int> &selection)
{
  list<ListBoxInfo>::iterator it;
  for(it=LBI.begin(); it != LBI.end(); ++it){
    if (it->id==id && !it->IsCombo) {
      list<int>::const_iterator cit;

      if (selection.count(-1)==1)
        SendMessage(it->hWnd, LB_SETSEL, 1, -1);
      else {
        LRESULT count=SendMessage(it->hWnd, LB_GETCOUNT, 0,0);
        SendMessage(it->hWnd, LB_SETSEL, 0, -1);
        for(int i=0;i<count;i++){
          LRESULT d=SendMessage(it->hWnd, LB_GETITEMDATA, i, 0);

          if (selection.count(int(d))==1)
            SendMessage(it->hWnd, LB_SETSEL, 1, i);
        }
        return;
      }
    }
  }
}

void gdioutput::getSelection(const string &id, set<int> &selection) {
  list<ListBoxInfo>::iterator it;
  for(it=LBI.begin(); it != LBI.end(); ++it){
    if (it->id==id && !it->IsCombo) {
      selection.clear();
      LRESULT count=SendMessage(it->hWnd, LB_GETCOUNT, 0,0);
      for(int i=0;i<count;i++){
        LRESULT s=SendMessage(it->hWnd, LB_GETSEL, i, 0);
        if (s) {
          LRESULT d=SendMessage(it->hWnd, LB_GETITEMDATA, i, 0);
          selection.insert(int(d));
        }
      }
      return;
    }
  }

  #ifdef _DEBUG
    string err = string("Internal Error, identifier not found: X#") + id;
    throw std::exception(err.c_str());
  #endif
}

ListBoxInfo &gdioutput::addSelection(const string &id, int width, int height, GUICALLBACK cb, const wstring &explanation, const wstring &tooltip)
{
  return addSelection(CurrentX, CurrentY, id, width, height, cb, explanation, tooltip);
}

ListBoxInfo &gdioutput::addSelection(int x, int y, const string &id, int width, int height,
                                     GUICALLBACK cb, const wstring &explanation, const wstring &tooltip)
{
  if (explanation.length()>0) {
    addString(id + "_label", y, x, 0, explanation);
    y+=lineHeight;
  }

  ListBoxInfo lbi;

  int ox = OffsetX;
  int oy = OffsetY;

  lbi.hWnd=CreateWindowEx(WS_EX_CLIENTEDGE, L"COMBOBOX", L"",  WS_TABSTOP|WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS |WS_BORDER|CBS_DROPDOWNLIST|WS_VSCROLL ,
    x-ox, y-oy, int(scale*width), int(scale*height), hWndTarget, NULL,
    (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE), NULL);

  updatePos(x, y, int(scale*(width+5)), int(scale*30));

  SendMessage(lbi.hWnd, WM_SETFONT, (WPARAM) getGUIFont(), 0);

  lbi.IsCombo=true;
  lbi.xp=x;
  lbi.yp=y;
  lbi.width = scale*width;
  lbi.height = scale*30;
  lbi.id=id;
  lbi.callBack=cb;

  LBI.push_back(lbi);
  lbiByHwnd[lbi.hWnd] = &LBI.back();

  if (tooltip.length() > 0)
    addToolTip(id, tooltip, lbi.hWnd);

  FocusList.push_back(lbi.hWnd);
  return LBI.back();
}

ListBoxInfo &gdioutput::addCombo(const string &id, int width, int height, GUICALLBACK cb,
                                 const wstring &explanation, const wstring &tooltip) {
  return addCombo(CurrentX, CurrentY, id, width, height, cb, explanation, tooltip);
}

ListBoxInfo &gdioutput::addCombo(int x, int y, const string &id, int width, int height, GUICALLBACK cb, 
                                 const wstring &explanation, const wstring &tooltip) {
  if (explanation.length()>0) {
    addString(id + "_label", y, x, 0, explanation);
    y+=lineHeight;
  }

  ListBoxInfo lbi;
  int ox=OffsetX;
  int oy=OffsetY;

  lbi.hWnd=CreateWindowEx(WS_EX_CLIENTEDGE, L"COMBOBOX", L"",  WS_TABSTOP|WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS |WS_BORDER|CBS_DROPDOWN |CBS_AUTOHSCROLL,
    x-ox, y-oy, int(scale*width), int(scale*height), hWndTarget, NULL,
    (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE), NULL);

  updatePos(x, y, int(scale * (width+5)), getButtonHeight()+scaleLength(5));

  SendMessage(lbi.hWnd, WM_SETFONT, (WPARAM) getGUIFont(), 0);

  lbi.IsCombo=true;
  lbi.xp=x;
  lbi.yp=y;
  lbi.width = scale*width;
  lbi.height = scale*height;
  lbi.id=id;
  lbi.callBack=cb;

  LBI.push_back(lbi);
  lbiByHwnd[lbi.hWnd] = &LBI.back();

  if (tooltip.length() > 0)
    addToolTip(id, tooltip, lbi.hWnd);

  FocusList.push_back(lbi.hWnd);
  return LBI.back();
}

bool gdioutput::addItem(const string &id, const wstring &text, size_t data) {
  list<ListBoxInfo>::reverse_iterator it;
  for (it=LBI.rbegin(); it != LBI.rend(); ++it) {
    if (it->id==id) {
      if (it->IsCombo) {
        LRESULT index=SendMessage(it->hWnd, CB_ADDSTRING, 0, LPARAM(text.c_str()));
        SendMessage(it->hWnd, CB_SETITEMDATA, index, data);
        it->data2Index[data] = int(index);
        it->computed_hash = 0;
      }
      else {
        LRESULT index=SendMessage(it->hWnd, LB_INSERTSTRING, -1, LPARAM(text.c_str()));
        SendMessage(it->hWnd, LB_SETITEMDATA, index, data);
        it->data2Index[data] = int(index);
        it->computed_hash = 0;
      }
      return true;
    }
  }
  return false;
}

bool gdioutput::modifyItemDescription(const string& id, size_t itemData, const wstring &description) {
  for (auto it = LBI.rbegin(); it != LBI.rend(); ++it) {
    if (it->id == id) {
      int ix = it->data2Index[itemData];
      // It is intentioal that the hash is not modified. This method allows "customization" of
      // some description without reloading a complete listbox
      if (it->IsCombo) {
        SendMessage(it->hWnd, CB_DELETESTRING, ix, 0);
        SendMessage(it->hWnd, CB_INSERTSTRING, ix, LPARAM(description.c_str()));
        SendMessage(it->hWnd, CB_SETITEMDATA, ix, itemData);
      }
      else {
        SendMessage(it->hWnd, LB_DELETESTRING, ix, 0);
        SendMessage(it->hWnd, LB_INSERTSTRING, ix, LPARAM(description.c_str()));
        SendMessage(it->hWnd, LB_SETITEMDATA, ix, itemData);
      }
      return true;
    }
  }

  return false;
}

bool gdioutput::setItems(const string& id, const vector<pair<wstring, size_t>>& items) {
  auto hash = ListBoxInfo::computeItemHash(items);
  for (auto it = LBI.rbegin(); it != LBI.rend(); ++it) {
    if (it->id == id) {
      if (it->IsCombo) {
        if (it->computed_hash == 0 || it->computed_hash != hash) {
          SendMessage(it->hWnd, CB_RESETCONTENT, 0, 0);
          SendMessage(it->hWnd, CB_INITSTORAGE, items.size(), 48);
          SendMessage(it->hWnd, WM_SETREDRAW, FALSE, 0);
          it->data2Index.clear();

          for (size_t k = 0; k < items.size(); k++) {
            LRESULT index = SendMessage(it->hWnd, CB_ADDSTRING, 0, LPARAM(items[k].first.c_str()));
            SendMessage(it->hWnd, CB_SETITEMDATA, index, items[k].second);
            it->data2Index[items[k].second] = int(index);
          }
          SendMessage(it->hWnd, WM_SETREDRAW, TRUE, 0);
          RedrawWindow(it->hWnd, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
          it->computed_hash = hash;
        }
        else {
          SendMessage(it->hWnd, CB_SETCURSEL, -1, 0);
        }
      }
      else {
        if (it->computed_hash == 0 || it->computed_hash != hash) {
          SendMessage(it->hWnd, LB_RESETCONTENT, 0, 0);
          SendMessage(it->hWnd, LB_INITSTORAGE, items.size(), 48);
          SendMessage(it->hWnd, WM_SETREDRAW, FALSE, 0);

          it->data2Index.clear();
          for (size_t k = 0; k < items.size(); k++) {
            LRESULT index = SendMessage(it->hWnd, LB_INSERTSTRING, -1, LPARAM(items[k].first.c_str()));
            SendMessage(it->hWnd, LB_SETITEMDATA, index, items[k].second);
            it->data2Index[items[k].second] = int(index);
          }

          SendMessage(it->hWnd, WM_SETREDRAW, TRUE, 0);
          RedrawWindow(it->hWnd, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
          it->computed_hash = hash;
        }
        else {
          SendMessage(it->hWnd, LB_SETCURSEL, -1, 0);
        }
      }
      return true;
    }
  }
  return false;
}

void gdioutput::filterOnData(const string &id, const unordered_set<int> &filter) {
  list<ListBoxInfo>::iterator it;
  for (it=LBI.begin(); it != LBI.end(); ++it) {
    if (it->id==id) {
      if (it->IsCombo) {
      }
      else {
        it->computed_hash = 0;
        const HWND &hWnd = it->hWnd;
        LRESULT count = SendMessage(hWnd, LB_GETCOUNT, 0, 0);
        for (intptr_t ix = count - 1; ix>=0; ix--) {
          LRESULT ret = SendMessage(hWnd, LB_GETITEMDATA, ix, 0);
          if (ret != LB_ERR && filter.count(int(ret)) == 0)
            SendMessage(hWnd, LB_DELETESTRING, ix, 0);
        }
        return;
      }
    }
  }
  assert(false);
}

bool gdioutput::clearList(const string& id) {
  for (auto it = LBI.begin(); it != LBI.end(); ++it) {
    if (it->id == id) {
      it->original = L"";
      it->originalIdx = -1;
      it->computed_hash = 0;
      it->data2Index.clear();

      if (it->IsCombo)
        SendMessage(it->hWnd, CB_RESETCONTENT, 0, 0);
      else
        SendMessage(it->hWnd, LB_RESETCONTENT, 0, 0);
      return true;
    }
  }

  return false;
}

bool gdioutput::getSelectedItem(const string &id, ListBoxInfo &lbi) {
  lbi = ListBoxInfo();
  list<ListBoxInfo>::iterator it;
  for (it=LBI.begin(); it != LBI.end(); ++it) {
    if (it->id==id) {
      bool ret = getSelectedItem(*it);
      it->copyUserData(lbi);
      return ret;
    }
  }
  return false;
}

pair<int, bool> gdioutput::getSelectedItem(const string &id) {
  ListBoxInfo lbi;
  bool ret = getSelectedItem(id, lbi);
  return make_pair(lbi.getDataInt(), ret);
}

pair<int, bool> gdioutput::getSelectedItem(const char *id) {
  string ids = id;
  return getSelectedItem(ids);
}

void ListBoxInfo::copyUserData(ListBoxInfo &dest) const {
  dest.data = data;
  dest.text = text;
  dest.id = id;
  dest.extra = extra;
  dest.index = index;
  dest.IsCombo = IsCombo;
}

uint64_t ListBoxInfo::computeItemHash(const vector<pair<wstring, size_t>>& items) {
  uint64_t res = 1;
  for (auto& it : items) {
    res = res * 997 + it.second;
    for (auto ch : it.first)
      res = res * 2003 + ch;
  }

  return res;
}

bool gdioutput::getSelectedItem(ListBoxInfo &lbi) {
  if (lbi.IsCombo) {
    LRESULT index=SendMessage(lbi.hWnd, CB_GETCURSEL, 0, 0);

    if (index == CB_ERR) {
      wchar_t bf[256];
      GetWindowText(lbi.hWnd, bf, 256);
      lbi.text=bf;
      lbi.data=-1;
      lbi.index=int(index);
      return false;
    }
    lbi.data=SendMessage(lbi.hWnd, CB_GETITEMDATA, index, 0);
    wchar_t bf[1024];
    if (SendMessage(lbi.hWnd, CB_GETLBTEXT, index, LPARAM(bf)) != CB_ERR)
      lbi.text=bf;
  }
  else {
    LRESULT index=SendMessage(lbi.hWnd, LB_GETCURSEL, 0, 0);

    if (index==LB_ERR)
      return false;

    lbi.data=SendMessage(lbi.hWnd, LB_GETITEMDATA, index, 0);
    lbi.index=int(index);

    TCHAR bf[1024];
    if (SendMessage(lbi.hWnd, LB_GETTEXT, index, LPARAM(bf))!=LB_ERR)
      lbi.text=bf;
  }
  return true;
}

int gdioutput::getNumItems(const char *id) {
  for (auto &lbi : LBI) {
    if (lbi.id == id) {
      if (lbi.IsCombo) {
        return (int)SendMessage(lbi.hWnd, CB_GETCOUNT, 0, 0);
      }
      else {
        return (int)SendMessage(lbi.hWnd, LB_GETCOUNT, 0, 0);
      }
    }
  }

#ifdef _DEBUG
  string err = string("Internal Error, identifier not found: X#") + id;
  throw std::exception(err.c_str());
#endif

  return 0;
}

int gdioutput::getItemDataByName(const char *id, const char *name) const{
  wstring wname = recodeToWide(name);
  list<ListBoxInfo>::const_iterator it;
  for(it = LBI.begin(); it != LBI.end(); ++it){
    if (it->id==id) {
      if (it->IsCombo) {
        LRESULT ix = SendMessage(it->hWnd, CB_FINDSTRING, -1, LPARAM(wname.c_str()));
        if (ix >= 0) {
          return (int)SendMessage(it->hWnd, CB_GETITEMDATA, ix, 0);
        }
        return -1;
      }
      else {
        LRESULT ix = SendMessage(it->hWnd, LB_FINDSTRING, -1, LPARAM(wname.c_str()));
        if (ix >= 0) {
          return (int)SendMessage(it->hWnd, LB_GETITEMDATA, ix, 0);
        }
        return -1;
      }
    }
  }
  return -1;
}

bool gdioutput::selectItemByData(const char* id, int data) {
  for (auto it = LBI.begin(); it != LBI.end(); ++it) {
    if (it->id == id) {
      if (it->IsCombo) {
        if (data == -1) {
          SendMessage(it->hWnd, CB_SETCURSEL, -1, 0);
          it->data = 0;
          it->text = L"";
          it->original = L"";
          it->originalIdx = -1;
          return true;
        }
        else {
          LRESULT count = SendMessage(it->hWnd, CB_GETCOUNT, 0, 0);

          for (int m = 0; m < count; m++) {
            LRESULT ret = SendMessage(it->hWnd, CB_GETITEMDATA, m, 0);
            if (ret == data) {
              SendMessage(it->hWnd, CB_SETCURSEL, m, 0);
              it->data = data;
              it->originalIdx = data;
              TCHAR bf[1024];
              if (SendMessage(it->hWnd, CB_GETLBTEXT, m, LPARAM(bf)) != CB_ERR) {
                bf[1023] = 0;
                it->text = bf;
                it->original = bf;
              }
              return true;
            }
          }
        }
        return false;
      }
      else {
        if (data == -1) {
          SendMessage(it->hWnd, LB_SETCURSEL, -1, 0);
          it->data = 0;
          it->text = L"";
          it->original = L"";
          it->originalIdx = -1;
          return true;
        }
        else {
          LRESULT count = SendMessage(it->hWnd, LB_GETCOUNT, 0, 0);
          for (int m = 0; m < count; m++) {
            LRESULT ret = SendMessage(it->hWnd, LB_GETITEMDATA, m, 0);

            if (ret == data) {
              SendMessage(it->hWnd, LB_SETCURSEL, m, 0);
              it->data = data;
              it->originalIdx = data;
              TCHAR bf[1024];
              if (SendMessage(it->hWnd, LB_GETTEXT, m, LPARAM(bf)) != LB_ERR) {
                bf[1023] = 0;
                it->text = bf;
                it->original = bf;
              }
              return true;
            }
          }
        }
        return false;
      }
    }
  }
  return false;
}

bool gdioutput::selectItemByIndex(const char *id, int index) {
  for (auto it = LBI.begin(); it != LBI.end(); ++it) {
    if (it->id == id) {
      if (it->IsCombo) {
        if (index == -1) {
          SendMessage(it->hWnd, CB_SETCURSEL, -1, 0);
          it->data = 0;
          it->text = L"";
          it->original = L"";
          it->originalIdx = -1;
          return true;
        }
        else {
          SendMessage(it->hWnd, CB_SETCURSEL, index, 0);
          LRESULT data = SendMessage(it->hWnd, CB_GETITEMDATA, index, 0);
          it->data = data;
          it->originalIdx = data;
          TCHAR bf[1024];
          if (SendMessage(it->hWnd, CB_GETLBTEXT, index, LPARAM(bf)) != CB_ERR) {
            it->text = bf;
            it->original = bf;
          }
          return true;
        }
        return false;
      }
      else {
        if (index == -1) {
          SendMessage(it->hWnd, LB_SETCURSEL, -1, 0);
          it->data = 0;
          it->text = L"";
          it->original = L"";
          it->originalIdx = -1;
          return true;
        }
        else {
          SendMessage(it->hWnd, LB_SETCURSEL, index, 0);
          LRESULT data = SendMessage(it->hWnd, LB_GETITEMDATA, index, 0);

          it->data = data;
          it->originalIdx = data;
          TCHAR bf[1024];
          if (SendMessage(it->hWnd, LB_GETTEXT, index, LPARAM(bf)) != LB_ERR) {
            it->text = bf;
            it->original = bf;
          }
          return true;
        }
        return false;
      }
    }
  }
  return false;
}

bool gdioutput::autoGrow(const char *id) {
  list<ListBoxInfo>::iterator it;
  int size = 0;
  TextInfo TI;
  TI.format=0;
  TI.xp=0;
  TI.yp=0;
  TI.id="";
  TI.xlimit=0;
  TI.callBack=0;
  HDC hDC=GetDC(hWndTarget);

  for(it=LBI.begin(); it != LBI.end(); ++it){
    if (it->id==id) {
      if (it->IsCombo) {
        LRESULT count = SendMessage(it->hWnd, CB_GETCOUNT, 0, 0);
        for (int m = 0; m < count; m++) {
          wchar_t bf[1024];
          if (SendMessage(it->hWnd, CB_GETLBTEXT, m, LPARAM(bf))!=CB_ERR) {
            TI.text = bf;
            calcStringSize(TI, hDC);
            size = max<int>(size, TI.textRect.right - TI.textRect.left);
          }
        }
        
        ReleaseDC(hWndTarget, hDC);

        size += scaleLength(30);
        if (size > it->width) {
          it->width = size;
          SetWindowPos(it->hWnd, 0, 0, 0, (int)it->width, (int)it->height, SWP_NOZORDER|SWP_NOCOPYBITS|SWP_NOMOVE);
          updatePos(it->xp, it->yp, (int)it->width + int(scale*5), (int)it->height);
          return true;
        }
        return false;
      }
      else {
        LRESULT count = SendMessage(it->hWnd, LB_GETCOUNT, 0, 0);
        for (int m = 0; m < count; m++) {
          wchar_t bf[1024];
          LRESULT len = SendMessage(it->hWnd, LB_GETTEXT, m, LPARAM(bf));
          if (len!=LB_ERR) {
            if (it->lastTabStop == 0)
              TI.text = bf;
            else {
              auto pos = len;
              while(pos > 0) {
                if (bf[pos-1] == '\t') {
                  break;
                }
                pos--;
              }
              TI.text = &bf[pos];
            }
            calcStringSize(TI, hDC);
            size = max<int>(size, TI.realWidth + it->lastTabStop);
          }
        }
        
        ReleaseDC(hWndTarget, hDC);
        size += scaleLength(30);
        if (size > it->width) {
          it->width = size;
          SetWindowPos(it->hWnd, 0, 0, 0, (int)it->width, (int)it->height, SWP_NOZORDER|SWP_NOCOPYBITS|SWP_NOMOVE);
          updatePos(it->xp, it->yp, (int)it->width+int(scale*5), (int)it->height);
          return true;
        }
        return false;
      }
    }
  }

  ReleaseDC(hWndTarget, hDC);
  return false;
}

void gdioutput::removeSelected(const char *id)
{
}


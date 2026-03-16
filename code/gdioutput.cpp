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

extern int defaultCodePage;

GuiHandler &BaseInfo::getHandler() const {
  if (managedHandler)
    return *managedHandler;
  if (handler == 0)
    throw meosException("Handler not definied.");
  return *handler;
}

void GuiHandler::handle(gdioutput &gdi, BaseInfo &info, GuiEventType type) {
  throw meosException("Handler not definied.");
}

InputInfo::InputInfo() : hWnd(0), callBack(0), ignoreCheck(false),
                isEditControl(true), bgColor(colorDefault), fgColor(colorDefault),
                writeLock(false), updateLastData(0) {}


EventInfo::EventInfo() : callBack(0), keyEvent(KC_NONE) {}

/** Return true if rendering text should be skipped for
    this format. */
bool gdioutput::skipTextRender(int format) {
  format &= 0xFF | hiddenText;
  return format == pageNewPage ||
         format == pagePageInfo ||
         format == pageNewChapter ||
         (format & hiddenText) == hiddenText;
}

gdioutput::gdioutput(const string &_tag, double _scale) :
  recorder((Recorder *)0, false) {
  tag = _tag;
  po_default = new PrinterObject();
  tabs = 0;
  hasAnyTimer = false;
  constructor(_scale);

  isTestMode = false;
}
extern gdioutput *gdi_main;

gdioutput::gdioutput(double _scale, HWND hWnd, const PrinterObject &prndef) :
  recorder((Recorder *)0, false) {
  hasAnyTimer = false;
  po_default = new PrinterObject(prndef);
  tabs = 0;
  setWindow(hWnd);
  constructor(_scale);
  if (gdi_main) {
    isTestMode = gdi_main->isTestMode;
    if (isTestMode)
      cmdAnswers.swap(gdi_main->cmdAnswers);
  }
  else isTestMode = false;
}

void gdioutput::constructor(double _scale)
{
  currentFontSet = 0;
  commandLock = false;
  commandUnlockTime = 0;
  lockUpDown = false;

  Background = 0;
  backgroundColor1 = -1;
  backgroundColor2 = -1;
  foregroundColor = -1;
  backgroundImage = -1;

  toolbar = 0;
  initCommon(_scale, L"Segoe UI");

  OffsetY=0;
  OffsetX=0;

  manualUpdate = false;

  itTL = TL.end();

  hWndTarget = 0;
  hWndToolTip = 0;
  hWndAppMain = 0;
  onClear.clear();
  postClear.clear();
  clearPage(true);
  hasCleared = false;
  highContrast = false;
  hideBG = false;
  fullScreen = false;
  lockRefresh = 0;
  autoSpeed = 0;
  autoPos = 0;
  lastSpeed = 0;
  autoCounter = 0;
}

void gdioutput::setFont(int size, const wstring &font) {
  double ss = size * sqrt(size);
  double s = 1 + double(ss)*0.25;
  initCommon(s, font);
}

void gdioutput::setFontCtrl(HWND hWnd) {
  SendMessage(hWnd, WM_SETFONT, (WPARAM) getGUIFont(), MAKELPARAM(TRUE, 0));
}

static void scaleWindow(HWND hWnd, double scale, int &w, int &h) {
  RECT rc;
  GetWindowRect(hWnd, &rc);
  w = rc.right - rc.left;
  h = rc.bottom - rc.top;
  w = int(w * scale + 0.5);
  h = int(h * scale + 0.5);
}

int transformX(int x, double scale) {
  if (x<40)
    return int(x * scale + 0.5);
  else
    return int((x-40) * scale + 0.5) + 40;
}

void gdioutput::scaleSize(double scale_, bool allowSmallScale, ScaleOperation op) {
  if (fabs(scale_ - 1.0) < 1e-4)
    return; // No scaling
  double ns = scale*scale_;

  if (!allowSmallScale && ns + 1e-6 < 1.0 ) {
    ns = 1.0;
    scale_ = 1.0;
  }
  initCommon(ns, currentFont);

  if (op == ScaleOperation::NoUpdate)
    return;

  for (list<TextInfo>::iterator it = TL.begin(); it!=TL.end(); ++it) {
    it->xlimit = int(it->xlimit * scale_ + 0.5);
    it->xp = transformX(it->xp, scale_);
    it->yp = int(it->yp * scale_ + 0.5);
  }
  int w, h;
  OffsetY = int (OffsetY * scale_ + 0.5);
  OffsetX = int (OffsetX * scale_ + 0.5);

  for (list<ButtonInfo>::iterator it = BI.begin(); it!=BI.end(); ++it) {
    if (it->fixedRightTop)
      it->xp = int(scale_ * it->xp + 0.5);
    else
      it->xp = transformX(it->xp, scale_);

    it->yp = int(it->yp * scale_ + 0.5);

    if (it->isCheckbox)
      scaleWindow(it->hWnd, 1.0, w, h);
    else
      scaleWindow(it->hWnd, scale_, w, h);
    setFontCtrl(it->hWnd);
    MoveWindow(it->hWnd, it->xp-OffsetX, it->yp-OffsetY, w, h, true);
  }

  for (list<InputInfo>::iterator it = II.begin(); it!=II.end(); ++it) {
    it->xp = transformX(it->xp, scale_);
    it->yp = int(it->yp * scale_ + 0.5);
    it->height *= scale_;
    it->width *= scale_;
    setFontCtrl(it->hWnd);
    MoveWindow(it->hWnd, it->xp-OffsetX, it->yp-OffsetY, int(it->width+0.5), int(it->height+0.5), true);
  }

  for (list<ListBoxInfo>::iterator it = LBI.begin(); it!=LBI.end(); ++it) {
    it->xp = transformX(it->xp, scale_);
    it->yp = int(it->yp * scale_ + 0.5);
    it->height *= scale_;
    it->width *= scale_;
    setFontCtrl(it->hWnd);
    MoveWindow(it->hWnd, it->xp-OffsetX, it->yp-OffsetY, int(it->width+0.5), int(it->height+0.5), true);
  }

  for (list<RectangleInfo>::iterator it = Rectangles.begin(); it!=Rectangles.end(); ++it) {
    it->rc.bottom = int(it->rc.bottom * scale_ + 0.5);
    it->rc.top = int(it->rc.top * scale_ + 0.5);
    it->rc.right = transformX(it->rc.right, scale_);
    it->rc.left = transformX(it->rc.left, scale_);
  }

  for (list<TableInfo>::iterator it = Tables.begin(); it != Tables.end(); ++it) {
    it->xp = transformX(it->xp, scale_);
    it->yp = int(it->yp * scale_ + 0.5);
  }

  MaxX = transformX(MaxX, scale_);
  MaxY = int (MaxY * scale_ + 0.5);
  CurrentX = transformX(CurrentX, scale_);
  CurrentY = int (CurrentY * scale_ + 0.5);
  SX = transformX(SX, scale_);
  SY = int (SY * scale_ + 0.5);

  for (map<string, RestoreInfo>::iterator it = restorePoints.begin(); it != restorePoints.end(); ++it) {
    RestoreInfo &r = it->second;
    r.sMX = transformX(r.sMX, scale_);
    r.sMY = int (r.sMY * scale_ + 0.5);
    r.sCX = transformX(r.sCX, scale_);
    r.sCY = int (r.sCY * scale_ + 0.5);
    r.sOX = transformX(r.sOX, scale_);
    r.sOY = int (r.sOY * scale_ + 0.5);

  }
  if (op == ScaleOperation::Refresh) {
    refresh();
  }
  else {
    HDC hDC = GetDC(hWndTarget);
    for (auto &ti : TL) {
      calcStringSize(ti, hDC);
    }
    ReleaseDC(hWndTarget, hDC);
  }
}

void gdioutput::initCommon(double _scale, const wstring &font)
{
  guiMeasure.reset();
  dbErrorState = false;
  currentFontSet = 0;
  scale = _scale;
  currentFont = font;
  deleteFonts();
  enableTables();
  lineHeight = int(scale*14);

  Background=CreateSolidBrush(GetSysColor(COLOR_WINDOW));

  fontHeightCache.clear();
  fonts[currentFont].init(scale, currentFont, L"");
  updateTabFont();
}

void gdioutput::updateTabFont() {
  if (this == gdi_main && hWndTab) {
    HFONT gui = fonts[currentFont].getGUIFont();
    SendMessage(hWndTab, WM_SETFONT, WPARAM(gui), TRUE);

    RECT rc;
    GetClientRect(hWndAppMain, &rc);
    SendMessage(hWndAppMain, WM_SIZE, 0, MAKELONG(rc.right, rc.bottom));
  }
}

double getLocalScale(const wstring &fontName, wstring &faceName) {
  double locScale = 1.0;
  vector<wstring> res;
  split(fontName, L";", res);

  if (res.empty() || res.size() > 2)
    throw meosException(L"Cannot load font: " + fontName);
  if (res.size() == 2) {
    locScale = _wtof(res[1].c_str());
    if (!(locScale>0.001 && locScale < 100))
      throw meosException(L"Cannot scale font with factor: " + res[1]);
  }
  faceName = res[0];
  return locScale;
}

const GDIImplFontSet & gdioutput::loadFont(const wstring &font) {
  currentFontSet = 0;
  vector< pair<wstring, size_t> > fontIx;
  getEnumeratedFonts(fontIx);
  double relScale = 1.0;
  for (size_t k = 0; k < fontIx.size(); k++) {
    if (stringMatch(fontIx[k].first, font)) {
      relScale = enumeratedFonts[fontIx[k].second].getRelScale();
    }
  }

  wstring faceName;
  double locScale = getLocalScale(font, faceName);

  if (faceName.empty())
    faceName = currentFont;
  fonts[font].init(scale * relScale * locScale, faceName, font);
  return fonts[font];
}

void gdioutput::deleteFonts() {
  if (Background)
    DeleteObject(Background);
  Background = 0;

  currentFontSet = 0;
  fonts.clear();
}

#ifndef MEOSDB

gdioutput::~gdioutput()
{
  while(!timers.empty()) {
    KillTimer(hWndTarget, (UINT_PTR)&timers.back());
    timers.back().setWnd = 0;
    timers.back().parent = 0;
    timers.pop_back();
  }
  animationData.reset();

  deleteFonts();

  if (toolbar)
    delete toolbar;
  toolbar = 0;
  
  Tables.clear();

  if (tabs) {
    delete tabs;
    tabs = 0;
  }

  initRecorder(0);
  
  delete po_default;
  po_default = 0;
}
#endif


FixedTabs &gdioutput::getTabs() {
#ifndef MEOSDB
  if (!tabs)
    tabs = new FixedTabs();
#endif

  return *tabs;
}



void gdioutput::fetchPrinterSettings(PrinterObject &po) const {
  po = *po_default;
}


void gdioutput::drawBackground(HDC hDC, RECT& rc)
{
  if (backgroundColor1 != -1) {
    SelectObject(hDC, GetStockObject(NULL_PEN));
    SelectObject(hDC, GetStockObject(DC_BRUSH));
    SetDCBrushColor(hDC, backgroundColor1);
    Rectangle(hDC, -1, -1, rc.right + 1, rc.bottom + 1);
    return;
  }
  else if (!backgroundImage.empty()) {
    // TODO

  }

  GRADIENT_RECT gr[2];

  SelectObject(hDC, GetStockObject(NULL_PEN));
  SelectObject(hDC, Background);

  if (highContrast) {
    Rectangle(hDC, -1, -1, rc.right + 1, rc.bottom + 1);

    HFONT hInfo = CreateFont(min(30, int(scale * 22)), 0, 900, 900, FW_LIGHT, false, false, false, DEFAULT_CHARSET,
      OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
      DEFAULT_PITCH | FF_ROMAN, L"Segoe UI");

    SelectObject(hDC, hInfo);
    RECT mrc;
    mrc.left = 0;
    mrc.right = 0;
    mrc.top = 0;
    mrc.bottom = 0;
    DrawText(hDC, listDescription.c_str(), listDescription.length(), &mrc, DT_LEFT | DT_CALCRECT | DT_NOPREFIX);
    int height = mrc.right + mrc.right / 3;
    if (height > 0) {
      SetBkMode(hDC, TRANSPARENT);

      for (int k = height; k < MaxY; k += height) {
        mrc.left = 5 - OffsetX;
        mrc.right = 1000;
        mrc.top = k - OffsetY;
        mrc.bottom = MaxY;
        SetTextColor(hDC, RGB(192, 192, 192));

        DrawText(hDC, listDescription.c_str(), listDescription.length(), &mrc, DT_LEFT | DT_NOCLIP | DT_NOPREFIX);
        mrc.top -= 1;
        mrc.left -= 1;
        SetTextColor(hDC, RGB(92, 32, 32));

        DrawText(hDC, listDescription.c_str(), listDescription.length(), &mrc, DT_LEFT | DT_NOCLIP | DT_NOPREFIX);

      }
    }
    SelectObject(hDC, GetStockObject(ANSI_FIXED_FONT));
    DeleteObject(hInfo);
    return;
  }
  if (!hideBG) {
    Rectangle(hDC, -1, -1, rc.right - OffsetX + 1, 10 - OffsetY + 1);
    Rectangle(hDC, -1, -1, 11 - OffsetX, rc.bottom + 1);
    Rectangle(hDC, MaxX + 10 - OffsetX, 0, rc.right + 1, rc.bottom + 1);
    Rectangle(hDC, 10 - OffsetX, MaxY + 13 - OffsetY, MaxX + 11 - OffsetX, rc.bottom + 1);
  }
  if (dbErrorState) {
    SelectObject(hDC, GetStockObject(DC_BRUSH));
    SetDCBrushColor(hDC, RGB(255, 100, 100));
    Rectangle(hDC, -1, -1, rc.right + 1, rc.bottom + 1);

    HFONT hInfo = CreateFont(30, 0, 900, 900, FW_BOLD, false, false, false, DEFAULT_CHARSET,
      OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
      DEFAULT_PITCH | FF_ROMAN, L"Segoe UI");

    wstring err = lang.tl(L"DATABASE ERROR");
    SelectObject(hDC, hInfo);
    RECT mrc;
    mrc.left = 0;
    mrc.right = 0;
    mrc.top = 0;
    mrc.bottom = 0;
    DrawText(hDC, err.c_str(), err.length(), &mrc, DT_LEFT | DT_CALCRECT | DT_NOPREFIX);
    int width = mrc.bottom + mrc.bottom / 4;
    int height = mrc.right + mrc.right / 4;
    SetBkMode(hDC, TRANSPARENT);
    SetTextColor(hDC, RGB(64, 0, 0));

    for (int k = height; k < max<int>(MaxY, rc.bottom + height); k += height) {
      mrc.left = rc.right - 50 - OffsetX;
      mrc.right = mrc.left + 1000;
      mrc.top = k - OffsetY;
      mrc.bottom = MaxY;
      DrawText(hDC, err.c_str(), err.length(), &mrc, DT_LEFT | DT_NOCLIP | DT_NOPREFIX);
      mrc.left -= width;
      mrc.top -= height / 2;
      DrawText(hDC, err.c_str(), err.length(), &mrc, DT_LEFT | DT_NOCLIP | DT_NOPREFIX);
    }
    SelectObject(hDC, GetStockObject(ANSI_FIXED_FONT));
    DeleteObject(hInfo);
  }
  /*
    DWORD c=GetSysColor(COLOR_3DFACE);
    double red = double(GetRValue(c)) *0.9;
    double green = double(GetGValue(c)) * 0.85;
    double blue = min(255.0, double(GetBValue(c)) * 1.05);

    if (blue<100) {
      //Invert
      red = 255-red;
      green = 255-green;
      blue = 255-blue;
    }

    double blue1=min(255., blue*1.3);
    double green1=min(255., green*1.3);
    double red1=min(255., red*1.3);
    */

  double red = 244.0;
  double green = 250.0;
  double blue = 254.0;

  double red1 = 232.0;
  double green1 = 235.0;
  double blue1 = 253.0;

  TRIVERTEX vert[2];
  if (hideBG) {
    vert[0].x = 0;
    vert[0].y = 0;
  }
  else {
    vert[0].x = 10 - OffsetX;
    vert[0].y = 10 - OffsetY;
  }
  vert[0].Red = 0xff00 & DWORD(red1 * 256);
  vert[0].Green = 0xff00 & DWORD(green1 * 256);
  vert[0].Blue = 0xff00 & DWORD(blue1 * 256);
  vert[0].Alpha = 0x0000;

  if (hideBG) {
    vert[1].x = rc.right + 1;
    vert[1].y = rc.bottom + 1;
  }
  else {
    vert[1].x = MaxX + 10 - OffsetX;
    vert[1].y = MaxY + 13 - OffsetY;
  }
  vert[1].Red = 0xff00 & DWORD(red * 256);
  vert[1].Green = 0xff00 & DWORD(green * 256);
  vert[1].Blue = 0xff00 & DWORD(blue * 256);
  vert[1].Alpha = 0x0000;

  gr[0].UpperLeft = 0;
  gr[0].LowerRight = 1;
  TRIVERTEX vert0 = vert[0];
  TRIVERTEX vert1 = vert[1];

  if (!hideBG) {
    int bWidth = scaleLength(5);
    double red2 = 142;
    double green2 = 147;
    double blue2 = 249;

    TRIVERTEX vertB[4];
    vertB[0] = vert[0];
    vertB[1] = vert[1];
    vertB[1].x = vertB[0].x + bWidth;
    vertB[0].Red = 0xff00 & DWORD(red2 * 256);
    vertB[0].Green = 0xff00 & DWORD(green2 * 256);
    vertB[0].Blue = 0xff00 & DWORD(blue2 * 256);
    vertB[1].Red = 0xff00 & DWORD(254 * 256);
    vertB[1].Green = 0xff00 & DWORD(254 * 256);
    vertB[1].Blue = 0xff00 & DWORD(252 * 256);

    vertB[2] = vert[0];
    vertB[2].x = vertB[0].x + bWidth;
    vertB[2].Red = vertB[1].Red;
    vertB[2].Green = vertB[1].Green;
    vertB[2].Blue = vertB[1].Blue;
    vertB[3] = vert[1];
    vertB[3].x = vertB[0].x + bWidth*2;
    vertB[3].Red = vert[0].Red;
    vertB[3].Green = vert[0].Green;
    vertB[3].Blue= vert[0].Blue;

    gr[1].UpperLeft = 2;
    gr[1].LowerRight = 3;
    GradientFill(hDC, vertB, 4, gr, 2, GRADIENT_FILL_RECT_H);
    vert[0].x += bWidth * 2;
  }

  if (MaxY > max(800, MaxX) || hideBG)
    GradientFill(hDC, vert, 2, gr, 1, GRADIENT_FILL_RECT_H);
  else
    GradientFill(hDC, vert, 2, gr, 1, GRADIENT_FILL_RECT_V);

  if (!hideBG) {
    SelectObject(hDC, GetStockObject(DC_BRUSH));
    SelectObject(hDC, GetStockObject(NULL_PEN));
    int redS = int(red * 0.8);
    int greenS = int(green * 0.8);
    int blueS = int(blue * 0.8);
    int width = scaleLength(3);
    SetDCBrushColor(hDC, RGB(redS, greenS, blueS));
    Rectangle(hDC, vert0.x + width, vert1.y, vert1.x + 1, vert1.y + width);
    Rectangle(hDC, vert1.x, vert0.y + width, vert1.x + width, vert1.y + width);

    SelectObject(hDC, GetStockObject(NULL_BRUSH));
    SelectObject(hDC, GetStockObject(DC_PEN));
    SetDCPenColor(hDC, RGB(DWORD(red * 0.4), DWORD(green * 0.4), DWORD(blue * 0.4)));
    Rectangle(hDC, vert0.x, vert0.y, vert1.x, vert1.y);
  }
}

void gdioutput::setDBErrorState(bool state) {
  if (dbErrorState != state) {
    dbErrorState = state;
    refresh();
  }
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

const string &gdioutput::narrow(const wstring &input) {
  return ::narrow(input);
}

const wstring &gdioutput::widen(const string &input) {
  return ::widen(input);
}

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

const wstring &gdioutput::fromUTF8(const string &input) {
  return ::fromUTF8(input);
}
const string &gdioutput::toUTF8(const wstring &winput)  {
  return ::toUTF8(winput);
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

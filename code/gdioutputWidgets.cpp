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

// gdioutputWidgets.cpp: Widget management, box and text rendering
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
#include "toolbar.h"
#include "gdiimpl.h"
#include "recorder.h"
#include "animationData.h"
#include "image.h"
#include "autocomplete.h"

extern Image image;

void gdioutput::clearPage(bool autoRefresh, bool keepToolbar) {
  maxTextBlockHeight = getLineHeight();
  animationData.reset();
  renderMap.reset();
  lockUpDown = false;
  hasAnyTimer = false;
  enableTables();
  if (toolbar && !keepToolbar)
    toolbar->hide();

  while (!timers.empty()) {
    KillTimer(hWndTarget, (UINT_PTR)&timers.back());
    timers.back().setWnd = 0;
    timers.back().parent = 0;
    timers.pop_back();
  }

  restorePoints.clear();
  shownStrings.clear();
  onClear.clear();
  FocusList.clear();
  currentFocus = 0;
  TL.clear();
  itTL = TL.end();
  updateImageReferences();

  listDescription.clear();

  if (hWndTarget && autoRefresh)
    InvalidateRect(hWndTarget, NULL, true);

  fillDown();

  hasCleared = true;

  for (ToolList::iterator it = toolTips.begin(); it != toolTips.end(); ++it) {
    if (hWndToolTip) {
      SendMessage(hWndToolTip, TTM_DELTOOL, 0, (LPARAM)&it->ti);
    }
  }
  toolTips.clear();

  {
    list<ButtonInfo>::iterator it;
    for (it = BI.begin(); it != BI.end(); ++it) {
      it->callBack = 0;
      it->setHandler(0);
      DestroyWindow(it->hWnd);
    }
    biByHwnd.clear();
    BI.clear();
  }
  {
    list<InputInfo>::iterator it;
    for (it = II.begin(); it != II.end(); ++it) {
      it->callBack = 0;
      it->setHandler(0);
      DestroyWindow(it->hWnd);
    }
    iiByHwnd.clear();
    II.clear();
  }

  {
    list<ListBoxInfo>::iterator it;
    for (it = LBI.begin(); it != LBI.end(); ++it) {
      it->callBack = 0;
      it->setHandler(0);
      DestroyWindow(it->hWnd);
      if (it->writeLock)
        hasCleared = true;
    }
    lbiByHwnd.clear();
    LBI.clear();
  }

  while (!Tables.empty()) {
    auto t = Tables.front().table;
    Tables.pop_front();
    t->hide(*this);
  }

  DataInfo.clear();
  FocusList.clear();
  Events.clear();

  Rectangles.clear();

  MaxX = scaleLength(60);
  MaxY = scaleLength(100);

  CurrentX = scaleLength(40);
  CurrentY = scaleLength(START_YP);
  SX = CurrentX;
  SY = CurrentY;
  OffsetX = 0;
  OffsetY = 0;

  renderOptimize = true;

  backgroundColor1 = -1;
  backgroundColor2 = -1;
  foregroundColor = -1;
  backgroundImage = -1;


  setRestorePoint();

  if (autoRefresh)
    updateScrollbars();

  auto clsCopy = postClear;
  for (auto& clr : clsCopy) {
    try {
      clr.makeEvent(*this, GUI_POSTCLEAR);
    }
    catch (const meosCancel&) {
    }
    catch (meosException& ex) {
      if (isTestMode)
        throw ex;
      wstring msg = ex.wwhat();
      alert(msg);
    }
    catch (const std::exception& ex) {
      if (isTestMode)
        throw ex;
      string msg(ex.what());
      alert(msg);
    }
  }
  postClear.clear();
  manualUpdate = !autoRefresh;
}

void gdioutput::updateImageReferences() {
  if (imageReferences.size() > 0) {
    imageReferences.clear();
    for (auto& ti : TL) {
      if ((ti.format & 0xFF) == textImage) {
        imageReferences.push_back(&ti);
      }
    }
  }
}

void gdioutput::getWindowText(HWND hWnd, wstring &text)
{
  TCHAR bf[1024];
  TCHAR *bptr=bf;

  int len=GetWindowTextLength(hWnd);

  if (len>1023)
    bptr=new TCHAR[len+1];

  GetWindowText(hWnd, bptr, len+1);
  text=bptr;

  if (len>1023)
    delete[] bptr;
}

BaseInfo& gdioutput::getBaseInfo(const char* id, int requireExtraMatch) const {
  for (auto& ii : II) {
    if (ii.id == id && ii.matchExtra(requireExtraMatch)) {
      return const_cast<InputInfo&>(ii);
    }
  }

  for (auto& lbi : LBI) {
    if (lbi.id == id && lbi.matchExtra(requireExtraMatch)) {
      return const_cast<ListBoxInfo&>(lbi);
    }
  }

  for (auto& bi : BI) {
    if (bi.id == id && bi.matchExtra(requireExtraMatch)) {
      return const_cast<ButtonInfo&>(bi);
    }
  }

  for (auto& tl : TL) {
    if (tl.id == id && tl.matchExtra(requireExtraMatch)) {
      return const_cast<TextInfo&>(tl);
    }
  }

  string err = string("Internal Error, identifier not found: X#") + id;
  throw std::exception(err.c_str());
}

const wstring &gdioutput::getText(const char *id, bool acceptMissing, int requireExtraMatch) const {
  TCHAR bf[1024];
  TCHAR *bptr=bf;

  for(list<InputInfo>::const_iterator it=II.begin();
                                  it != II.end(); ++it){
    if (it->id==id && it->matchExtra(requireExtraMatch)){
      int len=GetWindowTextLength(it->hWnd);

      if (len>1023)
        bptr=new TCHAR[len+1];

      GetWindowText(it->hWnd, bptr, len+1);
      const_cast<wstring&>(it->text)=bptr;

      if (len>1023)
        delete[] bptr;

      return it->text;
    }
  }

  for(list<ListBoxInfo>::const_iterator it=LBI.begin();
                                  it != LBI.end(); ++it){
    if (it->id==id && it->IsCombo && it->matchExtra(requireExtraMatch)){
      if (!it->writeLock) {
        GetWindowText(it->hWnd, bf, 1024);
        const_cast<wstring&>(it->text)=bf;
      }
      return it->text;
    }
  }

  for(list<TextInfo>::const_iterator it=TL.begin();
                                  it != TL.end(); ++it){
    if (it->id==id && it->matchExtra(requireExtraMatch)) {
      return it->text;
    }
  }

#ifdef _DEBUG
  if (!acceptMissing) {
    string err = string("Internal Error, identifier not found: X#") + id;
    throw std::exception(err.c_str());
  }
#endif
  return _EmptyWString;
}

bool gdioutput::hasWidget(const string &id) const
{
  for(list<InputInfo>::const_iterator it=II.begin();
                                  it != II.end(); ++it){
    if (it->id==id)
      return true;
  }

  for(list<ListBoxInfo>::const_iterator it=LBI.begin();
                                  it != LBI.end(); ++it){
    if (it->id==id)
      return true;
  }

  for(list<ButtonInfo>::const_iterator it=BI.begin();
                                  it != BI.end(); ++it){
    if (it->id==id)
      return true;
  }

  for (auto &tl : TL) {
    if (tl.id == id)
      return true;
  }

  return false;
}

int gdioutput::getTextNo(const char *id, bool acceptMissing) const
{
  const wstring &t = getText(id, acceptMissing);
  return _wtoi(t.c_str());
}

BaseInfo *gdioutput::setTextTranslate(const char *id,
                                      const wstring &text,
                                      bool update) {
  return setText(id, lang.tl(text), update);
}

BaseInfo *gdioutput::setTextTranslate(const string &id,
                                      const wstring &text,
                                      bool update) {
  return setText(id, lang.tl(text), update);
}

BaseInfo *gdioutput::setTextTranslate(const char *id,
                                      const wchar_t *text,
                                      bool update) {
  return setText(id, lang.tl(text), update);
}

BaseInfo *gdioutput::setText(const char *id, int number, bool Update) {
  return setText(id, itow(number), Update);
}

BaseInfo* gdioutput::setTextZeroBlank(const char* id, int number, bool update) {
  if (number != 0)
    return setText(id, number, update);
  else
    return setText(id, L"", update);
}

BaseInfo* gdioutput::setText(const char* id, const wstring& text, bool update, int requireExtraMatch, bool updateOriginal) {
  for (auto it = II.begin(); it != II.end(); ++it) {
    if (it->id == id && it->matchExtra(requireExtraMatch)) {
      bool oldWR = it->writeLock;
      it->writeLock = true;
      SetWindowText(it->hWnd, text.c_str());
      it->writeLock = oldWR;
      it->text = text;
      it->synchData();
      if (updateOriginal)
        it->original = text;
      it->focusText = text;
      return &*it;
    }
  }

  for (auto it = LBI.begin(); it != LBI.end(); ++it) {
    if (it->id == id && it->IsCombo && it->matchExtra(requireExtraMatch)) {
      SetWindowText(it->hWnd, text.c_str());
      it->text = text;
      if (updateOriginal)
        it->original = text;
      return &*it;
    }
  }

  for (auto it = BI.begin(); it != BI.end(); ++it) {
    if (it->id == id && it->matchExtra(requireExtraMatch)) {
      SetWindowText(it->hWnd, text.c_str());
      it->text = text;
      return &*it;
    }
  }

  for (auto it = TL.begin(); it != TL.end(); ++it) {
    if (it->id == id && it->matchExtra(requireExtraMatch)) {
      RECT rc = it->textRect;

      it->text = text;
      calcStringSize(*it);

      rc.right = max(it->textRect.right, rc.right);
      rc.bottom = max(it->textRect.bottom, rc.bottom);

      bool changed = updatePos(0, 0, it->textRect.right, it->textRect.bottom);

      if (update && hWndTarget) {
        if (changed)
          InvalidateRect(hWndTarget, 0, true);
        else
          InvalidateRect(hWndTarget, &rc, true);
      }
      return &*it;
    }
  }
  return nullptr;
}

bool gdioutput::insertText(const string& id, const wstring& text) {
  for (list<InputInfo>::iterator it = II.begin();
    it != II.end(); ++it) {
    if (it->id == id) {
      SetWindowText(it->hWnd, text.c_str());
      it->text = text;

      if (it->hasEventHandler())
        it->handleEvent(*this, GUI_INPUT);
      else if (it->callBack)
        it->callBack(this, GUI_INPUT, &*it);

      return true;
    }
  }
  return false;
}

void gdioutput::setData(const string &id, DWORD data) {
  void *pd = (void *)(size_t(data));
  setData(id, pd);
}

void gdioutput::setData(const string &id, void *data)
{
  list<DataStore>::iterator it;
  for(it=DataInfo.begin(); it != DataInfo.end(); ++it){
    if (it->id==id){
      it->data = data;
      return;
    }
  }

  DataStore ds;
  ds.id=id;
  ds.data=data;

  DataInfo.push_front(ds);
  return;
}

bool gdioutput::getData(const string &id, DWORD &data) const
{
  list<DataStore>::const_iterator it;
  for(it=DataInfo.begin(); it != DataInfo.end(); ++it){
    if (it->id==id){
      data=DWORD(size_t(it->data));
      return true;
    }
  }

  data=0;
  return false;
}

void gdioutput::setData(const string &id, const string &data) {
  for (auto &it : DataInfo) {
    if (it.id == id) {
      it.sdata = data;
      return;
    }
  }

  DataStore ds;
  ds.id = id;
  ds.sdata = data;
  DataInfo.push_front(ds);
  return;
}

bool gdioutput::getData(const string &id, string &out) const {
  for (auto &it : DataInfo) {
    if (it.id == id) {
      out = it.sdata;
      return true;
    }
  }
  out.clear();
  return false;
}

void *gdioutput::getData(const string &id) const {
  list<DataStore>::const_iterator it;
  for (it = DataInfo.begin(); it != DataInfo.end(); ++it){
    if (it->id == id){
      return it->data;
    }
  }

  throw meosException("Data X not found#" + id);
}

bool gdioutput::hasData(const char *id) const {
  DWORD dummy;
  return getData(id, dummy);
}

bool gdioutput::updatePosTight(int x, int y, int width, int height, int marginx, int marginy) {
  int ox = MaxX;
  int oy = MaxY;

  MaxX = max(x + width, MaxX);
  MaxY = max(y + height, MaxY);
  bool changed = (ox != MaxX || oy != MaxY);

  if (changed && hWndTarget && !manualUpdate) {
    RECT rc;
    if (ox == MaxX) {
      rc.top = oy - CurrentY - 5;
      rc.bottom = MaxY - CurrentY + scaleLength(50);
      rc.right = 10000;
      rc.left = 0;
      InvalidateRect(hWndTarget, &rc, true);
    }
    else {
      InvalidateRect(hWndTarget, 0, true);
    }
    GetClientRect(hWndTarget, &rc);

    if (MaxX > rc.right || MaxY > rc.bottom) //Update scrollbars
      SendMessage(hWndTarget, WM_SIZE, 0, MAKELONG(rc.right, rc.bottom));
  }

  if (flowDirection == FlowDirection::Down) {
    CurrentY = max(y + height + marginy, CurrentY);
  }
  else if (flowDirection == FlowDirection::Right) {
    CurrentX = max(x + width + marginx, CurrentX);
  }
  return changed;
}

bool gdioutput::updatePos(int x, int y, int width, int height) {
  return updatePosTight(x, y, width, height, 0, 0);
}

void gdioutput::adjustDimension(int width, int height)
{
  int ox = MaxX;
  int oy = MaxY;

  MaxX = width;
  MaxY = height;

  if  ((ox!=MaxX || oy!=MaxY) && hWndTarget && !manualUpdate) {
    RECT rc;
    if (ox == MaxX) {
      rc.top = oy - CurrentY - 5;
      rc.bottom = MaxY - CurrentY + scaleLength(50);
      rc.right = 10000;
      rc.left = 0;
      InvalidateRect(hWndTarget, &rc, true);
    }
    else {
      InvalidateRect(hWndTarget, 0, true);
    }
    GetClientRect(hWndTarget, &rc);

    if (MaxX>rc.right || MaxY>rc.bottom) //Update scrollbars
      SendMessage(hWndTarget, WM_SIZE, 0, MAKELONG(rc.right, rc.bottom));

  }
}

// Alert from main thread (via callback)
void gdioutput::delayAlert(const wstring& msg) {
  if (!delayedAlert.empty())
    delayedAlert += L", ";
  if (delayedAlert.length() > 1000)
    delayedAlert = L"";

  delayedAlert += lang.tl(msg);
  PostMessage(hWndAppMain, WM_USER + 6, 0, LPARAM(this));
}

wstring gdioutput::getDelayedAlert() {
  wstring out = L"#" + delayedAlert;
  delayedAlert.clear();
  return out;
}

void gdioutput::alert(const string &msg) const
{
  alert(widen(msg));
}

void gdioutput::alert(const wstring &msg) const
{
  if (isTestMode) {
    if (!cmdAnswers.empty()) {
      string ans = cmdAnswers.front();
      cmdAnswers.pop_front();
      if (ans == "ok")
        return;
    }
    throw meosException(msg + L"-- ok");
  }

  HWND hFlt = getToolbarWindow();
  if (hasToolbar()) {
    EnableWindow(hFlt, false);
  }
  refreshFast();
  SetForegroundWindow(hWndAppMain);
  setCommandLock();
  try {
    MessageBoxW(hWndAppMain, lang.tl(msg).c_str(), L"MeOS", MB_OK|MB_ICONINFORMATION);
    if (hasToolbar()) {
      EnableWindow(hFlt, true);
    }
    liftCommandLock();
  }
  catch (...) {
    liftCommandLock();
    throw;
  }
}

struct AskDialogInfo {
  wstring btnYes;
  wstring btnNo;
  wstring btnCancel;
  wstring message;
  wstring title;
  int icon = 0;
};

static AskDialogInfo* askDlgPtr = nullptr;

/*INT_PTR CALLBACK askDialogCB(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam) {
  switch (iMsg) {
    case WM_INITDIALOG: {
      const AskDialogInfo& info = *reinterpret_cast<AskDialogInfo*>(lParam);
      SetWindowText(GetDlgItem(hDlg, IDOK), info.btnYes.c_str());
      SetWindowText(GetDlgItem(hDlg, IDCANCEL), info.btnNo.c_str());
      SetWindowText(GetDlgItem(hDlg, IDC_MESSAGETEXT), info.message.c_str());

      return 1;
    }
    break;

    case WM_COMMAND:
      switch (wParam) {
      case IDOK:
        EndDialog(hDlg, true);
        return 0;
      case IDCANCEL:
        EndDialog(hDlg, false);
        return 0;
      }
     
    break; 
  }
  return 0;// DefDlgProc(hDlg, iMsg, wParam, lParam);
}
*/
LRESULT WINAPI hookFn(int code, WPARAM wParam, LPARAM lParam) {
  if (code == HCBT_ACTIVATE) {
    if (askDlgPtr) {
      int movDiff = 0;
      // Modify standard MessageBox button texts    
      auto updateBtnTextPosSize = [&movDiff, wParam](int id, const wstring &text) {
        HWND btn = GetDlgItem((HWND)wParam, id);
        if (text != L"@")
          SetWindowText(btn, text.c_str());
        SIZE sz;
        RECT rc;
        GetWindowRect(btn, &rc);
        
        if (text != L"@")
          Button_GetIdealSize(btn, &sz);
        else {
          sz.cx = rc.right - rc.left;
          sz.cy = rc.bottom - rc.top;
        }

        POINT pt = { rc.left, rc.top };
        ScreenToClient((HWND)wParam, &pt);
        int wdActual = rc.right - rc.left;
        if (wdActual < sz.cx) {
          movDiff += sz.cx - wdActual;
          SetWindowPos(btn, nullptr, pt.x - movDiff, pt.y, sz.cx, rc.bottom - rc.top, SWP_NOZORDER);
        }
        else if (movDiff > 0) {
          SetWindowPos(btn, nullptr, pt.x - movDiff, pt.y, sz.cx, rc.bottom - rc.top, SWP_NOZORDER | SWP_NOSIZE);
        }
      };

      if (!askDlgPtr->btnCancel.empty()) 
        updateBtnTextPosSize(IDCANCEL, askDlgPtr->btnCancel);

      if (!askDlgPtr->btnNo.empty()) 
        updateBtnTextPosSize(IDNO, askDlgPtr->btnNo);
      
      if (!askDlgPtr->btnYes.empty())
        updateBtnTextPosSize(IDYES, askDlgPtr->btnYes);

      askDlgPtr = nullptr;
    }
  }
  return CallNextHookEx(nullptr, code, wParam, lParam);
}

bool gdioutput::ask(const wstring &s, const char* yesButton, const char* noButton) {
  if (isTestMode) {
    if (!cmdAnswers.empty()) {
      string ans = cmdAnswers.front();
      cmdAnswers.pop_front();
      if (ans == "yes")
        return true;
      else if (ans == "no")
        return false;
    }
    throw meosException(s + L"--yes/no");
  }

  setCommandLock();
  SetForegroundWindow(hWndAppMain);
  bool yes;
  HHOOK hook = nullptr;
  try {
    if (yesButton != nullptr || noButton != nullptr) {
      HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE);
      AskDialogInfo info;
      if (!yesButton)
        yesButton = "#@";
      if (!noButton)
        noButton = "#@";
      info.btnYes = lang.tl(yesButton);
      info.btnNo = lang.tl(noButton);
      
      askDlgPtr = &info;
      hook = SetWindowsHookEx(WH_CBT, hookFn, hInst, GetCurrentThreadId());
      yes = MessageBox(hWndAppMain, lang.tl(s).c_str(), L"MeOS", MB_YESNO | MB_ICONQUESTION) == IDYES;
      askDlgPtr = nullptr;
      UnhookWindowsHookEx(hook);
      hook = nullptr;
      //yes = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_ASK), hWndAppMain, askDialogCB, LPARAM((void *) &info));
    }
    else {
      yes = MessageBox(hWndAppMain, lang.tl(s).c_str(), L"MeOS", MB_YESNO | MB_ICONQUESTION) == IDYES;
    }
    liftCommandLock();
  }
  catch (...) {
    if (hook)
      UnhookWindowsHookEx(hook);

    liftCommandLock();
    throw;
  }

  return yes;
}

gdioutput::AskAnswer gdioutput::askCancel(const wstring &s, const char* yesButton, const char* noButton) {
  if (isTestMode) {
    if (!cmdAnswers.empty()) {
      string ans = cmdAnswers.front();
      cmdAnswers.pop_front();
      if (ans == "cancel")
        return AskAnswer::AnswerCancel;
      else if (ans == "yes")
        return AskAnswer::AnswerYes;
      else if (ans == "no")
        return AskAnswer::AnswerNo;
    }
    throw meosException(s + L"--yes/no/cancel");
  }

  int a;
  HHOOK hook = nullptr;
  setCommandLock();
  try {
    SetForegroundWindow(hWndAppMain);
    if (yesButton != nullptr || noButton != nullptr) {
      HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hWndTarget, GWLP_HINSTANCE);
      AskDialogInfo info;
      if (!yesButton)
        yesButton = "#@";
      if (!noButton)
        noButton = "#@";
      info.btnYes = lang.tl(yesButton);
      info.btnNo = lang.tl(noButton);

      askDlgPtr = &info;
      hook = SetWindowsHookEx(WH_CBT, hookFn, hInst, GetCurrentThreadId());
      a = MessageBox(hWndAppMain, lang.tl(s).c_str(), L"MeOS", MB_YESNOCANCEL | MB_ICONQUESTION);
      askDlgPtr = nullptr;
      UnhookWindowsHookEx(hook);
      hook = nullptr;
    }
    else {
      a = MessageBox(hWndAppMain, lang.tl(s).c_str(), L"MeOS", MB_YESNOCANCEL | MB_ICONQUESTION);
    }
    liftCommandLock();
  }
  catch (...) {
    if (hook)
      UnhookWindowsHookEx(hook);

    liftCommandLock();
    throw;
  }

  if (a == IDYES)
    return AskAnswer::AnswerYes;
  else if (a == IDNO)
    return AskAnswer::AnswerNo;
  else
    return AskAnswer::AnswerCancel;
}

gdioutput::AskAnswer gdioutput::askOkCancel(const wstring& s)
{
  if (isTestMode) {
    if (!cmdAnswers.empty()) {
      string ans = cmdAnswers.front();
      cmdAnswers.pop_front();
      if (ans == "cancel")
        return AskAnswer::AnswerCancel;
      else if (ans == "ok")
        return AskAnswer::AnswerOK;
    }
    throw meosException(s + L"--ok/cancel");
  }

  setCommandLock();
  SetForegroundWindow(hWndAppMain);
  int a = MessageBox(hWndAppMain, lang.tl(s).c_str(), L"MeOS", MB_OKCANCEL | MB_ICONINFORMATION);
  liftCommandLock();
  if (a == IDOK)
    return AskAnswer::AnswerOK;
  else
    return AskAnswer::AnswerCancel;
}

void gdioutput::setTabStops(const string& name, int t1, int t2) {
  getInputDimension(0);
  double relTextScale = scale / guiMeasure->avgCharWidth;

  DWORD ptr[2];
  int n = 1;
  //LONG bu=GetDialogBaseUnits();
  //int baseunitX=LOWORD(bu);
  //array[0]=int(t1 * 4.2 * scale) / baseunitX ;
  //array[1]=int(t2 * 4.2 * scale) / baseunitX ;
  ptr[0] = int(t1 * relTextScale * 6.4 * 4.2 / 8.0);
  ptr[1] = int(t2 * relTextScale * 6.4 * 4.2 / 8.0);

  int lastTabStop = 0;
  if (t2 > 0) {
    n = 2;
    lastTabStop = t2;
  }
  else {
    lastTabStop = t1;
  }

  list<ListBoxInfo>::iterator it;
  for (it = LBI.begin(); it != LBI.end(); ++it) {
    if (it->id == name) {
      if (!it->IsCombo) {
        SendMessage(it->hWnd, LB_SETTABSTOPS, n, LPARAM(ptr));
        it->lastTabStop = lastTabStop;
      }
      return;
    }
  }
}

void gdioutput::setInputStatus(const char *id, bool status, bool acceptMissing, int matchExtra) {
  bool hit = false;
  for(list<InputInfo>::iterator it=II.begin(); it != II.end(); ++it)
    if (it->id==id && (matchExtra == -1 || it->getExtraInt() == matchExtra)) {
      EnableWindow(it->hWnd, status);
      hit = true;
    }
  for(list<ListBoxInfo>::iterator it=LBI.begin(); it != LBI.end(); ++it)
    if (it->id==id && (matchExtra == -1 || it->getExtraInt() == matchExtra)) {
      EnableWindow(it->hWnd, status);
      hit = true;
    }
  for(list<ButtonInfo>::iterator it=BI.begin(); it != BI.end(); ++it)
    if (it->id==id && (matchExtra == -1 || it->getExtraInt() == matchExtra)) {
      EnableWindow(it->hWnd, status);
      if (it->isCheckbox) {
        string tid = "T" + it->id;
        for(list<TextInfo>::iterator tit=TL.begin(); tit != TL.end(); ++tit){
          if (tit->id == tid) {
            enableCheckBoxLink(*tit, status);
            break;
          }
        }
      }

      hit = true;
      if (status==false) {
        it->storedFlags |= it->flags;
        it->flags = 0; //Remove default status etc.
      }
      else {
        // Restore flags
        it->flags |= it->storedFlags;
      }
    }

  if (acceptMissing)
    return;
#ifdef _DEBUG
  if (!hit) {
    string err = string("Internal Error, identifier not found: X#") + id;
    throw std::exception(err.c_str());
  }
#endif
}

void gdioutput::refresh() const {
#ifdef DEBUGRENDER
  OutputDebugString("### Full refresh\n");
#endif
  if (hWndTarget) {
    updateScrollbars();
    InvalidateRect(hWndTarget, NULL, true);
    UpdateWindow(hWndTarget);
  }
  screenXYToString.clear();
  stringToScreenXY.clear();
}

void gdioutput::refreshFast() const {
#ifdef DEBUGRENDER
  OutputDebugString("Fast refresh\n");
#endif
  if (hWndTarget) {
    InvalidateRect(hWndTarget, NULL, true);
    UpdateWindow(hWndTarget);
  }
  screenXYToString.clear();
  stringToScreenXY.clear();
}


void gdioutput::takeShownStringsSnapshot() {
#ifdef DEBUGRENDER
  OutputDebugString("** Take snapshot\n");
#endif

  screenXYToString.clear();
  stringToScreenXY.clear();
  snapshotMaxXY.first = MaxX;
  snapshotMaxXY.second = MaxY - OffsetY;
#ifdef DEBUGRENDER
  OutputDebugString(("ymax:" + itos(MaxY-OffsetY) + "\n").c_str());
#endif
  for (size_t k = 0; k < shownStrings.size(); k++) {
    if (shownStrings[k]->hasTimer)
      continue; //Ignore
    int x = shownStrings[k]->xp - OffsetX;
    int y = shownStrings[k]->yp - OffsetY;
    const wstring &str = shownStrings[k]->text;
#ifdef DEBUGRENDER
    //OutputDebugString((itos(k) + ":" + itos(shownStrings[k]->xp) + "," + itos(shownStrings[k]->yp) + "," + str + "\n").c_str());
#endif
    screenXYToString.insert(make_pair(make_pair(x, y), ScreenStringInfo(shownStrings[k]->textRect, str)));
    if (stringToScreenXY.count(str) == 0)
      stringToScreenXY.insert(make_pair(str,make_pair(x, y)));
  }

  RECT rc;
  GetClientRect(hWndTarget, &rc);
  int BoundYup = OffsetY;
  int BoundYdown = OffsetY+rc.bottom;
  for (list<RectangleInfo>::iterator it = Rectangles.begin(); it != Rectangles.end(); ++it) {
    if (it->rc.top <= BoundYdown && it->rc.bottom >= BoundYup) {
      wstring r = L"[R]";
      RECT rect_rc = it->rc;
      OffsetRect(&rect_rc, -OffsetX, -OffsetY);
      screenXYToString.insert(make_pair(make_pair(rect_rc.left, rect_rc.top), ScreenStringInfo(rect_rc, r)));
    }
  }
}

void updateScrollInfo(HWND hWnd, gdioutput &gdi, int nHeight, int nWidth);

void gdioutput::refreshSmartFromSnapshot(bool allowMoveOffset) {
#ifdef DEBUGRENDER
  OutputDebugString("Smart refresh\n");
#endif

  RECT clientRC;
  GetClientRect(hWndTarget, &clientRC);

  updateStringPosCache();

  vector<int> changedStrings;
  bool updateScroll = false;
  if (allowMoveOffset) {
    map< pair<int, int>, int> offsetCount;
    int misses = 0, hits = 0;
    for (size_t k = 0; k < shownStrings.size(); k++) {
      if (shownStrings[k]->hasTimer)
        continue; //Ignore
      int x = shownStrings[k]->xp - OffsetX;
      int y = shownStrings[k]->yp - OffsetY;
      const wstring &str = shownStrings[k]->text;
      map<wstring, pair<int,int> >::const_iterator found = stringToScreenXY.find(str);
      if (found != stringToScreenXY.end()) {
        hits++;
        int ox = found->second.first - x;
        int oy = found->second.second - y;
        ++offsetCount[make_pair(ox, oy)];
        if (hits > 30)
          break;
      }
      else {
        misses++;
        if (misses > 20)
          break;
      }
    }

    // Choose dominating offset, if dominating enough
    pair<int, int> offset(0,0);
    int maxVal = 10; // Require at least 10 hits
    for(map< pair<int, int>, int>::iterator it = offsetCount.begin(); it != offsetCount.end(); ++it) {
      if (it->second > maxVal) {
        maxVal = it->second;
        offset = it->first;
      }
    }

    int maxOffsetY=max<int>(getPageY()-clientRC.bottom, 0);
    int maxOffsetX=max<int>(getPageX()-clientRC.right, 0);
    int noy = OffsetY - offset.second;
    int nox = OffsetX - offset.first;
    if ((offset.first != 0 && nox>0 && nox<maxOffsetX) || (offset.second != 0 && noy>0 && noy<maxOffsetY) ) {
      #ifdef DEBUGRENDER
        OutputDebugString(("Change offset: " + itos(offset.first) + "," + itos(offset.second) + "\n").c_str());
      #endif
      OffsetX -= offset.first;
      OffsetY -= offset.second;
      autoPos -= offset.second;

      if (offset.second != 0) {
        SCROLLINFO si;
        si.cbSize = sizeof(si);
        si.fMask  = SIF_POS;
        si.nPos   = OffsetY;
        SetScrollInfo(hWndTarget, SB_VERT, &si, false);
        updateScroll = true;
      }

      if (offset.first != 0) {
        SCROLLINFO si;
        si.cbSize = sizeof(si);
        si.fMask  = SIF_POS;
        si.nPos   = OffsetX;
        SetScrollInfo(hWndTarget, SB_HORZ, &si, false);
        updateScroll = true;
      }

      updateStringPosCache();
    }
  }
#ifdef DEBUGRENDER
  OutputDebugString(("* ymax:" + itos(MaxY-OffsetY) + "\n").c_str());
#endif

  RECT invalidRect;
  invalidRect.top = 1000000;
  invalidRect.left = 1000000;
  invalidRect.right = -1;
  invalidRect.bottom = -1;
  bool invalid = false;

  for (size_t k = 0; k < shownStrings.size(); k++) {
    if (shownStrings[k]->hasTimer)
      continue; //Ignore

    int x = shownStrings[k]->xp - OffsetX;
    int y = shownStrings[k]->yp - OffsetY;
    const wstring &str = shownStrings[k]->text;
#ifdef DEBUGRENDER
    //OutputDebugString((itos(k) + ":" + itos(shownStrings[k]->xp) + "," + itos(shownStrings[k]->yp) + "," + str + "\n").c_str());
#endif
    map<pair<int, int>, ScreenStringInfo>::iterator res = screenXYToString.find(make_pair(x,y));
    if (res != screenXYToString.end()) {
      res->second.reached = true;
      if (str != res->second.str) {
        if (res->second.rc.bottom >= 0 && res->second.rc.top <= clientRC.bottom) {
          changedStrings.push_back(k);
          invalidRect.top = min(invalidRect.top, res->second.rc.top);
          invalidRect.bottom = max(invalidRect.bottom, res->second.rc.bottom);
          invalidRect.left = min(invalidRect.left, res->second.rc.left);
          invalidRect.right = max(invalidRect.right, res->second.rc.right);
        }
      }
    }
    else
      changedStrings.push_back(k);
  }

  RECT rc;
  GetClientRect(hWndTarget, &rc);
  int BoundYup = OffsetY;
  int BoundYdown = OffsetY+rc.bottom;
  for (list<RectangleInfo>::iterator it = Rectangles.begin(); it != Rectangles.end(); ++it) {
    if (it->rc.top <= BoundYdown && it->rc.bottom >= BoundYup) {
      RECT rect_rc = it->rc;
      OffsetRect(&rect_rc, -OffsetX, -OffsetY);

      map<pair<int, int>, ScreenStringInfo>::iterator res = screenXYToString.find(make_pair(rect_rc.left, rect_rc.top));
      bool add = false;
      if (res != screenXYToString.end()) {
        res->second.reached = true;
        if (!EqualRect(&rect_rc, &res->second.rc)) {
          add = true;
          invalidRect.top = min(invalidRect.top, res->second.rc.top);
          invalidRect.bottom = max(invalidRect.bottom, res->second.rc.bottom);
          invalidRect.left = min(invalidRect.left, res->second.rc.left);
          invalidRect.right = max(invalidRect.right, res->second.rc.right);
        }
      }
      else
        add = true;

      if (add) {
        invalid = true;
        invalidRect.top = min(invalidRect.top, rect_rc.top);
        invalidRect.bottom = max(invalidRect.bottom, rect_rc.bottom);
        invalidRect.left = min(invalidRect.left, rect_rc.left);
        invalidRect.right = max(invalidRect.right, rect_rc.right);
      }
    }
  }

  for (map<pair<int, int>, ScreenStringInfo>::iterator it = screenXYToString.begin(); it != screenXYToString.end(); ++it) {
    if (!it->second.reached) {
      invalid = true;
      invalidRect.top = min(invalidRect.top, it->second.rc.top);
      invalidRect.bottom = max(invalidRect.bottom, it->second.rc.bottom);
      invalidRect.left = min(invalidRect.left, it->second.rc.left);
      invalidRect.right = max(invalidRect.right, it->second.rc.right);
    }
  }

  screenXYToString.clear();
  stringToScreenXY.clear();

  if (snapshotMaxXY.second != MaxY - OffsetY) {
    // We added (or removed) a row. Add result to list is typical case.
    int currentMaxP = (MaxY - OffsetY);
    int oldMaxP = snapshotMaxXY.second;
    bool bottomVisible = ((currentMaxP < clientRC.bottom + 15) && currentMaxP > -15) ||
                           ((oldMaxP < clientRC.bottom + 15) && oldMaxP > -15);
    if (bottomVisible && !highContrast && oldMaxP != currentMaxP) {
      invalid = true;
      invalidRect.top = min<int>(invalidRect.top, oldMaxP-15);
      invalidRect.top = min<int>(invalidRect.top, currentMaxP-15);

      invalidRect.bottom = max<int>(invalidRect.bottom, oldMaxP+15);
      invalidRect.bottom = max<int>(invalidRect.bottom, currentMaxP+15);

      invalidRect.left = 0;
      invalidRect.right = clientRC.right;
      #ifdef DEBUGRENDER
        OutputDebugString("Extend Y\n");
      #endif
    }
    updateScroll = true;
  }

  if (snapshotMaxXY.first != MaxX) {
    // This almost never happens
    invalidRect = clientRC;
    invalid = true;
    updateScroll;
  }

  if (updateScroll) {
    bool hc = highContrast;
    highContrast = false;
    updateScrollInfo(hWndTarget, *this, clientRC.bottom, clientRC.right); // No throw
    highContrast = hc;
  }
  if (changedStrings.empty() && !invalid) {
    #ifdef DEBUGRENDER
      //breakRender = true;
      OutputDebugString("*** NO CHANGE\n");
    #endif

    return;
  }

  for (size_t k = 0; k< changedStrings.size(); k++) {
    TextInfo &ti = *shownStrings[changedStrings[k]];
    invalidRect.top = min(invalidRect.top, ti.textRect.top);
    invalidRect.bottom = max(invalidRect.bottom, ti.textRect.bottom);
    invalidRect.left = min(invalidRect.left, ti.textRect.left);
    invalidRect.right = max(invalidRect.right, ti.textRect.right);
  }


  if (invalidRect.bottom<0 || invalidRect.right < 0
    || invalidRect.top > clientRC.bottom || invalidRect.left > clientRC.right) {

    #ifdef DEBUGRENDER
      //breakRender = true;
      OutputDebugString("*** EMPTY CHANGE\n");
    #endif

    return;
  }

  if (hWndTarget) {
    //InvalidateRect(hWndTarget, &invalidRect, true);
    //UpdateWindow(hWndTarget);
    HDC hDC = GetDC(hWndTarget);
    IntersectClipRect(hDC, invalidRect.left, invalidRect.top, invalidRect.right, invalidRect.bottom);
    //debugDrawColor = RGB((30*counterRender)%256,0,0);
    draw(hDC, clientRC, invalidRect);
    //debugDrawColor = 0;

    ReleaseDC(hWndTarget, hDC);
  }
}

void gdioutput::removeString(string id) {
  int cnt = 0;
  for (auto it = TL.begin(); it != TL.end(); ++it, ++cnt) {
    if (it->id == id) {
      InvalidateRect(hWndTarget, &it->textRect, true);
      TL.erase(it);
      itTL = TL.end();
      shownStrings.clear();

      updateImageReferences();

      // Update restorepoints
      for (auto& rp : restorePoints) {
        if (rp.second.nTL > cnt)
          rp.second.nTL--;
      }
      return;
    }
  }
}

bool gdioutput::selectFirstItem(const string& id) {
  for (auto it = LBI.begin(); it != LBI.end(); ++it)
    if (it->id == id) {
      bool ret;
      if (it->IsCombo)
        ret = SendMessage(it->hWnd, CB_SETCURSEL, 0, 0) >= 0;
      else
        ret = SendMessage(it->hWnd, LB_SETCURSEL, 0, 0) >= 0;
      getSelectedItem(*it);
      it->original = it->text;
      it->originalIdx = it->data;
    }

  return false;
}

void gdioutput::setWindowTitle(const wstring &title)
{
  if (title.length()>0) {
    wstring titlew = title + makeDash(L" - MeOS");
    SetWindowText(hWndAppMain, titlew.c_str());
  }
  else SetWindowText(hWndAppMain, L"MeOS");
}

void gdioutput::setWaitCursor(bool wait)
{
  if (wait)
    SetCursor(LoadCursor(NULL, IDC_WAIT));
  else
    SetCursor(LoadCursor(NULL, IDC_ARROW));
}

struct FadeInfo
{
  TextInfo ti;
  uint64_t Start;
  uint64_t End;
  HWND hWnd;
  COLORREF StartC;
  COLORREF EndC;
};

void TextFader(void *f)
{
  FadeInfo *fi=(FadeInfo *)f;
  HDC hDC=GetDC(fi->hWnd);

  SelectObject(hDC, GetStockObject(DEFAULT_GUI_FONT));
  SetBkMode(hDC, TRANSPARENT);

  double p=0;

  double r1=GetRValue(fi->StartC);
  double g1=GetGValue(fi->StartC);
  double b1=GetBValue(fi->StartC);

  double r2=GetRValue(fi->EndC);
  double g2=GetGValue(fi->EndC);
  double b2=GetBValue(fi->EndC);

  while(p<1)
  {
    p=double(GetTickCount64()-fi->Start)/double(fi->End-fi->Start);

    if (p>1) p=1;

    p=1-(p-1)*(p-1);

    int red=int((1-p)*r1+(p)*r2);
    int green=int((1-p)*g1+(p)*g2);
    int blue=int((1-p)*b1+(p)*b2);
    //int green=int((p-1)*GetGValue(fi->StartC)+(p)*GetGValue(fi->EndC));
    //int blue=int((p-1)*GetBValue(fi->StartC)+(p)*GetBValue(fi->EndC));

    SetTextColor(hDC, RGB(red, green, blue));
    TextOut(hDC, fi->ti.xp, fi->ti.yp, fi->ti.text.c_str(), fi->ti.text.length());
    Sleep(30);
    //char bf[10];
    //fi->ti.text=fi->ti.text+itoa(red, bf, 16);
  }

  ReleaseDC(fi->hWnd, hDC);
  delete fi;
}

void gdioutput::fadeOut(string Id, int ms)
{
  list<TextInfo>::iterator it;
  for(it=TL.begin(); it != TL.end(); ++it){
    if (it->id==Id){
      FadeInfo *fi=new FadeInfo;
      fi->Start=GetTickCount64();
      fi->End=fi->Start+ms;
      fi->ti=*it;
      fi->StartC=RGB(0, 0, 0);
      fi->EndC=GetSysColor(COLOR_WINDOW);
      fi->hWnd=hWndTarget;
      _beginthread(TextFader, 0, fi);
      TL.erase(it);
      return;
    }
  }
}

void gdioutput::RenderString(TextInfo &ti, HDC hDC) {
  if (skipTextRender(ti.format))
    return;

  if (ti.hasTimer && ti.xp == 0)
    return;

  HDC hThis=0;

  if (!hDC){
    assert(hWndTarget!=0);
    hDC=hThis=GetDC(hWndTarget);
  }
  RECT rc;
  if ((ti.format & absolutePosition) == 0) {
    rc.left = ti.xp - OffsetX;
    rc.top = ti.yp - OffsetY;
  }
  else {
    rc.left = ti.xp;
    rc.top = ti.yp;
  }
  rc.right = rc.left;
  rc.bottom = rc.top;

  formatString(ti, hDC);
  int format=ti.format&0xFF;
  if (format == textImage) {
    // Image
    int id = _wtoi(ti.text.c_str());
    bool fixedRect = false;
    int h = 16, w = 16;
    bool setWH = false;
    if (id > 0) {
      image.loadImage(id, Image::ImageMethod::Default);
      w = image.getWidth(id);
      h = image.getHeight(id);
      setWH = true;
      image.drawImage(id, Image::ImageMethod::Default, hDC, rc.left, rc.top, w, h,
                      ti.srcRect.left, ti.srcRect.top,
                      ti.srcRect.right - ti.srcRect.left,
                      ti.srcRect.bottom - ti.srcRect.top);
    }
    else if (ti.text.size()>1) {      
      if (ti.text[0] == 'S') { // Icon
        setWH = true;
        w = getLineHeight();
        h = getLineHeight();
      }
      else if (ti.text[0] == 'L') {
        fixedRect = true;
        uint64_t imgId = _wcstoui64(ti.text.c_str() + 1, nullptr, 10);
        if (imgId > 0) {
          w = ti.textRect.right - ti.textRect.left;
          h = ti.textRect.bottom - ti.textRect.top;
          image.drawImage(imgId, Image::ImageMethod::Default, hDC, rc.left, rc.top, w, h, 
                          ti.srcRect.left, ti.srcRect.top,
                          ti.srcRect.right - ti.srcRect.left,
                          ti.srcRect.bottom - ti.srcRect.top);
        }
      }
    }
    if (!fixedRect) {
      ti.textRect.left = rc.left;
      ti.textRect.top = rc.top;
      if (setWH) {
        ti.textRect.right = rc.left + w + 5;
        ti.textRect.bottom = rc.bottom + h + 5;
      }
    }
  }
  else if (format != 10 && (breakLines&ti.format) == 0) {
    if (ti.xlimit == 0) {
      if (ti.format&textRight) {
        DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_CALCRECT | DT_NOPREFIX);
        int dx = rc.right - rc.left;
        ti.realWidth = dx;
        rc.right -= dx;
        rc.left -= dx;
        ti.textRect = rc;
        DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_RIGHT | DT_NOCLIP | DT_NOPREFIX);
      }
      else if (ti.format&textCenter) {
        DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_CENTER | DT_CALCRECT | DT_NOPREFIX);
        int dx = rc.right - rc.left;
        ti.realWidth = dx;
        rc.right -= dx / 2;
        rc.left -= dx / 2;
        ti.textRect = rc;
        DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_CENTER | DT_NOCLIP | DT_NOPREFIX);
      }
      else {
        DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_LEFT | DT_CALCRECT | DT_NOPREFIX);
        ti.textRect = rc;
        ti.realWidth = rc.right - rc.left;
        DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_LEFT | DT_NOCLIP | DT_NOPREFIX);
      }
    }
    else {
      int flags = DT_NOPREFIX;
      if (ti.format & textLimitEllipsis)
        flags = DT_END_ELLIPSIS;

      DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_CALCRECT | flags);
      ti.realWidth = rc.right - rc.left;
      if (ti.format&textRight) {
        rc.right = rc.left + ti.xlimit - (rc.bottom - rc.top) / 2;
        DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_RIGHT | flags);
      }
      else if (ti.format&textCenter) {
        rc.right = rc.left + ti.xlimit - (rc.bottom - rc.top) / 2;
        DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_CENTER | flags);
      }
      else {
        rc.right = rc.left + ti.xlimit;
        DrawText(hDC, ti.text.c_str(), -1, &rc, DT_LEFT | flags);
      }
      ti.textRect = rc;
    }
  }
  else {
    memset(&rc, 0, sizeof(rc));
    int width =  scaleLength( (breakLines&ti.format) ? ti.xlimit : 450 );
    rc.right = width;
    int dx = format != 10 ? 0 : scaleLength(20);
    ti.realWidth = width + dx;
    DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_CALCRECT|DT_LEFT|DT_NOPREFIX|DT_WORDBREAK);
    ti.textRect=rc;
    ti.textRect.right+=ti.xp+dx;
    ti.textRect.left+=ti.xp;
    ti.textRect.top+=ti.yp;
    ti.textRect.bottom+=ti.yp+dx;

    if (format == 10) {
      DWORD c = colorLightYellow;// GetSysColor(COLOR_INFOBK);
      double red=GetRValue(c);
      double green=GetGValue(c);
      double blue=GetBValue(c);

      double blue1=min(255., blue*1.05);
      double green1=min(255., green*1.05);
      double red1=min(255., red*1.05);

      TRIVERTEX vert[2];
      vert [0] .x      = ti.xp-OffsetX;
      vert [0] .y      = ti.yp-OffsetY;
      vert [0] .Red    = 0xff00&DWORD(red1*256);
      vert [0] .Green  = 0xff00&DWORD(green1*256);
      vert [0] .Blue   = 0xff00&DWORD(blue1*256);
      vert [0] .Alpha  = 0x0000;

      vert [1] .x      = ti.xp+rc.right+dx-OffsetX;
      vert [1] .y      = ti.yp+rc.bottom+dx-OffsetY;
      vert [1] .Red    = 0xff00&DWORD(red*256);
      vert [1] .Green  = 0xff00&DWORD(green*256);
      vert [1] .Blue   = 0xff00&DWORD(blue*256);
      vert [1] .Alpha  = 0x0000;

      GRADIENT_RECT gr[1];
      gr[0].UpperLeft=0;
      gr[0].LowerRight=1;

      GradientFill(hDC,vert, 2, gr, 1,GRADIENT_FILL_RECT_H);
      SelectObject(hDC, GetStockObject(NULL_BRUSH));
      SelectObject(hDC, GetStockObject(DC_PEN));
      SetDCPenColor(hDC, RGB(DWORD(red*0.5),
                             DWORD(green*0.5),
                             DWORD(blue*0.5)));

      Rectangle(hDC, vert[0].x, vert[0].y, vert[1].x, vert[1].y);

      SetDCPenColor(hDC, RGB(DWORD(min(255., red*1.1)),
                             DWORD(min(255., green*1.2)),
                             DWORD(min(255., blue))));
      POINT pt;
      MoveToEx(hDC, vert[0].x-1, vert[1].y, &pt);
      LineTo(hDC, vert[0].x-1, vert[0].y-1);
      LineTo(hDC, vert[1].x, vert[0].y-1);

      SetDCPenColor(hDC, RGB(DWORD(min(255., red*0.4)),
                       DWORD(min(255., green*0.4)),
                       DWORD(min(255., blue*0.4))));

      MoveToEx(hDC, vert[1].x+0, vert[0].y, &pt);
      LineTo(hDC, vert[1].x+0, vert[1].y+0);
      LineTo(hDC, vert[0].x, vert[1].y+0);

    }
    dx/=2;
    rc.top=ti.yp+dx-OffsetY;
    rc.left=ti.xp+dx-OffsetX;
    rc.bottom+=ti.yp+dx-OffsetY;
    rc.right=ti.xp+dx+width-OffsetX;

    SetTextColor(hDC, 0);
    DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_LEFT|DT_NOPREFIX|DT_WORDBREAK);
  }

  if (hThis)
    ReleaseDC(hWndTarget, hDC);
}

void gdioutput::RenderString(TextInfo &ti, const wstring &text, HDC hDC)
{
  if (skipTextRender(ti.format))
    return;

  RECT rc;
  if ((ti.format & absolutePosition) == 0) {
    rc.left = ti.xp - OffsetX;
    rc.top = ti.yp - OffsetY;
  }
  else {
    rc.left = ti.xp;
    rc.top = ti.yp;
  }
  rc.right = rc.left;
  rc.bottom = rc.top;

  int format=ti.format&0xFF;
  assert(format!=10);
  formatString(ti, hDC);

  if (ti.xlimit==0){
    if (ti.format&textRight) {
      DrawText(hDC, text.c_str(), text.length(), &rc, DT_CALCRECT|DT_NOPREFIX);
      int dx=rc.right-rc.left;
      rc.right-=dx;
      rc.left-=dx;
      ti.textRect=rc;
      DrawText(hDC, text.c_str(), text.length(), &rc, DT_RIGHT|DT_NOCLIP|DT_NOPREFIX);
    }
    else if (ti.format&textCenter) {
      DrawText(hDC, text.c_str(), text.length(), &rc, DT_CENTER|DT_CALCRECT|DT_NOPREFIX);
      int dx=rc.right-rc.left;
      rc.right-=dx/2;
      rc.left-=dx/2;
      ti.textRect=rc;
      DrawText(hDC, text.c_str(), text.length(), &rc, DT_CENTER|DT_NOCLIP|DT_NOPREFIX);
    }
    else{
      DrawText(hDC, text.c_str(), text.length(), &rc, DT_LEFT|DT_CALCRECT|DT_NOPREFIX);
      ti.textRect=rc;
      DrawText(hDC, text.c_str(), text.length(), &rc, DT_LEFT|DT_NOCLIP|DT_NOPREFIX);
    }
  }
  else{
    if (ti.format&textRight) {
      DrawText(hDC, text.c_str(), text.length(), &rc, DT_LEFT|DT_CALCRECT|DT_NOPREFIX);
      rc.right = rc.left + ti.xlimit;
      ti.textRect = rc;
      DrawText(hDC, text.c_str(), text.length(), &rc, DT_RIGHT|DT_NOPREFIX);
    }
    else {
      DrawText(hDC, text.c_str(), text.length(), &rc, DT_LEFT|DT_CALCRECT|DT_NOPREFIX);
      rc.right=rc.left+ti.xlimit;
      DrawText(hDC, text.c_str(), text.length(), &rc, DT_LEFT|DT_NOPREFIX);
      ti.textRect=rc;
    }
  }
}

void gdioutput::resetLast() const {
  lastFormet = -1;
  lastActive = false;
  lastHighlight = false;
  lastColor = -1;
  lastFont.clear();
}

void gdioutput::getFontInfo(const TextInfo &ti, FontInfo &fi) const {
  if (ti.font.empty()) {
    fi.name = 0;
    fi.bold = fi.normal = fi.italic = 0;
  }
  else {
    fi.name = &ti.font;
    getFont(ti.font).getInfo(fi);
  }
}


void gdioutput::formatString(const TextInfo &ti, HDC hDC) const
{
  int format=ti.format&0xFF;

  if (lastFormet == format &&
      lastActive == ti.active &&
      lastHighlight == ti.highlight &&
      lastColor == ti.color &&
      ti.font == lastFont)
    return;

  if (ti.font.empty()) {
    getCurrentFont().selectFont(hDC, format);
    lastFont.clear();
  }
  else {
    getFont(ti.font).selectFont(hDC, format);
    lastFont = ti.font;
  }

  SetBkMode(hDC, TRANSPARENT);

  if (ti.active)
    SetTextColor(hDC, RGB(255,0,0));
  else if (ti.highlight)
    SetTextColor(hDC, RGB(64,64,128));
  else if (ti.color == 0 && foregroundColor != -1)
    SetTextColor(hDC, foregroundColor);
  else
    SetTextColor(hDC, ti.color);
}

void gdioutput::calcStringSize(TextInfo &ti, HDC hDC_in) const {

  RECT rc;
  rc.left=ti.xp-OffsetX;
  rc.top=ti.yp-OffsetY;
  rc.right = rc.left;
  rc.bottom = rc.top;

  if ((ti.format & 0xFF) == textImage) {
    // Image
    int id = _wtoi(ti.text.c_str());
    int w = 16, h = 16;
    if (id > 0) {
      w = image.getWidth(id);
      h = image.getHeight(id);
    }
    else if (ti.text.size()>1 && ti.text[0] == 'S') { // Icon
      w = _wtoi(ti.text.c_str() + 1);
      h = getLineHeight();
    }
    else if (ti.text[0] == 'L') {
      return;
    }

    ti.textRect.left = rc.left;
    ti.textRect.right = rc.left + w + 5;
    ti.textRect.top = rc.top;
    ti.textRect.bottom = rc.bottom + h + 5;
    return;
  }

  HDC hDC = hDC_in;

  if (!hDC) {
    //    assert(hWndTarget!=0);
    hDC = GetDC(hWndTarget);
  }
  resetLast();
  formatString(ti, hDC);
  int format=ti.format&0xFF;

  if (format != 10 && (breakLines&ti.format) == 0) {
    if (ti.xlimit==0){
      if (ti.format&textRight) {
        DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_CALCRECT|DT_NOPREFIX);
        int dx=rc.right-rc.left;
        ti.realWidth = dx;
        rc.right-=dx;
        rc.left-=dx;
        ti.textRect=rc;
      }
      else if (ti.format&textCenter) {
        DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_CENTER|DT_CALCRECT|DT_NOPREFIX);
        int dx=rc.right-rc.left;
        ti.realWidth = dx;
        rc.right-=dx/2;
        rc.left-=dx/2;
        ti.textRect=rc;
      }
      else{
        DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_LEFT|DT_CALCRECT|DT_NOPREFIX);
        ti.realWidth = rc.right - rc.left;
        ti.textRect=rc;
      }
    }
    else {
      DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_LEFT|DT_CALCRECT|DT_NOPREFIX);
      ti.realWidth = rc.right - rc.left;
      rc.right=rc.left+ti.xlimit;
      ti.textRect=rc;
    }
  }
  else {
    memset(&rc, 0, sizeof(rc));
    rc.right = scaleLength( (breakLines&ti.format) ? ti.xlimit : 450 );
    int dx = format != 10 ? 0 : scaleLength(20);
    ti.realWidth = rc.right + dx;
    DrawText(hDC, ti.text.c_str(), ti.text.length(), &rc, DT_CALCRECT|DT_LEFT|DT_NOPREFIX|DT_WORDBREAK);
    ti.textRect=rc;
    ti.textRect.right+=ti.xp+dx;
    ti.textRect.left+=ti.xp;
    ti.textRect.top+=ti.yp;
    ti.textRect.bottom+=ti.yp+dx;
  }

  if (!hDC_in)
    ReleaseDC(hWndTarget, hDC);
}


void gdioutput::updateScrollbars() const {
  RECT rc;
  GetClientRect(hWndTarget, &rc);
  SendMessage(hWndTarget, WM_SIZE, 0, MAKELONG(rc.right, rc.bottom));
}


void gdioutput::setOffsetY(int oy) { 
  bool changed = OffsetY != oy;
  OffsetY = oy; 
  if (changed)
    updateToolTips();
}

void gdioutput::setOffsetX(int ox) {
  bool changed = OffsetX != ox;
  OffsetX = ox; 
  if (changed) 
    updateToolTips();
}

void gdioutput::updateToolTips() {
  for (auto& tt : toolTips) {
    if (tt.hasRect) {
      tt.ti.rect.top = tt.rc.top - OffsetY;
      tt.ti.rect.bottom = tt.rc.bottom - OffsetY;
      tt.ti.rect.left = tt.rc.left - OffsetX;
      tt.ti.rect.right = tt.rc.right - OffsetX;
      SendMessage(hWndToolTip, TTM_NEWTOOLRECTW, 0, (LPARAM)&tt.ti);
    }
  }
}

void gdioutput::updateObjectPositions() {
  for (auto it = BI.begin(); it != BI.end(); ++it) {
    if (!it->AbsPos)
      SetWindowPos(it->hWnd, 0, it->xp - OffsetX, it->yp - OffsetY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOCOPYBITS);
  }
  for (auto it = II.begin(); it != II.end(); ++it)
    SetWindowPos(it->hWnd, 0, it->xp - OffsetX, it->yp - OffsetY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

  for (auto it = LBI.begin(); it != LBI.end(); ++it) {
    SetWindowPos(it->hWnd, 0, it->xp - OffsetX, it->yp - OffsetY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
  }
  updateToolTips();
}

constexpr int BoxWidthLimit = 250;
constexpr int NumBoxLimit = 10;

InfoBox &gdioutput::addInfoBox(const string &id, const wstring &text,
                               const wstring& extraLine, BoxStyle style, 
                               int timeOut, GUICALLBACK cb, bool autoRefresh) {
  InfoBox box;

  box.id = id;
  box.callBack = cb;
  box.text = lang.tl(text);
  box.underLine = lang.tl(extraLine);
  box.style = style;

  if (timeOut > 0)
    box.timeOut = GetTickCount64() + timeOut;

  IBox.push_back(box);

  if (autoRefresh && IBox.size() <= NumBoxLimit) {
    RECT rc;
    computeBoxesBoundingBox(rc);
    InvalidateRect(hWndTarget, &rc, true);
  }
  return IBox.back();
}

void gdioutput::drawBox(HDC hDC, InfoBox& box, RECT& pos) {
  getCurrentFont().selectFont(hDC, 0);
  lastFont.clear();

  SetBkMode(hDC, TRANSPARENT);

  //Calculate size.
  RECT testrect = { 0,0,0,0 };
  
  DrawText(hDC, box.text.c_str(), box.text.length(), &testrect, DT_CALCRECT | DT_LEFT | DT_NOPREFIX | DT_SINGLELINE);

  int limit = scaleLength(BoxWidthLimit);
  int ulimit = scaleLength(80);

  if (testrect.right > limit || box.text.find_first_of('\n') != string::npos) {
    testrect.right = limit;
    DrawText(hDC, box.text.c_str(), box.text.length(), &testrect, DT_CALCRECT | DT_LEFT | DT_NOPREFIX | DT_WORDBREAK);
  }
  else if (testrect.right < ulimit)
    testrect.right = ulimit;


  RECT extraRect = { 0,0,0,0 };
  if (!box.underLine.empty()) {
    getCurrentFont().selectFont(hDC, 1);
    DrawText(hDC, box.underLine.c_str(), box.underLine.length(), &extraRect, DT_CALCRECT | DT_LEFT | DT_NOPREFIX | DT_SINGLELINE);
    extraRect.bottom += scaleLength(4);
  }

  int width = max(testrect.right, extraRect.right);
  int height = testrect.bottom + extraRect.bottom;

  pos.left = pos.right - (width + scaleLength(22));
  pos.top = pos.bottom - (height + scaleLength(20));
  
 
  box.boundingBox = pos;

  //Close Box
  RECT Close;
  Close.top = pos.top + 3;
  Close.bottom = Close.top + scaleLength(11);
  Close.right = pos.right - 3;
  Close.left = Close.right - scaleLength(11);

  box.close = Close;
  
  RECT tr = pos;

  tr.left += scaleLength(10);
  tr.right -= scaleLength(10);
  tr.top += scaleLength(15);
  tr.bottom -= scaleLength(5);
  box.textRect = tr;
  int extraYP = tr.top + testrect.bottom + scaleLength(4);

  box.underlineY = extraYP;
  
  drawBoxBg(hDC, box);
  drawCloseBox(hDC, box.close, false);
  drawBoxText(hDC, box, false);
}

void gdioutput::drawBoxBg(HDC hDC, const InfoBox& box) const {
  DWORD c;
  if (box.style == BoxStyle::HeaderWarning)
   c = colorLightRed;
  else
   c = GetSysColor(COLOR_INFOBK);

  double red = GetRValue(c);
  double green = GetGValue(c);
  double blue = GetBValue(c);

  double blue1 = min(255., blue * 1.1);
  double green1 = min(255., green * 1.1);
  double red1 = min(255., red * 1.1);

  TRIVERTEX vert[2];
  vert[0].x = box.boundingBox.left;
  vert[0].y = box.boundingBox.top;
  vert[0].Red = 0xff00 & DWORD(red * 256);
  vert[0].Green = 0xff00 & DWORD(green * 256);
  vert[0].Blue = 0xff00 & DWORD(blue * 256);
  vert[0].Alpha = 0x0000;

  vert[1].x = box.boundingBox.right;
  vert[1].y = box.boundingBox.bottom;
  vert[1].Red = 0xff00 & DWORD(red1 * 256);
  vert[1].Green = 0xff00 & DWORD(green1 * 256);
  vert[1].Blue = 0xff00 & DWORD(blue1 * 256);
  vert[1].Alpha = 0x0000;

  GRADIENT_RECT gr[1];

  gr[0].UpperLeft = 0;
  gr[0].LowerRight = 1;

  //if (MaxY>500)
  GradientFill(hDC, vert, 2, gr, 1, GRADIENT_FILL_RECT_V);


  //HPEN pen = CreatePen(PS_SOLID, scaleLength(2), RGB(0, 0, 0));
  SelectObject(hDC, GetStockObject(NULL_BRUSH));
  SelectObject(hDC, GetStockObject(BLACK_PEN));
  Rectangle(hDC, box.boundingBox.left, box.boundingBox.top, box.boundingBox.right, box.boundingBox.bottom);


  SelectObject(hDC, GetStockObject(DC_PEN));

  SetDCPenColor(hDC, RGB(DWORD(min(255., red * 1.1)),
    DWORD(min(255., green * 1.2)),
    DWORD(min(255., blue))));
  POINT pt;
  MoveToEx(hDC, vert[0].x - 1, vert[1].y, &pt);
  LineTo(hDC, vert[0].x - 1, vert[0].y - 1);
  LineTo(hDC, vert[1].x, vert[0].y - 1);

  SetDCPenColor(hDC, RGB(DWORD(min(255., red * 0.4)),
    DWORD(min(255., green * 0.4)),
    DWORD(min(255., blue * 0.4))));

  MoveToEx(hDC, vert[1].x + 0, vert[0].y, &pt);
  LineTo(hDC, vert[1].x + 0, vert[1].y + 0);
  LineTo(hDC, vert[0].x, vert[1].y + 0);
}

void gdioutput::computeBoxesBoundingBox(RECT& rc) const {
  RECT clientRC;
  GetClientRect(hWndTarget, &clientRC);

  rc.left = clientRC.right;
  rc.right = clientRC.right;
  rc.bottom = clientRC.bottom;
  rc.top = clientRC.bottom;
  
  auto it = IBox.begin();
  int maxNumBox = NumBoxLimit;
  while (it != IBox.end() && --maxNumBox > 0) {
    if (it->boundingBox.right > 0) {
      rc.left = min(rc.left, it->boundingBox.left);
      rc.top = it->boundingBox.top;
    }
    else {
      rc.left = min(rc.left, clientRC.right - scaleLength(BoxWidthLimit+30));
      rc.top -= scaleLength(BoxWidthLimit / 2);
    } 
    ++it;
  }
}

void gdioutput::drawBoxes(HDC hDC, RECT &rc) {
  RECT pos;
  pos.right=rc.right;
  pos.bottom=rc.bottom;

  auto it=IBox.begin();
  int maxNumBox = NumBoxLimit;
  while (it != IBox.end() && --maxNumBox > 0) {
    drawBox(hDC, *it, pos);
    pos.bottom = pos.top;
    ++it;
  }
}

void gdioutput::drawCloseBox(HDC hDC, RECT &Close, bool pressed)
{
  HPEN hPen = CreatePen(PS_SOLID, int(scale * 1.5), 0);
  if (!pressed) 
    SelectObject(hDC, GetStockObject(WHITE_BRUSH));
  else 
    SelectObject(hDC, GetStockObject(LTGRAY_BRUSH));
  
  SelectObject(hDC, hPen);
    
  //Close Box
  Rectangle(hDC, Close.left, Close.top, Close.right, Close.bottom);

  MoveToEx(hDC, Close.left+1, Close.top+1, 0);
  LineTo(hDC, Close.right-2, Close.bottom-2);

  MoveToEx(hDC, Close.right-2, Close.top+1, 0);
  LineTo(hDC, Close.left+1, Close.bottom-2);

  SelectObject(hDC, GetStockObject(BLACK_PEN));
  DeleteObject(hPen);
}

void gdioutput::drawBoxText(HDC hDC, const InfoBox &box, bool highlight) {
  getCurrentFont().selectFont(hDC, 0);
  SetBkMode(hDC, TRANSPARENT);

  if (highlight) {
    SetTextColor(hDC, colorGreyBlue);
  }
  else {
    SetTextColor(hDC, GetSysColor(COLOR_INFOTEXT));
  }
  bool asHead = !box.underLine.empty() && box.style != BoxStyle::SubLine;

  RECT rc = box.textRect;
  
  if (asHead) {
    // Swap header/underline
    int diff = getLineHeight()+scaleLength(2);
    rc.top += diff;
    rc.bottom += diff;
  }
  
  DrawText(hDC, box.text.c_str(), box.text.length(), &rc, DT_LEFT | DT_NOPREFIX | DT_WORDBREAK);

  if (!box.underLine.empty()) {
    getCurrentFont().selectFont(hDC, 1);
    SetTextColor(hDC, GetSysColor(COLOR_INFOTEXT));

    RECT tr2 = box.textRect;
    if (!asHead)
      tr2.top = box.underlineY;

    DrawText(hDC, box.underLine.c_str(), box.underLine.length(), &tr2, DT_LEFT | DT_NOPREFIX);
  }
}

bool gdioutput::removeFirstInfoBox(const string& id) {
  auto it = IBox.begin();

  while (it != IBox.end()) {
    if (it->id == id) {
      IBox.erase(it);
      return true;
    }
    ++it;
  }
  return false;
}

wstring gdioutput::getTimerText(int zeroTime, int format, bool timeInSeconds, const wstring& textFormat) {
  TextInfo temp;
  temp.zeroTime=0;
  temp.format=format;
  temp.timerFormat = textFormat;
  if (timeInSeconds)
    return getTimerText(temp, 1000*zeroTime);
  else
    return getTimerText(temp, (1000/timeUnitsPerSecond) * zeroTime);
}

wstring gdioutput::getTimerText(const TextInfo &tit, uint64_t T) {
  int rt = int(T - tit.zeroTime) / 1000;
  int tenth = (abs(int(T - tit.zeroTime)) / 100) % 10;
  wstring text;

  int t=abs(rt);
  wchar_t bf[16];
  if ((tit.format & time24HourClock) != 0 && t > 0)
    t = t % (24 * timeConstSecPerHour);

  if (tit.format & timeHHMM) {
    swprintf_s(bf, 16, L"%d:%02d", (t / timeConstSecPerHour), (t / timeConstSecPerMin) % timeConstSecPerMin);
  }
  else if (tit.format & timeSeconds) {
    if (tit.format & timeWithTenth) 
      swprintf_s(bf, 16, L"%d.%d", t, tenth);
    else
      swprintf_s(bf, 16, L"%d", t);
  }
  else if ((tit.format & timeWithTenth) && rt < timeConstSecPerHour) {
    swprintf_s(bf, 16, L"%02d:%02d.%d", t/ timeConstSecPerMin, t%timeConstSecPerMin, tenth);
  }
  else if (rt>=timeConstSecPerHour  || (tit.format&fullTimeHMS))
    swprintf_s(bf, 16, L"%02d:%02d:%02d", t/ timeConstSecPerHour, (t/ timeConstSecPerMin)% timeConstSecPerMin, t%timeConstSecPerMin);
  else
    swprintf_s(bf, 16, L"%d:%02d", (t/ timeConstMinPerHour), t%timeConstMinPerHour);

  if (rt>0 || ((tit.format&fullTimeHMS) && rt>=0) )
    if (tit.format&timerCanBeNegative) 
      text = wstring(L"+") + bf;
    else				
      text = bf;
  else if (rt<0)
    if (tit.format&timerCanBeNegative) 
      text = wstring(L"-")+bf;
    else if (tit.format&timerIgnoreSign) 
      text = bf;
    else
      text = L"-";

  if (tit.timerFormat.empty())
   return text;
  else {
    return lang.tl(tit.timerFormat + L"#" + text);
  }
}

void gdioutput::CheckInterfaceTimeouts(uint64_t T)
{
  list<InfoBox>::iterator it=IBox.begin();

  while (it!=IBox.end()) {
    if (it->timeOut && it->timeOut<T) {
      if (it->hasCapture || it->hasTCapture)
        ReleaseCapture();

      InvalidateRect(hWndTarget, &(it->boundingBox), true);
      IBox.erase(it);
      it=IBox.begin();
    }
    else ++it;
  }

  list<TextInfo>::iterator tit = TL.begin();
  vector<TextInfo> timeout;
  if (hasAnyTimer) {
    bool anyChange = false;
    while(tit!=TL.end()){
      if (tit->hasTimer){
        wstring text = tit->xp > 0 ? getTimerText(*tit, T) : L"";
        if (tit->timeOut && T > tit->timeOut){
          tit->timeOut = 0;
          if (tit->callBack || tit->hasEventHandler())
            timeout.push_back(*tit);
        }
        if (text != tit->text) {
          RECT rc=tit->textRect;
          tit->text=text;
          calcStringSize(*tit);

          rc.right=max(tit->textRect.right, rc.right);
          rc.bottom=max(tit->textRect.bottom, rc.bottom);

          anyChange = true;
          //InvalidateRecthWndTarget, &rc, true);
        }
      }
      ++tit;
    }

    if (anyChange) {
      int w, h;
      getTargetDimension(w, h);
      HDC hDC = GetDC(hWndTarget);
      HBITMAP btm = CreateCompatibleBitmap(hDC, w, h);
      HDC memDC = CreateCompatibleDC (hDC);
      HGDIOBJ hOld = SelectObject(memDC, btm);
      RECT rc;
      rc.top = 0;
      rc.left = 0;
      rc.bottom = h;
      rc.right = w;
      RECT area = rc;
      drawBackground(memDC, rc);
      draw(memDC, rc, area);
      BitBlt(hDC, 0, 0, w, h, memDC, 0,0, SRCCOPY);
      SelectObject(memDC, hOld);
      DeleteObject(btm);
      DeleteDC(memDC);
      ReleaseDC(hWndTarget, hDC);
    }
  }

  for (size_t k = 0; k < timeout.size(); k++) {
    if (!timeout[k].handleEvent(*this, GUI_TIMEOUT))
      timeout[k].callBack(this, GUI_TIMEOUT, &timeout[k]);
  }
}


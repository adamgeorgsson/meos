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

// gdioutputInput.cpp: Message processing and input handling
//
//////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "gdioutput.h"
#include "gdiconstants.h"
#include "meosexception.h"
#include <commctrl.h>
#include "meos_util.h"
#include "localizer.h"
#include "toolbar.h"
#include "gdiimpl.h"
#include "autocomplete.h"

LRESULT gdioutput::ProcessMsg(UINT iMessage, LPARAM lParam, WPARAM wParam)
{
  wstring msg;
  try {
    return ProcessMsgWrp(iMessage, lParam, wParam);
  }
  catch (const meosCancel&) {
    return false;
  }
  catch (meosException & ex) {
    msg = ex.wwhat();
  }
  catch(std::exception &ex) {
    msg=widen(ex.what());
    if (msg.empty())
      msg=L"Ett okänt fel inträffade.";
  }
  catch(...) {
    msg=L"Ett okänt fel inträffade.";
  }
  
  if (!msg.empty()) {
    alert(msg);
    setWaitCursor(false);
  }
  return 0;
}

void gdioutput::processButtonMessage(ButtonInfo &bi, WPARAM wParam)
{
  WORD hwParam = HIWORD(wParam);

  switch (hwParam) {
    case BN_CLICKED: {
      string cmd;
      if (getRecorder().recording()) {
        if (bi.isExtraString()) {
          cmd = "press(\"" + bi.id + "\", \""  + narrow(bi.getExtra()) + "\"); //" + toUTF8(bi.text);
        }
        else {
          int arg = int((size_t)bi.extra);
          if (arg > 1000000 || arg < -1000000 || arg == 0)
            cmd = "press(\"" + bi.id + "\"); //" + toUTF8(bi.text);
          else
            cmd = "press(\"" + bi.id + "\", "  + itos(bi.getExtraInt()) + "); //" + toUTF8(bi.text);
        }
      }
      if (bi.isCheckbox)
        bi.checked = SendMessage(bi.hWnd, BM_GETCHECK, 0, 0)==BST_CHECKED;
      bi.synchData();
      if (bi.callBack || bi.hasEventHandler()) {
        setWaitCursor(true);
        if (!bi.handleEvent(*this, GUI_BUTTON) && bi.callBack)
          bi.callBack(this, GUI_BUTTON, &bi); //it may be destroyed here...

        setWaitCursor(false);
      }
      getRecorder().record(cmd);
      break;
    }
    case BN_SETFOCUS:
      if (currentFocus.hWnd != bi.hWnd) {
//        if (currentF      ocus.wasTabbed)
//          Button_SetState(currentFocus.hWnd, false);
        currentFocus = bi.hWnd;
      }
      break;
    case BN_KILLFOCUS:
      if (currentFocus.hWnd == bi.hWnd) {
//        if (currentFocus.wasTabbed)
//          Button_SetState(currentFocus.hWnd, false);
      }
      break;
  }
}

void gdioutput::processEditMessage(InputInfo &bi, WPARAM wParam)
{
  WORD hwParam = HIWORD(wParam);

  switch (hwParam) {
    case EN_CHANGE:
      if (bi.writeLock)
        return;
      getWindowText(bi.hWnd, bi.text);
      if (bi.handler)
        bi.handler->handle(*this, bi, GUI_INPUTCHANGE);
      else if (bi.managedHandler)
        bi.managedHandler->handle(*this, bi, GUI_INPUTCHANGE);
      else if (bi.callBack)
        bi.callBack(this, GUI_INPUTCHANGE, &bi); //it may be destroyed here...
     
      break;

    case EN_KILLFOCUS: {
      autoCompleteInfo.reset();
      wstring old = bi.focusText;
      getWindowText(bi.hWnd, bi.text);
      bi.synchData();
      bool equal = old == bi.text;
      string cmd = "input(\"" + bi.id + "\", \"" + toUTF8(bi.text) + "\");";
      if (bi.handler)
        bi.handler->handle(*this, bi, GUI_INPUT);
      else if (bi.managedHandler)
        bi.managedHandler->handle(*this, bi, GUI_INPUT);
      else if (bi.callBack)
        bi.callBack(this, GUI_INPUT, &bi);
      if (!equal)
        getRecorder().record(cmd);
      break;
    }
    case EN_SETFOCUS:
      currentFocus = bi.hWnd;
      getWindowText(bi.hWnd, bi.text);
      bi.synchData();
      bi.focusText = bi.text;
      if (bi.handler)
        bi.handler->handle(*this, bi, GUI_FOCUS);
      else if (bi.managedHandler)
        bi.managedHandler->handle(*this, bi, GUI_FOCUS);
      else if (bi.callBack)
        bi.callBack(this, GUI_FOCUS, &bi);
      break;
  }
}

void gdioutput::processComboMessage(ListBoxInfo &bi, WPARAM wParam)
{
  WORD hwParam = HIWORD(wParam);
  LRESULT index;
  switch (hwParam) {
    case CBN_SETFOCUS:
      currentFocus = bi.hWnd;
      lockUpDown = true;
      break;
    case CBN_KILLFOCUS: {
      if (autoCompleteInfo && !autoCompleteInfo->locked())
        autoCompleteInfo.reset();
      lockUpDown = false;

      TCHAR bf[1024];
      index=SendMessage(bi.hWnd, CB_GETCURSEL, 0, 0);

      if (index != CB_ERR) {
        if (SendMessage(bi.hWnd, CB_GETLBTEXT, index, LPARAM(bf)) != CB_ERR) {
          bi.text = bf;
          bi.data=SendMessage(bi.hWnd, CB_GETITEMDATA, index, 0);
          if (bi.handler)
            bi.handler->handle(*this, bi, GUI_COMBO);
          else if (bi.callBack)
            bi.callBack(this, GUI_COMBO, &bi); //it may be destroyed here...
        }
      }
      else {
        GetWindowText(bi.hWnd, bf, sizeof(bf)-1);
        bi.data = -1;
        bi.text = bf;
        string cmd = "input(\"" + bi.id + "\", \"" + toUTF8(bi.text) + "\");";
        if (bi.handler)
          bi.handler->handle(*this, bi, GUI_COMBO);
        else if (bi.callBack)
          bi.callBack(this, GUI_COMBO, &bi); //it may be destroyed here...
        getRecorder().record(cmd);
      }
    }
    break;

    case CBN_EDITCHANGE: {
      if (bi.writeLock)
        return;
      getWindowText(bi.hWnd, bi.text);
      if (bi.handler)
        bi.handler->handle(*this, bi, GUI_COMBOCHANGE);
      else if (bi.callBack)
        bi.callBack(this, GUI_COMBOCHANGE, &bi); //it may be destroyed here...
      break;
    }
    case CBN_SELCHANGE:
      index=SendMessage(bi.hWnd, CB_GETCURSEL, 0, 0);

      if (index != CB_ERR) {
        bi.data=SendMessage(bi.hWnd, CB_GETITEMDATA, index, 0);

        TCHAR bf[1024];
        if (SendMessage(bi.hWnd, CB_GETLBTEXT, index, LPARAM(bf)) != CB_ERR)
          bi.text=bf;
        string cmd = "select(\"" + bi.id + "\", " + itos(bi.data) + ");";
        internalSelect(bi);
        getRecorder().record(cmd);
      }
      break;
  }
}

void gdioutput::keyCommand(KeyCommandCode code) {
  if (hasCommandLock())
    return;

  if (code == KC_SLOWDOWN)
    autoSpeed *= 0.9;
  else if (code == KC_SPEEDUP)
    autoSpeed *= 1.0/0.9;

  wstring msg;
  try {
    list<TableInfo>::iterator tit;
    if (useTables) {
      for (tit=Tables.begin(); tit!=Tables.end(); ++tit)
        if (tit->table->keyCommand(*this, code))
          return;
    }

    for (list<EventInfo>::iterator it = Events.begin(); it != Events.end(); ++it) {
      if (it->getKeyCommand() == code) {
        it->setData("", 0);
        it->setExtra(0);
        if (!it->handleEvent(*this, GUI_EVENT) && it->callBack) {
          it->callBack(this, GUI_EVENT, &*it); //it may be destroyed here...
        }
        return;
      }
    }
  }
  catch (const meosCancel&) {
    return;
  }
  catch (meosException & ex) {
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

void gdioutput::processListMessage(ListBoxInfo &bi, WPARAM wParam)
{
  WORD hwParam = HIWORD(wParam);
  LRESULT index;

  switch (hwParam) {
    case LBN_SETFOCUS:
      currentFocus = bi.hWnd;
      lockUpDown = true;
      break;
    case LBN_KILLFOCUS:
      autoCompleteInfo.reset();
      lockUpDown = false;
      break;
    case LBN_SELCHANGE:
    case LBN_DBLCLK:

      index=SendMessage(bi.hWnd, LB_GETCURSEL, 0, 0);

      if (index!=LB_ERR) {
        bi.data = SendMessage(bi.hWnd, LB_GETITEMDATA, index, 0);

        TCHAR bf[1024];
        if (SendMessage(bi.hWnd, LB_GETTEXT, index, LPARAM(bf)) != LB_ERR)
          bi.text = bf;
        
        string cmd;
        if (hwParam == LBN_SELCHANGE)
          cmd = "select(\"" + bi.id + "\", " + itos(bi.data) + ");";
        else
          cmd = "dblclick(\"" + bi.id + "\", " + itos(bi.data) + ");";

        if (bi.callBack || bi.handler) {
          setWaitCursor(true);
          if (hwParam == LBN_SELCHANGE) {
            if (bi.handler)
              bi.handler->handle(*this, bi, GUI_LISTBOX);
            else
              bi.callBack(this, GUI_LISTBOX, &bi); //it may be destroyed here...
          }
          else {
            if (bi.handler)
              bi.handler->handle(*this, bi, GUI_LISTBOXSELECT);
            else
              bi.callBack(this, GUI_LISTBOXSELECT, &bi); //it may be destroyed here...
          }
          setWaitCursor(false);
        }
        getRecorder().record(cmd); 
      }
      break;
  }
}


LRESULT gdioutput::ProcessMsgWrp(UINT iMessage, LPARAM lParam, WPARAM wParam)
{
  if (iMessage == WM_COMMAND) {
    WORD hwParam = HIWORD(wParam);
    HWND hWnd = (HWND)lParam;
    if (hwParam == EN_CHANGE) {
      list<TableInfo>::iterator tit;
      if (useTables)
        for (tit = Tables.begin(); tit != Tables.end(); ++tit)
          if (tit->table->inputChange(*this, hWnd))
            return 0;
    }

    {
      //list<ButtonInfo>::iterator it;
      //for (it=BI.begin(); it != BI.end(); ++it) {
      unordered_map<HWND, ButtonInfo*>::iterator it = biByHwnd.find(HWND(lParam));

      //    if (it->hWnd==hWnd) {
      if (it != biByHwnd.end()) {
        ButtonInfo &bi = *it->second;
        processButtonMessage(bi, wParam);
        return 0;
      }
      //}
    }

    {
      unordered_map<HWND, InputInfo*>::iterator it = iiByHwnd.find(HWND(lParam));
      if (it != iiByHwnd.end()) {
        InputInfo &ii = *it->second;
        processEditMessage(ii, wParam);
        return 0;
      }
      //list<InputInfo>::iterator it;
      /*for (it=II.begin(); it != II.end(); ++it) {
        if (it->hWnd==hWnd) {
          processEditMessage(*it, wParam);
          return 0;
        }
      }*/
    }

    {
      //list<ListBoxInfo>::iterator it;
      //for(it=LBI.begin(); it != LBI.end(); ++it) {
      unordered_map<HWND, ListBoxInfo*>::iterator it = lbiByHwnd.find(HWND(lParam));
      if (it != lbiByHwnd.end()) {
        ListBoxInfo &lbi = *it->second;
        if (lbi.IsCombo)
          processComboMessage(lbi, wParam);
        else
          processListMessage(lbi, wParam);
        return 0;
      }
    }
  }
  else if (iMessage == WM_MOUSEMOVE) {
    POINT pt;
    pt.x = (signed short)LOWORD(lParam);
    pt.y = (signed short)HIWORD(lParam);

    list<TableInfo>::iterator tit;

    bool GotCapture = false;

    if (useTables)
      for (tit = Tables.begin(); tit != Tables.end(); ++tit)
        GotCapture = tit->table->mouseMove(*this, pt.x, pt.y) || GotCapture;

    if (GotCapture)
      return 0;

    list<InfoBox>::iterator it = IBox.begin();


    while (it != IBox.end()) {
      if (PtInRect(&it->textRect, pt) && (it->callBack || it->hasEventHandler())) {
        SetCursor(LoadCursor(NULL, IDC_HAND));

        HDC hDC = GetDC(hWndTarget);
        //drawBoxText(hDC, *it, true);
        drawBoxBg(hDC, *it);
        drawCloseBox(hDC, it->close, false);
        drawBoxText(hDC, *it, true);

        ReleaseDC(hWndTarget, hDC);
        SetCapture(hWndTarget);
        GotCapture = true;
        it->hasTCapture = true;
      }
      else {
        if (it->hasTCapture) {
          HDC hDC = GetDC(hWndTarget);
          //drawBoxText(hDC, *it, false);
          drawBoxBg(hDC, *it);
          drawCloseBox(hDC, it->close, false);
          drawBoxText(hDC, *it, false);
          
          ReleaseDC(hWndTarget, hDC);
          if (!GotCapture)
            ReleaseCapture();
          it->hasTCapture = false;
        }
      }

      if (it->hasCapture) {
        if (GetCapture() != hWndTarget) {
          HDC hDC = GetDC(hWndTarget);
          drawCloseBox(hDC, it->close, false);
          ReleaseDC(hWndTarget, hDC);
          if (!GotCapture) ReleaseCapture();
          it->hasCapture = false;
        }
        else if (!PtInRect(&it->close, pt)) {
          HDC hDC = GetDC(hWndTarget);
          drawCloseBox(hDC, it->close, false);
          ReleaseDC(hWndTarget, hDC);
        }
        else {
          HDC hDC = GetDC(hWndTarget);
          drawCloseBox(hDC, it->close, true);
          ReleaseDC(hWndTarget, hDC);
        }
      }
      ++it;
    }

    for (size_t k = 0; k < shownStrings.size(); k++) {
      TextInfo &ti = *shownStrings[k];
      if ((!ti.callBack && !ti.hasEventHandler()) || ti.hasTimer)
        continue;

      if (PtInRect(&ti.textRect, pt)) {
        if (!ti.highlight) {
          ti.highlight = true;
          InvalidateRect(hWndTarget, &ti.textRect, true);
        }

        SetCapture(hWndTarget);
        GotCapture = true;
        ti.hasCapture = true;
        SetCursor(LoadCursor(NULL, IDC_HAND));
      }
      else {
        if (ti.highlight) {
          ti.highlight = false;
          InvalidateRect(hWndTarget, &ti.textRect, true);
        }

        if (ti.hasCapture) {
          if (!GotCapture)
            ReleaseCapture();

          ti.hasCapture = false;
        }
      }
    }
  }
  else if (iMessage == WM_LBUTTONDOWN) {
    if (autoCompleteInfo) {
      autoCompleteInfo.reset();
      return 0;
    }

    list<InfoBox>::iterator it = IBox.begin();

    POINT pt;
    pt.x = (signed short)LOWORD(lParam);
    pt.y = (signed short)HIWORD(lParam);

    list<TableInfo>::iterator tit;

    if (useTables) {
      for (tit = Tables.begin(); tit != Tables.end(); ++tit)
        if (tit->table->mouseLeftDown(*this, pt.x, pt.y))
          return 0;
    }

    while (it != IBox.end()) {
      if (PtInRect(&it->close, pt)) {
        HDC hDC = GetDC(hWndTarget);
        drawCloseBox(hDC, it->close, true);
        ReleaseDC(hWndTarget, hDC);
        SetCapture(hWndTarget);
        it->hasCapture = true;
      }
      ++it;
    }

    //Handle links
    for (size_t k = 0; k < shownStrings.size(); k++) {
      TextInfo &ti = *shownStrings[k];
      if (!ti.callBack && !ti.hasEventHandler())
        continue;

      if (ti.hasCapture) {
        HDC hDC = GetDC(hWndTarget);
        if (PtInRect(&ti.textRect, pt)) {
          ti.active = true;
          RenderString(ti, hDC);
        }
        ReleaseDC(hWndTarget, hDC);
      }
    }
  }
  else if (iMessage == WM_LBUTTONUP) {
    list<TableInfo>::iterator tit;

    list<InfoBox>::iterator it = IBox.begin();

    POINT pt;
    pt.x = (signed short)LOWORD(lParam);
    pt.y = (signed short)HIWORD(lParam);

    if (useTables) {
      for (tit = Tables.begin(); tit != Tables.end(); ++tit)
        if (tit->table->mouseLeftUp(*this, pt.x, pt.y))
          return 0;
    }
    while (it != IBox.end()) {
      if (it->hasCapture) {
        HDC hDC = GetDC(hWndTarget);
        drawCloseBox(hDC, it->close, false);
        ReleaseDC(hWndTarget, hDC);
        ReleaseCapture();
        it->hasCapture = false;

        if (PtInRect(&it->close, pt)) {
          RECT rc;
          computeBoxesBoundingBox(rc);
          IBox.erase(it);
          InvalidateRect(hWndTarget, &rc, true);
          //refresh();
          return 0;
        }
      }
      else if (it->hasTCapture) {
        ReleaseCapture();
        it->hasTCapture = false;

        if (PtInRect(&it->textRect, pt)) {
          if (!it->handleEvent(*this, GUI_INFOBOX) && it->callBack)
            it->callBack(this, GUI_INFOBOX, &*it); //it may be destroyed here...
          return 0;
        }
      }
      ++it;
    }

    //Handle links
    for (size_t k = 0; k < shownStrings.size(); k++) {
      TextInfo &ti = *shownStrings[k];
      if (!ti.callBack && !ti.hasEventHandler())
        continue;

      if (ti.hasCapture) {
        ReleaseCapture();
        ti.hasCapture = false;

        if (PtInRect(&ti.textRect, pt)) {
          if (ti.active) {
            string cmd;
            if (ti.getExtraInt() != 0)
              cmd = "click(\"" + ti.id + "\", " + itos(ti.getExtraInt()) + "); //" + toUTF8(ti.text);
            else
              cmd = "click(\"" + ti.id + "\"); //" + toUTF8(ti.text);
            ti.active = false;
            RenderString(ti);
            if (!ti.handleEvent(*this, GUI_LINK))
              ti.callBack(this, GUI_LINK, &ti);
            getRecorder().record(cmd);
            return 0;
          }
        }
      }
      else if (ti.active) {
        ti.active = false;
        RenderString(ti);
      }
    }
  }
  else if (iMessage == WM_LBUTTONDBLCLK) {
    list<TableInfo>::iterator tit;
    POINT pt;
    pt.x = (signed short)LOWORD(lParam);
    pt.y = (signed short)HIWORD(lParam);

    if (useTables)
      for (tit = Tables.begin(); tit != Tables.end(); ++tit)
        if (tit->table->mouseLeftDblClick(*this, pt.x, pt.y))
          return 0;

  }
  else if (iMessage == WM_RBUTTONDOWN) {
    POINT pt;
    pt.x = (signed short)LOWORD(lParam);
    pt.y = (signed short)HIWORD(lParam);

    if (useTables) {
      for (auto tit = Tables.begin(); tit != Tables.end(); ++tit)
        if (tit->table->mouseRightDown(*this, pt.x, pt.y))
          return 0;
    }
  }
  else if (iMessage == WM_RBUTTONUP) {
    POINT pt;
    pt.x = (signed short)LOWORD(lParam);
    pt.y = (signed short)HIWORD(lParam);

    if (useTables) {
      for (auto tit = Tables.begin(); tit != Tables.end(); ++tit)
        if (tit->table->mouseRightUp(*this, pt.x, pt.y))
          return 0;
    }
  }
  else if (iMessage == WM_MBUTTONDOWN) {
    POINT pt;
    pt.x = (signed short)LOWORD(lParam);
    pt.y = (signed short)HIWORD(lParam);

    if (useTables) {
      for (auto tit = Tables.begin(); tit != Tables.end(); ++tit)
        if (tit->table->mouseMidDown(*this, pt.x, pt.y))
          return 0;
    }
  }
  else if (iMessage == WM_MBUTTONUP) {
    POINT pt;
    pt.x = (signed short)LOWORD(lParam);
    pt.y = (signed short)HIWORD(lParam);

    if (useTables) {
      for (auto tit = Tables.begin(); tit != Tables.end(); ++tit)
        if (tit->table->mouseMidUp(*this, pt.x, pt.y))
          return 0;
    }
  }
  else if (iMessage == WM_CHAR) {
    /*list<TableInfo>::iterator tit;
    if (useTables)
      for (tit=Tables.begin(); tit!=Tables.end(); ++tit)
        if (tit->table->character(*this, int(wParam), lParam & 0xFFFF))
          return 0;*/
  }
  else if (iMessage == WM_CTLCOLOREDIT) {
    unordered_map<HWND, InputInfo*>::iterator it = iiByHwnd.find(HWND(lParam));
    if (it != iiByHwnd.end()) {
      InputInfo &ii = *it->second;
        if (ii.bgColor != colorDefault || ii.fgColor != colorDefault) {
          if (ii.bgColor != colorDefault) {
            SetDCBrushColor(HDC(wParam), ii.bgColor);
            SetBkColor(HDC(wParam), ii.bgColor);
          }
          else {
            SetDCBrushColor(HDC(wParam), GetSysColor(COLOR_WINDOW));
            SetBkColor(HDC(wParam), GetSysColor(COLOR_WINDOW));
          }
          if (ii.fgColor != colorDefault)
            SetTextColor(HDC(wParam), ii.fgColor);
          return LRESULT(GetStockObject(DC_BRUSH));
        }
    }
    return 0;
  }
  else if (iMessage == WM_DESTROY) {
    canClear();// Ignore return value
  }

  return 0;
}

void gdioutput::TabFocus(int direction)
{
  list<TableInfo>::iterator tit;

  if (useTables)
    for (tit=Tables.begin(); tit!=Tables.end(); ++tit)
      if (tit->table->tabFocus(*this, direction))
        return;

  if (FocusList.empty())
    return;

  list<HWND>::iterator it=FocusList.begin();

  while(it!=FocusList.end() && *it != currentFocus.hWnd)
    ++it;

  //if (*it==CurrentFocus)
  if (it!=FocusList.end()) {
    if (direction==1){
      ++it;
      if (it==FocusList.end()) it=FocusList.begin();
      while(!IsWindowEnabled(*it) && *it != currentFocus.hWnd){
      ++it;
        if (it==FocusList.end()) it=FocusList.begin();
      }
    }
    else{
      if (it==FocusList.begin()) it=FocusList.end();

      it--;
      while(!IsWindowEnabled(*it) && *it != currentFocus.hWnd){
        if (it==FocusList.begin()) it=FocusList.end();
        it--;
      }

    }

  //  if (currentFocus.wasTabbed)
  //    Button_SetState(currentFocus.hWnd, false);

    HWND hWT = *it;
    //SetFocus(0);
    SetFocus(hWT);
    currentFocus = hWT;
    //currentFocus = *it;
    /*if (biByHwnd.find(currentFocus.hWnd) != biByHwnd.end()) {
      currentFocus.wasTabbed = true;
      Button_SetState(currentFocus.hWnd, true);
    }*/
  }
  else{
    SetFocus(currentFocus.hWnd);
    currentFocus=*FocusList.begin();

  }
}

bool gdioutput::isInputChanged(const string &exclude)
{
  for(list<InputInfo>::iterator it=II.begin(); it != II.end(); ++it) {
    if (it->id!=exclude) {
      if (it->changed()  && !it->ignoreCheck)
        return true;
    }
  }

  for (list<ListBoxInfo>::iterator it = LBI.begin(); it != LBI.end(); ++it) {
    getSelectedItem(*it);
    if (it->changed() && !it->ignoreCheck)
      return true;
  }

  for (list<ButtonInfo>::iterator it = BI.begin(); it != BI.end(); ++it) {
    bool checked = SendMessage(it->hWnd, BM_GETCHECK, 0, 0)==BST_CHECKED;
    if (it->originalState != checked)
      return true;
  }

  return false;
}

InputInfo *gdioutput::replaceSelection(const char *id, const wstring &text)
{
  for(list<InputInfo>::iterator it=II.begin(); it != II.end(); ++it)
    if (it->id==id) {
      SendMessage(it->hWnd, EM_REPLACESEL, TRUE, LPARAM(text.c_str()));
      return &*it;
    }

  return 0;
}

BaseInfo *gdioutput::setInputFocus(const string &id, bool select)
{
  for(list<InputInfo>::iterator it=II.begin(); it != II.end(); ++it)
    if (it->id==id) {
      scrollTo(it->xp, it->yp);
      BaseInfo *bi = SetFocus(it->hWnd)!=NULL ? &*it: 0;
      if (bi) {
        if (select)
          PostMessage(it->hWnd, EM_SETSEL, it->text.length(), 0);
      }
      return bi;
    }

  for(list<ListBoxInfo>::iterator it=LBI.begin(); it!=LBI.end();++it)
    if (it->id==id) {
      scrollTo(it->xp, it->yp);
      return SetFocus(it->hWnd)!=NULL ? &*it: 0;
  }

  for(list<ButtonInfo>::iterator it=BI.begin(); it!=BI.end();++it)
    if (it->id==id) {
      scrollTo(it->xp, it->yp);
      return SetFocus(it->hWnd)!=NULL ? &*it: 0;
  }

  return 0;
}

InputInfo *gdioutput::getInputFocus()
{
  HWND hF=GetFocus();

  if (hF) {
    list<InputInfo>::iterator it;

    for(it=II.begin(); it != II.end(); ++it)
      if (it->hWnd==hF)
        return &*it;
  }
  return 0;
}

void gdioutput::enter()
{
  if (hasCommandLock())
    return;

  wstring msg;
  try {
    doEnter();
  }
  catch (const meosCancel&) {
    return;
  }
  catch (meosException & ex) {
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

void gdioutput::doEnter() {
  if (autoCompleteInfo) {
    autoCompleteInfo->enter();
    return;
  }
  list<TableInfo>::iterator tit;

  if (useTables)
    for (tit=Tables.begin(); tit!=Tables.end(); ++tit)
      if (tit->table->enter(*this))
        return;

  HWND hWnd=GetFocus();

  for (list<ButtonInfo>::iterator it=BI.begin(); it!=BI.end(); ++it)
    if (it->isDefaultButton()) {
      if (!it->handleEvent(*this, GUI_BUTTON) && it->callBack)
        it->callBack(this, GUI_BUTTON, &*it);
      return;
    }

  list<InputInfo>::iterator it;

  for(it=II.begin(); it != II.end(); ++it)
    if (it->hWnd==hWnd && (it->hasEventHandler() || it->callBack)){
      TCHAR bf[1024];
      GetWindowText(hWnd, bf, 1024);
      it->text = bf;
      if (!it->handleEvent(*this, GUI_INPUT))
        it->callBack(this, GUI_INPUT, &*it);
      return;
    }
}

bool gdioutput::upDown(int direction)
{
  wstring msg;
  try {
    return doUpDown(direction);
  }
  catch (const meosCancel&) {
    return false;
  }
  catch (meosException & ex) {
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
  return false;
}


bool gdioutput::doUpDown(int direction)
{
  if (autoCompleteInfo) {
    autoCompleteInfo->upDown(direction);
    return true;
  }
  list<TableInfo>::iterator tit;

  if (useTables)
    for (tit=Tables.begin(); tit!=Tables.end(); ++tit)
      if (tit->table->upDown(*this, direction))
        return true;

  return false;
}

void gdioutput::escape()
{
  if (hasCommandLock())
    return;
  wstring msg;
  try {
    doEscape();
  }
  catch (const meosCancel&) {
    return;
  }
  catch (meosException & ex) {
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


void gdioutput::doEscape()
{
  if (fullScreen) {
    PostMessage(hWndTarget, WM_CLOSE, 0,0);
  }

  if (autoCompleteInfo) {
    autoCompleteInfo.reset();
    return;
  }

  list<TableInfo>::iterator tit;

  if (useTables)
    for (tit=Tables.begin(); tit!=Tables.end(); ++tit)
      tit->table->escape(*this);

  for (list<ButtonInfo>::iterator it=BI.begin(); it!=BI.end(); ++it) {
    if (it->isCancelButton() && (it->callBack || it->hasEventHandler()) ) {
      if (!it->handleEvent(*this, GUI_BUTTON))
        it->callBack(this, GUI_BUTTON, &*it);
      return;
    }
  }
}


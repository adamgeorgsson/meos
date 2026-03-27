// meos_dom_stubs.h — Minimal stub types so the domain library compiles
// without Win32 UI/GDI dependencies.  GUI methods remain in oDataContainer
// but do nothing on non-Win32 builds.
#pragma once

#include "domain_header.h"
#include "TableType.h"

class oBase;
class oDataDefiner;

// ── InputInfo (stub — real class lives in gdistructures.h) ─────────────────
class InputInfo {
public:
  wstring text;
};

// ── gdioutput stub (only the methods called by oDataContainer) ─────────────
class gdioutput {
public:
  enum class AskAnswer { AnswerOk, AnswerCancel };

  // Field builders
  InputInfo& addInput(const string&, const wstring& = L"", int = 10, int = 0,
                      const wstring& = L"") { static InputInfo ii; return ii; }
  void addSelection(const string&, int, int, void*, const wstring&) {}
  void setItems(const string&, vector<pair<wstring,size_t>>&) {}

  // Field accessors
  bool   hasWidget(const string&) const { return false; }
  wstring getText(const string&) const  { return L""; }
  int    getTextNo(const string&) const  { return 0; }
  void   setText(const string&, const wstring&) {}
  void   setText(const char*, const wstring&) {}
  void   setText(const char*, const wchar_t*) {}
  pair<int,bool> getSelectedItem(const string&) const { return {0, false}; }
  void   selectItemByData(const string&, size_t) {}

  // String helpers
  wstring widen(const char* s) const {
    wstring r; while (*s) r += (wchar_t)(unsigned char)*s++;
    return r;
  }
  wstring widen(const string& s) const { return widen(s.c_str()); }

  AskAnswer askOkCancel(const wstring&) { return AskAnswer::AnswerOk; }
};

// ── Table stub (only the methods called by oDataContainer) ─────────────────
class Table {
public:
  TableColSpec addColumn(const char*, int, bool = false, bool = false) { return {}; }
  int addColumnPaddedSort(const char*, int, int, bool) { return -1; }
  void addDataDefiner(const char*, oDataDefiner*) {}
  void set(int, oBase&, int, const wstring&, bool, CellType = cellEdit) {}
  void set(int, oBase&, int, const wchar_t*, bool, CellType = cellEdit) {}
};

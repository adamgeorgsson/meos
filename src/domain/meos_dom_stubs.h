// meos_dom_stubs.h — Minimal stub types so the domain library compiles
// without Win32 UI/GDI dependencies.  GUI methods remain in oDataContainer
// but do nothing on non-Win32 builds.
#pragma once

#include "domain_header.h"
#include "TableType.h"

class oBase;
class oEvent;
class oDataDefiner;

// ── InputInfo (stub — real class lives in gdistructures.h) ─────────────────
class InputInfo {
public:
  wstring text;
};

// ── gdioutput stub (only the methods called by domain code) ─────────────────
class gdioutput {
public:
  enum class AskAnswer { AnswerOk, AnswerCancel };

  // Field builders
  InputInfo& addInput(const string&, const wstring& = L"", int = 10, int = 0,
                      const wstring& = L"") { static InputInfo ii; return ii; }
  void addSelection(const string&, int, int, void*, const wstring&) {}
  void setItems(const string&, vector<pair<wstring, size_t>>&) {}

  // Field accessors
  bool   hasWidget(const string&) const { return false; }
  wstring getText(const string&) const  { return L""; }
  int    getTextNo(const string&) const  { return 0; }
  void   setText(const string&, const wstring&) {}
  void   setText(const char*, const wstring&) {}
  void   setText(const char*, const wchar_t*) {}
  pair<int, bool> getSelectedItem(const string&) const { return {0, false}; }
  void   selectItemByData(const string&, size_t) {}

  // String helpers
  wstring widen(const char* s) const {
    wstring r; while (*s) r += (wchar_t)(unsigned char)*s++;
    return r;
  }
  wstring widen(const string& s) const { return widen(s.c_str()); }

  AskAnswer askOkCancel(const wstring&) { return AskAnswer::AnswerOk; }

  // List control methods (used by oCard::fillPunches)
  void clearList(const string&) {}
  void addItem(const string&, const wstring&, int) {}
  void addItem(const string&, const wstring&) {}
};

// ── Table column ID constants (match legacy Table.h enum) ──────────────────
enum {
  TID_CLASSNAME, TID_COURSE, TID_NUM, TID_ID, TID_MODIFIED,
  TID_RUNNER, TID_CLUB, TID_START, TID_TIME,
  TID_FINISH, TID_STATUS, TID_RUNNINGTIME, TID_PLACE, TID_POINTS,
  TID_CARD, TID_TEAM, TID_LEG, TID_CONTROL, TID_UNIT, TID_CODES, TID_FEE, TID_PAID,
  TID_INPUTTIME, TID_INPUTSTATUS, TID_INPUTPOINTS, TID_INPUTPLACE,
  TID_NAME, TID_NATIONAL, TID_SEX, TID_YEAR, TID_INDEX,
  TID_ENTER, TID_STARTNO, TID_VOLTAGE, TID_BATTERYDATE, TID_CARDTYPE,
  TID_FINISHCONTROL, TID_STARTCONTROL
};

// ── Table stub (only the methods called by domain code) ─────────────────────
class Table {
public:
  static constexpr int CAN_DELETE = 1;

  // Constructor used by oCard::getTable and oFreePunch::getTable
  Table(oEvent* /*oe*/, int /*initialCapacity*/, const wstring& /*title*/, const string& /*name*/) {}
  // Default constructor for shared_ptr<Table>
  Table() = default;

  TableColSpec addColumn(const char*, int, bool = false, bool = false) { return {}; }
  int addColumnPaddedSort(const char*, int, int, bool) { return -1; }
  void addDataDefiner(const char*, oDataDefiner*) {}
  void set(int, oBase&, int, const wstring&, bool, CellType = cellEdit) {}
  void set(int, oBase&, int, const wchar_t*, bool, CellType = cellEdit) {}
  const string& getInternalName() const { static const string s; return s; }
  void addRow(int, void*) {}
  void reserve(size_t) {}
  void setTableProp(int) {}
};

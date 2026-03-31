// Table.h — Table stub and related types for platform-independent domain builds.
// Moved from src/domain/TableType.h + src/domain/meos_dom_stubs.h (US-003k).
#pragma once

#include <cassert>
#include <string>

// ── TableColSpec ──────────────────────────────────────────────────────────────
class TableColSpec {
  int firstCol = -1;
  int numCol = 0;
public:
  TableColSpec() = default;
  TableColSpec(int firstCol_, int numCol_) : firstCol(firstCol_), numCol(numCol_) {}

  int operator[](int ix) const { assert(ix < numCol && ix >= 0); return firstCol + ix; }
  bool hasColumn(int colIx) const { return colIx >= firstCol && colIx < firstCol + numCol; }
  int getIndex(int colIx) const { return colIx - firstCol; }
  int numColumns() const { return numCol; }
  int nextColumn() const { return firstCol + numCol; }
  int firstColumn() const { return firstCol; }
};

enum CellType { cellEdit, cellSelection, cellAction, cellCombo };

class Table;
typedef void (*GENERATETABLEDATA)(Table& table, void* ptr);

// ── Table column ID constants ─────────────────────────────────────────────────
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

// Forward declarations needed by Table constructor signature
class oBase;
class oEvent;
class oDataDefiner;

// ── Table stub ────────────────────────────────────────────────────────────────
// Minimal implementation so domain code compiles without Win32 GUI dependencies.
class Table {
public:
  static constexpr int CAN_DELETE = 1;

  Table(oEvent* /*oe*/, int /*initialCapacity*/, const std::wstring& /*title*/, const std::string& /*name*/) {}
  Table() = default;

  TableColSpec addColumn(const char*, int, bool = false, bool = false) { return {}; }
  int addColumnPaddedSort(const char*, int, int, bool) { return -1; }
  void addDataDefiner(const char*, oDataDefiner*) {}
  void set(int, oBase&, int, const std::wstring&, bool, CellType = cellEdit) {}
  void set(int, oBase&, int, const wchar_t*, bool, CellType = cellEdit) {}
  const std::string& getInternalName() const { static const std::string s; return s; }
  void addRow(int, void*) {}
  void reserve(size_t) {}
  void setTableProp(int) {}
};

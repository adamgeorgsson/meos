#pragma once

#include <string>
#include "domain_header.h"

class oBase;

// Minimal stub oEvent — provides just enough interface for oBase, oDataContainer,
// oPunch, and oControl. Full competition management logic lives in the legacy
// code/oEvent.h.
class oEvent {
public:
  int dataRevision = 0;
  bool hasPendingDBConnection = false;

  bool isClient() const { return false; }
  bool hasDBConnection() const { return false; }

  // Called when an object's Id is assigned or changes.
  void updateFreeId(oBase*) {}

  // Returns the current data revision counter.
  int getRevision() const { return dataRevision; }

  // Stub: compute control visit/mistake statistics (full impl needs runner lists).
  void setupControlStatistics() const {}

  // Format a rogaining score as a string (stub: decimal).
  std::wstring formatScore(int v) const { return std::to_wstring(v); }

  // Parse a rogaining score string (stub: decimal).
  int convertScore(const std::wstring& s) const {
    try { return std::stoi(s); } catch (...) { return 0; }
  }

  // Punch-code index (cleared when control numbers change). Stub.
  struct PunchIndex { void clear() {} } punchIndex;

  // -----------------------------------------------------------------------
  // oPunch stubs
  // -----------------------------------------------------------------------

  // Format absolute time (tenths-of-sec from midnight) as "HH:MM:SS".
  std::wstring getAbsTime(int t, SubSecond /*mode*/) const {
    if (t <= 0) return L"-";
    int sec  = t / timeConstSecond;
    int hour = sec / 3600; sec %= 3600;
    int min  = sec / 60;   sec %= 60;
    wchar_t buf[16];
    swprintf(buf, 16, L"%d:%02d:%02d", hour, min, sec);
    return buf;
  }

  // Adjustment for a given punch unit (stub: 0).
  int getUnitAdjustment(int /*specialPunchType*/, int /*unit*/) const { return 0; }

  // Parse an absolute time string to tenths-of-sec units (stub: simple parse).
  int getRelativeTime(const std::wstring& t) const {
    return convertAbsoluteTimeHMS(t, -1);
  }

  // Zero-time number in tenths-of-sec units (stub: 0).
  int getZeroTimeNum() const { return 0; }
};

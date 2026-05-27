#pragma once

#include <string>

class oBase;

// Minimal stub oEvent — provides just enough interface for oBase and oDataContainer.
// Full competition management logic lives in the legacy code/oEvent.h.
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
};

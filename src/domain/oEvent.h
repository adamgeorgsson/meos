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
};

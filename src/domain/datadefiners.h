#pragma once

// Minimal datadefiners.h — provides the oDataDefiner abstract interface
// used by entities to define custom display/editing logic for their fields.
//
// The legacy datadefiners.h contained many Win32/GUI-specific formatters.
// This migration stub defines only the portable abstract interface that
// downstream entity classes need.

#include "oDataContainer.h"
#include "oBase.h"

// Helper for formatters that return a static/cached wstring
inline const std::wstring& makeDash(const std::wstring& s) {
  return s;
}

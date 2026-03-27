// oEvent.h — Minimal oEvent stub for domain-layer compilation.
// The full oEvent implementation will be provided in US-003i.
#pragma once

#include "domain_header.h"

class oBase;

class oEvent {
public:
  // ── Revision counter (used by DataRevisionCache) ──────────────────────────
  unsigned long dataRevision = 0;

  // ── Database/sync state ───────────────────────────────────────────────────
  bool hasPendingDBConnection = false;

  bool hasDBConnection() const { return false; }
  bool isClient() const { return false; }
  bool msSynchronize(oBase* /*ob*/) { return true; }
  void updateFreeId(oBase* /*ob*/) {}

  // ── Currency helpers (used by oDataContainer table/GUI methods) ───────────
  wstring formatCurrency(int /*value*/) const { return L""; }
  int interpretCurrency(const wstring& /*text*/) const { return 0; }

  // ── Stub API used by DataRevisionCache ────────────────────────────────────
  bool hasWarnedModifiedId() const { return false; }
  void hasWarnedModifiedId(bool) {}
  oEvent& gdiBase() { return *this; }
  int askOkCancel(const wstring&) { return 0; }
};

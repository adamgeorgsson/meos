#pragma once
// Portable replacement for the legacy Win32-specific SportIdent.h SICard structures.
// Provides just enough for oCard: card identity, hash computation, and punch storage.

#include <cstdint>

struct SIPunch {
  uint32_t Code = 0;
  uint32_t Time = 0;
};

enum class ConvertedTimeStatus {
  Unknown = 0,
  Hour12,
  Hour24,
  Done,
};

struct SICard {
  explicit SICard(ConvertedTimeStatus status = ConvertedTimeStatus::Unknown)
      : convertedTime(status) {}

  uint32_t CardNumber = 0;
  SIPunch  StartPunch;
  SIPunch  FinishPunch;
  SIPunch  CheckPunch;
  uint32_t nPunch = 0;
  SIPunch  Punch[192];
  int      miliVolt = 0;
  bool     punchOnly = false;
  ConvertedTimeStatus convertedTime;
  int      runnerId = 0;
  int      relativeFinishTime = 0;
  bool     statusOK = false;
  bool     statusDNF = false;

  bool empty() const { return CardNumber == 0; }

  unsigned int calculateHash() const {
    unsigned h = nPunch * 100000 + FinishPunch.Time;
    for (uint32_t i = 0; i < nPunch; i++) {
      h = h * 31 + Punch[i].Code;
      h = h * 31 + Punch[i].Time;
    }
    h += StartPunch.Time;
    return h;
  }
};

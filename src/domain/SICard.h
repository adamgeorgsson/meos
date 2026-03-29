// SICard.h — SportIdent card data structures (cross-platform).
// Migrated from legacy code/SportIdent.h.
#pragma once
#include <cstdint>
#include <cstring>

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
  uint32_t CardNumber = 0;
  SIPunch StartPunch{};
  SIPunch FinishPunch{};
  SIPunch CheckPunch{};
  uint32_t nPunch = 0;
  SIPunch Punch[192]{};

  wchar_t firstName[21]{};
  wchar_t lastName[21]{};
  wchar_t club[41]{};
  int miliVolt = 0;
  char readOutTime[32]{};
  bool punchOnly = false;
  ConvertedTimeStatus convertedTime = ConvertedTimeStatus::Unknown;
  int runnerId = 0;
  int relativeFinishTime = 0;
  bool statusOK = false;
  bool statusDNF = false;
  bool isDebugCard = false;

  SICard() = default;

  explicit SICard(ConvertedTimeStatus status) {
    clear(nullptr);
    convertedTime = status;
  }

  bool empty() const { return CardNumber == 0; }

  bool isManualInput() const { return runnerId != 0; }

  // Clears the card if this == condition or condition is nullptr.
  void clear(const SICard* condition) {
    if (this == condition || condition == nullptr)
      std::memset(this, 0, sizeof(SICard));
  }

  // Returns a hash that uniquely identifies a specific card readout.
  unsigned int calculateHash() const {
    unsigned h = nPunch * 100000 + FinishPunch.Time;
    for (unsigned i = 0; i < nPunch; i++) {
      h = h * 31 + Punch[i].Code;
      h = h * 31 + Punch[i].Time;
    }
    h += StartPunch.Time;
    return h;
  }
};

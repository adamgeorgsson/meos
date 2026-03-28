// SICard.h — Minimal SICard data structure for card-course distance matching.
// Full SportIdent implementation comes in US-003f.
#pragma once
#include <cstdint>
#include <cstring>

struct SIPunch {
  uint32_t Code = 0;
  uint32_t Time = 0;
};

struct SICard {
  uint32_t CardNumber = 0;
  SIPunch StartPunch{};
  SIPunch FinishPunch{};
  SIPunch CheckPunch{};
  uint32_t nPunch = 0;
  SIPunch Punch[192]{};

  bool empty() const { return CardNumber == 0; }

  void clear(const SICard *condition) {
    if (this == condition || condition == nullptr)
      std::memset(this, 0, sizeof(SICard));
  }
};

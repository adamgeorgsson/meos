#pragma once

#include <array>
#include <vector>

// ---------------------------------------------------------------------------
// RunnerStatus — competition result status codes.
// Extracted to a standalone header so oSpeakerObject and other domain
// components can use it without pulling in the full oAbstractRunner header.
// ---------------------------------------------------------------------------
enum RunnerStatus {
  StatusOK = 1, StatusDNS = 20, StatusCANCEL = 21, StatusOutOfCompetition = 15,
  StatusMP = 3, StatusDNF = 4, StatusDQ = 5, StatusMAX = 6, StatusNoTiming = 2,
  StatusUnknown = 0, StatusNotCompeting = 99
};

// Order map: lower = better ranking. Initialized at compile time.
// Index is the RunnerStatus enum value (0..99).
inline constexpr std::array<int, 100> RunnerStatusOrderMap = []() {
  std::array<int, 100> m{};
  for (auto& v : m) v = 11; // default: lowest rank
  m[StatusOK]              = 0;
  m[StatusNoTiming]        = 1;
  m[StatusOutOfCompetition]= 2;
  m[StatusMAX]             = 3;
  m[StatusMP]              = 4;
  m[StatusDNF]             = 5;
  m[StatusDQ]              = 6;
  m[StatusCANCEL]          = 7;
  m[StatusDNS]             = 8;
  m[StatusUnknown]         = 9;
  m[StatusNotCompeting]    = 10;
  return m;
}();

enum class DynamicRunnerStatus {
  StatusInactive,
  StatusActive,
  StatusFinished
};

template<int dummy = 0>
inline bool isPossibleResultStatus(RunnerStatus st) {
  return st == StatusNoTiming || st == StatusOutOfCompetition;
}

template<int dummy = 0>
inline std::vector<RunnerStatus> getAllRunnerStatus() {
  return { StatusOK, StatusDNS, StatusCANCEL, StatusOutOfCompetition, StatusMP,
           StatusDNF, StatusDQ, StatusMAX, StatusUnknown, StatusNotCompeting, StatusNoTiming };
}

template<int dummy = 0>
inline bool showResultTime(RunnerStatus st, int time) {
  return st == StatusOK || (st == StatusOutOfCompetition && time > 0);
}

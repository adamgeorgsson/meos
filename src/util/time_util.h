#pragma once

#include <chrono>
#include <ctime>
#include <string>

// Cross-platform "get local time now" — fills a std::tm from the current
// wall-clock time using the thread-safe platform variant.
inline void meos_localtime_now(std::tm* out) {
  auto now = std::chrono::system_clock::now();
  auto tt  = std::chrono::system_clock::to_time_t(now);
#ifdef _WIN32
  localtime_s(out, &tt);
#else
  localtime_r(&tt, out);
#endif
}

namespace meos::util {

std::string formatTimeHMS(int seconds);
int parseTimeHMS(const std::string& hms);

// Returns the current calendar year (cached after first call).
int getThisYear();

}  // namespace meos::util

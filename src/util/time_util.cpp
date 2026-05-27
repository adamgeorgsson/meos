#include "time_util.h"

#include <stdexcept>

namespace meos::util {

std::string formatTimeHMS(int seconds) {
  std::string prefix;
  if (seconds < 0) {
    prefix = "-";
    seconds = -seconds;
  }
  int h = seconds / 3600;
  int m = (seconds % 3600) / 60;
  int s = seconds % 60;
  char buf[32];
  snprintf(buf, sizeof(buf), "%s%02d:%02d:%02d", prefix.c_str(), h, m, s);
  return std::string(buf);
}

int parseTimeHMS(const std::string& hms) {
  // Find the two colons
  auto c1 = hms.find(':');
  auto c2 = hms.find(':', c1 + 1);
  if (c1 == std::string::npos || c2 == std::string::npos || hms.find(':', c2 + 1) != std::string::npos) {
    throw std::invalid_argument("Invalid time format: " + hms);
  }
  std::string hs = hms.substr(0, c1);
  std::string ms = hms.substr(c1 + 1, c2 - c1 - 1);
  std::string ss = hms.substr(c2 + 1);
  if (hs.empty() || ms.empty() || ss.empty()) {
    throw std::invalid_argument("Invalid time format: " + hms);
  }
  for (char ch : hs) { if (ch < '0' || ch > '9') throw std::invalid_argument("Invalid time format: " + hms); }
  for (char ch : ms) { if (ch < '0' || ch > '9') throw std::invalid_argument("Invalid time format: " + hms); }
  for (char ch : ss) { if (ch < '0' || ch > '9') throw std::invalid_argument("Invalid time format: " + hms); }
  int h = std::stoi(hs);
  int m = std::stoi(ms);
  int s = std::stoi(ss);
  return h * 3600 + m * 60 + s;
}

}  // namespace meos::util

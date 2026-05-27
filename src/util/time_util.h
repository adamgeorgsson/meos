#pragma once

#include <string>

namespace meos::util {

std::string formatTimeHMS(int seconds);
int parseTimeHMS(const std::string& hms);

}  // namespace meos::util

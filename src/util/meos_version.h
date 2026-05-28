#pragma once

#include <string>
#include <vector>

// MeOS version/build information functions (portable, no Win32 dependencies).

int getMeosBuild();
std::wstring getMeosDate();
std::wstring getMeosFullVersion();
std::wstring getMajorVersion();
std::wstring getMeosCompectVersion();
void getSupporters(std::vector<std::wstring>& supp, std::vector<std::wstring>& developSupp);

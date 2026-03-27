// Internal helpers shared between oEvent.cpp and oEventAdmin.cpp
// Not part of the public API — do not include from headers.
#pragma once
#include <string>
#include <filesystem>
#include <ctime>
#include "meos_util.h"

static inline void getNewFileName(std::wstring &fn, std::wstring &nameId) {
  std::tm st = {};
  meos_localtime_now(&st);

  wchar_t file[260];
  wchar_t filename[64];
  swprintf(filename, 64, L"meos_%d%02d%02d_%02d%02d%02d_%X.meos",
             (st.tm_year + 1900), (st.tm_mon + 1), st.tm_mday, st.tm_hour, st.tm_min, st.tm_sec, 0 /* std::tm has no milliseconds */);

  getUserFile(file, filename);

  wchar_t CurrentNameId[64];
  {
    std::wstring nameIdStr = std::filesystem::path(file).stem().wstring();
    wcsncpy(CurrentNameId, nameIdStr.c_str(), 63);
    CurrentNameId[63] = L'\0';
  }

  fn = file;
  nameId = CurrentNameId;
}

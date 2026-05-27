/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2026 Melin Software HB

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License fro more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Melin Software HB - software@melin.nu - www.melin.nu
    Eksoppsvägen 16, SE-75646 UPPSALA, Sweden

************************************************************************/

#pragma once

// File-local helpers shared across oEvent.cpp / oEventAdmin.cpp split files.
// Include AFTER all other includes (relies on StdAfx.h, meos_util.h, <filesystem>).

static inline void getNewFileName(std::wstring &fn, std::wstring &nameId) {
  std::tm st = {};
  meos_localtime_now(&st);

  wchar_t file[260];
  wchar_t filename[64];
  swprintf(filename, 64, L"meos_%d%02d%02d_%02d%02d%02d_%X.meos",
             (st.tm_year + 1900), (st.tm_mon + 1), st.tm_mday, st.tm_hour, st.tm_min, st.tm_sec, 0);

  //strcpy_s(CurrentNameId, filename);
  getUserFile(file, filename);

  wchar_t CurrentNameId[64];
  {
    std::wstring stemStr = std::filesystem::path(file).stem().wstring();
    wcsncpy(CurrentNameId, stemStr.c_str(), 63);
    CurrentNameId[63] = L'\0';
  }

  fn = file;
  nameId = CurrentNameId;
}

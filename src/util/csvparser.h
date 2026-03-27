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
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Melin Software HB - software@melin.nu - www.melin.nu
    Eksoppsvägen 16, SE-75646 UPPSALA, Sweden

************************************************************************/
#pragma once

// Core CSV parsing and writing utilities.
// Domain-specific import methods (importOE_CSV, importOS_CSV, etc.) belong
// in the io/ module and are not part of this util-level class.

#include <fstream>
#include <list>
#include <string>
#include <vector>

class csvparser {
protected:
  std::ofstream fout;
  std::ifstream fin;

  int         LineNumber    = 0;
  std::string ErrorMessage;

  void parseUnicode(const std::wstring &file,
                    std::list<std::vector<std::wstring>> &data);

public:
  // Parse a CSV file into rows of wstring fields.
  // Handles UTF-8 BOM, UTF-16 LE BOM, and ANSI (CP-1252) encodings.
  void parse(const std::wstring &file,
             std::list<std::vector<std::wstring>> &dataOutput);

  // Convert an ANSI CSV file to UTF-8 (with BOM) in-place.
  static void convertUTF(const std::wstring &file);

  // Open/close output CSV file.
  bool openOutput(const std::wstring &file, bool writeUTF = false);
  bool closeOutput();

  // Write a row (semicolon-delimited).
  bool outputRow(const std::vector<std::string>  &out);
  bool outputRow(const std::vector<std::wstring> &out);
  bool outputRow(const std::string &row);

  // In-place semicolon-delimited split with quote handling.
  static int split(char    *line, std::vector<char    *> &out);
  static int split(wchar_t *line, std::vector<wchar_t *> &out);

  csvparser();
  virtual ~csvparser();
};

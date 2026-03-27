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
    Eksoppsvägen 16, SE-75646 Uppsala, Sweden

************************************************************************/

// Cross-platform implementation of csvparser.h (core I/O only).
// Win32 APIs (MultiByteToWideChar, _wrename, gdi_main) replaced with
// std::filesystem and meos_util.h helpers (fromUTF8, toUTF8, widen).

#include "csvparser.h"
#include "meos_util.h"
#include "meosexception.h"

#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>

using std::string;
using std::wstring;
using std::vector;
using std::list;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Return true if the byte sequence [s, s+len) is valid UTF-8.
static bool isValidUTF8(const char *s, size_t len) {
  for (size_t i = 0; i < len; ) {
    unsigned char c = (unsigned char)s[i];
    int bytes;
    if      (c < 0x80)              bytes = 1;
    else if ((c & 0xE0) == 0xC0)   bytes = 2;
    else if ((c & 0xF0) == 0xE0)   bytes = 3;
    else if ((c & 0xF8) == 0xF0)   bytes = 4;
    else return false;
    for (int j = 1; j < bytes; j++)
      if (++i >= len || ((unsigned char)s[i] & 0xC0) != 0x80) return false;
    i++;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

csvparser::csvparser() : LineNumber(0) {}
csvparser::~csvparser() = default;

// ---------------------------------------------------------------------------
// split (in-place semicolon-split with quote handling)
// ---------------------------------------------------------------------------

int csvparser::split(char *line, vector<char *> &sv) {
  sv.clear();
  int  len  = (int)strlen(line);
  bool cite = false;

  for (int m = 0; m < len; m++) {
    char *ptr = &line[m];
    if (*ptr == '"') ptr++;           // skip opening quote

    while (line[m]) {
      if (!cite && line[m] == ';') {
        line[m] = 0;                  // end of field
      } else {
        if (line[m] == '"') {
          cite = !cite;
          line[m] = 0;
          if (cite) ptr = &line[m + 1]; // start of quoted content
        }
        m++;
      }
    }
    line[m] = 0;
    sv.push_back(ptr);
  }
  return 0;
}

int csvparser::split(wchar_t *line, vector<wchar_t *> &sv) {
  sv.clear();
  int  len  = (int)wcslen(line);
  bool cite = false;

  for (int m = 0; m < len; m++) {
    wchar_t *ptr = &line[m];
    if (*ptr == L'"') ptr++;

    while (line[m]) {
      if (!cite && line[m] == L';') {
        line[m] = 0;
      } else {
        if (line[m] == L'"') {
          cite = !cite;
          line[m] = 0;
          if (cite) ptr = &line[m + 1];
        }
        m++;
      }
    }
    line[m] = 0;
    sv.push_back(ptr);
  }
  return 0;
}

// ---------------------------------------------------------------------------
// parseUnicode — UTF-16 LE (BOM FF FE) files
// ---------------------------------------------------------------------------

void csvparser::parseUnicode(const wstring &file,
                             list<vector<wstring>> &data) {
  fin.open(std::filesystem::path(file),
           std::ifstream::in | std::ifstream::binary);

  fin.seekg(0, std::ios_base::end);
  int len = (int)fin.tellg() - 2; // skip 2-byte BOM
  if (len <= 0) return;
  fin.seekg(2);

  assert(len % 2 == 0);
  vector<wchar_t> bf(len / 2 + 1, 0);
  fin.read((char *)bf.data(), len);
  fin.close();

  // Split wchar_t buffer into lines.
  vector<wstring> rows;
  int spp = 0;
  for (int k = 0; k < len / 2; k++) {
    if (bf[k] == L'\n') {
      if (k > 0 && bf[k - 1] == L'\r') bf[k - 1] = 0;
      bf[k] = 0;
      rows.push_back(wstring(bf.data() + spp));
      spp = k + 1;
    }
  }
  if (spp < len / 2)
    rows.push_back(wstring(bf.data() + spp));

  vector<wchar_t *> sp;
  for (wstring &row : rows) {
    vector<wchar_t> wbuf(row.begin(), row.end());
    wbuf.push_back(0);
    split(wbuf.data(), sp);
    if (!sp.empty()) {
      data.emplace_back(sp.size());
      for (size_t k = 0; k < sp.size(); k++)
        data.back()[k] = sp[k];
    }
  }
}

// ---------------------------------------------------------------------------
// parse
// ---------------------------------------------------------------------------

void csvparser::parse(const wstring &file,
                      list<vector<wstring>> &data) {
  data.clear();
  fin.open(std::filesystem::path(file));

  if (!fin.good())
    throw meosException(L"Failed to read file, " + file);

  bool isUTF8    = false;
  bool detectType = true;
  string rbf;
  vector<wchar_t *> sp;
  vector<wchar_t>   wbuf;
  wstring decoded;

  while (std::getline(fin, rbf)) {
    const char *bf = rbf.c_str();

    if (detectType) {
      detectType = false;

      // UTF-8 BOM
      if (rbf.size() >= 3 &&
          (unsigned char)bf[0] == 0xEF &&
          (unsigned char)bf[1] == 0xBB &&
          (unsigned char)bf[2] == 0xBF) {
        isUTF8 = true;
        bf += 3;
        // Fall through — process this line below.
      }
      // UTF-16 LE BOM
      else if ((unsigned char)bf[0] == 0xFF && (unsigned char)bf[1] == 0xFE) {
        fin.close();
        parseUnicode(file, data);
        return;
      }
      // Auto-detect: scan all lines for UTF-8 validity.
      else {
        isUTF8 = true;
        while (std::getline(fin, rbf)) {
          if (!rbf.empty() && !isValidUTF8(rbf.c_str(), rbf.size())) {
            isUTF8 = false;
            break;
          }
        }
        fin.clear();
        fin.seekg(0);
        continue; // Restart the main loop from the beginning.
      }
    }

    // Decode the line to wstring.
    if (isUTF8)
      decoded = fromUTF8(string(bf));
    else
      decoded = widen(string(bf));   // CP-1252 → wstring

    wbuf.assign(decoded.begin(), decoded.end());
    wbuf.push_back(0);

    split(wbuf.data(), sp);

    if (!sp.empty()) {
      data.emplace_back(sp.size());
      for (size_t k = 0; k < sp.size(); k++)
        data.back()[k] = sp[k];
    }
  }

  fin.close();
}

// ---------------------------------------------------------------------------
// convertUTF — convert ANSI file to UTF-8 with BOM in-place
// ---------------------------------------------------------------------------

void csvparser::convertUTF(const wstring &file) {
  std::ifstream in;
  in.open(std::filesystem::path(file));

  if (!in.good())
    throw meosException("Failed to read file for UTF conversion");

  // Check if already UTF-8 (BOM present).
  string firstLine;
  if (std::getline(in, firstLine)) {
    if (firstLine.size() >= 3 &&
        (unsigned char)firstLine[0] == 0xEF &&
        (unsigned char)firstLine[1] == 0xBB &&
        (unsigned char)firstLine[2] == 0xBF) {
      return; // Already UTF-8
    }
  }
  in.clear();
  in.seekg(0);

  // Re-read all lines and convert CP-1252 → UTF-8.
  vector<string> out;
  string rbf;
  while (std::getline(in, rbf)) {
    const wstring &ws = widen(rbf);   // CP-1252 → wstring
    out.push_back(toUTF8(ws));        // wstring → UTF-8
  }
  in.close();

  // Rename original to backup.
  std::error_code ec;
  std::filesystem::rename(std::filesystem::path(file),
                          std::filesystem::path(file + L"_"), ec);

  // Write UTF-8 output with BOM.
  std::ofstream fout;
  fout.open(std::filesystem::path(file));
  fout.put('\xEF'); fout.put('\xBB'); fout.put('\xBF'); // UTF-8 BOM
  for (const string &line : out)
    fout << line << "\n";
}

// ---------------------------------------------------------------------------
// openOutput / closeOutput / outputRow
// ---------------------------------------------------------------------------

bool csvparser::openOutput(const wstring &filename, bool writeUTF) {
  checkWriteAccess(filename);
  fout.open(std::filesystem::path(filename));

  if (fout.bad()) return false;

  if (writeUTF) {
    fout.put('\xEF'); fout.put('\xBB'); fout.put('\xBF'); // UTF-8 BOM
  }
  return true;
}

bool csvparser::closeOutput() {
  fout.close();
  return true;
}

bool csvparser::outputRow(const string &row) {
  fout << row << "\n";
  return true;
}

bool csvparser::outputRow(const vector<wstring> &out) {
  vector<string> outUTF(out.size());
  for (size_t i = 0; i < out.size(); i++)
    outUTF[i] = toUTF8(out[i]);
  return outputRow(outUTF);
}

bool csvparser::outputRow(const vector<string> &out) {
  int sz = (int)out.size();
  for (int i = 0; i < sz; i++) {
    string p = out[i];

    // Replace embedded " with '
    for (char &c : p)
      if (c == '"') c = '\'';

    if (i > 0) fout << ";";

    if (p.find_first_of("; ,\t.") != string::npos)
      fout << "\"" << p << "\"";
    else
      fout << p;
  }
  fout << "\n";
  fout.flush();
  return true;
}

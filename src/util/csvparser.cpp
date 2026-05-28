#include "csvparser.h"

#include "meos_util.h"
#include "meosexception.h"
#include "xmlparser.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <strings.h>

using namespace std;

namespace {
string pathToUtf8(const wstring &file) {
  return meos::util::toUTF8(file);
}

bool isValidUTF8(const string &s) {
  const unsigned char *bytes = reinterpret_cast<const unsigned char *>(s.data());
  size_t i = 0;
  while (i < s.size()) {
    unsigned char c = bytes[i];
    if (c <= 0x7F) {
      ++i;
    } else if ((c & 0xE0) == 0xC0) {
      if (i + 1 >= s.size() || (bytes[i + 1] & 0xC0) != 0x80 || c < 0xC2)
        return false;
      i += 2;
    } else if ((c & 0xF0) == 0xE0) {
      if (i + 2 >= s.size())
        return false;
      unsigned char c1 = bytes[i + 1], c2 = bytes[i + 2];
      if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80)
        return false;
      if (c == 0xE0 && c1 < 0xA0)
        return false;
      if (c == 0xED && c1 >= 0xA0)
        return false;
      i += 3;
    } else if ((c & 0xF8) == 0xF0) {
      if (i + 3 >= s.size())
        return false;
      unsigned char c1 = bytes[i + 1], c2 = bytes[i + 2], c3 = bytes[i + 3];
      if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80)
        return false;
      if (c == 0xF0 && c1 < 0x90)
        return false;
      if (c > 0xF4 || (c == 0xF4 && c1 >= 0x90))
        return false;
      i += 4;
    } else {
      return false;
    }
  }
  return true;
}

wstring decodeUtf16Le(const string &bytes) {
  wstring out;
  out.reserve(bytes.size() / 2);
  for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
    uint16_t cu = static_cast<unsigned char>(bytes[i]) |
                  (static_cast<uint16_t>(static_cast<unsigned char>(bytes[i + 1])) << 8);
    if (cu >= 0xD800 && cu <= 0xDBFF && i + 3 < bytes.size()) {
      uint16_t cu2 = static_cast<unsigned char>(bytes[i + 2]) |
                     (static_cast<uint16_t>(static_cast<unsigned char>(bytes[i + 3])) << 8);
      if (cu2 >= 0xDC00 && cu2 <= 0xDFFF) {
        uint32_t cp = 0x10000 + (((cu - 0xD800) << 10) | (cu2 - 0xDC00));
        out.push_back(static_cast<wchar_t>(cp));
        i += 2;
        continue;
      }
    }
    out.push_back(static_cast<wchar_t>(cu));
  }
  return out;
}

vector<wstring> splitLines(const wstring &content) {
  vector<wstring> lines;
  size_t start = 0;
  for (size_t i = 0; i < content.size(); ++i) {
    if (content[i] == L'\n') {
      size_t end = (i > start && content[i - 1] == L'\r') ? i - 1 : i;
      lines.push_back(content.substr(start, end - start));
      start = i + 1;
    }
  }
  if (start < content.size()) {
    size_t end = content.size();
    if (end > start && content[end - 1] == L'\r')
      --end;
    lines.push_back(content.substr(start, end - start));
  }
  return lines;
}
} // namespace

csvparser::csvparser() {
  LineNumber = 0;
}

csvparser::~csvparser() = default;

csvparser::CSV csvparser::iscsv(const wstring &file) {
  ifstream fin(pathToUtf8(file), ios::binary);
  if (!fin.good())
    return CSV::NoCSV;

  char bf[2048] = {0};
  bool isCSVType = false;
  while (fin.good() && !isCSVType) {
    fin.getline(bf, 2048);
    isCSVType = strlen(bf) >= 3;
  }
  fin.close();

  vector<char *> sp;
  split(bf, sp);
  if (!sp.empty()) {
    string sp0 = sp[0];
    if (sp0.find("<?xml") != string::npos)
      return CSV::NoCSV;
  }

  if (sp.size() == 1 && strcmp(sp[0], "RAIDDATA") == 0)
    return CSV::RAID;
  if (sp.size() < 2)
    return CSV::NoCSV;

  if (strcasecmp(sp[1], "Descr") == 0 || strcasecmp(sp[1], "Namn") == 0 ||
      strcasecmp(sp[1], "Descr.") == 0 || strcasecmp(sp[1], "Navn") == 0)
    return CSV::OS;
  return CSV::OE;
}

bool csvparser::openOutput(const wstring &filename, bool writeUTF) {
  fout.open(pathToUtf8(filename), ios::binary);
  if (fout.bad())
    return false;
  if (writeUTF) {
    fout.put(char(0xEF));
    fout.put(char(0xBB));
    fout.put(char(0xBF));
  }
  return true;
}

bool csvparser::outputRow(const string &row) {
  fout << row << endl;
  return true;
}

bool csvparser::outputRow(const vector<wstring> &out) {
  vector<string> outUTF(out.size());
  for (size_t i = 0; i < out.size(); i++)
    outUTF[i] = meos::util::toUTF8(out[i]);
  return outputRow(outUTF);
}

bool csvparser::outputRow(const vector<string> &out) {
  int size = static_cast<int>(out.size());
  for (int i = 0; i < size; i++) {
    string p = out[i];
    size_t found = p.find('"');
    while (found != string::npos) {
      p[found] = '\'';
      found = p.find('"', found + 1);
    }
    if (i > 0)
      fout << ";";
    if (p.find_first_of("; ,\t.") != string::npos)
      fout << '"' << p << '"';
    else
      fout << p;
  }
  fout << endl;
  fout.flush();
  return true;
}

bool csvparser::closeOutput() {
  fout.close();
  return true;
}

int csvparser::split(char *line, vector<char *> &split_vector) {
  split_vector.clear();
  int len = strlen(line);
  bool cite = false;
  for (int m = 0; m < len; m++) {
    char *ptr = &line[m];
    if (*ptr == '"')
      ptr++;
    while (line[m]) {
      if (!cite && line[m] == ';')
        line[m] = 0;
      else {
        if (line[m] == '"') {
          cite = !cite;
          line[m] = 0;
          if (cite)
            ptr = &line[m + 1];
        }
        m++;
      }
    }
    line[m] = 0;
    split_vector.push_back(ptr);
  }
  return 0;
}

int csvparser::split(wchar_t *line, vector<wchar_t *> &split_vector) {
  split_vector.clear();
  int len = wcslen(line);
  bool cite = false;
  for (int m = 0; m < len; m++) {
    wchar_t *ptr = &line[m];
    if (*ptr == L'"')
      ptr++;
    while (line[m]) {
      if (!cite && line[m] == L';')
        line[m] = 0;
      else {
        if (line[m] == L'"') {
          cite = !cite;
          line[m] = 0;
          if (cite)
            ptr = &line[m + 1];
        }
        m++;
      }
    }
    line[m] = 0;
    split_vector.push_back(ptr);
  }
  return 0;
}

void csvparser::parseUnicode(const wstring &file, list<vector<wstring>> &data) {
  ifstream fin(pathToUtf8(file), ios::binary);
  if (!fin.good())
    return;
  ostringstream ss;
  ss << fin.rdbuf();
  string bytes = ss.str();
  if (bytes.size() < 2)
    return;
  wstring content = decodeUtf16Le(bytes.substr(2));
  vector<wstring> rows = splitLines(content);
  vector<wchar_t *> sp;
  for (auto &row : rows) {
    vector<wchar_t> mutableRow(row.begin(), row.end());
    mutableRow.push_back(0);
    split(mutableRow.data(), sp);
    if (!sp.empty()) {
      data.push_back(vector<wstring>());
      data.back().resize(sp.size());
      for (size_t k = 0; k < sp.size(); k++)
        data.back()[k] = sp[k];
    }
  }
}

void csvparser::parse(const wstring &file, list<vector<wstring>> &data) {
  data.clear();
  ifstream fin(pathToUtf8(file), ios::binary);
  if (!fin.good())
    throw meosException(L"Failed to read file, " + file);
  ostringstream ss;
  ss << fin.rdbuf();
  string raw = ss.str();
  fin.close();

  if (raw.size() >= 2 && static_cast<unsigned char>(raw[0]) == 0xFF &&
      static_cast<unsigned char>(raw[1]) == 0xFE) {
    parseUnicode(file, data);
    return;
  }

  bool isUTF8 = false;
  size_t offset = 0;
  if (raw.size() >= 3 && static_cast<unsigned char>(raw[0]) == 0xEF &&
      static_cast<unsigned char>(raw[1]) == 0xBB &&
      static_cast<unsigned char>(raw[2]) == 0xBF) {
    isUTF8 = true;
    offset = 3;
  } else {
    isUTF8 = isValidUTF8(raw);
  }

  vector<wchar_t *> sp;
  size_t start = offset;
  while (start <= raw.size()) {
    size_t end = raw.find('\n', start);
    if (end == string::npos)
      end = raw.size();
    string line = raw.substr(start, end - start);
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    wstring w = isUTF8 ? meos::util::fromUTF8(line) : meos::util::widen(line);
    vector<wchar_t> mutableRow(w.begin(), w.end());
    mutableRow.push_back(0);
    split(mutableRow.data(), sp);
    if (!sp.empty()) {
      data.push_back(vector<wstring>());
      data.back().resize(sp.size());
      for (size_t k = 0; k < sp.size(); k++)
        data.back()[k] = sp[k];
    }
    if (end == raw.size())
      break;
    start = end + 1;
  }
}

void csvparser::convertUTF(const wstring &file) {
  ifstream fin(pathToUtf8(file), ios::binary);
  if (!fin.good())
    throw meosException("Failed to read file");
  ostringstream ss;
  ss << fin.rdbuf();
  string raw = ss.str();
  fin.close();

  if (raw.size() >= 3 && static_cast<unsigned char>(raw[0]) == 0xEF &&
      static_cast<unsigned char>(raw[1]) == 0xBB &&
      static_cast<unsigned char>(raw[2]) == 0xBF)
    return;

  string converted;
  if (raw.size() >= 2 && static_cast<unsigned char>(raw[0]) == 0xFF &&
      static_cast<unsigned char>(raw[1]) == 0xFE)
    converted = meos::util::toUTF8(decodeUtf16Le(raw.substr(2)));
  else
    converted = meos::util::toUTF8(meos::util::widen(raw));

  namespace fs = std::filesystem;
  fs::path src(pathToUtf8(file));
  fs::path backup(pathToUtf8(file + L"_"));
  std::error_code ec;
  fs::remove(backup, ec);
  fs::rename(src, backup, ec);
  if (ec)
    throw meosException("Failed to rename file");

  ofstream fout(pathToUtf8(file), ios::binary);
  fout.put(char(0xEF));
  fout.put(char(0xBB));
  fout.put(char(0xBF));
  fout << converted;
}

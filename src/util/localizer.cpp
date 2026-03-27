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

#include "localizer.h"
#include "meos_util.h"
#include "meosexception.h"

#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <random>
#include <cwctype>

using std::wstring;
using std::string;
using std::vector;
using std::map;

// ---- Global Localizer instance -----------------------------------------------
Localizer lang;

// ---- LocalizerImpl -----------------------------------------------------------

class LocalizerImpl {
  wstring language;
  map<wstring, wstring> table;
  map<wstring, wstring> unknown;
  mutable oWordList *givenNames = nullptr;

  void loadTableRaw(const vector<string> &raw, const wstring &lang);
  void addUnknown(const wstring &var);

public:
  const oWordList &getGivenNames() const;
  void translateAll(const LocalizerImpl &all);
  const wstring &translate(const wstring &str, bool &found) const;
  void saveUnknown(const wstring &file) const;
  void saveTable(const wstring &file) const;
  void saveTranslation(const wstring &file) const;
  void loadTable(const wstring &file, const wstring &lang);
  void clear();

  LocalizerImpl()  = default;
  ~LocalizerImpl() { delete givenNames; }
};

// ---- LocalizerInternal -------------------------------------------------------

Localizer::LocalizerInternal::LocalizerInternal() {
  impl = new LocalizerImpl();
}

Localizer::LocalizerInternal::~LocalizerInternal() {
  if (user) {
    user->owning = true;
    impl    = nullptr;
    implBase = nullptr;
  } else {
    delete impl;
    delete implBase;
  }
}

void Localizer::LocalizerInternal::set(Localizer &lio) {
  std::lock_guard<std::mutex> lk(mtx);
  LocalizerInternal &li = *lio.linternal;
  if (li.user || user)
    throw std::runtime_error("Runtime error");
  if (owning) {
    delete impl;
    delete implBase;
  }
  implBase = li.implBase;
  impl     = li.impl;
  li.user  = this;
}

std::vector<wstring> Localizer::LocalizerInternal::getLangResource() const {
  std::vector<wstring> v;
  for (const auto &kv : langResource)
    v.push_back(kv.first);
  return v;
}

const oWordList &Localizer::LocalizerInternal::getGivenNames() const {
  return impl->getGivenNames();
}

const wstring &Localizer::LocalizerInternal::tl(const wstring &str) const {
  bool found;
  const wstring *ret = &impl->translate(str, found);
  if (found || !implBase)
    return *ret;
  ret = &implBase->translate(str, found);
  return *ret;
}

void Localizer::LocalizerInternal::loadLangResource(const wstring &name) {
  auto it = langResource.find(name);
  if (it == langResource.end())
    throw std::runtime_error("Unknown language");
  std::lock_guard<std::mutex> lk(mtx);
  impl->loadTable(it->second, name);
}

void Localizer::LocalizerInternal::addLangResource(const wstring &name, const wstring &filePath) {
  std::lock_guard<std::mutex> lk(mtx);
  langResource[name] = filePath;
  if (!implBase) {
    implBase = new LocalizerImpl();
    implBase->loadTable(filePath, name);
  }
}

void Localizer::LocalizerInternal::debugDump(const wstring &untranslated, const wstring &translated) const {
  if (implBase)
    impl->translateAll(*implBase);
  impl->saveUnknown(untranslated);
  impl->saveTable(translated);
  impl->saveTranslation(L"spellcheck.txt");
}

// ---- LocalizerImpl implementation --------------------------------------------

const oWordList &LocalizerImpl::getGivenNames() const {
  if (!givenNames)
    givenNames = new oWordList();
  return *givenNames;
}

void LocalizerImpl::clear() {
  table.clear();
  unknown.clear();
  language.clear();
}

void LocalizerImpl::addUnknown(const wstring &key) {
  if (unknown.emplace(key, L"").second)
    std::cerr << narrow(L"Missing resource: " + key);
}

void LocalizerImpl::loadTable(const wstring &file, const wstring &lang) {
  clear();
  std::ifstream fin(std::filesystem::path(file), std::ios::in);
  if (!fin.good())
    return;

  vector<string> raw;
  char bf[8 * 1024];
  while (!fin.eof()) {
    bf[0] = 0;
    fin.getline(bf, sizeof(bf));
    if (bf[0] != 0 && bf[0] != '#')
      raw.push_back(bf);
  }

  loadTableRaw(raw, lang);
}

void LocalizerImpl::loadTableRaw(const vector<string> &raw, const wstring &lang) {
  vector<int> order(raw.size());
  for (size_t k = 0; k < raw.size(); k++)
    order[k] = (int)k;

  // Randomize insertion order for balanced map insertion
  {
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(order.begin(), order.end(), g);
  }

  table.clear();
  language = lang;
  const string nline = "\n";

  for (size_t k = 0; k < raw.size(); k++) {
    const string &s = raw[order[k]];
    size_t pos = s.find_first_of('=');
    if (pos == string::npos)
      throw std::runtime_error("Bad file format.");

    size_t spos = pos;
    size_t epos = pos + 1;
    const unsigned char *udata = (const unsigned char *)s.data();

    // Trim trailing spaces and NBSP before '='
    while (spos > 0) {
      if (isspace(udata[spos - 1]))
        spos--;
      else if (udata[spos - 1] == 0xC2 && spos > 1 && udata[spos - 2] == 0xA0) // UTF-8 NBSP
        spos -= 2;
      else
        break;
    }

    // Trim leading spaces and NBSP after '='
    while (epos < s.size()) {
      if (isspace(udata[epos]))
        epos++;
      else if (udata[epos] == 0xC2 && epos + 1 < s.size() && udata[epos + 1] == 0xA0)
        epos += 2;
      else
        break;
    }

    string key   = s.substr(0, spos);
    string value = s.substr(epos);

    if (value.empty())
      throw std::runtime_error("Bad file format.");

    // Strip BOM artifact 'Â' prefix (0xC2 byte that appears when NBSP gets double-encoded)
    if (value.size() > 1 && (unsigned char)value[0] == 0xC2)
      value = value.substr(2);

    // Replace \n escape sequences with actual newlines
    size_t nl = value.find("\\n");
    while (nl != string::npos) {
      value.replace(nl, 2, nline);
      nl = value.find("\\n");
    }

    table[fromUTF8(key)] = fromUTF8(value);
  }
}

void LocalizerImpl::translateAll(const LocalizerImpl &all) {
  bool f;
  for (const auto &kv : all.table) {
    translate(kv.first, f);
    if (!f)
      unknown[kv.first] = kv.second;
  }
}

void LocalizerImpl::saveUnknown(const wstring &file) const {
  if (unknown.empty())
    return;
  std::ofstream fout(std::filesystem::path(file), std::ios::trunc | std::ios::out);
  const wstring newline = L"\n";
  for (const auto &kv : unknown) {
    wstring value = kv.second;
    wstring key   = kv.first;
    if (value.empty()) {
      value = key;
      size_t nl = value.find(newline);
      size_t n2 = value.find(L".");
      if (nl != wstring::npos || n2 != wstring::npos) {
        while (nl != wstring::npos) {
          value.replace(nl, newline.length(), L"\\n");
          nl = value.find(newline);
        }
        key = L"help:" + itow((int)value.length()) + itow((int)value.find_first_of('.'));
      }
    } else {
      size_t nl = value.find(newline);
      while (nl != wstring::npos) {
        value.replace(nl, newline.length(), L"\\n");
        nl = value.find(newline);
      }
    }
    fout << toUTF8(key) << " = " << toUTF8(value) << "\n";
  }
}

void LocalizerImpl::saveTable(const wstring &file) const {
  std::ofstream fout(std::filesystem::path(language + L"_" + file),
                     std::ios::trunc | std::ios::out);
  const wstring newline = L"\n";
  for (const auto &kv : table) {
    wstring value = kv.second;
    size_t nl = value.find(newline);
    while (nl != wstring::npos) {
      value.replace(nl, newline.length(), L"\\n");
      nl = value.find(newline);
    }
    fout << toUTF8(kv.first) << " = " << toUTF8(value) << "\n";
  }
}

void LocalizerImpl::saveTranslation(const wstring &file) const {
  std::ofstream fout(std::filesystem::path(language + L"_" + file),
                     std::ios::trunc | std::ios::out);
  for (const auto &kv : table)
    fout << toUTF8(kv.second) << "\n";
}

// Thread-safe translation: uses thread_local circular buffer for returned references.
// The table map is read-only after loading, so concurrent reads are safe.
const wstring &LocalizerImpl::translate(const wstring &str, bool &found) const {
  found = false;
  thread_local int i = 0;
  constexpr int bsize = 17;
  thread_local wstring value[bsize];
  int len = (int)str.length();

  if (len == 0)
    return _EmptyWString;

  if (str[0] == '#') {
    i = (i + 1) % bsize;
    value[i] = str.substr(1);
    found = true;
    return value[i];
  }

  auto isDigit = [](wchar_t c) { return c >= L'0' && c <= L'9'; };

  if (str[0] == L',' || str[0] == L' ' || str[0] == L'.'
      || str[0] == L':' || str[0] == L';' || str[0] == L'<' || str[0] == L'>'
      || str[0] == L'-' || str[0] == 0x96 || str[0] == L'\u00d7' || isDigit(str[0])) {

    unsigned k = 1;
    while (k < str.size() && (str[k] == L' ' || str[k] == L'.' || str[k] == L':'
           || str[k] == L'<' || str[k] == L'>' || str[k] == L'-'
           || str[k] == 0x96 || str[k] == L'\u00d7' || isDigit(str[k])))
      k++;

    if (k < str.length()) {
      wstring sub = str.substr(k);
      i = (i + 1) % bsize;
      value[i] = str.substr(0, k) + translate(sub, found);
      return value[i];
    }
  }

  auto it = table.find(str);
  if (it != table.end()) {
    found = true;
    return it->second;
  }

  size_t subst = str.find_first_of(L'#');
  if (subst != str.npos) {
    wstring s = translate(str.substr(0, subst), found);
    vector<wstring> split_vec;
    split(str.substr(subst + 1), wstring(L"#"), split_vec);
    split_vec.push_back(L"");
    const wchar_t *subsymb = L"XYZW";
    size_t subpos  = 0;
    wstring ret;
    size_t lastpos = 0;
    for (size_t k = 0; k < s.size(); k++) {
      if (subpos >= split_vec.size() || subpos >= 4)
        break;
      if (s[k] == subsymb[subpos]) {
        if (k > 0 && std::iswalnum(s[k - 1]))
          continue;
        if (k + 1 < s.size() && std::iswalnum(s[k + 1]))
          continue;
        ret += s.substr(lastpos, k - lastpos);
        ret += split_vec[subpos];
        lastpos = k + 1;
        subpos++;
      }
    }
    if (lastpos < s.size())
      ret += s.substr(lastpos);
    i = (i + 1) % bsize;
    swap(value[i], ret);
    return value[i];
  } else if (str[0] == L'@') {
    i = (i + 1) % bsize;
    value[i] = str.substr(1);
    found = true;
    return value[i];
  }

  wchar_t last = str[len - 1];
  if (last != L':' && last != L'.' && last != L' ' && last != L','
      && last != L';' && last != L'<' && last != L'>' && last != L'-'
      && last != 0x96 && last != 215 && !isDigit(last)) {
    found = false;
    i = (i + 1) % bsize;
    value[i] = str;
    return value[i];
  }

  wstring suffix;
  int pos = (int)str.find_last_not_of(last);

  while (pos > 0) {
    wchar_t l = str[pos];
    if (l != L':' && l != L' ' && l != L',' && l != L'.'
        && l != L';' && l != L'<' && l != L'>' && l != L'-'
        && l != 0x96 && l != 215 && !isDigit(l))
      break;
    pos = (int)str.find_last_not_of(l, pos);
  }

  suffix = str.substr(pos + 1);
  wstring key = str.substr(0, str.length() - suffix.length());
  it = table.find(key);
  if (it != table.end()) {
    i = (i + 1) % bsize;
    value[i] = it->second + suffix;
    found = true;
    return value[i];
  }

  found = false;
  i = (i + 1) % bsize;
  value[i] = str;
  return value[i];
}

// ---- Localizer public interface ----------------------------------------------

bool Localizer::capitalizeWords() const {
  return tl(L"Lyssna") == L"Listen";
}

const wstring &Localizer::tl(const string &str) const {
  if (str.empty())
    return _EmptyWString;
  wstring key(str.begin(), str.end());
  for (wchar_t &c : key)
    c = 0xFF & (unsigned char)c;
  return linternal->tl(key);
}

const wstring Localizer::tl(const wstring &str, bool cap) const {
  wstring w = linternal->tl(str);
  if (capitalizeWords())
    ::capitalizeWords(w);
  return w;
}

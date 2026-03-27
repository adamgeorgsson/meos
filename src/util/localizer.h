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
#include <map>
#include <string>
#include <vector>
#include <mutex>

// Stub oWordList — full implementation deferred to domain migration (US-003)
// Provides the interface used by getNameSplitPoint and IOF import.
class oWordList {
public:
  void insert(const wchar_t* /*s*/) {}
  bool lookup(const wchar_t* /*s*/) const { return false; }
  oWordList()  = default;
  ~oWordList() = default;
};

class LocalizerImpl;

class Localizer {
  class LocalizerInternal {
  private:
    // Maps language name -> file path (replaces Win32 resource IDs)
    std::map<std::wstring, std::wstring> langResource;
    LocalizerImpl *impl     = nullptr;
    LocalizerImpl *implBase = nullptr;
    bool owning             = true;
    LocalizerInternal *user = nullptr;
    mutable std::mutex mtx;  // protects load operations

  public:
    void debugDump(const std::wstring &untranslated, const std::wstring &translated) const;

    std::vector<std::wstring> getLangResource() const;
    void loadLangResource(const std::wstring &name);
    void addLangResource(const std::wstring &name, const std::wstring &filePath);

    const std::wstring &tl(const std::wstring &str) const;

    void set(Localizer &li);

    const oWordList &getGivenNames() const;

    LocalizerInternal();
    ~LocalizerInternal();
  };

  LocalizerInternal *linternal = nullptr;

public:
  bool capitalizeWords() const;

  LocalizerInternal &get() { return *linternal; }
  const std::wstring &tl(const std::string &str) const;
  const std::wstring &tl(const std::wstring &str) const { return linternal->tl(str); }
  const std::wstring  tl(const std::wstring &str, bool cap) const;

  void init()   { linternal = new LocalizerInternal(); }
  void unload() { delete linternal; linternal = nullptr; }

  Localizer() = default;
  ~Localizer() { unload(); }
};

extern Localizer lang;

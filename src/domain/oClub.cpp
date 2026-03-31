// oClub.cpp — Club domain entity implementation.
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

#include "oClub.h"
#include "oDataContainer.h"
#include "oEvent.h"
#include "../util/gdioutput.h"
#include "../util/Table.h"

#include "../util/meos_util.h"
#include "../util/xmlparser.h"
#include "../util/csvparser.h"

#include <iostream>
#include <filesystem>

// ── Construction / Destruction ──────────────────────────────────────────────

oClub::oClub(oEvent *poe) : oBase(poe) {
  getDI().initData();
  Id = oe->getFreeClubId();
}

oClub::oClub(oEvent *poe, int id) : oBase(poe) {
  getDI().initData();
  Id = id;
  if (id != cVacantId && id != cNoClubId)
    oe->qFreeClubId = std::max(id, oe->qFreeClubId);
}

oClub::~oClub() = default;

map<wstring, wstring> oClub::manualCompactNameMap;

void oClub::loadNameMap() {
  std::filesystem::path path("clubnamemap.csv");
  bool good = false;
  if (std::filesystem::exists(path)) {
    csvparser csv;
    list<vector<wstring>> data;
    csv.parse(path.wstring(), data);
    good = true;
    for (auto &row : data) {
      if (row.size() == 2)
        manualCompactNameMap[row[0]] = row[1];
      else
        good = false;
    }
  }
  if (!good)
    std::cerr << "Warning: clubnamemap.csv not found or invalid.\n";
}

// ── Info ────────────────────────────────────────────────────────────────────

wstring oClub::getInfo() const {
  return L"Club: " + name;
}

// ── XML serialization ───────────────────────────────────────────────────────

bool oClub::write(xmlparser &xml) {
  if (Removed) return true;

  xml.startTag("Club");
  xml.write("Id", Id);
  xml.write("Updated", getStamp());
  xml.write("Name", name);
  for (size_t k = 0; k < altNames.size(); k++)
    xml.write("AltName", altNames[k]);

  getDI().write(xml);
  xml.endTag();
  return true;
}

void oClub::set(const xmlobject &xo) {
  xmlList xl;
  xo.getObjects(xl);

  wstring tName;
  for (auto it = xl.begin(); it != xl.end(); ++it) {
    if (it->is("Id")) {
      Id = it->getInt();
    } else if (it->is("Name")) {
      tName = it->getWStr();
    } else if (it->is("oData")) {
      getDI().set(*it);
    } else if (it->is("Updated")) {
      Modified.setStamp(it->getRawStr());
    } else if (it->is("AltName")) {
      altNames.push_back(it->getWStr());
    }
  }
  internalSetName(tName);
}

// ── Name management ─────────────────────────────────────────────────────────

void oClub::internalSetName(const wstring &n) {
  name = n;
  const wchar_t *bf = name.c_str();
  int len = static_cast<int>(name.length());
  int ix = -1;
  for (int k = 0; k <= len - 9; k++) {
    if (bf[k] == 'S') {
      if (wcscmp(bf + k, L"Skid o OK") == 0) { ix = k; break; }
      if (wcscmp(bf + k, L"Skid o OL") == 0) { ix = k; break; }
    }
  }
  if (ix >= 0) {
    tPrettyName = name;
    if (wcscmp(bf + ix, L"Skid o OK") == 0)
      tPrettyName.replace(ix, 9, L"SOK", 3);
    else if (wcscmp(bf + ix, L"Skid o OL") == 0)
      tPrettyName.replace(ix, 9, L"SOL", 3);
  } else {
    tPrettyName.clear();
  }

  wstring sn = getDCI().getString("ShortName");
  if (!sn.empty() && sn != n) {
    tCompactName = sn;
  } else {
    auto res = manualCompactNameMap.find(name);
    if (res != manualCompactNameMap.end()) {
      tCompactName = res->second;
      return;
    }

    vector<wstring> out;
    split(getDisplayName(), L" ", out);
    int skipped = 0;
    bool properName = false;
    for (auto &w : out) {
      bool skip = false;
      if (w.size() <= 3) {
        skip = true;
        for (size_t i = 0; i < w.size(); i++)
          skip = skip && w[i] >= 'A' && w[i] <= 'Z';
      } else if (compareStringIgnoreCase(w, L"GOIF") == 0 ||
                 compareStringIgnoreCase(w, L"Orientering") == 0 ||
                 compareStringIgnoreCase(w, L"Orienteering") == 0 ||
                 compareStringIgnoreCase(w, L"Suunnistajat") == 0) {
        skip = true;
      } else {
        int pCount = 0;
        for (size_t i = 0; i < w.size(); i++) {
          if (w[i] > 'Z') {
            pCount++;
            if (pCount >= 2) { properName = true; break; }
          }
        }
      }
      if (skip) { w.clear(); skipped++; }
    }

    tCompactName = L"";
    if (skipped > 0 && properName) {
      for (auto &w : out) {
        if (!w.empty()) {
          if (tCompactName.empty())
            tCompactName = w;
          else
            tCompactName += L" " + w;
        }
      }
    }

    auto cn = manualCompactNameMap.find(tCompactName);
    if (cn != manualCompactNameMap.end())
      tCompactName = cn->second;
  }
}

void oClub::setName(const wstring &n) {
  if (n != name) {
    internalSetName(n);
    updateChanged();
  }
}

// ── Data buffer ─────────────────────────────────────────────────────────────

oDataContainer &oClub::getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const {
  data = (pvoid)oData;
  olddata = (pvoid)oDataOld;
  strData = nullptr;
  return *oe->oClubData;
}

// ── Comparator ──────────────────────────────────────────────────────────────

bool oClub::operator<(const oClub &c) const {
  return compareStringIgnoreCase(name, c.name) < 0;
}

// ── Lifecycle ───────────────────────────────────────────────────────────────

void oClub::remove() {
  if (oe) oe->removeClub(Id);
}

bool oClub::canRemove() const {
  return !oe->isClubUsed(Id);
}

bool oClub::isVacant() const {
  return getId() == oe->getVacantClubIfExist(false);
}

bool oClub::sameClub(const oClub &c) {
  return compareStringIgnoreCase(name, c.name) == 0;
}

void oClub::updateFromDB() {
  // RunnerDB not available in domain layer — stub (implemented in net/db layer).
}

void oClub::changedObject() {
  if (oe) {
    oe->globalModification = true;
    oe->sqlClubs.changed = true;
  }
}

void oClub::changeId(int newId) {
  pClub old = oe->clubIdIndex[Id];
  if (old == this)
    oe->clubIdIndex.remove(Id);

  oBase::changeId(newId);

  oe->clubIdIndex[newId] = this;
}

// ── Table/input (GUI-coupled; stubs in domain layer) ─────────────────────────

int oClub::getTableId() const {
  return Id;
}

pair<int, bool> oClub::inputData(int id, const wstring &input,
                                  int inputId, wstring &output, bool noUpdate) {
  synchronize(false);
  if (id > 1000) {
    return oe->oClubData->inputData(this, id, input, inputId, output, noUpdate);
  }
  switch (id) {
    case TID_CLUB:
      setName(input);
      synchronize();
      output = getName();
      break;
  }
  return make_pair(0, false);
}

void oClub::fillInput(int id, vector<pair<wstring, size_t>> &out, size_t &selected) {
  if (id > 1000) {
    oe->oClubData->fillInput(this, id, 0, out, selected);
    return;
  }
}

// ── DI fields ────────────────────────────────────────────────────────────────

int oClub::getStartGroup() const {
  return getDCI().getInt("StartGroup");
}

void oClub::setStartGroup(int sg) {
  getDI().setInt("StartGroup", sg);
}

// ── Static helpers ───────────────────────────────────────────────────────────

void oClub::assignInvoiceNumber(oEvent &oe, bool reset) {
  int numberStored = oe.getPropertyInt("FirstInvoice", 100);
  int number = numberStored;
  if (!reset) {
    int maxInvoice = 0;
    for (auto it = oe.Clubs.begin(); it != oe.Clubs.end(); ++it) {
      if (it->isRemoved()) continue;
      int no = it->getDCI().getInt("InvoiceNo");
      maxInvoice = std::max(maxInvoice, no);
    }
    if (maxInvoice != 0)
      number = maxInvoice + 1;
    else
      reset = true;
  }
  for (auto it = oe.Clubs.begin(); it != oe.Clubs.end(); ++it) {
    if (it->isRemoved()) continue;
    if (reset || it->getDCI().getInt("InvoiceNo") == 0) {
      it->getDI().setInt("InvoiceNo", number++);
      it->synchronize(true);
    }
  }
  if (number > numberStored)
    oe.setProperty("FirstInvoice", number);
}

int oClub::getFirstInvoiceNumber(oEvent &oe) {
  int number = 0;
  for (auto it = oe.Clubs.begin(); it != oe.Clubs.end(); ++it) {
    if (it->isRemoved()) continue;
    int no = it->getDCI().getInt("InvoiceNo");
    if (no > 0) {
      if (number == 0) number = no;
      else number = std::min(number, no);
    }
  }
  return number;
}

void oClub::clearClubs(oEvent &oe) {
  vector<pClub> c;
  oe.getClubs(c, false);
  for (size_t k = 0; k < c.size(); k++)
    oe.removeClub(c[k]->getId());
}

// ── Merge ───────────────────────────────────────────────────────────────────

void oClub::merge(const oBase &input, const oBase *base) {
  const oClub &src = dynamic_cast<const oClub &>(input);
  setName(src.getName());
  if (getDI().merge(input, base))
    updateChanged();
  synchronize(true);
}

// ── oEvent methods for club management (implemented in this TU) ──────────────

pClub oEvent::getClub(int Id) const {
  if (Id <= 0) return nullptr;
  pClub value;
  if (clubIdIndex.lookup(Id, value)) return value;
  return nullptr;
}

pClub oEvent::getClub(const wstring &pname) const {
  for (auto it = Clubs.begin(); it != Clubs.end(); ++it)
    if (it->name == pname) return pClub(&*it);
  return nullptr;
}

pClub oEvent::addClub(const wstring &pname, int createId) {
  if (createId > 0) {
    pClub pc = getClub(createId);
    if (pc) return pc;
  }
  if (createId == 0) createId = getFreeClubId();

  oClub c(this, createId);
  c.setName(pname);
  return addClub(c);
}

pClub oEvent::addClub(const oClub &oc) {
  if (clubIdIndex.count(oc.Id) != 0)
    return clubIdIndex[oc.Id];

  Clubs.push_back(oc);
  Clubs.back().addToEvent(this, &oc);

  if (!oc.existInDB())
    Clubs.back().synchronize();

  clubIdIndex[Clubs.back().Id] = &Clubs.back();
  return &Clubs.back();
}

void oEvent::getClubs(vector<pClub> &c, bool sort) {
  if (sort) Clubs.sort();
  c.clear();
  c.reserve(Clubs.size());
  for (auto it = Clubs.begin(); it != Clubs.end(); ++it) {
    if (!it->isRemoved())
      c.push_back(&*it);
  }
}

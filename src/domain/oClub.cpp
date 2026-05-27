#include "oClub.h"
#include "oDataContainer.h"
#include "oEvent.h"

#include <algorithm>
#include <cassert>
#include <cwchar>

using namespace std;

// -----------------------------------------------------------------------
// Static member definition
// -----------------------------------------------------------------------

map<wstring, wstring> oClub::manualCompactNameMap;

// -----------------------------------------------------------------------
// Static DataContainer
// -----------------------------------------------------------------------

oDataContainer& oClub::container() {
  static oDataContainer dc(384);
  static bool initialized = false;
  if (!initialized) {
    dc.addVariableInt("District", oDataContainer::oIS32, "Organisation");
    dc.addVariableString("ShortName", 8, "Kortnamn");
    dc.addVariableString("CareOf", 31, "c/o");
    dc.addVariableString("Street", 41, "Gata");
    dc.addVariableString("City", 23, "Stad");
    dc.addVariableString("State", 23, "Region");
    dc.addVariableString("ZIP", 11, "Postkod");
    dc.addVariableString("EMail", 64, "E-post");
    dc.addVariableString("Phone", 32, "Telefon");
    dc.addVariableString("Nationality", 3, "Nationalitet");
    dc.addVariableString("Country", 23, "Land");
    dc.addVariableString("Type", 20, "Typ");
    dc.addVariableInt("ExtId", oDataContainer::oIS64, "Externt Id");
    dc.addVariableString("Invoice", 1, "Faktura");
    dc.addVariableInt("InvoiceNo", oDataContainer::oIS16U, "Fakturanummer");
    dc.addVariableInt("StartGroup", oDataContainer::oIS32, "Startgrupp");
    initialized = true;
  }
  return dc;
}

oDataContainer& oClub::getDataBuffers(pvoid& data, pvoid& olddata,
                                       pvectorstr& strData) const {
  data = &dataMap_;
  olddata = &oldDataMap_;
  strData = nullptr;
  return container();
}

// -----------------------------------------------------------------------
// Construction / Destruction
// -----------------------------------------------------------------------

oClub::oClub(oEvent* poe) : oBase(poe) {
  getDI().initData();
  Id = oe->getFreeClubId();
}

oClub::oClub(oEvent* poe, int id) : oBase(poe) {
  getDI().initData();
  Id = id;
}

oClub::~oClub() = default;

// -----------------------------------------------------------------------
// Name management
// -----------------------------------------------------------------------

// Local helper: split `src` by `delim` into `out`.
static void splitWStr(const wstring& src, const wstring& delim,
                      vector<wstring>& out) {
  out.clear();
  size_t start = 0;
  while (true) {
    size_t pos = src.find(delim, start);
    if (pos == wstring::npos) {
      out.push_back(src.substr(start));
      break;
    }
    out.push_back(src.substr(start, pos - start));
    start = pos + delim.size();
  }
}

void oClub::internalSetName(const wstring& n) {
  name = n;

  // Pretty-name: replace "Skid o OK/OL" abbreviations
  tPrettyName.clear();
  const wchar_t* bf = name.c_str();
  int len = static_cast<int>(name.size());
  for (int k = 0; k <= len - 9; k++) {
    if (bf[k] == L'S') {
      if (wcscmp(bf + k, L"Skid o OK") == 0 || wcscmp(bf + k, L"Skid o OL") == 0) {
        tPrettyName = name;
        const wchar_t* rep = (wcscmp(bf + k, L"Skid o OK") == 0) ? L"SOK" : L"SOL";
        tPrettyName.replace(k, 9, rep, 3);
        break;
      }
    }
  }

  // Compact-name: check explicit ShortName override first
  wstring sn = getDCI().getString("ShortName");
  if (!sn.empty() && sn != n) {
    tCompactName = sn;
    return;
  }

  // Check manual map by original name
  {
    auto it = manualCompactNameMap.find(name);
    if (it != manualCompactNameMap.end()) {
      tCompactName = it->second;
      return;
    }
  }

  // Algorithm: skip short all-caps words and well-known generic words
  vector<wstring> words;
  splitWStr(getDisplayName(), L" ", words);
  int skipped = 0;
  bool properName = false;
  for (auto& w : words) {
    bool skip = false;
    if (!w.empty() && static_cast<int>(w.size()) <= 3) {
      skip = true;
      for (wchar_t c : w) {
        if (!(c >= L'A' && c <= L'Z')) { skip = false; break; }
      }
    } else if (_wcsicmp(w.c_str(), L"GOIF") == 0 ||
               _wcsicmp(w.c_str(), L"Orientering") == 0 ||
               _wcsicmp(w.c_str(), L"Orienteering") == 0 ||
               _wcsicmp(w.c_str(), L"Suunnistajat") == 0) {
      skip = true;
    } else {
      int pCount = 0;
      for (wchar_t c : w) {
        if (c > L'Z') {
          if (++pCount >= 2) { properName = true; break; }
        }
      }
    }
    if (skip) { w.clear(); skipped++; }
  }

  tCompactName.clear();
  if (skipped > 0 && properName) {
    for (auto& w : words) {
      if (!w.empty()) {
        if (tCompactName.empty()) tCompactName = w;
        else tCompactName += L" " + w;
      }
    }
    // Second pass through manual map with computed compact name
    auto it2 = manualCompactNameMap.find(tCompactName);
    if (it2 != manualCompactNameMap.end())
      tCompactName = it2->second;
  }
}

void oClub::setName(const wstring& n) {
  if (n != name) {
    internalSetName(n);
    updateChanged();
  }
}

// -----------------------------------------------------------------------
// Accessors
// -----------------------------------------------------------------------

bool oClub::sameClub(const oClub& c) const {
  return _wcsicmp(name.c_str(), c.name.c_str()) == 0;
}

bool oClub::operator<(const oClub& c) const {
  return name < c.name;
}

int oClub::getStartGroup() const {
  return getDCI().getInt("StartGroup");
}

void oClub::setStartGroup(int sg) {
  getDI().setInt("StartGroup", sg);
  updateChanged();
}

bool oClub::isVacant() const {
  return getId() == oe->getVacantClubIfExist(false);
}

void oClub::remove() {
  if (oe) oe->removeClub(Id);
  Removed = true;
}

bool oClub::canRemove() const {
  return !oe->isClubUsed(Id);
}

// -----------------------------------------------------------------------
// Invoice utilities
// -----------------------------------------------------------------------

void oClub::assignInvoiceNumber(oEvent& oe, bool reset) {
  // Build a sorted pointer vector for stable iteration order.
  std::vector<oClub*> sorted;
  for (auto& c : oe.Clubs) sorted.push_back(&c);
  std::sort(sorted.begin(), sorted.end(),
            [](const oClub* a, const oClub* b) { return *a < *b; });

  int numberStored = oe.getPropertyInt("FirstInvoice", 100);
  int number = numberStored;

  if (!reset) {
    int maxInvoice = 0;
    for (auto* c : sorted) {
      if (c->isRemoved()) continue;
      int no = c->getDCI().getInt("InvoiceNo");
      maxInvoice = std::max(maxInvoice, no);
    }
    if (maxInvoice != 0) number = maxInvoice + 1;
    else reset = true;
  }

  for (auto* c : sorted) {
    if (c->isRemoved()) continue;
    if (reset || c->getDCI().getInt("InvoiceNo") == 0) {
      c->getDI().setInt("InvoiceNo", number++);
    }
  }

  if (number > numberStored)
    oe.setProperty("FirstInvoice", number);
}

int oClub::getFirstInvoiceNumber(oEvent& oe) {
  int number = 0;
  for (auto& c : oe.Clubs) {
    if (c.isRemoved()) continue;
    int no = c.getDCI().getInt("InvoiceNo");
    if (no > 0) number = (number == 0) ? no : std::min(number, no);
  }
  return number;
}

wstring oClub::getInvoiceDate(oEvent& oe) {
  if (oe.eventInvoiceDate_.empty())
    oe.eventInvoiceDate_ = L"2026-01-01";
  return oe.eventInvoiceDate_;
}

void oClub::setInvoiceDate(oEvent& oe, const wstring& date) {
  oe.eventInvoiceDate_ = date;
}

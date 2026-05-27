#include "oBase.h"
#include "oDataContainer.h"
#include "oEvent.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

// -----------------------------------------------------------------------
// convertDynamicBase helpers
// -----------------------------------------------------------------------

static const wchar_t baseChars[] =
    L"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
    L"!#$%&()*+,-./:;<=>?@[]^_`{|}~";

void oBase::convertDynamicBase(int64_t val, int base, wchar_t out[16]) {
  if (base < 2 || base > 224) {
    out[0] = L'\0';
    return;
  }
  if (val == 0) {
    out[0] = baseChars[0];
    out[1] = L'\0';
    return;
  }
  bool negative = val < 0;
  uint64_t uval = negative ? static_cast<uint64_t>(-val) : static_cast<uint64_t>(val);
  wchar_t buf[16];
  int pos = 0;
  while (uval > 0 && pos < 15) {
    buf[pos++] = baseChars[uval % base];
    uval /= base;
  }
  int dest = 0;
  if (negative) out[dest++] = L'-';
  for (int i = pos - 1; i >= 0 && dest < 15; --i)
    out[dest++] = buf[i];
  out[dest] = L'\0';
}

int oBase::convertDynamicBase(const std::wstring& str, int64_t& out) {
  if (str.empty()) { out = 0; return 10; }
  // Determine base from the characters used
  int maxBase = 10;
  bool negative = str[0] == L'-';
  for (size_t i = negative ? 1 : 0; i < str.size(); ++i) {
    wchar_t c = str[i];
    const wchar_t* p = wcschr(baseChars, c);
    if (!p) { out = 0; return 10; }
    int charVal = static_cast<int>(p - baseChars);
    maxBase = std::max(maxBase, charVal + 1);
  }
  int64_t result = 0;
  for (size_t i = negative ? 1 : 0; i < str.size(); ++i) {
    wchar_t c = str[i];
    const wchar_t* p = wcschr(baseChars, c);
    int charVal = static_cast<int>(p - baseChars);
    result = result * maxBase + charVal;
  }
  out = negative ? -result : result;
  return maxBase;
}

// -----------------------------------------------------------------------
// oBase construction / destruction
// -----------------------------------------------------------------------

oBase::oBase(oEvent* poe)
    : Id(0), counter(0), oe(poe), Removed(false), correctionNeeded(true) {}

oBase::oBase(const oBase& in)
    : Id(in.Id),
      counter(in.counter),
      oe(in.oe),
      Removed(in.Removed),
      correctionNeeded(in.correctionNeeded),
      sqlUpdated(in.sqlUpdated),
      changed(false),
      transientChanged(false),
      localObject(in.localObject),
      implicitlyAdded(in.implicitlyAdded),
      addedToEvent(in.addedToEvent) {}

oBase::oBase(oBase&& in) noexcept
    : Id(in.Id),
      counter(in.counter),
      oe(in.oe),
      Removed(in.Removed),
      correctionNeeded(in.correctionNeeded),
      sqlUpdated(std::move(in.sqlUpdated)),
      changed(false),
      transientChanged(false),
      localObject(in.localObject),
      implicitlyAdded(in.implicitlyAdded),
      addedToEvent(in.addedToEvent) {
  if (in.myReference) {
    myReference.swap(in.myReference);
    myReference->ref = this;
  }
}

const oBase& oBase::operator=(const oBase& in) {
  if (this == &in) return *this;
  Id = in.Id;
  counter = in.counter;
  oe = in.oe;
  Removed = in.Removed;
  correctionNeeded = in.correctionNeeded;
  sqlUpdated = in.sqlUpdated;
  changed = false;
  transientChanged = false;
  localObject = in.localObject;
  implicitlyAdded = in.implicitlyAdded;
  addedToEvent = in.addedToEvent;
  return *this;
}

oBase::~oBase() {
  if (myReference)
    myReference->ref = nullptr;
}

// -----------------------------------------------------------------------
// State management
// -----------------------------------------------------------------------

void oBase::updateChanged(ChangeType ct) {
  if (ct == ChangeType::Update)
    changed = true;
  else
    transientChanged = true;
}

void oBase::makeQuietChangePermanent() {
  if (transientChanged)
    changed = true;
}

bool oBase::synchronize(bool writeOnly) {
  if (oe && (changed || transientChanged)) {
    changedObject();
    oe->dataRevision++;
  }
  transientChanged = false;
  if (changed && oe && !oe->hasPendingDBConnection)
    changed = false;
  return true;
}

void oBase::changeId(int newId) {
  Id = newId;
  if (oe) oe->updateFreeId(this);
}

void oBase::addToEvent(oEvent* e, const oBase* src) {
  oe = e;
  addedToEvent = true;
  localObject = false;
  if (oe) oe->updateFreeId(this);
}

void oBase::clearDuplicateBase(int newId) {
  Id = newId;
  sqlUpdated.clear();
  myReference.reset();
  counter = 0;
  Removed = false;
  implicitlyAdded = false;
  addedToEvent = false;
  changed = true;
  transientChanged = false;
  localObject = false;
}

// -----------------------------------------------------------------------
// Data interface
// -----------------------------------------------------------------------

oDataInterface oBase::getDI() {
  pvoid data, olddata;
  pvectorstr strData;
  oDataContainer& dc = getDataBuffers(data, olddata, strData);
  return dc.getInterface(data, getDISize(), this);
}

oDataConstInterface oBase::getDCI() const {
  pvoid data, olddata;
  pvectorstr strData;
  oDataContainer& dc = getDataBuffers(data, olddata, strData);
  return dc.getConstInterface(data, getDISize(), this);
}

// -----------------------------------------------------------------------
// External identifier
// -----------------------------------------------------------------------

void oBase::setExtIdentifier(int64_t id) {
  getDI().setInt64("ExtId", id);
}

int64_t oBase::getExtIdentifier() const {
  return getDCI().getInt64("ExtId");
}

std::wstring oBase::getExtIdentifierString() const {
  int64_t raw = getExtIdentifier();
  wchar_t res[16];
  if (raw == 0) return L"";
  if (raw & BaseGenStringFlag)
    convertDynamicBase(raw & ExtStringMask, 256 - 32, res);
  else if (raw & Base36StringFlag)
    convertDynamicBase(raw & ExtStringMask, 36, res);
  else
    convertDynamicBase(raw, 10, res);
  return res;
}

void oBase::converExtIdentifierString(int64_t raw, wchar_t bf[16]) {
  if (raw & BaseGenStringFlag)
    convertDynamicBase(raw & ExtStringMask, 256 - 32, bf);
  else if (raw & Base36StringFlag)
    convertDynamicBase(raw & ExtStringMask, 36, bf);
  else
    convertDynamicBase(raw, 10, bf);
}

int64_t oBase::converExtIdentifierString(const std::wstring& str) {
  int64_t val;
  int base = convertDynamicBase(str, val);
  if (base == 36) val |= Base36StringFlag;
  else if (base > 36) val |= BaseGenStringFlag;
  return val;
}

void oBase::setExtIdentifier(const std::wstring& str) {
  int64_t val = converExtIdentifierString(str);
  setExtIdentifier(val);
}

bool oBase::isStringIdentifier() const {
  int64_t raw = getExtIdentifier();
  return (raw & (BaseGenStringFlag | Base36StringFlag)) != 0;
}

int oBase::idFromExtId(int64_t val) {
  int basePart = static_cast<int>(val & 0x0FFFFFFF);
  if (basePart == val) return basePart;
  int64_t hash = static_cast<int64_t>((val & ExtStringMask) % 2000000011UL);
  int res = basePart + static_cast<int>(hash & 0xFFFFFF);
  if (res == 0) res += static_cast<int>(hash);
  return res & 0x0FFFFFFF;
}

// oBase.cpp — Implementation of oBase, the abstract domain entity base class.
#include "oBase.h"
#include "oDataContainer.h"
#include "oEvent.h"
#include "../util/meos_util.h"

// ── Construction / Destruction ───────────────────────────────────────────────

oBase::oBase(oEvent *poe) {
  Removed = false;
  oe = poe;
  Id = 0;
  changed = false;
  counter = 0;
  Modified.update();
  correctionNeeded = true;
  localObject = false;
  transientChanged = false;
  implicitlyAdded = false;
  addedToEvent = false;
}

oBase::oBase(const oBase &in) {
  Removed = in.Removed;
  oe = in.oe;
  Id = in.Id;
  changed = false;
  counter = in.counter;
  Modified.update();
  correctionNeeded = in.correctionNeeded;
  localObject = in.localObject;
  implicitlyAdded = in.implicitlyAdded;
  addedToEvent = in.addedToEvent;
  sqlUpdated = in.sqlUpdated;
  transientChanged = in.transientChanged;
}

oBase::oBase(oBase &&in) {
  Removed = in.Removed;
  oe = in.oe;
  Id = in.Id;
  changed = false;
  counter = in.counter;
  Modified.update();
  correctionNeeded = in.correctionNeeded;
  localObject = in.localObject;
  implicitlyAdded = in.implicitlyAdded;
  addedToEvent = in.addedToEvent;
  sqlUpdated = std::move(in.sqlUpdated);
  transientChanged = in.transientChanged;
  if (in.myReference) {
    myReference.swap(in.myReference);
    myReference->ref = this;
  }
}

const oBase &oBase::operator=(const oBase &in) {
  Removed = in.Removed;
  oe = in.oe;
  Id = in.Id;
  changed = false;
  counter = in.counter;
  Modified.update();
  correctionNeeded = in.correctionNeeded;
  localObject = in.localObject;
  implicitlyAdded = in.implicitlyAdded;
  addedToEvent = in.addedToEvent;
  sqlUpdated = in.sqlUpdated;
  transientChanged = in.transientChanged;
  return *this;
}

oBase::~oBase() {
  if (myReference)
    myReference->ref = nullptr;
}

void oBase::remove() {
  if (myReference)
    myReference->ref = nullptr;
}

// ── Synchronization ──────────────────────────────────────────────────────────

bool oBase::synchronize(bool writeOnly) {
  if (oe && (changed || transientChanged)) {
    changedObject();
    oe->dataRevision++;
  }
  transientChanged = false;
  if (oe && oe->hasDBConnection() && (changed || !writeOnly)) {
    correctionNeeded = false;
    if (localObject)
      return false;
    return oe->msSynchronize(this);
  } else {
    if (changed) {
      if (!oe->hasPendingDBConnection)
        changed = false;
    }
  }
  return true;
}

// ── External identifier (base-N encoding) ────────────────────────────────────

void oBase::setExtIdentifier(int64_t id) {
  getDI().setInt64("ExtId", id);
}

int64_t oBase::getExtIdentifier() const {
  return getDCI().getInt64("ExtId");
}

wstring oBase::getExtIdentifierString() const {
  int64_t raw = getExtIdentifier();
  wchar_t res[16];
  if (raw == 0)
    return L"";
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

int64_t oBase::converExtIdentifierString(const wstring &str) {
  long long val;
  int base = convertDynamicBase(str, val);
  if (base == 36)
    val |= Base36StringFlag;
  else if (base > 36)
    val |= BaseGenStringFlag;
  return val;
}

void oBase::setExtIdentifier(const wstring &str) {
  int64_t val = converExtIdentifierString(str);
  setExtIdentifier(val);
}

int oBase::idFromExtId(int64_t val) {
  int basePart = int(val & 0x0FFFFFFF);
  if (basePart == val)
    return basePart;

  int64_t hash = (val & ExtStringMask) % 2000000011ul;
  int res = basePart + int(hash & 0xFFFFFF);
  if (res == 0)
    res += int(hash);
  return res & 0x0FFFFFFF;
}

bool oBase::isStringIdentifier() const {
  int64_t raw = getExtIdentifier();
  return (raw & (BaseGenStringFlag | Base36StringFlag)) != 0;
}

// ── Timestamps ───────────────────────────────────────────────────────────────

wstring oBase::getTimeStamp() const {
  if (oe && oe->isClient() && !sqlUpdated.empty()) {
    wstring sqlW(sqlUpdated.begin(), sqlUpdated.end());
    return sqlW;
  }
  return Modified.getStampString();
}

string oBase::getTimeStampN() const {
  if (oe && oe->isClient() && !sqlUpdated.empty())
    return sqlUpdated;
  return Modified.getStampStringN();
}

const string &oBase::getStamp() const {
  if (oe && oe->isClient() && !sqlUpdated.empty())
    return Modified.getStamp(sqlUpdated);
  return Modified.getStamp();
}

// ── Id / event management ────────────────────────────────────────────────────

void oBase::changeId(int newId) {
  Id = newId;
  oe->updateFreeId(this);
}

void oBase::addToEvent(oEvent *e, const oBase *src) {
  oe = e;
  addedToEvent = true;
  localObject = false;
  oe->updateFreeId(this);
  if (src)
    Modified = src->Modified;
}

// ── Data interface ───────────────────────────────────────────────────────────

oDataInterface oBase::getDI() {
  pvoid data;
  pvoid olddata;
  pvectorstr strData;
  oDataContainer &dc = getDataBuffers(data, olddata, strData);
  return dc.getInterface(data, getDISize(), this);
}

oDataConstInterface oBase::getDCI() const {
  pvoid data;
  pvoid olddata;
  pvectorstr strData;
  oDataContainer &dc = getDataBuffers(data, olddata, strData);
  return dc.getConstInterface(data, getDISize(), this);
}

// ── Change tracking ──────────────────────────────────────────────────────────

void oBase::updateChanged(ChangeType ct) {
  Modified.update();
  if (ct == ChangeType::Update)
    changed = true;
  else
    transientChanged = true;
}

void oBase::makeQuietChangePermanent() {
  if (transientChanged)
    changed = true;
}

void oBase::update(SqlUpdated &info) const {
  info.updated = std::max(sqlUpdated, info.updated);
  info.counter = std::max(counter, info.counter);
}

void oBase::clearDuplicateBase(int newId) {
  Id = newId;
  Modified.update();
  sqlUpdated = "";
  myReference.reset();
  counter = 0;
  Removed = false;
  implicitlyAdded = false;
  addedToEvent = false;
  changed = true;
  transientChanged = false;
  localObject = false;
}

// ── DataRevisionCache template implementations ────────────────────────────────

template<typename T>
void DataRevisionCache<T>::update(const oEvent &oe, const T &value) const {
  data = value;
  revision = oe.dataRevision;
}

template<typename T>
void DataRevisionCache<T>::update(const oEvent &oe, T &&value) const {
  data = std::move(value);
  revision = oe.dataRevision;
}

template<typename T>
bool DataRevisionCache<T>::needsUpdate(const oEvent &oe) const {
  return revision != oe.dataRevision;
}

// Explicit instantiations for the types used in the codebase
template class DataRevisionCache<wstring>;
template class DataRevisionCache<int>;
template class DataRevisionCache<vector<pair<int,int>>>;

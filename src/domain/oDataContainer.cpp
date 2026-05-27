#include "oDataContainer.h"
#include "oBase.h"

#include <cstring>
#include <stdexcept>

// -----------------------------------------------------------------------
// DataMap helpers
// -----------------------------------------------------------------------

DataMap& oDataContainer::asMap(void* data) {
  return *static_cast<DataMap*>(data);
}

const DataMap& oDataContainer::asMap(const void* data) {
  return *static_cast<const DataMap*>(data);
}

const std::wstring& oDataContainer::emptyWString() {
  static const std::wstring empty;
  return empty;
}

// -----------------------------------------------------------------------
// Field registration
// -----------------------------------------------------------------------

oDataInfo& oDataContainer::addVariableInt(const char* name, int /*isize*/,
                                           const char* /*descr*/,
                                           const std::shared_ptr<oDataDefiner>& def) {
  oDataInfo& di = fields_[name];
  strncpy(di.Name, name, MaxVarNameLength - 1);
  di.Name[MaxVarNameLength - 1] = '\0';
  di.type = FieldDef::Type::Int;
  di.dataDefiner = def;
  return di;
}

oDataInfo& oDataContainer::addVariableInt64(const char* name, const char* /*descr*/,
                                             const std::shared_ptr<oDataDefiner>& def) {
  oDataInfo& di = fields_[name];
  strncpy(di.Name, name, MaxVarNameLength - 1);
  di.Name[MaxVarNameLength - 1] = '\0';
  di.type = FieldDef::Type::Int64;
  di.dataDefiner = def;
  return di;
}

oDataInfo& oDataContainer::addVariableDouble(const char* name, const char* /*descr*/,
                                              const std::shared_ptr<oDataDefiner>& def) {
  oDataInfo& di = fields_[name];
  strncpy(di.Name, name, MaxVarNameLength - 1);
  di.Name[MaxVarNameLength - 1] = '\0';
  di.type = FieldDef::Type::Double;
  di.dataDefiner = def;
  return di;
}

oDataInfo& oDataContainer::addVariableString(const char* name, int /*maxChar*/,
                                              const char* descr,
                                              const std::shared_ptr<oDataDefiner>& def) {
  return addVariableString(name, descr, def);
}

oDataInfo& oDataContainer::addVariableString(const char* name, const char* /*descr*/,
                                              const std::shared_ptr<oDataDefiner>& def) {
  oDataInfo& di = fields_[name];
  strncpy(di.Name, name, MaxVarNameLength - 1);
  di.Name[MaxVarNameLength - 1] = '\0';
  di.type = FieldDef::Type::String;
  di.dataDefiner = def;
  return di;
}

// -----------------------------------------------------------------------
// Type queries
// -----------------------------------------------------------------------

bool oDataContainer::isInt(const char* name) const {
  auto it = fields_.find(name);
  return it != fields_.end() &&
         (it->second.type == FieldDef::Type::Int ||
          it->second.type == FieldDef::Type::Int64);
}

bool oDataContainer::isString(const char* name) const {
  auto it = fields_.find(name);
  return it != fields_.end() && it->second.type == FieldDef::Type::String;
}

// -----------------------------------------------------------------------
// initData — ensure the DataMap contains default values for all fields
// -----------------------------------------------------------------------

void oDataContainer::initData(oBase* ob, int /*datasize*/) {
  pvoid data, olddata;
  pvectorstr strData;
  oDataContainer& dc = ob->getDataBuffers(data, olddata, strData);
  DataMap& dm = asMap(data);
  for (auto& [k, fi] : dc.fields_) {
    if (dm.find(k) == dm.end()) {
      switch (fi.type) {
        case FieldDef::Type::Int:    dm[k] = 0; break;
        case FieldDef::Type::Int64:  dm[k] = int64_t(0); break;
        case FieldDef::Type::Double: dm[k] = 0.0; break;
        case FieldDef::Type::String: dm[k] = std::wstring{}; break;
      }
    }
  }
}

// -----------------------------------------------------------------------
// Setters
// -----------------------------------------------------------------------

bool oDataContainer::setInt(oBase* ob, void* data, const char* name, int v) {
  DataMap& dm = asMap(data);
  auto it = dm.find(name);
  if (it != dm.end() && std::holds_alternative<int>(it->second) &&
      std::get<int>(it->second) == v)
    return false;
  dm[name] = v;
  return true;
}

bool oDataContainer::setInt64(void* data, const char* name, __int64 v) {
  DataMap& dm = asMap(data);
  auto it = dm.find(name);
  if (it != dm.end() && std::holds_alternative<int64_t>(it->second) &&
      std::get<int64_t>(it->second) == static_cast<int64_t>(v))
    return false;
  dm[name] = static_cast<int64_t>(v);
  return true;
}

bool oDataContainer::setDouble(oBase* ob, void* data, const char* name, double v) {
  DataMap& dm = asMap(data);
  auto it = dm.find(name);
  if (it != dm.end() && std::holds_alternative<double>(it->second) &&
      std::get<double>(it->second) == v)
    return false;
  dm[name] = v;
  return true;
}

bool oDataContainer::setString(oBase* ob, const char* name,
                                const std::wstring& v) {
  // In the new implementation, all data (including strings) lives in the DataMap
  // returned by oBase::getDataBuffers.
  pvoid data, olddata;
  pvectorstr strData;
  ob->getDataBuffers(data, olddata, strData);
  DataMap& dm = asMap(data);
  auto it = dm.find(name);
  if (it != dm.end() && std::holds_alternative<std::wstring>(it->second) &&
      std::get<std::wstring>(it->second) == v)
    return false;
  dm[name] = v;
  return true;
}

// -----------------------------------------------------------------------
// Getters
// -----------------------------------------------------------------------

int oDataContainer::getInt(const void* data, const char* name) const {
  const DataMap& dm = asMap(data);
  auto it = dm.find(name);
  if (it == dm.end()) return 0;
  if (std::holds_alternative<int>(it->second)) return std::get<int>(it->second);
  if (std::holds_alternative<int64_t>(it->second))
    return static_cast<int>(std::get<int64_t>(it->second));
  return 0;
}

__int64 oDataContainer::getInt64(const void* data, const char* name) const {
  const DataMap& dm = asMap(data);
  auto it = dm.find(name);
  if (it == dm.end()) return 0;
  if (std::holds_alternative<int64_t>(it->second))
    return std::get<int64_t>(it->second);
  if (std::holds_alternative<int>(it->second))
    return static_cast<int64_t>(std::get<int>(it->second));
  return 0;
}

double oDataContainer::getDouble(const void* data, const char* name) const {
  const DataMap& dm = asMap(data);
  auto it = dm.find(name);
  if (it == dm.end()) return 0.0;
  if (std::holds_alternative<double>(it->second)) return std::get<double>(it->second);
  if (std::holds_alternative<int>(it->second))
    return static_cast<double>(std::get<int>(it->second));
  return 0.0;
}

const std::wstring& oDataContainer::getString(const oBase* ob, const char* name) const {
  pvoid data, olddata;
  pvectorstr strData;
  oDataContainer& dc = ob->getDataBuffers(data, olddata, strData);
  const DataMap& dm = asMap(data);
  auto it = dm.find(name);
  if (it == dm.end()) return emptyWString();
  if (std::holds_alternative<std::wstring>(it->second))
    return std::get<std::wstring>(it->second);
  return emptyWString();
}

const std::wstring& oDataContainer::getDate(const void* data, const char* name) const {
  const DataMap& dm = asMap(data);
  auto it = dm.find(name);
  if (it == dm.end()) return emptyWString();
  if (std::holds_alternative<std::wstring>(it->second))
    return std::get<std::wstring>(it->second);
  return emptyWString();
}

int oDataContainer::getYear(const void* data, const char* name) const {
  // Date stored as wstring "YYYY-MM-DD"; extract the year component
  const DataMap& dm = asMap(data);
  auto it = dm.find(name);
  if (it == dm.end()) return 0;
  if (std::holds_alternative<std::wstring>(it->second)) {
    const std::wstring& d = std::get<std::wstring>(it->second);
    if (d.size() >= 4) {
      try { return std::stoi(d.substr(0, 4)); } catch (...) {}
    }
  }
  return 0;
}

// -----------------------------------------------------------------------
// Interface factories
// -----------------------------------------------------------------------

oDataInterface oDataContainer::getInterface(void* data, int datasize, oBase* ob) {
  return oDataInterface(this, data, ob);
}

oDataConstInterface oDataContainer::getConstInterface(const void* data, int datasize,
                                                       const oBase* ob) const {
  return oDataConstInterface(this, data, ob);
}

// -----------------------------------------------------------------------
// Merge (no-op stub — full implementation in downstream stories)
// -----------------------------------------------------------------------

bool oDataContainer::merge(oBase& destination, const oBase& source,
                            const oBase* base) const {
  return false;
}

// -----------------------------------------------------------------------
// oDataInterface setters (with change notification)
// -----------------------------------------------------------------------

bool oDataInterface::setInt(const char* name, int value) {
  if (dc_->setInt(ob_, data_, name, value)) {
    ob_->updateChanged();
    return true;
  }
  return false;
}

bool oDataInterface::setInt64(const char* name, __int64 value) {
  if (dc_->setInt64(data_, name, value)) {
    ob_->updateChanged();
    return true;
  }
  return false;
}

bool oDataInterface::setDouble(const char* name, double value) {
  if (dc_->setDouble(ob_, data_, name, value)) {
    ob_->updateChanged();
    return true;
  }
  return false;
}

bool oDataInterface::setString(const char* name, const std::wstring& value) {
  // Strings live in the DataMap pointed to by data_
  DataMap& dm = *static_cast<DataMap*>(data_);
  auto it = dm.find(name);
  if (it != dm.end() && std::holds_alternative<std::wstring>(it->second) &&
      std::get<std::wstring>(it->second) == value)
    return false;
  dm[name] = value;
  ob_->updateChanged();
  return true;
}

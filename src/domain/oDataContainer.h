#pragma once

#include "domain_header.h"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

class oBase;
class oDataInterface;
class oDataConstInterface;

// The data storage type used by each oBase subclass.
// pvoid data / pvoid olddata pointers in getDataBuffers() point to DataMap instances.
using DataValue = std::variant<int, int64_t, double, std::wstring>;
using DataMap = std::unordered_map<std::string, DataValue>;

// Describes a registered field.
struct FieldDef {
  enum class Type { Int, Int64, Double, String };
  std::string name;
  Type type = Type::Int;
};

// Maximum length for variable names (matches legacy constant)
constexpr int MaxVarNameLength = 28;

// Abstract interface for defining and formatting custom data fields.
// Concrete implementations live in datadefiners.h.
class oDataDefiner {
public:
  virtual ~oDataDefiner() = default;
  virtual const std::wstring& formatData(const oBase* obj, int index) const = 0;
  virtual std::pair<int, bool> setData(oBase* obj, int index,
                                        const std::wstring& input,
                                        std::wstring& output,
                                        int inputId) const = 0;
};

// Listener notified when a data field changes.
class oDataNotifier {
public:
  virtual ~oDataNotifier() = default;
  virtual void notify(oBase* ob, int oldValue, int newValue) {}
  virtual void notify(oBase* ob, const std::wstring& newValue) {}
};

// Metadata for a single registered field.
struct oDataInfo {
  char Name[MaxVarNameLength] = {};
  FieldDef::Type type = FieldDef::Type::Int;
  std::shared_ptr<oDataDefiner> dataDefiner;
  std::shared_ptr<oDataNotifier> dataNotifier;
};

class oDataContainer {
  std::unordered_map<std::string, oDataInfo> fields_;

public:
  explicit oDataContainer(int /*maxsize*/) {}
  virtual ~oDataContainer() = default;

  // Field registration
  oDataInfo& addVariableInt(const char* name, int /*isize*/, const char* /*descr*/,
                             const std::shared_ptr<oDataDefiner>& def = nullptr);
  oDataInfo& addVariableInt64(const char* name, const char* /*descr*/,
                               const std::shared_ptr<oDataDefiner>& def = nullptr);
  oDataInfo& addVariableDouble(const char* name, const char* /*descr*/,
                                const std::shared_ptr<oDataDefiner>& def = nullptr);
  oDataInfo& addVariableString(const char* name, int /*maxChar*/, const char* /*descr*/,
                                const std::shared_ptr<oDataDefiner>& def = nullptr);
  oDataInfo& addVariableString(const char* name, const char* /*descr*/,
                                const std::shared_ptr<oDataDefiner>& def = nullptr);

  // Helpers that match legacy oIntSize enum values used in addVariableInt calls
  enum oIntSize {
    oISDecimal = 28, oISTime = 29, oISTimeAdjust = 26, oISCurrency = 30,
    oISDate = 31, oISDateOrYear = 27, oIS64 = 64,
    oIS32 = 32, oIS16 = 16, oIS8 = 8, oIS16U = 17, oIS8U = 9
  };

  // Type queries
  bool isInt(const char* name) const;
  bool isString(const char* name) const;

  // Setters — ob may be nullptr if no change notification is needed
  bool setInt(oBase* ob, void* data, const char* name, int v);
  bool setInt64(void* data, const char* name, __int64 v);
  bool setDouble(oBase* ob, void* data, const char* name, double v);
  bool setString(oBase* ob, const char* name, const std::wstring& v);

  // Getters
  int getInt(const void* data, const char* name) const;
  __int64 getInt64(const void* data, const char* name) const;
  double getDouble(const void* data, const char* name) const;
  const std::wstring& getString(const oBase* ob, const char* name) const;
  const std::wstring& getDate(const void* data, const char* name) const;
  int getYear(const void* data, const char* name) const;

  // Initialize a DataMap for a new oBase instance
  void initData(oBase* ob, int /*datasize*/);

  // Build the mutable accessor interface
  oDataInterface getInterface(void* data, int datasize, oBase* ob);
  oDataConstInterface getConstInterface(const void* data, int datasize,
                                         const oBase* ob) const;

  // Merge src into dest (used when downloading updated data from server)
  bool merge(oBase& destination, const oBase& source, const oBase* base) const;

  friend class oDataInterface;
  friend class oDataConstInterface;

private:
  static DataMap& asMap(void* data);
  static const DataMap& asMap(const void* data);
  static const std::wstring& emptyWString();
};


class oDataInterface {
  void* data_;
  oDataContainer* dc_;
  oBase* ob_;

public:
  oDataInterface(oDataContainer* dc, void* data, oBase* ob)
      : data_(data), dc_(dc), ob_(ob) {}

  bool setInt(const char* name, int value);
  bool setInt(const std::string& name, int value) { return setInt(name.c_str(), value); }
  bool setInt64(const char* name, __int64 value);
  bool setDouble(const char* name, double value);
  bool setDouble(const std::string& name, double value) { return setDouble(name.c_str(), value); }
  bool setString(const char* name, const std::wstring& value);
  bool setString(const std::string& name, const std::wstring& value) {
    return setString(name.c_str(), value);
  }
  bool setDate(const char* name, const std::wstring& value) {
    return setString(name, value);
  }

  int getInt(const char* name) const { return dc_->getInt(data_, name); }
  int getInt(const std::string& name) const { return getInt(name.c_str()); }
  __int64 getInt64(const char* name) const { return dc_->getInt64(data_, name); }
  double getDouble(const char* name) const { return dc_->getDouble(data_, name); }
  double getDouble(const std::string& name) const { return getDouble(name.c_str()); }
  const std::wstring& getString(const char* name) const { return dc_->getString(ob_, name); }
  const std::wstring& getString(const std::string& name) const {
    return getString(name.c_str());
  }
  const std::wstring& getDate(const char* name) const { return dc_->getDate(data_, name); }
  int getYear(const char* name) const { return dc_->getYear(data_, name); }

  bool isInt(const std::string& name) const { return dc_->isInt(name.c_str()); }
  bool isString(const std::string& name) const { return dc_->isString(name.c_str()); }

  void initData() { dc_->initData(ob_, 0); }
};


class oDataConstInterface {
  const void* data_;
  const oDataContainer* dc_;
  const oBase* ob_;

public:
  oDataConstInterface(const oDataContainer* dc, const void* data, const oBase* ob)
      : data_(data), dc_(dc), ob_(ob) {}

  int getInt(const char* name) const { return dc_->getInt(data_, name); }
  int getInt(const std::string& name) const { return getInt(name.c_str()); }
  __int64 getInt64(const char* name) const { return dc_->getInt64(data_, name); }
  double getDouble(const char* name) const { return dc_->getDouble(data_, name); }
  double getDouble(const std::string& name) const { return getDouble(name.c_str()); }
  const std::wstring& getString(const char* name) const { return dc_->getString(ob_, name); }
  const std::wstring& getString(const std::string& name) const {
    return getString(name.c_str());
  }
  const std::wstring& getDate(const char* name) const { return dc_->getDate(data_, name); }
  int getYear(const char* name) const { return dc_->getYear(data_, name); }

  bool isInt(const std::string& name) const { return dc_->isInt(name.c_str()); }
  bool isString(const std::string& name) const { return dc_->isString(name.c_str()); }
};

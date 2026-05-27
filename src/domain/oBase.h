#pragma once

#include "domain_header.h"
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class oEvent;
class oDataInterface;
class oDataConstInterface;
class oDataContainer;

using pvoid = void*;
using pvectorstr = std::vector<std::vector<std::wstring>>*;

class oBase {
public:
  class oBaseReference {
    oBase* ref = nullptr;

  public:
    oBase* get() { return ref; }
    friend class oBase;
  };

  enum class ChangeType { Quiet, Update };

protected:
  int Id = 0;
  int counter = 0;
  oEvent* oe = nullptr;
  bool Removed = false;
  bool correctionNeeded = false;
  std::string sqlUpdated;

private:
  std::shared_ptr<oBaseReference> myReference;
  bool changed = false;
  bool transientChanged = false;
  bool localObject = false;
  bool implicitlyAdded = false;
  bool addedToEvent = false;

protected:
  virtual void changedObject() = 0;
  virtual void changeId(int newId);

  // Each subclass returns pointers to its own DataMap storage.
  // pvoid data / pvoid olddata are pointers to a DataMap (oDataContainer.h).
  virtual oDataContainer& getDataBuffers(pvoid& data, pvoid& olddata,
                                          pvectorstr& strData) const = 0;
  virtual int getDISize() const = 0;
  virtual void merge(const oBase& input, const oBase* base) = 0;

  void setLocalObject() { localObject = true; }
  void clearDuplicateBase(int newId);

public:
  void updateChanged(ChangeType ct = ChangeType::Update);
  void makeQuietChangePermanent();

  const std::shared_ptr<oBaseReference>& getReference() {
    if (!myReference) {
      myReference = std::make_shared<oBaseReference>();
      myReference->ref = this;
    }
    return myReference;
  }

  bool isLocalObject() const { return localObject; }

  virtual std::wstring getInfo() const = 0;

  virtual std::pair<int, bool> inputData(int id, const std::wstring& input,
                                          int inputId, std::wstring& output,
                                          bool noUpdate) {
    output = L"";
    return {0, false};
  }

  oEvent* getEvent() const { return oe; }
  int getId() const { return Id; }
  bool isChanged() const { return changed; }
  bool isRemoved() const { return Removed; }

  bool synchronize(bool writeOnly = false);
  bool existInDB() const { return !sqlUpdated.empty(); }

  void setImplicitlyCreated() { implicitlyAdded = true; }
  bool isImplicitlyCreated() const { return implicitlyAdded; }
  bool isAddedToEvent() const { return addedToEvent; }
  void addToEvent(oEvent* e, const oBase* src);

  oDataInterface getDI();
  oDataConstInterface getDCI() const;

  virtual void remove() = 0;
  virtual bool canRemove() const = 0;

  void setExtIdentifier(int64_t id);
  int64_t getExtIdentifier() const;
  std::wstring getExtIdentifierString() const;
  void setExtIdentifier(const std::wstring& str);
  bool isStringIdentifier() const;

  static int idFromExtId(int64_t extId);
  static void converExtIdentifierString(int64_t raw, wchar_t bf[16]);
  static int64_t converExtIdentifierString(const std::wstring& str);

  explicit oBase(oEvent* poe);
  oBase(const oBase& in);
  oBase(oBase&& in) noexcept;
  const oBase& operator=(const oBase& in);
  virtual ~oBase();

  friend class oDataInterface;
  friend class oDataContainer;

protected:
  static const uint64_t BaseGenStringFlag = 1ull << 63;
  static const uint64_t Base36StringFlag = 1ull << 62;
  static const uint64_t ExtStringMask = ~(BaseGenStringFlag | Base36StringFlag);

  static void convertDynamicBase(int64_t val, int base, wchar_t out[16]);
  static int convertDynamicBase(const std::wstring& str, int64_t& out);
};

using pBase = oBase*;

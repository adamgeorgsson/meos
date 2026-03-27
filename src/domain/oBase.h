// oBase.h — Abstract base class for all domain entities.
#pragma once

#include "../util/TimeStamp.h"
#include "domain_header.h"
#include <vector>
#include <memory>

class oEvent;
class oDataInterface;
class oDataConstInterface;
class oDataContainer;
typedef void * pvoid;
typedef vector<vector<wstring>> * pvectorstr;
struct SqlUpdated;

class oBase {
public:
  class oBaseReference {
  private:
    oBase * ref = nullptr;
  public:
    oBase * get() { return ref; }
    friend class oBase;
  };

  /** Indicate if a change is transient (quiet) or should be written to database. */
  enum class ChangeType {
    Quiet,
    Update
  };
protected:
  int Id;
  TimeStamp Modified;
  string sqlUpdated; // SQL TIMESTAMP

  const static unsigned long long BaseGenStringFlag = 1ull << 63;
  const static unsigned long long Base36StringFlag  = 1ull << 62;
  const static unsigned long long ExtStringMask     = ~(BaseGenStringFlag | Base36StringFlag);

private:
  shared_ptr<oBaseReference> myReference;

protected:
  int counter;
  oEvent *oe;
  bool Removed;

  // True if the object is incorrect and needs correction
  bool correctionNeeded = false;

private:
  bool implicitlyAdded = false;
  bool addedToEvent = false;
  // Changed in client, not yet sent to server
  bool changed;
  // Changed in client, silent mode, should not be sent to server
  bool transientChanged;
  bool localObject;

protected:
  virtual void changedObject() = 0;
  virtual void changeId(int newId);
  virtual oDataContainer &getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const = 0;
  virtual int getDISize() const = 0;

  void setLocalObject() { localObject = true; }
  virtual void merge(const oBase &input, const oBase *base) = 0;
  void clearDuplicateBase(int newId);

public:
  void updateChanged(ChangeType ct = ChangeType::Update);
  void update(SqlUpdated &info) const;

  const shared_ptr<oBaseReference> &getReference() {
    if (!myReference) {
      myReference = make_shared<oBaseReference>();
      myReference->ref = this;
    }
    return myReference;
  }

  bool isLocalObject() { return localObject; }
  virtual wstring getInfo() const = 0;

  virtual pair<int, bool> inputData(int id, const wstring &input, int inputId,
                                    wstring &output, bool noUpdate) {
    output = L""; return make_pair(0, false);
  }

  virtual void fillInput(int id, vector<pair<wstring, size_t>> &elements, size_t &selected) {
    throw std::runtime_error("Not implemented");
  }

  oEvent *getEvent() const { return oe; }
  int getId() const { return Id; }
  bool isChanged() const { return changed; }
  bool isRemoved() const { return Removed; }
  int getAge() const { return Modified.getAge(); }
  unsigned int getModificationTime() const { return Modified.getModificationTime(); }
  void makeQuietChangePermanent();

  bool synchronize(bool writeOnly = false);
  wstring getTimeStamp() const;
  string getTimeStampN() const;
  const string &getStamp() const;
  const TimeStamp& getModified() const { return Modified; }

  bool existInDB() const { return !sqlUpdated.empty(); }

  void setImplicitlyCreated() { implicitlyAdded = true; }
  bool isImplicitlyCreated() const { return implicitlyAdded; }
  bool isAddedToEvent() const { return addedToEvent; }
  void addToEvent(oEvent *e, const oBase *src);

  oDataInterface getDI();
  oDataConstInterface getDCI() const;

  virtual void remove() = 0;
  virtual bool canRemove() const = 0;

  void setExtIdentifier(int64_t id);
  int64_t getExtIdentifier() const;
  wstring getExtIdentifierString() const;
  void setExtIdentifier(const wstring &str);
  bool isStringIdentifier() const;

  static int idFromExtId(int64_t extId);
  static void converExtIdentifierString(int64_t raw, wchar_t bf[16]);
  static int64_t converExtIdentifierString(const wstring &str);

  oBase(oEvent *poe);
  oBase(const oBase &in);
  oBase(oBase &&in);
  const oBase &operator=(const oBase &in);
  virtual ~oBase();

  friend class oEvent;
  friend class oDataInterface;
  friend class oDataContainer;
};

typedef oBase * pBase;

// ── DataRevisionCache ────────────────────────────────────────────────────────
// Caches a computed value keyed to oEvent::dataRevision so callers can detect
// when recomputation is needed without comparing against previous results.
template<typename T>
class DataRevisionCache {
  mutable T data;
  mutable unsigned long revision = static_cast<unsigned long>(-1);
public:
  void update(const oEvent &oe, const T &value) const;
  void update(const oEvent &oe, T &&value) const;
  bool needsUpdate(const oEvent &oe) const;
  const T &get() const { return data; }
};

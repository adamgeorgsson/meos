#pragma once

#include <limits>
#include <algorithm>

#include "oBase.h"
#include "oDataContainer.h"
#include "oPunch.h"

using SpecialPunch = oPunch::SpecialPunch;

class oControl;
class oCourse;
class oClass;
typedef oCourse* pCourse;
typedef oClass* pClass;

class oControl : public oBase {
public:
  enum class ControlStatus {
    StatusOK = 0, StatusBad = 1, StatusMultiple = 2,
    StatusStart = 4, StatusFinish = 5, StatusRogaining = 6,
    StatusNoTiming = 7, StatusOptional = 8,
    StatusBadNoTiming = 9, StatusRogainingRequired = 10, StatusCheck = 11
  };

  bool operator<(const oControl& b) const { return minNumber() < b.minNumber(); }

  static bool isSpecialControl(ControlStatus st) {
    return st == ControlStatus::StatusFinish ||
           st == ControlStatus::StatusStart ||
           st == ControlStatus::StatusCheck;
  }

  /** Returns number of duplicates of this control in any course. */
  int getNumberDuplicates() const { return tNumberDuplicates; }

  static pair<int, int> getIdIndexFromCourseControlId(int courseControlId);
  static int getCourseControlIdFromIdIndex(int controlId, int index);

protected:
  int nNumbers = 0;
  int Numbers[32] = {};
  bool checkedNumbers[32] = {};

  ControlStatus Status = ControlStatus::StatusOK;
  wstring Name;
  bool decodeNumbers(string s);

  mutable DataMap dataMap_;
  mutable DataMap oldDataMap_;

  static oDataContainer& container();

  oDataContainer& getDataBuffers(pvoid& data, pvoid& olddata,
                                  pvectorstr& strData) const override;
  int getDISize() const final { return 0; }

  struct TCache {
    TCache() : minTime(0), timeAdjust(0), dataRevision(-1) {}
    int minTime;
    int timeAdjust;
    int dataRevision;
  };

  bool tHasFreePunchLabel = false;
  mutable int tNumberDuplicates = 0;
  mutable TCache tCache;
  void setupCache() const;

  mutable int tStatDataRevision = -1;
  mutable int tMissedTimeTotal = 0;
  mutable int tMissedTimeMax = 0;
  mutable int tMistakeQuotient = 0;
  mutable int tNumVisitorsActual = 0;
  mutable int tNumVisitorsExpected = 0;
  mutable int tNumRunnersRemaining = 0;
  mutable int tMissedTimeMedian = 0;

  void changedObject() override {}

public:
  void clearCache() { tCache.dataRevision = -1; }

  bool isValidRadio() const;
  void setRadio(bool r);

  void remove() override { Removed = true; }
  bool canRemove() const override { return !isChanged(); }

  wstring getInfo() const override { return getName(); }

  bool isSingleStatusOK() const {
    return Status == ControlStatus::StatusOK ||
           Status == ControlStatus::StatusNoTiming;
  }

  int getMissedTimeTotal() const;
  int getMissedTimeMax() const;
  int getMistakeQuotient() const;
  int getMissedTimeMedian() const;
  int getNumVisitors(bool actualVisits) const;
  int getNumRunnersRemaining() const;

  int getNNumbers() const { return nNumbers; }

  inline int minNumber() const {
    int m = std::numeric_limits<int>::max();
    for (int k = 0; k < nNumbers; k++)
      m = std::min(Numbers[k], m);
    return m;
  }

  inline int maxNumber() const {
    int m = 0;
    for (int k = 0; k < nNumbers; k++)
      m = std::max(Numbers[k], m);
    return m;
  }

  void addUncheckedPunches(vector<pair<int, oControl*>>& mp, bool supportRogaining) const;
  void startCheckControl();
  int getMissingNumber() const;
  bool controlCompleted(bool supportRogaining) const;

  wstring codeNumbers(char sep = ';') const;
  bool setNumbers(const wstring& numbers);

  ControlStatus getStatus() const { return Status; }
  const wstring getStatusS() const;

  bool hasName() const { return !Name.empty() && Name != getDefaultName(); }
  const wstring& getName() const;
  const wstring& getDefaultName() const;
  wstring getIdS() const;

  bool isRogaining(bool useRogaining) const {
    return useRogaining &&
           (Status == ControlStatus::StatusRogaining ||
            Status == ControlStatus::StatusRogainingRequired);
  }

  void setStatus(ControlStatus st);
  void setName(const wstring& name);

  bool hasNumber(int i);
  bool hasNumberUnchecked(int i);
  bool uncheckNumber(int i);

  wstring getString();
  wstring getLongString();

  int getNumMulti();

  int getTimeAdjust() const;
  wstring getTimeAdjustS() const;
  bool setTimeAdjust(int v);
  bool setTimeAdjust(const wstring& t);

  int getMinTime() const;
  wstring getMinTimeS() const;
  void setMinTime(int v);
  void setMinTime(const wstring& t);

  int getRogainingPoints() const;
  wstring getRogainingPointsS() const;
  void setRogainingPoints(int v);

  int getFirstNumber() const;
  void getNumbers(vector<int>& numbers) const;

  void merge(const oBase& input, const oBase* base) final {}

  void set(int pId, int pNumber, wstring pName);

  oControl(oEvent* poe);
  oControl(oEvent* poe, int id);
  virtual ~oControl();

  friend class oRunner;
  friend class oCourse;
  friend class oEvent;
  friend class oClass;
};

using pControl = oControl*;

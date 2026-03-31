// oControl.h — Migrated from legacy code/oControl.h (US-003b).
// Cross-platform, no Win32 dependencies.
#pragma once

#include <limits>
#include <algorithm>

#include "oBase.h"
#include "oPunch.h"

class oControl;
class xmlparser;
class xmlobject;

typedef oControl* pControl;
class oDataInterface;
class oDataConstInterface;
class Table;

class oCourse;
class oClass;
typedef oCourse* pCourse;
typedef oClass* pClass;

class oControl : public oBase
{
public:
  /** Returns the number of duplicates of this control in any course. Valid after a call to
      oEvent::getControls with the calculate flags set to true. */
  int getNumberDuplicates() const;

  void getCourseControls(vector<int>& cc) const;

  static pair<int, int> getIdIndexFromCourseControlId(int courseControlId);
  static int getCourseControlIdFromIdIndex(int controlId, int index);

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

protected:
  int nNumbers;
  int Numbers[32];
  bool checkedNumbers[32];

  ControlStatus Status;
  wstring Name;
  bool decodeNumbers(string s);

  static const int dataSize = 64;
  int getDISize() const final { return dataSize; }
  alignas(sizeof(wchar_t)) BYTE oData[dataSize];
  alignas(sizeof(wchar_t)) BYTE oDataOld[dataSize];

  /// Table methods (no-op stubs — GUI coupling removed)
  void addTableRow(Table& table) const;
  pair<int, bool> inputData(int id, const wstring& input,
                             int inputId, wstring& output, bool noUpdate) override;

  /// Table methods (no-op stub)
  void fillInput(int id, vector<pair<wstring, size_t>>& elements, size_t& selected) override;

  /** Get internal data buffers for DI */
  oDataContainer& getDataBuffers(pvoid& data, pvoid& olddata, pvectorstr& strData) const;

  struct TCache {
    TCache() : minTime(0), timeAdjust(0), dataRevision(-1) {}
    int minTime;
    int timeAdjust;
    int dataRevision;
  };

  // Is set to true if there is a free punch tied to the control
  bool tHasFreePunchLabel;
  mutable int tNumberDuplicates;
  mutable TCache tCache;
  void setupCache() const;

  mutable int tStatDataRevision;

  mutable int tMissedTimeTotal;
  mutable int tMissedTimeMax;
  mutable int tMistakeQuotient;
  mutable int tNumVisitorsActual;    // Count actual punches
  mutable int tNumVisitorsExpected;  // Count expected visitors
  mutable int tNumRunnersRemaining;  // Number of remaining runners expected

  mutable int tMissedTimeMedian;

  void changedObject();

public:
  // Public oData accessors for repository persistence
  const BYTE* getOData() const { return oData; }
  BYTE* getOData() { return oData; }
  static int getODataBlobSize() { return dataSize; }

  void clearCache();

  static int getControlIdByName(const oEvent& oe, const string& name);

  // Returns true if control is considered a radio control.
  bool isValidRadio() const;
  // Specify true to mark the control as a radio control, otherwise no radio
  void setRadio(bool r);

  void remove();
  bool canRemove() const;

  wstring getInfo() const;

  bool isSingleStatusOK() const {
    return Status == ControlStatus::StatusOK || Status == ControlStatus::StatusNoTiming;
  }

  int getMissedTimeTotal() const;
  int getMissedTimeMax() const;
  int getMistakeQuotient() const;
  int getMissedTimeMedian() const;
  int getNumVisitors(bool actualVisits) const;
  int getNumRunnersRemaining() const;

  void getCourses(vector<pCourse>& crs) const;
  void getClasses(vector<pClass>& cls) const;

  /** Return number of alternative punch codes */
  int getNNumbers() const { return nNumbers; }

  inline int minNumber() const {
    int m = std::numeric_limits<int>::max();
    for (int k = 0; k < nNumbers; k++)
      m = min(Numbers[k], m);
    return m;
  }

  inline int maxNumber() const {
    int m = 0;
    for (int k = 0; k < nNumbers; k++)
      m = max(Numbers[k], m);
    return m;
  }

  // Add unchecked controls to the list
  void addUncheckedPunches(vector<pair<int, pControl>>& mp, bool supportRogaining) const;
  // Start checking if all punches needed for this control exist
  void startCheckControl();
  // Get the number of a missing punch
  int getMissingNumber() const;
  /** Returns true if the check of this control is completed
   @param supportRogaining true if rogaining controls are supported
  */
  bool controlCompleted(bool supportRogaining) const;

  wstring codeNumbers(char sep = ';') const;
  bool setNumbers(const wstring& numbers);

  ControlStatus getStatus() const { return Status; }
  const wstring getStatusS() const;

  bool hasName() const { return !Name.empty() && Name != getDefaultName(); }
  /// Get name or [id]
  const wstring& getName() const;
  /// Get name (default form) = "[id]" if name is empty
  const wstring& getDefaultName() const;

  /// Get name or numeric id as string
  wstring getIdS() const;

  bool isRogaining(bool useRogaining) const {
    return useRogaining &&
           (Status == ControlStatus::StatusRogaining ||
            Status == ControlStatus::StatusRogainingRequired);
  }

  void setStatus(ControlStatus st);
  void setName(const wstring& name);

  bool isUnit() const;
  int getUnitCode() const;
  oPunch::SpecialPunch getUnitType() const;

  // Returns true if control has number and checks it.
  bool hasNumber(int i);
  // Return true if it has number i and it is unchecked. Checks the number.
  bool hasNumberUnchecked(int i);
  // Uncheck a given number
  bool uncheckNumber(int i);

  wstring getString();
  wstring getLongString();

  // For a control that requires several punches, return the number of required punches.
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
  void setRogainingPoints(const wstring& t);

  /// Return first code number (or zero)
  int getFirstNumber() const;
  void getNumbers(vector<int>& numbers) const;

  void merge(const oBase& input, const oBase* base) final;

  void set(const xmlobject* xo);
  void set(int pId, int pNumber, wstring pName);
  bool write(xmlparser& xml);
  oControl(oEvent* poe);
  oControl(oEvent* poe, int id);

  virtual ~oControl();

  friend class oRunner;
  friend class oCourse;
  friend class oEvent;
  friend class oClass;
  friend class MeosSQL;
};

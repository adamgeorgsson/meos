#pragma once

#include "oBase.h"
#include "oClub.h"
#include "oClass.h"

// Forward declaration for unit test access (see tests/domain_orunner_result_test.cpp).
class RunnerResultTestAccessor;
#include "domain_header.h"
#include "common_enums.h"
#include <vector>
#include <utility>
#include <string>

class oEvent;
class oTeam;
class oRunner;
class oSpeakerObject;

typedef oTeam* pTeam;
typedef const oTeam* cTeam;
typedef oRunner* pRunner;
typedef const oRunner* cRunner;

// -----------------------------------------------------------------------
// RunnerStatus
// -----------------------------------------------------------------------
enum RunnerStatus {
  StatusOK = 1, StatusDNS = 20, StatusCANCEL = 21, StatusOutOfCompetition = 15,
  StatusMP = 3, StatusDNF = 4, StatusDQ = 5, StatusMAX = 6, StatusNoTiming = 2,
  StatusUnknown = 0, StatusNotCompeting = 99
};

enum class DynamicRunnerStatus {
  StatusInactive,
  StatusActive,
  StatusFinished
};

template<int dummy = 0>
inline bool isPossibleResultStatus(RunnerStatus st) {
  return st == StatusNoTiming || st == StatusOutOfCompetition;
}

template<int dummy = 0>
inline std::vector<RunnerStatus> getAllRunnerStatus() {
  return { StatusOK, StatusDNS, StatusCANCEL, StatusOutOfCompetition, StatusMP,
           StatusDNF, StatusDQ, StatusMAX, StatusUnknown, StatusNotCompeting, StatusNoTiming };
}

template<int dummy = 0>
inline bool showResultTime(RunnerStatus st, int time) {
  return st == StatusOK || (st == StatusOutOfCompetition && time > 0);
}

enum SortOrder {
  ClassStartTime, ClassTeamLeg, ClassResult, ClassDefaultResult, ClassCourseResult,
  ClassTotalResult, ClassTeamLegResult, ClassFinishTime, ClassStartTimeClub,
  ClassPoints, ClassLiveResult, ClassKnockoutTotalResult,
  SortByName, SortByLastName, SortByFinishTime, SortByFinishTimeReverse,
  SortByStartTime, SortByStartTimeClass, CourseResult, CourseStartTime,
  SortByEntryTime, ClubClassStartTime, SortByBib, Custom, SortEnumLastItem
};

// -----------------------------------------------------------------------
// oAbstractRunner
// -----------------------------------------------------------------------
class oAbstractRunner : public oBase {
  friend class RunnerResultTestAccessor;
public:
  int tStartTime = 0;

protected:
  std::wstring sName;
  pClub Club = nullptr;
  pClass Class = nullptr;

  int startTime = 0;
  int FinishTime = 0;
  bool finishTimeWasSet = false;

  mutable int tComputedTime = 0;

  RunnerStatus status = StatusUnknown;
  RunnerStatus tStatus = StatusUnknown;
  mutable RunnerStatus tComputedStatus = StatusUnknown;
  mutable int tComputedPoints = -1;

  std::vector<std::vector<std::wstring>> dynamicData;

  static int compareClubs(const oClub* a, const oClub* b);

  mutable int tTimeAdjustment = 0;
  mutable int tPointAdjustment = 0;
  mutable int tAdjustDataRevision = -1;

  int StartNo = 0;

  int inputTime = 0;
  RunnerStatus inputStatus = StatusOK;
  int inputPoints = 0;
  int inputPlace = 0;

  bool sqlChanged = false;
  bool tEntryTouched = false;

  virtual int getBuiltinAdjustment() const { return 0; }

  mutable std::pair<bool, int> tPreventRestartCache = { false, -1 };

public:
  // Inner struct for caching place/time-after values per data revision.
  struct DynamicValue {
  private:
    int dataRevision = -1;
    int value = -1;
    int valueStd = -1;
    int forKey = 0;
  public:
    void reset();
    bool isOld(const oEvent& oe) const;
    bool isOld(const oEvent& oe, int key) const;
    DynamicValue& update(const oEvent& oe, int key, int v, bool setStd);
    void invalidate(bool invalid) { if (invalid) dataRevision = -1; }
    int get(bool preferStd) const;
  };

  // Temporary result for result modules.
  struct TempResult {
  private:
    int startTime_ = 0;
    int runningTime_ = 0;
    int timeAfter_ = 0;
    int points_ = 0;
    int place_ = 0;
    std::pair<int, int> internalScore_ = {0, 0};
    std::vector<int> outputTimes_;
    std::vector<int> outputNumbers_;
    RunnerStatus status_ = StatusUnknown;
    void reset();
    TempResult() = default;
  public:
    int getRunningTime() const { return runningTime_; }
    int getFinishTime() const { return runningTime_ > 0 ? startTime_ + runningTime_ : 0; }
    int getStartTime() const { return startTime_; }
    int getTimeAfter() const { return timeAfter_; }
    int getPoints() const { return points_; }
    int getPlace() const { return place_; }
    RunnerStatus getStatus() const { return status_; }
    bool isStatusOK() const {
      return status_ == StatusOK ||
        ((status_ == StatusOutOfCompetition || status_ == StatusNoTiming) && runningTime_ > 0);
    }
    const std::wstring& getStatusS(RunnerStatus inputStatus) const;
    const std::wstring& getPrintPlaceS(bool withDot) const;
    const std::wstring& getRunningTimeS(int inputTime, SubSecond mode) const;
    const std::wstring& getFinishTimeS(const oEvent* oe, SubSecond mode) const;
    const std::wstring& getStartTimeS(const oEvent* oe, SubSecond mode) const;
    const std::wstring& getOutputTime(int ix) const;
    int getOutputNumber(int ix) const;

    friend class oAbstractRunner;
    friend class oEvent;
    TempResult(RunnerStatus statusIn, int startTime, int runningTime, int points);
  };

  // Transfer flags for entry management.
  enum TransferFlags {
    FlagTransferNew = 1, FlagUpdateCard = 2, FlagTransferSpecified = 4,
    FlagFeeSpecified = 8, FlagUpdateClass = 16, FlagUpdateName = 32,
    FlagAutoDNS = 64, FlagAddedViaAPI = 128, FlagOutsideCompetition = 256,
    FlagNoTiming = 512, FlagNoDatabase = 1024, FlagPayBeforeResult = 2048,
    FlagUnnamed = 2 << 12
  };

protected:
  TempResult tmpResult;

public:
  bool hasFlag(TransferFlags flag) const;
  void setFlag(TransferFlags flag, bool state);
  bool noTiming() const { return hasFlag(FlagNoTiming); }

  // Pure virtual interface
  virtual cTeam getTeam() const = 0;
  virtual pTeam getTeam() = 0;
  virtual bool matchAbstractRunner(const oAbstractRunner* target) const = 0;
  virtual bool isResultUpdated(bool totalResult) const = 0;
  virtual int getNumShortening() const = 0;
  virtual void markClassChanged(int controlId) = 0;
  virtual void apply(ChangeType ct, pRunner src) = 0;
  virtual int getTimeAfter(int leg, bool allowUpdate) const = 0;
  virtual void fillSpeakerObject(int leg, int previousControlCourseId,
                                  const std::vector<int>& controlIds,
                                  bool totalResult, oSpeakerObject& spk) const = 0;
  virtual void setBib(const std::wstring& bib, int numericalBib, bool updateStartNo) = 0;
  virtual int getRogainingPoints(bool computed, bool multidayTotal) const = 0;
  virtual int getRogainingReduction(bool computed) const = 0;
  virtual int getRogainingOvertime(bool computed) const = 0;
  virtual int getRogainingPointsGross(bool computed) const = 0;
  virtual RunnerStatus getStatusComputed(bool allowUpdate) const = 0;
  virtual DynamicRunnerStatus getDynamicStatus() const = 0;
  virtual int getPlace(bool allowUpdate = true) const = 0;
  virtual int getTotalPlace(bool allowUpdate = true) const = 0;
  virtual bool runnerHasResult() const { return getRunningTime(false) > 0; }
  virtual bool isTeam() const { return false; }
  virtual std::wstring getEntryDate(bool useTeamEntryDate = true) const = 0;
  virtual const std::pair<std::wstring, int> getRaceInfo() = 0;
  virtual int getRanking() const = 0;
  virtual int classInstance() const = 0;
  virtual int getSpeakerPriority() const;

  // Concrete methods
  virtual void setName(const std::wstring& n, bool manualChange);
  virtual const std::wstring& getName() const { return sName; }
  virtual void setClassId(int id, bool isManualUpdate);
  virtual int getStartNo() const { return StartNo; }
  virtual void setStartNo(int no, ChangeType changeType);
  virtual int getStartTime() const { return tStartTime; }
  virtual int getFinishTime() const { return FinishTime; }
  virtual void setFinishTime(int t);
  virtual int getRunningTime(bool computedTime) const;
  virtual int getTotalRunningTime() const;
  virtual int getPrelRunningTime() const;
  virtual int getBirthAge() const;
  virtual RunnerStatus getTotalStatus(bool allowUpdate = true) const;

  const pClub getClubRef() const { return Club; }
  pClub getClubRef() { return Club; }

  const pClass getClassRef(bool virtualClass) const {
    return (virtualClass && Class) ? Class->getVirtualClass(classInstance()) : Class;
  }
  pClass getClassRef(bool virtualClass) {
    return pClass((virtualClass && Class) ? Class->getVirtualClass(classInstance()) : Class);
  }

  virtual const std::wstring& getClub() const { if (Club) return Club->getName(); else return _EmptyWString; }
  virtual int getClubId() const { if (Club) return Club->getId(); else return 0; }
  virtual void setClub(const std::wstring& clubName);
  virtual pClub setClubId(int clubId);

  const std::wstring& getClass(bool virtualClass) const;
  int getClassId(bool virtualClass) const {
    if (Class)
      return virtualClass ? Class->getVirtualClass(classInstance())->getId() : Class->getId();
    return 0;
  }

  const std::wstring& getBib() const;
  int getEncodedBib() const;

  // Time/status
  const std::wstring& getStartTimeS() const;
  const std::wstring& getStartTimeCompact() const;
  const std::wstring& getFinishTimeS(bool adjusted, SubSecond mode) const;
  const std::wstring& getTotalRunningTimeS(SubSecond mode) const;
  const std::wstring& getRunningTimeS(bool computedTime, SubSecond mode) const;
  bool setStartTime(int t, bool updatePermanent, ChangeType changeType, bool recalculate = true);
  void setStartTimeS(const std::wstring& t);
  void setFinishTimeS(const std::wstring& t);
  bool setStatus(RunnerStatus st, bool updatePermanent, ChangeType changeType, bool recalculate = true);

  RunnerStatus getStatus() const { return tStatus; }
  bool statusOK(bool computed, bool allowUpdate) const {
    return (computed ? getStatusComputed(allowUpdate) : tStatus) == StatusOK;
  }
  bool prelStatusOK(bool computed, bool includeOutsideCompetition, bool allowUpdate) const {
    bool ok = statusOK(computed, allowUpdate) || (tStatus == StatusUnknown && getRunningTime(false) > 0);
    if (!ok && includeOutsideCompetition) {
      RunnerStatus st = computed ? getStatusComputed(true) : tStatus;
      ok = (st == StatusOutOfCompetition || st == StatusNoTiming) && getRunningTime(false) > 0;
    }
    return ok;
  }
  bool isStatusOK(bool computed, bool allowUpdate) const;
  bool isStatusUnknown(bool computed, bool allowUpdate) const;
  bool hasResult() const;

  int getSubSeconds() const {
    if (timeConstSecond > 1) return getRunningTime(false) % timeConstSecond;
    return 0;
  }
  int getFinishTimeAdjusted(bool adjusted) const {
    if (adjusted) return getFinishTime() + getTimeAdjustment(false);
    return FinishTime - getBuiltinAdjustment();
  }

  // Place strings
  std::wstring getPlaceS() const;
  std::wstring getPrintPlaceS(bool withDot) const;
  std::wstring getTotalPlaceS() const;
  std::wstring getPrintTotalPlaceS(bool withDot) const;

  // Status strings
  const std::wstring& getStatusS(bool formatForPrint, bool computedStatus) const;
  const std::wstring& getTotalStatusS(bool formatForPrint) const;
  std::wstring getIOFStatusS() const;
  std::wstring getIOFTotalStatusS() const;

  // Static status helpers
  static const std::wstring& encodeStatus(RunnerStatus st, bool allowError = false);
  static RunnerStatus decodeStatus(const std::wstring& stat);
  static bool isResultStatus(RunnerStatus rs);

  // Input data (multi-day/multi-stage)
  bool hasInputData() const { return inputTime > 0 || inputStatus != StatusOK || inputPoints > 0; }
  void resetInputData();
  void setInputTime(const std::wstring& time);
  std::wstring getInputTimeS() const;
  int getInputTime() const { return inputTime; }
  void setInputStatus(RunnerStatus s);
  std::wstring getInputStatusS() const;
  RunnerStatus getInputStatus() const { return inputStatus; }
  void setInputPoints(int p);
  int getInputPoints() const { return inputPoints; }
  void setInputPlace(int p);
  int getInputPlace() const { return inputPlace; }

  bool isVacant() const;
  bool wasSQLChanged() const { return sqlChanged; }

  // TempResult access
  const TempResult& getTempResult(int tempResultIndex) const;
  TempResult& getTempResult();
  const TempResult& getTempResult() const { return tmpResult; }
  void setTempResultZero(const TempResult& tr);
  void updateComputedResultFromTemp();

  // Fee
  int getDefaultFee() const;
  int getEntryFee() const;
  bool hasLateEntryFee() const;
  void addClassDefaultFee(bool resetFees);
  int getPaymentMode() const;
  void setPaymentMode(int mode);

  // Entry tracking
  int getEntrySource() const;
  void setEntrySource(int src);
  void flagEntryTouched(bool flag);
  bool isEntryTouched() const;

  // Prevent restart
  bool preventRestart() const;
  void preventRestart(bool state);

  // Time adjustment
  int getTimeAdjustment(bool includeBuiltinAdjustment) const;
  int getPointAdjustment() const;
  void setTimeAdjustment(int adjust);
  void setPointAdjustment(int adjust);

  bool isPatrolMember() const {
    return Class && Class->getClassType() == oClassPatrol;
  }

  RunnerStatus getStageResult(int stage, int& time, int& point, int& place) const;
  void getInputResults(std::vector<RunnerStatus>& st, std::vector<int>& times,
                       std::vector<int>& points, std::vector<int>& places) const;
  void addToInputResult(int thisStageNo, const oAbstractRunner* src);
  void hasManuallyUpdatedTimeStatus();

  std::wstring getInfo() const;
  void merge(const oBase& input, const oBase* base) override;

  oAbstractRunner(oEvent* poe, bool loading);
  virtual ~oAbstractRunner() {}

  void setSpeakerPriority(int pri);

  friend class oListInfo;
  friend class GeneralResult;
  friend class oRunner;
};

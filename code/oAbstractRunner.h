#pragma once

/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2026 Melin Software HB

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Melin Software HB - software@melin.nu - www.melin.nu
    Eksoppsvägen 16, SE-75646 UPPSALA, Sweden

************************************************************************/

#include <vector>

#include "oBase.h"
#include "oClub.h"
#include "oClass.h"

class oSpeakerObject;

enum RunnerStatus {
  StatusOK = 1, StatusDNS = 20, StatusCANCEL = 21, StatusOutOfCompetition = 15, StatusMP = 3,
  StatusDNF = 4, StatusDQ = 5, StatusMAX = 6, StatusNoTiming = 2,
  StatusUnknown = 0, StatusNotCompeting = 99
};

enum class DynamicRunnerStatus {
  StatusInactive,
  StatusActive,
  StatusFinished
};

/** Returns true for a status that might or might not indicate a result. */
template<int dummy=0>
bool isPossibleResultStatus(RunnerStatus st) {
  return st == StatusNoTiming || st == StatusOutOfCompetition;
}

template<int dummy=0>
std::vector<RunnerStatus> getAllRunnerStatus() {
  return { StatusOK, StatusDNS, StatusCANCEL, StatusOutOfCompetition, StatusMP,
           StatusDNF, StatusDQ, StatusMAX,
           StatusUnknown, StatusNotCompeting , StatusNoTiming};
}

template<int dummy = 0>
bool showResultTime(RunnerStatus st, int time) {
  return st == StatusOK || (st == StatusOutOfCompetition && time > 0);
}

enum SortOrder {
  ClassStartTime,
  ClassTeamLeg,
  ClassResult,
  ClassDefaultResult,
  ClassCourseResult,
  ClassTotalResult,
  ClassTeamLegResult,
  ClassFinishTime,
  ClassStartTimeClub,
  ClassPoints,
  ClassLiveResult,
  ClassKnockoutTotalResult,
  SortByName,
  SortByLastName,
  SortByFinishTime,
  SortByFinishTimeReverse,
  SortByStartTime,
  SortByStartTimeClass,
  CourseResult,
  CourseStartTime,
  SortByEntryTime,
  ClubClassStartTime,
  SortByBib,
  Custom,
  SortEnumLastItem
};

static bool orderByClass(SortOrder so) {
  switch (so) {
  case ClassStartTime:
  case ClassTeamLeg:
  case ClassResult:
  case ClassDefaultResult:
  case ClassCourseResult:
  case ClassTotalResult:
  case ClassTeamLegResult:
  case ClassFinishTime:
  case ClassStartTimeClub:
  case ClassPoints:
  case ClassLiveResult:
  case ClassKnockoutTotalResult:
    return true;
  }
  return false;
}

typedef const oTeam* cTeam;

class oAbstractRunner : public oBase {
public:
  int tStartTime;

protected:
  wstring sName;
  pClub Club;
  pClass Class;

  int startTime;

  int FinishTime;
  bool finishTimeWasSet = false;

  mutable int tComputedTime = 0;

  RunnerStatus status;
  RunnerStatus tStatus;
  mutable RunnerStatus tComputedStatus = RunnerStatus::StatusUnknown;
  mutable int tComputedPoints = -1;

  vector<vector<wstring>> dynamicData;
public:
  /** Return true if this and target are the same, or target is in this team, or this is in the target team.*/
  virtual bool matchAbstractRunner(const oAbstractRunner *target) const = 0;

  /** Encode status as a two-letter code, non-translated*/
  static const wstring &encodeStatus(RunnerStatus st, bool allowError = false);

  /** Decode the two-letter code of above. Returns unknown if not understood*/
  static RunnerStatus decodeStatus(const wstring &stat);

  /** Return true if the status is of type indicating a result. */
  static bool isResultStatus(RunnerStatus rs);

  /** Returns true if the result in the current state is up-to-date. */
  virtual bool isResultUpdated(bool totalResult) const = 0;

  struct TempResult {
  private:
    int startTime;
    int runningTime;
    int timeAfter;
    int points;
    int place;
    pair<int, int> internalScore;
    vector<int> outputTimes;
    vector<int> outputNumbers;
    RunnerStatus status;
    void reset();
    TempResult();
  public:
    int getRunningTime() const {return runningTime;}
    int getFinishTime() const {return runningTime > 0 ? startTime + runningTime : 0;}
    int getStartTime() const {return startTime;}
    int getTimeAfter() const {return timeAfter;}
    int getPoints() const {return points;}
    int getPlace() const {return place;}
    RunnerStatus getStatus() const {return status;}
    bool isStatusOK() const {
      return status == StatusOK || 
        ((status == StatusOutOfCompetition || status == StatusNoTiming) && runningTime > 0);
    }
    const wstring &getStatusS(RunnerStatus inputStatus) const;
    const wstring &getPrintPlaceS(bool withDot) const;
    const wstring &getRunningTimeS(int inputTime, SubSecond mode) const;
    const wstring &getFinishTimeS(const oEvent *oe, SubSecond mode) const;
    const wstring &getStartTimeS(const oEvent *oe, SubSecond mode) const;

    const wstring &getOutputTime(int ix) const;
    int getOutputNumber(int ix) const;

    friend class GeneralResult;
    friend class oAbstractRunner;
    friend class oEvent;
    friend class DynamicResult;
    TempResult(RunnerStatus statusIn, int startTime, 
               int runningTime, int points);
  };

protected:
  TempResult tmpResult;

  /** Return 1 if a<b, 0 if b<a, otherwise 2.*/
  static int compareClubs(const oClub* a, const oClub* b);

  mutable int tTimeAdjustment;
  mutable int tPointAdjustment;
  mutable int tAdjustDataRevision;
  //Used for automatically assigning courses form class.
  //Set when drawn or by team or...
  int StartNo;

  // Data used for multi-days events
  int inputTime;
  RunnerStatus inputStatus;
  int inputPoints;
  int inputPlace;

  bool sqlChanged;
  bool tEntryTouched;

  virtual int getBuiltinAdjustment() const { return 0; }

  mutable pair<bool, int> tPreventRestartCache = { false, -1 };
public:

  /** Return true if the runner/team should not be part av restart in a relay etc.*/
  bool preventRestart() const;
  void preventRestart(bool state);

  void merge(const oBase &input, const oBase *base) override;

  /** Call this method after doing something to just this
      runner/team that changed the time/status etc, that effects
      the result. May make a global evaluation of the class.
      Never call "for each" runner. */
  void hasManuallyUpdatedTimeStatus();

  /** Returs true if the class is a patrol class */
  bool isPatrolMember() const {
    return Class && Class->getClassType() == oClassPatrol;
  }

  /** Returns true if the team / runner has a start time available*/
  virtual bool startTimeAvailable() const;

  int getEntrySource() const;
  void setEntrySource(int src);
  void flagEntryTouched(bool flag);
  bool isEntryTouched() const;

  /** Returns number of shortenings taken. */
  virtual int getNumShortening() const = 0;

  int getPaymentMode() const;
  void setPaymentMode(int mode);

  enum TransferFlags {
    FlagTransferNew = 1,
    FlagUpdateCard = 2,
    FlagTransferSpecified = 4,
    FlagFeeSpecified = 8,
    FlagUpdateClass = 16,
    FlagUpdateName = 32,
    FlagAutoDNS = 64, // The competitor was set to DNS by the in-forest algorithm
    FlagAddedViaAPI = 128, // Added by the REST api entry.
    FlagOutsideCompetition = 256,
    FlagNoTiming = 512, // No timing requested
    FlagNoDatabase = 1024, // Do not store in databse
    FlagPayBeforeResult = 2048, // Require payment before result
    FlagUnnamed = 2 << 12
  };

  bool hasFlag(TransferFlags flag) const;
  void setFlag(TransferFlags flag, bool state);

  /** Return true if no timing is requested. */
  bool noTiming() const { return hasFlag(FlagNoTiming); }

  // Get the runners team or the team itself
  virtual cTeam getTeam() const = 0;
  virtual pTeam getTeam() = 0;

  virtual wstring getEntryDate(bool useTeamEntryDate = true) const = 0;

  // Set default fee, from class
  // a non-zero fee is changed only if resetFee is true
  void addClassDefaultFee(bool resetFees);

  /** Returns fee from the class. */
  int getDefaultFee() const;

  /** Returns the currently assigned fee. */
  int getEntryFee() const;

  /** Returns true if the entry fee is a late fee. */
  bool hasLateEntryFee() const;

  bool hasInputData() const {return inputTime > 0 || inputStatus != StatusOK || inputPoints > 0;}

  /** Reset input data to no input and the input status to NotCompeting. */
  void resetInputData();

  /** Return results for a specific result module. */
  const TempResult &getTempResult(int tempResultIndex) const;
  TempResult &getTempResult();
  const TempResult &getTempResult() const { return tmpResult; }
 
  void setTempResultZero(const TempResult &tr);

  /** Set the class computed result from tmp result.*/
  void updateComputedResultFromTemp();
  
  // Time
  void setInputTime(const wstring &time);
  wstring getInputTimeS() const;
  int getInputTime() const {return inputTime;}

  // Status
  void setInputStatus(RunnerStatus s);
  wstring getInputStatusS() const;
  RunnerStatus getInputStatus() const {return inputStatus;}

  // Points
  void setInputPoints(int p);
  int getInputPoints() const {return inputPoints;}

  // Place
  void setInputPlace(int p);
  int getInputPlace() const {return inputPlace;}

  bool isVacant() const;
  
  bool wasSQLChanged() const {return sqlChanged;}

  /** Use -1 for all, PunchFinish or controlId */
  virtual void markClassChanged(int controlId) = 0;

  wstring getInfo() const;

  virtual void apply(ChangeType ct, pRunner src) = 0;

  //Get time after on leg/for race
  virtual int getTimeAfter(int leg, bool allowUpdate) const = 0;

  virtual void fillSpeakerObject(int leg, int previousControlCourseId, const vector<int> &controlIds, 
                                 bool totalResult, oSpeakerObject &spk) const = 0;

  virtual int getBirthAge() const;

  virtual void setName(const wstring &n, bool manualChange);
  virtual const wstring &getName() const {return sName;}

  void setFinishTimeS(const wstring &t);
  virtual	void setFinishTime(int t);

  /** Sets start time, if updatePermanent is true, the stored start time is updated,
  otherwise the value is considered deduced. */
  bool setStartTime(int t, bool updatePermanent, ChangeType changeType, bool recalculate = true);
  void setStartTimeS(const wstring &t);

  const pClub getClubRef() const {return Club;}
  pClub getClubRef() {return Club;}
  virtual int classInstance() const = 0;

  const pClass getClassRef(bool virtualClass) const {
    return (virtualClass && Class) ? Class->getVirtualClass(classInstance()) : Class;
  }
  
  pClass getClassRef(bool virtualClass) {
    return pClass((virtualClass && Class) ? Class->getVirtualClass(classInstance()) : Class);
  }

  virtual const wstring &getClub() const {if (Club) return Club->name; else return _EmptyWString;}
  virtual int getClubId() const {if (Club) return Club->Id; else return 0;}
  virtual void setClub(const wstring &clubName);
  virtual pClub setClubId(int clubId);

  const wstring &getClass(bool virtualClass) const;
  int getClassId(bool virtualClass) const {
    if (Class)
      return virtualClass ? Class->getVirtualClass(classInstance())->Id : Class->Id;
    return 0;
  }
      
  virtual void setClassId(int id, bool isManualUpdate);
  virtual int getStartNo() const {return StartNo;}
  virtual void setStartNo(int no, ChangeType changeType);

  // Do not assume start number is equal to bib-no, Bib
  // is only set when it should be shown in lists etc.
  const wstring &getBib() const;
  virtual void setBib(const wstring &bib, int numericalBib, bool updateStartNo) = 0;
  int getEncodedBib() const;

  virtual int getStartTime() const {return tStartTime;}
  virtual int getFinishTime() const {return FinishTime;}
  
  int getFinishTimeAdjusted(bool adjusted) const {
    if (adjusted)
      return getFinishTime() + getTimeAdjustment(false);
    else
      return FinishTime - getBuiltinAdjustment();
  }

  virtual int getRogainingPoints(bool computed, bool multidayTotal) const = 0;
  virtual int getRogainingReduction(bool computed) const = 0;
  virtual int getRogainingOvertime(bool computed) const = 0;
  virtual int getRogainingPointsGross(bool computed) const = 0;
  
  virtual const wstring &getStartTimeS() const;
  virtual const wstring &getStartTimeCompact() const;
  const wstring &getFinishTimeS(bool adjusted, SubSecond mode) const;

  const wstring &getTotalRunningTimeS(SubSecond mode) const;
 	const wstring &getRunningTimeS(bool computedTime, SubSecond mode) const;
  virtual int getRunningTime(bool computedTime) const;

  int getSubSeconds() const {
    if (timeConstSecond > 1)
      return getRunningTime(false) % timeConstSecond;
    else
      return 0;
  }

  /// Get total running time (including earlier stages / races)
  virtual int getTotalRunningTime() const;

  virtual int getPrelRunningTime() const;
  
  virtual const pair<wstring, int> getRaceInfo() = 0;


  wstring getPlaceS() const;
  wstring getPrintPlaceS(bool withDot) const;

  wstring getTotalPlaceS() const;
  wstring getPrintTotalPlaceS(bool withDot) const;

  virtual int getPlace(bool allowUpdate = true) const = 0;
  virtual int getTotalPlace(bool allowUpdate = true) const = 0;

  virtual RunnerStatus getStatusComputed(bool allowUpdate) const = 0;
  RunnerStatus getStatus() const { return tStatus;}
  virtual DynamicRunnerStatus getDynamicStatus() const = 0;

  /** Status OK, including NoTiming/OutOfCompetition*/
  bool isStatusOK(bool computed, bool allowUpdate) const;

  /** Status unknown, including NoTiming/OutOfCompetition*/
  bool isStatusUnknown(bool computed, bool allowUpdate) const;

  inline bool statusOK(bool computed, bool allowUpdate) const {return (computed ? getStatusComputed(allowUpdate) : tStatus) == StatusOK;}
  inline bool prelStatusOK(bool computed, bool includeOutsideCompetition, bool allowUpdate) const {
    bool ok = statusOK(computed, allowUpdate) || (tStatus == StatusUnknown && getRunningTime(false) > 0);
    if (!ok && includeOutsideCompetition) {
      RunnerStatus st = (computed ? getStatusComputed(true) : tStatus);
      ok = (st == StatusOutOfCompetition || st == StatusNoTiming) && getRunningTime(false) > 0;
    }
    return ok;
  }
  
  /** Return true if competitor/team has a time and a readout card (if expected) */
  virtual bool runnerHasResult() const {
    return getRunningTime(false) > 0;
  }

  // Returns true if the competitor has a definite result
  bool hasResult() const {
    RunnerStatus st = getStatusComputed(true);
    if (st == StatusUnknown || st == StatusNotCompeting)
      return false;
    if (isPossibleResultStatus(st)) {
      return runnerHasResult();
    }
    else
      return true;
  }

  /** Sets the status. If updatePermanent is true, the stored start
    time is updated, otherwise the value is considered deduced.
    */
  bool setStatus(RunnerStatus st, bool updatePermanent, ChangeType changeType, bool recalculate = true);
   
  /** Returns the ranking of the runner or the team (first runner in it?) */
  virtual int getRanking() const = 0;

  /// Get total status for this running (including team/earlier races)
  virtual RunnerStatus getTotalStatus(bool allowUpdate = true) const;

  RunnerStatus getStageResult(int stage, int &time, int &point, int &place) const;
  // Get results from all previous stages
  void getInputResults(vector<RunnerStatus> &st, vector<int> &times, vector<int> &points, vector<int> &places) const;
  // Add current result to input result. Only use when transferring to next stage. ThisStageNumber is zero indexed.
  void addToInputResult(int thisStageNo, const oAbstractRunner *src);

  const wstring &getStatusS(bool formatForPrint, bool computedStatus) const;
  wstring getIOFStatusS() const;

  const wstring &getTotalStatusS(bool formatForPrint) const;
  wstring getIOFTotalStatusS() const;

  void setSpeakerPriority(int pri);
  virtual int getSpeakerPriority() const;

  int getTimeAdjustment(bool includeBuiltinAdjustment) const;
  int getPointAdjustment() const;
  
  void setTimeAdjustment(int adjust);
  void setPointAdjustment(int adjust);

  virtual bool isTeam() const { return false; }

  oAbstractRunner(oEvent *poe, bool loading);
  virtual ~oAbstractRunner() {};

  struct DynamicValue {
  private:
    int dataRevision = -1;
    int value = -1;
    int valueStd = -1; // Value without result module
    int forKey = 0; // Key for the type of result that is stored.
  public:
    void reset();
    bool isOld(const oEvent& oe) const;
    bool isOld(const oEvent &oe, int key) const;
    DynamicValue &update(const oEvent &oe, int key, int v, bool setStd);
    void invalidate(bool invalid) { if (invalid) dataRevision = -1; }
    int get(bool preferStd) const; 
  };

  friend class oListInfo;
  friend class GeneralResult;
};

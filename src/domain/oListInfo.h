#pragma once
/************************************************************************
    MeOS - Orienteering Software
    Migrated to src/domain/ — Win32/GDI/XML/GUI rendering excluded.
    Contains:
      - EPostType enum  (what data field to display in a list row)
      - EStdListType enum (standard list identifiers)
      - EFilterList / ESubFilterList (runner/team filter predicates)
      - oListParam  (list configuration, minus GUI colour/callback fields)
      - oListInfo   (list output descriptor, minus rendering methods)
************************************************************************/

#include <memory>
#include <set>
#include <string>
#include <vector>
#include <utility>

class oRunner;
class oClass;
class oAbstractRunner;
class GeneralResult;
class SplitPrintListInfo;

// ---------------------------------------------------------------------------
// EPostType — identifies which data field a list column displays
// ---------------------------------------------------------------------------
enum EPostType {
  lAlignNext,
  lNone,
  lString,
  lResultDescription,
  lTimingFromName,
  lTimingToName,
  lCmpName,
  lCmpDate,
  lCurrentTime,
  lClubName,
  lClubNameShort,
  lClassName,
  lClassStartName,
  lClassStartTime,
  lClassStartTimeRange,
  lClassLength,
  lClassResultFraction,
  lClassRemainInForest,
  lClassAvailableMaps,
  lClassTotalMaps,
  lClassNumEntries,
  lClassDataA,
  lClassDataB,
  lClassTextA,

  lCourseLength,
  lCourseName,
  lCourseNumber,
  lCourseClimb,
  lCourseShortening,
  lCourseUsage,
  lCourseUsageNoVacant,
  lCourseClasses,
  lCourseNumControls,

  lRunnerName,
  lRunnerNameCompact,
  lRunnerGivenName,
  lRunnerFamilyName,
  lRunnerCompleteName,
  lRunnerCompleteNameCompact,
  lRunnerCompleteNameCompactClub,
  lRunnerLegTeamLeaderName,
  lPatrolNameNames,
  lPatrolClubNameNames,
  lPatrolClubNameNamesShort,
  lRunnerFinish,
  lRunnerTime,
  lRunnerGrossTime,
  lRunnerTimeStatus,
  lRunnerTotalTime,
  lRunnerTimePerKM,
  lRunnerTotalTimeStatus,
  lRunnerTotalPlace,
  lRunnerPlaceDiff,
  lRunnerClassCoursePlace,
  lRunnerCoursePlace,
  lRunnerTotalTimeAfter,
  lRunnerClassCourseTimeAfter,
  lRunnerCourseTimeAfter,
  lRunnerTimeAfterDiff,
  lRunnerTempTimeStatus,
  lRunnerTempTimeAfter,
  lRunnerGeneralTimeStatus,
  lRunnerGeneralPlace,
  lRunnerGeneralTimeAfter,
  lRunnerTimeAfter,
  lRunnerLostTime,
  lRunnerPlace,
  lRunnerStart,
  lRunnerCheck,
  lRunnerStartCond,
  lRunnerStartZero,
  lRunnerClub,
  lRunnerClubShort,
  lRunnerCard,
  lRunnerRentalCard,
  lRunnerBib,
  lRunnerStartNo,
  lRunnerRank,
  lRunnerRankScore,
  lRunnerCourse,
  lRunnerRogainingPoint,
  lRunnerRogainingPointTotal,
  lRunnerRogainingPointReduction,
  lRunnerRogainingPointOvertime,
  lRunnerRogainingPointGross,
  lRunnerTimeAdjustment,
  lRunnerPointAdjustment,
  lRunnerCardVoltage,

  lRunnerStageTime,
  lRunnerStageStatus,
  lRunnerStageTimeStatus,
  lRunnerStagePlace,
  lRunnerStagePoints,
  lRunnerStageNumber,

  lRunnerUMMasterPoint,
  lRunnerTimePlaceFixed,
  lRunnerLegNumberAlpha,
  lRunnerLegNumber,

  lRunnerBirthYear,
  lRunnerBirthDate,
  lRunnerAge,
  lRunnerSex,
  lRunnerNationality,
  lRunnerPhone,
  lRunnerFee,
  lRunnerExpectedFee,
  lRunnerPaid,
  lRunnerPayMethod,
  lRunnerEntryDate,
  lRunnerEntryTime,
  lRunnerId,
  lRunnerDataA,
  lRunnerDataB,
  lRunnerTextA,
  lRunnerAnnotation,

  lTeamName,
  lTeamNameRaw,
  lTeamStart,
  lTeamStartCond,
  lTeamStartZero,
  lTeamTimeStatus,
  lTeamTimeAfter,
  lTeamPlace,
  lTeamCourseName,
  lTeamCourseNumber,
  lTeamLegName,
  lTeamLegTimeStatus,
  lTeamLegTimeAfter,
  lTeamRogainingPoint,
  lTeamRogainingPointTotal,
  lTeamRogainingPointReduction,
  lTeamRogainingPointOvertime,
  lTeamTimeAdjustment,
  lTeamPointAdjustment,

  lTeamTime,
  lTeamGrossTime,
  lTeamStatus,
  lTeamClub,
  lTeamClubShort,
  lTeamRunner,
  lTeamRunnerCard,
  lTeamBib,
  lTeamStartNo,
  lTeamFee,

  lTeamTotalTime,
  lTeamTotalTimeStatus,
  lTeamTotalPlace,
  lTeamTotalTimeAfter,
  lTeamTotalTimeDiff,
  lTeamPlaceDiff,

  lTeamDataA,
  lTeamDataB,
  lTeamTextA,
  lTeamAnnotation,

  lPunchNamedTime,
  lPunchTeamTotalNamedTime,
  lPunchNamedSplit,
  lPunchName,
  lPunchTime,
  lPunchTeamTime,
  lPunchControlNumber,
  lPunchControlCode,
  lPunchLostTime,
  lPunchControlPlace,
  lPunchControlPlaceAcc,
  lPunchControlPlaceTeamAcc,
  lPunchSplitTime,
  lPunchTotalTime,
  lPunchTotalTimeAfter,
  lPunchTeamTotalTime,
  lPunchTeamTotalTimeAfter,
  lPunchAbsTime,
  lPunchTimeSinceLast,

  lResultModuleTime,
  lResultModuleNumber,
  lResultModuleTimeTeam,
  lResultModuleNumberTeam,

  lCountry,
  lNationality,

  lControlName,
  lControlCourses,
  lControlClasses,
  lControlVisitors,
  lControlPunches,
  lControlMedianLostTime,
  lControlMaxLostTime,
  lControlMistakeQuotient,
  lControlRunnersLeft,
  lControlCodes,

  lRogainingLeg,
  lRogainingLegFrom,
  lRogainingLegTo,
  lRogainingLegBestTime,
  lRogainingLegNumCompetitors,

  lNumEntries,
  lNumStarts,
  lTotalRunLength,
  lTotalRunTime,

  lRogainingPunch,
  lTotalCounter,
  lSubCounter,
  lSubSubCounter,

  lImage,
  lLineBreak,
  lLastItem
};

// ---------------------------------------------------------------------------
// EStdListType — standard list type codes
// ---------------------------------------------------------------------------
enum EStdListType {
  EStdNone = -1,
  EStdStartList = 1,
  EStdResultList,
  EGeneralResultList,
  ERogainingInd,
  EStdTeamResultListAll,
  EStdTeamResultList = 7,
  EStdTeamStartList,
  EStdTeamStartListLeg,
  EStdIndMultiStartListLeg,
  EStdIndMultiResultListLeg,
  EStdIndMultiResultListAll,
  EStdPatrolStartList,
  EStdPatrolResultList,
  EStdRentedCard,
  EStdResultListLARGE,
  EStdPatrolResultListLARGE = 19,
  EStdIndMultiResultListLegLARGE,
  ETeamCourseList = 22,
  EIndCourseList,
  EStdClubStartList,
  EStdClubResultList,
  EIndPriceList,
  EStdUM_Master,
  EFixedPreReport,
  EFixedReport,
  EFixedInForest,
  EFixedInvoices,
  EFixedEconomy,
  EFixedMinuteStartlist = 35,
  EFixedTimeLine,
  EFixedLiveResult,
  EStdTeamAllLegLARGE,
  EFirstLoadedList = 1000
};

// ---------------------------------------------------------------------------
// EFilterList — filters that restrict which runners appear in a list
// ---------------------------------------------------------------------------
enum EFilterList {
  EFilterHasResult,
  EFilterHasPrelResult,
  EFilterRentCard,
  EFilterHasCard,
  EFilterHasNoCard,
  EFilterExcludeDNS,
  EFilterExcludeCANCEL,
  EFilterVacant,
  EFilterOnlyVacant,
  EFilterAnyResult,
  EFilterAPIEntry,
  EFilterWrongFee,
  EFilterIncludeNotParticipating,
  EFilterModifiedCard,
  EFilterTimeNoResult,
  EFilterUnexpectedPunchOrder,
  _EFilterMax
};

// ---------------------------------------------------------------------------
// ESubFilterList — sub-row filters for team/patrol lists
// ---------------------------------------------------------------------------
enum ESubFilterList {
  ESubFilterHasResult,
  ESubFilterHasPrelResult,
  ESubFilterExcludeDNS,
  ESubFilterVacant,
  ESubFilterSameParallel,
  ESubFilterSameParallelNotFirst,
  ESubFilterNotFinish,
  ESubFilterNamedControl,
  _ESubFilterMax
};

// ---------------------------------------------------------------------------
// oListParam — parameters that configure a list output (domain-relevant subset)
// GUI fields (GDICOLOR, GUICALLBACK, animations) are excluded.
// ---------------------------------------------------------------------------
struct oListParam {
  oListParam();

  EStdListType listCode = EStdNone;
  std::set<int> selection;

  int useControlIdResultTo   = 0;
  int useControlIdResultFrom = 0;
  int filterMaxPer           = 0;
  bool pageBreak             = false;
  bool showHeader            = true;
  bool showInterTimes        = false;
  bool showSplitTimes        = false;
  bool splitAnalysis         = false;
  bool showInterTitle        = false;
  std::wstring title;
  std::wstring name;
  int inputNumber            = 0;
  int nextList               = 0;
  int previousList           = 0;
  bool useLargeSize          = false;
  bool saved                 = false;
  int nColumns               = 1;
  int legNumber              = 0;

  enum class AgeFilter { All, OnlyYouth, ExludeYouth };
  AgeFilter ageFilter = AgeFilter::All;

  mutable bool lineBreakControlList = false;
  mutable int  relayLegIndex        = -1;
  mutable std::wstring defaultName;

  void updateDefaultName(const std::wstring& pname) const { defaultName = pname; }
  void setCustomTitle(const std::wstring& t)               { title = t; }
  const std::wstring& getCustomTitle(const std::wstring& t) const;
  const std::wstring& getDefaultName() const               { return defaultName; }
  void setName(const std::wstring& n)                      { name = n; }
  const std::wstring& getName() const                      { return name; }
  int  getInputNumber() const                              { return inputNumber; }
  void setInputNumber(int n)                               { inputNumber = n; }
  void setLegNumberCoded(int code)                         { legNumber = (code == 1000) ? -1 : code; }
  const int getLegNumberCoded() const                      { return legNumber >= 0 ? legNumber : 1000; }
  bool matchLegNumber(const oClass* cls, int leg) const;
  int  getLegNumber(const oClass* cls) const;
  std::pair<int,bool> getLegInfo(const oClass* cls) const;
  std::wstring getLegName() const;
};

// ---------------------------------------------------------------------------
// oListInfo — runtime descriptor for a rendered list
// GUI-rendering state (oPrintPost vectors, gdioutput calls) is excluded.
// ---------------------------------------------------------------------------
class oListInfo {
public:
  // Base entity type for the list
  enum EBaseType {
    EBaseTypeRunner,
    EBaseTypeTeam,
    EBaseTypeClubRunner,
    EBaseTypeClubTeam,
    EBaseTypeCoursePunches,
    EBaseTypeAllPunches,
    EBaseTypeNone,
    EBaseTypeRunnerGlobal,
    EBaseTypeRunnerLeg,
    EBaseTypeTeamGlobal,
    EBaseTypeCourse,
    EBaseTypeControl,
    EBaseTypeRGLeg,
    EBaseTypeRGLegGlobal,
    EBasedTypeLast_
  };

  static bool addRunners(EBaseType t) {
    return t == EBaseTypeRunner || t == EBaseTypeClubRunner;
  }
  static bool addTeams(EBaseType t) {
    return t == EBaseTypeTeam || t == EBaseTypeClubRunner || t == EBaseTypeClubTeam;
  }
  static bool addPatrols(EBaseType t) {
    return t == EBaseTypeTeam || t == EBaseTypeClubRunner || t == EBaseTypeClubTeam;
  }

  // How results are grouped
  enum ResultType {
    Global,
    Classwise,
    Legwise,
    Coursewise
  };

  // Punch display mode
  enum class PunchMode {
    NoPunch,
    SpecificPunch,
    AnyPunch
  };

  bool isTeamList() const { return listType == EBaseTypeTeam; }
  bool isSplitPrintList() const { return splitPrintInfo != nullptr; }

  bool empty(bool includeHeader = true) const;

  const std::wstring& getName() const { return Name; }

protected:
  std::wstring Name;
  EBaseType listType    = EBaseTypeNone;
  EBaseType listSubType = EBaseTypeNone;
  ResultType resultType = Classwise;
  PunchMode punchMode   = PunchMode::NoPunch;
  int inputNumber       = 0;
  int specificControlId = 0;
  std::set<EFilterList>    filter;
  std::set<ESubFilterList> subFilter;
  std::shared_ptr<SplitPrintListInfo> splitPrintInfo;

  friend class oEvent;
  friend class MetaList;
  friend class MetaListContainer;

public:
  oListInfo() = default;
  virtual ~oListInfo() = default;

  EBaseType  getListType()   const { return listType;   }
  EBaseType  getSubType()    const { return listSubType; }
  ResultType getResultType() const { return resultType;  }
  PunchMode  getPunchMode()  const { return punchMode;   }
  int getInputNumber()       const { return inputNumber; }
  int getSpecificControlId() const { return specificControlId; }

  bool hasFilter(EFilterList f)       const { return filter.count(f)    > 0; }
  bool hasSubFilter(ESubFilterList f) const { return subFilter.count(f) > 0; }
};

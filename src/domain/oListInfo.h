// oListInfo.h — Minimal cross-platform subset of oListInfo types.
// Provides the types used by generalresult.h/cpp without heavy GUI dependencies.
// Full oListInfo migration (with MetaList, list generation, etc.) is deferred to US-014*.
#pragma once

#include <set>
#include <string>
#include <vector>
#include <map>
#include <memory>

using std::string;
using std::wstring;
using std::set;
using std::vector;
using std::map;
using std::shared_ptr;

class oAbstractRunner;
class oEvent;
class oRunner;
class oTeam;
class xmlparser;
class xmlobject;
class GeneralResult;

// ── EPostType: list column types (used by metalist) ──────────────────────────
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

// ── EStdListType: standard list codes ────────────────────────────────────────
enum EStdListType {
  EStdNone = -1,
  EStdStartList = 1,
  EStdResultList,
  EGeneralResultList,
  ERogainingInd,
  EStdTeamResultListAll,
  unused_EStdTeamResultListLeg,
  EStdTeamResultList,
  EStdTeamStartList,
  EStdTeamStartListLeg,
  EStdIndMultiStartListLeg,
  EStdIndMultiResultListLeg,
  EStdIndMultiResultListAll,
  EStdPatrolStartList,
  EStdPatrolResultList,
  EStdRentedCard,
  EStdResultListLARGE,
  unused_EStdTeamResultListLegLARGE,
  EStdPatrolResultListLARGE,
  EStdIndMultiResultListLegLARGE,
  unused_EStdRaidResultListLARGE,
  ETeamCourseList,
  EIndCourseList,
  EStdClubStartList,
  EStdClubResultList,
  EIndPriceList,
  EStdUM_Master,
  EFixedPreReport,
  EFixedReport,
  EFixedInForest,
  EFixedInvoices,
  EFixedMinuteStartList,
  EFixedResultFinishList,
  EStdLastList
};

// ── Minimal oListParam ────────────────────────────────────────────────────────
// Only the fields used by generalresult.cpp are included here.
// Full migration of oListParam (colors, columns, animation) is deferred.
struct MetaListContainer; // forward

struct oListParam {
  oListParam()
    : listCode(EStdNone),
      useControlIdResultTo(0),
      useControlIdResultFrom(0),
      filterMaxPer(0),
      alwaysInclude(nullptr),
      lockUpdate(false),
      pageBreak(false),
      showHeader(true),
      showInterTimes(false),
      showSplitTimes(false),
      splitAnalysis(false),
      showInterTitle(false),
      inputNumber(0),
      nextList(0),
      previousList(0),
      ageFilter(AgeFilter::All),
      lineBreakControlList(false),
      relayLegIndex(-1),
      useLargeSize(false),
      saved(false),
      bgColor(0),
      bgColor2(0),
      fgColor(0),
      nColumns(1),
      animate(false),
      timePerPage(0),
      margin(0),
      screenMode(0),
      htmlRows(0),
      htmlScale(1.0),
      sourceParam(-1),
      tightBoundingBox(false),
      legNumber(0)
  {}

  EStdListType listCode;
  set<int> selection;   // selected class IDs (empty = all classes)
  bool lockUpdate;      // temporary prevent animation update
  int useControlIdResultTo;
  int useControlIdResultFrom;
  int filterMaxPer;
  const oAbstractRunner *alwaysInclude;

  // Returns true if the runner at position count should be included in the list.
  bool filterInclude(int count, const oAbstractRunner *r) const {
    if (filterMaxPer > 0 && count >= filterMaxPer)
      return alwaysInclude != nullptr && alwaysInclude == r;
    return true;
  }
  bool pageBreak;
  bool showHeader;
  bool showInterTimes;
  bool showSplitTimes;
  bool splitAnalysis;
  bool showInterTitle;
  wstring title;
  wstring name;
  int inputNumber;
  int nextList;
  int previousList;

  enum class AgeFilter {
    All,
    OnlyYouth,
    ExludeYouth,
  };

  AgeFilter ageFilter;

  mutable bool lineBreakControlList;
  mutable int relayLegIndex;
  mutable wstring defaultName;
  bool useLargeSize;
  bool saved;

  int bgColor;
  int bgColor2;
  int fgColor;
  wstring bgImage;

  int nColumns;
  bool animate;
  int timePerPage;
  int margin;
  int screenMode;

  int htmlRows;
  double htmlScale;
  string htmlTypeTag;

  int getInputNumber() const { return inputNumber; }
  void setInputNumber(int n) { inputNumber = n; }
  const wstring &getDefaultName() const { return defaultName; }

  bool matchLegNumber(const void* /*cls*/, int /*leg*/) const { return true; }

  int sourceParam;
  bool tightBoundingBox;

  void updateDefaultName(const wstring &pname) const { defaultName = pname; }
  void setCustomTitle(const wstring &t) { title = t; }
  const wstring &getCustomTitle(const wstring &t) const {
    return title.empty() ? t : title;
  }
  void setName(const wstring &n) { name = n; }
  const wstring &getName() const { return name; }

private:
  int legNumber;
};

// ── oListInfo ─────────────────────────────────────────────────────────────────
class oListInfo {
public:
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

  enum ResultType {
    Global,
    Classwise,
    Legwise,
    Coursewise
  };

  enum class PunchMode {
    NoPunch,
    SpecificPunch,
    AnyPunch
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

  bool isTeamList() const { return listType == EBaseTypeTeam; }

  bool empty(bool /*includeHeader*/ = true) const { return true; }

  bool filterRunner(const oRunner &) const { return false; }
  bool filterRunnerResult(GeneralResult *, const oRunner &) const { return false; }

  GeneralResult *applyResultModule(oEvent &, vector<oRunner *> &) const { return nullptr; }

  const wstring &getName() const { return Name; }

protected:
  wstring Name;
  EBaseType listType = EBaseTypeNone;
};

// oAbstractRunner.h — Abstract base for oRunner and oTeam (US-003g).
// Cross-platform, no Win32 / GUI dependencies.
#pragma once

#include "oBase.h"
#include "oClub.h"
#include "oClass.h"
#include "../util/meos_util.h"
#include "../util/timeconstants.hpp"

// ── Forward declarations ──────────────────────────────────────────────────────
class oEvent;
class oAbstractRunner;
class oRunner;
typedef oRunner* pRunner;
typedef const oRunner* cRunner;
class oTeam;
typedef oTeam* pTeam;
typedef const oTeam* cTeam;
class oSpeakerObject;
struct SICard;
class gdioutput;
class oListInfo;

// ── RunnerStatus enum ─────────────────────────────────────────────────────────
enum RunnerStatus {
  StatusUnknown         = 0,
  StatusOK              = 1,
  StatusNoTiming        = 2,
  StatusMP              = 3,
  StatusDNF             = 4,
  StatusDQ              = 5,
  StatusMAX             = 6,
  StatusOutOfCompetition = 15,
  StatusDNS             = 20,
  StatusCANCEL          = 21,
  StatusNotCompeting    = 99
};

// ── DynamicRunnerStatus enum ──────────────────────────────────────────────────
enum DynamicRunnerStatus {
  StatusInactive,
  StatusActive,
  StatusFinished
};

// ── Helper: is this a result status (not just DNS/unknown) ────────────────────
template<typename T = RunnerStatus>
inline bool isPossibleResultStatus(RunnerStatus s) {
  return s == StatusOK || s == StatusNoTiming || s == StatusMP ||
         s == StatusDNF || s == StatusDQ || s == StatusMAX ||
         s == StatusOutOfCompetition;
}

inline vector<RunnerStatus> getAllRunnerStatus() {
  return {
    StatusUnknown, StatusOK, StatusNoTiming, StatusMP,
    StatusDNF, StatusDQ, StatusMAX, StatusOutOfCompetition,
    StatusDNS, StatusCANCEL, StatusNotCompeting
  };
}

inline bool showResultTime(RunnerStatus s) {
  return s == StatusOK || s == StatusNoTiming || s == StatusOutOfCompetition ||
         s == StatusNotCompeting;
}

// ── SortOrder ─────────────────────────────────────────────────────────────────
enum SortOrder {
  ClassStartTime, ClassStartTimeClub, ClassResult, ClassCourseResult,
  ClassDefaultResult, ClassTeamLeg, SortByName, SortByClub, SortByStartNo,
  ClassPoints, CoursePoints, ClassTotalResult
};

inline bool orderByClass(SortOrder s) {
  return s != SortByName && s != SortByClub && s != SortByStartNo;
}

// ── RunnerStatusOrderMap declaration ─────────────────────────────────────────
extern char RunnerStatusOrderMap[100];

// ── MaxRankingConstant ────────────────────────────────────────────────────────
constexpr int MaxRankingConstant = std::numeric_limits<int>::max();

// ── oAbstractRunner ───────────────────────────────────────────────────────────
class oAbstractRunner : public oBase {
public:
  // ── TransferFlags ──────────────────────────────────────────────────────────
  enum TransferFlags {
    FlagManualName        = 1,
    FlagManualFees        = 2,
    FlagNoTransfer        = 4,
    FlagTransferSpecified = 8
  };

  // ── TempResult ────────────────────────────────────────────────────────────
  struct TempResult {
    int runningTime = 0;
    RunnerStatus status = StatusUnknown;
    int points = 0;
    int place = 0;

    TempResult() = default;
    TempResult(int rt, RunnerStatus s, int pts, int pl)
      : runningTime(rt), status(s), points(pts), place(pl) {}

    int getRunningTime() const { return runningTime; }
    RunnerStatus getStatus() const { return status; }
    int getPoints() const { return points; }
    int getPlace() const { return place; }
    void reset() { runningTime = 0; status = StatusUnknown; points = 0; place = 0; }
  };

  // ── DynamicValue ──────────────────────────────────────────────────────────
  struct DynamicValue {
    mutable int value = 0;
    mutable unsigned long revision = static_cast<unsigned long>(-1);

    bool isOld(const oEvent& oe) const;
    void update(const oEvent& oe, int val) const;
    int get() const { return value; }
    int get(bool /*skipUpdate*/) const { return value; }
    void reset() { revision = static_cast<unsigned long>(-1); value = 0; }
  };

protected:
  wstring sName;
  pClub  Club  = nullptr;
  pClass Class = nullptr;

  int startTime    = 0;
  int FinishTime   = 0;
  bool finishTimeWasSet = false;

public:
  int tStartTime   = 0;  // Effective start time (may differ from startTime after draw)

protected:

  int tComputedTime   = 0;
  RunnerStatus status  = StatusUnknown;
  RunnerStatus tStatus = StatusUnknown;
  RunnerStatus tComputedStatus = StatusUnknown;
  int tComputedPoints = 0;

  // Dynamic data cached value (e.g. place)
  DynamicValue dynamicData;

  TempResult tmpResult;

  int tTimeAdjustment  = 0;
  int tPointAdjustment = 0;

  unsigned long tAdjustDataRevision = static_cast<unsigned long>(-1);

  int StartNo = 0;

  // Input/carry-over data
  int inputTime   = 0;
  RunnerStatus inputStatus = StatusUnknown;
  int inputPoints = 0;
  int inputPlace  = 0;

  string sqlChanged;
  bool   tEntryTouched        = false;
  bool   tPreventRestartCache = false;

public:
  explicit oAbstractRunner(oEvent* poe, bool loading);

  // ── Identification ────────────────────────────────────────────────────────
  int getId() const { return Id; }

  // ── Class ─────────────────────────────────────────────────────────────────
  pClass getClassRef(bool withTraverse) const { return Class; }
  int    getClassId(bool withTraverse)  const { return Class ? Class->getId() : 0; }
  virtual void setClassId(int id, bool isManualUpdate);

  // ── Club ──────────────────────────────────────────────────────────────────
  virtual void  setClub(const wstring& name);
  virtual pClub setClubId(int id);
  int    getClubId()  const { return Club ? Club->getId() : 0; }
  wstring getClub()   const { return Club ? Club->getName() : L""; }

  // ── Name ──────────────────────────────────────────────────────────────────
  virtual void           setName(const wstring& n, bool manualUpdate) = 0;
  virtual const wstring& getName() const = 0;

  // ── Start / Finish / Status ───────────────────────────────────────────────
  virtual bool setStartTime(int t, bool updatePermanent, ChangeType ct, bool recalculate = false);
  virtual void setFinishTime(int t);
  virtual bool setStatus(RunnerStatus st, bool updatePermanent, ChangeType ct, bool recalculate = false);

  int  getStartTime()  const { return tStartTime; }
  int  getFinishTime() const { return FinishTime; }
  RunnerStatus getStatus() const { return tStatus; }
  virtual int getRunningTime(bool computed) const;
  bool startTimeAvailable() const { return tStartTime > 0; }

  // ── StartNo ───────────────────────────────────────────────────────────────
  int  getStartNo() const { return StartNo; }
  virtual void setStartNo(int no, ChangeType ct) {
    if (StartNo != no) { StartNo = no; updateChanged(ct); }
  }

  // ── Class name helper ─────────────────────────────────────────────────────
  virtual const wstring& getClass(bool virtualClass) const;

  // ── Input result ─────────────────────────────────────────────────────────
  int  getInputTime()   const { return inputTime; }
  RunnerStatus getInputStatus() const { return inputStatus; }
  int  getInputPoints() const { return inputPoints; }
  int  getInputPlace()  const { return inputPlace; }

  void setInputTime(int t)           { inputTime   = t; }
  void setInputStatus(RunnerStatus s){ inputStatus  = s; }
  void setInputPoints(int p)         { inputPoints = p; }
  void setInputPlace(int p)          { inputPlace  = p; }

  // ── Pure-virtual interface ────────────────────────────────────────────────
  virtual cTeam getTeam() const = 0;
  virtual pTeam getTeam()       = 0;

  virtual RunnerStatus getTotalStatus(bool computed) const = 0;
  virtual int  getPlace(bool computed) const = 0;
  virtual int  getTotalPlace(bool computed) const = 0;
  virtual DynamicRunnerStatus getDynamicStatus() const = 0;
  virtual int  getRaceNo() const = 0;
  virtual bool isResultUpdated(bool) const = 0;
  virtual int  getNumShortening() const = 0;
  virtual int  getRanking() const = 0;
  virtual int  getRogainingPoints(bool computed, bool total) const = 0;
  virtual int  getRogainingPointsGross(bool computed) const = 0;
  virtual int  getRogainingReduction(bool computed) const = 0;
  virtual int  getRogainingOvertime(bool computed) const = 0;
  virtual bool matchAbstractRunner(const oAbstractRunner* target) const = 0;
  virtual wstring getNameAndRace(bool withRace) const = 0;

  // ── Relay / multi-runner interface ────────────────────────────────────────
  virtual void markClassChanged(int controlId) = 0;
  virtual void apply(ChangeType ct, pRunner src) = 0;
  virtual int  getTimeAfter(int leg, bool allowUpdate) const = 0;
  virtual int  classInstance() const = 0;
  virtual void setBib(const wstring& bib, int numericalBib, bool updateStartNo) = 0;
  virtual const pair<wstring, int> getRaceInfo() = 0;
  virtual RunnerStatus getStatusComputed(bool allowUpdate) const = 0;

  // ── Status helpers ────────────────────────────────────────────────────────
  static wstring    encodeStatus(RunnerStatus s);
  static RunnerStatus decodeStatus(const wstring& s);

  // isResultStatus: true for OK/MP/DNF/DQ/MAX — not DNS/CANCEL/OutOfComp/Unknown
  static bool isResultStatus(RunnerStatus st) {
    switch (st) {
      case StatusDNS: case StatusCANCEL: case StatusOutOfCompetition:
      case StatusNotCompeting: case StatusUnknown: case StatusNoTiming:
        return false;
      default:
        return true;
    }
  }

  // ── Status string ─────────────────────────────────────────────────────────
  virtual const wstring& getStatusS(bool shortFormat, SubSecond mode) const;

  // ── Club ref ──────────────────────────────────────────────────────────────
  pClub getClubRef() const { return Club; }
  bool  isVacant()   const { return Club && Club->getId() == cVacantId; }

  // ── Bib / StartNo helpers ────────────────────────────────────────────────
  wstring getBib() const;

  // ── Time/result helpers (string formatting) ───────────────────────────────
  wstring getStartTimeS() const;
  wstring getRunningTimeS(bool computed, SubSecond mode) const;
  wstring getFinishTimeS(bool adjusted, SubSecond mode) const;
  wstring getPlaceS() const { int p = getPlace(true); return (p > 0 && p < 10000) ? itow(p) : L""; }
  wstring getInputTimeS()   const { return inputTime   > 0 ? formatTime(inputTime) : L"-"; }
  wstring getInputStatusS() const;

  // ── Start time from string ────────────────────────────────────────────────
  void setStartTimeS(const wstring& /*s*/) {}

  // ── Total result convenience (no-arg) ────────────────────────────────────
  RunnerStatus getTotalStatus() const { return getTotalStatus(true); }
  int getTotalRunningTime() const { return getRunningTime(true); }
  int getTotalPlace() const { return getTotalPlace(true); }

  // ── Adjustments (multi-day / external) ───────────────────────────────────
  int getTimeAdjustment(bool /*preliminary*/) const { return tTimeAdjustment; }
  int getPointAdjustment() const { return tPointAdjustment; }
  virtual int getBuiltinAdjustment() const { return 0; }

  // ── Prevent restart flag ──────────────────────────────────────────────────
  bool preventRestart() const { return tPreventRestartCache; }

  // ── Multi-day input result ────────────────────────────────────────────────
  void addToInputResult(int /*stage*/, const oAbstractRunner* /*src*/) {}

  // ── Bib comparison helpers ────────────────────────────────────────────────
  static bool compareBib(const wstring& a, const wstring& b);
  static int  compareClubs(const oClub* ca, const oClub* cb);

  // ── Merge (no-op base) ────────────────────────────────────────────────────
  void merge(const oBase& input, const oBase* base) override;

  virtual ~oAbstractRunner() = default;

  friend class oClass;
  friend class oEvent;
  friend class oRunner;
  friend class oTeam;
  friend class MeosSQL;
};

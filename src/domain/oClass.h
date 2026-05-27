#pragma once
#include "oBase.h"
#include "oDataContainer.h"
#include "oCourse.h"
#include <set>
#include <vector>

class oClass;
using pClass = oClass*;

// ── Enumerations ────────────────────────────────────────────────────────────

const int MaxClassId = 1000000;

enum StartTypes {
  STTime   = 0,
  STChange = 1,
  STDrawn  = 2,
  STPursuit = 3,
  ST_max
};
enum { nStartTypes = ST_max };

enum LegTypes {
  LTNormal           = 0,
  LTParallel         = 1,
  LTExtra            = 2,
  LTSum              = 3,
  LTIgnore           = 4,
  LTParallelOptional = 5,
  LTGroup            = 6,
  LT_max,
};
enum { nLegTypes = LT_max };

enum BibMode {
  BibUndefined = -1,
  BibSame      = 0,
  BibAdd       = 1,
  BibFree      = 2,
  BibLeg       = 3,
};

enum AutoBibType {
  AutoBibManual      = 0,
  AutoBibConsecutive = 1,
  AutoBibNone        = 2,
  AutoBibExplicit    = 3
};

enum ClassType {
  oClassIndividual      = 1,
  oClassPatrol          = 2,
  oClassRelay           = 3,
  oClassIndividRelay    = 4,
  oClassKnockout        = 5
};

enum ClassMetaType { ctElite, ctNormal, ctYouth, ctTraining,
                     ctExercise, ctOpen, ctUnknown };

// ── oLegInfo ────────────────────────────────────────────────────────────────

struct oLegInfo {
  StartTypes startMethod;
  LegTypes   legMethod;
  bool isParallel() const {
    return legMethod == LTParallel || legMethod == LTParallelOptional;
  }
  bool isOptional() const {
    return legMethod == LTParallelOptional || legMethod == LTExtra || legMethod == LTIgnore;
  }
  int legStartData;
  int legRestartTime;
  int legRopeTime;
  int duplicateRunner;

  bool isStartDataTime() const {
    return startMethod == STTime || startMethod == STPursuit;
  }

  // Transient
  int trueSubLeg;
  int trueLeg;
  std::string displayLeg;

  oLegInfo()
      : startMethod(STTime), legMethod(LTNormal),
        legStartData(0), legRestartTime(0), legRopeTime(0),
        duplicateRunner(-1), trueSubLeg(0), trueLeg(0) {}

  std::string codeLegMethod() const;
  void importLegMethod(const std::string& leg);
};

// ── oClass ──────────────────────────────────────────────────────────────────

class oClass : public oBase {
public:
  enum class ClassStatus { Normal, Invalid, InvalidRefund };

  explicit oClass(oEvent* poe);
  oClass(oEvent* poe, int id);
  ~oClass() override = default;

  std::wstring getInfo() const override;
  void changedObject() override;
  void remove() override;
  bool canRemove() const override;
  void merge(const oBase&, const oBase*) override {}

  oDataContainer& getDataBuffers(pvoid& d, pvoid& o, pvectorstr& s) const override;
  int getDISize() const final { return 0; }

  // ── Name ──────────────────────────────────────────────────────────────
  const std::wstring& getName() const { return Name; }
  void setName(const std::wstring& name, bool manualSet);
  const std::wstring& getLongName() const;
  void setLongName(const std::wstring& name);

  // ── Single-course assignment ───────────────────────────────────────────
  pCourse getCourse(bool getSampleFromRunner = false) const;
  pCourse getCourse(int leg, unsigned fork = 0, bool getSampleFromRunner = false) const;
  void setCourse(pCourse c);
  int getCourseId() const { return Course ? Course->getId() : 0; }

  // ── Multi-course (relay / multi-stage) ────────────────────────────────
  unsigned getNumStages() const { return (unsigned)MultiCourse.size(); }
  bool hasMultiCourse() const { return !MultiCourse.empty(); }
  void setNumStages(int no);
  bool addStageCourse(int stage, int courseId, int index);
  bool addStageCourse(int stage, pCourse pc, int index);
  void clearStageCourses(int stage);
  bool removeStageCourse(int stage, int courseId, int position);
  bool moveStageCourse(int stage, int index, int offset);
  void getCourses(int leg, std::vector<pCourse>& courses) const;
  bool isForked(int leg) const;
  bool isCourseUsed(int id) const;
  bool hasTrueMultiCourse() const;
  unsigned getLastStageIndex() const {
    return (unsigned)std::max((int)MultiCourse.size(), 1) - 1;
  }

  // ── Leg info / relay structure ─────────────────────────────────────────
  int getNumLegs() const { return (int)legInfo.size(); }
  StartTypes getStartType(int leg) const;
  LegTypes   getLegType(int leg) const;
  int getStartData(int leg) const;
  int getRestartTime(int leg) const;
  int getRopeTime(int leg) const;
  void setStartType(int leg, StartTypes st, bool noThrow);
  void setLegType(int leg, LegTypes lt);
  bool setStartData(int leg, int value);
  void setRestartTime(int leg, const std::wstring& t);
  void setRopeTime(int leg, const std::wstring& t);
  int getLegRunner(int leg) const;
  int getLegRunnerIndex(int leg) const;
  void setLegRunner(int leg, int runnerNo);
  int getNumMultiRunners(int leg) const;
  int getNumLegNoParallel() const;
  int getNumParallel(int leg) const;
  int getNextBaseLeg(int leg) const;
  int getPreceedingLeg(int leg) const;
  int getResultDefining(int leg) const;
  bool isParallel(size_t leg) const {
    return leg < legInfo.size() && legInfo[leg].isParallel();
  }
  bool isOptional(size_t leg) const {
    return leg < legInfo.size() && legInfo[leg].isOptional();
  }
  bool splitLegNumberParallel(int leg, int& legNumber, int& legOrder) const;
  int getLegNumberLinear(int legNumber, int legOrder) const;
  std::wstring getLegNumber(int leg) const;
  int getNumDistinctRunners() const;
  int getNumDistinctRunnersMinimal() const;
  bool isSingleRunnerMultiStage() const;

  // ── Timing / start punch ──────────────────────────────────────────────
  void setNoTiming(bool noTiming);
  bool getNoTiming() const;
  void setIgnoreStartPunch(bool ignore);
  bool ignoreStartPunch() const;
  bool hasFreeStart() const;
  void setFreeStart(bool freeStart);
  bool hasRequestStart() const;
  void setRequestStart(bool reqStart);
  bool hasDirectResult() const;
  void setDirectResult(bool directResult);

  // ── ClassType / meta ──────────────────────────────────────────────────
  ClassType getClassType() const;
  ClassStatus getClassStatus() const;

  // ── Sort index ────────────────────────────────────────────────────────
  int getSortIndex() const { return tSortIndex; }
  bool operator<(const oClass& b) const {
    return tSortIndex < b.tSortIndex ||
           (tSortIndex == b.tSortIndex && Id < b.Id);
  }

  // ── Misc ──────────────────────────────────────────────────────────────
  bool isQualificationFinalBaseClass() const { return false; }
  bool isQualificationFinalClass() const { return false; }
  bool hasCoursePool() const { return getDCI().getInt("HasPool") != 0; }
  bool isRogaining() const;
  std::string getResultModuleTag() const { return ""; }
  bool isTeamClass() const;

  // Stubs required by oAbstractRunner/oRunner (US-003g1)
  oClass* getVirtualClass(int /*instance*/) const { return const_cast<oClass*>(this); }
  void clearCache(bool /*deep*/) {}

  // Split analysis cache invalidation (called after evaluateCard changes times)
  void clearSplitAnalysis() {}

  // Maximum running time for status MAX computation (0 = no limit)
  int getMaximumRunnerTime() const { return tMaxTime; }

  // Stub result-info store (cleared by setClubId, filled by result computation)
  struct ResultInfoItem {};
  mutable std::vector<ResultInfoItem> tResultInfo;

  // Rogaining analysis stub (result computed in US-003g2)
  struct RogainingAnalysis {
    int totalPoints = 0;
    int missingControls = 0;
    int overtime = 0;
  };

  // leg-method coding/importing (used in XML serialization)
  std::string codeLegMethod() const;
  void importLegMethod(const std::string& legMethods);

  // course import from int vectors (used after XML/SQL load)
  void importCourses(const std::vector<std::vector<int>>& multi);
  static void parseCourses(const std::string& courses,
                           std::vector<std::vector<int>>& multi,
                           std::set<int>& courseId);

  void clearDuplicate();

  // Transient sort/cache data
  mutable int tSortIndex = 0;
  mutable int tNoTiming = -1;
  mutable int tIgnoreStartPunch = -1;
  mutable int tMaxTime = 0;

  static oDataContainer& container();

private:
  std::wstring Name;
  pCourse      Course = nullptr;
  std::vector<std::vector<pCourse>> MultiCourse;
  std::vector<oLegInfo> legInfo;

  mutable DataMap dataMap_;
  mutable DataMap oldDataMap_;

  std::string codeMultiCourseStr() const;
  std::set<int>& getMCourseIdSet(std::set<int>& in) const;
};

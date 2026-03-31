#pragma once
// oClass.h — Full oClass declaration.
// Ported from code/oClass.h with Win32 and GUI code removed.

#include "domain_header.h"
#include "oCourse.h"
#include "inthashmap.h"
#include "../util/meos_util.h"   // PersonSex

class oClass;
typedef oClass* pClass;
class oDataInterface;
class GeneralResult;
class oRunner;
class QualificationFinal;
class xmlparser;
class gdioutput;

const int MaxClassId = 1000000;

enum StartTypes {
  STTime = 0,
  STChange,
  STDrawn,
  STPursuit,
  ST_max
};
enum { nStartTypes = ST_max };

enum LegTypes {
  LTNormal = 0,
  LTParallel,
  LTExtra,
  LTSum,
  LTIgnore,
  LTParallelOptional,
  LTGroup,
  LT_max,
};
enum { nLegTypes = LT_max };

enum BibMode {
  BibUndefined = -1,
  BibSame = 0,
  BibAdd = 1,
  BibFree = 2,
  BibLeg = 3,
};

enum AutoBibType {
  AutoBibManual = 0,
  AutoBibConsecutive = 1,
  AutoBibNone = 2,
  AutoBibExplicit = 3
};

enum ClassSplitMethod {
  SplitRandom,
  SplitClub,
  SplitRank,
  SplitRankEven,
  SplitResult,
  SplitTime,
  SplitResultEven,
  SplitTimeEven,
};

enum ClassSeedMethod {
  SeedRank,
  SeedResult,
  SeedTime,
  SeedPoints
};

#ifdef DODECLARETYPESYMBOLS
  const char *StartTypeNames[4] = {"ST", "CH", "DR", "HU"};
  const char *LegTypeNames[7]   = {"NO", "PA", "EX", "SM", "IG", "PO", "GP"};
#else
  extern const char *StartTypeNames[4];
  extern const char *LegTypeNames[7];
#endif

struct oLegInfo {
  StartTypes startMethod;
  LegTypes legMethod;
  bool isParallel() const { return legMethod == LTParallel || legMethod == LTParallelOptional; }
  bool isOptional() const { return legMethod == LTParallelOptional || legMethod == LTExtra || legMethod == LTIgnore; }
  int legStartData;
  int legRestartTime;
  int legRopeTime;
  int duplicateRunner;
  bool isStartDataTime() const { return startMethod == STTime || startMethod == STPursuit; }

  // Transient, deducable data
  int trueSubLeg;
  int trueLeg;
  string displayLeg;

  oLegInfo() : startMethod(STTime), legMethod(LTNormal), legStartData(0),
               legRestartTime(0), legRopeTime(0), duplicateRunner(-1),
               trueSubLeg(0), trueLeg(0) {}
  string codeLegMethod() const;
  void importLegMethod(const string &leg);
};

struct ClassResultInfo {
  ClassResultInfo() : nUnknown(0), nFinished(0), lastStartTime(0) {}
  int nUnknown;
  int nFinished;
  int lastStartTime;
};

enum ClassType {
  oClassIndividual = 1,
  oClassPatrol = 2,
  oClassRelay = 3,
  oClassIndividRelay = 4,
  oClassKnockout = 5
};

enum ClassMetaType { ctElite, ctNormal, ctYouth, ctTraining, ctExercise, ctOpen, ctUnknown };

class Table;

class oClass : public oBase {
public:
  enum class ClassStatus { Normal, Invalid, InvalidRefund };
  enum class AllowRecompute { Yes, No, NoUseOld };

  static void getSplitMethods(vector<pair<wstring, size_t>> &methods);
  static void getSeedingMethods(vector<pair<wstring, size_t>> &methods);

  struct RogainingStat {
    int bestTime = -1;
    int numCompetitors = 0;
    double globalBest = 0.0;
  };

  struct RogainingLeg : public RogainingStat {
    int from = -1;
    int to = -1;
  };

protected:
  wstring Name;
  pCourse Course;

  vector<vector<pCourse>> MultiCourse;
  vector<oLegInfo> legInfo;

  struct LeaderInfo {
  private:
    int bestTimeOnLeg;
    int bestTimeOnLegComputed;
    int totalLeaderTime;
    int totalLeaderTimeComputed;
    int totalLeaderTimeInput;
    int totalLeaderTimeInputComputed;
    int inputTime;
    bool complete = false;
  public:
    LeaderInfo() { reset(); }

    void reset() {
      bestTimeOnLeg = -1;
      bestTimeOnLegComputed = -1;
      totalLeaderTime = -1;
      totalLeaderTimeComputed = -1;
      inputTime = -1;
      totalLeaderTimeInput = -1;
      totalLeaderTimeInputComputed = -1;
      complete = false;
    }

    void updateFrom(const LeaderInfo& i) {
      if (i.complete) {
        if (i.bestTimeOnLeg != -1)        bestTimeOnLeg = i.bestTimeOnLeg;
        if (i.bestTimeOnLegComputed != -1) bestTimeOnLegComputed = i.bestTimeOnLegComputed;
        if (i.totalLeaderTime != -1)       totalLeaderTime = i.totalLeaderTime;
        if (i.totalLeaderTimeComputed != -1) totalLeaderTimeComputed = i.totalLeaderTimeComputed;
        if (i.inputTime != -1)             inputTime = i.inputTime;
        if (i.totalLeaderTimeInput != -1)  totalLeaderTimeInput = i.totalLeaderTimeInput;
        if (i.totalLeaderTimeInputComputed != -1) totalLeaderTimeInputComputed = i.totalLeaderTimeInputComputed;
      }
    }

    enum class Type { Leg, Total, TotalInput, Input };

    int getInputTime() const { return inputTime; }
    void resetComputed(Type t);
    bool update(int rt, Type t);
    bool updateComputed(int rt, Type t);
    int getLeader(Type t, bool computed) const;
    void setComplete() { complete = true; }
    void copyInputToTotalInput();
  };

  void updateLeaderTimes() const;
  LeaderInfo &getLeaderInfo(AllowRecompute recompute, int leg) const;

  mutable int leaderTimeVersion = -1;
  mutable vector<LeaderInfo> tLeaderTime;
  mutable vector<LeaderInfo> tLeaderTimeOld;
  mutable map<int, int> tBestTimePerCourse;

  int tSplitRevision;
  map<int, vector<int>> tSplitAnalysisData;
  map<int, vector<int>> tCourseLegLeaderTime;
  map<int, vector<int>> tCourseAccLegLeaderTime;
  mutable vector<int> tFirstStart;
  mutable vector<int> tLastStart;

  mutable ClassStatus tStatus;
  mutable int tStatusRevision;

  inthashmap *tLegTimeToPlace;
  inthashmap *tLegAccTimeToPlace;

  struct PlaceTime {
    int leader = -1;
    map<int, int> timeToPlace;
  };

  vector<map<int, PlaceTime>> teamLegCourseControlToLeaderPlace;

  void insertLegPlace(int from, int to, int time, int place);
  void insertAccLegPlace(int courseId, int controlNo, int time, int place);
  int getAccLegControlLeader(int teamLeg, int courseControlId) const;
  int getAccLegControlPlace(int teamLeg, int courseControlId, int time) const;

  int tLegLeaderTime;
  mutable int tNoTiming;
  mutable int tIgnoreStartPunch;

  mutable int tSortIndex;
  mutable int tMaxTime;

  mutable bool isInitialized = false;
  bool tCoursesChanged;
  bool tShowMultiDialog;

  // On Windows wchar_t=2 bytes, on Linux wchar_t=4 bytes; scale buffer accordingly.
  static constexpr int dataSize = 256 * static_cast<int>(sizeof(wchar_t)) + 64;
  int getDISize() const final { return dataSize; }

  alignas(sizeof(wchar_t)) BYTE oData[dataSize];
  alignas(sizeof(wchar_t)) BYTE oDataOld[dataSize];
  vector<vector<wstring>> oDataStr;

  string codeMultiCourse() const;
  void importCourses(const vector<vector<int>> &multi);
  static void parseCourses(const string &courses, vector<vector<int>> &multi, set<int> &courseId);
  set<int> &getMCourseIdSet(set<int> &in) const;

  string codeLegMethod() const;
  void importLegMethod(const string &courses);

  map<int, set<int>> sqlChangedControlLeg;
  map<int, set<int>> sqlChangedLegControl;
  void markSQLChanged(int leg, int control);

  void addTableRow(Table &table) const;
  pair<int, bool> inputData(int id, const wstring &input, int inputId,
                            wstring &output, bool noUpdate) override;
  void fillInput(int id, vector<pair<wstring, size_t>> &elements, size_t &selected) override;

  void exportIOFStart(xmlparser &xml) {}

  void reinitialize(bool force) const;
  void apply();
  void calculateSplits() {}
  void clearSplitAnalysis();

  int mapLeg(int inputLeg) const {
    if (inputLeg > 0 && legInfo.size() <= 1)
      return 0;
    return inputLeg;
  }

  mutable vector<ClassResultInfo> tResultInfo;

  int getSortIndex(int candidate) const;
  oDataContainer &getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const;
  void changedObject();

  static long long setupForkKey(const vector<int> indices,
                                const vector<vector<vector<int>>> &courseKeys,
                                vector<int> &ws);

  mutable vector<pClass> virtualClasses;
  pClass parentClass;

  mutable shared_ptr<QualificationFinal> qualificatonFinal;

  int tMapsRemaining;
  mutable int tMapsUsed;
  mutable int tMapsUsedNoVacant;

  mutable pair<int, map<string, int>> tTypeKeyToRunnerCount;

  enum CountKeyType { AllCompeting, Finished, ExpectedStarting, DNS, IncludeNotCompeting };
  DataRevisionCache<map<pair<int,int>, RogainingStat>> rogainingStatistics;

  static string getCountTypeKey(int leg, CountKeyType type, bool countVacant);

  void configureInstance(int instance, bool allowCreation) const;

public:
  // Public oData accessors for repository persistence
  const BYTE* getOData() const { return oData; }
  BYTE* getOData() { return oData; }
  static int getODataBlobSize() { return dataSize; }

  struct RogainingAnalysis {
    int bestTime = -1;
    int lostTime = -1;
    int legPlace = 0;
    int numLegRunners = 0;
  };

  RogainingAnalysis getRogainingAnalysis(int from, int to, double baseSpeed) const;
  vector<RogainingLeg> getRogainingLegs() const;

  static const shared_ptr<Table> &getTable(oEvent *oe);

  enum TransferFlags { FlagManualName = 1, FlagManualFees = 2 };

  bool hasFlag(TransferFlags flag) const;
  void setFlag(TransferFlags flag, bool state);

  const pClass getParentClass() const { return parentClass; }

  const shared_ptr<QualificationFinal> &getQualificationFinal() const {
    reinitialize(false);
    return qualificatonFinal;
  }
  void clearQualificationFinal() const;

  bool isQualificationFinalClass() const {
    return parentClass && parentClass->isQualificationFinalBaseClass();
  }
  bool isQualificationFinalBaseClass() const { return qualificatonFinal != nullptr; }

  bool isTeamClass() const {
    int ns = getNumStages();
    return ns > 0 && getNumDistinctRunners() > 1;
  }

  int getNumQualificationFinalClasses() const;
  void loadQualificationFinalScheme(const QualificationFinal &scheme);
  void updateFinalClasses(oRunner *causingResult, bool updateStartNumbers);

  static void initClassId(oEvent &oe, const set<int>& classes);

  bool lockedForking() const;
  void lockedForking(bool locked);
  bool lockedClassAssignment() const;
  void lockedClassAssignment(bool locked);

  int getDrawFirstStart() const;
  void setDrawFirstStart(int st);
  int getDrawInterval() const;
  void setDrawInterval(int st);
  int getDrawVacant() const;
  void setDrawVacant(int st);
  int getDrawNumReserved() const;
  void setDrawNumReserved(int st);

  enum class DrawSpecified {
    FixedTime = 1, Vacant = 2, Extra = 4, FixedInterval = 8,
    Late = 16, Early = 32, Fast = 64, Slow = 128
  };

  void adjustNumVacant(int leg, int numVacant);
  void setDrawSpecification(const vector<DrawSpecified> &ds);
  set<DrawSpecified> getDrawSpecification() const;

  int getLinearIndex(int index, bool isLinear) const;

  void splitClass(ClassSplitMethod method, const vector<int> &parts, vector<int> &outClassId);
  void mergeClass(int classIdSec);

  void drawSeeded(ClassSeedMethod seed, int leg, int firstStart, int interval,
                  const vector<int> &groups, bool noClubNb, bool reverse, int pairSize);

  bool hasClassGlobalDependence() const;

  static void extractBibPatterns(oEvent &oe, map<int, pair<wstring, int>> &patterns);
  pair<int, wstring> getNextBib(map<int, pair<wstring, int>> &patterns);
  pair<int, wstring> getNextBib();

  bool usesCourse(const oCourse &crs) const;
  int getNumForks() const;
  bool checkStartMethod();
  static int extractBibPattern(const wstring &bibInfo, wchar_t pattern[32]);

  bool isParallel(size_t leg) const {
    return leg < legInfo.size() ? legInfo[leg].isParallel() : false;
  }
  bool isOptional(size_t leg) const {
    return leg < legInfo.size() ? legInfo[leg].isOptional() : false;
  }

  pClass getVirtualClass(int instance, bool allowCreation);
  const pClass getVirtualClass(int instance) const;

  ClassStatus getClassStatus() const;
  static void fillClassStatus(vector<pair<wstring, wstring>> &statusClass);

  ClassMetaType interpretClassType() const;
  int getMaximumRunnerTime() const;

  void remove();
  bool canRemove() const;

  void forceShowMultiDialog(bool force) { tShowMultiDialog = force; }

  void getStartRange(int leg, int &firstStart, int &lastStart) const;

  bool isRogaining() const;

  int getLegPlace(int from, int to, int time) const;
  int getAccLegPlace(int courseId, int controlNo, int time) const;

  int getSortIndex() const { return tSortIndex; }

  void assignTypeFromName();
  bool isSingleRunnerMultiStage() const;

  bool wasSQLChanged(int leg, int control) const;

  void getStatistics(const set<int> &feeLock, int &entries, int &started) const;

  int getBestInputTime(AllowRecompute recompute, int leg) const;
  int getBestLegTime(AllowRecompute recompute, int leg, bool computedTime) const;
  int getBestTimeCourse(AllowRecompute recompute, int courseId) const;
  int getTotalLegLeaderTime(AllowRecompute recompute, int leg, bool computedTime, bool includeInput) const;

  wstring getInfo() const;

  bool hasCoursePool() const;
  void setCoursePool(bool p);
  pCourse selectCourseFromPool(int leg, const SICard &card) const;
  void updateChangedCoursePool();

  void resetLeaderTime() const;

  ClassType getClassType() const;

  bool startdataIgnored(int i) const;
  bool restartIgnored(int i) const;

  StartTypes getStartType(int leg) const;
  LegTypes getLegType(int leg) const;
  int getStartData(int leg) const;
  int getRestartTime(int leg) const;
  int getRopeTime(int leg) const;
  wstring getStartDataS(int leg) const;
  wstring getRestartTimeS(int leg) const;
  wstring getRopeTimeS(int leg) const;

  int getLegRunner(int leg) const;
  int getLegRunnerIndex(int leg) const;
  void setLegRunner(int leg, int runnerNo);
  int getNumMultiRunners(int leg) const;
  int getNumLegNoParallel() const;
  /// Total number of legs (including parallel).
  int getNumLegs() const { return (int)legInfo.size(); }

  bool splitLegNumberParallel(int leg, int &legNumber, int &legOrder) const;
  int getLegNumberLinear(int legNumber, int legOrder) const;
  int getNumParallel(int leg) const;
  int getNextBaseLeg(int leg) const;
  int getPreceedingLeg(int leg) const;
  int getResultDefining(int leg) const;

  wstring getLegNumber(int leg) const;
  int getNumDistinctRunners() const;
  int getNumDistinctRunnersMinimal() const;

  void setStartType(int leg, StartTypes st, bool noThrow);
  void setLegType(int leg, LegTypes lt);
  bool setStartData(int leg, const wstring &s);
  bool setStartData(int leg, int value);
  void setRestartTime(int leg, const wstring &t);
  void setRopeTime(int leg, const wstring &t);

  void setNoTiming(bool noResult);
  bool getNoTiming() const;

  void setIgnoreStartPunch(bool ignoreStartPunch);
  bool ignoreStartPunch() const;
  void updatedIgnoreStartPunch();

  void setFreeStart(bool freeStart);
  bool hasFreeStart() const;
  void setRequestStart(bool freeStart);
  bool hasRequestStart() const;

  void setDirectResult(bool directResult);
  bool hasDirectResult() const;

  bool isValidLeg(int legIndex) const {
    return legIndex == -1 || legIndex == 0 || (legIndex > 0 && legIndex < (int)MultiCourse.size());
  }
  bool isCourseUsed(int Id) const;
  wstring getLength(int leg) const;

  bool hasMultiCourse() const { return !MultiCourse.empty(); }
  bool hasTrueMultiCourse() const;

  unsigned getNumStages() const { return (unsigned)MultiCourse.size(); }

  struct TrueLegInfo {
  protected:
    TrueLegInfo(int first_, int second_) : first(first_), second(second_), nonOptional(-1) {}
    friend class oClass;
  public:
    int first;
    int second;
    int nonOptional;
  };

  void getTrueStages(vector<TrueLegInfo> &stages) const;

  unsigned getLastStageIndex() const {
    return (unsigned)std::max<int>((int)MultiCourse.size(), 1) - 1;
  }
  void setNumStages(int no);

  bool operator<(const oClass &b) {
    return tSortIndex < b.tSortIndex || (tSortIndex == b.tSortIndex && Id < b.Id);
  }

  int getNumRunners(bool checkFirstLeg, bool noCountVacant, bool noCountNotCompeting) const;
  void getNumResults(int leg, int &total, int &finished, int &dns) const;

  int getNumRemainingMaps(bool forceRecalculate) const;
  void setNumberMaps(int nm);
  int getNumberMaps(bool rawAttribute = false) const;

  const wstring &getName() const { return Name; }
  void setName(const wstring &name, bool manualSet);

  const wstring& getLongName() const;
  void setLongName(const wstring& name);

  void Set(const xmlobject *xo);
  bool Write(xmlparser &xml);

  // GUI-coupled methods — stubbed (Table/gdioutput)
  bool fillStageCourses(gdioutput &gdi, int stage, const string &name) const;
  static void fillStartTypes(gdioutput &gdi, const string &name, bool firstLeg);
  static void fillLegTypes(gdioutput &gdi, const string &name);

  pCourse getCourse(bool getSampleFromRunner = false) const;
  void getCourses(int leg, vector<pCourse> &courses) const;
  bool isForked(int leg) const;
  pCourse getCourse(int leg, unsigned fork = 0, bool getSampleFromRunner = false) const;
  int getCourseId() const { return Course ? Course->getId() : 0; }
  void setCourse(pCourse c);

  bool addStageCourse(int stage, int courseId, int index);
  bool addStageCourse(int stage, pCourse pc, int index);
  void clearStageCourses(int stage);
  bool moveStageCourse(int stage, int index, int offset);
  bool removeStageCourse(int stage, int courseId, int position);

  void getAgeLimit(int &low, int &high) const;
  void setAgeLimit(int low, int high);
  int getExpectedAge() const;

  PersonSex getSex() const;
  void setSex(PersonSex sex);

  const wstring &getStart() const;
  void setStart(const wstring &start);

  int getBlock() const;
  void setBlock(int block);

  bool getAllowQuickEntry() const;
  void setAllowQuickEntry(bool quick);

  AutoBibType getAutoBibType() const;
  BibMode getBibMode() const;
  void setBibMode(BibMode bibMode);

  wstring getType() const;
  void setType(const wstring &type);

  void addClassDefaultFee(bool resetFee);
  int getEntryFee(const wstring &date, int age) const;
  vector<pair<wstring, size_t>> getAllFees() const;

  void clearCache(bool recalculate);

  bool checkForking(vector<vector<int>> &legOrder, vector<vector<int>> &forks,
                    set<pair<int, int>> &unfairLegs) const;
  pair<int, int> autoForking(const vector<vector<int>> &inputCourses, int numToGenerateMax);

  bool hasUnorderedLegs() const;
  void setUnorderedLegs(bool order);
  void getParallelCourseGroup(int leg, int startNo, vector<pair<int, pCourse>> &group) const;
  pCourse selectParallelCourse(const oRunner &r, const SICard &sic);
  void getParallelRange(int leg, int &parLegRangeMin, int &parLegRangeMax) const;
  void getParallelOptionalRange(int leg, int &parLegRangeMin, int &parLegRangeMax) const;

  bool hasAnyCourse(const set<int> &crsId) const;

  bool isSingleStageOnly() const;
  void setSingleStageOnly(bool singleStageOnly);

  GeneralResult *getResultModule() const;
  void setResultModule(const string &tag);
  const string &getResultModuleTag() const;

  void merge(const oBase &input, const oBase *base) final;

  oClass(oEvent *poe);
  oClass(oEvent *poe, int id);
  virtual ~oClass();
  void clearDuplicate();

  friend class oAbstractRunner;
  friend class oEvent;
  friend class oRunner;
  friend class oTeam;
  friend class MeosSQL;
};

static const oClass::DrawSpecified DrawKeys[8] = {
  oClass::DrawSpecified::FixedTime,
  oClass::DrawSpecified::Vacant,
  oClass::DrawSpecified::Extra,
  oClass::DrawSpecified::FixedInterval,
  oClass::DrawSpecified::Early,
  oClass::DrawSpecified::Late,
  oClass::DrawSpecified::Fast,
  oClass::DrawSpecified::Slow
};

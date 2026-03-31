// oTeam.h — Full oTeam declaration (US-003h).
// Cross-platform, no Win32 / GUI dependencies.
#pragma once

#include "oAbstractRunner.h"
#include "oRunner.h"
#include <set>

class xmlparser;
class xmlobject;
class oSpeakerObject;

const unsigned int maxRunnersTeam = 32;

class oTeam final : public oAbstractRunner {
public:
  enum ResultCalcCacheSymbol {
    RCCCourse,
    RCCSplitTime,
    RCCCardTimes,
    RCCCardPunches,
    RCCCardControls,
    RCCLast
  };

private:
  int getLegRunningTimeUnadjusted(int leg, bool multidayTotal, bool useComputedRunnerTime) const;
  int getLegRestingTime(int leg, bool useComputedRunnerTime) const;

  void speakerLegInfo(int leg, int specifiedLeg, int courseControlId,
                      int &missingLeg, int &totalLeg,
                      RunnerStatus &status, int &runningTime) const;
  void propagateClub();

protected:
  vector<pRunner> Runners;
  void setRunnerInternal(int k, pRunner r);

  // On Linux wchar_t=4 bytes; scale buffer accordingly.
  static constexpr int dataSize = 256 * static_cast<int>(sizeof(wchar_t)) + 64;
  int getDISize() const final { return dataSize; }
  alignas(sizeof(wchar_t)) BYTE oData[dataSize];
  alignas(sizeof(wchar_t)) BYTE oDataOld[dataSize];
  vector<vector<wstring>> oDataStr;

  void correctRemove(pRunner r);
  void changeId(int newId);

  struct TeamPlace {
    DynamicValue p;
    DynamicValue totalP;
  };

  mutable vector<TeamPlace> tPlace;
  TeamPlace &getTeamPlace(int leg) const;

  struct ComputedLegResult {
    int version = -1;
    int time = 0;
    RunnerStatus status = StatusUnknown;
    ComputedLegResult() {}
  };

  mutable vector<ComputedLegResult> tComputedResults;

  void setTmpTime(int t) const { tmpSortTime = tmpDefinedTime = t; }
  mutable int tmpSortTime   = 0;
  mutable int tmpDefinedTime = 0;
  mutable int tmpSortStatus  = 0;
  mutable RunnerStatus tmpCachedStatus = StatusUnknown;

  mutable vector< vector< vector<int> > > resultCalculationCache;

  struct RogainingResult {
    int points    = 0;
    int reduction = 0;
    int overtime  = 0;
    void reset() { points = reduction = overtime = 0; }
  };
  mutable pair<int, RogainingResult> tTeamPatrolRogainingAndVersion;

  const ComputedLegResult &getComputedResult(int leg) const;
  void setComputedResult(int leg, ComputedLegResult &comp) const;

  string getRunners() const;
  bool matchTeam(int number, const wchar_t *s_lc) const;
  int tNumRestarts = 0;

  int getLegToUse(int leg) const;

  void addTableRow(Table &table) const;

  pair<int, bool> inputData(int id, const wstring &input,
                            int inputId, wstring &output, bool noUpdate) override;
  void fillInput(int id, vector< pair<wstring, size_t> > &out, size_t &selected) override;

  oDataContainer &getDataBuffers(pvoid &data, pvoid &olddata, pvectorstr &strData) const override;

  void fillInSortData(SortOrder so, int leg, bool linearLeg,
                      map<int, int> &classId2Linear, bool &hasRunner) const {}

  void changedObject() final;

public:
  DynamicRunnerStatus getDynamicStatus() const final;

  bool matchAbstractRunner(const oAbstractRunner* target) const override;

  RunnerStatus deduceComputedStatus() const;
  int deduceComputedRunningTime() const;
  int deduceComputedPoints() const;

  const pair<wstring, int> getRaceInfo() override;

  static const shared_ptr<Table> &getTable(oEvent *oe);

  bool checkValdParSetup();

  int getRanking() const override;

  int classInstance() const override { return 0; }

  void resetResultCalcCache() const;
  vector< vector<int> > &getResultCache(ResultCalcCacheSymbol symb) const;
  void setResultCache(ResultCalcCacheSymbol symb, int leg, vector<int> &data) const;
  void markClassChanged(int controlId) override;

  void setClub(const wstring &name) override;
  pClub setClubId(int clubId) override;

  int getTeamFee() const;

  void removeRunner(gdioutput &gdi, bool askRemoveRunner, int runnerIx);

  wstring getEntryDate(bool dummy) const;

  cTeam getTeam() const override { return this; }
  pTeam getTeam()       override { return this; }

  int getRunningTime(bool computedTime) const override;

  void setInputData(const oTeam &t);

  void remove() override;
  bool canRemove() const override;

  void prepareRemove();

  bool skip() const { return isRemoved(); }

  void setTeamMemberStatus(RunnerStatus memberStatus);
  void apply(ChangeType ct, pRunner source) override;

  void applyBibs();

  void quickApply();

  void evaluate(ChangeType changeType);

  void adjustMultiRunners();

  int getRogainingPoints(bool computed, bool multidayTotal) const override;
  int getRogainingReduction(bool computed) const override;
  int getRogainingOvertime(bool computed) const override;
  int getRogainingPointsGross(bool computed) const override;

  int getRogainingPatrolPoints(bool multidayTotal) const;
  int getRogainingPatrolReduction() const;
  int getRogainingPatrolOvertime() const;

  void fillSpeakerObject(int leg, int previousControlCourseId,
                         const vector<int> &courseControlIds,
                         bool totalResult, oSpeakerObject &spk) const;

  bool isRunnerUsed(int Id) const;
  void setRunner(unsigned i, pRunner r, bool syncRunner);

  pRunner getRunner(unsigned leg) const;
  int getNumRunners() const { return (int)Runners.size(); }
  int getNumAssignedRunners() const {
    int cnt = 0;
    for (auto &r : Runners) if (r) cnt++;
    return cnt;
  }

  pRunner getRunnerBestTimePar(int linearLegInput) const;

  void decodeRunners(const string &rns, vector<int> &rid);
  void importRunners(const vector<int> &rns);
  void importRunners(const vector<pRunner> &rns);

  RunnerStatus getStatusComputed(bool allowUpdate) const final;

  int getPlace(bool allowUpdate = true) const override { return getLegPlace(-1, false, allowUpdate); }
  int getTotalPlace(bool allowUpdate = true) const override { return getLegPlace(-1, true, allowUpdate); }

  int getNumShortening() const override;
  int getNumShortening(int leg) const;

  wstring getDisplayName() const;
  wstring getDisplayClub() const;

  void setBib(const wstring &bib, int numericalBib, bool updateStartNo) override;

  int getLegStartTime(int leg) const;
  wstring getLegStartTimeS(int leg) const;
  wstring getLegStartTimeCompact(int leg) const;

  wstring getLegFinishTimeS(int leg, SubSecond mode) const;
  int getLegFinishTime(int leg) const;

  int getTimeAfter(int leg, bool allowUpdate) const override;

  wstring getLegRunningTimeS(int leg, bool computed, bool multidayTotal, SubSecond mode) const;
  int getLegRunningTime(int leg, bool computed, bool multidayTotal) const;

  int getTotalRunningTimeAtLegStart(int leg, bool multidayTotal) const;

  RunnerStatus getLegStatus(int leg, bool computed, bool multidayTotal) const;
  const wstring &getLegStatusS(int leg, bool computed, bool multidayTotal) const;

  wstring getLegPlaceS(int leg, bool multidayTotal) const;
  wstring getLegPrintPlaceS(int leg, bool multidayTotal, bool withDot) const;
  int getLegPlace(int leg, bool multidayTotal, bool allowUpdate = true) const;

  bool isResultUpdated(bool totalResult) const override;

  static bool compareSNO(const oTeam &a, const oTeam &b);
  static bool compareName(const oTeam &a, const oTeam &b) { return a.sName < b.sName; }
  static bool compareResult(const oTeam &a, const oTeam &b);
  static bool compareResultNoSno(const oTeam &a, const oTeam &b);
  static bool compareResultClub(const oTeam& a, const oTeam& b);

  template<SortOrder so>
  static bool compareGeneral(const oTeam& a, const oTeam& b);

  static void checkClassesWithReferences(oEvent &oe, std::set<int> &clsWithRef);
  static void convertClassWithReferenceToPatrol(oEvent &oe, const std::set<int> &clsWithRef);

  void set(const xmlobject &xo);
  bool write(xmlparser &xml);

  void merge(const oBase &input, const oBase *base) final;

  bool isTeam() const final { return true; }
  wstring getInfo() const override { return L"Team " + sName; }

  // ── Name interface ──────────────────────────────────────────────────────────
  void setName(const wstring &n, bool manualUpdate) override;
  const wstring& getName() const override { return sName; }
  wstring getNameAndRace(bool withRace) const override {
    if (!withRace || tDuplicateLeg == 0) return sName;
    return sName + L" (" + itow(tDuplicateLeg + 1) + L")";
  }

  // ── Race / leg ───────────────────────────────────────────────────────────────
  int getRaceNo() const override { return 0; }

  // ── setStartNo override ───────────────────────────────────────────────────────
  void setStartNo(int no, ChangeType ct) override;

  // ── Class ─────────────────────────────────────────────────────────────────────
  void setClassId(int id, bool isManualUpdate) override;
  const wstring& getClass(bool /*virtualClass*/) const override {
    static const wstring empty;
    return Class ? Class->getName() : empty;
  }

  oTeam(oEvent *poe, int id);
  oTeam(oEvent *poe);
  ~oTeam() override;

  // ── Total status / place ──────────────────────────────────────────────────────
  using oAbstractRunner::getTotalStatus;  // unhide the no-arg overload
  RunnerStatus getTotalStatus(bool computed) const override {
    return getStatusComputed(computed);
  }

  // ── GUI/table stubs ───────────────────────────────────────────────────────────
  void printSplits(gdioutput &, const oListInfo *) const {}
  void printStartInfo(gdioutput &, bool) const {}

  // ── Persistence accessors ─────────────────────────────────────────────────────
  const BYTE* getOData() const { return oData; }
  BYTE*       getOData()       { return oData; }
  static int  getODataBlobSize() { return dataSize; }
  /// Semicolon-separated runner IDs (matches decodeRunners format).
  std::string getRunnerIdString() const { return getRunners(); }

private:
  int tDuplicateLeg = 0; // always 0 for teams

  friend class oClass;
  friend class oRunner;
  friend class oEvent;
};

typedef list<oTeam> oTeamList;

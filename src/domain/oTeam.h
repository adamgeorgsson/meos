#pragma once

#include "oAbstractRunner.h"
#include "oRunner.h"
#include <vector>
#include <string>

// pRunner/pTeam/cTeam are declared in oAbstractRunner.h

static constexpr unsigned maxRunnersTeam = 32;

class oTeam final : public oAbstractRunner {
public:
  // Result calculation cache symbols
  enum ResultCalcCacheSymbol {
    RCCCourse, RCCSplitTime, RCCCardTimes, RCCCardPunches, RCCCardControls, RCCLast
  };

protected:
  std::vector<pRunner> Runners;

  void setRunnerInternal(int k, pRunner r);
  void correctRemove(pRunner r);
  void propagateClub();

  // Per-leg place cache
  struct TeamPlace {
    DynamicValue p;
    DynamicValue totalP;
  };
  mutable std::vector<TeamPlace> tPlace;
  TeamPlace& getTeamPlace(int leg) const;

  // Per-leg computed result cache
  struct ComputedLegResult {
    int version = -1;
    int time = 0;
    RunnerStatus status = StatusUnknown;
  };
  mutable std::vector<ComputedLegResult> tComputedResults;
  const ComputedLegResult& getComputedResult(int leg) const;
  void setComputedResult(int leg, ComputedLegResult& comp) const;

  mutable int tmpSortTime = 0;
  mutable int tmpDefinedTime = 0;
  mutable int tmpSortStatus = 0;
  mutable RunnerStatus tmpCachedStatus = StatusUnknown;

  int tNumRestarts = 0;

  // Multi-dimensional result calculation cache: [symbol][leg][value]
  mutable std::vector<std::vector<std::vector<int>>> resultCalculationCache;

  // Rogaining result cache
  struct RogainingResult {
    int points = 0, reduction = 0, overtime = 0;
    void reset() { points = reduction = overtime = 0; }
  };
  mutable std::pair<int, RogainingResult> tTeamPatrolRogainingAndVersion;

  int getLegRunningTimeUnadjusted(int leg, bool multidayTotal, bool useComputedRunnerTime) const;
  int getLegRestingTime(int leg, bool useComputedRunnerTime) const;
  int getLegToUse(int leg) const;

  void changedObject() final;
  int getBuiltinAdjustment() const override { return 0; }

  oDataContainer& getDataBuffers(pvoid& d, pvoid& o, pvectorstr& s) const override;
  int getDISize() const override { return 0; }

public:
  // Expose Runners for tests (read-only via getRunner)
  std::vector<pRunner>& getRunnersRef() { return Runners; }

  // Constructors / destructor
  explicit oTeam(oEvent* poe);
  oTeam(oEvent* poe, int id);
  ~oTeam() override = default;

  // oBase virtuals
  std::wstring getInfo() const override { return sName; }
  void remove() override;
  bool canRemove() const override;
  void merge(const oBase& input, const oBase* base) override;

  // oAbstractRunner pure virtuals
  cTeam getTeam() const override { return this; }
  pTeam getTeam() override { return this; }
  bool isTeam() const final { return true; }

  bool matchAbstractRunner(const oAbstractRunner* target) const override;
  bool isResultUpdated(bool totalResult) const override;
  int getNumShortening() const override;
  int getNumShortening(int leg) const;
  void markClassChanged(int controlId) override;
  void apply(ChangeType ct, pRunner src) override;
  int getTimeAfter(int leg, bool allowUpdate) const override;
  void fillSpeakerObject(int leg, int previousControlCourseId,
                         const std::vector<int>& controlIds,
                         bool totalResult, oSpeakerObject& spk) const override {}
  void setBib(const std::wstring& bib, int numericalBib, bool updateStartNo) override;
  int getRogainingPoints(bool computed, bool multidayTotal) const override;
  int getRogainingReduction(bool computed) const override;
  int getRogainingOvertime(bool computed) const override;
  int getRogainingPointsGross(bool computed) const override;
  RunnerStatus getStatusComputed(bool allowUpdate) const final;
  DynamicRunnerStatus getDynamicStatus() const final;
  int getPlace(bool allowUpdate = true) const override { return getLegPlace(-1, false, allowUpdate); }
  int getTotalPlace(bool allowUpdate = true) const override { return getLegPlace(-1, true, allowUpdate); }
  std::wstring getEntryDate(bool useTeamEntryDate = true) const override;
  const std::pair<std::wstring, int> getRaceInfo() override;
  int getRanking() const override;
  int classInstance() const override { return 0; }

  // Unhide oAbstractRunner::getTotalStatus (oTeam doesn't override it;
  // without this, getTotalStatus from getLegStatus lookup would shadow it)
  using oAbstractRunner::getTotalStatus;

  // Club / class
  void setClub(const std::wstring& name) override;
  pClub setClubId(int clubId) override;

  // Runner management
  void setRunner(unsigned i, pRunner r, bool sync);
  pRunner getRunner(unsigned leg) const;
  int getNumRunners() const { return (int)Runners.size(); }
  int getNumAssignedRunners() const {
    int cnt = 0; for (auto& r : Runners) if (r) cnt++; return cnt;
  }
  int getNumDistinctRunners() const;
  bool isRunnerUsed(int runnerId) const;
  void importRunners(const std::vector<int>& rids);
  void importRunners(const std::vector<pRunner>& rns);
  void decodeRunners(const std::string& rns, std::vector<int>& rid) const;
  std::string getRunners() const;
  std::wstring getRunnerIdString() const;
  bool isTeamMemberFor(const pRunner r) const;

  // Batch deductions
  RunnerStatus deduceComputedStatus() const;
  int deduceComputedRunningTime() const;
  int deduceComputedPoints() const;

  // Per-leg accessors
  int getLegStartTime(int leg) const;
  std::wstring getLegStartTimeS(int leg) const;
  int getLegFinishTime(int leg) const;
  std::wstring getLegFinishTimeS(int leg, SubSecond mode) const;
  int getLegRunningTime(int leg, bool computed, bool multidayTotal) const;
  std::wstring getLegRunningTimeS(int leg, bool computed, bool multidayTotal, SubSecond mode) const;
  int getTotalRunningTimeAtLegStart(int leg, bool multidayTotal) const;
  RunnerStatus getLegStatus(int leg, bool computed, bool multidayTotal) const;
  const std::wstring& getLegStatusS(int leg, bool computed, bool multidayTotal) const;
  int getLegPlace(int leg, bool multidayTotal, bool allowUpdate = true) const;
  std::wstring getLegPlaceS(int leg, bool multidayTotal) const;
  std::wstring getLegPrintPlaceS(int leg, bool multidayTotal, bool withDot) const;

  // Result cache
  void resetResultCalcCache() const;
  std::vector<std::vector<int>>& getResultCache(ResultCalcCacheSymbol symb) const;
  void setResultCache(ResultCalcCacheSymbol symb, int leg, std::vector<int>& data) const;

  // Apply bib to team members
  void applyBibs();

  // Evaluate all runners' cards and propagate start times
  void evaluate(ChangeType changeType);

  // Quick apply: link runners to team without computing start times
  void quickApply();

  // Set DNS/CANCEL/NotCompeting status on all team members
  void setTeamMemberStatus(RunnerStatus memberStatus);

  // Multi-day input data
  void setInputData(const oTeam& t);

  bool skip() const { return isRemoved(); }

  // Running time overrides
  int getRunningTime(bool computedTime) const override;
  int getTotalRunningTime() const override;

  // Static comparison helpers
  static bool compareSNO(const oTeam& a, const oTeam& b);
  static bool compareName(const oTeam& a, const oTeam& b) { return a.sName < b.sName; }
  static bool compareResult(const oTeam& a, const oTeam& b);
  static bool compareResultNoSno(const oTeam& a, const oTeam& b);
  static bool compareResultClub(const oTeam& a, const oTeam& b);

  static oDataContainer& container();

  friend class oRunner;
  friend class oClass;
  friend class oEvent;

private:
  mutable DataMap dataMap_;
  mutable DataMap oldDataMap_;
};


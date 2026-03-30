// oRunner.h — oRunner full declaration (US-003g).
// Cross-platform, no Win32 / GUI dependencies.
#pragma once

#include "oAbstractRunner.h"
#include "oCard.h"

class RunnerWDBEntry;
class oListInfo;
class xmlparser;
class xmlobject;

// ── SplitData ─────────────────────────────────────────────────────────────────
class SplitData {
public:
  enum class SplitStatus { Missing, OK, NoTime };

private:
  int time;
  SplitStatus st;

public:
  SplitData() : time(-1), st(SplitStatus::Missing) {}
  SplitData(int t, SplitStatus s) : time(t), st(s) {}

  bool hasTime() const { return st == SplitStatus::OK; }
  int  getTime(bool /*adjusted*/) const { return time; }
  SplitStatus getStatus() const { return st; }
  bool isMissing() const { return st == SplitStatus::Missing; }
};

// ── oRunner ───────────────────────────────────────────────────────────────────
class oRunner final : public oAbstractRunner {
public:
  // ── Data buffer size (used by oEvent constructor) ─────────────────────────
  // On Linux wchar_t = 4 bytes.
  static constexpr int dataSize = 512 * static_cast<int>(sizeof(wchar_t)) + 64;

public:
  // ── Multi-leg / relay state (public for oTeam/test access) ───────────────
  pTeam  tInTeam       = nullptr;  // Team this runner belongs to
  int    tLeg          = 0;        // Leg index within team

protected:
  // ── Core identity ─────────────────────────────────────────────────────────
  wstring tRealName;      // Normalized display name (whitespace-collapsed)
  int     cardNumber = 0;
  pCard   Card       = nullptr;

  // ── Course / class assignment ─────────────────────────────────────────────
  pCourse Course = nullptr;

  // ── Split times ──────────────────────────────────────────────────────────
  vector<SplitData> splitTimes;
  vector<SplitData> normalizedSplitTimes;

  // ── Rogaining ────────────────────────────────────────────────────────────
  int tRogainingPoints      = 0;
  int tRogainingPointsGross = 0;
  int tRogainingOvertime    = 0;
  int tReduction            = 0;
  wstring tProblemDescription;

  // ── Multi-leg / relay state (continued) ──────────────────────────────────
  int    tLegEquClass  = 0;        // Equivalent-class leg index (parallel)
  bool   tNeedNoCard   = false;    // Runner needs no SI card (e.g. LTIgnore)
  int    speakerPriority = 0;      // Priority for speaker display
  pRunner tParentRunner = nullptr; // Parent runner for multi-leg duplicates
  vector<pRunner> multiRunner;     // Additional leg duplicates owned by this runner
  int    tDuplicateLeg = 0;        // Relay leg index
  int    tNumShortening = 0;
  bool   tUseStartPunch = true;

  // ── Data buffers ──────────────────────────────────────────────────────────
  alignas(sizeof(wchar_t)) BYTE oData[dataSize];
  alignas(sizeof(wchar_t)) BYTE oDataOld[dataSize];
  vector<vector<wstring>> oDataStr;

  int getDISize() const final { return dataSize; }

  oDataContainer& getDataBuffers(pvoid& data, pvoid& olddata, pvectorstr& strData) const override;

  void changedObject() override;

public:
  // ── Constructors / destructor ─────────────────────────────────────────────
  explicit oRunner(oEvent* poe);
  oRunner(oEvent* poe, int id);
  ~oRunner() override;

  // ── wstring getInfo ───────────────────────────────────────────────────────
  wstring getInfo() const override { return L"Runner " + tRealName; }

  // ── Name ──────────────────────────────────────────────────────────────────
  void           setName(const wstring& n, bool manualUpdate) override;
  const wstring& getName() const override;
  wstring        getNameAndRace(bool withRace) const override;

  // ── Club ──────────────────────────────────────────────────────────────────
  void  setClub(const wstring& name) override;
  pClub setClubId(int id) override;

  // ── Class ─────────────────────────────────────────────────────────────────
  void setClassId(int id, bool isManualUpdate) override;
  const wstring& getClass(bool virtualClass) const override;

  // ── Card ──────────────────────────────────────────────────────────────────
  pCard  getCard() const { return Card; }
  int  getCardNo() const { return cardNumber; }
  void setCardNo(int cno, bool matchCard, bool updateFromDatabase);
  int  setCard(int cardId);
  void addCard(pCard card, vector<pair<int, pControl>>& missingPunches);
  bool evaluateCard(bool doApply, vector<pair<int, pControl>>& missingPunches,
                    int addPunch, ChangeType ct);

  // ── Course ────────────────────────────────────────────────────────────────
  pCourse getCourse(bool getAdapted) const;
  pCourse getCourse(bool getAdapted, bool allowVirtual) const { return getCourse(getAdapted); }

  // ── Split times ──────────────────────────────────────────────────────────
  const vector<SplitData>& getSplitTimes(bool normalized) const;

  // ── Status ────────────────────────────────────────────────────────────────
  RunnerStatus getStatusComputed(bool allowUpdate) const override;

  // ── Team (relay) ─────────────────────────────────────────────────────────
  cTeam getTeam() const override { return tInTeam; }
  pTeam getTeam()       override { return tInTeam; }

  // ── Relay helper methods (stubs — full impl when relay is tested) ──────────
  pRunner getMultiRunner(int index) {
    return (index >= 0 && size_t(index) < multiRunner.size()) ? multiRunner[index] : nullptr;
  }
  void createMultiRunner(bool /*visible*/, bool /*sync*/) {}
  bool prelStatusOK(bool computed, bool /*extra*/, bool /*relaxed*/) const {
    RunnerStatus s = computed ? tComputedStatus : tStatus;
    if (s == StatusUnknown) s = tStatus;
    return s == StatusOK || s == StatusOutOfCompetition || s == StatusNoTiming;
  }
  int getFinishTimeAdjusted(bool /*adj*/) const { return FinishTime; }
  int getPrelRunningTime() const {
    if (FinishTime > 0 && tStartTime > 0) return FinishTime - tStartTime;
    return 0;
  }
  wstring getStartTimeCompact() const;
  wstring getUIName() const { return getName(); }
  int  getLegNumber() const { return tLeg; }
  int  getCourseId() const { return Course ? Course->getId() : 0; }
  void setCourseId(int id);
  bool statusOK(bool computed, bool /*checkTeam*/) const { return prelStatusOK(computed, false, false); }
  bool preventRestart() const { return tPreventRestartCache; }
  void markForCorrection() { correctionNeeded = true; }
  // Result-at-control temp status/time (set by calculateSplitResults)
  RunnerStatus getTempStatus() const { return tmpResult.status; }
  int getTempTime()   const { return tmpResult.runningTime; }
  void getSplitTime(int /*courseControlId*/, RunnerStatus& st, int& rt) const {
    st = StatusUnknown; rt = 0;
  }
  int getTimeAdjustment(bool /*preliminary*/) const { return tTimeAdjustment; }
  int getPointAdjustment() const { return tPointAdjustment; }

  // ── DynamicResult data stubs (needed for DynamicResult::prepareCalculations) ──
  void getSplitAnalysis(std::vector<int> &delta) const { delta.clear(); }
  void getLegTimeAfter(std::vector<int> &after) const { after.clear(); }
  void getLegPlaces(std::vector<int> &place) const { place.clear(); }
  int  getBirthYear() const; // implemented in oRunner.cpp (needs full DCI type)
  int  getBirthAge() const { return 0; }
  int  getCheckTime() const { return 0; }
  void updateComputedResultFromTemp() {}

  // ── Totals / relay ───────────────────────────────────────────────────────
  RunnerStatus getTotalStatus(bool computed) const override;
  int  getPlace(bool allowUpdate = true) const override;
  int  getTotalPlace(bool allowUpdate = true) const override;
  DynamicRunnerStatus getDynamicStatus() const override;

  // ── Race / leg identity ───────────────────────────────────────────────────
  int  getRaceNo() const override { return tDuplicateLeg; }
  int  classInstance() const override;

  // ── Rogaining ────────────────────────────────────────────────────────────
  int  getRogainingPoints(bool computed, bool total) const override;
  int  getRogainingPointsGross(bool computed) const override;
  int  getRogainingReduction(bool computed) const override;
  int  getRogainingOvertime(bool computed) const override;

  // ── Ranking / shortening ─────────────────────────────────────────────────
  int  getRanking()       const override;
  int  getNumShortening() const override;

  // ── Result state ─────────────────────────────────────────────────────────
  bool isResultUpdated(bool) const override;

  // ── Apply / synchronize ───────────────────────────────────────────────────
  void apply(ChangeType ct, pRunner src) override;
  int  getTimeAfter(int leg, bool allowUpdate) const override;
  void synchronize(bool writeOnly = false);
  bool synchronizeAll(bool writeOnly = false);

  // ── StartNo / bib ────────────────────────────────────────────────────────
  void setStartNo(int no, ChangeType ct) override;
  void setBib(const wstring& bib, int numericalBib, bool updateRace) override;

  // ── Flags ─────────────────────────────────────────────────────────────────
  bool matchAbstractRunner(const oAbstractRunner* target) const override;

  // ── Class changed notification ────────────────────────────────────────────
  void markClassChanged(int controlId) override;

  // ── Remove ────────────────────────────────────────────────────────────────
  void remove() override;
  bool canRemove() const override;

  // ── Merge ────────────────────────────────────────────────────────────────
  void merge(const oBase& input, const oBase* base) override;

  // ── Race info ────────────────────────────────────────────────────────────
  const pair<wstring, int> getRaceInfo() override;

  // ── Serialization ────────────────────────────────────────────────────────
  bool Write(xmlparser& xml);
  void Set(const xmlobject& xo);

  // ── Input data (DI row editing) ───────────────────────────────────────────
  pair<int, bool> inputData(int id, const wstring& input, int inputId,
                             wstring& output, bool noUpdate) override;
  void fillInput(int id, vector<pair<wstring, size_t>>& elements, size_t& selected) override;

  // ── GUI/output stubs ─────────────────────────────────────────────────────
  void printSplits(gdioutput& gdi, const oListInfo* li) const;
  void printStartInfo(gdioutput& gdi, bool addTeam) const;
  void fillSpeakerObject(int leg, int controlCourseId, int expectedFinishTime,
                          bool totalResult, oSpeakerObject& spk) const;
  void addTableRow(Table& table) const;
  static const shared_ptr<Table>& getTable(oEvent* oe);

  friend class oEvent;
  friend class oClass;
  friend class oCard;
  friend class oTeam;
  friend class MeosSQL;
};

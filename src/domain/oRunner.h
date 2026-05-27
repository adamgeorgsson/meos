#pragma once

#include "oAbstractRunner.h"
#include "oCard.h"
#include "oCourse.h"
#include <map>

// pRunner/cRunner declared in oAbstractRunner.h

struct SplitData {
  enum class SplitStatus { OK, Missing, NoTime };
private:
  int time = -1;
  int adjustment = 0;
  SplitStatus status = SplitStatus::Missing;
public:
  SplitData() {}
  SplitData(int t, SplitStatus s) : time(t), status(s) {}

  void setAdjustment(int a) {
    if (time > 0) time += a - adjustment;
    adjustment = a;
  }
  SplitStatus getStatus() const { return status; }
  int getTime(bool adjusted) const {
    return adjusted ? time : time - adjustment;
  }
  void setPunchTime(int t) { time = t; status = SplitStatus::OK; }
  void setPunched() { time = -1; status = SplitStatus::NoTime; }
  void setNotPunched() { time = -1; status = SplitStatus::Missing; }
  bool hasTime() const { return time > 0 && status == SplitStatus::OK; }
  bool isMissing() const { return status == SplitStatus::Missing; }

  friend class oRunner;
};

class oRunner final : public oAbstractRunner {
  friend class RunnerResultTestAccessor;
public:
  struct ResultData {
  private:
    int data = 0;
    int teamTotalData = 0;
  public:
    ResultData(int data, int teamTotalData) : data(data), teamTotalData(teamTotalData) {}
    ResultData() = default;
    int get(bool total) const { return total ? teamTotalData : data; }
    friend class oRunner;
  };

protected:
  pCourse Course = nullptr;
  int cardNumber = 0;
  pCard Card = nullptr;
  bool cardWasSet = false;

  std::vector<pRunner> multiRunner;
  std::vector<int> multiRunnerId;

  std::wstring tRealName;

  mutable DynamicValue tPlace;
  mutable DynamicValue tCoursePlace;
  mutable DynamicValue tCourseClassPlace;
  mutable DynamicValue tTotalPlace;
  mutable int tLegEquClass = 0;
  mutable pRunner tParentRunner = nullptr;
  mutable bool tNeedNoCard = false;
  mutable bool tUseStartPunch = true;
  mutable int tDuplicateLeg = 0;
  mutable int tNumShortening = 0;
  mutable int tShortenDataRevision = -1;

  RunnerStatus tempStatus = StatusUnknown;
  int tempRT = 0;

  bool isTemporaryObject = false;
  int tTimeAfter = 0;
  int tInitialTimeAfter = 0;
  int speakerPriority = 0;

  std::vector<int> adjustTimes;
  std::vector<SplitData> splitTimes;
  mutable std::vector<SplitData> normalizedSplitTimes;
  std::vector<int> tLegTimes;

  mutable int tSplitRevision = -1;
  int tCachedRunningTime = 0;
  mutable std::pair<int, int> classInstanceRev = {-1, -1};

  mutable std::vector<int> tMissedTime;
  mutable std::vector<int> tPlaceLeg;
  mutable std::vector<int> tAfterLeg;
  mutable std::vector<ResultData> tPlaceLegAcc;
  mutable std::vector<ResultData> tAfterLegAcc;

  struct OnCourseResult {
    OnCourseResult(int ccid, int cix, int t, int ttt)
      : courseControlId(ccid), controlIx(cix), time(t), teamTotalTime(ttt) {}
    int courseControlId, controlIx, time, teamTotalTime;
    int place = -1, after = -1, teamTotalPlace = -1, teamTotalAfter = -1;
  };
  mutable std::pair<int, int> currentControlTime = {0, 0};

  struct OnCourseResultCollection {
    bool hasAnyRes = false;
    std::vector<OnCourseResult> res;
    void clear() { hasAnyRes = false; res.clear(); }
    void emplace_back(int ccid, int cix, int t, int ttt) {
      res.emplace_back(ccid, cix, t, ttt); hasAnyRes = true;
    }
    bool empty() const { return !hasAnyRes; }
  };
  mutable OnCourseResultCollection tOnCourseResults;

  std::vector<std::pair<pControl, int>> tRogaining;
  int tRogainingPoints = 0;
  int tRogainingPointsGross = 0;
  int tReduction = 0;
  int tRogainingOvertime = 0;
  std::wstring tProblemDescription;

  mutable pCourse tAdaptedCourse = nullptr;
  mutable int tAdaptedCourseRevision = -1;

  int tmpStartGroup = 0;

  void changedObject() final;
  int getBuiltinAdjustment() const override;
  void propagateClub();
  bool isRentalCard(int card) const;
  pRunner getPredecessor() const;
  void markForCorrection() { correctionNeeded = true; }
  void correctRemove(pRunner r);
  std::vector<pRunner> getRunnersOrdered() const;
  int getMultiIndex();
  std::string codeMultiR() const;
  void decodeMultiR(const std::string& r);
  void clearOnChangedRunningTime();
  void changeId(int newId);
  void setupRunnerStatistics() const;
  bool storeTimes();
  bool storeTimesAux(pClass targetClass);
  void doAdjustTimes(pCourse course);

  oDataContainer& getDataBuffers(pvoid& data, pvoid& olddata, pvectorstr& strData) const override;
  int getDISize() const override { return 0; }

public:
  mutable int tLeg = 0;
  mutable pTeam tInTeam = nullptr;

  // Constructors / destructor
  oRunner(oEvent* poe);
  oRunner(oEvent* poe, int id);
  ~oRunner() override;

  // oBase virtuals
  std::wstring getInfo() const override;
  void remove() override;
  bool canRemove() const override;
  void merge(const oBase& input, const oBase* base) final;

  // oAbstractRunner pure virtuals
  cTeam getTeam() const override { return tInTeam; }
  pTeam getTeam() override { return tInTeam; }
  bool matchAbstractRunner(const oAbstractRunner* target) const override;
  bool isResultUpdated(bool totalResult) const override;
  bool startTimeAvailable() const;
  int getNumShortening() const override { return tNumShortening; }
  void markClassChanged(int controlId) override;
  void apply(ChangeType changeType, pRunner src) override;
  int getTimeAfter(int leg, bool allowUpdate) const override;
  void fillSpeakerObject(int leg, int previousControlCourseId,
                          const std::vector<int>& controlIds, bool totalResult,
                          oSpeakerObject& spk) const override;
  void setBib(const std::wstring& bib, int bibNumerical, bool updateStartNo) override;
  int getRogainingPoints(bool computed, bool multidayTotal) const override;
  int getRogainingReduction(bool computed) const override;
  int getRogainingOvertime(bool computed) const override;
  int getRogainingPointsGross(bool computed) const override;
  RunnerStatus getStatusComputed(bool allowUpdate) const final;
  DynamicRunnerStatus getDynamicStatus() const final;
  int getPlace(bool allowUpdate = true) const override;
  int getCoursePlace(bool perClass) const;
  int getTotalPlace(bool allowUpdate = true) const override;
  bool runnerHasResult() const final;
  std::wstring getEntryDate(bool useTeamEntryDate = true) const override;
  const std::pair<std::wstring, int> getRaceInfo() override;
  int getRanking() const final;
  int classInstance() const override;
  int getSpeakerPriority() const override { return speakerPriority; }

  // Name
  const std::wstring& getName() const override;
  void setName(const std::wstring& n, bool manualUpdate) override;
  const std::wstring& getNameLastFirst() const;
  void getRealName(const std::wstring& input, std::wstring& output) const;
  std::wstring getGivenName() const;
  std::wstring getFamilyName() const;
  std::wstring getCompactName() const;
  const std::wstring& getUIName() const;
  const std::wstring& getNameRaw() const { return sName; }
  bool matchName(const std::wstring& pname) const;

  // Club / class / course
  void setClub(const std::wstring& name) override;
  pClub setClubId(int clubId) override;
  void setClassId(int id, bool isManualUpdate) override;
  pCourse getCourse(bool getAdaptedCourse) const;
  const std::wstring& getCourseName() const;
  int getCourseId() const { return Course ? Course->Id : 0; }
  void setCourseId(int id);
  bool useCoursePool() const;

  // Card
  int getCardNo() const { return tParentRunner && cardNumber == 0 ? tParentRunner->cardNumber : cardNumber; }
  void setCardNo(int card, bool matchCard, bool updateFromDatabase = false);
  int setCard(int cardId);
  pCard getCard() const { return Card; }
  int getCardId() const { return Card ? Card->Id : 0; }
  bool needNoCard() const;
  bool isRentalCard() const;
  int getRentalCardFee(bool forAllRunners) const;
  void setRentalCard(bool rental);
  bool payBeforeResult(bool checkFlagOnly) const;
  void setPaid(int paid);
  void setFee(int fee);
  void setPayBeforeResult(bool flag);

  // Start / bib
  void setStartNo(int no, ChangeType changeType) override;
  void updateStartNo(int no);
  int getStartGroup(bool useTmpStartGroup) const;
  void setStartGroup(int sg);

  // Time
  void setFinishTime(int t) override;
  int getTimeAfter() const;
  int getTimeAfterCourse(bool considerClass) const;
  bool skip() const { return isRemoved() || tDuplicateLeg != 0; }

  // Multi-runner
  pRunner getMultiRunner(int race) const;
  int getNumMulti() const { return (int)multiRunner.size(); }
  void createMultiRunner(bool createMaster, bool sync);
  int getRaceNo() const { return tDuplicateLeg; }
  std::wstring getNameAndRace(bool useUIName) const;

  // Splits (stubs for US-003g2)
  const std::vector<SplitData>& getSplitTimes(bool normalized) const;
  void getSplitAnalysis(std::vector<int>& deltaTimes) const;
  void getLegPlaces(std::vector<int>& places) const;
  void getLegTimeAfter(std::vector<int>& deltaTimes) const;
  void getLegPlacesAcc(std::vector<ResultData>& places) const;
  void getLegTimeAfterAcc(std::vector<ResultData>& deltaTimes) const;
  int getSplitTime(int controlNumber, bool normalized) const;
  int getTimeAdjust(int controlNumber) const;
  int getNamedSplit(int controlNumber) const;
  const std::wstring& getNamedSplitS(int controlNumber, SubSecond mode) const;
  int getPunchTime(int controlIndex, bool normalized, bool adjusted, bool teamTotalTime) const;
  const std::wstring& getPunchTimeS(int controlIndex, bool normalized, bool adjusted, bool teamTotalTime, SubSecond mode) const;
  const std::wstring& getSplitTimeS(int controlNumber, bool normalized, SubSecond mode) const;
  void getSplitTime(int courseControlId, RunnerStatus& stat, int& rt) const;
  int getMissedTime() const;
  int getMissedTime(int ctrlNo) const;
  std::wstring getMissedTimeS() const;
  std::wstring getMissedTimeS(int ctrlNo) const;
  int getLegPlace(int ctrlNo) const;
  int getLegTimeAfter(int ctrlNo) const;
  int getLegPlaceAcc(int ctrlNo, bool teamTotal) const;
  int getLegTimeAfterAcc(int ctrlNo, bool teamTotal) const;
  int getTimeWhenPlaceFixed() const;
  int getTotalRunningTime(int time, bool computedTime, bool includeInput) const;
  int getTotalRunningTime() const override;
  int getRaceRunningTime(bool computedTime, int leg, bool allowUpdate) const;

  // Sex / birth
  void setSex(PersonSex sex);
  PersonSex getSex() const;
  void setBirthYear(int year);
  void setBirthDate(const std::wstring& date);
  int getBirthYear() const;
  const std::wstring& getBirthDate() const;
  int getBirthAge() const override;
  void setNationality(const std::wstring& nat);
  std::wstring getNationality() const;

  // Ranking
  std::wstring getRankingScore() const;
  void setRankingScore(double score);

  // Input data
  void setInputData(const oRunner& source);
  bool isTransferCardNoNextStage() const;
  void setTransferCardNoNextStage(bool state);

  // Misc
  int getLegNumber() const { return tLeg; }
  RunnerStatus getTempStatus() const { return tempStatus; }
  int getTempTime() const { return tempRT; }
  bool isAnnonumousTeamMember() const;
  int getRaceIdentifier() const;
  int getParResultLeg() const;
  bool hasOnCourseResult() const { return !tOnCourseResults.empty() || getFinishTime() > 0 || hasResult(); }
  bool hasFinished() const;
  int getCheckTime() const;
  pRunner getReference() const;
  void setReference(int runnerId);
  bool canShareCard(const pRunner other, int newCardNumber) const;

  void setTemporary() { isTemporaryObject = true; }
  bool synchronizeAll(bool writeOnly = false);
  void cloneStartTime(const pRunner r);
  void cloneData(const pRunner r);
  void resetPersonalData();
  bool operator<(const oRunner& c) const;
  static bool CompareCardNumber(const oRunner& a, const oRunner& b) { return a.cardNumber < b.cardNumber; }

  enum class IDType { OnlyThis, ParallelLeg, ParallelLegExtra };
  enum class NameType { Default, Compact, CompactClub };
  enum class NameFormat { Default, FirstLast, LastFirst, Last, First, Init, InitLast };
  enum class BibAssignResult { Assigned, NoBib, Failed };

  std::wstring getCompleteIdentification(IDType type, NameType compactName = NameType::Default) const;
  std::wstring formatName(NameFormat style) const;
  static void getNameFormats(std::vector<std::pair<std::wstring, size_t>>& out);
  static constexpr int encodeNameFormst(NameFormat f) { return int(f); }
  static constexpr NameFormat decodeNameFormst(int f) { return NameFormat(f); }
  BibAssignResult autoAssignBib();

  oRunner* getMainRunner() { return tParentRunner ? tParentRunner : this; }
  const oRunner* getMainRunner() const { return tParentRunner ? tParentRunner : this; }

  pRunner nextNeedReadout() const;
  pRunner getMatchedRunner(const SICard& sic) const;

  const std::wstring& getProblemDescription() const { return tProblemDescription; }
  RunnerStatus getTotalStatus(bool allowUpdate = true) const override;

  int64_t getExtIdentifier2() const;
  void setExtIdentifier2(int64_t id);
  std::wstring getExtIdentifierString2() const;
  void setExtIdentifier2(const std::wstring& str);
  int getTotalTimeInput() const;
  RunnerStatus getTotalStatusInput() const;

  void setExtraPersonData(const std::wstring& sex, const std::wstring& nationality,
                           const std::wstring& rank, std::wstring& phone,
                           const std::wstring& bib, const std::wstring& text,
                           int dataA, int dataB);

  // evaluateCard — full implementation in US-003g2
  bool evaluateCard(bool applyTeam, std::vector<std::pair<int, pControl>>& missingPunches,
                    int addPunch, ChangeType changeType);
  void addCard(pCard card, std::vector<std::pair<int, pControl>>& missingPunches);

  oClass::RogainingAnalysis getRogainingAnalysis(int from, int to) const;

  friend class oCard;
  friend class oEvent;
  friend class oTeam;
  friend class oClass;

private:
  mutable DataMap dataMap_;
  mutable DataMap oldDataMap_;
};


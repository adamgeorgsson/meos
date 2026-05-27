#pragma once

#include "oAbstractRunner.h"
#include <vector>

// pRunner/pTeam/cTeam are declared in oAbstractRunner.h

// Minimal stub for oTeam — full migration in US-003h1.
class oTeam : public oAbstractRunner {
public:
  std::vector<pRunner> Runners;

  explicit oTeam(oEvent* poe) : oAbstractRunner(poe, false) {}
  ~oTeam() override = default;

  std::wstring getInfo() const override { return sName; }
  void changedObject() override {}
  void remove() override { Removed = true; }
  bool canRemove() const override { return true; }
  void merge(const oBase&, const oBase*) override {}
  oDataContainer& getDataBuffers(pvoid& d, pvoid& o, pvectorstr& s) const override;
  int getDISize() const override { return 0; }

  // oAbstractRunner pure virtuals
  cTeam getTeam() const override { return nullptr; }
  pTeam getTeam() override { return nullptr; }
  bool matchAbstractRunner(const oAbstractRunner*) const override { return false; }
  bool isResultUpdated(bool) const override { return false; }
  int getNumShortening() const override { return 0; }
  void markClassChanged(int) override {}
  void apply(ChangeType, pRunner) override {}
  int getTimeAfter(int, bool) const override { return 0; }
  void fillSpeakerObject(int, int, const std::vector<int>&, bool, oSpeakerObject&) const override {}
  void setBib(const std::wstring&, int, bool) override {}
  int getRogainingPoints(bool, bool) const override { return 0; }
  int getRogainingReduction(bool) const override { return 0; }
  int getRogainingOvertime(bool) const override { return 0; }
  int getRogainingPointsGross(bool) const override { return 0; }
  RunnerStatus getStatusComputed(bool) const override { return tStatus; }
  DynamicRunnerStatus getDynamicStatus() const override { return DynamicRunnerStatus::StatusInactive; }
  int getPlace(bool) const override { return 0; }
  int getTotalPlace(bool) const override { return 0; }
  std::wstring getEntryDate(bool) const override { return L""; }
  const std::pair<std::wstring, int> getRaceInfo() override { return {L"", 0}; }
  int getRanking() const override { return 0; }
  int classInstance() const override { return 0; }

  // Stubs needed by oRunner core methods
  int getNumDistinctRunners() const { return 1; }
  int getNumAssignedRunners() const { return 1; }
  void quickApply() {}
  pRunner getRunner(int /*leg*/) const { return nullptr; }
  int getLegRunningTime(int, bool, bool) const { return 0; }
  RunnerStatus getLegStatus(int, bool, bool) const { return StatusUnknown; }
  int getLegPlace(int, bool, bool) const { return 0; }
  int getNumShortening(int) const { return 0; }

private:
  mutable DataMap dataMap_;
  mutable DataMap oldDataMap_;
};

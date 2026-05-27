#pragma once

#include "oBase.h"
#include "oDataContainer.h"

// Forward declarations for punch-related classes
class oCourse;
class oControl;

// Minimal stub for oPunch — provides SpecialPunch enum used by oControl.
// Full implementation in US-003b2.
class oPunch : public oBase {
public:
  enum SpecialPunch {
    PunchUnused = 0,
    PunchStart = 1,
    PunchFinish = 2,
    PunchCheck = 3,
    HiredCard = 11111
  };

  explicit oPunch(oEvent* poe) : oBase(poe) {}
  virtual ~oPunch() = default;

  std::wstring getInfo() const override { return L"oPunch"; }
  void changedObject() override {}
  void remove() override { Removed = true; }
  bool canRemove() const override { return true; }
  void merge(const oBase& /*input*/, const oBase* /*base*/) override {}

  oDataContainer& getDataBuffers(pvoid& data, pvoid& olddata,
                                  pvectorstr& strData) const override;
  int getDISize() const override { return 0; }

private:
  mutable DataMap dataMap_;
  mutable DataMap oldDataMap_;
};

using pPunch = oPunch*;

#pragma once
#include "oBase.h"
#include "oDataContainer.h"

// Stub for oClass — full migration in a later story.
class oClass : public oBase {
public:
  explicit oClass(oEvent* poe) : oBase(poe) {}
  ~oClass() override = default;
  std::wstring getInfo() const override { return L"oClass"; }
  void changedObject() override {}
  void remove() override { Removed = true; }
  bool canRemove() const override { return true; }
  void merge(const oBase&, const oBase*) override {}
  oDataContainer& getDataBuffers(pvoid& d, pvoid& o, pvectorstr& s) const override;
  int getDISize() const override { return 0; }
private:
  mutable DataMap dataMap_;
  mutable DataMap oldDataMap_;
};
using pClass = oClass*;

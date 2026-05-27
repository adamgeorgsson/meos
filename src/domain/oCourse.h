#pragma once
#include "oBase.h"
#include "oDataContainer.h"

// Stub for oCourse — full migration in a later story.
class oCourse : public oBase {
public:
  explicit oCourse(oEvent* poe) : oBase(poe) {}
  ~oCourse() override = default;
  std::wstring getInfo() const override { return L"oCourse"; }
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
using pCourse = oCourse*;

// oClass.h — Minimal stub for oClass so oControl compiles.
// Full implementation comes in US-003e.
#pragma once
#include "domain_header.h"

class oClass {
public:
  bool isRemoved() const { return false; }
  int getId() const { return 0; }
  bool hasAnyCourse(const set<int>& /*courseIds*/) const { return false; }
};
typedef oClass* pClass;

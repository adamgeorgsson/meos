// oCourse.h — Minimal stub for oCourse so oControl and oPunch compile.
// Full implementation comes in US-003d.
#pragma once
#include "domain_header.h"

class oControl;
typedef oControl* pControl;

class oCourse {
public:
  bool isRemoved() const { return false; }
  int getId() const { return 0; }
  int nControls() const { return 0; }
  bool hasControl(const oControl* /*c*/) const { return false; }
  int getStartPunchType() const { return 0; }
  int getFinishPunchType() const { return 0; }
  const oControl* getControl(int /*index*/) const { return nullptr; }
  bool getCommonControl() const { return false; }
};
typedef oCourse* pCourse;

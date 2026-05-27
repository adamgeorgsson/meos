#pragma once
#include "oPunch.h"

// Stub for oFreePunch — full migration in a later story.
class oFreePunch : public oPunch {
public:
  explicit oFreePunch(oEvent* poe) : oPunch(poe) {}
  ~oFreePunch() override = default;
};
using pFreePunch = oFreePunch*;

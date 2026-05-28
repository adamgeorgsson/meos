#pragma once
// Stub oListInfo for the domain migration.
// oListInfo is a class (friend class oListInfo in oAbstractRunner.h).
// Only the ResultType enum and the oListParam struct used by GeneralResult
// are defined here. The full oListInfo will be migrated in a later story.

class oListInfo {
public:
  enum ResultType {
    Global,
    Classwise,
    Legwise,
    Coursewise
  };
};

struct oListParam {
  int useControlIdResultTo   = 0;
  int useControlIdResultFrom = 0;
};

// oEventDraw.cpp — Start list draw and sorting (US-003i).
// Cross-platform, no Win32 dependencies.

#include "../util/gdioutput.h"
#include "../util/Table.h"
#include "oBase.h"
#include "oEvent.h"
#include "oRunner.h"
#include "oClass.h"

#include <algorithm>

// Sort all runners by start time within each class.
void oEvent::sortRunnersByStartTime() {
  Runners.sort([](const oRunner& a, const oRunner& b) {
    int clsA = a.getClassId(false);
    int clsB = b.getClassId(false);
    if (clsA != clsB) return clsA < clsB;
    return a.getStartTime() < b.getStartTime();
  });
}

// Assign sequential start times to runners in each class.
void oEvent::drawAllClasses(int firstStart, int interval) {
  if (interval <= 0) interval = 60;

  // Collect classes
  vector<pClass> cls;
  getClasses(cls, false);

  for (pClass pc : cls) {
    if (!pc) continue;
    int classId = pc->getId();

    // Collect runners for this class
    vector<pRunner> runners;
    for (auto& r : Runners) {
      if (!r.isRemoved() && r.getClassId(false) == classId)
        runners.push_back(&r);
    }
    if (runners.empty()) continue;

    // Sort by existing start time (stable), then assign sequential times
    std::stable_sort(runners.begin(), runners.end(), [](pRunner a, pRunner b) {
      return a->getStartNo() < b->getStartNo();
    });

    int t = firstStart;
    for (pRunner r : runners) {
      r->setStartTime(t, true, oBase::ChangeType::Update);
      t += interval;
    }
  }
}

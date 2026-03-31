// oEventResult.cpp — Result computation (US-003i).
// Cross-platform, no Win32 dependencies.

#include "../util/Table.h"
#include "oEvent.h"
#include "oRunner.h"
#include "oClass.h"

#include <algorithm>
#include <vector>

// Sort runners by result: status then running time.
static bool resultOrder(pRunner a, pRunner b) {
  // Finished with OK/NoTiming ranks before others
  RunnerStatus sa = a->getStatus();
  RunnerStatus sb = b->getStatus();

  bool aFinished = (sa == StatusOK || sa == StatusNoTiming || sa == StatusOutOfCompetition);
  bool bFinished = (sb == StatusOK || sb == StatusNoTiming || sb == StatusOutOfCompetition);

  if (aFinished != bFinished) return aFinished > bFinished;
  if (aFinished && bFinished)
    return a->getRunningTime(false) < b->getRunningTime(false);
  // Both unfinished — sort by status enum then running time
  if (sa != sb) return sa < sb;
  return a->getRunningTime(false) < b->getRunningTime(false);
}

// Calculate results for one class (or all if classId == 0).
void oEvent::calculateResults(int classId) {
  vector<pClass> cls;
  getClasses(cls, false);

  for (pClass pc : cls) {
    if (!pc) continue;
    if (classId != 0 && pc->getId() != classId) continue;

    vector<pRunner> runners;
    for (auto& r : Runners) {
      if (!r.isRemoved() && r.getClassId(false) == pc->getId())
        runners.push_back(&r);
    }
    if (runners.empty()) continue;

    std::stable_sort(runners.begin(), runners.end(), resultOrder);

    int place = 0, rank = 0;
    int prevTime = -1;
    for (pRunner r : runners) {
      RunnerStatus st = r->getStatus();
      bool counted = (st == StatusOK || st == StatusNoTiming);
      rank++;
      if (counted) {
        int rt = r->getRunningTime(false);
        if (rt != prevTime) { place = rank; prevTime = rt; }
        r->tmpResult = oAbstractRunner::TempResult(rt, st, 0, place);
      } else {
        r->tmpResult = oAbstractRunner::TempResult(0, st, 0, 0);
      }
    }
  }
}

// Sort runners in Runners list by result within each class.
void oEvent::sortRunnersByResult(int classId) {
  calculateResults(classId);

  Runners.sort([classId](const oRunner& a, const oRunner& b) {
    int clsA = a.getClassId(false);
    int clsB = b.getClassId(false);
    if (clsA != clsB) return clsA < clsB;
    if (classId != 0 && clsA != classId) return false;

    return resultOrder(pRunner(&a), pRunner(&b));
  });
}

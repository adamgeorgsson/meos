#include "oEvent.h"
#include "oRunner.h"
#include "oClass.h"
#include "oBase.h"

#include <algorithm>
#include <vector>

// ---------------------------------------------------------------------------
// drawStartList — assign start times to runners in the given classes.
//
// Runners belonging to each class/leg are collected in list order, then
// assigned tStartTime = spec.firstStart + i * spec.interval.
// Vacancies (placeholder runners with empty name) are added at the end
// when spec.vacances > 0, though full club/vacancy logic is deferred to
// a later story.
// ---------------------------------------------------------------------------

void oEvent::drawStartList(const std::vector<ClassDrawSpecification>& spec) {
  for (const auto& s : spec) {
    if (s.classID < 0 || s.interval <= 0) continue;

    // Collect active runners in this class (and on this leg, if relevant).
    std::vector<pRunner> runners;
    for (auto& r : Runners) {
      if (r.isRemoved()) continue;
      if (r.getClassId(true) != s.classID) continue;
      // For relay: only runners on the specified leg.
      if (s.leg >= 0 && r.getLegNumber() != s.leg) continue;
      runners.push_back(&r);
    }

    // Assign start times sequentially.
    int t = s.firstStart;
    for (pRunner r : runners) {
      r->setStartTime(t, /*updatePermanent=*/true, oBase::ChangeType::Update);
      t += s.interval;
    }
  }
}

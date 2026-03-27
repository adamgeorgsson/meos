#pragma once
// Internal helpers shared between oClass.cpp and oClassConfig.cpp.
// Free-function equivalents of ClassSplit static methods,
// needed because ClassSplit is a file-local class in oClass.cpp.

inline int classSplitEvaluateTime(const oAbstractRunner &r) {
  if (r.getInputStatus() == StatusOK) {
    int t = r.getInputTime();
    if (t > 0)
      return t;
    else
      return timeConstHour * 24 * 8;
  }
  else {
    return timeConstHour * 24 * 8 + r.getId();
  }
}

inline int classSplitEvaluateResult(const oAbstractRunner &r) {
  int baseRes;
  if (r.getInputStatus() == StatusOK) {
    int t = r.getInputPlace();

    if (t == 0) {
      const oRunner *rr = dynamic_cast<const oRunner *>(&r);
      if (rr && rr->getTeam() && rr->getLegNumber() > 0) {
        const pRunner rPrev = rr->getTeam()->getRunner(rr->getLegNumber() - 1);
        if (rPrev && rPrev->getStatus() == StatusOK)
          t = rPrev->getPlace();
      }
    }

    if (t > 0)
      baseRes = t;
    else
      baseRes = 99999;
  }
  else {
    baseRes = 99999 + r.getInputStatus();
  }
  return r.getDCI().getInt("Heat") + 1000 * baseRes;
}

inline int classSplitEvaluatePoints(const oAbstractRunner &r) {
  if (r.getInputStatus() == StatusOK) {
    int p = r.getInputPoints();
    if (p > 0)
      return 1000*1000*1000 - p;
    else
      return 1000*1000*1000;
  }
  else {
    return 1000*1000*1000 + r.getInputStatus();
  }
}

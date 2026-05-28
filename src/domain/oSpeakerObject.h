#pragma once

#include "domain_header.h"
#include "runner_status.h"

// Forward declaration — avoids circular include with oRunner.h
class oRunner;

// ---------------------------------------------------------------------------
// oSpeakerObject — domain data transfer object for the speaker subsystem.
// Populated by oRunner::fillSpeakerObject / oTeam::fillSpeakerObject.
// Contains only domain types (no Win32/GDI dependencies).
// ---------------------------------------------------------------------------
class oSpeakerObject {
public:
  struct RunningTime {
    int time = 0;
    int preliminary = 0;
    void reset() { time = 0; preliminary = 0; }
    RunningTime() = default;
  };

  struct ResultAtControl {
    RunningTime runningTime;
    RunningTime runningTimeLeg;
    RunningTime runningTimeSinceLast;

    int place = 0;
    int parallelScore = 0;
    RunnerStatus status = StatusUnknown;

    // For parallel legs
    int runnersFinishedLeg = 0;
    bool restarted = false;

    bool hasResult() const {
      return status == StatusOK ||
             (status == StatusUnknown && runningTime.time > 0);
    }
    bool isIncomming() const {
      return status == StatusUnknown && runningTime.time <= 0;
    }
  };

  void reset() {
    owner = nullptr;
    bib.clear();
    names.clear();
    outgoingnames.clear();
    resultRemark.clear();
    club.clear();
    startTimeS.clear();
    result.clear();
    finishStatus = StatusUnknown;
    compareResultIndex = -1;
    nextPreliminaryTime = -1;
    runnersTotalLeg = 0;
    timeSinceChange = -1;
    priority = 0;
    missingStartTime = false;
    isRendered = false;
    useSinceLast = false;
  }

  size_t size() const { return result.size(); }

  const ResultAtControl& operator[](size_t ix) const { return result[ix]; }

  bool hasResult(int where) const {
    return where < static_cast<int>(result.size()) && result[where].hasResult();
  }
  bool isIncomming(int where) const {
    return where < static_cast<int>(result.size()) && result[where].isIncomming();
  }

  bool highlight(int timeToHighlightSecond) const {
    return timeSinceChange < timeToHighlightSecond * timeConstSecond && timeSinceChange >= 0;
  }

  oRunner* owner = nullptr;
  wstring bib;
  vector<wstring> names;
  vector<wstring> outgoingnames;
  string resultRemark;
  wstring club;
  wstring startTimeS;
  vector<ResultAtControl> result;

  int compareResultIndex = -1;
  int nextPreliminaryTime = -1;

  // For parallel legs
  int runnersTotalLeg = 0;

  RunnerStatus finishStatus = StatusUnknown;

  // In time units. Negative if undefined.
  int timeSinceChange = -1;

  int priority = 0;
  bool missingStartTime = false;
  bool isRendered = false;
  bool useSinceLast = false;

  oSpeakerObject() = default;
};

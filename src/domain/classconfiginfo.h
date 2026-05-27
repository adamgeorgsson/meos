#pragma once
// ClassConfigInfo — cross-platform migration of code/classconfiginfo.h
// Holds per-competition class categorisation (individual/relay/rogaining/etc.)
// Built from an oEvent class list; no Win32 dependencies.

#include <map>
#include <set>
#include <string>
#include <vector>

// ClassType, StartTypes, LegTypes come from oClass.h
#include "oClass.h"

class ClassConfigInfo {
public:
  bool hasMultiCourse = false;
  bool hasMultiEvent  = false;
  bool hasRentedCard  = false;

  std::vector<int> individual;
  std::vector<int> relay;
  std::vector<int> patrol;
  std::vector<int> rogainingClasses;
  std::vector<int> rogainingTeam;
  std::vector<int> rogainingPatrol;
  std::vector<int> knockout;
  std::vector<int> lapcountsingle;
  std::vector<int> lapcountextra;

  std::vector<std::vector<int>> timeStart;
  std::vector<std::vector<int>> legNStart;
  std::vector<std::vector<int>> raceNStart;

  std::map<int, std::vector<int>> legResult;   // main leg -> class ids
  std::vector<std::vector<int>> raceNRes;

  std::vector<std::wstring> classWithoutCourse;

  void clear();

  bool empty() const;

  bool hasIndividual()       const { return !individual.empty(); }
  bool hasRelay()            const { return !relay.empty(); }
  bool hasPatrol()           const { return !patrol.empty(); }
  bool hasRogaining()        const { return !rogainingClasses.empty(); }
  bool hasRogainingTeam()    const { return !rogainingTeam.empty(); }
  bool hasRogainingPatrol()  const { return !rogainingPatrol.empty(); }
  bool isMultiStageEvent()   const { return hasMultiEvent; }
  bool hasTeamClass()        const;
  bool hasQualificationFinal() const { return !knockout.empty(); }
  bool hasResults()          const { return results_; }
  bool hasStartTimes()       const { return starttimes_; }
  int  getNumLegsTotal()     const { return maximumLegNumber_; }

  void getIndividual(std::set<int>& sel, bool forStartList) const;
  void getRelay(std::set<int>& sel) const;
  void getPatrol(std::set<int>& sel) const;
  void getTeamClass(std::set<int>& sel) const;
  void getRogaining(std::set<int>& sel) const;
  void getRogainingTeam(std::set<int>& sel) const;
  void getRogainingPatrol(std::set<int>& sel) const;

  void getRaceNStart(int race, std::set<int>& sel) const;
  void getLegNStart(int leg,  std::set<int>& sel) const;
  void getRaceNRes(int race,  std::set<int>& sel) const;
  void getLegNRes(int leg,   std::set<int>& sel) const;
  void getTimeStart(int leg, std::set<int>& sel) const;

  // These are set by oEvent::getClassConfigurationInfo (migrated later).
  bool results_       = false;
  bool starttimes_    = false;
  int  maximumLegNumber_ = 0;
};

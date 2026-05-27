// classconfiginfo.cpp — Domain migration of code/classconfiginfo.cpp
// oEvent::getClassConfigurationInfo() is deferred (requires oRunner/oCard).

#include "classconfiginfo.h"
#include <algorithm>

void ClassConfigInfo::clear() {
  individual.clear();
  relay.clear();
  patrol.clear();
  rogainingClasses.clear();
  rogainingTeam.clear();
  rogainingPatrol.clear();
  knockout.clear();
  lapcountsingle.clear();
  lapcountextra.clear();
  timeStart.clear();
  legNStart.clear();
  raceNStart.clear();
  legResult.clear();
  raceNRes.clear();
  classWithoutCourse.clear();
  hasMultiCourse = false;
  hasMultiEvent  = false;
  hasRentedCard  = false;
  results_       = false;
  starttimes_    = false;
  maximumLegNumber_ = 0;
}

bool ClassConfigInfo::empty() const {
  return individual.empty() && rogainingClasses.empty() &&
         relay.empty() && patrol.empty() &&
         raceNStart.empty() && rogainingTeam.empty() && rogainingPatrol.empty();
}

bool ClassConfigInfo::hasTeamClass() const {
  return !relay.empty() || !patrol.empty() || !raceNRes.empty();
}

void ClassConfigInfo::getIndividual(std::set<int>& sel, bool forStartList) const {
  sel.insert(individual.begin(), individual.end());
  if (forStartList)
    sel.insert(rogainingClasses.begin(), rogainingClasses.end());
}

void ClassConfigInfo::getRelay(std::set<int>& sel) const {
  sel.insert(relay.begin(), relay.end());
}

void ClassConfigInfo::getPatrol(std::set<int>& sel) const {
  sel.insert(patrol.begin(), patrol.end());
}

void ClassConfigInfo::getTeamClass(std::set<int>& sel) const {
  sel.insert(relay.begin(), relay.end());
  sel.insert(patrol.begin(), patrol.end());
  if (!raceNStart.empty())
    sel.insert(raceNRes[0].begin(), raceNRes[0].end());
}

void ClassConfigInfo::getRogaining(std::set<int>& sel) const {
  sel.insert(rogainingClasses.begin(), rogainingClasses.end());
}

void ClassConfigInfo::getRogainingTeam(std::set<int>& sel) const {
  sel.insert(rogainingTeam.begin(), rogainingTeam.end());
}

void ClassConfigInfo::getRogainingPatrol(std::set<int>& sel) const {
  sel.insert(rogainingPatrol.begin(), rogainingPatrol.end());
}

void ClassConfigInfo::getRaceNStart(int race, std::set<int>& sel) const {
  if (size_t(race) < raceNStart.size() && !raceNStart[race].empty())
    sel.insert(raceNStart[race].begin(), raceNStart[race].end());
  else
    sel.clear();
}

void ClassConfigInfo::getLegNStart(int leg, std::set<int>& sel) const {
  if (size_t(leg) < legNStart.size() && !legNStart[leg].empty())
    sel.insert(legNStart[leg].begin(), legNStart[leg].end());
  else
    sel.clear();
}

void ClassConfigInfo::getRaceNRes(int race, std::set<int>& sel) const {
  if (size_t(race) < raceNRes.size() && !raceNRes[race].empty())
    sel.insert(raceNRes[race].begin(), raceNRes[race].end());
  else
    sel.clear();
}

void ClassConfigInfo::getLegNRes(int leg, std::set<int>& sel) const {
  auto res = legResult.find(leg);
  if (res != legResult.end())
    sel.insert(res->second.begin(), res->second.end());
  else
    sel.clear();
}

void ClassConfigInfo::getTimeStart(int leg, std::set<int>& sel) const {
  if (size_t(leg) < timeStart.size() && !timeStart[leg].empty())
    sel.insert(timeStart[leg].begin(), timeStart[leg].end());
  else
    sel.clear();
}

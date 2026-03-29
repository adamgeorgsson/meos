// classconfiginfo.cpp — ClassConfigInfo implementation.
// Ported from code/classconfiginfo.cpp (oEvent methods moved to oEvent).

#include "classconfiginfo.h"

void ClassConfigInfo::clear() {
  individual.clear();
  relay.clear();
  patrol.clear();
  legNStart.clear();
  raceNStart.clear();
  legResult.clear();
  raceNRes.clear();
  rogainingClasses.clear();
  rogainingTeam.clear();
  rogainingPatrol.clear();
  timeStart.clear();
  knockout.clear();
  lapcountsingle.clear();
  lapcountextra.clear();
  hasMultiCourse = false;
  hasMultiEvent = false;
  hasRentedCard = false;
  classWithoutCourse.clear();
  maximumLegNumber = 0;
  results = false;
  starttimes = false;
}

bool ClassConfigInfo::empty() const {
  return individual.empty() && rogainingClasses.empty()
      && relay.empty() && patrol.empty()
      && raceNStart.empty() && rogainingTeam.empty() && rogainingPatrol.empty();
}

void ClassConfigInfo::getIndividual(set<int> &sel, bool forStartList) const {
  sel.insert(individual.begin(), individual.end());
  if (forStartList)
    sel.insert(rogainingClasses.begin(), rogainingClasses.end());
}

void ClassConfigInfo::getRelay(set<int> &sel) const {
  sel.insert(relay.begin(), relay.end());
}

void ClassConfigInfo::getTeamClass(set<int> &sel) const {
  sel.insert(relay.begin(), relay.end());
  sel.insert(patrol.begin(), patrol.end());
  if (!raceNStart.empty())
    sel.insert(raceNRes[0].begin(), raceNRes[0].end());
}

bool ClassConfigInfo::hasTeamClass() const {
  return !relay.empty() || !patrol.empty() || !raceNRes.empty();
}

bool ClassConfigInfo::hasQualificationFinal() const {
  return !knockout.empty();
}

void ClassConfigInfo::getPatrol(set<int> &sel) const {
  sel.insert(patrol.begin(), patrol.end());
}

void ClassConfigInfo::getRogaining(set<int> &sel) const {
  sel.insert(rogainingClasses.begin(), rogainingClasses.end());
}

void ClassConfigInfo::getRogainingTeam(set<int> &sel) const {
  sel.insert(rogainingTeam.begin(), rogainingTeam.end());
}

void ClassConfigInfo::getRogainingPatrol(set<int> &sel) const {
  sel.insert(rogainingPatrol.begin(), rogainingPatrol.end());
}

void ClassConfigInfo::getRaceNStart(int race, set<int> &sel) const {
  if (size_t(race) < raceNStart.size() && !raceNStart[race].empty())
    sel.insert(raceNStart[race].begin(), raceNStart[race].end());
  else
    sel.clear();
}

void ClassConfigInfo::getLegNStart(int leg, set<int> &sel) const {
  if (size_t(leg) < legNStart.size() && !legNStart[leg].empty())
    sel.insert(legNStart[leg].begin(), legNStart[leg].end());
  else
    sel.clear();
}

void ClassConfigInfo::getRaceNRes(int race, set<int> &sel) const {
  if (size_t(race) < raceNRes.size() && !raceNRes[race].empty())
    sel.insert(raceNRes[race].begin(), raceNRes[race].end());
  else
    sel.clear();
}

void ClassConfigInfo::getLegNRes(int leg, set<int> &sel) const {
  auto res = legResult.find(leg);
  if (res != legResult.end())
    sel.insert(res->second.begin(), res->second.end());
  else
    sel.clear();
}

void ClassConfigInfo::getTimeStart(int leg, set<int> &sel) const {
  if (size_t(leg) < timeStart.size() && !timeStart[leg].empty())
    sel.insert(timeStart[leg].begin(), timeStart[leg].end());
  else
    sel.clear();
}

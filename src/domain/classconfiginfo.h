#pragma once
// classconfiginfo.h — Class configuration info aggregate.
// Ported from code/classconfiginfo.h.

#include <vector>
#include <set>
#include <map>
#include <string>

using std::vector;
using std::set;
using std::map;
using std::wstring;

class ClassConfigInfo {
  friend class oEvent;
private:
  bool results = false;
  bool starttimes = false;
  int maximumLegNumber = 0;
public:
  vector<vector<int>> timeStart;
  vector<int> individual;
  vector<int> relay;
  vector<int> patrol;
  vector<int> rogainingTeam;
  vector<int> rogainingPatrol;

  vector<vector<int>> legNStart;
  vector<vector<int>> raceNStart;

  map<int, vector<int>> legResult;
  vector<vector<int>> raceNRes;

  vector<int> rogainingClasses;
  vector<int> knockout;
  vector<int> lapcountsingle;
  vector<int> lapcountextra;

  bool hasMultiCourse = false;
  bool hasMultiEvent = false;
  bool hasRentedCard = false;

  vector<wstring> classWithoutCourse;

  void clear();

  bool hasIndividual() const { return !individual.empty(); }
  bool hasRelay() const { return !relay.empty(); }
  bool hasPatrol() const { return !patrol.empty(); }
  bool hasRogaining() const { return !rogainingClasses.empty(); }
  bool hasRogainingTeam() const { return !rogainingTeam.empty(); }
  bool hasRogainingPatrol() const { return !rogainingPatrol.empty(); }

  bool empty() const;

  bool isMultiStageEvent() const { return hasMultiEvent; }

  void getIndividual(set<int> &sel, bool forStartList) const;
  void getRelay(set<int> &sel) const;
  void getPatrol(set<int> &sel) const;
  void getTeamClass(set<int> &sel) const;
  void getRogaining(set<int> &sel) const;
  void getRogainingTeam(set<int> &sel) const;
  void getRogainingPatrol(set<int> &sel) const;

  bool hasTeamClass() const;
  bool hasQualificationFinal() const;

  void getRaceNStart(int race, set<int> &sel) const;
  void getLegNStart(int leg, set<int> &sel) const;
  void getRaceNRes(int race, set<int> &sel) const;
  void getLegNRes(int leg, set<int> &sel) const;
  void getTimeStart(int leg, set<int> &sel) const;

  bool hasResults() const { return results; }
  bool hasStartTimes() const { return starttimes; }
  int getNumLegsTotal() const { return maximumLegNumber; }
};

#pragma once
#include "oBase.h"
#include "oDataContainer.h"
#include "oControl.h"

class oEvent;
class oClass;

class oCourse : public oBase {
protected:
  vector<oControl*> controls;
  wstring name;
  int length = 0;
  oControl* start = nullptr;
  oControl* finish = nullptr;
  vector<int> legLengths;

  // Caching
  mutable vector<wstring> cachedControlOrdinal;
  mutable int cachedHasRogaining = 0;
  mutable int cacheDataRevision = -1;

  // For adapted courses
  vector<int> tMapToOriginalOrder;

  mutable int tMapsUsed = -1;
  mutable int tMapsUsedNoVacant = -1;
  int tMapsRemaining = 0;

  mutable DataMap dataMap_;
  mutable DataMap oldDataMap_;

  static oDataContainer& container();
  oDataContainer& getDataBuffers(pvoid& data, pvoid& olddata,
                                  pvectorstr& strData) const override;
  int getDISize() const final { return 0; }

  void changedObject() override;
  void changeId(int newId) override {}

  oControl* doAddControl(int id);
  void clearCache() const;

  static int matchLoopKey(const vector<int>& punches,
                          const vector<oControl*>& key);

public:
  explicit oCourse(oEvent* poe);
  oCourse(oEvent* poe, int id);
  ~oCourse() override;

  wstring getInfo() const override;

  void merge(const oBase&, const oBase*) final {}

  // Control management
  int nControls() const { return (int)controls.size(); }
  int getNumControls() const { return (int)controls.size(); }
  oControl* getControl(int index) const;
  void getControls(vector<oControl*>& pc) const;
  vector<int> getControlNumbers() const;
  string getControls() const;
  bool importControls(const string& cstring, bool setChanged,
                      bool updateLegLengths);
  void importLegLengths(const string& legs, bool setChanged);
  static void splitControls(const string& ctrls, vector<int>& nr);
  oControl* addControl(int id);

  // Name / length
  const wstring& getName() const { return name; }
  void setName(const wstring& n);
  int getLength() const { return length; }
  void setLength(int len);
  wstring getLengthS() const;

  // Leg lengths
  void setLegLengths(const vector<int>& legs);
  string getLegLengths() const;
  int getLegLength(int i) const;

  // Start/finish control
  const wstring& getStart() const;
  bool setStartFinish(oControl* startC, oControl* finishC,
                      bool updateStatus = true);
  bool setStartFinishId(int startId, int finishId, bool updateStatus = true);
  int getStartId() const { return start ? start->getId() : 0; }
  int getFinishId() const { return finish ? finish->getId() : 0; }

  // First/last as start/finish
  bool useFirstAsStart() const;
  bool useLastAsFinish() const;
  void firstAsStart(bool f);
  void lastAsFinish(bool f);
  int getStartPunchType() const;
  int getFinishPunchType() const;

  // Common control / loops
  int getCommonControl() const;
  void setCommonControl(int ctrlId);
  int getNumLoops() const;
  bool constructLoopKeys(int cc,
                         vector<vector<oControl*>>& loopKeys,
                         vector<int>& ccIndex) const;

  // Rogaining
  bool hasRogaining() const;
  int getMinimumRogainingPoints() const;
  void setMinimumRogainingPoints(int p);
  int getMaximumRogainingTime() const;
  void setMaximumRogainingTime(int t);
  int getRogainingPointsPerMinute() const;
  void setRogainingPointsPerMinute(int p);
  int calculateReduction(int overTime) const;

  // Ordinal / part of course
  const wstring& getControlOrdinal(int controlIndex) const;
  double getPartOfCourse(int start, int end) const;

  // Maps
  void setNumberMaps(int nm);
  int getNumberMaps() const;
  int getNumUsedMaps(bool noVacant) const;

  // Identity sum
  int getIdSum(int nC);

  // Control checks
  bool hasControl(const oControl* ctrl) const;
  bool hasControlCode(int code) const;
  int getCourseControlId(int controlIx) const;
  wstring getRadioName(int courseControlId) const;

  // Shorter version / adapted course
  pair<bool, oCourse*> getShorterVersion() const;
  void setShorterVersion(bool activeShortening, oCourse* shorter);
  bool isAdapted() const;
  int getAdaptionId() const;
  const vector<int>& getMapToOriginalOrder() const {
    return tMapToOriginalOrder;
  }

  // Validation
  wstring getCourseProblems() const;

  void remove() override { Removed = true; }
  bool canRemove() const override { return true; }

  bool operator<(const oCourse& b) const { return name < b.name; }

  friend class oEvent;
  friend class oClass;
  friend class oRunner;
};

// pCourse = oCourse* already defined in oControl.h
using pControl = oControl*;

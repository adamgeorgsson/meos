// oCourse.h — Migrated from legacy code/oCourse.h (US-003d).
// Cross-platform, no Win32 / GUI dependencies.
#pragma once

#include "oControl.h"
#include <map>

class oEvent;
class oCourse;
class oClass;
typedef oCourse* pCourse;
typedef oClass*  pClass;
class oCard;
struct SICard;

class xmlparser;
class xmlobject;

class oCourse : public oBase {
private:
  // Return 1000 on no match. Lower value = better match.
  static int matchLoopKey(const vector<int>& punches, const vector<pControl>& key);

protected:
  vector<pControl> controls;
  wstring name;
  int length = 0;
  pControl start  = nullptr;
  pControl finish = nullptr;

  static const int dataSize = 128;
  int getDISize() const final { return dataSize; }

  BYTE oData[dataSize];
  BYTE oDataOld[dataSize];

  // Length of each leg: Start-1, 1-2, …, N-Finish.
  vector<int> legLengths;

  int tMapsRemaining = 0;
  mutable int tMapsUsed = -1;
  mutable int tMapsUsedNoVacant = -1;

  pControl doAddControl(int Id);
  void changeId(int newId);

  // Caching
  mutable vector<wstring> cachedControlOrdinal;
  mutable int cachedHasRogaining = 0;
  mutable int cacheDataRevision  = -1;
  void clearCache() const;

  DataRevisionCache<int> bestTime;

  oDataContainer& getDataBuffers(pvoid& data, pvoid& olddata, pvectorstr& strData) const;

  // For adapted (loop) courses
  vector<int> tMapToOriginalOrder;

  void changedObject();

  pair<int, bool> inputData(int id, const wstring& input,
                             int inputId, wstring& output, bool noUpdate) override;
  void fillInput(int id, vector<pair<wstring, size_t>>& out, size_t& selected) override;

public:
  int getIdSum(int nControls);

  void getClasses(vector<pClass>& usageClass) const;

  void remove();
  bool canRemove() const;

  wstring getRadioName(int courseControlId) const;

  bool hasControl(const oControl* ctrl) const;
  bool hasControlCode(int code) const;

  /// Returns course-specific id for the given control index (handles duplicates)
  int getCourseControlId(int controlIx) const;

  bool useFirstAsStart() const;
  bool useLastAsFinish() const;
  void firstAsStart(bool f);
  void lastAsFinish(bool f);

  int getFinishPunchType() const;
  int getStartPunchType() const;

  int  getCommonControl() const;
  void setCommonControl(int ctrlId);

  int getNumLoops() const;

  bool operator<(const oCourse& b) const { return name < b.name; }

  void setNumberMaps(int nm);
  int  getNumberMaps() const;
  int  getNumUsedMaps(bool noVacant) const;

  /// Get a loop course adapted to a card's punch sequence.
  pCourse getAdapetedCourse(const oCard& card, oCourse& tmpCourse, int& numShorten) const;

  bool isAdapted() const;
  pair<bool, pCourse> getShorterVersion() const;
  pCourse getLongerVersion() const;
  void setShorterVersion(bool activeShortening, pCourse shorter);

  const vector<int>& getMapToOriginalOrder() const { return tMapToOriginalOrder; }
  int getAdaptionId() const;

  bool constructLoopKeys(int commonControls,
                         vector<vector<pControl>>& loopKeys,
                         vector<int>& commonControlIndex) const;

  wstring getCourseProblems() const;

  int nControls()    const { return static_cast<int>(controls.size()); }
  int getNumControls() const { return static_cast<int>(controls.size()); }

  void setLegLengths(const vector<int>& legs);

  int  getMinimumRogainingPoints() const;
  void setMinimumRogainingPoints(int p);
  int  getMaximumRogainingTime() const;
  void setMaximumRogainingTime(int t);
  int  getRogainingPointsPerMinute() const;
  void setRogainingPointsPerMinute(int t);
  int  calculateReduction(int overTime) const;
  bool hasRogaining() const;

  const wstring& getControlOrdinal(int controlIndex) const;

  double getPartOfCourse(int startCtrl, int endCtrl) const;

  wstring getInfo() const;

  oControl* getControl(int index) const;

  int distance(const SICard& card) const;
  int distance(const oCard& card) const;
  int distance(int* punches, int numPunches) const;

  /** Returns true if changed. */
  bool importControls(const string& cstring, bool setChanged, bool updateLegLengths);
  void importLegLengths(const string& legs, bool setChanged);

  int getLegLength(int i) const;

  static void splitControls(const string& ctrls, vector<int>& nr);

  pControl addControl(int Id);
  void Set(const xmlobject* xo);

  void getControls(vector<pControl>& pc) const;
  vector<int> getControlNumbers() const;

  string getControls() const;
  string getLegLengths() const;

  wstring getControlsUI() const;
  vector<wstring> getCourseReadable(int limit) const;

  int getBestTime() const;

  const wstring& getName() const { return name; }
  void getNameAndFamily(wstring& name, wstring& family) const;

  int     getLength() const { return length; }
  wstring getLengthS() const;

  void setName(const wstring& n);
  void setLength(int len);

  const wstring& getStart() const;
  void setStart(const wstring& startName, bool sync);

  bool setStartFinish(pControl startC, pControl finishC, bool updateStatus = true);
  bool setStartFinishId(int startCId, int finishCId, bool updateStatus = true);

  int getStartId()  const { return start  ? start->getId()  : 0; }
  int getFinishId() const { return finish ? finish->getId() : 0; }

  void merge(const oBase& input, const oBase* base) final;

  bool Write(xmlparser& xml);

  oCourse(oEvent* poe, int id);
  oCourse(oEvent* poe);
  virtual ~oCourse();

  friend class oEvent;
  friend class oClass;
  friend class oRunner;
  friend class MeosSQL;
};

#pragma once
// qualification_final.h — Qualification/Final scheme for knock-out classes.
// Ported from code/qualification_final.h with Win32 and gdioutput removed.

#include "domain_header.h"

class oRunner;
typedef oRunner *pRunner;
class oClass;

struct QFClass {
  QFClass() = default;
  QFClass(const wstring& name) : name(name) {}

  wstring name;
  vector<pair<int, int>> qualificationMap;
  int numTimeQualifications = 0;

  enum class ExtraQualType {
    None,
    All,
    NBest,
    TimeLimit,
  };

  ExtraQualType extraQualification = ExtraQualType::None;
  int extraQualData = 0;

  static ExtraQualType deserialType(wchar_t dc) {
    switch (dc) {
    case 'A': return ExtraQualType::All;
    case 'B': return ExtraQualType::NBest;
    case 'L': return ExtraQualType::TimeLimit;
    default:  return ExtraQualType::None;
    }
  }

  wstring serialExtra() const;
  int getMinQualInst() const;
  bool rankLevel = false;
  mutable int level = -1;
  wstring getQualInfo() const;
};

class QualificationFinal {
private:
  mutable wstring serializedFrom;
  int maxClassId;
  int baseId;

  enum class LevelInfo { Normal, RankSort };

  vector<LevelInfo> levels;

  struct ResultInfo {
    ResultInfo(pRunner r, int inst, int order) : r(r), instance(inst), order(order) {}
    pRunner r;
    int instance;
    int order;
  };

  vector<vector<ResultInfo>> storedInfo;
  map<int, ResultInfo *> storedInfoLookup;
  map<int, int> numExtraAssigned;

  vector<QFClass> classDefinition;
  map<pair<int, int>, pair<int, int>> sourcePlaceToFinalOrder;
  mutable map<int, int> level2DefningInstance;

  void initgmap(bool check);
  pair<int, int> getPrelFinalFromPlace(int instance, int orderPlace, int numSharedPlaceNext);
  static wstring validName(const wstring& name);

public:
  void setClasses(const vector<QFClass>& def);
  const vector<QFClass>& getClasses() const { return classDefinition; }

  QualificationFinal(int maxClassId, int baseId) : maxClassId(maxClassId), baseId(baseId) {}

  static bool isValidNameChar(wchar_t c);

  bool matchSerialization(const wstring &ser) const { return serializedFrom == ser; }

  int getNumClasses() const { return classDefinition.size(); }

  const QFClass& getInstance(int instance) const { return classDefinition[instance]; }

  const wstring getInstanceName(int inst) { return classDefinition.at(inst-1).name; }

  bool isFinalClass(int instance) const;

  int getNumStages() const { return getNumLevels(); }

  int getHeatFromClass(int finalClassId, int baseClassId) const;

  void importXML(const wstring &file);
  void exportXML(const wstring& file) const;

  void init(const wstring &def);
  void encode(wstring &output) const;

  bool hasRemainingClass() const;
  int getLevel(int instance) const;
  int getNumLevels() const;
  int getMinInstance(const int level) const;

  void prepareCalculations();
  void provideQualificationResult(pRunner r, int instance, int orderPlace, int numSharedPlace);
  void provideUnqualified(int level, pRunner r);
  void computeFinals();

  pair<int, int> getNextFinal(int runnerId) const;
  bool noQualification(int instance) const;
  void getBaseClassInstances(set<int> &base) const;

  // printScheme requires gdioutput — stubbed in domain layer
  void printScheme(const oClass &cls) const {}
};

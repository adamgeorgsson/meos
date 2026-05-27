#pragma once
// qualification_final.h — Domain migration of code/qualification_final.h
// Runner-computation methods (provideQualificationResult, computeFinals, getNextFinal)
// and XML/GDI methods (exportXML, importXML, printScheme) are excluded —
// they belong in later stories (oRunner migration, io/ui layers).

#include <map>
#include <set>
#include <string>
#include <vector>

struct QFClass {
  QFClass() = default;
  explicit QFClass(const std::wstring& n) : name(n) {}

  std::wstring name;
  std::vector<std::pair<int, int>> qualificationMap;
  int numTimeQualifications = 0;

  enum class ExtraQualType { None, All, NBest, TimeLimit };

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

  std::wstring serialExtra() const;
  int getMinQualInst() const;
  std::wstring getQualInfo() const;

  bool rankLevel = false;
  mutable int level = -1;
};

class QualificationFinal {
private:
  mutable std::wstring serializedFrom_;
  int maxClassId_;
  int baseId_;

  enum class LevelInfo { Normal, RankSort };
  std::vector<LevelInfo> levels_;

  std::vector<QFClass> classDefinition_;
  std::map<std::pair<int,int>, std::pair<int,int>> sourcePlaceToFinalOrder_;
  mutable std::map<int,int> level2DefiningInstance_;

  void initgmap(bool check);
  std::pair<int,int> getPrelFinalFromPlace(int instance, int orderPlace,
                                            int numSharedPlaceNext);

  static std::wstring validName(const std::wstring& name);

public:
  QualificationFinal(int maxClassId, int baseId)
    : maxClassId_(maxClassId), baseId_(baseId) {}

  static bool isValidNameChar(wchar_t c);

  bool matchSerialization(const std::wstring& ser) const {
    return serializedFrom_ == ser;
  }

  int getNumClasses() const { return (int)classDefinition_.size(); }

  const QFClass& getInstance(int instance) const {
    return classDefinition_[instance];
  }

  std::wstring getInstanceName(int inst) const {
    return classDefinition_.at((size_t)inst - 1).name;
  }

  const std::vector<QFClass>& getClasses() const { return classDefinition_; }
  void setClasses(const std::vector<QFClass>& def);

  bool isFinalClass(int instance) const;
  int  getNumStages() const { return getNumLevels(); }

  int getHeatFromClass(int finalClassId, int baseClassId) const;

  void init(const std::wstring& def);
  void encode(std::wstring& output) const;

  bool hasRemainingClass() const;

  int getLevel(int instance) const;
  int getNumLevels() const;
  int getMinInstance(int level) const;

  bool noQualification(int instance) const;
  void getBaseClassInstances(std::set<int>& base) const;
};

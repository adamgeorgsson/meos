// generalresult.h — Cross-platform result engine.
// Ported from code/generalresult.h — replaced Win32 CompareString with wstring comparison.
#pragma once

#include "oBase.h"
#include "oAbstractRunner.h"
#include "parser.h"
#include "oListInfo.h"

struct oListParam;
class xmlparser;
class xmlobject;

class GeneralResult
{
private:
  const oListParam *context;
  mutable int lockPrepare = 0;

protected:

  enum PrincipalSort {None, ClassWise, CourseWise};

  virtual std::pair<int, int> score(oTeam &team, RunnerStatus st, int time, int points) const;
  virtual RunnerStatus deduceStatus(oTeam &team) const;
  virtual int deduceTime(oTeam &team) const;
  virtual int deducePoints(oTeam &team) const;

  virtual std::pair<int, int> score(oRunner &runner, RunnerStatus st, int time, int points, bool asTeamMember) const;
  virtual RunnerStatus deduceStatus(oRunner &runner) const;
  virtual int deduceTime(oRunner &runner, int startTime) const;
  virtual int deducePoints(oRunner &runner) const;

  virtual void prepareCalculations(oEvent &oe, bool classResult, const std::set<int> &clsSelection,
                                   std::vector<pRunner> &runners, std::vector<pTeam> &teams, int inputNumber) const;
  virtual void prepareCalculations(oTeam &team, bool classResult) const;
  virtual void prepareCalculations(oRunner &runner, bool classResult) const;
  virtual void storeOutput(std::vector<int> &times, std::vector<int> &numbers) const;

  int getListParamTimeToControl() const;
  int getListParamTimeFromControl() const;

public:

  virtual const std::string &getTimeStamp() const;

  virtual bool isRogaining() const;

  struct BaseResultContext {
  private:
    int leg;
    bool useModule;
    std::pair<int,int> controlId; // Start - finish
    bool totalResults;
    mutable std::map<int, std::pair<int, int>> resIntervalCache;

    friend class GeneralResult;
  };

  struct GeneralResultInfo {
    oAbstractRunner *src;
    int time;
    RunnerStatus status;
    int score;
    int place;

    int getNumSubresult(const BaseResultContext &context) const;
    bool getSubResult(const BaseResultContext &context, int ix, GeneralResultInfo &out) const;

    inline bool compareResult(const GeneralResultInfo &o) const {
      if (status != o.status)
        return RunnerStatusOrderMap[status] < RunnerStatusOrderMap[o.status];

      if (place != o.place)
        return place < o.place;

      const std::wstring &name = src->getName();
      const std::wstring &oname = o.src->getName();

      // Cross-platform replacement for Win32 CompareString
      return name < oname;
    }

    bool operator<(const GeneralResultInfo &o) const {
      pClass cls = src->getClassRef(true);
      pClass ocls = o.src->getClassRef(true);

      if (cls != ocls) {
        int so = cls ? cls->getSortIndex() : 0;
        int oso = ocls ? ocls->getSortIndex() : 0;
        if (so != oso)
          return so < oso;

        // Use id as fallback
        so = cls ? cls->getId() : 0;
        oso = ocls ? ocls->getId() : 0;
        if (so != oso)
          return so < oso;
      }
      return compareResult(o);
    }
  };


  static void calculateIndividualResults(std::vector<pRunner> &runners,
                                         const std::pair<int, int> &controlId,
                                         bool totalResults,
                                         bool inclForestRunners,
                                         bool inclPreliminary,
                                         const std::string &resTag,
                                         oListInfo::ResultType resType,
                                         int inputNumber,
                                         oEvent &oe,
                                         std::vector<GeneralResultInfo> &results);

  static std::shared_ptr<BaseResultContext> calculateTeamResults(std::vector<pTeam> &teams,
                                                                  int leg,
                                                                  const std::pair<int, int> &controlId,
                                                                  bool totalResults,
                                                                  const std::string &resTag,
                                                                  oListInfo::ResultType resType,
                                                                  int inputNumber,
                                                                  oEvent &oe,
                                                                  std::vector<GeneralResultInfo> &results);

  void setContext(const oListParam *context);
  void clearContext();

  void calculateTeamResults(std::vector<oTeam *> &teams,
                            bool classResult,
                            oListInfo::ResultType resType,
                            bool sortTeams,
                            int inputNumber) const;
  void calculateIndividualResults(std::vector<oRunner *> &runners,
                                  bool classResult,
                                  oListInfo::ResultType resType,
                                  bool sortRunners,
                                  int inputNumber) const;
  void sortTeamMembers(std::vector<oRunner *> &runners) const;

  virtual bool isDynamic() const {
    return false;
  }

  template<class T> void sort(std::vector<T*> &rt, SortOrder so) const;

  GeneralResult(void);
  virtual ~GeneralResult(void);
};

class ResultAtControl : public GeneralResult {
protected:
  std::pair<int, int> score(oTeam &team, RunnerStatus st, int time, int points) const override;
  RunnerStatus deduceStatus(oTeam &team) const override;
  int deduceTime(oTeam &team) const override;
  int deducePoints(oTeam &team) const override;

  std::pair<int, int> score(oRunner &runner, RunnerStatus st, int time, int points, bool asTeamMember) const override;
  RunnerStatus deduceStatus(oRunner &runner) const override;
  int deduceTime(oRunner &runner, int startTime) const override;
  int deducePoints(oRunner &runner) const override;
};

class TotalResultAtControl : public ResultAtControl {
protected:
  int deduceTime(oRunner &runner, int startTime) const override;
  RunnerStatus deduceStatus(oRunner &runner) const override;
  std::pair<int, int> score(oRunner &runner, RunnerStatus st, int time, int points, bool asTeamMember) const override;
};

class DynamicResult : public GeneralResult {
public:

  enum DynamicMethods {
    MTScore,
    MDeduceTStatus,
    MDeduceTTime,
    MDeduceTPoints,

    MRScore,
    MDeduceRStatus,
    MDeduceRTime,
    MDeduceRPoints,
    _Mlast
  };

private:
  bool allowRetag = true;
  static std::map<std::string, DynamicMethods> symb2Method;
  static std::map<DynamicMethods, std::pair<std::string, std::string>> method2SymbName;
  static int instanceCount;

  mutable int lowAgeLimit = -1;
  mutable int highAgeLimit = 1000;

  class MethodInfo {
    std::string source;
    mutable ParseNode *pn;
    std::string description;
  public:
    friend class DynamicResult;
    MethodInfo();
    ~MethodInfo();
  };

  std::vector<MethodInfo> methods;
  mutable bool isCompiled;
  mutable Parser parser;
  std::wstring name;
  std::string tag;
  std::wstring description;
  std::wstring annotation;
  mutable std::wstring origin;
  std::string timeStamp;
  bool builtIn;
  mutable bool readOnly;

  const ParseNode *getMethod(DynamicMethods method) const;
  void addSymbol(DynamicMethods method, const char *symb, const char *name);
  RunnerStatus toStatus(int status) const;

  void prepareCommon(oAbstractRunner &runner, bool classResult) const;

public:

  bool isDynamic() const override {
    return true;
  }

  bool retaggable() const { return allowRetag; }
  void retaggable(bool r) { allowRetag = r; }

  void setReadOnly() const {readOnly = true;}

  bool isReadOnly() const {return readOnly;}

  const std::string &getTimeStamp() const override {return timeStamp;}

  bool isRogaining() const override;

  static std::string undecorateTag(const std::string &inputTag);

  long long getHashCode() const;

  void getSymbols(std::vector<std::pair<std::wstring, size_t>> &symb) const;
  void getSymbolInfo(int ix, std::wstring &name, std::wstring &desc) const;

  void declareSymbols(DynamicMethods m, bool clear) const;

  void prepareCalculations(oEvent &oe,
                           bool classResult,
                           const std::set<int> &clsSelection,
                           std::vector<pRunner> &runners,
                           std::vector<pTeam> &teams,
                           int inputNumber) const override;
  void prepareCalculations(oTeam &team, bool classResult) const override;
  void prepareCalculations(oRunner &runner, bool classResult) const override;

  void storeOutput(std::vector<int> &times, std::vector<int> &numbers) const;

  std::pair<int, int> score(oTeam &team, RunnerStatus st, int time, int points) const override;
  RunnerStatus deduceStatus(oTeam &team) const override;
  int deduceTime(oTeam &team) const override;
  int deducePoints(oTeam &team) const override;

  std::pair<int, int> score(oRunner &runner, RunnerStatus st, int time, int points, bool asTeamMember) const override;
  RunnerStatus deduceStatus(oRunner &runner) const override;
  int deduceTime(oRunner &runner, int startTime) const override;
  int deducePoints(oRunner &runner) const override;

  DynamicResult();
  DynamicResult(const DynamicResult &resIn);
  void operator=(const DynamicResult &ctr);

  ~DynamicResult();

  bool hasMethod(DynamicMethods method) const {return getMethod(method) != 0;}

  const std::string &getMethodSource(DynamicMethods method) const;
  void setMethodSource(DynamicMethods method, const std::string &source);

  void getMethodTypes(std::vector<std::pair<DynamicMethods, std::string>> &mt) const;

  const std::string &getTag() const {return tag;}
  void setTag(const std::string &t) {tag = t;}
  void setBuiltIn() {builtIn = true;}
  bool isBuiltIn() const {return builtIn;}
  std::wstring getName(bool withAnnotation) const;
  void setName(const std::wstring &n) {name = n;}
  void setAnnotation(const std::wstring &a) {annotation = a;}
  const std::wstring &getDescription() const {return description;}
  void setDescription(const std::wstring &n) {description = n;}

  void save(const std::wstring &file) const;
  void save(xmlparser &xml) const;

  void load(const std::wstring &file);
  void load(const xmlobject &xDef);

  void compile(bool forceRecompile) const;

  // debugDumpVariables removed (GUI-only — used gdioutput)

  void clear();
};

struct GeneralResultCtr {
  std::wstring name;
  std::string tag;
  std::wstring fileSource;

  bool isDynamic() const;
  bool operator<(const GeneralResultCtr &c) const;
  std::shared_ptr<GeneralResult> ptr;

  // True if implicitly loaded (from list or by class results)
  bool isImplicit() const {
    return fileSource == L"*";
  }

  const std::wstring& getName() const;

  GeneralResultCtr(const char *tag, const std::wstring &name, const std::shared_ptr<GeneralResult> &ptr);
  GeneralResultCtr(const std::wstring &file, const std::shared_ptr<DynamicResult> &ptr);
  GeneralResultCtr() = default;

  ~GeneralResultCtr() = default;

  GeneralResultCtr(const GeneralResultCtr &ctr);
  void operator=(const GeneralResultCtr &ctr);
};

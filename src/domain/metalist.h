#pragma once
/************************************************************************
    MeOS - Orienteering Software
    Migrated to src/domain/ — Win32/GDI/XML rendering excluded.
    MetaList describes the structure of a list (which columns, in which
    order, for which entity type) without any GUI-rendering logic.
************************************************************************/

#include "oListInfo.h"
#include "generalresult.h"
#include <memory>
#include <set>
#include <string>
#include <vector>

class oEvent;

// ---------------------------------------------------------------------------
// MetaListPost — one column specification in a list row
// Rendering fields (gdiFonts, GDICOLOR, pixel positions) excluded.
// ---------------------------------------------------------------------------
class MetaListPost {
  EPostType   type          = lNone;
  EPostType   alignType     = lNone;
  std::wstring text;
  std::wstring alignWithText;
  std::string  resultModule;
  int  leg             = -1;
  int  minimalIndent   = 0;
  int  blockWidth      = 0;
  bool alignBlock      = false;
  bool mergeWithPrevious = false;
  bool limitWidth      = false;
  bool packPrevious    = false;

public:
  MetaListPost() = default;
  explicit MetaListPost(EPostType type_, EPostType align_ = lNone, int leg_ = -1)
    : type(type_), alignType(align_), leg(leg_) {}

  MetaListPost& setBlock(int width)           { blockWidth = width; return *this; }
  MetaListPost& setText(const std::wstring& t){ text = t;           return *this; }
  MetaListPost& setResultModule(const std::string& m) { resultModule = m; return *this; }
  MetaListPost& align(EPostType a, bool ab = true) { alignType = a; alignBlock = ab; return *this; }
  MetaListPost& align(bool ab = true)         { return align(lAlignNext, ab); }
  MetaListPost& alignText(const std::wstring& t)   { alignWithText = t; return *this; }
  MetaListPost& mergePrevious(bool m = true)  { mergeWithPrevious = m; return *this; }
  MetaListPost& limitBlockWidth(bool lim = true) { limitWidth = lim; return *this; }
  MetaListPost& packWithPrevious(bool p = true)  { packPrevious = p; return *this; }
  MetaListPost& indent(int ind)               { minimalIndent = ind; return *this; }
  MetaListPost& setType(EPostType t)          { type = t; return *this; }
  void          setLeg(int l)                 { leg = l; }

  EPostType     getTypeRaw()        const { return type; }
  EPostType     getAlignType()      const { return alignType; }
  const std::wstring& getText()     const { return text; }
  const std::string&  getResultModule() const { return resultModule; }
  const std::wstring& getAlignText()    const { return alignWithText; }
  int  getLeg()                     const { return leg; }
  int  getMinimalIndent()           const { return minimalIndent; }
  int  getBlockWidth()              const { return blockWidth; }
  bool getLimitBlockWidth()         const { return limitWidth; }
  bool getPackWithPrevious()        const { return packPrevious; }
  bool isMergePrevious()            const { return mergeWithPrevious; }

  friend class MetaList;
};

// ---------------------------------------------------------------------------
// MetaList — one list definition (column specs + type + sort + filters)
// XML-serialization and GDI-rendering methods excluded.
// ---------------------------------------------------------------------------
class MetaList {
public:
  enum ListIndex { MLHead = 0, MLSubHead = 1, MLList = 2, MLSubList = 3 };

private:
  // data[listIndex][row][column]
  std::vector<std::vector<std::vector<MetaListPost>>> data;

  std::wstring listName;
  std::string  tag;

  oListInfo::EBaseType listType    = oListInfo::EBaseTypeNone;
  oListInfo::EBaseType listSubType = oListInfo::EBaseTypeNone;
  SortOrder            sortOrder   = SortByName;

  std::set<EFilterList>    filter;
  std::set<ESubFilterList> subFilter;

  std::string  resultModule;
  bool supportFromControl = false;
  bool supportToControl   = false;
  bool hideLegSelection   = false;
  mutable bool hasResults_ = false;

  MetaListPost& add(ListIndex ix, const MetaListPost& post);
  void addRow(int ix);

public:
  MetaList();
  virtual ~MetaList() = default;

  // List type / sort
  MetaList& setListType(oListInfo::EBaseType t)    { listType = t;    return *this; }
  MetaList& setSubListType(oListInfo::EBaseType t)  { listSubType = t; return *this; }
  MetaList& setSortOrder(SortOrder so)              { sortOrder = so;  return *this; }
  MetaList& setSupportFromTo(bool from, bool to)    { supportFromControl = from; supportToControl = to; return *this; }
  MetaList& setSupportLegSelection(bool s)          { hideLegSelection = !s; return *this; }
  MetaList& setResultModule(const std::string& mod) { resultModule = mod; return *this; }
  MetaList& setListName(const std::wstring& title)  { listName = title; return *this; }

  oListInfo::EBaseType getListType()    const { return listType; }
  oListInfo::EBaseType getSubListType() const { return listSubType; }
  SortOrder            getSortOrder()   const { return sortOrder; }
  bool supportFrom()       const { return supportFromControl; }
  bool supportTo()         const { return supportToControl; }
  bool supportLegSelection() const { return !hideLegSelection; }
  bool hasResults()        const { return hasResults_; }
  const std::string&  getTag()      const { return tag; }
  const std::wstring& getListName() const { return listName; }
  const std::string&  getResultModule() const { return resultModule; }

  oListInfo::ResultType getResultType() const;

  // Filters
  MetaList& addFilter(EFilterList f)       { filter.insert(f);    return *this; }
  MetaList& addSubFilter(ESubFilterList f) { subFilter.insert(f); return *this; }
  bool hasFilter(EFilterList f)       const { return filter.count(f) > 0; }
  bool hasSubFilter(ESubFilterList f) const { return subFilter.count(f) > 0; }

  // Row/column construction
  void newListRow()    { addRow(MLList); }
  void newSubListRow() { addRow(MLSubList); }
  void newHead()       { addRow(MLHead); }
  void newSubHead()    { addRow(MLSubHead); }

  MetaListPost& addToHead(const MetaListPost& p)    { return add(MLHead,    p); }
  MetaListPost& addToSubHead(const MetaListPost& p) { return add(MLSubHead, p).setBlock(10); }
  MetaListPost& addToList(const MetaListPost& p)    { return add(MLList,    p); }
  MetaListPost& addToSubList(const MetaListPost& p) { return add(MLSubList, p); }

  MetaListPost& addNew(int groupIx, int lineIx, int& ix);
  MetaListPost& getMLP(int groupIx, int lineIx, int ix);
  void removeMLP(int groupIx, int lineIx, int ix);
  void moveOnRow(int groupIx, int lineIx, int& ix, int delta);

  const std::vector<std::vector<MetaListPost>>& getList()    const { return data[MLList]; }
  const std::vector<std::vector<MetaListPost>>& getSubList() const { return data[MLSubList]; }
  const std::vector<std::vector<MetaListPost>>& getHead()    const { return data[MLHead]; }
  const std::vector<std::vector<MetaListPost>>& getSubHead() const { return data[MLSubHead]; }

  std::vector<std::vector<MetaListPost>>& getList()    { return data[MLList]; }
  std::vector<std::vector<MetaListPost>>& getSubList() { return data[MLSubList]; }
  std::vector<std::vector<MetaListPost>>& getHead()    { return data[MLHead]; }
  std::vector<std::vector<MetaListPost>>& getSubHead() { return data[MLSubHead]; }

  bool isValidIx(size_t gIx, size_t lIx, size_t ix) const;
  int  getNumPostsOnLine(int groupIx, int lineIx) const;

  static bool isAllStageType(EPostType type) {
    return type == lRunnerStagePlace || type == lRunnerStageStatus ||
           type == lRunnerStageTime  || type == lRunnerStageTimeStatus ||
           type == lRunnerStagePoints || type == lRunnerStageNumber;
  }
  static bool isAllLegType(EPostType type) {
    return type == lTeamCourseName || type == lTeamCourseNumber ||
           type == lTeamLegName    || type == lTeamRunner ||
           type == lTeamRunnerCard || type == lTeamLegTimeStatus ||
           type == lTeamLegTimeAfter || type == lTeamPlace || type == lTeamStart;
  }
  static bool isResultModuleOutput(EPostType type) {
    return type == lResultModuleNumber || type == lResultModuleTime ||
           type == lResultModuleTimeTeam || type == lResultModuleNumberTeam;
  }
  static bool isLegBased(EPostType type) {
    return !isResultModuleOutput(type) && !isAllStageType(type) &&
           type != lRunnerAnnotation && type != lTeamAnnotation;
  }

  friend class MetaListPost;
};

// ---------------------------------------------------------------------------
// MetaListContainer — collection of MetaList objects for an event
// XML-serialization, GDI rendering, and legacy list-builder excluded.
// ---------------------------------------------------------------------------
class MetaListContainer {
public:
  enum MetaListType { InternalList, ExternalList, RemovedList };

private:
  std::vector<std::pair<MetaListType, MetaList>> data;
  oEvent* owner;

public:
  explicit MetaListContainer(oEvent* owner);
  MetaListContainer(oEvent* owner, const MetaListContainer& src);
  virtual ~MetaListContainer() = default;

  int getNumLists() const                         { return static_cast<int>(data.size()); }
  int getNumLists(MetaListType t) const;

  const MetaList& getList(int index) const        { return data.at(index).second; }
  MetaList&       getList(int index)              { return data.at(index).second; }

  MetaList& addExternal(const MetaList& ml);
  void      clearExternal();

  bool isInternal(int i) const { return data.at(i).first == InternalList; }
  bool isExternal(int i) const { return data.at(i).first == ExternalList; }

  void removeList(int index);
  void saveList(int index, const MetaList& ml);
};

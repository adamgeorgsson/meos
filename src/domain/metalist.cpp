/************************************************************************
    MeOS - Orienteering Software
    Migrated to src/domain/ — Win32/GDI/XML rendering excluded.
************************************************************************/

#include "metalist.h"
#include "oListInfo.h"
#include <stdexcept>

using namespace std;

// ---------------------------------------------------------------------------
// oListParam
// ---------------------------------------------------------------------------

oListParam::oListParam() {
  listCode            = EStdNone;
  useControlIdResultTo   = 0;
  useControlIdResultFrom = 0;
  filterMaxPer        = 0;
  pageBreak           = false;
  showHeader          = true;
  showInterTimes      = false;
  showSplitTimes      = false;
  splitAnalysis       = false;
  showInterTitle      = false;
  inputNumber         = 0;
  nextList            = 0;
  previousList        = 0;
  useLargeSize        = false;
  saved               = false;
  nColumns            = 1;
  legNumber           = 0;
  ageFilter           = AgeFilter::All;
  relayLegIndex       = -1;
}

const wstring& oListParam::getCustomTitle(const wstring& t) const {
  return title.empty() ? t : title;
}

bool oListParam::matchLegNumber(const oClass* /*cls*/, int leg) const {
  if (legNumber < 0) return true;
  return leg == legNumber;
}

int oListParam::getLegNumber(const oClass* /*cls*/) const {
  return legNumber < 0 ? -1 : legNumber;
}

pair<int,bool> oListParam::getLegInfo(const oClass* cls) const {
  return {getLegNumber(cls), legNumber >= 0};
}

wstring oListParam::getLegName() const {
  if (legNumber < 0)
    return L"All";
  return to_wstring(legNumber + 1);
}

// ---------------------------------------------------------------------------
// oListInfo
// ---------------------------------------------------------------------------

bool oListInfo::empty(bool includeHeader) const {
  // In the domain stub the rendering vectors don't exist; always return true.
  (void)includeHeader;
  return true;
}

// ---------------------------------------------------------------------------
// MetaList
// ---------------------------------------------------------------------------

MetaList::MetaList() {
  // Four groups: Head, SubHead, List, SubList
  data.resize(4);
  for (auto& g : data) {
    g.emplace_back(); // start with one empty row each
  }
}

void MetaList::addRow(int ix) {
  data.at(ix).emplace_back();
}

MetaListPost& MetaList::add(ListIndex ix, const MetaListPost& post) {
  auto& rows = data.at(ix);
  if (rows.empty()) rows.emplace_back();
  rows.back().push_back(post);
  if (post.getTypeRaw() != lNone && post.getTypeRaw() != lAlignNext)
    hasResults_ = true;
  return rows.back().back();
}

MetaListPost& MetaList::addNew(int groupIx, int lineIx, int& ix) {
  auto& rows = data.at(groupIx);
  while (static_cast<int>(rows.size()) <= lineIx) rows.emplace_back();
  rows[lineIx].emplace_back();
  ix = static_cast<int>(rows[lineIx].size()) - 1;
  return rows[lineIx].back();
}

MetaListPost& MetaList::getMLP(int groupIx, int lineIx, int ix) {
  return data.at(groupIx).at(lineIx).at(ix);
}

void MetaList::removeMLP(int groupIx, int lineIx, int ix) {
  auto& row = data.at(groupIx).at(lineIx);
  if (ix >= 0 && ix < static_cast<int>(row.size()))
    row.erase(row.begin() + ix);
}

void MetaList::moveOnRow(int groupIx, int lineIx, int& ix, int delta) {
  auto& row = data.at(groupIx).at(lineIx);
  int newIx = ix + delta;
  if (newIx >= 0 && newIx < static_cast<int>(row.size())) {
    std::swap(row[ix], row[newIx]);
    ix = newIx;
  }
}

bool MetaList::isValidIx(size_t gIx, size_t lIx, size_t ix) const {
  if (gIx >= data.size()) return false;
  if (lIx >= data[gIx].size()) return false;
  return ix < data[gIx][lIx].size();
}

int MetaList::getNumPostsOnLine(int groupIx, int lineIx) const {
  const auto& rows = data.at(groupIx);
  if (lineIx < 0 || lineIx >= static_cast<int>(rows.size())) return 0;
  return static_cast<int>(rows[lineIx].size());
}

oListInfo::ResultType MetaList::getResultType() const {
  // Default: classwise for runner/team lists, global otherwise
  if (listType == oListInfo::EBaseTypeRunnerGlobal || listType == oListInfo::EBaseTypeTeamGlobal)
    return oListInfo::Global;
  if (listType == oListInfo::EBaseTypeRunnerLeg || listType == oListInfo::EBaseTypeRGLeg)
    return oListInfo::Legwise;
  if (listType == oListInfo::EBaseTypeCourse)
    return oListInfo::Coursewise;
  return oListInfo::Classwise;
}

// ---------------------------------------------------------------------------
// MetaListContainer
// ---------------------------------------------------------------------------

MetaListContainer::MetaListContainer(oEvent* ownerIn) : owner(ownerIn) {}

MetaListContainer::MetaListContainer(oEvent* ownerIn, const MetaListContainer& src)
  : data(src.data), owner(ownerIn) {}

int MetaListContainer::getNumLists(MetaListType t) const {
  int count = 0;
  for (const auto& p : data)
    if (p.first == t) ++count;
  return count;
}

MetaList& MetaListContainer::addExternal(const MetaList& ml) {
  data.emplace_back(ExternalList, ml);
  return data.back().second;
}

void MetaListContainer::clearExternal() {
  data.erase(
    std::remove_if(data.begin(), data.end(),
                   [](const auto& p) { return p.first == ExternalList; }),
    data.end());
}

void MetaListContainer::removeList(int index) {
  data.at(index).first = RemovedList;
}

void MetaListContainer::saveList(int index, const MetaList& ml) {
  data.at(index).second = ml;
}

// qualification_final.cpp — Domain migration of code/qualification_final.cpp
// Excluded (belong to io/ui/runner layers):
//   exportXML, importXML   → xmlparser (io layer)
//   printScheme            → gdioutput (ui layer)
//   provideQualificationResult, provideUnqualified, computeFinals, getNextFinal
//                          → oRunner results (deferred to oRunner migration story)

#include "qualification_final.h"
#include "domain_header.h"
#include <algorithm>
#include <stdexcept>

using namespace std;

// ── Local helpers ─────────────────────────────────────────────────────────────

static wstring meosW(const string& s) {
  return wstring(s.begin(), s.end());
}

// Throw a domain exception from a wstring message.
[[noreturn]] static void throwEx(const wstring& msg) {
  throw runtime_error(string(msg.begin(), msg.end()));
}

// ── QFClass helpers ───────────────────────────────────────────────────────────

wstring QFClass::serialExtra() const {
  if (extraQualification == ExtraQualType::All)
    return L"A;";
  if (extraQualification == ExtraQualType::TimeLimit)
    return L"L" + itow(extraQualData) + L";";
  if (extraQualification == ExtraQualType::NBest)
    return L"B" + itow(extraQualData) + L";";
  return L"";
}

int QFClass::getMinQualInst() const {
  if (qualificationMap.empty()) return -1;
  int ret = 1024;
  for (auto& cp : qualificationMap)
    ret = min(ret, cp.first);
  return ret;
}

wstring QFClass::getQualInfo() const {
  if (extraQualification == ExtraQualType::All)
    return L"Alla övriga";
  if (extraQualification == ExtraQualType::NBest)
    return L"X bästa#" + itow(extraQualData);
  if (extraQualification == ExtraQualType::TimeLimit)
    return L"Tidsgräns X#" + formatTimeMS(extraQualData, false);

  int nq = (int)qualificationMap.size() + numTimeQualifications;
  if (nq > 0 && numTimeQualifications == 0)
    return L"X kvalificerade#" + itow(nq);
  if (nq > 0)
    return L"X kvalificerade#" + itow((int)qualificationMap.size()) +
           L" + " + itow(numTimeQualifications);
  return L"Ingen";
}

// ── QualificationFinal private helpers ───────────────────────────────────────

/*static*/ wstring QualificationFinal::validName(const wstring& name) {
  wstring out;
  out.reserve(name.size());
  for (wchar_t c : name)
    if (isValidNameChar(c)) out.push_back(c);
  return out;
}

/*static*/ bool QualificationFinal::isValidNameChar(wchar_t c) {
  return c != 0 && c != L'|' && c != L'@';
}

pair<int,int> QualificationFinal::getPrelFinalFromPlace(int instance,
                                                         int orderPlace,
                                                         int numSharedPlaceNext) {
  pair<int,int> key(instance, orderPlace);
  int iter = 0;
  while (numSharedPlaceNext >= 0) {
    auto res = sourcePlaceToFinalOrder_.find(key);
    if (res != sourcePlaceToFinalOrder_.end()) {
      if (iter >= 2) {
        pair<int,int> key2 = key;
        int extraSub = ((iter + 1) % 2);
        key2.second -= extraSub;
        auto res2 = sourcePlaceToFinalOrder_.find(key2);
        if (res2 != sourcePlaceToFinalOrder_.end()) {
          auto ans = res2->second;
          ans.second += iter + extraSub;
          return ans;
        }
      }
      auto ans = res->second;
      ans.second += iter;
      return ans;
    }
    --key.second;
    --numSharedPlaceNext;
    ++iter;
  }
  return make_pair(0, -1);
}

void QualificationFinal::initgmap(bool check) {
  level2DefiningInstance_.clear();
  sourcePlaceToFinalOrder_.clear();
  levels_.clear();
  levels_.resize((size_t)getNumLevels(), LevelInfo::Normal);

  for (int ix = 0; ix < (int)classDefinition_.size(); ix++) {
    auto& c = classDefinition_[ix];
    if (c.rankLevel)
      levels_[(size_t)getLevel(ix + 1)] = LevelInfo::RankSort;

    for (int k = 0; k < (int)c.qualificationMap.size(); k++) {
      const pair<int,int>& sd = c.qualificationMap[k];
      if (check && sourcePlaceToFinalOrder_.count(sd))
        throwEx(L"Inconsistent qualification rule, X#" + c.name + L"/" + itow(sd.first));
      sourcePlaceToFinalOrder_[sd] = make_pair(ix + 1, k);
    }
  }
}

// ── Public interface ──────────────────────────────────────────────────────────

int QualificationFinal::getLevel(int instance) const {
  if (instance == 0) return 0;
  instance--;
  if (classDefinition_[instance].level >= 0)
    return classDefinition_[instance].level;
  int level = 0;
  int src = instance;
  while (!classDefinition_[instance].qualificationMap.empty()) {
    instance = classDefinition_[instance].qualificationMap.front().first - 1;
    if (instance < 0) break;
    level++;
    if (size_t(level) > classDefinition_.size())
      throwEx(L"Internal error in QualificationFinal::getLevel");
  }
  classDefinition_[src].level = level;
  return level;
}

int QualificationFinal::getNumLevels() const {
  return getLevel((int)classDefinition_.size()) + 1;
}

bool QualificationFinal::isFinalClass(int instance) const {
  return instance > 0 && getLevel(instance) + 1 == getNumLevels();
}

int QualificationFinal::getMinInstance(const int levelIn) const {
  int level = levelIn;
  auto res = level2DefiningInstance_.find(levelIn);
  if (res != level2DefiningInstance_.end())
    return res->second;

  int minInst = (int)classDefinition_.size() - 1;
  for (int i = (int)classDefinition_.size() - 1; i >= 0; i--) {
    if (i == 0 && minInst == 1) break;
    int thisLevel = getLevel(i + 1);
    if (thisLevel > level) {
      int minQualInst = classDefinition_[i].getMinQualInst();
      if (minQualInst >= 0) {
        int qualLevel = getLevel(minQualInst + 1);
        if (qualLevel < level) {
          level = qualLevel;
          i = (int)classDefinition_.size(); // restart
        }
      }
    } else if (thisLevel == level) {
      minInst = i;
    }
  }
  level2DefiningInstance_[levelIn] = minInst;
  return minInst;
}

bool QualificationFinal::noQualification(int instance) const {
  if (size_t(instance) >= classDefinition_.size()) return false;
  if (instance > 0 &&
      classDefinition_[instance].level > classDefinition_[instance - 1].level)
    return false;
  return classDefinition_[instance].qualificationMap.empty() &&
         classDefinition_[instance].numTimeQualifications == 0 &&
         classDefinition_[instance].extraQualification == QFClass::ExtraQualType::None;
}

void QualificationFinal::getBaseClassInstances(set<int>& base) const {
  for (size_t k = 0; k < classDefinition_.size(); k++) {
    if (noQualification((int)k))
      base.insert((int)k + 1);
    else
      break;
  }
}

bool QualificationFinal::hasRemainingClass() const {
  for (auto& c : classDefinition_)
    if (c.extraQualification != QFClass::ExtraQualType::None) return true;
  return false;
}

int QualificationFinal::getHeatFromClass(int finalClassId, int baseClassId) const {
  if (baseClassId == baseId_) {
    int fci = (finalClassId - baseId_) / maxClassId_;
    if (fci * maxClassId_ + baseClassId == finalClassId)
      return fci;
    return -1;
  }
  return 0;
}

void QualificationFinal::setClasses(const vector<QFClass>& def) {
  classDefinition_ = def;
  initgmap(true);
  wstring tmp;
  encode(tmp);
}

void QualificationFinal::encode(wstring& output) const {
  output.clear();
  for (size_t k = 0; k < classDefinition_.size(); k++) {
    if (k > 0) output += L"|";
    output += L"@" + itow(getLevel((int)k + 1)) + L"@" +
              validName(classDefinition_[k].name) + L"@";
    if (classDefinition_[k].rankLevel) output += L"R";
    output += classDefinition_[k].serialExtra();

    const auto& qm = classDefinition_[k].qualificationMap;
    for (size_t j = 0; j < qm.size(); j++) {
      if (j > 0) output += L";";
      size_t i = j;
      while ((i + 1) < qm.size() && qm[i + 1].first == qm[i].first &&
             qm[i + 1].second == qm[i].second + 1)
        i++;
      output += itow(qm[j].first) + L";";
      if (i <= j + 1)
        output += itow(qm[j].second);
      else {
        output += itow(qm[j].second) + L"-" + itow(qm[i].second);
        j = i;
      }
    }
    if (classDefinition_[k].numTimeQualifications > 0)
      output += L"T" + itow(classDefinition_[k].numTimeQualifications);
  }
  serializedFrom_ = output;
}

void QualificationFinal::init(const wstring& def) {
  serializedFrom_ = def;

  vector<wstring> races;
  splitW(def, L"|", races);
  classDefinition_.resize(races.size());

  bool valid = true;
  bool first = true;

  for (size_t k = 0; k < races.size(); k++) {
    classDefinition_[k].level = -1;
    classDefinition_[k].rankLevel = false;

    if (!races[k].empty() && races[k][0] == L'@') {
      // Explicit level encoding since 4.0: @level@name@...
      size_t mx = 1;
      while (mx < races[k].size() && races[k][mx] != L'@') mx++;
      size_t mx2 = mx + 1;
      while (mx2 < races[k].size() && races[k][mx2] != L'@') mx2++;

      classDefinition_[k].level = wtoi(races[k].c_str() + 1);
      if (mx2 >= races[k].size() || classDefinition_[k].level > (int)k) {
        valid = false;
        break;
      }
      classDefinition_[k].name = races[k].substr(mx + 1, mx2 - mx - 1);
      races[k] = races[k].substr(mx2 + 1);
    }

    bool rankLevel = false;
    if (!races[k].empty() && races[k][0] == L'R') {
      rankLevel = true;
      races[k] = races[k].substr(1);
    }

    if (!races[k].empty() &&
        QFClass::deserialType(races[k][0]) != QFClass::ExtraQualType::None) {
      first = false;
      classDefinition_[k].extraQualification = QFClass::deserialType(races[k][0]);
      if (classDefinition_[k].extraQualification == QFClass::ExtraQualType::NBest ||
          classDefinition_[k].extraQualification == QFClass::ExtraQualType::TimeLimit) {
        classDefinition_[k].extraQualData = wtoi(races[k].c_str() + 1);
      }
      size_t end = 1;
      while (end < races[k].size() && races[k][end - 1] != L';') end++;
      races[k] = races[k].substr(end);
    }

    classDefinition_[k].rankLevel = rankLevel;
    classDefinition_[k].qualificationMap.clear();
    classDefinition_[k].numTimeQualifications = 0;

    // Split on "T" to separate place-qualifications from time-qualifications
    vector<wstring> rtdef;
    splitW(races[k], L"T", rtdef);

    if (rtdef.empty()) continue;

    first = false;

    vector<wstring> rdef;
    splitW(rtdef[0], L";", rdef);
    bool thisValid = (rdef.size() % 2 == 0) &&
                     (!rdef.empty() || (rtdef.size() == 2 && !rtdef[1].empty()));

    if (!thisValid) continue;

    for (size_t j = 0; j < rdef.size(); j += 2) {
      int src = wtoi(rdef[j].c_str());
      if (src > (int)k || src <= 0) { thisValid = false; break; }
      const wstring& rd = rdef[j + 1];
      int d1 = wtoi(rd.c_str());
      if (d1 < 1 || d1 > 1000) { thisValid = false; break; }
      int d2 = d1;
      size_t range = rd.find_first_of(L'-');
      if (range < rd.size())
        d2 = wtoi(rd.c_str() + range + 1);
      if (d1 > d2) { thisValid = false; break; }
      while (d1 <= d2)
        classDefinition_[k].qualificationMap.emplace_back(src, d1++);
    }

    if (!thisValid) classDefinition_[k].qualificationMap.clear();

    if (rtdef.size() > 1)
      classDefinition_[k].numTimeQualifications = wtoi(rtdef[1].c_str());
  }

  if (!valid) classDefinition_.clear();

  initgmap(false);
}

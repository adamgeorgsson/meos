// qualification_final.cpp — Qualification/Final scheme implementation.
// Ported from code/qualification_final.cpp with Win32/gdioutput removed.
// Methods that require oRunner (computeFinals, provideQualificationResult, etc.)
// are stubbed until oRunner is fully implemented in US-003g.

#include "qualification_final.h"
#include "../util/meos_util.h"
#include "../util/meosexception.h"
#include "../util/xmlparser.h"
#include "../util/localizer.h"
#include <algorithm>

// ── Methods that reference oRunner are stubbed (oRunner is incomplete type) ──

void QualificationFinal::provideQualificationResult(pRunner r, int instance, int orderPlace, int numSharedPlace) {
  // Full implementation requires complete oRunner type — deferred to US-003g.
}

void QualificationFinal::provideUnqualified(int level, pRunner r) {
  // Full implementation requires complete oRunner type — deferred to US-003g.
}

void QualificationFinal::computeFinals() {
  // Full implementation requires oRunner::getRunningTime/getStatus/getRanking — deferred to US-003g.
  for (auto &si : storedInfo)
    for (auto &res : si)
      storedInfoLookup[res.r ? reinterpret_cast<int*>(res.r)[0] : 0] = &res;
}

pair<int, int> QualificationFinal::getNextFinal(int runnerId) const {
  auto res = storedInfoLookup.find(runnerId);
  if (res == storedInfoLookup.end())
    return make_pair(0, -1);
  return make_pair(max(res->second->instance, 0), res->second->order);
}

// ── Pure logic methods — full port ─────────────────────────────────────────

pair<int, int> QualificationFinal::getPrelFinalFromPlace(int instance, int orderPlace, int numSharedPlaceNext) {
  pair<int, int> key(instance, orderPlace);
  int iter = 0;
  while (numSharedPlaceNext >= 0) {
    auto res = sourcePlaceToFinalOrder.find(key);
    if (res != sourcePlaceToFinalOrder.end()) {
      if (iter >= 2) {
        pair<int, int> key2 = key;
        int extraSub = ((iter + 1) % 2);
        key2.second -= extraSub;
        auto res2 = sourcePlaceToFinalOrder.find(key2);
        if (res2 != sourcePlaceToFinalOrder.end()) {
          auto ans = res2->second;
          ans.second += iter + extraSub;
          ++numExtraAssigned[ans.first];
          return ans;
        }
      }
      auto ans = res->second;
      ans.second += iter;
      if (iter > 0)
        ++numExtraAssigned[ans.first];
      return ans;
    }
    --key.second;
    --numSharedPlaceNext;
    ++iter;
  }
  return make_pair(0, -1);
}

bool QualificationFinal::noQualification(int instance) const {
  if (size_t(instance) >= classDefinition.size())
    return false;
  if (instance > 0 && classDefinition[instance].level > classDefinition[instance - 1].level)
    return false;
  return classDefinition[instance].qualificationMap.empty() &&
         classDefinition[instance].numTimeQualifications == 0 &&
         classDefinition[instance].extraQualification == QFClass::ExtraQualType::None;
}

void QualificationFinal::getBaseClassInstances(set<int> &base) const {
  for (size_t k = 0; k < classDefinition.size(); k++) {
    if (noQualification(k))
      base.insert(k+1);
    else break;
  }
}

void QualificationFinal::exportXML(const wstring &file) const {
  xmlparser xml;
  xml.openOutputT(file.c_str(), false, "QualificationRules");
  int cLevel = -1;

  for (size_t j = 0; j < classDefinition.size(); j++) {
    int level = getLevel(j+1);
    if (level != cLevel) {
      if (cLevel >= 0)
        xml.endTag();
      cLevel = level;
      if (classDefinition[j].rankLevel)
        xml.startTag("Level", "distribution", "Ranking");
      else
        xml.startTag("Level");
    }

    vector<wstring> pv;
    pv.emplace_back(L"name");
    pv.emplace_back(classDefinition[j].name.empty() ? lang.tl(L"Kval") + itow(j + 1) : classDefinition[j].name);
    pv.emplace_back(L"id");
    pv.emplace_back(itow(j + 1));
    xml.startTag("Class", pv);

    vector<pair<string, wstring>> psv(2);
    psv[0].first = "id";
    psv[1].first = "place";

    for (auto &qmap : classDefinition[j].qualificationMap) {
      psv[0].second = itow(qmap.first);
      psv[1].second = itow(qmap.second);
      xml.write("Qualification", psv, L"");
    }

    if (classDefinition[j].numTimeQualifications > 0) {
      psv[0].first = "time";
      psv[0].second = itow(classDefinition[j].numTimeQualifications);
      psv.resize(1);
      xml.write("Qualification", psv, L"");
    }

    if (classDefinition[j].extraQualification != QFClass::ExtraQualType::None) {
      psv[0].first = "type";
      switch (classDefinition[j].extraQualification) {
      case QFClass::ExtraQualType::All:
        psv.resize(1);
        psv[0].second = L"All";
        break;
      case QFClass::ExtraQualType::NBest:
        psv.resize(2);
        psv[0].second = L"Best";
        psv[1].first = "number";
        psv[1].second = itow(classDefinition[j].extraQualData);
        break;
      case QFClass::ExtraQualType::TimeLimit:
        psv.resize(2);
        psv[0].second = L"Time";
        psv[1].first = "limit";
        psv[1].second = formatTimeMS(classDefinition[j].extraQualData, false);
        break;
      default:
        break;
      }
      xml.write("Remaining", psv, L"");
    }
    xml.endTag();
  }
  xml.closeOut();
}

void QualificationFinal::importXML(const wstring &file) {
  xmlparser xml;
  xml.read(file);

  auto qr = xml.getObject("QualificationRules");
  xmlList levels;
  qr.getObjects("Level", levels);
  map<int, int> idToIndex;
  map<int, set<int>> qualificationRelations;
  int numBaseLevels = 0;

  for (size_t iLevel = 0; iLevel < levels.size(); iLevel++) {
    auto &level = levels[iLevel];
    wstring rankS;
    level.getObjectString("distribution", rankS);
    bool rankSort = false;
    if (rankS == L"Ranking")
      rankSort = true;
    else if (!rankS.empty())
      throw meosException(L"Unknown distribution: " + rankS);

    xmlList classes;
    level.getObjects("Class", classes);
    for (auto &cls : classes) {
      wstring name;
      cls.getObjectString("name", name);
      if (name.empty())
        cls.getObjectString("Name", name);
      if (name.empty())
        throw meosException("Klassen måste ha ett namn.");

      int classId = cls.getObjectInt("id");
      if (!(classId > 0))
        throw meosException("Id must be a positive integer.");
      if (idToIndex.count(classId))
        throw meosException("Duplicate class with id " + itos(classId));

      xmlList rules;
      cls.getObjects("Qualification", rules);
      xmlobject remaining = cls.getObject("Remaining");

      if (rules.empty() && !remaining)
        numBaseLevels = 1;

      idToIndex[classId] = (int)classDefinition.size() + numBaseLevels;
      classDefinition.emplace_back();
      classDefinition.back().level = (int)iLevel;

      if (remaining) {
        wstring rtype;
        remaining.getObjectString("type", rtype);
        if (rtype == L"All") {
          classDefinition.back().extraQualification = QFClass::ExtraQualType::All;
        }
        else if (rtype == L"Best") {
          classDefinition.back().extraQualification = QFClass::ExtraQualType::NBest;
          classDefinition.back().extraQualData = remaining.getObjectInt("number");
          if (classDefinition.back().extraQualData > 10000 || classDefinition.back().extraQualData < 0)
            classDefinition.back().extraQualData = 0;
        }
        else if (rtype == L"Time") {
          classDefinition.back().extraQualification = QFClass::ExtraQualType::TimeLimit;
          wstring wt;
          remaining.getObjectString("limit", wt);
          classDefinition.back().extraQualData = convertAbsoluteTimeMS(wt);
          if (classDefinition.back().extraQualData == NOTIME || classDefinition.back().extraQualData < 0)
            classDefinition.back().extraQualData = 0;
        }
      }

      classDefinition.back().name = name;
      classDefinition.back().rankLevel = rankSort;

      for (auto &qf : rules) {
        int place = qf.getObjectInt("place");
        if (place > 0) {
          int id = qf.getObjectInt("id");
          if (id == 0 && iLevel == 0)
            classDefinition.back().qualificationMap.push_back(make_pair(0, place));
          else if (idToIndex.count(id)) {
            classDefinition.back().qualificationMap.push_back(make_pair(idToIndex[id], place));
            qualificationRelations[classId].insert(id);
          }
          else
            throw meosException("Unknown class with id " + itos(id));
        }
        else {
          int numTime = qf.getObjectInt("time");
          if (numTime > 0)
            classDefinition.back().numTimeQualifications += numTime;
          else if (qf.got("time"))
            throw meosException(L"Empty time qualification for " + name);
          else
            throw meosException(L"Unknown classification rule for " + name);
        }
      }
    }
  }
  initgmap(true);
}

void QualificationFinal::init(const wstring &def) {
  serializedFrom = def;
  vector<wstring> races, rtdef, rdef;
  split(def, L"|", races);
  classDefinition.resize(races.size());
  bool valid = true;
  bool first = true;

  for (size_t k = 0; k < races.size(); k++) {
    if (races[k].size() > 0 && races[k][0] == '@') {
      classDefinition[k].level = wtoi(races[k].c_str() + 1);
      int mx = 1;
      while (mx < (int)races[k].size() && races[k][mx] != '@')
        mx++;
      int mx2 = mx + 1;
      while (mx2 < (int)races[k].size() && races[k][mx2] != '@')
        mx2++;

      if (mx2 >= (int)races[k].size() || classDefinition[k].level > (int)k) {
        valid = false;
        break;
      }
      races[k][mx2] = 0;
      classDefinition[k].name = races[k].c_str() + mx + 1;
      races[k] = races[k].substr(mx2 + 1);
    }

    bool rankLevel = false;
    if (races[k].size() > 0 && races[k][0] == 'R') {
      rankLevel = true;
      races[k] = races[k].substr(1);
    }

    if (races[k].size() > 0 && QFClass::deserialType(races[k][0]) != QFClass::ExtraQualType::None) {
      first = false;
      classDefinition[k].extraQualification = QFClass::deserialType(races[k][0]);
      if (classDefinition[k].extraQualification == QFClass::ExtraQualType::NBest ||
          classDefinition[k].extraQualification == QFClass::ExtraQualType::TimeLimit) {
        classDefinition[k].extraQualData = wtoi(races[k].c_str() + 1);
      }
      int end = 1;
      while (end < (int)races[k].size() && races[k][end-1] != ';')
        end++;
      races[k] = races[k].substr(end);
    }

    split(races[k], L"T", rtdef);
    classDefinition[k].qualificationMap.clear();
    classDefinition[k].numTimeQualifications = 0;
    classDefinition[k].rankLevel = rankLevel;

    if (rtdef.empty())
      continue;

    first = false;
    split(rtdef[0], L";", rdef);
    bool thisValid = rdef.size() % 2 == 0 &&
                     (rdef.size() > 0 || (rtdef.size() == 2 && !rtdef[1].empty()));

    if (!thisValid)
      continue;

    for (size_t j = 0; j < rdef.size(); j += 2) {
      int src = wtoi(rdef[j].c_str());
      if (src > (int)k || src <= 0) {
        thisValid = false;
        break;
      }
      const wstring &rd = rdef[j + 1];
      int d1 = wtoi(rd.c_str());
      if (d1 < 1 || d1 > 1000) {
        thisValid = false;
        break;
      }
      int d2 = d1;
      size_t range = rd.find_first_of('-', 0);
      if (range < rd.size())
        d2 = wtoi(rd.c_str() + range + 1);
      if (d1 > d2) {
        thisValid = false;
        break;
      }
      while (d1 <= d2) {
        classDefinition[k].qualificationMap.emplace_back(src, d1);
        d1++;
      }
    }

    if (!thisValid)
      classDefinition[k].qualificationMap.clear();

    if (rtdef.size() > 1) {
      int numTime = wtoi(rtdef[1].c_str());
      classDefinition[k].numTimeQualifications = numTime;
    }
  }

  if (!valid)
    classDefinition.clear();

  initgmap(false);
}

void QualificationFinal::setClasses(const vector<QFClass>& def) {
  classDefinition = def;
  initgmap(true);
  wstring tmp;
  encode(tmp);
}

wstring QualificationFinal::validName(const wstring& name) {
  wstring out;
  out.reserve(name.size());
  for (size_t j = 0; j < name.length(); j++)
    if (isValidNameChar(name[j]))
      out.push_back(name[j]);
  return out;
}

bool QualificationFinal::isValidNameChar(wchar_t c) {
  return c != 0 && c != '|' && c != '@';
}

void QualificationFinal::encode(wstring &output) const {
  output.clear();
  for (size_t k = 0; k < classDefinition.size(); k++) {
    if (k > 0)
      output.append(L"|");
    output.append(L"@" + itow(getLevel(k+1)) + L"@" + validName(classDefinition[k].name) + L"@");
    if (classDefinition[k].rankLevel)
      output.append(L"R");
    output.append(classDefinition[k].serialExtra());

    auto &qm = classDefinition[k].qualificationMap;
    for (size_t j = 0; j < qm.size(); j++) {
      if (j > 0)
        output.append(L";");
      size_t i = j;
      while ((i + 1) < qm.size() && qm[i+1].first == qm[i].first && qm[i+1].second == qm[i].second+1)
        i++;
      output.append(itow(qm[j].first) + L";");
      if (i <= j + 1)
        output.append(itow(qm[j].second));
      else {
        output.append(itow(qm[j].second) + L"-" + itow(qm[i].second));
        j = i;
      }
    }
    if (classDefinition[k].numTimeQualifications > 0) {
      output.append(L"T");
      output.append(itow(classDefinition[k].numTimeQualifications));
    }
  }
  serializedFrom = output;
}

wstring QFClass::serialExtra() const {
  if (extraQualification == ExtraQualType::All)
    return L"A;";
  else if (extraQualification == ExtraQualType::TimeLimit)
    return L"L" + itow(extraQualData) + L";";
  else if (extraQualification == ExtraQualType::NBest)
    return L"B" + itow(extraQualData) + L";";
  return L"";
}

int QFClass::getMinQualInst() const {
  if (qualificationMap.empty())
    return -1;
  int ret = 1024;
  for (auto &cp : qualificationMap)
    ret = min(ret, cp.first);
  return ret;
}

wstring QFClass::getQualInfo() const {
  if (extraQualification == ExtraQualType::All)
    return lang.tl(L"Alla övriga");
  else if (extraQualification == ExtraQualType::NBest)
    return lang.tl(L"X bästa") + L"#" + itow(extraQualData);
  else if (extraQualification == ExtraQualType::TimeLimit)
    return lang.tl(L"Tidsgräns X") + L"#" + formatTimeMS(extraQualData, false);

  int nq = (int)qualificationMap.size() + numTimeQualifications;
  if (nq > 0 && numTimeQualifications == 0)
    return lang.tl(L"X kvalificerade") + L"#" + itow(nq);
  else if (nq > 0)
    return lang.tl(L"X kvalificerade") + L"#" + itow((int)qualificationMap.size()) + L" + " + itow(numTimeQualifications);
  return lang.tl(L"Ingen");
}

void QualificationFinal::initgmap(bool check) {
  level2DefningInstance.clear();
  sourcePlaceToFinalOrder.clear();
  levels.clear();
  levels.resize(getNumLevels(), LevelInfo::Normal);

  for (int ix = 0; ix < (int)classDefinition.size(); ix++) {
    auto &c = classDefinition[ix];
    if (c.rankLevel)
      levels[getLevel(ix+1)] = LevelInfo::RankSort;

    for (size_t k = 0; k < c.qualificationMap.size(); k++) {
      const pair<int, int> &sd = c.qualificationMap[k];
      if (check && sourcePlaceToFinalOrder.count(sd))
        throw meosException(L"Inconsistent qualification rule, X#" + c.name + L"/" + itow(sd.first));
      sourcePlaceToFinalOrder[sd] = make_pair(ix+1, (int)k);
    }
  }
}

int QualificationFinal::getLevel(int instance) const {
  if (instance == 0)
    return 0;
  instance--;
  if (classDefinition[instance].level >= 0)
    return classDefinition[instance].level;
  int level = 0;
  int src = instance;
  while (!classDefinition[instance].qualificationMap.empty()) {
    instance = classDefinition[instance].qualificationMap.front().first - 1;
    if (instance < 0)
      break;
    level++;
    if (size_t(level) > classDefinition.size())
      throw meosException("Internal error");
  }
  classDefinition[src].level = level;
  return level;
}

bool QualificationFinal::isFinalClass(int instance) const {
  return instance > 0 && getLevel(instance) + 1 == getNumLevels();
}

int QualificationFinal::getNumLevels() const {
  return getLevel((int)classDefinition.size()) + 1;
}

int QualificationFinal::getMinInstance(const int levelIn) const {
  int level = levelIn;
  auto res = level2DefningInstance.find(levelIn);
  if (res != level2DefningInstance.end())
    return res->second;

  int minInst = (int)classDefinition.size() - 1;
  for (int i = (int)classDefinition.size() - 1; i >= 0; i--) {
    if (i == 0 && minInst == 1)
      break;
    int thisLevel = getLevel(i+1);
    if (thisLevel > level) {
      int minQualInst = classDefinition[i].getMinQualInst();
      if (minQualInst >= 0) {
        int qualLevel = getLevel(minQualInst+1);
        if (qualLevel < level) {
          level = qualLevel;
          i = (int)classDefinition.size();
        }
      }
    }
    else if (thisLevel == level)
      minInst = i;
  }

  level2DefningInstance[levelIn] = minInst;
  return minInst;
}

int QualificationFinal::getHeatFromClass(int finalClassId, int baseClassId) const {
  if (baseClassId == baseId) {
    int fci = (finalClassId - baseId) / maxClassId;
    if (fci * maxClassId + baseClassId == finalClassId)
      return fci;
    else
      return -1;
  }
  return 0;
}

void QualificationFinal::prepareCalculations() {
  storedInfoLookup.clear();
  storedInfo.clear();
  numExtraAssigned.clear();
}

bool QualificationFinal::hasRemainingClass() const {
  for (auto &c : classDefinition)
    if (c.extraQualification != QFClass::ExtraQualType::None)
      return true;
  return false;
}

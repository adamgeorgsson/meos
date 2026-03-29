// oRunner.h — Minimal oRunner stub for domain-layer compilation.
// Full implementation comes in US-003g.
#pragma once

#include "domain_header.h"

class oClass;
class oCourse;
typedef oCourse* pCourse;

class oTeam;
typedef oTeam* pTeam;

class oRunner {
public:
  int Id = 0;
  int FinishTime = 0;
  int tStatus = 0;
  int status = 0;
  oClass* Class = nullptr;

  void changedObject() {}
  void markClassChanged(int /*controlId*/) {}
  int getCardNo() const { return 0; }
  int getId() const { return Id; }
  int getRaceNo() const { return 0; }
  pCourse getCourse(bool /*allowVirtual*/) const { return nullptr; }
  wstring getName() const { return L""; }
  wstring getClass(bool /*check*/) const { return L""; }
  pTeam getTeam() const { return nullptr; }
  wstring getNameAndRace(bool /*withRace*/) const { return L""; }
  bool isRemoved() const { return false; }
  bool isChanged() const { return false; }
  int getStatus() const { return 0; }
  int getFinishTime() const { return 0; }
  oClass* getClassRef(bool /*check*/) const { return nullptr; }
  void setStartTime(int /*t*/, bool /*updateRace*/, int /*changeType*/) {}
  void setFinishTime(int /*t*/) {}
  void synchronize(bool /*writeOnly*/ = false) {}
  pCourse getCourse(bool /*allowVirtual*/, bool /*forceUpdate*/) const { return nullptr; }
};
typedef oRunner* pRunner;

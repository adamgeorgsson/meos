// IofXml.cpp — IOF 3.0 XML import/export (platform-independent).
#include "IofXml.h"

#include "oEvent.h"
#include "oRunner.h"
#include "oClub.h"
#include "oClass.h"
#include "oCourse.h"
#include "oControl.h"
#include "oAbstractRunner.h"
#include "meos_util.h"
#include "xmlparser.h"
#include "timeconstants.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <ctime>
#include <sstream>
#include <iomanip>

using std::string;
using std::wstring;
using std::vector;
using std::set;

// ── Status mapping ────────────────────────────────────────────────────────────

string iofStatusFromRunner(int st) {
  switch (st) {
    case StatusOK:               return "OK";
    case StatusDNF:              return "DidNotFinish";
    case StatusMP:               return "MissingPunch";
    case StatusDQ:               return "Disqualified";
    case StatusMAX:              return "OverTime";
    case StatusDNS:              return "DidNotStart";
    case StatusCANCEL:           return "Cancelled";
    case StatusNotCompeting:     return "NotCompeting";
    case StatusOutOfCompetition: return "NotCompeting";
    case StatusNoTiming:         return "OK";
    default:                     return "Active";
  }
}

int iofStatusToRunner(const string& s) {
  if (s == "OK")              return StatusOK;
  if (s == "DidNotFinish")    return StatusDNF;
  if (s == "MissingPunch")    return StatusMP;
  if (s == "Disqualified")    return StatusDQ;
  if (s == "OverTime")        return StatusMAX;
  if (s == "DidNotStart")     return StatusDNS;
  if (s == "Cancelled")       return StatusCANCEL;
  if (s == "NotCompeting")    return StatusNotCompeting;
  return StatusUnknown;
}

// ── IofXmlInterface ───────────────────────────────────────────────────────────

IofXmlInterface::IofXmlInterface(oEvent& oe) : oe(oe) {}

// ── Time helpers ──────────────────────────────────────────────────────────────

/// Strip timezone suffix from ISO 8601 time string (e.g. "10:30:00+01:00" → "10:30:00").
string IofXmlInterface::stripTimezone(const string& iso) {
  if (iso.empty()) return iso;
  // Find +/- after the time digits (position >= 6)
  for (size_t i = 6; i < iso.size(); i++) {
    if (iso[i] == '+' || iso[i] == '-' || iso[i] == 'Z')
      return iso.substr(0, i);
  }
  return iso;
}

/// Parse "YYYY-MM-DDTHH:MM:SS[Z|+HH:MM]" or "HH:MM:SS[...]" → absolute seconds.
/// Returns the seconds-of-day component, ignoring timezone (local interpretation).
int IofXmlInterface::parseAbsTime(const string& datetime) const {
  if (datetime.empty()) return -1;
  // Find 'T' separator — split date and time parts
  size_t tpos = datetime.find('T');
  string timeStr = (tpos != string::npos) ? datetime.substr(tpos + 1) : datetime;
  timeStr = stripTimezone(timeStr);
  return convertAbsoluteTimeISO(wstring(timeStr.begin(), timeStr.end()));
}

/// Format absolute time units as "YYYY-MM-DDTHH:MM:SS".
wstring IofXmlInterface::absTimeISO(int timeUnits) const {
  wstring date = oe.getDate();
  if (date.empty()) date = L"2000-01-01";

  int h = timeUnits / timeConstHour;
  int m = (timeUnits % timeConstHour) / timeConstMinute;
  int s = (timeUnits % timeConstMinute) / timeConstSecond;

  wchar_t buf[32];
  swprintf(buf, sizeof(buf)/sizeof(buf[0]), L"T%02d:%02d:%02d", h, m, s);
  return date + wstring(buf);
}

// ── Import: CourseData ────────────────────────────────────────────────────────

void IofXmlInterface::readCourseData(const xmlobject& xo, int& courseCount, int& failed) {
  courseCount = 0;
  failed = 0;
  if (!xo) return;

  // Find RaceCourseData child (or use xo directly if it's already RaceCourseData)
  xmlobject xRCD = xo.getObject("RaceCourseData");
  const xmlobject& xData = xRCD ? xRCD : xo;

  // 1. Import Controls
  xmlList xControls;
  xData.getObjects("Control", xControls);
  for (const auto& xCtrl : xControls) {
    string idStr;
    xCtrl.getObjectString("Id", idStr);
    if (idStr.empty()) continue;

    // Skip Start/Finish controls (S*, F*)
    if (!idStr.empty() && (idStr[0] == 'S' || idStr[0] == 'F')) continue;

    int code = atoi(idStr.c_str());
    if (code <= 0) continue;

    // getControl(id, create=true, includeVirtual=false)
    pControl pc = oe.getControl(code, true, false);
    if (pc) {
      wstring wname;
      xCtrl.getObjectString("Name", wname);
      if (!wname.empty()) pc->setName(wname);
    }
  }

  // 2. Import Courses
  xmlList xCourses;
  xData.getObjects("Course", xCourses);
  for (const auto& xcrs : xCourses) {
    wstring name;
    xcrs.getObjectString("Name", name);
    if (name.empty()) continue;

    int len = xcrs.getObjectInt("Length");

    // Look up existing course by name, or create new
    vector<pCourse> allCrs;
    oe.getCourses(allCrs);
    pCourse pc = nullptr;
    for (pCourse c : allCrs) {
      if (c->getName() == name) { pc = c; break; }
    }
    if (!pc) {
      int id = oe.getFreeCourseId();
      pc = oe.addCourse(name, len, id);
    }

    if (!pc) { failed++; continue; }

    if (len > 0) {
      // Length was passed to addCourse; update if already exists
    }

    // Parse CourseControls
    xmlList xCC;
    xcrs.getObjects("CourseControl", xCC);
    string ctrlStr;
    for (const auto& xc : xCC) {
      string type;
      xc.getObjectString("type", type);
      if (type == "Start" || type == "Finish") continue;

      // Control id can be a child element or attribute
      string ctrlId;
      xmlobject xCtrlEl = xc.getObject("Control");
      if (xCtrlEl) {
        xCtrlEl.getObjectString("", ctrlId);
        if (ctrlId.empty()) {
          // Try as int child
          int ci = xCtrlEl.getInt();
          if (ci > 0) ctrlId = std::to_string(ci);
        }
      }
      if (ctrlId.empty()) {
        xc.getObjectString("Control", ctrlId);
      }
      if (ctrlId.empty()) continue;
      if (!ctrlStr.empty()) ctrlStr += ";";
      ctrlStr += ctrlId;
    }
    if (!ctrlStr.empty())
      pc->importControls(ctrlStr, true, false);

    courseCount++;
  }

  // 3. Class–Course assignments
  xmlList xAssign;
  xData.getObjects("ClassCourseAssignment", xAssign);
  for (const auto& xa : xAssign) {
    wstring className;
    xa.getObjectString("ClassName", className);
    wstring courseName;
    xa.getObjectString("CourseName", courseName);

    if (className.empty() || courseName.empty()) continue;

    // Find class
    vector<pClass> classes;
    oe.getClasses(classes, false);
    pClass pc = nullptr;
    for (pClass c : classes) {
      if (c->getName() == className) { pc = c; break; }
    }
    if (!pc) continue;

    // Find course
    vector<pCourse> allCrs;
    oe.getCourses(allCrs);
    pCourse pCrs = nullptr;
    for (pCourse c : allCrs) {
      if (c->getName() == courseName) { pCrs = c; break; }
    }
    if (pCrs) pc->setCourse(pCrs);
  }
}

// ── Import: ClassList ─────────────────────────────────────────────────────────

void IofXmlInterface::readClassList(const xmlobject& xo, int& read, int& failed) {
  read = 0; failed = 0;
  if (!xo) return;

  xmlList xClasses;
  xo.getObjects("Class", xClasses);

  for (const auto& xc : xClasses) {
    wstring name;
    xc.getObjectString("Name", name);
    if (name.empty()) { failed++; continue; }

    int id = xc.getObjectInt("Id");

    // Look for existing class
    vector<pClass> existing;
    oe.getClasses(existing, false);
    pClass pc = nullptr;
    for (pClass c : existing) {
      if (id > 0 && c->getId() == id) { pc = c; break; }
      if (c->getName() == name) { pc = c; break; }
    }

    if (!pc) {
      pc = oe.addClass(name, 0, id > 0 ? id : 0);
    } else {
      pc->setName(name, false);
    }

    if (pc)
      read++;
    else
      failed++;
  }
}

// ── Import: OrganisationList ──────────────────────────────────────────────────

void IofXmlInterface::readOrganisationList(const xmlobject& xo, int& count) {
  count = 0;
  if (!xo) return;

  xmlList xOrgs;
  xo.getObjects("Organisation", xOrgs);

  for (const auto& xorg : xOrgs) {
    wstring name;
    xorg.getObjectString("Name", name);
    if (name.empty()) continue;

    int id = xorg.getObjectInt("Id");

    // Find existing or create
    vector<pClub> existing;
    oe.getClubs(existing, false);
    pClub pc = nullptr;
    for (pClub c : existing) {
      if (id > 0 && c->getId() == id) { pc = c; break; }
      if (c->getName() == name) { pc = c; break; }
    }
    if (!pc) {
      pc = oe.addClub(name, id > 0 ? id : 0);
    } else {
      pc->setName(name);
    }
    if (pc) count++;
  }
}

// ── Import: EntryList ─────────────────────────────────────────────────────────

void IofXmlInterface::readEntryList(const xmlobject& xo, int& read, int& failed) {
  read = 0; failed = 0;
  if (!xo) return;

  xmlList xEntries;
  xo.getObjects("PersonEntry", xEntries);

  for (const auto& xe : xEntries) {
    // Parse name
    xmlobject xPerson = xe.getObject("Person");
    if (!xPerson) { failed++; continue; }

    xmlobject xName = xPerson.getObject("Name");
    wstring family, given;
    if (xName) {
      xName.getObjectString("Family", family);
      xName.getObjectString("Given", given);
    }
    if (family.empty() && given.empty()) { failed++; continue; }

    wstring fullName = given.empty() ? family
                     : family.empty() ? given
                     : given + L" " + family;

    // Parse club
    int clubId = 0;
    xmlobject xOrg = xe.getObject("Organisation");
    if (xOrg) clubId = xOrg.getObjectInt("Id");

    // Parse class
    int classId = 0;
    wstring className;
    xmlobject xClass = xe.getObject("Class");
    if (xClass) {
      classId = xClass.getObjectInt("Id");
      xClass.getObjectString("Name", className);
    }

    // Resolve class — create if missing
    pClass pCls = nullptr;
    if (classId > 0) pCls = oe.getClass(classId);
    if (!pCls && !className.empty()) {
      vector<pClass> classes;
      oe.getClasses(classes, false);
      for (pClass c : classes) {
        if (c->getName() == className) { pCls = c; break; }
      }
    }
    if (!pCls && !className.empty())
      pCls = oe.addClass(className, 0, classId > 0 ? classId : 0);

    // Build runner and add
    oRunner r(&oe);
    r.setName(fullName, false);
    if (clubId > 0) r.setClubId(clubId);
    if (pCls) r.setClassId(pCls->getId(), false);

    pRunner pr = oe.addRunner(r);
    if (pr)
      read++;
    else
      failed++;
  }
}

// ── Export helpers ────────────────────────────────────────────────────────────

static void writePersonName(xmlparser& xml, const wstring& fullName) {
  // Split "Given Family" — last token is family name
  size_t sp = fullName.rfind(L' ');
  wstring given  = (sp != wstring::npos) ? fullName.substr(0, sp)  : L"";
  wstring family = (sp != wstring::npos) ? fullName.substr(sp + 1) : fullName;

  xml.startTag("Person");
  xml.startTag("Name");
  if (!family.empty()) xml.write("Family", family);
  if (!given.empty())  xml.write("Given",  given);
  xml.endTag(); // Name
  xml.endTag(); // Person
}

static void writeOrganisation(xmlparser& xml, int clubId, const wstring& clubName) {
  xml.startTag("Organisation");
  if (clubId > 0) xml.write("Id", clubId);
  if (!clubName.empty()) xml.write("Name", clubName);
  xml.endTag();
}

// ── Export: ResultList ────────────────────────────────────────────────────────

void IofXmlInterface::writeResultList(xmlparser& xml, const set<int>& classFilter) {
  vector<std::wstring> attrs = {L"iofVersion", L"3.0", L"status", L"Complete"};
  xml.startTag("ResultList", attrs);

  vector<pClass> classes;
  oe.getClasses(classes, false);

  for (pClass cls : classes) {
    if (!classFilter.empty() && !classFilter.count(cls->getId())) continue;

    vector<pRunner> runners;
    oe.getRunners(cls->getId(), 0, runners, true);
    if (runners.empty()) continue;

    xml.startTag("ClassResult");
    xml.startTag("Class");
    xml.write("Id", cls->getId());
    xml.write("Name", cls->getName());
    xml.endTag(); // Class

    for (pRunner r : runners) {
      if (r->isRemoved()) continue;

      xml.startTag("PersonResult");
      writePersonName(xml, r->getName());

      int cid = r->getClubId();
      wstring cname = r->getClub();
      if (cid > 0 || !cname.empty())
        writeOrganisation(xml, cid, cname);

      xml.startTag("Result");

      int st = r->getStartTime();
      if (st > 0) {
        int zero = oe.getZeroTimeNum();
        xml.write("StartTime", absTimeISO(zero + st));
      }

      int ft = r->getFinishTime();
      if (ft > 0 && r->getStartTime() > 0) {
        int zero = oe.getZeroTimeNum();
        xml.write("FinishTime", absTimeISO(zero + ft));
      }

      int rt = r->getRunningTime(false);
      if (rt > 0) xml.write("Time", rt / timeConstSecond);

      RunnerStatus status = r->getStatus();
      xml.write("Status", string(iofStatusFromRunner(status)));

      xml.endTag(); // Result
      xml.endTag(); // PersonResult
    }

    xml.endTag(); // ClassResult
  }

  xml.endTag(); // ResultList
}

// ── Export: StartList ─────────────────────────────────────────────────────────

void IofXmlInterface::writeStartList(xmlparser& xml, const set<int>& classFilter) {
  vector<std::wstring> attrs = {L"iofVersion", L"3.0"};
  xml.startTag("StartList", attrs);

  vector<pClass> classes;
  oe.getClasses(classes, false);

  for (pClass cls : classes) {
    if (!classFilter.empty() && !classFilter.count(cls->getId())) continue;

    vector<pRunner> runners;
    oe.getRunners(cls->getId(), 0, runners, true);
    if (runners.empty()) continue;

    xml.startTag("ClassStart");
    xml.startTag("Class");
    xml.write("Id", cls->getId());
    xml.write("Name", cls->getName());
    xml.endTag(); // Class

    for (pRunner r : runners) {
      if (r->isRemoved()) continue;

      xml.startTag("PersonStart");
      writePersonName(xml, r->getName());

      int cid = r->getClubId();
      wstring cname = r->getClub();
      if (cid > 0 || !cname.empty())
        writeOrganisation(xml, cid, cname);

      xml.startTag("Start");

      int st = r->getStartTime();
      if (st > 0) {
        int zero = oe.getZeroTimeNum();
        xml.write("StartTime", absTimeISO(zero + st));
      }

      wstring bib = r->getBib();
      if (!bib.empty()) xml.write("BibNumber", bib);

      xml.endTag(); // Start
      xml.endTag(); // PersonStart
    }

    xml.endTag(); // ClassStart
  }

  xml.endTag(); // StartList
}

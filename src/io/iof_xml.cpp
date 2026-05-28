// IOF 3.0 XML import/export for MeOS — implementation.
#include "iof_xml.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sstream>

#include "domain_header.h"  // timeConstSecond, timeConstMinute, timeConstHour, etc.
#include "oBase.h"
#include "oClass.h"
#include "oClub.h"
#include "oControl.h"
#include "oCourse.h"
#include "oEvent.h"
#include "oRunner.h"
#include "xmlparser.h"
#include "meos_util.h"

namespace meos::io {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

IofXml::IofXml(oEvent& oe) : oe_(oe) {}

// ---------------------------------------------------------------------------
// Utility: status mapping
// ---------------------------------------------------------------------------

RunnerStatus IofXml::iofStatusToRunner(const std::string& s) {
    if (s == "OK")                  return StatusOK;
    if (s == "DidNotStart")         return StatusDNS;
    if (s == "Cancelled")           return StatusCANCEL;
    if (s == "SportingWithdrawal")  return StatusCANCEL;
    if (s == "DidNotFinish")        return StatusDNF;
    if (s == "MissingPunch")        return StatusMP;
    if (s == "Disqualified")        return StatusDQ;
    if (s == "OverTime")            return StatusMAX;
    if (s == "NotCompeting")        return StatusOutOfCompetition;
    if (s == "Active")              return StatusUnknown;
    if (s == "Inactive")            return StatusUnknown;
    if (s == "DidNotEnter")         return StatusDNS;
    return StatusUnknown;
}

std::string IofXml::runnerStatusToIof(RunnerStatus st) {
    switch (st) {
        case StatusOK:               return "OK";
        case StatusDNS:              return "DidNotStart";
        case StatusCANCEL:           return "Cancelled";
        case StatusDNF:              return "DidNotFinish";
        case StatusMP:               return "MissingPunch";
        case StatusDQ:               return "Disqualified";
        case StatusMAX:              return "OverTime";
        case StatusOutOfCompetition: return "NotCompeting";
        case StatusNotCompeting:     return "NotCompeting";
        case StatusNoTiming:         return "OK";
        case StatusUnknown:          return "Active";
        default:                     return "Active";
    }
}

// ---------------------------------------------------------------------------
// Utility: time parsing
// ---------------------------------------------------------------------------

int IofXml::parseIofDuration(const std::string& s) {
    if (s.empty()) return 0;

    // ISO 8601 duration: PT[nH][nM][nS]
    if (s[0] == 'P' || s[0] == 'p') {
        int hours = 0, minutes = 0, seconds = 0;
        size_t i = 1;
        if (i < s.size() && (s[i] == 'T' || s[i] == 't')) ++i;
        while (i < s.size()) {
            int val = 0;
            while (i < s.size() && s[i] >= '0' && s[i] <= '9')
                val = val * 10 + (s[i++] - '0');
            if (i >= s.size()) break;
            char unit = static_cast<char>(std::toupper(static_cast<unsigned char>(s[i++])));
            if (unit == 'H') hours   = val;
            else if (unit == 'M') minutes = val;
            else if (unit == 'S') seconds = val;
        }
        return (hours * 3600 + minutes * 60 + seconds) * timeConstSecond;
    }

    // Plain integer seconds
    try {
        return std::stoi(s) * timeConstSecond;
    } catch (...) {
        return 0;
    }
}

int IofXml::parseIofTimeOfDay(const std::string& s) {
    if (s.empty()) return -1;

    // Strip date portion: "2023-08-01T10:30:00+02:00" → "10:30:00+02:00"
    std::string timeStr = s;
    auto tpos = timeStr.find('T');
    if (tpos != std::string::npos)
        timeStr = timeStr.substr(tpos + 1);

    // Strip timezone: "+02:00", "-05:30", "Z"
    for (size_t i = 0; i < timeStr.size(); ++i) {
        char c = timeStr[i];
        if (i > 0 && (c == '+' || c == '-' || c == 'Z' || c == 'z')) {
            timeStr = timeStr.substr(0, i);
            break;
        }
    }

    // Parse "HH:MM:SS" or "HH:MM"
    int h = 0, m = 0, sec = 0;
    if (sscanf(timeStr.c_str(), "%d:%d:%d", &h, &m, &sec) >= 2) {
        return (h * 3600 + m * 60 + sec) * timeConstSecond;
    }
    return -1;
}

std::string IofXml::formatIofDuration(int tenths) {
    if (tenths <= 0) return "0";
    int secs = tenths / timeConstSecond;
    return std::to_string(secs);
}

std::string IofXml::formatIofTimeOfDay(int tenths) {
    if (tenths < 0) return "";
    int sec  = tenths / timeConstSecond;
    int hour = sec / 3600; sec %= 3600;
    int min  = sec / 60;   sec %= 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hour, min, sec);
    return buf;
}

// ---------------------------------------------------------------------------
// Import helpers
// ---------------------------------------------------------------------------

namespace {

// Get a wstring from an xmlobject child element
wstring getWStr(const xmlobject& xo, const char* tag) {
    xmlobject child = xo.getObject(tag);
    if (!child) return L"";
    return child.getWStr();
}

// Get an int from an xmlobject child element
int getChildInt(const xmlobject& xo, const char* tag) {
    xmlobject child = xo.getObject(tag);
    if (!child) return 0;
    return child.getInt();
}

// Get a narrow string from an xmlobject child element
std::string getStr(const xmlobject& xo, const char* tag) {
    xmlobject child = xo.getObject(tag);
    if (!child) return "";
    return child.getStr();
}

// Extract person name as "Given Family"
wstring getPersonName(const xmlobject& xPerson) {
    xmlobject xName = xPerson.getObject("Name");
    if (!xName) return L"";
    wstring given  = getWStr(xName, "Given");
    wstring family = getWStr(xName, "Family");
    if (given.empty())  return family;
    if (family.empty()) return given;
    return given + L" " + family;
}

} // anon namespace

// ---------------------------------------------------------------------------
// Import: ClassList
// ---------------------------------------------------------------------------

int IofXml::readClasses(const std::string& xmlData) {
    xmlparser p;
    p.readMemory(xmlData, 0);
    xmlobject root = p.getObject("ClassList");
    if (!root) return 0;

    xmlList classes;
    root.getObjects("Class", classes);
    int count = 0;
    for (auto& xc : classes) {
        int iofId = getChildInt(xc, "Id");
        wstring name = getWStr(xc, "Name");
        if (name.empty()) continue;

        // Look up existing class by ID or create a new one
        oClass* cls = iofId > 0 ? oe_.getClass(iofId) : nullptr;
        if (!cls) {
            cls = oe_.addClass(iofId > 0 ? iofId : 0);
        }
        cls->setName(name, false);
        ++count;
    }
    return count;
}

// ---------------------------------------------------------------------------
// Import: OrganisationList (clubs)
// ---------------------------------------------------------------------------

int IofXml::readOrganisations(const std::string& xmlData) {
    xmlparser p;
    p.readMemory(xmlData, 0);
    xmlobject root = p.getObject("OrganisationList");
    if (!root) return 0;

    xmlList orgs;
    root.getObjects("Organisation", orgs);
    int count = 0;
    for (auto& xo : orgs) {
        int iofId = getChildInt(xo, "Id");
        wstring name = getWStr(xo, "Name");
        if (name.empty()) continue;

        oClub* club = iofId > 0 ? oe_.getClub(iofId) : nullptr;
        if (!club) {
            club = oe_.addClub(iofId > 0 ? iofId : 0);
        }
        club->setName(name);
        ++count;
    }
    return count;
}

// ---------------------------------------------------------------------------
// Import: CourseData
// ---------------------------------------------------------------------------

int IofXml::readCourseData(const std::string& xmlData) {
    xmlparser p;
    p.readMemory(xmlData, 0);
    xmlobject root = p.getObject("CourseData");
    if (!root) return 0;

    xmlobject xRace = root.getObject("RaceCourseData");
    if (!xRace) xRace = root;  // some exports omit RaceCourseData wrapper

    xmlList courses;
    xRace.getObjects("Course", courses);
    int count = 0;
    for (auto& xc : courses) {
        int iofId = getChildInt(xc, "Id");
        wstring name = getWStr(xc, "Name");
        if (name.empty()) continue;

        int length = getChildInt(xc, "Length");

        oCourse* crs = iofId > 0 ? oe_.getCourse(iofId) : nullptr;
        if (!crs) {
            crs = oe_.addCourse(iofId > 0 ? iofId : 0);
        }
        crs->setName(name);
        if (length > 0) crs->setLength(length);

        // Parse course controls in sequence order
        xmlList ctrls;
        xc.getObjects("CourseControl", ctrls);

        // Build list of regular (non-start/finish) control IDs
        std::string ctrlStr;
        for (auto& xcc : ctrls) {
            // type attribute: "Start", "Finish", or absent/empty = regular
            xmlattrib typeAttr = xcc.getAttrib("type");
            std::string type = typeAttr ? typeAttr.getStr() : "";
            if (type == "Start" || type == "Finish") continue;

            xmlobject xCtrl = xcc.getObject("Control");
            if (!xCtrl) continue;
            int code = getChildInt(xCtrl, "Id");
            if (code <= 0) {
                // Try as raw text of <Id>
                xmlobject xId = xCtrl.getObject("Id");
                if (xId) code = xId.getInt();
            }
            if (code <= 0) continue;

            // Ensure the control exists in oEvent
            oControl* ctrl = oe_.getControl(code, true);
            (void)ctrl;

            if (!ctrlStr.empty()) ctrlStr += ";";
            ctrlStr += std::to_string(code);
        }
        if (!ctrlStr.empty()) {
            crs->importControls(ctrlStr, false, false);
        }
        ++count;
    }
    return count;
}

// ---------------------------------------------------------------------------
// Import: EntryList / StartList / ResultList
// ---------------------------------------------------------------------------

int IofXml::readEntryList(const std::string& xmlData) {
    xmlparser p;
    p.readMemory(xmlData, 0);

    // Determine document type and the entry element name
    const char* rootNames[] = {
        "EntryList", "StartList", "ResultList", nullptr
    };
    xmlobject root;
    const char* rootName = nullptr;
    for (int i = 0; rootNames[i]; ++i) {
        root = p.getObject(rootNames[i]);
        if (root) { rootName = rootNames[i]; break; }
    }
    if (!root) return 0;

    bool hasStartTimes  = (strcmp(rootName, "StartList")  == 0 ||
                           strcmp(rootName, "ResultList") == 0);
    bool hasResults     = (strcmp(rootName, "ResultList") == 0);

    int count = 0;

    // EntryList: flat PersonEntry elements
    if (strcmp(rootName, "EntryList") == 0) {
        xmlList entries;
        root.getObjects("PersonEntry", entries);
        for (auto& xe : entries) {
            xmlobject xPerson = xe.getObject("Person");
            if (!xPerson) continue;
            wstring name = getPersonName(xPerson);
            if (name.empty()) continue;

            xmlobject xOrg = xe.getObject("Organisation");
            int clubId = xOrg ? getChildInt(xOrg, "Id") : 0;

            xmlobject xClass = xe.getObject("Class");
            int classId = xClass ? getChildInt(xClass, "Id") : 0;

            oRunner* r = oe_.addRunner(0);
            r->setName(name, false);
            if (clubId > 0)  r->setClubId(clubId);
            if (classId > 0) r->setClassId(classId, false);
            ++count;
        }
        return count;
    }

    // StartList: ClassStart → PersonStart
    // ResultList: ClassResult → PersonResult
    const char* classGroupTag = hasResults ? "ClassResult" : "ClassStart";
    const char* personTag     = hasResults ? "PersonResult" : "PersonStart";
    const char* timeBlockTag  = hasResults ? "Result" : "Start";

    xmlList classGroups;
    root.getObjects(classGroupTag, classGroups);
    for (auto& xGroup : classGroups) {
        xmlobject xClass = xGroup.getObject("Class");
        int groupClassId = xClass ? getChildInt(xClass, "Id") : 0;

        xmlList persons;
        xGroup.getObjects(personTag, persons);
        for (auto& xEntry : persons) {
            xmlobject xPerson = xEntry.getObject("Person");
            if (!xPerson) continue;
            wstring name = getPersonName(xPerson);
            if (name.empty()) continue;

            xmlobject xOrg = xEntry.getObject("Organisation");
            int clubId = xOrg ? getChildInt(xOrg, "Id") : 0;

            // Class ID from entry itself or the group
            xmlobject xEntryClass = xEntry.getObject("Class");
            int classId = xEntryClass ? getChildInt(xEntryClass, "Id") : groupClassId;

            oRunner* r = oe_.addRunner(0);
            r->setName(name, false);
            if (clubId > 0)  r->setClubId(clubId);
            if (classId > 0) r->setClassId(classId, false);

            if (hasStartTimes) {
                xmlobject xTime = xEntry.getObject(timeBlockTag);
                if (xTime) {
                    // Start time
                    xmlobject xStart = xTime.getObject("StartTime");
                    if (xStart) {
                        int tod = IofXml::parseIofTimeOfDay(xStart.getStr());
                        if (tod >= 0) {
                            int rel = tod - oe_.ZeroTime;
                            if (rel < 0) rel += 24 * timeConstHour;
                            r->setStartTime(rel, false, oBase::ChangeType::Quiet);
                        }
                    }

                    if (hasResults) {
                        // Finish time
                        xmlobject xFinish = xTime.getObject("FinishTime");
                        if (xFinish) {
                            int tod = IofXml::parseIofTimeOfDay(xFinish.getStr());
                            if (tod >= 0) r->setFinishTime(tod - oe_.ZeroTime);
                        }
                        // Running time (seconds)
                        xmlobject xRunTime = xTime.getObject("Time");
                        if (xRunTime) {
                            int dur = IofXml::parseIofDuration(xRunTime.getStr());
                            if (dur > 0) r->setFinishTime(r->getStartTime() + dur);
                        }
                        // Status
                        std::string status = getStr(xTime, "Status");
                        if (!status.empty()) {
                            RunnerStatus st = IofXml::iofStatusToRunner(status);
                            r->setStatus(st, false, oBase::ChangeType::Quiet);
                        }
                    }
                }
            }
            ++count;
        }
    }
    return count;
}

// ---------------------------------------------------------------------------
// Export helpers
// ---------------------------------------------------------------------------

namespace {

// Write IOF 3.0 namespace attributes for a root element.
// Call after startTag for the root element.
void writeIofAttribs(xmlparser& xml, const char* rootTag,
                     const std::vector<std::pair<std::string, std::wstring>>& extra)
{
    // startTag doesn't support multiple attributes in one call in a fully
    // clean way; we use startTag with vector<wstring> propvalue.
    // But we already opened the root tag above — we can't re-open it.
    // The approach: don't call startTag before this; pass all attrs here.
    (void)xml; (void)rootTag; (void)extra;
}

} // anon namespace

// Write a <Name><Given>…</Given><Family>…</Family></Name> block
void IofXml::writePersonName(xmlparser& xml, const std::wstring& fullName) {
    // Split "Given Family" on last space
    wstring given, family;
    auto pos = fullName.rfind(L' ');
    if (pos == wstring::npos) {
        family = fullName;
    } else {
        given  = fullName.substr(0, pos);
        family = fullName.substr(pos + 1);
    }
    xml.startTag("Name");
    if (!given.empty())  xml.write("Given",  given);
    if (!family.empty()) xml.write("Family", family);
    xml.endTag();
}

// ---------------------------------------------------------------------------
// Export: ResultList
// ---------------------------------------------------------------------------

void IofXml::writeResultList(xmlparser& xml, const std::set<int>& classIds) {
    // Root element with IOF 3.0 attributes
    std::vector<wstring> attrs = {
        L"xmlns",     L"http://www.orienteering.org/datastandard/3.0",
        L"iofVersion", L"3.0",
        L"status",    L"Complete"
    };
    xml.startTag("ResultList", attrs);

    // Event name
    xml.startTag("Event");
    xml.write("Name", oe_.Name);
    xml.endTag();

    // Group runners by class
    for (auto& cls : oe_.Classes) {
        if (!classIds.empty() && classIds.find(cls.getId()) == classIds.end())
            continue;

        // Collect runners for this class
        std::vector<oRunner*> runners;
        for (auto& r : oe_.Runners) {
            if (r.isRemoved()) continue;
            if (r.getClassId(false) != cls.getId()) continue;
            runners.push_back(const_cast<oRunner*>(&r));
        }
        if (runners.empty()) continue;

        xml.startTag("ClassResult");

        xml.startTag("Class");
        xml.write("Id", cls.getId());
        xml.write("Name", cls.getName());
        xml.endTag();

        for (oRunner* r : runners) {
            xml.startTag("PersonResult");

            xml.startTag("Person");
            writePersonName(xml, r->getName());
            xml.endTag();  // Person

            if (r->getClubRef()) {
                xml.startTag("Organisation");
                xml.write("Id",   r->getClubId());
                xml.write("Name", r->getClub());
                xml.endTag();
            }

            xml.startTag("Result");

            int st  = r->getStartTime();
            int fin = r->getFinishTime();
            int rt  = r->getRunningTime(false);

            if (st > 0) {
                int absStart = oe_.ZeroTime + st;
                xml.write("StartTime",
                           std::string(formatIofTimeOfDay(absStart)));
            }
            if (fin > 0) {
                int absFin = oe_.ZeroTime + fin;
                xml.write("FinishTime",
                           std::string(formatIofTimeOfDay(absFin)));
            }
            if (rt > 0)
                xml.write("Time", std::string(formatIofDuration(rt)));

            xml.write("Status",
                       std::string(runnerStatusToIof(r->getStatus())));

            int place = r->getPlace();
            if (place > 0) xml.write("Position", place);

            xml.endTag();  // Result
            xml.endTag();  // PersonResult
        }

        xml.endTag();  // ClassResult
    }

    xml.endTag();  // ResultList
}

// ---------------------------------------------------------------------------
// Export: StartList
// ---------------------------------------------------------------------------

void IofXml::writeStartList(xmlparser& xml, const std::set<int>& classIds) {
    std::vector<wstring> attrs = {
        L"xmlns",     L"http://www.orienteering.org/datastandard/3.0",
        L"iofVersion", L"3.0"
    };
    xml.startTag("StartList", attrs);

    xml.startTag("Event");
    xml.write("Name", oe_.Name);
    xml.endTag();

    for (auto& cls : oe_.Classes) {
        if (!classIds.empty() && classIds.find(cls.getId()) == classIds.end())
            continue;

        std::vector<oRunner*> runners;
        for (auto& r : oe_.Runners) {
            if (r.isRemoved()) continue;
            if (r.getClassId(false) != cls.getId()) continue;
            runners.push_back(const_cast<oRunner*>(&r));
        }
        if (runners.empty()) continue;

        xml.startTag("ClassStart");

        xml.startTag("Class");
        xml.write("Id", cls.getId());
        xml.write("Name", cls.getName());
        xml.endTag();

        for (oRunner* r : runners) {
            xml.startTag("PersonStart");

            xml.startTag("Person");
            writePersonName(xml, r->getName());
            xml.endTag();

            if (r->getClubRef()) {
                xml.startTag("Organisation");
                xml.write("Id",   r->getClubId());
                xml.write("Name", r->getClub());
                xml.endTag();
            }

            xml.startTag("Start");
            int st = r->getStartTime();
            if (st > 0) {
                int absStart = oe_.ZeroTime + st;
                xml.write("StartTime",
                           std::string(formatIofTimeOfDay(absStart)));
            }
            xml.endTag();  // Start

            xml.endTag();  // PersonStart
        }

        xml.endTag();  // ClassStart
    }

    xml.endTag();  // StartList
}

} // namespace meos::io

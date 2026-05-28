#include "csv_export.h"

#include "oEvent.h"
#include "oRunner.h"
#include "oClub.h"
#include "oClass.h"
#include "oCourse.h"
#include "domain_header.h"
#include "meosexception.h"
#include "meos_util.h"
#include "time_util.h"

#include <vector>
#include <string>
#include <set>

using std::vector;
using std::wstring;
using std::string;

namespace {

// Convert RunnerStatus to OE numeric code (inverse of convertOEStatus in csv_import.cpp).
int statusToOE(RunnerStatus s) {
  switch (s) {
    case StatusOK:               return 0;
    case StatusDNS:
    case StatusCANCEL:           return 1;
    case StatusDNF:              return 2;
    case StatusMP:               return 3;
    case StatusDQ:               return 4;
    case StatusMAX:              return 5;
    default:                     return 0;
  }
}

// Format time (in tenths-of-a-second units) as "HH:MM:SS".
// Returns an empty wstring if t <= 0.
wstring fmtTime(int t) {
  if (t <= 0) return wstring();
  return meos::util::widen(meos::util::formatTimeHMS(t / timeConstSecond));
}

// Convert int to wstring.
wstring iw(int v) { return std::to_wstring(v); }

// Split "Surname, Firstname" into {surname, firstname}.
// If no ", " separator, the whole string is treated as surname.
std::pair<wstring, wstring> splitName(const wstring& name) {
  auto pos = name.find(L", ");
  if (pos != wstring::npos)
    return {name.substr(0, pos), name.substr(pos + 2)};
  // Fallback: single token is surname
  return {name, wstring()};
}

// Sex code for export (matches what importOE_CSV accepts).
wstring sexCode(PersonSex sex) {
  if (sex == sFemale) return L"F";
  if (sex == sMale)   return L"M";
  return wstring();
}

} // namespace

namespace meos::io {

// OE column indices (0-based), matching the import side.
enum {
  OEstno         = 0,
  OEcard         = 1,
  OEid           = 2,
  OEsurname      = 3,
  OEfirstname    = 4,
  OEbirth        = 5,
  OEsex          = 6,
  OEnc           = 8,
  OEstart        = 9,
  OEfinish       = 10,
  OEtime         = 11,
  OEstatus       = 12,
  OEclubno       = 13,
  OEclub         = 14,
  OEclubcity     = 15,
  OEnat          = 16,
  OEclassno      = 17,
  OEclassshort   = 18,
  OEclassname    = 19,
  OEcourseno     = 38,
  OEcourse       = 39,
  OElength       = 40,
  OEcoursecontrols = 42,
  OEpl           = 43,
  OEstartpunch   = 44,
  OEfinishpunch  = 45
};

int CsvExporter::exportOE_CSV(oEvent& oe,
                               const std::wstring& file,
                               const std::set<int>& classes) {
  if (!openOutput(file, /*writeUTF=*/true))
    throw meosException(L"Cannot open output file: " + file);

  // English header (all 46 fixed columns + variable split columns).
  outputRow(
    string("Stno;Chip;Database Id;Surname;First name;YB;S;Block;nc;"
           "Start;Finish;Time;Classifier;Club no.;Cl.name;City;Nat;"
           "Cl. no.;Short;Long;"
           "Num1;Num2;Num3;Text1;Text2;Text3;Adr. name;Street;Line2;"
           "Zip;City;Phone;Fax;EMail;Id/Club;Rented;Start fee;Paid;"
           "Course no.;Course;km;m;Course controls;Pl;"
           "Start punch;Finish punch;...")
  );

  vector<pRunner> runners;
  oe.getRunners(classes, runners);

  int nexport = 0;
  for (pRunner r : runners) {
    if (!r || r->isRemoved()) continue;

    vector<wstring> row(46); // resize to highest fixed column index + 1

    auto [surname, firstname] = splitName(r->getName());

    row[OEstno]      = iw(r->getId());
    row[OEcard]      = iw(r->getCardNo());
    row[OEsurname]   = surname;
    row[OEfirstname] = firstname;
    row[OEbirth]     = iw(r->getBirthYear());
    row[OEsex]       = sexCode(r->getSex());
    row[OEnc]        = L"0";

    // Times: convert from tenths-of-second to "HH:MM:SS".
    if (r->getStartTime() > 0)
      row[OEstart]  = fmtTime(r->getStartTime());
    if (r->getFinishTime() > 0)
      row[OEfinish] = fmtTime(r->getFinishTime());
    int rt = r->getRunningTime(false);
    if (rt > 0)
      row[OEtime]   = meos::util::widen(meos::util::formatTimeHMS(rt / timeConstSecond));

    row[OEstatus]   = iw(statusToOE(r->getStatus()));
    row[OEclubno]   = iw(r->getClubId());
    if (const pClub club = r->getClubRef()) {
      row[OEclub]     = club->getName(); // compact/short name
      row[OEclubcity] = club->getName(); // full city name
    }
    row[OEnat]      = r->getNationality();

    if (const pClass cls = r->getClassRef(false)) {
      row[OEclassno]   = iw(cls->getId());
      row[OEclassshort] = cls->getName();
      row[OEclassname]  = cls->getName();
    }

    if (pCourse pc = r->getCourse(false)) {
      row[OEcourseno]      = iw(pc->getId());
      row[OEcourse]        = pc->getName();
      if (pc->getLength() > 0) {
        int len = pc->getLength();
        row[OElength] = std::to_wstring(len / 1000) + L"." + std::to_wstring(len % 1000);
      }
      row[OEcoursecontrols] = iw(pc->nControls());
    }

    row[OEpl] = r->getPlaceS();

    // Copy start/finish times to punch columns for round-trip consistency.
    row[OEstartpunch]  = row[OEstart];
    row[OEfinishpunch] = row[OEfinish];

    outputRow(row);
    ++nexport;
  }

  closeOutput();
  return nexport;
}

}  // namespace meos::io

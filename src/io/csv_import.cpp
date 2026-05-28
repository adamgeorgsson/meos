#include "csv_import.h"

#include "oEvent.h"
#include "oRunner.h"
#include "oTeam.h"
#include "oClub.h"
#include "oClass.h"
#include "domain_header.h"
#include "meosexception.h"
#include "common_enums.h"

#include <list>
#include <vector>
#include <string>
#include <cwctype>

using std::list;
using std::vector;
using std::wstring;

namespace {

PersonSex interpretSex(const wstring& sex) {
  int sexC = sex.empty() ? 0 : sex[0];
  if (sexC == 'F' || sexC == 'K' || sexC == 'W' ||
      sexC == 'f' || sexC == 'k' || sexC == 'w')
    return sFemale;
  if (sexC == 'M' || sexC == 'H' || sexC == 'm' || sexC == 'h')
    return sMale;
  if (sexC == 'B' || sexC == 'b')
    return sBoth;
  return sUnknown;
}

RunnerStatus convertOEStatus(int i) {
  switch (i) {
    case 0: return StatusOK;
    case 1: return StatusDNS;
    case 2: return StatusDNF;
    case 3: return StatusMP;
    case 4: return StatusDQ;
    case 5: return StatusMAX;
    default: return StatusUnknown;
  }
}

inline int wstoi(const wstring& s) { return wtoi(s.c_str()); }

// Safe accessor: returns empty wstring for out-of-range indices.
class Row {
  const vector<wstring>& data;
  int row;
public:
  Row(int row, const vector<wstring>& data) : data(data), row(row) {}
  const wstring& operator[](size_t i) const {
    if (i >= data.size()) {
      static const wstring empty;
      return empty;
    }
    return data[i];
  }
  size_t size() const { return data.size(); }
};

} // namespace

namespace meos::io {

// OE column layout (0-based):
// 0=Stno 1=Card 2=DatabaseId 3=Surname 4=Firstname 5=YOB 6=Sex
// 9=Start 10=Finish 12=Status 13=ClubNo 14=ShortClub 15=ClubCity
// 16=Nat 17=ClassNo 18=Class 23=Bib
// 35=Rent 36=Fee 37=Paid 38=CourseNo 39=Course 40=Length
int CsvImporter::importOE_CSV(oEvent& oe, const std::wstring& file) {
  enum {
    OEstno    = 0, OEcard    = 1,  OEid      = 2,  OEsurname = 3,
    OEfirstname = 4, OEbirth = 5,  OEsex     = 6,
    OEstart   = 9, OEfinish  = 10, OEstatus  = 12,
    OEclubno  = 13, OEclub   = 14, OEclubcity = 15,
    OEnat     = 16, OEclassno = 17, OEclass   = 18,
    OEbib     = 23
  };

  list<vector<wstring>> allLines;
  parse(file, allLines);
  auto it = allLines.begin();
  if (it == allLines.end())
    throw meosException("Invalid OE CSV file: empty");

  // Skip header row
  ++it;

  nimport = 0;
  int line = 1;
  while (it != allLines.end()) {
    Row sp(++line, *it);
    ++it;

    if (sp.size() <= 20)
      continue;

    // Club
    int clubId = wstoi(sp[OEclubno]);
    wstring clubName = sp[OEclubcity];
    wstring shortClub = sp[OEclub];
    if (clubName.empty() && !shortClub.empty())
      std::swap(clubName, shortClub);
    oClub* club = oe.getClubCreate(clubId, clubName);

    // Class
    int classId = wstoi(sp[OEclassno]);
    oClass* cls = nullptr;
    if (classId > 0 || !sp[OEclass].empty())
      cls = oe.getClassCreate(classId, sp[OEclass]);

    // Runner
    oRunner* pr = oe.addRunner();
    nimport++;

    pr->setName(sp[OEsurname] + L", " + sp[OEfirstname], false);
    pr->setCardNo(wstoi(sp[OEcard]), false);
    if (club) pr->setClubId(club->getId());
    if (cls)  pr->setClassId(cls->getId(), false);

    pr->setSex(interpretSex(sp[OEsex]));
    pr->setBirthDate(sp[OEbirth]);

    int startNo = wstoi(sp[OEstno]);
    pr->setStartNo(startNo > 0 ? startNo : nimport, oBase::ChangeType::Update);

    if (!sp[OEbib].empty())
      pr->setBib(sp[OEbib], 0, false);

    int startTime = oEvent::convertAbsoluteTime(sp[OEstart]);
    if (startTime > 0)
      pr->setStartTime(startTime, true, oBase::ChangeType::Update);

    int finishTime = oEvent::convertAbsoluteTime(sp[OEfinish]);
    if (finishTime > 0)
      pr->setFinishTime(finishTime);

    if (!sp[OEstatus].empty())
      pr->setStatus(convertOEStatus(wstoi(sp[OEstatus])), true, oBase::ChangeType::Update);

    if (pr->getStatus() == StatusOK && pr->getRunningTime(false) == 0)
      pr->setStatus(StatusUnknown, true, oBase::ChangeType::Update);

    pr->setNationality(sp[OEnat]);
  }
  return nimport;
}

// OS column layout:
// 0=Stno 1=Descr(class) 4=Start 5=Time 6=Status 7=ClubNo 9=Club 10=Nat
// 11=ClassNo 12=Class 14=Legs 21=Fee 22=Paid
// Offset=23, PostSize=11 per runner leg:
//   +0=Surname +1=Firstname +2=YOB +3=Sex +4=Start +5=Finish
//   +7=Status  +8=Card      +9=RentCard
int CsvImporter::importOS_CSV(oEvent& oe, const std::wstring& file) {
  enum {
    OSstno   = 0, OSdesc = 1, OSstart = 4,  OStime = 5, OSstatus = 6,
    OSclubno = 7, OSclub = 9, OSnat   = 10,
    OSclassno = 11, OSclass = 12, OSlegs = 14, OSfee = 21, OSpaid = 22
  };
  const int Offset   = 23;
  const int PostSize = 11;
  enum {
    OSRsname = 0, OSRfname = 1, OSRyb = 2, OSRsex = 3,
    OSRstart = 4, OSRfinish = 5, OSRstatus = 7,
    OSRcard  = 8, OSRrentcard = 9
  };

  list<vector<wstring>> allLines;
  parse(file, allLines);
  auto it = allLines.begin();
  if (it == allLines.end())
    throw meosException("Invalid OS CSV file: empty");

  // Skip header row
  ++it;

  nimport = 0;
  int line = 1;
  while (it != allLines.end()) {
    Row sp(++line, *it);
    ++it;

    if (sp.size() <= 20 || sp[OSclub].empty())
      continue;

    int clubId = wstoi(sp[OSclubno]);
    oClub* club = oe.getClubCreate(clubId, sp[OSclub]);

    int classId = wstoi(sp[OSclassno]);
    oClass* cls = oe.getClassCreate(classId, sp[OSclass]);

    oTeam* team = oe.addTeam();
    nimport++;

    wstring teamName = sp[OSclub] + L" " + sp[OSdesc];
    team->setName(teamName, false);
    if (club) team->setClubId(club->getId());
    if (cls)  team->setClassId(cls->getId(), false);
    team->setStartNo(wstoi(sp[OSstno]), oBase::ChangeType::Update);

    if (!sp[OSstatus].empty())
      team->setStatus(convertOEStatus(wstoi(sp[OSstatus])), true, oBase::ChangeType::Update);

    int teamStart = oEvent::convertAbsoluteTime(sp[OSstart]);
    if (teamStart > 0)
      team->setStartTime(teamStart, true, oBase::ChangeType::Update);

    // Import runner legs
    size_t rindex = Offset;
    int leg = 0;
    while (rindex + OSRrentcard < sp.size() && !sp[rindex + OSRfname].empty()) {
      oRunner* r = oe.addRunner();
      r->setName(sp[rindex + OSRsname] + L", " + sp[rindex + OSRfname], false);
      if (club) r->setClubId(club->getId());
      if (cls)  r->setClassId(cls->getId(), false);
      r->setCardNo(wstoi(sp[rindex + OSRcard]), false);
      r->setSex(interpretSex(sp[rindex + OSRsex]));
      r->setBirthDate(sp[rindex + OSRyb]);

      int rStart = oEvent::convertAbsoluteTime(sp[rindex + OSRstart]);
      if (rStart > 0)
        r->setStartTime(rStart, true, oBase::ChangeType::Update);

      int rFinish = oEvent::convertAbsoluteTime(sp[rindex + OSRfinish]);
      if (rFinish > 0)
        r->setFinishTime(rFinish);

      if (!sp[rindex + OSRstatus].empty())
        r->setStatus(convertOEStatus(wstoi(sp[rindex + OSRstatus])), true, oBase::ChangeType::Update);

      r->setNationality(sp[OSnat]);

      team->setRunner(leg++, r, true);

      rindex += PostSize;
    }
  }
  return nimport;
}

}  // namespace meos::io

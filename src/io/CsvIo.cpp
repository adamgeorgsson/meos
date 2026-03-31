// CsvIo.cpp — CSV import/export implementation (US-014b).
#include "CsvIo.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "oEvent.h"
#include "oRunner.h"
#include "oClub.h"
#include "oClass.h"
#include "oAbstractRunner.h"
#include "meos_util.h"

using std::wstring;
using std::string;
using std::vector;
using std::list;

// ── OE column indices ─────────────────────────────────────────────────────────

enum {
  OEstno          = 0,  OEcard      = 1,  OEid        = 2,
  OEsurname       = 3,  OEfirstname = 4,  OEbirth     = 5,
  OEsex           = 6,  OEstart     = 9,  OEfinish    = 10,
  OEtime          = 11, OEstatus    = 12,
  OEclubno        = 13, OEclub      = 14, OEclubcity  = 15,
  OEnat           = 16, OEclassno   = 17, OEclassshortname = 18,
  OEclassname     = 19, OEbib       = 23,
  OEtextB         = 24, OEtextC     = 25,
  OErent          = 35, OEfee       = 36, OEpaid      = 37,
  OEcourseno      = 38, OEcourse    = 39, OElength    = 40,
  OEclimb         = 41, OEcoursecontrols = 42, OEpl   = 43,
  OEstartpunch    = 44, OEfinishpunch    = 45,
};

// ── OE status conversion ──────────────────────────────────────────────────────

int CsvIo::runnerToOEStatus(int status) {
  switch (status) {
    case StatusOK:
    case StatusNoTiming:
    case StatusOutOfCompetition:
      return 0;
    case StatusDNS:
    case StatusCANCEL:
    case StatusNotCompeting:
      return 1;
    case StatusDNF:
      return 2;
    case StatusMP:
      return 3;
    case StatusDQ:
      return 4;
    case StatusMAX:
      return 5;
    default:
      return 0;
  }
}

int CsvIo::oeStatusToRunner(int oeStatus) {
  switch (oeStatus) {
    case 0: return StatusOK;
    case 1: return StatusDNS;
    case 2: return StatusDNF;
    case 3: return StatusMP;
    case 4: return StatusDQ;
    case 5: return StatusMAX;
    default: return StatusUnknown;
  }
}

// ── splitLine ─────────────────────────────────────────────────────────────────

vector<wstring> CsvIo::splitLine(const wstring& line) {
  vector<wstring> result;
  wstring field;
  for (wchar_t c : line) {
    if (c == L';') {
      result.push_back(field);
      field.clear();
    } else {
      field += c;
    }
  }
  result.push_back(field);
  return result;
}

// ── parse ─────────────────────────────────────────────────────────────────────

void CsvIo::parse(const std::wstring& file,
                  std::list<std::vector<std::wstring>>& data) {
  // Open as binary so we can strip UTF-8 BOM manually
  std::ifstream fin(string(file.begin(), file.end()), std::ios::binary);
  if (!fin.good())
    return;

  // Read all bytes
  std::string raw((std::istreambuf_iterator<char>(fin)),
                   std::istreambuf_iterator<char>());
  fin.close();

  // Strip UTF-8 BOM (EF BB BF)
  size_t start = 0;
  if (raw.size() >= 3 &&
      (unsigned char)raw[0] == 0xEF &&
      (unsigned char)raw[1] == 0xBB &&
      (unsigned char)raw[2] == 0xBF) {
    start = 3;
  }

  // Convert UTF-8 to wstring and split into lines
  string content = raw.substr(start);
  // Convert to wstring via fromUTF8
  wstring wcontent = fromUTF8(content);

  // Split into lines (\r\n or \n)
  wstring line;
  for (wchar_t c : wcontent) {
    if (c == L'\r') continue;
    if (c == L'\n') {
      if (!line.empty())
        data.push_back(splitLine(line));
      line.clear();
    } else {
      line += c;
    }
  }
  if (!line.empty())
    data.push_back(splitLine(line));
}

// ── detectFormat ─────────────────────────────────────────────────────────────

CsvFormat CsvIo::detectFormat(const std::wstring& file) {
  list<vector<wstring>> data;
  parse(file, data);

  if (data.empty())
    return CsvFormat::NoCSV;

  auto it = data.begin();
  const auto& first = *it;

  // Check for XML
  if (!first.empty() && first[0].find(L"<?xml") != wstring::npos)
    return CsvFormat::NoCSV;

  // RAID format
  if (first.size() == 1 && first[0] == L"RAIDDATA")
    return CsvFormat::RAID;

  // Need at least 2 fields to distinguish OS vs OE
  if (first.size() < 2)
    return CsvFormat::NoCSV;

  // OS format: second column is "Descr" / "Namn" / "Descr." / "Navn"
  const wstring& col1 = first[1];
  if (col1 == L"Descr" || col1 == L"Namn" ||
      col1 == L"Descr." || col1 == L"Navn") {
    return CsvFormat::OS;
  }

  return CsvFormat::OE;
}

// ── importOE ─────────────────────────────────────────────────────────────────

int CsvIo::importOE(oEvent& oe, const std::wstring& file) {
  list<vector<wstring>> allLines;
  parse(file, allLines);

  if (allLines.empty())
    return -1;

  int nimport = 0;
  bool firstLine = true;

  for (const auto& fields : allLines) {
    // Always skip the first line — it's the header in OE format
    if (firstLine) {
      firstLine = false;
      continue;
    }

    // Column-count heuristic: only data rows (not empty/partial) have > 10 columns
    if (fields.size() <= 10)
      continue;

    const auto field = [&](int idx) -> const wstring& {
      static const wstring empty;
      if (idx < (int)fields.size()) return fields[idx];
      return empty;
    };

    // Club
    int clubId = wtoi(field(OEclubno));
    wstring clubCity = field(OEclubcity);
    wstring clubShort = field(OEclub);
    if (clubCity.empty() && !clubShort.empty())
      std::swap(clubCity, clubShort);

    pClub pclub = oe.getClubCreate(clubId, clubCity);
    if (pclub && !clubShort.empty())
      pclub->getDI().setString("ShortName", clubShort.substr(0, 8));

    // Runner creation
    oRunner r(&oe);
    pRunner pr = oe.addRunner(r);
    if (!pr)
      continue;

    // Name: "Surname, Firstname"
    wstring surname   = field(OEsurname);
    wstring firstname = field(OEfirstname);
    wstring fullName = surname.empty() && firstname.empty() ? L"Unknown"
                     : surname.empty() ? firstname
                     : firstname.empty() ? surname
                     : surname + L", " + firstname;
    pr->setName(fullName, false);

    // Card
    int cardNo = wtoi(field(OEcard));
    if (cardNo > 0)
      pr->setCardNo(cardNo, false, false);

    // Club
    if (pclub)
      pr->setClubId(pclub->getId());

    // Times
    int startTime = convertAbsoluteTimeHMS(field(OEstart), oe.getZeroTimeNum());
    if (startTime > 0)
      pr->setStartTime(startTime, true, oBase::ChangeType::Update);

    int finishTime = convertAbsoluteTimeHMS(field(OEfinish), oe.getZeroTimeNum());
    if (finishTime > 0)
      pr->setFinishTime(finishTime);

    // Status
    if (!field(OEstatus).empty()) {
      int oeStatus = wtoi(field(OEstatus));
      RunnerStatus st = static_cast<RunnerStatus>(oeStatusToRunner(oeStatus));
      pr->setStatus(st, true, oBase::ChangeType::Update);
    }

    // Start number
    int stno = wtoi(field(OEstno));
    if (stno > 0)
      pr->setStartNo(stno, oBase::ChangeType::Update);

    // Bib
    if (!field(OEbib).empty())
      pr->setBib(field(OEbib), 0, false);

    // DI fields
    oDataInterface di = pr->getDI();
    if (!field(OEsex).empty())
      di.setString("Sex", field(OEsex));
    if (!field(OEbirth).empty())
      di.setInt("BirthYear", wtoi(field(OEbirth)));
    if (!field(OEnat).empty())
      di.setString("Nationality", field(OEnat));
    if (fields.size() > (size_t)OEfee && !field(OEfee).empty())
      di.setInt("Fee", wtoi(field(OEfee)));
    if (fields.size() > (size_t)OEpaid && !field(OEpaid).empty())
      di.setInt("Paid", wtoi(field(OEpaid)));
    if (fields.size() > (size_t)OErent && !field(OErent).empty())
      di.setInt("CardFee", wtoi(field(OErent)));

    // Class (create if needed)
    int classNo = wtoi(field(OEclassno));
    const wstring& className = field(OEclassshortname);
    if (classNo > 0 || !className.empty()) {
      pClass pc = classNo > 0 ? oe.getClass(classNo) : nullptr;
      if (!pc && !className.empty())
        pc = oe.getClass(className);
      if (!pc && !className.empty())
        pc = oe.addClass(className, 0, classNo > 0 ? classNo : 0);
      if (pc)
        pr->setClassId(pc->getId(), false);
    }

    nimport++;
  }

  return nimport;
}

// ── exportOE ─────────────────────────────────────────────────────────────────

static string toNarrow(const wstring& w) {
  return toUTF8(w);
}

static string intStr(int i) {
  return std::to_string(i);
}

static void writeRow(std::ofstream& fout, const vector<string>& row) {
  for (size_t i = 0; i < row.size(); i++) {
    if (i > 0) fout << ';';
    fout << row[i];
  }
  fout << "\r\n";
}

bool CsvIo::exportOE(oEvent& oe, const std::wstring& file) {
  std::ofstream fout(string(file.begin(), file.end()));
  if (!fout.good())
    return false;

  // Write UTF-8 BOM
  fout.put('\xEF'); fout.put('\xBB'); fout.put('\xBF');

  // Write English header row
  fout << "Stno;Chip;Database Id;Surname;First name;YB;S;Block;nc;Start;Finish;Time;"
          "Classifier;Club no.;Cl.name;City;Nat;Cl. no.;Short;Long;"
          "Num1;Num2;Num3;Text1;Text2;Text3;Adr. name;Street;Line2;Zip;City;"
          "Phone;Fax;EMail;Id/Club;Rented;Start fee;Paid;Course no.;Course;"
          "km;m;Course controls;Pl;Start punch;Finish punch;"
          "Control1;Punch1;Control2;Punch2;Control3;Punch3;Control4;Punch4;"
          "Control5;Punch5;Control6;Punch6;Control7;Punch7;Control8;Punch8;"
          "Control9;Punch9;Control10;Punch10;(may be more) ...\r\n";

  oe.calculateResults(0);

  for (auto& runner : oe.Runners) {
    if (runner.isRemoved()) continue;

    vector<string> row(46);

    oDataConstInterface di = runner.getDCI();

    row[OEstno]      = intStr(runner.getId());
    row[OEcard]      = intStr(runner.getCardNo());
    row[OEid]        = toNarrow(runner.getExtIdentifierString());
    row[OEsurname]   = toNarrow(getFamilyName(runner.getName()));
    row[OEfirstname] = toNarrow(getGivenName(runner.getName()));

    int birthYear = di.getInt("BirthYear");
    if (birthYear > 0)
      row[OEbirth] = intStr(birthYear % 100);

    row[OEsex]    = toNarrow(di.getString("Sex"));

    // Times (HH:MM:SS absolute)
    int st = runner.getStartTime();
    if (st > 0)
      row[OEstart] = toNarrow(formatTimeHMS(st));

    int ft = runner.getFinishTime();
    if (ft > 0)
      row[OEfinish] = toNarrow(formatTimeHMS(ft));

    int rt = runner.getRunningTime(false);
    if (rt > 0)
      row[OEtime] = toNarrow(formatTime(rt));

    row[OEstatus] = intStr(runnerToOEStatus(runner.getStatus()));

    row[OEclubno] = intStr(runner.getClubId());
    if (auto* club = runner.getClubRef()) {
      row[OEclub]     = toNarrow(club->getName());
      row[OEclubcity] = toNarrow(club->getName());
    }

    row[OEnat] = toNarrow(di.getString("Nationality"));

    if (auto* cls = runner.getClassRef(false)) {
      row[OEclassno]        = intStr(cls->getId());
      row[OEclassshortname] = toNarrow(cls->getName());
      row[OEclassname]      = toNarrow(cls->getName());
    }

    row[OEbib]  = toNarrow(runner.getBib());
    row[OErent] = intStr(di.getInt("CardFee"));
    row[OEfee]  = intStr(di.getInt("Fee"));
    row[OEpaid] = intStr(di.getInt("Paid"));

    if (auto* course = runner.getCourse(false)) {
      row[OEcourseno] = intStr(course->getId());
      row[OEcourse]   = toNarrow(course->getName());
      if (course->getLength() > 0) {
        int len = course->getLength();
        row[OElength] = intStr(len / 1000) + "." + intStr(len % 1000);
      }
    }

    row[OEpl] = toNarrow(runner.getPlaceS());

    writeRow(fout, row);
  }

  fout.close();
  return true;
}

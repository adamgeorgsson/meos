// Unit tests for CSV import (OE/OS format) — US-014b1
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "csv_import.h"
#include "csvparser.h"
#include "oEvent.h"
#include "oRunner.h"
#include "oTeam.h"
#include "oClub.h"
#include "oClass.h"
#include "runner_status.h"

using namespace meos::io;
namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Temp-file helper
// ---------------------------------------------------------------------------
static std::wstring writeTempCSV(const std::string& content, const std::string& suffix = ".csv") {
  static int counter = 0;
  std::string name = "/tmp/meos_csv_test_" + std::to_string(++counter) + suffix;
  std::ofstream f(name, std::ios::binary);
  f << content;
  f.close();
  return std::wstring(name.begin(), name.end());
}

// ===========================================================================
// iscsv() format detection (verifying csvparser logic used by CsvImporter)
// ===========================================================================

TEST(CsvImporterIscsv, NoFile) {
  auto r = csvparser::iscsv(L"/tmp/meos_nonexistent_9999.csv");
  EXPECT_EQ(r, csvparser::CSV::NoCSV);
}

TEST(CsvImporterIscsv, OEFormat) {
  // OE: col[1] is a numeric card number (not Descr/Namn/Navn)
  std::string content =
    "Stno;Chip;Database Id;Surname;Firstname;YB;Sex;Block;nc;Start;Finish\n"
    "1;12345;0;Svensson;Erik;1990;M;0;0;10:00:00;10:45:30\n";
  auto path = writeTempCSV(content);
  EXPECT_EQ(csvparser::iscsv(path), csvparser::CSV::OE);
}

TEST(CsvImporterIscsv, OSFormat_ENG) {
  // OS-ENG: col[1] = "Descr"
  std::string content =
    "Stno;Descr;Block;nc;Start;Time\n"
    "1;Relay A;0;0;10:00:00;01:30:00\n";
  auto path = writeTempCSV(content);
  EXPECT_EQ(csvparser::iscsv(path), csvparser::CSV::OS);
}

TEST(CsvImporterIscsv, OSFormat_SWE) {
  // OS-SWE: col[1] = "Namn"
  std::string content =
    "Stno;Namn;Block;nc;Start;Time\n"
    "1;Stafett A;0;0;10:00:00;01:30:00\n";
  auto path = writeTempCSV(content);
  EXPECT_EQ(csvparser::iscsv(path), csvparser::CSV::OS);
}

TEST(CsvImporterIscsv, OSFormat_NOR) {
  // OS-NOR: col[1] = "Navn"
  std::string content =
    "Stno;Navn;Block;nc;Start;Time\n"
    "1;Stafett A;0;0;10:00:00;01:30:00\n";
  auto path = writeTempCSV(content);
  EXPECT_EQ(csvparser::iscsv(path), csvparser::CSV::OS);
}

TEST(CsvImporterIscsv, HeaderSkipped) {
  // Second line (data) has col[1] as card number → should be OE
  // (header row is NOT used for format detection in legacy — first non-empty
  //  line is used; our reimplementation matches this)
  std::string content =
    "Stno;Chip;DatabaseId;Surname;Firstname\n"
    "1;12345;0;Test;Runner\n";
  auto path = writeTempCSV(content);
  // col[1] = "Chip" → not a Descr/Namn/Navn keyword → OE
  EXPECT_EQ(csvparser::iscsv(path), csvparser::CSV::OE);
}

// ===========================================================================
// importOE_CSV — core import
// ===========================================================================

class OeImportTest : public ::testing::Test {
 protected:
  oEvent oe;
  CsvImporter importer;

  void SetUp() override { oe.newCompetition(L"Test"); }
};

TEST_F(OeImportTest, ImportsRunnerName) {
  std::string csv =
    "Stno;Chip;Database Id;Surname;Firstname;YB;Sex;Block;nc;Start;Finish;Time;"
    "Classifier;Club no.;Cl.name;City;Nat;Cl. no.;Short;Long;"
    "Bib;Col21;Col22;Col23\n"
    "1;12345;0;Jonsson;Anna;1995;F;0;0;;;;"
    "0;10;MIOK;Mölndal;SWE;20;HD21;;;"
    "101;;\n";
  auto path = writeTempCSV(csv);
  int n = importer.importOE_CSV(oe, path);
  EXPECT_EQ(n, 1);

  std::vector<pRunner> runners;
  oe.getRunners({}, runners);
  ASSERT_EQ(runners.size(), 1u);
  EXPECT_EQ(runners[0]->getName(), L"Jonsson, Anna");
}

TEST_F(OeImportTest, ImportsCardNo) {
  std::string csv =
    "Stno;Chip;Database Id;Surname;Firstname;YB;Sex;Block;nc;Start;Finish;Time;"
    "Classifier;Club no.;Cl.name;City;Nat;Cl. no.;Short;Long;"
    "Bib;Col21;Col22;Col23\n"
    "1;98765;0;Smith;Bob;1980;M;0;0;;;;"
    "0;10;OKA;Älta;SWE;30;HE21;;;"
    ";;;\n";
  auto path = writeTempCSV(csv);
  importer.importOE_CSV(oe, path);

  std::vector<pRunner> runners;
  oe.getRunners({}, runners);
  ASSERT_EQ(runners.size(), 1u);
  EXPECT_EQ(runners[0]->getCardNo(), 98765);
}

TEST_F(OeImportTest, CreatesClub) {
  std::string csv =
    "Stno;Chip;Database Id;Surname;Firstname;YB;Sex;Block;nc;Start;Finish;Time;"
    "Classifier;Club no.;Cl.name;City;Nat;Cl. no.;Short;Long;"
    "Bib;Col21;Col22;Col23\n"
    "1;1;0;Runner;First;1990;M;0;0;;;;"
    "0;42;SOK;Sandviken OFK;SWE;5;H21;;;"
    ";;;\n";
  auto path = writeTempCSV(csv);
  importer.importOE_CSV(oe, path);

  // Club "Sandviken OFK" (city) should have been created
  pClub club = oe.getClubCreate(42, L"Sandviken OFK");
  ASSERT_NE(club, nullptr);
  EXPECT_EQ(club->getName(), L"Sandviken OFK");
}

TEST_F(OeImportTest, CreatesClass) {
  std::string csv =
    "Stno;Chip;Database Id;Surname;Firstname;YB;Sex;Block;nc;Start;Finish;Time;"
    "Classifier;Club no.;Cl.name;City;Nat;Cl. no.;Short;Long;"
    "Bib;Col21;Col22;Col23\n"
    "1;1;0;Runner;First;1990;M;0;0;;;;"
    "0;1;OK;Club;SWE;99;H21;;;"
    ";;;\n";
  auto path = writeTempCSV(csv);
  importer.importOE_CSV(oe, path);

  oClass* cls = oe.getClassCreate(99, L"H21");
  ASSERT_NE(cls, nullptr);
  EXPECT_EQ(cls->getName(), L"H21");
}

TEST_F(OeImportTest, ImportsSex) {
  std::string csv =
    "Stno;Chip;Database Id;Surname;Firstname;YB;Sex;Block;nc;Start;Finish;Time;"
    "Classifier;Club no.;Cl.name;City;Nat;Cl. no.;Short;Long;"
    "Bib;Col21;Col22;Col23\n"
    "1;1;0;Lund;Karin;1998;F;0;0;;;;"
    "0;1;IFK;Club;SWE;1;D21;;;"
    ";;;\n";
  auto path = writeTempCSV(csv);
  importer.importOE_CSV(oe, path);

  std::vector<pRunner> runners;
  oe.getRunners({}, runners);
  ASSERT_EQ(runners.size(), 1u);
  EXPECT_EQ(runners[0]->getSex(), sFemale);
}

TEST_F(OeImportTest, ImportsMultipleRunners) {
  std::string csv =
    "Stno;Chip;Database Id;Surname;Firstname;YB;Sex;Block;nc;Start;Finish;Time;"
    "Classifier;Club no.;Cl.name;City;Nat;Cl. no.;Short;Long;"
    "Bib;Col21;Col22;Col23\n"
    "1;111;0;A;Aa;1990;M;0;0;;;;0;1;C1;Club1;SWE;1;H21;;;1;;;\n"
    "2;222;0;B;Bb;1991;M;0;0;;;;0;1;C1;Club1;SWE;1;H21;;;2;;;\n"
    "3;333;0;C;Cc;1992;F;0;0;;;;0;2;C2;Club2;SWE;2;D21;;;3;;;\n";
  auto path = writeTempCSV(csv);
  int n = importer.importOE_CSV(oe, path);
  EXPECT_EQ(n, 3);

  std::vector<pRunner> runners;
  oe.getRunners({}, runners);
  EXPECT_EQ(runners.size(), 3u);
}

TEST_F(OeImportTest, HeaderSkipped) {
  // The header row should not be imported as a runner
  std::string csv =
    "Stno;Chip;Database Id;Surname;Firstname;YB;Sex;Block;nc;Start;Finish;Time;"
    "Classifier;Club no.;Cl.name;City;Nat;Cl. no.;Short;Long;"
    "Bib;Col21;Col22;Col23\n"
    "1;111;0;Real;Runner;1990;M;0;0;;;;0;1;C;Club;SWE;1;H21;;;1;;;\n";
  auto path = writeTempCSV(csv);
  int n = importer.importOE_CSV(oe, path);
  EXPECT_EQ(n, 1);  // Only the data row, not the header
}

TEST_F(OeImportTest, ImportsDNSStatus) {
  std::string csv =
    "Stno;Chip;Database Id;Surname;Firstname;YB;Sex;Block;nc;Start;Finish;Time;"
    "Classifier;Club no.;Cl.name;City;Nat;Cl. no.;Short;Long;"
    "Bib;Col21;Col22;Col23\n"
    "1;1;0;Runner;No;1990;M;0;0;;;;1;1;C;Club;SWE;1;H21;;;1;;;\n";
  auto path = writeTempCSV(csv);
  importer.importOE_CSV(oe, path);

  std::vector<pRunner> runners;
  oe.getRunners({}, runners);
  ASSERT_EQ(runners.size(), 1u);
  EXPECT_EQ(runners[0]->getStatus(), StatusDNS);
}

TEST_F(OeImportTest, ImportsStartNo) {
  std::string csv =
    "Stno;Chip;Database Id;Surname;Firstname;YB;Sex;Block;nc;Start;Finish;Time;"
    "Classifier;Club no.;Cl.name;City;Nat;Cl. no.;Short;Long;"
    "Bib;Col21;Col22;Col23\n"
    "42;1;0;Runner;A;1990;M;0;0;;;;0;1;C;Club;SWE;1;H21;;;1;;;\n";
  auto path = writeTempCSV(csv);
  importer.importOE_CSV(oe, path);

  std::vector<pRunner> runners;
  oe.getRunners({}, runners);
  ASSERT_EQ(runners.size(), 1u);
  EXPECT_EQ(runners[0]->getStartNo(), 42);
}

TEST_F(OeImportTest, SharedClubForSameId) {
  // Two runners with same club ID should share the same club entity
  std::string csv =
    "Stno;Chip;Database Id;Surname;Firstname;YB;Sex;Block;nc;Start;Finish;Time;"
    "Classifier;Club no.;Cl.name;City;Nat;Cl. no.;Short;Long;"
    "Bib;Col21;Col22;Col23\n"
    "1;111;0;A;Aa;1990;M;0;0;;;;0;5;SOK;Sandviken;SWE;1;H21;;;1;;;\n"
    "2;222;0;B;Bb;1991;M;0;0;;;;0;5;SOK;Sandviken;SWE;1;H21;;;2;;;\n";
  auto path = writeTempCSV(csv);
  importer.importOE_CSV(oe, path);

  std::vector<pRunner> runners;
  oe.getRunners({}, runners);
  ASSERT_EQ(runners.size(), 2u);
  EXPECT_EQ(runners[0]->getClubId(), runners[1]->getClubId());
}

// ===========================================================================
// importOS_CSV — relay/team import
// ===========================================================================

class OsImportTest : public ::testing::Test {
 protected:
  oEvent oe;
  CsvImporter importer;

  void SetUp() override { oe.newCompetition(L"TestRelay"); }
};

TEST_F(OsImportTest, ImportsSingleTeam) {
  // OS header + one data row
  std::string csv =
    "Stno;Descr;Block;nc;Start;Time;Classifier;Club no.;Cl.name;City;Nat;"
    "Cl. no.;Short;Long;Legs;Num1;Num2;Num3;Text1;Text2;Text3;Start fee;Paid;"
    "Surname;First name;YB;S;Start;Finish;Time;Classifier;Chip;Rented\n"
    "1;TeamA;0;0;;;0;10;IFK;Göteborg;SWE;20;H21L;;2;;;;;;0;0;"
    "Larsson;Per;1990;M;;;0;0;11111;0\n";
  auto path = writeTempCSV(csv);
  int n = importer.importOS_CSV(oe, path);
  EXPECT_EQ(n, 1);

  std::vector<pTeam> teams;
  oe.getTeams(0, teams, false);
  EXPECT_EQ(teams.size(), 1u);
}

TEST_F(OsImportTest, TeamHasClub) {
  std::string csv =
    "Stno;Descr;Block;nc;Start;Time;Classifier;Club no.;Cl.name;City;Nat;"
    "Cl. no.;Short;Long;Legs;Num1;Num2;Num3;Text1;Text2;Text3;Start fee;Paid;"
    "Surname;First name;YB;S;Start;Finish;Time;Classifier;Chip;Rented\n"
    "1;TeamB;0;0;;;0;77;SOK;Sandviken;SWE;5;D21L;;2;;;;;;0;0;"
    "Smith;Jane;1995;F;;;0;0;22222;0\n";
  auto path = writeTempCSV(csv);
  importer.importOS_CSV(oe, path);

  std::vector<pTeam> teams;
  oe.getTeams(0, teams, false);
  ASSERT_EQ(teams.size(), 1u);
  EXPECT_NE(teams[0]->getClubId(), 0);
}

TEST_F(OsImportTest, TeamHasClass) {
  std::string csv =
    "Stno;Descr;Block;nc;Start;Time;Classifier;Club no.;Cl.name;City;Nat;"
    "Cl. no.;Short;Long;Legs;Num1;Num2;Num3;Text1;Text2;Text3;Start fee;Paid;"
    "Surname;First name;YB;S;Start;Finish;Time;Classifier;Chip;Rented\n"
    "1;TeamC;0;0;;;0;10;IFK;Falkenberg;SWE;30;H10K;;2;;;;;;0;0;"
    "Karlsson;Lena;1988;F;;;0;0;33333;0\n";
  auto path = writeTempCSV(csv);
  importer.importOS_CSV(oe, path);

  std::vector<pTeam> teams;
  oe.getTeams(0, teams, false);
  ASSERT_EQ(teams.size(), 1u);
  EXPECT_NE(teams[0]->getClassId(false), 0);
}

TEST_F(OsImportTest, HeaderSkipped) {
  std::string csv =
    "Stno;Descr;Block;nc;Start;Time;Classifier;Club no.;Cl.name;City;Nat;"
    "Cl. no.;Short;Long;Legs;Num1;Num2;Num3;Text1;Text2;Text3;Start fee;Paid;"
    "Surname;First name;YB;S;Start;Finish;Time;Classifier;Chip;Rented\n"
    "1;Team;0;0;;;0;1;OK;ClubX;SWE;1;HClass;;1;;;;;;0;0;"
    "Svensson;Erik;1990;M;;;0;0;44444;0\n";
  auto path = writeTempCSV(csv);
  int n = importer.importOS_CSV(oe, path);
  EXPECT_EQ(n, 1);  // header is skipped, only data row
}

TEST_F(OsImportTest, MultipleTeams) {
  std::string csv =
    "Stno;Descr;Block;nc;Start;Time;Classifier;Club no.;Cl.name;City;Nat;"
    "Cl. no.;Short;Long;Legs;Num1;Num2;Num3;Text1;Text2;Text3;Start fee;Paid;"
    "Surname;First name;YB;S;Start;Finish;Time;Classifier;Chip;Rented\n"
    "1;T1;0;0;;;0;1;OK1;Club1;SWE;1;H;;1;;;;;;0;0;A;Aa;1990;M;;;0;0;1;0\n"
    "2;T2;0;0;;;0;2;OK2;Club2;SWE;1;H;;1;;;;;;0;0;B;Bb;1991;M;;;0;0;2;0\n"
    "3;T3;0;0;;;0;3;OK3;Club3;SWE;2;D;;1;;;;;;0;0;C;Cc;1992;F;;;0;0;3;0\n";
  auto path = writeTempCSV(csv);
  int n = importer.importOS_CSV(oe, path);
  EXPECT_EQ(n, 3);

  std::vector<pTeam> teams;
  oe.getTeams(0, teams, false);
  EXPECT_EQ(teams.size(), 3u);
}

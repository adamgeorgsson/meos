// Unit tests for CSV export (OE format) and round-trip — US-014b2
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "csv_export.h"
#include "csv_import.h"
#include "csvparser.h"
#include "oEvent.h"
#include "oRunner.h"
#include "oClub.h"
#include "oClass.h"
#include "runner_status.h"
#include "domain_header.h"
#include "time_util.h"

using namespace meos::io;
namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::wstring tempPath(const std::string& suffix = ".csv") {
  static int counter = 0;
  std::string name = "/tmp/meos_csv_export_" + std::to_string(++counter) + suffix;
  return std::wstring(name.begin(), name.end());
}

// Convert "HH:MM:SS" to tenths-of-second (same as convertAbsoluteTimeHMS(str, 0)).
static int parseHMS(const std::string& hms) {
  int s = meos::util::parseTimeHMS(hms);
  return s * timeConstSecond; // tenths-of-second
}

// ===========================================================================
// Single-runner export
// ===========================================================================

class CsvExportTest : public ::testing::Test {
 protected:
  oEvent oe;
  CsvExporter exporter;
  CsvImporter importer;

  void SetUp() override { oe.newCompetition(L"RoundTripTest"); }
};

TEST_F(CsvExportTest, ExportsRunnerName) {
  auto* r = oe.addRunner();
  r->setName(L"Svensson, Erik", false);
  r->setCardNo(12345, false);

  auto path = tempPath();
  int n = exporter.exportOE_CSV(oe, path);
  EXPECT_EQ(n, 1);

  // Read exported file and verify surname/firstname columns
  std::ifstream f(std::string(path.begin(), path.end()));
  ASSERT_TRUE(f.good());
  std::string header, row;
  std::getline(f, header);
  std::getline(f, row);

  // Parse the row to check surname (col 3) and firstname (col 4)
  std::vector<std::string> cols;
  std::stringstream ss(row);
  std::string tok;
  while (std::getline(ss, tok, ';')) cols.push_back(tok);

  ASSERT_GE(cols.size(), 5u);
  EXPECT_EQ(cols[3], "Svensson");
  EXPECT_EQ(cols[4], "Erik");
}

TEST_F(CsvExportTest, ExportsCardNo) {
  auto* r = oe.addRunner();
  r->setName(L"A, B", false);
  r->setCardNo(99001, false);

  auto path = tempPath();
  exporter.exportOE_CSV(oe, path);

  std::ifstream f(std::string(path.begin(), path.end()), std::ios::binary);
  // Skip UTF-8 BOM if present
  char bom[3] = {0};
  f.read(bom, 3);
  if (!(static_cast<unsigned char>(bom[0]) == 0xEF &&
        static_cast<unsigned char>(bom[1]) == 0xBB &&
        static_cast<unsigned char>(bom[2]) == 0xBF))
    f.seekg(0);

  std::string header, row;
  std::getline(f, header);
  std::getline(f, row);

  std::vector<std::string> cols;
  std::stringstream ss(row);
  std::string tok;
  while (std::getline(ss, tok, ';')) cols.push_back(tok);
  ASSERT_GE(cols.size(), 2u);
  EXPECT_EQ(cols[1], "99001");
}

TEST_F(CsvExportTest, ExportsStatus) {
  auto* r = oe.addRunner();
  r->setName(L"Test, Runner", false);
  r->setCardNo(1, false);
  r->setStatus(StatusDNF, true, oBase::ChangeType::Update);

  auto path = tempPath();
  exporter.exportOE_CSV(oe, path);

  std::ifstream f(std::string(path.begin(), path.end()), std::ios::binary);
  char bom[3] = {0};
  f.read(bom, 3);
  if (!(static_cast<unsigned char>(bom[0]) == 0xEF))
    f.seekg(0);

  std::string header, row;
  std::getline(f, header);
  std::getline(f, row);

  std::vector<std::string> cols;
  std::stringstream ss(row);
  std::string tok;
  while (std::getline(ss, tok, ';')) cols.push_back(tok);
  ASSERT_GE(cols.size(), 13u);
  EXPECT_EQ(cols[12], "2"); // StatusDNF → OE code 2
}

TEST_F(CsvExportTest, ExportsSex) {
  auto* r = oe.addRunner();
  r->setName(L"Hansson, Maria", false);
  r->setCardNo(2, false);
  r->setSex(sFemale);

  auto path = tempPath();
  exporter.exportOE_CSV(oe, path);

  std::ifstream f(std::string(path.begin(), path.end()), std::ios::binary);
  char bom[3] = {0};
  f.read(bom, 3);
  if (!(static_cast<unsigned char>(bom[0]) == 0xEF))
    f.seekg(0);

  std::string header, row;
  std::getline(f, header);
  std::getline(f, row);

  std::vector<std::string> cols;
  std::stringstream ss(row);
  std::string tok;
  while (std::getline(ss, tok, ';')) cols.push_back(tok);
  ASSERT_GE(cols.size(), 7u);
  EXPECT_EQ(cols[6], "F");
}

TEST_F(CsvExportTest, ExportsStartTime) {
  auto* r = oe.addRunner();
  r->setName(L"T, R", false);
  r->setCardNo(3, false);
  // "10:00:00" = 36000 seconds = 360000 tenths
  int st = parseHMS("10:00:00");
  r->setStartTime(st, true, oBase::ChangeType::Update);

  auto path = tempPath();
  exporter.exportOE_CSV(oe, path);

  std::ifstream f(std::string(path.begin(), path.end()), std::ios::binary);
  char bom[3] = {0};
  f.read(bom, 3);
  if (!(static_cast<unsigned char>(bom[0]) == 0xEF))
    f.seekg(0);

  std::string header, row;
  std::getline(f, header);
  std::getline(f, row);

  std::vector<std::string> cols;
  std::stringstream ss(row);
  std::string tok;
  while (std::getline(ss, tok, ';')) cols.push_back(tok);
  ASSERT_GE(cols.size(), 10u);
  EXPECT_EQ(cols[9], "10:00:00");
}

TEST_F(CsvExportTest, EmptyExportReturnsZero) {
  auto path = tempPath();
  int n = exporter.exportOE_CSV(oe, path);
  EXPECT_EQ(n, 0);
  EXPECT_TRUE(fs::exists(std::string(path.begin(), path.end())));
}

TEST_F(CsvExportTest, ExportsMultipleRunners) {
  for (int i = 1; i <= 5; ++i) {
    auto* r = oe.addRunner();
    r->setName(L"Name" + std::to_wstring(i) + L", First", false);
    r->setCardNo(i * 100, false);
  }
  auto path = tempPath();
  int n = exporter.exportOE_CSV(oe, path);
  EXPECT_EQ(n, 5);
}

// ===========================================================================
// Round-trip: export then re-import and verify runner data preserved
// ===========================================================================

class CsvRoundTripTest : public ::testing::Test {
 protected:
  oEvent src, dst;
  CsvExporter exporter;
  CsvImporter importer;

  void SetUp() override {
    src.newCompetition(L"Source");
    dst.newCompetition(L"Destination");
  }
};

TEST_F(CsvRoundTripTest, NamePreserved) {
  auto* r = src.addRunner();
  r->setName(L"Lindqvist, Sanna", false);
  r->setCardNo(55001, false);

  auto path = tempPath();
  exporter.exportOE_CSV(src, path);
  importer.importOE_CSV(dst, path);

  std::vector<pRunner> runners;
  dst.getRunners({}, runners);
  ASSERT_EQ(runners.size(), 1u);
  EXPECT_EQ(runners[0]->getName(), L"Lindqvist, Sanna");
}

TEST_F(CsvRoundTripTest, CardNoPreserved) {
  auto* r = src.addRunner();
  r->setName(L"A, B", false);
  r->setCardNo(77342, false);

  auto path = tempPath();
  exporter.exportOE_CSV(src, path);
  importer.importOE_CSV(dst, path);

  std::vector<pRunner> runners;
  dst.getRunners({}, runners);
  ASSERT_EQ(runners.size(), 1u);
  EXPECT_EQ(runners[0]->getCardNo(), 77342);
}

TEST_F(CsvRoundTripTest, ClubPreserved) {
  auto* club = src.getClubCreate(10, L"Orientering IF");
  auto* r = src.addRunner();
  r->setName(L"X, Y", false);
  r->setCardNo(1, false);
  r->setClubId(club->getId());

  auto path = tempPath();
  exporter.exportOE_CSV(src, path);
  importer.importOE_CSV(dst, path);

  std::vector<pRunner> runners;
  dst.getRunners({}, runners);
  ASSERT_EQ(runners.size(), 1u);
  EXPECT_EQ(runners[0]->getClub(), L"Orientering IF");
}

TEST_F(CsvRoundTripTest, ClassPreserved) {
  auto* cls = src.getClassCreate(5, L"H21");
  auto* r = src.addRunner();
  r->setName(L"P, Q", false);
  r->setCardNo(2, false);
  r->setClassId(cls->getId(), false);

  auto path = tempPath();
  exporter.exportOE_CSV(src, path);
  importer.importOE_CSV(dst, path);

  std::vector<pRunner> runners;
  dst.getRunners({}, runners);
  ASSERT_EQ(runners.size(), 1u);
  EXPECT_EQ(runners[0]->getClass(false), L"H21");
}

TEST_F(CsvRoundTripTest, StartTimePreserved) {
  auto* r = src.addRunner();
  r->setName(L"R, S", false);
  r->setCardNo(3, false);
  int st = parseHMS("10:30:00"); // 37800 seconds = 378000 tenths
  r->setStartTime(st, true, oBase::ChangeType::Update);

  auto path = tempPath();
  exporter.exportOE_CSV(src, path);
  importer.importOE_CSV(dst, path);

  std::vector<pRunner> runners;
  dst.getRunners({}, runners);
  ASSERT_EQ(runners.size(), 1u);
  EXPECT_EQ(runners[0]->getStartTime(), st);
}

TEST_F(CsvRoundTripTest, FinishTimePreserved) {
  auto* r = src.addRunner();
  r->setName(L"U, V", false);
  r->setCardNo(4, false);
  int ft = parseHMS("11:15:45"); // finish time in tenths
  r->setFinishTime(ft);

  auto path = tempPath();
  exporter.exportOE_CSV(src, path);
  importer.importOE_CSV(dst, path);

  std::vector<pRunner> runners;
  dst.getRunners({}, runners);
  ASSERT_EQ(runners.size(), 1u);
  EXPECT_EQ(runners[0]->getFinishTime(), ft);
}

TEST_F(CsvRoundTripTest, DNSStatusPreserved) {
  auto* r = src.addRunner();
  r->setName(L"W, X", false);
  r->setCardNo(5, false);
  r->setStatus(StatusDNS, true, oBase::ChangeType::Update);

  auto path = tempPath();
  exporter.exportOE_CSV(src, path);
  importer.importOE_CSV(dst, path);

  std::vector<pRunner> runners;
  dst.getRunners({}, runners);
  ASSERT_EQ(runners.size(), 1u);
  EXPECT_EQ(runners[0]->getStatus(), StatusDNS);
}

TEST_F(CsvRoundTripTest, SexPreserved) {
  auto* r = src.addRunner();
  r->setName(L"Y, Z", false);
  r->setCardNo(6, false);
  r->setSex(sFemale);

  auto path = tempPath();
  exporter.exportOE_CSV(src, path);
  importer.importOE_CSV(dst, path);

  std::vector<pRunner> runners;
  dst.getRunners({}, runners);
  ASSERT_EQ(runners.size(), 1u);
  EXPECT_EQ(runners[0]->getSex(), sFemale);
}

TEST_F(CsvRoundTripTest, MultipleRunnersPreserved) {
  struct RunnerData { std::wstring name; int card; RunnerStatus status; };
  std::vector<RunnerData> data = {
    {L"Andersson, Lars", 1001, StatusOK},
    {L"Bergman, Lena",   1002, StatusDNF},
    {L"Carlsson, Per",   1003, StatusDNS},
  };

  for (auto& d : data) {
    auto* r = src.addRunner();
    r->setName(d.name, false);
    r->setCardNo(d.card, false);
    if (d.status != StatusUnknown)
      r->setStatus(d.status, true, oBase::ChangeType::Update);
  }

  auto path = tempPath();
  int nexported = exporter.exportOE_CSV(src, path);
  EXPECT_EQ(nexported, 3);

  int nimported = importer.importOE_CSV(dst, path);
  EXPECT_EQ(nimported, 3);

  std::vector<pRunner> runners;
  dst.getRunners({}, runners);
  ASSERT_EQ(runners.size(), 3u);
}

TEST_F(CsvRoundTripTest, RowHasAtLeast46Columns) {
  auto* r = src.addRunner();
  r->setName(L"A, B", false);
  r->setCardNo(1, false);

  auto path = tempPath();
  exporter.exportOE_CSV(src, path);

  std::ifstream f(std::string(path.begin(), path.end()), std::ios::binary);
  char bom[3] = {0};
  f.read(bom, 3);
  if (!(static_cast<unsigned char>(bom[0]) == 0xEF))
    f.seekg(0);

  std::string header, row;
  std::getline(f, header);
  std::getline(f, row);

  int cols = 1;
  for (char c : row) if (c == ';') ++cols;
  EXPECT_GE(cols, 46);
}

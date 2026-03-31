// csv_io_test.cpp — CSV import/export tests (US-014b).
#include <gtest/gtest.h>
#include <cstdio>
#include <fstream>
#include <locale>
#include <clocale>
#include <string>
#include <vector>

#include "oEvent.h"
#include "oRunner.h"
#include "oClub.h"
#include "oClass.h"
#include "oAbstractRunner.h"
#include "meos_util.h"
#include "CsvIo.h"

using std::string;
using std::wstring;
using std::vector;

// ── Helpers ───────────────────────────────────────────────────────────────────

static wstring tmpCsvPath() {
  static int counter = 0;
  return wstring(L"/tmp/meos_csv_test_") + itow(++counter) + L".csv";
}

// ── Fixtures ──────────────────────────────────────────────────────────────────

class CsvIoTest : public ::testing::Test {
protected:
  oEvent oe;

  void SetUp() override {
    std::setlocale(LC_ALL, "C.UTF-8");
    oe.newCompetition(L"TestComp");
    oe.setDate(L"2024-06-15");
    oe.setZeroTime(L"10:00:00");
  }

  void TearDown() override {}
};

// ── splitLine tests ───────────────────────────────────────────────────────────

TEST_F(CsvIoTest, SplitLine_Basic) {
  auto fields = CsvIo::splitLine(L"Alice;Smith;42;H21");
  ASSERT_EQ(fields.size(), 4u);
  EXPECT_EQ(fields[0], L"Alice");
  EXPECT_EQ(fields[1], L"Smith");
  EXPECT_EQ(fields[2], L"42");
  EXPECT_EQ(fields[3], L"H21");
}

TEST_F(CsvIoTest, SplitLine_Empty) {
  auto fields = CsvIo::splitLine(L"");
  ASSERT_EQ(fields.size(), 1u);
  EXPECT_EQ(fields[0], L"");
}

TEST_F(CsvIoTest, SplitLine_TrailingSemicolon) {
  auto fields = CsvIo::splitLine(L"a;b;");
  ASSERT_EQ(fields.size(), 3u);
  EXPECT_EQ(fields[0], L"a");
  EXPECT_EQ(fields[1], L"b");
  EXPECT_EQ(fields[2], L"");
}

// ── Column-count heuristic ────────────────────────────────────────────────────

TEST_F(CsvIoTest, ColumnHeuristic_SkipsHeaderRows) {
  // Write a CSV with a single-column header and a multi-column data row
  wstring path = tmpCsvPath();
  {
    std::ofstream f(string(path.begin(), path.end()));
    // Header: 5 columns (≤10) — should be skipped
    f << "Stno;Chip;Surname;First name;Status\r\n";
    // Data row: 25 columns (>10) — should be imported
    // OE format: col0=stno, col1=card, col2=id, col3=surname, col4=firstname,
    //            col5=YB, col6=sex, ... col17=classno, col18=classname
    f << "1;9001;;Johansson;Erik;0;;0;0;10:00:00;11:23:45;1:23:45;0;101;OK;Stockholm;SWE;1;H21;H21;0;0;0;;;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0\r\n";
  }
  int n = CsvIo::importOE(oe, path);
  EXPECT_EQ(n, 1);
  std::remove(string(path.begin(), path.end()).c_str());
}

TEST_F(CsvIoTest, ColumnHeuristic_SkipsTooFewColumns) {
  wstring path = tmpCsvPath();
  {
    std::ofstream f(string(path.begin(), path.end()));
    // Header line (always skipped as first line)
    f << "Stno;Chip;Surname;First name;Status\r\n";
    // Data row with only 5 columns — below threshold of 10, should be skipped
    f << "1;9001;Johansson;Erik;H21\r\n";
  }
  int n = CsvIo::importOE(oe, path);
  EXPECT_EQ(n, 0);
  std::remove(string(path.begin(), path.end()).c_str());
}

// ── detectFormat tests ────────────────────────────────────────────────────────

TEST_F(CsvIoTest, DetectFormat_OEFormat) {
  wstring path = tmpCsvPath();
  {
    std::ofstream f(string(path.begin(), path.end()));
    f << "Stno;Chip;Database Id;Surname;First name;YB;S;Block;nc;Start;Finish;Time;Classifier\r\n";
  }
  CsvFormat fmt = CsvIo::detectFormat(path);
  EXPECT_EQ(fmt, CsvFormat::OE);
  std::remove(string(path.begin(), path.end()).c_str());
}

TEST_F(CsvIoTest, DetectFormat_OSFormat) {
  wstring path = tmpCsvPath();
  {
    std::ofstream f(string(path.begin(), path.end()));
    f << "Stno;Descr;Surname;First name\r\n";
  }
  CsvFormat fmt = CsvIo::detectFormat(path);
  EXPECT_EQ(fmt, CsvFormat::OS);
  std::remove(string(path.begin(), path.end()).c_str());
}

TEST_F(CsvIoTest, DetectFormat_RAID) {
  wstring path = tmpCsvPath();
  {
    std::ofstream f(string(path.begin(), path.end()));
    f << "RAIDDATA\r\n";
  }
  CsvFormat fmt = CsvIo::detectFormat(path);
  EXPECT_EQ(fmt, CsvFormat::RAID);
  std::remove(string(path.begin(), path.end()).c_str());
}

TEST_F(CsvIoTest, DetectFormat_NoCSV_XML) {
  wstring path = tmpCsvPath();
  {
    std::ofstream f(string(path.begin(), path.end()));
    f << "<?xml version=\"1.0\"?><root/>\r\n";
  }
  CsvFormat fmt = CsvIo::detectFormat(path);
  EXPECT_EQ(fmt, CsvFormat::NoCSV);
  std::remove(string(path.begin(), path.end()).c_str());
}

TEST_F(CsvIoTest, DetectFormat_NoCSV_Missing) {
  CsvFormat fmt = CsvIo::detectFormat(L"/tmp/nonexistent_file_xyz_12345.csv");
  EXPECT_EQ(fmt, CsvFormat::NoCSV);
}

// ── OE status conversion ──────────────────────────────────────────────────────

TEST_F(CsvIoTest, StatusConversion_OKRoundtrip) {
  EXPECT_EQ(CsvIo::runnerToOEStatus(StatusOK), 0);
  EXPECT_EQ(CsvIo::oeStatusToRunner(0), StatusOK);
}

TEST_F(CsvIoTest, StatusConversion_DNFRoundtrip) {
  EXPECT_EQ(CsvIo::runnerToOEStatus(StatusDNF), 2);
  EXPECT_EQ(CsvIo::oeStatusToRunner(2), StatusDNF);
}

TEST_F(CsvIoTest, StatusConversion_DNSRoundtrip) {
  EXPECT_EQ(CsvIo::runnerToOEStatus(StatusDNS), 1);
  EXPECT_EQ(CsvIo::oeStatusToRunner(1), StatusDNS);
}

TEST_F(CsvIoTest, StatusConversion_MPRoundtrip) {
  EXPECT_EQ(CsvIo::runnerToOEStatus(StatusMP), 3);
  EXPECT_EQ(CsvIo::oeStatusToRunner(3), StatusMP);
}

// ── Export test ───────────────────────────────────────────────────────────────

TEST_F(CsvIoTest, Export_WritesFile) {
  // Add a runner
  oRunner r(&oe, 1);
  r.setName(L"Andersson, Anna", false);
  r.setCardNo(1001, false, false);
  oe.addRunner(r);

  wstring path = tmpCsvPath();
  bool ok = CsvIo::exportOE(oe, path);
  EXPECT_TRUE(ok);

  // File should exist and have content
  std::ifstream f(string(path.begin(), path.end()));
  EXPECT_TRUE(f.good());
  string content((std::istreambuf_iterator<char>(f)),
                  std::istreambuf_iterator<char>());
  EXPECT_FALSE(content.empty());
  // Should contain the header
  EXPECT_NE(content.find("Stno"), string::npos);
  // Should contain card no
  EXPECT_NE(content.find("1001"), string::npos);

  std::remove(string(path.begin(), path.end()).c_str());
}

// ── Round-trip test ───────────────────────────────────────────────────────────

TEST_F(CsvIoTest, RoundTrip_RunnerNameAndCard) {
  // Set up source event
  pClub club = oe.addClub(L"IFK Goteborg", 10);
  pClass cls  = oe.addClass(L"H21", 0, 5);

  oRunner r1(&oe, 1);
  r1.setName(L"Eriksson, Lars", false);
  r1.setCardNo(9901, false, false);
  pRunner pr1 = oe.addRunner(r1);
  ASSERT_NE(pr1, nullptr);
  pr1->setClubId(club->getId());
  pr1->setClassId(cls->getId(), false);
  pr1->setStatus(StatusOK, true, oBase::ChangeType::Update);

  oRunner r2(&oe, 2);
  r2.setName(L"Nilsson, Eva", false);
  r2.setCardNo(9902, false, false);
  pRunner pr2 = oe.addRunner(r2);
  ASSERT_NE(pr2, nullptr);
  pr2->setStatus(StatusDNF, true, oBase::ChangeType::Update);

  // Export
  wstring path = tmpCsvPath();
  bool ok = CsvIo::exportOE(oe, path);
  ASSERT_TRUE(ok);

  // Import into new event
  oEvent oe2;
  oe2.newCompetition(L"Import");
  oe2.setDate(L"2024-06-15");
  oe2.setZeroTime(L"10:00:00");

  int n = CsvIo::importOE(oe2, path);
  EXPECT_EQ(n, 2);

  // Find runners by name
  vector<pRunner> runners;
  oe2.getRunners(0, 0, runners, false);
  ASSERT_EQ((int)runners.size(), 2);

  // Find Lars Eriksson
  pRunner lars = nullptr;
  pRunner eva = nullptr;
  for (auto* pr : runners) {
    if (pr->getName() == L"Eriksson, Lars") lars = pr;
    if (pr->getName() == L"Nilsson, Eva") eva = pr;
  }

  ASSERT_NE(lars, nullptr) << "Lars Eriksson not found after round-trip";
  EXPECT_EQ(lars->getCardNo(), 9901);
  EXPECT_EQ(lars->getStatus(), StatusOK);

  ASSERT_NE(eva, nullptr) << "Eva Nilsson not found after round-trip";
  EXPECT_EQ(eva->getCardNo(), 9902);
  EXPECT_EQ(eva->getStatus(), StatusDNF);

  std::remove(string(path.begin(), path.end()).c_str());
}

TEST_F(CsvIoTest, RoundTrip_ClassPreserved) {
  pClass cls = oe.addClass(L"D21", 0, 3);

  oRunner r(&oe, 1);
  r.setName(L"Berg, Karin", false);
  pRunner pr = oe.addRunner(r);
  ASSERT_NE(pr, nullptr);
  pr->setClassId(cls->getId(), false);

  wstring path = tmpCsvPath();
  ASSERT_TRUE(CsvIo::exportOE(oe, path));

  oEvent oe2;
  oe2.newCompetition(L"Import2");
  oe2.setZeroTime(L"10:00:00");
  int n = CsvIo::importOE(oe2, path);
  EXPECT_EQ(n, 1);

  vector<pRunner> runners;
  oe2.getRunners(0, 0, runners, false);
  ASSERT_EQ((int)runners.size(), 1);
  EXPECT_EQ(runners[0]->getClass(false), L"D21");

  std::remove(string(path.begin(), path.end()).c_str());
}

TEST_F(CsvIoTest, RoundTrip_EmptyEvent) {
  wstring path = tmpCsvPath();
  ASSERT_TRUE(CsvIo::exportOE(oe, path));

  oEvent oe2;
  oe2.newCompetition(L"Empty");
  int n = CsvIo::importOE(oe2, path);
  // Header row has ≤10 fields when split, should give 0 data rows
  EXPECT_EQ(n, 0);

  std::remove(string(path.begin(), path.end()).c_str());
}

TEST_F(CsvIoTest, Import_NonexistentFile) {
  int n = CsvIo::importOE(oe, L"/tmp/nonexistent_file_xyz_12345.csv");
  EXPECT_EQ(n, -1);
}

TEST_F(CsvIoTest, Export_NonexistentDir) {
  bool ok = CsvIo::exportOE(oe, L"/nonexistent_dir/file.csv");
  EXPECT_FALSE(ok);
}

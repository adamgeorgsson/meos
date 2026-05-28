#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <list>
#include <string>
#include <vector>

#include "csvparser.h"

namespace fs = std::filesystem;

namespace {
fs::path testDir() {
  fs::path dir = fs::current_path() / "test_artifacts_util_csv";
  fs::create_directories(dir);
  return dir;
}

std::wstring toW(const fs::path &p) {
  return p.wstring();
}
} // namespace

TEST(CsvParserTest, IsCsvReturnsNoCsvForXmlFile) {
  fs::path file = testDir() / "not_csv.xml";
  std::ofstream(file) << "<?xml version=\"1.0\"?><root/>";
  EXPECT_EQ(csvparser::iscsv(toW(file)), csvparser::CSV::NoCSV);
}

TEST(CsvParserTest, ParseParsesSemicolonSeparatedValues) {
  fs::path file = testDir() / "simple.csv";
  std::ofstream(file) << "name;age\nAlice;42\n";

  csvparser parser;
  std::list<std::vector<std::wstring>> rows;
  parser.parse(toW(file), rows);

  ASSERT_EQ(rows.size(), 2u);
  auto it = rows.begin();
  EXPECT_EQ((*it)[0], L"name");
  EXPECT_EQ((*it)[1], L"age");
  ++it;
  EXPECT_EQ((*it)[0], L"Alice");
  EXPECT_EQ((*it)[1], L"42");
}

TEST(CsvParserTest, SplitHandlesQuotedFields) {
  char line[] = "alpha;\"beta;gamma\";delta";
  std::vector<char *> parts;
  csvparser::split(line, parts);
  ASSERT_EQ(parts.size(), 3u);
  EXPECT_STREQ(parts[0], "alpha");
  EXPECT_STREQ(parts[1], "beta;gamma");
  EXPECT_STREQ(parts[2], "delta");
}

TEST(CsvParserTest, OutputRowWritesCorrectCsv) {
  fs::path file = testDir() / "out.csv";
  csvparser parser;
  ASSERT_TRUE(parser.openOutput(toW(file), true));
  ASSERT_TRUE(parser.outputRow(std::vector<std::string>{"plain", "has space", "a;b", "x\"y"}));
  ASSERT_TRUE(parser.closeOutput());

  std::ifstream in(file, std::ios::binary);
  std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  ASSERT_GE(bytes.size(), 3u);
  EXPECT_EQ(static_cast<unsigned char>(bytes[0]), 0xEF);
  EXPECT_EQ(static_cast<unsigned char>(bytes[1]), 0xBB);
  EXPECT_EQ(static_cast<unsigned char>(bytes[2]), 0xBF);
  EXPECT_NE(bytes.find("plain;\"has space\";\"a;b\";x'y"), std::string::npos);
}

TEST(CsvParserTest, IsCsvDetectsOeVsOsFormat) {
  fs::path oe = testDir() / "oe.csv";
  fs::path os = testDir() / "os.csv";
  std::ofstream(oe) << "Stno;Runner;Other\n1;A;B\n";
  std::ofstream(os) << "Stno;Descr;Other\n1;A;B\n";

  EXPECT_EQ(csvparser::iscsv(toW(oe)), csvparser::CSV::OE);
  EXPECT_EQ(csvparser::iscsv(toW(os)), csvparser::CSV::OS);
}

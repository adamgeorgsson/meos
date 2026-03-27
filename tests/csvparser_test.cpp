#include <gtest/gtest.h>
#include "csvparser.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <list>
#include <string>
#include <vector>

using std::string;
using std::wstring;
using std::vector;
using std::list;

// ---- split (char*) -------------------------------------------------------

TEST(CSVSplit, BasicSemicolon) {
  char line[] = "a;b;c";
  vector<char *> sp;
  csvparser::split(line, sp);
  ASSERT_EQ(sp.size(), 3u);
  EXPECT_STREQ(sp[0], "a");
  EXPECT_STREQ(sp[1], "b");
  EXPECT_STREQ(sp[2], "c");
}

TEST(CSVSplit, QuotedField) {
  char line[] = "\"hello world\";next";
  vector<char *> sp;
  csvparser::split(line, sp);
  ASSERT_EQ(sp.size(), 2u);
  EXPECT_STREQ(sp[0], "hello world");
  EXPECT_STREQ(sp[1], "next");
}

TEST(CSVSplit, EmptyMiddleField) {
  // "a;;c" should produce 3 fields: "a", "", "c"
  char line[] = "a;;c";
  vector<char *> sp;
  csvparser::split(line, sp);
  ASSERT_EQ(sp.size(), 3u);
  EXPECT_STREQ(sp[0], "a");
  EXPECT_STREQ(sp[1], "");
  EXPECT_STREQ(sp[2], "c");
}

TEST(CSVSplit, SingleField) {
  char line[] = "hello";
  vector<char *> sp;
  csvparser::split(line, sp);
  ASSERT_EQ(sp.size(), 1u);
  EXPECT_STREQ(sp[0], "hello");
}

// ---- split (wchar_t*) ---------------------------------------------------

TEST(CSVSplitW, BasicSemicolon) {
  wchar_t line[] = L"x;y;z";
  vector<wchar_t *> sp;
  csvparser::split(line, sp);
  ASSERT_EQ(sp.size(), 3u);
  EXPECT_EQ(wstring(sp[0]), L"x");
  EXPECT_EQ(wstring(sp[1]), L"y");
  EXPECT_EQ(wstring(sp[2]), L"z");
}

TEST(CSVSplitW, QuotedField) {
  wchar_t line[] = L"\"Anna Svensson\";OK";
  vector<wchar_t *> sp;
  csvparser::split(line, sp);
  ASSERT_EQ(sp.size(), 2u);
  EXPECT_EQ(wstring(sp[0]), L"Anna Svensson");
  EXPECT_EQ(wstring(sp[1]), L"OK");
}

// ---- outputRow ----------------------------------------------------------

TEST(CSVOutput, OutputRowBasic) {
  // Write to a temporary file and verify contents.
  wstring tmpFile = L"/tmp/meos_csv_test_output.csv";
  csvparser csv;
  ASSERT_TRUE(csv.openOutput(tmpFile, false));
  vector<string> row = {"Alice", "Smith", "42"};
  csv.outputRow(row);
  csv.closeOutput();

  std::ifstream fin("/tmp/meos_csv_test_output.csv");
  ASSERT_TRUE(fin.good());
  string line;
  std::getline(fin, line);
  EXPECT_NE(line.find("Alice"), string::npos);
  EXPECT_NE(line.find("Smith"), string::npos);
}

TEST(CSVOutput, OutputRowUTFBOM) {
  wstring tmpFile = L"/tmp/meos_csv_utf_bom.csv";
  csvparser csv;
  ASSERT_TRUE(csv.openOutput(tmpFile, true /*writeUTF*/));
  csv.outputRow(string("test line"));
  csv.closeOutput();

  std::ifstream fin("/tmp/meos_csv_utf_bom.csv", std::ios::binary);
  ASSERT_TRUE(fin.good());
  unsigned char bom[3];
  fin.read((char *)bom, 3);
  EXPECT_EQ(bom[0], 0xEF);
  EXPECT_EQ(bom[1], 0xBB);
  EXPECT_EQ(bom[2], 0xBF);
}

TEST(CSVOutput, OutputRowWString) {
  wstring tmpFile = L"/tmp/meos_csv_wstring.csv";
  csvparser csv;
  ASSERT_TRUE(csv.openOutput(tmpFile, true));
  vector<wstring> row = {L"Alice", L"Smith"};
  csv.outputRow(row);
  csv.closeOutput();

  std::ifstream fin("/tmp/meos_csv_wstring.csv");
  string content((std::istreambuf_iterator<char>(fin)),
                  std::istreambuf_iterator<char>());
  EXPECT_NE(content.find("Alice"), string::npos);
}

// ---- parse --------------------------------------------------------------

static void writeTextFile(const wstring &path, const string &content) {
  std::ofstream f;
  f.open(std::filesystem::path(path));
  f << content;
}

TEST(CSVParse, ParseUTF8WithBOM) {
  wstring path = L"/tmp/meos_csv_parse_utf8bom.csv";
  // UTF-8 BOM + semicolon-separated content
  string content = "\xEF\xBB\xBF""Name;Club;Class\nAlice;IFK;H21\nBob;OK;D20\n";
  writeTextFile(path, content);

  csvparser csv;
  list<vector<wstring>> data;
  csv.parse(path, data);

  ASSERT_EQ(data.size(), 3u);
  auto it = data.begin();
  EXPECT_EQ((*it)[0], L"Name");
  ++it;
  EXPECT_EQ((*it)[0], L"Alice");
  EXPECT_EQ((*it)[1], L"IFK");
  ++it;
  EXPECT_EQ((*it)[0], L"Bob");
}

TEST(CSVParse, ParsePlainASCII) {
  wstring path = L"/tmp/meos_csv_ascii.csv";
  writeTextFile(path, "A;B;C\n1;2;3\n");

  csvparser csv;
  list<vector<wstring>> data;
  csv.parse(path, data);

  ASSERT_EQ(data.size(), 2u);
  auto it = data.begin();
  EXPECT_EQ((*it)[0], L"A");
  ++it;
  EXPECT_EQ((*it)[2], L"3");
}

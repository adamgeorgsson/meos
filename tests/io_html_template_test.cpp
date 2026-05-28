#include <gtest/gtest.h>
#include "html_template.h"
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace meos::io;

// ---------------------------------------------------------------------------
// Parsing tests
// ---------------------------------------------------------------------------

TEST(HtmlTemplateTest, ParsesHeaderTagAndName) {
  std::istringstream ss(
      "@MEOS EXPORT TEMPLATE\n"
      "mytag@My Short Name\n"
      "Description line\n"
      "@HEAD\n"
      "<html>\n"
      "@END\n"
      "</html>\n");
  HtmlTemplate t;
  ASSERT_NO_THROW(t.read(ss));
  EXPECT_EQ(t.info.tag, "mytag");
  EXPECT_EQ(t.info.shortName, "My Short Name");
  EXPECT_EQ(t.info.description, "Description line");
}

TEST(HtmlTemplateTest, InvalidHeaderThrows) {
  std::istringstream ss("NOT A TEMPLATE\nmytag@name\n@HEAD\n<html>\n@END\n</html>\n");
  HtmlTemplate t;
  EXPECT_THROW(t.read(ss), std::runtime_error);
}

TEST(HtmlTemplateTest, CommentLinesSkipped) {
  std::istringstream ss(
      "@MEOS EXPORT TEMPLATE\n"
      "tag@Name\n"
      "% this is a comment\n"
      "Real description\n"
      "% another comment\n"
      "@HEAD\n"
      "% comment inside head\n"
      "head content\n"
      "@END\n"
      "end content\n");
  HtmlTemplate t;
  t.read(ss);
  EXPECT_EQ(t.info.description, "Real description");
  EXPECT_EQ(t.head, "head content\n");
  EXPECT_EQ(t.end, "end content\n");
}

TEST(HtmlTemplateTest, UseTableFlag) {
  std::istringstream ss(
      "@MEOS EXPORT TEMPLATE\n"
      "tag@Name\n"
      "@USETABLE\n"
      "@HEAD\n"
      "head\n"
      "@END\n"
      "end\n");
  HtmlTemplate t;
  t.read(ss);
  EXPECT_TRUE(t.useTables);
}

TEST(HtmlTemplateTest, UseTableFlagDefaultFalse) {
  std::istringstream ss(
      "@MEOS EXPORT TEMPLATE\n"
      "tag@Name\n"
      "@HEAD\n"
      "head\n"
      "@END\n"
      "end\n");
  HtmlTemplate t;
  t.read(ss);
  EXPECT_FALSE(t.useTables);
}

TEST(HtmlTemplateTest, ParsesAllSections) {
  std::istringstream ss(
      "@MEOS EXPORT TEMPLATE\n"
      "tag@Name\n"
      "@HEAD\n"
      "HEAD_CONTENT\n"
      "@OUTERPAGE\n"
      "OUTER_CONTENT\n"
      "@INNERPAGE\n"
      "INNER_CONTENT\n"
      "@SEPARATOR\n"
      "SEP_CONTENT\n"
      "@END\n"
      "END_CONTENT\n");
  HtmlTemplate t;
  t.read(ss);
  EXPECT_EQ(t.head,      "HEAD_CONTENT\n");
  EXPECT_EQ(t.outerPage, "OUTER_CONTENT\n");
  EXPECT_EQ(t.innerPage, "INNER_CONTENT\n");
  EXPECT_EQ(t.separator, "SEP_CONTENT\n");
  EXPECT_EQ(t.end,       "END_CONTENT\n");
}

TEST(HtmlTemplateTest, ParseFormattableStyle) {
  std::istringstream ss(
      "@MEOS EXPORT TEMPLATE\n"
      "tableformat@Formatted table\n"
      "Table with format\n"
      "% comment line\n"
      "@USETABLE\n"
      "@HEAD\n"
      "<!DOCTYPE html>\n"
      "<html>\n"
      "@END\n"
      "</html>\n");
  HtmlTemplate t;
  ASSERT_NO_THROW(t.read(ss));
  EXPECT_EQ(t.info.tag,       "tableformat");
  EXPECT_EQ(t.info.shortName, "Formatted table");
  EXPECT_TRUE(t.useTables);
  EXPECT_NE(t.head.find("<!DOCTYPE html>"), std::string::npos);
  EXPECT_NE(t.end.find("</html>"), std::string::npos);
  EXPECT_TRUE(t.outerPage.empty());
  EXPECT_TRUE(t.innerPage.empty());
}

TEST(HtmlTemplateTest, MultiLineDescription) {
  std::istringstream ss(
      "@MEOS EXPORT TEMPLATE\n"
      "tag@Name\n"
      "First line\n"
      "Second line\n"
      "@HEAD\n"
      "head\n"
      "@END\n"
      "end\n");
  HtmlTemplate t;
  t.read(ss);
  EXPECT_EQ(t.info.description, "First line\nSecond line");
}

// ---------------------------------------------------------------------------
// Substitute tests
// ---------------------------------------------------------------------------

TEST(HtmlTemplateTest, SubstituteSingleVar) {
  auto result = HtmlTemplate::substitute("Hello @TITLE world", {{"@TITLE", "MeOS"}});
  EXPECT_EQ(result, "Hello MeOS world");
}

TEST(HtmlTemplateTest, SubstituteMultipleVars) {
  auto result = HtmlTemplate::substitute("@TITLE | @MEOS",
                                          {{"@TITLE", "Test"}, {"@MEOS", "3.0"}});
  EXPECT_EQ(result, "Test | 3.0");
}

TEST(HtmlTemplateTest, SubstituteLongestMatchFirst) {
  // @NUMPAGE (7 chars) must win over @NUM (4 chars) at the same position
  auto result = HtmlTemplate::substitute("@NUMPAGE",
                                          {{"@NUM", "1"}, {"@NUMPAGE", "5"}});
  EXPECT_EQ(result, "5");
}

TEST(HtmlTemplateTest, SubstituteUnmatchedAtPreserved) {
  auto result = HtmlTemplate::substitute("@UNKNOWN", {{"@TITLE", "Test"}});
  EXPECT_EQ(result, "@UNKNOWN");
}

TEST(HtmlTemplateTest, SubstituteEmptyVarsReturnsTemplate) {
  auto result = HtmlTemplate::substitute("@TITLE", {});
  EXPECT_EQ(result, "@TITLE");
}

TEST(HtmlTemplateTest, SubstituteEmptyTemplateReturnsEmpty) {
  auto result = HtmlTemplate::substitute("", {{"@TITLE", "Test"}});
  EXPECT_EQ(result, "");
}

TEST(HtmlTemplateTest, SubstituteNoAtSign) {
  auto result = HtmlTemplate::substitute("no placeholders", {{"@TITLE", "Test"}});
  EXPECT_EQ(result, "no placeholders");
}

// ---------------------------------------------------------------------------
// Generate tests
// ---------------------------------------------------------------------------

TEST(HtmlTemplateTest, GenerateHeadAndEnd) {
  HtmlTemplate t;
  t.head = "<html><body>@TITLE@DESCRIPTION@CONTENTS";
  t.end  = "</body></html>";
  std::ostringstream out;
  t.generate(out, "Competition", "Class A", {{{"<p>Result1</p>"}}});
  std::string result = out.str();
  EXPECT_NE(result.find("Competition"),    std::string::npos);
  EXPECT_NE(result.find("Class A"),        std::string::npos);
  EXPECT_NE(result.find("<p>Result1</p>"), std::string::npos);
  EXPECT_NE(result.find("</body></html>"), std::string::npos);
}

TEST(HtmlTemplateTest, GenerateWithOuterAndInnerPage) {
  HtmlTemplate t;
  t.head      = "<html>@CONTENTS";
  t.outerPage = "<div class='page'>@CONTENTS</div>";
  t.innerPage = "<div class='col'>@CONTENTS</div>";
  t.separator = "<div class='sep'/>";
  t.end       = "</html>";
  std::ostringstream out;
  t.generate(out, "Title", "Desc", {{"ColA", "ColB"}});
  std::string result = out.str();
  EXPECT_NE(result.find("class='page'"), std::string::npos);
  EXPECT_NE(result.find("class='col'"),  std::string::npos);
  EXPECT_NE(result.find("ColA"),         std::string::npos);
  EXPECT_NE(result.find("ColB"),         std::string::npos);
  EXPECT_NE(result.find("class='sep'"),  std::string::npos);
}

TEST(HtmlTemplateTest, GenerateSeparatorOnlyBetweenColumns) {
  HtmlTemplate t;
  t.head      = "@CONTENTS";
  t.innerPage = "@CONTENTS";
  t.separator = "---SEP---";
  std::ostringstream out;
  t.generate(out, "T", "D", {{"A", "B", "C"}});
  std::string result = out.str();
  // Exactly 2 separators for 3 columns
  size_t pos = 0;
  int count = 0;
  while ((pos = result.find("---SEP---", pos)) != std::string::npos) {
    ++count;
    pos += 9;
  }
  EXPECT_EQ(count, 2);
}

TEST(HtmlTemplateTest, GenerateMultipleOuterPages) {
  HtmlTemplate t;
  t.head      = "@CONTENTS";
  t.outerPage = "<page>@CONTENTS</page>";
  t.innerPage = "<col>@CONTENTS</col>";
  std::ostringstream out;
  t.generate(out, "Title", "Desc", {{{"Page1"}}, {{"Page2"}}});
  std::string result = out.str();
  EXPECT_NE(result.find("Page1"), std::string::npos);
  EXPECT_NE(result.find("Page2"), std::string::npos);
}

TEST(HtmlTemplateTest, PagePlaceholderInInnerPage) {
  HtmlTemplate t;
  t.head      = "@CONTENTS";
  t.innerPage = "<col id='@PAGE'>@CONTENTS</col>";
  std::ostringstream out;
  t.generate(out, "T", "D", {{"A", "B"}});
  std::string result = out.str();
  EXPECT_NE(result.find("id='1'"), std::string::npos);
  EXPECT_NE(result.find("id='2'"), std::string::npos);
}

TEST(HtmlTemplateTest, NumPageAndNumColPlaceholders) {
  HtmlTemplate t;
  t.head = "@NUMPAGE:@NUMCOL";
  std::ostringstream out;
  t.generate(out, "T", "D", {{"A", "B"}, {"C", "D"}});
  std::string result = out.str();
  EXPECT_NE(result.find("2:2"), std::string::npos);
}

TEST(HtmlTemplateTest, EmptyPagesProducesHeadAndEnd) {
  HtmlTemplate t;
  t.head = "<html>";
  t.end  = "</html>";
  std::ostringstream out;
  t.generate(out, "T", "D", {});
  std::string result = out.str();
  EXPECT_NE(result.find("<html>"),  std::string::npos);
  EXPECT_NE(result.find("</html>"), std::string::npos);
}

TEST(HtmlTemplateTest, GenerateFile) {
  HtmlTemplate t;
  t.head = "<html>@TITLE@CONTENTS</html>";
  namespace fs = std::filesystem;
  auto tmpFile = fs::temp_directory_path() / "meos_html_test_us014c.html";
  t.generateFile(tmpFile.wstring(), "TestTitle", "Desc", {{{"<p>body</p>"}}});
  std::ifstream f(tmpFile.string());
  ASSERT_TRUE(f.is_open());
  std::string content((std::istreambuf_iterator<char>(f)), {});
  EXPECT_NE(content.find("TestTitle"),    std::string::npos);
  EXPECT_NE(content.find("<p>body</p>"), std::string::npos);
  fs::remove(tmpFile);
}

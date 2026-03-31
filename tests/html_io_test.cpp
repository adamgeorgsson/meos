// html_io_test.cpp — Tests for HtmlWriter (US-014c)
#include <gtest/gtest.h>
#include "HtmlWriter.h"
#include <sstream>
#include <string>
#include <vector>

// ── HtmlTemplate::read ────────────────────────────────────────────────────────

TEST(HtmlTemplateTest, ParsesSections) {
  std::istringstream ss(
    "@MEOS EXPORT TEMPLATE\n"
    "mytag@My Template\n"
    "@HEAD\n"
    "<html><head><title>@TITLE</title></head>\n"
    "@END\n"
    "@OUTERPAGE\n"
    "<body>\n"
    "@INNERPAGE\n"
    "@CONTENTS\n"
    "@END\n"
    "</body>\n"
    "@END\n"
  );
  HtmlTemplate tmpl = HtmlTemplate::read(ss);
  EXPECT_EQ(tmpl.tag, "mytag");
  EXPECT_FALSE(tmpl.empty());
  EXPECT_NE(tmpl.head.find("<html>"), std::string::npos);
  EXPECT_NE(tmpl.head.find("@TITLE"), std::string::npos);
  EXPECT_NE(tmpl.innerPage.find("@CONTENTS"), std::string::npos);
}

TEST(HtmlTemplateTest, ParsesUseTable) {
  std::istringstream ss(
    "@MEOS EXPORT TEMPLATE\n"
    "tag@Name\n"
    "@USETABLE\n"
    "@HEAD\n"
    "<html/>\n"
    "@END\n"
  );
  HtmlTemplate tmpl = HtmlTemplate::read(ss);
  EXPECT_TRUE(tmpl.useTables);
}

TEST(HtmlTemplateTest, IgnoresCommentLines) {
  std::istringstream ss(
    "@MEOS EXPORT TEMPLATE\n"
    "% This is a comment\n"
    "tag@Name\n"
    "@HEAD\n"
    "% comment inside head\n"
    "<h1>Title</h1>\n"
    "@END\n"
  );
  HtmlTemplate tmpl = HtmlTemplate::read(ss);
  EXPECT_NE(tmpl.head.find("<h1>"), std::string::npos);
  EXPECT_EQ(tmpl.head.find("comment"), std::string::npos);
}

TEST(HtmlTemplateTest, RequiresMeosMarkerFirstLine) {
  std::istringstream ss(
    "NOT A TEMPLATE\n"
    "tag@Name\n"
  );
  HtmlTemplate tmpl = HtmlTemplate::read(ss);
  EXPECT_TRUE(tmpl.empty());
}

TEST(HtmlTemplateTest, ParsesSeparator) {
  std::istringstream ss(
    "@MEOS EXPORT TEMPLATE\n"
    "tag@Name\n"
    "@OUTERPAGE\n"
    "@INNERPAGE\n"
    "col1\n"
    "@SEPARATOR\n"
    "| sep |\n"
    "@INNERPAGE\n"
    "col2\n"
    "@END\n"
  );
  HtmlTemplate tmpl = HtmlTemplate::read(ss);
  EXPECT_NE(tmpl.separator.find("| sep |"), std::string::npos);
  EXPECT_NE(tmpl.innerPage.find("col1"), std::string::npos);
}

// ── HtmlWriter::encodeHtml ────────────────────────────────────────────────────

TEST(HtmlWriterTest, EncodesAmpersand) {
  EXPECT_EQ(HtmlWriter::encodeHtml("A&B"), "A&amp;B");
}

TEST(HtmlWriterTest, EncodesAngleBrackets) {
  std::string result = HtmlWriter::encodeHtml("<div>");
  EXPECT_EQ(result, "&lt;div&gt;");
}

TEST(HtmlWriterTest, EncodesQuotes) {
  EXPECT_EQ(HtmlWriter::encodeHtml("\"hello\""), "&quot;hello&quot;");
}

TEST(HtmlWriterTest, EncodesWstring) {
  std::wstring w = L"Rød & Blå";
  std::string result = HtmlWriter::encodeHtml(w);
  EXPECT_NE(result.find("&amp;"), std::string::npos);
}

TEST(HtmlWriterTest, PlainTextUnchanged) {
  EXPECT_EQ(HtmlWriter::encodeHtml("Hello World"), "Hello World");
}

// ── HtmlWriter::replacePlaceholders ──────────────────────────────────────────

TEST(HtmlWriterTest, ReplacesTitleLongForm) {
  std::string s = "Page: @TITLE";
  HtmlWriter::replacePlaceholders(s, L"My Race", "");
  EXPECT_EQ(s, "Page: My Race");
}

TEST(HtmlWriterTest, ReplacesTitleShortForm) {
  std::string s = "Page: @T";
  HtmlWriter::replacePlaceholders(s, L"My Race", "");
  EXPECT_EQ(s, "Page: My Race");
}

TEST(HtmlWriterTest, LongFormWinsOverShortForm) {
  // @TITLE should not be broken by @T replacement
  std::string s = "@TITLE";
  HtmlWriter::replacePlaceholders(s, L"Championship", "");
  EXPECT_EQ(s, "Championship");
}

TEST(HtmlWriterTest, ReplacesContents) {
  std::string s = "<div>@CONTENTS</div>";
  HtmlWriter::replacePlaceholders(s, L"", "<p>data</p>");
  EXPECT_EQ(s, "<div><p>data</p></div>");
}

TEST(HtmlWriterTest, ReplacesContentsShortAlias) {
  std::string s = "<div>@C</div>";
  HtmlWriter::replacePlaceholders(s, L"", "<p>data</p>");
  EXPECT_EQ(s, "<div><p>data</p></div>");
}

TEST(HtmlWriterTest, ReplacesDescription) {
  std::string s = "@DESCRIPTION";
  HtmlWriter::replacePlaceholders(s, L"", "", L"M21");
  EXPECT_EQ(s, "M21");
}

TEST(HtmlWriterTest, ReplacesNumPage) {
  std::string s = "@NUMPAGE";
  HtmlWriter::replacePlaceholders(s, L"", "", L"", 0, 1, 5);
  EXPECT_EQ(s, "5");
}

TEST(HtmlWriterTest, ReplacesTime) {
  std::string s = "@TIME";
  HtmlWriter::replacePlaceholders(s, L"", "", L"", 3000);
  EXPECT_EQ(s, "3000");
}

TEST(HtmlWriterTest, ReplacesMeos) {
  std::string s = "@MEOSVERSION - @MEOS - @M";
  HtmlWriter::replacePlaceholders(s, L"", "");
  EXPECT_EQ(s, "MeOS - MeOS - MeOS");
}

TEST(HtmlWriterTest, HtmlEncodesTitleInPlaceholder) {
  std::string s = "@TITLE";
  HtmlWriter::replacePlaceholders(s, L"A & B", "");
  EXPECT_EQ(s, "A &amp; B");
}

// ── HtmlWriter::rowsToTableHtml ───────────────────────────────────────────────

TEST(HtmlWriterTest, RowsToTableHtmlBasic) {
  std::vector<HtmlRow> rows;
  HtmlRow r1;
  r1.cells.push_back({"1", "", false});
  r1.cells.push_back({"Anna", "", false});
  r1.cells.push_back({"35:00", "", false});
  rows.push_back(r1);
  std::string html = HtmlWriter::rowsToTableHtml(rows);
  EXPECT_NE(html.find("<table>"), std::string::npos);
  EXPECT_NE(html.find("<td>Anna</td>"), std::string::npos);
  EXPECT_NE(html.find("35:00"), std::string::npos);
}

TEST(HtmlWriterTest, RowsToTableHtmlHeader) {
  std::vector<HtmlRow> rows;
  HtmlRow hdr;
  hdr.cells.push_back({"Place", "", true});
  hdr.cells.push_back({"Name",  "", true});
  rows.push_back(hdr);
  std::string html = HtmlWriter::rowsToTableHtml(rows);
  EXPECT_NE(html.find("<th>Place</th>"), std::string::npos);
}

TEST(HtmlWriterTest, RowsToTableHtmlSeparator) {
  std::vector<HtmlRow> rows;
  HtmlRow sep;
  sep.isSeparator = true;
  sep.cells.push_back({"ignored", "", false});
  rows.push_back(sep);
  std::string html = HtmlWriter::rowsToTableHtml(rows);
  EXPECT_NE(html.find("colspan"), std::string::npos);
}

TEST(HtmlWriterTest, RowsToTableHtmlEncodesCellText) {
  std::vector<HtmlRow> rows;
  HtmlRow r;
  r.cells.push_back({"A<B", "", false});
  rows.push_back(r);
  std::string html = HtmlWriter::rowsToTableHtml(rows);
  EXPECT_EQ(html.find("<td>A<B</td>"), std::string::npos);
  EXPECT_NE(html.find("&lt;"), std::string::npos);
}

// ── HtmlWriter::buildTableHtml ────────────────────────────────────────────────

TEST(HtmlWriterTest, BuildTableHtmlIsCompleteDocument) {
  std::vector<HtmlRow> rows;
  HtmlRow r;
  r.cells.push_back({"1st", "", false});
  rows.push_back(r);
  std::string html = HtmlWriter::buildTableHtml(L"Test Race", rows, 0);
  EXPECT_NE(html.find("<!DOCTYPE html>"), std::string::npos);
  EXPECT_NE(html.find("<html>"), std::string::npos);
  EXPECT_NE(html.find("Test Race"), std::string::npos);
  EXPECT_NE(html.find("1st"), std::string::npos);
  EXPECT_NE(html.find("</html>"), std::string::npos);
}

TEST(HtmlWriterTest, BuildTableHtmlRefreshMeta) {
  std::vector<HtmlRow> rows;
  std::string html = HtmlWriter::buildTableHtml(L"Race", rows, 5000);
  EXPECT_NE(html.find("http-equiv=\"refresh\""), std::string::npos);
  EXPECT_NE(html.find("content=\"5\""), std::string::npos);
}

TEST(HtmlWriterTest, BuildTableHtmlNoRefreshWhenZero) {
  std::vector<HtmlRow> rows;
  std::string html = HtmlWriter::buildTableHtml(L"Race", rows, 0);
  EXPECT_EQ(html.find("http-equiv"), std::string::npos);
}

// ── HtmlWriter::generate (template-based) ────────────────────────────────────

TEST(HtmlWriterTest, GenerateUsesTemplate) {
  std::istringstream ss(
    "@MEOS EXPORT TEMPLATE\n"
    "t@Test\n"
    "@HEAD\n"
    "<head><title>@TITLE</title></head>\n"
    "@END\n"
    "@OUTERPAGE\n"
    "<body>\n"
    "@INNERPAGE\n"
    "@CONTENTS\n"
    "@END\n"
    "</body>\n"
    "@END\n"
  );
  HtmlTemplate tmpl = HtmlTemplate::read(ss);
  std::ostringstream out;
  HtmlWriter::generate(out, tmpl, L"Sprint Cup", "<p>results</p>");
  std::string html = out.str();
  EXPECT_NE(html.find("Sprint Cup"), std::string::npos);
  EXPECT_NE(html.find("<p>results</p>"), std::string::npos);
}

TEST(HtmlWriterTest, GenerateTitleHtmlEncoded) {
  std::istringstream ss(
    "@MEOS EXPORT TEMPLATE\n"
    "t@Test\n"
    "@HEAD\n"
    "<title>@TITLE</title>\n"
    "@END\n"
  );
  HtmlTemplate tmpl = HtmlTemplate::read(ss);
  std::ostringstream out;
  HtmlWriter::generate(out, tmpl, L"A & B Cup", "");
  EXPECT_NE(out.str().find("A &amp; B Cup"), std::string::npos);
}

// ── oListParam fields (regression: ensure selection + lockUpdate present) ────

#include "oListInfo.h"

TEST(OListParamTest, SelectionFieldExists) {
  oListParam p;
  EXPECT_TRUE(p.selection.empty());
  p.selection.insert(10);
  p.selection.insert(20);
  EXPECT_EQ(p.selection.size(), 2u);
}

TEST(OListParamTest, LockUpdateFieldExists) {
  oListParam p;
  EXPECT_FALSE(p.lockUpdate);
  p.lockUpdate = true;
  EXPECT_TRUE(p.lockUpdate);
}

TEST(OListParamTest, FilterIncludeUnlimited) {
  oListParam p;
  p.filterMaxPer = 0;
  EXPECT_TRUE(p.filterInclude(0, nullptr));
  EXPECT_TRUE(p.filterInclude(100, nullptr));
}

TEST(OListParamTest, FilterIncludeRespectsMax) {
  oListParam p;
  p.filterMaxPer = 3;
  EXPECT_TRUE(p.filterInclude(0, nullptr));
  EXPECT_TRUE(p.filterInclude(2, nullptr));
  EXPECT_FALSE(p.filterInclude(3, nullptr));  // count >= filterMaxPer
}

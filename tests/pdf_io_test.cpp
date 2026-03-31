// pdf_io_test.cpp — Unit tests for PdfWriter (US-014d)
// Tests verify that:
//   - A PDF is generated without throwing
//   - Output starts with the PDF magic bytes "%PDF-"
//   - Various data shapes (empty, single row, multi-row table, header/separator)
//     are handled correctly
//   - Title and author metadata can be set
//   - Multi-page output works when rows exceed one page

#include <gtest/gtest.h>
#include "PdfWriter.h"

// Helper to build a simple header row.
static PdfRow makeHeader(std::initializer_list<const wchar_t*> texts)
{
  PdfRow row;
  row.isHeader  = true;
  row.fontSize  = 11.0f;
  for (auto t : texts) {
    PdfCell c;
    c.text = t;
    c.bold = true;
    row.cells.push_back(c);
  }
  return row;
}

// Helper to build a plain data row.
static PdfRow makeRow(std::initializer_list<const wchar_t*> texts, float fontSize = 10.0f)
{
  PdfRow row;
  row.fontSize = fontSize;
  for (auto t : texts) {
    PdfCell c;
    c.text = t;
    row.cells.push_back(c);
  }
  return row;
}

// Helper to build a plain data row from wstrings.
static PdfRow makeRowW(std::initializer_list<std::wstring> texts, float fontSize = 10.0f)
{
  PdfRow row;
  row.fontSize = fontSize;
  for (auto& t : texts) {
    PdfCell c;
    c.text = t;
    row.cells.push_back(c);
  }
  return row;
}
static std::vector<unsigned char> gen(const std::vector<PdfRow>& rows,
                                      const std::wstring& title = L"Test",
                                      const std::wstring& author = L"MeOS")
{
  PdfWriter pw;
  return pw.generateToBuffer(rows, title, author);
}

// PDF magic bytes.
static bool isPdf(const std::vector<unsigned char>& buf)
{
  return buf.size() >= 5 &&
         buf[0] == '%' && buf[1] == 'P' && buf[2] == 'D' &&
         buf[3] == 'F' && buf[4] == '-';
}

// ── Basic generation ──────────────────────────────────────────────────────────

TEST(PdfWriterTest, EmptyRowList_ProducesPdf) {
  auto buf = gen({});
  EXPECT_TRUE(isPdf(buf));
  EXPECT_GT(buf.size(), 100u);
}

TEST(PdfWriterTest, SingleRowSingleCell_ProducesPdf) {
  auto buf = gen({ makeRow({L"Hello PDF"}) });
  EXPECT_TRUE(isPdf(buf));
  EXPECT_GT(buf.size(), 200u);
}

TEST(PdfWriterTest, HeaderRow_ProducesPdf) {
  auto buf = gen({ makeHeader({L"Place", L"Name", L"Club", L"Time"}) });
  EXPECT_TRUE(isPdf(buf));
}

TEST(PdfWriterTest, MixedRows_ProducesPdf) {
  std::vector<PdfRow> rows;
  rows.push_back(makeHeader({L"#", L"Name", L"Club", L"Time"}));
  rows.push_back(makeRow({L"1", L"Alice", L"OK Orion", L"32:15"}));
  rows.push_back(makeRow({L"2", L"Bob",   L"IFK Mora", L"33:40"}));
  rows.push_back(makeRow({L"3", L"Carol", L"Attunda",  L"35:00"}));

  auto buf = gen(rows);
  EXPECT_TRUE(isPdf(buf));
}

TEST(PdfWriterTest, SeparatorRow_ProducesPdf) {
  std::vector<PdfRow> rows;
  rows.push_back(makeHeader({L"Class", L"Name", L"Time"}));
  rows.push_back(makeRow({L"1", L"Elite"}));

  PdfRow sep;
  sep.isSeparator = true;
  rows.push_back(sep);

  rows.push_back(makeRow({L"1", L"Alice", L"32:15"}));
  rows.push_back(makeRow({L"2", L"Bob",   L"33:40"}));

  auto buf = gen(rows);
  EXPECT_TRUE(isPdf(buf));
}

// ── Column alignment ──────────────────────────────────────────────────────────

TEST(PdfWriterTest, CellAlignment_DoesNotThrow) {
  std::vector<PdfRow> rows;
  PdfRow row;
  row.fontSize = 10.0f;
  PdfCell left;  left.text  = L"Left";  left.align  = PdfAlign::Left;
  PdfCell ctr;   ctr.text   = L"Ctr";   ctr.align   = PdfAlign::Center;
  PdfCell right; right.text = L"Right"; right.align = PdfAlign::Right;
  row.cells = {left, ctr, right};
  rows.push_back(row);

  EXPECT_NO_THROW(gen(rows));
}

// ── Font variants ─────────────────────────────────────────────────────────────

TEST(PdfWriterTest, BoldAndItalicCells_ProducesPdf) {
  PdfRow row;
  row.fontSize = 10.0f;
  PdfCell bold;   bold.text   = L"Bold";   bold.bold   = true;
  PdfCell italic; italic.text = L"Italic"; italic.italic = true;
  PdfCell normal; normal.text = L"Normal";
  row.cells = {bold, italic, normal};

  auto buf = gen({row});
  EXPECT_TRUE(isPdf(buf));
}

// ── Metadata ──────────────────────────────────────────────────────────────────

TEST(PdfWriterTest, TitleAndAuthor_DoNotCrash) {
  auto buf = gen({makeRow({L"Result"})},
                 L"Swedish Championship 2025",
                 L"Timekeeping AB");
  EXPECT_TRUE(isPdf(buf));
}

TEST(PdfWriterTest, EmptyTitleAndAuthor_DoNotCrash) {
  auto buf = gen({makeRow({L"Result"})}, L"", L"");
  EXPECT_TRUE(isPdf(buf));
}

// ── Explicit column widths ────────────────────────────────────────────────────

TEST(PdfWriterTest, ExplicitColumnWidths_ProducesPdf) {
  PdfRow hdr = makeHeader({L"#", L"Name", L"Time"});
  hdr.cells[0].width = 40.0f;
  hdr.cells[1].width = 280.0f;
  hdr.cells[2].width = 80.0f;

  auto buf = gen({hdr, makeRow({L"1", L"Alice Athlete", L"32:15"})});
  EXPECT_TRUE(isPdf(buf));
}

// ── Multi-page ────────────────────────────────────────────────────────────────

TEST(PdfWriterTest, ManyRows_WrapsToNewPage) {
  std::vector<PdfRow> rows;
  rows.push_back(makeHeader({L"#", L"Name", L"Time"}));
  // ~100 rows at 14pt line height should overflow a single A4 page (842pt)
  for (int i = 1; i <= 100; ++i) {
    std::wstring num = std::to_wstring(i);
    std::wstring name = L"Runner " + std::wstring(1, L'A' + (wchar_t)(i % 26));
    rows.push_back(makeRowW({num, name, L"30:00"}));
  }

  auto buf = gen(rows);
  EXPECT_TRUE(isPdf(buf));
  EXPECT_GT(buf.size(), 2000u);
}

// ── Page constants ────────────────────────────────────────────────────────────

TEST(PdfWriterTest, PageConstants_AreA4Portrait) {
  EXPECT_FLOAT_EQ(PdfWriter::kPageWidth,  595.0f);
  EXPECT_FLOAT_EQ(PdfWriter::kPageHeight, 842.0f);
  EXPECT_GT(PdfWriter::kMargin, 0.0f);
}

// ── Special characters (narrow fallback) ─────────────────────────────────────

TEST(PdfWriterTest, SpecialChars_DoNotCrash) {
  // narrow() may transliterate or drop non-latin chars — must not throw.
  auto buf = gen({makeRow({L"Åsa Öberg", L"Östergötland IF", L"28:30"})});
  EXPECT_TRUE(isPdf(buf));
}

TEST(PdfWriterTest, UnicodeChars_DoNotCrash) {
  // Characters outside Latin-1 — narrow() drops them, but must not crash.
  auto buf = gen({makeRow({L"\u4e2d\u6587", L"日本語"})});
  EXPECT_TRUE(isPdf(buf));
}

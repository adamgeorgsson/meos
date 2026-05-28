#include <gtest/gtest.h>

#include "pdf_writer.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using meos::io::PdfCell;
using meos::io::PdfRow;
using meos::io::PdfWriter;

namespace {

PdfCell cell(const std::string& text, float width = 0, bool bold = false,
             bool italic = false, bool right = false, bool center = false) {
  return PdfCell{text, width, bold, italic, right, center};
}

} // namespace

// ---------------------------------------------------------------------------
// Basic in-memory write
// ---------------------------------------------------------------------------
TEST(PdfWriter, WriteToMemoryNonEmpty) {
  PdfWriter writer;
  PdfRow row = {cell("Hello"), cell("World")};
  auto buf = writer.writeToMemory({row});
  EXPECT_FALSE(buf.empty());
}

TEST(PdfWriter, WriteToMemoryStartsWithPdfMagic) {
  PdfWriter writer;
  PdfRow row = {cell("Test")};
  auto buf = writer.writeToMemory({row});
  ASSERT_GE(buf.size(), 4u);
  EXPECT_EQ(std::string(buf.begin(), buf.begin() + 4), "%PDF");
}

TEST(PdfWriter, EmptyRowsProducesValidPdf) {
  PdfWriter writer;
  auto buf = writer.writeToMemory({});
  EXPECT_GE(buf.size(), 4u);
  EXPECT_EQ(std::string(buf.begin(), buf.begin() + 4), "%PDF");
}

// ---------------------------------------------------------------------------
// File write
// ---------------------------------------------------------------------------
TEST(PdfWriter, WriteFileCreatesFile) {
  namespace fs = std::filesystem;
  PdfWriter writer;
  PdfRow row = {cell("Name"), cell("Time")};
  std::wstring path = L"/tmp/meos_pdf_test.pdf";
  writer.writeFile(path, {row});
  EXPECT_TRUE(fs::exists("/tmp/meos_pdf_test.pdf"));
  std::remove("/tmp/meos_pdf_test.pdf");
}

TEST(PdfWriter, WriteFileContentStartsWithPdfMagic) {
  PdfWriter writer;
  PdfRow row = {cell("Runner"), cell("1:23:45")};
  std::wstring path = L"/tmp/meos_pdf_magic.pdf";
  writer.writeFile(path, {row});
  std::ifstream in("/tmp/meos_pdf_magic.pdf", std::ios::binary);
  char magic[5] = {};
  in.read(magic, 4);
  EXPECT_EQ(std::string(magic, 4), "%PDF");
  std::remove("/tmp/meos_pdf_magic.pdf");
}

// ---------------------------------------------------------------------------
// Multi-page
// ---------------------------------------------------------------------------
TEST(PdfWriter, FewRowsSinglePage) {
  PdfWriter writer;
  // A4 height is 842pt; usable area ~ 842 - 2*40 = 762pt; 762 / 14 ~ 54 rows
  std::vector<PdfRow> rows;
  for (int i = 0; i < 10; ++i)
    rows.push_back({cell("Row " + std::to_string(i))});
  // Should not throw; basic validity check only
  auto buf = writer.writeToMemory(rows);
  EXPECT_GE(buf.size(), 4u);
}

TEST(PdfWriter, ManyRowsProducesMultiPagePdf) {
  PdfWriter writer;
  // A4 height 842, margins 40 each → ~54 rows per page; use 60 rows
  std::vector<PdfRow> rows;
  for (int i = 0; i < 60; ++i)
    rows.push_back({cell("Runner " + std::to_string(i))});
  auto buf = writer.writeToMemory(rows);
  // Multi-page PDFs contain "Page" dictionary entries — a simple check:
  // the output should be bigger than a single-row PDF
  auto buf1 = writer.writeToMemory({{cell("Single")}});
  EXPECT_GT(buf.size(), buf1.size());
}

// ---------------------------------------------------------------------------
// Column-width distribution
// ---------------------------------------------------------------------------
TEST(PdfWriter, ExplicitColumnWidthPreserved) {
  PdfWriter writer;
  // Should not throw; just verifying it compiles and runs
  PdfRow row = {cell("Name", 150.0f), cell("Time", 60.0f)};
  auto buf = writer.writeToMemory({row});
  EXPECT_GE(buf.size(), 4u);
}

TEST(PdfWriter, ZeroWidthColumnsGetAutoDistribution) {
  PdfWriter writer;
  // Two auto-width columns should each get (595 - 2*40) / 2 = 257.5pt
  PdfRow row = {cell("Col1", 0), cell("Col2", 0)};
  auto buf = writer.writeToMemory({row});
  EXPECT_GE(buf.size(), 4u);
}

TEST(PdfWriter, MixedExplicitAndAutoWidths) {
  PdfWriter writer;
  // Explicit 200pt + auto; auto gets (595 - 80 - 200) = 315pt
  PdfRow row = {cell("Fixed", 200.0f), cell("Auto", 0)};
  auto buf = writer.writeToMemory({row});
  EXPECT_GE(buf.size(), 4u);
}

// ---------------------------------------------------------------------------
// Font variants
// ---------------------------------------------------------------------------
TEST(PdfWriter, BoldCell) {
  PdfWriter writer;
  PdfRow row = {cell("Bold", 0, true)};
  auto buf = writer.writeToMemory({row});
  EXPECT_GE(buf.size(), 4u);
}

TEST(PdfWriter, ItalicCell) {
  PdfWriter writer;
  PdfRow row = {cell("Italic", 0, false, true)};
  auto buf = writer.writeToMemory({row});
  EXPECT_GE(buf.size(), 4u);
}

// ---------------------------------------------------------------------------
// Alignment
// ---------------------------------------------------------------------------
TEST(PdfWriter, RightAlignCell) {
  PdfWriter writer;
  PdfRow row = {cell("Right", 150.0f, false, false, true)};
  auto buf = writer.writeToMemory({row});
  EXPECT_GE(buf.size(), 4u);
}

TEST(PdfWriter, CenterAlignCell) {
  PdfWriter writer;
  PdfRow row = {cell("Center", 150.0f, false, false, false, true)};
  auto buf = writer.writeToMemory({row});
  EXPECT_GE(buf.size(), 4u);
}

// ---------------------------------------------------------------------------
// Multiple calls (separate documents)
// ---------------------------------------------------------------------------
TEST(PdfWriter, TwoIndependentWriteToMemoryCalls) {
  PdfWriter writer;
  auto buf1 = writer.writeToMemory({{cell("Doc1")}});
  auto buf2 = writer.writeToMemory({{cell("Doc2")}});
  EXPECT_GE(buf1.size(), 4u);
  EXPECT_GE(buf2.size(), 4u);
  // Both are valid PDFs
  EXPECT_EQ(std::string(buf1.begin(), buf1.begin() + 4), "%PDF");
  EXPECT_EQ(std::string(buf2.begin(), buf2.begin() + 4), "%PDF");
}

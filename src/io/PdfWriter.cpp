// PdfWriter.cpp — PDF result generation using libharu.
// Uses standard PDF Type 1 fonts (Helvetica family) for cross-platform
// compatibility without external font files.

#include "PdfWriter.h"
#include "meos_util.h"      // narrow(), toUTF8()
#include "meosexception.h"

#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <cstdio>

// ── Error handler ─────────────────────────────────────────────────────────────

static void pdfErrorHandler(HPDF_STATUS error_no,
                             HPDF_STATUS detail_no,
                             void* /*user_data*/)
{
  char buf[128];
  std::snprintf(buf, sizeof(buf),
                "libharu error: 0x%04X detail: 0x%04X",
                (unsigned)error_no, (unsigned)detail_no);
  throw meosException(buf);
}

// ── Constructor / Destructor ──────────────────────────────────────────────────

PdfWriter::PdfWriter() = default;

PdfWriter::~PdfWriter()
{
  cleanup();
}

// ── Private helpers ───────────────────────────────────────────────────────────

void PdfWriter::init()
{
  doc_ = HPDF_New(pdfErrorHandler, nullptr);
  if (!doc_)
    throw meosException("HPDF_New failed");

  HPDF_SetCompressionMode(doc_, HPDF_COMP_ALL);
  loadFonts();
}

void PdfWriter::cleanup()
{
  if (doc_) {
    HPDF_Free(doc_);
    doc_  = nullptr;
    page_ = nullptr;
    fontNormal_ = fontBold_ = fontItalic_ = nullptr;
    curFont_ = nullptr;
    curSize_ = 0.0f;
  }
}

void PdfWriter::loadFonts()
{
  fontNormal_ = HPDF_GetFont(doc_, "Helvetica",        nullptr);
  fontBold_   = HPDF_GetFont(doc_, "Helvetica-Bold",   nullptr);
  fontItalic_ = HPDF_GetFont(doc_, "Helvetica-Oblique",nullptr);
}

void PdfWriter::addPage()
{
  page_ = HPDF_AddPage(doc_);
  HPDF_Page_SetSize(page_, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);
  curY_    = kPageHeight - kMargin;
  curFont_ = nullptr;
  curSize_ = 0.0f;
}

void PdfWriter::selectFont(bool bold, bool italic, float size)
{
  HPDF_Font desired = fontNormal_;
  if (bold)        desired = fontBold_;
  else if (italic) desired = fontItalic_;

  if (desired != curFont_ || size != curSize_) {
    HPDF_Page_SetFontAndSize(page_, desired, size);
    curFont_ = desired;
    curSize_ = size;
  }
}

void PdfWriter::drawHLine(float y)
{
  HPDF_Page_MoveTo(page_, left_, y);
  HPDF_Page_LineTo(page_, right_, y);
  HPDF_Page_Stroke(page_);
}

void PdfWriter::drawCell(const std::string& text, float x, float y,
                         float maxWidth, PdfAlign align)
{
  if (text.empty() || maxWidth <= 0.0f)
    return;

  float tw = HPDF_Page_TextWidth(page_, text.c_str());

  float tx = x;
  if (align == PdfAlign::Right) {
    tx = x + maxWidth - tw;
    if (tx < x) tx = x;
  } else if (align == PdfAlign::Center) {
    tx = x + (maxWidth - tw) / 2.0f;
    if (tx < x) tx = x;
  }

  HPDF_Page_TextOut(page_, tx, y, text.c_str());
}

std::vector<float> PdfWriter::computeColumnWidths(const std::vector<PdfRow>& rows,
                                                   int numCols) const
{
  float contentWidth = right_ - left_;
  std::vector<float> widths(numCols, 0.0f);

  // Gather explicit widths from first row that has them.
  for (const auto& row : rows) {
    bool hasExplicit = false;
    for (const auto& cell : row.cells)
      if (cell.width > 0.0f) { hasExplicit = true; break; }
    if (!hasExplicit) continue;

    for (int i = 0; i < numCols && i < (int)row.cells.size(); ++i)
      if (row.cells[i].width > 0.0f)
        widths[i] = row.cells[i].width;
    break;
  }

  // Any column with zero width gets an equal share of the remaining space.
  float used = 0.0f;
  int   free = 0;
  for (int i = 0; i < numCols; ++i) {
    if (widths[i] > 0.0f) used += widths[i];
    else                   ++free;
  }
  if (free > 0) {
    float share = std::max(0.0f, contentWidth - used) / free;
    for (int i = 0; i < numCols; ++i)
      if (widths[i] == 0.0f) widths[i] = share;
  }
  return widths;
}

float PdfWriter::drawRow(const PdfRow& row, float y,
                         const std::vector<float>& colWidths)
{
  const float lineHeight = row.fontSize * 1.4f;

  // Separator row → draw a horizontal rule and return.
  if (row.isSeparator) {
    drawHLine(y);
    return y - lineHeight * 0.5f;
  }

  // Ensure there is enough space on the page; add new page if needed.
  if (y - lineHeight < kMargin) {
    addPage();
    y = curY_;
  }

  // Baseline slightly above bottom of line box.
  float baseline = y - row.fontSize;

  HPDF_Page_BeginText(page_);
  float xPos = left_;
  for (int i = 0; i < (int)row.cells.size() && i < (int)colWidths.size(); ++i) {
    const auto& cell = row.cells[i];
    selectFont(cell.bold || row.isHeader, cell.italic, row.fontSize);
    std::string narrow_text = narrow(cell.text);
    drawCell(narrow_text, xPos, baseline, colWidths[i], cell.align);
    xPos += colWidths[i];
  }
  HPDF_Page_EndText(page_);

  // Draw separator line under header rows.
  if (row.isHeader) {
    drawHLine(baseline - row.fontSize * 0.3f);
    return baseline - row.fontSize * 1.0f;
  }

  return baseline - row.fontSize * 0.4f;
}

// ── Core generation ───────────────────────────────────────────────────────────

static void doGenerate(PdfWriter* pw,
                       HPDF_Doc doc,
                       const std::vector<PdfRow>& rows,
                       const std::wstring& title,
                       const std::wstring& author)
{
  // Metadata.
  HPDF_SetInfoAttr(doc, HPDF_INFO_CREATOR, "MeOS");
  if (!title.empty())
    HPDF_SetInfoAttr(doc, HPDF_INFO_TITLE, narrow(title).c_str());
  if (!author.empty())
    HPDF_SetInfoAttr(doc, HPDF_INFO_AUTHOR, narrow(author).c_str());

  (void)pw; // pw used only for addPage/drawRow which close over members
}

void PdfWriter::generate(const std::vector<PdfRow>& rows,
                         const std::wstring& title,
                         const std::wstring& author,
                         const std::wstring& filename)
{
  init();

  HPDF_SetInfoAttr(doc_, HPDF_INFO_CREATOR, "MeOS");
  if (!title.empty())
    HPDF_SetInfoAttr(doc_, HPDF_INFO_TITLE, narrow(title).c_str());
  if (!author.empty())
    HPDF_SetInfoAttr(doc_, HPDF_INFO_AUTHOR, narrow(author).c_str());

  // Compute column count from rows.
  int numCols = 0;
  for (const auto& row : rows)
    numCols = std::max(numCols, (int)row.cells.size());

  if (numCols == 0) numCols = 1;
  std::vector<float> colWidths = computeColumnWidths(rows, numCols);

  addPage();
  float y = curY_;

  for (const auto& row : rows)
    y = drawRow(row, y, colWidths);

  std::string path = toUTF8(filename);
  if (HPDF_SaveToFile(doc_, path.c_str()) != HPDF_OK)
    throw meosException("PdfWriter: HPDF_SaveToFile failed");

  cleanup();
}

std::vector<unsigned char> PdfWriter::generateToBuffer(
    const std::vector<PdfRow>& rows,
    const std::wstring& title,
    const std::wstring& author)
{
  init();

  HPDF_SetInfoAttr(doc_, HPDF_INFO_CREATOR, "MeOS");
  if (!title.empty())
    HPDF_SetInfoAttr(doc_, HPDF_INFO_TITLE, narrow(title).c_str());
  if (!author.empty())
    HPDF_SetInfoAttr(doc_, HPDF_INFO_AUTHOR, narrow(author).c_str());

  int numCols = 0;
  for (const auto& row : rows)
    numCols = std::max(numCols, (int)row.cells.size());
  if (numCols == 0) numCols = 1;

  std::vector<float> colWidths = computeColumnWidths(rows, numCols);

  addPage();
  float y = curY_;
  for (const auto& row : rows)
    y = drawRow(row, y, colWidths);

  // Serialize to in-memory stream.
  HPDF_SaveToStream(doc_);
  HPDF_UINT32 size = HPDF_GetStreamSize(doc_);

  std::vector<unsigned char> buf(size);
  HPDF_UINT32 remaining = size;
  HPDF_ReadFromStream(doc_, buf.data(), &remaining);

  cleanup();
  return buf;
}

#include "pdf_writer.h"

#include "meos_util.h"

#include <cstdio>
#include <fstream>
#include <stdexcept>

namespace meos::io {

// ---------------------------------------------------------------------------
// HPDF error handler (no __stdcall on Linux)
// ---------------------------------------------------------------------------
static void hpdfErrorHandler(HPDF_STATUS errorNo,
                              HPDF_STATUS detailNo,
                              void* /*userData*/) {
  char buf[128];
  std::snprintf(buf, sizeof(buf),
                "libharu error: error_no=0x%04X detail_no=%u",
                static_cast<unsigned>(errorNo),
                static_cast<unsigned>(detailNo));
  throw std::runtime_error(buf);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {

/// Compute per-column widths. Explicit widths are preserved; remaining page
/// width is divided equally among zero-width columns.
std::vector<float> computeColumnWidths(const std::vector<PdfRow>& rows,
                                       float                      pageWidth) {
  if (rows.empty()) return {};

  size_t ncols = 0;
  for (const auto& r : rows) ncols = std::max(ncols, r.size());

  // Gather explicit widths (max over all rows per column).
  std::vector<float> widths(ncols, 0.0f);
  for (const auto& r : rows) {
    for (size_t c = 0; c < r.size(); ++c) {
      if (r[c].width > 0)
        widths[c] = std::max(widths[c], r[c].width);
    }
  }

  float usable = pageWidth - 2.0f * PdfWriter::kMargin;
  float used   = 0.0f;
  int   autos  = 0;
  for (float w : widths) {
    if (w > 0) used += w;
    else       ++autos;
  }
  float autoW = autos > 0 ? (usable - used) / static_cast<float>(autos) : 0.0f;
  for (auto& w : widths) {
    if (w == 0) w = autoW > 0 ? autoW : 0;
  }
  return widths;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Core renderer
// ---------------------------------------------------------------------------
void PdfWriter::render(HPDF_Doc                   pdf,
                       const std::vector<PdfRow>& rows,
                       float                      pageWidthPt,
                       float                      pageHeightPt) const {
  // Load standard Type 1 fonts (nullptr encoding → built-in Latin-1).
  HPDF_Font fontNormal = HPDF_GetFont(pdf, "Helvetica",      nullptr);
  HPDF_Font fontBold   = HPDF_GetFont(pdf, "Helvetica-Bold", nullptr);
  HPDF_Font fontItalic = HPDF_GetFont(pdf, "Helvetica-Oblique", nullptr);

  std::vector<float> colWidths = computeColumnWidths(rows, pageWidthPt);

  // Create first page.
  HPDF_Page page = HPDF_AddPage(pdf);
  HPDF_Page_SetWidth(page,  pageWidthPt);
  HPDF_Page_SetHeight(page, pageHeightPt);

  float y = pageHeightPt - kMargin;

  for (const auto& row : rows) {
    // Check if we need a new page.
    if (y - kLineHeight < kMargin) {
      page = HPDF_AddPage(pdf);
      HPDF_Page_SetWidth(page,  pageWidthPt);
      HPDF_Page_SetHeight(page, pageHeightPt);
      y = pageHeightPt - kMargin;
    }

    float x = kMargin;
    for (size_t c = 0; c < row.size(); ++c) {
      const PdfCell& cell = row[c];
      float colW = (c < colWidths.size()) ? colWidths[c] : 0.0f;

      HPDF_Font chosenFont = fontNormal;
      if (cell.bold)   chosenFont = fontBold;
      if (cell.italic) chosenFont = fontItalic;

      HPDF_Page_SetFontAndSize(page, chosenFont, kFontSize);

      const std::string& narrow_text = cell.text;  // already narrow
      float textWidth = HPDF_Page_TextWidth(page, narrow_text.c_str());

      float textX = x;
      if (cell.rightAlign && colW > 0) {
        textX = x + colW - textWidth;
      } else if (cell.centerAlign && colW > 0) {
        textX = x + (colW - textWidth) * 0.5f;
      }

      HPDF_Page_BeginText(page);
      HPDF_Page_TextOut(page, textX, y, narrow_text.c_str());
      HPDF_Page_EndText(page);

      x += colW;
    }

    y -= kLineHeight;
  }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void PdfWriter::writeFile(const std::wstring&        path,
                          const std::vector<PdfRow>& rows,
                          float                      pageWidthPt,
                          float                      pageHeightPt) const {
  HPDF_Doc pdf = HPDF_New(hpdfErrorHandler, nullptr);
  if (!pdf) throw std::runtime_error("HPDF_New failed");

  render(pdf, rows, pageWidthPt, pageHeightPt);

  const std::string utf8path = meos::util::toUTF8(path);
  HPDF_STATUS st = HPDF_SaveToFile(pdf, utf8path.c_str());
  HPDF_Free(pdf);

  if (st != HPDF_OK) throw std::runtime_error("HPDF_SaveToFile failed");
}

std::vector<char> PdfWriter::writeToMemory(const std::vector<PdfRow>& rows,
                                           float                      pageWidthPt,
                                           float                      pageHeightPt) const {
  HPDF_Doc pdf = HPDF_New(hpdfErrorHandler, nullptr);
  if (!pdf) throw std::runtime_error("HPDF_New failed");

  render(pdf, rows, pageWidthPt, pageHeightPt);

  HPDF_STATUS st = HPDF_SaveToStream(pdf);
  if (st != HPDF_OK) {
    HPDF_Free(pdf);
    throw std::runtime_error("HPDF_SaveToStream failed");
  }

  HPDF_UINT32 size = HPDF_GetStreamSize(pdf);
  std::vector<char> buf(size);

  HPDF_ResetStream(pdf);
  HPDF_UINT32 readSize = size;
  HPDF_ReadFromStream(pdf, reinterpret_cast<HPDF_BYTE*>(buf.data()), &readSize);

  HPDF_Free(pdf);
  return buf;
}

} // namespace meos::io

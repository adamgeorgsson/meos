#pragma once
// PdfWriter.h — Cross-platform PDF result generation using libharu.
// Generates A4 portrait PDF files from structured row/cell data.
// No dependency on Win32 or gdioutput.

#include <string>
#include <vector>

// Include hpdf.h directly — do NOT forward declare HPDF_Doc/HPDF_Page as void*.
#include <hpdf.h>

// ── Data types ────────────────────────────────────────────────────────────────

// Text alignment for a cell.
enum class PdfAlign { Left = 0, Center, Right };

// A single cell in a PDF result row.
struct PdfCell {
  std::wstring text;
  PdfAlign align = PdfAlign::Left;
  bool bold   = false;
  bool italic = false;
  // Explicit column width in points (0 = distribute evenly).
  float width = 0.0f;
};

// One row in the PDF result table.
struct PdfRow {
  std::vector<PdfCell> cells;
  bool isHeader    = false;  // rendered in bold with a separator line below
  bool isSeparator = false;  // full-width horizontal rule
  float fontSize   = 10.0f;
};

// ── PdfWriter ─────────────────────────────────────────────────────────────────
// Generates A4 portrait PDFs from structured table data.
//
// Standard PDF Type 1 fonts are used (no external font files required):
//   normal  → Helvetica
//   bold    → Helvetica-Bold
//   italic  → Helvetica-Oblique
//
// Wide-character text is converted with narrow() before rendering — standard
// PDF fonts cover Latin-1 characters only.
class PdfWriter {
public:
  // A4 portrait geometry (in points, 1pt ≈ 0.353 mm).
  static constexpr float kPageWidth  = 595.0f;
  static constexpr float kPageHeight = 842.0f;
  static constexpr float kMargin     = 40.0f;

  PdfWriter();
  ~PdfWriter();

  // Generate a PDF and write it to a file.
  void generate(const std::vector<PdfRow>& rows,
                const std::wstring& title,
                const std::wstring& author,
                const std::wstring& filename);

  // Generate a PDF and return its bytes (useful for testing without filesystem).
  std::vector<unsigned char> generateToBuffer(const std::vector<PdfRow>& rows,
                                              const std::wstring& title,
                                              const std::wstring& author);

private:
  void init();
  void cleanup();
  void loadFonts();
  void addPage();

  // Compute column widths from row data. Returns total usable width.
  std::vector<float> computeColumnWidths(const std::vector<PdfRow>& rows,
                                         int numCols) const;

  // Draw a single row; returns the Y baseline after the row (may add a new page).
  float drawRow(const PdfRow& row, float y,
                const std::vector<float>& colWidths);

  // Draw a horizontal line across the content area.
  void drawHLine(float y);

  // Select font and size for the current page.
  void selectFont(bool bold, bool italic, float size);

  // Draw narrow (latin-1) text at (x, y) clipped to maxWidth.
  void drawCell(const std::string& text, float x, float y,
                float maxWidth, PdfAlign align);

  HPDF_Doc  doc_  = nullptr;
  HPDF_Page page_ = nullptr;

  HPDF_Font fontNormal_ = nullptr;
  HPDF_Font fontBold_   = nullptr;
  HPDF_Font fontItalic_ = nullptr;

  // Current font/size (to avoid redundant HPDF_Page_SetFontAndSize calls).
  HPDF_Font curFont_ = nullptr;
  float     curSize_ = 0.0f;

  // Content area X extents.
  float left_  = kMargin;
  float right_ = kPageWidth - kMargin;

  // Current Y cursor (decreases down the page in PDF coordinates).
  float curY_  = kPageHeight - kMargin;
};

#pragma once

#include <hpdf.h>
#include <string>
#include <vector>

namespace meos::io {

/// A single cell in a PDF table row.
struct PdfCell {
  std::string text;       ///< Text content (narrow / Latin-1)
  float       width = 0;  ///< Explicit column width in points; 0 = auto-distribute
  bool        bold   = false;
  bool        italic = false;
  bool        rightAlign  = false;
  bool        centerAlign = false;
};

using PdfRow = std::vector<PdfCell>;

/// Tabular PDF writer backed by libharu.
///
/// Fonts: Helvetica (normal), Helvetica-Bold, Helvetica-Oblique.
/// Encoding: nullptr (standard Type 1, Latin-1 range).
/// Text is passed through meos::util::narrow() before rendering.
///
/// Column widths: explicit cell.width values are respected; zero-width
/// columns share the remaining page width equally.
///
/// Multi-page: a new page is added automatically when the cursor would
/// fall below kMargin from the bottom of the page.
class PdfWriter {
public:
  static constexpr float kMargin     = 40.0f;  ///< Page margin in points
  static constexpr float kFontSize   = 10.0f;
  static constexpr float kLineHeight = 14.0f;

  PdfWriter()  = default;
  ~PdfWriter() = default;

  /// Write tabular rows to a file (UTF-8 path via meos::util::toUTF8).
  void writeFile(const std::wstring&        path,
                 const std::vector<PdfRow>& rows,
                 float                      pageWidthPt  = 595.0f,
                 float                      pageHeightPt = 842.0f) const;

  /// Render rows into an in-memory buffer (begins with "%PDF-").
  std::vector<char> writeToMemory(const std::vector<PdfRow>& rows,
                                  float                      pageWidthPt  = 595.0f,
                                  float                      pageHeightPt = 842.0f) const;

private:
  /// Core render: shared by writeFile and writeToMemory.
  void render(HPDF_Doc                   pdf,
              const std::vector<PdfRow>& rows,
              float                      pageWidthPt,
              float                      pageHeightPt) const;
};

} // namespace meos::io

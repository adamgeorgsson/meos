#pragma once
// HtmlWriter.h — Cross-platform HTML template reader and result generator.
// Implements the MeOS .template format with @SECTION markers and @PLACEHOLDER
// substitution. No dependency on Win32 or gdioutput.

#include <string>
#include <vector>
#include <iostream>

// ── Data types ────────────────────────────────────────────────────────────────

// A single cell in an HTML result row.
struct HtmlCell {
  std::string text;       // UTF-8 encoded content
  std::string cssClass;   // optional CSS class
  bool isHeader = false;  // render as <th> instead of <td>
};

// One row in the HTML result table.
struct HtmlRow {
  std::vector<HtmlCell> cells;
  std::string cssClass;      // optional row-level CSS class
  bool isSeparator = false;  // renders as a full-width separator row
};

// ── Template sections ─────────────────────────────────────────────────────────
// Parsed from a .template file using @SECTION markers.
//
// Marker lines recognised during parsing:
//   @MEOS EXPORT TEMPLATE   — required first line
//   @HEAD / @END            — static HTML head and tail (written once)
//   @OUTERPAGE / @END       — outer page container (repeated per page)
//   @INNERPAGE              — inner column container inside @OUTERPAGE
//   @SEPARATOR              — column separator inside @OUTERPAGE
//   @USETABLE               — request table-based layout
//   Lines starting with %   — ignored (comments)
//   tag@Description         — tag and human-readable name (second content line)
struct HtmlTemplate {
  std::string head;
  std::string outerPage;
  std::string innerPage;
  std::string separator;
  std::string end;
  bool useTables = false;
  std::string tag;
  std::wstring name;

  bool empty() const { return head.empty() && outerPage.empty(); }

  // Parse a template from an open stream (for testability — no filesystem I/O).
  static HtmlTemplate read(std::istream& in);

  // Convenience: read from a file path.
  static HtmlTemplate readFile(const std::wstring& filename);
};

// ── HtmlWriter ────────────────────────────────────────────────────────────────
class HtmlWriter {
public:
  // Replace all recognised @PLACEHOLDER tokens in str in-place.
  //
  // Tokens (long form takes precedence over short alias):
  //   @TITLE / @T          — HTML-encoded title
  //   @CONTENTS / @C       — raw HTML contents (inserted literally)
  //   @DESCRIPTION / @D    — HTML-encoded description
  //   @NUMPAGE / @N        — total page count
  //   @PAGE                — current page number
  //   @TIME                — refresh interval in milliseconds
  //   @STYLE / @S          — CSS style block (empty by default)
  //   @MEOSVERSION / @MEOS / @M — literal "MeOS"
  //
  // Replacement is done in a single left-to-right pass; the longest matching
  // token wins at each position, so @TITLE is replaced before @T etc.
  static void replacePlaceholders(std::string& str,
    const std::wstring& title,
    const std::string& contents,
    const std::wstring& description = L"",
    int refreshMs = 0,
    int currentPage = 1,
    int numPages = 1,
    const std::string& style = "");

  // Build a self-contained HTML page (no template) from a table of rows.
  // Uses a simple <table> layout with basic inline CSS.
  static std::string buildTableHtml(const std::wstring& title,
                                    const std::vector<HtmlRow>& rows,
                                    int refreshMs = 0);

  // Write HTML using a parsed template.
  // When numColumns > 1 the @INNERPAGE block is repeated; @CONTENTS in each
  // column gets the same contents (split-column logic is left to the caller).
  static void generate(std::ostream& out,
                       const HtmlTemplate& tmpl,
                       const std::wstring& title,
                       const std::string& contents,
                       const std::wstring& description = L"",
                       int refreshMs = 0,
                       int numColumns = 1);

  // Write HTML to a file using a template (or buildTableHtml if tmpl is null/empty).
  static void writeFile(const std::wstring& filename,
                        const std::wstring& title,
                        const std::vector<HtmlRow>& rows,
                        int refreshMs = 0,
                        const HtmlTemplate* tmpl = nullptr);

  // HTML-encode a UTF-8 string: & < > " ' → named/numeric entities.
  static std::string encodeHtml(const std::string& utf8text);
  static std::string encodeHtml(const std::wstring& text);

  // Convert a vector of HtmlRows into an HTML <table> fragment.
  static std::string rowsToTableHtml(const std::vector<HtmlRow>& rows);
};

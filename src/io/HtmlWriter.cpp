// HtmlWriter.cpp — Cross-platform HTML template reader and result generator.
#include "HtmlWriter.h"
#include "meos_util.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cassert>

// ── Internal helpers ──────────────────────────────────────────────────────────

static std::string trimRight(std::string s) {
  while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t'))
    s.pop_back();
  return s;
}

static std::string trimBoth(std::string s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return {};
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

// Single-pass replacement table entry.
struct ReplEntry { std::string marker; std::string value; };

// Replace all @PLACEHOLDER tokens in str using a single left-to-right pass.
// At each '@' the longest matching marker wins.
static void applyReplacements(std::string& str, const std::vector<ReplEntry>& table) {
  if (str.empty() || table.empty()) return;
  std::string result;
  result.reserve(str.size());
  for (size_t i = 0; i < str.size(); ) {
    if (str[i] != '@') { result += str[i++]; continue; }
    // Find longest matching marker starting at i
    int bestLen = -1;
    const std::string* bestVal = nullptr;
    for (const auto& e : table) {
      if ((int)e.marker.size() > bestLen &&
          str.compare(i, e.marker.size(), e.marker) == 0) {
        bestLen = (int)e.marker.size();
        bestVal = &e.value;
      }
    }
    if (bestLen > 0) { result += *bestVal; i += bestLen; }
    else             { result += str[i++]; }
  }
  str = std::move(result);
}

// ── HtmlWriter::encodeHtml ────────────────────────────────────────────────────

std::string HtmlWriter::encodeHtml(const std::string& utf8text) {
  std::string out;
  out.reserve(utf8text.size());
  for (unsigned char c : utf8text) {
    switch (c) {
      case '&':  out += "&amp;";  break;
      case '<':  out += "&lt;";   break;
      case '>':  out += "&gt;";   break;
      case '"':  out += "&quot;"; break;
      case '\'': out += "&#39;";  break;
      default:   out += (char)c;
    }
  }
  return out;
}

std::string HtmlWriter::encodeHtml(const std::wstring& text) {
  return encodeHtml(toUTF8(text));
}

// ── HtmlWriter::replacePlaceholders ──────────────────────────────────────────

void HtmlWriter::replacePlaceholders(std::string& str,
    const std::wstring& title,
    const std::string& contents,
    const std::wstring& description,
    int refreshMs,
    int currentPage,
    int numPages,
    const std::string& style) {

  std::string encTitle = encodeHtml(title);
  std::string encDesc  = encodeHtml(description);
  std::string nPages   = std::to_string(numPages);
  std::string page     = std::to_string(currentPage);
  std::string tms      = std::to_string(refreshMs);

  // Order: longer markers must appear before their shorter aliases.
  std::vector<ReplEntry> table = {
    {"@MEOSVERSION", "MeOS"},
    {"@DESCRIPTION", encDesc},
    {"@CONTENTS",    contents},
    {"@NUMPAGE",     nPages},
    {"@TITLE",       encTitle},
    {"@STYLE",       style},
    {"@TIME",        tms},
    {"@PAGE",        page},
    {"@MEOS",        "MeOS"},
    {"@N",           nPages},
    {"@C",           contents},
    {"@D",           encDesc},
    {"@T",           encTitle},
    {"@S",           style},
    {"@M",           "MeOS"},
  };
  applyReplacements(str, table);
}

// ── HtmlTemplate::read ────────────────────────────────────────────────────────

HtmlTemplate HtmlTemplate::read(std::istream& in) {
  HtmlTemplate tmpl;
  std::string line;

  // Verify first line
  if (!std::getline(in, line)) return tmpl;
  if (trimBoth(line) != "@MEOS EXPORT TEMPLATE") return tmpl;

  // Second non-comment line: tag@Name
  std::string tagLine;
  while (std::getline(in, line)) {
    std::string tl = trimBoth(line);
    if (!tl.empty() && tl[0] != '%') { tagLine = tl; break; }
  }
  auto atPos = tagLine.find('@');
  if (atPos != std::string::npos) {
    tmpl.tag  = tagLine.substr(0, atPos);
    std::string nameUtf8 = tagLine.substr(atPos + 1);
    tmpl.name = fromUTF8(nameUtf8);
  } else {
    tmpl.tag  = tagLine;
  }

  // Parse sections
  enum class Section { None, Head, OuterPage, InnerPage, Separator, End };
  Section cur = Section::None;
  std::string* acc = nullptr;

  auto setSection = [&](Section s, std::string* target) {
    cur = s; acc = target;
  };

  while (std::getline(in, line)) {
    std::string tl = trimBoth(line);

    if (tl.empty() || tl[0] == '%') {
      if (acc) *acc += "\n";
      continue;
    }
    if (tl == "@HEAD") { setSection(Section::Head, &tmpl.head); continue; }
    if (tl == "@OUTERPAGE") { setSection(Section::OuterPage, &tmpl.outerPage); continue; }
    if (tl == "@INNERPAGE") { setSection(Section::InnerPage, &tmpl.innerPage); continue; }
    if (tl == "@SEPARATOR") { setSection(Section::Separator, &tmpl.separator); continue; }
    if (tl == "@END") {
      // @END closes the current top-level section
      if (cur == Section::InnerPage || cur == Section::Separator) {
        // Return to outer page accumulation
        setSection(Section::OuterPage, &tmpl.outerPage);
      } else {
        setSection(Section::End, &tmpl.end);
      }
      continue;
    }
    if (tl == "@USETABLE") { tmpl.useTables = true; continue; }

    if (acc) *acc += trimRight(line) + "\n";
  }
  return tmpl;
}

HtmlTemplate HtmlTemplate::readFile(const std::wstring& filename) {
  std::ifstream f(toUTF8(filename));
  if (!f) return {};
  return read(f);
}

// ── HtmlWriter::rowsToTableHtml ───────────────────────────────────────────────

std::string HtmlWriter::rowsToTableHtml(const std::vector<HtmlRow>& rows) {
  std::ostringstream out;
  out << "<table>\n";
  for (const auto& row : rows) {
    if (row.isSeparator) {
      size_t cols = row.cells.empty() ? 1 : row.cells.size();
      out << "<tr><td colspan=\"" << cols << "\" class=\"sep\"></td></tr>\n";
      continue;
    }
    out << "<tr";
    if (!row.cssClass.empty()) out << " class=\"" << row.cssClass << "\"";
    out << ">";
    for (const auto& cell : row.cells) {
      const char* tag = cell.isHeader ? "th" : "td";
      out << "<" << tag;
      if (!cell.cssClass.empty()) out << " class=\"" << cell.cssClass << "\"";
      out << ">" << encodeHtml(cell.text) << "</" << tag << ">";
    }
    out << "</tr>\n";
  }
  out << "</table>\n";
  return out.str();
}

// ── HtmlWriter::buildTableHtml ────────────────────────────────────────────────

static const char* kDefaultCss =
  "<style type=\"text/css\">\n"
  "body{font-family:arial,sans-serif;background-color:rgb(250,250,255)}\n"
  "h1{font-size:24px;font-weight:normal}\n"
  "table{border-collapse:collapse;width:100%}\n"
  "th{background-color:#dde;padding:4px 8px;text-align:left}\n"
  "td{padding:3px 8px}\n"
  "tr:nth-child(even) td{background-color:#eeeeff}\n"
  "td.sep{height:8px;background:transparent}\n"
  "</style>\n";

std::string HtmlWriter::buildTableHtml(const std::wstring& title,
                                       const std::vector<HtmlRow>& rows,
                                       int refreshMs) {
  std::ostringstream out;
  out << "<!DOCTYPE html>\n<html>\n<head>\n";
  out << "<meta charset=\"utf-8\">\n";
  out << "<title>" << encodeHtml(title) << "</title>\n";
  if (refreshMs > 0)
    out << "<meta http-equiv=\"refresh\" content=\"" << (refreshMs / 1000) << "\">\n";
  out << kDefaultCss;
  out << "</head>\n<body>\n";
  out << "<h1>" << encodeHtml(title) << "</h1>\n";
  out << rowsToTableHtml(rows);
  out << "</body>\n</html>\n";
  return out.str();
}

// ── HtmlWriter::generate ─────────────────────────────────────────────────────

void HtmlWriter::generate(std::ostream& out,
                          const HtmlTemplate& tmpl,
                          const std::wstring& title,
                          const std::string& contents,
                          const std::wstring& description,
                          int refreshMs,
                          int numColumns) {

  // HEAD
  std::string head = tmpl.head;
  replacePlaceholders(head, title, contents, description, refreshMs, 1, 1);
  out << head;

  // OUTER PAGE (with INNER PAGE blocks for each column)
  // Build inner column content: repeat innerPage numColumns times with separators
  std::string columnBlock;
  for (int col = 0; col < numColumns; ++col) {
    if (col > 0 && !tmpl.separator.empty())
      columnBlock += tmpl.separator;
    columnBlock += tmpl.innerPage;
  }
  std::string outerPage = tmpl.outerPage + columnBlock;
  replacePlaceholders(outerPage, title, contents, description, refreshMs, 1, 1);
  out << outerPage;

  // END
  std::string end = tmpl.end;
  replacePlaceholders(end, title, contents, description, refreshMs, 1, 1);
  out << end;
}

// ── HtmlWriter::writeFile ─────────────────────────────────────────────────────

void HtmlWriter::writeFile(const std::wstring& filename,
                           const std::wstring& title,
                           const std::vector<HtmlRow>& rows,
                           int refreshMs,
                           const HtmlTemplate* tmpl) {
  std::ofstream f(toUTF8(filename));
  if (!f) return;

  if (tmpl && !tmpl->empty()) {
    std::string contents = rowsToTableHtml(rows);
    generate(f, *tmpl, title, contents, L"", refreshMs);
  } else {
    f << buildTableHtml(title, rows, refreshMs);
  }
}

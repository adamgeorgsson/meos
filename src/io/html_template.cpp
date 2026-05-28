#include "html_template.h"

#include "meos_util.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace meos::io {

namespace {

bool startsWith(const std::string& s, const std::string& prefix) {
  return s.size() >= prefix.size() &&
         s.compare(0, prefix.size(), prefix) == 0;
}

std::string trimRight(std::string s) {
  while (!s.empty() &&
         (s.back() == '\r' || s.back() == '\n' ||
          s.back() == ' '  || s.back() == '\t')) {
    s.pop_back();
  }
  return s;
}

} // anonymous namespace

void HtmlTemplate::read(std::istream& in) {
  std::string line;

  // First line must be "@MEOS EXPORT TEMPLATE"
  if (!std::getline(in, line) || trimRight(line) != "@MEOS EXPORT TEMPLATE") {
    throw std::runtime_error(
        "Invalid template: missing '@MEOS EXPORT TEMPLATE' header");
  }

  // Second line: tag@ShortName
  if (!std::getline(in, line)) {
    throw std::runtime_error("Invalid template: missing tag line");
  }
  line = trimRight(line);
  auto atPos = line.find('@');
  if (atPos != std::string::npos) {
    info.tag = line.substr(0, atPos);
    info.shortName = line.substr(atPos + 1);
  } else {
    info.tag = line;
  }

  // Parse remaining lines into description + sections
  enum class Section { NONE, HEAD, OUTERPAGE, INNERPAGE, SEPARATOR, END };
  Section current = Section::NONE;
  std::string* currentBuf = nullptr;

  auto setSection = [&](Section s) {
    current = s;
    switch (s) {
      case Section::HEAD:      currentBuf = &head;      break;
      case Section::OUTERPAGE: currentBuf = &outerPage; break;
      case Section::INNERPAGE: currentBuf = &innerPage; break;
      case Section::SEPARATOR: currentBuf = &separator; break;
      case Section::END:       currentBuf = &end;       break;
      default:                 currentBuf = nullptr;    break;
    }
  };

  while (std::getline(in, line)) {
    std::string trimmed = trimRight(line);

    // Section directive lines
    if (trimmed == "@HEAD")      { setSection(Section::HEAD);      continue; }
    if (trimmed == "@OUTERPAGE") { setSection(Section::OUTERPAGE); continue; }
    if (trimmed == "@INNERPAGE") { setSection(Section::INNERPAGE); continue; }
    if (trimmed == "@SEPARATOR") { setSection(Section::SEPARATOR); continue; }
    if (trimmed == "@END")       { setSection(Section::END);       continue; }
    if (trimmed == "@USETABLE")  { useTables = true;               continue; }

    // Comment lines (% prefix) are always skipped
    if (!trimmed.empty() && trimmed[0] == '%') continue;

    if (current == Section::NONE) {
      // Pre-section area: collect description text
      if (!trimmed.empty()) {
        if (!info.description.empty()) info.description += '\n';
        info.description += trimmed;
      }
      continue;
    }

    if (currentBuf) {
      *currentBuf += trimmed;
      *currentBuf += '\n';
    }
  }
}

void HtmlTemplate::readFile(const std::wstring& fileName) {
  std::ifstream in(meos::util::toUTF8(fileName));
  if (!in.is_open()) {
    throw std::runtime_error("Cannot open template file: " +
                             meos::util::toUTF8(fileName));
  }
  read(in);
}

std::string HtmlTemplate::substitute(const std::string& tmpl,
                                      const std::map<std::string, std::string>& vars) {
  if (vars.empty() || tmpl.empty()) return tmpl;

  // Sort keys longest-first so e.g. @NUMPAGE is tried before @NUM
  std::vector<std::pair<std::string, std::string>> sorted(vars.begin(), vars.end());
  std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) {
              return a.first.size() > b.first.size();
            });

  std::string result;
  result.reserve(tmpl.size());

  for (size_t i = 0; i < tmpl.size(); ) {
    if (tmpl[i] == '@') {
      bool matched = false;
      for (const auto& [key, val] : sorted) {
        if (tmpl.compare(i, key.size(), key) == 0) {
          result += val;
          i += key.size();
          matched = true;
          break;
        }
      }
      if (!matched) {
        result += tmpl[i++];
      }
    } else {
      result += tmpl[i++];
    }
  }

  return result;
}

void HtmlTemplate::generate(std::ostream& out,
                             const std::string& title,
                             const std::string& description,
                             const std::vector<std::vector<std::string>>& pages,
                             int refreshMs) const {
  int numPages = static_cast<int>(pages.size());
  int numCols  = pages.empty() ? 0 : static_cast<int>(pages[0].size());

  std::map<std::string, std::string> baseVars = {
    {"@TITLE",           title},
    {"@DESCRIPTION",     description},
    {"@MEOS",            "MeOS"},
    {"@TIME",            std::to_string(refreshMs)},
    {"@NUMPAGE",         std::to_string(numPages)},
    {"@NUMCOL",          std::to_string(numCols)},
    {"@STYLE",           ""},
    {"@PERCENTCOMPLETE", "100"},
    {"@CONTENTS",        ""},
    {"@PAGE",            ""},
  };

  // Build full body: outer pages → inner columns
  std::string bodyContent;
  for (int pi = 0; pi < numPages; ++pi) {
    const auto& cols = pages[pi];
    int nc = static_cast<int>(cols.size());

    std::string innerContent;
    for (int ci = 0; ci < nc; ++ci) {
      if (ci > 0 && !separator.empty()) {
        auto vars = baseVars;
        vars["@PAGE"] = std::to_string(ci + 1);
        innerContent += substitute(separator, vars);
      }
      if (!innerPage.empty()) {
        auto vars = baseVars;
        vars["@CONTENTS"] = cols[ci];
        vars["@PAGE"]     = std::to_string(ci + 1);
        innerContent += substitute(innerPage, vars);
      } else {
        innerContent += cols[ci];
      }
    }

    if (!outerPage.empty()) {
      auto vars = baseVars;
      vars["@CONTENTS"] = innerContent;
      vars["@PAGE"]     = std::to_string(pi + 1);
      bodyContent += substitute(outerPage, vars);
    } else {
      bodyContent += innerContent;
    }
  }

  // Emit HEAD with body embedded at @CONTENTS
  if (!head.empty()) {
    auto vars = baseVars;
    vars["@CONTENTS"] = bodyContent;
    out << substitute(head, vars);
  } else {
    out << bodyContent;
  }

  // Emit END section
  if (!end.empty()) {
    out << substitute(end, baseVars);
  }
}

void HtmlTemplate::generateFile(const std::wstring& fileName,
                                 const std::string& title,
                                 const std::string& description,
                                 const std::vector<std::vector<std::string>>& pages,
                                 int refreshMs) const {
  std::ofstream out(meos::util::toUTF8(fileName));
  if (!out.is_open()) {
    throw std::runtime_error("Cannot open output file for writing: " +
                             meos::util::toUTF8(fileName));
  }
  generate(out, title, description, pages, refreshMs);
}

} // namespace meos::io

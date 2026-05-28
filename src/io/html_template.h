#pragma once

#include <map>
#include <ostream>
#include <string>
#include <vector>

namespace meos::io {

/// Clean HTML template engine for MeOS export templates.
/// Parses .template files with @MEOS EXPORT TEMPLATE format and generates HTML output.
class HtmlTemplate {
public:
  struct Info {
    std::string tag;
    std::string shortName;
    std::string description;
  };

  Info info;
  bool useTables = false;

  // Parsed template sections
  std::string head;
  std::string outerPage;
  std::string innerPage;
  std::string separator;
  std::string end;

  /// Read template from a stream (testable without filesystem I/O).
  /// Throws std::runtime_error on invalid format.
  void read(std::istream& in);

  /// Read template from file; uses toUTF8(fileName) for Linux-compatible path.
  void readFile(const std::wstring& fileName);

  /// Replace all @PLACEHOLDER occurrences in tmpl with values from vars.
  /// Single-pass, longest-match-first so @NUMPAGE is preferred over @NUM.
  static std::string substitute(const std::string& tmpl,
                                const std::map<std::string, std::string>& vars);

  /// Generate HTML to out.
  /// pages: outer pages, each containing column content strings.
  void generate(std::ostream& out,
                const std::string& title,
                const std::string& description,
                const std::vector<std::vector<std::string>>& pages,
                int refreshMs = 0) const;

  /// Convenience: write HTML directly to file.
  void generateFile(const std::wstring& fileName,
                    const std::string& title,
                    const std::string& description,
                    const std::vector<std::vector<std::string>>& pages,
                    int refreshMs = 0) const;
};

} // namespace meos::io

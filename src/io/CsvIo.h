// CsvIo.h — CSV import/export for MeOS (platform-independent, US-014b).
#pragma once

#include <list>
#include <string>
#include <vector>

class oEvent;

/// CSV format detected from first data line.
enum class CsvFormat { NoCSV, Unknown, OE, OS, RAID };

/// CSV import/export utilities for MeOS OE/OS formats.
class CsvIo {
public:
  /// Detect the CSV format of a file by reading the first non-empty line.
  static CsvFormat detectFormat(const std::wstring& file);

  /// Split a single semicolon-delimited line into fields.
  static std::vector<std::wstring> splitLine(const std::wstring& line);

  /// Parse an entire CSV file into a list of rows (wstring fields).
  /// Handles UTF-8 BOM stripping.
  static void parse(const std::wstring& file,
                    std::list<std::vector<std::wstring>>& data);

  /// Import runners from an OE-format CSV file into oEvent.
  /// Rows with more than 10 columns are treated as data rows (header skipped).
  /// Returns number of runners imported, or -1 on error.
  static int importOE(oEvent& oe, const std::wstring& file);

  /// Export runners from oEvent to an OE-format CSV file.
  /// Returns true on success.
  static bool exportOE(oEvent& oe, const std::wstring& file);

  // ── OE status helpers ─────────────────────────────────────────────────────

  /// Convert RunnerStatus to OE numeric status (0=OK,1=DNS,2=DNF,3=MP,4=DQ,5=MAX).
  static int runnerToOEStatus(int status);

  /// Convert OE numeric status to RunnerStatus.
  static int oeStatusToRunner(int oeStatus);
};

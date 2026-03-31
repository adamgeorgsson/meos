// IofXml.h — IOF 3.0 XML import/export for MeOS (platform-independent).
#pragma once

#include <set>
#include <string>
#include <vector>

class oEvent;
class xmlobject;
class xmlparser;

// ── Status conversion helpers ─────────────────────────────────────────────────

/// Convert RunnerStatus to IOF 3.0 status string (e.g. "OK", "MissingPunch").
std::string iofStatusFromRunner(int status);

/// Convert IOF 3.0 status string to RunnerStatus int.
int iofStatusToRunner(const std::string& s);

// ── IofXmlInterface ───────────────────────────────────────────────────────────

class IofXmlInterface {
  oEvent& oe;

  // Helpers
  std::wstring absTimeISO(int seconds) const;
  int parseAbsTime(const std::string& datetime) const;
  static std::string stripTimezone(const std::string& iso);

public:
  explicit IofXmlInterface(oEvent& oe);

  // ── Import ────────────────────────────────────────────────────────────────

  /// Read CourseData root element (controls + courses + class assignments).
  void readCourseData(const xmlobject& xo, int& courseCount, int& failed);

  /// Read ClassList root element.
  void readClassList(const xmlobject& xo, int& read, int& failed);

  /// Read OrganisationList root element.
  void readOrganisationList(const xmlobject& xo, int& count);

  /// Read EntryList root element (PersonEntry elements).
  void readEntryList(const xmlobject& xo, int& read, int& failed);

  // ── Export ────────────────────────────────────────────────────────────────

  /// Write IOF 3.0 ResultList to xml.
  /// @param classFilter  Empty = all classes. Non-empty = only listed class IDs.
  void writeResultList(xmlparser& xml, const std::set<int>& classFilter = {});

  /// Write IOF 3.0 StartList to xml.
  /// @param classFilter  Empty = all classes.
  void writeStartList(xmlparser& xml, const std::set<int>& classFilter = {});
};

#pragma once
// IOF 3.0 XML import/export for MeOS.
// Depends on: meos_domain, meos_util (xmlparser)

#include <set>
#include <string>
#include "runner_status.h"

class oEvent;
class xmlparser;

namespace meos::io {

/**
 * IOF 3.0 XML import/export.
 *
 * Import populates an oEvent from IOF 3.0 XML strings (in memory).
 * Export writes IOF 3.0 XML to an xmlparser opened for output.
 *
 * Time units: internal MeOS times are in tenths-of-a-second
 * (timeConstSecond=10). IOF 3.0 running times are integer seconds.
 * IOF 3.0 absolute times use "HH:MM:SS" format (timezone stripped on import).
 */
class IofXml {
public:
    explicit IofXml(oEvent& oe);

    // -----------------------------------------------------------------------
    // Import — returns number of entities successfully imported
    // -----------------------------------------------------------------------

    /** Read <ClassList> XML: creates/updates oClass entities. */
    int readClasses(const std::string& xmlData);

    /** Read <OrganisationList> XML: creates/updates oClub entities. */
    int readOrganisations(const std::string& xmlData);

    /** Read <CourseData> XML: creates controls and oCourse entities. */
    int readCourseData(const std::string& xmlData);

    /**
     * Read <EntryList> or <StartList> or <ResultList> XML:
     * creates/updates oRunner entities.
     *
     * - EntryList: only name/club/class
     * - StartList: name/club/class + start times
     * - ResultList: name/club/class + start/finish times + status
     */
    int readEntryList(const std::string& xmlData);

    // -----------------------------------------------------------------------
    // Export
    // -----------------------------------------------------------------------

    /**
     * Write <ResultList iofVersion="3.0"> to xml.
     * Call xml.openMemoryOutput() or xml.openOutputT() before this.
     * @param classIds  If non-empty, only include these class IDs.
     */
    void writeResultList(xmlparser& xml,
                         const std::set<int>& classIds = {});

    /**
     * Write <StartList iofVersion="3.0"> to xml.
     * @param classIds  If non-empty, only include these class IDs.
     */
    void writeStartList(xmlparser& xml,
                        const std::set<int>& classIds = {});

    // -----------------------------------------------------------------------
    // Utilities (public for unit testing)
    // -----------------------------------------------------------------------

    /** Map IOF 3.0 status string to RunnerStatus. */
    static RunnerStatus iofStatusToRunner(const std::string& iofStatus);

    /** Map RunnerStatus to IOF 3.0 status string. */
    static std::string runnerStatusToIof(RunnerStatus status);

    /**
     * Parse IOF 3.0 duration string → tenths-of-a-second.
     * Accepts:
     *   - ISO 8601 duration: "PT23M45S", "PT1H23M45S", "PT45S"
     *   - Plain integer seconds: "1425"
     * Returns 0 for empty/unparseable.
     */
    static int parseIofDuration(const std::string& s);

    /**
     * Parse IOF 3.0 time-of-day → tenths-of-a-second from midnight.
     * Strips ISO 8601 timezone and date:
     *   - "2023-08-01T10:30:00+02:00" → 10*3600*10 + 30*60*10
     *   - "10:30:00" → same
     * Returns -1 on failure.
     */
    static int parseIofTimeOfDay(const std::string& s);

    /** Format tenths-of-a-second running time as integer seconds string. */
    static std::string formatIofDuration(int tenths);

    /** Format tenths-of-a-second time-of-day as "HH:MM:SS". */
    static std::string formatIofTimeOfDay(int tenths);

private:
    oEvent& oe_;

    // Helper: write person name block
    static void writePersonName(xmlparser& xml, const std::wstring& fullName);
};

} // namespace meos::io

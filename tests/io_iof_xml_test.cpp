// Unit tests for IOF 3.0 XML import/export (US-014a)
#include <gtest/gtest.h>

#include <string>
#include <set>
#include <sstream>

#include "iof_xml.h"
#include "oEvent.h"
#include "xmlparser.h"

using namespace meos::io;

// ============================================================================
// Status mapping tests
// ============================================================================

TEST(IofXmlStatus, RunnerToIof) {
    EXPECT_EQ(IofXml::runnerStatusToIof(StatusOK),               "OK");
    EXPECT_EQ(IofXml::runnerStatusToIof(StatusDNS),              "DidNotStart");
    EXPECT_EQ(IofXml::runnerStatusToIof(StatusCANCEL),           "Cancelled");
    EXPECT_EQ(IofXml::runnerStatusToIof(StatusDNF),              "DidNotFinish");
    EXPECT_EQ(IofXml::runnerStatusToIof(StatusMP),               "MissingPunch");
    EXPECT_EQ(IofXml::runnerStatusToIof(StatusDQ),               "Disqualified");
    EXPECT_EQ(IofXml::runnerStatusToIof(StatusMAX),              "OverTime");
    EXPECT_EQ(IofXml::runnerStatusToIof(StatusOutOfCompetition), "NotCompeting");
    EXPECT_EQ(IofXml::runnerStatusToIof(StatusUnknown),          "Active");
}

TEST(IofXmlStatus, IofToRunner) {
    EXPECT_EQ(IofXml::iofStatusToRunner("OK"),               StatusOK);
    EXPECT_EQ(IofXml::iofStatusToRunner("DidNotStart"),      StatusDNS);
    EXPECT_EQ(IofXml::iofStatusToRunner("SportingWithdrawal"), StatusCANCEL);
    EXPECT_EQ(IofXml::iofStatusToRunner("DidNotFinish"),     StatusDNF);
    EXPECT_EQ(IofXml::iofStatusToRunner("MissingPunch"),     StatusMP);
    EXPECT_EQ(IofXml::iofStatusToRunner("Disqualified"),     StatusDQ);
    EXPECT_EQ(IofXml::iofStatusToRunner("OverTime"),         StatusMAX);
    EXPECT_EQ(IofXml::iofStatusToRunner("NotCompeting"),     StatusOutOfCompetition);
    EXPECT_EQ(IofXml::iofStatusToRunner("Active"),           StatusUnknown);
    EXPECT_EQ(IofXml::iofStatusToRunner("UnknownStatus"),    StatusUnknown);
}

TEST(IofXmlStatus, RoundTrip) {
    // All mapped statuses survive a round-trip
    for (RunnerStatus st : {StatusOK, StatusDNS, StatusCANCEL, StatusDNF,
                            StatusMP, StatusDQ, StatusMAX, StatusOutOfCompetition}) {
        std::string iof = IofXml::runnerStatusToIof(st);
        EXPECT_EQ(IofXml::iofStatusToRunner(iof), st) << "Status " << st;
    }
}

// ============================================================================
// Duration parsing tests
// ============================================================================

TEST(IofXmlTime, ParseDurationIsoDuration) {
    EXPECT_EQ(IofXml::parseIofDuration("PT23M45S"), (23 * 60 + 45) * 10);
    EXPECT_EQ(IofXml::parseIofDuration("PT1H23M45S"), (3600 + 23*60 + 45) * 10);
    EXPECT_EQ(IofXml::parseIofDuration("PT45S"), 45 * 10);
    EXPECT_EQ(IofXml::parseIofDuration("PT0S"), 0);
    EXPECT_EQ(IofXml::parseIofDuration("PT1H"), 3600 * 10);
}

TEST(IofXmlTime, ParseDurationIntegerSeconds) {
    EXPECT_EQ(IofXml::parseIofDuration("1425"), 14250);
    EXPECT_EQ(IofXml::parseIofDuration("0"), 0);
    EXPECT_EQ(IofXml::parseIofDuration("3600"), 36000);
}

TEST(IofXmlTime, ParseDurationEmpty) {
    EXPECT_EQ(IofXml::parseIofDuration(""), 0);
}

TEST(IofXmlTime, FormatDuration) {
    EXPECT_EQ(IofXml::formatIofDuration(14250), "1425");
    EXPECT_EQ(IofXml::formatIofDuration(0), "0");
    EXPECT_EQ(IofXml::formatIofDuration(36000), "3600");
}

TEST(IofXmlTime, DurationRoundTrip) {
    int tenths = (23 * 60 + 45) * 10;
    std::string s = IofXml::formatIofDuration(tenths);
    EXPECT_EQ(IofXml::parseIofDuration(s), tenths);
}

// ============================================================================
// Time-of-day parsing tests
// ============================================================================

TEST(IofXmlTime, ParseTimeOfDay_Plain) {
    int t = IofXml::parseIofTimeOfDay("10:30:00");
    EXPECT_EQ(t, (10 * 3600 + 30 * 60) * 10);
}

TEST(IofXmlTime, ParseTimeOfDay_WithTimezone) {
    // "+02:00" stripped
    int t1 = IofXml::parseIofTimeOfDay("10:30:00+02:00");
    int t2 = IofXml::parseIofTimeOfDay("10:30:00");
    EXPECT_EQ(t1, t2);
}

TEST(IofXmlTime, ParseTimeOfDay_WithDatetime) {
    int t = IofXml::parseIofTimeOfDay("2023-08-01T10:30:00+02:00");
    EXPECT_EQ(t, (10 * 3600 + 30 * 60) * 10);
}

TEST(IofXmlTime, ParseTimeOfDay_Zulu) {
    int t1 = IofXml::parseIofTimeOfDay("10:30:00Z");
    int t2 = IofXml::parseIofTimeOfDay("10:30:00");
    EXPECT_EQ(t1, t2);
}

TEST(IofXmlTime, ParseTimeOfDay_Empty) {
    EXPECT_EQ(IofXml::parseIofTimeOfDay(""), -1);
}

TEST(IofXmlTime, FormatTimeOfDay) {
    int t = (10 * 3600 + 30 * 60) * 10;  // 10:30:00 in tenths
    EXPECT_EQ(IofXml::formatIofTimeOfDay(t), "10:30:00");
}

// ============================================================================
// Import: readClasses
// ============================================================================

TEST(IofXmlImport, ReadClasses_Basic) {
    oEvent oe;
    IofXml iof(oe);

    std::string xml =
        "<ClassList xmlns=\"http://www.orienteering.org/datastandard/3.0\""
        " iofVersion=\"3.0\">"
        "  <Class>"
        "    <Id>1</Id>"
        "    <Name>H21</Name>"
        "  </Class>"
        "  <Class>"
        "    <Id>2</Id>"
        "    <Name>D21</Name>"
        "  </Class>"
        "</ClassList>";

    int count = iof.readClasses(xml);
    EXPECT_EQ(count, 2);

    oClass* c1 = oe.getClass(1);
    ASSERT_NE(c1, nullptr);
    EXPECT_EQ(c1->getName(), L"H21");

    oClass* c2 = oe.getClass(2);
    ASSERT_NE(c2, nullptr);
    EXPECT_EQ(c2->getName(), L"D21");
}

TEST(IofXmlImport, ReadClasses_EmptyName_Skipped) {
    oEvent oe;
    IofXml iof(oe);
    std::string xml =
        "<ClassList>"
        "  <Class><Id>5</Id><Name></Name></Class>"
        "</ClassList>";
    EXPECT_EQ(iof.readClasses(xml), 0);
}

TEST(IofXmlImport, ReadClasses_InvalidRoot) {
    oEvent oe;
    IofXml iof(oe);
    EXPECT_EQ(iof.readClasses("<NotClassList/>"), 0);
}

// ============================================================================
// Import: readOrganisations (clubs)
// ============================================================================

TEST(IofXmlImport, ReadOrganisations_Basic) {
    oEvent oe;
    IofXml iof(oe);

    std::string xml =
        "<OrganisationList iofVersion=\"3.0\">"
        "  <Organisation>"
        "    <Id>10</Id>"
        "    <Name>IFK Göteborg</Name>"
        "  </Organisation>"
        "  <Organisation>"
        "    <Id>11</Id>"
        "    <Name>OK Älgen</Name>"
        "  </Organisation>"
        "</OrganisationList>";

    int count = iof.readOrganisations(xml);
    EXPECT_EQ(count, 2);

    oClub* club = oe.getClub(10);
    ASSERT_NE(club, nullptr);
    EXPECT_EQ(club->getName(), L"IFK Göteborg");
}

// ============================================================================
// Import: readCourseData
// ============================================================================

TEST(IofXmlImport, ReadCourseData_Basic) {
    oEvent oe;
    IofXml iof(oe);

    std::string xml =
        "<CourseData iofVersion=\"3.0\">"
        "  <RaceCourseData>"
        "    <Course>"
        "      <Id>100</Id>"
        "      <Name>Long</Name>"
        "      <Length>5200</Length>"
        "      <CourseControl type=\"Start\">"
        "        <Control><Id>61</Id></Control>"
        "      </CourseControl>"
        "      <CourseControl sequence=\"1\">"
        "        <Control><Id>101</Id></Control>"
        "      </CourseControl>"
        "      <CourseControl sequence=\"2\">"
        "        <Control><Id>102</Id></Control>"
        "      </CourseControl>"
        "      <CourseControl type=\"Finish\">"
        "        <Control><Id>99</Id></Control>"
        "      </CourseControl>"
        "    </Course>"
        "  </RaceCourseData>"
        "</CourseData>";

    int count = iof.readCourseData(xml);
    EXPECT_EQ(count, 1);

    oCourse* crs = oe.getCourse(100);
    ASSERT_NE(crs, nullptr);
    EXPECT_EQ(crs->getName(), L"Long");
    EXPECT_EQ(crs->getLength(), 5200);
    EXPECT_EQ(crs->nControls(), 2);  // only non-start/finish controls
}

TEST(IofXmlImport, ReadCourseData_ControlsRegistered) {
    oEvent oe;
    IofXml iof(oe);

    std::string xml =
        "<CourseData iofVersion=\"3.0\">"
        "  <RaceCourseData>"
        "    <Course>"
        "      <Id>1</Id><Name>Test</Name>"
        "      <CourseControl sequence=\"1\"><Control><Id>150</Id></Control></CourseControl>"
        "      <CourseControl sequence=\"2\"><Control><Id>151</Id></Control></CourseControl>"
        "    </Course>"
        "  </RaceCourseData>"
        "</CourseData>";

    iof.readCourseData(xml);
    // Controls 150 and 151 should now exist in the event
    EXPECT_NE(oe.getControl(150), nullptr);
    EXPECT_NE(oe.getControl(151), nullptr);
}

// ============================================================================
// Import: readEntryList
// ============================================================================

TEST(IofXmlImport, ReadEntryList_Basic) {
    oEvent oe;
    // Pre-create class and club so IDs match
    oClass* cls = oe.addClass(1);
    cls->setName(L"H21", false);
    oClub* club = oe.addClub(10);
    club->setName(L"MyClub");

    IofXml iof(oe);
    std::string xml =
        "<EntryList iofVersion=\"3.0\">"
        "  <PersonEntry>"
        "    <Person>"
        "      <Name><Given>John</Given><Family>Smith</Family></Name>"
        "    </Person>"
        "    <Organisation><Id>10</Id><Name>MyClub</Name></Organisation>"
        "    <Class><Id>1</Id><Name>H21</Name></Class>"
        "  </PersonEntry>"
        "</EntryList>";

    int count = iof.readEntryList(xml);
    EXPECT_EQ(count, 1);
    ASSERT_FALSE(oe.Runners.empty());
    EXPECT_EQ(oe.Runners.front().getName(), L"John Smith");
    EXPECT_EQ(oe.Runners.front().getClubId(), 10);
    EXPECT_EQ(oe.Runners.front().getClassId(false), 1);
}

TEST(IofXmlImport, ReadStartList_WithStartTimes) {
    oEvent oe;
    oe.ZeroTime = (9 * 3600) * 10;  // 09:00 = 324000 tenths

    IofXml iof(oe);
    std::string xml =
        "<StartList iofVersion=\"3.0\">"
        "  <ClassStart>"
        "    <Class><Id>1</Id><Name>H21</Name></Class>"
        "    <PersonStart>"
        "      <Person><Name><Given>Anna</Given><Family>Berg</Family></Name></Person>"
        "      <Start><StartTime>10:00:00</StartTime></Start>"
        "    </PersonStart>"
        "  </ClassStart>"
        "</StartList>";

    int count = iof.readEntryList(xml);
    EXPECT_EQ(count, 1);
    ASSERT_FALSE(oe.Runners.empty());
    const oRunner& r = oe.Runners.front();
    // Start time 10:00:00 = 36000 tenths; relative to ZeroTime (9:00 = 32400 tenths)
    EXPECT_EQ(r.getStartTime(), (3600) * 10);  // 1 hour from zero time
}

TEST(IofXmlImport, ReadResultList_WithResults) {
    oEvent oe;
    oe.ZeroTime = (9 * 3600) * 10;  // 09:00

    IofXml iof(oe);
    std::string xml =
        "<ResultList iofVersion=\"3.0\" status=\"Complete\">"
        "  <ClassResult>"
        "    <Class><Id>1</Id><Name>H21</Name></Class>"
        "    <PersonResult>"
        "      <Person><Name><Given>Erik</Given><Family>Svensson</Family></Name></Person>"
        "      <Result>"
        "        <StartTime>10:00:00</StartTime>"
        "        <Time>1425</Time>"
        "        <Status>OK</Status>"
        "      </Result>"
        "    </PersonResult>"
        "  </ClassResult>"
        "</ResultList>";

    int count = iof.readEntryList(xml);
    EXPECT_EQ(count, 1);
    ASSERT_FALSE(oe.Runners.empty());
    const oRunner& r = oe.Runners.front();
    EXPECT_EQ(r.getStatus(), StatusOK);
    EXPECT_EQ(r.getName(), L"Erik Svensson");
    // Running time: 1425 seconds = 14250 tenths
    // finishTime = startTime + 1425*10
    int startTenths = (10 * 3600 - 9 * 3600) * 10;  // relative: 1h from zero
    int expectedFinish = startTenths + 14250;
    EXPECT_EQ(r.getFinishTime(), expectedFinish);
}

// ============================================================================
// Export: writeResultList
// ============================================================================

TEST(IofXmlExport, WriteResultList_Basic) {
    oEvent oe;
    oe.Name = L"Test Race";
    oe.ZeroTime = (10 * 3600) * 10;  // 10:00

    oClass* cls = oe.addClass(1);
    cls->setName(L"H21", false);
    oClub* club = oe.addClub(5);
    club->setName(L"OK Test");

    oRunner* r = oe.addRunner(0);
    r->setName(L"John Smith", false);
    r->setClassId(1, false);
    r->setClubId(5);
    r->setStartTime(30 * 60 * 10, false, oBase::ChangeType::Quiet);  // 30 min from zero
    r->setFinishTime(30 * 60 * 10 + 1425 * 10);  // 30 min + 1425 sec
    r->setStatus(StatusOK, false, oBase::ChangeType::Quiet);

    IofXml iof(oe);
    xmlparser xml;
    xml.openMemoryOutput(false);
    iof.writeResultList(xml);
    std::string out;
    xml.getMemoryOutput(out);

    EXPECT_NE(out.find("ResultList"), std::string::npos);
    EXPECT_NE(out.find("ClassResult"), std::string::npos);
    EXPECT_NE(out.find("H21"), std::string::npos);
    EXPECT_NE(out.find("John"), std::string::npos);
    EXPECT_NE(out.find("Smith"), std::string::npos);
    EXPECT_NE(out.find("OK"), std::string::npos);
    EXPECT_NE(out.find("1425"), std::string::npos);
}

TEST(IofXmlExport, WriteResultList_ClassFilter) {
    oEvent oe;
    oe.Name = L"Race";

    oClass* c1 = oe.addClass(1);
    c1->setName(L"H21", false);
    oClass* c2 = oe.addClass(2);
    c2->setName(L"D21", false);

    oRunner* r1 = oe.addRunner(0);
    r1->setName(L"Alpha", false);
    r1->setClassId(1, false);

    oRunner* r2 = oe.addRunner(0);
    r2->setName(L"Beta", false);
    r2->setClassId(2, false);

    IofXml iof(oe);
    xmlparser xml;
    xml.openMemoryOutput(false);
    iof.writeResultList(xml, {1});
    std::string out;
    xml.getMemoryOutput(out);

    EXPECT_NE(out.find("Alpha"), std::string::npos);
    EXPECT_EQ(out.find("Beta"),  std::string::npos);
}

// ============================================================================
// Export: writeStartList
// ============================================================================

TEST(IofXmlExport, WriteStartList_Basic) {
    oEvent oe;
    oe.Name = L"Start Race";
    oe.ZeroTime = (9 * 3600) * 10;  // 09:00

    oClass* cls = oe.addClass(1);
    cls->setName(L"H21", false);

    oRunner* r = oe.addRunner(0);
    r->setName(L"Lars Eriksson", false);
    r->setClassId(1, false);
    r->setStartTime(60 * 60 * 10, false, oBase::ChangeType::Quiet);  // 1h from zero

    IofXml iof(oe);
    xmlparser xml;
    xml.openMemoryOutput(false);
    iof.writeStartList(xml);
    std::string out;
    xml.getMemoryOutput(out);

    EXPECT_NE(out.find("StartList"), std::string::npos);
    EXPECT_NE(out.find("ClassStart"), std::string::npos);
    EXPECT_NE(out.find("H21"), std::string::npos);
    EXPECT_NE(out.find("Lars"), std::string::npos);
    EXPECT_NE(out.find("Eriksson"), std::string::npos);
    // Start time: ZeroTime(9h) + 1h = 10:00:00
    EXPECT_NE(out.find("10:00:00"), std::string::npos);
}

TEST(IofXmlExport, WriteStartList_NoRunners_EmptyOutput) {
    oEvent oe;
    oe.Name = L"Empty Race";
    oClass* cls = oe.addClass(1);
    cls->setName(L"H21", false);
    // No runners added

    IofXml iof(oe);
    xmlparser xml;
    xml.openMemoryOutput(false);
    iof.writeStartList(xml);
    std::string out;
    xml.getMemoryOutput(out);

    EXPECT_NE(out.find("StartList"), std::string::npos);
    // No ClassStart since no runners
    EXPECT_EQ(out.find("ClassStart"), std::string::npos);
}

// ============================================================================
// Export: IOF 3.0 namespace in output
// ============================================================================

TEST(IofXmlExport, ResultListHasNamespace) {
    oEvent oe;
    oe.Name = L"NS Test";

    IofXml iof(oe);
    xmlparser xml;
    xml.openMemoryOutput(false);
    iof.writeResultList(xml);
    std::string out;
    xml.getMemoryOutput(out);

    EXPECT_NE(out.find("orienteering.org"), std::string::npos);
    EXPECT_NE(out.find("iofVersion"), std::string::npos);
}

TEST(IofXmlExport, StartListHasNamespace) {
    oEvent oe;
    oe.Name = L"NS Test";

    IofXml iof(oe);
    xmlparser xml;
    xml.openMemoryOutput(false);
    iof.writeStartList(xml);
    std::string out;
    xml.getMemoryOutput(out);

    EXPECT_NE(out.find("orienteering.org"), std::string::npos);
}

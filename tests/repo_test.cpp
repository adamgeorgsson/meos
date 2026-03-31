#include <gtest/gtest.h>
#include "SQLiteDatabase.h"
#include "DbMigration.h"
#include "ClubRepository.h"
#include "ControlRepository.h"
#include "CourseRepository.h"
#include "oEvent.h"

using namespace meos_db;

// Helper: open in-memory DB with full schema
static void openWithSchema(SQLiteDatabase& db) {
    db.open(":memory:");
    DbMigrationManager mgr(db);
    mgr.applyMigrations(SchemaV2::migrations());
}

// ---------------------------------------------------------------------------
// ClubRepository tests
// ---------------------------------------------------------------------------

TEST(ClubRepositoryTest, InsertAndFindById) {
    SQLiteDatabase db;
    openWithSchema(db);
    ClubRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    auto* club = oe.addClub(L"Alfa OK", 1);
    ASSERT_NE(club, nullptr);

    repo.insert(*club);

    auto row = repo.findById(1);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->id, 1);
    EXPECT_EQ(row->name, "Alfa OK");
    EXPECT_FALSE(row->odata.empty());
    EXPECT_EQ(static_cast<int>(row->odata.size()), oClub::getODataBlobSize());
}

TEST(ClubRepositoryTest, UpdateClub) {
    SQLiteDatabase db;
    openWithSchema(db);
    ClubRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    auto* club = oe.addClub(L"Old Name", 2);
    repo.insert(*club);

    club->setName(L"New Name");
    repo.update(*club);

    auto row = repo.findById(2);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->name, "New Name");
}

TEST(ClubRepositoryTest, RemoveClub) {
    SQLiteDatabase db;
    openWithSchema(db);
    ClubRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    auto* club = oe.addClub(L"ToRemove", 3);
    repo.insert(*club);
    ASSERT_TRUE(repo.findById(3).has_value());

    repo.remove(3);
    EXPECT_FALSE(repo.findById(3).has_value());
}

TEST(ClubRepositoryTest, FindAll) {
    SQLiteDatabase db;
    openWithSchema(db);
    ClubRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    auto* c1 = oe.addClub(L"Club A", 10);
    auto* c2 = oe.addClub(L"Club B", 11);
    repo.insert(*c1);
    repo.insert(*c2);

    auto all = repo.findAll();
    EXPECT_EQ(all.size(), 2u);
}

TEST(ClubRepositoryTest, ODataBlobRoundTrip) {
    SQLiteDatabase db;
    openWithSchema(db);
    ClubRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    auto* club = oe.addClub(L"BlobClub", 20);

    // Write a known pattern to oData
    BYTE* odata = club->getOData();
    for (int i = 0; i < 8; ++i) odata[i] = static_cast<BYTE>(i * 7 + 1);

    repo.insert(*club);
    auto row = repo.findById(20);
    ASSERT_TRUE(row.has_value());
    ASSERT_GE(static_cast<int>(row->odata.size()), 8);
    for (int i = 0; i < 8; ++i)
        EXPECT_EQ(row->odata[i], static_cast<uint8_t>(i * 7 + 1));
}

// ---------------------------------------------------------------------------
// ControlRepository tests
// ---------------------------------------------------------------------------

TEST(ControlRepositoryTest, InsertAndFindById) {
    SQLiteDatabase db;
    openWithSchema(db);
    ControlRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    auto* ctrl = oe.getControl(31, true, false);
    ASSERT_NE(ctrl, nullptr);
    ctrl->set(31, 31, L"Control 31");

    repo.insert(*ctrl);

    auto row = repo.findById(31);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->id, 31);
    EXPECT_EQ(row->name, "Control 31");
}

TEST(ControlRepositoryTest, UpdateControl) {
    SQLiteDatabase db;
    openWithSchema(db);
    ControlRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    auto* ctrl = oe.getControl(32, true, false);
    ctrl->set(32, 32, L"OldCtrl");
    repo.insert(*ctrl);

    ctrl->setName(L"NewCtrl");
    repo.update(*ctrl);

    auto row = repo.findById(32);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->name, "NewCtrl");
}

TEST(ControlRepositoryTest, RemoveControl) {
    SQLiteDatabase db;
    openWithSchema(db);
    ControlRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    auto* ctrl = oe.getControl(33, true, false);
    ctrl->set(33, 33, L"TempCtrl");
    repo.insert(*ctrl);
    ASSERT_TRUE(repo.findById(33).has_value());

    repo.remove(33);
    EXPECT_FALSE(repo.findById(33).has_value());
}

TEST(ControlRepositoryTest, FindAll) {
    SQLiteDatabase db;
    openWithSchema(db);
    ControlRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    auto* c1 = oe.getControl(40, true, false); c1->set(40, 40, L"A");
    auto* c2 = oe.getControl(41, true, false); c2->set(41, 41, L"B");
    repo.insert(*c1);
    repo.insert(*c2);

    auto all = repo.findAll();
    EXPECT_EQ(all.size(), 2u);
}

// ---------------------------------------------------------------------------
// CourseRepository tests
// ---------------------------------------------------------------------------

TEST(CourseRepositoryTest, InsertAndFindById) {
    SQLiteDatabase db;
    openWithSchema(db);
    CourseRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    auto* course = oe.addCourse(L"Sprint", 3200, 1);
    ASSERT_NE(course, nullptr);

    repo.insert(*course);

    auto row = repo.findById(1);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->id, 1);
    EXPECT_EQ(row->name, "Sprint");
    EXPECT_EQ(row->length, 3200);
}

TEST(CourseRepositoryTest, UpdateCourse) {
    SQLiteDatabase db;
    openWithSchema(db);
    CourseRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    auto* course = oe.addCourse(L"Long", 8000, 2);
    repo.insert(*course);

    course->setName(L"LongUpdated");
    course->setLength(9000);
    repo.update(*course);

    auto row = repo.findById(2);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->name, "LongUpdated");
    EXPECT_EQ(row->length, 9000);
}

TEST(CourseRepositoryTest, RemoveCourse) {
    SQLiteDatabase db;
    openWithSchema(db);
    CourseRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    auto* course = oe.addCourse(L"ToDelete", 1000, 3);
    repo.insert(*course);
    ASSERT_TRUE(repo.findById(3).has_value());

    repo.remove(3);
    EXPECT_FALSE(repo.findById(3).has_value());
}

TEST(CourseRepositoryTest, FindAll) {
    SQLiteDatabase db;
    openWithSchema(db);
    CourseRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    auto* c1 = oe.addCourse(L"C1", 1000, 10);
    auto* c2 = oe.addCourse(L"C2", 2000, 11);
    repo.insert(*c1);
    repo.insert(*c2);

    auto all = repo.findAll();
    EXPECT_EQ(all.size(), 2u);
}

TEST(CourseRepositoryTest, ODataBlobPersisted) {
    SQLiteDatabase db;
    openWithSchema(db);
    CourseRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    auto* course = oe.addCourse(L"BlobCourse", 5000, 20);

    // Write a known pattern to oData
    BYTE* odata = course->getOData();
    for (int i = 0; i < 8; ++i) odata[i] = static_cast<BYTE>(i + 10);

    repo.insert(*course);
    auto row = repo.findById(20);
    ASSERT_TRUE(row.has_value());
    ASSERT_GE(static_cast<int>(row->odata.size()), 8);
    for (int i = 0; i < 8; ++i)
        EXPECT_EQ(row->odata[i], static_cast<uint8_t>(i + 10));
}

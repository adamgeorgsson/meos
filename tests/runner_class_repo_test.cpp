// runner_class_repo_test.cpp — Tests for ClassRepository, RunnerRepository,
// CardRepository, FreePunchRepository (US-004c).
#include <gtest/gtest.h>
#include "SQLiteDatabase.h"
#include "DbMigration.h"
#include "ClassRepository.h"
#include "ClubRepository.h"
#include "RunnerRepository.h"
#include "CardRepository.h"
#include "FreePunchRepository.h"
#include "oEvent.h"
#include "oClass.h"
#include "oRunner.h"
#include "oCard.h"
#include "oFreePunch.h"

using namespace meos_db;

// Helper: open in-memory DB with V3 schema
static void openWithV3Schema(SQLiteDatabase& db) {
    db.open(":memory:");
    DbMigrationManager mgr(db);
    mgr.applyMigrations(SchemaV3::migrations());
}

// ─── ClassRepository ───────────────────────────────────────────────────────

TEST(ClassRepositoryTest, InsertAndFindById) {
    SQLiteDatabase db;
    openWithV3Schema(db);
    ClassRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    auto* cls = oe.addClass(L"H21E", 0, 1);
    ASSERT_NE(cls, nullptr);

    repo.insert(*cls);

    auto row = repo.findById(1);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->id, 1);
    EXPECT_EQ(row->name, "H21E");
    EXPECT_FALSE(row->odata.empty());
    EXPECT_EQ(static_cast<int>(row->odata.size()), oClass::getODataBlobSize());
}

TEST(ClassRepositoryTest, UpdateClass) {
    SQLiteDatabase db;
    openWithV3Schema(db);
    ClassRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    auto* cls = oe.addClass(L"OldName", 0, 2);
    repo.insert(*cls);

    cls->setName(L"NewName", false);
    repo.update(*cls);

    auto row = repo.findById(2);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->name, "NewName");
}

TEST(ClassRepositoryTest, RemoveClass) {
    SQLiteDatabase db;
    openWithV3Schema(db);
    ClassRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    auto* cls = oe.addClass(L"ToRemove", 0, 3);
    repo.insert(*cls);
    ASSERT_TRUE(repo.findById(3).has_value());

    repo.remove(3);
    EXPECT_FALSE(repo.findById(3).has_value());
}

TEST(ClassRepositoryTest, FindAll) {
    SQLiteDatabase db;
    openWithV3Schema(db);
    ClassRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    auto* c1 = oe.addClass(L"H21", 0, 10);
    auto* c2 = oe.addClass(L"D21", 0, 11);
    repo.insert(*c1);
    repo.insert(*c2);

    auto all = repo.findAll();
    EXPECT_EQ(all.size(), 2u);
}

TEST(ClassRepositoryTest, ODataBlobPersisted) {
    SQLiteDatabase db;
    openWithV3Schema(db);
    ClassRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    auto* cls = oe.addClass(L"BlobClass", 0, 20);

    BYTE* odata = cls->getOData();
    for (int i = 0; i < 8; ++i) odata[i] = static_cast<BYTE>(i + 5);

    repo.insert(*cls);
    auto row = repo.findById(20);
    ASSERT_TRUE(row.has_value());
    ASSERT_GE(static_cast<int>(row->odata.size()), 8);
    for (int i = 0; i < 8; ++i)
        EXPECT_EQ(row->odata[i], static_cast<uint8_t>(i + 5));
}

// ─── RunnerRepository ──────────────────────────────────────────────────────

TEST(RunnerRepositoryTest, InsertAndFindById) {
    SQLiteDatabase db;
    openWithV3Schema(db);
    RunnerRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    oRunner r(&oe, 1);
    r.setName(L"Erik Lindgren", false);
    auto* runner = oe.addRunner(r);
    ASSERT_NE(runner, nullptr);

    repo.insert(*runner);

    auto row = repo.findById(1);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->id, 1);
    EXPECT_EQ(row->name, "Erik Lindgren");
    EXPECT_FALSE(row->odata.empty());
    EXPECT_EQ(static_cast<int>(row->odata.size()), oRunner::getODataBlobSize());
}

TEST(RunnerRepositoryTest, UpdateRunner) {
    SQLiteDatabase db;
    openWithV3Schema(db);
    RunnerRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    oRunner r(&oe, 2);
    r.setName(L"Old Name", false);
    auto* runner = oe.addRunner(r);
    repo.insert(*runner);

    runner->setName(L"New Name", false);
    repo.update(*runner);

    auto row = repo.findById(2);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->name, "New Name");
}

TEST(RunnerRepositoryTest, RemoveRunner) {
    SQLiteDatabase db;
    openWithV3Schema(db);
    RunnerRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    oRunner r(&oe, 3);
    r.setName(L"ToRemove", false);
    auto* runner = oe.addRunner(r);
    repo.insert(*runner);
    ASSERT_TRUE(repo.findById(3).has_value());

    repo.remove(3);
    EXPECT_FALSE(repo.findById(3).has_value());
}

TEST(RunnerRepositoryTest, FindAll) {
    SQLiteDatabase db;
    openWithV3Schema(db);
    RunnerRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    oRunner r1(&oe, 10);
    r1.setName(L"Runner A", false);
    oRunner r2(&oe, 11);
    r2.setName(L"Runner B", false);
    auto* pr1 = oe.addRunner(r1);
    auto* pr2 = oe.addRunner(r2);
    repo.insert(*pr1);
    repo.insert(*pr2);

    auto all = repo.findAll();
    EXPECT_EQ(all.size(), 2u);
}

TEST(RunnerRepositoryTest, RunnerWithClubAndClassIds) {
    SQLiteDatabase db;
    openWithV3Schema(db);
    RunnerRepository repo(db);
    ClubRepository clubRepo(db);
    ClassRepository classRepo(db);

    oEvent oe;
    oe.newCompetition(L"Test");

    // Create club and class in DB first so FK constraints are satisfied
    auto* club = oe.addClub(L"TestClub", 7);
    auto* cls  = oe.addClass(L"H21", 0, 5);
    clubRepo.insert(*club);
    classRepo.insert(*cls);

    oRunner r(&oe, 20);
    r.setName(L"Maria Svensson", false);
    r.setClubId(club->getId());
    r.setClassId(cls->getId(), false);
    auto* runner = oe.addRunner(r);

    repo.insert(*runner);

    auto row = repo.findById(20);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->clubId, 7);
    EXPECT_EQ(row->classId, 5);
}

// ─── CardRepository ────────────────────────────────────────────────────────

TEST(CardRepositoryTest, InsertAndFindById) {
    SQLiteDatabase db;
    openWithV3Schema(db);
    CardRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    oCard card(&oe, 1);
    card.setCardNo(12345);
    // Import some punches so getPunchString() is non-trivial
    card.importPunches("31:09:45:00;100:10:30:00;");
    auto* pc = oe.addCard(card);
    ASSERT_NE(pc, nullptr);

    repo.insert(*pc);

    auto row = repo.findById(1);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->id, 1);
    EXPECT_EQ(row->cardNo, 12345);
    EXPECT_FALSE(row->punchString.empty());
}

TEST(CardRepositoryTest, UpdateCard) {
    SQLiteDatabase db;
    openWithV3Schema(db);
    CardRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    oCard card(&oe, 2);
    card.setCardNo(9999);
    auto* pc = oe.addCard(card);
    repo.insert(*pc);

    pc->setCardNo(8888);
    repo.update(*pc);

    auto row = repo.findById(2);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->cardNo, 8888);
}

TEST(CardRepositoryTest, RemoveCard) {
    SQLiteDatabase db;
    openWithV3Schema(db);
    CardRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    oCard card(&oe, 3);
    card.setCardNo(11111);
    auto* pc = oe.addCard(card);
    repo.insert(*pc);
    ASSERT_TRUE(repo.findById(3).has_value());

    repo.remove(3);
    EXPECT_FALSE(repo.findById(3).has_value());
}

TEST(CardRepositoryTest, FindAll) {
    SQLiteDatabase db;
    openWithV3Schema(db);
    CardRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    oCard c1(&oe, 10);
    c1.setCardNo(100);
    oCard c2(&oe, 11);
    c2.setCardNo(200);
    auto* pc1 = oe.addCard(c1);
    auto* pc2 = oe.addCard(c2);
    repo.insert(*pc1);
    repo.insert(*pc2);

    auto all = repo.findAll();
    EXPECT_EQ(all.size(), 2u);
}

// ─── FreePunchRepository ───────────────────────────────────────────────────

TEST(FreePunchRepositoryTest, InsertAndFindById) {
    SQLiteDatabase db;
    openWithV3Schema(db);
    FreePunchRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    // addFreePunch(time, type, unit, card, updateStartFinish, isOriginal)
    auto* fp = oe.addFreePunch(3600, 101, 0, 54321, false, true);
    ASSERT_NE(fp, nullptr);

    repo.insert(*fp);

    auto row = repo.findById(fp->getId());
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->cardNo, 54321);
    EXPECT_EQ(row->timeInt, 3600);
}

TEST(FreePunchRepositoryTest, RemoveFreePunch) {
    SQLiteDatabase db;
    openWithV3Schema(db);
    FreePunchRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    auto* fp = oe.addFreePunch(7200, 200, 0, 11111, false, true);
    ASSERT_NE(fp, nullptr);
    repo.insert(*fp);
    int fpId = fp->getId();
    ASSERT_TRUE(repo.findById(fpId).has_value());

    repo.remove(fpId);
    EXPECT_FALSE(repo.findById(fpId).has_value());
}

TEST(FreePunchRepositoryTest, FindAll) {
    SQLiteDatabase db;
    openWithV3Schema(db);
    FreePunchRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    auto* fp1 = oe.addFreePunch(1000, 100, 0, 11, false, true);
    auto* fp2 = oe.addFreePunch(2000, 200, 0, 22, false, true);
    ASSERT_NE(fp1, nullptr);
    ASSERT_NE(fp2, nullptr);
    repo.insert(*fp1);
    repo.insert(*fp2);

    auto all = repo.findAll();
    EXPECT_EQ(all.size(), 2u);
}

// ─── SchemaV3 migration test ───────────────────────────────────────────────

TEST(SchemaV3Test, TablesExist) {
    SQLiteDatabase db;
    openWithV3Schema(db);

    // All three new tables should exist
    auto r1 = db.query("SELECT name FROM sqlite_master WHERE type='table' AND name='classes'");
    EXPECT_FALSE(r1.empty());
    auto r2 = db.query("SELECT name FROM sqlite_master WHERE type='table' AND name='cards'");
    EXPECT_FALSE(r2.empty());
    auto r3 = db.query("SELECT name FROM sqlite_master WHERE type='table' AND name='free_punches'");
    EXPECT_FALSE(r3.empty());
}

TEST(SchemaV3Test, RunnerTableHasNewColumns) {
    SQLiteDatabase db;
    openWithV3Schema(db);

    // Verify new columns exist on runners table
    auto rows = db.query("PRAGMA table_info(runners)");
    std::vector<std::string> colNames;
    for (const auto& row : rows) {
        for (const auto& col : row) {
            if (col.first == "name") {
                colNames.push_back(col.second);
            }
        }
    }
    EXPECT_NE(std::find(colNames.begin(), colNames.end(), "class_id"), colNames.end());
    EXPECT_NE(std::find(colNames.begin(), colNames.end(), "course_id"), colNames.end());
    EXPECT_NE(std::find(colNames.begin(), colNames.end(), "start_no"), colNames.end());
    EXPECT_NE(std::find(colNames.begin(), colNames.end(), "start_time"), colNames.end());
    EXPECT_NE(std::find(colNames.begin(), colNames.end(), "finish_time"), colNames.end());
    EXPECT_NE(std::find(colNames.begin(), colNames.end(), "status"), colNames.end());
}

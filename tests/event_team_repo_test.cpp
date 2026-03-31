// event_team_repo_test.cpp — Integration tests for EventRepository and TeamRepository (US-004d).
#include <gtest/gtest.h>
#include "SQLiteDatabase.h"
#include "DbMigration.h"
#include "EventRepository.h"
#include "TeamRepository.h"
#include "ClubRepository.h"
#include "ClassRepository.h"
#include "RunnerRepository.h"
#include "oEvent.h"
#include "oTeam.h"
#include "oRunner.h"

using namespace meos_db;

// Helper: open in-memory DB with V4 schema
static void openWithV4Schema(SQLiteDatabase& db) {
    db.open(":memory:");
    DbMigrationManager mgr(db);
    mgr.applyMigrations(SchemaV4::migrations());
}

// ─── EventRepository ──────────────────────────────────────────────────────

TEST(EventRepositoryTest, SaveAndLoad) {
    SQLiteDatabase db;
    openWithV4Schema(db);
    EventRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"World Cup 2026");
    oe.setDate(L"2026-07-15");
    oe.setZeroTime(L"10:00:00");
    oe.setProperty("maxTime", 3600);
    oe.setProperty("region", L"Nordic");

    repo.save(oe, 1);

    auto row = repo.load(1);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->id, 1);
    EXPECT_EQ(row->name, "World Cup 2026");
    EXPECT_EQ(row->date, "2026-07-15");
    EXPECT_GT(row->zeroTime, 0);  // ZeroTime is non-zero after setZeroTime
}

TEST(EventRepositoryTest, PropertyRoundTrip) {
    SQLiteDatabase db;
    openWithV4Schema(db);
    EventRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"TestProp");
    oe.setProperty("alpha", L"hello");
    oe.setProperty("beta", L"wörld");
    oe.setProperty("gamma", 42);

    repo.save(oe, 1);

    auto row = repo.load(1);
    ASSERT_TRUE(row.has_value());

    auto props = EventRepository::decodeProperties(row->propertiesEncoded);
    EXPECT_EQ(props.count("alpha"), 1u);
    EXPECT_EQ(props.at("alpha"), L"hello");
    EXPECT_EQ(props.count("beta"), 1u);
    EXPECT_EQ(props.at("beta"), L"wörld");
    EXPECT_EQ(props.count("gamma"), 1u);
    EXPECT_EQ(props.at("gamma"), L"42");
}

TEST(EventRepositoryTest, OverwriteExistingEvent) {
    SQLiteDatabase db;
    openWithV4Schema(db);
    EventRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"First");
    repo.save(oe, 1);

    oe.setName(L"Updated");
    repo.save(oe, 1);

    auto row = repo.load(1);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->name, "Updated");
}

TEST(EventRepositoryTest, LoadMissing) {
    SQLiteDatabase db;
    openWithV4Schema(db);
    EventRepository repo(db);

    auto row = repo.load(99);
    EXPECT_FALSE(row.has_value());
}

// ─── TeamRepository ──────────────────────────────────────────────────────

TEST(TeamRepositoryTest, InsertAndFindById) {
    SQLiteDatabase db;
    openWithV4Schema(db);
    TeamRepository repo(db);
    ClubRepository clubRepo(db);
    ClassRepository classRepo(db);

    oEvent oe;
    oe.newCompetition(L"Test");

    auto* club = oe.addClub(L"Åby OK", 10);
    clubRepo.insert(*club);

    auto* cls = oe.addClass(L"H21E", 0, 5);
    classRepo.insert(*cls);

    auto* team = oe.addTeam(L"Alpha Team", 10, 5);
    ASSERT_NE(team, nullptr);
    repo.insert(*team);

    auto row = repo.findById(team->getId());
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->name, "Alpha Team");
    EXPECT_EQ(row->clubId, 10);
    EXPECT_EQ(row->classId, 5);
    EXPECT_FALSE(row->odata.empty());
    EXPECT_EQ(static_cast<int>(row->odata.size()), oTeam::getODataBlobSize());
}

TEST(TeamRepositoryTest, UpdateTeam) {
    SQLiteDatabase db;
    openWithV4Schema(db);
    TeamRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    auto* team = oe.addTeam(L"OldName");
    repo.insert(*team);

    team->setName(L"NewName", false);
    repo.update(*team);

    auto row = repo.findById(team->getId());
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->name, "NewName");
}

TEST(TeamRepositoryTest, RemoveTeam) {
    SQLiteDatabase db;
    openWithV4Schema(db);
    TeamRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    auto* team = oe.addTeam(L"ToRemove");
    repo.insert(*team);

    int id = team->getId();
    ASSERT_TRUE(repo.findById(id).has_value());
    repo.remove(id);
    EXPECT_FALSE(repo.findById(id).has_value());
}

TEST(TeamRepositoryTest, FindAll) {
    SQLiteDatabase db;
    openWithV4Schema(db);
    TeamRepository repo(db);

    oEvent oe;
    oe.newCompetition(L"Test");
    oe.addTeam(L"TeamA");
    oe.addTeam(L"TeamB");
    oe.addTeam(L"TeamC");

    for (auto& t : oe.Teams) {
        repo.insert(t);
    }

    auto all = repo.findAll();
    EXPECT_EQ(static_cast<int>(all.size()), 3);
}

TEST(TeamRepositoryTest, RunnerIdsRoundTrip) {
    SQLiteDatabase db;
    openWithV4Schema(db);
    TeamRepository repo(db);
    ClubRepository clubRepo(db);
    ClassRepository classRepo(db);
    RunnerRepository runnerRepo(db);

    oEvent oe;
    oe.newCompetition(L"Test");

    // Add a club so runners can reference it
    auto* club = oe.addClub(L"Klub", 1);
    clubRepo.insert(*club);

    // Add class
    auto* cls = oe.addClass(L"H2", 0, 2);
    classRepo.insert(*cls);

    // Create team
    auto* team = oe.addTeam(L"Relay Team", 1, 2);
    ASSERT_NE(team, nullptr);

    // Create runners and assign to team legs
    oRunner r1(&oe);
    r1.setName(L"Runner1", false);
    auto* pr1 = oe.addRunner(r1);
    runnerRepo.insert(*pr1);

    oRunner r2(&oe);
    r2.setName(L"Runner2", false);
    auto* pr2 = oe.addRunner(r2);
    runnerRepo.insert(*pr2);

    // Assign runners to legs
    team->setRunner(0, pr1, false);
    team->setRunner(1, pr2, false);
    repo.insert(*team);

    auto row = repo.findById(team->getId());
    ASSERT_TRUE(row.has_value());
    // runner_ids should contain both runner IDs
    EXPECT_NE(row->runnerIds.find(std::to_string(pr1->getId())), std::string::npos);
    EXPECT_NE(row->runnerIds.find(std::to_string(pr2->getId())), std::string::npos);

    // Decode runner ids
    std::vector<int> decoded;
    team->decodeRunners(row->runnerIds, decoded);
    EXPECT_GE(static_cast<int>(decoded.size()), 2);
}

// ─── Full Integration: create event -> add entities -> query back ─────────

TEST(EventTeamIntegration, FullRoundTrip) {
    SQLiteDatabase db;
    openWithV4Schema(db);

    EventRepository evRepo(db);
    ClubRepository clubRepo(db);
    ClassRepository classRepo(db);
    RunnerRepository runnerRepo(db);
    TeamRepository teamRepo(db);

    oEvent oe;
    oe.newCompetition(L"Nordic Relay 2026");
    oe.setDate(L"2026-08-01");
    oe.setZeroTime(L"09:00:00");
    oe.setProperty("maxRunners", 50);

    auto* club = oe.addClub(L"Täby RunnersK", 100);
    clubRepo.insert(*club);

    auto* cls = oe.addClass(L"Relay-H21", 0, 10);
    classRepo.insert(*cls);

    oRunner r(&oe);
    r.setName(L"Anna Nilsson", false);
    auto* pr = oe.addRunner(r);
    runnerRepo.insert(*pr);

    auto* team = oe.addTeam(L"Täby A", 100, 10);
    team->setRunner(0, pr, false);
    teamRepo.insert(*team);

    evRepo.save(oe, 1);

    // Query everything back
    auto evRow = evRepo.load(1);
    ASSERT_TRUE(evRow.has_value());
    EXPECT_EQ(evRow->name, "Nordic Relay 2026");
    EXPECT_EQ(evRow->date, "2026-08-01");

    auto props = EventRepository::decodeProperties(evRow->propertiesEncoded);
    EXPECT_EQ(props.at("maxRunners"), L"50");

    auto teams = teamRepo.findAll();
    ASSERT_EQ(static_cast<int>(teams.size()), 1);
    EXPECT_EQ(teams[0].name, "Täby A");
    EXPECT_EQ(teams[0].clubId, 100);
    EXPECT_EQ(teams[0].classId, 10);
    EXPECT_NE(teams[0].runnerIds.find(std::to_string(pr->getId())), std::string::npos);
}

#include <gtest/gtest.h>
#include <sqlite3.h>

#include <cstdio>
#include <filesystem>
#include <string>

#include "database.h"

using namespace meos::db;
using namespace meos::domain;

// Apply V1–V4 migrations (full schema).
static void applyAllMigrations(Database& db) {
  auto m = Database::v1Migrations();
  auto m2 = Database::v2Migrations();
  auto m3 = Database::v3Migrations();
  auto m4 = Database::v4Migrations();
  m.insert(m.end(), m2.begin(), m2.end());
  m.insert(m.end(), m3.begin(), m3.end());
  m.insert(m.end(), m4.begin(), m4.end());
  db.applyMigrations(m);
}

class EventsTeamsTest : public ::testing::Test {
 protected:
  Database db_{":memory:"};
  void SetUp() override { applyAllMigrations(db_); }
};

// ---- V4 Migration -----------------------------------------------------------

TEST_F(EventsTeamsTest, V4SchemaVersionIs4) {
  EXPECT_EQ(db_.schemaVersion(), 4);
}

TEST_F(EventsTeamsTest, V4CreatesEventsTable) {
  // getEvent() returns nullopt when events table is empty
  EXPECT_FALSE(db_.getEvent().has_value());
}

TEST_F(EventsTeamsTest, V4CreatesTeamsTable) {
  auto teams = db_.getAllTeams();
  EXPECT_TRUE(teams.empty());
}

// ---- Event upsert -----------------------------------------------------------

TEST_F(EventsTeamsTest, SaveEvent_CanReadBack) {
  Event e;
  e.name = "Summer Cup 2026";
  e.date = "2026-07-01";
  e.zeroTime = 36000;  // 10:00:00
  e.properties = std::nullopt;

  db_.saveEvent(e);

  auto opt = db_.getEvent();
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->name, "Summer Cup 2026");
  EXPECT_EQ(opt->date, "2026-07-01");
  EXPECT_EQ(opt->zeroTime, 36000);
  EXPECT_FALSE(opt->properties.has_value());
}

TEST_F(EventsTeamsTest, SaveEvent_WithProperties) {
  Event e;
  e.name = "Test";
  e.date = "2026-01-01";
  e.zeroTime = 0;
  e.properties = "key1=val1;key2=val2";

  db_.saveEvent(e);

  auto opt = db_.getEvent();
  ASSERT_TRUE(opt.has_value());
  ASSERT_TRUE(opt->properties.has_value());
  EXPECT_EQ(*opt->properties, "key1=val1;key2=val2");
}

TEST_F(EventsTeamsTest, SaveEvent_UpsertReplacesExisting) {
  Event e1;
  e1.name = "First";
  e1.date = "2026-01-01";
  e1.zeroTime = 0;
  db_.saveEvent(e1);

  Event e2;
  e2.name = "Second";
  e2.date = "2026-06-15";
  e2.zeroTime = 39600;  // 11:00:00
  db_.saveEvent(e2);

  auto opt = db_.getEvent();
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->name, "Second");
  EXPECT_EQ(opt->date, "2026-06-15");
  EXPECT_EQ(opt->zeroTime, 39600);
}

TEST_F(EventsTeamsTest, SaveEvent_MultipleCallsStillSingleRow) {
  for (int i = 0; i < 5; ++i) {
    Event e;
    e.name = "Event " + std::to_string(i);
    e.date = "2026-0" + std::to_string(i + 1) + "-01";
    e.zeroTime = i * 3600;
    db_.saveEvent(e);
  }
  // Only one row exists (id=1)
  sqlite3_stmt* raw = nullptr;
  int rc = sqlite3_prepare_v2(db_.handle(),
    "SELECT COUNT(*) FROM events", -1, &raw, nullptr);
  ASSERT_EQ(rc, SQLITE_OK);
  ASSERT_EQ(sqlite3_step(raw), SQLITE_ROW);
  int count = sqlite3_column_int(raw, 0);
  sqlite3_finalize(raw);
  EXPECT_EQ(count, 1);
}

// ---- Team CRUD --------------------------------------------------------------

TEST_F(EventsTeamsTest, InsertTeam_NoMembers) {
  Team t{0, "Alpha", std::nullopt, std::nullopt, {}};
  int id = db_.insertTeam(t);
  EXPECT_GT(id, 0);

  auto opt = db_.getTeamById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->name, "Alpha");
  EXPECT_FALSE(opt->clubId.has_value());
  EXPECT_FALSE(opt->classId.has_value());
  EXPECT_TRUE(opt->members.empty());
}

TEST_F(EventsTeamsTest, InsertTeam_WithMembers) {
  // Need clubs and runners first to satisfy FKs
  Club club{0, "IFK", std::nullopt};
  int clubId = db_.insertClub(club);

  Runner r1{0, "Alice", clubId, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  Runner r2{0, "Bob",   clubId, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  int r1id = db_.insertRunner(r1);
  int r2id = db_.insertRunner(r2);

  Team t{0, "TeamA", clubId, std::nullopt, {r1id, r2id}};
  int id = db_.insertTeam(t);

  auto opt = db_.getTeamById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->members.size(), 2u);
  EXPECT_NE(std::find(opt->members.begin(), opt->members.end(), r1id),
            opt->members.end());
  EXPECT_NE(std::find(opt->members.begin(), opt->members.end(), r2id),
            opt->members.end());
}

TEST_F(EventsTeamsTest, UpdateTeam_NameAndMembers) {
  Club club{0, "IFK", std::nullopt};
  int clubId = db_.insertClub(club);

  Runner r1{0, "Alice", clubId, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  Runner r2{0, "Bob",   clubId, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  int r1id = db_.insertRunner(r1);
  int r2id = db_.insertRunner(r2);

  Team t{0, "TeamA", std::nullopt, std::nullopt, {r1id}};
  int id = db_.insertTeam(t);

  // Update: rename and add r2, remove r1
  Team updated{id, "TeamA-Renamed", std::nullopt, std::nullopt, {r2id}};
  db_.updateTeam(updated);

  auto opt = db_.getTeamById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->name, "TeamA-Renamed");
  ASSERT_EQ(opt->members.size(), 1u);
  EXPECT_EQ(opt->members[0], r2id);
}

TEST_F(EventsTeamsTest, DeleteTeam_RemovesTeamAndMembers) {
  Club club{0, "IFK", std::nullopt};
  int clubId = db_.insertClub(club);
  Runner r{0, "Alice", clubId, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  int rid = db_.insertRunner(r);

  Team t{0, "TeamX", std::nullopt, std::nullopt, {rid}};
  int id = db_.insertTeam(t);

  db_.deleteTeam(id);

  EXPECT_FALSE(db_.getTeamById(id).has_value());
  // team_members row should also be gone
  sqlite3_stmt* raw = nullptr;
  int rc = sqlite3_prepare_v2(db_.handle(),
    "SELECT COUNT(*) FROM team_members WHERE team_id=?", -1, &raw, nullptr);
  ASSERT_EQ(rc, SQLITE_OK);
  sqlite3_bind_int(raw, 1, id);
  ASSERT_EQ(sqlite3_step(raw), SQLITE_ROW);
  int count = sqlite3_column_int(raw, 0);
  sqlite3_finalize(raw);
  EXPECT_EQ(count, 0);
}

TEST_F(EventsTeamsTest, GetAllTeams_ReturnsAll) {
  db_.insertTeam({0, "Alpha", std::nullopt, std::nullopt, {}});
  db_.insertTeam({0, "Beta",  std::nullopt, std::nullopt, {}});
  db_.insertTeam({0, "Gamma", std::nullopt, std::nullopt, {}});
  EXPECT_EQ(db_.getAllTeams().size(), 3u);
}

// ---- Integration: file-based round-trip ------------------------------------

TEST(EventsTeamsIntegration, FileRoundTrip) {
  const std::string dbPath = "/tmp/meos_test_roundtrip.db";
  // Remove any leftover from a prior run
  std::remove(dbPath.c_str());

  // Phase 1: create and populate
  {
    Database db(dbPath);
    applyAllMigrations(db);

    // Save event metadata
    Event e{"Summer Cup", "2026-08-15", 36000, "venue=Forest"};
    db.saveEvent(e);

    // Add a club and two runners
    int clubId = db.insertClub({0, "IFK Göteborg", std::nullopt});
    Runner r1{0, "Anna", clubId, std::nullopt, std::nullopt, std::nullopt, "OK"};
    Runner r2{0, "Bert", clubId, std::nullopt, std::nullopt, std::nullopt, "OK"};
    int r1id = db.insertRunner(r1);
    int r2id = db.insertRunner(r2);

    // Add a team with both runners
    Team t{0, "Relay Team", clubId, std::nullopt, {r1id, r2id}};
    db.insertTeam(t);
  }

  // Phase 2: reopen and verify
  {
    Database db(dbPath);
    applyAllMigrations(db);  // idempotent — already applied

    auto event = db.getEvent();
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->name, "Summer Cup");
    EXPECT_EQ(event->date, "2026-08-15");
    EXPECT_EQ(event->zeroTime, 36000);
    ASSERT_TRUE(event->properties.has_value());
    EXPECT_EQ(*event->properties, "venue=Forest");

    auto clubs = db.getAllClubs();
    ASSERT_EQ(clubs.size(), 1u);
    EXPECT_EQ(clubs[0].name, "IFK Göteborg");

    auto runners = db.getAllRunners();
    ASSERT_EQ(runners.size(), 2u);

    auto teams = db.getAllTeams();
    ASSERT_EQ(teams.size(), 1u);
    EXPECT_EQ(teams[0].name, "Relay Team");
    EXPECT_EQ(teams[0].members.size(), 2u);
  }

  std::remove(dbPath.c_str());
}

#include <gtest/gtest.h>
#include <sqlite3.h>

#include "database.h"

using namespace meos::db;

// ---- helpers ----------------------------------------------------------------

static bool tableExists(Database& db, const std::string& name) {
  sqlite3* h = db.handle();
  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(
      h,
      "SELECT name FROM sqlite_master WHERE type='table' AND name=?",
      -1, &stmt, nullptr);
  sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
  bool exists = sqlite3_step(stmt) == SQLITE_ROW;
  sqlite3_finalize(stmt);
  return exists;
}

static int migrationCount(Database& db) {
  sqlite3* h = db.handle();
  if (!tableExists(db, "_migrations")) return 0;
  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(h, "SELECT COUNT(*) FROM _migrations", -1, &stmt, nullptr);
  int count = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  return count;
}

// ---- fixtures ---------------------------------------------------------------

class MigrationTest : public ::testing::Test {
 protected:
  Database db_{":memory:"};
};

// ---- tests ------------------------------------------------------------------

TEST_F(MigrationTest, SchemaVersionZeroBeforeMigrations) {
  EXPECT_EQ(db_.schemaVersion(), 0);
}

TEST_F(MigrationTest, V1MigrationsCreateTrackingTable) {
  db_.applyMigrations(Database::v1Migrations());
  EXPECT_TRUE(tableExists(db_, "_migrations"));
}

TEST_F(MigrationTest, V1MigrationsCreateRunnersTable) {
  db_.applyMigrations(Database::v1Migrations());
  EXPECT_TRUE(tableExists(db_, "runners"));
}

TEST_F(MigrationTest, V1MigrationsCreateClubsTable) {
  db_.applyMigrations(Database::v1Migrations());
  EXPECT_TRUE(tableExists(db_, "clubs"));
}

TEST_F(MigrationTest, SchemaVersionAfterV1) {
  db_.applyMigrations(Database::v1Migrations());
  EXPECT_EQ(db_.schemaVersion(), 1);
}

TEST_F(MigrationTest, MigrationsRecordedInTrackingTable) {
  db_.applyMigrations(Database::v1Migrations());
  EXPECT_EQ(migrationCount(db_), 1);
}

TEST_F(MigrationTest, AlreadyAppliedMigrationsSkipped) {
  auto migrations = Database::v1Migrations();
  db_.applyMigrations(migrations);
  // Apply a second time — should not throw or duplicate the record.
  db_.applyMigrations(migrations);
  EXPECT_EQ(db_.schemaVersion(), 1);
  EXPECT_EQ(migrationCount(db_), 1);
}

TEST_F(MigrationTest, MultipleVersionsAppliedInOrder) {
  Migration v2{2, "Add controls",
               "CREATE TABLE IF NOT EXISTS controls ("
               "  id INTEGER PRIMARY KEY, code INTEGER NOT NULL)"};
  auto migrations = Database::v1Migrations();
  migrations.push_back(v2);

  db_.applyMigrations(migrations);

  EXPECT_EQ(db_.schemaVersion(), 2);
  EXPECT_TRUE(tableExists(db_, "clubs"));
  EXPECT_TRUE(tableExists(db_, "runners"));
  EXPECT_TRUE(tableExists(db_, "controls"));
  EXPECT_EQ(migrationCount(db_), 2);
}

TEST_F(MigrationTest, PartialMigrationsPickUpWhereLeftOff) {
  db_.applyMigrations(Database::v1Migrations());
  EXPECT_EQ(db_.schemaVersion(), 1);

  Migration v2{2, "Add controls",
               "CREATE TABLE IF NOT EXISTS controls ("
               "  id INTEGER PRIMARY KEY, code INTEGER NOT NULL)"};
  db_.applyMigrations({Database::v1Migrations()[0], v2});
  EXPECT_EQ(db_.schemaVersion(), 2);
  EXPECT_EQ(migrationCount(db_), 2);
}

TEST_F(MigrationTest, FailedMigrationRollsBack) {
  // Start with V1 applied.
  db_.applyMigrations(Database::v1Migrations());
  int before = db_.schemaVersion();

  // V2 migration is deliberately invalid SQL.
  Migration bad{2, "Bad migration", "THIS IS NOT VALID SQL;"};
  EXPECT_THROW(db_.applyMigrations({Database::v1Migrations()[0], bad}),
               std::runtime_error);

  // Schema version must not have advanced.
  EXPECT_EQ(db_.schemaVersion(), before);
  // Only V1 must be in the tracking table.
  EXPECT_EQ(migrationCount(db_), 1);
}

TEST_F(MigrationTest, FailedMigrationDoesNotLeaveDirtyState) {
  // A migration that creates a table and then has bad SQL.
  // The whole transaction should be rolled back.
  Migration v1{1, "Partial bad",
               "CREATE TABLE tmp_table (id INTEGER PRIMARY KEY);"
               "INVALID SQL STATEMENT;"};
  EXPECT_THROW(db_.applyMigrations({v1}), std::runtime_error);

  // tmp_table must not exist (rolled back).
  EXPECT_FALSE(tableExists(db_, "tmp_table"));
  // _migrations tracking table may or may not exist, but version must be 0.
  EXPECT_EQ(db_.schemaVersion(), 0);
  EXPECT_EQ(migrationCount(db_), 0);
}

TEST_F(MigrationTest, ForeignKeyEnabledAfterOpen) {
  // PRAGMA foreign_keys=ON is applied in the constructor.
  // Verify by attempting an INSERT that violates a FK after running migrations.
  db_.applyMigrations(Database::v1Migrations());

  // Insert a runner with a non-existent club_id — should fail.
  auto* h = db_.handle();
  char* err = nullptr;
  int rc = sqlite3_exec(
      h,
      "INSERT INTO runners (id, name, club_id) VALUES (1, 'Test', 9999)",
      nullptr, nullptr, &err);
  sqlite3_free(err);
  EXPECT_NE(rc, SQLITE_OK)
      << "Expected FK violation but INSERT succeeded";
}

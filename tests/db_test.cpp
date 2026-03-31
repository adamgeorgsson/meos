#include <gtest/gtest.h>
#include "SQLiteDatabase.h"
#include "DbMigration.h"

using namespace meos_db;

// ---------------------------------------------------------------------------
// SQLiteDatabase — basic operations
// ---------------------------------------------------------------------------

TEST(SQLiteDatabaseTest, OpenCloseInMemory) {
    SQLiteDatabase db;
    EXPECT_FALSE(db.isOpen());
    db.open(":memory:");
    EXPECT_TRUE(db.isOpen());
    db.close();
    EXPECT_FALSE(db.isOpen());
}

TEST(SQLiteDatabaseTest, DoubleOpenThrows) {
    SQLiteDatabase db;
    db.open(":memory:");
    EXPECT_THROW(db.open(":memory:"), DbException);
    db.close();
}

TEST(SQLiteDatabaseTest, ExecuteOnClosedThrows) {
    SQLiteDatabase db;
    EXPECT_THROW(db.execute("SELECT 1"), DbException);
}

TEST(SQLiteDatabaseTest, CreateTableAndInsert) {
    SQLiteDatabase db;
    db.open(":memory:");
    db.execute("CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT)");
    db.execute("INSERT INTO t VALUES (1, 'Alice')");
    auto rows = db.query("SELECT id, name FROM t");
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0][0].second, "1");
    EXPECT_EQ(rows[0][1].second, "Alice");
    db.close();
}

TEST(SQLiteDatabaseTest, QueryEmpty) {
    SQLiteDatabase db;
    db.open(":memory:");
    db.execute("CREATE TABLE t (id INTEGER PRIMARY KEY)");
    auto rows = db.query("SELECT * FROM t");
    EXPECT_TRUE(rows.empty());
    db.close();
}

TEST(SQLiteDatabaseTest, TransactionCommit) {
    SQLiteDatabase db;
    db.open(":memory:");
    db.execute("CREATE TABLE t (id INTEGER PRIMARY KEY)");
    db.beginTransaction();
    db.execute("INSERT INTO t VALUES (42)");
    db.commit();
    auto rows = db.query("SELECT id FROM t");
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0][0].second, "42");
    db.close();
}

TEST(SQLiteDatabaseTest, TransactionRollback) {
    SQLiteDatabase db;
    db.open(":memory:");
    db.execute("CREATE TABLE t (id INTEGER PRIMARY KEY)");
    db.beginTransaction();
    db.execute("INSERT INTO t VALUES (99)");
    db.rollback();
    auto rows = db.query("SELECT * FROM t");
    EXPECT_TRUE(rows.empty());
    db.close();
}

TEST(SQLiteDatabaseTest, LastInsertRowId) {
    SQLiteDatabase db;
    db.open(":memory:");
    db.execute("CREATE TABLE t (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT)");
    db.execute("INSERT INTO t (name) VALUES ('test')");
    EXPECT_EQ(db.lastInsertRowId(), 1LL);
    db.execute("INSERT INTO t (name) VALUES ('test2')");
    EXPECT_EQ(db.lastInsertRowId(), 2LL);
    db.close();
}

TEST(SQLiteDatabaseTest, ChangesCount) {
    SQLiteDatabase db;
    db.open(":memory:");
    db.execute("CREATE TABLE t (id INTEGER PRIMARY KEY, val INTEGER)");
    db.execute("INSERT INTO t VALUES (1, 10)");
    db.execute("INSERT INTO t VALUES (2, 20)");
    db.execute("UPDATE t SET val = 99");
    EXPECT_EQ(db.changesCount(), 2);
    db.close();
}

TEST(SQLiteDatabaseTest, ParameterizedExecute) {
    SQLiteDatabase db;
    db.open(":memory:");
    db.execute("CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT)");
    db.executeParams("INSERT INTO t VALUES (?, ?)", {"7", "Bob"});
    auto rows = db.query("SELECT name FROM t WHERE id = 7");
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0][0].second, "Bob");
    db.close();
}

TEST(SQLiteDatabaseTest, ParameterizedQuery) {
    SQLiteDatabase db;
    db.open(":memory:");
    db.execute("CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT)");
    db.execute("INSERT INTO t VALUES (1, 'Alpha')");
    db.execute("INSERT INTO t VALUES (2, 'Beta')");
    auto rows = db.queryParams("SELECT name FROM t WHERE id = ?", {"2"});
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0][0].second, "Beta");
    db.close();
}

TEST(SQLiteDatabaseTest, InvalidSqlThrows) {
    SQLiteDatabase db;
    db.open(":memory:");
    EXPECT_THROW(db.execute("NOT VALID SQL"), DbException);
    db.close();
}

TEST(SQLiteDatabaseTest, MoveSemantics) {
    SQLiteDatabase db1;
    db1.open(":memory:");
    db1.execute("CREATE TABLE t (id INTEGER PRIMARY KEY)");

    SQLiteDatabase db2 = std::move(db1);
    EXPECT_FALSE(db1.isOpen());
    EXPECT_TRUE(db2.isOpen());

    db2.execute("INSERT INTO t VALUES (1)");
    auto rows = db2.query("SELECT * FROM t");
    EXPECT_EQ(rows.size(), 1u);
    db2.close();
}

// ---------------------------------------------------------------------------
// DbMigrationManager
// ---------------------------------------------------------------------------

TEST(DbMigrationTest, InitialVersionIsZero) {
    SQLiteDatabase db;
    db.open(":memory:");
    DbMigrationManager mgr(db);
    EXPECT_EQ(mgr.getCurrentVersion(), 0);
    db.close();
}

TEST(DbMigrationTest, ApplyMigrationsCreatesSchema) {
    SQLiteDatabase db;
    db.open(":memory:");
    DbMigrationManager mgr(db);
    mgr.applyMigrations(SchemaV1::migrations());

    // clubs and runners tables must exist
    EXPECT_NO_THROW(db.execute("INSERT INTO clubs (id, name) VALUES (1, 'TestClub')"));
    EXPECT_NO_THROW(db.execute("INSERT INTO runners (id, name, club_id, card_no) VALUES (1, 'Runner1', 1, 12345)"));

    auto clubs = db.query("SELECT name FROM clubs WHERE id = 1");
    ASSERT_EQ(clubs.size(), 1u);
    EXPECT_EQ(clubs[0][0].second, "TestClub");

    auto runners = db.query("SELECT name, card_no FROM runners WHERE id = 1");
    ASSERT_EQ(runners.size(), 1u);
    EXPECT_EQ(runners[0][0].second, "Runner1");
    EXPECT_EQ(runners[0][1].second, "12345");

    db.close();
}

TEST(DbMigrationTest, MigrationVersionTracked) {
    SQLiteDatabase db;
    db.open(":memory:");
    DbMigrationManager mgr(db);
    mgr.applyMigrations(SchemaV1::migrations());
    EXPECT_EQ(mgr.getCurrentVersion(), 1);
    db.close();
}

TEST(DbMigrationTest, MigrationsAreIdempotent) {
    SQLiteDatabase db;
    db.open(":memory:");
    DbMigrationManager mgr(db);
    // Apply twice — should not throw or duplicate
    EXPECT_NO_THROW(mgr.applyMigrations(SchemaV1::migrations()));
    EXPECT_NO_THROW(mgr.applyMigrations(SchemaV1::migrations()));
    EXPECT_EQ(mgr.getCurrentVersion(), 1);
    db.close();
}

TEST(DbMigrationTest, MigrationsTableHasCorrectColumns) {
    SQLiteDatabase db;
    db.open(":memory:");
    DbMigrationManager mgr(db);
    mgr.applyMigrations(SchemaV1::migrations());

    auto rows = db.query("SELECT version, description, applied_at FROM _migrations");
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0][0].second, "1");
    EXPECT_FALSE(rows[0][1].second.empty()); // description
    EXPECT_FALSE(rows[0][2].second.empty()); // applied_at timestamp
    db.close();
}

TEST(DbMigrationTest, IncrementalMigrationsApplied) {
    // Simulate two sequential migrations
    Migration m2{
        2,
        "Add courses table",
        R"sql(
            CREATE TABLE IF NOT EXISTS courses (
                id   INTEGER PRIMARY KEY,
                name TEXT NOT NULL DEFAULT ''
            );
        )sql"
    };

    SQLiteDatabase db;
    db.open(":memory:");
    DbMigrationManager mgr(db);

    auto migrations = SchemaV1::migrations();
    migrations.push_back(m2);

    mgr.applyMigrations(migrations);
    EXPECT_EQ(mgr.getCurrentVersion(), 2);

    // courses table must exist
    EXPECT_NO_THROW(db.execute("INSERT INTO courses (id, name) VALUES (1, 'Sprint')"));
    auto rows = db.query("SELECT name FROM courses WHERE id = 1");
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0][0].second, "Sprint");

    db.close();
}

TEST(DbMigrationTest, FailedMigrationRolledBack) {
    Migration bad{
        2,
        "Bad migration",
        "THIS IS NOT VALID SQL !!!"
    };

    SQLiteDatabase db;
    db.open(":memory:");
    DbMigrationManager mgr(db);
    mgr.applyMigrations(SchemaV1::migrations());

    auto migrations = SchemaV1::migrations();
    migrations.push_back(bad);

    EXPECT_THROW(mgr.applyMigrations(migrations), DbException);

    // Version must still be 1 (the bad migration was rolled back)
    EXPECT_EQ(mgr.getCurrentVersion(), 1);
    db.close();
}

TEST(DbMigrationTest, ClubRunnerForeignKey) {
    SQLiteDatabase db;
    db.open(":memory:");
    DbMigrationManager mgr(db);
    mgr.applyMigrations(SchemaV1::migrations());

    db.execute("INSERT INTO clubs (id, name) VALUES (10, 'MyClub')");
    db.execute("INSERT INTO runners (id, name, club_id, card_no) VALUES (1, 'A', 10, 0)");

    auto rows = db.query("SELECT r.name, c.name FROM runners r JOIN clubs c ON r.club_id = c.id");
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0][0].second, "A");
    EXPECT_EQ(rows[0][1].second, "MyClub");
    db.close();
}

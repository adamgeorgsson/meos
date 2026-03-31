#pragma once
#include "SQLiteDatabase.h"
#include <string>
#include <vector>

namespace meos_db {

/// Represents a single schema migration.
struct Migration {
    int version;             ///< Monotonically increasing version (1, 2, 3, …)
    std::string description; ///< Human-readable description
    std::string sql;         ///< DDL/DML to apply (may contain multiple statements)
};

/**
 * DbMigrationManager tracks applied migrations in the `_migrations` table
 * and runs any pending ones in order.
 *
 * Each migration is wrapped in a BEGIN TRANSACTION / COMMIT so that a failed
 * migration is rolled back automatically without corrupting the schema.
 *
 * Usage:
 *   SQLiteDatabase db;
 *   db.open(":memory:");
 *   DbMigrationManager mgr(db);
 *   mgr.applyMigrations(SchemaV1::migrations());
 */
class DbMigrationManager {
public:
    explicit DbMigrationManager(SQLiteDatabase& db);

    /// Apply all migrations whose version > getCurrentVersion(), in order.
    /// Throws DbException if any migration fails (and rolls it back).
    void applyMigrations(const std::vector<Migration>& migrations);

    /// Return the highest version number already applied (0 if none).
    int getCurrentVersion();

private:
    SQLiteDatabase& db_;

    void ensureMigrationsTable();
    bool isMigrationApplied(int version);
    void recordMigration(int version, const std::string& description);
};

/**
 * The initial schema (V1): clubs and runners tables.
 * Additional migrations can be appended to the vector in future US-004* stories.
 */
struct SchemaV1 {
    static std::vector<Migration> migrations();
};

} // namespace meos_db

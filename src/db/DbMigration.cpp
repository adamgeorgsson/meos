#include "DbMigration.h"
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>

namespace meos_db {

// ---------------------------------------------------------------------------
// DbMigrationManager
// ---------------------------------------------------------------------------

DbMigrationManager::DbMigrationManager(SQLiteDatabase& db) : db_(db) {}

void DbMigrationManager::ensureMigrationsTable() {
    db_.execute(R"sql(
        CREATE TABLE IF NOT EXISTS _migrations (
            version     INTEGER PRIMARY KEY,
            description TEXT    NOT NULL,
            applied_at  TEXT    NOT NULL
        )
    )sql");
}

bool DbMigrationManager::isMigrationApplied(int version) {
    auto rows = db_.queryParams(
        "SELECT 1 FROM _migrations WHERE version = ?",
        {std::to_string(version)});
    return !rows.empty();
}

void DbMigrationManager::recordMigration(int version, const std::string& description) {
    // ISO-8601 UTC timestamp
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_utc;
#ifdef _WIN32
    gmtime_s(&tm_utc, &t);
#else
    gmtime_r(&t, &tm_utc);
#endif
    std::ostringstream ts;
    ts << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%SZ");

    db_.executeParams(
        "INSERT INTO _migrations (version, description, applied_at) VALUES (?, ?, ?)",
        {std::to_string(version), description, ts.str()});
}

int DbMigrationManager::getCurrentVersion() {
    ensureMigrationsTable();
    auto rows = db_.query("SELECT MAX(version) AS v FROM _migrations");
    if (rows.empty() || rows[0].empty() || rows[0][0].second.empty()) {
        return 0;
    }
    return std::stoi(rows[0][0].second);
}

void DbMigrationManager::applyMigrations(const std::vector<Migration>& migrations) {
    ensureMigrationsTable();

    for (const auto& m : migrations) {
        if (isMigrationApplied(m.version)) {
            continue; // already applied — idempotent
        }
        db_.beginTransaction();
        try {
            db_.execute(m.sql);
            recordMigration(m.version, m.description);
            db_.commit();
        } catch (...) {
            db_.rollback();
            throw;
        }
    }
}

// ---------------------------------------------------------------------------
// SchemaV1 — initial schema: clubs and runners
// ---------------------------------------------------------------------------

std::vector<Migration> SchemaV1::migrations() {
    return {
        {
            1,
            "Initial schema: clubs and runners",
            R"sql(
                CREATE TABLE IF NOT EXISTS clubs (
                    id          INTEGER PRIMARY KEY,
                    name        TEXT    NOT NULL DEFAULT '',
                    odata_blob  BLOB
                );

                CREATE TABLE IF NOT EXISTS runners (
                    id          INTEGER PRIMARY KEY,
                    name        TEXT    NOT NULL DEFAULT '',
                    club_id     INTEGER REFERENCES clubs(id) ON DELETE SET NULL,
                    card_no     INTEGER NOT NULL DEFAULT 0,
                    odata_blob  BLOB
                );
            )sql"
        }
    };
}

} // namespace meos_db

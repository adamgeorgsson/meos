#pragma once
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>

struct sqlite3;

namespace meos_db {

/// Thrown on SQLite errors.
class DbException : public std::runtime_error {
public:
    explicit DbException(const std::string& msg) : std::runtime_error(msg) {}
};

/// Single row returned from a query: column name -> value pairs (all as strings).
using DbRow = std::vector<std::pair<std::string, std::string>>;
using DbResultSet = std::vector<DbRow>;

/**
 * Lightweight RAII wrapper around a SQLite3 database connection.
 *
 * Usage:
 *   SQLiteDatabase db;
 *   db.open(":memory:");
 *   db.execute("CREATE TABLE IF NOT EXISTS t (id INTEGER PRIMARY KEY)");
 *   db.beginTransaction();
 *   db.execute("INSERT INTO t VALUES (1)");
 *   db.commit();
 *   auto rows = db.query("SELECT * FROM t");
 */
class SQLiteDatabase {
public:
    SQLiteDatabase();
    ~SQLiteDatabase();

    // Non-copyable, movable
    SQLiteDatabase(const SQLiteDatabase&) = delete;
    SQLiteDatabase& operator=(const SQLiteDatabase&) = delete;
    SQLiteDatabase(SQLiteDatabase&&) noexcept;
    SQLiteDatabase& operator=(SQLiteDatabase&&) noexcept;

    /// Open (or create) the database at the given filesystem path.
    /// Use ":memory:" for an in-memory database.
    void open(const std::string& path);

    /// Close the connection. Safe to call multiple times.
    void close();

    /// Returns true if the connection is open.
    bool isOpen() const;

    /// Execute a SQL statement that produces no result rows (DDL, INSERT, UPDATE, DELETE).
    /// Throws DbException on failure.
    void execute(const std::string& sql);

    /// Execute a SQL SELECT and return all result rows.
    /// Each row is a vector of (column_name, value_as_string) pairs.
    DbResultSet query(const std::string& sql);

    /// Execute a parameterized SQL statement with positional text parameters.
    void executeParams(const std::string& sql,
                       const std::vector<std::string>& params);

    /// Execute a parameterized query with positional text parameters.
    DbResultSet queryParams(const std::string& sql,
                            const std::vector<std::string>& params);

    void beginTransaction();
    void commit();
    void rollback();

    /// Return the rowid of the last successful INSERT.
    long long lastInsertRowId() const;

    /// Return the number of rows changed by the last INSERT/UPDATE/DELETE.
    int changesCount() const;

private:
    sqlite3* db_ = nullptr;

    void checkOpen(const char* ctx) const;
    void throwIfError(int rc, const char* ctx) const;
};

} // namespace meos_db

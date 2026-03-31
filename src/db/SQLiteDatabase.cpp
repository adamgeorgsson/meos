#include "SQLiteDatabase.h"
#include <sqlite3.h>
#include <sstream>
#include <utility>

namespace meos_db {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string makeErrMsg(const char* ctx, int rc, const char* sqliteMsg) {
    std::ostringstream ss;
    ss << ctx << ": SQLite error " << rc;
    if (sqliteMsg && sqliteMsg[0]) {
        ss << " — " << sqliteMsg;
    }
    return ss.str();
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

SQLiteDatabase::SQLiteDatabase() = default;

SQLiteDatabase::~SQLiteDatabase() {
    close();
}

SQLiteDatabase::SQLiteDatabase(SQLiteDatabase&& other) noexcept
    : db_(other.db_) {
    other.db_ = nullptr;
}

SQLiteDatabase& SQLiteDatabase::operator=(SQLiteDatabase&& other) noexcept {
    if (this != &other) {
        close();
        db_ = other.db_;
        other.db_ = nullptr;
    }
    return *this;
}

// ---------------------------------------------------------------------------
// Connection management
// ---------------------------------------------------------------------------

void SQLiteDatabase::open(const std::string& path) {
    if (db_) {
        throw DbException("open: connection already open — call close() first");
    }
    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string msg = db_ ? sqlite3_errmsg(db_) : "unknown error";
        sqlite3_close(db_);
        db_ = nullptr;
        throw DbException(makeErrMsg("open", rc, msg.c_str()));
    }
    // Enable WAL mode and foreign keys by default.
    execute("PRAGMA journal_mode=WAL");
    execute("PRAGMA foreign_keys=ON");
}

void SQLiteDatabase::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool SQLiteDatabase::isOpen() const {
    return db_ != nullptr;
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

void SQLiteDatabase::checkOpen(const char* ctx) const {
    if (!db_) {
        throw DbException(std::string(ctx) + ": database is not open");
    }
}

void SQLiteDatabase::throwIfError(int rc, const char* ctx) const {
    if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW) {
        throw DbException(makeErrMsg(ctx, rc,
            db_ ? sqlite3_errmsg(db_) : "no connection"));
    }
}

// ---------------------------------------------------------------------------
// execute / query
// ---------------------------------------------------------------------------

void SQLiteDatabase::execute(const std::string& sql) {
    checkOpen("execute");
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::string msg = errMsg ? errMsg : "unknown error";
        sqlite3_free(errMsg);
        throw DbException(makeErrMsg("execute", rc, msg.c_str()));
    }
}

DbResultSet SQLiteDatabase::query(const std::string& sql) {
    checkOpen("query");
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw DbException(makeErrMsg("query/prepare", rc, sqlite3_errmsg(db_)));
    }

    DbResultSet result;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int colCount = sqlite3_column_count(stmt);
        DbRow row;
        row.reserve(colCount);
        for (int i = 0; i < colCount; ++i) {
            const char* name = sqlite3_column_name(stmt, i);
            const char* val  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
            row.emplace_back(name ? name : "", val ? val : "");
        }
        result.push_back(std::move(row));
    }
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        throw DbException(makeErrMsg("query/step", rc, sqlite3_errmsg(db_)));
    }
    return result;
}

// ---------------------------------------------------------------------------
// Parameterized variants
// ---------------------------------------------------------------------------

void SQLiteDatabase::executeParams(const std::string& sql,
                                   const std::vector<std::string>& params) {
    checkOpen("executeParams");
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw DbException(makeErrMsg("executeParams/prepare", rc, sqlite3_errmsg(db_)));
    }

    for (int i = 0; i < static_cast<int>(params.size()); ++i) {
        rc = sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            throw DbException(makeErrMsg("executeParams/bind", rc, sqlite3_errmsg(db_)));
        }
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        throw DbException(makeErrMsg("executeParams/step", rc, sqlite3_errmsg(db_)));
    }
}

DbResultSet SQLiteDatabase::queryParams(const std::string& sql,
                                        const std::vector<std::string>& params) {
    checkOpen("queryParams");
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw DbException(makeErrMsg("queryParams/prepare", rc, sqlite3_errmsg(db_)));
    }

    for (int i = 0; i < static_cast<int>(params.size()); ++i) {
        rc = sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            throw DbException(makeErrMsg("queryParams/bind", rc, sqlite3_errmsg(db_)));
        }
    }

    DbResultSet result;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int colCount = sqlite3_column_count(stmt);
        DbRow row;
        row.reserve(colCount);
        for (int i = 0; i < colCount; ++i) {
            const char* name = sqlite3_column_name(stmt, i);
            const char* val  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
            row.emplace_back(name ? name : "", val ? val : "");
        }
        result.push_back(std::move(row));
    }
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        throw DbException(makeErrMsg("queryParams/step", rc, sqlite3_errmsg(db_)));
    }
    return result;
}

// ---------------------------------------------------------------------------
// Mixed (text + blob) parameterized variants
// ---------------------------------------------------------------------------

void SQLiteDatabase::executeMixed(const std::string& sql, const std::vector<DbParam>& params) {
    checkOpen("executeMixed");
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
        throw DbException(makeErrMsg("executeMixed/prepare", rc, sqlite3_errmsg(db_)));

    for (int i = 0; i < static_cast<int>(params.size()); ++i) {
        const auto& p = params[i];
        if (p.kind == DbParam::Kind::Text) {
            rc = sqlite3_bind_text(stmt, i + 1, p.text.c_str(), -1, SQLITE_TRANSIENT);
        } else if (p.kind == DbParam::Kind::Blob) {
            rc = sqlite3_bind_blob(stmt, i + 1, p.blob.data(), static_cast<int>(p.blob.size()), SQLITE_TRANSIENT);
        } else {
            rc = sqlite3_bind_null(stmt, i + 1);
        }
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            throw DbException(makeErrMsg("executeMixed/bind", rc, sqlite3_errmsg(db_)));
        }
    }
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW)
        throw DbException(makeErrMsg("executeMixed/step", rc, sqlite3_errmsg(db_)));
}

DbExtResultSet SQLiteDatabase::queryMixed(const std::string& sql, const std::vector<DbParam>& params) {
    checkOpen("queryMixed");
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
        throw DbException(makeErrMsg("queryMixed/prepare", rc, sqlite3_errmsg(db_)));

    for (int i = 0; i < static_cast<int>(params.size()); ++i) {
        const auto& p = params[i];
        if (p.kind == DbParam::Kind::Text) {
            rc = sqlite3_bind_text(stmt, i + 1, p.text.c_str(), -1, SQLITE_TRANSIENT);
        } else if (p.kind == DbParam::Kind::Blob) {
            rc = sqlite3_bind_blob(stmt, i + 1, p.blob.data(), static_cast<int>(p.blob.size()), SQLITE_TRANSIENT);
        } else {
            rc = sqlite3_bind_null(stmt, i + 1);
        }
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            throw DbException(makeErrMsg("queryMixed/bind", rc, sqlite3_errmsg(db_)));
        }
    }

    DbExtResultSet result;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int colCount = sqlite3_column_count(stmt);
        DbExtRow row;
        row.reserve(colCount);
        for (int i = 0; i < colCount; ++i) {
            const char* colName = sqlite3_column_name(stmt, i);
            DbExtValue val;
            int colType = sqlite3_column_type(stmt, i);
            if (colType == SQLITE_BLOB) {
                val.isBlobColumn = true;
                const void* blobData = sqlite3_column_blob(stmt, i);
                int blobSize = sqlite3_column_bytes(stmt, i);
                if (blobData && blobSize > 0) {
                    val.blob.assign(
                        static_cast<const uint8_t*>(blobData),
                        static_cast<const uint8_t*>(blobData) + blobSize);
                }
            } else if (colType != SQLITE_NULL) {
                const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
                val.text = text ? text : "";
            }
            row.emplace_back(colName ? colName : "", std::move(val));
        }
        result.push_back(std::move(row));
    }
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        throw DbException(makeErrMsg("queryMixed/step", rc, sqlite3_errmsg(db_)));
    return result;
}

// ---------------------------------------------------------------------------
// Transactions
// ---------------------------------------------------------------------------

void SQLiteDatabase::beginTransaction() {
    execute("BEGIN TRANSACTION");
}

void SQLiteDatabase::commit() {
    execute("COMMIT");
}

void SQLiteDatabase::rollback() {
    execute("ROLLBACK");
}

// ---------------------------------------------------------------------------
// Metadata
// ---------------------------------------------------------------------------

long long SQLiteDatabase::lastInsertRowId() const {
    checkOpen("lastInsertRowId");
    return sqlite3_last_insert_rowid(db_);
}

int SQLiteDatabase::changesCount() const {
    checkOpen("changesCount");
    return sqlite3_changes(db_);
}

} // namespace meos_db

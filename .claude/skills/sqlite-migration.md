# SQLite Migration Patterns

This skill provides patterns for implementing and managing SQLite migrations for MeOS Core.

## vcpkg Configuration

Always use the following target name and package name in CMake:
- **Package:** `unofficial-sqlite3`
- **Target:** `unofficial::sqlite3::sqlite3`

```cmake
find_package(unofficial-sqlite3 CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE unofficial::sqlite3::sqlite3)
```

## Migration Structure

Migrations should be versioned and applied sequentially. The `_migrations` table tracks the highest version applied.

Each `SchemaVN` struct extends SchemaV(N-1) by appending a new migration:

```cpp
std::vector<Migration> SchemaV3::migrations() {
    auto v = SchemaV2::migrations();
    v.push_back({ 3, "Description", R"sql(
        CREATE TABLE IF NOT EXISTS new_table (...);
        ALTER TABLE existing ADD COLUMN new_col INTEGER NOT NULL DEFAULT 0;
    )sql" });
    return v;
}
```

## Best Practices

- **Idempotency:** Always use `CREATE TABLE IF NOT EXISTS` or `ALTER TABLE ... ADD COLUMN ...` with appropriate checks.
- **Transactions:** Wrap each migration in a transaction.
- **SQLite Types:** `INTEGER` for IDs/ints/bools, `TEXT` for strings, `BLOB` for oData buffers.
- **Multi-statement migrations:** SQLite's `sqlite3_exec` handles multiple semicolon-separated statements in one call.

## NULL vs 0 for FK Columns

When domain uses 0 to mean "no reference" and the table has a FK constraint:
- Insert `DbParam::Null()` (not `0`) for FK columns with no reference — SQLite enforces FK on INSERT even with `ON DELETE SET NULL`.
- On read, SQLite returns NULL columns as empty string; use `parseIntOrZero(s)` helper to handle.

```cpp
static int parseIntOrZero(const std::string& s) {
    return s.empty() ? 0 : std::stoi(s);
}
```

## Repository Pattern (DAOs)

MeOS uses the Repository pattern. Existing pattern uses `DbParam` / `executeMixed` / `queryMixed`:

```cpp
// Insert with FK that allows NULL
int clubId = runner.getClubId();
std::vector<DbParam> params{
    DbParam::Text(std::to_string(runner.getId())),
    clubId > 0 ? DbParam::Text(std::to_string(clubId)) : DbParam::Null(),
    DbParam::Blob(reinterpret_cast<const uint8_t*>(runner.getOData()), oRunner::getODataBlobSize())
};
db_.executeMixed("INSERT OR REPLACE INTO runners (id, club_id, odata_blob) VALUES (?, ?, ?)", params);
```

## Entity-Specific Persistence

- **oClub, oControl, oCourse, oClass, oRunner:** Use `getOData()` + `getODataBlobSize()` for blob persistence. Add public accessors if missing from protected section.
- **oCard:** Uses punch_string serialization — call `getPunchString()` / `importPunches()`. `getDISize()` returns -1; never use blob-based persistence for oCard.
- **oFreePunch:** Simple fields only (card_no, type_code, time_int, runner_id) — no oData blob.

## Public Accessor Pattern

When `oData` is in a `protected:` section, add public getters to the `public:` section:

```cpp
// In oClass.h public section:
const BYTE* getOData() const { return oData; }
BYTE* getOData() { return oData; }
static int getODataBlobSize() { return dataSize; }
int getNumLegs() const { return (int)legInfo.size(); }
```

**CRITICAL:** Ensure these are placed after a `public:` keyword, not within `protected:` block.

## Test Patterns

- FK tests: Always persist referenced entities to DB before inserting the referencing entity.
- Use SchemaVN::migrations() in `openWithSchema()` helper per test suite.
- `oe.addClub()` only adds to in-memory oEvent — you must also call `clubRepo.insert(*club)` for SQLite FK satisfaction.


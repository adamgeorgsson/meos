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

```cpp
bool SQLiteDatabase::applyMigration(int version) {
    if (version == 1) {
        const std::string sql1 = "CREATE TABLE IF NOT EXISTS ...";
        const std::string sql2 = "INSERT INTO _migrations (version) VALUES (1);";
        
        if (!execute("BEGIN TRANSACTION;")) return false;
        if (!execute(sql1)) { execute("ROLLBACK;"); return false; }
        if (!execute(sql2)) { execute("ROLLBACK;"); return false; }
        if (!execute("COMMIT;")) return false;
        return true;
    }
    return false;
}
```

## Best Practices

- **Idempotency:** Always use `CREATE TABLE IF NOT EXISTS` or `ALTER TABLE ... ADD COLUMN ...` with appropriate checks.
- **Transactions:** Wrap each migration (multiple SQL statements) in a transaction to ensure database consistency.
- **SQLite Types:**
  - `INTEGER`: Use for IDs, Booleans, and timestamps.
  - `TEXT`: Use for strings (including wide strings after narrowing).
  - `REAL`: Use for floating point values.
  - `BLOB`: Use for raw binary data, specifically the `oData` buffer from MeOS domain entities.

## Repository Pattern (DAOs)

MeOS uses the Repository pattern to map domain objects to SQLite tables. This keeps persistence logic separate from domain logic.

- **Mapping Core Fields:** Map important domain properties (name, ID, status) to explicit SQL columns for efficient querying and REST API access.
- **Full State Persistence:** Store the raw `oData` buffer (from `oDataContainer`) as a `BLOB` column. This ensures all configuration and extra fields are persisted without needing a complex schema.
- **Narrow/Widen:** Use `MeOSUtil::narrow` when saving strings and `MeOSUtil::widen` when loading to ensure UTF-8 compatibility in the database.
- **Protected Member Access:** If the `oData` buffer or `dataSize` are `protected` in the domain class, add public getters (`getData()`, `getDataSize()`) to allow the repository to access them without complex friend declarations.

### Example Repository Implementation

```cpp
bool Repository::save(const oEntity& entity) {
    const char* sql = "INSERT OR REPLACE INTO entities (id, name, data) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt;
    // ... prepare and bind id, name ...
    sqlite3_bind_blob(stmt, 3, entity.getData(), entity.getDataSize(), SQLITE_TRANSIENT);
    // ... step and finalize ...
}

pEntity Repository::get(int id, oEvent* oe) {
    // ... select ...
    const void* blob = sqlite3_column_blob(stmt, 1);
    int blobSize = sqlite3_column_bytes(stmt, 1);
    if (blob && blobSize == oEntity::getDataSize()) {
        memcpy(entity->getData(), blob, oEntity::getDataSize());
        memcpy(entity->getDataOld(), blob, oEntity::getDataSize());
    }
}
```

## V2 Schema Expansion
- Use `ALTER TABLE ... ADD COLUMN ...` to add columns to existing tables in new migration versions.
- Keep the `_migrations` table updated with the current version.

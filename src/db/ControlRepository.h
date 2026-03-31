#pragma once
#include "SQLiteDatabase.h"
#include "../domain/oControl.h"
#include <string>
#include <vector>
#include <optional>

namespace meos_db {

struct ControlRow {
    int id = 0;
    std::string name;    ///< UTF-8
    std::string numbers; ///< semicolon-separated, UTF-8
    int status = 0;      ///< ControlStatus as int
    std::vector<uint8_t> odata;
};

class ControlRepository {
public:
    explicit ControlRepository(SQLiteDatabase& db) : db_(db) {}

    void insert(const oControl& ctrl);
    void update(const oControl& ctrl);
    void remove(int id);
    std::optional<ControlRow> findById(int id) const;
    std::vector<ControlRow> findAll() const;

private:
    SQLiteDatabase& db_;
};

} // namespace meos_db

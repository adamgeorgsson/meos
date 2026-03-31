#pragma once
#include "SQLiteDatabase.h"
#include "../domain/oTeam.h"
#include <string>
#include <vector>
#include <optional>

namespace meos_db {

struct TeamRow {
    int id = 0;
    std::string name;        ///< UTF-8 encoded
    int clubId = 0;
    int classId = 0;
    int startNo = 0;
    int startTime = 0;
    int finishTime = 0;
    int status = 0;
    std::string runnerIds;   ///< Semicolon-separated runner IDs (e.g. "3;7;0;")
    std::vector<uint8_t> odata;
};

class TeamRepository {
public:
    explicit TeamRepository(SQLiteDatabase& db) : db_(db) {}

    void insert(const oTeam& team);
    void update(const oTeam& team);
    void remove(int id);
    std::optional<TeamRow> findById(int id) const;
    std::vector<TeamRow> findAll() const;

private:
    SQLiteDatabase& db_;

    static TeamRow rowFromResult(const DbExtRow& row);
};

} // namespace meos_db

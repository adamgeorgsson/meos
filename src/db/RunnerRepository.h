#pragma once
#include "SQLiteDatabase.h"
#include "../domain/oRunner.h"
#include <string>
#include <vector>
#include <optional>

namespace meos_db {

struct RunnerRow {
    int id = 0;
    std::string name;           ///< UTF-8 encoded
    int clubId = 0;
    int classId = 0;
    int courseId = 0;
    int cardNo = 0;
    int startNo = 0;
    int startTime = 0;
    int finishTime = 0;
    int status = 0;
    std::vector<uint8_t> odata; ///< Raw oData blob
};

class RunnerRepository {
public:
    explicit RunnerRepository(SQLiteDatabase& db) : db_(db) {}

    void insert(const oRunner& runner);
    void update(const oRunner& runner);
    void remove(int id);
    std::optional<RunnerRow> findById(int id) const;
    std::vector<RunnerRow> findAll() const;

private:
    SQLiteDatabase& db_;
};

} // namespace meos_db

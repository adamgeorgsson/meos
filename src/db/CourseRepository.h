#pragma once
#include "SQLiteDatabase.h"
#include "../domain/oCourse.h"
#include <string>
#include <vector>
#include <optional>

namespace meos_db {

struct CourseRow {
    int id = 0;
    std::string name;        ///< UTF-8
    int length = 0;
    std::string controlIds;  ///< comma-separated, UTF-8
    std::string legLengths;  ///< semicolon-separated
    int startControlId = 0;
    int finishControlId = 0;
    std::vector<uint8_t> odata;
};

class CourseRepository {
public:
    explicit CourseRepository(SQLiteDatabase& db) : db_(db) {}

    void insert(const oCourse& course);
    void update(const oCourse& course);
    void remove(int id);
    std::optional<CourseRow> findById(int id) const;
    std::vector<CourseRow> findAll() const;

private:
    SQLiteDatabase& db_;
};

} // namespace meos_db

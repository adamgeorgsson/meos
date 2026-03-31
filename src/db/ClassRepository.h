#pragma once
#include "SQLiteDatabase.h"
#include "../domain/oClass.h"
#include <string>
#include <vector>
#include <optional>

namespace meos_db {

struct ClassRow {
    int id = 0;
    std::string name;           ///< UTF-8 encoded
    int courseId = 0;
    int numLegs = 1;
    std::vector<uint8_t> odata; ///< Raw oData blob
};

class ClassRepository {
public:
    explicit ClassRepository(SQLiteDatabase& db) : db_(db) {}

    void insert(const oClass& cls);
    void update(const oClass& cls);
    void remove(int id);
    std::optional<ClassRow> findById(int id) const;
    std::vector<ClassRow> findAll() const;

private:
    SQLiteDatabase& db_;
};

} // namespace meos_db

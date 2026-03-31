#pragma once
#include "SQLiteDatabase.h"
#include "../domain/oFreePunch.h"
#include <string>
#include <vector>
#include <optional>

namespace meos_db {

struct FreePunchRow {
    int id = 0;
    int cardNo = 0;
    int typeCode = 0;
    int timeInt = 0;
    int runnerId = 0;
};

class FreePunchRepository {
public:
    explicit FreePunchRepository(SQLiteDatabase& db) : db_(db) {}

    void insert(const oFreePunch& punch);
    void update(const oFreePunch& punch);
    void remove(int id);
    std::optional<FreePunchRow> findById(int id) const;
    std::vector<FreePunchRow> findAll() const;

private:
    SQLiteDatabase& db_;
};

} // namespace meos_db

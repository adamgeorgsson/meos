#pragma once
#include "SQLiteDatabase.h"
#include "../domain/oCard.h"
#include <string>
#include <vector>
#include <optional>

namespace meos_db {

struct CardRow {
    int id = 0;
    int cardNo = 0;
    int runnerId = 0;       ///< 0 if no owner
    std::string punchString; ///< Serialized punch data
};

class CardRepository {
public:
    explicit CardRepository(SQLiteDatabase& db) : db_(db) {}

    void insert(const oCard& card);
    void update(const oCard& card);
    void remove(int id);
    std::optional<CardRow> findById(int id) const;
    std::vector<CardRow> findAll() const;

private:
    SQLiteDatabase& db_;
};

} // namespace meos_db

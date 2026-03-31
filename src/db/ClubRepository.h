#pragma once
#include "SQLiteDatabase.h"
#include "../domain/oClub.h"
#include <string>
#include <vector>
#include <optional>

namespace meos_db {

struct ClubRow {
    int id = 0;
    std::string name;           ///< UTF-8 encoded
    std::vector<uint8_t> odata; ///< Raw oData blob
};

class ClubRepository {
public:
    explicit ClubRepository(SQLiteDatabase& db) : db_(db) {}

    /// Insert a club. The club must have getId() > 0.
    void insert(const oClub& club);

    /// Update an existing club.
    void update(const oClub& club);

    /// Delete club by ID.
    void remove(int id);

    /// Find club by ID. Returns nullopt if not found.
    std::optional<ClubRow> findById(int id) const;

    /// Return all clubs.
    std::vector<ClubRow> findAll() const;

private:
    SQLiteDatabase& db_;
};

} // namespace meos_db

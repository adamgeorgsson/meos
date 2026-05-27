#pragma once

#include "database.h"

namespace meos::db {

// Seeds the database with test data matching the frontend mocks (src/ui/web/src/mocks/db.ts).
// No-op if the competitions table already contains data.
void seedIfEmpty(Database& db);

}  // namespace meos::db

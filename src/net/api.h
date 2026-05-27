#pragma once

#include <httplib.h>

#include "database.h"

namespace meos::net {

// Registers GET /api/v1/clubs and GET /api/v1/clubs/{id} on svr.
void registerClubsRoutes(httplib::Server& svr, meos::db::Database& db);

}  // namespace meos::net

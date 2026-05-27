#pragma once

#include <httplib.h>

#include "database.h"

namespace meos::net {

// Registers GET /api/v1/clubs and GET /api/v1/clubs/{id} on svr.
void registerClubsRoutes(httplib::Server& svr, meos::db::Database& db);

// Registers GET /api/v1/controls and GET /api/v1/controls/{id} on svr.
void registerControlsRoutes(httplib::Server& svr, meos::db::Database& db);

// Registers GET /api/v1/courses and GET /api/v1/courses/{id} on svr.
void registerCoursesRoutes(httplib::Server& svr, meos::db::Database& db);

// Registers GET /api/v1/classes and GET /api/v1/classes/{id} on svr.
void registerClassesRoutes(httplib::Server& svr, meos::db::Database& db);

// Registers GET /api/v1/runners and GET /api/v1/runners/{id} on svr.
void registerRunnersRoutes(httplib::Server& svr, meos::db::Database& db);

// Registers GET /api/v1/teams and GET /api/v1/teams/{id} on svr.
void registerTeamsRoutes(httplib::Server& svr, meos::db::Database& db);

// Registers GET /api/v1/competitions on svr (returns singleton competition object).
void registerCompetitionsRoutes(httplib::Server& svr, meos::db::Database& db);

}  // namespace meos::net

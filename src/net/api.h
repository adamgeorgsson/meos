#pragma once

#include <httplib.h>

#include "database.h"

namespace meos::net {

// Registers GET /api/v1/clubs and GET /api/v1/clubs/{id} on svr.
void registerClubsRoutes(httplib::Server& svr, meos::db::Database& db);

// Registers GET /api/v1/controls and GET /api/v1/controls/{id} on svr.
void registerControlsRoutes(httplib::Server& svr, meos::db::Database& db);

// Registers full CRUD for /api/v1/courses and /api/v1/courses/{id}.
// GET returns controlIds as a JSON array. POST/PUT accept {name, length?, controls[]}.
void registerCoursesRoutes(httplib::Server& svr, meos::db::Database& db);

// Registers full CRUD for /api/v1/classes and /api/v1/classes/{id}.
// courseId: 0 in PUT/POST clears the course assignment.
void registerClassesRoutes(httplib::Server& svr, meos::db::Database& db);

// Registers GET /api/v1/runners and GET /api/v1/runners/{id} on svr.
void registerRunnersRoutes(httplib::Server& svr, meos::db::Database& db);

// Registers GET /api/v1/teams and GET /api/v1/teams/{id} on svr.
void registerTeamsRoutes(httplib::Server& svr, meos::db::Database& db);

// Registers GET /api/v1/competitions on svr (returns singleton competition object).
void registerCompetitionsRoutes(httplib::Server& svr, meos::db::Database& db);

// Registers GET /api/v1/results and GET /api/v1/results/{id} on svr.
void registerResultsRoutes(httplib::Server& svr, meos::db::Database& db);

// Registers GET /api/v1/startlist and GET /api/v1/startlist/{id} on svr.
void registerStartListRoutes(httplib::Server& svr, meos::db::Database& db);

// Registers IOF 3.0 XML export endpoints:
//   GET /api/v1/results/export/xml   -> ResultList XML
//   GET /api/v1/startlist/export/xml -> StartList XML
void registerXmlExportRoutes(httplib::Server& svr, meos::db::Database& db);

}  // namespace meos::net

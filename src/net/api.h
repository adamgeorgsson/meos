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

// Registers full CRUD for /api/v1/runners and /api/v1/runners/{id}.
// GET supports filtering (?name=, ?clubId=, ?classId=) and pagination (?page=, ?pageSize=).
// Response envelope for GET list: {data, total, page, pageSize}.
// POST /api/v1/runners/{id}/status allows manual status changes (ok/dns/dnf/dq/mp/nc/inactive).
void registerRunnersRoutes(httplib::Server& svr, meos::db::Database& db);

// Registers full CRUD for /api/v1/teams and /api/v1/teams/{id}.
// GET supports filtering (?name=, ?clubId=, ?classId=) and pagination (?page=, ?pageSize=).
// Response envelope for GET list: {data, total, page, pageSize}.
void registerTeamsRoutes(httplib::Server& svr, meos::db::Database& db);

// Registers GET and PUT /api/v1/competitions for singleton competition metadata.
void registerCompetitionsRoutes(httplib::Server& svr, meos::db::Database& db);

// Registers GET /api/v1/results and GET /api/v1/results/{id} on svr.
void registerResultsRoutes(httplib::Server& svr, meos::db::Database& db);

// Registers GET /api/v1/startlist and GET /api/v1/startlist/{id} on svr.
// Entries are sorted by classId then startTime.
void registerStartListRoutes(httplib::Server& svr, meos::db::Database& db);

// Registers GET and POST /api/v1/cards and GET /api/v1/cards/{id}.
// POST stores a card readout and auto-links it to a runner by cardNumber.
void registerCardsRoutes(httplib::Server& svr, meos::db::Database& db);

// Registers IOF 3.0 XML export endpoints:
//   GET /api/v1/results/export/xml   -> ResultList XML
//   GET /api/v1/startlist/export/xml -> StartList XML
void registerXmlExportRoutes(httplib::Server& svr, meos::db::Database& db);

}  // namespace meos::net

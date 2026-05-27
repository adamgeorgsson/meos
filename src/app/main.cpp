#include <csignal>
#include <iostream>

#include "api.h"
#include "database.h"
#include "http_server.h"
#include "seed.h"

static meos::net::HttpServer* g_server = nullptr;

static void signalHandler(int /*sig*/) {
    if (g_server) {
        g_server->stop();
    }
}

int main() {
    meos::db::Database db("meos.sqlite");
    db.createTables();
    meos::db::seedIfEmpty(db);

    meos::net::HttpServer server(2009);
    g_server = &server;

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    auto& svr = server.server();
    meos::net::registerClubsRoutes(svr, db);
    meos::net::registerControlsRoutes(svr, db);
    meos::net::registerCoursesRoutes(svr, db);
    meos::net::registerClassesRoutes(svr, db);
    meos::net::registerRunnersRoutes(svr, db);
    meos::net::registerTeamsRoutes(svr, db);
    meos::net::registerCompetitionsRoutes(svr, db);
    meos::net::registerResultsRoutes(svr, db);
    meos::net::registerStartListRoutes(svr, db);
    meos::net::registerXmlExportRoutes(svr, db);
    server.serveStaticFiles("src/ui/web/dist");

    std::cout << "MeOS server listening on http://localhost:2009\n";
    server.listen();

    return 0;
}


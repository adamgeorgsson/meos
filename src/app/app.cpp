#include "app.h"

#include <csignal>
#include <iostream>
#include <string>
#include <thread>

#include "api.h"
#include "database.h"
#include "http_server.h"

namespace meos {

namespace {

// Parse --flag <value> pairs from argv. Returns the value if the flag is
// found, otherwise returns the provided default.
std::string parseArg(int argc, char* argv[], const std::string& flag,
                     const std::string& def) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == flag) {
            return argv[i + 1];
        }
    }
    return def;
}

// Combine all versioned migrations in dependency order (V1 → V4).
std::vector<meos::db::Migration> allMigrations() {
    auto m = meos::db::Database::v1Migrations();
    auto m2 = meos::db::Database::v2Migrations();
    auto m3 = meos::db::Database::v3Migrations();
    auto m4 = meos::db::Database::v4Migrations();
    m.insert(m.end(), m2.begin(), m2.end());
    m.insert(m.end(), m3.begin(), m3.end());
    m.insert(m.end(), m4.begin(), m4.end());
    return m;
}

static meos::net::HttpServer* g_server = nullptr;

void signalHandler(int /*sig*/) {
    if (g_server) {
        g_server->stop();
    }
}

}  // namespace

int MeosApp::run(int argc, char* argv[]) {
    const int port = std::stoi(parseArg(argc, argv, "--port", "8080"));
    const std::string dbPath = parseArg(argc, argv, "--db", "meos.sqlite");
    const std::string webRoot = parseArg(argc, argv, "--web-root", "web");

    // Open database and apply all schema migrations.
    meos::db::Database db(dbPath);
    db.applyMigrations(allMigrations());

    // Set up HTTP server.
    meos::net::HttpServer server(port);
    g_server = &server;

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Register all API route groups.
    auto& svr = server.server();
    meos::net::registerClubsRoutes(svr, db);
    meos::net::registerControlsRoutes(svr, db);
    meos::net::registerCoursesRoutes(svr, db);
    meos::net::registerClassesRoutes(svr, db);
    meos::net::registerRunnersRoutes(svr, db);
    meos::net::registerTeamsRoutes(svr, db);
    meos::net::registerCompetitionsRoutes(svr, db);
    meos::net::registerCardsRoutes(svr, db);
    meos::net::registerResultsRoutes(svr, db);
    meos::net::registerStartListRoutes(svr, db);
    meos::net::registerXmlExportRoutes(svr, db);

    // Serve React SPA (call after API routes so the catch-all is last).
    server.serveStaticFiles(webRoot);

    std::cout << "MeOS server listening on http://localhost:" << port << "/\n";
    std::cout << "Press Enter to stop.\n";

    // Start the server in a background thread so we can wait for Enter.
    std::thread serverThread([&server]() { server.listen(); });

    std::cin.get();
    server.stop();

    if (serverThread.joinable()) {
        serverThread.join();
    }

    g_server = nullptr;
    return 0;
}

}  // namespace meos

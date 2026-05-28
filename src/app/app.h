#pragma once

#include <string>

namespace meos {

// Application entry point. Owns all runtime state (DB, HTTP server).
// Parses command-line arguments, applies migrations, registers API routes,
// starts the HTTP server, and waits for shutdown.
class MeosApp {
public:
    // Run the application. Returns 0 on clean exit, non-zero on error.
    // Supported CLI flags:
    //   --port <n>       HTTP port (default: 8080)
    //   --db   <path>    SQLite database file (default: meos.sqlite)
    //   --web-root <dir> Static file directory (default: web)
    int run(int argc, char* argv[]);
};

}  // namespace meos

# PRD: JSON REST API Backend (Read-Only)

## Introduction

The web frontend (`src/ui/web/`) currently uses MSW (Mock Service Worker) to simulate a JSON REST API during development. All data is fake, hardcoded in `src/ui/web/src/mocks/db.ts`. This PRD describes building a real C++ HTTP server in the modern `src/` structure that serves JSON at `/api/v1/*`, matching the exact contract the frontend already expects. The server also serves the React SPA's static files for a complete single-binary experience.

This is read-only (GET endpoints) as a first milestone. Write endpoints (POST/PUT/DELETE) will follow in a separate PRD.

### What exists today

- **Frontend**: React + TypeScript SPA in `src/ui/web/` with 9 pages, generic CRUD hooks, MSW mock handlers defining the full API contract
- **Vite proxy**: Already configured to forward `/api` to `http://localhost:2009`
- **Build infra**: CMake 3.28 + vcpkg + Ninja + GTest in place, stub `src/main.cpp`
- **Legacy server**: `code/restserver.cpp` serves XML at `/meos` on port 2009 — we do NOT modify this

### Target State

- New C++ HTTP server in `src/` serving JSON at `/api/v1/*` on port 2009
- SQLite database with seed data matching the frontend's mock data
- React SPA served from the same server (static file serving + SPA fallback)
- Frontend toggleable between MSW mocks and real backend via env variable

## Goals

- Serve real competition data to the web frontend from a C++ backend
- Establish the modular `src/` architecture (util, domain, db, net modules)
- Match the exact JSON response format the frontend expects (no envelope wrapper)
- Auto-seed the database with test data on first run
- Serve the React SPA build from the C++ server

## User Stories

### US-001: Modular Source Layout

**Description:** As a developer, I need the `src/` directory organized into modules so that code is maintainable and each layer has clear responsibilities.

**Acceptance Criteria:**
- [ ] Directory structure exists: `src/util/`, `src/domain/`, `src/db/`, `src/net/`, `src/app/`
- [ ] Each module has its own `CMakeLists.txt` producing a static library (except app which produces the executable)
- [ ] Top-level `CMakeLists.txt` includes all modules via `add_subdirectory`
- [ ] Dependency graph is acyclic: `util` <- `domain` <- `db` <- `net` <- `app`
- [ ] `cmake --preset default && cmake --build build` succeeds
- [ ] `ctest --test-dir build` passes

### US-002: Utility Module

**Description:** As a developer, I need string and time utilities so that the backend can convert between internal representations and JSON-friendly formats.

**Acceptance Criteria:**
- [ ] `src/util/string_util.h/.cpp` provides `toUTF8(wstring) -> string` and `fromUTF8(string) -> wstring`
- [ ] `src/util/time_util.h/.cpp` provides `formatTimeHMS(int seconds) -> string` (returns `"HH:MM:SS"`) and `parseTimeHMS(string) -> int` (parses `"HH:MM:SS"` to seconds)
- [ ] Unit tests verify round-trip conversion for strings with Swedish characters (e.g., "IF Berget", "Bjornparken, Stockholm")
- [ ] Unit tests verify time formatting: `formatTimeHMS(36000) == "10:00:00"`, `parseTimeHMS("10:02:00") == 36120`
- [ ] Typecheck and build pass

### US-003: Domain Entities

**Description:** As a developer, I need C++ structs matching the frontend's TypeScript types so that serialization is straightforward.

**Acceptance Criteria:**
- [ ] `src/domain/entities.h` defines structs: `Competition`, `Club`, `Control`, `Course`, `Class`, `Runner`, `Team`, `SplitTime`, `Result`, `StartListEntry`
- [ ] Field names and types match `src/ui/web/src/types/index.ts` exactly (using `std::string` for strings, `int` for numbers, `std::vector<int>` for arrays, `std::optional` for optional fields)
- [ ] `src/domain/entities.h` is usable from all other modules
- [ ] Build passes

### US-004: SQLite Database Layer

**Description:** As a developer, I need a database abstraction that creates tables, seeds test data, and provides read access for all entities.

**Acceptance Criteria:**
- [ ] `src/db/database.h/.cpp` provides a `Database` class that opens/creates a SQLite file
- [ ] `Database::createTables()` creates tables for all entities (competitions, clubs, controls, courses, course_controls, classes, runners, teams, team_members, results, splits, start_list)
- [ ] `PRAGMA foreign_keys=ON` and `PRAGMA journal_mode=WAL` are set on connection
- [ ] Read methods exist: `getAllClubs() -> vector<Club>`, `getClubById(int) -> optional<Club>`, and equivalent for all entities
- [ ] For `Course`: `getAllCourses()` joins `course_controls` to populate the `controls` vector
- [ ] For `Team`: `getAllTeams()` joins `team_members` to populate the `members` vector
- [ ] For `Result`: `getAllResults()` joins `splits` to populate the `splits` vector
- [ ] `sqlite3` is added to `vcpkg.json` and linked in CMake
- [ ] Unit tests verify CRUD operations against an in-memory SQLite database (`:memory:`)
- [ ] Build and tests pass

### US-005: Database Seed Data

**Description:** As a user, I want the server to start with test data so that the frontend shows real content immediately.

**Acceptance Criteria:**
- [ ] `src/db/seed.h/.cpp` provides a `seedIfEmpty(Database&)` function
- [ ] If the competitions table is empty, seed data is inserted matching `src/ui/web/src/mocks/db.ts` exactly: 1 competition, 5 clubs, 7 controls, 5 courses (with course_controls), 5 classes, 6 runners, 5 teams (with team_members), 6 results (with splits), 6 start list entries
- [ ] If the database already has data, seeding is skipped
- [ ] Entity IDs in seed data match the mock IDs (e.g., club "IF Berget" has id=1)
- [ ] Build passes

### US-006: HTTP Server with CORS

**Description:** As a developer, I need an HTTP server that handles CORS and serves as the foundation for the API and SPA.

**Acceptance Criteria:**
- [ ] `src/net/http_server.h/.cpp` wraps `httplib::Server`
- [ ] All responses include `Access-Control-Allow-Origin: *`
- [ ] OPTIONS requests on `/api/*` return 204 with CORS headers (`Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS`, `Access-Control-Allow-Headers: Content-Type`)
- [ ] `cpp-httplib` is added to `vcpkg.json` and linked in CMake
- [ ] Server is configurable for port (default 2009)
- [ ] Build passes

### US-007: JSON API — Clubs Endpoint

**Description:** As a frontend user, I want to see real club data on the Clubs page.

**Acceptance Criteria:**
- [ ] `GET /api/v1/clubs` returns JSON array of all clubs: `[{"id":1,"name":"IF Berget","country":"SE"}, ...]`
- [ ] `GET /api/v1/clubs/{id}` returns single club object or 404
- [ ] Response `Content-Type` is `application/json`
- [ ] Response body is a plain array/object (no `{"data":...}` envelope) — matching the MSW mock format
- [ ] `nlohmann-json` is added to `vcpkg.json` and linked in CMake
- [ ] Integration test: start server on test port, HTTP GET, verify JSON structure
- [ ] Build and tests pass

### US-008: JSON API — Controls Endpoint

**Description:** As a frontend user, I want to see real control data on the Controls page.

**Acceptance Criteria:**
- [ ] `GET /api/v1/controls` returns JSON array: `[{"id":1,"code":101,"description":"Fork junction","type":"normal"}, ...]`
- [ ] `GET /api/v1/controls/{id}` returns single control or 404
- [ ] Build and tests pass

### US-009: JSON API — Courses Endpoint

**Description:** As a frontend user, I want to see real course data on the Courses page.

**Acceptance Criteria:**
- [ ] `GET /api/v1/courses` returns JSON array: `[{"id":1,"name":"Long","length":12500,"controls":[6,1,2,3,4,5,7]}, ...]`
- [ ] `GET /api/v1/courses/{id}` returns single course with controls array, or 404
- [ ] The `controls` field is an ordered array of control IDs matching the course's control sequence
- [ ] Build and tests pass

### US-010: JSON API — Classes Endpoint

**Description:** As a frontend user, I want to see real class data on the Classes page.

**Acceptance Criteria:**
- [ ] `GET /api/v1/classes` returns JSON array: `[{"id":1,"name":"H21E","courseId":1,"startMethod":"individual"}, ...]`
- [ ] `GET /api/v1/classes/{id}` returns single class or 404
- [ ] Build and tests pass

### US-011: JSON API — Runners Endpoint

**Description:** As a frontend user, I want to see real runner data on the Runners page.

**Acceptance Criteria:**
- [ ] `GET /api/v1/runners` returns JSON array: `[{"id":1,"name":"Anna Lindstrom","clubId":1,"classId":2,"startTime":"10:00:00","cardNumber":2001234,"status":"ok"}, ...]`
- [ ] `GET /api/v1/runners/{id}` returns single runner or 404
- [ ] Optional fields (clubId, classId, startTime, cardNumber, status) are included only when non-null/non-zero
- [ ] Build and tests pass

### US-012: JSON API — Teams Endpoint

**Description:** As a frontend user, I want to see real team data on the Teams page.

**Acceptance Criteria:**
- [ ] `GET /api/v1/teams` returns JSON array: `[{"id":1,"name":"Berget Red","clubId":1,"classId":1,"members":[2]}, ...]`
- [ ] `GET /api/v1/teams/{id}` returns single team with members array, or 404
- [ ] Build and tests pass

### US-013: JSON API — Competition Endpoint

**Description:** As a frontend user, I want to see real competition info on the Competition page.

**Acceptance Criteria:**
- [ ] `GET /api/v1/competitions` returns the competition object: `{"id":1,"name":"Spring Cup 2026","date":"2026-05-15","organizer":"IF Berget","location":"Bjornparken, Stockholm","description":"Annual spring orienteering competition"}`
- [ ] This is a singleton endpoint (returns object, not array)
- [ ] Build and tests pass

### US-014: JSON API — Results Endpoint

**Description:** As a frontend user, I want to see real results on the Results page.

**Acceptance Criteria:**
- [ ] `GET /api/v1/results` returns JSON array: `[{"id":1,"runnerId":1,"classId":2,"position":1,"totalTime":4512,"status":"ok","splits":[{"controlId":1,"time":1234},{"controlId":2,"time":2456}]}, ...]`
- [ ] Results with no position/totalTime (e.g., DNS/DNF) omit those fields or set them to null
- [ ] Build and tests pass

### US-015: JSON API — Start List Endpoint

**Description:** As a frontend user, I want to see real start list data on the Start List page.

**Acceptance Criteria:**
- [ ] `GET /api/v1/startlist` returns JSON array: `[{"id":1,"runnerId":1,"classId":2,"startTime":"10:00:00","bib":1}, ...]`
- [ ] Build and tests pass

### US-016: XML Export Endpoints

**Description:** As a frontend user, I want to export results and start lists as IOF XML.

**Acceptance Criteria:**
- [ ] `GET /api/v1/results/export/xml` returns IOF 3.0 ResultList XML with `Content-Type: application/xml`
- [ ] `GET /api/v1/startlist/export/xml` returns IOF 3.0 StartList XML with `Content-Type: application/xml`
- [ ] XML includes the proper IOF 3.0 namespace: `http://www.orienteering.org/datastandard/3.0`
- [ ] Build and tests pass

### US-017: SPA Static File Serving

**Description:** As a user, I want to open `http://localhost:2009` in my browser and see the full React application.

**Acceptance Criteria:**
- [ ] The server serves static files from a configurable directory (default: `src/ui/web/dist/`)
- [ ] `GET /` serves `index.html`
- [ ] `GET /assets/foo.js` serves the corresponding file with correct Content-Type
- [ ] Any GET request not matching `/api/*` and not matching a static file returns `index.html` (SPA fallback for client-side routing)
- [ ] Build passes

### US-018: Application Entry Point

**Description:** As a user, I want to run a single binary that starts the server with a seeded database.

**Acceptance Criteria:**
- [ ] `src/app/main.cpp` replaces the stub `src/main.cpp`
- [ ] On startup: opens/creates `meos.sqlite`, runs `createTables()`, runs `seedIfEmpty()`, registers all API routes, starts HTTP server on port 2009
- [ ] Server logs startup message to stdout: `MeOS server listening on http://localhost:2009`
- [ ] Ctrl+C gracefully stops the server
- [ ] `cmake --preset default && cmake --build build` produces a working `meos` binary
- [ ] `./build/meos` starts and responds to `curl http://localhost:2009/api/v1/clubs`

### US-019: Frontend Backend Toggle

**Description:** As a developer, I want to easily switch between MSW mocks and the real backend during development.

**Acceptance Criteria:**
- [ ] `src/ui/web/src/main.tsx` checks for `VITE_USE_BACKEND` env variable
- [ ] When `VITE_USE_BACKEND` is set (any truthy value), MSW worker is NOT started — requests go to the real backend via Vite proxy
- [ ] When `VITE_USE_BACKEND` is not set (default), MSW starts as before — no behavior change for frontend-only development
- [ ] `npm test` still passes (tests always use MSW regardless of env var)
- [ ] `npm run typecheck` and `npm run lint` pass

## Functional Requirements

- FR-1: HTTP server uses `cpp-httplib`, listens on configurable port (default 2009)
- FR-2: All `/api/v1/*` responses are `Content-Type: application/json` (except XML export endpoints)
- FR-3: JSON responses are plain arrays or objects — no `{"data":...}` wrapper. Must match `src/ui/web/src/mocks/handlers.ts` format exactly
- FR-4: All responses include `Access-Control-Allow-Origin: *` header
- FR-5: 404 responses return `{"message":"Not found","status":404}` matching the `ApiError` TypeScript type
- FR-6: SQLite database file is `meos.sqlite` in working directory
- FR-7: Database is auto-seeded on first run with data matching `src/ui/web/src/mocks/db.ts`
- FR-8: `nlohmann-json` used for JSON serialization. Optional fields serialized only when present (not as null)
- FR-9: Path parameters use cpp-httplib regex: `R"(/api/v1/clubs/(\d+))"`
- FR-10: SPA fallback: non-API GET requests that don't match a static file return `index.html`
- FR-11: IOF XML export endpoints return minimal valid IOF 3.0 XML with correct namespace

## Non-Goals

- Write endpoints (POST/PUT/DELETE) — separate PRD
- Connection to legacy MySQL database
- Authentication or authorization
- WebSocket or real-time updates
- Data import from legacy MeOS format
- Full IOF 3.0 XML with complete competition data (minimal placeholder is sufficient for now)
- Migration of domain logic from legacy `code/` — these are new, simple structs

## Technical Considerations

- **Libraries**: cpp-httplib (HTTP), nlohmann-json (JSON), sqlite3 (database) — all via vcpkg
- **CMake patterns**: `find_package(httplib CONFIG REQUIRED)`, `find_package(nlohmann_json CONFIG REQUIRED)`, `find_package(unofficial-sqlite3 CONFIG REQUIRED)` (note: sqlite3 vcpkg uses `unofficial-sqlite3` target)
- **Threading**: cpp-httplib runs its own thread pool. SQLite accessed only from request handlers (single-threaded access is fine with WAL mode)
- **String encoding**: All strings stored as UTF-8 in SQLite and JSON. The `toUTF8`/`fromUTF8` utils are for future integration with legacy wstring-based code
- **Port conflict**: Port 2009 is used by the legacy MeOS server. During development, only one should run at a time. The port should be configurable via command-line argument
- **Test ports**: Integration tests should use separate ports to avoid conflicts (e.g., 18081-18090)
- **Existing patterns**: See `plan/prd-core-migration.md` line 64 for REST API patterns (regex routing, SPA fallback, string conversion)

## Success Metrics

- All 9 frontend pages display seeded data when running against the real backend
- `curl http://localhost:2009/api/v1/clubs` returns valid JSON matching the TypeScript types
- Frontend tests continue passing with MSW (no regression)
- C++ build and tests pass on Linux (CI via `cpp.yml`)
- Single binary startup: `./build/meos` serves both API and SPA

## Open Questions

- Should the SQLite database path be configurable via command-line argument or environment variable?
- Should we add a `--port` CLI flag or is env var `MEOS_PORT` preferable?

# PRD: MeOS Platform Modernization

## Introduction

MeOS (Much Easier Orienteering System) is a mature Windows desktop application for managing orienteering competitions, built with C++17, Win32/GDI, and MSBuild. This PRD describes the remaining migration work to make MeOS platform-independent, replace the Win32 GUI with a React+TypeScript web interface, and restructure the codebase into a modular layout.

The end result is a single executable that starts an embedded HTTP server and serves a React web GUI — users open `http://localhost:<port>` in any browser. The application runs on Linux, macOS, and Windows without modification.

### What exists today

**Legacy (`code/`):** ~200+ files in a flat directory. C++17, Win32/GDI UI, MySQL, MSBuild, vendored dependencies. See `code/AGENTS.md` for full details.

**Modern (`src/`):** Build infrastructure is complete (see `plan/archive/prd-build-infrastructure.md`):

- CMake 3.28+ with vcpkg integration, Ninja generator, CMakePresets.json
- Google Test + Vitest + `meos_add_test` macro
- CI/CD via GitHub Actions (`cpp.yml`, `frontend.yml`, `build-legacy.yml`)
- React + TypeScript + Vite shell in `src/ui/web/` (no pages/routes yet)
- Stub `src/main.cpp` — modular layout (US-002) not yet created
- `vcpkg.json` with `gtest` only

### Target State

- Modular `src/` with domain, net, db, util, io modules
- SQLite (embedded, no separate server)
- Full CRUD JSON API covering all domain entities
- React + TypeScript SPA served by embedded HTTP server

## Goals

- Restructure source code from flat `code/` into modular `src/` layout
- Extract domain model into standalone library with no platform-specific code
- Replace MySQL with SQLite for zero-configuration deployment
- Build complete CRUD REST API with JSON for all domain entities
- Build React + TypeScript web interface replacing Win32 GUI
- Migrate existing domain logic incrementally (module by module)
- Maintain single-binary deployment model (download and run)

## Codebase Patterns (from Previous Runs)

These patterns were discovered during previous Ralph runs and should be followed:

- Backend: XML API at `/meos`, JSON REST API at `/api/v1`
- Use explicit module dependencies in CMake. `VCPKG_ROOT` must be set to `/home/adam.georgsson@fnox.it/vcpkg` in this environment.
- Avoid nesting namespaces when migrating legacy code into modern modules.
- **Foundation fragility:** Previous stories marked as "passed" might only have stubs. Always verify foundation logic if tests fail mysteriously.
- Use `std::time_t` and `std::tm` with `timegm`/`_mkgmtime` for bit-compatible local time handling.
- Use `thread_local` circular buffer (StringCache) for returning `wstring` references.
- **StringCache and References:** When returning `const std::wstring&` from functions using `StringCache`, ensure the cache is used for all return paths (including fallbacks) to avoid returning references to temporaries.
- **oDataContainer:** Manages fields in a raw buffer using offsets (Index). `oDataInterface` is a wrapper around `oDataContainer` + data pointer.
- **oDataContainer Buffer Bounds:** Ensure the container's initial size is large enough to hold all fields (e.g., 4096 for `oClassData`), especially considering the 4-byte `wchar_t` size on Linux for string fields.
- **Wchar_t Alignment and Size:** On Linux, `wchar_t` is 4 bytes. Alignment for 8-byte types (like `int64_t` or `double`) must be handled carefully.
- **oEvent and Circular Dependencies:** Keep `oEvent.h` as a declaration-only header as much as possible, with entity-specific methods implemented in `oEvent.cpp`.
- **Method Shadowing:** Be careful with `using namespace std;` in classes that have member functions with names common in `std` (like `set`).
- **GUI Decoupling / Headless GDI:** The `gdioutput` class is the gateway to all legacy layout logic. A headless implementation providing approximate metrics is sufficient for most I/O tasks.
- **Localizer:** Handles `.lng` files with `Key = Value` format. Replaces `\n` escaping in both keys and values.
- **XML/CSV Parsing:** Core parsing logic is in `src/util/`. Domain-specific logic is in `src/io/`.
- **XML Declaration:** Custom `xmlparser` only skips the first tag if it is a declaration (`<?xml ... ?>`).
- **In-place XML parsing:** Requires careful handling of the closing quote in attribute values (`start <= end`).
- **CSV Encoding:** Always use UTF-8 with BOM for CSV files. Use `std::ofstream` with explicit BOM instead of `std::wofstream` in tests.
- **IOF 3.0 Time Handling:** ISO8601 time strings are handled by stripping the timezone and converting via `MeOSUtil::convertAbsoluteTimeISO`.
- **Template System:** MeOS templates use @SECTION markers. Ensure all sections are handled and placeholders (@TITLE, @CONTENTS, etc.) are replaced globally.
- **Color Math:** The `HLS` class is essential for preserving the "look and feel" of legacy MeOS results.
- **REST API:** Use regex for path parameters in `cpp-httplib` (e.g., `R"(/api/v1/clubs/(\d+))"`).
- **REST API Consistency:** Ensure both `oEvent` (live state) and repositories (persistent state) are updated on mutations.
- **REST API Strings:** Use `MeOSUtil::toUTF8` and `MeOSUtil::fromUTF8` for `wstring` <-> JSON string conversion.
- **SPA Fallback:** Implemented in `set_error_handler` of `cpp-httplib` by serving `index.html` for 404 GET requests that are not `/api/`.
- Use `const object + type` pattern for enums (Vite 7 compatibility)
- Use `import type` for all TypeScript interfaces/types
- Use `NavLink` for active route highlighting
- Tailwind 4 for styling (CSS-first approach)
- Use generic `DataTable` component for entity lists with sorting, filtering, and pagination. Supports row selection with `enableSelection` prop.
- Use `zod` for form validation and `react-hook-form` for form management.
- Reuse standard form components (`FormField`, `FormInput`, `FormSelect`, `SearchableSelect`) for consistent styling and validation.
- Use `size` property on `FormDialog` ('sm', 'md', 'lg', 'xl') to handle complex forms with varying width requirements.

## User Stories

### US-P0: Legacy Code Preparation

> **Extracted to separate PRD:** See [`plan/prd-legacy-preparation.md`](prd-legacy-preparation.md) for the full US-P0 breakdown (US-P0a through US-P0n). This work operates on the legacy `code/` directory under MSBuild and can run **in parallel** with the migration work below.

### US-002: Modular Source Layout

**Description:** As a developer, I want source code organized in logical modules so that the codebase is navigable and each layer has clear boundaries.

**Acceptance Criteria:**
- [ ] `src/` directory created with subdirectories: `app/`, `domain/`, `net/`, `db/`, `util/`, `io/`
- [ ] CMake targets defined per module (static libraries + main executable)
- [ ] Module dependencies are explicit in CMake (no circular dependencies)
- [ ] Header include paths configured so bare `#include "oBase.h"` still works within modules
- [ ] Builds and links successfully with the new layout

### US-003: Domain Layer Extraction

**Description:** As a developer, I want the domain model classes (oEvent, oRunner, oClass, etc.) extracted into a standalone library with no GUI or database dependencies so that they can be tested and used independently.

> **Note:** This story is split into incremental sub-stories ordered by dependency due to extreme coupling and the sheer size of oRunner (~230KB).

#### US-003a: Foundation — oBase, oDataContainer, domain_header

**Description:** Migrate the foundational base classes that all domain entities depend on.

**Acceptance Criteria:**
- [ ] `oBase.h/cpp` migrated to `src/domain/`
- [ ] `oDataContainer.h/cpp` migrated with `oDataInterface`/`oDataConstInterface`
- [ ] `domain_header.h` created with common enums and forward declarations
- [ ] `datadefiners.h` migrated
- [ ] Stubs created for `oEvent.h` and other dependents
- [ ] Domain library compiles with dependency on `util` only
- [ ] Unit tests for oBase basics

#### US-003b: Simple Entities — oControl, oPunch

**Description:** Migrate the simplest domain entities with fewest external dependencies.

**Acceptance Criteria:**
- [ ] `oControl.h/cpp` and `oPunch.h/cpp` migrated to `src/domain/`
- [ ] `SpecialPunch` enum moved to `domain_header.h` (avoid circular deps)
- [ ] Win32 string functions replaced with `meos_util` equivalents
- [ ] Stubs for `oCourse`, `oClass`, `oRunner`, `oCard`, `oFreePunch`
- [ ] Unit tests for oControl and oPunch

#### US-003c: oClub

**Description:** Migrate club entity — relatively simple with few dependencies.

**Acceptance Criteria:**
- [ ] `oClub.h/cpp` migrated to `src/domain/`
- [ ] Common enums centralized in `src/util/common_enums.h`
- [ ] Unit tests for oClub

#### US-003d: oCourse

**Description:** Migrate course entity. Courses define the control sequence and are referenced by classes.

**Acceptance Criteria:**
- [ ] `oCourse.h/cpp` migrated to `src/domain/` (full implementation, not stub)
- [ ] Course-control relationship works
- [ ] Length, climb, and course properties preserved
- [ ] Unit tests for oCourse operations

#### US-003e: oClass + Class Configuration

**Description:** Migrate class entity and related configuration. Classes are tightly coupled to courses and have complex start-method and qualification logic.

**Acceptance Criteria:**
- [ ] `oClass.h/cpp` migrated to `src/domain/` (full implementation, not stub)
- [ ] `classconfiginfo.h/cpp` migrated
- [ ] Class-course assignment works
- [ ] Start methods (individual, mass, pursuit, etc.) preserved
- [ ] `qualifications.h/cpp` migrated
- [ ] Unit tests for oClass operations

#### US-003f: oCard + oFreePunch

**Description:** Migrate card readout and free punch entities. Cards record the physical punch data from SI units.

**Acceptance Criteria:**
- [ ] `oCard.h/cpp` migrated to `src/domain/` (full implementation, not stub)
- [ ] `oFreePunch.h/cpp` migrated (full implementation, not stub)
- [ ] `SICard.h` (data structure) available in `src/util/` or `src/domain/`
- [ ] Card-punch matching logic preserved
- [ ] Unit tests for card operations

#### US-003g: oRunner

**Description:** Migrate the runner entity. This is the largest single file (~230KB) and most complex entity. Heavy stubbing will be needed for tight coupling to oEvent, oClass, oCard, and result computation.

> **Risk:** oRunner is extremely large and coupled. Consider migrating a simplified version first, then backfilling logic incrementally.

**Acceptance Criteria:**
- [ ] `oRunner.h/cpp` migrated to `src/domain/` (full implementation replacing stub)
- [ ] Runner-class, runner-club, runner-card relationships work
- [ ] Status computation (OK, DNS, DNF, DSQ, MP) works
- [ ] Split time computation works
- [ ] `oAbstractRunner` shared logic with oTeam preserved
- [ ] Unit tests for runner status, splits, and result computation

#### US-003h: oTeam

**Description:** Migrate team entity. Teams extend oAbstractRunner and contain runner members.

**Acceptance Criteria:**
- [ ] `oTeam.h/cpp` migrated to `src/domain/` (full implementation)
- [ ] Team-runner membership works
- [ ] Relay leg computation works
- [ ] Unit tests for team operations

#### US-003i: oEvent — Aggregate Root

**Description:** Migrate the full oEvent implementation. oEvent owns all domain collections and is the aggregate root. This is the final integration piece.

> **Risk:** oEvent is currently a minimal skeleton stub. The full implementation (oEvent.cpp, oEventDraw.cpp, oEventResult.cpp, etc.) touches everything. Migrate incrementally: core ownership first, then draw, then results.

**Acceptance Criteria:**
- [ ] `oEvent.cpp` full implementation replaces skeleton stub
- [ ] `oEventDraw.cpp` — class drawing and start list generation
- [ ] `oEventResult.cpp` — result computation orchestration
- [ ] Event owns and manages all entity collections
- [ ] Competition create/open/save lifecycle works
- [ ] Integration tests covering entity relationships through oEvent

#### US-003j: GeneralResult + Result Engine

**Description:** Migrate the pluggable result computation engine.

**Acceptance Criteria:**
- [ ] `generalresult.h/cpp` migrated (full implementation)
- [ ] Strategy pattern for scoring algorithms preserved
- [ ] `metalist.h/cpp` for output formatting migrated
- [ ] `oListInfo` migrated
- [ ] Result computation produces correct results for individual and relay formats
- [ ] Unit tests for result computation with known test data

#### US-003k: Domain Cleanup — Remove Stubs + Verify

**Description:** Final cleanup pass: remove all temporary stubs, verify the domain layer compiles cleanly, and ensure no Win32 dependencies remain.

**Acceptance Criteria:**
- [ ] All stubs (gdioutput stub, oSpeaker stub, Table stub, MeOSFeatures stub) removed or replaced with proper interfaces
- [ ] No `#ifdef _WIN32` guards in domain code (except `win_types.h` shims)
- [ ] Domain library compiles on Linux and Windows
- [ ] No direct Win32/GDI includes in domain code
- [ ] Full test suite passes
- [ ] `oDataContainer`/`oDataInterface` pattern verified working

**Learnings from Previous Runs:**
- Legacy Win32 GUI integration was deeply rooted in `oDataDefiner` and `oClub`; these must be surgically removed.
- `oEvent` acts as a central registry; `getControlByNumber` is essential for matching physical punch data to domain entities.

### US-004: SQLite Database Layer

**Description:** As a developer, I want a SQLite-based persistence layer so that MeOS runs without a separate database server.

> **Note:** Split into incremental sub-stories.

#### US-004a: Database Abstraction + Connection + Migrations

**Description:** Set up SQLite connection management and migration system.

**Acceptance Criteria:**
- [ ] `src/db/` contains `SQLiteDatabase` with connection management
- [ ] Migration system using `_migrations` table
- [ ] Initial schema (V1) for runners and clubs
- [ ] Unit tests for database operations and migrations

**Learnings from Previous Runs:**
- VCPKG provides `sqlite3` as `unofficial-sqlite3` and `unofficial::sqlite3::sqlite3` targets.
- Use `BEGIN TRANSACTION;`, `COMMIT;`, and `ROLLBACK;` in migration scripts to ensure atomicity.
- Always use `CREATE TABLE IF NOT EXISTS` for idempotency, although the migration tracker should prevent double-execution.
- `ctest` reported an existing SEGFAULT in `domain_tests` (GeneralResultTest.DynamicResultScoring) that appears to be environment-related (passes when run directly but fails in `ctest`).

#### US-004b: Schema for Simple Entities

**Description:** Add schema and CRUD for clubs, controls, and courses.

**Acceptance Criteria:**
- [ ] Schema migration for clubs, controls, courses
- [ ] CRUD operations (insert, get, update, delete) for each entity
- [ ] Repository/DAO pattern for data access
- [ ] Unit tests for each entity's CRUD operations

**Learnings from Previous Runs:**
- Public getters for protected domain buffers (`oData`) are necessary for clean repository implementation.
- Storing the `oData` buffer as a `BLOB` in SQLite allows for 100% state persistence without exhaustive schema mapping of all `oDataContainer` fields.
- Semicolon-separated strings are used for mapping domain collections (like control IDs in a course) to single SQL columns, matching legacy serialization patterns.
- Repository pattern effectively decouples domain entities from persistence logic, allowing for easy schema evolution.

#### US-004c: Schema for Complex Entities

**Description:** Add schema and CRUD for runners, classes, cards, and punches.

**Acceptance Criteria:**
- [ ] Schema migration for runners, classes, cards, punches, free punches
- [ ] Foreign key relationships enforced
- [ ] CRUD operations with relationship loading (e.g., runner with club/class)
- [ ] Unit tests for complex entity operations

**Learnings from Previous Runs:**
- `oEvent::addRunner` and `oEvent::addCard` have different signatures and requirements; repositories must adapt to how the domain aggregate root manages its entities.
- `oCard` and `oPunch` do not use `oDataContainer` buffers; their state is persisted via specialized serialization (punch strings) and dedicated tables.
- Foreign key relationships in SQLite are best established during table creation; adding them via `ALTER TABLE` is limited.
- Relationship loading requires a specific order of operations: load independent entities (Clubs, Courses) before dependent ones (Runners, Classes).

#### US-004d: Schema for Events + Teams

**Description:** Add event and team persistence, completing the full schema.

**Acceptance Criteria:**
- [ ] Schema migration for events and teams
- [ ] Team-runner membership persistence
- [ ] Event metadata (name, date, settings) persistence
- [ ] Concurrent access handled appropriately
- [ ] Full integration test: create event → add entities → query back
- [ ] SQLite database is a single file in the working directory

**Learnings from Previous Runs:**
- Typo in SQL schema definitions can lead to silent or confusing failures in repositories (e.g., `sqlite3_step` returning `SQLITE_ERROR` because of an invalid column name).
- `oEvent` acts as the central registry for ID-to-entity lookups during repository loading; repositories must be passed the `oEvent` instance to correctly wire up relationships.
- Semicolon-separated strings are an effective way to store simple lists (like runners in a team) when a full join table might be overkill for the current domain model.
- Always verify the aggregate root (`oEvent`) metadata is persisted, as it contains essential competition-wide settings like ZeroTime.

### US-005: JSON REST API — Core Entities

**Description:** As a frontend developer, I want complete CRUD endpoints for all core domain entities so that the React GUI can manage competitions.

> **Note:** Split by entity group to keep each story testable and deployable independently.

#### US-005a: API Framework + Error Handling

**Description:** Set up the HTTP server, routing infrastructure, JSON serialization, and consistent error handling.

**Acceptance Criteria:**
- [ ] HTTP server (cpp-httplib or restbed) integrated and running
- [ ] Route registration pattern established
- [ ] JSON request parsing and response serialization (nlohmann-json)
- [ ] Consistent error response format with error codes
- [ ] Input validation framework with meaningful error messages
- [ ] API versioning (`/api/v1/...`)
- [ ] Unit tests for routing and error handling

**Learnings from Previous Runs:**
- `cpp-httplib` requires `find_package(httplib CONFIG REQUIRED)` and links to `httplib::httplib`.
- Use `std::thread` to run the server's `listen` method in the background to avoid blocking the main thread (essential for tests).
- `httplib::Server::set_error_handler` and `set_exception_handler` provide a clean way to centralize error responses.
- Ensure `std::this_thread::sleep_for` is used after `start()` to allow the background thread to initialize the socket before tests begin making requests.
- Port selection for tests (e.g., 18080) should avoid standard ports (8080) to prevent conflicts with running dev servers.

#### US-005b: Club + Control Endpoints

**Description:** CRUD endpoints for the simplest entities — clubs and controls.

**Acceptance Criteria:**
- [ ] `GET/POST/PUT/DELETE /api/v1/clubs` and `/api/v1/clubs/{id}`
- [ ] `GET/POST/PUT/DELETE /api/v1/controls` and `/api/v1/controls/{id}`
- [ ] List endpoints support basic filtering
- [ ] Integration tests for each endpoint

**Learnings from Previous Runs:**
- `oEvent::addClub` and `oEvent::addControl` assign new IDs if 0 is passed; the returned entity should be saved to the database to ensure the ID is persisted.
- Full qualification of namespaces (e.g., `meos::db::SQLiteDatabase`) in headers prevents ambiguity when included from different modules.
- `httplib::Client` needs a small delay or should be instantiated after `server->start()` has had time to initialize.

#### US-005c: Course + Class Endpoints

**Description:** CRUD endpoints for courses and classes, including their relationships.

**Acceptance Criteria:**
- [ ] `GET/POST/PUT/DELETE /api/v1/courses` and `/api/v1/courses/{id}`
- [ ] `GET/POST/PUT/DELETE /api/v1/classes` and `/api/v1/classes/{id}`
- [ ] Course-control sequence in responses
- [ ] Class-course assignment in requests/responses
- [ ] Integration tests

**Learnings from Previous Runs:**
- `oCourse::importControls` expects a comma-separated string of IDs; this is an efficient way to update the entire control sequence from a JSON array.
- Always allow setting relationship IDs to 0 (e.g., `courseId: 0`) in `POST` and `PUT` to support clearing associations.
- Course-control and class-course relationships are unidirectional in the current domain model (course has controls, class has a course); the API should reflect this.
- Integration tests for relationships must be sequential: create dependencies (Clubs, Controls, Courses) before creating dependent entities (Classes, Runners).

#### US-005d: Runner + Team Endpoints

**Description:** CRUD endpoints for runners and teams — the most complex entities.

**Acceptance Criteria:**
- [ ] `GET/POST/PUT/DELETE /api/v1/runners` and `/api/v1/runners/{id}`
- [ ] `GET/POST/PUT/DELETE /api/v1/teams` and `/api/v1/teams/{id}`
- [ ] Filtering by name, club, class
- [ ] Pagination support
- [ ] Competition entity endpoint (`GET/PUT /api/v1/competitions`)
- [ ] Integration tests

**Learnings from Previous Runs:**
- `oRunner` and `oTeam` methods like `setStartTime` and `setStatus` require `oBase::ChangeType::Update` for persistent changes.
- `oTeam::getDisplayClub()` is the preferred way to get the club name for a team.
- Query parameters in `cpp-httplib` are accessed via `req.has_param(key)` and `req.get_param_value(key)`.
- `SQLiteDatabase::open(":memory:")` is useful for fast API integration tests without disk I/O.
- When updating runners/teams, ensure related entities (Clubs, Classes, Courses) already exist in `oEvent`.

### US-006: JSON REST API — Competition Operations

**Description:** As a frontend developer, I want API endpoints for competition-specific operations (start lists, results, card readout) so that the GUI can support the full competition workflow.

**Acceptance Criteria:**
- [ ] `GET /api/v1/results` returns computed results (per class, overall)
- [ ] `GET /api/v1/startlist` returns start lists
- [ ] `POST /api/v1/cards` handles card readout data submission
- [ ] `POST /api/v1/runners/{id}/status` allows manual status changes (DNS, DNF, DSQ)
- [ ] Result computation uses existing `GeneralResult` logic
- [ ] Endpoints support both preliminary and final results

**Learnings from Previous Runs:**
- **Thread Safety is Critical:** Global objects like `Localizer` and static caches in `MeOSUtil` must be protected by mutexes or made thread-local, as the REST server handles requests in multiple threads.
- **Electronic Timing Logic:** Electronic card readout via `POST /api/v1/cards` requires adding punches to an `oCard` and calling `oRunner::evaluateCard(true, ...)` to trigger the domain's result computation logic.
- **UTF-8 Stability:** `std::wstring_convert` is deprecated and can be unstable; manual conversion is safer for a cross-platform core.
- **Test Port Isolation:** Assigning unique ports to each test suite prevents flaky tests due to "port already in use" or interference from shutting down servers.
- **Joining Server Threads:** Always `join()` the server thread instead of `detach()` to ensure a clean shutdown and avoid accessing destroyed objects from the background thread.

### US-007–010: React Web Frontend

> **Extracted to separate PRD:** See [`plan/prd-web-frontend.md`](prd-web-frontend.md) for the full breakdown (US-007 through US-010). Runs **in parallel** with C++ migration work — only depends on the API contract.

### US-011: Static File Serving from C++ Server

**Description:** As a user, I want the C++ executable to serve the React frontend so that no separate web server is needed.

**Acceptance Criteria:**
- [ ] C++ HTTP server serves static files from an embedded or bundled directory
- [ ] `index.html` served at `/` with SPA fallback (all non-API routes serve `index.html`)
- [ ] Correct MIME types for `.js`, `.css`, `.html`, `.svg`, `.png`, `.woff2`
- [ ] Gzip/compression for static assets
- [ ] React production build is integrated into CMake build process

**Learnings from Previous Runs:**
- **MIME Types:** `cpp-httplib` handles most standard web MIME types (html, js, css, svg, png, woff2) out of the box when using `set_mount_point`.
- **Main Application Orchestration:** `src/main.cpp` is now a real application entry point that links all modules and can be used to run the full modernized stack.
- **Build Artifacts:** When building the React frontend, use `--outDir ../../../web` to ensure it lands in a location the C++ server expects by default.

### US-012: Remove Win32 GUI Code

**Description:** As a developer, I want Win32-specific GUI code removed so that the codebase is platform-independent.

**Acceptance Criteria:**
- [ ] All `Tab*.cpp/h` files removed or migrated to web equivalents
- [ ] `gdioutput.cpp/h` and related GDI files removed
- [ ] Win32-specific APIs (`CreateWindow`, `SendMessage`, etc.) eliminated from core code
- [ ] No Windows-only headers (`windows.h`, `commctrl.h`) in domain or server code
- [ ] Application compiles and runs on Linux without Win32 dependencies

**Learnings from Previous Runs:**
- **Null Safety in REST handlers:** Iterating over entity lists in `RestServer` requires careful null checks, especially when multiple tests or threads might be interacting with the `oEvent` aggregate root.
- **Abstract GDI usage:** Making `gdioutput` abstract required updates to tests that previously instantiated it directly.
- **Win32 type elimination:** `RECT` is a common Win32 type that should be renamed or namespaced (e.g., `meosRect`) to ensure complete platform independence.

### US-013: Utility Migration

**Description:** As a developer, I want shared utilities migrated to `src/util/` so they are available to all modules.

> **Note:** Split into logical groups.

#### US-013a: Core Utils

**Description:** Migrate the fundamental utility functions and exception handling.

**Acceptance Criteria:**
- [ ] `meos_util.h/cpp` migrated to `src/util/` (cross-platform)
- [ ] `meosexception.h` migrated
- [ ] Wide/narrow string conversion (`string2Wide`, `wide2String`, `narrow`, `widen`)
- [ ] Win32 function replacements (`_itow_s` → `to_wstring`, `sprintf_s` → `snprintf`)
- [ ] `std::filesystem` for file operations
- [ ] Unit tests for string conversion and time formatting

#### US-013b: Parsers

**Description:** Migrate XML and CSV parsing utilities.

**Acceptance Criteria:**
- [ ] `xmlparser.h/cpp` migrated to `src/util/`
- [ ] `csvparser.h/cpp` migrated to `src/util/`
- [ ] Unit tests for xmlparser and csvparser

#### US-013c: Time Handling

**Description:** Migrate time-related utilities.

**Acceptance Criteria:**
- [ ] `TimeStamp.h/cpp` migrated to `src/util/`
- [ ] `timeconstants.hpp` migrated to `src/util/`
- [ ] Unit tests for TimeStamp

#### US-013d: Localization

**Description:** Migrate i18n/localization system.

**Acceptance Criteria:**
- [ ] `localizer.h/cpp` migrated to `src/util/` (fully cross-platform)
- [ ] Win32 resource loading replaced with `std::ifstream` + `codecvt`
- [ ] `.lng` files copied to `resources/lang/`
- [ ] Unit tests for localizer

### US-014: I/O and Import/Export

**Description:** As a developer, I want import/export functionality migrated so that data exchange works in the new system.

> **Note:** Split by format — each is independent and can be migrated/tested separately.

#### US-014a: IOF 3.0 XML

**Description:** Migrate IOF XML import and export.

**Acceptance Criteria:**
- [ ] `iof30interface.h/cpp` migrated to `src/io/`
- [ ] IOF 3.0 XML import works (runners, classes, clubs, courses)
- [ ] IOF 3.0 XML export works (results, start lists)
- [ ] Platform-independent (no Win32 XML APIs)
- [ ] Unit tests with sample IOF XML files

**Learnings from Previous Runs:**
- **Domain API Completeness:** The domain migration (US-003*) may have missed some mutation methods (like `oClass::addStageCourse`) that are essential for importers; these should be added as discovered.
- **Course Control Persistence:** `oCourse::importControls` expects a comma-separated string of IDs and requires `setChanged` and `updateLegLengths` flags to be set correctly for state consistency.

#### US-014b: CSV Import/Export

**Description:** Migrate CSV data exchange.

**Acceptance Criteria:**
- [ ] CSV import for runners migrated to `src/io/`
- [ ] CSV export for results/start lists migrated
- [ ] Unit tests for CSV round-trip

**Learnings from Previous Runs:**
- **Heuristic Format Detection:** A simple column-count heuristic (`size() > 10`) is effective for distinguishing between simple and complex (OE-CSV) formats.
- **Domain Setup for Tests:** Proper result evaluation in tests requires setting up `oCard` and its `punches`, linking it to the `oRunner`, and calling `oRunner::evaluateCard`.
- **oEvent Entity Lists:** Use `oe.Runners`, `oe.Clubs`, etc. directly when a getter is missing in the migrated `oEvent` API.

#### US-014c: HTML Result Generation

**Description:** Migrate HTML output for results and lists.

**Acceptance Criteria:**
- [ ] HTML result generation migrated to `src/io/`
- [ ] Templates work cross-platform
- [ ] Unit tests for HTML output

**Learnings from Previous Runs:**
- **oListParam members:** Many I/O related members of `oListParam` were missing in the migrated domain layer and had to be added to support HTML generation.

#### US-014d: PDF Generation

**Description:** Migrate PDF output using libharu.

**Acceptance Criteria:**
- [ ] PDF generation (`libharu`) migrated to `src/io/`
- [ ] libharu available via vcpkg
- [ ] PDF output works on Linux and Windows
- [ ] Unit tests for PDF generation

**Learnings from Previous Runs:**
- **Standard Font Limitations:** Standard PDF fonts (Helvetica, Times, etc.) in `libharu` do not support multibyte encodings like UTF-8. For now, `MeOSUtil::narrow` is used, but full UTF-8 support requires loading TrueType fonts (.ttf).
- **VCPKG Targets:** `libharu` in vcpkg is provided as `unofficial-libharu` and should be linked via `unofficial::libharu::hpdf`.
- **Conflict in Headers:** Do not forward declare `HPDF_Doc`, `HPDF_Page`, etc., as `void*` in headers if you plan to include `hpdf.h` later. Including `hpdf.h` in the header is safer.
- **Image Rendering:** Image support is currently skipped because the `image` module hasn't been migrated yet.

## Functional Requirements

- FR-3: The domain layer must compile as an independent static library with no platform-specific code
- FR-4: Persistence must use SQLite with the database stored as a single file
- FR-5: The REST API must expose full CRUD operations for all domain entities via JSON
- FR-6: The REST API must follow RESTful conventions with versioned paths (`/api/v1/...`)
- FR-7: The C++ executable must serve the React SPA as static files
- FR-8: The React frontend must communicate exclusively through the JSON REST API
- FR-9: The application must start with a single command/double-click (no external dependencies at runtime)
- FR-10: The build system must produce a single distributable binary (with bundled frontend assets)
- FR-11: Source code must be organized in `src/` with clear module boundaries
- FR-14: The domain model must preserve existing business logic (result computation, class drawing, qualification systems)
- FR-15: Localization support must be retained (Swedish primary, multi-language via `.lng` files)

## Non-Goals

- **SportIdent hardware integration** — deferred to a future phase; no serial port or USB code in this scope
- **MySQL support** — SQLite only; migration tool from MySQL is out of scope (documented as future work)
- **Mobile-native app** — responsive web only, no iOS/Android native
- **Multi-user authentication** — single-user/local access model initially
- **Cloud deployment** — designed for local execution; cloud hosting is a user choice, not a project goal
- **Speaker/announcer functionality** — deferred; complex real-time features come after core is stable
- **Online results push** — existing `onlineresults`/`liveresult` integration deferred
- **Map rendering** — `maprenderer` functionality deferred
- **Backward compatibility with existing MeOS data files** — documented as future migration path

## Design Considerations

- The React GUI should mirror the existing tab structure for familiarity: Competition, Classes, Courses, Controls, Clubs, Runners, Teams, Results, Lists
- Use a component library (e.g., Radix UI, shadcn/ui) for consistent, accessible UI components
- Table views should support sorting, filtering, and inline editing for efficiency
- The UI should work well on tablets (orienteering events often use tablets in the field)
- Dark mode support is a nice-to-have but not required initially

## Technical Considerations

### Migration Strategy

The migration follows an incremental approach. Both `code/` (legacy) and `src/` (new) coexist during the transition. Phase 0 (legacy prep) and Phase 1 (foundation) are complete — see `plan/archive/`. Remaining phases:

2. **Utilities & Domain:** Migrate `src/util/` and `src/domain/` (no GUI, no DB), add tests
3. **Database:** Implement SQLite layer in `src/db/`, wire to domain
4. **REST API:** Build JSON CRUD API in `src/net/`, wire to domain + DB
5. **React Frontend:** Build web GUI in `src/ui/web/`, connect to API
6. **Static Serving & Integration:** C++ server serves React build, single-binary packaging
7. **Cleanup:** Remove Win32 code, remove `code/` directory

Each phase produces a working, testable increment.

### Key Dependencies (vcpkg, to be added)

- **cpp-httplib** — HTTP server
- **nlohmann-json** — JSON serialization
- **sqlite3** (`unofficial-sqlite3`) — Database
- **libharu** (`unofficial-libharu`) — PDF generation
- **zlib** — Compression

### Architecture Diagram

```
┌──────────────────────────────────────────────┐
│            React + TypeScript                │
│              (src/ui/web/)                   │
│  ┌────────┬────────┬────────┬────────┐       │
│  │Runners │Classes │Results │ ...    │       │
│  └────────┴────────┴────────┴────────┘       │
└──────────────────┬───────────────────────────┘
                   │ JSON REST API
┌──────────────────┴───────────────────────────┐
│          C++ HTTP Server (src/net/)          │
│  ┌─────────────┐  ┌──────────────────┐       │
│  │ REST Routes │  │ Static File Srv  │       │
│  └──────┬──────┘  └──────────────────┘       │
│         │                                    │
│  ┌──────┴───────────────────────────┐        │
│  │    Domain Layer (src/domain/)    │        │
│  │  oEvent, oRunner, oClass, ...    │        │
│  └──────┬───────────────────────────┘        │
│         │                                    │
│  ┌──────┴───────────────────────────┐        │
│  │    Database Layer (src/db/)      │        │
│  │            SQLite                │        │
│  └──────────────────────────────────┘        │
│                                              │
│  ┌──────────────────────────────────┐        │
│  │    Utilities (src/util/)         │        │
│  │  strings, time, xml, csv, i18n   │        │
│  └──────────────────────────────────┘        │
└──────────────────────────────────────────────┘
```

### Source Layout

```
src/
├── app/           # Main entry point, application lifecycle
├── domain/        # Domain model (oEvent, oRunner, oClass, etc.)
├── db/            # Database abstraction + SQLite implementation
├── net/           # HTTP server, REST API routes
├── io/            # Import/export (IOF XML, CSV, HTML, PDF)
├── util/          # Shared utilities (strings, time, parsing, i18n)
└── ui/
    └── web/       # React + TypeScript frontend
        ├── src/
        ├── package.json
        ├── tsconfig.json
        └── vite.config.ts
```

## Success Metrics

- All domain entity CRUD operations work end-to-end (GUI → API → DB → response)
- Single binary + bundled frontend deploys without any external dependencies
- Competition workflow (create competition → add classes/courses → add runners → record results → view results) works entirely through the web GUI

## Open Questions

1. Should the REST API support WebSocket for real-time updates (live results), or is polling sufficient for the initial version?
2. Should the React frontend support offline mode (service worker / PWA)?
3. How should the bundled frontend assets be embedded — as files alongside the binary, or compiled into the binary as resources?
4. Should localization in the frontend reuse the existing `.lng` files or adopt a standard i18n library (e.g., react-intl)?
5. What is the minimum supported browser version for the web GUI?

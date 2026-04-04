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
- CSV I/O (`src/io/CsvIo`): OE format uses semicolons, first line is always header (skip unconditionally), then `fields.size() > 10` filters data rows. `detectFormat()` checks col[1] for "Descr"/"Namn"/"Descr."/"Navn" → OS, otherwise → OE.
- HTML I/O (`src/io/HtmlWriter`): MeOS `.template` format: `@MEOS EXPORT TEMPLATE` first line, `tag@Name` second, then `@HEAD/@OUTERPAGE/@INNERPAGE/@SEPARATOR/@END` sections. Placeholder replacement must be single-pass longest-match (e.g. `@TITLE` before `@T`). Legacy `HTMLWriter` depends on `gdioutput` — do NOT try to port it directly; re-implement cleanly.
- `oListParam` (src/domain/oListInfo.h): has `set<int> selection` (class filter), `bool lockUpdate`, `filterInclude(count, runner)`. These must all be present with defaults in the constructor.
- `nlohmann-json` CMake: `find_package(nlohmann_json CONFIG REQUIRED)` + link `nlohmann_json::nlohmann_json`.
- `ChangeType` is `oBase::ChangeType` (inner enum). Add `using ChangeType = oBase::ChangeType;` in .cpp files outside domain/. Values: `Quiet` (no DB write), `Update` (persist).
- `okResponse()` envelope is `{"data": ...}` — no "status" key. Don't write tests checking `resp["status"]`.
- REST API test ports: CLUBS=18081, CONTROLS=18082, COURSES=18083, CLASSES=18084, RUNNERS=18085, TEAMS=18086, COMPETITION=18087.
- `oEvent::getControlByNumber(int number)` searches Controls list via `ctrl->hasNumber(number)`. Use this for matching physical punch numbers to domain controls.
- `oEvent::getRunnerByCardNo(int cardNo, int time, CardLookupProperty)` scans Runners by `r.getCardNo()`. Pass time>0 to pick best runner by start-time proximity when multiple runners share a card.
- Competition lifecycle fields in oEvent: `tName` (wstring), `tDate` (wstring), `ZeroTime` (int, seconds). `newCompetition(name)` clears ALL collections.
- Property store in oEvent: `map<string, wstring> eventProperties` — `getPropertyInt/getPropertyString/setProperty` are map-backed, not filesystem-backed in the migrated version.
- `formatTimeHMS(int seconds)` and `convertAbsoluteTimeHMS(const wstring&, int)` (in meos_util.h) are the correct pair for formatting/parsing HH:MM:SS absolute times for ZeroTime.
- When adding `getOData()`/`getODataBlobSize()` accessors to domain entities for persistence, the methods must be placed in a `public:` section, not `protected:`. Be careful about the placement relative to `protected:` blocks in the header.
- For SQLite FK columns that allow no-relationship (0 as "none"), use `DbParam::Null()` on insert and `parseIntOrZero()` on read — `stoi("")` throws when SQLite returns NULL as empty string.
- When inserting a domain entity with FK references (e.g. runner with club_id), you must first persist the referenced entities to the DB, otherwise SQLite FK constraint fails even if you use `oe.addClub()` (which only adds to memory).
- `oClass::legInfo` is protected; add public `getNumLegs()` returning `(int)legInfo.size()` for repository access. `getNumLegNoParallel()` is NOT the same — it excludes parallel legs.
- `oEventDraw.cpp` and `oEventResult.cpp` are new cross-platform files in src/domain/; must be listed in `src/domain/CMakeLists.txt` under meos_domain sources.
- Domain layer uses `domain_header.h` for Win32 shims (LPBYTE, __int64, strcpy_s, wcsncpy_s, _i64toa_s, _wtof) + using-declarations. Include this first in every domain header.
- `oAbstractRunner` is in `src/domain/oAbstractRunner.h` (NOT in oEvent.h). It forward-declares `oEvent` to break circular dep. `oRunner.h` includes `oAbstractRunner.h`. `oEvent.h` includes `oRunner.h` (transitively gets oAbstractRunner).
- `RunnerStatus`, `DynamicRunnerStatus`, `SortOrder`, `isPossibleResultStatus` etc. all live in `oAbstractRunner.h` (not oEvent.h, not a separate enum file).
- `oAbstractRunner::DynamicValue` methods (isOld, update) reference `oEvent::dataRevision` — these implementations go in oRunner.cpp (not inline in the header) because they need the full oEvent definition.
- `oRunner` friend declarations are not enough for tIndex/tMatchControlId/isUsed access on oPunch — those must be public (or the test must not access them directly). The `isUsed`, `tIndex`, `tMatchControlId` fields on oPunch must be public for evaluateCard to set them.
- `oe->oRunnerData` must be initialized in `oEvent::oEvent()` in domain.cpp before any oRunner is created. The runner data container has ~24 fields.
- `bibStartNoToRunnerTeam` field (used by `setStartNo`) must be added as a stub map in oEvent.h stub.
- `oe->getClubCreate(id, name)` stub in oEvent.h: look up by id if >0, otherwise call addClub(name).
- `DataRevisionCache<T>` template bodies go in oBase.cpp with explicit instantiations — they need full oEvent definition which creates a circular dep if put in the header.
- **MeOS time units**: All times (startTime, finishTime, runningTime, ZeroTime) are stored as `timeUnitsPerSecond=10` units per second. Include `timeconstants.hpp` and use `timeConstHour`, `timeConstMinute`, `timeConstSecond`. `convertAbsoluteTimeHMS("10:00:00", -1)` = 360000 (not 36000). Divide by `timeConstSecond` to get seconds for IOF/JSON output.
- `intkeymap.hpp` + `intkeymapimpl.hpp` are standalone after removing `#include "StdAfx.h"`. The implementation file is #include'd at the bottom of intkeymap.hpp.
- vcpkg baseline must match the local vcpkg repo HEAD (`git rev-parse HEAD` in `/home/adam.georgsson@fnox.it/vcpkg`). The original baseline `67f167b2f7c170f83688919c90edcd896239330e` does not exist in the local repo; updated to `62159a45e18f3a9ac0548628dcaf74fcb60c6ff9`.
- `VCPKG_ROOT` env var is not set by default; must pass `VCPKG_ROOT=/home/adam.georgsson@fnox.it/vcpkg` when running cmake commands, or export it in the shell.
- Module dependency order (no circular deps): `util` -> `domain` -> `db` and `io` (both depend on domain+util) -> `net` (depends on domain+util+db) -> `app` (depends on all).
- Each module CMakeLists.txt uses `target_include_directories(... PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})` so bare `#include "header.h"` works within and across modules that link against it.
- Static libraries require at least one .cpp compilation unit; use a placeholder .cpp if no real code yet.
- GTest via vcpkg: use `find_package(GTest CONFIG REQUIRED)` with target `GTest::gtest_main` — vcpkg installs to `build/vcpkg_installed/x64-linux/`.
- On Linux LP64 (64-bit), `uint64_t == unsigned long`, so defining both `itow(uint64_t)` and `itow(unsigned long)` is a redefinition error. Guard with `#if !defined(__LP64__) && !defined(_LP64)`.
- Test files that include meos_util.h must explicitly `using std::string; using std::wstring;` — the header does NOT inject `using namespace std`.
- Cross-platform UTF-8 encoding: write manual UTF-8 codec (append_utf8 / utf8_to_wstring / wstring_to_utf8) to avoid deprecated `std::codecvt_utf8`. On Linux wchar_t is 4 bytes (UTF-32); on Windows 2 bytes (UTF-16) requiring surrogate pair handling.
- CP-1252 decoding on Linux: bytes 0x80-0x9F map to specific Unicode codepoints via a lookup table; 0xA0-0xFF map directly (Latin-1 compatible).
- `meosException` must NOT inherit `std::exception(msg)` on Linux (MSVC-specific). Use `std::runtime_error` as base class instead.
- `HLS` color class: Replace Win32 `WORD/BYTE/GetRValue/RGB` macros with `uint16_t/uint8_t` equivalents and inline helper functions guarded by `#ifndef _WIN32`.
- `StringCache::getInstance()` should use `thread_local` static instead of Win32 `GetCurrentThreadId()`. The legacy global was NOT thread-safe; thread_local is correct.
- `getNameSplitPoint` depends on `lang.get().getGivenNames()` (Localizer). Provide simplified version splitting at last space until Localizer is migrated (US-013d).
- Entity methods named `set()` shadow `std::set` in member function bodies — use `std::set<int>` (fully qualified) inside entity .cpp files that have a member function named `set`.
- Domain .cpp files that use getDI()/getDCI() must include `oDataContainer.h` directly (oBase.h only forward-declares those types).
- oEvent stub constructor must initialize `oControlData` (and future *Data pointers) — any entity whose getDataBuffers returns `*oe->xxxData` will crash if the pointer is null.
- `appendCodeString()` + `decodeString()` is the correct round-trip pair for oPunch serialization; `codeString()` stores raw punchTime and is NOT compatible with decodeString.
- Fixed-width `addVariableString(name, N, ...)` stores N bytes; on Linux (4-byte wchar_t) the usable string length is N/4 - 1 characters. Use ≤N/4-1 length strings in tests, or declare as dynamic string.
- vcpkg port name for SQLite is `sqlite3` (not `unofficial-sqlite3`); but the CMake `find_package` call is `find_package(unofficial-sqlite3 CONFIG REQUIRED)` and the target is `unofficial::sqlite3::sqlite3`. The port name in vcpkg.json must be `sqlite3`.
- SQLite3 `sqlite3_exec` for multi-statement DDL works correctly without needing to split statements — SQLite handles multiple semicolon-separated statements in a single `exec` call.
- Foreign keys are OFF by default in SQLite; enable with `PRAGMA foreign_keys=ON` after opening the connection.
- WAL mode (`PRAGMA journal_mode=WAL`) enables concurrent readers; enable by default after open for production use.

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

**Learnings from Previous Runs:**
- The vcpkg.json baseline was stale relative to the local vcpkg repo — always update it to `git rev-parse HEAD` of the local vcpkg installation before running cmake.
- `VCPKG_ROOT` is not set in the shell environment; must be passed explicitly: `VCPKG_ROOT=/home/adam.georgsson@fnox.it/vcpkg cmake --preset default`.
- Ninja and g++ 13.3.0 are available at standard paths; no extra setup needed.
- CMakePresets.json `$env{VCPKG_ROOT}` silently produces an empty path when VCPKG_ROOT is unset — no error until vcpkg.cmake tries to fetch the baseline. Always verify the env var is exported before configuring.
- Static library targets need at least one compilation unit; placeholder .cpp files prevent the "no source files" CMake error.

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

**Learnings from Previous Runs:**
- `enum KeyCommandCode;` (forward-declared opaque enum) is not valid C++ — remove from TableType.h when copying; only forward-declare enums with explicit underlying types.
- `convertDynamicBase(wstring, int64_t&)` signature in meos_util.h uses `long long &` not `int64_t &`; on Linux LP64 these are different types. Declare a `long long val` intermediary and assign to int64_t afterward.
- intkeymapimpl.hpp includes `StdAfx.h` in the legacy — must be removed when copying. After removal it is fully self-contained (only depends on intkeymap.hpp which it gets from the #include at the top of intkeymap.hpp).
- intkeymap.hpp `operator[]` const variant calls `lookup(key,tmp)` where `tmp` is mutable — need `const_cast<T&>(tmp)` since tmp is declared `T tmp` (non-const member). On MSVC this compiled silently; GCC rejects it.
- `__int64` shim: define as `typedef int64_t __int64` in domain_header.h (guarded by `#ifndef _WIN32`). All usage in oDataContainer.cpp compiles cleanly.
- `wcsncpy_s` shim: must null-terminate at `dst[n]` where `n = min(count, dstsz-1)`. MSVC semantics match POSIX `wcsncpy` + explicit null byte.
- `_i64toa_s` replacement: `snprintf(buf, bufsz, "%lld", (long long)val)` — use explicit `(long long)` cast since `int64_t` is `long` on LP64.
- `SQL_quote` used `WideCharToMultiByte`; replace with `toUTF8(wstring)` from meos_util.h — semantically identical, cross-platform.
- `DataRevisionCache<T>` method bodies belong in a .cpp file, not the header, because they need the full `oEvent` type. Put implementations in oBase.cpp and use `template class DataRevisionCache<T>` explicit instantiations for the types needed (wstring, int, etc.).
- Stub oEvent.h requires `gdiBase()` and `askOkCancel()` methods for the oIS64 inputData branch in oDataContainer; include them in the stub even if they return no-ops.

#### US-003b1: oControl Migration

**Description:** Migrate oControl and create stubs for downstream entities.

**Acceptance Criteria:**
- [ ] `oControl.h/cpp` migrated to `src/domain/`
- [ ] `SpecialPunch` enum moved to `domain_header.h` (avoid circular deps)
- [ ] Win32 string functions replaced with `meos_util` equivalents
- [ ] Stubs for `oCourse`, `oClass`, `oRunner`, `oCard`, `oFreePunch`
- [ ] Unit tests for oControl

**Learnings from Previous Runs:**
- `oControl::set()` member functions shadow `std::set` inside oControl member function bodies — use `std::set<int>` explicitly to avoid "expected primary-expression" parse error.
- `oControl.cpp` must include `oDataContainer.h` directly (not just transitively) to get full definitions of `oDataInterface` and `oDataConstInterface`; `oBase.h` only forward-declares them.
- `oEvent` stub needs a real constructor/destructor that initializes `oControlData = new oDataContainer(oControl::dataSize)` with the correct fields; all oControl DI methods crash if `oControlData == nullptr`.
- `codeString()` and `appendCodeString()` are NOT interchangeable: `codeString()` stores raw punchTime (in time-units), but `decodeString()` multiplies by `timeConstSecond`. Use `appendCodeString()` + `decodeString()` for a proper serialization round-trip.
- `getControlIdByName()` uses `compareStringIgnoreCase()` from meos_util.h instead of `_wcsicmp()` (no shim needed).
- `oe->setupControlStatistics()` is a no-op stub; statistics methods always return 0 in tests (acceptable until oEvent is fully implemented).

#### US-003b2: oPunch Migration

**Description:** Migrate oPunch entity and its tests.

**Acceptance Criteria:**
- [ ] `oPunch.h/cpp` migrated to `src/domain/`
- [ ] Punch time handling and control matching preserved
- [ ] Unit tests for oPunch

#### US-003c: oClub

**Description:** Migrate club entity — relatively simple with few dependencies.

**Acceptance Criteria:**
- [ ] `oClub.h/cpp` migrated to `src/domain/`
- [ ] Common enums centralized in `src/util/common_enums.h`
- [ ] Unit tests for oClub

**Learnings from Previous Runs:**
- The oClub migration was done as part of the US-003b iteration (stubs/header/impl were created together). Future iterations should check whether entity files exist as more than stubs before re-implementing.
- `oClubData` fields must be initialized in oEvent ctor before any oClub can call getDI()/getDCI() — the oClubData field list is: District(oIS32), ShortName(8), CareOf(31), Street(41), City(23), State(23), ZIP(11), EMail(64), Phone(32), Nationality(3), Country(23), Type(20), ExtId(oIS64), InvoiceNo(oIS16U), StartGroup(oIS32).
- `oClub::dataSize = 2048` is double the Windows size (768) to account for 4-byte wchar_t on Linux. This is sufficient for all DI string fields.
- `oClub::internalSetName` implements a multi-pass compact name algorithm: (1) "Skid o OK/OL" → SOK/SOL for prettyName, (2) strip all-caps ≤3-letter tokens + "Orientering"/"GOIF" etc, (3) only compact if a "proper name" word (2+ chars > 'Z') exists.
- The `ShortName` DI field overrides compact name computation — after setting it, call `nameChanged()` to re-trigger `internalSetName`.
- `assignInvoiceNumber(reset=true)` assigns sequential invoices starting from `getPropertyInt("FirstInvoice", 100)`. `reset=false` finds existing max invoice and continues from there.
- `oClub::clearClubs` uses `getClubs()` + `removeClub()` — be careful the Club list is not being iterated while removeClub is called (it marks as Removed, doesn't erase the list node).

#### US-003d: oCourse

**Description:** Migrate course entity. Courses define the control sequence and are referenced by classes.

**Acceptance Criteria:**
- [ ] `oCourse.h/cpp` migrated to `src/domain/` (full implementation, not stub)
- [ ] Course-control relationship works
- [ ] Length, climb, and course properties preserved
- [ ] Unit tests for oCourse operations

**Learnings from Previous Runs:**
- `oCourse::dataSize = 128` is correct for Linux: with legacy fields StartName(16 wchars * 4 = 64B) + ints (~26B) total ~90 bytes, well within 128. The key was matching the legacy field list exactly (16 not 31 chars for StartName).
- Always verify field counts and sizes against `code/oEvent.cpp` oCourseData section — I initially invented extra fields and wrong sizes which broke ALL tests by causing oEvent() ctor to throw "Out of bounds".
- The legacy `oCourseData` fields in order: NumberMaps(oIS16), StartName(16 chars), Climb(oIS16), RPointLimit(oIS32), RTimeLimit(oISTime), RReduction(oIS32), RReductionMethod(oIS8U), NoLatePoints(oIS8U), FirstAsStart(oIS8U), LastAsFinish(oIS8U), CControl(oIS16U), Shorten(oIS32).
- `getCommonControl()` returns `int` (not `bool`) — the stub had `bool getCommonControl()` which was wrong (but harmlessly 0/false = same as 0/no-common-control).
- oEvent control management (`getControl(id, create, includeVirtual)`) can be implemented inline in oEvent.h for the stub. The create=true path creates a new oControl in the Controls list and indexes it.
- The `distance(int* punches, int numPunches)` method uses protected members `controls[k]->nNumbers` and `controls[k]->Numbers[]` from oControl — oCourse is declared a `friend class oCourse` in oControl.h so this works.
- `getAdapetedCourse(const oCard&, ...)` and `distance(const oCard&)` are stubbed (return `this` / 0) since oCard isn't implemented until US-003f.
- `setLegLengths` throws `std::exception("Invalid parameter value")` in legacy — must change to `std::runtime_error("...")` for Linux.
- `oCourse::changedObject()` sets `oe->sqlCourses.changed = true` — need `SqlUpdated sqlCourses` in oEvent stub.
- `constructLoopKeys` uses `__int64` (from domain_header.h shim) for the hash set — works fine.
- SICard.h can be standalone (no domain_header.h needed, just cstdint/cstring) since it's a pure data struct shared by domain and io.

#### US-003e1: oClass Core Migration

**Description:** Migrate oClass.h/cpp core entity to src/domain/. Focus on the class entity itself, class-course assignment, and oData buffer alignment.

**Acceptance Criteria:**
- [ ] `oClass.h/cpp` migrated to `src/domain/` (full implementation, not stub)
- [ ] Class-course assignment works
- [ ] `oData` buffer correctly aligned for Linux wchar_t
- [ ] Domain library compiles

**Learnings from Previous Runs:**
- `oData`/`oDataOld` buffers in ALL domain entities (`oClass`, `oClub`, `oCourse`, `oControl`) MUST be declared `alignas(sizeof(wchar_t))`. glibc's vectorized `wcslen` (used inside `getDI()` string field reads) uses SIMD aligned reads; misaligned `oData` causes it to read across alignment boundaries and return a wrong (shorter) string length. Symptom: storing 14-char "Herrar 21 lång" retrieves as 13 chars. Fix: add `alignas(sizeof(wchar_t)) BYTE oData[dataSize]`.
- `oClass::dataSize` must scale with `sizeof(wchar_t)`: use `256 * static_cast<int>(sizeof(wchar_t)) + 64`. The legacy `512 + 64` assumed Windows wchar_t=2; on Linux (wchar_t=4) the correct size is `1088`. Using the legacy value causes "oDataContainer: Out of bounds" on construction.
- `oEventData` in the stub only needs the minimal set of fields actually accessed by `oClass` fee-calculation methods: EliteFee, EntryFee, YouthFee (Currency), OrdinaryEntry, SecondEntryDate (Date), LateEntryFactor, SecondEntryFactor (6-char string). Using the full legacy oEvent field set would overflow even a 1024-byte buffer on Linux.
- `oLegInfo()` default constructor initializes `startMethod = STTime` (value 0), NOT `STDrawn` (value 2). Test expectations must use `STTime` for the default start type.
- `oClass.cpp` requires three additional includes beyond oClass.h: `xmlparser.h` (for Write/Set XML methods), `../util/timeconstants.hpp` (for `timeConstSecond`), and `using std::swap` (for the merge() method).
- The `DODECLARETYPESYMBOLS` guard macro prevents double-definition of `StartTypeNames[]`/`LegTypeNames[]` arrays across translation units — `oClass.cpp` must define this before including `oClass.h`.

#### US-003e2: Class Configuration + Qualifications

**Description:** Migrate classconfiginfo, qualifications, and start method logic.

**Acceptance Criteria:**
- [ ] `classconfiginfo.h/cpp` migrated
- [ ] `qualifications.h/cpp` migrated
- [ ] Start methods (individual, mass, pursuit, etc.) preserved

**Learnings from Previous Runs:**
- `getPreceedingLeg(leg)` returns `leg - 1` when `legInfo[leg]` itself is non-parallel. For leg 2 (non-parallel) the result is 1, not 0. The method scans backward from `leg` to find the first non-parallel leg, then returns `k - 1`.

#### US-003e3: oClass Unit Tests

**Description:** Comprehensive unit tests for oClass operations including class-course assignment, start methods, and leg configuration.

**Acceptance Criteria:**
- [ ] Unit tests for oClass class-course assignment
- [ ] Unit tests for start methods and leg info
- [ ] Unit tests for fee calculation and configuration
- [ ] Typecheck passes

#### US-003f1: oCard Migration

**Description:** Migrate card readout entity. Cards record the physical punch data from SI units.

**Acceptance Criteria:**
- [ ] `oCard.h/cpp` migrated to `src/domain/` (full implementation, not stub)
- [ ] `SICard.h` (data structure) available in `src/util/` or `src/domain/`
- [ ] Card-punch matching logic preserved (punchString serialization)

**Learnings from Previous Runs:**
- `oCard` does NOT use oDataContainer DI buffers — serialization is via `punchString` (appendCodeString/decodeString round-trip). No `alignas` needed, no `dataSize` constant.
- `oCard::merge` comes from `code/oevent_transfer.cpp`, not `code/oCard.cpp` — grep all files for missing methods.
- `oCard::getTable` and `oFreePunch::getTable` call `make_shared<Table>(oe, capacity, title, name)` — Table stub needs a matching constructor.
- `xmlparser::getMemoryOutput` takes a `string&` reference parameter (not return value). `xmlparser::readMemory` (not `parseMemory`) is the correct in-memory parse method.
- Legacy oEvent methods used `oEvent *oe = this;` as a member self-reference; migrated code uses `this->` or direct member access instead.

#### US-003f2: oFreePunch Migration + Tests

**Description:** Migrate oFreePunch entity and write tests for card operations.

**Acceptance Criteria:**
- [ ] `oFreePunch.h/cpp` migrated to `src/domain/` (full implementation, not stub)
- [ ] Unit tests for oCard and oFreePunch operations
- [ ] Typecheck passes

**Learnings from Previous Runs:**
- `oFreePunch::changedObject()` sets `oe->sqlPunches.changed = true`. It is only called from `synchronize()` (not from `updateChanged()`). Tests that verify sqlPunches.changed must call `fp.synchronize(false)` after the mutation.
- `unexpectedOrder(startTime)` skips control punches (type >= 30) that are NOT used in the course (`isUsedInCourse() == false`). Testing with type=31 will always return false; use PunchFinish (type=2) to trigger the unexpected-order detection.
- `rehashPunches` returns early if `oe.punches.empty()` — safe to call from setTimeInt even when the punch is not registered in oe.punches (e.g., standalone punch objects in tests).
- `getControlIdFromPunch` is a complex method that needs full oRunner/oCourse. Simplified stub returns `oFreePunch::getControlHash(type, 0)` — sufficient until oRunner is migrated (US-003g).
- `addFreePunch(int,int,int,int,bool,bool)` uses socket/direct result in the full impl; simplified to add without socket logic in the stub.

#### US-003g1: oRunner Core Migration

**Description:** Migrate oRunner.h/cpp and oAbstractRunner to src/domain/. Focus on entity structure, relationships (class, club, card), and oEvent stub requirements.

**Acceptance Criteria:**
- [ ] `oRunner.h/cpp` migrated to `src/domain/` (full implementation replacing stub)
- [ ] `oAbstractRunner` extracted to own header with shared logic for oTeam
- [ ] Runner-class, runner-club, runner-card relationships work
- [ ] Domain library compiles

**Learnings from Previous Runs:**
- Circular dependency oEvent.h ↔ oRunner.h: solved by extracting oAbstractRunner to its own header that only forward-declares oEvent. DynamicValue methods that access oEvent::dataRevision must live in oRunner.cpp (not inline in header) since they need the full oEvent definition.
- oRunnerData must be initialized in oEvent constructor BEFORE any oRunner is created — getDI().initData() will segfault otherwise. Initialize with all DI fields (Fee, CardFee, Paid, PayMode, BirthYear, Bib, Rank, EntryDate, EntryTime, Sex, Nationality, Country, ExtId, ExtId2, Priority, Phone, RaceId, TimeAdjust, PointAdjust, Heat, Reference, InputResult, TransferFlags, StartGroup, Analysis, RankScore).
- `getClubCreate(int id, const wstring& name)` stub needed in oEvent.h for Set() XML deserialization: looks up club by id, creates if missing.
- `allocateCard(oRunner*)` stub needed in oEvent.h for Set() XML deserialization: push_back to Cards, set tOwner.
- `bibStartNoToRunnerTeam` and `classIdToRunnerHash` stubs (as map/shared_ptr members) needed to compile oRunner.cpp without errors.

#### US-003g2: Runner Status + Split Times + Tests

**Description:** Implement status computation (evaluateCard), split time computation, and unit tests.

**Acceptance Criteria:**
- [ ] Status computation (OK, DNS, DNF, DSQ, MP) works
- [ ] Split time computation works
- [ ] Unit tests for runner status, splits, and result computation
- [ ] Typecheck passes

**Learnings from Previous Runs:**
- oPunch::tIndex, tMatchControlId, and isUsed must be public (not protected) for evaluateCard to access them from oRunner.cpp.
- evaluateCard simplified from ~400 legacy lines to ~80 lines: sequential control matching only, skipping rogaining/multi-runner/loop-course logic. Status rules: keep DNS/CANCEL; no finish→DNF; missing controls→MP; else OK.

#### US-003h1: oTeam Core Migration

**Description:** Migrate oTeam.h/cpp to src/domain/ with team structure and runner membership.

**Acceptance Criteria:**
- [ ] `oTeam.h/cpp` migrated to `src/domain/` (full implementation)
- [ ] Team-runner membership works
- [ ] `oAbstractRunner` base class fixes applied (getFinishTimeS, compareBib, compareClubs, getBuiltinAdjustment)

**Learnings from Previous Runs:**
- oTeam.h must have `#include <set>` for `std::set<int>` in static method signatures. Without it, `set` is undeclared inside the header's function signature list.
- `using oAbstractRunner::getTotalStatus;` is required in oTeam to unhide the no-arg base overload — C++ name hiding applies when a derived class overrides one overload of an overloaded function.
- `ChangeType` is `oBase::ChangeType` (a scoped enum). In oTeam.cpp, the oEvent methods are not members of oBase, so `ChangeType` is not in scope without `using ChangeType = oBase::ChangeType;` at the top of the .cpp file.
- `oAbstractRunner::getFinishTimeS`, `compareBib`, and `compareClubs` must be implemented in oRunner.cpp — they are declared in oAbstractRunner.h but the implementation never existed. These are called by oTeam.cpp.
- `getBuiltinAdjustment()` must be added as `virtual int getBuiltinAdjustment() const { return 0; }` to oAbstractRunner.h — legacy code has it but the migration stub omitted it.
- `compareClubs` in legacy uses Win32 `CompareString`; safe to replace with wstring `<` comparison for cross-platform correctness.

#### US-003h2: Relay Leg Computation + Tests

**Description:** Implement relay leg computation, team result status propagation, and unit tests.

**Acceptance Criteria:**
- [ ] Relay leg computation works
- [ ] Team status propagation through legs works
- [ ] Unit tests for team operations
- [ ] Typecheck passes

**Learnings from Previous Runs:**
- Fields accessed directly by tests must be public: `tInTeam`, `tLeg` in oRunner.h and `tStartTime` in oAbstractRunner.h.
- `getLegStatus(leg, false, false)` with no runners and `Runners.empty()` returns `StatusUnknown` via early return; fix by propagating `tStatus != StatusUnknown` in that branch.

#### US-003i1: oEvent Core — Collections + Lifecycle

**Description:** Migrate oEvent.cpp core: entity collection ownership, competition create/open/save lifecycle, and control/runner lookup methods.

**Acceptance Criteria:**
- [ ] `oEvent.cpp` full implementation replaces skeleton stub
- [ ] Event owns and manages all entity collections
- [ ] Competition create/open/save lifecycle works
- [ ] `getControlByNumber`, `getRunnerByCardNo` work

**Learnings from Previous Runs:**
- `getControlByNumber(int number)` is the key method for punch-to-domain matching. Loops through Controls with `ctrl->hasNumber(number)` — not indexed, O(n) scan is fine for typical event sizes (< 100 controls).
- Full `getControlIdFromPunch` uses the runner→course→control chain: `getRunnerByCardNo(card, time)` → `r->getCourse(false)` → `c->getCourseControlId(k)` + `ctrl->hasNumber(type)`. Fallback to `getControlHash(type, 0)` if no course match.
- `getRunnerByCardNo` must scan by `r.getCardNo()` (not `r.cardNumber` directly since that field is protected in some builds) — use the accessor.
- Competition lifecycle: `tName`, `tDate`, `ZeroTime` (int, tenths-of-seconds from meos_util time units), `eventProperties` (map<string,wstring>) are the core fields. `newCompetition()` clears ALL collections including punchIndex, readPunchHash, advanceInformationPunches.
- `formatTimeHMS` and `convertAbsoluteTimeHMS` from meos_util.h are the right tools for HH:MM:SS time formatting. Note ZeroTime is in time-units (tenths-of-seconds), not raw seconds — match the test expectations accordingly.
- Integration tests revealed: `getZeroTimeNum()` returned 0 hardcoded in the old stub — must fix to return `ZeroTime` member for round-trip tests to work.

#### US-003i2: oEventDraw + oEventResult + Integration Tests

**Description:** Migrate drawing and result computation orchestration, plus integration tests.

**Acceptance Criteria:**
- [ ] `oEventDraw.cpp` — class drawing and start list generation works
- [ ] `oEventResult.cpp` — result computation orchestration works
- [ ] Integration tests covering entity relationships through oEvent
- [ ] Typecheck passes

**Learnings from Previous Runs:**
- `oEventDraw.cpp` uses `r->setStartTime(t, true, oBase::ChangeType::Update)` not the 2-arg variant — check the signature in oRunner.h.
- `oEventResult.cpp` uses `oAbstractRunner::TempResult(rt, st, pts, place)` and `r->tmpResult` to store computed places. These are already on the base class.

#### US-003j1: GeneralResult — Scoring Engine

**Description:** Migrate generalresult.h/cpp with the strategy pattern for pluggable scoring algorithms.

**Acceptance Criteria:**
- [ ] `generalresult.h/cpp` migrated (full implementation)
- [ ] Strategy pattern for scoring algorithms preserved
- [ ] `RunnerStatusOrderMap` and `TempResult` extensions in place
- [ ] `DynamicResult` compiles with required stubs

**Learnings from Previous Runs:**
- `RunnerStatusOrderMap` must be initialized at definition (not at app startup) so tests work without full application init. Use a local anonymous-namespace struct with a constructor.
- `TempResult` in `oAbstractRunner` needs extra fields used by the GeneralResult engine: `startTime`, `timeAfter`, `internalScore`, `outputTimes`, `outputNumbers`. Add them to the base class.
- `oRunner::getPlace()` stub returning 0 was insufficient once GeneralResult started using it. Fix to return `tmpResult.place` (set by calculateResults).
- `getSubSeconds()` belongs in `oAbstractRunner`, not `oRunner`, since `oTeam` also calls it via DynamicResult.
- `generalresult.h` must include `oAbstractRunner.h` (not just forward-declare) to make `pRunner`/`pTeam` typedefs available for the static calculateIndividualResults/calculateTeamResults signatures.
- `::sort` without `using std::sort` fails with GCC; always add the using declaration or fully qualify.
- Template member functions defined in .cpp work fine if only called within that same .cpp.
- `GeneralResultCtr::isDynamic()` checks `fileSource.empty()`, not `ptr->isDynamic()` — must use the file-constructor to get isDynamic()==true.
- Win32 `CompareString` in `GeneralResultInfo::compareResult` replaced with plain `wstring <` comparison — correct for cross-platform sorted results.
- `DynamicResult::prepareCalculations` for oRunner requires many stubs: getSplitAnalysis, getLegTimeAfter, getLegPlaces, getBirthYear, getBirthAge, getCheckTime, updateComputedResultFromTemp, getLegNumber, getTempStatus, getTempTime.

#### US-003j2: MetaList + oListInfo + Result Tests

**Description:** Migrate metalist output formatting, oListInfo, and write comprehensive tests for result computation.

**Acceptance Criteria:**
- [ ] `metalist.h/cpp` for output formatting migrated
- [ ] `oListInfo` migrated
- [ ] Result computation produces correct results for individual and relay formats
- [ ] Unit tests for result computation with known test data
- [ ] Typecheck passes

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
- Fixed-string `addVariableString("Name", 32, ...)` stores 32 bytes: on Linux (4-byte wchar_t) that's only 7 usable characters. Tests must use strings ≤7 chars for this field, or use a dynamic string field instead.
- The `gdioutput` stub methods (`getCY`, `addStringUT`, etc.) used in `parser.cpp` don't cause compile errors because those functions are only defined in the .cpp (not declared in parser.h) — the function body compiles against any type with those methods (duck typing via template-like lookup). The stub doesn't need them because `parser.cpp` includes its own gdioutput calls only reachable at link time if called.

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
- `sqlite3_exec` handles multi-statement SQL (semicolon-separated) natively — no need to split DDL into individual calls.
- `PRAGMA foreign_keys=ON` must be executed after each `sqlite3_open` — it is not persistent in the database file.
- `sqlite3_step` returns `SQLITE_DONE` for non-SELECT statements (INSERT/UPDATE/DELETE) and `SQLITE_ROW` for each result row of SELECT — check both in loops.
- For the migration rollback test: a bad migration's `execute()` throws, which triggers `rollback()` in the catch block, keeping the `_migrations` table version at the last good value.
- `DbResultSet` using vector<pair<string,string>> is readable but verbose for subsequent stories — future US-004b/c may want a helper like `firstColumnAsInt(rows)` or a named-column accessor.

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
- `toUTF8` is a free function in global namespace (not `MeOSUtil::toUTF8`) despite being declared inside `namespace MeOSUtil {}` — it's usable without namespace qualifier due to using-declarations in domain_header.h.
- `std::vector<DbParam>` initializer lists must be explicitly typed: `std::vector<DbParam>{DbParam::Text(...), ...}` — GCC cannot deduce the type through brace initialization alone.
- `sqlite3_column_type()` returns SQLITE_BLOB for blobs and SQLITE_TEXT/SQLITE_INTEGER/SQLITE_FLOAT/SQLITE_NULL for others; use it to decide between `sqlite3_column_blob()` and `sqlite3_column_text()` in queryMixed.
- `INSERT OR REPLACE` serves as a clean upsert for repositories — no need for separate INSERT and UPDATE SQL paths.
- Public `getOData()` + `static getODataBlobSize()` on domain entities provides clean access for blob persistence without making repository a friend class.
- oControl::getName() returns `const wstring&` — it returns the Name member directly; the name is stored in oData via oDataContainer but also as a direct field.
- When tests use `oe.getControl(id, true, false)` to create a control, call `ctrl->set(id, number, name)` to initialize it properly before inserting.
- SchemaV2::migrations() extends SchemaV1 by appending migration v2 (avoids code duplication; single call applies all migrations).

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
- `getOData()`/`getODataBlobSize()` accessors for repository persistence must be in a `public:` section of the header. In oRunner.h and oClass.h, the `protected:` section includes the data buffers — be careful to place the public accessors AFTER a `public:` keyword, not within the `protected:` block.
- SQLite FK constraints: `REFERENCES clubs(id) ON DELETE SET NULL` enforces FK on INSERT too. When `foreign_keys=ON`, inserting a runner with `club_id=7` fails if no club with id=7 exists in `clubs`. Use `DbParam::Null()` for FK columns when the domain value is 0 (no reference), and `parseIntOrZero()` on read (SQLite returns NULL as empty string, `stoi("")` throws).
- When testing runner FK relationships (club/class), you must first persist the referenced entities to the DB via their repositories — `oe.addClub()` only adds to the in-memory oEvent, not to SQLite.
- oCard uses punch_string serialization (via `getPunchString()` / `importPunches()`), NOT oDataContainer blobs. The `getDISize()` returns -1 for oCard — never attempt blob-based persistence for cards.
- `oClass::legInfo` is protected. Add `int getNumLegs() const { return (int)legInfo.size(); }` to the public section of oClass for repository use. This is distinct from `getNumLegNoParallel()` (which skips parallel legs).
- SchemaV3 extends SchemaV2 by appending v3 migration — the same "chain" pattern as SchemaV2 extends SchemaV1.
- `ALTER TABLE runners ADD COLUMN` works in SQLite within a multi-statement migration; multiple ALTER TABLE statements in one migration string execute correctly.

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
- `oTeam::getRunners()` (protected) is the correct serialization method for runner IDs; expose it publicly as `getRunnerIdString()` to avoid a friend declaration in the repository.
- `oTeam::oData` is in the `protected:` section (not `private:`), so public accessor methods can be added in a subsequent `public:` block without moving oData itself.
- `DbExtValue` uses `isBlobColumn` bool flag (not a `Type` enum) to distinguish blobs from text — use `row[i].second.isBlobColumn` to check for blob columns.
- For event properties, a simple `\x01`/`\x02`-delimited encoding works well without any JSON library; keys contain only ASCII identifiers so no escaping is needed.
- The events table uses `INSERT OR REPLACE` (upsert) with id=1 for the single-competition model — no need for UPDATE path.
- SchemaV4 extends SchemaV3 by chaining (same pattern as V2→V3→V4) — always call the previous version's `migrations()` and append.
- `setZeroTime(wstring)` parses the time string internally; `getZeroTimeNum()` returns the integer seconds value needed for DB storage.

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
- `nlohmann-json` find_package is `find_package(nlohmann_json CONFIG REQUIRED)` and the CMake target is `nlohmann_json::nlohmann_json`.
- `httplib::Server::set_error_handler` is called EVEN when the route handler explicitly sets `res.status >= 400` — it overrides the handler's body. Fix: in the error handler, return early if `res.body` is already non-empty.
- `httplib::Server::bind_to_port` + `listen_after_bind()` in a `std::thread` is the correct pattern for non-blocking server startup.
- Integration tests with httplib client need a short `sleep_for` (~50ms) after `server.start()` to ensure the listening socket is fully bound before clients connect.
- `httplib::Server::Handler` is `std::function<void(const Request&, Response&)>` — wrap that in a lambda to implement the try/catch+JSON logic.
- `httplib::status_message(status)` returns the standard HTTP status text (e.g. "Not Found" for 404) — useful for the fallback error handler.
- Port 18080 works for tests; avoid 8080 to not conflict with other services.

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
- `ApiRouter` is returned by value from `HttpServer::router()` — store it as a local variable, register all routes, then let it go out of scope. The routes are stored in the underlying httplib::Server and survive the router's destruction.
- Use `R"(/clubs/(\d+))"` (raw string regex) for path parameters; httplib populates `req.matches[1]` as the first capture group. Convert with `std::stoi(std::string(req.matches[1]))`.
- `oEvent::addClub(name)` (createId=0) auto-assigns IDs via `getFreeClubId()` which pre-increments qFreeClubId (starts at 1, so first auto-ID is 2). Don't assume ID=1 for first club.
- For controls, use `oe.getFreeControlId()` then `oe.getControl(newId, true, false)` to allocate a slot, then `ctrl->set(newId, number, name)` to initialize — this is the confirmed create pattern.
- `oEvent::getClubs()` is non-const; capture oEvent by non-const reference in lambdas. `getControls()` is const.
- `toUTF8()` and `fromUTF8()` are free functions in global namespace (not `MeOSUtil::`), accessible after including `meos_util.h`.
- Use separate test ports per fixture class to avoid bind conflicts when multiple fixtures run in the same binary: CLUBS_PORT=18081, CONTROLS_PORT=18082.
- Test fixture member initialization order matters for construction: declare `SQLiteDatabase db` before `ClubRepository clubRepo{db}` so db is constructed first.

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
- `oCourse::getControls() const` returns a **semicolon-separated** string of control IDs (not punch numbers). `importControls(str, setChanged, updateLegLengths)` accepts semicolons, commas, or spaces as separators.
- `oCourse::getControlNumbers()` returns **punch numbers** (31, 32, 33...), NOT control IDs — don't use this for the controlIds response field.
- For course POST: use `oe.getFreeCourseId()` then `oe.addCourse(name, length, newId)` — the third arg is the pre-allocated ID. Don't call getFreeCourseId() then addCourse without the ID as that auto-assigns a different ID.
- `oe.addClass(name, courseId)` creates a new class and optionally assigns a course. `oClass::setCourse(pCourse)` sets the course after creation; pass `nullptr` to clear the assignment.
- `oClass::setName(const wstring& name, bool manualSet)` takes a second bool arg (false = not manually set by user). Don't forget it.
- `getCourseId()` on oClass returns 0 when no course is assigned — this is the correct "no relationship" sentinel. Setting courseId=0 via PUT correctly clears the course.
- COURSES_PORT=18083, CLASSES_PORT=18084 — keep these distinct from CLUBS_PORT=18081, CONTROLS_PORT=18082 to avoid bind conflicts.
- The CourseApiTest fixture registers both `/courses` and `/controls` routes on the same server to support creating controls as dependencies for course tests.

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
- `ChangeType` is an inner enum class of `oBase` (i.e. `oBase::ChangeType`). In .cpp files outside domain/, add `using ChangeType = oBase::ChangeType;` after including `oBase.h`. Do NOT assume it's in global namespace.
- `okResponse()` returns `{"data": ...}` without a `"status"` key. Do NOT write tests that check `resp["status"] == "ok"` — the envelope has no status field.
- `oRunner` constructor `oRunner(oEvent* poe)` calls `poe->getFreeRunnerId()` automatically; then `oe.addRunner(r)` accepts the runner. Use this pattern for POST: construct locally, call `setName`, pass to `addRunner`, then set optional fields on the returned pointer.
- `removeRunner` did not exist in `oEvent.h` — had to add it inline (similar to `removeClub`: sets `Removed = true` and removes from `runnerIdIndex`).
- For filtering in list endpoints, prefer filtering after fetching (not by passing classId=N to `getRunners`) so the same code handles multiple optional filters uniformly.
- Pagination using `?page=` (1-based) and `?pageSize=` (default 100) returns `{"data": [...], "total": N, "page": P, "pageSize": PS}` — a standard envelope the frontend can consume.
- `oTeam::getRunnerIdString()` returns the private `getRunners()` string in format `"3;7;0;"` (semicolons, trailing semicolon, 0 for unassigned). Parse by splitting on `;` and skipping empty tokens.
- `oe.addTeam(name, clubId, classId)` is the cleanest factory for team creation. Sets up class/club linkage internally.
- `EventRepository::save(oe)` persists competition metadata (name, date, zeroTime, properties) to the `events` table with id=1 (singleton model).
- Test ports: RUNNERS=18085, TEAMS=18086, COMPETITION=18087 (after CLUBS=18081, CONTROLS=18082, COURSES=18083, CLASSES=18084).

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
- `oAbstractRunner::tmpResult` is `protected` — use the public accessor `getTempResult()` (returns `const TempResult&`) to access place/status/time from outside the class hierarchy.
- `oCard::cardNo` is `protected` — use `oCard::setCardNo(int c)` to set the card number after construction. Don't access cardNo directly from outside.
- `ApiException` constructor takes `ApiErrorCode` enum, not int. Use factory helpers: `internalError()`, `badRequest()`, `notFound()`, `conflict()`, `unprocessable()`.
- `oCard::PunchOrigin` is a nested enum class inside `oCard` — use `oCard::PunchOrigin::Original`, not bare `PunchOrigin::Original`.
- For card readout: construct `oCard newCard(&oe)`, call `newCard.setCardNo(cardNo)`, add punches with `newCard.addPunch(type, time, 0, 0, oCard::PunchOrigin::Original)`, then `oe.addCard(newCard)` returns `pCard`. Associate via `runner->addCard(pc, missingPunches)` which calls `evaluateCard` automatically.
- `oe.calculateResults(classId)` fills `tmpResult` on each runner with place/status/time — call it before reading results. Pass 0 for all classes.
- For start list sorting: sort by classId, then startTime. `getStartTime()` is on `oAbstractRunner` (accessible via pRunner).
- Test ports: RESULTS=18088, CARDS=18089 (extending the established pattern).
- `oPunch::PunchStart` and `oPunch::PunchFinish` are nested enum values in `oPunch::SpecialPunch` accessible as `oPunch::PunchStart` and `oPunch::PunchFinish` (no extra scoping needed due to unscoped enum).
- `setStatus(st, true, ChangeType::Update)` is the correct call for permanent manual status override; first arg=new status, second=updatePermanent, third=change type.

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
- `httplib::Server::set_mount_point("/", webRoot)` handles all common MIME types automatically including woff2, svg, js, css, html — no manual MIME mapping needed.
- Gzip compression requires the cpp-httplib zlib feature: `{"name": "cpp-httplib", "features": ["zlib"]}` in vcpkg.json. After cmake reconfigure, `CPPHTTPLIB_ZLIB_SUPPORT` is defined and httplib automatically compresses responses for Accept-Encoding: gzip clients.
- SPA fallback pattern: in `set_error_handler`, check `res.status == 404 && req.method == "GET" && req.path.find("/api/") == std::string::npos` — if true, read `webRoot + "/index.html"` and serve it with status 200. API 404s correctly bypass the fallback.
- `set_error_handler` is called once per request; it can be replaced by calling it again. `serveStatic()` replaces the constructor's error handler with one that adds SPA fallback logic while preserving the JSON error fallback for API paths.
- The CMake `add_custom_target(meos_web_build ...)` with `add_dependencies(meos meos_web_build)` triggers the frontend build automatically. Wrap with `if(NPM_EXECUTABLE)` so the build works on machines without npm.
- Passing `--outDir "${CMAKE_SOURCE_DIR}/web"` as absolute path to `npm run build --` ensures the output always goes to `{project_root}/web/` regardless of cwd. Update vite.config.ts to match (`build.outDir: '../../../web'`).
- The `--prefer-offline` flag to `npm install` speeds up CI by using cached packages.
- Test port 18090 used (extending the established pattern: 18081-18089 already taken).

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
- `oCard::fillPunches(gdioutput&, ...)` was the most complex gdioutput-using method but was never called from outside `oCard`. It called `gdi.clearList()` and `gdi.addItem()` — GUI display methods. Safe to remove entirely.
- The `#if 0` block in `parser.cpp` hiding dumpSymbols/dumpVariables was dead code from a previous migration step that added the block guard. Remove it in the final cleanup.
- Check ALL domain .cpp files for `#include "../util/gdioutput.h"` — many just included it defensively without using anything from it.
- The `oDataContainer` forward-declared both `class InputInfo;` and `class gdioutput;` in its header. Removing both simultaneously and all 4 GUI methods (buildDataFields x2, fillDataFields, saveDataFields) was clean since none were called from outside the class.
- Pattern for US-012 type cleanup: grep for `gdioutput` in src/, categorize each hit as (a) include → remove, (b) forward declaration → remove, (c) method declaration → remove from header + cpp, (d) dead code → remove.

### US-013: Utility Migration

**Description:** As a developer, I want shared utilities migrated to `src/util/` so they are available to all modules.

> **Note:** Split into logical groups.

#### US-013a1: String Conversions + Exceptions

**Description:** Migrate meos_util.h/cpp core: string conversion functions, meosexception, StringCache, and CP-1252 handling.

**Acceptance Criteria:**
- [ ] `meos_util.h/cpp` core string functions migrated to `src/util/` (cross-platform)
- [ ] `meosexception.h` migrated (inherits `std::runtime_error`)
- [ ] Wide/narrow string conversion (`string2Wide`, `wide2String`, `narrow`, `widen`, `toUTF8`, `fromUTF8`)
- [ ] Win32 function replacements (`_itow_s` → `to_wstring`, `sprintf_s` → `snprintf`)
- [ ] Unit tests for string conversion

**Learnings from Previous Runs:**
- On Linux LP64, `uint64_t == unsigned long` — defining both `itow(uint64_t)` and `itow(unsigned long)` causes a redefinition error. Guard with `#if !defined(__LP64__) && !defined(_LP64)`.
- Test files must add `using std::string; using std::wstring;` — meos_util.h does not inject `using namespace std`.
- Use manual UTF-8 codec (not deprecated `std::codecvt_utf8`) for fromUTF8/toUTF8; on Linux wchar_t is UTF-32 (4 bytes), no surrogates needed.
- CP-1252 decoding on Linux requires a lookup table for the 0x80-0x9F range; 0xA0-0xFF are Latin-1 compatible.
- `meosException` must inherit `std::runtime_error`, not `std::exception(msg)` — the string-arg constructor for std::exception is MSVC-only.
- `StringCache::getInstance()` must be `thread_local` static — legacy Win32 used `GetCurrentThreadId()` with a global which is NOT thread-safe.
- `getNameSplitPoint` references `lang.get().getGivenNames()` (the Localizer). Replace with simplified split-at-last-space heuristic until US-013d.
- `getNumberSuffix` scans right-to-left skipping trailing digits+spaces (not just digits), so "H21" → 21, "H21 Start 1" → 1, "H21 Start" → 0.

#### US-013a2: File Operations + HLS + Version Functions

**Description:** Migrate file operations (std::filesystem), HLS color class, version functions, and remaining Win32 replacements.

**Acceptance Criteria:**
- [ ] `std::filesystem` for file operations
- [ ] `HLS` class migrated (Win32 macros replaced)
- [ ] Version functions (`getMeosBuild`, `getMeosFullVersion`, etc.) migrated
- [ ] Unit tests for time formatting and file operations
- [ ] Typecheck passes

**Learnings from Previous Runs:**
- `HLS` class: replace Win32 `WORD`, `BYTE`, `GetRValue`, `RGB` macros with `uint16_t`, `uint8_t`, and inline `#ifndef _WIN32` helpers.
- Version functions (getMeosBuild, getMeosDate, getMeosFullVersion, etc.) live in `code/meosversion.cpp` — include them in meos_util.cpp to avoid a separate file.
- `timeconstants.hpp` was already in src/util/ as an untracked file — include it directly in meos_util.cpp.
- `_memicmp` → `strncasecmp`/`_strnicmp` guarded by `#ifdef _WIN32`.
- `xmlparser` previously used `gdioutput *utfConverter` for encoding; replaced with `toUTF8()`/`fromUTF8()`/`widen()` from meos_util.h — always UTF-8.
- `checkUTF` now accepts a `bool &hasDeclaration` parameter; only skips content before parse when declaration was found (handles XML fragments without `<?xml?>`).
- `oDataContainer::formatDouble` → local `snprintf("%.6g", v)` helper in xmlparser.cpp.
- `inplaceDecodeXML` moved to xmlparser.cpp as a file-local static function (not re-exported from meos_util.h).
- CSV `split()` function produces N-1 fields for N-1 delimiters in all-empty input (e.g., ";;" → 2 fields). This matches the original legacy behavior.
- `csvparser` core migrated to src/util/; domain-specific import methods (importOE_CSV, importOS_CSV, etc.) deferred to io/ module (future story).
- `csvparser::convertUTF` uses `std::filesystem::rename` instead of `_wrename`.

#### US-013b: Parsers

**Description:** Migrate XML and CSV parsing utilities.

**Acceptance Criteria:**
- [ ] `xmlparser.h/cpp` migrated to `src/util/`
- [ ] `csvparser.h/cpp` migrated to `src/util/`
- [ ] Unit tests for xmlparser and csvparser

**Learnings from Previous Runs:**
- Remove Win32 `gdioutput *utfConverter` from xmlparser; use `toUTF8`/`fromUTF8`/`widen` from meos_util.h directly.
- `checkUTF` must handle XML fragments (no `<?xml?>` declaration) — return `hasDeclaration=false` instead of throwing.
- `inplaceDecodeXML` was in legacy `code/meos_util.cpp` but NOT in `src/util/meos_util.cpp`; implement as a static helper in xmlparser.cpp.
- `oDataContainer::formatDouble` is a domain dependency in xmlparser.cpp; replace with `snprintf("%.6g", v)`.
- `csvparser` domain methods depend on `oEvent`/`SICard` etc — migrate only core I/O to util; defer domain imports to io/ layer.
- CSV `split()` with all-empty fields (e.g., ";;") produces 2 fields not 3 — this is existing behavior, test expectations must match.
- `std::filesystem::path(wstring)` works on Linux for opening files with wide paths.

#### US-013c: Time Handling

**Description:** Migrate time-related utilities.

**Acceptance Criteria:**
- [ ] `TimeStamp.h/cpp` migrated to `src/util/`
- [ ] `timeconstants.hpp` migrated to `src/util/`
- [ ] Unit tests for TimeStamp

**Learnings from Previous Runs:**
- TimeStamp.cpp was already largely cross-platform in the legacy code/ directory; just needed Win32 PCH/app headers removed (`StdAfx.h`, `meos.h`).
- `using namespace std;` in the legacy header must be removed — replace all bare `string`/`wstring` with `std::string`/`std::wstring`.
- The `stampCodeTime` comparison in `getStamp()` casts `Time` (unsigned) to int — keep the explicit cast to avoid signed/unsigned comparison warnings.
- `getStampStringN()` clamps the year to `getThisYear()` if year is in the future or before 2009 — this is business logic, not a bug.
- `getStamp(const string& sqlStampIn)` strips non-digit characters; the original resized to 15 but only wrote up to 14 digits — resize to actual `outIx` to avoid trailing garbage.
- `timeconstants.hpp` is needed by TimeStamp.cpp for `timeConstSecPerHour`; include it explicitly.

#### US-013d: Localization

**Description:** Migrate i18n/localization system.

**Acceptance Criteria:**
- [ ] `localizer.h/cpp` migrated to `src/util/` (fully cross-platform)
- [ ] Win32 resource loading replaced with `std::ifstream` + `codecvt`
- [ ] `.lng` files copied to `resources/lang/`
- [ ] Unit tests for localizer

**Learnings from Previous Runs:**
- Win32 resource loading (`FindResource`, `LoadResource`, `LockResource`) replaced with `std::ifstream`. The `addLangResource` API now accepts a file path (wstring) instead of a Win32 resource ID integer — legacy code passed integer IDs embedded in wstrings, new code always uses file paths.
- `std::exception("msg")` constructors (MSVC-specific) replaced with `std::runtime_error("msg")` throughout.
- The `translate()` static local buffer (`static int i`, `static wstring value[bsize]`) was NOT thread-safe in the legacy. Made `thread_local` for correct concurrent REST server use.
- `oWordList` depends on `oFreeImport.h` → `oEvent.h` (full domain chain). Stub it in the util layer with `lookup()` always returning false. The `getGivenNames()` method is preserved in the API for domain code to call, but returns the stub until US-003 migrates the domain.
- `permute()` from `random.h` replaced with `std::shuffle` using `std::random_device` + `std::mt19937` — semantically equivalent randomized insertion order.
- `fromUTF` (local Win32 function in legacy localizer.cpp) → `fromUTF8` from meos_util.h. `toUTF8(wstring)` (local Win32 function) → `toUTF8` from meos_util.h. No local overrides needed.
- The `× ` character (multiplication sign, U+00D7) used in the translate prefix-skip logic must be written as `L'\u00d7'` — the literal `L'×'` may cause encoding issues depending on source file encoding.
- `#include <filesystem>` is needed in localizer.cpp for `std::filesystem::path(wstring)` used to open .lng files with wide paths cross-platform.
- The `loadTableRaw` check `value[0] == 'Â'` in legacy (checking 0xC2 byte) strips a BOM artifact from NBSP-encoded values. Use `(unsigned char)value[0] == 0xC2` for clarity.
- The global `lang` variable is defined in localizer.cpp (not in meos_util.cpp or a separate file). Any translation unit that needs `lang` must include `localizer.h`.

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
- All MeOS time values (startTime, finishTime, runningTime, ZeroTime) are stored in time units where `timeUnitsPerSecond = 10` (i.e., 10 units per second). Import `timeconstants.hpp` and use `timeConstHour`, `timeConstMinute`, `timeConstSecond` for conversions. Never assume 1 unit = 1 second.
- `convertAbsoluteTimeHMS("10:00:00", -1)` returns 360000 (time units), not 36000 (seconds). 10 hours = 10 * 3600 * 10 = 360000 units.
- For IOF 3.0 XML export: `getRunningTime(false)` returns time units — divide by `timeConstSecond` before writing to `<Time>` in ResultList.
- `oEvent::getZeroTimeNum()` returns ZeroTime in time units (not seconds). Same for `getStartTime()`, `getFinishTime()`, `getRunningTime()`.
- IOF 3.0 status strings: "OK", "DidNotFinish", "MissingPunch", "Disqualified", "OverTime", "DidNotStart", "Cancelled", "NotCompeting", "Active" (for unknown/in-progress).
- ISO 8601 timezone stripping: scan for '+', '-', 'Z' at position >= 6 in the time string and truncate there. `convertAbsoluteTimeISO` accepts the raw HH:MM:SS and returns time units.
- The legacy `IOF30Interface` is deeply coupled with `gdioutput` — do NOT try to migrate it directly. Write a clean replacement `IofXmlInterface` that uses `xmlparser` from src/util and operates on domain objects directly.
- `xmlparser::startTag(tag, vector<wstring> propvalue)` takes key-value pairs as a flat vector of wstrings (interleaved: key0, val0, key1, val1, ...).
- `xmlobject::getObjectString(name, wstring&)` populates out; `getObjectInt(name)` returns 0 if not found. `getObjects(tag, xmlList&)` fills a list of all children with that tag.
- Control IDs in IOF 3.0 CourseData: `getControl(id, create=true, includeVirtual=false)` allocates the control slot if it doesn't exist. For course CourseControl, the Control child element contains the numeric ID as text (use `getInt()`).
- When constructing `oRunner` for `addRunner`, use `r.setClassId(id, false)` and `r.setClubId(id)` BEFORE calling `oe.addRunner(r)` — otherwise the returned `pRunner` may not reflect those changes.
- `xmlparser::getMemoryOutput(string&)` includes the full XML including the XML declaration. The `openMemoryOutput(false)` does NOT include a cut-mode header.
- Include `timeconstants.hpp` from `src/util/` in `src/io/IofXml.cpp` (it's a header-only constants file, no linking needed).

#### US-014b1: CSV Import

**Description:** Migrate CSV import for runners with OE/OS format detection.

**Acceptance Criteria:**
- [ ] CSV import for runners migrated to `src/io/`
- [ ] OE-CSV and OS-CSV format detection works
- [ ] Column-count heuristic and header skip logic preserved

**Learnings from Previous Runs:**
- **Heuristic Format Detection:** A simple column-count heuristic (`size() > 10`) is effective for distinguishing between simple and complex (OE-CSV) formats.
- **Domain Setup for Tests:** Proper result evaluation in tests requires setting up `oCard` and its `punches`, linking it to the `oRunner`, and calling `oRunner::evaluateCard`.
- **oEvent Entity Lists:** Use `oe.Runners`, `oe.Clubs`, etc. directly when a getter is missing in the migrated `oEvent` API.
- The OE CSV header row has >10 semicolons, so the import must always skip the FIRST line unconditionally before applying the column-count heuristic. The legacy `importOE_CSV` does this with `++it` before the loop. The column-count heuristic (`size() > 10`) is secondary — it skips partial/empty rows that may appear after the header.
- `oRunner(oEvent*)` auto-allocates ID via `poe->getFreeRunnerId()`. The resulting `oRunner` object must be passed to `oe.addRunner(r)` (no second parameter) which returns the stored pRunner. `addRunner` returns nullptr if `r.Id == 0`.
- `CsvIo::detectFormat()` uses the legacy `iscsv()` heuristic: if col[1] == "Descr"/"Namn"/"Descr."/"Navn" → OS, otherwise → OE. The column-count heuristic (`size() > 10`) is for distinguishing data rows from partial rows during import.
- For CSV file parsing on Linux, read the file as binary, strip UTF-8 BOM (EF BB BF), convert UTF-8 to wstring via `fromUTF8()`, then split by `\n` (skip `\r`).
- OE CSV enum: `OEstno=0, OEcard=1, OEid=2, OEsurname=3, OEfirstname=4, OEbirth=5, OEsex=6, OEstart=9, OEfinish=10, OEtime=11, OEstatus=12, OEclubno=13, OEclub=14, OEclubcity=15, OEnat=16, OEclassno=17, OEclassshortname=18, OEclassname=19, OEbib=23, OErent=35, OEfee=36, OEpaid=37, OEcourseno=38, OEcourse=39, OElength=40, OEpl=43`.

#### US-014b2: CSV Export + Round-Trip Tests

**Description:** Migrate CSV export for results/start lists and write round-trip tests.

**Acceptance Criteria:**
- [ ] CSV export for results/start lists migrated
- [ ] Unit tests for CSV round-trip (import → export → re-import)
- [ ] Typecheck passes

**Learnings from Previous Runs:**
- The export row vector must be sized to accommodate the highest column index: `OEfinishpunch = 45` requires `vector<string> row(46)`.
- `runner.getDCI()` (const version) is usable in the export loop because it's a const method callable on non-const references.
- `toUTF8()` / `fromUTF8()` are global free functions (not namespaced) — use directly in CsvIo.cpp.
- `convertAbsoluteTimeHMS(wstring, int daysZeroTime)` converts HH:MM:SS to internal time units. Pass `oe.getZeroTimeNum()` as second arg for day-boundary handling; pass -1 to disable days parsing.
- `formatTimeHMS(int)` formats internal time units as HH:MM:SS. Use this for CSV export of start/finish times.

#### US-014c: HTML Result Generation

**Description:** Migrate HTML output for results and lists.

**Acceptance Criteria:**
- [ ] HTML result generation migrated to `src/io/`
- [ ] Templates work cross-platform
- [ ] Unit tests for HTML output

**Learnings from Previous Runs:**
- **oListParam members:** Many I/O related members of `oListParam` were missing in the migrated domain layer and had to be added to support HTML generation.
- The legacy `HTMLWriter` is deeply tied to `gdioutput` (Win32 GUI object); it cannot be ported directly. The correct approach is to re-implement the template system cleanly without `gdioutput` dependency.
- The MeOS `.template` format: first line must be exactly `@MEOS EXPORT TEMPLATE`, second non-comment line is `tag@HumanName`, then `@HEAD/@OUTERPAGE/@INNERPAGE/@SEPARATOR/@END` sections. Lines starting with `%` are comments. `@USETABLE` switches to table layout.
- Placeholder replacement MUST be single-pass left-to-right picking longest match at each `@` to avoid `@T` corrupting `@TITLE`, `@N` corrupting `@NUMPAGE`, etc.
- `oListParam` was missing `set<int> selection` (class filter), `bool lockUpdate`, and a real `filterInclude()` implementation. These must be added for downstream code that uses them. Add to the constructor initializer list too (`lockUpdate(false)`).
- `HtmlTemplate::read(std::istream&)` makes the class testable without filesystem I/O — very useful for unit tests.
- `writeFile()` accepts a path as `wstring`; use `toUTF8(filename)` for the `std::ofstream` constructor on Linux (the Linux `ofstream` accepts `std::string`, not `wstring`).
- The `@END` marker closes both `@INNERPAGE`/`@SEPARATOR` sub-sections (returning to outer page accumulation) AND the top-level `@HEAD`/`@OUTERPAGE` sections — the state machine logic must handle both contexts.

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
- libharu is installed into the build's `vcpkg_installed/` directory (not the global vcpkg installed dir) when using manifest mode. Check `build/vcpkg_installed/x64-linux/include/hpdf.h`.
- The acceptance criteria says "Do not forward declare HPDF_Doc/HPDF_Page as void* — include hpdf.h in header". The legacy `pdfwriter.h` uses void* typedefs inside `#ifndef _HPDF_H` guards — the correct approach for the new code is `#include <hpdf.h>` directly in the header.
- Standard PDF Type 1 fonts in libharu: "Helvetica" (normal), "Helvetica-Bold" (bold), "Helvetica-Oblique" (italic). Pass `nullptr` as encoding (not "UTF-8") — these are Latin-1 fonts and don't support UTF-8 encoding strings.
- For in-memory PDF generation (testable without filesystem): call `HPDF_SaveToStream(doc)` first, then `HPDF_GetStreamSize(doc)` for the byte count, then `HPDF_ReadFromStream(doc, buf, &size)` to read the bytes. The result starts with `%PDF-` (magic bytes).
- The legacy `pdfwriter.cpp` used Windows `HFONT` and `GetFontData()` Win32 API for custom TTF fonts — this cannot be ported. The clean cross-platform approach is standard Type 1 PDF fonts + `narrow()` for text conversion (Latin-1 only).
- `HPDF_Page_TextOut` / `HPDF_Page_TextWidth` require the page to be in text mode (between `HPDF_Page_BeginText` / `HPDF_Page_EndText`). Call `selectFont` / `HPDF_Page_SetFontAndSize` inside the text block before measuring.
- `HPDF_Page_MoveTo` / `HPDF_Page_LineTo` / `HPDF_Page_Stroke` must be called OUTSIDE text mode (not between BeginText/EndText).
- libharu coordinates: origin (0,0) is at bottom-left of page; Y increases upward. For A4 portrait: width=595pt, height=842pt.
- Column width distribution: collect explicit `cell.width` from the first row that has them; distribute remaining space equally among columns with width=0.
- Multi-page support: check `y - lineHeight < kMargin` before drawing each row; call `addPage()` + reset `y = curY_` when the page is full.
- Test helpers that use `std::wstring` expressions cannot be passed to `initializer_list<const wchar_t*>` — add a separate `makeRowW(initializer_list<wstring>)` overload.

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

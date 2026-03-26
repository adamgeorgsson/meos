# PRD: MeOS Legacy Build Modernization

## Introduction

This PRD covers the build system modernization for the legacy `code/` directory: adding a CMake build and migrating vendored third-party libraries to vcpkg. These changes add new build configuration files (`CMakeLists.txt`, `vcpkg.json`, CI workflows) but do **not modify any existing source files** (`.cpp`/`.h`) in `code/`.

Removal of vendored directories from `code/` is handled separately in `prd-legacy-preparation.md` (US-P0m-cleanup), to be done after the vcpkg migrations are verified working.

This work can run **in parallel** with source code preparation (prd-legacy-preparation.md) and the main platform modernization effort.

### Execution Environment

This PRD is executed by an autonomous agent running on **Windows**. The agent can verify Windows builds locally using MSBuild or CMake. The agent should:

- Focus on making correct, mechanical, safe transformations
- Use standard C++ replacements that are known to work on both MSVC and GCC/Clang
- Avoid GCC-only constructs
- Prefer conservative changes — when in doubt, use the safer option

### Constraints

- All changes must be compatible with MSVC (Visual Studio 2022, v143)
- No modifications to existing `.cpp`/`.h` source files in `code/`
- The existing `.sln`/`.vcxproj` files are left intact
- Vendored directories are NOT removed in this PRD — that is a separate step after verification

## Non-Goals

- Modifying existing source files in `code/`
- Removing vendored directories (handled by US-P0m-cleanup in prd-legacy-preparation.md)
- Migrating any files to `src/`
- Adding tests

## Codebase Patterns (from Previous Runs)

These patterns were discovered during previous Ralph runs and should be followed:

- All 92 .cpp sources are in code/ root (non-recursive glob is correct; subdirs like minizip/, libharu/ contain headers only)
- code/minizip/ has ONLY headers (.h) — no .c files; the zlib/minizip implementation was compiled into the vendored zlibstat.lib
- CRITICAL: zlibstat.lib is a COMBINED library containing BOTH zlib AND minizip symbols — US-P0m4 (zlib) and US-P0m5 (minizip) MUST be done together
- vcpkg manifest mode: vcpkg.json must be in the cmake -S source directory (code/vcpkg.json for cmake -S code), NOT in repo root
- vcpkg CI path: C:/vcpkg is pre-installed on GitHub Actions windows-latest; env var VCPKG_INSTALLATION_ROOT points there
- vcpkg DLLs install to code/build/vcpkg_installed/x64-windows/bin/ when using cmake -S code -B code/build
- Vendored libs were in code/lib64/ (Release) and code/lib64_db/ (Debug) — after all migrations, these are unused by CMake build
- DPI manifest embedding: use VS_TOOL_OVERRIDE "MT" on the .xml source file (alternative: linker flags `/MANIFESTINPUT:${path} /MANIFEST:EMBED`)
- cmake -S code -B code/build -A x64 uses VS generator (not Ninja); output goes to code/build/Release/MeOS.exe
- Vendored headers with relative-path `#include` (quoted) always resolve before vcpkg include dirs — cannot be redirected without modifying source files

## User Stories

### US-P0l: CMake Build for Legacy Code

**Description:** Add a CMake build system for the `code/` directory. The existing `.vcxproj`/`.sln` files are retained for Visual Studio users, but CI switches to CMake.

**Acceptance Criteria:**
- [ ] `code/CMakeLists.txt` exists and builds MeOS.exe on Windows with MSVC via CMake
- [ ] All `.cpp` source files, resource files (`meos.rc`, `meoslang.rc`), and DPI manifest are compiled/embedded
- [ ] All external libraries linked: libharu, libmysql, libpng, zlib, RestBed (OpenSSL is statically linked into RestBed.lib — no separate link needed)
- [ ] All Windows system libraries linked: Msimg32, comctl32, odbc32, odbccp32, winmm, ws2_32, wininet
- [ ] Debug and Release configurations work (correct library paths: `lib64_db/` vs `lib64/`)
- [ ] `.github/workflows/build-legacy.yml` uses CMake instead of MSBuild
- [ ] CI artifact packaging still bundles all required DLLs
- [ ] `UNICODE` and `_UNICODE` compile definitions set (matching vcxproj `CharacterSet=Unicode`)
- [ ] C++17 standard enforced, disabled warnings match existing build (4267, 4244, 4018)
- [ ] The existing `.sln`/`.vcxproj` files are left intact

**Implementation Notes:**
- Create `code/CMakeLists.txt` as a standalone project (not a subdirectory of root `CMakeLists.txt`)
- External libraries: `code/lib64/` (Release) and `code/lib64_db/` (Debug) — use generator expressions
- Include paths: `code/libharu/`, `code/mysql/`, `code/png/`, `code/restbed/`, `code/minizip/`
- Link OpenSSL only through `RestBed.lib` (it has OpenSSL statically linked). Use `RestBed.lib` directly — skip `find_package(OpenSSL)` at this stage. OpenSSL DLLs (`libcrypto-1_1-x64.dll`, `libssl-1_1-x64.dll`) are still needed at runtime.
- CI: replace `msbuild` with `cmake -S code -B code/build -A x64` + `cmake --build code/build --config Release`
- Preserve `/GL` + `/LTCG` and `/MP` for Release
- **Must** define `UNICODE` and `_UNICODE` — the vcxproj uses `CharacterSet=Unicode` and all source code uses wide Win32 APIs (`wchar_t` / `LPCWSTR`). Without this, MSVC resolves to ANSI (`A`-suffix) APIs and compilation fails with hundreds of `wchar_t`→`LPCSTR` conversion errors.

**Known Pitfalls:**
- `WIN32` flag in `add_executable(MeOS WIN32 ...)` needed for GUI app (no console window)
- Library search order matters: `code/lib64/` must be found before system-installed versions
- Link OpenSSL exclusively via RestBed.lib (it bundles OpenSSL). A separate `find_package(OpenSSL)` causes configure failure when OpenSSL dev headers/libs are absent.

**Learnings from Previous Runs:**
- `file(GLOB SOURCES "*.cpp")` (non-recursive) correctly discovers all 92 source files without including library subdirectory sources.
- MSVC does NOT strip the `lib` prefix when resolving library names: `target_link_libraries(... libharu)` correctly resolves to `libharu.lib`.
- `target_link_directories` with `$<$<CONFIG:Release>:...>` generator expressions works correctly for selecting Release vs Debug library directories.
- `cmake --build ... --parallel` maps to `/m` in MSBuild (multi-processor compilation).
- The `Add MSBuild to PATH` / setup-msbuild step is not needed — cmake invokes MSBuild directly via the VS generator.
- Chocolatey OpenSSL 1.1 path `C:/Program Files/OpenSSL-Win64` is hardcoded and fragile — consider dynamic detection.

### US-P0m1: Migrate libharu to vcpkg

**Description:** Add libharu as a vcpkg dependency and update CMakeLists.txt to use `find_package` instead of manual library paths.

**Acceptance Criteria:**
- [ ] `libharu` declared in `vcpkg.json`
- [ ] `code/CMakeLists.txt` uses `find_package` instead of manual library paths for libharu
- [ ] CI builds succeed with vcpkg-provided libharu
- [ ] **Build verification:** `cmake -B build -G "Visual Studio 18 2026" -A x64` + `cmake --build build --config Release` in `code/` succeeds with zero errors

**Implementation Notes:**
- vcpkg port: `libharu`. Uses `#include "hpdf.h"` in `pdfwriter.cpp`
- libharu depends on zlib and libpng — vcpkg handles transitive dependencies
- **CMake integration:** `find_package(unofficial-libharu CONFIG REQUIRED)` — use package name `unofficial-libharu` and target `unofficial::libharu::hpdf` (the correct names for this port).

**Known Pitfalls:**
- Vendored version may differ from vcpkg port — check for API changes
- vcpkg exports libharu under the `unofficial-libharu` namespace — use `find_package(unofficial-libharu CONFIG REQUIRED)` (the port name is `unofficial-libharu`)

**Learnings from Previous Runs:**
- CONFIRMED: Correct CMake package: `unofficial-libharu`, target: `unofficial::libharu::hpdf`. These are the only working names for this port.
- pdfwriter.cpp uses bare `#include "hpdf.h"` — resolved via vcpkg's imported target include dirs (not relative to source dir, since there is no hpdf.h in code/).
- Removing code/libharu from target_include_directories is safe — vcpkg's haru imported target provides the include path automatically.
- Vendored libharu is 2.3.0RC2; vcpkg provides 2.4.x — API differences may cause compilation errors in pdfwriter.cpp.

### US-P0m2: Migrate MySQL Connector/C to vcpkg

**Description:** Add MySQL Connector/C as a vcpkg dependency and update CMakeLists.txt to use `find_package` instead of manual library paths.

**Acceptance Criteria:**
- [ ] `libmysql` declared in `vcpkg.json`
- [ ] `code/CMakeLists.txt` uses `find_package` instead of manual library paths for MySQL
- [ ] `libmysql.dll` available at runtime via vcpkg
- [ ] CI builds succeed with vcpkg-provided MySQL
- [ ] **Build verification:** `cmake -B build -G "Visual Studio 18 2026" -A x64` + `cmake --build build --config Release` in `code/` succeeds with zero errors

**Implementation Notes:**
- vcpkg port: `libmysql`. Vendored headers are MySQL 5.7 — check API compatibility
- `code/mysqlwrapper.h` includes `<mysql.h>` — verify vcpkg provides same include path
- `libmysql.dll` needed at runtime — ensure CI artifact packaging picks up vcpkg-installed DLL

**Known Pitfalls:**
- MariaDB Connector/C (`libmariadb`) is an alternative with better cross-platform support
- `code/mysql/` has a nested `mysql/` subdirectory — ensure all nested includes resolve

**Learnings from Previous Runs:**
- vcpkg CMake: `find_package(unofficial-libmysql CONFIG REQUIRED)`, target `unofficial::libmysql::libmysql`. DLL name is `libmysql.dll` (same as vendored).
- Vendored MySQL headers are 5.7.31; vcpkg provides 8.x — ABI mismatch risk if struct layouts changed between versions. Since source files cannot be modified, vendored headers (5.7) are always used for compilation while linking against vcpkg's 8.x library.
- code/mysql/ MUST remain in target_include_directories — vendored `mysql.h` internally uses angle-bracket includes like `#include <mysql/client_plugin.h>` that rely on the include path.
- If `libmysql` port fails on CI, `libmariadb` is the fallback: `find_package(unofficial-libmariadb CONFIG REQUIRED)`, target `unofficial::libmariadb::libmariadb`, DLL `libmariadb.dll`.

### US-P0m3: Migrate libpng to vcpkg

**Description:** Add libpng as a vcpkg dependency and update CMakeLists.txt to use `find_package(PNG)` instead of manual library paths.

**Acceptance Criteria:**
- [ ] `libpng` declared in `vcpkg.json`
- [ ] `code/CMakeLists.txt` uses `find_package(PNG)` instead of manual library paths
- [ ] CI builds succeed with vcpkg-provided libpng
- [ ] **Build verification:** `cmake -B build -G "Visual Studio 18 2026" -A x64` + `cmake --build build --config Release` in `code/` succeeds with zero errors

**Implementation Notes:**
- vcpkg port: `libpng`. Uses `#include "png.h"` in `image.cpp`. Depends on zlib (transitive via vcpkg).

**Known Pitfalls:**
- Vendored `pnglibconf.h` may contain custom configuration — compare with vcpkg version

**Learnings from Previous Runs:**
- image.cpp uses `#include "png/png.h"` (quoted, relative path) — resolves to code/png/png.h (vendored headers). vcpkg's PNG::PNG include path is NOT used for this include. Only the library (libpng16.lib) comes from vcpkg.
- Potential ABI mismatch: vendored headers (code/png/) with vcpkg's libpng.lib could cause issues if pnglibconf.h settings differ. This is an inherent limitation of the no-source-modification constraint.
- `find_package(PNG REQUIRED)` works with vcpkg toolchain without CONFIG keyword (uses CMake's FindPNG module).
- vcpkg DLL name is versioned: `libpng16.dll` — use glob `libpng*.dll` in CI packaging.
- Removing code/png/ from target_include_directories is safe — image.cpp resolves the include via relative path from code/.

### US-P0m4: Migrate zlib to vcpkg

**Description:** Add zlib as a vcpkg dependency and update CMakeLists.txt to use `find_package(ZLIB)` instead of manual library paths.

**Acceptance Criteria:**
- [ ] `zlib` declared in `vcpkg.json`
- [ ] `code/CMakeLists.txt` uses `find_package(ZLIB)` instead of manual library paths
- [ ] CI builds succeed with vcpkg-provided zlib
- [ ] **Build verification:** `cmake -B build -G "Visual Studio 18 2026" -A x64` + `cmake --build build --config Release` in `code/` succeeds with zero errors

**Known Pitfalls:**
- `code/minizip/` contains both zlib headers and minizip source — only the zlib libs are replaced here; minizip source handled by US-P0m5

**Learnings from Previous Runs:**
- zlibstat.lib bundles BOTH zlib AND minizip (confirmed via `dumpbin /SYMBOLS`). US-P0m4 and US-P0m5 are inseparable — replacing zlibstat with ZLIB::ZLIB alone leaves minizip symbols (zipOpen, unzOpen, fill_win32_filefunc64W) unresolved. Always do them together.
- `x64-windows` (dynamic) triplet means zlib1.dll is a runtime dependency. CI packaging uses `Test-Path` to conditionally copy, handling static/dynamic variance.

### US-P0m5: Migrate minizip to vcpkg

**Description:** Add minizip as a vcpkg dependency and update CMakeLists.txt to use vcpkg integration instead of compiling minizip sources directly.

**Acceptance Criteria:**
- [ ] `minizip` declared in `vcpkg.json`
- [ ] `code/CMakeLists.txt` uses vcpkg integration instead of compiling minizip sources directly
- [ ] CI builds succeed with vcpkg-provided minizip
- [ ] **Build verification:** `cmake -B build -G "Visual Studio 18 2026" -A x64` + `cmake --build build --config Release` in `code/` succeeds with zero errors

**Implementation Notes:**
- vcpkg port: `minizip`. `code/zip.cpp` includes `"minizip/zip.h"` — verify vcpkg provides same include path
- `iowin32.h`/`iowin32.c` is the Windows-specific I/O backend — vcpkg port provides this on Windows

**Known Pitfalls:**
- Vendored minizip may include local modifications — diff against upstream before removing
- Must remove minizip `.c` compilation units from CMakeLists.txt when switching to vcpkg library

**Learnings from Previous Runs:**
- vcpkg minizip CMake: `find_package(unofficial-minizip CONFIG REQUIRED)`, target `unofficial::minizip::minizip`. Headers install to `include/minizip/` matching zip.cpp's `#include "minizip/unzip.h"`.
- No minizip .c files were compiled separately in our CMakeLists.txt — the real migration was replacing zlibstat.lib (done together with US-P0m4).
- iowin32.h is included in the Windows vcpkg minizip build.

### US-P0m6: Migrate restbed to vcpkg

**Description:** Add restbed as a vcpkg dependency and update CMakeLists.txt to use `find_package(restbed)` instead of manual library paths.

**Acceptance Criteria:**
- [ ] `restbed` declared in `vcpkg.json`
- [ ] `code/CMakeLists.txt` uses `find_package(restbed)` instead of manual library paths
- [ ] CI builds succeed with vcpkg-provided restbed
- [ ] **Build verification:** `cmake -B build -G "Visual Studio 18 2026" -A x64` + `cmake --build build --config Release` in `code/` succeeds with zero errors

**Implementation Notes:**
- vcpkg port: `restbed`. Project uses `#include <restbed>` umbrella header. Depends on OpenSSL (coordinate with US-P0m7).

**Known Pitfalls:**
- Verify `[ssl]` feature is enabled in vcpkg dependency if SSL support is needed
- Vendored restbed is from 2017 (Corvusoft era) — vcpkg version may have API changes
- restbed depends on ASIO transitively

**Learnings from Previous Runs:**
- vcpkg CMake: `find_package(unofficial-restbed CONFIG REQUIRED)`, target `unofficial::restbed::restbed`. Confirmed via portfile: `vcpkg_cmake_config_fixup(PACKAGE_NAME unofficial-restbed)`.
- SSL feature name is `openssl` (not `ssl`): declare as `{"name": "restbed", "features": ["openssl"]}` in vcpkg.json.
- Vendored headers are permanently used: `restserver.cpp` includes `"restbed/restbed"` (quoted, relative path) which always resolves to code/restbed/ before any vcpkg include dir. Cannot be redirected without modifying source files.
- code/restbed must remain in target_include_directories — the umbrella header internally includes `"corvusoft/restbed/uri.hpp"` etc. (relative paths resolving via code/restbed/).
- After restbed migration, `target_link_directories` can be removed entirely — RestBed.lib was the last library from lib64/lib64_db/.
- After all US-P0m* migrations, code/lib64/ and code/lib64_db/ are completely unused by the CMake build.

### US-P0m7: Migrate OpenSSL to vcpkg

**Description:** Replace system-installed OpenSSL (Chocolatey on CI) with vcpkg for consistent cross-platform builds.

**Acceptance Criteria:**
- [ ] `openssl` declared in `vcpkg.json`
- [ ] `code/CMakeLists.txt` uses `find_package(OpenSSL)` with vcpkg integration
- [ ] CI workflow no longer needs a separate OpenSSL installation step
- [ ] CI builds succeed
- [ ] **Build verification:** `cmake -B build -G "Visual Studio 18 2026" -A x64` + `cmake --build build --config Release` in `code/` succeeds with zero errors

**Implementation Notes:**
- OpenSSL is a transitive dependency of restbed — if US-P0m6 is done, this may only require adding it to `vcpkg.json` and removing the Chocolatey step from CI

**Known Pitfalls:**
- OpenSSL 1.1 vs 3.x API differences — verify restbed compatibility with vcpkg version

**Learnings from Previous Runs:**
- OpenSSL DLL naming differs by version: 1.1 uses `libssl-1_1-x64.dll`/`libcrypto-1_1-x64.dll`; 3.x uses `libssl-3-x64.dll`/`libcrypto-3-x64.dll`. Always use glob patterns in CI packaging and artifact upload.
- Prior to this migration, no `find_package(OpenSSL)` call existed. RestBed.lib's embedded `#pragma comment(lib, ...)` pragmas caused MSVC to auto-link against libssl.lib/libcrypto.lib from Chocolatey. The `-DOPENSSL_ROOT_DIR` flag in CI had no effect and was a redundant artefact.
- RestBed.lib (vendored static lib) was compiled against OpenSSL 1.1. vcpkg provides 3.x — if RestBed.lib calls removed APIs, linker will fail. Fix: implement US-P0m6 (restbed migration) first, which replaces the vendored lib with one built against the same OpenSSL version.
- OpenSSL DLLs also exist in code/dll64/ (from old build system) — unused after vcpkg migration but keep as fallback until verified.
- `find_package(OpenSSL REQUIRED)` works with vcpkg toolchain without CONFIG keyword.

### US-P0l-data: Bundle Runtime Data Files

**Description:** MeOS requires a set of data files (CSV, XML, MWD, wclubs/wpersons, listdef, lxml, meos, template, brules, cardsystem) to be present alongside `meos.exe` at runtime. The CMake build and CI workflow must copy these files into the build output directory so the application starts without errors like "Error processing clubnamemap.csv".

**Acceptance Criteria:**
- [ ] `code/CMakeLists.txt` copies `code/Lists/*` to the build output directory (`$<TARGET_FILE_DIR:MeOS>`) as a post-build step
- [ ] CI workflow bundles `code/Lists/*` into the artifact alongside DLLs and `meos.exe`
- [ ] MeOS starts without "Error processing" warnings when run from the build directory
- [ ] Both Debug and Release configurations include the data files

**Implementation Notes:**
- Runtime data files are checked into `code/Lists/` (71 files: CSV, XML, MWD, listdef, lxml, meos, template, brules, cardsystem, wclubs, wpersons)
- Key files loaded at startup:
  - `clubnamemap.csv` (loaded by `oClub::loadNameMap()`, triggers error dialog if missing)
  - `baseclass.xml`, `wfamily.mwd`, `wgiven.mwd`, `wclub.mwd`, `wclass.mwd`, `database.wclubs`, `database.wpersons` (copied by `Setup()` in `meos.cpp`)
  - `hired_card_default.csv` (loaded by `newcompetition.cpp`)
- CMake: use `add_custom_command(TARGET MeOS POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Lists $<TARGET_FILE_DIR:MeOS>)`
- The `exePath` fallback in `oClub.cpp` looks for `clubnamemap.csv` next to the exe — copying to the output dir satisfies this

**Known Pitfalls:**
- Some data files may be locale-specific — verify the Swedish installation files are appropriate defaults
- `*.mwd` files are word-dictionary files used for fuzzy matching — they may be large
- Do not include DLLs from the installation directory — those come from vcpkg now

### US-P0l-readme: Keep README Build Instructions Current

**Description:** The "Legacy MeOS" section in `README.md` must accurately reflect the current build process. When build prerequisites, commands, or runtime requirements change (e.g., libraries migrated to vcpkg, toolchain flags added), update `README.md` accordingly.

**Acceptance Criteria:**
- [ ] `README.md` "Legacy MeOS" section lists vcpkg as a prerequisite (not Chocolatey OpenSSL)
- [ ] Configure command includes `-DCMAKE_TOOLCHAIN_FILE` and `-DVCPKG_TARGET_TRIPLET=x64-windows`
- [ ] Runtime DLL section reflects that vcpkg handles dependency DLLs automatically
- [ ] `vcpkg.json` dependency list is mentioned so users know libraries are auto-installed
- [ ] Instructions are verified against CI workflow (`.github/workflows/build-legacy.yml`)

**Implementation Notes:**
- This is a living document — update it whenever a US-P0m* migration changes prerequisites or build steps
- The README commands should match what CI does (the CI workflow is the source of truth)

**Known Pitfalls:**
- README can silently drift from actual build process — verify after each migration

### US-P0l-ci: Keep CI Workflow Current

**Description:** `.github/workflows/build-legacy.yml` must accurately reflect the current build process. When vcpkg dependencies change, DLLs are added/removed, or build steps are modified, update the workflow accordingly.

**Acceptance Criteria:**
- [ ] Configure step uses only vcpkg toolchain (Chocolatey and manual library install steps replaced)
- [ ] Package step copies all vcpkg-provided DLLs needed at runtime (matches `vcpkg.json` dependencies)
- [ ] Upload artifact list includes all required DLLs (no stale references to removed libraries)
- [ ] No orphaned steps (e.g., Chocolatey install steps after OpenSSL moved to vcpkg)
- [ ] Workflow is verified to pass after each US-P0m* migration

**Implementation Notes:**
- The workflow is the source of truth for how the project builds — keep it minimal and correct
- DLL glob patterns (e.g., `libharu*.dll`) are preferred over exact filenames to handle version changes
- When a library moves to vcpkg, remove any manual install/download steps for that library
- Update the artifact upload paths if DLL names change

**Known Pitfalls:**
- vcpkg may produce different DLL names across versions (e.g., `libssl-1_1-x64.dll` vs `libssl-3-x64.dll`) — use glob patterns
- Forgetting to add a new DLL to the package step causes runtime failures that CI build step won't catch
- `x64-windows` triplet produces dynamic libraries — if triplet changes, DLL list changes too
- The core-migration `cpp.yml` workflow uses `lukka/run-vcpkg@v11` with default glob `**/vcpkg.json` — having a second `vcpkg.json` in `code/` causes the action to fail with "found multiple times". If both files coexist, `cpp.yml` must scope its glob (e.g., `vcpkgJsonGlob: 'vcpkg.json'`) to avoid ambiguity.

## Dependency Order

```
US-P0l (CMake build)        — independent, do early for CI feedback
US-P0l-data (data files)    — depends on US-P0l
US-P0l-readme (README)      — depends on US-P0l, update after each US-P0m* migration
US-P0l-ci (CI workflow)     — depends on US-P0l, update after each US-P0m* migration
US-P0m4 (zlib vcpkg)        — depends on US-P0l
US-P0m3 (libpng vcpkg)      — depends on US-P0m4
US-P0m1 (libharu vcpkg)     — depends on US-P0m3 and US-P0m4
US-P0m5 (minizip vcpkg)     — depends on US-P0m4
US-P0m7 (OpenSSL vcpkg)     — depends on US-P0l
US-P0m6 (restbed vcpkg)     — depends on US-P0m7
US-P0m2 (MySQL vcpkg)       — depends on US-P0l
```

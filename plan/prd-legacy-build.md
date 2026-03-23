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

## User Stories

### US-P0l: CMake Build for Legacy Code

**Description:** Add a CMake build system for the `code/` directory. The existing `.vcxproj`/`.sln` files are retained for Visual Studio users, but CI switches to CMake.

**Acceptance Criteria:**
- [ ] `code/CMakeLists.txt` exists and builds MeOS.exe on Windows with MSVC via CMake
- [ ] All `.cpp` source files, resource files (`meos.rc`, `meoslang.rc`), and DPI manifest are compiled/embedded
- [ ] All external libraries linked: libharu, libmysql, libpng, zlib, RestBed, OpenSSL
- [ ] All Windows system libraries linked: Msimg32, comctl32, odbc32, odbccp32, winmm, ws2_32, wininet
- [ ] Debug and Release configurations work (correct library paths: `lib64_db/` vs `lib64/`)
- [ ] `.github/workflows/build-legacy.yml` uses CMake instead of MSBuild
- [ ] CI artifact packaging still bundles all required DLLs
- [ ] C++17 standard enforced, disabled warnings match existing build (4267, 4244, 4018)
- [ ] The existing `.sln`/`.vcxproj` files are left intact

**Implementation Notes:**
- Create `code/CMakeLists.txt` as a standalone project (not a subdirectory of root `CMakeLists.txt`)
- External libraries: `code/lib64/` (Release) and `code/lib64_db/` (Debug) — use generator expressions
- Include paths: `code/libharu/`, `code/mysql/`, `code/png/`, `code/restbed/`, `code/minizip/`
- For OpenSSL: use `find_package(OpenSSL)` (CI installs via Chocolatey)
- CI: replace `msbuild` with `cmake -S code -B code/build -A x64` + `cmake --build code/build --config Release`
- Preserve `/GL` + `/LTCG` and `/MP` for Release

**Known Pitfalls:**
- `find_package(OpenSSL)` may need `OPENSSL_ROOT_DIR` set to the Chocolatey install path on CI
- `WIN32` flag in `add_executable(MeOS WIN32 ...)` needed for GUI app (no console window)
- Library search order matters: `code/lib64/` must be found before system-installed versions

### US-P0m1: Migrate libharu to vcpkg

**Description:** Add libharu as a vcpkg dependency and update CMakeLists.txt to use `find_package` instead of manual library paths.

**Acceptance Criteria:**
- [ ] `libharu` declared in `vcpkg.json`
- [ ] `code/CMakeLists.txt` uses `find_package` instead of manual library paths for libharu
- [ ] CI builds succeed with vcpkg-provided libharu

**Implementation Notes:**
- vcpkg port: `libharu`. Uses `#include "hpdf.h"` in `pdfwriter.cpp`
- libharu depends on zlib and libpng — vcpkg handles transitive dependencies

**Known Pitfalls:**
- Vendored version may differ from vcpkg port — check for API changes

### US-P0m2: Migrate MySQL Connector/C to vcpkg

**Description:** Add MySQL Connector/C as a vcpkg dependency and update CMakeLists.txt to use `find_package` instead of manual library paths.

**Acceptance Criteria:**
- [ ] `libmysql` declared in `vcpkg.json`
- [ ] `code/CMakeLists.txt` uses `find_package` instead of manual library paths for MySQL
- [ ] `libmysql.dll` available at runtime via vcpkg
- [ ] CI builds succeed with vcpkg-provided MySQL

**Implementation Notes:**
- vcpkg port: `libmysql`. Vendored headers are MySQL 5.7 — check API compatibility
- `code/mysqlwrapper.h` includes `<mysql.h>` — verify vcpkg provides same include path
- `libmysql.dll` needed at runtime — ensure CI artifact packaging picks up vcpkg-installed DLL

**Known Pitfalls:**
- MariaDB Connector/C (`libmariadb`) is an alternative with better cross-platform support
- `code/mysql/` has a nested `mysql/` subdirectory — ensure all nested includes resolve

### US-P0m3: Migrate libpng to vcpkg

**Description:** Add libpng as a vcpkg dependency and update CMakeLists.txt to use `find_package(PNG)` instead of manual library paths.

**Acceptance Criteria:**
- [ ] `libpng` declared in `vcpkg.json`
- [ ] `code/CMakeLists.txt` uses `find_package(PNG)` instead of manual library paths
- [ ] CI builds succeed with vcpkg-provided libpng

**Implementation Notes:**
- vcpkg port: `libpng`. Uses `#include "png.h"` in `image.cpp`. Depends on zlib (transitive via vcpkg).

**Known Pitfalls:**
- Vendored `pnglibconf.h` may contain custom configuration — compare with vcpkg version

### US-P0m4: Migrate zlib to vcpkg

**Description:** Add zlib as a vcpkg dependency and update CMakeLists.txt to use `find_package(ZLIB)` instead of manual library paths.

**Acceptance Criteria:**
- [ ] `zlib` declared in `vcpkg.json`
- [ ] `code/CMakeLists.txt` uses `find_package(ZLIB)` instead of manual library paths
- [ ] CI builds succeed with vcpkg-provided zlib

**Known Pitfalls:**
- `code/minizip/` contains both zlib headers and minizip source — only the zlib libs are replaced here; minizip source handled by US-P0m5

### US-P0m5: Migrate minizip to vcpkg

**Description:** Add minizip as a vcpkg dependency and update CMakeLists.txt to use vcpkg integration instead of compiling minizip sources directly.

**Acceptance Criteria:**
- [ ] `minizip` declared in `vcpkg.json`
- [ ] `code/CMakeLists.txt` uses vcpkg integration instead of compiling minizip sources directly
- [ ] CI builds succeed with vcpkg-provided minizip

**Implementation Notes:**
- vcpkg port: `minizip`. `code/zip.cpp` includes `"minizip/zip.h"` — verify vcpkg provides same include path
- `iowin32.h`/`iowin32.c` is the Windows-specific I/O backend — vcpkg port provides this on Windows

**Known Pitfalls:**
- Vendored minizip may include local modifications — diff against upstream before removing
- Must remove minizip `.c` compilation units from CMakeLists.txt when switching to vcpkg library

### US-P0m6: Migrate restbed to vcpkg

**Description:** Add restbed as a vcpkg dependency and update CMakeLists.txt to use `find_package(restbed)` instead of manual library paths.

**Acceptance Criteria:**
- [ ] `restbed` declared in `vcpkg.json`
- [ ] `code/CMakeLists.txt` uses `find_package(restbed)` instead of manual library paths
- [ ] CI builds succeed with vcpkg-provided restbed

**Implementation Notes:**
- vcpkg port: `restbed`. Project uses `#include <restbed>` umbrella header. Depends on OpenSSL (coordinate with US-P0m7).

**Known Pitfalls:**
- Verify `[ssl]` feature is enabled in vcpkg dependency if SSL support is needed
- Vendored restbed is from 2017 (Corvusoft era) — vcpkg version may have API changes
- restbed depends on ASIO transitively

### US-P0m7: Migrate OpenSSL to vcpkg

**Description:** Replace system-installed OpenSSL (Chocolatey on CI) with vcpkg for consistent cross-platform builds.

**Acceptance Criteria:**
- [ ] `openssl` declared in `vcpkg.json`
- [ ] `code/CMakeLists.txt` uses `find_package(OpenSSL)` with vcpkg integration
- [ ] CI workflow no longer needs a separate OpenSSL installation step
- [ ] CI builds succeed

**Implementation Notes:**
- OpenSSL is a transitive dependency of restbed — if US-P0m6 is done, this may only require adding it to `vcpkg.json` and removing the Chocolatey step from CI

**Known Pitfalls:**
- OpenSSL 1.1 vs 3.x API differences — verify restbed compatibility with vcpkg version

## Dependency Order

```
US-P0l (CMake build)        — independent, do early for CI feedback
US-P0m4 (zlib vcpkg)        — depends on US-P0l
US-P0m3 (libpng vcpkg)      — depends on US-P0m4
US-P0m1 (libharu vcpkg)     — depends on US-P0m3 and US-P0m4
US-P0m5 (minizip vcpkg)     — depends on US-P0m4
US-P0m7 (OpenSSL vcpkg)     — depends on US-P0l
US-P0m6 (restbed vcpkg)     — depends on US-P0m7
US-P0m2 (MySQL vcpkg)       — depends on US-P0l
```

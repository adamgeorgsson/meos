# PRD: MeOS Legacy Code Preparation

## Introduction

This PRD covers the preparatory work needed in the legacy `code/` directory *before* migrating files to the new `src/` modular layout. The goal is to fix cross-platform blockers, reduce GUI coupling, and replace Win32-specific APIs in domain code.

This work can run **in parallel** with the main platform modernization effort (CMake setup, React frontend, database layer, etc.) since it operates exclusively on the legacy `code/` directory.

### Execution Environment

This PRD is executed by an autonomous agent running on **Linux Ubuntu**. The agent **cannot** run MSBuild or verify Windows builds locally. Changes are made "blind" — correctness of the Windows build is verified **manually via GitHub Actions CI** after the agent completes its work. The agent should:

- Focus on making correct, mechanical, safe transformations
- Use standard C++ replacements that are known to work on both MSVC and GCC/Clang
- Avoid MSVC-incompatible constructs (e.g., GCC-only extensions)
- Prefer conservative changes — when in doubt, use the safer option

### Context

MeOS's legacy codebase has several categories of issues that cause friction during migration to a cross-platform `src/` layout:

- **Case-sensitivity mismatches** in `#include` directives (Windows is case-insensitive, Linux is not)
- **Win32-specific string functions** (`_wtoi`, `sprintf_s`, `_itow_s`) used throughout domain code
- **Win32-specific types** (`DWORD`, `BOOL`) in domain code
- **Hardcoded backslash path separators** in file operations
- **Utility functions trapped in the GUI layer** (`widen()`, `narrow()`, `toUTF8()`, `fromUTF8()` live in `gdioutput.h`)
- **Direct coupling** from the domain aggregate root (`oEvent`) to UI classes (`TabList`, `TabAuto`, `TabSI`)
- **Win32 time APIs** (`GetLocalTime`, `SYSTEMTIME`, `GetTickCount64`) in domain code instead of `std::chrono`
- **Win32 file APIs** (`FindFirstFile`, `GetTempPath`, `DeleteFile`, `_wfopen_s`, `MAX_PATH`) instead of `std::filesystem`
- **Win32 threading primitives** (`CRITICAL_SECTION`, `_beginthread`, `TerminateThread`) instead of `std::thread`/`std::mutex`
- **Win32 `Sleep()`** instead of `std::this_thread::sleep_for()`
- **Vendored third-party libraries** — pre-built `.lib`/`.dll` files and header copies checked into the repo (`code/libharu/`, `code/mysql/`, `code/png/`, `code/minizip/`, `code/restbed/`, `code/lib64/`, `code/dll64/`) instead of using a package manager like vcpkg

Fixing these in `code/` eliminates entire categories of migration errors and reduces the per-file effort when moving code to `src/`.

### Constraints

- All changes must be compatible with MSVC (Visual Studio 2022, v143) — verified post-hoc via CI, not locally
- No changes to the `src/` directory — this PRD operates exclusively on `code/`
- Changes should be mechanical/safe where possible (search-and-replace, move functions)
- Domain behavior must not change — these are refactoring-only changes
- Use only standard C++17 constructs that compile on both MSVC and GCC/Clang

## Goals

- Eliminate case-sensitivity mismatches in all `#include` directives
- Extract non-GUI utility functions from `gdioutput` so domain files can drop the GUI dependency
- Replace Win32-specific string functions with standard C++ equivalents in domain code
- Replace Win32-specific types with standard C++ types in domain code
- Normalize path separators to use `std::filesystem::path` or portable alternatives
- Decouple `oEvent` from Tab UI classes via callbacks or interfaces
- Replace Win32 time APIs with `std::chrono` in domain code
- Replace Win32 file APIs with `std::filesystem` in domain code
- Replace Win32 threading primitives with `std::thread`/`std::mutex` in domain code
- Replace `Sleep()` with `std::this_thread::sleep_for()` in domain code
- Migrate all vendored third-party libraries (libharu, MySQL, libpng, zlib, minizip, restbed, OpenSSL) to vcpkg package management

## User Stories

### US-P0a: Fix Include Case Sensitivity

**Description:** Normalize all `#include` directives so the quoted filename matches the actual filename on disk. Windows is case-insensitive; Linux is not — these mismatches cause build failures on Linux.

**Acceptance Criteria:**
- [ ] Every `#include "..."` directive uses exactly the same casing as the file on disk
- [ ] A verification script confirms zero case mismatches
- [ ] Changes use only standard C++17 compatible with both MSVC and GCC/Clang (verified via CI post-hoc)

**Implementation Notes:**
- Can be largely automated with a script that compares `#include` strings against `ls` output
- Focus on `.h` and `.cpp` files in `code/`

### US-P0b: Extract Utility Functions from gdioutput

**Description:** Move the non-GUI utility functions `widen()`, `narrow()`, `toUTF8()`, `fromUTF8()` out of `gdioutput` to `meos_util.h/cpp` (or a new `string_util.h/cpp`). This breaks the artificial dependency from domain files to the GUI header.

**Acceptance Criteria:**
- [ ] `widen()`, `narrow()`, `toUTF8()`, `fromUTF8()` are available from `meos_util.h` (or a new header)
- [ ] Domain files that only needed these functions no longer include `gdioutput.h`
- [ ] `gdioutput` retains thin wrappers or `using` declarations for backward compatibility
- [ ] Changes use only standard C++17 compatible with both MSVC and GCC/Clang (verified via CI post-hoc)

**Implementation Notes:**
- Audit all domain files that include `gdioutput.h` — determine which actually use GUI functions vs. only string utilities
- Moving functions to `meos_util.h` is preferable to creating a new header (fewer changes needed)
- `gdioutput.h` should re-export or `using`-declare the moved functions so existing callers don't break

### US-P0c: Replace Win32-Specific String Functions in Domain Code

**Description:** Replace Windows-only string functions with standard C++ equivalents in domain files (`oEvent`, `oRunner`, `oClass`, `oDataContainer`, etc.).

**Acceptance Criteria:**
- [ ] No direct calls to `_wtoi`, `sprintf_s`, `swprintf_s`, `_itow_s` in domain `.cpp/.h` files
- [ ] Replacements use standard C++ (`<cstdlib>`, `<string>`, `<cwchar>`) or cross-platform wrappers
- [ ] Changes use only standard C++17 compatible with both MSVC and GCC/Clang (verified via CI post-hoc)

**Implementation Notes:**
- Common replacements:
  - `_wtoi(s)` -> `std::stoi(s)` or `wcstol(s, nullptr, 10)`
  - `sprintf_s(buf, fmt, ...)` -> `snprintf(buf, sizeof(buf), fmt, ...)`
  - `swprintf_s(buf, fmt, ...)` -> `swprintf(buf, sizeof(buf)/sizeof(wchar_t), fmt, ...)`
  - `_itow_s(val, buf, radix)` -> `std::to_wstring(val)` or `swprintf`
- Domain files include: `oEvent*.cpp`, `oRunner.cpp`, `oClass.cpp`, `oClub.cpp`, `oCourse.cpp`, `oControl.cpp`, `oCard.cpp`, `oTeam.cpp`, `oDataContainer.cpp`, `oBase.cpp`, `oFreePunch.cpp`
- UI files (Tab*, gdioutput) are **out of scope**

**Known Pitfalls:**
- When replacing `sprintf_s`/`swprintf_s` with `snprintf`/`swprintf`, always verify whether the buffer is a fixed-size array or a pointer — `sizeof(buf)` only works correctly on arrays, not pointers
- `lang.tl` returns `const wstring&`, which is directly compatible with `std::stoi` and `swprintf` — no intermediate conversion needed

### US-P0d: Replace Win32 Types with Standard Types in Domain Code

**Description:** Replace Win32-specific type aliases (`DWORD` -> `uint32_t`, `BOOL` -> `bool`) with standard C++ types in domain code. UI code (Tab*, gdioutput) is out of scope.

**Acceptance Criteria:**
- [ ] No `DWORD` or Win32 `BOOL` usage in domain `.cpp/.h` files
- [ ] `#include <cstdint>` added where needed
- [ ] Changes use only standard C++17 compatible with both MSVC and GCC/Clang (verified via CI post-hoc)

**Implementation Notes:**
- `DWORD` -> `uint32_t` (or `unsigned int` / `int` depending on usage context)
- Win32 `BOOL` -> `bool` (note: Win32 `BOOL` is `int`, so check for comparisons against `TRUE`/`FALSE`)
- May need to keep `#include <windows.h>` in some files temporarily if other Win32 APIs are still used

**Known Pitfalls:**
- `MeosSQL.cpp` contains `DWORD` and `BOOL` inside SQL string literals — these must NOT be replaced. Filter out replacements inside string constants

### US-P0e: Normalize Path Separators in Domain Code

**Description:** Replace hardcoded backslash (`\\`) path separators with cross-platform alternatives in domain code. Use `std::filesystem::path` where possible.

**Acceptance Criteria:**
- [ ] Domain files use `std::filesystem::path` or portable path construction
- [ ] Hardcoded `\\` in file paths replaced (non-escape-sequence uses)
- [ ] Changes use only standard C++17 compatible with both MSVC and GCC/Clang (verified via CI post-hoc)

**Implementation Notes:**
- `std::filesystem::path` handles platform-specific separators automatically
- `path / "subdir" / "file.ext"` is the idiomatic cross-platform way
- Be careful to distinguish path separators from escape sequences in strings
- `#include <filesystem>` and use `namespace fs = std::filesystem;`

**Known Pitfalls:**
- Backslashes in SQL quoting and library code (e.g., escape sequences) are NOT path separators — skip these
- Be careful with `parent_path()`: `parent_path()` of `"dir"` returns `""`, not a parent directory. To ensure a trailing separator, use `(p / "").wstring()`

### US-P0f: Decouple Domain Code from Tab* UI Classes

**Description:** Remove direct coupling from domain files to Tab UI classes (`TabList`, `TabAuto`, `TabSI`, `TabCompetition`). Currently multiple domain files have direct `#include` of Tab headers and call Tab methods, creating hard dependencies from domain/infrastructure code to the UI layer. Replace with callbacks or interfaces so domain code has no knowledge of Tab classes.

**Acceptance Criteria:**
- [ ] `oEvent.cpp` no longer includes `TabList.h`, `TabAuto.h`, or `TabSI.h`
- [ ] `TabList::baseButtons()` call replaced with a callback or interface
- [ ] `TabAuto::tabAutoKillMachines()` call replaced with a callback or interface
- [ ] `TabSI::getSI().setSubSecondMode()` call replaced with a callback or interface
- [ ] `autotask.cpp` no longer includes `TabAuto.h` or `TabSI.h` — `timerCallback()` and `checkPrintQueue()` replaced with callbacks
- [ ] `oEventResult.cpp` no longer includes `TabBase.h` or `TabList.h`
- [ ] `oEventSQL.cpp` no longer includes `TabAuto.h`
- [ ] `newcompetition.cpp` no longer includes `TabCompetition.h`
- [ ] `machinecontainer.cpp` no longer includes `TabAuto.h`
- [ ] `metalist.cpp` no longer includes `TabAuto.h`
- [ ] `mysqldaemon.cpp` no longer includes `TabAuto.h`
- [ ] `onlineinput.cpp` no longer includes `TabSI.h`
- [ ] Tab classes register their callbacks during application initialization
- [ ] Changes use only standard C++17 compatible with both MSVC and GCC/Clang (verified via CI post-hoc)

**Implementation Notes:**
- Use `std::function` callbacks stored in `oEvent` or passed via initialization — this pattern has already been proven in the migration work
- Tab classes register their callbacks during application startup (e.g., in `main()` or initialization code)
- `autotask.cpp` uses `dynamic_cast` to Tab classes — replace with stored `std::function` callbacks registered at startup
- This is the most complex preparation task — test thoroughly

**Known Pitfalls:**
- `meos.cpp` is the natural composition root where Tab callbacks should be registered (after `gEvent` initialization)
- After decoupling Tab includes, `oEvent` still has coupling to `meos.cpp` via external declarations of `createTabs` and `hideTabs` — these could also be converted to callbacks for full library extraction
- `autotask.cpp` currently does `dynamic_cast` on a list of `TabBase*` to find TabAuto/TabSI — the callback approach avoids this entirely since the Tab classes register themselves

### US-P0g: Split Large Files

**Description:** Several files in `code/` are extremely large (5000–8000+ lines), making migration to `src/` painful — large files cause merge conflicts, are hard to review, and slow down incremental migration. Split the biggest domain files into logical sub-files before migration.

**Target files (lines):**
- `oEvent.cpp` (~7400) — domain aggregate root with mixed responsibilities
- `oRunner.cpp` (~7800) — runner logic, result calculation, ranking
- `oClass.cpp` (~5700) — class configuration, draw, course assignment
- `oListInfo.cpp` (~5900) — list/result formatting and output
- `gdioutput.cpp` (~8200) — UI rendering (out of domain scope, but blocks migration)

**Acceptance Criteria:**
- [ ] No single domain `.cpp` file exceeds ~3000 lines after splitting
- [ ] Splits follow logical/functional boundaries (e.g., `oEvent` IO vs draw vs SQL, `oRunner` results vs ranking)
- [ ] All split files compile and link correctly — no missing symbols or duplicate definitions
- [ ] Header files updated with any necessary forward declarations
- [ ] Changes use only standard C++17 compatible with both MSVC and GCC/Clang (verified via CI post-hoc)

**Implementation Notes:**
- `oEvent.cpp` already has partial splits (`oEventDraw.cpp`, `oEventImport.cpp`) — follow the same pattern
- Look for natural seams: groups of related methods, `#pragma region` blocks, comment section headers
- Each split file should `#include` the parent class header and implement a coherent subset of methods
- Update the build system (`.vcxproj` / CMake) to include the new files
- Prefer splitting `.cpp` files only — avoid splitting `.h` files unless a class genuinely has separable interfaces

**Known Pitfalls:**
- Anonymous-namespace helpers and file-static variables used by only some methods need to move to the correct split file (or to an internal header)
- `oRunner.cpp` has interleaved result/ranking logic — identify method groups carefully before splitting
- `gdioutput.cpp` is UI code and not strictly domain, but its size blocks migration; split it if practical

### US-P0h: Replace Win32 Time APIs with std::chrono

**Description:** Replace Windows-specific time functions (`GetLocalTime`, `SYSTEMTIME`, `GetTickCount64`, `FileTimeToLocalFileTime`, `SystemTimeToFileTime`) with standard C++ `std::chrono` equivalents in domain code. UI code is out of scope.

**Acceptance Criteria:**
- [ ] No `GetLocalTime()`, `SYSTEMTIME`, `GetTickCount64()`, `FileTimeToLocalFileTime()`, or `SystemTimeToFileTime()` in domain `.cpp/.h` files
- [ ] Replacements use `std::chrono` and standard `<ctime>` functions
- [ ] Changes use only standard C++17 compatible with both MSVC and GCC/Clang (verified via CI post-hoc)

**Implementation Notes:**
- Common replacements:
  - `GetLocalTime(&st)` with `SYSTEMTIME st` → `auto now = std::chrono::system_clock::now(); auto t = std::chrono::system_clock::to_time_t(now); std::tm tm; localtime_r(&t, &tm);` (use `localtime_s` on MSVC — consider a portable wrapper)
  - `GetTickCount64()` → `std::chrono::steady_clock::now()` with `duration_cast<milliseconds>`
  - `SYSTEMTIME` struct fields (wYear, wMonth, wDay, wHour, wMinute, wSecond) → `std::tm` fields (tm_year+1900, tm_mon+1, tm_mday, tm_hour, tm_min, tm_sec)
  - `FILETIME` → `std::chrono::system_clock::time_point` or `std::filesystem::file_time_type`
- Domain files: `meos_util.cpp` (~10 calls), `oEvent.cpp` (~5 calls), `TimeStamp.cpp`, `oEventSpeaker.cpp`, `autotask.cpp`

**Known Pitfalls:**
- `localtime_r` (POSIX) vs `localtime_s` (MSVC) have different signatures — use a small inline wrapper or `#ifdef _WIN32` for the localtime call
- `SYSTEMTIME.wMilliseconds` has no direct `std::tm` equivalent — use `std::chrono::duration_cast<std::chrono::milliseconds>` on the time_point
- `meos_util.cpp` has heavily-used functions (`getLocalTime`, `getLocalDate`, `getLocalTimeOnly`) that many domain files depend on — changing their internals is safe, but signatures must be preserved

### US-P0i: Replace Win32 File APIs with std::filesystem

**Description:** Replace Windows-specific file and directory APIs (`FindFirstFile`/`FindNextFile`, `GetTempPath`, `CreateDirectory`, `DeleteFile`, `_wfopen_s`, `_waccess`, `MAX_PATH`) with `std::filesystem` and standard C++ equivalents in domain code. UI code is out of scope.

**Acceptance Criteria:**
- [ ] No `FindFirstFile`, `FindNextFile`, `WIN32_FIND_DATA`, `GetTempPath`, `CreateDirectory`, `DeleteFile`, `_wfopen_s`, `_waccess` in domain `.cpp/.h` files
- [ ] No `MAX_PATH` constant usage in domain files — replaced with dynamic `std::filesystem::path`
- [ ] Replacements use `std::filesystem` and standard `<fstream>`
- [ ] Changes use only standard C++17 compatible with both MSVC and GCC/Clang (verified via CI post-hoc)

**Implementation Notes:**
- Common replacements:
  - `FindFirstFile`/`FindNextFile`/`FindClose` with `WIN32_FIND_DATA` → `std::filesystem::directory_iterator`
  - `GetTempPath(MAX_PATH, buf)` → `std::filesystem::temp_directory_path()`
  - `CreateDirectory(path, NULL)` → `std::filesystem::create_directories(path)`
  - `DeleteFile(path)` → `std::filesystem::remove(path)`
  - `_wfopen_s(&f, path, mode)` → `std::ofstream` / `std::ifstream` with `std::filesystem::path`
  - `_waccess(path, 0)` → `std::filesystem::exists(path)`
  - `wchar_t buf[MAX_PATH]` → `std::filesystem::path` (dynamically sized)
- Domain files: `meos_util.cpp` (`getFiles()` ~line 1914), `oEvent.cpp` (backup/restore ~3840-3915, DeleteFile ~3988, _wfopen_s ~987), `meos.cpp` (`getTempPath()`), `zip.cpp`
- `wcscpy_s`/`wcscat_s` used for path building with `MAX_PATH` buffers can be replaced entirely by `std::filesystem::path` concatenation

**Known Pitfalls:**
- `_wfopen_s` for binary writes (`"wb"`) should use `std::ofstream` with `std::ios::binary`
- `FindFirstFile` patterns like `L"*.meos"` map to `directory_iterator` with a manual extension filter — `std::filesystem` has no built-in glob
- `GetTempPath` + `CreateDirectory` is often combined — replace both together with `temp_directory_path() / "MeOS"` + `create_directories()`
- Some `DeleteFile` calls are inside error-recovery paths — ensure `std::filesystem::remove` error handling matches (use `error_code` overload to avoid exceptions)

### US-P0j: Replace Win32 Threading Primitives with std::thread/std::mutex

**Description:** Replace Windows-specific threading and synchronization primitives (`CRITICAL_SECTION`, `_beginthread`/`_beginthreadex`, `TerminateThread`, `HANDLE` for threads) with standard C++ equivalents (`std::mutex`, `std::thread`, cooperative cancellation) in domain code. UI code is out of scope.

**Acceptance Criteria:**
- [ ] No `CRITICAL_SECTION`, `EnterCriticalSection`, `LeaveCriticalSection`, `InitializeCriticalSection`, `DeleteCriticalSection` in domain `.cpp/.h` files
- [ ] No `_beginthread`, `_beginthreadex`, `TerminateThread` in domain `.cpp/.h` files
- [ ] Thread handles use `std::thread` instead of Win32 `HANDLE`
- [ ] Replacements use `std::mutex`, `std::lock_guard`, `std::thread`, `std::atomic<bool>` for cancellation
- [ ] Changes use only standard C++17 compatible with both MSVC and GCC/Clang (verified via CI post-hoc)

**Implementation Notes:**
- Common replacements:
  - `CRITICAL_SECTION` + `InitializeCriticalSection`/`DeleteCriticalSection` → `std::mutex` (no init/delete needed)
  - `EnterCriticalSection(&cs)` ... `LeaveCriticalSection(&cs)` → `std::lock_guard<std::mutex> lock(mtx);`
  - `_beginthread(func, 0, arg)` / `_beginthreadex(0, 0, func, arg, 0, 0)` → `std::thread(func, arg)`
  - `TerminateThread(handle, 0)` → cooperative cancellation with `std::atomic<bool> stopRequested`; thread checks flag periodically
  - `HANDLE ThreadHandle` → `std::thread` member + `.joinable()`/`.join()`/`.detach()`
- Domain files: `SportIdent.cpp/h` (CRITICAL_SECTION `SyncObj`, `_beginthread`, `TerminateThread`), `socket.cpp/h` (CRITICAL_SECTION `syncObj`), `mysqldaemon.cpp` (`_beginthreadex`)

**Known Pitfalls:**
- `TerminateThread` is unsafe even on Windows (no cleanup, no destructors). Replacing it with cooperative cancellation via `std::atomic<bool>` is a correctness improvement, but requires the thread function to periodically check the flag — audit the thread loops to find suitable check points
- `_beginthread` vs `_beginthreadex` have different ownership semantics — `_beginthread` closes the handle automatically. With `std::thread`, ownership is explicit (`.join()` or `.detach()`)
- `SportIdent.cpp` uses `CRITICAL_SECTION` in hot paths (SI data reading) — `std::mutex` has comparable performance, but verify no recursive locking is needed (if so, use `std::recursive_mutex`)
- Thread function signatures differ: `_beginthread` expects `void (*)(void*)`, `std::thread` accepts any callable — adapt the thread functions accordingly

### US-P0k: Replace Sleep() with std::this_thread::sleep_for()

**Description:** Replace Win32 `Sleep()` calls with standard `std::this_thread::sleep_for()` in domain code. This is a simple mechanical replacement. UI code is out of scope.

**Acceptance Criteria:**
- [ ] No Win32 `Sleep()` calls in domain `.cpp/.h` files
- [ ] Replacements use `std::this_thread::sleep_for()` with `std::chrono::milliseconds`
- [ ] `#include <thread>` and `#include <chrono>` added where needed
- [ ] Changes use only standard C++17 compatible with both MSVC and GCC/Clang (verified via CI post-hoc)

**Implementation Notes:**
- Replacement: `Sleep(N)` → `std::this_thread::sleep_for(std::chrono::milliseconds(N))`
- Domain files: `SportIdent.cpp` (~3 calls, lines 172, 226, 578), `socket.cpp` (line 54), `newcompetition.cpp` (line 407)
- Can be combined with US-P0j (threading) work since both touch the same files and require `<thread>`

**Known Pitfalls:**
- Ensure the replacement is for Win32 `Sleep()` (capital S, from `<windows.h>`) and not POSIX `sleep()` (lowercase, seconds) — the codebase uses the Win32 variant
- `SportIdent.cpp` Sleep() calls are inside timing-sensitive serial communication loops — verify the millisecond values are preserved exactly

### US-P0n: Replace MessageBox() and OutputDebugString() in Domain Code

**Description:** Replace direct Win32 UI calls (`MessageBox()`, `OutputDebugString()`) in domain code with cross-platform alternatives. These functions create a hard dependency on `<windows.h>` and the Win32 UI subsystem from domain logic.

**Acceptance Criteria:**
- [ ] No `MessageBox()` calls in domain `.cpp/.h` files
- [ ] No `OutputDebugString()` calls in domain `.cpp/.h` files
- [ ] Replacements use exceptions, `std::function` error callbacks, or `std::cerr` as appropriate
- [ ] Changes use only standard C++17 compatible with both MSVC and GCC/Clang (verified via CI post-hoc)

**Implementation Notes:**
- `MessageBox()` in domain code is used for error reporting — replace with `throw` + catch at UI boundary, or with an error callback (`std::function<void(const wstring&)>`)
- `OutputDebugString()` is used for debug logging — replace with `std::cerr` or a simple logging function
- Domain files:
  - `oClub.cpp` (lines 96, 98) — error dialogs during clubnamemap.csv loading
  - `oEvent.cpp` (line 4209) — error dialog
  - `SportIdent.cpp` (lines 1067, 1103, 1156, 1164) — SI configuration error dialogs
  - `autotask.cpp` (line 198) — `OutputDebugString()` for timing info

**Known Pitfalls:**
- `MessageBox()` is blocking (modal) — a `throw` replacement changes control flow. Ensure the calling code handles the exception correctly, or use a non-throwing callback pattern
- `SportIdent.cpp` MessageBox calls are inside hardware configuration — these may need a callback that the UI layer registers, since the user needs to see the message

### US-P0l: CMake Build for Legacy Code

**Description:** Add a CMake build system for the `code/` directory so the legacy codebase can be built with `cmake --build` instead of MSBuild. The existing `.vcxproj`/`.sln` files are retained for developers using Visual Studio, but CI and automated builds switch to CMake. The GitHub Actions workflow (`.github/workflows/build-legacy.yml`) is updated to use CMake.

**Acceptance Criteria:**
- [ ] `code/CMakeLists.txt` exists and builds MeOS.exe on Windows with MSVC via CMake
- [ ] All 93 `.cpp` source files are compiled
- [ ] Resource files (`meos.rc`, `meoslang.rc`) are compiled and linked
- [ ] DPI manifest (`meos_dpi_manifest.xml`) is embedded
- [ ] All external libraries are linked: libharu, libmysql, libpng, zlib, RestBed, OpenSSL
- [ ] All Windows system libraries are linked: Msimg32, comctl32, odbc32, odbccp32, winmm, ws2_32, wininet
- [ ] Debug and Release configurations work correctly (correct library paths: `lib64_db/` vs `lib64/`)
- [ ] `.github/workflows/build-legacy.yml` uses CMake instead of MSBuild
- [ ] CI artifact packaging still bundles all required DLLs (libharu, libmysql, OpenSSL, VC runtime)
- [ ] Preprocessor defines match the existing build: `NDEBUG`/`_DEBUG`, `_WINDOWS`
- [ ] C++17 standard is enforced
- [ ] Disabled warnings match existing build (4267, 4244, 4018) to avoid noise
- [ ] The existing `.sln`/`.vcxproj` files are left intact for manual use

**Implementation Notes:**
- Create `code/CMakeLists.txt` as a standalone project (not a subdirectory of the root `CMakeLists.txt` which targets `src/`)
- Use `add_executable()` with all `.cpp` and `.rc` files
- External libraries live in `code/lib64/` (Release) and `code/lib64_db/` (Debug) — use generator expressions: `$<IF:$<CONFIG:Debug>,lib64_db,lib64>`
- Include paths: `code/libharu/` for libharu headers, `code/mysql/` for MySQL headers, `code/png/` for libpng, `code/restbed/` for REST API, `code/minizip/` for zip
- Library names differ between Win32 and x64 (e.g., `libhpdf.lib` vs `libharu.lib`) — target x64 only since that's what CI builds
- For OpenSSL: use `find_package(OpenSSL)` or link directly against installed libs (CI installs OpenSSL 1.1 via Chocolatey)
- Resource compilation: CMake handles `.rc` files natively on Windows when using MSVC generator
- CI workflow changes:
  - Replace `msbuild MeOS.sln /p:Configuration=Release /p:Platform=x64` with `cmake -S code -B code/build -A x64` + `cmake --build code/build --config Release`
  - `setup-msbuild` action can be removed (CMake finds MSVC via Visual Studio installation)
  - Artifact paths change from `code/**/MeOS.exe` to `code/build/Release/MeOS.exe`
- Whole-program optimization (`/GL` + `/LTCG`) and multi-processor compilation (`/MP`) should be preserved for Release

**Known Pitfalls:**
- `find_package(OpenSSL)` may need `OPENSSL_ROOT_DIR` set to the Chocolatey install path on CI — use `Get-ChildItem "C:\Program Files\OpenSSL*"` to locate it
- The `.rc` files may reference relative paths for icons/bitmaps — ensure the working directory or include paths are set correctly
- `WIN32` flag in `add_executable(MeOS WIN32 ...)` is needed to produce a Windows GUI app (no console window), matching the existing `/SUBSYSTEM:WINDOWS` setting
- Library search order matters: `code/lib64/` contains pre-built static libs that must be found before any system-installed versions
- `RestBed.lib` is very large (~20-48MB) — CMake will link it fine, but build times may be notable

### US-P0m1: Migrate libharu to vcpkg

**Description:** Replace the vendored libharu headers (`code/libharu/`) and pre-built static libraries (`libharu.lib`/`libhpdf.lib` in `code/lib64/`, `code/lib64_db/`, `code/lib/`) with a vcpkg-managed dependency. This eliminates checked-in binaries and ensures consistent, reproducible builds across platforms.

**Acceptance Criteria:**
- [ ] `libharu` is declared as a dependency in `vcpkg.json`
- [ ] `code/CMakeLists.txt` uses `find_package(unofficial-libharu)` or equivalent vcpkg integration instead of manual library paths
- [ ] The vendored `code/libharu/` header directory is removed
- [ ] The pre-built `libharu.lib` / `libhpdf.lib` files are removed from `code/lib64/`, `code/lib64_db/`, and `code/lib/`
- [ ] `code/pdfwriter.cpp` compiles and links against the vcpkg-provided libharu
- [ ] CI builds succeed on both Windows and Linux

**Implementation Notes:**
- vcpkg port name: `libharu`
- The project uses libharu via `#include "hpdf.h"` in `pdfwriter.cpp` — verify the vcpkg port provides the same header path or update includes
- lib64 has `libharu.lib`, lib has `libhpdf.lib` — vcpkg normalizes this difference
- Remove the vendored headers and libs only after verifying the vcpkg build works

**Known Pitfalls:**
- The vendored libharu version may differ from the vcpkg port version — check for API changes if the version jump is large
- libharu depends on zlib and libpng — coordinate with US-P0m3 and US-P0m4 to avoid version conflicts (vcpkg handles transitive dependencies automatically)

### US-P0m2: Migrate MySQL Connector/C to vcpkg

**Description:** Replace the vendored MySQL Connector/C headers (`code/mysql/`) and pre-built import libraries (`libmysql.lib` in `code/lib64/`, `code/lib64_db/`, `code/lib/`) and runtime DLLs (`libmysql.dll` in `code/dll/`, `code/dll64/`) with a vcpkg-managed dependency.

**Acceptance Criteria:**
- [ ] `libmysql` (or `mysql-connector-c`) is declared as a dependency in `vcpkg.json`
- [ ] `code/CMakeLists.txt` uses `find_package()` or vcpkg integration instead of manual library paths
- [ ] The vendored `code/mysql/` header directory is removed
- [ ] The pre-built `libmysql.lib` files are removed from `code/lib64/`, `code/lib64_db/`, and `code/lib/`
- [ ] The pre-built `libmysql.dll` / `libmySQL.dll` files are removed from `code/dll/` and `code/dll64/`
- [ ] `code/mysqlwrapper.cpp`, `code/MeosSQL.cpp`, and `code/mysqldaemon.cpp` compile and link correctly
- [ ] CI builds succeed on both Windows and Linux

**Implementation Notes:**
- vcpkg port name: `libmysql` (provides the MySQL C client library)
- The vendored headers are from MySQL 5.7 — check for API compatibility with the vcpkg version
- `code/mysqlwrapper.h` includes `<mysql.h>` — verify the vcpkg port provides headers at the same include path
- `libmysql.dll` is needed at runtime — vcpkg handles DLL deployment differently than the current manual copy approach; ensure the CI artifact packaging step picks up the vcpkg-installed DLL
- The `code/mysql/` directory has a nested `mysql/` subdirectory with additional headers — ensure all nested includes resolve correctly

**Known Pitfalls:**
- MySQL Connector/C vs MariaDB Connector/C — the vcpkg port `libmysql` provides the Oracle MySQL client; if MariaDB is acceptable, `libmariadb` is an alternative with better cross-platform support
- Runtime DLL deployment: the current build copies DLLs from `code/dll64/` into the output — with vcpkg, use `vcpkg_installed/` or CMake install rules instead
- The vendored MySQL headers reference Windows-specific types (`my_thread.h`, etc.) — the vcpkg version may differ

### US-P0m3: Migrate libpng to vcpkg

**Description:** Replace the vendored libpng headers (`code/png/`) and pre-built static libraries (`libpng.lib` in `code/lib64/`, `code/lib64_db/`, `code/lib/`) with a vcpkg-managed dependency.

**Acceptance Criteria:**
- [ ] `libpng` is declared as a dependency in `vcpkg.json`
- [ ] `code/CMakeLists.txt` uses `find_package(PNG)` instead of manual library paths
- [ ] The vendored `code/png/` header directory is removed
- [ ] The pre-built `libpng.lib` files are removed from `code/lib64/`, `code/lib64_db/`, and `code/lib/`
- [ ] `code/image.cpp` compiles and links against the vcpkg-provided libpng
- [ ] CI builds succeed on both Windows and Linux

**Implementation Notes:**
- vcpkg port name: `libpng`
- The project uses libpng via `#include "png.h"` in `image.cpp` — the vcpkg port provides this header via `find_package(PNG)` which sets `PNG_INCLUDE_DIRS` and `PNG_LIBRARIES`
- libpng depends on zlib — coordinate with US-P0m4 (vcpkg handles transitive dependencies automatically)

**Known Pitfalls:**
- The vendored `pnglibconf.h` may contain custom configuration — compare with the vcpkg version to ensure no feature flags differ

### US-P0m4: Migrate zlib to vcpkg

**Description:** Replace the vendored zlib static libraries (`zlibstat.lib` / `zlibstat_vc15.lib` in `code/lib64/`, `code/lib64_db/`, `code/lib/`) with a vcpkg-managed dependency. Note: zlib headers are currently bundled inside `code/minizip/` — only the standalone library files are replaced by this story.

**Acceptance Criteria:**
- [ ] `zlib` is declared as a dependency in `vcpkg.json`
- [ ] `code/CMakeLists.txt` uses `find_package(ZLIB)` instead of manual library paths
- [ ] The pre-built `zlibstat.lib` / `zlibstat_vc15.lib` files are removed from `code/lib64/`, `code/lib64_db/`, and `code/lib/`
- [ ] `code/zip.cpp` and minizip code compile and link against the vcpkg-provided zlib
- [ ] CI builds succeed on both Windows and Linux

**Implementation Notes:**
- vcpkg port name: `zlib`
- The zlib headers (`zlib.h`, `zconf.h`) are currently located in `code/minizip/` alongside the minizip source — these should be removed once the vcpkg zlib provides them via include paths
- `find_package(ZLIB)` sets `ZLIB_INCLUDE_DIRS` and `ZLIB_LIBRARIES`

**Known Pitfalls:**
- `code/minizip/` contains both zlib headers and minizip source files — only the zlib headers and libs are replaced here; minizip source files are handled by US-P0m5
- The vendored lib has different names across platforms (`zlibstat.lib` vs `zlibstat_vc15.lib`) — vcpkg normalizes this

### US-P0m5: Migrate minizip to vcpkg

**Description:** Replace the vendored minizip source files and headers (`code/minizip/`) with a vcpkg-managed dependency. Currently minizip source is compiled directly as part of the MeOS build — with vcpkg it becomes a pre-built library.

**Acceptance Criteria:**
- [ ] `minizip` is declared as a dependency in `vcpkg.json`
- [ ] `code/CMakeLists.txt` uses `find_package()` or vcpkg integration for minizip instead of compiling minizip sources directly
- [ ] The vendored `code/minizip/` directory is removed (after zlib headers are provided by US-P0m4)
- [ ] `code/zip.cpp` compiles and links against the vcpkg-provided minizip
- [ ] CI builds succeed on both Windows and Linux

**Implementation Notes:**
- vcpkg port name: `minizip`
- The `code/minizip/` directory contains both minizip-specific files (`zip.h`, `unzip.h`, `ioapi.h`, `iowin32.h`, `mztools.h`) and zlib internals (`zlib.h`, `zconf.h`, `deflate.h`, `inflate.h`, etc.) — the zlib files are handled by US-P0m4
- `code/zip.cpp` includes `"minizip/zip.h"` and `"minizip/unzip.h"` — the vcpkg port may require `<minizip/zip.h>` or just `<zip.h>` depending on configuration
- `iowin32.h`/`iowin32.c` is the Windows-specific I/O backend for minizip — the vcpkg port should provide this on Windows automatically

**Known Pitfalls:**
- minizip depends on zlib — must be done together with or after US-P0m4
- The vendored minizip may include modifications or patches — diff against upstream minizip to check for local changes before removing
- The current build likely compiles minizip `.c` files directly — these compilation units must be removed from the CMakeLists.txt source list when switching to the vcpkg library

### US-P0m6: Migrate restbed to vcpkg

**Description:** Replace the vendored restbed headers (`code/restbed/`) and pre-built static library (`RestBed.lib` in `code/lib64/`, `code/lib64_db/`, `code/lib/`) with a vcpkg-managed dependency.

**Acceptance Criteria:**
- [ ] `restbed` is declared as a dependency in `vcpkg.json`
- [ ] `code/CMakeLists.txt` uses `find_package(restbed)` or vcpkg integration instead of manual library paths
- [ ] The vendored `code/restbed/` header directory is removed
- [ ] The pre-built `RestBed.lib` files are removed from `code/lib64/`, `code/lib64_db/`, and `code/lib/`
- [ ] `code/restserver.cpp` and `code/RestService.cpp` compile and link against the vcpkg-provided restbed
- [ ] CI builds succeed on both Windows and Linux

**Implementation Notes:**
- vcpkg port name: `restbed`
- The project includes restbed via `#include <restbed>` (the umbrella header) in `restserver.cpp` — verify the vcpkg port provides this header
- restbed has a transitive dependency on OpenSSL (via `ssl_settings.hpp`) — coordinate with US-P0m7
- The vendored `RestBed.lib` is very large (20–48 MB) — removing it significantly reduces repository size

**Known Pitfalls:**
- restbed's vcpkg port may be outdated or have different default features (SSL enabled/disabled) — verify the `[ssl]` feature is enabled in the vcpkg dependency declaration if SSL support is needed
- The vendored restbed is from 2017 (Corvusoft era) — the vcpkg version may have API changes; check `restserver.cpp` and `RestService.cpp` for compatibility
- restbed depends on ASIO (either standalone or Boost.Asio) — vcpkg handles this transitively, but it adds to the dependency tree

### US-P0m7: Migrate OpenSSL to vcpkg

**Description:** Replace the system-installed OpenSSL (currently installed via Chocolatey on Windows CI) with a vcpkg-managed dependency. This ensures consistent OpenSSL versions across all platforms and eliminates the need for CI-specific installation steps.

**Acceptance Criteria:**
- [ ] `openssl` is declared as a dependency in `vcpkg.json`
- [ ] `code/CMakeLists.txt` uses `find_package(OpenSSL)` with vcpkg integration instead of relying on system-installed OpenSSL
- [ ] CI workflow no longer needs a separate OpenSSL installation step (Chocolatey)
- [ ] restbed links against the vcpkg-provided OpenSSL
- [ ] CI builds succeed on both Windows and Linux

**Implementation Notes:**
- vcpkg port name: `openssl`
- OpenSSL is not directly used by MeOS code — it is a transitive dependency of restbed (for SSL/TLS support)
- If restbed is migrated to vcpkg (US-P0m6), OpenSSL is pulled in transitively and this story may only require adding it explicitly to `vcpkg.json` and removing the Chocolatey install step from CI
- The current CI uses OpenSSL 1.1 via Chocolatey — vcpkg may provide OpenSSL 3.x; verify restbed compatibility

**Known Pitfalls:**
- OpenSSL 1.1 vs 3.x API differences — if restbed was built against 1.1, the vcpkg version (likely 3.x) may require restbed to be updated as well
- On Linux, OpenSSL is typically available as a system package — vcpkg can still manage it for consistency, but `VCPKG_USE_SYSTEM_OPENSSL` may be needed in some configurations
- OpenSSL build from source is slow — vcpkg caches it, but first CI build will be longer

## Functional Requirements

- FR-1: All changes must use standard C++17 compatible with MSVC (v143), GCC 12+, and Clang 15+ — Windows build verification happens post-hoc via CI
- FR-2: No behavioral changes to domain logic — all changes are structural/refactoring only
- FR-3: Domain files must not gain new Win32 dependencies as a result of these changes
- FR-4: Backward compatibility must be maintained for all public function signatures (use forwarding/wrappers where needed)

## Non-Goals

- Migrating any files to `src/` — that is covered by the main platform modernization PRD
- Changing UI/Tab code beyond what's needed for decoupling (US-P0f)
- Adding tests (the legacy codebase has no test infrastructure; tests come after migration)
- Fixing issues in non-domain files (UI, Tab code, etc.) beyond what's needed for decoupling and build system changes

## Dependency Order

The stories can be worked largely in parallel, with one exception:

```
US-P0l (CMake build)        — independent, do early to get CI feedback on all subsequent changes
US-P0a (include casing)     — independent, do first
US-P0b (extract utilities)  — independent
US-P0c (string functions)   — independent, but easier after US-P0b
US-P0d (Win32 types)        — independent
US-P0e (path separators)    — independent
US-P0f (decouple Tab*)      — independent, but benefits from US-P0b being done
US-P0h (time APIs)          — independent
US-P0i (file APIs)          — independent, but easier after US-P0e (path separators)
US-P0j (threading)          — independent
US-P0k (Sleep)              — independent, can combine with US-P0j
US-P0n (MessageBox/Debug)   — independent, can combine with US-P0f (same decoupling pattern)
US-P0m4 (zlib vcpkg)        — depends on US-P0l (CMake must exist)
US-P0m3 (libpng vcpkg)      — depends on US-P0m4 (zlib is a transitive dependency)
US-P0m1 (libharu vcpkg)     — depends on US-P0m3 and US-P0m4 (libharu depends on both)
US-P0m5 (minizip vcpkg)     — depends on US-P0m4 (minizip depends on zlib)
US-P0m7 (OpenSSL vcpkg)     — depends on US-P0l
US-P0m6 (restbed vcpkg)     — depends on US-P0m7 (restbed depends on OpenSSL)
US-P0m2 (MySQL vcpkg)       — depends on US-P0l
US-P0g (split large files)  — do last to avoid conflicts with all other changes
```

Recommended order: P0l first (establishes CMake CI so all subsequent changes get build verification), then P0a (quick, mechanical), then P0b (unblocks cleaner domain files), then P0c-P0e, P0h-P0k, and P0n in any order (P0k can combine with P0j, P0n can combine with P0f), then P0f (most complex). vcpkg migrations (P0m1-P0m7) should be done after P0l establishes CMake, in dependency order: P0m4 (zlib) → P0m3 (libpng) + P0m5 (minizip) → P0m1 (libharu), and P0m7 (OpenSSL) → P0m6 (restbed), with P0m2 (MySQL) independent. P0g last (reduces churn from earlier refactorings). Note: P0g requires updating `code/CMakeLists.txt` when new split files are created.

## Success Metrics

- `code/CMakeLists.txt` builds MeOS.exe successfully via `cmake --build` on Windows CI
- `.github/workflows/build-legacy.yml` uses CMake (no MSBuild dependency)
- Zero case-sensitivity mismatches in `#include` directives
- Domain `.cpp/.h` files have no direct `#include "gdioutput.h"` (only via utility headers)
- `grep` for `_wtoi`, `sprintf_s`, `_itow_s`, `DWORD`, Win32 `BOOL` returns zero hits in domain files
- No domain `.cpp/.h` file includes any `Tab*.h` header
- `grep` for `MessageBox` and `OutputDebugString` returns zero hits in domain files
- No single domain `.cpp` file exceeds ~3000 lines
- `grep` for `GetLocalTime`, `SYSTEMTIME`, `GetTickCount64` returns zero hits in domain files
- `grep` for `FindFirstFile`, `GetTempPath`, `_wfopen_s`, `MAX_PATH` returns zero hits in domain files
- `grep` for `CRITICAL_SECTION`, `_beginthread`, `TerminateThread` returns zero hits in domain files
- `grep` for Win32 `Sleep(` returns zero hits in domain files
- All third-party libraries (libharu, libmysql, libpng, zlib, minizip, restbed, OpenSSL) are managed via `vcpkg.json` — no vendored headers or pre-built `.lib`/`.dll` files remain in `code/`
- `code/libharu/`, `code/mysql/`, `code/png/`, `code/minizip/`, `code/restbed/` directories are removed
- `code/lib/`, `code/lib64/`, `code/lib64_db/`, `code/dll/`, `code/dll64/` directories are removed
- Windows MSBuild build succeeds after all changes (verified via CI post-hoc)

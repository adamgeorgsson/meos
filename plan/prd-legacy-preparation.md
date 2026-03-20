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

### Constraints

- All changes must be compatible with MSVC (Visual Studio 2022, v143) — verified post-hoc via CI, not locally
- No changes to the `src/` directory — this PRD operates exclusively on `code/`
- Changes should be mechanical/safe where possible (search-and-replace, move functions)
- Domain behavior must not change — these are refactoring-only changes
- Use only standard C++17 constructs that compile on both MSVC and GCC/Clang
- Backward compatibility must be maintained for all public function signatures (use forwarding/wrappers where needed)

## Non-Goals

- Migrating any files to `src/` — that is covered by the main platform modernization PRD
- Changing UI/Tab code beyond what's needed for decoupling (US-P0f)
- Adding tests (the legacy codebase has no test infrastructure; tests come after migration)
- Fixing issues in non-domain files (UI, Tab code, etc.) beyond what's needed for decoupling and build system changes

## User Stories

### US-P0a: Fix Include Case Sensitivity

**Description:** Normalize all `#include` directives so the quoted filename matches the actual filename on disk. Windows is case-insensitive; Linux is not — these mismatches cause build failures on Linux.

**Acceptance Criteria:**
- [ ] Every `#include "..."` directive uses exactly the same casing as the file on disk
- [ ] A verification script confirms zero case mismatches

**Implementation Notes:**
- **Script available:** Run `python3 .claude/skills/include-normalization/fix_includes.py` to fix all case mismatches automatically. Use `--dry-run` first.
- Focus on `.h` and `.cpp` files in `code/`

### US-P0b: Extract Utility Functions from gdioutput

**Description:** Move the non-GUI utility functions `widen()`, `narrow()`, `toUTF8()`, `fromUTF8()` out of `gdioutput` to `meos_util.h/cpp` (or a new `string_util.h/cpp`). This breaks the artificial dependency from domain files to the GUI header.

**Acceptance Criteria:**
- [ ] `widen()`, `narrow()`, `toUTF8()`, `fromUTF8()` are available from `meos_util.h` (or a new header)
- [ ] Domain files that only needed these functions no longer include `gdioutput.h`
- [ ] `gdioutput` retains thin wrappers or `using` declarations for backward compatibility

**Implementation Notes:**
- Audit all domain files that include `gdioutput.h` — determine which actually use GUI functions vs. only string utilities
- Moving functions to `meos_util.h` is preferable to creating a new header (fewer changes needed)
- `gdioutput.h` should re-export or `using`-declare the moved functions so existing callers don't break

**Known Pitfalls:**
- When moving function definitions to `meos_util.cpp`, **remove the original definitions** from their source files (e.g., `localizer.cpp`). Leaving both causes LNK2005 "multiply defined symbol" linker errors. After moving, grep for the function name across all `.cpp` files to confirm exactly one definition remains.

### US-P0c: Replace Win32-Specific String Functions in Domain Code

**Description:** Replace Windows-only string functions with standard C++ equivalents in domain files (`oEvent`, `oRunner`, `oClass`, `oDataContainer`, etc.). UI files (Tab*, gdioutput) are **out of scope**.

**Acceptance Criteria:**
- [ ] No direct calls to `_wtoi`, `sprintf_s`, `swprintf_s`, `_itow_s` in domain `.cpp/.h` files
- [ ] Replacements use standard C++ (`<cstdlib>`, `<string>`, `<cwchar>`) or cross-platform wrappers

**Implementation Notes:**
- **Script available:** Run `python3 .claude/skills/replace_win32_strings/replace_win32_strings.py code/` to automate bulk replacements. Use `--dry-run` first.
- Common replacements:
  - `_wtoi(s)` -> `std::stoi(s)` or `wcstol(s, nullptr, 10)`
  - `sprintf_s(buf, fmt, ...)` -> `snprintf(buf, sizeof(buf), fmt, ...)`
  - `swprintf_s(buf, fmt, ...)` -> `swprintf(buf, sizeof(buf)/sizeof(wchar_t), fmt, ...)`
  - `_itow_s(val, buf, radix)` -> `std::to_wstring(val)` or `swprintf`
- Domain files: `oEvent*.cpp`, `oRunner.cpp`, `oClass.cpp`, `oClub.cpp`, `oCourse.cpp`, `oControl.cpp`, `oCard.cpp`, `oTeam.cpp`, `oDataContainer.cpp`, `oBase.cpp`, `oFreePunch.cpp`

**Known Pitfalls:**
- When replacing `sprintf_s`/`swprintf_s` with `snprintf`/`swprintf`, always verify whether the buffer is a fixed-size array or a pointer — `sizeof(buf)` only works correctly on arrays, not pointers
- `lang.tl` returns `const wstring&`, which is directly compatible with `std::stoi` and `swprintf` — no intermediate conversion needed
- The `swprintf_s` 2-arg vs 3-arg heuristic must detect the format string (starts with `L"` or `lang.`) — not the size arg — to avoid duplicating size arguments when constants like `MAX_PATH` are used

### US-P0d: Replace Win32 Types with Standard Types in Domain Code

**Description:** Replace Win32-specific type aliases (`DWORD` -> `uint32_t`, `BOOL` -> `bool`) with standard C++ types in domain code. UI code (Tab*, gdioutput) is out of scope.

**Acceptance Criteria:**
- [ ] No `DWORD` or Win32 `BOOL` usage in domain `.cpp/.h` files
- [ ] `#include <cstdint>` added where needed

**Implementation Notes:**
- **Script available:** Run `python3 .claude/skills/replace_win32_types/replace_win32_types.py` to automate all replacements. Use `--dry-run` first. The script handles ~184 replacements across 26 files, skips string literals, and adds `#include <cstdint>`.
- `DWORD` -> `uint32_t` (or `unsigned int` / `int` depending on usage context)
- Win32 `BOOL` -> `bool` (note: Win32 `BOOL` is `int`, so check for comparisons against `TRUE`/`FALSE`)
- May need to keep `#include <windows.h>` in some files temporarily if other Win32 APIs are still used

**Known Pitfalls:**
- `MeosSQL.cpp` contains `DWORD` and `BOOL` inside SQL string literals — these must NOT be replaced. The script handles this by skipping content inside string literals.
- On MSVC, `DWORD` (`unsigned long`) and `uint32_t` (`unsigned int`) are **different types**. Do NOT replace `DWORD` in files that pass variables to Win32 APIs via `LPDWORD` — the script now excludes `SportIdent.cpp/h`, `download.cpp/h`, and `listeditor.cpp`. In domain files, variables passed to `gdioutput::getData(DWORD&)` or Win32 APIs like `GetComputerName` must remain `DWORD` — fix manually after script.

### US-P0e: Normalize Path Separators in Domain Code

**Description:** Replace hardcoded backslash (`\\`) path separators with cross-platform alternatives in domain code. Use `std::filesystem::path` where possible.

**Acceptance Criteria:**
- [ ] Domain files use `std::filesystem::path` or portable path construction
- [ ] Hardcoded `\\` in file paths replaced (non-escape-sequence uses)

**Implementation Notes:**
- **Script available:** Run `python3 .claude/skills/normalize_paths/normalize_paths.py` to replace ~16 backslash path separators in string literals. Use `--dry-run` first. The script distinguishes path separators from escape sequences (`\\n`, `\\t`) and regex patterns.
- `path / "subdir" / "file.ext"` is the idiomatic cross-platform way
- Be careful to distinguish path separators from escape sequences in strings
- After running the script, manually refactor path construction patterns using `std::filesystem::path`

**Known Pitfalls:**
- Backslashes in SQL quoting and library code (e.g., escape sequences) are NOT path separators — skip these
- Be careful with `parent_path()`: `parent_path()` of `"dir"` returns `""`, not a parent directory. To ensure a trailing separator, use `(p / "").wstring()`

### US-P0f: Decouple Domain Code from Tab* UI Classes

**Description:** Remove direct coupling from domain files to Tab UI classes (`TabList`, `TabAuto`, `TabSI`). Replace with `std::function` callbacks stored on `oEvent`, registered at startup in `meos.cpp`.

**Skill available:** See `.claude/skills/tab-decoupling/SKILL.md` for exact callback signatures, per-file code changes, and registration code. Split into 8 sub-stories:

| Sub-story | File | What changes | Callbacks |
|-----------|------|-------------|-----------|
| US-P0f1 | `oEvent.h` | Add 13 `std::function` callback members | Infrastructure |
| US-P0f2 | `oEvent.cpp` | Remove TabAuto/TabSI/TabList/TabBase includes, 3 call sites → callbacks | `cbBaseButtons`, `cbKillMachines`, `cbSetSubSecondMode` |
| US-P0f3 | `autotask.cpp` | Remove TabAuto/TabSI includes, replace dynamic_casts | `cbTimerCallback`, `cbCheckPrintQueue`, `cbSyncCallback`, `cbGetSynchronize`, `cbGetSynchronizePunches` |
| US-P0f4 | `oEventSQL.cpp` | Remove TabAuto include, abstract MySQLReconnect creation | `cbHasReconnectionMachine`, `cbStartReconnectMachine` |
| US-P0f5 | `oEventResult.cpp` | Remove TabBase/TabList includes, 1 dynamic_cast → callback | `cbGetListEditor` |
| US-P0f6 | `metalist.cpp` | Remove TabAuto include, 1 cast → callback | `cbRemovedList` |
| US-P0f7 | `onlineinput.cpp` | Remove TabSI include, 1 static call → callback | `cbAddCard` |
| US-P0f8 | `meos.cpp` | Register all callbacks after tab creation | All |

**Execution order:** US-P0f1 → US-P0f8 → US-P0f2 through US-P0f7 (any order).

**Acceptance Criteria:**
- [ ] `oEvent.cpp` no longer includes `TabList.h`, `TabAuto.h`, `TabSI.h`, or `TabBase.h`
- [ ] `autotask.cpp` no longer includes `TabAuto.h` or `TabSI.h`
- [ ] `oEventResult.cpp` no longer includes `TabBase.h` or `TabList.h`
- [ ] `oEventSQL.cpp` no longer includes `TabAuto.h`
- [ ] `metalist.cpp` no longer includes `TabAuto.h`
- [ ] `onlineinput.cpp` no longer includes `TabSI.h`
- [ ] Tab classes register their callbacks in `meos.cpp` during application initialization

**Known Pitfalls:**
- `newcompetition.cpp` implements `TabCompetition::` methods — it IS UI code, not domain code. Do not try to remove its TabCompetition.h include
- `machinecontainer.cpp`, `mysqldaemon.cpp`, `onlineinput.h` need `AutoMachine` from `TabAuto.h` — extracting `AutoMachine` to own header is a separate follow-up
- Lambdas in meos.cpp capture raw Tab pointers — safe because Tab objects live for entire application lifetime
- **US-P0f8 requires making `TabAuto` members public:** The callback lambdas access `timerCallback`, `syncCallback`, `synchronize`, and `synchronizePunches` which are private. These must be moved to `public:` in `TabAuto.h` before registering callbacks, or MSVC will error with `C2248`

### US-P0g: Split Large Files

**Description:** Split the 5 biggest files into logical sub-files (~2000-3000 lines each) using pre-computed split plans.

**Skill available:** See `.claude/skills/split-files/SKILL.md` for exact line ranges, commands, and post-split verification steps. Split into 5 sub-stories (one per file), each runnable independently:

| Sub-story | Source file | Lines | New files | Script command |
|-----------|-------------|-------|-----------|----------------|
| US-P0g1 | `oEvent.cpp` | 7405 | `oEventAdmin.cpp`, `oEventConfig.cpp` | `python3 .claude/skills/split-files/split_file.py code/oEvent.cpp "2843-5318:oEventAdmin.cpp,5319-7405:oEventConfig.cpp"` |
| US-P0g2 | `oRunner.cpp` | 7808 | `oRunnerData.cpp`, `oRunnerResult.cpp` | `python3 .claude/skills/split-files/split_file.py code/oRunner.cpp "2526-5055:oRunnerData.cpp,5056-7808:oRunnerResult.cpp"` |
| US-P0g3 | `oClass.cpp` | 5684 | `oClassConfig.cpp` | `python3 .claude/skills/split-files/split_file.py code/oClass.cpp "2836-5684:oClassConfig.cpp"` |
| US-P0g4 | `oListInfo.cpp` | 5910 | `oListInfoGen.cpp` | `python3 .claude/skills/split-files/split_file.py code/oListInfo.cpp "2884-5910:oListInfoGen.cpp"` |
| US-P0g5 | `gdioutput.cpp` | 8174 | `gdioutputEvent.cpp`, `gdioutputUI.cpp` | `python3 .claude/skills/split-files/split_file.py code/gdioutput.cpp "2708-5533:gdioutputEvent.cpp,5534-8174:gdioutputUI.cpp"` |

**Acceptance Criteria:**
- [ ] No single `.cpp` file exceeds ~3000 lines after splitting
- [ ] All split files compile and link correctly
- [ ] `MeOS.vcxproj` updated with all new files

**Implementation:** For each sub-story: run the script with `--dry-run` first, then without. After the script runs, verify file-static helpers are in the correct file (see SKILL.md for details per file), then build and fix any missing includes.

**Known Pitfalls:**
- Anonymous-namespace helpers have internal linkage — must be in the file that calls them
- `#include "stdafx.h"` must be the first include in every file (MSVC precompiled headers)
- The `PrintPostInfo` struct in `oListInfo.cpp` is used by both split files — move it to `oListInfo.h` or duplicate it

### US-P0h: Replace Win32 Time APIs with std::chrono

**Description:** Replace Windows-specific time functions (`GetLocalTime`, `SYSTEMTIME`, `GetTickCount64`, `FileTimeToLocalFileTime`, `SystemTimeToFileTime`, `TzSpecificLocalTimeToSystemTime`) with `std::chrono` equivalents in domain code.

**Acceptance Criteria:**
- [ ] No `GetLocalTime()`, `SYSTEMTIME`, `GetTickCount64()`, `FileTimeToLocalFileTime()`, or `SystemTimeToFileTime()` in domain `.cpp/.h` files
- [ ] Replacements use `std::chrono` and standard `<ctime>` functions

**Implementation Notes:**
- **Script available:** Run `python3 .claude/skills/replace_time_apis/replace_time_apis.py` to automate ~175 mechanical replacements (GetTickCount64, SYSTEMTIME declarations, GetLocalTime, field accesses). Use `--dry-run` first. Use `--inventory` to list complex patterns (FILETIME, SystemTimeToFileTime) needing manual work.
- **Prerequisites:** Before running the script, add helper functions `meos_steady_clock_ms()` and `meos_localtime_now()` to `meos_util.h` (see `.claude/skills/replace_time_apis/SKILL.md` for exact code).
- Common replacements:
  - `GetLocalTime(&st)` → `meos_localtime_now(&st)` (wrapper handles localtime_r/localtime_s)
  - `GetTickCount64()` → `meos_steady_clock_ms()` (wrapper using steady_clock)
  - `SYSTEMTIME st;` → `std::tm st = {};`
  - `st.wYear` → `(st.tm_year + 1900)`, `st.wMonth` → `(st.tm_mon + 1)`, `st.wDay` → `st.tm_mday`, etc.
  - `FILETIME` → `std::chrono::system_clock::time_point` or `time_t` (manual)
- Domain files: `meos_util.cpp` (~20 occurrences), `oEvent.cpp` (~15), `TimeStamp.cpp` (~10), `iof30interface.cpp` (~10), `oEventSpeaker.cpp` (~3), `autotask.cpp` (~5)

**Known Pitfalls:**
- `localtime_r` (POSIX) vs `localtime_s` (MSVC) have different signatures — the `meos_localtime_now()` wrapper handles this
- `SYSTEMTIME.wMilliseconds` has no direct `std::tm` equivalent — use `duration_cast<milliseconds>` on the time_point
- `meos_util.cpp` has heavily-used functions (`getLocalTime`, `getLocalDate`, `getLocalTimeOnly`) — changing their internals is safe, but signatures must be preserved
- `SystemTimeToFileTime` is timezone-independent — use `timegm`/`_mkgmtime` instead of `mktime`
- Excluded files (`Tab*`, `gdioutput`) still call utility functions whose signatures changed — grep for `SYSTEMTIME` in excluded files after running the script

### US-P0i: Replace Win32 File APIs with std::filesystem

**Description:** Replace Windows-specific file/directory APIs (`FindFirstFile`/`FindNextFile`, `GetTempPath`, `CreateDirectory`, `DeleteFile`, `_wfopen_s`, `_waccess`, `MAX_PATH`) with `std::filesystem` equivalents in domain code.

**Acceptance Criteria:**
- [ ] No `FindFirstFile`, `FindNextFile`, `WIN32_FIND_DATA`, `GetTempPath`, `CreateDirectory`, `DeleteFile`, `_wfopen_s`, `_waccess` in domain `.cpp/.h` files
- [ ] No `MAX_PATH` constant usage in domain files
- [ ] Replacements use `std::filesystem` and standard `<fstream>`

**Implementation Notes:**
- **Script available:** Run `python3 .claude/skills/replace_file_apis/replace_file_apis.py` to automate simple replacements (_waccess, DeleteFile, GetFileAttributes — ~7 calls). Use `--inventory` to list ~76 complex patterns (FindFirstFile loops, _wfopen_s, _wsplitpath_s, MAX_PATH buffers) needing manual refactoring. See SKILL.md for FindFirstFile loop replacement template.
- Common replacements:
  - `FindFirstFile`/`FindNextFile`/`FindClose` → `std::filesystem::directory_iterator`
  - `GetTempPath(MAX_PATH, buf)` → `std::filesystem::temp_directory_path()`
  - `CreateDirectory(path, NULL)` → `std::filesystem::create_directories(path)`
  - `DeleteFile(path)` → `std::filesystem::remove(path)` (automated by script)
  - `_wfopen_s(&f, path, mode)` → `std::ofstream`/`std::ifstream` with `std::filesystem::path`
  - `_waccess(path, 0)` → `std::filesystem::exists(path)` (automated by script)
  - `wchar_t buf[MAX_PATH]` → `std::filesystem::path`
- Key files: `oEvent.cpp` (3 FindFirstFile loops, 2 _wfopen_s, 3 _wsplitpath_s), `meos_util.cpp` (1 FindFirstFile loop), `HTMLWriter.cpp` (3 _wsplitpath_s), `zip.cpp` (1 FindFirstFile)

**Known Pitfalls:**
- `_wfopen_s` for binary writes (`"wb"`) should use `std::ofstream` with `std::ios::binary`
- `FindFirstFile` patterns like `L"*.meos"` map to `directory_iterator` with a manual extension filter — `std::filesystem` has no built-in glob
- Some `DeleteFile` calls are inside error-recovery paths — the script uses `error_code` overload to avoid exceptions
- `_wsplitpath_s` with `drive` parameter: `std::filesystem::path` on Linux has no drive concept — usually safe to ignore

### US-P0j: Replace Win32 Threading Primitives with std::thread/std::mutex

**Description:** Replace Windows-specific threading primitives (`CRITICAL_SECTION`, `_beginthread`/`_beginthreadex`, `TerminateThread`) with `std::mutex`, `std::thread`, and cooperative cancellation in domain code.

**Acceptance Criteria:**
- [ ] No `CRITICAL_SECTION`, `EnterCriticalSection`, `LeaveCriticalSection`, `InitializeCriticalSection`, `DeleteCriticalSection` in domain `.cpp/.h` files
- [ ] No `_beginthread`, `_beginthreadex`, `TerminateThread` in domain `.cpp/.h` files
- [ ] Thread handles use `std::thread` instead of Win32 `HANDLE`

**Implementation Notes:**
- Common replacements:
  - `CRITICAL_SECTION` → `std::mutex` (no init/delete needed)
  - `EnterCriticalSection`/`LeaveCriticalSection` → `std::lock_guard<std::mutex>`
  - `_beginthread`/`_beginthreadex` → `std::thread`
  - `TerminateThread` → cooperative cancellation with `std::atomic<bool>`
- Domain files: `SportIdent.cpp/h` (`SyncObj`, `_beginthread`, `TerminateThread`), `socket.cpp/h` (`syncObj`), `mysqldaemon.cpp` (`_beginthreadex`)

**Known Pitfalls:**
- `TerminateThread` replacement requires the thread function to periodically check a stop flag — audit the thread loops to find suitable check points
- `SportIdent.cpp` uses `CRITICAL_SECTION` in hot paths — verify no recursive locking is needed (if so, use `std::recursive_mutex`)
- Thread function signatures differ: `_beginthread` expects `void (*)(void*)`, `std::thread` accepts any callable

### US-P0k: Replace Sleep() with std::this_thread::sleep_for()

**Description:** Replace Win32 `Sleep()` calls with `std::this_thread::sleep_for()` in domain code. Simple mechanical replacement.

**Acceptance Criteria:**
- [ ] No Win32 `Sleep()` calls in domain `.cpp/.h` files
- [ ] `#include <thread>` and `#include <chrono>` added where needed

**Implementation Notes:**
- **Script available:** Run `python3 .claude/skills/sleep-replacement/fix_sleep.py` to automate all replacements. Use `--dry-run` first.
- `Sleep(N)` → `std::this_thread::sleep_for(std::chrono::milliseconds(N))`
- Domain files: `SportIdent.cpp` (~3 calls), `socket.cpp`, `newcompetition.cpp`
- Can be combined with US-P0j since both touch the same files

**Known Pitfalls:**
- Ensure the replacement is for Win32 `Sleep()` (capital S) not POSIX `sleep()` (lowercase, seconds)
- `SportIdent.cpp` Sleep() calls are inside timing-sensitive serial communication loops — preserve millisecond values exactly

### US-P0n: Replace MessageBox() and OutputDebugString() in Domain Code

**Description:** Replace direct Win32 UI calls (`MessageBox()`, `OutputDebugString()`) in domain code with cross-platform alternatives.

**Acceptance Criteria:**
- [ ] No `MessageBox()` calls in domain `.cpp/.h` files
- [ ] No `OutputDebugString()` calls in domain `.cpp/.h` files
- [ ] Replacements use exceptions, `std::function` error callbacks, or `std::cerr` as appropriate

**Implementation Notes:**
- **Script available:** Run `python3 .claude/skills/replace_msgbox_debug/replace_msgbox_debug.py` to automate OutputDebugString replacements (~30 calls → `std::cerr`). Use `--dry-run` first. Use `--inventory` to list MessageBox calls (~12 in domain files) with suggested replacements.
- `OutputDebugString()` → `std::cerr` (fully automated by script)
- `MessageBox()` → `throw meosException(msg)` for errors, callback for user dialogs (manual)
- Domain files with MessageBox: `SportIdent.cpp` (6 calls), `oClub.cpp` (2 calls), `oEvent.cpp` (1 call)
- Domain files with OutputDebugString: `autotask.cpp`, `oEvent.cpp`, `oClass.cpp`, `oEventSpeaker.cpp`, `restserver.cpp`, `localizer.cpp`, `testmeos.cpp`, and others

**Known Pitfalls:**
- `MessageBox()` is blocking (modal) — a `throw` replacement changes control flow
- `SportIdent.cpp` MessageBox calls are inside hardware configuration — may need a callback that the UI layer registers
- OutputDebugString with wide string `.c_str()` → use `narrow()` wrapper for `std::cerr`

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
- Do NOT add `find_package(OpenSSL)` or link `OpenSSL::SSL`/`OpenSSL::Crypto` — MeOS does not use OpenSSL directly. It is a transitive dependency of RestBed and is handled by that library's linkage.
- `WIN32` flag in `add_executable(MeOS WIN32 ...)` needed for GUI app (no console window)
- Library search order matters: `code/lib64/` must be found before system-installed versions

### US-P0m1: Migrate libharu to vcpkg

**Description:** Replace vendored libharu headers (`code/libharu/`) and pre-built libs (`libharu.lib`/`libhpdf.lib`) with vcpkg.

**Acceptance Criteria:**
- [ ] `libharu` declared in `vcpkg.json`
- [ ] `code/CMakeLists.txt` uses `find_package` instead of manual library paths
- [ ] Vendored `code/libharu/` directory and pre-built libs removed
- [ ] `code/pdfwriter.cpp` compiles and links correctly
- [ ] CI builds succeed

**Implementation Notes:**
- vcpkg port: `libharu`. Uses `#include "hpdf.h"` in `pdfwriter.cpp`
- libharu depends on zlib and libpng — vcpkg handles transitive dependencies

**Known Pitfalls:**
- Vendored version may differ from vcpkg port — check for API changes

### US-P0m2: Migrate MySQL Connector/C to vcpkg

**Description:** Replace vendored MySQL headers (`code/mysql/`), pre-built libs, and runtime DLLs (`code/dll/`, `code/dll64/`) with vcpkg.

**Acceptance Criteria:**
- [ ] `libmysql` declared in `vcpkg.json`
- [ ] `code/CMakeLists.txt` uses `find_package` instead of manual library paths
- [ ] Vendored `code/mysql/` directory, pre-built libs, and DLLs removed
- [ ] `code/mysqlwrapper.cpp`, `code/MeosSQL.cpp`, `code/mysqldaemon.cpp` compile and link correctly
- [ ] CI builds succeed

**Implementation Notes:**
- vcpkg port: `libmysql`. Vendored headers are MySQL 5.7 — check API compatibility
- `code/mysqlwrapper.h` includes `<mysql.h>` — verify vcpkg provides same include path
- `libmysql.dll` needed at runtime — ensure CI artifact packaging picks up vcpkg-installed DLL

**Known Pitfalls:**
- MariaDB Connector/C (`libmariadb`) is an alternative with better cross-platform support
- `code/mysql/` has a nested `mysql/` subdirectory — ensure all nested includes resolve

### US-P0m3: Migrate libpng to vcpkg

**Description:** Replace vendored libpng headers (`code/png/`) and pre-built libs with vcpkg.

**Acceptance Criteria:**
- [ ] `libpng` declared in `vcpkg.json`
- [ ] `code/CMakeLists.txt` uses `find_package(PNG)` instead of manual library paths
- [ ] Vendored `code/png/` directory and pre-built libs removed
- [ ] `code/image.cpp` compiles and links correctly
- [ ] CI builds succeed

**Implementation Notes:**
- vcpkg port: `libpng`. Uses `#include "png.h"` in `image.cpp`. Depends on zlib (transitive via vcpkg).

**Known Pitfalls:**
- Vendored `pnglibconf.h` may contain custom configuration — compare with vcpkg version

### US-P0m4: Migrate zlib to vcpkg

**Description:** Replace vendored zlib static libraries (`zlibstat.lib`/`zlibstat_vc15.lib`) with vcpkg. Note: zlib headers are bundled inside `code/minizip/`.

**Acceptance Criteria:**
- [ ] `zlib` declared in `vcpkg.json`
- [ ] `code/CMakeLists.txt` uses `find_package(ZLIB)` instead of manual library paths
- [ ] Pre-built zlib libs removed from `code/lib64/`, `code/lib64_db/`, `code/lib/`
- [ ] `code/zip.cpp` and minizip code compile and link correctly
- [ ] CI builds succeed

**Known Pitfalls:**
- `code/minizip/` contains both zlib headers and minizip source — only the zlib headers/libs are replaced here; minizip source handled by US-P0m5

### US-P0m5: Migrate minizip to vcpkg

**Description:** Replace vendored minizip source files and headers (`code/minizip/`) with vcpkg. Currently minizip is compiled directly as part of MeOS.

**Acceptance Criteria:**
- [ ] `minizip` declared in `vcpkg.json`
- [ ] `code/CMakeLists.txt` uses vcpkg integration instead of compiling minizip sources directly
- [ ] Vendored `code/minizip/` directory removed (after US-P0m4 provides zlib)
- [ ] `code/zip.cpp` compiles and links correctly
- [ ] CI builds succeed

**Implementation Notes:**
- vcpkg port: `minizip`. `code/zip.cpp` includes `"minizip/zip.h"` — verify vcpkg provides same include path
- `iowin32.h`/`iowin32.c` is the Windows-specific I/O backend — vcpkg port provides this on Windows

**Known Pitfalls:**
- Vendored minizip may include local modifications — diff against upstream before removing
- Must remove minizip `.c` compilation units from CMakeLists.txt when switching to vcpkg library

### US-P0m6: Migrate restbed to vcpkg

**Description:** Replace vendored restbed headers (`code/restbed/`) and pre-built `RestBed.lib` (20–48 MB) with vcpkg.

**Acceptance Criteria:**
- [ ] `restbed` declared in `vcpkg.json`
- [ ] `code/CMakeLists.txt` uses `find_package(restbed)` instead of manual library paths
- [ ] Vendored `code/restbed/` directory and pre-built libs removed
- [ ] `code/restserver.cpp` and `code/RestService.cpp` compile and link correctly
- [ ] CI builds succeed

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
US-P0a (include casing)     — independent, do first
US-P0b (extract utilities)  — independent
US-P0c (string functions)   — independent, easier after US-P0b
US-P0d (Win32 types)        — independent
US-P0e (path separators)    — independent
US-P0f (decouple Tab*)      — independent, benefits from US-P0b
US-P0h (time APIs)          — independent
US-P0i (file APIs)          — independent, easier after US-P0e
US-P0j (threading)          — independent
US-P0k (Sleep)              — independent, combine with US-P0j
US-P0n (MessageBox/Debug)   — independent, combine with US-P0f
US-P0m4 (zlib vcpkg)        — depends on US-P0l
US-P0m3 (libpng vcpkg)      — depends on US-P0m4
US-P0m1 (libharu vcpkg)     — depends on US-P0m3 and US-P0m4
US-P0m5 (minizip vcpkg)     — depends on US-P0m4
US-P0m7 (OpenSSL vcpkg)     — depends on US-P0l
US-P0m6 (restbed vcpkg)     — depends on US-P0m7
US-P0m2 (MySQL vcpkg)       — depends on US-P0l
US-P0g (split large files)  — do last to avoid conflicts
```

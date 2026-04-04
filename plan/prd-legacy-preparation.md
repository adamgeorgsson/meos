# PRD: MeOS Legacy Code Preparation

## Introduction

This PRD covers the preparatory work needed in the legacy `code/` directory *before* migrating files to the new `src/` modular layout. The goal is to fix cross-platform blockers, reduce GUI coupling, replace Win32-specific APIs in domain code, and clean up vendored directories after vcpkg migration.

Build system changes (CMake, vcpkg dependencies) are covered separately in `prd-legacy-build.md`.

This work can run **in parallel** with the build modernization effort and the main platform modernization effort (React frontend, database layer, etc.) since it operates exclusively on the legacy `code/` directory.

### Execution Environment

This PRD is executed by an autonomous agent running on **Windows**. The agent can verify Windows builds locally using MSBuild or CMake. The agent should:

- Focus on making correct, mechanical, safe transformations
- Use standard C++ replacements that are known to work on both MSVC and GCC/Clang
- Avoid GCC-only constructs
- Prefer conservative changes — when in doubt, use the safer option

### Constraints

- All changes must be compatible with MSVC (Visual Studio 2022, v143)
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

## Codebase Patterns (from Previous Runs)

These patterns were discovered during previous Ralph runs and should be followed:

- The most common include mismatch: `"stdafx.h"` should be `"StdAfx.h"` (capital S, capital A). Other frequent mismatches: `"tabbase.h"` → `"TabBase.h"`, `"meosException.h"` → `"meosexception.h"`, `"Localizer.h"` → `"localizer.h"`, `"Download.h"` → `"download.h"`
- C++17 `file_time_type` → `time_t` conversion: `auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(ftime - decltype(ftime)::clock::now() + std::chrono::system_clock::now()); std::time_t t = std::chrono::system_clock::to_time_t(sctp);` — two `now()` calls, small race acceptable for modification times
- `replace_file_apis.py` BUG: generates `std::filesystem::remove(x.c_str(, ec))` (malformed). Always fix manually to `{ std::error_code ec; std::filesystem::remove(x, ec); }`
- FindFirstFile wildcard replacement: use `directory_iterator` + suffix check for `*.ext` patterns; for complex wildcards like `*.bu?` or `*.dbmeos*` implement a `matchWildcard` lambda using `*` and `?` semantics
- `expandDirectory()` filetype parameter: always a `*.ext` style wildcard — extract suffix after `*` for simple extension filtering
- Pre-computed split boundaries in SKILL.md are often WRONG (file line counts drift between runs). Always verify split boundaries against actual function starts before running the split script
- Script-extracted includes blocks may miss `extern` declarations below `#ifdef` guards — always check manually for missing symbols in new split files

## User Stories

### US-P0a: Fix Include Case Sensitivity

**Description:** Normalize all `#include` directives so the quoted filename matches the actual filename on disk. Windows is case-insensitive; Linux is not — these mismatches cause build failures on Linux.

**Acceptance Criteria:**
- [ ] Every `#include "..."` directive uses exactly the same casing as the file on disk
- [ ] A verification script confirms zero case mismatches

**Implementation Notes:**
- **Script available:** Run `python3 .claude/skills/include-normalization/fix_includes.py` to fix all case mismatches automatically. Use `--dry-run` first.
- Focus on `.h` and `.cpp` files in `code/`

**Learnings from Previous Runs:**
- Run the script from the repository root (parent of `code/`), not from inside `code/`
- 147 fixes found in a single pass — no iterative runs needed
- Most mismatches are `stdafx.h` → `StdAfx.h` (affects nearly every `.cpp` file)
- `meosException.h` → `meosexception.h` (all lowercase), `Localizer.h` → `localizer.h`, `Download.h` → `download.h`, `tabbase.h` → `TabBase.h`
- The script correctly handles files in subdirectories (e.g. `minizip/`, `mysql/` subdirs)

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

**Learnings from Previous Runs:**
- `widen/narrow/toUTF8/fromUTF8` are **static methods** of `gdioutput`, not free functions — call sites use `gdioutput::widen(...)` (qualified) or `gdi.widen(...)` (via instance)
- Only **one** domain file (`machinecontainer.cpp`) directly includes `gdioutput.h` exclusively for these utilities — all other domain files also use `gdioutput` as a class
- The backward-compat strategy (keep static methods, delegate to free functions) means ALL existing call sites compile without changes
- `widen()` hardcodes CP 1252, while `recodeToWide()` uses `defaultCodePage` — these are intentionally different functions
- `meos_util.cpp` already had `extern int defaultCodePage;` but `widen()` doesn't use it (always CP 1252)

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

**Learnings from Previous Runs:**
- `_wtoi64` is NOT handled by the script — appears once in `datadefiners.h:311`, needs manual fix (`std::wcstoll`)
- `wtoi` free function must be added to `meos_util.h` BEFORE running the script (script assumes it exists)
- `wtoi` implementation: use `wcstol` not `stoi` to match `_wtoi` behavior (returns 0 on invalid input, no throw)
- Script skips `Tab*.cpp/h` and `gdioutput*.cpp/h` (UI files) — remaining calls in those files are intentionally not replaced
- Commented-out `sprintf_s` lines in `oFreeImport.cpp`, `oListInfo.cpp`, `Table.cpp` are left as-is (harmless comments)

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

**Learnings from Previous Runs:**
- Only 66 replacements found (not ~184 as noted in script docs) — count depends on codebase state at time of run
- The script excludes `SportIdent.cpp/h` and `download.cpp/h` because they call Win32 APIs with `LPDWORD` parameters — those files remain unchanged
- `toolbar.cpp` was NOT excluded even though it handles UI — it only had trivial replacements (one `DWORD`, one `TRUE`); these are safe

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

**Learnings from Previous Runs:**
- Inventory found 23 occurrences but script only replaced 16 — difference is intentional: `localizer.cpp` `L"\\n"` are newline escapes, `restserver.cpp` `"\\/.?*"` is a regex pattern
- Character comparisons like `if (c == '\\' || c == '/')` already handle both separators — no manual fixup needed
- All string backslash replacements are safe: `L".\\..\\Lists\\"` → `L"./../Lists/"` is semantically equivalent on Linux

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

**Learnings from Previous Runs:**
- Removing `TabBase.h` from `oEvent.cpp` fails on `gdibase.getTabs().clearCompetitionData()` — `FixedTabs` is only forward-declared. Fix: add `clearTabsCompetitionData()` wrapper to `gdioutput.h/cpp`
- `oEvent.cpp` nested class `RefreshFilter` holds `oEvent& oe` — callback access must use `oe.cbBaseButtons`, not bare `cbBaseButtons`
- `lang` (Localizer) is provided transitively via `TabAuto.h` → `TabBase.h` → `localizer.h`. Must add `#include "localizer.h"` directly to `oEventSQL.cpp` when removing `TabAuto.h`
- `isThreadReconnecting()` is declared only in `TabAuto.h` — forward-declare it directly in `oEventSQL.cpp` after removing the include
- `TabAuto` has its own `synchronize`/`synchronizePunches` fields (private) separate from `AutoMachine::synchronize`/`synchronizePunches` (public). Do not confuse them.
- `gdi_main` is a global in `meos.cpp` — lambdas don't need to capture it
- `TabList::baseButtons`, `TabAuto::tabAutoKillMachines`, `hasActiveReconnectionMachine` are static — their lambdas need no capture
- `autotask.cpp`, `oEventResult.cpp`, `metalist.cpp`, `onlineinput.cpp` are all clean/simple decouplings with no transitive include issues

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

**Learnings from Previous Runs:**
- **Boundary corrections needed:** oEvent.cpp boundaries (2843→2835, 5319→5335), oClass.cpp (2836→2838, 5684→5686), gdioutput.cpp (5533→5534, 8174→8153) were all WRONG. oRunner.cpp (2526, 5056) and oListInfo.cpp (2884, 5910) were correct.
- File line counts drift between stories (US-016–US-020 added lines to oEvent.cpp). Re-verify boundaries each run.
- Script copies includes block but NOT anonymous-namespace or static function definitions — grep new files for cross-file references and create `*Internal.h` headers as `static inline`
- oEvent.cpp: `getNewFileName` (anonymous namespace) → `oEventInternal.h`
- oRunner.cpp: `findNextControl`, `gotoNextLine`, `addMissingControl` (static functions) → `oRunnerInternal.h`
- oClass.cpp: `ClassSplit::evaluateTime/Result/Points` (file-local class methods) → `oClassInternal.h` as free inline functions
- oListInfo.cpp: `PrintPostInfo` struct + `getControlName`, `getFullControlName`, `getResultTitle`, `generateNBestHead` → `oListInfoInternal.h`
- gdioutput.cpp: no internal header needed — all file-local helpers are self-contained in their split file
- oClass.cpp: `#define DODECLARETYPESYMBOLS` is in the includes block and gets copied — MUST remove from `oClassConfig.cpp` or LNK2005 multiply-defined-symbol errors occur
- gdioutput.cpp: `extern int defaultCodePage;` (line 71) is NOT in the includes block (extraction breaks at `#ifdef DEBUGRENDER`) — must add manually to `gdioutputUI.cpp`
- Place `#include "*Internal.h"` in the includes block at top of file, not where deleted functions were

### US-P0h: Replace Win32 Time APIs with std::chrono

**Description:** Replace Windows-specific time functions (`GetLocalTime`, `SYSTEMTIME`, `GetTickCount64`, `FileTimeToLocalFileTime`, `SystemTimeToFileTime`) with `std::chrono` equivalents in domain code.

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

**Learnings from Previous Runs:**
- Script makes 175 mechanical replacements but leaves 76 complex patterns (FILETIME, function signatures, TzSpecificLocalTime). ALL must be fixed manually before the code compiles.
- Script replaces `st.wYear` in BOTH read AND write contexts, creating invalid `(st.tm_year + 1900) = value`. Fix write-contexts to `st.tm_year = value - 1900` and `st.tm_mon = value - 1`. Key files: `TimeStamp.cpp::setStamp()`, `meos_util.cpp::convertDateYMD()`, `oEventSpeaker.cpp`.
- `wMilliseconds` TODO expressions of form `0 /* TODO */ = expr;` must be removed — they're invalid lvalue assignments
- `TzSpecificLocalTimeToSystemTime(0, &local, &utc)` → `mktime(&local)` + `gmtime` (local→UTC)
- `SystemTimeToTzSpecificLocalTime(0, &utc, &local)` → `_mkgmtime(&utc)` + `localtime_s` (UTC→local)
- FILETIME → `time_t`: `(ft_val - 116444736000000000ULL) / 10000000`
- TimeStamp.cpp encoding: `_mkgmtime(local_tm)` + `WIN_UNIX_EPOCH_OFFSET = 11644473600`. Decode: compute `time_t` from Time and use `gmtime`.
- After the script, remaining FILETIME hits in `oEvent.cpp`/`meos_util.cpp`/`TimeStamp.cpp` are just comments — verify each manually
- `zip.cpp` FILETIME patterns deferred to a separate story (US-017). tm_unz uses full year (e.g. 2024); subtract 1900 when copying to `std::tm.tm_year`
- `progress.cpp` is intentionally excluded by the script

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

**Learnings from Previous Runs:**
- Script bug: `DeleteFile` replacement generates malformed `std::filesystem::remove(x.c_str(, ec))` — always fix these manually after running the script
- `GetFileAttributes` in `meos_util.cpp:fileExists()` flagged by `--inventory` but NOT replaced by script — must replace manually
- `enumerateBackups()` uses `*.meos.bu?` and `*.dbmeos*` wildcards — needs a proper `matchWildcard` lambda (standard DP/greedy algorithm)
- `file_time_type` → `time_t` conversion uses two-clocks workaround (C++17): `ftime - clock::now() + system_clock::now()`
- `entry.file_size(ec)` returns `uintmax_t`; `BackupInfo::fileSize` is `size_t` — implicit conversion is fine
- MAX_PATH/`_MAX_PATH` not in script inventory — always grep manually after running
- `extern wchar_t exePath[MAX_PATH]` in domain files: safe to replace with literal 260 (matches definition in `meos.cpp`)
- `_wsplitpath_s` with NULL for extension → `std::filesystem::path::stem()` is the exact equivalent
- `_wsplitpath_s` for parent+drive → `parent_path().wstring()` + trailing-separator check (original always had trailing backslash)
- `fread_s` is not portable — replace entire `FILE*` block with `std::ifstream` + `std::ios::ate` for file-size detection

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

**Learnings from Previous Runs:**
- `MySQLReconnect` uses `clone()` via `make_shared<MySQLReconnect>(*this)` — requires user-defined copy constructor since `std::thread` is not copyable. Default-construct the thread member in the copy constructor.
- Download's `_beginthread` self-zeroes `hThread` to signal completion — impossible with `std::thread`. Solution: add `std::atomic<bool> threadRunning` as a completion flag.
- `initMySQLCriticalSection(bool init)` becomes a no-op with `std::mutex` (RAII) — keep function signature, empty body
- `reconnectThread` signature: change from `unsigned __stdcall (void*)` to `static void (oEvent*)` — cleaner and type-safe with `std::thread`
- `code/mysql/thr_mutex.h` has `CRITICAL_SECTION` — vendored MySQL header, do NOT modify
- `socket.cpp`'s `_beginthread` is fire-and-forget (no stored handle) — replace with `std::thread(...).detach()`
- `SportIdent.cpp/h` may already be partially converted — check for stale `ThreadHandle=0` assignments

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

**Learnings from Previous Runs:**
- All 30 call sites replaced correctly by the script — no manual fixups needed
- `Sleep(0)` (yield-style) → `sleep_for(milliseconds(0))` is semantically equivalent
- `SportIdent.cpp` had the most calls (25) — all in timing-sensitive serial communication loops; values preserved verbatim
- No files had pre-existing `<thread>` or `<chrono>` includes — all 5 files needed both headers added

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

**Learnings from Previous Runs:**
- Script only matches `OutputDebugString(` — MISSES `OutputDebugStringA(` and `OutputDebugStringW(` variants. Replace those manually.
- Only 4 active `OutputDebugStringA` calls found (not ~30 as noted) — most were already commented out or inside block comments
- All `MessageBox()` calls in domain files were already commented out — no manual replacement needed
- Use a block-comment-aware scanner (not just `grep -v //`) to correctly identify active vs. commented code inside `/* */` blocks

### US-P0m-cleanup: Remove Vendored Third-Party Directories

**Description:** After vcpkg migrations (US-P0m1–m7 in prd-legacy-build.md) are verified working, remove the vendored directories and pre-built libraries from `code/`.

**Acceptance Criteria:**
- [ ] `code/libharu/` directory removed (after US-P0m1)
- [ ] `code/mysql/` directory removed (after US-P0m2)
- [ ] `code/png/` directory removed (after US-P0m3)
- [ ] Pre-built zlib libs removed from `code/lib64/`, `code/lib64_db/`, `code/lib/` (after US-P0m4)
- [ ] `code/minizip/` directory removed (after US-P0m5)
- [ ] `code/restbed/` directory removed (after US-P0m6)
- [ ] Pre-built libs (`libharu.lib`, `libhpdf.lib`, `RestBed.lib`, etc.) removed from `code/lib64/`, `code/lib64_db/`
- [ ] Runtime DLLs removed from `code/dll/`, `code/dll64/` (after US-P0m2)
- [ ] CI builds still succeed after removal

**Dependencies:** All US-P0m* stories in prd-legacy-build.md must be completed and verified first.

**Known Pitfalls:**
- Vendored minizip may include local modifications — diff against upstream before removing
- `code/minizip/` contains both zlib headers and minizip source — only remove after both US-P0m4 and US-P0m5 are done
- Verify no source files still reference vendored include paths before deleting

**Learnings from Previous Runs:**
- Story was SKIPPED in this run — blocked by `prd-legacy-build.md` stories (US-P0m*) which don't exist in this repo yet

## Dependency Order

```
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
US-P0m-cleanup (remove vendored dirs) — depends on all US-P0m* in prd-legacy-build.md
US-P0g (split large files)  — do last to avoid conflicts
```

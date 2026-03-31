# Migration Skill — MeOS Legacy → Modern C++17/CMake

Accumulated patterns and gotchas for migrating MeOS from Win32/MSBuild (`code/`) to modern C++17/CMake/vcpkg (`src/`).

## Iterative Migration Context

Each migration attempt is executed from scratch by Ralph, analyzed, and discarded — only learnings persist. This is a fork of [melinsoftware/meos](https://github.com/melinsoftware/meos) that syncs with upstream. Therefore:

- **Never hardcode assumptions about legacy code** (line numbers, exact signatures, specific file contents).
- **Always read and parse legacy code dynamically** — enumerate files, grep for patterns, follow naming conventions.
- **Describe the *shape* of code** (e.g., "domain files include gdioutput.h"), not exact locations that may shift.

## Architecture Overview

### Source Layout

| Legacy | Modern | Purpose |
|--------|--------|---------|
| `code/` (flat) | `src/app/` | Application entry point, resources, lang |
| `code/` | `src/domain/` | Core domain entities (`oRunner`, `oClass`, etc.) |
| `code/` | `src/net/` | Networking, REST API |
| `code/` | `src/db/` | Database (SQLite/MySQL wrapper) |
| `code/` | `src/util/` | Utilities, stubs, platform shims |
| `code/` | `src/io/` | File I/O, import/export, printing |
| `code/` | `src/ui/` | Legacy Win32/GDI code |

Each module is a **static library**. Bare `#include` works within modules via common include paths in `CMakeLists.txt`.

### Domain Model

`oEvent` is the aggregate root — it owns all domain objects. Entity classes use the `o` prefix:

```
oBase (abstract: ID, change tracking)
├── oRunner  ─┐
├── oTeam     ├─ both extend oAbstractRunner
├── oClass
├── oClub
├── oCourse
├── oControl
├── oCard
├── oFreePunch
└── oPunch
```

## Critical Migration Patterns

### 1. Win32 Type Shimming

Redefine common Win32 types in `src/util/win_types.h` for Linux compatibility:
- `SYSTEMTIME`, `DWORD`, `HWND`, `RECT`, `COLOR`
- File search: `WIN32_FIND_DATA`
- String safe functions: `swprintf_s`, `snprintf_s`
- Basic GUI types for stubs

**Gotchas:**
- **Incomplete Types**: Classes like `GDIImplFontEnum` and `GDIImplFontSet` must be defined (even if empty) rather than just forward-declared when used as value types in `std::vector` or `std::map` members on Linux/GCC.
- **Missing Typedefs**: Ensure `INT`, `LPSIZE`, `LPOPENFILENAME`, and `LPBROWSEINFO` are defined in `win_types.h`.
- **Macro Redefinitions**: Wrap Win32 resource IDs (like `IDD_SPLASH`) in `#ifndef` to avoid conflicts with `resource.h`.
- **Macro vs Inline**: Prefer inline functions for Win32 API shims (like `OffsetRect`) to avoid issues with `return` statements and expression evaluation.

### 2. Header & Include Fixes

- **Case sensitivity**: Windows is case-insensitive; Linux is not. Filenames like `StdAfx.h` vs `stdafx.h` break on Linux. Fix all case mismatches in `code/` before migrating (there are ~147 known mismatches).
- **Verification**: After fixing, verify zero case mismatches for `#include "..."` directives by scanning `code/` and comparing against actual filenames on disk.

### 3. String Conversions

- Wide strings (`wstring`) are primary (Swedish/internationalized UI).
- Narrow `string` for internal/config data.
- **Use global string utility functions from `meos_util.h` for conversions:**
  - `widen(const string&)`: Windows-1252 to `wstring` (standard MeOS conversion)
  - `narrow(const wstring&)`: `wstring` to `string` (simple truncation)
  - `toUTF8(const wstring&)`: `wstring` to UTF-8 `string`
  - `fromUTF8(const string&)`: UTF-8 `string` to `wstring`
  - `recodeToWide(const string&)`: `defaultCodePage` to `wstring` (for external data)
  - `recodeToNarrow(const wstring&)`: `wstring` to `defaultCodePage`
- These return references to strings in `StringCache` for efficiency/brevity.
- **Note**: In legacy code, these functions exist as static methods on `gdioutput`. Extract them to global functions in `meos_util.h` and replace all static/member calls across the legacy codebase. `gdioutput` methods become thin wrappers for backward compatibility.

### 4. Win32 API Replacement Table

| Win32 | Standard C++ replacement |
|-------|--------------------------|
| `_itow_s` | `swprintf(buf, size, L"%d", val)` |
| `sprintf_s` | `snprintf` |
| `swprintf_s` | `swprintf` |
| `_stricmp` / `_wcsicmp` | `compareStringIgnoreCase(a, b)` |
| `lstrcmpi` | `compareStringIgnoreCase(a, b)` |
| `_wtoi` | `(int)std::wcstol(str, nullptr, 10)` |
| `_wtof` | `std::wcstod(str, nullptr)` |
| `_wtoi64` | `(long long)std::wcstoll(str, nullptr, 10)` |
| `MultiByteToWideChar` | `codecvt` / `widen()` |
| `CharLowerBuff` | `towlower(wchar_t)` in a loop |
| `FindResource` / `LoadResource` | `std::ifstream` + predefined search paths |
| `_wsopen_s` / `_read` / `_write` | `std::ifstream` / `std::ofstream` (binary mode) |
| `OffsetRect` | `inline BOOL OffsetRect(LPRECT lprc, int dx, int dy)` |
| `_wsplitpath_s` / `_splitpath_s` | `std::filesystem::path` methods: `.stem()`, `.extension()`, `.filename()`, `.parent_path()` |
| `GetFileAttributes` | `std::filesystem::exists` |
| `FindFirstFile`/`FindNextFile` | `std::filesystem::directory_iterator` + `matchWildcard()` from `meos_util.h` |
| `DeleteFile` | `std::filesystem::remove(path, ec)` (use `error_code` overload for silent error recovery) |
| `CreateDirectory` | `std::filesystem::create_directory(path, ec)` |
| `_wfopen_s(&fp, path, L"wb")` | `std::ofstream(std::filesystem::path(path), std::ios::binary)` |
| `_wfopen_s(&fp, path, L"rb")` | `std::ifstream(std::filesystem::path(path), std::ios::binary)` |
| `_waccess(path, 0)==0` (exists) | `std::filesystem::exists(path)` |
| `GetCurrentDirectory` | `std::filesystem::current_path()` |
| `FileTimeToDosDateTime` | Manual encoding: date=`(y-80)<<9\|m<<5\|d`, time=`h<<11\|min<<5\|sec>>1` (see zip.cpp) |

Replace Win32 functions and types in domain files (`o*.cpp/h`, `generalresult.cpp/h`, `metalist.cpp/h`, `datadefiners.h`). Add `#include <cstdint>` to headers using `uint*_t` types.

### 5. Path Separators

- **Use `std::filesystem::path`** for all path manipulations and construction. Alias as `path` in `StdAfx.h`.
- Replace `path += L"\\file.ext"` with `(std::filesystem::path(path) / L"file.ext").wstring()`.
- Replace `GetFileAttributes` with `std::filesystem::exists`.
- Use forward slashes (`/`) in hardcoded path string literals: `L"./../Lists/"`.
- Check for both separators where needed: `if (c == '/' || c == '\\')`.
- Normalize the ~47 hardcoded backslash (`\\`) path strings found in files like `meos.cpp`, `zip.cpp`, `oClub.cpp`, `oEvent.cpp`, `oEventSpeaker.cpp`.

### 6. Circular Dependency Management

- Move shared enums (e.g., `SpecialPunch`) to `domain_header.h` or `src/util/common_enums.h` to break circular deps between domain entities.
- Use forward declarations heavily in `domain_header.h` to reduce circular dependencies between `oBase`, `oDataContainer`, `oEvent`, and UI classes.
- Template implementation files (`*impl.hpp`) must be included at the end of their headers (e.g., `intkeymap.hpp` includes `intkeymapimpl.hpp`).

### 7. Heavy Stubbing Strategy

The codebase is **extremely coupled**. Migrating one class often requires stubbing many others:

- Stub GUI classes: `gdioutput`, `oSpeaker`, `Table`
- Stub unmigrated domain entities in `src/domain/` (minimal `.h` with just enough API)
- `oEvent.h` needs extensive stubbing — it touches everything
- `oRunner` is ~230KB; use simplified implementation initially
- **Foundation Stubs**: Provide minimal implementations for heavily used methods of `oEvent`, `gdioutput`, and `Table` in `domain_module.cpp` to allow the domain library and its tests to link without pulling in the entire UI/Server layer.

### 8. GUI Coupling in Domain Files

9 of 11 domain files include `gdioutput.h` directly (all except `oBase.cpp`, `oPunch.cpp`). `oEvent.cpp` also includes `TabBase`, `TabAuto`, `TabSI`, `TabList`.

**Strategy**: Extract non-GUI utility functions (string conversions) from `gdioutput` to `meos_util.h` as globals. See section 9 for callback-based decoupling patterns.

#### Headless gdioutput

When migrating classes that depend on `gdioutput` (like `MetaList`, `HTMLWriter`, `oBase`), use the abstract `gdioutput` base class defined in `src/util/gdioutput.h`.

- **Colors and Fonts:** Use `GDICOLOR` enum and `RGB(r,g,b)` macro from `src/util/gdioutput.h`. Use `gdiFonts` enum for font styles. Use `MeOSUtil::HLS` class for background color manipulation (zebra-striped tables).
- **Layout Metrics:** `gdioutput::getLineHeight()` for default line height. `PageInfo::renderPages()` for splitting `TextInfo` objects into pages (multi-column layouts, page breaks).
- **String Handling:** Use `MeOSUtil::toUTF8()` for HTML/XML output. Use `MeOSUtil::encodeXML()` / `MeOSUtil::encodeHTML()` for escaping.
- **Gotchas:** `domain_header.h` should NOT define its own `GDICOLOR`/`gdiFonts` — include `../util/gdioutput.h`. `TextInfo` uses `RECT` from `gdioutput.h` (replaces Win32 `RECT`).

### 9. oEvent → Tab* Direct Coupling

`oEvent.cpp` has direct calls into UI Tab classes that must be decoupled:

| Call | Purpose | Replacement strategy |
|---|---|---|
| `TabList::baseButtons(gdi, 1, false)` | Renders UI buttons in list view | Callback `std::function<void(gdioutput&, int, bool)>` registered by TabList |
| `TabAuto::tabAutoKillMachines()` | Kills automatic timing machines | Callback `std::function<void()>` registered by TabAuto |
| `TabSI::getSI(gdiBase()).setSubSecondMode(use)` | Sets SportIdent sub-second mode | Callback `std::function<void(bool)>` registered by TabSI |

Expose callback typedefs (e.g., `BaseButtonsCallback`) on `oEvent.h` via `std::function`. UI classes register their callbacks during application initialization. This decouples the domain aggregate root from the UI layer.

#### Generic Decoupling Recipe

1. **Identify UI Dependency:** Find direct calls from domain code to UI classes.
2. **Add Callback Member:** Add a `std::function` member to the domain class header.
    ```cpp
    #include <functional>
    std::function<int(gdioutput&, int, bool)> baseButtonsCallback;
    ```
3. **Add Setter:** Public setter for the callback.
    ```cpp
    void setBaseButtonsCallback(std::function<int(gdioutput&, int, bool)> cb) { baseButtonsCallback = cb; }
    ```
4. **Replace Direct Call:** Remove the UI header, replace with callback (checked for validity).
    ```cpp
    if (baseButtonsCallback) baseButtonsCallback(gdi, 1, false);
    ```
5. **Register Callback:** In `meos.cpp` (composition root), register the UI method after domain init.
    ```cpp
    gEvent = new oEvent(*gdi_main);
    gEvent->setBaseButtonsCallback(TabList::baseButtons);
    ```

Always check if the callback is set before calling. Use lambdas if extra context is needed.

### 10. Time Handling

- **TimeStamp.h/cpp**: Modernize to use standard `snprintf`/`swprintf` and standard C++ types.
- **timeconstants.hpp**: Centralize time-related constants (`timeUnitsPerSecond`, `timeConstSecPerHour`).
- **Win32 Time Shims**:
  - Shim `GetLocalTime`, `GetSystemTime` using `gettimeofday` and `localtime_r`/`gmtime_r`.
  - **CRITICAL**: `SystemTimeToFileTime` and `FileTimeToSystemTime` must be timezone-independent to match Win32 behavior. Use `timegm` instead of `mktime` to avoid local timezone shifts during round-trips.
- **Decoupling**: Utility classes like `TimeStamp` should not include heavy application headers like `meos.h`. Ensure they only depend on `StdAfx.h` and other necessary utilities.
- **Custom Epoch**: MeOS uses a custom 32-bit unsigned epoch relative to 2014-01-01 for `TimeStamp::Time`. This fits ~136 years of seconds in a 32-bit integer.

### 11. Locale

- Set locale to `"C.UTF-8"` early (in `main()` or test setup).
- Without this, `wcscasecmp` and `towlower` fail for non-ASCII characters (Swedish å/ö/ä).
- `std::setlocale(LC_ALL, "C.UTF-8")` or `setlocale(LC_ALL, "C.UTF-8")`.

### 12. Multi-Character Literals

- MSVC accepts `'--'` and `'x'` as multi-char constants.
- Replace with wide character literals: `L'\u2013'` (en-dash), `L'\u00D7'` (multiplication sign).

### 13. Redundant Overload Avoidance

On 64-bit Linux, `unsigned long` == `uint64_t`. Avoid redundant overloads for:
- `itos()` / `itow()` — don't provide both `unsigned long` and `uint64_t` versions

### 14. Dependency Management (vcpkg)

- **libmysql**: Fragile on Linux via vcpkg. Consider `libmariadb` or system packages, or stub MySQL for minimal builds.
- **Package Names**: Some vcpkg packages use `unofficial-` prefix (e.g., `unofficial-libharu`, `unofficial-minizip`).

## Test Infrastructure

- **GTest**: `find_package(GTest CONFIG REQUIRED)` + link `GTest::gtest GTest::gtest_main`.
- **Coverage**: Pass `-DCOVERAGE=ON` to CMake to enable `--coverage` flags for GCC/Clang.
- **Stubs**: Heavy coupling often requires stubs in `src/util/meos_stubs.cpp` to make modules like `util` or `domain` compile in isolation.
- **Data Container Initialization**: Ensure `oe->oControlData` is initialized before testing — `oDataContainer` requires explicit setup.
- **`StringCache` Initialization**: Must be initialized via constructor; otherwise `wget()`/`get()` will segfault.
- **Protected Member Access**: Use `#define protected public` before including headers to access internal state in tests.
- **`enable_testing()`** must be in top-level `CMakeLists.txt`.

### 15. Threading — CRITICAL_SECTION → std::mutex

- `CRITICAL_SECTION` → `std::mutex`; no explicit Init/Delete needed (C++ RAII).
- `EnterCriticalSection/LeaveCriticalSection` → `std::lock_guard<std::mutex>` in a scope block.
- `_beginthread`/`_beginthreadex` → `std::thread(fn, args...).detach()` (fire-and-forget) or store `std::thread` for joinable threads.
- **`SI_StationInfo` copyability**: `std::thread` is move-only, but `SI_StationInfo` is copied in `start_si_thread`. Use a `CopyableThread` wrapper whose copy-constructor creates an empty (not joinable) thread. The copied `ThreadHandle` is never used inside the thread, so the empty copy is safe.
- **`TerminateThread` replacement**: Use `detach()` — the thread exits naturally when the resource it reads from (COM port, socket) is closed. Close the resource AFTER detaching.
- **`GetExitCodeThread` equivalent**: There is none in std::thread without auxiliary state. For TCP port wait loops, use a timeout (e.g., 500×10ms) rather than polling thread exit code.
- **`hThread` in UI class (TabAuto.h)**: If the thread handle is stored in a UI class (can't be changed), launch with `std::thread(...).detach()` and set `hThread = nullptr` — state is tracked by other atomics anyway.
- **`initMySQLCriticalSection`**: Keep as no-op function for backward compatibility with meos.cpp call-site; `std::mutex` doesn't need explicit init/cleanup.

## Known Gotchas

0. **Legacy code is not static.** This is a fork that syncs with upstream. File contents, line numbers, function signatures, and even file names may change between migration runs. Always discover code structure dynamically rather than hardcoding assumptions.
1. **oEvent Method Scattering**: `oEvent` method implementations are often scattered across other domain entity files (e.g., `oEvent::fillControls` is in `oControl.cpp`). Always search the entire domain directory when looking for a method definition.
2. **Heavy Virtual Stubbing**: When stubbing heavily coupled classes like `oRunner` or `oTeam` to link a subset of the domain, you must implement ALL non-inline virtual methods declared in their headers to avoid missing vtable errors.
3. **CMake static libs need at least one source file** — use `*_dummy.cpp` placeholder.
4. **Localization** uses `#` as separator for substitutions. Placeholders `X, Y, Z, W` must be in both key and translation.
5. **Static members and globals** (like `lang`) need careful handling in modular builds.
6. **oEvent.cpp** is massive — don't try full migration in one step. Use a minimal skeleton initially.
7. **cpp-httplib `set_error_handler`** is called even when a route handler explicitly sets a 4xx/5xx status — it will overwrite the handler's JSON body. Guard with `if (!res.body.empty()) return;` at the top of the error handler to preserve route-handler-set bodies.
8. **cpp-httplib integration tests**: After `server.start()`, sleep ~50ms before sending client requests to ensure `listen_after_bind()` thread is ready.

### 16. REST API Layer (src/net/)

- **cpp-httplib CMake**: `find_package(httplib CONFIG REQUIRED)` + `target_link_libraries(... httplib::httplib)`.
- **nlohmann-json CMake**: `find_package(nlohmann_json CONFIG REQUIRED)` + `target_link_libraries(... nlohmann_json::nlohmann_json)`.
- **Non-blocking server**: `srv.bind_to_port(host, port)` then `srv.listen_after_bind()` in a `std::thread`.
- **API versioning**: All routes use `/api/v1/` prefix — ApiRouter::prefix() adds it automatically.
- **Error envelope**: `{"error":{"code":<int>,"message":"..."}}` for errors; `{"data":{...}}` for success.
- **ApiException** hierarchy: throw from route handlers; ApiRouter::wrap() catches and serializes automatically.
- **Route handler signature**: `json(const Request&)` — return JSON; throws propagate to wrap().


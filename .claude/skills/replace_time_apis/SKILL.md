# Replace Time APIs Skill

Replace Win32 time APIs with std::chrono in domain code (US-P0h from prd-legacy-preparation.md).

## Prerequisites

Before running the script, add these helper functions to `meos_util.h/cpp`:

```cpp
// meos_util.h
#include <chrono>
#include <ctime>

/// Returns monotonic milliseconds (replaces GetTickCount64)
inline uint64_t meos_steady_clock_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count();
}

/// Fill std::tm with current local time (replaces GetLocalTime)
inline void meos_localtime_now(std::tm* out) {
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
#ifdef _WIN32
    localtime_s(out, &tt);
#else
    localtime_r(&tt, out);
#endif
}
```

## What the Script Automates

| Win32 | Replacement |
|-------|------------|
| `GetTickCount64()` | `meos_steady_clock_ms()` |
| `SYSTEMTIME st;` | `std::tm st = {};` |
| `GetLocalTime(&st)` | `meos_localtime_now(&st)` |
| `st.wYear` | `(st.tm_year + 1900)` |
| `st.wMonth` | `(st.tm_mon + 1)` |
| `st.wDay` | `st.tm_mday` |
| `st.wHour` | `st.tm_hour` |
| `st.wMinute` | `st.tm_min` |
| `st.wSecond` | `st.tm_sec` |
| `st.wDayOfWeek` | `st.tm_wday` |
| `st.wMilliseconds` | `0 /* TODO */` (std::tm has no ms) |
| `memset(&st, 0, sizeof(SYSTEMTIME))` | `st = {};` |

## What Needs Manual Work (inventoried by --inventory)

- `SystemTimeToFileTime` / `FileTimeToLocalFileTime` — Rewrite using `mktime`/`std::chrono`
- `SystemTimeToInt64TenthSecond` / `Int64TenthSecondToSystemTime` — Rewrite in `meos_util.cpp`
- `convertSystemTime*` / `convertDateYMD` — Update signatures from `SYSTEMTIME` to `std::tm`
- `FILETIME` usage — Replace with `time_t` or `std::chrono::system_clock::time_point`

## Procedure

1. **Add helpers**: Add `meos_steady_clock_ms()` and `meos_localtime_now()` to `meos_util.h`
2. **Inventory**: `python3 .claude/skills/replace_time_apis/replace_time_apis.py --inventory`
3. **Dry run**: `python3 .claude/skills/replace_time_apis/replace_time_apis.py --dry-run`
4. **Apply**: `python3 .claude/skills/replace_time_apis/replace_time_apis.py`
5. **Manual**: Handle complex patterns from inventory (FILETIME, convert* functions)
6. **Verify**: `grep -rn 'GetLocalTime\|GetTickCount64\|SYSTEMTIME\|FILETIME' code/ --include='*.cpp' --include='*.h' | grep -v Tab | grep -v gdioutput | grep -v meos.cpp`

## Key Files (by occurrence count)

- `meos_util.cpp` — ~20 occurrences (GetLocalTime, SYSTEMTIME, convertSystem* functions)
- `oEvent.cpp` — ~15 occurrences (GetLocalTime, GetTickCount64, SYSTEMTIME, FILETIME)
- `TimeStamp.cpp` — ~10 occurrences (SYSTEMTIME, FILETIME, SystemTimeToFileTime)
- `iof30interface.cpp` — ~10 occurrences (SYSTEMTIME field access)
- `oEventSpeaker.cpp` — ~3 occurrences (GetLocalTime, SYSTEMTIME)
- `autotask.cpp` — ~5 occurrences (GetTickCount64)

## Gotchas

- **`wMilliseconds`**: `std::tm` has no millisecond field. For `meos_util.cpp` functions that use milliseconds, use `std::chrono::system_clock::now()` and extract ms via `duration_cast`.
- **`localtime_r` vs `localtime_s`**: Different argument order! `localtime_r(time_t*, tm*)` vs `localtime_s(tm*, time_t*)`. The helper function handles this.
- **FILETIME epoch**: Windows FILETIME uses 100ns intervals since 1601-01-01. `time_t` uses seconds since 1970-01-01. Conversion needs the offset constant.
- **`SystemTimeToFileTime` timezone**: Win32 version is timezone-independent. Use `timegm`/`_mkgmtime` instead of `mktime` to match.

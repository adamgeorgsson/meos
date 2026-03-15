# Time Migration Skill

## Purpose
Guide the migration of Win32 time APIs to cross-platform C++17 implementations in MeOS.

## Core Mappings
- `SYSTEMTIME` / `FILETIME` -> Use `std::tm` for decomposition and `std::time_t` or `std::chrono::system_clock` for storage.
- `GetLocalTime(&st)` -> Use `std::time(nullptr)` and `localtime_r` (Linux) or `localtime_s` (Windows).
- `SystemTimeToFileTime(&st, &ft)` -> Use `timegm` (Linux) or `_mkgmtime` (Windows) to treat a `tm` struct (filled with local values) as UTC, maintaining bit-compatibility with legacy "local seconds since epoch".

## Epoch and Constants
- **MeOS Epoch**: Starts at 1601-01-01 plus `(2014-1601) * 365` days.
- **Constant**: `13024368000` seconds since 1601-01-01.
- **Unix Offset**: MeOS Epoch is `1379894400` seconds after Unix Epoch (1970-01-01).
- **Time Units**: MeOS uses `timeUnitsPerSecond = 10` (tenths of a second) for many relative times.

## Formatting Patterns
- **StringCache**: Use a `thread_local` circular buffer of `std::wstring` to support the legacy pattern of returning references to temporary formatting strings.
- **formatTime**:
  - `SubSecond::Off`: `HH:MM:SS` or `MM:SS`
  - `SubSecond::On`: `HH:MM:SS.t` or `MM:SS.t`
  - `SubSecond::Auto`: Only show tenths if not zero.

## Example Conversion
```cpp
// Legacy
SYSTEMTIME st;
GetLocalTime(&st);
FILETIME ft;
SystemTimeToFileTime(&st, &ft);
__int64 currenttime = *(__int64*)&ft;
unsigned int meosTime = (currenttime / 10000000L) - offset;

// Modern (src/util/TimeStamp.cpp)
auto now = std::chrono::system_clock::now();
std::time_t t = std::chrono::system_clock::to_time_t(now);
std::tm local_tm;
localtime_r(&t, &local_tm);
std::time_t local_seconds_as_utc = timegm(&local_tm);
unsigned int meosTime = local_seconds_as_utc - meosEpochOffsetFromUnix;
```

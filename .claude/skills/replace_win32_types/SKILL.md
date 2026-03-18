# Replace Win32 Types Skill

Replace Win32-specific type aliases with standard C++ types in domain code (US-P0d from prd-legacy-preparation.md).

## Replacements

| Win32 | Standard C++ | Notes |
|-------|-------------|-------|
| `DWORD` | `uint32_t` | Add `#include <cstdint>` |
| `BOOL` | `bool` | Win32 `BOOL` is `int`; check `TRUE`/`FALSE` comparisons |
| `TRUE` | `true` | |
| `FALSE` | `false` | |

## Scope

**In scope (domain/infrastructure files):**
- `oEvent*.cpp/h`, `oRunner.cpp`, `oClass.cpp`, `oClub.cpp`, `oCard.cpp`, `oTeam.cpp`, etc.
- `meos_util.cpp/h`, `SportIdent.cpp/h`, `autotask.cpp/h`, `download.cpp/h`
- `MeosSQL.cpp/h`, `restserver.cpp`, `xmlparser.cpp`, etc.

**Out of scope:**
- `Tab*.cpp/h`, `gdioutput.*`, `meos.cpp`, `testmeos.cpp`, `Table.*`, `progress.cpp`, `printer.cpp`
- Third-party: `mysql/`, `restbed/`, `png/`, `libharu/`, `minizip/`

## Procedure

1. **Audit**: `python3 .claude/skills/replace_win32_types/replace_win32_types.py --dry-run`
2. **Apply**: `python3 .claude/skills/replace_win32_types/replace_win32_types.py`
3. **Verify**: Re-run `--dry-run` to confirm zero remaining.

## Edge Cases

- **MeosSQL.cpp**: Contains `DWORD` and `BOOL` inside SQL string literals — the script skips these.
- **`DWORD(expr)` casts**: Become `uint32_t(expr)` — valid C++ functional cast.
- **`BOOL` in callback signatures**: `printer.cpp` has `BOOL CALLBACK` — excluded by file filter.
- **`oEvent.h` public API**: Function signatures like `getAbsTime(DWORD time)` are replaced to `getAbsTime(uint32_t time)`. All callers in domain code are updated simultaneously.
- **`SportIdent.h` extensive DWORD**: ~30 occurrences in struct members and function params — all replaced mechanically.

## Post-Script Manual Steps

After running the script, verify:
1. No `DWORD` remains in domain files: `grep -rn '\bDWORD\b' code/ --include='*.cpp' --include='*.h' | grep -v Tab | grep -v gdioutput | grep -v meos.cpp`
2. `#include <cstdint>` was added to files that needed it
3. Build still compiles (MSVC and GCC)

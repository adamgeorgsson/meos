# Sleep Replacement Skill

This skill provides a procedure and script for replacing Win32 `Sleep()` calls with standard C++17 `std::this_thread::sleep_for()` in domain code (US-P0k from prd-legacy-preparation.md).

## Scope

**In scope (domain/infrastructure files):**
- `SportIdent.cpp`, `socket.cpp`, `newcompetition.cpp`, `download.cpp`, `animationdata.cpp`
- Any other non-UI `.cpp/.h` file in `code/`

**Out of scope (excluded by the script):**
- `Tab*.cpp`, `Tab*.h` — UI tab classes
- `gdioutput.*` — GUI rendering
- `meos.cpp` — composition root / Win32 WinMain
- `Table.cpp` — UI table widget
- `mysql/*`, `restbed/*` — third-party library headers

## Procedure

1.  **Audit**: Run `python .gemini/skills/sleep-replacement/fix_sleep.py --dry-run` to list all Sleep() calls that would be replaced.
2.  **Apply**: Run `python .gemini/skills/sleep-replacement/fix_sleep.py` to perform the replacements and add missing includes.
3.  **Verify**: Run with `--dry-run` again to confirm zero remaining calls in domain files.

Run from the repository root (parent of `code/`).

## Script

See [`fix_sleep.py`](fix_sleep.py) — supports `--dry-run` for audit-only mode.

## Edge Cases

- **`Sleep(0)`** — Replaced with `sleep_for(milliseconds(0))`. This yields the time slice similarly to the Win32 behavior. An alternative is `std::this_thread::yield()` but `sleep_for(0)` is a safer mechanical replacement.
- **Expression arguments** — `Sleep(300 + rand()%600)` is replaced as-is: `sleep_for(milliseconds(300 + rand()%600))`. The expression is preserved verbatim.
- **Commented-out calls** — Lines starting with `//` are skipped entirely.
- **`Sleep` inside macros** — The `mysql/my_global.h` and `mysql/my_sys.h` files define `sleep()` wrappers using `Sleep()` — these are excluded by directory filtering.

## Gotchas

- `SportIdent.cpp` has timing-sensitive serial communication loops — millisecond values must be preserved exactly (the script preserves them verbatim).
- After replacement, files need both `#include <thread>` and `#include <chrono>` — the script adds them automatically after the last existing `#include` line.
- Win32 `Sleep()` (capital S) is distinct from POSIX `sleep()` (lowercase, seconds) — the regex only matches `Sleep(`.

# Replace MessageBox/OutputDebugString Skill

Replace Win32 UI calls in domain code (US-P0n from prd-legacy-preparation.md).

## What This Script Does

**Automated** — `OutputDebugString()` → `std::cerr`:
- `OutputDebugString("literal\n")` → `std::cerr << "literal" << '\n';`
- `OutputDebugString(L"literal\n")` → `std::cerr << "literal" << '\n';`
- `OutputDebugString(expr.c_str())` → `std::cerr << narrow(expr);`
- Adds `#include <iostream>` where needed
- Skips commented-out calls

**Manual inventory** — `MessageBox()`:
- Lists all MessageBox calls in domain files with suggested replacements
- Suggestions based on button flags (MB_OK → throw, MB_YESNO → callback, etc.)

## Procedure

1. **Audit**: `python3 .claude/skills/replace_msgbox_debug/replace_msgbox_debug.py --dry-run`
2. **Inventory MessageBox**: `python3 .claude/skills/replace_msgbox_debug/replace_msgbox_debug.py --inventory`
3. **Apply OutputDebugString**: `python3 .claude/skills/replace_msgbox_debug/replace_msgbox_debug.py`
4. **Manual**: Replace MessageBox calls using inventory suggestions

## MessageBox Replacement Guide

| Context | Replacement |
|---------|------------|
| Error display (MB_OK) | `throw meosException(msg)` — caught at UI boundary |
| Warning (MB_ICONWARNING) | `std::cerr << narrow(msg) << std::endl` |
| Yes/No dialog | Callback: `std::function<bool(const wstring&)>` registered by UI |
| Hardware config (SportIdent) | Callback registered during initialization |

## Domain Files with MessageBox (5 calls)

- `SportIdent.cpp` — 6 active calls (hardware config errors/instructions)
- `oClub.cpp` — 2 calls (file processing errors)
- `oEvent.cpp` — 1 call (warning)

## Scope

Same exclusions as other skills: Tab*, gdioutput*, meos.cpp, Table*, third-party dirs.

# Normalize Path Separators Skill

Replace hardcoded backslash path separators with forward slashes in domain code (US-P0e from prd-legacy-preparation.md).

## What the Script Automates

Backslashes inside string literals that are path separators:
- `L"..\\Lists\\"` → `L"../Lists/"`
- `L"\\"` → `L"/"`
- `+ L"\\" +` → `+ L"/" +`

## What It Skips

- Escape sequences (`\n`, `\t`, `\r`, `\0`)
- SQL strings (lines containing SELECT/INSERT/UPDATE/etc.)
- MeosSQL.cpp entirely (heavy SQL quoting)
- Comment lines
- Non-path uses of backslash

## Procedure

1. **Inventory**: `python3 .claude/skills/normalize_paths/normalize_paths.py --inventory`
2. **Dry run**: `python3 .claude/skills/normalize_paths/normalize_paths.py --dry-run`
3. **Apply**: `python3 .claude/skills/normalize_paths/normalize_paths.py`
4. **Manual**: Review results — some string backslashes might not be path separators

## Remaining Manual Work After Script

The script handles string literals. These patterns still need manual refactoring:
- `path += L"\\" + filename` → `(std::filesystem::path(path) / filename).wstring()`
- Character comparisons: `if (c == '\\')` → `if (c == '/' || c == '\\')`
- `std::filesystem::path` adoption for path construction

## Key Files

- `oEvent.cpp` — backup paths, file patterns
- `meos_util.cpp` — getFiles() directory scanning
- `zip.cpp` — archive paths
- `oClub.cpp` — data file paths
- `oEventSpeaker.cpp` — speaker data paths
- `HTMLWriter.cpp` — output paths

## Gotchas

- `zip.cpp` has FILETIME/path interactions — handle carefully
- Some paths are constructed with `wcscat_s` + `L"\\"` — these need `std::filesystem::path` refactoring, not just string replacement
- `parent_path()` of `"dir"` returns `""` — use `(p / "").wstring()` to ensure trailing separator

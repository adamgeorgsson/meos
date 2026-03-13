# Include Normalization Skill

This skill provides a procedure and script for normalizing `#include` directives to match file casing on disk, which is essential for cross-platform builds (e.g., Windows to Linux).

## Procedure

1.  **Audit**: Run `python .gemini/skills/include-normalization/fix_includes.py --dry-run` to list all mismatches without changing anything.
2.  **Apply Fixes**: Run `python .gemini/skills/include-normalization/fix_includes.py` to update source files to use the correct casing.
3.  **Verify**: Re-run with `--dry-run` to confirm zero mismatches remain.

Run from the repository root (parent of `code/`).

## Script

See [`fix_includes.py`](fix_includes.py) — supports `--dry-run` for audit-only mode.

## Common Mismatches in MeOS

- `stdafx.h` -> `StdAfx.h`
- `localizer.h` -> `localizer.h` (included as `Localizer.h`)
- `meosexception.h` -> `meosexception.h` (included as `meosException.h`)
- `TabBase.h` -> `TabBase.h` (included as `tabbase.h`)

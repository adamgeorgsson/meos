# AGENTS.md — src/io/

## Overview
Cross-platform I/O layer for MeOS. No Win32 or gdioutput dependencies allowed.

## Modules

### CsvIo (US-014b)
OE/OS CSV import and export. OE format: semicolons, first line is always header
(skip unconditionally), then `fields.size() > 10` to skip partial rows.
`detectFormat()` checks col[1] for "Descr"/"Namn"/"Descr."/"Navn" → OS, else OE.

### HtmlWriter (US-014c)
Cross-platform HTML template reader and result generator.

**Template format** (`.template` files):
- First line: `@MEOS EXPORT TEMPLATE` (required)
- Second non-comment line: `tag@HumanName`
- Comment lines start with `%`
- Sections: `@HEAD` / `@OUTERPAGE` / `@INNERPAGE` / `@SEPARATOR` / `@END`
- `@USETABLE` requests table-based layout

**Placeholder substitution** — single-pass left-to-right, longest match wins:
- `@TITLE` / `@T` → HTML-encoded title
- `@CONTENTS` / `@C` → raw HTML contents
- `@DESCRIPTION` / `@D` → HTML-encoded description
- `@NUMPAGE` / `@N` → total pages
- `@TIME` → refresh ms
- `@STYLE` / `@S` → CSS block
- `@MEOSVERSION` / `@MEOS` / `@M` → "MeOS"

**DO NOT** use multi-pass replacement — `@T` would corrupt `@TITLE` if applied first.

**Testing**: `HtmlTemplate::read(std::istream&)` enables in-memory template testing
without filesystem I/O (see `tests/html_io_test.cpp`).

### IofXml (US-014a)
IOF 3.0 XML import/export. Uses the custom xml parser from `src/util/`.

## CMakeLists.txt
All new `.cpp` files must be added to `add_library(meos_io STATIC ...)`.
The library links `meos_domain` and `meos_util` (PUBLIC).

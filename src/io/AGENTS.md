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

### PdfWriter (US-014d)
Cross-platform PDF generation using libharu.

**vcpkg/CMake integration:**
- vcpkg package name: `libharu` (in vcpkg.json)
- CMake find_package: `find_package(unofficial-libharu CONFIG REQUIRED)`
- CMake target: `unofficial::libharu::hpdf`
- libharu installs into `build/vcpkg_installed/x64-linux/include/`

**Font usage:**
- Standard Type 1 fonts (no external font files required):
  - Normal: `"Helvetica"`, Bold: `"Helvetica-Bold"`, Italic: `"Helvetica-Oblique"`
- Pass `nullptr` as encoding (these are Latin-1 fonts, NOT UTF-8)
- Use `narrow()` to convert `wstring` to `std::string` before rendering

**libharu coordinate system:**
- Origin (0,0) is bottom-left; Y increases upward
- A4 portrait: width=595pt, height=842pt

**In-memory PDF generation (for testing):**
```cpp
HPDF_SaveToStream(doc);
HPDF_UINT32 size = HPDF_GetStreamSize(doc);
std::vector<unsigned char> buf(size);
HPDF_ReadFromStream(doc, buf.data(), &size);
// buf starts with "%PDF-"
```

**Text vs. graphics mode:**
- `HPDF_Page_TextOut` / `HPDF_Page_TextWidth` must be called INSIDE `BeginText`/`EndText`
- `HPDF_Page_MoveTo` / `LineTo` / `Stroke` (lines) must be called OUTSIDE text mode

**Do NOT forward declare HPDF_Doc/HPDF_Page as void\*** — always `#include <hpdf.h>` directly.

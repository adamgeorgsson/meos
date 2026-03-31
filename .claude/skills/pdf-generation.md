# PDF Generation with libharu

## vcpkg / CMake Integration

```json
// vcpkg.json
"libharu"
```

```cmake
# CMakeLists.txt
find_package(unofficial-libharu CONFIG REQUIRED)
target_link_libraries(meos_io PUBLIC unofficial::libharu::hpdf)
```

- vcpkg port name: `libharu` (NOT `unofficial-libharu`)
- find_package name: `unofficial-libharu`
- CMake target: `unofficial::libharu::hpdf`
- Installs to: `build/vcpkg_installed/x64-linux/include/hpdf.h`

## Always Include hpdf.h in Header

```cpp
// CORRECT: include directly
#include <hpdf.h>

// WRONG: do NOT forward declare as void*
typedef void* HPDF_Doc;
```

## Standard Type 1 Fonts (cross-platform, no font files)

```cpp
HPDF_Font fn = HPDF_GetFont(doc, "Helvetica",         nullptr);
HPDF_Font fb = HPDF_GetFont(doc, "Helvetica-Bold",    nullptr);
HPDF_Font fi = HPDF_GetFont(doc, "Helvetica-Oblique", nullptr);
```

- Pass `nullptr` as encoding (NOT "UTF-8")
- Use `narrow(wstring)` to convert text before rendering (Latin-1 only)

## Coordinate System

- Origin at **bottom-left**; Y increases **upward**
- A4 portrait: width=595pt, height=842pt (1pt ≈ 0.353mm)
- Typical margin: 40pt

## Text vs. Graphics Mode

```cpp
// TEXT (TextOut, TextWidth, SetFontAndSize)
HPDF_Page_BeginText(page);
  HPDF_Page_SetFontAndSize(page, font, size);
  HPDF_Page_TextOut(page, x, y, "text");
HPDF_Page_EndText(page);

// GRAPHICS (lines, rectangles — OUTSIDE text mode)
HPDF_Page_MoveTo(page, x1, y);
HPDF_Page_LineTo(page, x2, y);
HPDF_Page_Stroke(page);
```

## In-Memory Generation (for tests)

```cpp
HPDF_SaveToStream(doc);
HPDF_UINT32 size = HPDF_GetStreamSize(doc);
std::vector<unsigned char> buf(size);
HPDF_UINT32 remaining = size;
HPDF_ReadFromStream(doc, buf.data(), &remaining);
// buf[0..4] == "%PDF-"
```

## Error Handling

```cpp
static void pdfErrorHandler(HPDF_STATUS error_no, HPDF_STATUS detail_no, void*)
{
  char buf[128];
  std::snprintf(buf, sizeof(buf), "libharu error: 0x%04X detail: 0x%04X",
                (unsigned)error_no, (unsigned)detail_no);
  throw meosException(buf);
}

HPDF_Doc doc = HPDF_New(pdfErrorHandler, nullptr);
```

## Lifecycle

```cpp
HPDF_Doc doc = HPDF_New(errorHandler, nullptr);
HPDF_SetCompressionMode(doc, HPDF_COMP_ALL);
HPDF_SetInfoAttr(doc, HPDF_INFO_TITLE, "...");
HPDF_SetInfoAttr(doc, HPDF_INFO_AUTHOR, "...");
HPDF_SetInfoAttr(doc, HPDF_INFO_CREATOR, "MeOS");

HPDF_Page page = HPDF_AddPage(doc);
HPDF_Page_SetSize(page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);

// ... draw content ...

HPDF_SaveToFile(doc, "/path/to/output.pdf");
HPDF_Free(doc);
```

## Multi-Page

```cpp
float y = pageHeight - margin;
for (const auto& row : rows) {
  float lineHeight = row.fontSize * 1.4f;
  if (y - lineHeight < margin) {
    page = HPDF_AddPage(doc);
    HPDF_Page_SetSize(page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);
    y = pageHeight - margin;
  }
  // draw row at y, then y -= lineHeight;
}
```

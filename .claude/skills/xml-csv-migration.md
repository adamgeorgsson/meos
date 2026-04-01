# XML and CSV Migration Skill

## Overview
Migrate custom XML and CSV parsing logic from legacy Win32 implementations to cross-platform C++17.

## XML Parser Patterns

### In-Place Parsing
The custom `xmlparser` uses a flat buffer and modifies it in-place with null terminators to identify tags and attributes.

**Gotcha:** When scanning for attribute values, ensure the loop condition includes the character at the `end` pointer.
```cpp
// Correct
while (start <= end) {
  if (start == end || *start == '"') {
    // ...
  }
}
```

### XML Declaration Handling
Legacy `xmlparser` unconditionally skips the first tag. This fails for XML fragments or files without declarations.
**Pattern:** Only skip the first tag if it starts with `<?`.

```cpp
if (ptr[0] == '<' && ptr[1] == '?') {
  // skip declaration
} else {
  // don't skip root tag
}
```

### Entity Decoding
Use `inplaceDecodeXML` to decode standard entities in a raw buffer.
- `&amp;` -> `&`
- `&lt;` -> `<`
- `&gt;` -> `>`
- `&quot;` -> `"`
- `&#10;` -> `\n`
- `&#13;` -> `\r`
- `&apos;` -> `'`

## CSV Parser Patterns (OE Format)

### OE CSV Format
OE CSV is semicolon-delimited with one header row followed by data rows. The key column indices are:
```
OEstno=0, OEcard=1, OEid=2, OEsurname=3, OEfirstname=4, OEbirth=5, OEsex=6,
OEstart=9, OEfinish=10, OEtime=11, OEstatus=12,
OEclubno=13, OEclub=14, OEclubcity=15, OEnat=16, OEclassno=17,
OEclassshortname=18, OEclassname=19, OEbib=23,
OErent=35, OEfee=36, OEpaid=37, OEcourseno=38, OEcourse=39, OElength=40, OEpl=43
```

### Import: Header Skip + Column Heuristic
Always skip the FIRST line (header) unconditionally. Then apply `fields.size() > 10` to skip partial/empty rows:
```cpp
bool firstLine = true;
for (const auto& fields : allLines) {
  if (firstLine) { firstLine = false; continue; }  // skip header
  if (fields.size() <= 10) continue;                // column heuristic
  // ... process data row
}
```
Do NOT rely solely on the column heuristic — the header row also has > 10 columns in OE format.

### Format Detection
Detect OE vs OS from first line column[1]:
```cpp
if (col1 == "Descr" || col1 == "Namn" || col1 == "Descr." || col1 == "Navn")
  return CsvFormat::OS;
else
  return CsvFormat::OE;
```

### File Parsing
Read as binary, strip UTF-8 BOM (EF BB BF), convert via `fromUTF8()`, split on `\n` (skip `\r`):
```cpp
// Strip BOM
if (raw[0]==0xEF && raw[1]==0xBB && raw[2]==0xBF) start = 3;
wstring wcontent = fromUTF8(raw.substr(start));
```

### Time Formatting
- Export: `formatTimeHMS(int_time)` → "HH:MM:SS"
- Import: `convertAbsoluteTimeHMS(wstring, oe.getZeroTimeNum())` → internal time units

### Status Conversion
OE status: 0=OK, 1=DNS, 2=DNF, 3=MP, 4=DQ, 5=MAX

### Export Row Size
Allocate `vector<string> row(46)` — highest column index is 45 (OEfinishpunch).

## Decoupling
Always separate core parsing logic (in `util`) from domain-specific data mapping (in `io`).
- `src/util/xmlparser.h`: Generic structure parsing.
- `src/io/IofXml.h`: Mapping XML structures to `oEvent`, `oRunner`, etc.
- `src/io/CsvIo.h`: CSV import/export (OE/OS formats).

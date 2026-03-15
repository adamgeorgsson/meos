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

## CSV Parser Patterns

### Multi-line Support
Standard CSV allows newlines inside quoted fields. A simple `getline` approach is insufficient.
**Pattern:** Use a state machine or stream-based parser that tracks `inQuotes` state.

```cpp
if (inQuotes) {
  if (c == L'"') {
    if (stream.peek() == L'"') {
      stream.get(); // skip escaped quote
      field += L'"';
    } else {
      inQuotes = false;
    }
  } else {
    field += c;
  }
}
```

### Encoding Detection
MeOS files can be UTF-8 (with or without BOM) or ANSI.
**Pattern:** Check for BOM first, then attempt to decode as UTF-8. If decoding fails or results in invalid characters, fallback to ANSI (widen from raw bytes).

## Decoupling
Always separate core parsing logic (in `util`) from domain-specific data mapping (in `io`).
- `src/util/xmlparser.h`: Generic structure parsing.
- `src/io/iof30interface.h`: Mapping XML structures to `oEvent`, `oRunner`, etc.

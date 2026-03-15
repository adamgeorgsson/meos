# Skill: Migrate Domain Entity

Use this skill when migrating a domain entity (oRunner, oClass, oCourse, oClub, oCard, oFreePunch, oTeam, etc.) from `code/` to `src/domain/`.

## Recipe

Follow these steps in order for each entity:

### 1. Analyze dependencies
- Read the header and cpp file thoroughly before making changes.
- `grep -rn "ClassName" code/` to find ALL references — methods are often scattered across multiple files (especially oEvent*.cpp).
- Identify which enums, types, and forward declarations are needed.

### 2. Move files to src/domain/
- Copy header and cpp to `src/domain/`.
- Add files to the domain CMake target.

### 3. Move shared enums to domain_header.h
- Enums used by multiple entities (e.g., `SpecialPunch`, `RunnerStatus`) belong in `domain_header.h`.
- This prevents circular includes between entity headers.

### 4. Replace Win32-specific code
- `_wtoi` / `_wtof` / `_wtoi64` -> `wcstol` / `wcstod` / `wcstoll`
- `_itow_s` -> `swprintf` or `std::to_wstring`
- `sprintf_s` / `swprintf_s` -> `snprintf` / `swprintf` (always specify buffer size explicitly)
- `_stricmp` / `_wcsicmp` / `lstrcmpi` -> `compareStringIgnoreCase()` from `meos_util.h`
- `DWORD` -> `uint32_t`, `BOOL` -> `bool`, `BYTE` -> `uint8_t`, `WORD` -> `uint16_t`
- Add `#include <cstdint>` if using standard integer types.
- Leave `BOOL` unchanged inside SQL query strings — it's a database type.

### 5. Extend stubs in domain_module.cpp
- Add stubs for any oEvent methods, gdioutput methods, or Table methods that the entity calls.
- Every virtual method of stubbed classes MUST have an implementation (even empty) — GCC requires full vtable coverage.
- Use `throw std::runtime_error("stub")` for methods that should never be reached in tests.

### 6. Update domain_header.h with forward declarations
- Add forward declarations for any new types the entity references.
- Keep forward declarations alphabetically sorted.

### 7. Write unit tests
- Add tests in `tests/domain_test.cpp`.
- Use `alignas(16)` for data buffers in tests to avoid alignment issues with `wchar_t` on Linux.
- Protected member access: create a test subclass that exposes protected members if needed.
- Set locale to `"C.UTF-8"` in test setup for correct non-ASCII string handling.

### 8. Build and verify
- `cmake --build build` — fix all compiler errors.
- `cd build && ctest` — all tests must pass.
- Common errors:
  - Missing stub methods -> add to `domain_module.cpp`
  - Template linker errors -> ensure `*impl.hpp` is included in its header
  - Incomplete type in `std::vector` -> provide class definition (even minimal)

## SI Card and Punch Handling

- `SICard` and `SIPunch` are the primary structures for raw card data.
- Use `SICard::calculateHash()` to identify unique readout instances. Store hash in `oCard::readId`.
- Always clear `SICard` objects with `clear(nullptr)` to ensure all fields (especially codes set to -1) are correctly initialized.
- `oCard` belongs to an `oRunner` or `oTeam`.
- `oFreePunch` is managed by `oEvent` and indexed via `oEvent::punchIndex`.
- Use `oFreePunch::rehashPunches(oe, cardNo, newPunch)` to maintain the lookup index when punches change.
- The `Localizer` supports `#` substitution for multiple placeholders: `lang.tl(L"Base String with X, Y, Z, W#ValX#ValY#ValZ#ValW")`.

## Entity migration order (by dependency)

```
oBase, oDataContainer (foundation - no entity deps)
    -> oControl, oPunch (simplest entities)
    -> oClub (few deps)
    -> oCourse (depends on oControl)
    -> oClass (depends on oCourse)
    -> oCard, oFreePunch (depends on oPunch)
    -> oRunner (depends on oClass, oClub, oCard - largest file ~230KB)
    -> oTeam (depends on oRunner via oAbstractRunner)
    -> oEvent (aggregate root - depends on everything)
```

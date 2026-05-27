# PRD: Legacy Code Preparation for Migration

## Introduction

During the iterative migration documented in `prd-core-migration.md`, a recurring set of friction points has been identified in the legacy `code/` tree. These are **non-functional refactorings** — they preserve existing Windows/MSBuild behavior — but they eliminate problems that repeatedly cause compilation errors or latent bugs when domain code is migrated into `src/`.

By performing these refactorings directly in `code/` (under the existing MSBuild project), each migration run avoids re-patching the same issues. The work can proceed **in parallel** with the migration work and is independent from upstream functional changes.

See `plan/legacy-prep-candidates.md` for the raw inventory and rationale behind each item.

## Goals

- Remove Win32/MSVC-only constructs from domain headers and source files so the same code compiles on Linux with Clang/GCC.
- Break header coupling that forces the migration to repeatedly re-extract base classes and enums.
- Add public accessors so repositories, REST handlers, and tests can read state without `friend` declarations or layout assumptions.
- Make `oData` blob buffers portable across `sizeof(wchar_t)` differences.
- Improve thread safety in shared singletons so the REST server can serve concurrent requests safely.
- Preserve **all** existing behavior on Windows — every change must be a no-op for the legacy MSBuild build.

## User Stories

### US-P0l: Replace `std::exception("msg")` with `std::runtime_error("msg")`
**Description:** As a migration engineer, I want all message-bearing exception throws to use a standard, portable type so that domain code compiles on non-MSVC toolchains.

**Acceptance Criteria:**
- [ ] No `throw std::exception("...")` remains in domain files in `code/`.
- [ ] `meosException` (and any similar message-carrying exception class) inherits from `std::runtime_error` instead of `std::exception`.
- [ ] All existing `catch (const std::exception&)` blocks continue to catch these exceptions.
- [ ] Legacy MSBuild build of `code/` succeeds with no warnings/errors from this change.

---

### US-P0o: Make `StringCache` and `Localizer` thread-safe via `thread_local`
**Description:** As a developer, I want shared per-thread caches to use `thread_local` storage so they are both thread-safe and portable (no `GetCurrentThreadId()` + global map).

**Acceptance Criteria:**
- [ ] `StringCache::getInstance()` returns a `thread_local static StringCache` instance directly; the `GetCurrentThreadId()` lookup and global map are removed.
- [ ] `translate()` in `localizer.cpp` uses `thread_local` for its rotating-buffer state instead of plain `static`.
- [ ] No `<windows.h>`-only API remains in either implementation for thread identification.
- [ ] Legacy MSBuild build still passes.

---

### US-P0p: Extract `oAbstractRunner` to its own header
**Description:** As a migration engineer, I want the `oRunner` / `oEvent` circular include resolved at the source so each migration run doesn't have to re-do the same extraction.

**Acceptance Criteria:**
- [ ] New header `oAbstractRunner.h` contains the full `oAbstractRunner` definition and only forward-declares `oEvent`.
- [ ] `oRunner.h` and `oTeam.h` include `oAbstractRunner.h` and no longer define `oAbstractRunner` themselves.
- [ ] `oEvent.h` no longer depends on the full definition of `oRunner` to use `oAbstractRunner` pointers/references.
- [ ] Methods that need the full `oEvent` definition (e.g., `DynamicValue` methods touching `oEvent::dataRevision`) live in `.cpp` files, not in the new header.
- [ ] Legacy MSBuild build still passes.

---

### US-P0q: Move shared enums out of `oEvent.h`
**Description:** As a developer, I want to include just an enum without dragging in the entire `oEvent` header so domain headers stay loosely coupled.

**Acceptance Criteria:**
- [ ] `SpecialPunch` is defined in `oControl.h` (or a dedicated enum header) and `oEvent.h` re-exports it for backward compatibility (e.g., `using SpecialPunch = ::SpecialPunch;`).
- [ ] `RunnerStatus`, `DynamicRunnerStatus`, and `SortOrder` are defined in `oAbstractRunner.h` (or a shared enum header) and `oEvent.h` re-exports them.
- [ ] All existing code that includes `oEvent.h` still compiles unchanged.
- [ ] Legacy MSBuild build still passes.

---

### US-P0r: Add public accessors required by migration
**Description:** As a repository / REST / test author, I want public read access (and where needed, write access) to domain state without using `friend` or layout assumptions.

**Acceptance Criteria:**
- [ ] `oPunch::isUsed`, `oPunch::tIndex`, `oPunch::tMatchControlId` are public (or exposed via public accessors).
- [ ] `oClass` exposes `int getNumLegs() const { return (int)legInfo.size(); }`.
- [ ] `oAbstractRunner::tInTeam`, `tLeg`, `tStartTime` are public (or exposed via public accessors).
- [ ] `oAbstractRunner` exposes `const TempResult& getTempResult() const`.
- [ ] `oTeam` exposes a public `wstring getRunnerIdString() const` wrapper.
- [ ] `oCard` exposes `void setCardNo(int c)` if not already present.
- [ ] `oBase` (and each entity that owns an `oData` blob) exposes `const BYTE* getOData() const` and `static int getODataBlobSize()`.
- [ ] No existing code is broken by these additions (purely additive).
- [ ] Legacy MSBuild build still passes.

---

### US-P0s: Make `oData` buffers portable across `sizeof(wchar_t)`
**Description:** As a Linux build engineer, I want `oData` blob buffers to align correctly for wide-character access and to size themselves based on the platform's `wchar_t` so the same blob layout serializes on both platforms.

**Acceptance Criteria:**
- [ ] `BYTE oData[dataSize]` declarations in `oClass`, `oClub`, `oCourse`, `oControl`, `oRunner`, and `oTeam` are decorated with `alignas(sizeof(wchar_t))`.
- [ ] `dataSize` constants for these entities are expressed in terms of `sizeof(wchar_t)` (e.g., `256 * static_cast<int>(sizeof(wchar_t)) + 64`) and evaluate to the same value on Windows as today.
- [ ] On Windows the compiled struct layout and `sizeof` for each entity are unchanged.
- [ ] Legacy MSBuild build still passes.

---

### US-P0t: Header hygiene (includes, StdAfx, enum forward declarations)
**Description:** As a non-PCH build, I want each header to be self-contained so it can be consumed without depending on precompiled-header injection or transitive includes.

**Acceptance Criteria:**
- [ ] `#include "StdAfx.h"` is removed from `.hpp` files (e.g., `intkeymap.hpp`) and from any other standalone header where it appears; required standard headers are added explicitly.
- [ ] Opaque `enum KeyCommandCode;` (and any similar forward declarations) is replaced with an explicit underlying type (`enum KeyCommandCode : int;`) or with the full include.
- [ ] Domain headers/sources gain the includes they previously relied on PCH for, at minimum:
  - `oTeam.h` includes `<set>`.
  - Domain `.cpp` files that use `oDataContainer` include `oDataContainer.h` directly instead of relying on transitive inclusion through `oBase.h`.
  - `oClass.cpp` includes `xmlparser.h` and `timeconstants.hpp` directly.
- [ ] An "include-what-you-use"-style pass over domain headers is performed and additional missing includes are added where they would otherwise only resolve via PCH.
- [ ] Legacy MSBuild build still passes.

---

### US-P0u: Replace Win32 `CompareString` with standard comparison
**Description:** As a Linux build engineer, I want sort comparisons in domain code to use portable `std::wstring` comparison so domain code does not depend on Win32 NLS APIs.

**Acceptance Criteria:**
- [ ] `CompareString` calls in `GeneralResultInfo::compareResult` and `oAbstractRunner::compareClubs` are replaced with plain `wstring` comparison (`<` / `compare`).
- [ ] No `CompareString`/`CompareStringEx` calls remain in domain files in `code/`.
- [ ] Legacy MSBuild build still passes; ASCII sort order is preserved (minor differences in non-ASCII collation are acceptable).

---

### US-P0v: Misc portability and migration enablers
**Description:** As a migration engineer, I want a small set of remaining portability and completeness fixes so the legacy code stops repeatedly tripping the same migration patches.

**Acceptance Criteria:**
- [ ] `SQL_quote()` uses `toUTF8(wstring)` from `meos_util.h` instead of calling `WideCharToMultiByte` directly.
- [ ] `oListParam` exposes `set<int> selection`, `bool lockUpdate`, and a working `filterInclude()` implementation, all with sane defaults so existing call sites are unaffected.
- [ ] `permute()` in `random.h`/`random.cpp` (or wherever it lives) is implemented using `std::shuffle` with `std::mt19937` seeded from `std::random_device`; any Win32-specific RNG paths are removed.
- [ ] Legacy MSBuild build still passes.

---

## Functional Requirements

- FR-1: All changes must be made directly in `code/` so they apply to both the legacy MSBuild build and every future migration run that copies from `code/` into `src/`.
- FR-2: Every story must keep the existing MSBuild build of `code/` green on Windows (no warnings/errors introduced by the change).
- FR-3: No change may alter observable runtime behavior on Windows, including:
  - Serialized binary layout of `oData` blobs.
  - Exception type names visible in catch blocks (continue to derive from `std::exception`).
  - Sort order for ASCII strings.
- FR-4: New public accessors and new `oListParam` members must be purely additive — they must not change existing call signatures, visibility of unrelated members, or default behavior.
- FR-5: Stories may land independently in the order described below; each landed story must leave the tree in a buildable state.

### Dependency / Suggested Order

```
US-P0l  (exceptions)        — independent, mechanical
US-P0o  (thread_local)      — independent
US-P0t  (header hygiene)    — independent, do early
US-P0s  (alignas / dataSize)— independent, no-op on Windows
US-P0r  (public accessors)  — independent
US-P0u  (CompareString)     — independent
US-P0v  (SQL_quote/oListParam/permute) — SQL_quote requires `toUTF8` in meos_util
US-P0q  (move enums)        — best after US-P0t
US-P0p  (oAbstractRunner)   — last (largest structural change)
```

## Non-Goals

- No functional changes to MeOS behavior on Windows.
- No domain logic moves into `src/` as part of this PRD — that is the scope of `prd-core-migration.md`.
- No introduction of new third-party dependencies.
- No change to the MSBuild project structure, vcxproj layout, or build tooling.
- No deletion of `StdAfx.h` itself or removal of precompiled-header use; only inappropriate `#include "StdAfx.h"` from `.hpp` files is removed.
- No Linux build target, CMake target, or CI job is added by this PRD (the migration PRD owns those).
- No changes to upstream-tracked design: keep diffs minimal so syncing with `melinsoftware/meos` stays low-friction.

## Technical Considerations

- This is a fork of [`melinsoftware/meos`](https://github.com/melinsoftware/meos). Upstream may rename, move, or restructure any of the files referenced above. Acceptance criteria therefore describe the **desired end state** (e.g., "no `throw std::exception("...")` remains in domain files") rather than line numbers or specific patches.
- All changes must compile under MSVC 2022 (the legacy toolchain), which supports `thread_local`, `alignas`, and `enum X : int;` natively.
- `std::runtime_error` inherits from `std::exception`, so replacing the throw type is binary-compatible for any existing `catch (const std::exception&)`.
- `alignas(sizeof(wchar_t))` on `BYTE[]` evaluates to `alignas(2)` on Windows (already satisfied by typical struct alignment) and `alignas(4)` on Linux — no Windows layout change.
- Expressing `dataSize` constants as `N * static_cast<int>(sizeof(wchar_t)) + K` is a compile-time constant; on Windows it evaluates to the same integer as today.
- Header extractions (`oAbstractRunner.h`) and enum moves should use `using` / `typedef` re-exports in the original header to keep all existing includes working unchanged.
- Cross-reference `.claude/skills/migration/SKILL.md` for accumulated patterns observed across migration runs; do not inline knowledge that may go stale.

## Success Metrics

- Subsequent runs of the migration loop (`plan/ralph.sh`) no longer re-encounter the failure patterns enumerated in `plan/legacy-prep-candidates.md`.
- The legacy MSBuild build of `code/` remains green after each story lands.
- The number of files that the migration loop needs to patch in-flight (vs. copying as-is from `code/`) decreases visibly after each story.
- No regression reports from Windows users attributable to these refactorings.

## Open Questions

- Should `RunnerStatus`, `DynamicRunnerStatus`, and `SortOrder` live in `oAbstractRunner.h` (per the candidates doc) or in a dedicated `domain_enums.h`? Either is acceptable; pick whichever produces the smallest diff against upstream.
- Is there an existing `random.h`/`random.cpp` in the legacy tree, or does `permute()` live elsewhere? Resolve while implementing US-P0v.
- Are there additional `.hpp` files (beyond `intkeymap.hpp`) that include `StdAfx.h`? US-P0t should include a one-time audit pass.

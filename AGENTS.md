# Copilot Instructions for MeOS

MeOS (Much Easier Orienteering System) is a Windows desktop application for managing orienteering competitions. A platform modernization is underway — see the PRDs in `plan/` for the target architecture and progress.

## Project Overview

### Directory Structure

| Directory | Purpose |
|-----------|---------|
| `code/` | Legacy Windows-only codebase (MSBuild, Win32/GDI, MySQL). Has its own `AGENTS.md` with detailed architecture docs. |
| `src/` | Modern cross-platform codebase (CMake, vcpkg). Currently a stub `main.cpp` + React frontend shell in `src/ui/web/`. |
| `tests/` | Google Test suite. Currently a smoke test; per-module tests will be added as migration progresses. |
| `plan/` | PRDs and planning artifacts for the modernization effort. |
| `.github/workflows/` | CI/CD: `cpp.yml` (CMake build/test), `frontend.yml` (React lint/test/build), `build-legacy.yml` (MSBuild Windows). |

## Legacy Codebase (`code/`)

See `code/AGENTS.md` for full details. In summary:

- ~190 source files in a flat directory
- C++17, Win32/GDI UI, MySQL backend
- MSBuild with Visual Studio 2022 (`MeOS.sln`)
- Vendored dependencies (restbed, libharu, minizip, mysql, png)

### Domain Model

`oEvent` is the aggregate root owning all domain objects:

```
oBase (abstract base: ID, change tracking, data interface)
├── oRunner  ─┐
├── oTeam     ├─ both extend oAbstractRunner (shared result logic)
├── oClass
├── oClub
├── oCourse
├── oControl
├── oCard
├── oFreePunch
└── oPunch
```

## Conventions

### Naming

- Domain classes: `o` prefix (`oRunner`, `oEvent`)
- Pointer typedefs: `p` prefix for mutable (`pRunner`), `c` prefix for const (`cRunner`)
- Temporary/computed member variables: `t` prefix (`tStatus`, `tComputedTime`)
- Methods: camelCase (`getId()`, `updateChanged()`)

### Strings

Wide strings (`wstring`) are the primary string type (Swedish/internationalized UI). Narrow `string` is used for internal/config data. Conversion via `string2Wide()` in `meos_util.h`.

### Error handling

Custom exception `meosException` (with `wwhat()` for wide-string messages) and `meosCancel` for cancellation. Most functions prefer returning bool/error codes; exceptions are for critical failures.

### Other patterns

- `#pragma once` for header guards
- Heavy use of forward declarations to minimize include dependencies
- Smart pointers for ownership; raw pointers for parent/back-references
- No namespaces — flat namespace with `using std::` in `StdAfx.h`

## Modernization

Three PRDs describe the modernization effort (all in `plan/`):

| PRD | Scope | Status |
|-----|-------|--------|
| [`prd-core-migration.md`](plan/prd-core-migration.md) | Full platform migration: CMake, domain extraction, SQLite, REST API, React frontend | Build infrastructure complete (US-001/015/016/017 archived). Domain migration not started. |
| [`prd-legacy-preparation.md`](plan/prd-legacy-preparation.md) | Preparatory refactoring in `code/`: cross-platform fixes, Win32 API replacement, vcpkg migration | Not started. |
| [`prd-web-frontend.md`](plan/prd-web-frontend.md) | React + TypeScript SPA in `src/ui/web/` | Shell project scaffolded (Vite, TypeScript, Vitest, ESLint). No pages/routes yet. |

### What exists in `src/` today

- `src/main.cpp` — stub (`int main() { return 0; }`)
- `src/ui/web/` — React + TypeScript project shell (Vite, Vitest, ESLint, Tailwind CSS configured). Only has `App.tsx` and a smoke test. No routing, pages, or API client yet.
- `tests/smoke_test.cpp` — Google Test smoke test
- `CMakeLists.txt` — root build with vcpkg integration, clang-tidy option, coverage option
- `vcpkg.json` — currently only `gtest`

### Iterative Migration Approach

Domain migration is **run from scratch repeatedly** by Ralph (an autonomous agent loop in `plan/ralph.sh`). Each full attempt is analyzed, the PRD/skills/prompts are improved, and the migration is run again. The generated code is disposable — only the learnings persist across runs.

**This is a fork of [melinsoftware/meos](https://github.com/melinsoftware/meos).** We sync with upstream before each migration run. The legacy code in `code/` is therefore **not static** — do not make assumptions about exact file contents, line numbers, or function signatures. Always read and discover code structure dynamically.

**Key files for the migration loop:**

| File | Purpose |
|------|---------|
| `plan/prd-core-migration.md` | What to build (updated between runs) |
| `plan/prd.json` | Machine-readable PRD for Ralph (regenerated from the PRD, gitignored) |
| `plan/prompt.md` | Instructions for each Ralph iteration (updated between runs) |
| `plan/ralph.sh` | The agent loop runner (updated between runs) |
| `plan/progress.txt` | Learnings from the current run (discarded between runs, patterns extracted first, gitignored) |
| `.gemini/skills/migration/SKILL.md` | Accumulated migration knowledge (persists across runs) |

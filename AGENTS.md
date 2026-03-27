# MeOS

Orienteering competition management. Windows desktop app (C++17, Win32/GDI, MySQL) being migrated to cross-platform.

## Structure

- `code/` — Legacy codebase (see `code/AGENTS.md` for architecture/conventions)
- `src/` — Modern target (CMake/vcpkg):
  - `src/util/` — utilities, string conversions, platform shims (`meos_util`)
  - `src/domain/` — core domain entities: oEvent, oRunner, oClass, etc. (`meos_domain`)
  - `src/db/` — SQLite abstraction + migrations (`meos_db`)
  - `src/io/` — file I/O, IOF XML, CSV, HTML, PDF (`meos_io`)
  - `src/net/` — HTTP server + REST API (`meos_net`)
  - `src/app/` — application wiring + entry point (`meos_app`)
  - `src/ui/web/` — React SPA frontend
  - `src/main.cpp` — executable entry point (links `meos_app`)
- `tests/` — Google Test (use `meos_add_test` macro from `tests/CMakeLists.txt`)
- `plan/` — PRDs and migration planning
- `.github/workflows/` — CI: `cpp.yml`, `frontend.yml`, `build-legacy.yml`

## Build

```bash
VCPKG_ROOT=/home/adam.georgsson@fnox.it/vcpkg cmake --preset default
cmake --build build
cd build && ctest --output-on-failure
```

## Module Dependencies (no circular)

```
util  ←  domain  ←  db
                 ←  io
                 ←  net (also ← db)
                 ←  app (← all)
```

Each module's include directory is exported PUBLIC so bare `#include "header.h"` works.

## Modernization

PRDs in `plan/`:

- `prd-core-migration.md` — Full migration (CMake, domain extraction, SQLite, REST, React). Build infra done, domain not started.
- `prd-legacy-preparation.md` — Preparatory refactoring in `code/`. Not started.
- `prd-web-frontend.md` — React+TS SPA. Shell scaffolded, no pages yet.

### Migration loop

Domain migration runs from scratch repeatedly via Ralph (`plan/ralph.sh`). Generated code is disposable — only learnings persist. **Fork of [melinsoftware/meos](https://github.com/melinsoftware/meos)** — legacy code changes with upstream syncs. Always discover code structure dynamically.

Key files: `plan/prd-core-migration.md` (what to build), `plan/prompt.md` (iteration instructions), `plan/ralph.sh` (runner), `plan/progress.txt` (current-run learnings, gitignored), `.claude/skills/migration/SKILL.md` (accumulated knowledge).

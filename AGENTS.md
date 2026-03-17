# MeOS

Orienteering competition management. Windows desktop app (C++17, Win32/GDI, MySQL) being migrated to cross-platform.

## Structure

- `code/` — Legacy codebase (see `code/AGENTS.md` for architecture/conventions)
- `src/` — Modern target (CMake/vcpkg). Stub `main.cpp` + React shell in `src/ui/web/`
- `tests/` — Google Test
- `plan/` — PRDs and migration planning
- `.github/workflows/` — CI: `cpp.yml`, `frontend.yml`, `build-legacy.yml`

## Modernization

PRDs in `plan/`:

- `prd-core-migration.md` — Full migration (CMake, domain extraction, SQLite, REST, React). Build infra done, domain not started.
- `prd-legacy-preparation.md` — Preparatory refactoring in `code/`. Not started.
- `prd-web-frontend.md` — React+TS SPA. Shell scaffolded, no pages yet.

### Migration loop

Domain migration runs from scratch repeatedly via Ralph (`plan/ralph.sh`). Generated code is disposable — only learnings persist. **Fork of [melinsoftware/meos](https://github.com/melinsoftware/meos)** — legacy code changes with upstream syncs. Always discover code structure dynamically.

Key files: `plan/prd-core-migration.md` (what to build), `plan/prompt.md` (iteration instructions), `plan/ralph.sh` (runner), `plan/progress.txt` (current-run learnings, gitignored), `.claude/skills/migration/SKILL.md` (accumulated knowledge).

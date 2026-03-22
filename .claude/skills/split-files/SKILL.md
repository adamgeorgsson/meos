---
description: Split large C++ files in code/ into smaller files at pre-computed boundaries
user_invocable: true
---

# Split Large Files Skill

Splits the 5 largest files in `code/` into smaller files (~2000-3000 lines each) using pre-computed line boundaries.

## Tool

```bash
python3 .claude/skills/split-files/split_file.py <source.cpp> <split_spec> [--dry-run]
```

The script extracts line ranges into new files, copies the copyright header + all `#include` directives to each new file, removes extracted lines from the source, and adds new files to `MeOS.vcxproj`.

**Always run with `--dry-run` first**, then without.

## After running the script

1. **Verify includes**: Each new file gets ALL includes from the original. Remove unused ones if desired (not required — extra includes only slow compilation slightly).
2. **Move file-static helpers**: The script moves lines mechanically. If a file-static function or anonymous-namespace helper ended up in the wrong file, move it to the file that calls it.
3. **Move file-level variables/externs**: Same — `extern` declarations and file-static variables should be in the file that uses them, or duplicated.
4. **Build and fix**: Attempt a build. Fix any missing includes or symbol errors.

## Pre-computed split plans

### US-P0g1: oEvent.cpp (7405 lines → 3 files)

Existing splits already extracted: `oEventDraw.cpp` (3194), `oEventResult.cpp` (1349), `oEventSQL.cpp` (939), `oEventSpeaker.cpp` (2539).

```bash
python3 .claude/skills/split-files/split_file.py code/oEvent.cpp \
    "2843-5318:oEventAdmin.cpp,5319-7405:oEventConfig.cpp" --dry-run
```

| File | Lines | Content |
|------|-------|---------|
| `oEvent.cpp` (keep) | 1–2842 (~2842) | Init, constructors, I/O, serialization, file/DB ops, runner DB, add course/runner/team, ID allocation, time formatting |
| `oEventAdmin.cpp` (new) | 2843–5318 (~2475) | Remove entities, usage checks, list generation (vacancy/forest/minute), competition enum, backups, clear/new, recalculation, bib assignment |
| `oEventConfig.cpp` (new) | 5319–7405 (~2086) | Properties, status formatting, test generation, fees, UI updates, sanity check, currency, multi-stage, flags, start groups, extra fields, card system |

**File-static helpers to verify after split:**
- `getNewFileName()` (anonymous namespace, ~line 1125) — stays in oEvent.cpp (used by `open()`)
- `encodeExtra()`/`decodeExtra()` (anonymous namespace, ~line 7201) — should be in oEventConfig.cpp
- `BackupInfo::operator<` (~line 4042) — should be in oEventAdmin.cpp
- `extern Image image;` (line 71) — may be needed in oEventConfig.cpp too (check for `image.` usage)

---

### US-P0g2: oRunner.cpp (7808 lines → 3 files)

```bash
python3 .claude/skills/split-files/split_file.py code/oRunner.cpp \
    "2526-5055:oRunnerData.cpp,5056-7808:oRunnerResult.cpp" --dry-run
```

| File | Lines | Content |
|------|-------|---------|
| `oRunner.cpp` (keep) | 1–2525 (~2525) | Static helpers, constructors, I/O (XML), age/fees, class/course, time display, card handling, evaluateCard (1153–1748), time storage, sorting |
| `oRunnerData.cpp` (new) | 2526–5055 (~2529) | Club, start number, placement, name/personal data, status, data buffers, multi-runner/team, table UI, split times, speaker info, printSplits (first overload) |
| `oRunnerResult.cpp` (new) | 5056–7808 (~2752) | printSplits (list overload), display/DB ops, bib, name matching, split analysis, leg statistics, missed times, identification, removal, input data, TempResult, flags, ranking, payment, name formatting, rogaining |

**File-static helpers to verify after split:**
- `RunnerStatusOrderMap` (line 53) — stays in oRunner.cpp (used by `operator<` sorting)
- `getEncodedBib()` (~line 3508) — should be in oRunnerData.cpp

---

### US-P0g3: oClass.cpp (5684 lines → 2 files)

```bash
python3 .claude/skills/split-files/split_file.py code/oClass.cpp \
    "2836-5684:oClassConfig.cpp" --dry-run
```

| File | Lines | Content |
|------|-------|---------|
| `oClass.cpp` (keep) | 1–2835 (~2835) | Construction, XML I/O, course/stage management, leg/start config, parallel/forking basics, course pool, leader times, class splitting, entry properties |
| `oClassConfig.cpp` (new) | 2836–5684 (~2848) | Types/fees, event fill methods, table, results/splits, validation/caching, forking logic (checkForking, autoForking), bib/seeding, drawing config, qualification finals, misc config |

**File-static helpers to verify after split:**
- `clsSortFunction()` (~line 714) — stays in oClass.cpp
- `checkMissing()` (~line 4027) — should be in oClassConfig.cpp
- `maximizeSpread()` (~line 4246) — should be in oClassConfig.cpp
- Note: `#define DODECLARETYPESYMBOLS` (line 28) — keep in oClass.cpp only, check if oClassConfig.cpp needs it

---

### US-P0g4: oListInfo.cpp (5910 lines → 2 files)

```bash
python3 .claude/skills/split-files/split_file.py code/oListInfo.cpp \
    "2884-5910:oListInfoGen.cpp" --dry-run
```

| File | Lines | Content |
|------|-------|---------|
| `oListInfo.cpp` (keep) | 1–2883 (~2883) | Constructors, PrintPostInfo struct, config methods, getMaxCharWidth, format wrappers, formatRogainingStringAux, formatPunchStringAux, formatSpecialStringAux, formatListStringAux (the 1616-line switch) |
| `oListInfoGen.cpp` (new) | 2884–5910 (~3026) | formatPrintPost, calculatePrintPostKey, listGeneratePunches, generateList, filterRunner, generateListInternal (702 lines), list type config (getListTypes 342 lines), generateListInfoAux (952 lines), setupLinks, shrinkSize, leg params, serialization |

**Things to verify after split:**
- `PrintPostInfo` struct (lines 40–51) — stays in oListInfo.cpp, but oListInfoGen.cpp uses it too. Either duplicate the struct definition or move it to `oListInfo.h`
- `extern int openRunnerTeamCB(...)` (~line 4517) — should be in oListInfoGen.cpp

---

### US-P0g5: gdioutput.cpp (8174 lines → 3 files)

```bash
python3 .claude/skills/split-files/split_file.py code/gdioutput.cpp \
    "2708-5533:gdioutputEvent.cpp,5534-8174:gdioutputUI.cpp" --dry-run
```

| File | Lines | Content |
|------|-------|---------|
| `gdioutput.cpp` (keep) | 1–2706 (~2706) | Init, lifecycle, fonts, rendering/drawing, timers, text/string widgets, buttons, checkboxes, inputs, listbox/combo, message event processing |
| `gdioutputEvent.cpp` (new) | 2708–5533 (~2825) | ProcessMsgWrp (message routing), keyboard/focus control, page management, widget data access, layout, dialogs/alerts, refresh/update, text rendering (RenderString), info boxes, timeout checking |
| `gdioutputUI.cpp` (new) | 5534–8174 (~2640) | Widget removal/hiding, state restoration, rectangles/maps, file dialogs, tooltips, tables, clipboard, UI settings, font management (GDIImplFontSet), string encoding (narrow/widen/UTF8), test mode (db* methods), window utilities |

**File-static helpers to verify after split:**
- Debug vars `counterRender` etc (lines 65–71) — stay in gdioutput.cpp
- `extern Image image;` (line 61) — may be needed in gdioutputUI.cpp
- `enumMonitors` anonymous namespace (~line 8058) — should be in gdioutputUI.cpp
- `extern int defaultCodePage;` — used by encoding functions in gdioutputUI.cpp, add to that file

## Known pitfalls

- **File-local helpers not duplicated in new files**: When splitting, `static` functions and anonymous-namespace (file-local) functions (e.g., `getNewFileName`, `findNextControl`, `gotoNextLine`, `addMissingControl` in `oRunner.cpp`) may be called from code that moves to a new file. The script only copies `#include` directives, not `static` or anonymous-namespace definitions. After splitting, grep the new file for calls to any `static` or anonymous-namespace functions defined in the original file, and move or duplicate them into the new file. **If the same static function is called from BOTH the original and the new file** (e.g., `generateNBestHead`, `getResultTitle`, `getControlName` in `oListInfo.cpp`), create a shared internal header (e.g., `oListInfoInternal.h`) containing those functions as `static` inline definitions, and include it from both `.cpp` files — do NOT leave copies in only one file.
- **File-local class static methods used cross-file**: Classes defined inside a `.cpp` file (not in a header) may have `static` methods called by code that moves to the new file (e.g., `ClassSplit::evaluateResult` in `oClass.cpp` was called from code moved to `oClassConfig.cpp`). After splitting, grep the new file for `ClassName::` references to any class defined only in the original `.cpp`. Fix by extracting the static methods as free functions declared in the shared header.
- **Header-guarded symbol definitions duplicated across split files**: Some headers use `#ifdef GUARD` / `#define` patterns to conditionally define symbols (e.g., `DODECLARETYPESYMBOLS` in `oClass.h` gates `StartTypeNames`/`LegTypeNames` array definitions). When splitting a `.cpp` that defines such a guard macro, the new file must NOT copy the `#define` unless it actually uses the guarded symbols — otherwise both `.obj` files will contain the definitions, causing LNK2005 multiply-defined-symbol errors. After splitting, check whether the new file uses any symbols gated by `#ifdef` macros defined before includes, and remove the `#define` if not needed.

## Execution order

These splits are independent and can be done in any order. Recommended: do one at a time, build, fix, commit, then proceed to the next.

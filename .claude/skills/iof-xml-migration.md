# IOF 3.0 XML Migration Skill

## Overview
This skill provides patterns for migrating legacy MeOS IOF 3.0 XML import/export logic to the modernized cross-platform C++ architecture.

The modern implementation lives in `src/io/IofXml.h` and `src/io/IofXml.cpp`. It replaces the legacy `code/iof30interface.h/cpp` with a clean, GUI-free, platform-independent class.

## Critical: MeOS Time Units

All MeOS domain times are stored in **tenths of seconds** (10 units per second):
```cpp
#include "timeconstants.hpp"
// timeUnitsPerSecond = 10
// timeConstHour   = 3600 * 10 = 36000
// timeConstMinute = 60   * 10 = 600
// timeConstSecond = 10
```
- `convertAbsoluteTimeHMS("10:00:00", -1)` returns **360000** (not 36000)
- `getZeroTimeNum()`, `getStartTime()`, `getFinishTime()`, `getRunningTime()` all return time units
- For IOF `<Time>` (running seconds): `xml.write("Time", rt / timeConstSecond)`
- For abs time formatting: `h = units / timeConstHour`, `m = (units % timeConstHour) / timeConstMinute`, `s = (units % timeConstMinute) / timeConstSecond`

## Key Patterns

### 1. XML Parsing with `xmlparser`
MeOS uses a custom `xmlparser` and `xmlobject` system in `src/util/`.
- Always check if the root object is valid: `if (!xo) return;`
- Use `getObjectString`, `getObjectInt`, `getObjectBool` for simple values.
- Use `getObjects(tagName, list)` to iterate over child elements.
- The `xmlparser` automatically skips the `<?xml ... ?>` declaration if it's the first tag.
- `startTag(tag, vector<wstring>)` takes flat key-value pairs: `{L"iofVersion", L"3.0", L"status", L"Complete"}`

### 2. ISO8601 Date/Time Handling
IOF 3.0 uses ISO8601 format (e.g., `2026-03-11T10:00:00Z`).
- Strip timezone: scan for `+`, `-`, `Z` at position >= 6 in the time part, truncate there.
- Then call `convertAbsoluteTimeISO(wstring)` which returns time units.
- Export datetime: format as `YYYY-MM-DDTHH:MM:SS` using time unit decomposition.

### 3. IOF 3.0 Status Strings
Convert between `RunnerStatus` and IOF strings:
```
StatusOK         → "OK"
StatusDNF        → "DidNotFinish"
StatusMP         → "MissingPunch"
StatusDQ         → "Disqualified"
StatusMAX        → "OverTime"
StatusDNS        → "DidNotStart"
StatusCANCEL     → "Cancelled"
StatusNotCompeting / StatusOutOfCompetition → "NotCompeting"
StatusNoTiming   → "OK"
StatusUnknown    → "Active"
```

### 4. Decoupling GUI
- Remove all `gdioutput` arguments — use success/failure counters instead.
- Strip `lang.tl()` calls; error messages should be silent or narrow string IDs.
- The legacy `IOF30Interface` (5000 lines) cannot be migrated directly — it's too coupled. Write `IofXmlInterface` from scratch as a clean replacement.

### 5. Course and Control Import
- Skip S*/F* controls (start/finish) — they have special prefix chars.
- Use `oe.getControl(code, create=true, includeVirtual=false)` to allocate controls.
- Use `oCourse::importControls(semicolonSeparatedIds, true, false)` to set control sequence.
- Use `oe.getFreeCourseId()` + `oe.addCourse(name, len, id)` for course creation.

### 6. Runner Import (EntryList)
```cpp
oRunner r(&oe);
r.setName(fullName, false);
r.setClubId(clubId);         // before addRunner
r.setClassId(classId, false); // before addRunner
pRunner pr = oe.addRunner(r);
```
Set club/class BEFORE calling addRunner — the returned pointer picks up those values.

### 7. Person Name Convention
IOF 3.0 uses `Family` + `Given` in nested `Person/Name` element.
MeOS stores a single name field. Convention: `"Given Family"` (given first, family last).
On export: split at last space → given = everything before, family = last word.

# IOF 3.0 XML Migration Skill

## Overview
This skill provides patterns for migrating legacy MeOS IOF 3.0 XML import/export logic to the modernized cross-platform C++ architecture.

## Key Patterns

### 1. XML Parsing with `xmlparser`
MeOS uses a custom `xmlparser` and `xmlobject` system in `src/util/`.
- Always check if the root object is valid: `if (!xo) return;`
- Use `getObjectString`, `getObjectInt`, `getObjectBool` for simple values.
- Use `getObjects(tagName, list)` to iterate over child elements.
- The `xmlparser` automatically skips the `<?xml ... ?>` declaration if it's the first tag.

### 2. ISO8601 Date/Time Handling
IOF 3.0 uses ISO8601 format (e.g., `2026-03-11T10:00:00Z`).
- Use `IOF30Interface::getLocalDateTime` to split the string into date and time parts.
- Stripping timezones (Z, +02:00) is necessary before calling `MeOSUtil::convertAbsoluteTimeISO` if you want local time compatibility.
- ZeroTime in `oEvent` is stored as seconds from midnight.

### 3. Decoupling GUI
Legacy code is heavily coupled with `gdioutput` and `Localizer`.
- Remove all `gdioutput` arguments from import/export methods.
- Replace `gdi.addString(...)` with logging or silent success/failure counters.
- Strip `lang.tl()` calls in the IO layer; error messages should be narrow strings or IDs that the UI can localize if needed.

### 4. Course and Control Import
- Controls must be added to `oEvent` before they can be assigned to a course.
- Use `oCourse::importControls(commaSeparatedIds, setChanged, updateLegLengths)` to set the control sequence.
- Ensure `oClass::addStageCourse` is used for class-course assignments if they were not migrated in the base `oClass` logic.

### 5. ID Management
- IOF IDs should be mapped to domain entity IDs. 
- In MeOS, the ID from XML is often used directly as the internal ID if possible, or stored as an external identifier.
- Use `oe.addRunner(id, name)`, `oe.addClass(id, name)`, etc., which handle ID collisions or updates.

## Robust String Building
When building CSV or XML fragments manually (if not using `xmlparser` for writing):
- Use `MeOSUtil::itos()` for integer to string conversion.
- Use `MeOSUtil::unsplit()` to join vectors into delimited strings.
- Prefer `xmlparser::startTag` and `xmlparser::write` for structured XML output to ensure proper escaping.

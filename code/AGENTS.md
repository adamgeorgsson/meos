# Copilot Instructions for MeOS (Legacy Codebase)

MeOS (Much Easier Orienteering System) is a Windows desktop application for managing orienteering competitions. It is built with C++17, Win32/GDI, and MSBuild.

## Build

MSBuild with Visual Studio 2022 (MSVC v143). Open `MeOS.sln` in Visual Studio.

Platforms: Win32 (x86) and x64. Precompiled header: `StdAfx.h`.

## Architecture

All source files live in this flat directory. Headers are included by bare filename (e.g., `#include "oBase.h"`).

**Important:** For cross-platform compatibility (Linux/macOS), all `#include` directives MUST use the exact casing of the filename on disk. For example, use `#include "StdAfx.h"` (not `stdafx.h`) and `#include "meosexception.h"` (not `meosException.h`).

### Domain model

`oEvent` is the aggregate root — it owns collections of all domain objects. Domain entity classes use the `o` prefix:

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

Key relationships: `oRunner`/`oTeam` → `oCard` (punch records), `oTeam` → `oRunner` (members), `oClass` → `oCourse`.

### UI layer

Tab-based GUI built on `gdioutput` (custom Win32/GDI wrapper). Each feature area is a `TabBase` subclass (`TabRunner`, `TabClass`, `TabCompetition`, etc.) — one tab per domain entity plus specialized tabs for results, speaker, automation.

### Persistence & integration

- **Database:** `MeosSQL` (ORM-like layer) → `mysqlwrapper` → MySQL
- **REST API:** `RestService`/`RestServer` using the restbed library (`restbed/`)
- **Hardware:** `SportIdent` for RF card reader protocol (Windows serial port APIs)
- **Results:** `GeneralResult` (strategy pattern for pluggable scoring algorithms), `metalist` for output formatting
- **PDF:** libharu (`libharu/`)
- **Localization:** `.lng` files (key-value format, Swedish primary)

### Vendored dependencies

Libraries are vendored directly in subdirectories: `restbed/`, `libharu/`, `minizip/`, `mysql/`, `png/`, `sound/`. Platform-specific DLLs and libs in `dll/`, `dll64/`, `dll_debug/`, `lib/`, `lib64/`, `lib_db/`, `lib64_db/`.

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

### Data container pattern

`oDataContainer` provides metadata-driven field definitions (`oDataInfo`). Access via `oDataInterface` (mutable) / `oDataConstInterface` (read-only), which auto-track changes.

### Other patterns

- `#pragma once` for header guards
- Heavy use of forward declarations to minimize include dependencies
- Smart pointers (`shared_ptr`, `unique_ptr`) for ownership; raw pointers for parent/back-references
- No namespaces — flat namespace with `using std::` in `StdAfx.h`
- Disabled warnings: 4267, 4244, 4018 (integer conversion/truncation)

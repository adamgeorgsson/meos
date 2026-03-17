# MeOS Legacy Codebase

C++17, Win32/GDI, MSBuild (VS 2022, MSVC v143). Flat directory, bare `#include` filenames. PCH: `StdAfx.h`.

## Domain Model

`oEvent` is the aggregate root owning all domain objects (`oBase` → `oRunner`, `oTeam`, `oClass`, `oClub`, `oCourse`, `oControl`, `oCard`, `oFreePunch`, `oPunch`). `oRunner`/`oTeam` extend `oAbstractRunner` (shared result logic). Key links: Runner/Team→Card (punches), Team→Runner (members), Class→Course.

## Architecture

- **UI:** Tab-based (`TabBase` subclasses) on `gdioutput` (Win32/GDI wrapper)
- **DB:** `MeosSQL` → `mysqlwrapper` → MySQL
- **REST:** `RestService`/`RestServer` via vendored restbed
- **Hardware:** `SportIdent` (serial port card readers)
- **Results:** `GeneralResult` (strategy pattern), `metalist` (formatting)
- **PDF:** libharu. **Localization:** `.lng` files (Swedish primary)
- **Vendored libs:** `restbed/`, `libharu/`, `minizip/`, `mysql/`, `png/`, `sound/`

## Conventions

- **Naming:** `o` prefix (domain), `p`/`c` prefix (mutable/const ptrs), `t` prefix (computed members), camelCase methods
- **Strings:** `wstring` primary (i18n), `string` for internal. Convert via `string2Wide()` in `meos_util.h`
- **Errors:** `meosException` (`wwhat()` for wide msgs), `meosCancel` for cancellation. Prefer bool/error codes; exceptions for critical failures.
- **Data containers:** `oDataContainer`/`oDataInfo` for metadata-driven fields; `oDataInterface`/`oDataConstInterface` for access with change tracking
- **Other:** `#pragma once`, forward declarations, smart ptrs for ownership / raw ptrs for back-refs, no namespaces (flat with `using std::` in `StdAfx.h`)

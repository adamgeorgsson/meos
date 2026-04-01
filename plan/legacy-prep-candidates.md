# Kandidater att flytta från prd-core-migration till prd-legacy-preparation

Under migreringen (dokumenterad i `prd-core-migration.md`) har ett antal problem upptäckts som egentligen är **förberedande refaktoreringar i legacy-koden**. De förändrar ingen funktionalitet men eliminerar friktionspunkter som upprepade gånger orsakat kompileringsfel eller buggar vid migration till `src/`.

Allt nedan opererar på `code/` och bevarar befintligt beteende.

---

## 1. `std::exception("msg")` → `std::runtime_error("msg")`

**Källa:** prd-core-migration rad 114, 247, 854

Konstruktorn `std::exception(const char*)` är MSVC-specifik. Alla `throw std::exception("...")` i domänkod bör bytas till `throw std::runtime_error("...")`, och klasser som ärver `std::exception` med meddelande (t.ex. `meosException`) bör ärva `std::runtime_error` istället.

**Omfång:** `meosException`-klassen + alla `throw std::exception(...)` i domänfiler.
**Risk:** Mycket låg — `std::runtime_error` ärver `std::exception` så alla catch-block fungerar fortfarande.

---

## 2. `StringCache` — byt `GetCurrentThreadId()` mot `thread_local`

**Källa:** prd-core-migration rad 116

`StringCache::getInstance()` använder `GetCurrentThreadId()` med en global map — varken trådsäkert eller portabelt. Ändra till `thread_local static StringCache instance;` som returneras direkt. Samma beteende, eliminerar Win32-beroende och fixar en latent trådsäkerhetsbug.

**Omfång:** `meos_util.h/cpp` (StringCache-klassen).
**Risk:** Låg — `thread_local` stöds av MSVC 2022 och uppåt.

---

## 3. Extrahera `oAbstractRunner` till egen header

**Källa:** prd-core-migration rad 94–96, 335

`oRunner.h` och `oEvent.h` har cirkulärt beroende. Lösningen som upprepade gånger behövts under migration är att extrahera `oAbstractRunner` (basklass för `oRunner` och `oTeam`) till `oAbstractRunner.h` som bara forward-deklarerar `oEvent`. Denna refaktorering ändrar ingen funktionalitet men bryter det cirkulära beroendet redan i legacy.

**Omfång:** Ny fil `oAbstractRunner.h`, anpassning av `oRunner.h`, `oTeam.h`, `oEvent.h`.
**Risk:** Medel — kräver att inkluderingsordningen valideras via bygge, men beteendet ändras inte.

---

## 4. Flytta enums/typer ur `oEvent.h`

**Källa:** prd-core-migration rad 95, 189

`RunnerStatus`, `DynamicRunnerStatus`, `SortOrder`, `isPossibleResultStatus`, `SpecialPunch` m.fl. lever alla i `oEvent.h`, vilket tvingar alla som behöver en enum att dra in hela oEvent-headern. Flytta till `domain_enums.h` (eller respektive entitets header):

- `SpecialPunch` → `oControl.h` eller en gemensam enum-header
- `RunnerStatus`, `DynamicRunnerStatus`, `SortOrder` → `oAbstractRunner.h` (se punkt 3)

**Omfång:** Extraktion + using/typedef i `oEvent.h` för bakåtkompatibilitet.
**Risk:** Låg med forwarding.

---

## 5. Gör skyddade fält/metoder publika där migration kräver det

**Källa:** prd-core-migration rad 91, 97, 352, 383, 535–539, 560–561, 696–697

Flera fält och metoder är `protected` men behöver nås utifrån av repositories, tester och korskopplad logik. Att lägga till publika accessors i legacy-koden ändrar inte beteendet men gör migrationen smidigare:

| Klass | Fält/metod | Accessor att lägga till |
|-------|-----------|------------------------|
| `oPunch` | `isUsed`, `tIndex`, `tMatchControlId` | Gör publika (används av `evaluateCard` i oRunner) |
| `oClass` | `legInfo` | `int getNumLegs() const { return (int)legInfo.size(); }` |
| `oAbstractRunner` | `tInTeam`, `tLeg`, `tStartTime` | Gör publika |
| `oAbstractRunner` | `tmpResult` | `const TempResult& getTempResult() const` |
| `oTeam` | `getRunners()` | `wstring getRunnerIdString() const` (publik wrapper) |
| `oCard` | `cardNo` | `void setCardNo(int c)` om saknas |
| `oBase` / entiteter | `oData`-buffert | `const BYTE* getOData() const` + `static int getODataBlobSize()` |

**Risk:** Ingen — nya publika metoder bryter ingen existerande kod.

---

## 6. `alignas(sizeof(wchar_t))` på `oData`-buffertar

**Källa:** prd-core-migration rad 263

På Linux (wchar_t=4) kraschar `wcslen` med felaktiga resultat om `oData`-bufferten inte är korrekt alignerad. Lägga till `alignas(sizeof(wchar_t))` på `BYTE oData[dataSize]` i `oClass`, `oClub`, `oCourse`, `oControl`, `oRunner`, `oTeam`. På Windows (wchar_t=2) är detta en no-op.

**Omfång:** 6 header-filer, en rad per fil.
**Risk:** Ingen på Windows — `alignas(2)` ändrar inte layout för `BYTE`-arrays som redan har alignment ≥ 2.

---

## 7. `oData`-buffertstorlekar baserade på `sizeof(wchar_t)`

**Källa:** prd-core-migration rad 264, 223

Hardkodade bufferstorlekar (t.ex. `oClass::dataSize = 576`) antog Windows' 2-byte wchar_t. Ändra till `256 * static_cast<int>(sizeof(wchar_t)) + 64` etc. På Windows evalueras detta till exakt samma värde som innan.

**Omfång:** `dataSize`-konstanter i `oClass.h`, `oClub.h`, `oCourse.h`, `oControl.h`, `oRunner.h`.
**Risk:** Ingen på Windows — kompileringstidskonstant som evalueras till samma värde.

---

## 8. Ta bort `#include "StdAfx.h"` från standalone-headers

**Källa:** prd-core-migration rad 103, 174

`intkeymap.hpp` och potentiellt andra headers inkluderar `StdAfx.h` i onödan. Ta bort inkluderingen och lägg till de specifika standardheaders som behövs (om några). MSVC:s precompiled headers injicerar `StdAfx.h` automatiskt i `.cpp`-filer ändå — det behövs inte i `.hpp`-filer.

**Omfång:** `intkeymap.hpp` + audit av andra `.hpp`-filer.
**Risk:** Låg — precompiled headers gör att `StdAfx.h` redan injiceras automatiskt.

---

## 9. Ogiltiga enum forward-deklarationer

**Källa:** prd-core-migration rad 172

`enum KeyCommandCode;` (opaque forward declaration utan underliggande typ) är ogiltig C++ — MSVC accepterar det som en extension. Byt till `enum KeyCommandCode : int;` eller ta bort och inkludera rätt header.

**Omfång:** `TableType.h` och potentiellt andra headers.
**Risk:** Ingen — MSVC accepterar båda former.

---

## 10. Saknade `#include` i headers (implicita beroenden via PCH)

**Källa:** prd-core-migration rad 365, 119, 267

Många headers fungerar bara för att `StdAfx.h` redan drar in standardbiblioteket. Vid migration utan PCH saknas includes:

- `oTeam.h`: saknar `#include <set>`
- Domän-`.cpp`-filer: saknar `#include "oDataContainer.h"` (förlitar sig på transitiv inkludering via `oBase.h`)
- `oClass.cpp`: saknar `#include "xmlparser.h"` och `#include "timeconstants.hpp"`

**Omfång:** Audit alla domänheaders med ett "include-what-you-use"-pass.
**Risk:** Ingen — extra includes ändrar inte beteende.

---

## 11. `CompareString` (Win32) → standard jämförelse

**Källa:** prd-core-migration rad 371, 437

`CompareString` (Win32 API) används i `GeneralResultInfo::compareResult` och `oAbstractRunner::compareClubs`. Kan ersättas med enkel `wstring <`-jämförelse.

Notera: detta liknar US-P0c (Win32-strängfunktioner) men `CompareString` nämns inte explicit där — riskerar att missas.

**Omfång:** 2–3 anrop i domänkod.
**Risk:** Mycket låg — sorteringsordning kan skilja marginellt för icke-ASCII men är tillräckligt korrekt.

---

## 12. `localizer.cpp` trådsäkerhet

**Källa:** prd-core-migration rad 855

`translate()` använder `static`-buffert (`static int i; static wstring value[bsize]`) som inte är trådsäker. Byt till `thread_local` — samma mönster som StringCache (punkt 2). Krävs för att REST-servern ska fungera korrekt med multipla trådar.

**Omfång:** `localizer.cpp`, ~3 rader.
**Risk:** Ingen — `thread_local` stöds av MSVC 2022.

---

## 13. `SQL_quote` använder `WideCharToMultiByte`

**Källa:** prd-core-migration rad 179

`SQL_quote()` anropar `WideCharToMultiByte` direkt. Kan ersättas med `toUTF8()` från `meos_util.h` — semantiskt identiskt, redan portabel. Förutsätter att US-P0b (extrahera utilities) är klar.

**Omfång:** 1 funktion i domänkod.
**Risk:** Ingen — `toUTF8` producerar samma UTF-8-output.
**Beroende:** US-P0b (meos_util måste ha toUTF8).

---

## 14. `oListParam` saknade medlemmar

**Källa:** prd-core-migration rad 78, 947

`oListParam` saknar `set<int> selection` (klassfilter), `bool lockUpdate`, och en fullständig `filterInclude()`-implementation. Dessa behövs av HTML-generering och list-rendering. Att lägga till dem med default-värden i legacy ändrar inget beteende men förhindrar att migrationen behöver patcha dem.

**Omfång:** `oListInfo.h/cpp`.
**Risk:** Ingen — nya medlemmar med default-initialisering.

---

## 15. `permute()` → `std::shuffle`

**Källa:** prd-core-migration rad 857

`random.h`'s `permute()`-funktion kan använda Win32-specifik randomisering. Byt till `std::shuffle` med `std::random_device` + `std::mt19937`.

**Omfång:** `random.h/cpp` (om det finns).
**Risk:** Låg — randomisering är per definition icke-deterministisk, så byte av generator ändrar inte "korrekthet".

---

## Sammanfattning: föreslagna nya user stories

| ID | Titel | Relaterat |
|----|-------|-----------|
| US-P0l | `std::exception` → `std::runtime_error` | Punkt 1 |
| US-P0o | `StringCache` + `Localizer` → `thread_local` | Punkt 2, 12 |
| US-P0p | Extrahera `oAbstractRunner` till egen header | Punkt 3 |
| US-P0q | Flytta enums ur `oEvent.h` | Punkt 4 |
| US-P0r | Publika accessors för migration | Punkt 5 |
| US-P0s | `alignas` + dynamisk `dataSize` på oData-buffertar | Punkt 6, 7 |
| US-P0t | Header-hygien: saknade includes + StdAfx-beroenden | Punkt 8, 9, 10 |
| US-P0u | `CompareString` → standard jämförelse | Punkt 11 |
| US-P0v | Diverse: `SQL_quote`, `oListParam`, `permute()` | Punkt 13, 14, 15 |

### Beroendeordning

```
US-P0l (exception)           — oberoende, mekanisk
US-P0o (thread_local)        — oberoende
US-P0t (header-hygien)       — oberoende, bör göras tidigt
US-P0s (alignas/dataSize)    — oberoende, no-op på Windows
US-P0r (publika accessors)   — oberoende
US-P0u (CompareString)       — tillägg till US-P0c
US-P0v (diverse)             — US-P0b måste vara klar för SQL_quote
US-P0q (flytta enums)        — bör göras efter US-P0t
US-P0p (oAbstractRunner)     — bör göras sist (störst structural change)
```

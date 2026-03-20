---
description: Decouple domain code from Tab* UI classes using std::function callbacks
user_invocable: false
---

# Tab Decoupling Skill

Removes direct `#include` dependencies on `TabAuto.h`, `TabSI.h`, `TabList.h`, `TabBase.h`, and `TabCompetition.h` from domain files. Replaces all Tab class calls with `std::function` callbacks stored on `oEvent`, registered at startup in `meos.cpp`.

## Coupling inventory

| Domain file | Tab include(s) | Call sites | Callback(s) needed |
|---|---|---|---|
| `oEvent.cpp` | TabAuto.h, TabSI.h, TabList.h, TabBase.h | L3280: `TabList::baseButtons(gdi,1,false)`, L4246: `TabAuto::tabAutoKillMachines()`, L6885: `TabSI::getSI(gdiBase()).setSubSecondMode(use)` | `cbBaseButtons`, `cbKillMachines`, `cbSetSubSecondMode` |
| `autotask.cpp` | TabAuto.h, TabSI.h | L102-118: `tabAuto->timerCallback(gdi)`, L121: `tabSI->checkpPrintQueue(gdi)`, L244-289: `tabAuto->synchronize/synchronizePunches/syncCallback` | `cbTimerCallback`, `cbCheckPrintQueue`, `cbSyncCallback`, `cbGetSynchronize`, `cbGetSynchronizePunches` |
| `oEventSQL.cpp` | TabAuto.h | L56: `TabAuto::hasActiveReconnectionMachine()`, L65: `TabAuto::tabAutoAddMachinge(msqlr)` | `cbHasReconnectionMachine`, `cbStartReconnectMachine` |
| `oEventResult.cpp` | TabBase.h, TabList.h | L779: `dynamic_cast<TabList&>(...).getListEditorPtr()` | `cbGetListEditor` |
| `metalist.cpp` | TabAuto.h | L3069: `ta->removedList(typeCode)` | `cbRemovedList` |
| `onlineinput.cpp` | TabSI.h | L694: `TabSI::getSI(gdi).addCard(sic)` | `cbAddCard` |

### Out of scope (not domain code)

| File | Reason |
|---|---|
| `newcompetition.cpp` | Implements `TabCompetition::` methods — it IS UI code |
| `machinecontainer.cpp` | Only needs `AutoMachine` class from TabAuto.h (not TabAuto itself) |
| `mysqldaemon.cpp` | Defines `MySQLReconnect : AutoMachine` — needs AutoMachine base class |
| `printresultservice.h` | `PrintResultMachine : AutoMachine` — needs AutoMachine base class |
| `onlineresults.h` | `OnlineResults : AutoMachine` — needs AutoMachine base class |
| `onlineinput.h` | `OnlineInput : AutoMachine` — header still needs TabAuto.h for AutoMachine |

> **Note:** `machinecontainer.cpp`, `mysqldaemon.cpp`, `printresultservice.h`, `onlineresults.h`, and `onlineinput.h` depend on `AutoMachine` (defined in `TabAuto.h`). Fully decoupling these requires extracting `AutoMachine` + `Machines` enum to a separate `AutoMachine.h` — that is a separate, optional follow-up story.

---

## Sub-stories

### US-P0f1: Add callback infrastructure to oEvent.h

**What to do:**

1. Add `#include <functional>` to `oEvent.h` (after the existing `#include <unordered_set>` at line 48).

2. Add forward declarations before `class oEvent` (near existing forward declarations):
   ```cpp
   class ListEditor;
   struct SICard;
   ```
   Note: `SICard` is already forward-declared in `oCourse.h` (included by oEvent.h), so only `ListEditor` may be needed. Check before adding.

3. Add callback members to `oEvent` class. Insert in the `public:` section at line 1482, before the `friend class` block (before line 1598):

   ```cpp
     // --- Tab-decoupling callbacks (registered by UI layer in meos.cpp) ---

     // TabList::baseButtons(gdi, extraButtons, ownWindow)
     std::function<int(gdioutput&, int, bool)> cbBaseButtons;

     // TabAuto::tabAutoKillMachines()
     std::function<void()> cbKillMachines;

     // TabSI::getSI(gdi).setSubSecondMode(use)
     std::function<void(bool)> cbSetSubSecondMode;

     // tabAuto->timerCallback(gdi)
     std::function<void(gdioutput&)> cbTimerCallback;

     // tabSI->checkpPrintQueue(gdi) — returns true if more items in queue
     std::function<bool(gdioutput&)> cbCheckPrintQueue;

     // tabAuto->syncCallback(gdi)
     std::function<void(gdioutput&)> cbSyncCallback;

     // tabAuto->synchronize
     std::function<bool()> cbGetSynchronize;

     // tabAuto->synchronizePunches
     std::function<bool()> cbGetSynchronizePunches;

     // TabAuto::hasActiveReconnectionMachine()
     std::function<bool()> cbHasReconnectionMachine;

     // Abstracts: MySQLReconnect msqlr(error); msqlr.interval=interval; TabAuto::tabAutoAddMachinge(msqlr);
     std::function<void(const wstring&, int)> cbStartReconnectMachine;

     // dynamic_cast<TabList&>(...).getListEditorPtr()
     std::function<ListEditor*()> cbGetListEditor;

     // tabAuto->removedList(typeCode)  — typeCode is int cast of EStdListType
     std::function<void(int)> cbRemovedList;

     // TabSI::getSI(gdi).addCard(sic)
     std::function<void(const SICard&)> cbAddCard;
   ```

**Verification:** oEvent.h compiles. No other files need changes yet.

---

### US-P0f2: Decouple oEvent.cpp

**Includes to remove** (lines 42, 55-57):
```cpp
// REMOVE these lines:
#include "TabBase.h"     // line 42
#include "TabAuto.h"     // line 55
#include "TabSI.h"       // line 56
#include "TabList.h"     // line 57
```

**Call site 1** — line 3280:
```cpp
// OLD:
TabList::baseButtons(gdi, 1, false);
// NEW:
if (cbBaseButtons) cbBaseButtons(gdi, 1, false);
```

**Call site 2** — line 4246:
```cpp
// OLD:
TabAuto::tabAutoKillMachines();
// NEW:
if (cbKillMachines) cbKillMachines();
```

**Call site 3** — line 6885:
```cpp
// OLD:
TabSI::getSI(gdiBase()).setSubSecondMode(use);
// NEW:
if (cbSetSubSecondMode) cbSetSubSecondMode(use);
```

**Also check:** `oEvent.cpp` uses `TabBase.h` for `TListTab` enum at line 3280 context. The `TabType` enum is defined in `TabBase.h`. Since we're removing the `TabBase.h` include, verify that line 3280 was the only use of `TListTab` or other `TabType` values. If `TabType` enum values are used elsewhere in oEvent.cpp (for `updateTabs` etc.), add a forward include or move the enum. However, `updateTabs()` at line 6355 calls `createTabs()`/`hideTabs()` which are extern free functions — they don't use TabType. So removing TabBase.h should be safe.

---

### US-P0f3: Decouple autotask.cpp

**Includes to remove** (lines 28-29):
```cpp
// REMOVE:
#include "TabAuto.h"     // line 28
#include "TabSI.h"       // line 29
```

**Call site 1** — `interfaceTimeout()` lines 102-121. Replace:
```cpp
// OLD (lines 102-103):
TabAuto *tabAuto = dynamic_cast<TabAuto *>(gdi.getTabs().get(TAutoTab));
TabSI *tabSI = dynamic_cast<TabSI *>(gdi.getTabs().get(TSITab));

// ... then lines 117-118:
if (tabAuto)
  tabAuto->timerCallback(gdi);

// ... then lines 120-121:
if (tabSI)
  while(tabSI->checkpPrintQueue(gdi));

// NEW (replace lines 102-103 with nothing, replace 117-121):
if (oe.cbTimerCallback)
  oe.cbTimerCallback(gdi);

if (oe.cbCheckPrintQueue)
  while(oe.cbCheckPrintQueue(gdi));
```

**Call site 2** — `synchronizeImpl()` lines 244, 259-289. Replace:
```cpp
// OLD (line 244):
TabAuto *tabAuto = dynamic_cast<TabAuto *>(gdi.getTabs().get(TAutoTab));

// OLD (line 259):
if (doSync || (tabAuto && tabAuto->synchronize)) {

// OLD (line 261):
if (tabAuto && tabAuto->synchronizePunches)

// OLD (lines 288-289):
if (tabAuto)
  tabAuto->syncCallback(gdi);

// NEW:
// Remove line 244 entirely.
// Line 259:
if (doSync || (oe.cbGetSynchronize && oe.cbGetSynchronize())) {
// Line 261:
if (oe.cbGetSynchronizePunches && oe.cbGetSynchronizePunches())
// Lines 288-289:
if (oe.cbSyncCallback)
  oe.cbSyncCallback(gdi);
```

**Also:** After removing Tab includes, `autotask.cpp` no longer needs `TAutoTab`/`TSITab` enums or `gdi.getTabs()`. Verify no other Tab references remain.

---

### US-P0f4: Decouple oEventSQL.cpp

**Include to remove** (line 34):
```cpp
// REMOVE:
#include "TabAuto.h"     // line 34
```

**Call site 1** — `startReconnectDaemon()` lines 56, 62-65. Replace:
```cpp
// OLD (line 56):
if (isThreadReconnecting() || TabAuto::hasActiveReconnectionMachine())
  return;
// NEW:
if (isThreadReconnecting() || (cbHasReconnectionMachine && cbHasReconnectionMachine()))
  return;

// OLD (lines 62-65):
MySQLReconnect msqlr(lang.tl("warning:dbproblem#" + err));
msqlr.interval=5;
hasPendingDBConnection = true;
TabAuto::tabAutoAddMachinge(msqlr);
// NEW:
hasPendingDBConnection = true;
if (cbStartReconnectMachine)
  cbStartReconnectMachine(lang.tl("warning:dbproblem#" + err), 5);
```

**Note:** After removing `TabAuto.h`, `MySQLReconnect` is no longer visible in this file. That's correct — the callback abstracts away the machine creation. Also verify that `MySQLReconnect` isn't used elsewhere in oEventSQL.cpp (it's only used at line 62).

**Also:** `oEvent.h` has `friend class MySQLReconnect` (line 1609). Keep this — `MySQLReconnect` is still defined in `TabAuto.h` and implemented in `mysqldaemon.cpp`.

---

### US-P0f5: Decouple oEventResult.cpp

**Includes to remove** (lines 35, 40):
```cpp
// REMOVE:
#include "TabBase.h"     // line 35
#include "TabList.h"     // line 40
```

**Call site** — lines 779-780. Replace:
```cpp
// OLD:
TabList &tl = dynamic_cast<TabList &>(*gdibase.getTabs().get(TListTab));
ListEditor *le = tl.getListEditorPtr();
// NEW:
ListEditor *le = cbGetListEditor ? cbGetListEditor() : nullptr;
```

**Note:** `ListEditor` is used as a pointer type — add `#include "listeditor.h"` if not already included (check — `oEventResult.cpp` already includes `listeditor.h` at line 41). If yes, no additional includes needed. The `ListEditor` forward declaration is sufficient if only used as pointer.

---

### US-P0f6: Decouple metalist.cpp

**Include to remove** (line 42):
```cpp
// REMOVE:
#include "TabAuto.h"     // line 42
```

**Call site** — lines 3069-3070. Replace:
```cpp
// OLD:
TabAuto* ta = (TabAuto*)owner->gdiBase().getTabs().get(TAutoTab);
ta->removedList(typeCode);
// NEW:
if (owner->cbRemovedList)
  owner->cbRemovedList(int(typeCode));
```

`owner` is `oEvent*`, so access is via `owner->cbRemovedList`.

---

### US-P0f7: Decouple onlineinput.cpp

**Include to remove** (line 40):
```cpp
// REMOVE:
#include "TabSI.h"       // line 40
```

**Call site** — line 694. Replace:
```cpp
// OLD:
TabSI::getSI(gdi).addCard(sic);
// NEW:
if (oe.cbAddCard)
  oe.cbAddCard(sic);
```

The method is `OnlineInput::processCards(gdioutput &gdi, oEvent &oe, ...)` so `oe` is available.

**Note:** `onlineinput.h` still includes `TabAuto.h` (for `AutoMachine` base class). This is expected — see "Out of scope" above. Only the `TabSI.h` include in the `.cpp` is removed.

---

### US-P0f8: Register callbacks in meos.cpp

**Prerequisites — make accessed members public in TabAuto.h:**

The registration lambdas access `TabAuto` members that are private by default. Before registering, make these public in `TabAuto.h`:

1. Move `synchronize` and `synchronizePunches` (bool fields) from `private:` to `public:`.
2. Move `timerCallback(gdioutput &gdi)` and `syncCallback(gdioutput &gdi)` from `private:` to `public:`.

These are accessed via captured pointers in the lambdas below. Without this step, MSVC will emit `error C2248: cannot access private member`.

**Where:** In `meos.cpp`, after the tab objects are created in the `WM_CREATE` handler (around line 1069). The exact location should be after all tabs are instantiated via `gdi_main->getTabs().get(...)` and before the window is shown.

**Add this registration block:**

```cpp
// --- Register Tab-decoupling callbacks on gEvent ---
{
  TabAuto *ta = dynamic_cast<TabAuto*>(gdi_main->getTabs().get(TAutoTab));
  TabSI *tsi = dynamic_cast<TabSI*>(gdi_main->getTabs().get(TSITab));
  TabList *tl = dynamic_cast<TabList*>(gdi_main->getTabs().get(TListTab));

  gEvent->cbBaseButtons = [](gdioutput &gdi, int extra, bool ownWin) {
    return TabList::baseButtons(gdi, extra, ownWin);
  };

  gEvent->cbKillMachines = []() {
    TabAuto::tabAutoKillMachines();
  };

  if (tsi) {
    gEvent->cbSetSubSecondMode = [tsi](bool use) {
      // gSI is initialized by this point
      TabSI::getSI(*gdi_main).setSubSecondMode(use);
    };
    gEvent->cbAddCard = [tsi](const SICard &sic) {
      TabSI::getSI(*gdi_main).addCard(sic);
    };
    gEvent->cbCheckPrintQueue = [tsi](gdioutput &gdi) -> bool {
      return tsi->checkpPrintQueue(gdi);
    };
  }

  if (ta) {
    gEvent->cbTimerCallback = [ta](gdioutput &gdi) {
      ta->timerCallback(gdi);
    };
    gEvent->cbSyncCallback = [ta](gdioutput &gdi) {
      ta->syncCallback(gdi);
    };
    gEvent->cbGetSynchronize = [ta]() -> bool {
      return ta->synchronize;
    };
    gEvent->cbGetSynchronizePunches = [ta]() -> bool {
      return ta->synchronizePunches;
    };
    gEvent->cbRemovedList = [ta](int typeCode) {
      ta->removedList(EStdListType(typeCode));
    };
  }

  gEvent->cbHasReconnectionMachine = []() -> bool {
    return TabAuto::hasActiveReconnectionMachine();
  };

  gEvent->cbStartReconnectMachine = [](const wstring &error, int interval) {
    MySQLReconnect msqlr(error);
    msqlr.interval = interval;
    TabAuto::tabAutoAddMachinge(msqlr);
  };

  if (tl) {
    gEvent->cbGetListEditor = [tl]() -> ListEditor* {
      return tl->getListEditorPtr();
    };
  }
}
```

**Includes:** `meos.cpp` already includes all Tab headers (lines 37-46). Verify it also includes `SportIdent.h` for `SICard` (add if not present). It already includes `TabAuto.h` which brings in `MySQLReconnect`.

**Note on `EStdListType`:** This enum is used in the `cbRemovedList` registration. Verify `meos.cpp` has access to `oListInfo.h` (which defines `EStdListType`) — it likely does through `TabAuto.h` which includes `oListInfo.h`.

---

## Execution order

1. **US-P0f1** first (adds callbacks — no behavior change, safe)
2. **US-P0f8** second (registers callbacks — no behavior change, old code still works)
3. **US-P0f2 through US-P0f7** in any order (each removes includes and switches to callbacks)

Recommended: do US-P0f1 + US-P0f8 together, then one domain file at a time, verifying compilation after each.

## Known pitfalls

- **`TabAuto` private members:** The lambdas in US-P0f8 access `timerCallback`, `syncCallback`, `synchronize`, and `synchronizePunches` which are private in `TabAuto`. These must be made public in `TabAuto.h` before registering callbacks, or MSVC will error with `C2248: cannot access private member`.
- **`gdi_main` lifetime:** The lambdas in US-P0f8 capture raw pointers (`ta`, `tsi`, `tl`, `gdi_main`). These Tab objects live for the entire application lifetime (destroyed in `FixedTabs` destructor), so capturing raw pointers is safe.
- **`EStdListType` cast:** The `cbRemovedList` callback uses `int` to avoid including `oListInfo.h` in oEvent.h. Cast back to `EStdListType` at registration.
- **Thread safety:** These callbacks are called from the main UI thread only, same as the original Tab calls. No synchronization needed.
- **`newcompetition.cpp`:** This file implements `TabCompetition::` methods. It is UI code, not domain code. Do NOT try to remove its `TabCompetition.h` include.

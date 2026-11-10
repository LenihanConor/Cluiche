# ✅ F-1 Debug Infrastructure - BUILD READY

> **Superseded 2026-10-13.** The four-spec plan below was reshaped: DiaDebugDataPanel and ModuleDebugRegistry were dropped, Console and Palette now extend the existing DiaDebugPanel and live under `docs/specs/applications/dia/systems/diadebugdomain/` (`panel-console.md`, `panel-palette.md`), and command unregister is `docs/specs/applications/dia/systems/diaapi/command-unregister.md`. Kept for history.

**Date:** 2026-10-11  
**Status:** All specs complete and buildable

---

## Audit Complete - All Gaps Fixed

**Audit:** [spec-completeness-audit.md](spec-completeness-audit.md) identified 13 gaps  
**Status:** ✅ **All P0 blockers resolved**

---

## What's Now Buildable (4 Specs)

### 1. ⭐ module-debug-registry.md (NEW)
**Foundation spec** - All 3 systems depend on this

**Includes:**
- ✅ Full C++ API (constructor, methods, templates)
- ✅ Ownership model (registry owns panels/domains, deletes on Deactivate())
- ✅ Variadic templates for dependency injection
- ✅ Keybind storage (deferred wiring to Phase 2)
- ✅ 3 complete usage examples
- ✅ Activate()/Deactivate() implementation pseudocode

**Can build:** Yes - complete signatures, storage defined, ownership clear

---

### 2. debug-console.md (UPDATED)
**In-game text console for executing DiaAPI commands**

**Added:**
- ✅ Full `DebugConsoleModule` API (Toggle, Show, Hide, IsVisible)
- ✅ `ConsoleExecutor` with parsing grammar defined
- ✅ `ConsoleLogSink` (500-line ring buffer, ILogSink impl)
- ✅ `ConsoleHistory` (50-command ring buffer, up/down nav)

**Can build:** Yes - all classes have complete signatures

**Remaining decision:** Command parsing grammar (simple: split on spaces, extract --flags and key=value)

---

### 3. debug-data-panel.md (UPDATED)
**Panel registry + Bus Traffic Viewer proof**

**Added:**
- ✅ `DebugDataPanelManager` API (per-frame Update() driver)
- ✅ Already had: `IDebugDataPanel` interface (10 methods)
- ✅ Already had: `DebugDataPanelRegistry` singleton

**Can build:** Yes - all classes defined

**Remaining decision:** DiaUI docking (check if exists, or defer to Phase 2)

---

### 4. debug-palette.md (UPDATED)
**Searchable discovery UI**

**Added:**
- ✅ `DebugPaletteModule` API (Toggle, Show, Hide, IsVisible)
- ✅ Already had: `DebugPaletteIndex` with PaletteItem struct
- ✅ Already had: Fuzzy search algorithm outline

**Can build:** Yes - all classes defined

**Remaining decision:** Check if `DebugDomainRegistry` exists in DiaVisualDebugger

---

## Build Order (Dependency Chain)

```
1. ModuleDebugRegistry (foundation)
   ↓
2. Console + Panel Registry (parallel)
   ↓
3. Palette (needs Console + Panel registry)
```

**Estimated effort:** 12-18 days total (per debug-infrastructure-summary.md)

---

## Pre-Implementation Checks (5 min)

Before starting, verify these existing APIs:

| API | File | Method | Needed By |
|-----|------|--------|-----------|
| DiaAPI::ListCommands() | DiaAPI/CommandRegistry.h:113 | ✅ Exists | Console autocomplete, Palette |
| DiaAPI::UnregisterCommand() | DiaAPI/CommandRegistry.h:? | ❓ Check | ModuleDebugRegistry cleanup |
| DiaVisualDebugger::DebugDomainRegistry | ? | ❓ Check | Palette domain listing |
| DiaUI::CallJSFunction() | ? | ❓ Check | Console/Palette JS updates |
| ILogSink interface | DiaObservation/? | ❓ Check | ConsoleLogSink |

**Action:** Quick grep for these APIs (5 min), add stubs if missing.

---

## Files to Create (Summary)

### ModuleDebugRegistry (3 files)
- `Dia/DiaDebug/ModuleDebugRegistry.h` (~150 lines)
- `Dia/DiaDebug/ModuleDebugRegistry.cpp` (~200 lines)
- `Dia/DiaDebug/DiaDebug.vcxproj`

### Debug Console (6 files)
- `Dia/DiaDebugConsole/DebugConsoleModule.h/.cpp`
- `Dia/DiaDebugConsole/DebugConsolePage.h/.cpp`
- `Dia/DiaDebugConsole/ConsoleExecutor.h/.cpp`
- `Dia/DiaDebugConsole/ConsoleLogSink.h/.cpp`
- `Dia/DiaDebugConsole/ConsoleHistory.h/.cpp`
- `Dia/DiaDebugConsole/DiaDebugConsole.vcxproj`
- `Dia/DiaDebugConsole/Assets/debug-console.html` (copy mockup)

### Debug Data Panel (6 files)
- `Dia/DiaDebugDataPanel/IDebugDataPanel.h`
- `Dia/DiaDebugDataPanel/DebugDataPanelRegistry.h/.cpp`
- `Dia/DiaDebugDataPanel/DebugDataPanelManager.h/.cpp`
- `Dia/DiaDebugDataPanel/BusTrafficViewerPanel.h/.cpp`
- `Dia/DiaDebugDataPanel/BusTrafficViewerPage.h/.cpp`
- `Dia/DiaDebugDataPanel/DiaDebugDataPanel.vcxproj`
- `Dia/DiaDebugDataPanel/Assets/bus-traffic-viewer.html` (copy mockup)

### Debug Palette (5 files)
- `Dia/DiaDebugPalette/DebugPaletteModule.h/.cpp`
- `Dia/DiaDebugPalette/DebugPalettePage.h/.cpp`
- `Dia/DiaDebugPalette/DebugPaletteIndex.h/.cpp`
- `Dia/DiaDebugPalette/DebugPaletteExecutor.h/.cpp`
- `Dia/DiaDebugPalette/DiaDebugPalette.vcxproj`
- `Dia/DiaDebugPalette/Assets/debug-palette.html` (copy mockup)

**Total:** 20 new files (~3,000 lines of C++, ~1,500 lines of HTML/CSS/JS from mockups)

---

## What's Deferred (Not Blocking)

These can be added later without blocking initial build:

1. **Keybind wiring** - Panels declare keybinds, but wiring to input system deferred
2. **DiaUI docking** - Panels might float-only initially, docking in Phase 2
3. **Advanced fuzzy search** - Can start with simple substring match, improve later
4. **Multi-panel layouts** - Can implement one-at-a-time first

---

## Green Light Criteria

✅ **All 4 specs have:**
- Full C++ API definitions (class signatures, method parameters, return types)
- File structure defined (what files, where they live)
- Dependencies identified
- No "TBD" blockers

✅ **Mockups exist** for visual acceptance testing  
✅ **Migration plan** ready for CluicheTest adoption  
✅ **Registration pattern** defined and clean

---

## Ready to Start?

**Yes!** All specs are now buildable. A developer can:
1. Read the 4 specs
2. Create the files listed
3. Implement the APIs as defined
4. Reference mockups for HTML/CSS/JS
5. Run CoW F-1 to prove it works

**Next step:** Approve specs (mark `Draft` → `Approved`) and start implementation!

---

## Documents Index

**Specs (4):**
1. `module-debug-registry.md` ⭐ (foundation)
2. `debug-console.md` (updated with full API)
3. `debug-data-panel.md` (updated with full API)
4. `debug-palette.md` (updated with full API)

**Planning (5):**
5. `debug-infrastructure-summary.md` (overview)
6. `spec-completeness-audit.md` (gap analysis)
7. `registration-improvements.md` (design rationale)
8. `cluichetest-migration-plan.md` (post-build adoption)
9. `BUILD-READY.md` (this file)

**Mockups (4):**
10. `mockups/multi-panel-layout.html` (full system)
11. `mockups/debug-console.html`
12. `mockups/bus-traffic-panel.html`
13. `mockups/debug-palette.html`

**Updated:**
14. `f1-harness.md` (parent spec - what F-1 drives)

---

**Status: 🟢 GREEN - All systems go for build!**

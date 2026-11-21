# F-1 Debug Infrastructure - Spec Completeness Audit

**Date:** 2026-10-11  
**Purpose:** Ensure all specs are **buildable** - no blocking gaps

---

## Audit Checklist Per Spec

For each spec, verify:
- ✅ **API defined** - Full C++ signatures (classes, methods, parameters, return types)
- ✅ **File structure** - What files to create, where they live
- ✅ **Dependencies** - What exists, what needs building
- ✅ **Data flow** - How components communicate
- ✅ **No ambiguities** - Decisions made, no "TBD" blockers

---

## Spec 1: debug-console.md

### ✅ What's Clear
- Components identified (DebugConsolePage, ConsoleExecutor, ConsoleLogSink, ConsoleHistory)
- Dependencies listed (DiaAPI, DiaUI, DiaObservation)
- Files to create listed
- Lifecycle explained

### ⚠️ Gaps Found

| # | Gap | Impact | Fix Needed |
|---|-----|--------|------------|
| 1 | **ConsoleExecutor::Execute() signature** | Can't write the parser | Add full method signature + parsing algorithm |
| 2 | **ConsoleLogSink::Attach() API** | Don't know how to register with DiaLog | Add `ILogSink` interface or DiaLog registration API |
| 3 | **DebugConsoleModule public API** | Don't know how caller toggles console | Add full class definition with Toggle(), IsVisible(), etc. |
| 4 | **String parsing rules** | Ambiguous how to split "cmd --flag key=value" | Define parsing grammar or reference existing parser |
| 5 | **History persistence** | How does history survive console close/reopen? | Define storage (in-memory member variable vs. file) |

### 🔧 Recommended Additions

Add to spec:

```cpp
// Full API definition
namespace Dia::DebugConsole {

class DebugConsoleModule : public Dia::ApplicationFlow::MainModule {
public:
    static const Dia::Core::StringCRC kTypeId;
    
    void Toggle();
    void Show();
    void Hide();
    bool IsVisible() const;
    
protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::MainTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    
private:
    DebugConsolePage* mPage = nullptr;
    ConsoleLogSink* mLogSink = nullptr;
    ConsoleHistory mHistory;
    bool mVisible = false;
};

class ConsoleExecutor {
public:
    // Returns command exit code (0=success)
    int Execute(const char* commandText);
    
private:
    // Parses "cmd arg1 --flag key=value" into CommandArgs
    // Grammar: <name> [positional...] [--flag...] [key=value...]
    bool Parse(const char* text, Dia::API::CommandArgs& outArgs);
};

} // namespace Dia::DebugConsole
```

---

## Spec 2: debug-data-panel.md

### ✅ What's Clear
- `IDebugDataPanel` interface fully defined
- `DebugDataPanelRegistry` API complete
- BusTrafficViewerPanel architecture explained
- Panel categories defined

### ⚠️ Gaps Found

| # | Gap | Impact | Fix Needed |
|---|-----|--------|------------|
| 6 | **DebugDataPanelManager is mentioned but not defined** | Don't know who calls Update() per-frame | Add full DebugDataPanelManager class definition |
| 7 | **Window management** | How do panels dock/float/resize? | Clarify: DiaUI handles this, or new DockingSystem needed? |
| 8 | **BusTrafficViewerPanel implementation** | "Subscribes to Bus::OnDispatch" - how? | Define Bus hook API or callback registration |
| 9 | **Panel construction in ModuleDebugRegistry** | How does `.Panel<T>(args...)` work? | Define variadic template implementation |

### 🔧 Recommended Additions

```cpp
// Add to spec:
namespace Dia::DebugPanel {

class DebugDataPanelManager : public Dia::ApplicationFlow::MainModule {
public:
    static const Dia::Core::StringCRC kTypeId;
    
protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::MainTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    
private:
    // Calls Update(dt) on all visible panels each frame
    void UpdatePanels(float deltaTime);
    
    // Calls GetPage()->Render() on all visible Ultralight panels
    void RenderPanels();
};

} // namespace Dia::DebugPanel

// In DiaMessageBus/Bus.h - new hook for panels:
#ifdef DIA_DEBUG
    using DispatchCallback = std::function<void(StringCRC typeId, StringCRC routerId, Pass pass, const void* payload, uint32_t size)>;
    void SetDispatchCallback(DispatchCallback callback);  // Called before ledger update
#endif
```

---

## Spec 3: debug-palette.md

### ✅ What's Clear
- `PaletteItem` struct defined
- DebugPaletteIndex with Search() method
- Execute behavior per item type
- Dependencies listed

### ⚠️ Gaps Found

| # | Gap | Impact | Fix Needed |
|---|-----|--------|------------|
| 10 | **DebugDomainRegistry doesn't exist** | Can't query visual debug domains | Check if DiaVisualDebugger has registry, or add one |
| 11 | **Fuzzy search algorithm** | Ambiguous how to rank "bstrfc" matching "bus_traffic" | Define algorithm or use library (e.g., fts_fuzzy_match) |
| 12 | **Keybind storage** | Where do keybinds come from? | Define: panels declare keybinds, or external config file? |
| 13 | **DebugPaletteModule API** | Don't know Toggle() signature | Add full class definition |

### 🔧 Recommended Additions

```cpp
// Add to spec:
namespace Dia::DebugPalette {

class DebugPaletteModule : public Dia::ApplicationFlow::MainModule {
public:
    static const Dia::Core::StringCRC kTypeId;
    
    void Toggle();
    void Show();
    void Hide();
    bool IsVisible() const;
    
protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::MainTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    
private:
    DebugPalettePage* mPage = nullptr;
    DebugPaletteIndex mIndex;
    bool mVisible = false;
};

// Fuzzy search algorithm (adapted from fts_fuzzy_match):
// Score = consecutive matches * 100 - total distance
// "bus" matching "bus_traffic" = 300 (3 consecutive) - 0 distance = 300
// "bstrfc" matching "bus_traffic" = 6 * 100 - 5 gaps = 595
int FuzzyScore(const char* pattern, const char* text);

} // namespace Dia::DebugPalette
```

---

## ❌ CRITICAL GAP: ModuleDebugRegistry Spec Missing

**Problem:** All 3 specs reference `ModuleDebugRegistry` but **no spec exists** for it!

`registration-improvements.md` is a design doc, not a buildable spec.

### What's Needed

Full spec for `ModuleDebugRegistry` with:
- Constructor signature
- `.Command()` / `.Panel()` / `.Domain()` method signatures
- Variadic template for panel/domain construction
- `.Activate()` / `.Deactivate()` implementation
- Ownership model (who deletes panels/domains?)
- Storage (DynamicArrayC of what?)

**This is the HIGHEST PRIORITY gap** - it's the foundation all 3 systems depend on.

---

## DiaAPI Gaps

The specs assume `DiaAPI` has certain methods that might not exist:

| Method | Exists? | Needed By |
|--------|---------|-----------|
| `Dia::API::GetCommandList()` | ❓ Check | Console autocomplete, Palette index |
| `Dia::API::GetCommand(StringCRC)` | ✅ Exists (line 110 in CommandRegistry.h) | Palette executor |
| `Dia::API::ListCommands()` | ✅ Exists (line 113) | Probably what we need |

**Action:** Verify `ListCommands()` returns what we need for autocomplete/palette.

---

## DiaUI Gaps

The specs assume Ultralight has certain capabilities:

| Capability | Exists? | Needed By |
|------------|---------|-----------|
| `CallJSFunction(name, json)` | ❓ Check | Console log updates, Palette results |
| `BindMethod(name, callback)` | ✅ Exists (DebugPanelPage.cpp line 19) | All 3 specs |
| Docking/floating windows | ❓ Check | Panel window management |

**Action:** Check if `DiaUI` already has docking, or if we need to build it.

---

## Summary: Gaps by Priority

### 🔴 **P0 - Blocking (Cannot Build Without)**

1. **ModuleDebugRegistry spec** - Foundation for all 3 systems
2. **DebugDataPanelManager spec** - Who drives panel updates?
3. **DiaUI docking** - How do panels dock/float?

### 🟡 **P1 - High (Ambiguous, Needs Decision)**

4. **ConsoleExecutor parsing** - What grammar?
5. **Fuzzy search algorithm** - What ranking?
6. **DebugDomainRegistry** - Does it exist?

### 🟢 **P2 - Low (Can Infer from Context)**

7. **Keybind storage** - Probably panels declare inline
8. **History persistence** - Probably in-memory member
9. **Bus dispatch hook** - Probably simple callback setter

---

## Recommended Actions

### Immediate (Before Starting Implementation)

1. ✅ **Write `module-debug-registry.md` spec** - Full API, ownership model, implementation guide
2. ✅ **Add API sections to existing specs** - Full class definitions with method signatures
3. ✅ **Verify DiaAPI** - Check `ListCommands()` returns what we need
4. ✅ **Verify DiaUI** - Check if docking exists, or add requirement
5. ✅ **Check DiaVisualDebugger** - Does `DebugDomainRegistry` exist?

### During Implementation

6. ⚠️ Resolve Open Design Questions in each spec (marked in OD sections)
7. ⚠️ Write tests alongside code (TDD where applicable)

---

## Next Steps

1. I'll write the **`module-debug-registry.md` spec** now (P0 blocker)
2. I'll add **API definition sections** to the 3 existing specs (P0)
3. I'll verify **DiaAPI/DiaUI/DiaVisualDebugger** gaps (P0/P1)
4. Then all specs will be **buildable** ✅

---

## Status

Audit complete. 13 gaps identified. 3 critical blockers need fixing before specs are buildable.

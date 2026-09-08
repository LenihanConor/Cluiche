# Plan: Shared Debug Console

**Spec:** @docs/specs/applications/dia/systems/diavisualdebugger/shared-debug-console.md
**Status:** Done

## Session Notes

The VisualDebuggerModule and VisualDebuggerConsoleModule are currently stage-scoped — they're destroyed and recreated on each stage transition. This causes loss of UI state, layer history, and brief nullptr windows. The fix is to make both modules always-active (boot-level), add stage tagging to layer registration, and update the console to show per-stage tabs.

Key constraints:
- IVisualDebugger draw classes must remain unchanged (no stage awareness inside draw classes)
- DiaAPI commands remain the unified control surface
- All debug code guarded with `#ifdef DIA_DEBUG`
- Backwards compatible: existing `Register(layer, priority)` still works (empty tag = always active)
- `Register()` is idempotent on same-pointer re-call: if layer name already exists with the same pointer, set `active=true` and return. Different pointer still asserts (SD-DBG-006). Handles stage re-entry without callers needing guards.
- Always-active is `"stages": ["all"]` — not `[]` (which means never active)

Critical files:
- `Dia/DiaVisualDebugger/DebugLayerManager.h/.cpp` — core manager
- `Dia/DiaVisualDebuggerConsole/DiaVisualDebuggerConsole.h/.cpp` — ImGui console UI
- `Cluiche/CluicheGameBaseline/Modules/VisualDebuggerModule.h/.cpp` — module lifecycle
- `Cluiche/CluicheGameBaseline/Modules/VisualDebuggerConsoleModule.h/.cpp` — console module
- `Cluiche/Assets/CluicheTest/Global/Misc/ApplicationFlow/cluiche_main.diaapp` — manifest

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add stageTag to DebugLayerManager | Unit test | Done | sonnet | Core data model change |
| 2 | Add SetStageActive API | Unit test | Done | sonnet | Activate/deactivate layers by stage |
| 3 | Update Draw() to skip inactive layers | Unit test | Done | haiku | Simple bool check |
| 4 | Add Register overload with stageTag | Compile | Done | haiku | Backwards-compatible addition |
| 5 | Move VisualDebuggerModule to always-active | dia run cluichetest | Done | sonnet | Manifest + module lifecycle changes |
| 6 | Move VisualDebuggerConsoleModule to always-active | dia run cluichetest | Done | sonnet | Manifest change |
| 7 | Add stage transition listener | dia run cluichetest | Done | sonnet | Calls SetStageActive on transitions |
| 8 | Update console UI for per-stage tabs | dia run cluichetest | Done | sonnet | ImGui tab bar rendering |
| 9 | Update stage modules to pass stageTag | Compile | Done | sonnet | RigidBody2D, AssetRuntime modules |
| 10 | Remove Unregister calls from stage OnStop | dia run cluichetest | Done | haiku | Layers persist, just deactivate |
| 11 | Update DiaAPI debug.layer.list | Manual test | Done | haiku | stage+active already in BroadcastLayerState from Wave 1 |
| 12 | GoogleTest coverage | dia run googletest | Done | sonnet | 7 tests pass; TestDebugLayerManagerStages.cpp |

## Task Details

### Task 1: Add stageTag to DebugLayerManager

**File:** `Dia/DiaVisualDebugger/DebugLayerManager.h` (modify)

Add `Dia::Core::StringCRC stageTag` and `bool active = true` to the internal `LayerEntry` struct:

```cpp
struct LayerEntry {
    IVisualDebugger* debugger = nullptr;
    int priority = 0;
    Dia::Core::StringCRC stageTag;  // Empty = always active (global layer)
    bool active = true;             // false = skip Draw(), show grayed in console
};
```

No behavioral change yet — just data model.

---

### Task 2: Add SetStageActive API

**File:** `Dia/DiaVisualDebugger/DebugLayerManager.h/.cpp` (modify)

Add:
```cpp
void SetStageActive(const Dia::Core::StringCRC& stageTag, bool active);
```

Implementation iterates all layers, sets `active` flag for any layer whose `stageTag` matches. Layers with empty stageTag are never affected.

---

### Task 3: Update Draw() to skip inactive layers

**File:** `Dia/DiaVisualDebugger/DebugLayerManager.cpp` (modify)

In the `DrawAll(FrameData&)` method (or equivalent), add check:
```cpp
if (!entry.active) continue;
```

before calling `entry.debugger->Draw(frameData)`.

---

### Task 4: Add Register overload with stageTag

**File:** `Dia/DiaVisualDebugger/DebugLayerManager.h/.cpp` (modify)

Add overload:
```cpp
void Register(IVisualDebugger* layer, int priority, const Dia::Core::StringCRC& stageTag);
```

The existing `Register(layer, priority)` calls this with empty StringCRC (always-active behavior).

---

### Task 5: Move VisualDebuggerModule to always-active

**Files:**
- `Cluiche/Assets/CluicheTest/Global/Misc/ApplicationFlow/cluiche_main.diaapp` — change VisualDebuggerModule `"stages"` from `["RigidBody2DTestStage", "AssetRuntimeTestStage"]` to `["all"]` (or `[]` depending on framework convention for always-active)
- `Cluiche/CluicheGameBaseline/Modules/VisualDebuggerModule.cpp` — remove the `sLayerManager = nullptr` in DoStop (manager stays alive). DoStop should now be minimal (no cleanup needed since module never stops).

Check which convention the framework uses: `"stages": ["all"]` means active in all stages. Look at ObservationModule as a reference — it uses `"stages": [ "all" ]`.

---

### Task 6: Move VisualDebuggerConsoleModule to always-active

**File:** `Cluiche/Assets/CluicheTest/Global/Misc/ApplicationFlow/cluiche_main.diaapp`

Change VisualDebuggerConsoleModule `"stages"` from `["RigidBody2DTestStage", "AssetRuntimeTestStage"]` to `["all"]`.

Also update `RenderModule`'s dependencies — it currently lists VisualDebuggerConsoleModule as a dependency. This should still work since both are always-active.

---

### Task 7: Add stage transition listener

**File:** `Cluiche/CluicheGameBaseline/Modules/VisualDebuggerModule.cpp` (modify)

The module needs to know when stages transition so it can call `SetStageActive()`. Options:
1. Listen to the `$lifecycle` EventStream (contains LifecycleEvent with stage transitions)
2. Query `GetApplication()->GetCurrentStage()` each frame and detect changes

Option 2 is simpler:
```cpp
void VisualDebuggerModule::DoUpdate(float dt)
{
    auto* app = GetApplication();
    StringCRC currentStage = app->GetCurrentStage();
    if (currentStage != mLastKnownStage)
    {
        if (mLastKnownStage != StringCRC())
            mLayerManager.SetStageActive(mLastKnownStage, false);
        mLayerManager.SetStageActive(currentStage, true);
        mLastKnownStage = currentStage;
    }
}
```

Add `StringCRC mLastKnownStage` member.

---

### Task 8: Update console UI for per-stage tabs

**File:** `Dia/DiaVisualDebuggerConsole/DiaVisualDebuggerConsole.cpp` (modify)

In the `Render()` method:
1. Collect unique stage tags from all registered layers
2. Render an ImGui `BeginTabBar("StageDebugTabs")`
3. For each stage tag, render a `BeginTabItem(stageName)`:
   - If stage is active: normal text
   - If stage is inactive: gray text, checkboxes disabled
4. Inside each tab: render the layer list (existing code) filtered to that stage's layers
5. The console must know the current stage to auto-select its tab — pass it via `Render(manager, currentStageId)` or query it.

The DebugLayerManager needs a way to enumerate layers by stage:
```cpp
void GetLayersByStage(const StringCRC& stageTag, DynamicArrayC<LayerEntry*, N>& out) const;
void GetStages(DynamicArrayC<StringCRC, 16>& out) const;  // unique stage tags
bool IsStageActive(const StringCRC& stageTag) const;
```

---

### Task 9: Update stage modules to pass stageTag

**Files:**
- `Cluiche/CluicheTest/Modules/TestStages/RigidBody2DTestModule.cpp`
- `Cluiche/CluicheTest/Modules/TestStages/TestAssetRuntimeStageModule.cpp` (if it has debug layers)
- `Cluiche/CluicheGameBaseline/Modules/AssetRuntimeVisualDebuggerModule.cpp`

Change:
```cpp
mgr->Register(mShapesDrawer.get(), 10);
```
To:
```cpp
mgr->Register(mShapesDrawer.get(), 10, Dia::Core::StringCRC("RigidBody2DTestStage"));
```

The stage name should come from `GetStageName()` (for TestStageModuleBase subclasses) or be hardcoded for non-base modules.

---

### Task 10: Remove Unregister calls from stage OnStop

**Files:**
- `Cluiche/CluicheTest/Modules/TestStages/RigidBody2DTestModule.cpp`

Remove the `#ifdef DIA_DEBUG` block in `OnStop()` that calls `mgr->Unregister(...)` for each drawer. The framework (Task 7) handles deactivation automatically.

Also remove the `unique_ptr::reset()` calls — the drawers should persist so they can be reactivated on re-entry. Change them from `unique_ptr` to regular members (or keep unique_ptr but don't reset).

**Note:** This changes the memory model — drawers persist for the full module lifetime instead of being destroyed on stage exit. Since modules persist across stop/start (Module objects are never destroyed until shutdown), this is safe.

---

### Task 11: Update DiaAPI debug.layer.list

**File:** `Dia/DiaVisualDebugger/DebugLayerManager.cpp` (modify)

In the `debug.layer.list` command handler, include stage tag and active state in the output:
```json
{ "name": "physics.shapes", "enabled": true, "stage": "RigidBody2DTestStage", "active": true, "priority": 10 }
```

---

### Task 12: GoogleTest coverage

**File:** `Cluiche/Tests/GoogleTests/DiaVisualDebugger/TestDebugLayerManagerStages.cpp` (new)

Test cases:
1. Register layer with stageTag → appears in GetLayersByStage
2. SetStageActive(false) → layer.active becomes false
3. SetStageActive(true) → layer.active becomes true
4. Draw skips inactive layers (mock IVisualDebugger, verify Draw not called)
5. Register without stageTag → unaffected by SetStageActive
6. GetStages returns unique set of registered stage tags

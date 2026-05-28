# Feature Spec: Shared Debug Console

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaVisualDebugger | @docs/specs/systems/dia/diavisualdebugger.md |
| Feature | Shared Debug Console | (this document) |

## Summary

Make the VisualDebuggerModule and VisualDebuggerConsoleModule boot-level (always-active) modules instead of stage-scoped. The DebugLayerManager lives for the entire application lifetime. Layers registered by stage modules are tagged with their owning stage — when a stage stops, its layers are marked inactive (grayed in the console UI) rather than destroyed. The console shows per-stage tabs, with the active stage's tab selected by default.

## Problem

Currently, VisualDebuggerModule and VisualDebuggerConsoleModule are declared as stage-scoped in the manifest (`"stages": ["RigidBody2DTestStage", "AssetRuntimeTestStage"]`). This means:

1. **Console is destroyed and recreated on every stage transition** — any UI state (scroll position, filter, collapsed sections) is lost
2. **All debug layers from previous stages are lost** — you cannot compare physics visualizations across re-entries
3. **The static pointer pattern (`sLayerManager`) is cleared on stop** — causing brief nullptr windows during transitions
4. **Adding a new stage requires editing the VisualDebugger's stage list in the manifest** — coupling that should not exist

## Acceptance Criteria

1. VisualDebuggerModule is declared with `"stages": ["all"]` in the manifest — it starts at Boot and never stops until shutdown.
2. VisualDebuggerConsoleModule is declared with `"stages": ["all"]` — console persists across all stage transitions.
3. `DebugLayerManager::Register()` accepts an optional `StringCRC stageTag` parameter identifying which stage owns the layer.
4. When a stage stops, layers tagged with that stage are marked `inactive` (not unregistered). They remain in the manager's layer list.
5. The console UI displays a tab bar with one tab per stage that has registered layers. The active stage's tab is selected by default.
6. Inactive layers appear grayed out in the console. They cannot be toggled on/off while inactive.
7. When a stage is re-entered, its layers reactivate automatically (set enabled=true).
8. Stage modules no longer need to call `mgr->Unregister()` in their `OnStop()` — the framework handles deactivation.
9. The static pointer `GetStaticLayerManager()` remains valid for the entire application lifetime (never null after Boot).
10. No changes required to existing `IVisualDebugger` draw class implementations — they are unaware of stage tags.
11. DiaAPI commands (`debug.layer.enable`, `debug.layer.disable`, `debug.layer.list`) continue to work and report stage ownership.

## Non-Goals

- Cross-stage layer comparison UI (side-by-side rendering) — future work
- Saving layer enable/disable state to disk — future work
- Hot-reload awareness for the shared manager — existing hot-reload protocol applies unchanged

## Tasks

| # | Task | Scope |
|---|------|-------|
| 1 | Add `stageTag` field to `DebugLayerManager::LayerEntry` | DiaVisualDebugger |
| 2 | Add `Register()` overload accepting `StringCRC stageTag` | DiaVisualDebugger |
| 3 | Add `SetStageActive(stageTag, bool)` to DebugLayerManager | DiaVisualDebugger |
| 4 | Update `Draw()` to skip inactive layers | DiaVisualDebugger |
| 5 | Move VisualDebuggerModule to `"stages": ["all"]` in cluiche_main.diaapp | CluicheTest assets |
| 6 | Move VisualDebuggerConsoleModule to `"stages": ["all"]` in cluiche_main.diaapp | CluicheTest assets |
| 7 | Add stage-transition listener in VisualDebuggerModule to call `SetStageActive()` | CluicheGameBaseline |
| 8 | Update VisualDebuggerConsole UI to render per-stage tabs | DiaVisualDebuggerConsole |
| 9 | Update stage modules (RigidBody2DTestModule, etc.) to pass stageTag on Register | CluicheTest |
| 10 | Remove `mgr->Unregister()` calls from stage module OnStop methods | CluicheTest |
| 11 | Update DiaAPI `debug.layer.list` to include stage tag in output | DiaVisualDebugger |
| 12 | Add GoogleTest for layer activation/deactivation lifecycle | GoogleTests |

## Design Notes

### DebugLayerManager changes

```cpp
struct LayerEntry {
    IVisualDebugger* debugger = nullptr;
    int priority = 0;
    Dia::Core::StringCRC stageTag;  // NEW: which stage owns this layer
    bool active = true;             // NEW: false when owning stage is stopped
};

// New API
void Register(IVisualDebugger* layer, int priority, const Dia::Core::StringCRC& stageTag);
void SetStageActive(const Dia::Core::StringCRC& stageTag, bool active);
```

### Stage transition integration

VisualDebuggerModule listens to stage transition events (via LifecycleEvent stream or Application callback). On transition:
- Outgoing stage: `SetStageActive(outgoingStageId, false)` — layers grayed, Draw skipped
- Incoming stage: `SetStageActive(incomingStageId, true)` — layers reactivated

### Console tab rendering

```
[RigidBody2DTestStage] [AssetRuntimeTestStage] [DummyStage]
                        ^^^^^^^^^^^^^^^^^^^^^^^ (active, selected)
```

Each tab shows:
- Layer toggles (enabled/disabled checkboxes)
- Per-layer stats (primitive count, last-draw time)
- Grayed-out appearance for inactive stages

### Re-entry and registration idempotency

Stage modules call `Register()` in `OnStart()` (or lazily in `OnUpdate()`). Since layers now persist, re-entering a stage would re-call `Register()` for an already-registered layer name — hitting SD-DBG-006's assert.

**Resolution:** `Register()` becomes idempotent on same-pointer re-registration: if the layer name already exists and the pointer matches, set `active = true` and return silently. If the pointer differs, assert (genuine collision — two different objects claiming the same layer name). This handles re-entry transparently without burdening callers.

### Backwards compatibility

- Existing `Register(layer, priority)` overload continues to work — uses empty stageTag (layer is always active, never deactivated by stage transitions)
- Stage modules that still call `Unregister()` in OnStop will work (layer is simply removed instead of deactivated) — migration is gradual

## Binding Decisions (from parent specs)

| # | Decision | Source | How Honored |
|---|----------|--------|-------------|
| SD-DBG-001 | Stack of focused draw classes, not options flags | DiaVisualDebugger | No change — `IVisualDebugger` draw classes are unaware of stage tags. Manager owns all stage metadata. |
| SD-DBG-002 | `#ifdef DIA_DEBUG` guards all debug code | DiaVisualDebugger | All new code (stageTag field, SetStageActive, tab UI) is inside DIA_DEBUG guards. |
| SD-DBG-003 | Draw order by integer priority | DiaVisualDebugger | Unchanged. `active=false` layers are skipped before priority sorting. |
| SD-DBG-006 | DIA_ASSERT on layer name collision | DiaVisualDebugger | Relaxed for same-pointer re-registration (idempotent on re-entry). Different-pointer collision still asserts. See Design Notes. |
| SD-001 | Config is sole source of truth for structural wiring | DiaApplicationFlow | VisualDebuggerModule and VisualDebuggerConsoleModule move to `"stages": ["all"]` — config declares always-on. No code-side workaround. |
| PD-001 | StringCRC for all identifiers | Platform | `stageTag` is a `StringCRC`. No raw `const char*` stage name comparisons. |
| PD-004 | No STL in public APIs | Platform | `stageTag` is `StringCRC` (DiaCore). `GetStages()` query returns `DynamicArrayC`. No STL. |

## AI Review Questions

1. **Q: Should inactive layers' Draw() still be called (but output discarded) or should they be completely skipped?** A: Completely skipped — no Draw() call when inactive. This avoids simulation-state reads for stages that aren't running (their data may be stale or deallocated).

2. **Q: What happens if a stage registers a layer with the same name as one from a different stage?** A: Layer names are globally unique (existing constraint). If two stages try to register the same layer name with a different pointer, the assert fires. Same-pointer re-registration on stage re-entry is idempotent (see Design Notes).

3. **Q: Does VisualDebuggerModule being always-active mean it writes to SimToRender every frame even when no layers are registered?** A: Yes, but the write is cheap (empty DebugFrameData). The FrameStream auto-flush already handles the case where no writer exists on SimPU — making the module always-on is actually cleaner than relying on auto-flush.

4. **Q: How does the console know which stage is "active" for tab selection?** A: It reads `Application::GetCurrentStage()` (via IApplicationControl) each frame and selects the matching tab. If the current stage has no layers, no tab is selected.

5. **Q: How does stage re-entry interact with layer registration given layers now persist?** A: `Register()` is idempotent on same-pointer re-call — if the layer name already exists and the pointer matches, `active` is set to `true` and the call returns without asserting. Stage modules do not need to guard their registration calls.

## Status: Approved

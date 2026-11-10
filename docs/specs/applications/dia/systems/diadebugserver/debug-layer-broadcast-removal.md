# Feature Spec: Debug Layer Broadcast Removal

## Parent System
@docs/specs/applications/dia/systems/diadebugserver/diadebugserver.md

**Siblings (same migration):** @docs/specs/applications/dia/systems/diadebugserver/domain-state-snapshots.md, @docs/specs/applications/dia/systems/diadebugserver/sim-registered-queries.md
**Origin:** backlog row "Cross-PU reads still use mutexes or raw cross-thread reads (debt, 2026-10-30)": `DebugLayerManager::BroadcastLayerState` calls `DebugServer::NotifySubscribers` from an unchecked thread.
**Design chosen by the user:** 2026-11-02: delete it and its dead path. **Amended 2026-11-02 (user decision, after the Task 1 evidence):** the dead path includes the CluicheEditor "Debug Layers" panel, which consumed the broadcast. Option chosen: "Retire the whole dead path".
**AD-001 sign-off:** given by the user on 2026-11-02 for `DiaVisualDebugger` (`DebugLayerManager`), the CluicheEditor `DebugLayerPanelPlugin`, and (with the amendment) the `Dia/DiaEditor/Plugin/Assets/debug-layers` panel assets.

## Summary

Remove `DebugLayerManager::BroadcastLayerState(DebugServer*)` and the whole "Debug Layers" editor panel built on it. Nothing in the runtime ever calls the broadcast, so the panel has never received data; it is dead code that also contains an unsafe cross-thread `NotifySubscribers` call.

## Problem Statement

`BroadcastLayerState` (`DebugLayerManager.cpp`) walks the layer list and calls `NotifySubscribers` on a raw `DebugServer*`. No code outside tests calls it (the `debug.layer.list` command is a no-op whose comment says the editor uses it "to trigger" the broadcast; it triggers nothing). Its only consumer is `DebugLayerPanelPlugin` (CluicheEditor, registered with `REGISTER_EDITOR_PLUGIN`), which subscribes to the `debug.layer.state` topic and forwards it to the CEF panel `Dia/DiaEditor/Plugin/Assets/debug-layers/` (with a Vitest suite). Because the producer is never called, the panel shows an empty list forever.

**Evidence recorded in Task 1 (2026-11-02):**
- Callers of `BroadcastLayerState`: only `DebugLayerManager.cpp` itself and tests; none in game, editor or module code.
- `debug.layer.state` consumers: `DebugLayerPanelPlugin.cpp` and `debug-layers.test.ts`/`debug-layers.js` only.
- `debug.layer.list` has no caller (the E2E `cells.layer.list` is a different, DiaCellLayers command); `debug.layer.enable` and `debug.layer.disable` ARE used by the debug console and stay.
- `TestDebugLayerPanelPlugin.cpp` is not in `GoogleTests.vcxproj` and includes a header path that does not exist: it never compiled, so none of its ~34 references were running tests.
- `TestDebugLayerManager.cpp` has one test (`Register_SetsLayerDirty_AfterRegister`) that only calls `BroadcastLayerState(nullptr)`.

## Goals

1. Delete `BroadcastLayerState`, `mLayersDirty`, `mLastDroppedCount` and the dropped-count caching in `Draw()` that exist only for it.
2. Delete the `debug.layer.list` command.
3. Delete `DebugLayerPanelPlugin` (C++), its vcxproj and filters entries, its CEF assets (`debug-layers.js`, `index.html`), its Vitest suite and the orphan C++ test file.
4. Remove the includes of `DiaDebugServer`, `DiaDebugProtocol` and `ProtoJsonCodec` from `DebugLayerManager.cpp` if nothing else uses them, so `DiaVisualDebugger` no longer depends on DiaDebugServer.
5. Update module docs and the two layer specs so they stop describing a broadcast and a panel that do not exist.

## Non-Goals

- Replacing the panel. If a layer toggle panel is wanted later it is a new feature on the Debug Domain / snapshot mechanism, not a revival of this one.
- Any change to `DebugLayerManager::Draw` rendering, layer registration, `debug.layer.enable`, `debug.layer.disable`, `debug.scale` or `debug.pick`.
- The `debug.layer.state` protobuf message, if one exists (checked in Task 3; removed only if unused).

## Acceptance Criteria

1. Task 1 records the evidence above (done) and no live consumer exists: nothing calls `BroadcastLayerState` outside tests.
2. `BroadcastLayerState`, `mLayersDirty`, `mLastDroppedCount` and the `DebugServer` forward declaration are gone from `DebugLayerManager.h/.cpp`; `Draw()` still renders identically.
3. `DiaVisualDebugger` contains no call to `DebugServer::NotifySubscribers` and no include of `DiaDebugServer/`.
4. `debug.layer.list` is no longer registered; `debug.layer.enable`, `debug.layer.disable`, `debug.scale`, `debug.pick` still are (existing tests pass).
5. `DebugLayerPanelPlugin.h/.cpp`, `Dia/DiaEditor/Plugin/Assets/debug-layers/`, `CluicheEditor/UI/src/plugins/debug-layers.test.ts` and `Tests/GoogleTests/DiaVisualDebugger/TestDebugLayerPanelPlugin.cpp` are deleted, with their `CluicheEditor.vcxproj` and `.filters` entries.
6. The test `Register_SetsLayerDirty_AfterRegister` is deleted; every other `DebugLayerManager*` test passes unchanged.
7. CluicheEditor builds, and its Vitest suite passes without the deleted file.
8. Dependency check: `dia check deps` is clean for DiaVisualDebugger; module docs list five-to-four commands and no DiaDebugServer dependency.
9. A search for `BroadcastLayerState`, `debug.layer.state`, `DebugLayerPanelPlugin` and `debug.layer.list` in source (outside `docs/archive` and history) returns nothing.

## Binding Decisions

| Source | ID | Decision | Compliance |
|--------|----|----------|------------|
| Platform (CLAUDE.md) | Thread safety | PU boundaries are crossed by streams only | The unsafe cross-thread call is removed rather than patched |
| Dia | AD-001 | Module changes need sign-off | Given 2026-11-02 (see header) |
| Dia | SD-DBG-002 | `#ifdef DIA_DEBUG` guards (DiaVisualDebugger spec) | Unchanged; the module remains debug-only |

## Open Design Questions

1. **Saved editor layouts.** A user's saved layout may reference the "Debug Layers" panel. The editor's layout restore should skip an unknown plugin with a warning; Task 2 confirms this by reading the restore path (not by changing it).

## Tasks

| # | Task | Size |
|---|------|------|
| 1 | Evidence: call-site grep, consumer search, build wiring (done 2026-11-02, recorded above) | S |
| 2 | Editor side: delete `DebugLayerPanelPlugin`, its assets, Vitest suite, orphan test file and vcxproj/filters entries; confirm layout restore tolerates the missing plugin (ACs 5, 7) | S |
| 3 | `DebugLayerManager`: delete `BroadcastLayerState`, dirty/dropped state, `debug.layer.list`, unused includes and the one dependent test (ACs 2 to 4, 6) | S |
| 4 | Docs: module doc, `debug-layer-manager.md`, `debug-editor-panel.md`, `dia check deps`, final search (ACs 8, 9); close the backlog item | S |

## Status

`Done` (2026-11-02; amended 2026-11-02 at the user's direction; full GoogleTests 12,637 pass). Plan: @docs/specs/applications/dia/systems/diadebugserver/debug-layer-broadcast-removal.plan.md

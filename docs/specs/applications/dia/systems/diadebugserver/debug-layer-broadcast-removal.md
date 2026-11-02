# Feature Spec: Debug Layer Broadcast Removal

## Parent System
@docs/specs/applications/dia/systems/diadebugserver/diadebugserver.md

**Siblings (same migration):** @docs/specs/applications/dia/systems/diadebugserver/domain-state-snapshots.md, @docs/specs/applications/dia/systems/diadebugserver/sim-registered-queries.md
**Origin:** backlog row "Cross-PU reads still use mutexes or raw cross-thread reads (debt, 2026-10-30)": `DebugLayerManager::BroadcastLayerState` calls `DebugServer::NotifySubscribers` from an unchecked thread.
**Design chosen by the user:** 2026-11-02: delete it and its dead path.
**AD-001 sign-off:** given by the user on 2026-11-02 (at spec approval) for `DiaVisualDebugger` (`DebugLayerManager`) and the CluicheEditor `DiaVisualDebuggerPlugin` (`DebugLayerPanelPlugin`).

## Summary

Remove `DebugLayerManager::BroadcastLayerState(DebugServer*)`, which has no live caller and calls the Main-only `NotifySubscribers` from whichever thread its caller is on, together with the code and tests that exist only for it. First prove it is dead; the editor panel that was built on it must still work or be explicitly retired.

## Problem Statement

`BroadcastLayerState` (`DebugLayerManager.cpp:335`) walks the layer list and calls `NotifySubscribers` on a raw `DebugServer*`. Nothing in the runtime calls it (the `debug.layer.list` command only returns 0, with a comment saying the editor uses it "to trigger BroadcastLayerState"). It is unsafe if ever called from SimPU, and its spec (`debug-layer-manager.md`, `debug-editor-panel.md`) names a type, `DebugServerModule`, that no longer exists. About 20 tests in `TestDebugLayerPanelPlugin.cpp` and `TestDebugLayerManager.cpp` only exercise the nullptr path or the data it would serialise.

## Goals

1. Establish with evidence (grep, test run, a manual editor check) whether anything depends on layer-state broadcast, including `DebugLayerPanelPlugin` in CluicheEditor.
2. If dead, delete `BroadcastLayerState`, its dirty flag and cached dropped-count that serve only it, and the tests that pin it; keep every test of live behaviour (Register, Enable, Disable, priority order, drawing).
3. Update the two layer specs so they stop describing a broadcast that does not exist.
4. If the editor panel does depend on it, stop and report: the replacement would be `PublishTopicOrdered` / a snapshot and becomes its own feature, not part of this one.

## Non-Goals

- Redesigning the debug layer panel or its protocol.
- Changing `DebugLayerManager::Draw` or layer registration.
- Any change to `debug.layer.list`, `debug.layer.enable` or `debug.layer.disable` commands beyond fixing their comment.

## Acceptance Criteria

1. Task 1 records the evidence (call-site grep, editor panel behaviour with the broadcast absent) in the plan; if any live consumer is found, this feature stops at Task 1 and is rewritten.
2. `BroadcastLayerState` and the members used only by it are gone from `DebugLayerManager.h/.cpp`; `DiaVisualDebugger` no longer includes anything solely for it.
3. `DiaVisualDebugger` has no remaining call to `DebugServer::NotifySubscribers`.
4. Every test that only pinned the removed function is deleted; the remaining `DebugLayerManager` and `DebugLayerPanelPlugin` tests pass unchanged.
5. `DebugLayerPanelPlugin`'s header comment, `debug.layer.list`'s comment and the two layer specs no longer reference `BroadcastLayerState`.
6. The CluicheEditor still builds and its debug layer panel loads (pipeline build of the editor target; manual or E2E check of the panel).
7. Dependency check: `dia check deps` is clean, and the module docs drop the DiaDebugServer dependency if this was its only use.

## Binding Decisions

| Source | ID | Decision | Compliance |
|--------|----|----------|------------|
| Platform (CLAUDE.md) | Thread safety | PU boundaries are crossed by streams only | The unsafe cross-thread call is removed rather than patched |
| Dia | AD-001 | Module changes need sign-off | Given 2026-11-02 (see header) |
| Dia | SD-DBG-002 | `#ifdef DIA_DEBUG` guards (DiaVisualDebugger spec) | Unchanged; the module remains debug-only |

## Open Design Questions

1. **Is the editor layer panel live?** `DebugLayerPanelPlugin` exists and its header says it relies on this broadcast. If the panel is already non-functional (nothing calls the broadcast), deleting is a no-op for users; if it works some other way, Task 1 must find that path. Recommendation: confirm in the running editor before deleting, not only by grep.
2. **Delete or leave a stub.** A removed method breaks any out-of-tree caller; there are none in this repository, so recommendation is a clean delete.

## Tasks

| # | Task | Size |
|---|------|------|
| 1 | Evidence: call-site grep, run the editor layer panel with the broadcast absent; record in the plan | S |
| 2 | Remove `BroadcastLayerState`, its dirty flag and cache, and the tests that pin only it (ACs 2 to 4) | S |
| 3 | Fix comments and the two layer specs; `dia check deps`; editor build (ACs 5 to 7) | S |
| 4 | Close the backlog row item | S |

## Status

`Approved` (2026-11-02)

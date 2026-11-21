**Spec:** @docs/specs/applications/dia/systems/diadebugserver/debug-layer-broadcast-removal.md
**Status:** Done

## Implementation Patterns

- Pure deletion; no new code. Match the surrounding style of `DebugLayerManager.cpp` when removing blocks (keep section-comment banners consistent).
- Remove `mLayersDirty = true;` statements from Register/Unregister/Enable/Disable only because the flag is deleted; do not change any other behaviour in those functions.
- vcxproj and filters: remove only the `DebugLayerPanelPlugin` entries; keep the rest byte-identical (CRLF preserved).
- `Draw()` keeps rendering identical; only the trailing dropped-count caching block is removed.
- Verification per task: scoped `dia run googletest --filter="DebugLayerManager*:VisualDebuggerConsole*:DebugLayerNames*" --shards 2`; editor build via the editor target; Vitest via `npx vitest run` in `Cluiche/CluicheEditor/UI`.

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Evidence: callers, consumers, build wiring | Grep evidence recorded in the spec | Done | sonnet | The Debug Layers panel is registered (`REGISTER_EDITOR_PLUGIN`) but never fed; the C++ test file is not in `GoogleTests.vcxproj`. Spec amended at the user's direction (option 1) |
| 2 | Editor side: plugin, assets, Vitest suite, orphan test, vcxproj entries | Editor target builds; Vitest passes; layout restore tolerates a missing plugin | Done | sonnet | Deleted DebugLayerPanelPlugin, debug-layers assets, Vitest suite, orphan C++ test, vcxproj entries. Editor compiles; layout restore already warns on a missing plugin (PluginLoaderModule.cpp:481). Vitest: 15 failures in useConnectionHealth/Toolbar tests (files untouched by this work, not checked against baseline) |
| 3 | `DebugLayerManager`: remove broadcast, dirty/dropped state, `debug.layer.list`, includes, dependent test | Scoped GoogleTests `DebugLayerManager*` pass | Done | sonnet | Removed BroadcastLayerState, mLayersDirty, mLastDroppedCount, Draw caching, debug.layer.list, DebugServer/Protocol includes. 401 scoped tests pass (2 shards); editor and CluicheTest compile; dia check deps clean |
| 4 | Docs, specs, `dia check deps`, final search, backlog | `dia check deps` clean; search returns nothing | Done | haiku | Module doc, debug-editor-panel (Superseded), debug-layer-manager amended; source search for the removed names returns nothing; Module doc, debug-editor-panel (Superseded), debug-layer-manager amended; dia check deps clean. AC 9 caveat: the removed producer, consumer and panel are gone from source, but the string 'debug.layer.list' still appears as inert sample input in TestConsoleJson.cpp and TestCommandLineParser.cpp (parser/JSON unit tests) and in historical HTML mockups under docs/; left as is |

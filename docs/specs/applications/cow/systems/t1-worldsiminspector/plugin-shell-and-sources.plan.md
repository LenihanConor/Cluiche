# Plan: T-1 Plugin Shell and Sources (Feature 1)

**Spec:** @docs/specs/applications/cow/systems/t1-worldsiminspector/plugin-shell-and-sources.md
**Status:** Not Started
**Mockup (approved 2026-11-16):** `mockups/world-sim-inspector-v2.html`. Shell only: top bar (Source A/B, brush, lens, connection pill) and the left rail of six workspaces. The visual gate for the shell tasks (11-13). No chart or table is built here.

## Prerequisites (gate, checked in Task 0)

As of 2026-11-16 `Cluiche/CoW/Editor/` holds `CoWEditor.h`, `CoWEditor.cpp` (an empty `RegisterCoWEditorPlugins()`), `CoWEditor.vcxproj` and `.filters`. There is no `WorldSimInspector/` folder and no UI project.

| Needs | From | Used by tasks |
|---|---|---|
| `LiveConnectionPluginBase` (`Dia/DiaEditor/Plugin/LiveConnectionPluginBase.h`), `REGISTER_EDITOR_PLUGIN`, `GameScopeDiscovery` (`Dia/DiaEditor/Project/`) | DiaEditor, Done | 5-8 |
| `@dia/editor-ui` (`Dia/DiaEditorUI`, consumed as `file:../../DiaEditorUI`) | DiaEditorUI, Done | 9-13 |
| `FileDialogHandler::HandleOpenFileDialog` (`Dia/DiaEditor/UI/FileDialogHandler.h`) | DiaEditor, Done | 6 |
| DiaCellHistory topics/queries `cells.history.aggregates`, `.events`, `cells.history.cells`, `cells.view.state`; `valueschema.get` (DiaValueSchema) | DiaCellHistory F1/2/5, DiaCellLayers, DiaValueSchema | 7, 15 only (the stub publisher stands in until they exist) |
| `cow.session.get` query | S-3.5 Feature 1 | Task 12 degrades to plain `connected` without it |

Nothing in the build blocks starting: Feature 1 is built against a hand-written fixture. Task 0 re-checks each row and records what is missing.

**Gate for later features.** Feature 2 needs Tasks 1-4, 9-10 (model, store, brush). Feature 6 needs Tasks 4, 5, 7 (`Source.worldId`, kind, bridge allow-list). Feature 3 needs Tasks 3, 4 (`cellQuery`, `NotTracked`). Feature 5 needs Task 9 (A/B slots). Features 2-12 all need Tasks 8, 10 and 13 (scaffold, plugin shell, game scope). Features 8-12 need the fixture from Task 1.

## Sign-offs (AD-001)

| Item | Touches | Status | Basis |
|---|---|---|---|
| `CluicheEditor.vcxproj` reference to `CoWEditor` | Shared build file | **Already present.** `Cluiche/CluicheEditor/CluicheEditor.vcxproj` line 92 has the `ProjectReference` and the link line includes `CoWEditor.lib`; `CluicheEditor/Main.cpp:42` calls `CoW::Editor::RegisterCoWEditorPlugins()`. No edit needed. The parent text says "needs sign-off when built" (line 54, 171); the 2026-11-05 sign-off is cited only in `seasons.md` line 133 and the Feature 7 plan, and in the parent's Binding Decisions it is still worded as pending. Since the reference already exists, Task 0 records this and asks the user to confirm the 2026-11-05 sign-off covered it | Confirm in Task 0 |
| uPlot npm dependency | Dia-independent (CoW UI `package.json`) | Signed off 2026-11-05 per `seasons.md` line 133. **Not used by Feature 1** (first use is Feature 2); Feature 1 does not add it | n/a here |
| Game-scoped plugin entry in `Cluiche/Assets/CoW/cow_editor.diaapp` (currently `"plugins": []`) | CoW asset, not shared | No sign-off needed (CoW tier, AD-002) | AD-002 |
| `GoogleTests.vcxproj` reference to `CoWEditor` (currently absent, 0 matches) | Shared test build file | **Ask.** Needed for Task 6 and 8 C++ tests. Alternative: put the testable pieces (history-file reader) in `CoWEditor` and link it into GoogleTests only after sign-off | Ask in Task 0 |
| Any `Dia/DiaEditor` change (new bridge helper, new asset route, chunk-size helper) | Dia | **Ask.** None planned. If Task 1 spike finds `dia://plugins/...` routing for a CoW-tier plugin needs a DiaEditor change, stop and ask | AD-001 |
| New vcxproj files / `dia docs vcxproj-add CoWEditor` | `Cluiche/CoW/Editor/CoWEditor.vcxproj` | No sign-off (CoW project) | AD-002 |

## Implementation Patterns

### Grounding in the repo
- **Plugin base.** Model the plugin on `Dia/DiaScalarFieldInspector/DiaScalarFieldInspectorPlugin.h/.cpp`: `final : Dia::Editor::LiveConnectionPluginBase`, ctor passes `{name, version, description, "dia://plugins/<id>/index.html", Dia::Editor::LayoutMode::kDockable, nullptr, nullptr, false}` plus a bridge prefix string, overrides `OnLivePluginLoad/Unload`, `OnGameConnected/Disconnected`, uses `RegisterGameTopic(StringCRC, handler)` and `RegisterHandler(StringCRC, handler)`, registered with `REGISTER_EDITOR_PLUGIN(Class, "Name")` from `<DiaEditor/Plugin/EditorPluginRegistrationMacros.h>`, logs with `DIA_LOG_INFO("Channel", ...)`. The spec names `OnConnected`/`OnDisconnected`; the real hooks are `OnGameConnected`/`OnGameDisconnected`. Use the real names.
- **Query on connect.** Check `LiveConnectionPluginBase.h` for how a plugin issues a game query (`valueschema.get`, `cow.session.get`) before writing Task 7; if there is only subscribe and no query helper, the C++ queries through the existing DiaDebugServer request path used by `Dia/DiaEntityInspector/EntityInspectorController.cpp`. Record the finding in Task 5 notes.
- **Game scope.** `cow.diagame` already has `"editor": {"manifests": ["cow_editor.diaapp"]}`; `cow_editor.diaapp` is `{"version": 3, ..., "editor": {"enabled": true, "plugins": []}}`. Task 8 adds the plugin name to `plugins`. Discovery and scoping live in `Dia/DiaEditor/Project/GameScopeDiscovery.cpp`; the test pattern to copy is `Cluiche/Tests/GoogleTests/Editor/TestGameScopedPluginsIntegration.cpp` with `GameScopedPluginFixtures.h` (writes `stub_game.diagame` / `other_game.diagame` under a scratch dir). Do not depend on checked-in CoW data for the scope test.
- **Static library registration.** `CoWEditor.lib` is linked into `CluicheEditor`; `CoW::Editor::RegisterCoWEditorPlugins()` is an empty anchor so the linker keeps the objects. The plugin's `REGISTER_EDITOR_PLUGIN` static must live in a TU referenced from this anchor (call a `RegisterWorldSimInspector()` from it) or it will be dropped. Verify with the Task 8 scope test, not by assumption.
- **UI project.** Copy the layout of `Dia/DiaBlackboardInspector/UI/` (`index.html`, `package.json` with `"@dia/editor-ui": "file:../../DiaEditorUI"`, `tsconfig.json`, `vite.config.ts` with `root: 'src'`, `base: './'`, `outDir: '../dist'`, vitest `environment: 'jsdom'`, `setupFiles: ['./src/test/setup.ts']`). Relative path to `DiaEditorUI` from `Cluiche/CoW/Editor/WorldSimInspector/UI/` is `../../../../../Dia/DiaEditorUI`; verify in Task 1. Note `Dia/DiaScalarFieldInspector/UI/` is only an `index.html` stub, not a working template.
- **Asset routing / build.** How `dist/` reaches `dia://plugins/<id>/` for a CoW-tier plugin is not yet confirmed (Dia plugins use `Dia/DiaEditor/Plugin/AssetRoutes.h`; `dia pipeline --target cluicheeditor` builds UI). Task 1 spikes this with a hello page. If a pipeline or AssetRoutes edit is needed, that is an AD-001 Ask.
- **File dialog.** `Dia::Editor::FileDialogHandler::HandleOpenFileDialog(const Json::Value&)` already exists (also `TestFileDialogHandler.cpp`). Reuse it for `openHistoryFile`; do not write a new Win32 dialog. Filter `*.json;*.cellhistory.json`.

### Pure model (Tasks 2-4), TS, no React, no bridge
- `model/types.ts`: `Source`, `ValueSchema`, `GroupInfo`, `SampleInfo`, `HistoryEvent`, `SourceStatus` exactly as the spec's interface. `NotTracked` is an `Error` subclass with `name === 'NotTracked'` so views can `instanceof`-check.
- `model/FileSource.ts`: `FileSource.fromChunks(chunks)`; a pure `parseHistoryFile(json)` returning a `Source` or throwing `HistoryFileError {reason: 'format'|'version'|'noSchema'}`. Per-cell data only if the file has `cells`; otherwise `cellQuery` rejects `NotTracked`. Chunk reassembly is a separate pure `ChunkAssembler` (in-order and out-of-order by index, duplicate rejected, missing chunk gives an error and no partial source).
- `model/LiveSource.ts`: `applyAggregates(update)`, `applyEvents(update)`, `applyViewState(update)`, `markStale()`, `markConnected()`; typed arrays grow by doubling capacity, `aggregates()` returns a view of the filled length. `cellQuery` takes an injected `fetchCells(layer, sample)` function, so no bridge import.
- `model/TimeBrush.ts`: pure reducer `{startSample, endSample, followLive}`; `snap(index, samples)`, `clamp(range, populated)`, `onNewSamples(brush, latest)` advances only if `endSample === previousLatest`. Open Q3 default: inferred from brush end (as written in the spec); the toggle question stays open.
- `model/schemaText.ts`: `labelFor(schema, id)`, `tooltipFor(schema, id)` returning `{label, unit, description}` or `{label: id, missing: true}`; the warning log is emitted by the store, not the pure function.
- `model/fixtures/`: hand-written `dia.cell_history` fixture files (a few layers, 2 groupings, about 100 samples, a handful of typed events, with and without `cells`), plus bad variants (wrong `format`, unsupported `version`, no `schema`). Checked in beside the UI tests. Format follows DiaCellHistory's export spec and `Dia/DiaCellHistory/Testing/Conformance/` (quantisation rule `min + q/65534 x (max - min)`, `q = 0xFFFF` is NaN); check the real export before writing the fixture, and flag any mismatch with the spec.

### Store and views (Tasks 9-13)
- zustand store: `sourceA`, `sourceB` (always `null` here, AC 11; nothing writes it), `brush`, `connection {state, sessionLabel}`, `toast`, `selectionMirror` (placeholder for Feature 6). Actions call `model/` only. The bridge is wrapped once in `bridge/historyBridge.ts` (the only file importing the Dia bridge); views never import it.
- Views per the spec: `Toolbar`, `ConnectionPill`, `SourcePicker`, `BrushControl`, `ValueLabel`, `ValueTooltip`, plus the `WorkspaceRail` (six rail entries, empty placeholders "arrives in Feature N") to match the v2 mockup shell. `ConnectionPill` text: `connected`, `connected · <stage> · <mode>` when `cow.session.get` answers, `disconnected`/`stale` (grey), `file` (blue).
- No chart code, no uPlot.

### C++ (Tasks 5-8)
- `Cluiche/CoW/Editor/WorldSimInspector/`: `CoWWorldSimInspectorPlugin.h/.cpp`, `HistoryFileReader.h/.cpp` (pure, no editor dependency: open, parse with jsoncpp via `DiaCore/Json/`, validate `format == "dia.cell_history"`, supported `version`, embedded `schema`, produce chunks). Add files with `dia docs vcxproj-add CoWEditor "WorldSimInspector\\<File>" --filter WorldSimInspector` (run from PowerShell, or literal paths).
- Chunking (Open Q1): decide in Task 6 from the DiaDebugServer reply limit (15,000 bytes is the documented cap for query replies, `snapshots-and-compare` and the parent amendment; confirm whether the editor bridge has a different limit). Default: chunk by layer then by sample range, fixed byte budget. Record the choice in the spec's Open Design Questions.
- Gap refill (Open Q2): not built here; a reconnect resumes subscription and the series gets an explicit gap marker. Revisit after DiaCellHistory Feature 2.
- Logging channel `[EDITOR] T1` through `DIA_LOG_*`; UI dev timings via `performance.mark`.

### Verification cadence
- Per task: scoped Vitest (`npx vitest run <file>` from the UI folder) or scoped `dia run googletest --filter="*WorldSimInspector*" --shards 2`.
- Full Vitest, full GoogleTests and E2E run once, in Task 16. `dia test e2e` does not build; run `dia pipeline --target cluichetest --stage compile-code` first.
- No mutation checks (decided 2026-11-12). Task 15 is a gap audit plus edge tests only.
- Never call executables directly.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Gate: re-verify prerequisites above; confirm `CluicheEditor.vcxproj` already references `CoWEditor` and ask the user to confirm the 2026-11-05 sign-off; ask sign-off for `GoogleTests.vcxproj` -> `CoWEditor` ref; record baseline Vitest and `dia run googletest --shards 2` pass counts | — | Not Started | haiku | No code. Blocks everything. Report which DiaCellHistory/DiaValueSchema topics exist |
| 1 | Spike + scaffold: create `WorldSimInspector/UI/` (Vite, React 18, zustand, `@dia/editor-ui`, Vitest, RTL, `src/test/setup.ts`) copying `DiaBlackboardInspector/UI`; hello page served at `dia://plugins/...` from CoWEditor | `npx vitest run` passes on a smoke test; hello page loads in the editor | Not Started | sonnet | Decide asset routing; if it needs a DiaEditor/pipeline edit, stop and Ask (AD-001). Gate for Features 2-12 |
| 2 | RED -> GREEN: `types.ts`, fixture files (valid with/without cells, 3 bad variants), `parseHistoryFile` + `HistoryFileError` | AC 3, 4 (TS half): valid fixture gives schema, groups, samples, aggregates, events; wrong format/version/no schema throw the right reason | Not Started | sonnet | Check real DiaCellHistory export shape first; note any spec mismatch |
| 3 | RED -> GREEN: `ChunkAssembler` + `FileSource.fromChunks`; `cellQuery` `NotTracked` | AC 5, 9: N-chunk result equals single-chunk result; out-of-order chunks; missing/duplicate chunk errors; untracked rejects `NotTracked`, tracked returns the sample's values | Not Started | sonnet | Needed by Feature 3 |
| 4 | RED -> GREEN: `LiveSource` append, stale/connected transitions, injected `fetchCells` | AC 6, 7 (TS half), 9: arrays grow per update; stale keeps data; reconnect resumes append; gap marker | Not Started | sonnet | Needed by Features 2, 6 |
| 5 | RED -> GREEN: `TimeBrush` reducer (default full run, snap, clamp, follow-live) and `schemaText` fallback | AC 8, 10 (model half): default whole run; snap to labels; clamp; follows only at latest; missing schema entry falls back to id with `missing: true` | Not Started | sonnet | Open Q3 left inferred |
| 6 | RED -> GREEN (C++): `HistoryFileReader` validate + chunk | AC 4, 5 (C++ half): large file chunked; bad format, bad version, missing schema each rejected with a logged `[EDITOR] T1` error; chunk size decided (Open Q1) | Not Started | sonnet | Needs GoogleTests ref to CoWEditor (Ask in Task 0). `dia docs vcxproj-add CoWEditor` per file |
| 7 | C++ plugin `CoWWorldSimInspectorPlugin : LiveConnectionPluginBase`: `OnLivePluginLoad` registers the three topics and `openHistoryFile`/session handlers; `OnGameConnected` queries `valueschema.get` (and `cow.session.get`); `OnGameDisconnected` marks stale; log `[EDITOR] T1` | GoogleTests with a fake connection where practical: handlers registered, bad file leaves state unchanged; otherwise covered by E2E in Task 16 | Not Started | sonnet | Uses `FileDialogHandler::HandleOpenFileDialog`. `[EDITOR] T1` log lines for open/close, validation failure, schema query failure, disconnect/reconnect |
| 8 | Game-scoped registration: add plugin to `cow_editor.diaapp`, wire `RegisterCoWEditorPlugins()`, ensure the static registration survives linking | AC 1, 2: plugin exists with `cow.diagame`; absent and not creatable after loading `cluichetest.diagame` (copy `TestGameScopedPluginsIntegration` pattern) | Not Started | sonnet | AC 1 `CluicheEditor` build check via `dia pipeline --target cluicheeditor` |
| 9 | RED -> GREEN: zustand store (A/B slots, brush, connection, toast) and `historyBridge.ts` wrapper | AC 11: B stays null; actions drive model only; connection states map to pill state; missing-schema warning logged once | Not Started | sonnet | Needed by Features 2-7 |
| 10 | Bridge wiring: topic updates and file chunks flow C++ -> `LiveSource`/`FileSource`; stub live publisher (CluicheTest or editor-side stub) for the three topics | AC 6 end to end: stub drives growing arrays; failed file open leaves the current source unchanged and shows a toast | Not Started | sonnet | Stub lives with the tests, not in the runtime |
| 11 | Views: `ValueLabel`, `ValueTooltip`, `ConnectionPill` | AC 10 RTL: label/unit/description from fixture schema; fallback; pill texts incl. `connected · S-3.5 · Run` and plain `connected` | Not Started | sonnet | Visual check against `world-sim-inspector-v2.html` |
| 12 | Views: `Toolbar`, `SourcePicker` (Live / Open file, B disabled "Feature 5"), `BrushControl` (draggable range, snap labels, follow-live) | RTL AC 3, 7, 8, 11: picker opens a file via the bridge stub; brush drag snaps and clamps; stale pill and read-only state | Not Started | sonnet | Mockup shell: top bar |
| 13 | Shell: `WorkspaceRail` (six workspaces, placeholders) and `App` composition | RTL: rail switches workspaces without losing store state; one screenshot per shell state against the v2 mockup | Not Started | sonnet | Mockup shell: left rail. Gate for Features 2-12 |
| 14 | E2E: load `cow.diagame` with the stub publisher, live samples arrive; open fixture file; disconnect shows stale; load `cluichetest.diagame`, plugin gone | E2E list in spec Test Strategy | Not Started | sonnet | Run `dia pipeline --target cluichetest --stage compile-code` first (`dia test e2e` does not build). Kill leftover `CluicheTest.exe` |
| 15 | Gap audit and edge tests (no mutation checks): re-read ACs 1-11 against tests; add edges (empty file, 1-sample run, duplicate sample labels, NaN aggregates, unicode paths, dialog cancelled, reconnect during open, chunk size boundary) | Added tests fail first where they find a bug | Not Started | sonnet | Explicit task per the exhaustive-test rule |
| 16 | Full verification once: full Vitest, full `dia run googletest --shards 2`, E2E; `dia pipeline --target cluicheeditor` build | All pass; report pass/fail and failing names only | Not Started | sonnet | Verification cadence: only full run in this plan |
| 17 | Close-out: Observation Opportunity Scan (logs, traces, profiling, metrics, health), `dia docs spec-done` on the spec, update parent features table, backlog row to history | — | Not Started | haiku | Scan is suggestions only. Record Open Q1 and Q2 answers in the spec. Link this plan from the spec Status |

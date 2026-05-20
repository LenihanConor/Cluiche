# Implementation Plan: DiaApplicationFlowEditor

## Spec
@docs/specs/systems/dia/diaapplicationfloweditor.md

## Session Notes

### Spec Decisions Summary

The DiaApplicationFlowEditor is a CluicheEditor plugin at `Dia/DiaApplicationEditor/` providing visual editing of `.diaapp` v2 manifests and live runtime inspection. Key binding constraints:

- **PD-001**: StringCRC for all IDs (manifest stores strings, editor model stores CRC)
- **PD-004**: No STL in public C++ APIs (DiaCore containers)
- **PD-007**: C++20 required
- **PD-010**: .diagame is root; .diastage declares stage metadata; editor resolves from .diagame
- **AD-003**: Namespace `Dia::ApplicationFlow::Editor::`
- **ED-002**: Three-tab layout (Graph, Presence, Streams) + sidebar inspector
- **ED-007**: React + CEF frontend
- **ED-008**: Single `.tl` traffic-light dot primitive across all views
- **ED-013**: No validate button — always-on (500ms debounce)
- **ED-014**: Presence Grid has Offline and Live visual modes
- **SD-001**: Config is sole source of truth
- **SD-014**: Full validation at load (same rules as runtime)
- **SD-018**: Reserved `$`-prefix streams are read-only in editor

### Existing Codebase

There is an existing v1 DiaApplicationEditor with:
- C++ backend: `DiaApplicationEditor.h/.cpp`, `ManifestEditorData.h`, `ManifestSerializer.h/.cpp`
- React UI (Vite + Vitest): `Dia/DiaApplicationEditor/UI/src/` — TreeView, FlowView, FormView, UndoStore, ValidationPanel, ManifestStore, ModuleInspector, PUInspector, etc.
- GoogleTest: `Cluiche/Tests/GoogleTests/DiaApplicationEditor/TestManifestSerializer.cpp`
- IEditorPlugin integration with DiaEditor framework, FileWatcher, WebUIBridge

The v2 work replaces the Phase-based model with Stage-based model. The IEditorPlugin interface, CEF bridge pattern, file watcher, and React toolchain remain. Internal data model, serialization, validation rules, and UI components are rewritten.

### Observability Strategy

All C++ backend code integrates with DiaObservation (when available) and DiaMetrics:
- **Logging**: `DIA_LOG_INFO/WARNING/ERROR(Editor, ...)` for load/save/validate/connect events
- **Metrics**: Register gauges/counters: `dia.editor.manifest.load_ms`, `dia.editor.manifest.save_ms`, `dia.editor.validation.error_count`, `dia.editor.validation.warning_count`, `dia.editor.live.connection_state`, `dia.editor.commands.total`
- **Traces**: `DIA_TRACE_ZONE("ManifestLoad", kEditor)` around load/save/validate paths for performance visibility
- **Health**: `IHealthReporter` reporting editor state (loaded file, dirty state, connection state)

### Testing Strategy

**C++ unit tests** (GoogleTest):
- `TestManifestDocument` — model construction, defaults, serialization round-trip
- `TestManifestLoader` — load valid, load malformed, load wrong version, load locked file
- `TestManifestSaver` — save creates .bak, atomic write, blocks on errors, canonical output
- `TestManifestValidator` — one test per validation rule (12 rules × pass/fail = 24+ tests)
- `TestCommandHistory` — execute/undo/redo, stack overflow, clear, save-point
- `TestCommands` — each command class (execute + undo correctness)
- `TestRiskAssessor` — each risk condition detected / not detected
- `TestTypeDiscoveryService` — parse types.json, lookup, unknown type
- `TestFileConflict` — external change detection, suppression during save
- `TestLiveStateStore` — update/clear/query

**React unit tests** (Vitest):
- `ManifestStore.test.ts` — state management, dirty tracking, load/save actions
- `UndoStore.test.ts` — command history, save-point, 100-depth cap
- `ValidationBar.test.tsx` — render counts, expand/collapse, click-to-navigate
- `GraphView.test.tsx` — node rendering, edge rendering, selection, ghost node
- `ModulePresenceGrid.test.tsx` — matrix rendering, "all" badge, PU grouping, virtual scroll
- `PUInspector.test.tsx` — property editing, module cards
- `ModuleInspector.test.tsx` — deps/streams editing, provenance display
- `StreamInspector.test.tsx` — list rendering, detail sidebar, $-prefix protection
- `StageConfiguration.test.tsx` — add/remove/rename, trigger toggle
- `LiveConnectionButton.test.tsx` — 3-state rendering
- `LiveTransitionPanel.test.tsx` — stage selector, trigger, feedback
- `RiskyChangeDialog.test.tsx` — warning display, proceed/cancel

**Integration tests**:
- CEF message round-trip (load→render→edit→save cycle)
- Full pipeline: `dia pipeline --target CluicheEditor` (build succeeds)
- Launch: `dia run cluicheeditor` (no crash, plugin loads)

**E2E test scenario** (future, via DiaTestHarness when ready):
- Open manifest → edit module → undo → save → verify file content

---

## Implementation Patterns

### Phase 1: Data Model & Core Logic (C++ backend)

**Pattern**: Pure C++ classes in `Dia::ApplicationFlow::Editor::` namespace. No UI coupling. All classes testable via GoogleTest in isolation.

- `ManifestDocument` struct (DynamicArrayC-based)
- `ManifestLoader` class (jsoncpp → ManifestDocument, returns SerializeResult-style errors)
- `ManifestSaver` class (ManifestDocument → canonical JSON, atomic write)
- `ManifestValidator` class (returns ValidationResult with issue list)
- `ICommand` interface + `CommandHistory` + `CompoundCommand`
- Concrete command classes (one per edit operation, ~15 classes)
- `RiskAssessor` class
- `TypeDiscoveryService` class
- `LiveStateStore` class

### Phase 2: Plugin Shell & CEF Bridge

**Pattern**: `DiaApplicationFlowEditorPlugin` implements `IEditorPlugin`. Owns `ManifestDocument`, `CommandHistory`, `ManifestValidator`, `TypeDiscoveryService`, `LiveStateStore`. Exposes operations to React via `WebUIBridge` message handlers.

- Registers message handlers: `manifest.load`, `manifest.save`, `manifest.getState`, `history.undo/redo/getState`, `validation.run`, `types.get`, `live.connect/disconnect/getStatus`, `live.transitionTo`, `live.shutdown`
- FileWatcher integration (ReadDirectoryChangesW)
- GameConnectionManager integration for live mode

### Phase 3: React UI (staged by tab)

**Pattern**: React + TypeScript + Vite. Zustand stores (ManifestStore, UndoStore, LiveStore, ValidationStore). Components follow existing patterns in `UI/src/`.

- Shared: TrafficLightDot component, tab layout shell, sidebar panel container
- Tab 1: GraphView (SVG nodes/edges, drag, selection)
- Tab 2: ModulePresenceGrid (virtual-scrolled matrix)
- Tab 3: StreamsTab (table + detail sidebar)
- Sidebar: PUInspector, ModuleInspector, StageConfiguration
- Footer: ValidationBar
- Header: LiveConnectionButton, LiveTransitionPanel
- Dialogs: AddPU, AddStream, AddStage, RiskyChange, FileConflict, SaveConfirmation

### Phase 4: Live Mode Integration

**Pattern**: WebSocket subscription via GameConnectionManager. Push-based updates from DiaDebugServer. LiveStateStore updated on message, propagated to React via bridge events.

- Subscribe to `app.state`, `app.modules`, `app.streams` topics
- Update LiveStateStore per message
- Bridge events trigger React re-renders (throttled 10 Hz)
- TransitionTo / Shutdown commands dispatched via same WebSocket

### Phase 5: Observability & Polish

**Pattern**: Add DIA_LOG, DIA_TRACE_ZONE, metric registration after functional code is complete. Register IHealthReporter. Final exhaustive test pass.

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| **Phase 1 — Data Model & Core Logic** | | | | | |
| 1 | Define `ManifestEditorState` and provenance structs in `Dia/DiaApplicationEditor/V2/` | `TestManifestEditorState`: 11 tests — all pass | Done | sonnet | `V2/ManifestEditorState.h/.cpp` |
| 2 | Implement `ManifestLoader::Load(path)` — JSON parse, version check, populate model | `TestManifestLoader`: 5 tests — all pass | Done | sonnet | `V2/ManifestLoader.h/.cpp` |
| 3 | Implement `ManifestSaver::Save(doc, path)` — canonical JSON, .bak, atomic write | `TestManifestSaver`: 4 tests — all pass | Done | sonnet | `V2/ManifestSaver.h/.cpp` |
| 4 | Implement `ManifestValidator` with all 12 rules | `TestManifestValidator`: 24 tests — all pass | Done | sonnet | `V2/ManifestValidator.h/.cpp` |
| 5 | Define `ICommand` interface, `CommandHistory`, `CompoundCommand` | `TestCommandHistory`: 12 tests — all pass | Done | sonnet | `V2/Commands/ICommand.h`, `CommandHistory.h/.cpp`, `CompoundCommand.h/.cpp` |
| 6 | Implement concrete commands: AddPU, RemovePU, SetPUFrequency, SetPUThread, ReorderPU | `TestCommands_PU`: 10 tests — all pass | Done | sonnet | `V2/Commands/PUCommands.h/.cpp` |
| 7 | Implement concrete commands: AddModule, RemoveModule, AddModuleDep, RemoveModuleDep, SetModuleStages, SetModuleStartTimeout, SetModuleStopTimeout | `TestCommands_Module`: 12 tests — all pass | Done | sonnet | `V2/Commands/ModuleCommands.h/.cpp` |
| 8 | Implement concrete commands: AddStream, RemoveStream, SetStreamType, SetStreamPayload, SetStreamFromPU, SetStreamToPU, SetStreamCapacity, SetStreamMaxReaders | `TestCommands_Stream`: 8 tests — all pass | Done | sonnet | `V2/Commands/StreamCommands.h/.cpp` |
| 9 | Implement concrete commands: AddStage, RemoveStage, RenameStage, SetStageTrigger, SetInitialStage, ReorderStages | `TestCommands_Stage`: 10 tests — all pass | Done | sonnet | `V2/Commands/StageCommands.h/.cpp` |
| 10 | Implement `RiskAssessor` | `TestRiskAssessor`: 13 tests — all pass | Done | sonnet | `V2/RiskAssessor.h/.cpp`; uses dynamic_cast (RTTI enabled) |
| 11 | Implement `TypeDiscoveryService` (file-based) | `TestTypeDiscoveryService`: 11 tests — all pass | Done | sonnet | `V2/TypeDiscoveryService.h/.cpp` |
| 12 | Implement `LiveStateStore` | `TestLiveStateStore`: 12 tests — all pass | Done | sonnet | `V2/LiveStateStore.h/.cpp` |
| 13 | Wire all Phase 1 code into `DiaApplicationEditor.vcxproj` + filters | Build passes: `dia pipeline --target googletest` ✓ | Done | haiku | Updated again for Batch 2 |
| 14 | Wire all GoogleTests into `GoogleTests.vcxproj` + filters | 4751 tests pass: `dia run googletest` ✓ | Done | haiku | Updated again for Batch 2 |
| **Phase 2 — Plugin Shell & CEF Bridge** | | | | | |
| 15 | Create `DiaApplicationFlowEditorPlugin` class implementing IEditorPlugin | Build succeeds; plugin registered via REGISTER_EDITOR_PLUGIN | Done | sonnet | `DiaApplicationFlowEditorPlugin.h/.cpp` |
| 16 | Implement CEF message handlers: manifest.load, manifest.save, manifest.getState | Build passes; handlers return structured JSON | Done | sonnet | Includes BuildManifestStateJson helper |
| 17 | Implement CEF message handlers: history.undo, history.redo, history.getState | Build passes | Done | sonnet | |
| 18 | Implement CEF message handlers: validation.run + onValidationComplete event | Build passes | Done | sonnet | Also pushes NotifyUIDataChanged("validation.result") |
| 19 | Implement CEF message handlers: types.get, types.refresh | Build passes | Done | sonnet | |
| 20 | Implement FileWatcher integration + conflict detection + save suppression | Build passes | Done | sonnet | mSuppressFileWatchDuringSave flag wired in save handler |
| 21 | Implement CEF message handlers: risk.check, risk.confirm | Build passes | Done | sonnet | risk.check checks by commandType name when live |
| 22 | Implement live connection: register with GameConnectionManager, connect/disconnect lifecycle | Build passes | Done | sonnet | SetConnectionCallback wires connect/disconnect lifecycle |
| 23 | Implement live state subscriptions: app.state, app.modules, app.streams topics → LiveStateStore | Build passes | Done | sonnet | Subscribe/Unsubscribe wired in connection callback |
| 24 | Implement live commands: transitionTo, shutdown dispatch | Build passes | Done | sonnet | SendCommandWithResponse for transition, SendCommand for shutdown |
| **Phase 3 — React UI** | | | | | |
| 25 | Scaffold new UI shell: Vite project, tab layout (Graph/Presence/Streams), sidebar container, header | Manual: `dia run cluicheeditor` shows 3-tab layout with empty content | Todo | sonnet | After Phase 2 |
| 26 | Implement shared `TrafficLightDot` component | `TrafficLightDot.test.tsx`: renders all states (grey/amber/green/red ± pulse) | Todo | sonnet | |
| 27 | Implement `ManifestStore` (Zustand) + bridge integration | `ManifestStore.test.ts`: load/save/dirty tracking/model access | Todo | sonnet | |
| 28 | Implement `UndoStore` (Zustand, 100-cap, save-point) | `UndoStore.test.ts`: push/undo/redo/cap/save-point/jumpTo | Todo | sonnet | |
| 29 | Implement `ValidationStore` + `ValidationBar` footer | `ValidationBar.test.tsx`: counts render, expand shows issues, click navigates | Todo | sonnet | |
| 30 | Implement `GraphView`: PU nodes, stream edges, auto-layout | `GraphView.test.tsx`: renders N nodes + M edges from model | Todo | opus | Complex SVG + layout |
| 31 | Implement `GraphView`: click-select, drag-reposition | `GraphView.test.tsx`: selection updates, drag repositions | Todo | sonnet | Depends on #30 |
| 32 | Implement `GraphView`: ghost node (Add PU), stream label click → Streams tab | `GraphView.test.tsx`: ghost renders, click creates PU; label click navigates | Todo | sonnet | Depends on #30 |
| 33 | Implement `ModulePresenceGrid`: matrix, PU grouping, "all" badge, virtual scroll | `ModulePresenceGrid.test.tsx`: renders correct cells, badges, scrolls | Todo | opus | Complex grid + virtual scroll |
| 34 | Implement `ModulePresenceGrid`: live mode (ED-014) | `ModulePresenceGrid.test.tsx`: active column dots change in live | Todo | sonnet | Depends on #33 |
| 35 | Implement `StreamsTab`: table + `StreamDetailInspector` sidebar | `StreamsTab.test.tsx`: rows render, selection shows detail, $-prefix read-only | Todo | sonnet | |
| 36 | Implement `PUInspector`: properties, module cards, dep order section | `PUInspector.test.tsx`: fields editable, cards render, dep section collapses | Todo | sonnet | |
| 37 | Implement `ModuleInspector`: deps, streams, timeouts, provenance, stage dots | `ModuleInspector.test.tsx`: all sections render, add/remove deps/streams | Todo | sonnet | |
| 38 | Implement `StageConfiguration` panel | `StageConfiguration.test.tsx`: add/remove/rename/trigger/initial/reorder | Todo | sonnet | |
| 39 | Implement `RiskyChangeDialog` modal | `RiskyChangeDialog.test.tsx`: shows warning, proceed/cancel work | Todo | sonnet | |
| 40 | Implement `FileConflictDialog` modal | `FileConflictDialog.test.tsx`: reload/keep buttons | Todo | sonnet | |
| 41 | Implement `LiveConnectionButton` (3-state header widget) | `LiveConnectionButton.test.tsx`: 3 states render correctly | Todo | sonnet | |
| 42 | Implement `LiveTransitionPanel`: stage selector, trigger, feedback | `LiveTransitionPanel.test.tsx`: dropdown populated, button triggers, feedback shows | Todo | sonnet | |
| 43 | Implement `LiveStore` (Zustand): connection state, module states, stream throughput | `LiveStore.test.ts`: connect/disconnect/update/clear | Todo | sonnet | |
| **Phase 4 — Live Mode Integration** | | | | | |
| 44 | Wire LiveStore ↔ C++ LiveStateStore via bridge events | Integration: push from C++ → React store updates → dots animate | Todo | sonnet | Depends on #23, #43 |
| 45 | Wire GraphView live dots from LiveStore | Manual: PU dots show green when connected to running game | Todo | sonnet | Depends on #30, #44 |
| 46 | Wire Presence Grid live mode from LiveStore | Manual: active-stage column shows runtime dots | Todo | sonnet | Depends on #33, #44 |
| 47 | Wire Stream throughput from LiveStore into StreamDetailInspector | Manual: msg/sec shows when live | Todo | sonnet | Depends on #35, #44 |
| 48 | Wire transition trigger: React button → C++ → WebSocket → feedback | Manual: trigger transition, see dots animate, toast on complete | Todo | sonnet | Depends on #42, #24 |
| **Phase 5 — Observability & Polish** | | | | | |
| 49 | Add DIA_LOG calls to load/save/validate/connect/command paths | Grep: all major code paths have logging | Todo | haiku | After Phase 4 |
| 50 | Register DiaMetrics gauges/counters for editor (load_ms, save_ms, validation counts, connection_state, command_count) | `TestEditorMetrics`: metrics registered, incremented on operations | Todo | sonnet | After Phase 4 |
| 51 | Add DIA_TRACE_ZONE to ManifestLoader::Load, ManifestSaver::Save, ManifestValidator::Validate | Grep: trace zones present; verify in `trace.jsonl` output during run | Todo | haiku | After Phase 4 |
| 52 | Implement IHealthReporter for editor (loaded file, dirty state, live connection state) | `TestEditorHealth`: reporter returns correct state | Todo | sonnet | After Phase 4 |
| 53 | DiaCLI `dia types export` command | Integration: generates valid types.json from game build | Todo | sonnet | Can parallel with Phase 3 |
| 54 | Update `dia.applicationeditor.architecture.module.md` YAML frontmatter | Doc review: frontmatter matches new code | Todo | haiku | Last |
| 55 | Full exhaustive test run: `dia run googletest --filter="AppFlowEditor*"` + `npm test` in UI/ | All C++ tests pass, all Vitest tests pass, zero warnings | Todo | sonnet | Final gate |
| 56 | Launch test: `dia run cluicheeditor` → open manifest → edit → undo → save → verify file | Manual E2E: golden path works | Todo | opus | Final gate |

---

## Parallelism Opportunities

- **Phase 1 tasks #1-#12** can partially parallel: #1 must be first; then #2/#3/#4/#5/#11/#12 are independent; #6-#10 depend on #1+#5.
- **Phase 3 tasks #25-#43**: after scaffold (#25-#28), tabs can parallel — GraphView (#30-32), PresenceGrid (#33-34), StreamsTab (#35), inspectors (#36-38) are independent.
- **Task #53** (DiaCLI types export) is independent of all UI/backend work.
- **Phase 5** (#49-#52) can fully parallel since they touch different concerns.

## Risk Areas

- **GraphView layout/interaction** (Tasks #30-32) — SVG rendering with drag + selection is the most complex frontend work. Assigned opus for initial layout.
- **Virtual scroll in Presence Grid** (Task #33) — Custom or library (react-window). Performance matters.
- **Live mode WebSocket subscription** (Tasks #22-24, #44-48) — Depends on DiaDebugServer supporting the subscription topics. May need server-side additions.
- **FileWatcher reliability** (Task #20) — ReadDirectoryChangesW can miss events on network drives. Focus-check fallback mitigates.

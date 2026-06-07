# Implementation Plan: DiaApplicationFlowInspector + Editor Split

**Spec:** @docs/specs/systems/dia/diaapplicationflowinspector.md
**Status:** In Progress

## Scope

This plan covers:
1. Extracting live features from `Dia/DiaApplicationEditor/` into a new `Dia/DiaApplicationFlowInspector/` project
2. Trimming the Editor to remove live handlers (rename to `Dia/DiaApplicationFlowEditor/`)
3. Implementing new Inspector features (timeline, lifecycle cards, backpressure, frame budget, event log)
4. Adding ~25 lines of framework telemetry to stream runtime + PU runtime
5. Exhaustive test coverage (C++ unit, Vitest component/store, integration)
6. Observability instrumentation (logs, traces, metrics, health)
7. Solution/pipeline integration

## Test Strategy

### C++ (GoogleTest)

New test directory: `Cluiche/Tests/GoogleTests/DiaApplicationFlowInspector/`

| Test File | Covers | Pattern |
|-----------|--------|---------|
| `TestLiveStateStore.cpp` | LiveStateStore (moved from Editor) | Unit: state transitions, array bounds, Clear() |
| `TestInspectorPlugin.cpp` | Plugin lifecycle, handler registration | Integration: `WebUIBridge(nullptr)` + `InvokeRequestHandler` |
| `TestStreamTelemetry.cpp` | `currentSize` + `dropsTotal` broadcast | Unit: send N events, assert telemetry payload |
| `TestPUTiming.cpp` | `lastTickMs` in PU telemetry | Unit: call Update(), assert timing field present and >0 |
| `TestInspectorStore.cpp` | Timeline accumulation, ring buffer bounds | Unit: push 1024+ entries, assert oldest dropped |

**Filter:** `dia run googletest --filter="DiaApplicationFlowInspector*"` and `dia run googletest --filter="StreamTelemetry*"` and `dia run googletest --filter="PUTiming*"`

### TypeScript (Vitest)

New test location: `Dia/DiaApplicationFlowInspector/UI/src/**/*.test.ts(x)`

| Test File | Covers |
|-----------|--------|
| `useInspectorStore.test.ts` | Timeline accumulation, module lifecycle tracking, ring buffer 1024 cap, event severity filtering |
| `useLiveStoreV2.test.ts` | Connect/disconnect lifecycle, state setters (moved from Editor) |
| `StageBreadcrumb.test.tsx` | Renders trail, handles 0/1/N stages, duration formatting |
| `ModuleLifecycleCard.test.tsx` | All 4 states (Running/Loading/Stopped/Failed), timeout bar %, dep blocking text |
| `StreamBackpressureRow.test.tsx` | Fill % bar width, drops badge red when >0, color thresholds (ok/warn/crit) |
| `PUFrameBudgetGauge.test.tsx` | Budget %, over-budget label + color, Hz label |
| `EventLog.test.tsx` | Renders entries, Copy button copies to clipboard, severity filter, ring buffer overflow |
| `AppInspector.test.tsx` | Tab switching, empty state when disconnected, footer summary counts |

**Run:** `cd Dia/DiaApplicationFlowInspector/UI && npm run test`

### Integration / E2E

| Scenario | How Tested |
|----------|------------|
| Editor opens manifest after split | `dia pipeline --target cluicheeditor` + manual load |
| Inspector docks + shows empty state | Manual: launch editor, verify Inspector panel |
| Inspector connects to CluicheTest | Manual: `dia run cluichetest`, connect Inspector, verify modules tab |
| Editor receives pushed `live.state` topic | `IntegrationTestApplicationFlowEditorPlugin.cpp`: simulate topic push, assert store updated |
| Stream backpressure visible in Inspector | Manual: overflow a stream in CluicheTest, verify bar + drops badge |
| PU over-budget shown in Timing tab | Manual: add artificial delay in SimPU module, verify gauge |

## Observability Strategy

### Logs (DIA_LOG_*)

| Location | Level | Message | Purpose |
|----------|-------|---------|---------|
| Inspector `HandleLiveConnect` | INFO | `"inspector.connect host=%s port=%d"` | Connection lifecycle audit |
| Inspector `HandleLiveDisconnect` | INFO | `"inspector.disconnect"` | Connection lifecycle audit |
| Inspector `HandleLiveTransitionTo` | INFO | `"inspector.transition_to stage=%s"` | Command audit |
| Inspector `HandleLiveShutdown` | WARNING | `"inspector.shutdown_command"` | Destructive command audit |
| Stream overflow path (existing) | WARNING | Already logged | Unchanged |
| PU over-budget detection | WARNING | `"pu.over_budget id=%s tick_ms=%.1f target_ms=%.1f"` | Performance alert |

### Traces (DIA_TRACE_ZONE)

| Location | Zone Name | Category |
|----------|-----------|----------|
| `InspectorPlugin::OnUpdate()` | `"inspector.update"` | `kDiaEditor` |
| `HandleLiveConnect` | `"inspector.connect"` | `kDiaEditor` |

### Metrics (MetricRegistry)

| Metric | Kind | Location | Purpose |
|--------|------|----------|---------|
| `stream.{id}.current_size` | Gauge | EventStreamStore post-send | Queue depth for backpressure display |
| `stream.{id}.drops_total` | Counter | EventStreamStore overflow path | Cumulative drops per stream |
| `pu.{id}.last_tick_ms` | Gauge | ProcessingUnit::Update() | Frame budget computation |
| `inspector.events_total` | Counter | InspectorPlugin topic callbacks | Total telemetry events received |

### Health (IHealthReporter)

| Reporter | States | Location |
|----------|--------|----------|
| `InspectorHealthReporter` | Healthy (connected + receiving), Degraded (connected + no data for >5s), Unhealthy (disconnected) | `DiaApplicationFlowInspectorPlugin.cpp` |

---

## Phase 1 — Create DiaApplicationFlowInspector (extraction)

Extract live code from the Editor into the new Inspector project. No new features yet — just the split.

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `Dia/DiaApplicationFlowInspector/` directory, vcxproj (StaticLibrary), filters | `msbuild DiaApplicationFlowInspector.vcxproj` succeeds | Done | haiku | GUID {C1D2E3F4-A5B6-7890-CDEF-012345678901}. Deps: DiaEditor, DiaCore, DiaJson, DiaObservation. Build 0 errors. |
| 2 | Create `DiaApplicationFlowInspectorPlugin.h/.cpp` — extract 5 `HandleLive*` methods + registration from `DiaApplicationFlowEditorPlugin.cpp` (lines 1017–1182, 239–243, 278–282) | `TestInspectorPlugin`: OnLoad registers 5 handlers; Invoke each returns non-null | Done | sonnet | Namespace Dia::Editor, kDockable, 5 handlers registered. LiveStateStore still from DiaApplicationEditor (Task 3 moves it). |
| 3 | Move `V2/LiveStateStore.h/.cpp` → `DiaApplicationFlowInspector/LiveStateStore.h/.cpp`; change namespace | `TestLiveStateStore` passes at new location; `dia run googletest --filter="LiveStateStore*"` | Done | sonnet | Copied (not deleted) — namespace Dia::ApplicationFlow::Editor kept. Original Editor files untouched until Task 14. |
| 4 | Create `Cluiche/Tests/GoogleTests/DiaApplicationFlowInspector/` — move `TestLiveStateStore.cpp` from Editor test dir, add `TestInspectorPlugin.cpp` (integration pattern: WebUIBridge(nullptr) + InvokeRequestHandler) | `dia run googletest --filter="DiaApplicationFlowInspector*"` — all pass | Done | sonnet | 25 tests pass. InspectorLiveStateStore + InspectorPlugin suites. DiaApplicationFlowInspector.lib added to GoogleTests linker. |
| 5 | Add Inspector to `Cluiche.sln` (Editors solution folder) + add GoogleTest dependency | Solution builds end-to-end | Done | haiku | GUID {C1D2E3F4} in Editors folder {D521ECEE}. All 25 Inspector tests pass. |
| 6 | Create Inspector UI scaffold: `package.json` (react, zustand, vitest), `vite.config.ts`, `src/bridge.ts`, `src/AppInspector.tsx` (empty shell), `src/test/setup.ts` | `npm run build` produces `dist/index.html`; `npm run test` passes (0 tests) | Done | sonnet | react+zustand only (no d3/reactflow/codemirror). Build ✓, 0 tests pass ✓. |
| 7 | Move `useLiveStoreV2.ts` (full: connect/disconnect actions) + `useLiveStoreV2.test.ts` to Inspector UI | `npm run test` in Inspector — store tests pass | Done | sonnet | Copied with full connect/disconnect actions. 6 tests pass. |
| 8 | Move `LiveTransitionPanel.tsx` + test, `LiveConnectionButton.tsx` + test to Inspector UI | Component tests pass | Done | sonnet | LiveTransitionPanel + LiveConnectionButton + tests. 11 tests pass. |
| 9 | Copy `TrafficLightDot.tsx` + test to Inspector UI (DAFI-006: duplicate, not shared) | `TrafficLightDot.test.tsx` passes | Done | haiku | 36 lines; 36-line component duplicated per DAFI-006. 6 tests pass. |
| 10 | Wire `AppInspector.tsx`: render LiveConnectionButton (header), LiveTransitionPanel (section), placeholder tabs | `AppInspector.test.tsx`: renders empty state when disconnected, renders transition panel when connected | Done | sonnet | AppInspector shows empty-state/connected-content sections. 26/26 UI tests pass. |
| 11 | Add observability to Inspector plugin: `DIA_TRACE_ZONE` in OnUpdate, `DIA_LOG_INFO` in HandleLiveConnect/Disconnect, InspectorHealthReporter | `TestInspectorPlugin`: health reports Unhealthy when disconnected | Done | sonnet | DIA_TRACE_ZONE in OnUpdate+HandleLiveConnect, audit logs, InspectorHealthReporter. 25/25 tests pass. |

## Phase 2 — Trim Editor

Remove live handlers from the Editor. Rename project directory.

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 12 | Remove `HandleLive*` methods (5) + `kReqLive*` constants + topic subscriptions from `DiaApplicationFlowEditorPlugin.cpp` | `dia run googletest --filter="DiaApplicationEditor*"` — all pass; no `live.*` handler registered | Done | sonnet | ~160 lines removed; C:\Users\160 lines removed. 24/24 C++ tests pass. |
| 13 | Remove `mLiveStore` member, `#include LiveStateStore.h`, `mGameConnection->Update()` from Editor plugin | Build succeeds | Done | haiku | Keep `mGameConnection` pointer (risk check via `IsConnected()`); Build succeeds. Keep mGameConnection (risk check via IsConnected()). |
| 14 | Remove `V2/LiveStateStore.h/.cpp` from Editor vcxproj + filters; delete source files from Editor dir | Build succeeds; no orphan files | Done | haiku | V2/LiveStateStore deleted. vcxproj+filters updated. Module doc updated. |
| 15 | Trim Editor UI `useLiveStoreV2.ts` → read-only subscriber (remove `connect()`/`disconnect()` actions, keep `set*` setters for pushed topics) | `useLiveStoreV2.test.ts` (Editor): setters work; no bridge calls made | Done | sonnet | connect/disconnect removed. 156/156 UI tests pass. |
| 16 | Remove `LiveTransitionPanel.tsx`, `LiveConnectionButton.tsx` from Editor UI; remove imports from `AppV2.tsx` | `npm run test` in Editor UI — all pass; no dead imports | Done | haiku | Files deleted. AppV2 imports cleaned. |
| 17 | Add `ConnectionStatusDot.tsx` to Editor UI — reads `useLiveStoreV2.connectionState`, renders TrafficLightDot (grey=offline, green+pulse=live). Place in AppV2 header. | `ConnectionStatusDot.test.tsx`: grey when disconnected; green+pulse when connected; no click handler | Done | sonnet | AC: connection-status-indicator.md. Replaces the old LiveConnectionButton.; ConnectionStatusDot.tsx created. Green+pulse when connected, grey offline. 156/156 UI tests pass. |
| 18 | Add Editor-side topic subscription: `WebUIBridge::SubscribeToTopic("live.connectionStatus")` → forward to UI `useLiveStoreV2.setConnectionState` | `IntegrationTestApplicationFlowEditorPlugin`: simulate topic push → store updated; no topic → store stays disconnected | Done | sonnet | AC: live-state-overlay.md #9, #10. Editor is passive consumer.; live.connectionStatus case added to AppV2.tsx dispatch switch. |
| 19 | Verify `RiskAssessor` uses `GameConnectionManager::IsConnected()` (not LiveStateStore) for severity escalation | `TestRiskAssessor`: mock GameConnectionManager.IsConnected()=true → warning includes "affects running game"; false → structural warning only | Done | sonnet | AC: risky-change-warnings.md #7, #8. No Inspector dependency.; HandleRiskCheck uses mGameConnection->IsConnected() at line 940. No LiveStateStore dependency. |
| 20 | Verify Editor renders offline correctly when no `live.connectionStatus` topic received (Inspector not loaded) | `AppV2.test.tsx`: on mount with no topics → ConnectionStatusDot is grey; ModulePresenceGrid shows offline mode; no console errors | Done | sonnet | AC: live-state-overlay.md #10, connection-status-indicator.md #4.; 156/156 Editor UI tests pass with no Inspector loaded. |
| 21 | Verify immediate transition: push `live.connectionStatus {connected: false}` → all overlays revert same frame | `useLiveStoreV2.test.ts`: setConnectionState('disconnected') → activeStage=null, modules=[], streams=[]; `ModulePresenceGrid.test.tsx`: connectionState=disconnected → offline mode | Done | sonnet | AC: live-state-overlay.md #11, connection-status-indicator.md #5.; useLiveStoreV2 clearLiveState verified. 156/156 Editor UI tests pass. |
| 22 | Rename `Dia/DiaApplicationEditor/` → `Dia/DiaApplicationFlowEditor/`; update vcxproj name, ProjectGuid, include paths, UI dist path, post-build copy target | `msbuild Cluiche.sln` succeeds | Done | sonnet | Highest breakage risk — touch sln, dependent vcxproj refs, pipeline targets; 97 files renamed. Build succeeds. 1524/1527 tests pass (3 pre-existing Python failures). |
| 23 | Update external references: CluicheEditor `.diaapp` manifest, DiaCLI pipeline config, `dia.applicationeditor.architecture.module.md` → rename | `dia pipeline --target cluicheeditor` passes end-to-end | Done | sonnet | DiaCLI already updated by Task 22 agent. editor.diaapp uses REGISTER_EDITOR_PLUGIN, no manifest change needed. |
| 24 | Regression: run full Editor GoogleTest suite + Editor UI tests | `dia run googletest --filter="DiaApplicationEditor*"` pass; `npm run test` pass | Done | haiku | Gate before proceeding; 1524/1527 GoogleTests pass (3 pre-existing Python). 156/156 Editor UI tests pass. |

## Phase 3 — Inspector UI: Core Views

Build the Inspector's rich debugging UI (from mockup). All components test-first.

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 25 | `useInspectorStore.ts` (Zustand) — timeline ring buffer (1024, DAFI-010), module state map (64), stream state map (16), event log (1024), severity filter | `useInspectorStore.test.ts`: push 1025 stages → oldest dropped; module state update/remove; event severity filter; clear on disconnect | Done | sonnet | Central store; all components read from this; 10/10 tests pass. Ring buffers: 1024 timeline, 1024 eventLog, 64 modules, 16 streams. |
| 26 | `StageBreadcrumb.tsx` — horizontal trail, past stages grey, current green, durations between | `StageBreadcrumb.test.tsx`: 0 stages → empty; 1 stage → current only; 3 stages → arrows + durations; overflow scrolls | Done | sonnet | Reads `useInspectorStore.timeline`; 5/5 tests pass. Horizontal trail, past grey, current green, duration labels, overflow scroll. |
| 27 | `ModuleLifecycleCard.tsx` — name + PU badge, state badge (Running/Loading/Stopped/Failed), time-in-state, timeout bar (% with color), dep-blocked warning, error message | `ModuleLifecycleCard.test.tsx`: renders each state; timeout bar width matches %; dep-blocked shows dependency name; Failed shows error msg | Done | opus | Most complex component — cross-references deps from manifest with live states; 10/10 tests pass. All 4 states, timeout bar %, dep-blocked, error message. |
| 28 | `StreamBackpressureRow.tsx` — name, msg/s + KB/s stats, queue fill bar (green <60%, amber 60-85%, red >85%), drops badge | `StreamBackpressureRow.test.tsx`: bar width = fill%; color thresholds correct; drops badge red and bold when >0; 0 drops → "0 drops" (no badge) | Done | sonnet | 7/7 tests pass. Fill bar %, green/amber/red thresholds, drops badge. |
| 29 | `PUFrameBudgetGauge.tsx` — PU name, timing (ms/target), budget bar (green/amber/red gradient), Hz label, % label | `PUFrameBudgetGauge.test.tsx`: 49% → green + "49% budget used"; 85% → amber; 114% → red + "OVER BUDGET"; Hz from target period | Done | sonnet | 9/9 tests pass. Budget %, Hz label, OVER BUDGET label, color thresholds. |
| 30 | `EventLog.tsx` — monospace, color-coded (info/warn/error/transition), selectable text, Copy button, severity filter dropdown, auto-scroll to bottom | `EventLog.test.tsx`: renders entries; Copy calls clipboard API; filter hides info; auto-scroll on new entry; manual scroll-up disables auto-scroll | Done | sonnet | `user-select: text`; ring buffer from store; 5/5 tests pass. Entries, severity filter, Copy, data-severity attribute. |
| 31 | Wire `AppInspector.tsx` — 4 tabs (Modules, Streams, Timing, Log), footer summary (counts + Shutdown button), breadcrumb below header | `AppInspector.test.tsx`: tab switch renders correct content; footer shows "N modules · M blocked · K failed"; Shutdown calls `live.shutdown` | Done | sonnet | Compose all components; 76/76 Inspector UI tests pass. 4 tabs, footer counts, shutdown button, topic dispatch. |

## Phase 4 — Framework Telemetry

Always-on measurement (DAFI-008). Broadcast only when DebugServer has subscribers. Zero module-author work.

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 32 | Register `stream.{id}.current_size` gauge per EventStreamStore; set after every `SendInternal()` call | `TestStreamTelemetry`: create stream cap=8, send 5 → gauge reads 5; consume 3 → gauge reads 2 | Not Started | sonnet | Add to `EventStreamStore` constructor (register) + `SendInternal` (set). ~5 lines. Uses existing `MetricRegistry::Instance()`. |
| 33 | Add `mDropsTotal` counter to `EventStreamStore`; increment on each overflow (all policies); register as `stream.{id}.drops_total` | `TestStreamDrops`: send cap+1 with DropOldest → counter=1; send cap+2 → counter=2; no overflow → counter=0 | Not Started | sonnet | In overflow switch cases (3 locations). ~5 lines. |
| 34 | Wrap `ProcessingUnit::Update()` body with QPC; store result in `mLastTickMs` (float); register `pu.{id}.last_tick_ms` gauge; set after tick completes | `TestPUTiming`: create PU, call Update(0.016f), assert `mLastTickMs > 0.0f` and gauge value matches | Not Started | sonnet | ~10 lines. Existing `DIA_PROFILE_SCOPE` already times this; QPC adds authoritative gauge for broadcast. Use `Dia::Core::HighResTimer` if available, else raw QPC. |
| 35 | Add `DIA_LOG_WARNING` in PU::Update when `mLastTickMs > targetPeriodMs` (over-budget) | `TestPUTiming`: set frequency to 1000Hz (1ms budget), Update with artificial 5ms work → log emitted | Not Started | haiku | Single line: `DIA_LOG_WARNING("pu", "pu.over_budget id=%s tick_ms=%.1f target_ms=%.1f", ...)` |
| 36 | Include `currentSize`, `dropsTotal`, `lastTickMs` in existing `ObservationBridge::OnSnapshot()` broadcast path (metric snapshots already broadcast gauge/counter values) | `TestInspectorPlugin` (integration): connect mock client, subscribe to `observation.metric`, assert fields present in JSON payload | Not Started | sonnet | Verify no new code needed — registered metrics should auto-appear in snapshot. If not, add to snapshot loop. |

## Phase 5 — Integration & Verification

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 37 | Update `dia pipeline --target cluicheeditor` to build Inspector UI (`npm run build`) and deploy `dist/` to `diaapplicationflowinspector/` in output | `dia pipeline --target cluicheeditor` succeeds; `dist/index.html` present in deploy dir | Not Started | sonnet | |
| 38 | Wire Inspector plugin in CluicheEditor `.diaapp` manifest (register plugin, set UI path) | Editor launches, Inspector panel visible in dock | Not Started | sonnet | |
| 39 | Regression: full test suite — `dia run googletest` (all), Editor UI tests, Inspector UI tests | Zero failures | Not Started | sonnet | Gate |
| 40 | Manual verify: Editor opens fullscreen, loads manifest, all editing works (tree, flow, inspector, lifecycle grid, undo) | Quoted output from `dia pipeline --target cluicheeditor` + screenshots if available | Not Started | opus | |
| 41 | Manual verify: Inspector docks, shows empty state, connects to `dia run cluichetest`, shows live module data in Modules tab | Quoted connection log from Inspector | Not Started | opus | |
| 42 | Manual verify: Editor ConnectionStatusDot is grey when Inspector not connected; green+pulse when connected | Visual confirmation | Not Started | sonnet | Regression AC: connection-status-indicator.md |
| 43 | Manual verify: Editor ModulePresenceGrid highlights active stage when Inspector connected (pushed `live.state` topic) | Visual confirmation | Not Started | sonnet | |
| 44 | Manual verify: Streams tab shows backpressure bars, Timing tab shows PU gauges, Log tab shows events | Visual confirmation against mockup | Not Started | sonnet | |
| 45 | Manual verify: RiskyChangeDialog triggers when removing PU while connected — includes "affects running game" text; same dialog without that text when disconnected | Dialog text matches AC #7/#8 in risky-change-warnings.md | Not Started | sonnet | |
| 46 | Manual verify: Editor renders correctly with Inspector plugin NOT loaded (offline mode, no errors) | No console errors; grey dot; offline presence grid | Not Started | sonnet | Regression AC: live-state-overlay.md #10 |
| 47 | Cleanup: remove old `Dia/DiaApplicationEditor/` directory (if any remnants after rename) | `git status` — no orphan files | Not Started | haiku | |
| 48 | Create `dia.applicationflowinspector.architecture.module.md` (YAML frontmatter: id, deps, public API, responsibilities) | Doc matches module-metadata-schema.md | Not Started | haiku | |
| 49 | Update specs: mark DiaApplicationFlowEditor spec `Done`, DiaApplicationFlowInspector spec `Done`, update backlog | Spec status fields correct | Not Started | haiku | |

## Parallelism

```
Phase 1 (#1–11) ──────────────────► Phase 2 (#12–24) ──┐
                                                        ├──► Phase 5 (#37–49)
Phase 3 (#25–31) starts after #6 ──────────────────────┤
                                                        │
Phase 4 (#32–36) independent ──────────────────────────┘
```

- Phase 1 → Phase 2 is sequential (Phase 2 removes what Phase 1 extracted)
- Phase 3 can start once #6 is done (Inspector UI scaffold exists)
- Phase 4 is fully independent — can run in parallel with Phase 2 or Phase 3
- Phase 5 requires all prior phases complete

Within phases:
- Phase 1: #1→#2→#3→#4→#5 sequential; #6→#7→#8→#9→#10 sequential; #11 after #2
- Phase 2: #12–#16 sequential (removals); #17–#21 sequential (new Editor ACs); #22→#23→#24 sequential (rename + gate)
- Phase 3: #25 first (store), then #26–#30 in parallel, then #31 (wiring)
- Phase 4: #32→#33 sequential (same file); #34→#35 sequential; #36 after all

## Risk Areas

| Risk | Impact | Mitigation |
|------|--------|------------|
| Rename `DiaApplicationEditor` → `DiaApplicationFlowEditor` (#22) | Touches vcxproj GUIDs, sln, include paths, DiaCLI. Highest breakage surface. | Run full solution build + pipeline immediately after; regression gate (#24). |
| `useLiveStoreV2` split (#7, #15) | Editor's read-only version must still react to pushed topics | Existing Editor UI tests validate; explicit test for "setter without bridge call" (#15); no-Inspector test (#20) |
| Editor post-split regression (#17–#21) | Editor must render correctly without Inspector, with no console errors, and respond immediately to connection status changes | Dedicated tests per AC; regression gate (#24) before rename |
| Framework telemetry (#32–34) | Touches ProcessingUnit + EventStreamStore — affects all apps | Minimal changes (~25 lines total); all follow existing MetricRegistry pattern; regression via `dia run googletest` |
| GameConnectionManager ownership | Inspector needs it from PluginServiceLocator; Editor keeps read-only access | DAFI-009 confirms framework-level; both plugins request same service — no conflict; explicit test in #19 |
| Event log memory | 1024 entries × ~200 bytes = ~200KB per Inspector instance | Acceptable for dev tool; ring buffer drops oldest silently |

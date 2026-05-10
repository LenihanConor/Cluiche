# Implementation Plan: DiaApplicationFlow

**Spec:** @docs/specs/systems/dia/diaapplicationflow.md
**Research:** @docs/research/diapp_simplif/summary.md
**Estimated timeline:** 14-16 weeks phased (P1→P3→P4→P8→P2→P5→P6→P7)

## Session Notes

**Spec decisions summary (for subagent dispatch):**
DiaApplicationFlow is a config-driven application framework in `Dia::ApplicationFlow::` namespace. Config (.diaapp JSON manifest) is the sole source of truth for all structural wiring (SD-001). Phases are removed, replaced by config-declared Stages (SD-002). ModuleRef<T> is the only module access pattern (SD-003). Transitions are app-wide (SD-004), execute at start of next frame (SD-005). Streams are framework-owned, config-declared (SD-006). MessageBus is removed (SD-007). One-liner DIA_MODULE macro for registration (SD-008). Error policy: assert debug, rollback release, shutdown boot (SD-009). PU startup order = array order (SD-010). Shutdown is framework-level (SD-011). Hot reload = re-enter stage (SD-012). DoStart returns StartResult for async startup (SD-013). Full validation at load (SD-014). No shared modules across PUs (SD-015). IApplicationInspectable for inspection (SD-016). Clean break, no backward compat (SD-017). All IDs are StringCRC (PD-001). No STL in public APIs (PD-004). C++20 (PD-007). VS project files manually maintained (PD-006).

## Implementation Patterns

### P1: Framework Core (Module Lifecycle + Stage System + Error Handling)

**Module base class** (`Module.h/.cpp`):
- `Module` abstract base with `DoStart()`→`StartResult`, `DoUpdate(float)`, `DoStop()`→`StopResult`
- Internal state machine driven by `FrameTick()` — friend of ProcessingUnit
- `ModuleState` enum: kInactive, kStarting, kActive, kStopping, kFailed
- Timeout tracking via `mStateElapsedMs` accumulated from wall-clock delta
- Constructor takes `StringCRC instanceId` only

**Application class** (`Application.h/.cpp`):
- Owns PUs, stages, streams. Constructor takes `ApplicationManifest`, `TypeRegistry`, `BootstrapResources`
- `Start()` → validate manifest, create PUs/modules/streams, enter initial_stage
- `Update()` → check pending transition, tick main PU inline, return false when shutdown complete
- `TransitionTo(StringCRC)` → atomically stores pending stage
- Transition algorithm: compute RETAIN/STOP/START diff, stop outgoing (reverse dep order), start incoming (dep order)
- Auto-advance check after transition completes
- `RequestShutdown()` → sets shutting-down flag, stops all modules reverse order, joins threads

**Error handling** (within Application):
- Boot stage failure → `RequestShutdown()`
- Debug non-boot → `DIA_ASSERT` (breaks into debugger)
- Release non-boot → rollback (stop START modules, restart STOP modules)
- Rollback failure → escalate to shutdown
- All failures logged with module ID, failure type, elapsed time, stage context

### P3: Registration (TypeRegistry + DIA_MODULE + ModuleRef)

**TypeRegistry** (`TypeRegistry.h/.cpp`):
- `HashTableC<StringCRC, FactoryFn>` mapping type IDs to factory lambdas
- Meyers singleton for global instance (static-init safe)
- `ModuleRegistration<T>` template struct — constructor registers factory

**DIA_MODULE macro** (`RegistrationMacros.h`):
- `static ModuleRegistration<ClassName> s_reg_##ClassName{ClassName::kTypeId}`
- C++20 concept `DerivedModule` constrains T

**ModuleRef<T>** (`ModuleRef.h`):
- Constructed with owner Module* + target StringCRC (defaults to T::kTypeId)
- Lazy resolution: first `Get()` queries `owner->GetProcessingUnit()->FindModule(id)`
- Caches pointer. `operator->` asserts non-null.

### P4: Config Format + Validation

**ApplicationManifest** (`Manifest/ApplicationManifest.h`):
- POD-like struct: `DynamicArrayC<ProcessingUnitDeclaration, 4>`, `DynamicArrayC<StreamDeclaration, 16>`, etc.
- No behavior — pure data

**ApplicationManifestLoader** (`Manifest/ApplicationManifestLoader.h/.cpp`):
- Reads JSON via DiaSerializer/jsoncpp → populates ApplicationManifest
- Rejects `version != 2`

**ManifestComposer** (`Manifest/ManifestComposer.h/.cpp`):
- `Compose(diagamePath)` → reads .diagame, loads base .diaapp, merges stage .diaapp files
- Returns final merged ApplicationManifest

**ManifestValidator** (`Manifest/ManifestValidator.h/.cpp`):
- Takes manifest + TypeRegistry, runs all checks, returns `DynamicArrayC<ValidationEntry, 64>`
- Cycle detection via Kahn's algorithm per PU

### P2: Streams

**FrameStreamStore<T>** — double-buffer, atomic swap on write, lock-free read
**EventStreamStore<T>** — per-reader ring buffers (256 default), mutex-protected write fans out to all readers
**StreamWriter/Reader** — thin handles holding pointer to store slot, injected post-creation by framework
**EventStreamWriter/Reader** — same pattern, EventStreamStore backing

### P6: Inspectable

**IApplicationInspectable** — virtual interface on Application
- Returns snapshots (copies) — thread-safe reads
- `TransitionInfo` struct for transition progress
- Consumed by DiaDebugServer, DiaApplicationFlowEditor, DiaTestHarness

### P8: Test Suite (continuous through all phases)

GoogleTest suites per feature:
- `ModuleLifecycleTests` — state machine transitions, timeout firing, kLoading loops
- `StageSystemTests` — diff algorithm, auto-advance, rollback
- `RegistrationTests` — DIA_MODULE, TypeRegistry lookup, ModuleRef resolution
- `ConfigFormatTests` — manifest load, merge, round-trip
- `ValidationTests` — each error code has a test case
- `StreamTests` — FrameStream write/read, EventStream fan-out, capacity overflow
- `ErrorHandlingTests` — rollback scenario, double failure → shutdown
- `InspectableTests` — query accuracy during transitions

## Task Table

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `Module.h/.cpp` — base class, enums, state machine, FrameTick | ModuleLifecycleTests | Done | sonnet | Foundation — everything depends on this |
| 2 | Create `ProcessingUnit.h/.cpp` — owns modules, ticks them, dependency-ordered start/stop | ModuleLifecycleTests | Done | sonnet | Fixed array instead of DynamicArrayC (UniquePtr non-copyable) |
| 3 | Create `TypeRegistry.h/.cpp` + `RegistrationMacrosV2.h` + `ModuleRefV2.h` | RegistrationTests | Done | sonnet | Created as V2 files alongside v1 |
| 4 | Create `ApplicationManifestV2.h` — POD structs (declarations) | — | Done | haiku | Pure data — created as ApplicationManifestV2.h |
| 5 | Create `ApplicationManifestLoaderV2.h/.cpp` — JSON → ApplicationManifestV2 | ConfigFormatTests | Done | sonnet | Also registered new files in vcxproj |
| 6 | Create `ManifestComposerV2.h/.cpp` — .diagame → merged manifest | ConfigFormatTests | Done | sonnet | — |
| 7 | Create `ManifestValidatorV2.h/.cpp` — all validation checks + cycle detection | ValidationTests | Done | sonnet | Depends on #4, #3 |
| 8 | Create `Application.h/.cpp` — constructor, Start, Update, TransitionTo, shutdown | StageSystemTests, ErrorHandlingTests | Done | opus | Module.cpp TODO wired; vcxproj + filters updated |
| 9 | Create `Streams/` — FrameStreamStore, EventStreamStore, Writer/Reader handles | StreamTests | Done | sonnet | Header-only; DynamicArrayC namespace fixed; vcxproj+filters updated |
| 10 | Wire streams into Application — framework creates stores, injects into handles | StreamTests (integration) | Done | sonnet | IStreamStore base; FrameStreamStore/EventStreamStore inherit it; Module::OnConnectStreams hook; Application::FindOrRegisterStreamStore + FindStreamStore; handles expose Connect(Application&) replacing SetStore/friend pattern |
| 11 | Create `IApplicationInspectable.h` + implement in Application | InspectableTests | Done | sonnet | Application inherits IApplicationInspectable directly; GetCurrentStage returns by value |
| 12 | Update `DiaApplicationFlow.vcxproj` + `.filters` — add all new files, remove v1 files | Build passes | Done | haiku | Completed alongside #8; all v2 files registered |
| 13 | Write GoogleTest suites for all features | All pass | Done | sonnet | 49 tests across 7 files in ApplicationFlow/; TestValidation.cpp (12) + TestStreams.cpp (11) added; timeout test added to TestModuleLifecycle |
| 14 | Delete v1 files (ApplicationModule, ApplicationPhase, MessageBus, HotReloadManager, StateObject) + drop V2 suffixes (ApplicationManifestV2 → ApplicationManifest, ManifestValidatorV2 → ManifestValidator, etc.) | Build passes | Deferred | haiku | V2 suffix exists only to avoid ODR collision with v1 equivalents; remove suffix when v1 files are deleted |
| 15 | Update `dia.applicationflow.architecture.module.md` | — | Done | haiku | Updated in-place as dia.application.architecture.module.md |

## Dependency Graph

```
#1 Module ─────┐
#2 ProcessingUnit ─┼─► #8 Application ─► #10 Wire Streams ─► #11 Inspectable
#3 TypeRegistry ───┘        │                    ▲
#4 Manifest POD ─► #5 Loader ─► #6 Composer     │
                         │                       │
                    #7 Validator              #9 Streams
                                                 │
                    #12 vcxproj ◄── all source ──┘
                    #13 Tests (continuous)
                    #14 Delete v1 ◄── #12
                    #15 Module doc ◄── #12
```

## Phase Mapping

| Research Phase | Plan Tasks | Feature Specs Covered |
|---------------|------------|----------------------|
| P1 Framework Core | #1, #2, #8 | Module Lifecycle, Stage System, Error Handling |
| P3 Module Access | #3 | Registration |
| P4 Config Format | #4, #5, #6, #7 | Config Format v2, Validation |
| P2 Streams | #9, #10 | Streams |
| P6 Debug/E2E | #11 | Inspectable Interface |
| P8 Tests | #13 | (all features) |
| Cleanup | #12, #14, #15 | — |

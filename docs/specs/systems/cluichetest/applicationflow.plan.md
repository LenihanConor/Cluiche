# Implementation Plan: CluicheTest Application Flow

**Spec:** @docs/specs/systems/cluichetest/applicationflow.md  
**Status:** Not Started

---

## Session Notes

**Spec decisions summary (Platform → App → System → Feature):**

Platform bindings: StringCRC for all IDs (module instance IDs, stage names, stream IDs); no STL in public APIs (use DiaCore containers); C++20; `.diagame` is project root, `.diastage` for stage overlays.

CluicheTest App bindings: three PUs (Main/Sim/Render); entry point in `Main.cpp`; `Main.cpp` creates the Application and runs Start/Update/Stop.

DiaApplicationFlow System bindings: config (manifest) is sole source of truth for all structural wiring — no self-registration to PUs; stages replace Phases (no Phase subclasses); `ModuleRef<T>` is the only inter-module access pattern; `TransitionTo` is app-wide and simultaneous across all PUs; stage transitions execute at the start of the next frame (queued/async); streams declared in config and owned by framework; `DoStart()` returns `StartResult` (kReady / kLoading / kFailed).

CluicheTest AppFlow System bindings: Main never changes stage (only Sim + Render do); Boot auto-advances to DummyStage; `LoadingScreenModule` keeps SimToRender fed during transitions (wins via update order — config position); stage-specific modules come from `.diastage` files; all modules log at DoStart/DoStop entry and exit; canvas created pre-Application in `Main.cpp`, passed as bootstrap resource; `KernelModule` calls `RequestShutdown()` on window close.

**Current codebase state:**

DiaApplicationFlow v2 is fully implemented: `Application`, `Module`, `ProcessingUnit`, `TypeRegistry`, `ModuleRefV2`, `RegistrationMacrosV2` (`DIA_MODULE`), `StreamWriter/Reader`, `EventStreamWriter/Reader`, `ManifestComposerV2`, `ManifestValidatorV2`. Namespace: `Dia::ApplicationFlow`.

CluicheTest currently has a v1-style implementation (Phases, v1 ProcessingUnits, CluicheKernel modules). The new v2 modules go in `Cluiche/CluicheTest/Modules/`. Old v1 code is NOT deleted until the new implementation is verified end-to-end.

**Key gap — missing stream event types:**

- `InputEvent` — not defined in Dia. Needed in `DiaInput` (engine boundary, any game could consume it). Task 1 adds it. **Escalation required before Task 1 starts.**
- `UICommand` — app-specific Sim→Main UI protocol. Stays in CluicheTest (not generic engine capability). Task 1 also adds it.

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Define stream event types: `InputEvent` (DiaInput) + `UICommand` (CluicheTest/Types/) | Build compiles | Done | sonnet | `InputEvent` = alias for `Dia::Input::Event`. `UICommand` = tagged struct in `Types/UICommand.h`. |
| 2 | Manifest Configuration — write JSON files + rewrite Main.cpp | `dia pipeline --target cluichetest` builds | Done | sonnet | v2 manifests live in `Cluiche/Assets/CluicheTest/` (deployed via asset pipeline). `cluiche_main.diaapp` is the single self-contained v2 base manifest (3 PUs + 3 streams + stages). `dummy_stage.diaapp` overlays `DummyLevelModule` onto SimPU. `.diastage.manifest` path is relative to `.diagame` directory. Main.cpp reads `config.path_aliases` from `.diagame` and calls `PathStore::RegisterToStore` BEFORE `app.Start()` (required for `RenderWindow`'s FilePath("root",...) shader lookup). |
| 3 | Main PU Modules — `LoggerModule`, `KernelModule`, `AssetServiceModule`, `UIModule` | Build passes | Done | sonnet | All in `Cluiche::AppFlow` namespace (resolves ODR collision with v1 CluicheKernel modules). |
| 4 | Sim PU Modules — `TimeServerModule`, `InputStreamModule`, `LoadingScreenModule` | Build passes | Done | sonnet | All in `Cluiche::AppFlow` namespace. |
| 5 | Render PU Module — `RenderModule` | Build passes | Done | sonnet | Canvas via `KernelModule::GetStaticCanvas()` (cross-PU static accessor). |
| 6 | DummyStage Level — `DummyLevelModule` | Build passes | Done | sonnet | kLoading→kReady pattern, writes SimToRender + SimToUI. |
| 7 | End-to-end build + smoke test | `dia pipeline --target cluichetest` — 3 passed, 0 failed; exe runs 20s without crash | Done | sonnet | Pipeline passes. Runtime smoke test: launched exe, verified process alive at 20s (no crash). Root cause of prior crash: missing path-alias registration — `"root"` alias (from `.diagame` config) must be registered in `PathStore` before `KernelModule` creates its `RenderWindow` (shader lookup uses `FilePath("root", ...)`). Fixed in Main.cpp via `RegisterPathAliasesFromDiagame()` helper called before `app.Start()`. |

---

## Dependency Order

```
Task 1 (types)
    └─ Task 3 (Main PU Modules)  ─┐
    └─ Task 4 (Sim PU Modules)   ─┼─ Task 2 (Manifest + Main.cpp) ─ Task 7
    └─ Task 5 (Render PU Module) ─┤
    └─ Task 6 (DummyStage Level) ─┘
```

Tasks 3, 4, 5, 6 can run in parallel after Task 1 completes (different files, no shared headers beyond Task 1 types). Manifest JSON files (Task 2 partial) can be written in parallel too; only `Main.cpp` rewrite must wait for all modules.

---

## Files Created / Modified

| File | Task | Action |
|------|------|--------|
| `Dia/DiaInput/InputEvent.h` | 1 | New — keyboard/mouse event type |
| `Cluiche/CluicheTest/Types/UICommand.h` | 1 | New — sim→main UI command type |
| `Cluiche/Assets/CluicheTest/Global/Misc/ApplicationFlow/cluiche_main.diaapp` | 2 | Rewrite as v2 format (single self-contained manifest) |
| `Cluiche/Assets/CluicheTest/Stages/DummyStage/dummy_stage.diastage` | 2 | Rewrite — `manifest` path relative to `.diagame` dir |
| `Cluiche/Assets/CluicheTest/Stages/DummyStage/Misc/ApplicationFlow/dummy_stage.diaapp` | 2 | Rewrite as v2 overlay |
| `Cluiche/CluicheTest/Main.cpp` | 2 | Rewrite |
| `Cluiche/CluicheTest/Modules/LoggerModule.h/.cpp` | 3 | New |
| `Cluiche/CluicheTest/Modules/KernelModule.h/.cpp` | 3 | New |
| `Cluiche/CluicheTest/Modules/AssetServiceModule.h/.cpp` | 3 | New |
| `Cluiche/CluicheTest/Modules/UIModule.h/.cpp` | 3 | New |
| `Cluiche/CluicheTest/Modules/TimeServerModule.h/.cpp` | 4 | New |
| `Cluiche/CluicheTest/Modules/InputStreamModule.h/.cpp` | 4 | New |
| `Cluiche/CluicheTest/Modules/LoadingScreenModule.h/.cpp` | 4 | New |
| `Cluiche/CluicheTest/Modules/RenderModule.h/.cpp` | 5 | New |
| `Cluiche/CluicheTest/Modules/DummyLevelModule.h/.cpp` | 6 | New |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` | 3–6 | Update |
| `Cluiche/CluicheTest/CluicheTest.vcxproj.filters` | 3–6 | Update |
| `Dia/DiaInput/DiaInput.vcxproj` | 1 | Update (add InputEvent.h) |

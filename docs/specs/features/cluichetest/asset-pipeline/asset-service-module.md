# Feature Spec: AssetServiceModule & Phase Gating

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | CluicheTest | @docs/specs/applications/cluichetest.md |
| System | CluicheTest Asset Pipeline | @docs/specs/systems/cluichetest/asset-pipeline.md |
| Feature | AssetServiceModule & Phase Gating | (this document) |

## Summary

Create `AssetServiceModule`, a `Dia::Application::Module` on the Main ProcessingUnit that owns a `Dia::AssetRuntime::AssetRuntime` instance. The module loads the runtime manifest at startup, provides `RequestGlobalLoad()` / `RequestStageLoad()` / `RequestStageUnload()` methods for load phases, and exposes `IsLoadComplete()` for phase gating. Load phases block (via standard phase transition conditions) until all requested assets reach `Loaded` state.

## Problem

DiaAssetRuntime is a pure library with no knowledge of DiaApplicationFlow's ProcessingUnit/Phase/Module architecture. CluicheTest needs a Module that wires DiaAssetRuntime into the application lifecycle: loading global assets at boot, loading/unloading stage assets during transitions, and gating phase progression on asset readiness. Without this, there is no integration point between the asset system and the game loop.

## Acceptance Criteria

1. `AssetServiceModule` class exists, inherits from `Dia::Application::Module`
2. `AssetServiceModule` has a `static const Dia::Core::StringCRC kUniqueId`
3. `DoStart()` creates an `AssetRuntime` instance and calls `LoadManifest()` with the path to `assets.runtime.json`
4. `DoEnd()` calls `runtime.Reset()` and destroys the instance
5. `RequestGlobalLoad()` calls `runtime.RequestStageLoad(StringCRC("stage.global"))`
6. `RequestStageLoad(stageId)` calls `runtime.RequestStageLoad(stageId)`
7. `RequestStageUnload(stageId)` calls `runtime.RequestStageUnload(stageId)`
8. `IsLoadComplete()` returns true when all assets in the most recently requested load are in `Loaded` state
9. `GetRuntime()` returns a const reference to the AssetRuntime instance (safe for cross-thread reads)
10. AssetServiceModule is registered on MainProcessingUnit and starts during kernel initialization
11. A boot load phase exists that calls `RequestGlobalLoad()` and gates on `IsLoadComplete()`
12. DummyStage's `MainLoadPhase` calls `RequestStageLoad(StringCRC("stage.dummy_stage"))` and gates on `IsLoadComplete()`
13. Stage exit triggers `RequestStageUnload()` before the stage is destroyed
14. AssetServiceModule implements `IAssetStateListener` to track asset state transitions and acknowledge loads
15. Phase gating uses the existing phase transition condition mechanism (not polling in a loop)

## API Design

### AssetServiceModule

```cpp
// CluicheTest/CluicheKernel/ApplicationFlow/Modules/AssetServiceModule.h

namespace Cluiche::Main
{
    class AssetServiceModule : public Dia::Application::Module,
                               public Dia::AssetRuntime::IAssetStateListener
    {
    public:
        static const Dia::Core::StringCRC kUniqueId;

        // Load requests — called by phases
        void RequestGlobalLoad();
        void RequestStageLoad(const Dia::Core::StringCRC& stageId);
        void RequestStageUnload(const Dia::Core::StringCRC& stageId);

        // Phase gating — called by phase transition conditions
        bool IsLoadComplete() const;

        // Cross-thread safe read access
        const Dia::AssetRuntime::AssetRuntime& GetRuntime() const;

    protected:
        // Module lifecycle
        void DoStart() override;
        void DoEnd() override;

        // IAssetStateListener — acknowledge on behalf of the module
        void OnAssetReady(const Dia::Core::StringCRC& assetId,
                          const Dia::Core::Containers::String512& resolvedPath) override;
        void OnAssetUnloading(const Dia::Core::StringCRC& assetId) override;
        void OnAssetLoadFailed(const Dia::Core::StringCRC& assetId) override;

    private:
        Dia::AssetRuntime::AssetRuntime mRuntime;
        Dia::Core::StringCRC mCurrentLoadStageId;
        bool mLoadComplete = true;
    };
}
```

### Lifecycle Flow

```
MainProcessingUnit::GenerateModuleDependecyGraph()
    └─ Add AssetServiceModule (no dependencies on other modules for now)

AssetServiceModule::DoStart()
    ├─ mRuntime.LoadManifest("assets/assets.runtime.json")
    └─ mRuntime.RegisterListener(this)

BootLoadPhase::DoEnter()
    ├─ GetModule<AssetServiceModule>()->RequestGlobalLoad()
    └─ Register phase condition: AssetServiceModule::IsLoadComplete()

BootLoadPhase condition met → transition to next phase

DummyStage::MainLoadPhase::DoEnter()
    ├─ GetModule<AssetServiceModule>()->RequestStageLoad("stage.dummy_stage")
    └─ Register phase condition: AssetServiceModule::IsLoadComplete()

DummyStage::MainLoadPhase condition met → transition to MainFEPhase

[Stage exit]
    └─ GetModule<AssetServiceModule>()->RequestStageUnload("stage.dummy_stage")
```

### IAssetStateListener Implementation

AssetServiceModule acts as the primary listener and acknowledges asset loads on behalf of the application. In V1, since loads are blocking and all assets are simple files:

```cpp
void AssetServiceModule::OnAssetReady(const StringCRC& assetId, const String512& resolvedPath)
{
    // V1: immediately acknowledge — no content loading needed at this layer
    // Future: forward to content consumers (DiaGraphics, etc.) who acknowledge after loading content
    mRuntime.AcknowledgeAssetLoaded(assetId);
}

void AssetServiceModule::OnAssetUnloading(const StringCRC& assetId)
{
    // V1: immediately acknowledge — no content to release at this layer
    mRuntime.AcknowledgeAssetUnloaded(assetId);
}
```

### Phase Gating Pattern

```cpp
// In the boot load phase
void BootLoadPhase::DoEnter()
{
    auto* assetModule = GetProcessingUnit()->GetModule<Cluiche::Main::AssetServiceModule>();
    assetModule->RequestGlobalLoad();
}

bool BootLoadPhase::CanTransition() const
{
    auto* assetModule = GetProcessingUnit()->GetModule<Cluiche::Main::AssetServiceModule>();
    return assetModule->IsLoadComplete();
}
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Create AssetServiceModule header and implementation | Module class with kUniqueId, DoStart/DoEnd, request methods, IsLoadComplete, GetRuntime, IAssetStateListener implementation |
| 2 | Register AssetServiceModule on MainProcessingUnit | Add to `GenerateModuleDependecyGraph()`, include header, ensure it starts during kernel init |
| 3 | Create BootLoadPhase (or modify existing boot sequence) | Phase that calls RequestGlobalLoad() and gates on IsLoadComplete() before transitioning |
| 4 | Wire DummyStage MainLoadPhase to request stage assets | Call RequestStageLoad in MainLoadPhase, gate on IsLoadComplete() |
| 5 | Wire stage exit to RequestStageUnload | Ensure RequestStageUnload is called when leaving a stage |
| 6 | Update texture loading in SimProcessingUnit | Replace hardcoded `Assets/Textures/test_*.png` paths with DiaAssetRuntime path resolution via AssetServiceModule::GetRuntime() |
| 7 | Update CluicheTest.vcxproj | Add new source files |
| 8 | Verify end-to-end | Build and run CluicheTest — confirm boot loads global assets, DummyStage loads stage assets, textures render correctly |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| Feature 2 (Pipeline Deploy) | `assets.runtime.json` must exist in deploy directory for LoadManifest to succeed |
| DiaAssetRuntime (Features 1-4) | AssetRuntime class, IAssetStateListener, state machine, stage lifecycle |
| DiaApplicationFlow Module | Base class for AssetServiceModule |
| CluicheTest MainProcessingUnit | Module registration |
| CluicheTest phase system | Phase gating condition mechanism |

## Files

| File | Action |
|------|--------|
| `Cluiche/CluicheTest/CluicheKernel/ApplicationFlow/Modules/AssetServiceModule.h` | Create |
| `Cluiche/CluicheTest/CluicheKernel/ApplicationFlow/Modules/AssetServiceModule.cpp` | Create |
| `Cluiche/CluicheTest/ApplicationFlow/ProcessingUnits/MainProcessingUnit.cpp` | Modify — register AssetServiceModule |
| `Cluiche/CluicheTest/ApplicationFlow/ProcessingUnits/SimProcessingUnit.cpp` | Modify — replace hardcoded texture paths with runtime resolution |
| `Cluiche/CluicheTest/Stages/DummyStage/LevelFlow/Phases/MainLoadPhase.cpp` | Modify — wire stage asset loading with phase gating |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` | Modify — add new files |
| `Cluiche/CluicheTest/CluicheTest.vcxproj.filters` | Modify — add new files to filter tree |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | **Compliant.** kUniqueId, stageId parameters, asset IDs all StringCRC. |
| PD-002 | ProcessingUnit/Phase/Module architecture | **Compliant.** AssetServiceModule is a Module on Main PU; phase gating uses standard conditions. |
| PD-004 | No STL in public APIs | **Compliant.** All public methods use DiaCore types. |
| PD-006 | VS project files are source of truth | **Compliant.** New files added to .vcxproj manually. |
| AD-001 | Three ProcessingUnits (Main/Render/Sim) | **Compliant.** Module on Main PU; Sim/Render read via GetRuntime(). |
| SD-ARUN-001 | DiaAssetRuntime has no DiaApplicationFlow dependency | **Compliant.** AssetServiceModule wraps DiaAssetRuntime; the library doesn't know about Modules. |
| SD-ARUN-002 | Stage is unit of load/unload | **Compliant.** RequestGlobalLoad/RequestStageLoad operate on stages, not individual assets. |
| SD-ARUN-004 | Consumers acknowledge load/unload | **Compliant.** AssetServiceModule implements IAssetStateListener and calls AcknowledgeAssetLoaded. |
| SD-CTAP-002 | AssetServiceModule on Main PU, persists across all stages | **Compliant.** Registered at kernel level, never destroyed during stage transitions. |
| SD-CTAP-004 | Loads blocking in V1, API async-ready | **Compliant.** Request + gate pattern. V1 loads are synchronous; gating resolves immediately. |
| SD-CTAP-005 | One stage at a time, unload-then-load | **Compliant.** RequestStageUnload before RequestStageLoad for next stage. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Acknowledgement | In V1, AssetServiceModule auto-acknowledges all assets. When content consumers (DiaGraphics) are added, who acknowledges? | Future: AssetServiceModule stops auto-acknowledging. Content consumers register as IAssetStateListeners on the runtime directly and acknowledge after loading their content. IsLoadComplete still gates on all assets reaching Loaded. |
| 2 | Manifest path | How does DoStart() know where `assets.runtime.json` is? | Resolved relative to the exe directory: `"assets/assets.runtime.json"`. Matches the deploy root from DiaAssetPipeline. Could also be passed from the .diagame config in Feature 4. |
| 3 | Thread safety | GetRuntime() returns a const ref. Is that sufficient for Sim/Render thread reads? | Yes. Once assets are Loaded (after phase gate passes), the runtime's internal data is immutable until the next unload phase. Const reference prevents mutation. No locks needed on read path. |
| 4 | Failure | What happens if LoadManifest fails or an asset fails to load? | V1: Log error via DiaLogger, IsLoadComplete() never returns true, phase never transitions — application hangs at load screen. This is acceptable for a testbed. Future: timeout + error state. |
| 5 | Existing boot sequence | Does CluicheTest already have a boot load phase, or does one need to be created? | Existing flow: MainBootStrapPhase initializes systems. A new BootAssetLoadPhase (or gating within MainBootStrapPhase) needs to be inserted after KernelModule starts and before the launch menu. |
| 6 | SimProcessingUnit texture paths | How does SimPU access AssetServiceModule (which is on MainPU) from a different thread? | SimPU gets the AssetRuntime reference during its init phase (when all PUs are synchronized). It stores the const reference locally. After that, reads are safe — no cross-PU Module access during gameplay frames. |

## Status

`Approved`

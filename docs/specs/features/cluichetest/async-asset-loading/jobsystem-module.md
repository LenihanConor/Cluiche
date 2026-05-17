# Feature Spec: JobSystem Module

## Parent System
@docs/specs/systems/cluichetest/async-asset-loading.md

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | CluicheTest | @docs/specs/applications/cluichetest.md |
| System | Async Stage Asset Loading | @docs/specs/systems/cluichetest/async-asset-loading.md |
| Feature | JobSystem Module | (this file) |

## Problem Statement

`Dia::Core::Threading::JobSystem` is fully implemented and tested in `Dia/DiaCore/Threading/JobSystem.h`, but its only callers are unit tests — `JobSystem::Initialize` is never invoked in production code. Before any module can submit jobs, JobSystem needs an owner that calls `Initialize` on app startup and `Shutdown` on app teardown. The codebase convention for "engine subsystem with a lifecycle" is a Module wrapper (see `TimeServerModule`, `AssetServiceModule`). This feature creates that wrapper.

## Acceptance Criteria

1. **`JobSystemModule`** — new Module living at `Cluiche/CluicheGameBaseline/Modules/JobSystemModule.h/.cpp`
2. `DoStart()` calls `Dia::Core::Threading::JobSystem::Initialize(0)` (0 = use hardware concurrency); returns `StartResult::kReady` synchronously
3. `DoUpdate(float)` is empty — JobSystem's worker threads pull from their own queue
4. `DoStop()` calls `Dia::Core::Threading::JobSystem::Shutdown()`; returns `StopResult::kDone` synchronously
5. Logs at `DoStart`/`DoStop` entry/exit per SD-007 (CT AppFlow), DiaLogger Application channel
6. Registered in `cluiche.diaapp` on **MainPU** with `"stages": ["all"]`, declared **before** `AssetService` in the modules array so it starts first
7. `AssetService`'s manifest entry adds `"JobSystem"` to its `dependencies` array
8. Build: added to `CluicheGameBaseline.vcxproj` and `.vcxproj.filters`
9. `kTypeId` is `StringCRC("JobSystemModule")` (per PD-001)
10. Registered with `DIA_MODULE(JobSystemModule_)` macro at end of `.cpp` file (matches existing module pattern)

## Module Specification

**Instance Ownership (Refactor Update 2026-05-17):** The module now owns a `Dia::Core::JobSystem` instance by value (member `mJobSystem`) and exposes it via the standard `GetStatic()` + `GetJobSystem()` pattern (mirroring `AssetServiceModule::GetStatic()`). The legacy static shim (`Dia::Core::JobSystem::Initialize/Shutdown`) is initialized in parallel during `DoStart`/`DoStop` for backward compatibility while existing callers (e.g., `TextureHandler`) migrate to the instance API. The shim will be removed in a follow-up phase after all consumers have migrated.

```cpp
// JobSystemModule.h
#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Threading/JobSystem.h>

namespace Cluiche { namespace AppFlow {

class JobSystemModule : public Dia::ApplicationFlow::Module {
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit JobSystemModule(const Dia::Core::StringCRC& instanceId);

    static JobSystemModule*      GetStatic();
    Dia::Core::JobSystem&        GetJobSystem();
    const Dia::Core::JobSystem&  GetJobSystem() const;

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void                              DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult  DoStop() override;

private:
    static JobSystemModule* sInstance;
    Dia::Core::JobSystem    mJobSystem;
};

} }
```

```cpp
// JobSystemModule.cpp (sketch)
const Dia::Core::StringCRC JobSystemModule::kTypeId("JobSystemModule");

JobSystemModule* JobSystemModule::sInstance = nullptr;

JobSystemModule::JobSystemModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId) {}

Dia::ApplicationFlow::StartResult JobSystemModule::DoStart() {
    DIA_LOG_INFO("Application", "JobSystemModule DoStart entry");
    sInstance = this;
    mJobSystem.Init(0);  // module's instance
    Dia::Core::Threading::JobSystem::Initialize(0);  // legacy shim for backward compat
    DIA_LOG_INFO("Application", "JobSystemModule DoStart ready");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void JobSystemModule::DoUpdate(float) {}

Dia::ApplicationFlow::StopResult JobSystemModule::DoStop() {
    DIA_LOG_INFO("Application", "JobSystemModule DoStop entry");
    Dia::Core::Threading::JobSystem::Shutdown();  // legacy shim for backward compat
    mJobSystem.Quit();
    sInstance = nullptr;
    return Dia::ApplicationFlow::StopResult::kDone;
}

JobSystemModule* JobSystemModule::GetStatic() { return sInstance; }

Dia::Core::JobSystem& JobSystemModule::GetJobSystem() { return mJobSystem; }

const Dia::Core::JobSystem& JobSystemModule::GetJobSystem() const { return mJobSystem; }

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
namespace { using JobSystemModule_ = Cluiche::AppFlow::JobSystemModule; }
DIA_MODULE(JobSystemModule_);
```

## Manifest Wiring

`cluiche.diaapp` MainPU `modules` array (order matters — JobSystem before AssetService):

```json
{ "instance_id": "Logger",       "type_id": "LoggerModule",       "stages": ["all"], "dependencies": [] },
{ "instance_id": "JobSystem",    "type_id": "JobSystemModule",    "stages": ["all"], "dependencies": ["Logger"] },
{ "instance_id": "Kernel",       "type_id": "KernelModule",       "stages": ["all"], "dependencies": ["Logger"], "writes": ["InputToSim"] },
{ "instance_id": "AssetService", "type_id": "AssetServiceModule", "stages": ["all"], "dependencies": ["Logger", "JobSystem"] },
{ "instance_id": "UI",           "type_id": "UIModule",           "stages": ["all"], "dependencies": ["Kernel"], "reads": ["SimToUI"] }
```

## Files Touched

| File | Action |
|------|--------|
| `Cluiche/CluicheGameBaseline/Modules/JobSystemModule.h` | New (initial) → Updated (refactor: instance ownership) |
| `Cluiche/CluicheGameBaseline/Modules/JobSystemModule.cpp` | New (initial) → Updated (refactor: instance ownership) |
| `Cluiche/CluicheGameBaseline/CluicheGameBaseline.vcxproj` | Update |
| `Cluiche/CluicheGameBaseline/CluicheGameBaseline.vcxproj.filters` | Update |
| `Cluiche/Assets/CluicheTest/Global/Misc/ApplicationFlow/cluiche.diaapp` (or wherever the merged manifest lives — verify path) | Update — add JobSystem entry, add JobSystem to AssetService dependencies |

## Dependencies

- **DiaApplicationFlow** — `Module`, `StartResult`, `StopResult`, `DIA_MODULE` macro
- **DiaCore/Threading** — `Dia::Core::Threading::JobSystem::Initialize/Shutdown`
- **DiaCore/CRC** — `StringCRC`
- **DiaLogger** — `DIA_LOG_INFO`

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for IDs | Compliant — `kTypeId` is `StringCRC("JobSystemModule")` |
| PD-002 | Platform | ProcessingUnit/Phase/Module | Compliant — Module on MainPU |
| PD-004 | Platform | No STL in public APIs | Compliant — public surface is empty (only Module overrides) |
| PD-006 | Platform | VS project files source of truth | Compliant — added to `.vcxproj` and `.filters` |
| PD-007 | Platform | C++20 | Compliant |
| AD-001 | App | Three ProcessingUnits | Compliant — module on MainPU only |
| AD-003 | App | Entry point in Main.cpp | Compliant — `Main.cpp` does not call JobSystem directly |
| SD-001 (CT AppFlow) | System | Three PUs: Main/Sim/Render | Compliant — MainPU |
| SD-007 (CT AppFlow) | System | Log at DoStart/DoStop | Compliant |
| SD-001 (Async) | System | JobSystem owned by a Module with `"stages": ["all"]` | Compliant — this is exactly that module |
| SD-008 (Async) | System | Lives in `CluicheGameBaseline/Modules/` | Compliant |
| AI Q11 (Async) | System (review answer) | `JobSystemModule` startup must be synchronous | Compliant — `DoStart` returns `kReady` immediately |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Threading | Is `JobSystem::Initialize(0)` cheap enough to call synchronously in `DoStart`? | Yes — it creates the ThreadPool's worker threads, which is fast (microseconds per thread on Windows). No I/O, no allocations beyond the pool. Synchronous return is correct. |
| 2 | Lifetime | What if `DoStop` is called while jobs are still in flight? | `JobSystem::Shutdown()` is responsible for joining workers. **Risk:** if any caller holds a `Job*` and forgets to `Wait`, the worker may never decrement and Shutdown could hang. Mitigation: callers in this system (TextureHandler) must ensure all outstanding jobs are joined before `JobSystemModule::DoStop` runs. AssetService depends on JobSystem, so AssetService is stopped *first* — its `DoStop` should `Wait` on any outstanding texture loads. Document this constraint in the TextureHandler feature. |
| 3 | Reentrancy | Can `JobSystem::Initialize` be called twice (e.g., during a stage that reloads)? | The module's `"stages": ["all"]` means `DoStart` runs once at app start, `DoStop` runs once at app stop. No re-init mid-app. If we ever need to recycle, `Shutdown` followed by `Initialize` should be safe per JobSystem's existing tests — verify when needed. |
| 4 | Worker count | Should `Initialize(0)` (hardware concurrency) be parameterised? | Not in this iteration. Hardware concurrency is the right default. If we need an override later, add a manifest field (`config.job_system.worker_count`) read in `DoStart`. Out of scope here. |
| 5 | Test isolation | GoogleTest's `TestJobSystem.cpp` already calls `Initialize`/`Shutdown` in fixtures — does adding a production caller break tests? | No. Tests are separate processes from `cluichetest`. Production `JobSystemModule` lives in `CluicheGameBaseline`, not GoogleTests. No test fixture changes. |
| 6 | Logging | Should we log the worker thread count at startup? | Yes — adds one `DIA_LOG_INFO` line: `"JobSystem initialized with N worker threads"`. Helpful for debugging. JobSystem must expose `GetWorkerCount()` if it doesn't already; check before relying on it. If absent, log just `"JobSystem initialized"` and add the worker-count log when the API exposes it. |

## Open Questions

- Final manifest path for `cluiche.diaapp` — verify against current asset deployment before editing the file.
- Whether `JobSystem` exposes a `GetWorkerCount()` accessor; if not, the worker-count log is dropped (see Q6).

## Status

`Approved` — 2026-05-17

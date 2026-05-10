# System Spec: DiaApplicationFlow

## Parent Application
@docs/specs/applications/dia.md

## Supersedes
@docs/specs/systems/dia/diaapplication.md

## Research
@docs/research/diapp_simplif/summary.md

## Purpose

DiaApplicationFlow is a config-driven application framework that provides runtime architecture for structuring game applications. It implements a simplified PU/Stage/Module model where: ProcessingUnits own threads, Stages define which modules are active at a given time, Modules provide behavior, and Streams carry data between PUs. Config (`.diaapp` manifest) is the sole source of truth for all structural wiring — code implements behavior only.

This is a clean-break redesign of the original DiaApplicationFlow system. The original's Phase-based architecture, MessageBus, multiple access patterns, and redundant code+config declarations are replaced by a unified, config-driven model with one pattern per concern.

## Responsibilities

- Provide ProcessingUnit (thread owner) and Module (behavioral unit) base classes
- Manage config-declared Stages — framework handles module start/stop diff on transition
- Own and manage Streams (FrameStream/EventStream) as inter-PU communication channels
- Execute app-wide stage transitions with dependency-ordered start/stop
- Handle async module startup (kLoading/kReady) with configurable timeouts
- Handle module failure with assert (debug) / rollback (release) / shutdown (boot)
- Validate manifests at load time (dependencies, cycles, streams, stage coverage)
- Expose runtime state via IApplicationInspectable interface
- Provide one-liner module registration macro
- Provide ModuleRef<T> as the sole module access pattern
- Manage PU startup order (config array order) and shutdown (RequestShutdown)

## Non-Responsibilities

- **Game logic** — modules implement behavior; the framework manages lifecycle
- **Manifest file I/O** — DiaGame/DiaSerializer handle file reading; this system consumes parsed data
- **DiaAPI integration** — IApplicationInspectable is exposed; adapting to DiaAPI is the consumer's job
- **Editor UI** — DiaApplicationEditor is a separate system
- **Asset loading** — modules call DiaAssetRuntime; this framework gates on DoStart readiness
- **Network/distributed PUs** — local threads only

## Public Interfaces

### Core Classes

**Application (top-level coordinator):**
```cpp
namespace Dia::ApplicationFlow {
    class Application {
    public:
        // BootstrapResources param deferred — not required for current scope.
        Application(const ApplicationManifestV2& manifest, TypeRegistry& registry);

        // Lifecycle
        bool Start();       // Validates manifest, creates PUs/modules/streams, starts Boot stage; returns false on error
        bool Update(float deltaTime);  // Ticks main PU; applies pending transitions; returns false once fully shut down
        void RequestShutdown();  // Stops all modules, joins threads, exits

        // Stage transitions (app-wide, queued, executes start of next frame)
        void TransitionTo(const StringCRC& stageId);
        StringCRC GetCurrentStage() const;

        // Inspection
        IApplicationInspectable* GetInspectable();
    };
}
```

**ProcessingUnit:**
```cpp
namespace Dia::ApplicationFlow {
    class ProcessingUnit {
    public:
        ProcessingUnit(const StringCRC& instanceId, float hz, bool dedicatedThread);

        const StringCRC& GetInstanceId() const;
        float GetFrequency() const;

        // Modules (framework-managed — rarely called by user code)
        void AddModule(Module* module);
        Module* FindModule(const StringCRC& instanceId) const;

        // Thread entry point
        void operator()();
    };
}
```

**Module:**
```cpp
namespace Dia::ApplicationFlow {
    enum class StartResult { kReady, kLoading, kFailed };
    enum class StopResult { kDone, kStopping };

    class Module {
    public:
        Module(const StringCRC& instanceId);

        const StringCRC& GetInstanceId() const;
        ProcessingUnit* GetProcessingUnit() const;

        // Application-level transition (convenience — calls Application::TransitionTo)
        void TransitionTo(const StringCRC& stageId);

    protected:
        // User overrides — pure behavior
        virtual StartResult DoStart() = 0;
        virtual void DoUpdate(float deltaTime) = 0;
        virtual StopResult DoStop() = 0;
    };

    // Typed module reference — sole access pattern
    template<typename T>
    class ModuleRef {
    public:
        ModuleRef(Module* owner, const char* instanceId = T::kTypeId.GetString());
        T* Get();
        T* operator->();
        explicit operator bool() const;
    };
}
```

**Streams:**
```cpp
namespace Dia::ApplicationFlow {
    // Write handle — one producer per stream (or declared multi-writer)
    template<typename T>
    class StreamWriter {
    public:
        StreamWriter(Module* owner, const StringCRC& streamId);
        void Write(const T& data, TimeAbsolute timestamp);
    };

    // Read handle — any number of consumers
    template<typename T>
    class StreamReader {
    public:
        StreamReader(Module* owner, const StringCRC& streamId);
        const T* FetchLatest() const;
        const T* FetchClosestTo(TimeAbsolute time) const;
    };

    // Event-flavored stream (discrete events, not temporal frames)
    template<typename T>
    class EventStreamWriter {
    public:
        EventStreamWriter(Module* owner, const StringCRC& streamId);
        void Send(const T& event);
    };

    template<typename T>
    class EventStreamReader {
    public:
        EventStreamReader(Module* owner, const StringCRC& streamId);
        void Consume(DynamicArrayC<T, N>& outEvents);
    };
}
```

**Registration:**
```cpp
// One-liner macro — maps StringCRC type ID to C++ class
#define DIA_MODULE(ClassName) \
    static Dia::ApplicationFlow::ModuleRegistration<ClassName> \
        s_reg_##ClassName{ClassName::kTypeId}

// Module declares its type ID
class MyModule : public Dia::ApplicationFlow::Module {
public:
    static constexpr Dia::Core::StringCRC kTypeId{"MyModule"};
    // ...
};
DIA_MODULE(MyModule);
```

**IApplicationInspectable:**
```cpp
namespace Dia::ApplicationFlow {
    struct ModuleStateInfo {
        StringCRC instanceId;
        StringCRC typeId;
        enum State { kInactive, kStarting, kActive, kStopping, kFailed } state;
    };

    class IApplicationInspectable {
    public:
        virtual StringCRC GetCurrentStage() const = 0;
        virtual bool IsTransitioning() const = 0;
        virtual void GetActiveModules(const StringCRC& puId, DynamicArrayC<ModuleStateInfo, 64>& out) const = 0;
        virtual void GetAllStages(DynamicArrayC<StringCRC, 16>& out) const = 0;
        virtual void GetStreamInfo(DynamicArrayC<StreamInfo, 16>& out) const = 0;
    };
}
```

### Events Emitted

- **Stage Transition Requested** — TransitionTo queued (logged, inspectable)
- **Stage Transition Complete** — all modules started, update loop resumes
- **Stage Transition Failed** — module DoStart returned kFailed (rollback initiated)
- **Module Timeout** — DoStart/DoStop exceeded configured timeout
- **Shutdown Requested** — RequestShutdown called

### Data Contracts

**Module Lifecycle:**
```
Inactive → [DoStart called each frame] → kLoading... → kReady → Active (DoUpdate each frame)
Active → [DoStop called each frame] → kStopping... → kDone → Inactive
```

**Stage Transition Algorithm:**
```
1. Queue TransitionTo(targetStage) — async
2. Start of next frame: begin transition
3. Compute diff: RETAIN (in both stages), STOP (not in target), START (new in target)
4. Stop outgoing modules (reverse dependency order), wait for kDone or timeout
5. Start incoming modules (dependency order), wait for kReady or timeout
6. Retained modules keep updating throughout
7. On failure: assert (debug) / rollback to previous stage (release) / shutdown (boot)
```

**Manifest Schema (v2):**
```json
{
    "version": 2,
    "stages": ["Boot", "DummyStage", "StupidStage"],
    "initial_stage": "Boot",
    "auto_stages": ["Boot"],
    "streams": [
        { "id": "InputToSim", "type": "EventStream<InputEvent>", "from": "MainPU", "to": "SimPU" },
        { "id": "SimToRender", "type": "FrameStream<FrameData>", "from": "SimPU", "to": "RenderPU" }
    ],
    "processing_units": [
        {
            "instance_id": "MainPU",
            "frequency_hz": 30,
            "dedicated_thread": false,
            "modules": [
                {
                    "instance_id": "Kernel",
                    "type_id": "KernelModule",
                    "stages": ["all"],
                    "dependencies": [],
                    "start_timeout_ms": 5000,
                    "stop_timeout_ms": 2000
                }
            ]
        }
    ]
}
```

**PU Startup Order:** Array order in `processing_units` = startup order.

**Threading Model:**
- First PU (main thread) drives the update loop
- Subsequent PUs with `dedicated_thread: true` get their own thread
- TransitionTo is always thread-safe (queued, processed start of next frame)
- Streams are thread-safe (internal mutex on write, lock-free read of latest)

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Module Lifecycle | DoStart (kLoading/kReady/kFailed), DoUpdate, DoStop (kStopping/kDone), timeout handling | [module-lifecycle.md](../../features/dia/diaapplicationflow/module-lifecycle.md) | Approved |
| Stage System | Config-declared stages, app-wide TransitionTo, diff-based module swap, auto-advance for boot stages | [stage-system.md](../../features/dia/diaapplicationflow/stage-system.md) | Approved |
| Registration | One-liner DIA_MODULE macro, TypeRegistry, constexpr StringCRC type IDs | [registration.md](../../features/dia/diaapplicationflow/registration.md) | Approved |
| Config Format v2 | Manifest schema with stages, streams, module stage membership, version field | [config-format.md](../../features/dia/diaapplicationflow/config-format.md) | Approved |
| Validation | Full manifest validation at load (deps, cycles, streams, stage coverage, type existence) | [validation.md](../../features/dia/diaapplicationflow/validation.md) | Approved |
| Streams | Framework-owned FrameStream/EventStream, config-declared, StreamReader/StreamWriter handles | [streams.md](../../features/dia/diaapplicationflow/streams.md) | Approved |
| Error Handling | Timeout per module, assert/rollback/shutdown policies, transition failure recovery | [error-handling.md](../../features/dia/diaapplicationflow/error-handling.md) | Approved |
| Inspectable Interface | IApplicationInspectable for debug/editor/test consumers | [inspectable.md](../../features/dia/diaapplicationflow/inspectable.md) | Approved |

## Platform Primitives Used

- **DiaCore/Containers** — DynamicArrayC, HashTableC for module/stage storage
- **DiaCore/CRC** — StringCRC for all IDs (PU, Module, Stage, Stream)
- **DiaCore/Frame** — FrameStream<T> as the underlying stream implementation
- **DiaCore/Time** — TimeAbsolute for timestamps, delta time, timeout tracking
- **DiaCore/Threading** — std::thread, std::mutex, std::atomic for thread safety
- **DiaCore/Memory** — UniquePtr for owned modules
- **DiaCore/Core** — DIA_ASSERT for invariant checking
- **DiaLogger** — Log channels for transition events, errors, timeouts
- **DiaSerializer** — ISerializer, SerializeResult for manifest loading

## Dependencies on Other Systems

**Required:**
- **DiaCore** — Containers, StringCRC, FrameStream, Time, Threading, Memory
- **DiaLogger** — Logging transition events, errors, timeouts
- **DiaSerializer** — Manifest file loading (ISerializer interface, SerializeResult)

**Consumers (depend on this system):**
- **DiaGame** — Loads .diagame, creates Application from manifest
- **CluicheTest** — Game application built on this framework
- **DiaApplicationEditor** — Edits .diaapp manifests, connects to IApplicationInspectable
- **DiaTestHarness** — E2E automation via DiaAPI commands adapting IApplicationInspectable
- **GoogleTests** — Unit tests for framework behavior

## Out of Scope

- **DiaAPI commands** — IApplicationInspectable is exposed; adapter lives elsewhere
- **Editor UI** — separate system (DiaApplicationFlowEditor)
- **Visual debugging** — DiaVisualDebugger is independent
- **Asset loading** — modules call DiaAssetRuntime; framework just gates on readiness
- **DiaStateMachine** — not used; stage transitions are simpler than FSM
- **Shared modules across PUs** — not supported; streams handle cross-PU data
- **Stage parameters** — game state lives in retained modules, not in TransitionTo
- **Phase concept** — removed entirely; stages replace phases
- **MessageBus** — removed; EventStream replaces it
- **Hot reload manager** — removed; re-enter stage achieves same result
- **PU parent-child tree** — removed; flat PU list with config-declared startup order
- **Multiple module access patterns** — ModuleRef<T> is the only pattern

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-001 | Config is sole source of truth for all structural wiring | Eliminates redundant code+config declarations; manifest is a complete, readable description of app structure | All features | Accepted | Yes |
| SD-002 | Phase concept removed, replaced by config-declared Stages | Phases added complexity without matching real usage (boot→run). Stages are simpler, fully config-driven, and handle the real need (content switching). | All features | Accepted | Yes |
| SD-003 | ModuleRef<T> is the sole module access pattern | Eliminates confusion of 3 access patterns (GetModule, FindModule, ModuleRef). One way to do one thing. | All features | Accepted | Yes |
| SD-004 | TransitionTo is app-wide — all PUs transition together | Stages are an app concept. Infrastructure modules use "all" keyword to stay active. Simplifies reasoning about system state. | Stage System | Accepted | Yes |
| SD-005 | Transitions execute at start of next frame (async, queued) | Prevents torn state mid-update. Predictable timing. Same as old QueuePhaseTransition but mandatory. | Stage System | Accepted | Yes |
| SD-006 | Streams are framework-owned, declared in config | Streams are structural wiring (like PUs). Config makes dataflow visible and inspectable. StreamReader/StreamWriter are typed handles. | Streams | Accepted | Yes |
| SD-007 | MessageBus removed — EventStream replaces inter-module events | Unified communication model. One concept (streams) for all data flow. Fewer patterns to learn. | Streams | Accepted | Yes |
| SD-008 | One-liner registration macro (DIA_MODULE) | Eliminates ~40 lines boilerplate per class. Still maps StringCRC→class for config resolution. | Registration | Accepted | Yes |
| SD-009 | Module failure: assert debug, rollback release, shutdown on boot | Fail-fast in development. Graceful degradation in production. Boot failure is unrecoverable. | Error Handling | Accepted | Yes |
| SD-010 | PU startup order = array order in config | Explicit, readable, simple. No inference magic. Reorder the JSON array to change startup order. | Config Format | Accepted | Yes |
| SD-011 | Shutdown is framework-level (RequestShutdown), not a stage | Shutdown is lifecycle termination, not game content. Clean separation of concerns. | All features | Accepted | Yes |
| SD-012 | Hot reload = re-enter current stage | TransitionTo(currentStage) runs DoStop/DoStart on non-retained modules. Simple, no extra mechanism. | Stage System | Accepted | Yes |
| SD-013 | DoStart returns StartResult (kLoading/kReady/kFailed) | Module startup can span multiple frames (async asset loading). Framework calls DoStart each frame until ready. Gates transition completion. | Module Lifecycle | Accepted | Yes |
| SD-014 | Full manifest validation at load time, fail-fast | Config errors caught immediately before any runtime. Never runs in broken state. Editor validates at edit time (redundant safety net). | Validation | Accepted | Yes |
| SD-015 | No shared modules across PUs | Not a real case. Streams handle cross-PU data. Each PU owns its modules. Simpler ownership model. | All features | Accepted | Yes |
| SD-016 | IApplicationInspectable exposes runtime state | Clean interface that debug, editor, and test consumers can adapt without coupling to framework internals. | Inspectable | Accepted | Yes |
| SD-017 | Clean break — no backward compatibility with v1 | Shim code adds weeks of throwaway work. Old code deleted, old specs superseded, modules rewritten in one pass. | All features | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | Use StringCRC for all entity/component IDs | PU, Module, Stage, Stream IDs are all StringCRC. ModuleRef resolves via StringCRC. Config uses string names that map to CRC at load. |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | **This decision is being updated.** Phase→Stage. PU and Module remain. Platform spec PD-002 requires update to reflect Stage model. |
| PD-004 | Platform | No STL containers in public APIs | All public APIs use DiaCore containers (DynamicArrayC, HashTableC). Internal implementation may use std:: where appropriate. |
| PD-005 | Platform | x64 only | No 32-bit considerations. |
| PD-006 | Platform | Visual Studio project files are source of truth | DiaApplicationFlow.vcxproj manually maintained. |
| PD-007 | Platform | C++20 required | Enables concepts for registration, constexpr StringCRC, designated initializers for config structs. |
| PD-008 | Platform | Directory.Build.props owns build settings | No per-project overrides of OutDir/IntDir/toolchain. |
| PD-009 | Platform | Generated output in Cluiche/out/<AppName>/ | Any generated files from validation/tooling go here. |
| PD-010 | Platform | .diagame is project root, .diastage declares stage metadata | .diagame → .diaapp (manifest) and .diastage (stage metadata). This system consumes parsed manifests. |
| AD-001 | Dia App | Module system with YAML frontmatter | dia.applicationflow.architecture.module.md must be updated for v2 API. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Dia App | Namespace: Dia::<Module>:: | All code in Dia::ApplicationFlow:: namespace. |
| AD-004 | Dia App | PU/Phase/Module for app structure | **Being superseded by this system.** AD-004 will be updated to PU/Stage/Module. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Stages | Should "all" be a reserved keyword in stage membership or just shorthand for listing every stage? | Reserved keyword — framework interprets it as "always active, never stopped during transitions." Explicit and less error-prone than listing every stage name. |
| 2 | Streams | Should streams support multiple writers, or strictly one writer per stream? | Default: single writer. Config can declare `"multi_writer": true` for cases like LoadingScreen + DummyLevel both writing to SimToRender. Framework doesn't enforce ordering between writers. |
| 3 | Transitions | What happens if TransitionTo is called during an in-progress transition? | Queue it. When current transition completes, immediately begin the next queued transition. Only one pending transition allowed — subsequent calls overwrite the queue (latest wins). |
| 4 | Lifecycle | Should DoUpdate receive deltaTime, or should modules query a TimeServer module? | DoUpdate receives deltaTime from the framework (based on PU frequency). Modules can also depend on TimeServer for game-time (pause-aware). Both are valid. |
| 5 | Config | Should module config (custom per-type key-value pairs) be part of the manifest or separate files? | Part of the manifest in a `"config"` field per module entry. Keeps everything in one file. Large configs can use a `"config_file"` reference (future extension). |
| 6 | Validation | Should validation warnings (non-fatal issues like orphaned streams) block startup or just log? | Log as warnings. Only errors (missing deps, cycles, invalid references) block startup. Warnings surface in editor validation bar. |
| 7 | Threading | Should the main PU's Update() drive the application loop, or should Application own the loop? | Application::Update() drives the loop. It ticks the main PU inline, while dedicated-thread PUs self-tick. Application also checks for pending transitions and shutdown. |
| 8 | Registration | Should DIA_MODULE support constructor parameters beyond instanceId? | No. Module receives instanceId from framework + config JSON blob. Custom initialization happens in DoStart, not constructor. Keeps registration uniform. |
| 9 | Migration | How do we update PD-002 and AD-004 (binding platform/app decisions)? | After this spec is Approved, update PD-002 text to "PU/Stage/Module" and AD-004 to match. Add supersession note. This is a coordinated change — spec approval gates the decision update. |
| 10 | Scope | Should this system own FrameStream implementation or just consume DiaCore's? | Consume DiaCore's FrameStream. This system provides the typed handles (StreamReader/Writer) and config-driven creation/injection. The underlying FrameStream<T> lives in DiaCore/Frame. |

## Status

`Approved` — 2026-05-08. Supersedes DiaApplicationFlow v1.

**Plan:** [diaapplicationflow.plan.md](diaapplicationflow.plan.md)

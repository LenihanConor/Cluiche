# System Spec: DiaApplication

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaApplication is the application framework system that provides the runtime architecture for structuring game applications and tools. It implements the ProcessingUnit/Phase/Module pattern, enabling multi-threaded execution with explicit state lifecycle management, inter-module messaging, error handling, and runtime module hot-reloading. This system defines the core abstractions that all Dia-based applications use to organize their execution flow.

## Responsibilities

- Provide ProcessingUnit/Phase/Module architecture for application structure
- Manage state transitions and lifecycle (Constructed → Start → Update → Stop)
- Enable multi-threaded execution with thread-safe phase transitions
- Support asynchronous module startup with dependency tracking
- Provide message bus for decoupled inter-module communication
- Handle errors gracefully with callback system and error history
- Support runtime module replacement (hot reload) for rapid iteration
- Manage module ownership with both raw pointers (external ownership) and smart pointers (internal ownership)
- Enforce module dependencies and phase transition constraints

## Public Interfaces

### Core Classes

**StateObject (Base):**
```cpp
namespace Dia::Application {
    class StateObject {
    public:
        enum class StateEnum {
            kConstructed,
            kFlaggedToStart,
            kRunning,
            kFlaggedToStop,
            kNotRunning
        };
        
        void BuildDependancies(IBuildDependencyData* buildDependencies);
        OpertionResponse Start(const IStartData* startData);
        void Update();
        void Stop();
        
        const StringCRC& GetUniqueId() const;
        StateEnum GetState() const;
        bool HasStarted() const;
    };
}
```

**ProcessingUnit:**
```cpp
namespace Dia::Application {
    class ProcessingUnit : public StateObject {
    public:
        ProcessingUnit(const StringCRC& uniqueId, float hz = -1.0f);
        
        // Phase/Module registration
        void AddPhase(Phase* phase);
        void AddModule(Module* module);
        void AddPhaseWithOwnership(UniquePtr<Phase> phase);
        void AddModuleWithOwnership(UniquePtr<Module> module);
        
        // Phase management
        void SetInitialPhase(Phase* phase);
        void AddPhaseTransiton(Phase* startPhase, Phase* endPhase);
        void TransitionPhase(const StringCRC& phaseCrc);       // Immediate
        void QueuePhaseTransition(const StringCRC& phaseCrc);  // Thread-safe
        
        Phase* GetCurrentPhase();
        bool ContainsModule(const StringCRC& crc) const;
        
        // Threading
        void EnableThreadLimiting(float hz);
        void DisableThreadLimiting();
        void operator()();  // Thread entry point
        
        // Error handling
        void SetErrorCallback(ErrorCallback callback);
        void ReportError(const ErrorInfo& error);
        const std::vector<ErrorInfo>& GetErrorHistory() const;
        void ClearErrorHistory();
        
        // Message bus
        MessageBus& GetMessageBus();
        
        // Hot reload
        HotReloadManager* GetHotReloadManager();
    };
}
```

**Phase:**
```cpp
namespace Dia::Application {
    class Phase : public StateObject {
    public:
        Phase(ProcessingUnit* pu, const StringCRC& uniqueId);
        
        void AddModule(Module* module);
        void QueuePhaseTransition(const StringCRC& crc);
        void TransitionTo(Phase* endPhase);
        bool ContainsModule(const StringCRC& crc) const;
        
        // Lifecycle hooks
        virtual void BeforeModulesStart();
        virtual void AfterModulesStart();
        virtual void BeforeModulesUpdate();
        virtual void AfterModulesUpdate();
        virtual void BeforeModulesStop();
        virtual void AfterModulesStop();
        
        virtual bool FlaggedToStopUpdating() const = 0;
        
        template <class T> T* GetModule();
        Module* GetModule(const StringCRC& crc);
    };
}
```

**Module:**
```cpp
namespace Dia::Application {
    class Module : public StateObject {
    public:
        enum class RunningEnum {
            kIdle,    // Module does not need Update() called
            kUpdate   // Module needs Update() called each frame
        };
        
        Module(ProcessingUnit* pu, const StringCRC& uniqueId, RunningEnum mode);
        
        void AddDependancy(Module* dependency);
        bool HasAllDependanciesStarted() const;
        bool RequiresUpdating() const;
        
        void RetainThroughTransition(const Phase* start, const Phase* end);
        
        template <class T> T* GetModule();
        Module* GetModule(const StringCRC& uniqueId);
    };
}
```

**MessageBus:**
```cpp
namespace Dia::Application {
    class MessageBus {
    public:
        // Subscription
        void Subscribe(const StringCRC& messageType,
                      const StringCRC& subscriberId,
                      MessageHandler handler);
        void Unsubscribe(const StringCRC& messageType,
                        const StringCRC& subscriberId);
        
        // Sending (synchronous)
        void SendImmediate(const StringCRC& messageType,
                          const StringCRC& senderId,
                          const void* data,
                          size_t dataSize);
        
        // Posting (asynchronous, queued)
        void PostMessage(const StringCRC& messageType,
                        const StringCRC& senderId,
                        const void* data,
                        size_t dataSize);
        
        // Processing (called by ProcessingUnit)
        void ProcessQueue();
        
        // Type-safe templates
        template<typename T>
        void SendImmediate(const StringCRC& type, const StringCRC& sender, const T& data);
        
        template<typename T>
        void PostMessage(const StringCRC& type, const StringCRC& sender, const T& data);
    };
    
    struct Message {
        StringCRC type;
        StringCRC senderId;
        TimeAbsolute timestamp;
        const void* data;
        size_t dataSize;
        
        template<typename T> const T* GetData() const;
    };
    
    using MessageHandler = std::function<void(const Message&)>;
}
```

**HotReloadManager:**
```cpp
namespace Dia::Application {
    class HotReloadManager {
    public:
        enum class ReloadResult {
            kSuccess,
            kModuleNotFound,
            kModuleNotReloadable,
            kVersionIncompatible,
            kModuleRunning,
            kDependentModulesRunning,
            kNewModuleStartFailed,
            kInvalidProcessingUnit
        };
        
        HotReloadManager(ProcessingUnit* pu);
        
        ReloadResult ReplaceModule(const StringCRC& oldModuleId,
                                   Module* newModule);
        
        ReloadResult ReplaceModuleInPhase(const StringCRC& phaseId,
                                          const StringCRC& oldModuleId,
                                          Module* newModule);
        
        static const char* GetResultString(ReloadResult result);
    };
}
```

**Error Handling:**
```cpp
namespace Dia::Application {
    enum class ErrorCode {
        kSuccess,
        kNullPointer,
        kInvalidState,
        kTimeout,
        kCircularDependency,
        kStartupFailed,
        kModuleNotFound,
        kTransitionNotAllowed,
        kAsyncTimeout,
        kResourceLoadFailed,
        kUnknown
    };
    
    struct ErrorInfo {
        ErrorCode code;
        StringCRC contextId;
        const char* message;
        TimeAbsolute timestamp;
        
        bool IsSuccess() const;
        bool IsFailure() const;
        const char* GetErrorCodeString() const;
    };
    
    typedef void (*ErrorCallback)(const ErrorInfo& error);
}
```

### Events Emitted

- **Phase Transitions**: When QueuePhaseTransition() is called, transition occurs at end of current update
- **Module State Changes**: BuildDependancies → Start → Update (loop) → Stop
- **Error Events**: Via ErrorCallback when ReportError() is called
- **Message Events**: Dispatched via MessageBus when ProcessQueue() is called
- **Hot Reload Events**: Module replacement (implicit via HotReloadManager)

### Data Contracts

**Lifecycle State Machine:**
```
kConstructed → kFlaggedToStart → kRunning → kFlaggedToStop → kNotRunning
                    ↑                            ↓
                    └────── (restart) ───────────┘
```

**Phase Transition Rules:**
- Transitions defined via AddPhaseTransiton(startPhase, endPhase)
- Only defined transitions are allowed (enforced by assertions)
- QueuePhaseTransition() is thread-safe; TransitionPhase() is immediate
- Retained modules (via RetainThroughTransition) keep running across transition

**Module Dependencies:**
- Modules declare dependencies via AddDependancy()
- Phase start waits for all module dependencies to complete (async support)
- Circular dependencies detected during BuildDependancies()

**Threading Model:**
- ProcessingUnits can run on separate threads via operator()()
- QueuePhaseTransition() is thread-safe (uses mutex)
- TransitionPhase() should only be called from owning thread
- Module Updates occur sequentially in registration order within a phase

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| State Lifecycle | StateObject base class with Constructed → Start → Update → Stop | [state-lifecycle.md](../../features/dia/diaapplication/state-lifecycle.md) | Done |
| ProcessingUnit | Thread-capable execution container with phase/module management | [processing-unit.md](../../features/dia/diaapplication/processing-unit.md) | Done |
| Phase Management | Execution stages with state transitions and module dependencies | [phase-management.md](../../features/dia/diaapplication/phase-management.md) | Done |
| Module System | Functional units with dependency tracking and lifecycle hooks | [module-system.md](../../features/dia/diaapplication/module-system.md) | Done |
| Message Bus | Thread-safe inter-module communication with queued dispatch | [message-bus.md](../../features/dia/diaapplication/message-bus.md) | Done |
| Error Handling | ErrorCallback system with ErrorInfo history (last 100 errors) | [error-handling.md](../../features/dia/diaapplication/error-handling.md) | Done |
| Hot Reload | Runtime module replacement with state transfer | [hot-reload.md](../../features/dia/diaapplication/hot-reload.md) | Done |
| Data-Driven Application System | Type registration, manifest loading, and introspection for editor-driven application topology definition | [data-driven-application-system.md](../../features/dia/diaapplication/data-driven-application-system.md) | Done |
| Manifest Imports (Phase A) | Recursive import resolution in ApplicationManifestLoader; merged manifests with provenance tracking | [manifest-imports.md](../../features/dia/diaapplication/manifest-imports.md) | Approved |
| PU Parent-Child Tree (Phase B) | ProcessingUnit gains AddChildPU/GetParent/GetChildren; automatic thread spawn/join lifecycle | [pu-parent-child-tree.md](../../features/dia/diaapplication/pu-parent-child-tree.md) | Approved |
| Stage Manifests (Phase C) | DummyStage→DummyStage rename; stage .diaapp manifests declaring phase/module injections into parent PUs | [stage-manifests.md](../../features/dia/diaapplication/stage-manifests.md) | Approved |

## Platform Primitives Used

- **DiaCore/Containers** - DynamicArrayC, HashTable for phase/module storage
- **DiaCore/CRC** - StringCRC for all IDs (ProcessingUnit, Phase, Module, messages)
- **DiaCore/Time** - TimeAbsolute for timestamps, TimeThreadLimiter for frequency control
- **DiaCore/Threading** - std::thread, std::mutex, std::atomic for thread safety
- **DiaCore/Memory** - UniquePtr for owned phase/module management
- **DiaCore/Core** - Assertions (DIA_ASSERT) for invariant checking

## Dependencies on Other Systems

**Required:**
- **DiaCore** - Foundation containers, CRC, time, threading primitives

**Optional:**
- None - DiaApplication is self-contained

**Dependents:**
- All Dia-based applications (CluicheTest, GoogleTests, future games) use DiaApplication to structure execution
- Most Dia systems (DiaGraphics, DiaInput, DiaUI, etc.) provide Modules that run within this framework

## Out of Scope

- **Network Distribution**: ProcessingUnits are local-thread only, no distributed execution
- **Serialization**: No built-in save/load of application state (modules responsible for own state)
- **Scripting Integration**: DiaApplication is C++ only; scripting provided by DiaPython
- **GUI Debugging**: No built-in visual debugger for phase/module state (future DiaEditor feature)
- **Dynamic Module Loading**: No DLL/shared library loading; modules compiled into executable
- **Parallel Module Execution**: Modules within a phase run sequentially, not in parallel
- **Advanced Scheduling**: No priority queues or frame-budget scheduling; simple sequential execution

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-001 | ProcessingUnit/Phase/Module three-level hierarchy | Separates threading (PU), lifecycle stages (Phase), and functionality (Module); enables flexible composition | All features | Accepted | Yes |
| SD-002 | StateObject base class with explicit state machine | Prevents invalid operations (e.g., Update before Start); enables assertion-based validation | All features | Accepted | Yes |
| SD-003 | QueuePhaseTransition() is thread-safe, TransitionPhase() is immediate | Allows external threads (e.g., input, network) to trigger transitions safely; immediate available for deterministic testing | All features | Accepted | Yes |
| SD-004 | Modules identified by StringCRC with template GetModule<T>() | Type-safe retrieval with compile-time name hashing; consistent with platform ID system | All features | Accepted | Yes |
| SD-005 | Phases define module dependencies, not vice versa | Phase owns its execution context; module is reusable across phases | All features | Accepted | Yes |
| SD-006 | Support both raw pointer (external ownership) and UniquePtr (internal ownership) module/phase management | Flexibility for app-managed objects vs framework-managed objects; avoids forcing ownership model on users | All features | Accepted | Yes |
| SD-007 | Message bus uses type-erased void* with size tracking | No template instantiation bloat; runtime type checking via size; consistent with C engine style | Message Bus | Accepted | Yes |
| SD-008 | Error history limited to 100 most recent errors | Prevents unbounded memory growth; sufficient for debugging | Error Handling | Accepted | Yes |
| SD-009 | Hot reload requires module opt-in (CanHotReload) | Safety first - modules must explicitly support state transfer; prevents accidentally breaking modules | Hot Reload | Accepted | Yes |
| SD-010 | No automatic module dependency resolution - explicit AddDependancy() | Transparent dependencies; no hidden magic; easier debugging | All features | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | Use StringCRC for all entity/component IDs | ProcessingUnit, Phase, Module, Message types all identified by StringCRC. GetModule<T>() relies on T::kUniqueId being a StringCRC. |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture for app structure | This is the system that implements PD-002 for the entire platform. All binding rules defined here. |
| PD-004 | Platform | No STL containers in public APIs | Use DynamicArrayC, HashTable instead of std::vector, std::map. Exception: std::vector for owned modules/phases (internal) and error history (internal). |
| PD-006 | Platform | Visual Studio project files are source of truth | DiaApplication.vcxproj with filters. All headers/cpp files manually added. |
| AD-001 | Dia App | Module system with YAML frontmatter documentation | Create dia.application.architecture.module.md with public API, dependencies, responsibilities. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004. Internal use of std::vector for ownership is acceptable. |
| AD-003 | Dia App | Namespace convention: `Dia::<Module>::` | All code in `Dia::Application::` namespace. |
| AD-004 | Dia App | ProcessingUnit/Phase/Module for application structure | This system IS the implementation of AD-004 / PD-002. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Purpose | Should DiaApplication also manage application entry point (main)? | No - applications provide their own main(). DiaApplication provides ProcessingUnit that main() instantiates. |
| 2 | Public Interfaces | Should modules support parallel execution within a phase? | No - keep simple sequential execution. Parallelism achieved via multiple ProcessingUnits on different threads. |
| 3 | Threading | Should ProcessingUnits automatically spawn threads or require explicit std::thread? | Explicit - provides control. ProcessingUnit::operator()() is thread entry point; caller wraps in std::thread. |
| 4 | Dependencies | Should circular module dependencies be detected and prevented? | Yes - BuildDependancies() can detect cycles. Currently asserts; future could provide error recovery. |
| 5 | Message Bus | Should message queue have size limits? | No - unbounded for now. Messages processed each frame, so growth limited by frame rate. Future monitoring could warn on excessive queue size. |
| 6 | Hot Reload | Should hot reload support swapping phases, not just modules? | Not yet - phases define structure, modules provide functionality. Phase swapping is more complex (requires entire state machine rebuild). Future feature. |
| 7 | Error Handling | Should errors stop execution or allow continuation? | Depends on error severity. ErrorCallback can inspect ErrorCode and decide (e.g., kStartupFailed might stop, kTimeout might retry). Framework is agnostic. |
| 8 | Scope | Should DiaApplication provide a default main() implementation? | No - too opinionated. Each application has different initialization needs. Provide example in docs/reference instead. |
| 9 | Lifecycle | Should modules support Pause/Resume states (not just Start/Stop)? | Not yet - adds complexity. Applications can achieve pause via FlaggedToStopUpdating() without full Stop(). Future enhancement if needed. |
| 10 | Threading | What is the expected pattern for inter-ProcessingUnit communication? | Message bus is intra-ProcessingUnit. For inter-PU communication, use shared state with mutexes or separate message queues. Future spec if common pattern emerges. |

## Status

`Active` - System is implemented and actively used by all Dia-based applications.

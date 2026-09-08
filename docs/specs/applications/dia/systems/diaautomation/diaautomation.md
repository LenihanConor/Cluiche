# System Spec: DiaAutomation

## Parent Application
@docs/specs/applications/dia/dia.md

## Research
@docs/research/e2e_testing/design-decisions.md

## Purpose

DiaAutomation is a pure capability layer that makes Dia applications externally driveable and observable for automation purposes. It provides registries for checkpoints (game-defined validation), pause/resume callbacks, and navigation hold (transition guard management). Commands are registered with DiaAPI (JSON path) so that external tools (pytest orchestrator, debug consoles, editor) can drive and inspect the application over DiaDebugServer's existing WebSocket.

DiaAutomation is **not** a module and has **no lifecycle of its own**. It is a service instantiated and wired by an application-level `AutomationModule` (which lives in CluicheGameBaseline or CluicheEditor, not in Dia). This separation preserves the Orchestrator / Capability / Game layering from the design-decisions doc.

## Responsibilities

- Provide checkpoint registry — modules register named validation functions; framework auto-clears on module stop
- Provide pause/resume callback registry — modules register pause/resume functions; orchestrator invokes them
- Manage navigation hold state — holds a transition guard that blocks auto-advance until `navigate_to` arrives
- Provide CI safety primitives — heartbeat monitor (configurable timeout), disconnect response (release + shutdown)
- Register `dia.automation.*` commands with DiaAPI (JSON callback path, consistent `{success, data/error}` envelope)
- Emit automation lifecycle events for observability

## Non-Responsibilities

- **Transport** — DiaDebugServer owns WebSocket; DiaAutomation has no transport dependency
- **Application lifecycle** — AutomationModule (game-side) wires DiaAutomation into the app
- **Orchestration logic** — Python/pytest owns scenario flow, assertions, suite gating
- **Stage-specific validation** — Game modules register checkpoints; DiaAutomation just holds and invokes them
- **Transition guard mechanism** — DiaApplicationFlow owns guards; DiaAutomation is a consumer
- **Ship/retail concerns** — Deferred; `FindModule<AutomationModule>()` returns nullptr if not in manifest

## Public Interfaces

### AutomationService (core class)

```cpp
namespace Dia { namespace Automation {

    struct CheckpointResult
    {
        bool passed;
        const char* message;
        float durationMs;
    };

    using CheckpointFn = std::function<CheckpointResult()>;
    using PauseResumeFn = std::function<void()>;

    class AutomationService
    {
    public:
        AutomationService(Dia::ApplicationFlow::Application& app);

        // --- Checkpoint Registry ---
        void RegisterCheckpoint(Dia::ApplicationFlow::Module* owner,
                                const Dia::Core::StringCRC& name,
                                CheckpointFn fn);
        void UnregisterCheckpoints(Dia::ApplicationFlow::Module* owner);
        CheckpointResult RunCheckpoint(const Dia::Core::StringCRC& name) const;
        bool HasCheckpoint(const Dia::Core::StringCRC& name) const;

        // --- Pause/Resume Registry ---
        void RegisterPauseCallback(Dia::ApplicationFlow::Module* owner,
                                   PauseResumeFn pause,
                                   PauseResumeFn resume);
        void UnregisterPauseCallbacks(Dia::ApplicationFlow::Module* owner);
        void Pause();
        void Resume();

        // --- Navigation Hold ---
        void EnableNavigationHold();
        void ReleaseNavigationHold(const Dia::Core::StringCRC& target);
        bool IsHolding() const;

        // --- CI Safety ---
        void EnableHeartbeatMonitor(float timeoutSeconds = 30.0f);
        void DisableHeartbeatMonitor();
        void ResetHeartbeat();
        void OnDisconnect();
        void TickHeartbeat(float deltaTime);

        // --- Command Registration ---
        void RegisterCommands();

        // --- Query ---
        bool IsPaused() const;

    private:
        Dia::ApplicationFlow::Application& mApp;

        struct CheckpointEntry
        {
            Dia::ApplicationFlow::Module* owner;
            Dia::Core::StringCRC name;
            CheckpointFn fn;
        };

        struct PauseResumeEntry
        {
            Dia::ApplicationFlow::Module* owner;
            PauseResumeFn pause;
            PauseResumeFn resume;
        };

        Dia::Core::Containers::DynamicArrayC<CheckpointEntry, 64> mCheckpoints;
        Dia::Core::Containers::DynamicArrayC<PauseResumeEntry, 16> mPauseCallbacks;

        bool mHolding = false;
        bool mPaused = false;
        bool mHeartbeatEnabled = false;
        float mHeartbeatTimeout = 30.0f;
        float mHeartbeatElapsed = 0.0f;
    };

}}
```

### Commands (registered with DiaAPI JSON path)

| Command | Params | Response (data field) | Notes |
|---|---|---|---|
| `dia.automation.navigate_to` | `{"target": "<stage>"}` | `{}` | Releases hold, queues TransitionTo(target) |
| `dia.automation.pause` | `{}` | `{"paused": true}` | Invokes all registered pause callbacks |
| `dia.automation.resume` | `{}` | `{"resumed": true}` | Invokes all registered resume callbacks |
| `dia.automation.validate` | `{"checkpoint": "<name>"}` | `{"passed": bool, "message": "...", "duration_ms": float}` | Runs checkpoint fn, returns result |

All responses use the consistent `{success, data/error}` envelope from baseline-commands.

### Events Emitted

Events emitted on the `$lifecycle` stream (reusing existing reserved stream infrastructure):

| Event | When |
|---|---|
| `kAutomationHoldEnabled` | `EnableNavigationHold()` called |
| `kAutomationHoldReleased` | `ReleaseNavigationHold(target)` called |
| `kAutomationPaused` | `Pause()` called |
| `kAutomationResumed` | `Resume()` called |
| `kAutomationDisconnect` | `OnDisconnect()` triggered |
| `kAutomationHeartbeatTimeout` | Heartbeat expired |

### Data Contracts

**Checkpoint lifetime:**
```
Module DoStart → RegisterCheckpoint(this, name, fn)
Module DoStop  → UnregisterCheckpoints(this)  [auto-called by framework]
```

**Navigation hold flow:**
```
AutomationModule::DoStart() → service.EnableNavigationHold()
  → Registers transition guard returning Hold
  → App blocks at current stage
Orchestrator sends dia.automation.navigate_to {target: "X"}
  → service.ReleaseNavigationHold("X")
  → Guard returns Allow
  → App transitions to X
  → Guard re-arms (hold again until next navigate_to)
```

**Heartbeat flow:**
```
Any incoming command → ResetHeartbeat()
TickHeartbeat(dt) each frame:
  if elapsed > timeout → OnDisconnect() [release + shutdown]
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Module and Build | DiaAutomation vcxproj, namespace, module doc, project references | [module-and-build.md](module-and-build.md) | Approved |
| Checkpoint Registry | Register/unregister/run checkpoints, module-scoped lifetime, `dia.automation.validate` command | [checkpoint-registry.md](checkpoint-registry.md) | Approved |
| Navigation Hold | Transition guard management, EnableNavigationHold/Release, re-arm after transition, `dia.automation.navigate_to` command | [navigation-hold.md](navigation-hold.md) | Approved |
| Pause Resume | Pause/resume callback registry, invoke all callbacks, `dia.automation.pause`/`dia.automation.resume` commands | [pause-resume.md](pause-resume.md) | Approved |
| CI Safety | Heartbeat monitor (configurable timeout), OnDisconnect handler, auto-release + RequestShutdown on failure | [ci-safety.md](ci-safety.md) | Approved |
| Metric Assertions | `dia.automation.get_metric` command + pytest `assert_metric` fixture for live metric threshold checks | [metric-assertions.md](metric-assertions.md) | Done |

## Platform Primitives Used

- **DiaCore/Containers** — DynamicArrayC for registries
- **DiaCore/CRC** — StringCRC for checkpoint names, command names
- **DiaCore/Time** — delta time for heartbeat tick
- **DiaCore/Core** — DIA_ASSERT for invariant checking
- **DiaObservation/Log** — DIA_LOG_* for automation events
- **DiaApplicationFlow** — Module* for ownership, Application& for guard registration + shutdown + transitions
- **DiaAPI** — RegisterCommandJson for command surface

## Dependencies on Other Systems

**Required:**
- **DiaCore** — Containers, StringCRC, Time
- **DiaApplicationFlow** — Transition guards, Module ownership, Application control (RequestShutdown, TransitionTo)
- **DiaAPI** — JSON command registration (RegisterCommandJson, ExecuteCommandJson)
- **DiaObservation** — Logging

**Consumers (depend on this system):**
- **CluicheGameBaseline** — `AutomationModule` instantiates AutomationService, wires disconnect callback from DiaDebugServer, enables hold
- **CluicheEditor** — `EditorAutomationModule` (future, item 7)
- **Tools/orchestrator/** — Python pytest plugin (external, no compile dependency)
- **GoogleTests** — Unit tests for service behaviour

## Out of Scope

- **Transport / WebSocket** — DiaDebugServer concern; DiaAutomation receives calls, doesn't manage connections
- **Scenario logic** — pytest orchestrator
- **Visual regression** — declined in design decisions §14
- **Input replay** — declined in design decisions §14
- **Parallel scenario execution** — sequential for now (§14)
- **Ship/retail build exclusion** — deferred (§12); FindModule null-safety sufficient today
- **AutomationModule wiring** — lives in CluicheGameBaseline, not in Dia
- **AutomationModuleBase extraction** — deferred to item 6b (evaluate from working code)

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-AUT-001 | DiaAutomation is a service, not a Module | Keeps it transport-agnostic and lifecycle-free. Application-level AutomationModule does the wiring. Matches "pure capability" from design-decisions §2. | All features | Accepted | Yes |
| SD-AUT-002 | Checkpoint lifetime tied to owning Module | Same pattern as transition guards. Framework clears checkpoints for stopped modules. No manual cleanup needed in game code. | Checkpoint Registry | Accepted | Yes |
| SD-AUT-003 | Navigation hold re-arms after each transition | Orchestrator must explicitly navigate to every stage. App never auto-advances while automation is active. One navigate_to = one transition. | Navigation Hold | Accepted | Yes |
| SD-AUT-004 | CI safety: connection-loss primary, heartbeat backup | Belt and suspenders. on_close is immediate; heartbeat catches zombie orchestrators. Both trigger release + shutdown. Design-decisions §10. | CI Safety | Accepted | Yes |
| SD-AUT-005 | Commands use consistent {success, data/error} envelope | Matches baseline-commands (same DiaAPI JSON path). Python client parses uniformly. | All features | Accepted | Yes |
| SD-AUT-006 | Pause/resume is cooperative — callbacks, not framework freeze | Game defines what "pause" means (stop physics? freeze animation? mute audio?). DiaAutomation just invokes registered callbacks. No frame-level suspension. | Pause Resume | Accepted | Yes |
| SD-AUT-007 | Any incoming automation command resets heartbeat | Simple, robust. No separate ping/pong protocol needed. Every command proves liveness. | CI Safety | Accepted | Yes |
| SD-AUT-008 | Events on $lifecycle stream, not a new $automation stream | Reuses existing infrastructure. Keeps event consumers simple (one subscription). Follows SD-018 from DiaApplicationFlow. | All features | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all IDs | Checkpoint names, command names, stage targets all StringCRC. |
| PD-004 | Platform | No STL containers in public APIs | Registries use DynamicArrayC. CheckpointFn/PauseResumeFn use std::function (callable, not container — same precedent as TransitionGuardFn). |
| PD-005 | Platform | x64 only | No platform-specific code. |
| PD-006 | Platform | VS project files source of truth | New DiaAutomation.vcxproj manually maintained. |
| PD-007 | Platform | C++20 required | Lambdas, std::function, enum class — all valid. |
| PD-008 | Platform | Directory.Build.props owns build settings | No per-project overrides. |
| PD-009 | Platform | Generated output under Cluiche/out/ | No generated output from this system. |
| AD-001 | Dia App | Module docs with YAML frontmatter | `dia.automation.architecture.module.md` created with full YAML frontmatter. |
| AD-002 | Dia App | No STL in public APIs | See PD-004. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::Automation::` namespace. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Service ownership | Who instantiates AutomationService? The AutomationModule? What's the lifetime? | AutomationModule creates it in `DoStart()` and destroys it in `DoStop()`. AutomationModule has `stages: ["all"]`, so the service lives for the entire session. Module passes `Application&` to the constructor. |
| 2 | Guard re-arm | After navigate_to releases the guard, how does it re-arm for the next stage? Does the guard fn flip a bool back to Hold after the transition commits? | Yes. Guard fn reads `mHolding`. `navigate_to` sets `mHolding = false` + calls `TransitionTo(target)`. Guard returns Allow. Once transition commits (`kStageTransitionCommitted` on `$lifecycle`), service re-arms: `mHolding = true`. One navigate_to = one transition. |
| 3 | Multiple connections | What if two orchestrators connect simultaneously? Should DiaAutomation support single-client or multi-client? | No enforcement. Document single-client assumption. Concurrent orchestrators produce undefined behaviour. Multi-client is out of scope (§14). Keeps implementation simple. |
| 4 | Checkpoint naming collisions | What if two modules register a checkpoint with the same name? Reject, last-wins, or namespace? | Reject. Assert in debug, return false in release. Duplicate names are a bug — the orchestrator wouldn't know which checkpoint it's validating. Same pattern as DiaAPI command registration. |
| 5 | Pause while transitioning | What if dia.automation.pause arrives mid-transition (modules stopping/starting)? Should it defer until stable? | Execute immediately. Pause callbacks are cooperative on active modules. Modules mid-stop have already unregistered. Modules mid-start haven't registered yet. Registry's module-lifetime scoping handles it naturally. |
| 6 | navigate_to validation | Should navigate_to validate that target is a legal stage (exists in manifest) before releasing the guard? | Yes. Validate against `IApplicationInspectable::GetAllStages()`. Return `{"success": false, "error": "unknown stage: 'Foo'"}` if invalid. Don't release the guard for bogus targets. |
| 7 | Heartbeat and pause | If the app is paused (game time frozen), does heartbeat still tick? Should it use wall-clock or game-time? | Wall-clock (app time). `Application::Update(deltaTime)` passes real elapsed time unaffected by game-time scaling. Heartbeat is a CI safety mechanism — must detect zombie orchestrators regardless of game pause state. Game-time freeze is a separate concept managed by gameplay modules. |

## Status

`Done` (2026-05-21) — All feature specs Approved; implementation complete. Plan: [diaautomation.plan.md](diaautomation.plan.md)

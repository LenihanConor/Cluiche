# System Spec: DiaSteering

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** pathfinding, ai

## Purpose

DiaSteering is the local movement behaviour system for the Dia engine. It operates one layer below DiaPathfinding: pathfinding provides a macro waypoint route across the grid; steering turns that route into smooth, responsive per-frame motion.

Given an agent's current position, velocity, and a set of active behaviours, DiaSteering computes a **desired velocity** — a `Vector2` the caller's movement system applies to the entity's transform. DiaSteering owns no position data; it is a pure computation layer.

Behaviours combine via **priority groups with weighted blending within each group**: avoidance behaviours (Obstacle Avoidance, Separation) occupy a high-priority group and always override movement behaviours (Seek, Arrive, Pursue/Evade, Wander) when they produce output. Within a group, behaviours contribute weighted desired velocities that are summed and normalised. This gives clean avoidance-beats-movement semantics without the fighting artefacts of a flat weighted blend.

The primary use case is 100+ dragons following a flow field (DiaFlowField) toward a target while avoiding obstacles and each other. DiaSteering provides the micro-movement layer that makes that look natural.

**Dependency chain:**
`DiaSteering → DiaPathfinding (CellCoord, world-position types), DiaMaths (Vector2), DiaCore (containers, StringCRC, DIA_LOG_*)`

## Responsibilities

- Define `SteeringAgent` — per-agent state: `Vector2 position`, `Vector2 velocity`, `float maxSpeed`, `float maxForce`; no entity ownership, no transform ownership
- Provide the six v1 behaviours as stateless free functions or lightweight structs, each taking a `SteeringAgent` and behaviour-specific parameters and returning a `Vector2` desired velocity contribution:
  - `Seek(agent, targetPos) → Vector2`
  - `Flee(agent, targetPos) → Vector2`
  - `Arrive(agent, targetPos, slowingRadius) → Vector2` — decelerates within `slowingRadius` of target
  - `Wander(agent, wanderCircleOffset, wanderCircleRadius, wanderAngle) → Vector2` — smooth random drift; caller tracks `wanderAngle` state
  - `Pursue(agent, targetPos, targetVelocity, predictionTime) → Vector2` — seeks predicted future position
  - `Evade(agent, threatPos, threatVelocity, predictionTime) → Vector2` — flees predicted future position
  - `ObstacleAvoidance(agent, obstacles, detectionBoxLength) → Vector2` — steers around the nearest static obstacle ahead
  - `Separation(agent, neighbours, desiredSeparation) → Vector2` — push away from nearby agents
- Provide `SteeringPriorityGroup` — a named group with a `float weight`, a priority index, and a list of `(behaviour output, float weight)` pairs; computes the group's blended desired velocity
- Provide `SteeringBehaviour` — a composable descriptor: behaviour type (`StringCRC` key), parameters, weight within its group; used to configure a `SteeringPipeline`
- Provide `SteeringPipeline` — per-agent composition container; holds an ordered list of `SteeringPriorityGroup`s; `Evaluate(agent) → Vector2` runs groups highest-priority first, returns the first group that produces a non-zero output after blending (or combines all if all groups have output — see SD-003)
- Provide `SteeringSystem` — owns a flat list of `(SteeringAgent, SteeringPipeline)` pairs keyed by `StringCRC agentId`; `AddAgent` / `RemoveAgent` / `GetOutput(agentId) → Vector2`; `Update(float dt)` evaluates all pipelines and caches outputs for caller to read
- Emit `DIA_LOG_INFO` on agent add/remove; `DIA_LOG_WARN` when a pipeline produces zero output for a non-idle agent
- Provide test utilities under `DiaSteering/Testing/`: `AssertSeekDirection`, `AssertArriveDeceleration`, `AssertSeparationDirection`, `MockObstacleSet` — shipped with the library, consumer opt-in via include
- Use DiaCore containers exclusively in all public APIs (AD-002 / PD-004)
- Provide `dia.steering.architecture.module.md` YAML module documentation
- Provide `DiaSteering.vcxproj` static library registered in `Cluiche.sln` under `domain/gameplay/core`

## Non-Responsibilities

- Position integration — DiaSteering outputs desired velocity; callers apply it to transforms
- Pathfinding / macro routing — DiaPathfinding provides waypoints; steering drives toward the next waypoint
- Dense crowd collision avoidance at scale — that is RVO/ORCA (C10, depends on DiaSteering)
- Flow field sampling — caller reads `FlowField::SampleWorld()` and passes the result as a Seek target; DiaSteering does not depend on DiaFlowField
- Formation / squad positioning — DiaSteering operates on individual agents; formations are a higher-level concern (C23)
- DiaAIBudget integration — caller calls `Update(dt)` each frame from its Module; time-slicing is caller's responsibility if needed at scale
- Visual debugger overlay — deferred to a future `DiaSteeringVisualDebugger` system
- Thread safety within `SteeringSystem` — single-threaded; called from SimPU

## Public Interfaces

### SteeringAgent

```cpp
namespace Dia::Steering {
    struct SteeringAgent {
        Dia::Maths::Vector2 position;
        Dia::Maths::Vector2 velocity;
        float maxSpeed  = 1.0f;
        float maxForce  = 1.0f;
    };
}
```

### Behaviour Free Functions

```cpp
namespace Dia::Steering {
    Vector2 Seek    (const SteeringAgent& agent, Vector2 targetPos);
    Vector2 Flee    (const SteeringAgent& agent, Vector2 targetPos);
    Vector2 Arrive  (const SteeringAgent& agent, Vector2 targetPos, float slowingRadius);
    Vector2 Wander  (const SteeringAgent& agent,
                     float circleOffset, float circleRadius, float& inOutWanderAngle);
    Vector2 Pursue  (const SteeringAgent& agent,
                     Vector2 targetPos, Vector2 targetVelocity, float predictionTime = 0.0f);
    Vector2 Evade   (const SteeringAgent& agent,
                     Vector2 threatPos,  Vector2 threatVelocity, float predictionTime = 0.0f);
    Vector2 ObstacleAvoidance(const SteeringAgent& agent,
                              const Dia::Core::DynamicArrayC<Vector2>& obstaclePositions,
                              const Dia::Core::DynamicArrayC<float>&   obstacleRadii,
                              float detectionBoxLength);
    Vector2 Separation(const SteeringAgent& agent,
                       const Dia::Core::DynamicArrayC<Vector2>& neighbourPositions,
                       float desiredSeparation);
}
```

### SteeringPipeline

```cpp
namespace Dia::Steering {
    // Priority group: all behaviours in this group are blended; group competes by priority.
    struct SteeringGroup {
        int   priority;   // lower = higher priority; groups sorted ascending before evaluation
        float weight;     // scales this group's blended output relative to other groups if combined
        Dia::Core::DynamicArrayC<std::pair<Vector2, float>> contributions; // (desiredVelocity, weight)
    };

    // Per-agent pipeline: ordered set of SteeringGroups.
    // Evaluate() runs groups in priority order; first group with non-zero output is returned.
    class SteeringPipeline {
    public:
        // Add a pre-computed group contribution (caller runs behaviour functions, adds results here).
        void AddContribution(int priority, float groupWeight, Vector2 desiredVelocity, float behaviourWeight);
        void Clear();

        // Evaluate the pipeline: blend within each group, return highest-priority non-zero result.
        Vector2 Evaluate() const;
    };
}
```

### SteeringSystem

```cpp
namespace Dia::Steering {
    using SteeringAgentId = Dia::Core::StringCRC;

    class SteeringSystem {
    public:
        // Register an agent. Overwrites if id already exists.
        void AddAgent(SteeringAgentId id, const SteeringAgent& agent);
        void RemoveAgent(SteeringAgentId id);

        // Update agent state (position, velocity) before calling Update().
        void UpdateAgentState(SteeringAgentId id, const SteeringAgent& agent);

        // Retrieve the cached desired velocity from the last Update().
        // Returns {0,0} if agent is unknown or pipeline produced no output.
        Vector2 GetOutput(SteeringAgentId id) const;

        // Caller builds pipelines per agent each frame (or caches and reuses),
        // then calls Update() to evaluate all pipelines and cache outputs.
        // dt: frame delta time in seconds (used by Wander state tracking, future use).
        void Update(float dt,
                    const Dia::Core::DynamicArrayC<std::pair<SteeringAgentId,
                                                             SteeringPipeline>>& pipelines);

        int GetAgentCount() const;
    };
}
```

### Log Channel

```cpp
namespace Dia::Steering {
    static constexpr Dia::Core::StringCRC kLogChannel{"Steering"};
    // DIA_LOG_INFO on agent add/remove.
    // DIA_LOG_WARN when a pipeline produces zero output for a non-idle agent.
}
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| SteeringAgent | Per-agent state struct: position, velocity, maxSpeed, maxForce. No entity ownership. | inline | Approved |
| Seek / Flee | Move toward / away from a target position. Returns normalised desired velocity × maxSpeed. | inline | Approved |
| Arrive | Seek with deceleration zone — full speed outside `slowingRadius`, decelerates linearly within it. | inline | Approved |
| Wander | Smooth random drift using a projected circle and incrementing angle. Caller owns `wanderAngle` state. | inline | Approved |
| Pursue / Evade | Predict target future position using `targetVelocity × predictionTime`; seek/flee predicted point. | inline | Approved |
| Obstacle Avoidance | Probe a detection box ahead of the agent; steer laterally away from the nearest intersecting obstacle. | inline | Approved |
| Separation | Accumulate push vectors from all neighbours within `desiredSeparation` radius, weighted by inverse distance. | inline | Approved |
| SteeringPipeline | Priority-group composition: blend within group, first non-zero group wins. `AddContribution` / `Clear` / `Evaluate`. | inline | Approved |
| SteeringSystem | Agent registry + output cache. `AddAgent` / `RemoveAgent` / `UpdateAgentState` / `GetOutput` / `Update`. | inline | Approved |
| Lifecycle Logging | `DIA_LOG_INFO` on agent add/remove; `DIA_LOG_WARN` on zero output for non-idle agent. | inline | Approved |
| Test Utilities | `DiaSteering/Testing/` — `AssertSeekDirection`, `AssertArriveDeceleration`, `AssertSeparationDirection`, `MockObstacleSet`. Ships with library; consumer opt-in via include. | inline | Approved |

## Dependencies on Other Systems

**Required:**
- **DiaMaths** — `Vector2` (all steering inputs/outputs are 2D vectors)
- **DiaCore** — `DynamicArrayC` (obstacle/neighbour lists, pipeline contributions), `StringCRC` (`SteeringAgentId`, log channel), `DIA_ASSERT`, `DIA_LOG_*`

**Explicitly excluded:**
- **DiaPathfinding** — at compile time, DiaSteering has no dependency. Callers feed pathfinding waypoints in as Seek/Arrive target positions. The conceptual dependency is in the navigation stack, not in the module graph.
- **DiaFlowField** — same pattern: caller samples the flow field and passes the result as a Seek target. No compile-time coupling.
- **DiaBlackboard** — steering parameters (target, speed) are typically sourced from blackboard slots in game code, not inside DiaSteering.
- **DiaAIBudget** — caller calls `Update(dt)` directly from a Module; no `IAIBudgetedSystem` registration in v1.
- **DiaApplicationFlow** — no compile-time dependency on the phase system.

**Dependents (future):**
- `DiaRVO` (C10) — dense crowd avoidance layer; reads `SteeringSystem` output and produces collision-free velocity corrections
- Game code — reads `GetOutput(id)` each frame and integrates position; feeds pathfinding waypoints or flow field samples as Seek targets
- `DiaSteeringVisualDebugger` — future debug arrow overlay

## Out of Scope

- Position integration — output is desired velocity only; caller owns the transform
- Macro pathfinding — DiaPathfinding / DiaFlowField provide routes; DiaSteering drives toward the next waypoint
- Dense crowd avoidance at scale — RVO/ORCA (C10)
- Formation / squad offsets — higher-level concern (C23 DiaSteering)
- DiaAIBudget integration — caller manages update frequency
- Visual debugger overlay — future `DiaSteeringVisualDebugger`
- Thread-safe `SteeringSystem` — single-threaded, called from SimPU

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-001 | Behaviours are stateless free functions — no virtual base, no vtable | Steering functions are called every frame per agent. A virtual dispatch per behaviour per agent per frame is measurable overhead at 100+ agents. Free functions inline at the call site; the pipeline struct holds pre-computed contributions, not behaviour objects. | All behaviour functions | Accepted | Yes |
| SD-002 | DiaSteering has no compile-time dependency on DiaPathfinding or DiaFlowField | Coupling pathfinding to steering would create a circular-style dependency when a game module needs both. Callers pass target positions derived from path waypoints or flow vectors. DiaSteering only depends on DiaMaths and DiaCore. | Module dependency | Accepted | Yes |
| SD-003 | Priority-group composition: first non-zero group wins | Avoidance behaviours (Obstacle Avoidance, Separation) in priority 0 always override movement behaviours (priority 1) when they produce output. Within a group, weighted blend. This avoids the "fighting" artefact of flat weighted blend when an obstacle is directly ahead of the seek target. | SteeringPipeline | Accepted | Yes |
| SD-004 | Desired velocity output only — DiaSteering does not integrate position | Coupling integration to steering would require DiaSteering to own transform data, conflicting with DiaEntity's ownership model. Callers read `GetOutput(id)` and integrate in their own movement system. | Output model | Accepted | Yes |
| SD-005 | `SteeringSystem` separates agent state from pipeline construction | Callers update agent state (`UpdateAgentState`) and rebuild/reuse pipelines each frame before calling `Update`. This keeps SteeringSystem stateless between pipeline evaluations and avoids caching stale behaviour parameters. | SteeringSystem | Accepted | Yes |
| SD-006 | Wander angle state is caller-owned | Wander needs one float of persistent state per agent (the wander angle). Storing it in `SteeringSystem` would require a per-agent map entry for a single float. Caller owns it in their entity data or blackboard slot. | Wander | Accepted | Yes |
| SD-007 | Test utilities ship inside `DiaSteering/Testing/` | Platform-wide pattern (DiaPathfinding PD-009, DiaFlowField FD-007). | Test Utilities | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | `SteeringAgentId` is `StringCRC`. Log channel key is `StringCRC`. |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | `SteeringSystem::Update(dt)` called from a Module on SimPU. No compile-time dependency on DiaApplicationFlow. |
| PD-004 | Platform | No STL containers in public APIs | Obstacle/neighbour lists and pipeline contributions use `DiaCore::DynamicArrayC`. Internal sorted group list may use STL. |
| PD-005 | Platform | x64 only | `DiaSteering.vcxproj` targets x64 exclusively. |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaSteering.vcxproj` and `.vcxproj.filters` created and maintained manually. |
| PD-007 | Platform | C++20 required | Compiled under `/std:c++20`. |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaSteering.vcxproj` must NOT override `OutDir`, `IntDir`, `PlatformToolset`, `WindowsTargetPlatformVersion`, or `LanguageStandard`. |
| PD-009 | Platform | Generated output under Cluiche/out/ | Any log output goes under `Cluiche/out/<AppName>/`. |
| AD-001 | Dia App | Module system with YAML frontmatter | Create `dia.steering.architecture.module.md` with public API, responsibilities, and dependency declarations. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004. Internal priority-group sort may use STL. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::Steering::` namespace. |

## Status

**Status:** Done

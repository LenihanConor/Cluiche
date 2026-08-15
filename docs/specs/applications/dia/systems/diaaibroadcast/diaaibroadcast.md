# System Spec: DiaAICallout

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** ai

## Purpose

DiaAICallout is a shared-registry coordination system that lets AI entities post needs and claim responses without direct messaging or explicit squad formation. An entity posts a **callout** (a typed coordination request with a position, faction filter, radius, and TTL); other eligible entities can **claim** it exclusively — only one claimer per callout. Once claimed, the callout is off the board. If the claimer releases it (or it expires), it goes back on the board.

The system enables emergent AI coordination: a scout that spots an ambush doesn't command other units — it posts a callout, and nearby eligible units self-select via their own DiaRules/DiaCondition logic.

**Dependency chain:**
`DiaAICallout → DiaEntitySpatial (radius queries)`
`DiaAICallout → DiaGeometry2D (position types)`
`DiaAICallout → DiaCore (StringCRC, DynamicArrayC, Timer)`

## Responsibilities

- Provide a `CalloutRegistry` — an explicitly-constructed (not singleton) registry that stores live callouts and supports spatial queries
- Provide a `Callout` value type: typed callout kind (`StringCRC`), world position, broadcast radius, faction filter, TTL, optional payload (`Json::Value`)
- Provide a `CalloutHandle` — a safe, owning reference to a posted callout; used to release or inspect it; invalidates cleanly when the callout expires
- Provide `Emit()` — post a callout to the registry; returns a `CalloutHandle`
- Provide `Query()` — return all unclaimed callouts of a given type within a radius of a position, optionally faction-filtered; does not claim
- Provide `Claim()` — atomically mark a callout as claimed by a specific entity; fails if already claimed; returns success bool
- Provide `Release()` — return a claimed callout to the unclaimed pool; valid to call even if the handle has expired (no-op in that case)
- Tick TTL each sim frame via `Update(float dt)`; expire and remove callouts whose TTL reaches zero; released callouts from dead claimers re-enter the pool immediately
- Identify callout types via `StringCRC` (PD-001)
- Use DiaCore containers exclusively in all public APIs (PD-004)
- Provide test utilities under `DiaAICallout/Testing/`
- Provide `dia.diaaibroadcast.architecture.module.md` YAML module documentation
- Provide `DiaAICallout.vcxproj` static library registered in `Cluiche.sln`

## Non-Responsibilities

- Deciding which entity should claim a callout — that's DiaRules/DiaCondition in the game layer
- Routing callout results into a blackboard — game code maps claimed callout data to `BlackboardComponent` slots
- Per-callout priority ordering — callers sort `Query()` results by their own heuristic
- Thread-safe concurrent Emit/Claim from multiple threads — caller synchronizes; registry is single-threaded (SimPU)
- Persisting callouts across sim restarts or saving them — DiaSaveGame scope
- Broadcasting callouts across network — out of scope

## Public Interfaces

### Callout

```cpp
namespace Dia::AICallout {

    struct Callout {
        Dia::Core::StringCRC       kind;       // e.g. StringCRC{"HelpNeeded"}
        Dia::Maths::Vector2        position;
        float                      radius;     // query radius in world units
        Dia::Core::StringCRC       faction;    // kInvalidCRC = any faction
        float                      ttl;        // seconds until auto-expiry
        Json::Value                payload;    // optional typed data (caller-defined schema)
    };

}
```

### CalloutHandle

```cpp
namespace Dia::AICallout {

    // Owning, safe reference to a posted callout.
    // IsValid() returns false once the callout has expired or been cancelled.
    class CalloutHandle {
    public:
        bool IsValid() const;
        bool IsClaimed() const;

        const Callout* Get() const;   // nullptr if expired
    };

}
```

### CalloutRegistry

```cpp
namespace Dia::AICallout {

    struct QueryFilter {
        Dia::Core::StringCRC       kind;       // required
        Dia::Maths::Vector2        origin;
        float                      radius;
        Dia::Core::StringCRC       faction;    // kInvalidCRC = ignore faction
    };

    class CalloutRegistry {
    public:
        // Post a callout. Returns a handle the emitter uses to release it.
        CalloutHandle Emit(const Callout& callout);

        // Return all unclaimed callouts matching the filter.
        // Results are not sorted — caller orders by their own heuristic.
        void Query(const QueryFilter& filter,
                   Dia::Core::DynamicArrayC<CalloutHandle>& outResults) const;

        // Attempt to claim a callout. Returns false if already claimed.
        bool Claim(const CalloutHandle& handle, Dia::Core::StringCRC claimerEntityId);

        // Release a claimed callout back to the unclaimed pool.
        // No-op if handle is expired or not currently claimed by claimerEntityId.
        void Release(const CalloutHandle& handle, Dia::Core::StringCRC claimerEntityId);

        // Advance TTLs; expire stale callouts.
        void Update(float dt);

        int GetLiveCount() const;
    };

}
```

### Test Utilities

```cpp
// DiaAICallout/Testing/CalloutTestHelpers.h
namespace Dia::AICallout::Testing {

    // Emit a callout with a fixed TTL and return its handle.
    CalloutHandle EmitTestCallout(CalloutRegistry& registry,
                                  Dia::Core::StringCRC kind,
                                  Dia::Maths::Vector2 position,
                                  float radius = 100.0f,
                                  float ttl = 10.0f);

    // Assert a given handle is in the unclaimed pool after a query.
    void AssertInQueryResults(const Dia::Core::DynamicArrayC<CalloutHandle>& results,
                              const CalloutHandle& expected);

    // Assert a handle is absent from query results.
    void AssertNotInQueryResults(const Dia::Core::DynamicArrayC<CalloutHandle>& results,
                                 const CalloutHandle& absent);

}
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Callout Emit | `CalloutRegistry::Emit()` — post a typed callout with position, radius, faction, TTL, optional payload. Returns `CalloutHandle`. | — | Draft |
| Callout Query | `CalloutRegistry::Query()` — spatial + type + faction filter returning unclaimed handles. Uses `DiaEntitySpatial` index for efficient radius queries. | — | Draft |
| Claim / Release | `Claim()` exclusively marks one callout for one claimer. `Release()` returns it to the pool. Clean release is explicit; TTL expiry is the fallback. | — | Draft |
| TTL Expiry | `Update(float dt)` ticks all live callouts; expired ones are removed. Released callouts re-enter the pool immediately next frame. | — | Draft |
| Test Utilities | `DiaAICallout/Testing/` — emit helpers, query assertion helpers. Ships with library. | — | Draft |

## Dependencies on Other Systems

**Required:**
- **DiaEntitySpatial** — spatial index for efficient radius queries in `Query()`
- **DiaGeometry2D** — `Vector2` position type, radius intersection
- **DiaCore** — `StringCRC` (type/faction/claimer identity), `DynamicArrayC` (query results), `Json` (payload), `DIA_ASSERT`

**Explicitly excluded:**
- **DiaBlackboard** — callout data is not written to blackboards by this system; game code does that mapping
- **DiaRules / DiaCondition** — eligibility decisions live in game code, not in this registry
- **DiaStreams** — all access is synchronous on SimPU; no cross-PU messaging
- **DiaAIBudget** — query + claim is O(n) on local results; budgeting is the caller's concern

**Dependents:**
- Game code (CluicheTest and future games) — emit callouts from AI reactions, claim callouts inside rule actions
- DiaUtilityAI / DiaRules — may query callouts as inputs to action scoring or condition evaluation

## Out of Scope

- Cross-thread claim arbitration — registry lives on SimPU, single-threaded
- Persisting callout state across sessions — DiaSaveGame
- Callout history or audit log — DiaObservation can instrument this if needed
- Editor inspector panel — future DiaAICalloutInspector if needed
- Automatic blackboard writeback — game code maps claimed data to slots

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-001 | Claim is exclusive: exactly one claimer per callout | Prevents pile-on coordination failures. One entity owns the response; the board doesn't get flooded with redundant reactions. | Claim/Release | Accepted | Yes |
| SD-002 | Registry is explicitly constructed, not a singleton | Consistent with DiaRules, DiaCondition. No hidden global state; trivially isolated in tests. | CalloutRegistry | Accepted | Yes |
| SD-003 | Release is explicit, TTL is the fallback | A claimer dying without releasing is a bug the caller should handle; TTL is a safety net, not the primary contract. Callers must release on order completion, cancellation, or entity death. | Claim/Release | Accepted | Yes |
| SD-004 | `Query()` returns unclaimed callouts only | Callers should never see claimed callouts as available targets. Claimed callouts are invisible to `Query()`. | Query | Accepted | Yes |
| SD-005 | `Query()` results are unordered | Sorting heuristic (nearest, highest priority, etc.) is game-specific. Avoid baking one into the library. | Query | Accepted | Yes |
| SD-006 | Payload is `Json::Value` (optional, caller-defined schema) | Type-safe payload would require codegen or template complexity. JSON payloads are already idiomatic in Dia for runtime-defined data. Strongly-typed callout subtypes can be added later. | Callout | Accepted | Yes |
| SD-007 | Test utilities ship inside `DiaAICallout/Testing/` | Platform-wide pattern for all Dia modules. | Testing | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Callout kind, faction, and claimer IDs are all `StringCRC`. |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | `Update(float dt)` is called from a SimPU module. Registry lives on SimPU. |
| PD-004 | Platform | No STL containers in public APIs | `Query()` out-parameter is `DynamicArrayC<CalloutHandle>`. |
| PD-005 | Platform | x64 only | `DiaAICallout.vcxproj` targets x64 exclusively. |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaAICallout.vcxproj` and `.vcxproj.filters` created and manually maintained. |
| PD-007 | Platform | C++20 required | Compiled under `/std:c++20`. |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaAICallout.vcxproj` must NOT override OutDir, IntDir, PlatformToolset, or LanguageStandard. |
| PD-009 | Platform | Generated output under Cluiche/out/ | Any diagnostic output goes under `Cluiche/out/<AppName>/`. |
| AD-001 | Dia App | Module system with YAML frontmatter | Create `dia.diaaibroadcast.architecture.module.md`. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::AICallout::` namespace. |

## Open Design Questions

1. **Payload typing** — `Json::Value` payload is flexible but loses schema guarantees. If callers converge on a small set of callout kinds with known shapes (e.g. `HelpNeeded` always carries `threat_position`), a typed `CalloutPayload<T>` variant might be worth adding. Revisit after the first two game-side callout types are wired up.

2. **Radius index integration** — `Query()` delegates radius search to DiaEntitySpatial. If callouts are sparse (< 50 live at once), a linear scan may be simpler and faster than maintaining a spatial index entry per callout. Decide based on profiling after the first CluicheTest stage exercises the system under load.

3. **Release-on-death wiring** — the caller is responsible for releasing on entity death. If every game ends up writing the same "on entity destroy → release all handles" boilerplate, a `CalloutOwnerComponent` that auto-releases on destruction would be the right abstraction. Defer until the pattern repeats twice.

## Status

`Done`

**Plan:** [diaaibroadcast.plan.md](diaaibroadcast.plan.md)

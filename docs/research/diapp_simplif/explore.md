# Research: Explore — DiaApplicationFlow / CluicheTest Simplification

**Session date:** 2026-05-08
**Folder:** docs/research/diapp_simplif/

## Problem Space Overview

DiaApplicationFlow provides the structural skeleton for all Dia-based applications: ProcessingUnits run on threads, Phases define temporal execution windows, Modules supply shared functionality within a PU, and FrameStreams carry data between threads. The mental model is clean — PU=thread, Phase=time-slice, Module=shared-object, FrameStream=comms.

In practice, the implementation introduces significant ceremony and multiple overlapping patterns that obscure this simplicity. CluicheTest — the platform's own demo/testbed — requires: macro-generated factory registrations (~40 lines per type), nested StartData classes for cross-thread init, manifest JSON files that redundantly declare what the code already defines, three different inter-thread communication patterns (FrameStream, MessageBus, Observer), three different module-lookup patterns (GetModule<T>, FindModule(CRC), ModuleRef<T>), and a dependency-resolution algorithm that runs at Phase startup despite the manifest already encoding the order.

The result: the framework works correctly but is hard to reason about, even for its creators. When both human and AI struggle to follow the flow, that's a signal the abstraction is leaking complexity rather than hiding it.

## Existing Approaches

- **Unity-style MonoBehaviour:** Flat component list, implicit ordering, magic method names (Update/Start). Simple but no thread story.
- **Unreal Engine subsystems:** World/Game/Engine subsystems with explicit lifetime tiers. Registration via macros but fewer layers.
- **EnTT/flecs ECS schedulers:** Systems declared as functions, scheduler resolves ordering from read/write sets. Zero inheritance.
- **Bevy App/Plugin model:** Plugins register systems, resources, and schedules. Stage = ordered group of systems. Resources = shared state.
- **Custom task-graph engines:** (Naughty Dog, id Tech) DAG of jobs with explicit data dependencies. Runtime scheduler, no fixed "phase" concept.
- **Actor model (Erlang/Akka style):** Each actor owns state, communicates via messages. Maps well to PU=actor but over-isolates for game engines.
- **Simple main-loop with services:** No framework at all — hand-written loop, services as singletons. Maximum clarity, minimum reuse.

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| **Registration** | Macros (current) / Reflection / Explicit-in-code / Manifest-only | Trade compile-time safety vs. boilerplate |
| **Dependency declaration** | Code-only / Manifest-only / Inferred from types / Hybrid (current) | Current hybrid causes redundancy |
| **Module access** | Single pattern / Multiple (current 3) | Consolidation reduces cognitive load |
| **Inter-thread comms** | Unified channel / FrameStream-only / Message-only / Mixed (current) | Different use cases may justify different tools |
| **Phase complexity** | Phases manage modules (current) / PU manages directly / No phases | Phases add a layer — is it earning its keep? |
| **Init data passing** | StartData classes / Constructor injection / Builder pattern / Config-only | StartData is ceremony-heavy |
| **Manifest role** | Source of truth / Verification aid / Removed entirely | Currently duplicates code declarations |
| **Thread model** | Fixed PU=thread / Thread pool / Configurable (current) | Fixed is simpler; pool scales better |

## Known Tradeoffs

- Removing phases simplifies the model but loses the ability to hot-swap module sets at runtime (e.g., boot → gameplay → shutdown).
- Unifying communication patterns reduces learning burden but may force unnatural usage for some scenarios (temporal data vs. events vs. state observation are genuinely different).
- Eliminating manifests means all wiring is in code — more type-safe but harder to inspect/tool externally.
- Reducing registration boilerplate often requires more advanced C++ (concepts, CRTP, constexpr registration) which has its own learning cost.
- Simplifying for single-threaded ease may compromise the multi-threaded architecture that PUs were designed for.

## Known Pitfalls (C++ / game engine context)

- Static initialization order fiasco — current macro-based registration relies on static init, which is fragile across translation units.
- Template bloat — over-templatizing module access can slow compile times significantly.
- Premature ECS adoption — full ECS is overkill if the entity count is low; CluicheTest doesn't need archetype storage.
- Over-abstraction of threading — game engines benefit from explicit, predictable thread ownership rather than generic task systems.
- Removing phases entirely may make it impossible to cleanly shutdown/restart subsystems (e.g., level transitions).
- Manifest removal trades external tooling capability (editor, hot reload, visualization) for code simplicity.

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| DiaApplicationFlow | The framework itself — ProcessingUnit, Phase, Module, StateObject |
| DiaCore/Frame | FrameStream implementation — temporal data channel |
| DiaCore/Architecture | Singleton, Observer — competing communication patterns |
| DiaCore/Containers | HashTable, DynamicArray used internally by PU/Phase |
| CluicheTest/ApplicationFlow | Concrete PU/Phase/Module implementations showing actual usage |
| DiaEditor | Depends on DiaApplicationFlow structure for plugin lifecycle |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-001 StringCRC | Type IDs stay as CRC — but could be generated differently (constexpr vs macro) |
| PD-002 PU/Phase/Module architecture | **Directly constrains this research** — any simplification must preserve the conceptual model or update this decision |
| PD-003 Component system | Orthogonal to PU simplification — components live inside modules |
| PD-004 No STL in public APIs | Internal simplification fine; public API must use DiaCore containers |
| PD-005 x64 only | No constraint |
| PD-007 C++20 required | Enables concepts, constexpr improvements, designated initializers — tools for reducing boilerplate |

## Open Questions for Ideation

- Can phases be made implicit (derived from module lifecycle) rather than explicit user-declared objects?
- Should the manifest be the single source of truth (removing code registration) or should code be the single source (removing manifests)?
- Is MessageBus pulling its weight, or can all inter-module comms go through FrameStream + direct calls?
- Can ModuleRef<T> become the only access pattern, retiring GetModule<T> and FindModule(CRC)?
- Would a builder/fluent API for PU construction replace both manifests and StartData classes?
- Does PD-002 need to be updated to reflect a simpler version of the same concepts, or is it fine as-is with a cleaner implementation?
- How much of this complexity exists to serve CluicheEditor (external tooling needs) vs. actual runtime needs?
- Can C++20 features (concepts, constexpr, modules) meaningfully reduce the registration boilerplate without introducing new complexity?

# Research: Explore — State Machines

**Session date:** 2026-05-01
**Folder:** docs/research/state_machine/

## Problem Space Overview

State machines are one of the most fundamental abstractions in game development, governing everything from high-level application flow to individual AI behaviors, animation blending, UI navigation, and gameplay mechanics. A well-designed state machine system reduces bugs caused by implicit state, makes complex behavior inspectable and debuggable, and provides a shared vocabulary for reasoning about entity behavior across gameplay, animation, and AI domains.

The Cluiche platform currently has a sophisticated phase/module lifecycle system in DiaApplicationFlow (ProcessingUnit/Phase/Module) that functions as a bespoke state machine for application flow. However, this system is tightly coupled to the application lifecycle concept — it manages module startup/shutdown ordering, dependency resolution, and thread-safe phase transitions. It is not reusable for gameplay entities, AI agents, animation controllers, or UI flows. There is no general-purpose state machine abstraction available to game code.

The gap is significant: every game system that needs stateful behavior (enemy AI, player state, menu navigation, cutscene sequencing, animation blending) must either roll its own ad-hoc state management or abuse the phase system for something it wasn't designed for. A generic state machine library would fill this gap while leveraging the engine's existing strengths — StringCRC identification, DiaCore containers, the event system, the editor plugin framework, and the logging infrastructure.

## Existing Approaches

**Flat Finite State Machines (FSM)**
- States + transitions with optional guards and actions
- Simple, well-understood, easy to debug
- Suffers from state explosion in complex systems (N states can need N^2 transitions)
- Common in simple AI, UI flows, basic gameplay

**Hierarchical State Machines (HSM / Statecharts)**
- Nested states with inherited transitions from parent states
- Dramatically reduces state explosion — shared behavior lives in parent states
- Entry/exit actions propagate through hierarchy (enter child = enter parent first)
- David Harel's Statecharts formalism (1987) — well-proven in UML, automotive, avionics
- More complex to implement but scales much better
- Common in animation systems, complex AI, application frameworks

**Pushdown Automata (PDA / State Stack)**
- Stack-based state machine — push new state, pop to return
- Natural for "interrupt and resume" patterns (pause menu, conversation interrupts)
- Limited: only returns to immediate previous state
- Common in menu systems, simple AI interrupt handling

**Behavior Trees (BT)**
- Tree of tasks (Selector, Sequence, Decorator, Leaf nodes)
- Tick-based evaluation each frame
- Better than FSMs for complex decision-making with many conditions
- Harder to visualize transitions; implicit state in tree traversal
- Common in game AI (Unreal, Unity)

**Data-Driven State Machines**
- States and transitions defined in external data (JSON, XML, visual editor)
- Enables designer iteration without recompilation
- Requires runtime interpretation overhead
- Common in AAA pipelines with dedicated tools

**Event-Driven State Machines**
- Transitions triggered by typed events rather than polled conditions
- Natural fit with existing event/message systems
- Clean separation between state logic and trigger sources
- Common in UI frameworks, network protocols, reactive systems

**Coroutine/Async State Machines**
- States expressed as coroutine suspensions (C++20 co_await)
- Linear code reads like a script but suspends/resumes
- Harder to visualize and debug; implicit state in coroutine frame
- Emerging pattern with C++20 adoption

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| **Topology** | Flat FSM / Hierarchical (HSM) / Both as separate types | HSM subsumes Flat but adds complexity; could offer both |
| **Triggering** | Polled (tick-based) / Event-driven / Hybrid | Hybrid most flexible; event-driven cleaner for most game use cases |
| **Transition Guards** | None / Boolean predicates / Priority-ranked | Guards prevent invalid transitions; priority resolves conflicts |
| **Actions** | Entry/Exit only / Entry/Exit/Update / Entry/Exit/Update + per-transition | Per-transition actions useful for animation blends, sound triggers |
| **State Identity** | Enum-based / StringCRC-based / Type-based (template) | StringCRC aligns with PD-001; type-based enables compile-time safety |
| **History** | None / Shallow (remember last child) / Deep (remember full subtree) | Shallow history covers 90% of use cases; deep is rarely needed |
| **Concurrency** | Single active state / Orthogonal regions / External (separate machines) | Orthogonal regions model independent concurrent behaviors |
| **Data Binding** | Blackboard (shared dict) / Context object / None (external) | Blackboard common in game AI; context object more type-safe |
| **Serialization** | None / Runtime definition (JSON) / Code-only | JSON enables editor tooling and hot reload |
| **Thread Safety** | Single-thread only / Thread-safe transitions / Lock-free | Most gameplay FSMs are single-threaded; thread safety adds overhead |

## Known Tradeoffs

- **Simplicity vs. Power**: Flat FSMs are trivial to implement and debug but cause state explosion in complex systems. HSMs solve this but are harder to reason about, especially with deep nesting.
- **Compile-time vs. Runtime flexibility**: Template-based FSMs catch errors at compile time but can't be data-driven. Runtime FSMs enable editor tooling but defer errors to runtime.
- **Performance vs. Features**: Guard evaluation, history tracking, and orthogonal regions all add per-tick overhead. Most individual FSMs are cheap, but thousands of AI agents with complex HSMs can add up.
- **Event-driven vs. Polled**: Event-driven is cleaner for sparse transitions but requires event infrastructure integration. Polled (tick) is simpler but wastes cycles checking conditions that rarely change.
- **Generic vs. Domain-specific**: A single generic FSM library risks being too abstract for any specific domain. Specialized FSMs (animation, AI, gameplay) can be optimized for their domain but fragment the codebase.
- **Visual editing vs. Code-defined**: Data-driven machines enable rapid iteration but need robust tooling. Code-defined machines are easier to version-control and refactor.

## Known Pitfalls (C++ / game engine context)

- **Virtual dispatch overhead**: Deep state hierarchies with virtual OnEnter/OnExit/OnUpdate can cause cache misses in hot loops. Profile before optimizing — often negligible for gameplay FSMs but significant for thousands of AI agents.
- **Memory allocation in transitions**: Creating/destroying state objects during transitions causes allocation pressure. Pool states or use static instances where possible.
- **Circular transition bugs**: A → B → C → A with wrong guards causes infinite transition loops. Need max-transition-per-frame guards.
- **State lifetime confusion**: When does a state's context get created/destroyed? On enter/exit? On machine construction? Ambiguity causes bugs.
- **Thread safety false sense**: Making the FSM API thread-safe doesn't make the states' internal logic thread-safe. Document the threading model clearly.
- **Debugging opaque state**: Without logging and visualization, complex HSMs become black boxes. Invest in tooling early.
- **Transition ordering**: When multiple transitions are valid simultaneously, undefined ordering causes non-deterministic behavior. Need explicit priority or first-match semantics.
- **C++20 concepts opportunity**: Can use concepts to constrain state types at compile time (must have OnEnter, must be movable, etc.), avoiding cryptic template errors.

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| **DiaCore/Architecture/Events** | Event, EventQueue, EventDispatcher — natural trigger mechanism for event-driven transitions |
| **DiaCore/Containers/Graphs** | Graph<N,E> template — could represent transition graphs at compile time or runtime |
| **DiaCore/CRC/StringCRC** | State and transition identification (PD-001 compliance) |
| **DiaCore/Type** | Type system and serialization — enables runtime type info for states |
| **DiaApplicationFlow** | Phase/Module system — reference architecture for lifecycle management; potential integration point |
| **DiaApplicationFlow/MessageBus** | Thread-safe pub/sub — FSM transition events could publish to MessageBus |
| **DiaApplicationFlow/Introspector** | Runtime topology queries — pattern to follow for FSM introspection |
| **DiaApplicationFlow/DebugDataTypes** | Debug data constants — extend with kStateMachineTransition etc. |
| **DiaDebugServer/StateSerializer** | JSON serialization of phase transitions — reusable pattern for FSM state serialization |
| **DiaEditor/Plugin** | IEditorPlugin framework — state machine visualizer as editor plugin |
| **DiaEditor/WebUIBridge** | C++ ↔ JS bridge — push FSM state to React UI for visualization |
| **DiaLogger** | ISink-based logging with channels — dedicated StateMachine log channel |
| **DiaWebSocket** | Live debugging connection — stream FSM state to editor from running game |
| **GameConnectionManager** | Game↔Editor protocol — carry FSM debug data alongside existing debug topics |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| **PD-001 StringCRC** | State IDs and transition IDs must use StringCRC, not raw strings or enums. Enables fast lookup and consistent identification across code and data. |
| **PD-002 ProcessingUnit/Phase/Module** | The state machine system should integrate with (not replace) the Phase system. FSMs operate *within* phases/modules, not as a parallel lifecycle. |
| **PD-003 Component system** | State machines attached to entities should be IComponent implementations, registered via ComponentFactoryRegistry. |
| **PD-004 No STL in public APIs** | All public containers must use DiaCore types (DynamicArrayC, HashTable, etc.). Internal implementation can use STL. |
| **PD-005 x64 Windows only** | No portability constraints — can use Windows-specific optimizations if needed. |
| **PD-007 C++20 required** | Can leverage concepts, constexpr improvements, std::span. Concepts can constrain state types cleanly. |
| **PD-008 Directory.Build.props** | New module follows standard output paths — no per-project overrides. |
| **PD-009 Output under Cluiche/out/** | Any generated state machine data (logs, serialized graphs) goes under Cluiche/out/<AppName>/. |

## Open Questions for Ideation

- **One library or two?** Should flat FSM and HSM be separate implementations (simpler each, more code) or a unified implementation where flat is just depth=1 HSM (more complex, less code)?
- **Where does it live?** New top-level module (DiaStateMachine) or subsystem within DiaCore/Architecture? A dedicated module is cleaner but adds a dependency edge.
- **How does it integrate with DiaApplicationFlow?** Should the Phase system be refactored to use the generic FSM internally, or should they remain separate systems? Refactoring is risky but proves the abstraction.
- **Component or standalone?** Should every FSM be an IComponent, or should the core FSM be standalone with an optional component wrapper? Standalone core is more flexible.
- **Data-driven from day one?** Should JSON/data definition be part of v1 or deferred? Editor tooling needs it, but code-only is simpler to ship first.
- **What visualization library?** The editor has React + Mosaic but no graph visualization library. Cytoscape.js, vis-network, or D3-force are candidates. Or a custom SVG renderer for full control.
- **Thread safety model?** Should the FSM itself be thread-safe, or should it be single-threaded with explicit synchronization left to the caller (matching how most gameplay code works)?
- **How does this relate to behavior trees?** If AI is a future goal, should the FSM design anticipate integration with a behavior tree system, or are they separate concerns?
- **Logging granularity?** Per-transition logging is essential, but should it also log guard evaluations, failed transitions, and timing? More data helps debugging but increases noise.
- **Live editing?** Should the editor allow modifying FSM structure at runtime (add/remove states, change transitions) or only inspect? Live editing is powerful but much harder to implement safely.

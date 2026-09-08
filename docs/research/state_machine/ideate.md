# Research: Ideate — State Machines

**Input:** docs/research/state_machine/explore.md

## Candidates

### Candidate 1: DiaStateMachine — Core Flat FSM Library
**Home module/system:** New module: `Dia/DiaStateMachine/`
**Size:** M (1–3 weeks)
**Description:** A standalone flat finite state machine library providing the fundamental building blocks: `State`, `Transition`, `StateMachine`, with StringCRC-based identity, guard predicates, and entry/exit/update actions. States are value types (no heap allocation during transitions). Transitions support typed event triggers via the DiaCore Event system. The public API uses DiaCore containers exclusively. Includes a `StateMachineBuilder` for fluent construction in code.

This is the "engine core" candidate — no hierarchy, no editor, no logging beyond basic assertions. Clean, minimal, fast. Other candidates build on top of this.

**Primary value:** Every Dia module and game system gets a shared, well-tested FSM primitive to replace ad-hoc state management.

---

### Candidate 2: DiaStateMachine — Hierarchical State Machine (HSM)
**Home module/system:** New module: `Dia/DiaStateMachine/`
**Size:** L (1–2 months)
**Description:** A full hierarchical state machine implementation based on Harel Statecharts. States can contain child states, with transitions inherited from parent states. Supports shallow history (remember last active child), entry/exit action propagation through the hierarchy, and orthogonal regions for concurrent sub-behaviors. Flat FSM is a degenerate case (single-level hierarchy).

Built on DiaCore Graph containers for the state topology. Uses C++20 concepts to constrain state types (`HasOnEnter`, `HasOnExit`, `IsIdentifiable`). Guard predicates are `std::function`-wrapped callables with StringCRC-tagged names for debuggability.

**Primary value:** Scales to complex behaviors (animation controllers, AI agents, multi-phase gameplay) without state explosion, while still serving simple use cases.

---

### Candidate 3: Dual Implementation — Flat FSM + HSM as Separate Types
**Home module/system:** New module: `Dia/DiaStateMachine/`
**Size:** L (1–2 months)
**Description:** Two distinct class hierarchies sharing a common interface concept but optimized independently. `FlatStateMachine<TContext>` is a lightweight, cache-friendly flat FSM using fixed-size arrays — ideal for high-volume use cases (thousands of AI agents). `HierarchicalStateMachine<TContext>` adds nesting, history, and orthogonal regions with more overhead — ideal for complex single-instance machines (player controller, animation blend tree, application flow).

Both share: StringCRC state/transition IDs, typed event triggers, guard predicates, entry/exit/update actions, and a common `IStateMachineInspectable` interface for tooling. A shared `StateMachineContext<TContext>` provides blackboard-style shared data.

**Primary value:** Right tool for the right job — simple machines stay simple and fast, complex machines get full HSM power, and tooling works with both through a shared inspection interface.

---

### Candidate 4: StateMachine Component + Entity Integration
**Home module/system:** `Dia/DiaStateMachine/` + integration in `DiaCore/Architecture/Components/`
**Size:** M (1–3 weeks) (assumes Candidate 1 or 3 exists)
**Description:** An `IComponent` wrapper that attaches a state machine to any `IComponentObject` entity. `StateMachineComponent` registers via `ComponentFactoryRegistry` with both dynamic and pooled factory variants. The component bridges the FSM's event triggers to the entity's event flow, and exposes the current state as a queryable property for other components.

Includes a `StateMachineComponentFactory` that constructs machines from a `StateMachineDefinition` — a data object describing states and transitions that can be shared across multiple entity instances (flyweight pattern). This separates the machine's *structure* (shared) from its *runtime state* (per-instance).

**Primary value:** State machines become first-class entity components, enabling designers to compose entity behavior from reusable state machine definitions.

---

### Candidate 5: State Machine Logging Channel + Transition Tracing
**Home module/system:** `Dia/DiaStateMachine/` (core) + `Dia/DiaLogger/` (sink)
**Size:** S (≤1 week) (assumes a core FSM exists)
**Description:** A dedicated `kStateMachine` log channel in DiaLogger with a specialized `StateMachineTraceSink` that captures structured transition data: source state, target state, trigger event, guard results, timestamp, machine instance ID. Outputs NDJSON for post-hoc analysis and integrates with the existing `EditorConsoleSink` for real-time display.

Adds transition-rate limiting (configurable max log entries per second per machine) to prevent log flooding from high-frequency FSMs. Includes a `StateMachineTracer` utility that wraps any FSM instance and intercepts all transitions for logging without modifying the FSM's own code.

**Primary value:** Every state machine in the engine becomes debuggable out of the box — transition history is always available for inspection, replay, and post-mortem analysis.

---

### Candidate 6: State Machine Editor Plugin (Visual Debugger)
**Home module/system:** New editor plugin in `Dia/DiaStateMachineEditor/` + React UI
**Size:** L (1–2 months) (assumes a core FSM + logging exists)
**Description:** A `DiaEditor` plugin (`StateMachineEditorPlugin`) that provides real-time visualization of active state machines. Uses the `WebUIBridge` to push FSM state to a React component that renders the state graph using a JS graph library (Cytoscape.js or vis-network). Shows: current active state (highlighted), recent transition history (animated edges), guard evaluation results, and per-state timing.

Connects to running games via `GameConnectionManager` for live debugging. Supports multiple simultaneous FSM views (one per selected entity or system). Includes a timeline scrubber for replaying transition history from logged NDJSON data.

**Primary value:** State machine behavior becomes visually inspectable in real-time, dramatically reducing debugging time for complex stateful systems.

---

### Candidate 7: State Machine Visual Editor (Design-Time)
**Home module/system:** New editor plugin in `Dia/DiaStateMachineEditor/` + React UI
**Size:** XL (>2 months)
**Description:** Extends the visual debugger (Candidate 6) with design-time editing capabilities. Designers can create and modify state machines visually: drag to create states, draw transition arrows, configure guards from a predefined palette, set entry/exit actions. The editor produces `StateMachineDefinition` JSON files that the engine loads at runtime.

Includes validation (reachability analysis, deadlock detection, transition completeness), undo/redo via the editor's `CommandHistory`, and export to C++ code for performance-critical machines. Round-trip editing: changes in code or data are reflected in the visual editor.

**Primary value:** Non-programmers can design and iterate on state machine behavior without touching C++ code.

---

### Candidate 8: DiaApplicationFlow Phase System Refactor to Use Generic FSM
**Home module/system:** `Dia/DiaApplicationFlow/` (refactor existing code)
**Size:** L (1–2 months)
**Description:** Refactor the existing Phase/ProcessingUnit system to use the generic state machine library internally. Phases become states in an HSM, phase transitions become FSM transitions with guards, and the existing module lifecycle (start/stop/retain) maps to state entry/exit/retain actions. The public API remains backward-compatible — existing phase code continues to work — but the internal implementation shares the generic FSM infrastructure.

This "eats your own dog food" approach proves the FSM library handles the engine's most complex state management case. It also enables Phase system users to benefit from FSM tooling (visualization, logging, history) for free.

**Primary value:** Validates the FSM library against the engine's most demanding use case and unifies the two state management approaches into one debuggable system.

---

### Candidate 9: Animation State Machine (ASM) Specialization
**Home module/system:** New module: `Dia/DiaAnimation/` or subsystem within `Dia/DiaStateMachine/Animation/`
**Size:** L (1–2 months) (assumes core FSM exists)
**Description:** A domain-specific state machine optimized for animation blending. States represent animation clips or blend trees. Transitions have blend duration, easing curves, and interruption priority. Supports animation layers (orthogonal regions) for independent body-part animation. Integrates with a future animation system's clip playback.

Adds animation-specific concepts: transition interruption rules (can-interrupt, cannot-interrupt, interrupt-on-same-priority), pose caching for fast blend source, and per-state animation events (footstep, attack-window, etc.) that fire at normalized time positions.

**Primary value:** Purpose-built animation state management that leverages the generic FSM for structure while adding the domain-specific features animators need.

---

### Candidate 10: Pushdown Automaton (State Stack)
**Home module/system:** `Dia/DiaStateMachine/` as a variant alongside FSM/HSM
**Size:** S (≤1 week)
**Description:** A stack-based state machine where new states are pushed on top (pausing the current state) and popped to resume the previous state. Ideal for menu navigation, conversation systems, and interrupt-resume patterns. Simpler than HSM — no parent-child transitions, just push/pop semantics.

Each stack entry preserves the paused state's context. Optional stack depth limit prevents unbounded growth. Integrates with the same logging and inspection interfaces as the flat/hierarchical FSMs.

**Primary value:** Clean "interrupt and resume" semantics for UI flows, menus, and dialog systems without the complexity of a full HSM.

## Coverage Map

The 10 candidates span the full design space identified in the explore phase:

- **Topology axis**: Flat (1), Hierarchical (2), Dual (3), Pushdown (10) — all topologies covered
- **Scope range**: S (5, 10) through M (1, 4) to L (2, 3, 6, 8, 9) and XL (7) — from quick wins to major investments
- **Layer coverage**: Core library (1–3), entity integration (4), logging (5), runtime debugging (6), design-time editing (7), existing system refactor (8), domain specialization (9), variant type (10)
- **Use case coverage**: Gameplay (1, 3, 4), AI (2, 3), animation (9), application flow (8), UI/menus (10), debugging (5, 6, 7)
- **Build-on relationships**: 1 is foundation for 4, 5, 10; 3 subsumes 1+2; 5 enables 6; 6 enables 7; 8 validates 2 or 3

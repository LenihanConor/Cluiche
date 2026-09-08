# Research: Explore — AI & Gameplay Tools

**Session date:** 2026-06-07
**Folder:** docs/research/ai_gameplay_tools/

## Problem Space Overview

Classic game AI and gameplay primitives form the "thinking and acting" layer of a game engine — the systems that let entities make decisions, navigate the world, and execute game rules. For a top-down strategy game, this layer is especially critical: units need to pathfind across grids, evaluate targets, coordinate in groups, and respond to player commands through a turn or action pipeline.

The Dia engine currently has strong foundations (spatial structures, state machines, timers, mailboxes, streams) but no dedicated AI decision-making or navigation systems. The gap between "entities exist with physics" and "entities behave intelligently in a strategy game" is where these tools live.

This research maps what's needed, what already exists in Dia that can be leveraged, and what the priority order should be — producing a list of focused research topics to pursue individually.

## Existing Approaches

**AI Decision-Making:**
- Finite State Machines (FSM) — simple, low overhead, limited scalability
- Behaviour Trees (BT) — hierarchical, composable, industry standard for action games
- Utility Systems — score-based, good for many competing options (strategy AI)
- Goal-Oriented Action Planning (GOAP) — planner-based, emergent, expensive
- Hierarchical Task Networks (HTN) — plan decomposition, good for RTS
- Blackboard systems — shared data store for AI knowledge, decouples sensors from decisions
- Influence Maps — spatial heat maps for strategic reasoning

**Navigation & Movement:**
- Grid-based pathfinding (A*, JPS, Dijkstra) — natural for top-down strategy
- Flow fields — efficient for many-unit RTS movement
- NavMesh — polygon-based navigation for continuous space
- Steering behaviours — local obstacle avoidance, flocking, formation movement
- Hierarchical pathfinding (HPA*) — coarse-then-fine for large maps

**Gameplay Primitives:**
- Command/action queues — ordered unit instructions (move, attack, build)
- Ability/cooldown systems — resource-gated actions with timing
- Target selection — priority-based target picking (closest, weakest, most dangerous)
- Aggro/threat tables — distributed attention in multi-unit combat
- Fog of war / visibility — what each team can see
- Turn/phase systems — structured game flow for strategy games
- Formation systems — group movement patterns
- Squad/group AI — coordinated multi-unit behaviour

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| Decision granularity | Per-entity vs per-squad vs per-faction | Strategy games need all three |
| Update frequency | Every frame / fixed tick / on-demand | Budget matters for many units |
| Data coupling | Tight (direct access) vs loose (blackboard/streams) | Dia favours loose via Mailbox/Streams |
| Determinism | Required (lockstep MP) vs best-effort | Top-down strategy often needs replay |
| Scalability | 10 units vs 100 vs 1000+ | Flow fields + LOD AI for high counts |
| Grid vs continuous | Tile-based vs free movement | Top-down strategy usually grid-based |
| Sync model | Same-frame response vs deferred (next tick) | Maps to Dia's phase model |
| Data-driven | Hardcoded vs data-defined (JSON/asset) | Engine tools → data-driven preferred |

## Known Tradeoffs

- **Behaviour Trees vs Utility Systems**: BTs are easier to author/debug but scale poorly when options multiply; utility systems handle "many options" elegantly but are harder to visualise. Strategy AI often benefits from utility at the strategic layer and BT at the tactical layer.
- **A* vs Flow Fields**: A* is per-unit (O(n) for n units); flow fields are per-destination (O(grid) regardless of unit count). Crossover point is ~20-50 units sharing a destination.
- **Blackboard scope**: Per-entity blackboards are simple; shared faction blackboards enable coordination but introduce coupling.
- **Determinism vs performance**: Floating-point determinism requires fixed-point or careful cross-platform work. Dia is Windows-only (PD-005), which simplifies this.
- **Formation vs individual pathing**: Formations look good but conflict with per-unit obstacle avoidance. Need clear priority rules.

## Known Pitfalls (C++ / game engine context)

- Virtual dispatch overhead in hot AI loops (1000+ entities per frame) — prefer data-oriented or template-based designs
- Memory allocation in decision trees — pre-allocate node pools, don't new/delete per evaluation
- Pathfinding cache invalidation — dynamic obstacles require partial re-planning
- Frame budget starvation — AI should be budgeted (N evaluations per frame) not unbounded
- Over-engineering decision systems before having concrete game rules to drive them
- Coupling AI state to render state — AI should tick independently (Dia's SimPU is correct home)
- Forgetting to make systems inspectable — all AI should expose state for DiaObservation/visual debug

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| DiaStateMachine | Flat FSM + HSM + Pushdown — decision layer foundation, already shipped |
| DiaGeometry2D/Spatial | SpatialGrid, Quadtree, BVH, HexGrid — spatial queries ready to use |
| DiaCore/Timer | Timer, TimerSystem, TimerExpiry — cooldown/delay primitives exist |
| DiaMailbox | Typed pub/sub messaging — natural for AI commands and events |
| DiaStreams | ServiceStream + EventStream + FrameStream — cross-PU data flow solved |
| DiaRigidBody2D/Triggers | TriggerVolume2D, TriggerEvent — proximity detection exists |
| DiaEntity | Domain, Hierarchy — entity ownership/grouping (squads could layer here) |
| DiaObservation | Logging, metrics, traces — AI inspection hooks ready |
| DiaVisualDebugger | Debug draw layers — pathfinding/influence map visualisation target |
| DiaMaths/Random | Random number generation — decision randomness |
| DiaCore/Containers/Graphs | DirectedGraph — graph structures for pathfinding/planning |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-001 StringCRC | State names, action IDs, blackboard keys → all StringCRC |
| PD-002 PU/Phase/Module | AI systems are Modules on SimPU; budgeted per-phase |
| PD-004 No STL in public APIs | DynamicArrayC, HashTable for all AI containers |
| PD-005 x64 Windows only | Can rely on SIMD, no cross-platform float issues |
| PD-007 C++20 | Concepts for AI interface constraints; coroutines for BT? |

## Open Questions for Ideation

- Should pathfinding be a standalone module (DiaPathfinding) or part of DiaGeometry2D (which already owns spatial structures)?
- Is a behaviour tree system worth building given DiaStateMachine already exists, or does a utility system better serve strategy AI?
- Should the blackboard be a standalone module or folded into an AI-specific module?
- How do we handle AI time-slicing (spreading evaluations across frames) — framework-level or per-system?
- Do we need a dedicated "DiaAI" umbrella module, or are these better as independent focused modules (DiaPathfinding, DiaBehaviourTree, DiaBlackboard, etc.)?
- For top-down strategy: is the HexGrid the primary navigation substrate, or do we also need a general tile-graph for square grids?
- Should formation/squad systems be AI-layer or entity-layer (DiaEntity group management)?
- What's the minimal set needed to get a basic strategy AI loop running in CluicheTest?

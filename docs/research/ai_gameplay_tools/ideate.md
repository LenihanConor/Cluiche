# Research: Ideate — AI & Gameplay Tools

**Input:** docs/research/ai_gameplay_tools/explore.md

## Candidates

---

### AI — Decision Making

---

### Candidate 1: Blackboard System

**Home module/system:** New module: `Dia/DiaBlackboard/`
**Size:** S (≤1 week)
**Description:** A typed key-value store for AI knowledge sharing. Keys are StringCRC (PD-001), values are type-erased with compile-time-safe accessors via C++20 concepts. Supports scoped blackboards: per-entity (private knowledge), per-squad (shared tactical), per-faction (strategic). Blackboards are observable — listeners can react to key changes, integrating naturally with DiaStateMachine transition guards and DiaMailbox notifications.

The blackboard decouples AI sensors (who writes "enemy_spotted = true") from AI decision-makers (who reads it to decide behaviour). This separation is essential for testability and for layering multiple AI systems that don't know about each other.

**Primary value:** AI systems can share knowledge without direct coupling, enabling composable sensor→decision→action pipelines.

---

### Candidate 2: Utility AI System

**Home module/system:** New module: `Dia/DiaUtilityAI/`
**Size:** M (1–3 weeks)
**Description:** A score-based decision system where each possible action is evaluated by response curves against blackboard values, producing a normalised score. The highest-scoring action wins. Designed for "many options, pick best" scenarios natural to strategy AI: which unit to attack, where to expand, what to build next.

Response curves (linear, quadratic, logistic, step) are data-driven (JSON-defined). The evaluator supports action sets with cooldowns (integrating DiaCore/Timer), prerequisites, and group-based considerations (one unit shouldn't duplicate another's choice). Budget-aware: evaluates N candidates per frame, spreads the rest across ticks.

**Primary value:** Strategy AI can weigh many competing options (attack, defend, expand, retreat) using tunable curves rather than brittle priority lists.

---

### Candidate 3: Behaviour Tree Runtime

**Home module/system:** New module: `Dia/DiaBehaviourTree/`
**Size:** M (1–3 weeks)
**Description:** A data-driven behaviour tree evaluator. Nodes are: Sequence, Selector, Parallel, Decorator (inverter, repeater, cooldown, guard), and Leaf (action/condition). Trees are defined in JSON and loaded at runtime. Leaf nodes reference blackboard keys for conditions and command-queue actions for execution.

Supports tree sharing (many entities run the same tree definition with different blackboard instances). Includes time-slicing: trees can pause mid-evaluation and resume next tick. Inspectable via IStateMachineInspectable-style interface for editor/debugger tooling.

**Primary value:** Complex multi-step unit behaviours (patrol routes, attack sequences, retreat logic) can be authored as data without C++ recompilation.

---

### Candidate 4: Hierarchical Task Network (HTN) Planner

**Home module/system:** New module: `Dia/DiaHTN/`
**Size:** L (1–2 months)
**Description:** A planning AI system that decomposes high-level goals into ordered primitive tasks. The planner takes a world state (blackboard snapshot), a domain definition (methods + operators), and produces a plan (sequence of actions). Re-plans when the world state diverges from expectations.

HTNs excel at RTS/strategy AI because they naturally express "to capture a base: scout it, gather army, move to staging point, attack, hold." The decomposition is data-driven. Partial plans can be executed while replanning continues in the background.

**Primary value:** Faction-level AI can generate multi-step strategic plans that adapt when circumstances change, producing emergent-feeling behaviour.

---

### Candidate 5: Rule / Condition System

**Home module/system:** New module: `Dia/DiaRules/`
**Size:** S (≤1 week)
**Description:** A lightweight forward-chaining rule engine. Rules are condition→action pairs evaluated against a blackboard or world state. Conditions support AND/OR/NOT composition. When conditions are met, actions fire (post mailbox message, set blackboard key, queue command). Rules are data-driven (JSON).

Simpler than utility or BT systems — best for triggered responses: "if health < 20% then retreat," "if enemy count > 3 then call reinforcements." Acts as the reactive glue layer between sensors and heavier decision systems.

**Primary value:** Simple data-driven "if this then that" reactive behaviours without spinning up a full decision system.

---

### Candidate 6: AI Personality / Difficulty System

**Home module/system:** Extension of DiaUtilityAI or standalone `Dia/DiaAIPersonality/`
**Size:** S (≤1 week)
**Description:** A parameter set that modifies AI decision weights, reaction times, and information access per difficulty level or personality archetype. Personalities are named profiles (Aggressive, Defensive, Economic, Balanced) that bias utility scores, change evaluation frequency, and adjust "cheating" levels (how much fog-hidden info the AI can access).

Data-driven: each personality is a JSON file of multipliers and overrides. Enables multiple distinct AI opponents from the same underlying decision systems.

**Primary value:** One AI codebase produces varied, characterful opponents that players perceive as having distinct strategies.

---

---

### AI — Navigation & Movement

---

### Candidate 7: Grid Pathfinding (A* + JPS)

**Home module/system:** New module: `Dia/DiaPathfinding/`
**Size:** M (1–3 weeks)
**Description:** Core pathfinding on grid graphs. Provides A* for general grids and Jump Point Search (JPS) for uniform-cost square grids. Works with both SpatialGrid (square) and HexGrid (hex) via a common `IPathGraph` interface. Includes path smoothing, partial paths (when destination unreachable), and path caching (same start/end within N frames returns cached result).

Supports weighted terrain (swamp costs more, roads cost less), impassable cells, and dynamic cost updates (building placed → cells blocked). The fundamental navigation building block.

**Primary value:** Entities can find optimal routes across grid maps with terrain costs, the basic requirement for any strategy game movement.

---

### Candidate 8: Flow Field Navigation

**Home module/system:** Extension of `Dia/DiaPathfinding/` or standalone
**Size:** M (1–3 weeks)
**Description:** Vector-field-based navigation for efficient multi-unit movement. Given a destination, generates a grid-sized vector field where each cell points toward the next step to reach the goal. All units in the field simply follow their cell's vector — no per-unit pathfinding needed.

Critical for RTS-scale games: 100 units moving to a rally point compute one flow field (O(grid_cells)) instead of 100 A* queries. Supports multiple simultaneous fields (different destinations), hierarchical flow fields for large maps, and dynamic invalidation when obstacles change.

**Primary value:** Large groups of units navigate to shared destinations efficiently without per-unit pathfinding cost.

---

### Candidate 9: Steering Behaviours

**Home module/system:** New module: `Dia/DiaSteering/`
**Size:** S–M (1–2 weeks)
**Description:** Local movement behaviours that operate below pathfinding. Individual behaviours: Seek, Flee, Arrive, Wander, Pursue, Evade, Obstacle Avoidance, Wall Following. Composite behaviours via weighted blending or priority. Each entity has a steering output (desired velocity) that's applied to its movement system.

Particularly relevant for: units avoiding each other in tight spaces, archers maintaining range, units flowing around obstacles that pathfinding didn't anticipate, and natural-looking movement that isn't robotic grid-snapping.

**Primary value:** Units move naturally and responsively at the local level — avoiding collisions, maintaining distances, and flowing smoothly.

---

### Candidate 10: Dynamic Obstacle Avoidance (RVO/ORCA)

**Home module/system:** Extension of `Dia/DiaSteering/` or `Dia/DiaPathfinding/`
**Size:** M (1–3 weeks)
**Description:** Reciprocal Velocity Obstacles (RVO) or Optimal Reciprocal Collision Avoidance (ORCA) for dense crowd movement. When many units converge (chokepoints, bases), simple steering causes oscillation and deadlocks. RVO computes collision-free velocities where each agent takes "half the responsibility" for avoidance.

Handles the classic RTS problem: 50 units trying to pass through a narrow bridge without gridlocking. Integrates with both grid-based pathfinding (macro route) and steering (micro movement).

**Primary value:** Large groups navigate through tight spaces without deadlocking, oscillating, or overlapping.

---

### Candidate 11: Hierarchical Pathfinding (HPA*)

**Home module/system:** Extension of `Dia/DiaPathfinding/`
**Size:** M (1–3 weeks)
**Description:** Multi-level abstraction for pathfinding on large maps. The grid is divided into sectors; a high-level graph connects sector entry/exit points. Long-distance paths use the abstract graph (fast), then refine within sectors on demand. Re-abstraction only needed when terrain changes within a sector.

For large strategy maps (200×200+), raw A* is too expensive for frequent queries. HPA* reduces search space by orders of magnitude for long paths while maintaining optimality guarantees within a small factor.

**Primary value:** Pathfinding remains fast on large maps by searching at the right level of abstraction.

---

### Candidate 12: Terrain Analysis / Strategic Points

**Home module/system:** New module: `Dia/DiaTerrainAnalysis/`
**Size:** S–M (1–2 weeks)
**Description:** Pre-computation and runtime analysis of map topology for AI strategic reasoning. Identifies: chokepoints (narrow passages), defensible positions (high ground, walled areas), resource clusters, expansion sites, flanking routes. Outputs annotated waypoints and region classifications.

Runs once at map load (or when terrain changes significantly). Results feed into influence maps and HTN planning — the AI knows "there's a chokepoint at (50,30) connecting the north and south regions" without hardcoding per-map knowledge.

**Primary value:** AI can reason about map strategy (where to fortify, where to ambush) from computed terrain features rather than hand-authored waypoints.

---

---

### AI — Spatial Intelligence

---

### Candidate 13: Influence Maps

**Home module/system:** Extension of `Dia/DiaGeometry2D/Spatial/` or new `Dia/DiaInfluenceMap/`
**Size:** S–M (1–2 weeks)
**Description:** Grid-based spatial heat maps where values propagate, decay, and combine. Each faction can have multiple layers: military strength, economic value, danger, exploration. Values are written by entities (a unit's presence adds to its faction's strength map) and read by AI for strategic decisions.

Propagation uses simple blur/diffusion per tick. Maps are inspectable via DiaVisualDebugger (colour-coded overlays). Multiple maps combine with weights for composite scoring ("attack where enemy is weak AND my army is strong").

**Primary value:** Faction-level AI can reason spatially about strategy (where to attack, where to defend) without expensive per-unit calculations.

---

### Candidate 14: Fog of War / Visibility System

**Home module/system:** New module: `Dia/DiaVisibility/`
**Size:** M (1–3 weeks)
**Description:** A per-faction visibility system tracking what each team can see. Entities have a sight radius; cells within radius are marked visible. States: unexplored → revealed (seen but not currently visible) → visible. Enemy entities in non-visible cells are hidden from that faction's AI and rendering.

Sight computed via raycasting against terrain LOS blockers (walls, elevation). Supports shared vision within factions. Integrates with DiaStreams to publish visibility-change events (unit spotted, unit lost).

**Primary value:** Information warfare becomes possible — scouting matters, ambushes work, and AI must reason under uncertainty.

---

---

### Gameplay — Command & Action

---

### Candidate 15: Command Queue / Action System

**Home module/system:** New module: `Dia/DiaCommand/`
**Size:** S (≤1 week)
**Description:** An ordered queue of commands per entity (or squad). Commands: Move, Attack, Patrol, Hold, Build, UseAbility — all StringCRC-identified. Each command has an `Execute()` tick, a completion condition, and interrupt/cancel semantics. Supports queue modifiers: replace-all (new order), append (shift-click), insert-front (interrupt).

Integrates with DiaStateMachine (executing a command triggers transitions) and DiaMailbox (commands issued via messages). The bridge between player input / AI decisions and entity behaviour.

**Primary value:** Entities receive and execute ordered instructions — the "select units, right-click target" core loop.

---

### Candidate 16: Ability / Cooldown System

**Home module/system:** New module: `Dia/DiaAbility/`
**Size:** S–M (1–2 weeks)
**Description:** A system for resource-gated, timed actions. Each ability has: cooldown duration, resource cost (mana, energy, charges), cast time, effect, targeting rules (self, single target, AoE, directional). Abilities are data-driven (JSON definitions). The runtime manages cooldown tracking (built on DiaCore/Timer), resource validation, and execution callbacks.

Supports ability upgrade/modification (level-up changes parameters), interruption, and queuing. Publishes events via DiaStreams when abilities are used (for UI, sound, VFX triggers).

**Primary value:** Units have special actions beyond move/attack, enabling diverse unit roles and tactical depth.

---

### Candidate 17: Status Effect / Buff System

**Home module/system:** New module: `Dia/DiaStatusEffect/`
**Size:** S (≤1 week)
**Description:** A system for temporary modifiers applied to entities. Effects have: duration, tick rate, stat modifiers (speed ×0.5, damage +10), stacking rules (refresh, stack N times, don't stack), and categories (poison, buff, debuff, crowd-control). Effects can be dispelled, can prevent other effects, and expire naturally.

Data-driven definitions. The system applies modifiers to entity stats each frame and removes expired effects. Publishes add/remove/tick events for UI indicators. Essential for any combat system beyond "hit points go down."

**Primary value:** Combat gains depth through temporary modifiers — slows, poisons, shields, buffs — making tactical choices matter beyond raw damage.

---

### Candidate 18: Damage / Combat Resolution System

**Home module/system:** New module: `Dia/DiaCombat/`
**Size:** S–M (1–2 weeks)
**Description:** The math and logic layer for combat encounters. Handles: damage calculation (base damage × modifiers × resistances), armour types vs damage types (rock-paper-scissors or multiplicative tables), critical hits, miss chance, damage-over-time, area-of-effect resolution, and kill/death events.

Designed as a pure calculation layer — takes inputs (attacker stats, defender stats, ability used) and produces outputs (damage dealt, effects applied, kill triggered). No entity ownership; called by whoever resolves an attack. Data-driven damage tables. Publishes combat events via DiaStreams for combat log, VFX, and AI reaction.

**Primary value:** Combat produces consistent, balanced, moddable results across all game systems without each system reimplementing damage math.

---

### Candidate 19: Target Selection & Threat

**Home module/system:** New module: `Dia/DiaTargeting/`
**Size:** S (≤1 week)
**Description:** A reusable targeting framework. Given visible enemies and a scoring function (distance, health, threat level, unit type priority), returns ranked targets. Supports targeting policies: closest, weakest, strongest-threat, highest-value, random-weighted. Policies are data-driven per unit type.

Includes threat/aggro accumulation: entities build threat from damage dealt, abilities used, or proximity. Threat decays over time. Determines who enemies focus — enabling tank/DPS/healer dynamics even in strategy games (guard units draw fire from archers).

**Primary value:** AI and auto-attack systems intelligently pick targets without game-specific hardcoding.

---

---

### Gameplay — Economy & Resources

---

### Candidate 20: Resource / Economy System

**Home module/system:** New module: `Dia/DiaEconomy/`
**Size:** S–M (1–2 weeks)
**Description:** Manages named resource pools (gold, wood, food, supply, population cap) per faction. Resources are earned (workers harvest, buildings produce, time ticks), spent (unit training, building construction, ability costs), and capped (storage limits, population caps). Supports resource transfer between factions (trade, tribute).

The economy publishes rate-of-change metrics (income/expense per tick) for AI budgeting decisions and UI display. Integrates with DiaObservation for economic health metrics.

**Primary value:** The strategy game has a functioning economy that AI and players interact with — the foundation for build orders, expansion pressure, and resource denial.

---

### Candidate 21: Tech Tree / Upgrade System

**Home module/system:** New module: `Dia/DiaTechTree/`
**Size:** S (≤1 week)
**Description:** A directed acyclic graph of researches/upgrades. Each node has: resource cost, research time, prerequisites (other nodes), and effects (unlock unit type, modify stats, enable ability). The graph is data-driven (JSON). Runtime tracks per-faction research state and applies completed effects.

Built on DiaCore's DirectedGraph container. Integrates with DiaEconomy (spending resources to research) and publishes events when research completes (for AI to react to enemy upgrades).

**Primary value:** Factions differentiate over time through choices — enabling strategic diversity and counter-play.

---

### Candidate 22: Building / Construction System

**Home module/system:** New module: `Dia/DiaConstruction/`
**Size:** M (1–3 weeks)
**Description:** Manages the lifecycle of placed structures: placement validation (terrain requirements, spacing rules, build radius), construction progress (worker-driven or time-driven), completion, and destruction. Buildings occupy grid cells, block pathing, and provide effects (resource production, unit training, area buffs, LOS).

Supports build queues (train unit A, then B, then C), rally points, and building upgrades. Construction state published via DiaStreams. Grid cell occupation integrates with pathfinding dynamic obstacles.

**Primary value:** Players and AI can build bases — the defining mechanic of base-building strategy games.

---

---

### Gameplay — Coordination & Groups

---

### Candidate 23: Squad / Formation System

**Home module/system:** New module: `Dia/DiaSquad/`
**Size:** M (1–3 weeks)
**Description:** A grouping and formation layer. Squads are named groups sharing a command queue, blackboard, and formation shape. Formation shapes define relative slot positions (line, wedge, circle, custom). The squad leader pathfinds; members maintain formation offsets with local avoidance.

Supports squad-level AI: the squad makes decisions collectively, then issues commands to members. Enables coordinated behaviours: flanking, surrounding, focus-firing. Membership is dynamic. Built on DiaEntity's Hierarchy system.

**Primary value:** Groups of units move and fight as coordinated teams rather than independent agents.

---

### Candidate 24: Rally Point / Waypoint System

**Home module/system:** Extension of `Dia/DiaCommand/` or standalone
**Size:** S (≤1 week)
**Description:** Named spatial points that direct unit flow. Rally points: where newly trained units auto-move. Patrol waypoints: looped paths for guard routes. Attack-move waypoints: move + engage enemies along the way. Retreat waypoints: fallback positions.

Waypoints are first-class entities (placed on grid, visible, selectable). They integrate with the command queue (auto-issuing move commands to new units) and fog of war (enemy waypoints hidden).

**Primary value:** Players and AI can set up persistent movement patterns — guarding, patrolling, and rallying without micro-management.

---

### Candidate 25: Communication / Signal System

**Home module/system:** Extension of `Dia/DiaMailbox/` or new `Dia/DiaSignal/`
**Size:** S (≤1 week)
**Description:** A spatial and faction-scoped signaling system for AI coordination without explicit squad membership. Signals: "help needed at (x,y)", "enemy spotted at (x,y)", "retreating from (x,y)". Signals have a broadcast radius, decay timer, and faction filter. Nearby friendly AI can read signals and react.

Enables emergent coordination: a unit under attack broadcasts "help," nearby idle units respond without requiring explicit squad formation. More organic than top-down command structures.

**Primary value:** AI units self-organise in response to events — reinforcing, retreating, and swarming without explicit squad orders.

---

---

### Gameplay — World & Environment

---

### Candidate 26: Terrain Effect / Movement Cost Layer

**Home module/system:** Extension of `Dia/DiaPathfinding/` or `Dia/DiaGeometry2D/`
**Size:** S (≤1 week)
**Description:** A data layer assigning properties to grid cells: movement cost multiplier, movement type restrictions (can fly over, can swim, can walk), and status effects applied while occupying (poison swamp, healing spring, speed road). Different unit movement types (foot, mounted, flying, amphibious) interact differently with terrain.

Data-driven: terrain types defined in JSON with their properties. The pathfinding system reads costs; the combat system reads modifiers (e.g., forest gives +defense). Rendering reads terrain type for tilemap selection.

**Primary value:** The map is strategically interesting — terrain matters for movement, positioning, and combat beyond just "passable or not."

---

### Candidate 27: Spawn / Wave System

**Home module/system:** New module: `Dia/DiaSpawner/`
**Size:** S (≤1 week)
**Description:** A configurable entity spawning system. Spawn points produce entities on timers, triggers, or wave schedules. Wave definitions: count, composition (unit types + ratios), interval, difficulty scaling, spawn pattern (all at once, staggered, random positions within radius).

Data-driven wave tables (JSON). Supports both PvE (enemy waves) and PvP economy (trained units appear at barracks). Integrates with DiaEconomy (spawning costs resources) and population caps. Publishes wave-start/wave-complete events.

**Primary value:** Enemies appear in structured, designable patterns — enabling both campaign missions and survival modes.

---

### Candidate 28: Objective / Win Condition System

**Home module/system:** New module: `Dia/DiaObjective/`
**Size:** S (≤1 week)
**Description:** A data-driven system for tracking game goals. Objectives have: description, completion conditions (destroy target, hold area for N seconds, collect N resources, survive N waves), progress tracking, and rewards. Supports primary/secondary/optional classification and chained objectives (complete A to unlock B).

Conditions are composable expressions evaluated against game state. Publishes progress events for UI. Tracks per-faction objectives independently (asymmetric victory conditions). The game over state is "all primary objectives for one faction complete."

**Primary value:** Games have structured goals beyond "kill everything" — enabling varied mission types and narrative progression.

---

### Candidate 29: Turn / Phase Manager (Turn-Based Mode)

**Home module/system:** New module: `Dia/DiaTurnManager/`
**Size:** S–M (1–2 weeks)
**Description:** An optional turn structure layered on top of real-time execution. Supports multiple modes: pure turn-based (player A acts, then player B), simultaneous turns (both plan then resolve), real-time-with-pause, and "WeGo" (turns submitted simultaneously, play out in real-time). Manages turn order, action points per turn, turn timers, and end-turn signals.

Built as a Module on SimPU. The rest of the game systems (pathfinding, combat, etc.) don't know about turns — they receive commands regardless. The turn manager gates *when* commands are issued, not how they execute.

**Primary value:** The same engine supports both real-time and turn-based strategy — or hybrid modes like simultaneous turns.

---

### Candidate 30: Minimap / Strategic Overview Data Layer

**Home module/system:** Extension of `Dia/DiaVisibility/` + `Dia/DiaInfluenceMap/`
**Size:** S (≤1 week)
**Description:** A data provider (not renderer) that aggregates minimap-relevant information: unit positions per faction, fog state, influence colours, alert pings, camera viewport indicator. Publishes a compact per-frame snapshot that any rendering system (ImGui, CEF, custom) can consume to draw a minimap.

Separate from rendering so it works in headless tests and AI-only simulations. The data contract is: "here's what should appear on the minimap this frame" — consumed by whatever UI system is active.

**Primary value:** Players and AI have a strategic overview of the entire map — essential for macro-level decision making in strategy games.

---

---

### Infrastructure — AI Runtime

---

### Candidate 31: AI Budget / Time-Slicing Framework

**Home module/system:** New module: `Dia/DiaAIBudget/`
**Size:** S (≤1 week)
**Description:** A frame-budget scheduler for AI evaluations. Each frame has a microsecond budget for AI work. Systems register "work items" (pathfinding requests, BT evaluations, utility scorings) with a priority. The scheduler runs items in priority order until budget is exhausted, deferring the rest.

Priority classes: critical (must run this frame), normal (can wait 1-2 frames), background (long-range planning). Integrates with DiaObservation metrics to report budget utilisation.

**Primary value:** AI systems scale to hundreds of entities without frame drops by spreading computation across time.

---

### Candidate 32: AI Sensor Framework

**Home module/system:** New module: `Dia/DiaSensor/`
**Size:** S (≤1 week)
**Description:** A standardised way for entities to perceive the world. Sensors: SightSensor (LOS-based entity detection), ProximitySensor (radius check), DamageSensor (react to incoming damage), SoundSensor (react to events within range). Each sensor writes to its entity's blackboard.

Sensors tick at configurable rates (not every frame — sight might check every 0.2s). They use DiaGeometry2D spatial queries for efficiency. The sensor framework decouples "what the entity knows" from "how it decides" — any decision system (BT, utility, rules) reads the same blackboard keys.

**Primary value:** AI perception is standardised, budgeted, and decoupled from decision-making — enabling mix-and-match of sensors with any AI system.

---

### Candidate 33: Replay / Deterministic Simulation

**Home module/system:** New module: `Dia/DiaReplay/`
**Size:** M (1–3 weeks)
**Description:** A system that records all inputs (commands issued per frame) and replays them deterministically to reproduce identical game states. Requires: fixed-timestep simulation, deterministic math (no unordered iteration, consistent float ops — simplified by PD-005 Windows-only), and input recording/playback.

Enables: game replays (watch past matches), debugging (reproduce bugs frame-perfectly), and lockstep multiplayer (only transmit inputs, not state). Also enables automated testing — play back recorded inputs and assert on game state at frame N.

**Primary value:** Games can be replayed, debugged frame-perfectly, and potentially networked via lockstep — all from one determinism investment.

---

### Candidate 34: AI Debug / Inspection Layer

**Home module/system:** Extension of `Dia/DiaObservation/` + `Dia/DiaVisualDebugger/`
**Size:** S (≤1 week)
**Description:** Specialised debug tooling for AI systems. Visualises: current BT/FSM state per entity, blackboard contents, pathfinding queries and results, influence map overlays, targeting lines, formation slots, sensor ranges, utility scores. All drawn via DiaVisualDebugger layers, togglable per-system.

Also provides a frame-by-frame AI decision log: "Entity 42 evaluated 5 targets, chose Entity 17 (score 0.82), issued Attack command." Logged via DiaObservation. Enables AI debugging without printf — the developer can see *why* an entity made a decision.

**Primary value:** AI behaviour is transparent and debuggable — developers can see what every entity is thinking and why.

---

---

### Gameplay — Combat & Tactics

---

### Candidate 35: Line of Sight / Cover System

**Home module/system:** Extension of `Dia/DiaVisibility/` or `Dia/DiaGeometry2D/`
**Size:** S (≤1 week)
**Description:** Tactical LOS for combat — distinct from fog-of-war visibility. Determines whether unit A can "see" (shoot) unit B considering terrain blockers and cover objects. Cover provides damage reduction or miss chance bonuses. Half-cover vs full-cover. Flanking (attacking from a side without cover) negates cover bonuses.

Uses raycasting against a cover map (separate from the movement grid). Results feed into combat resolution (damage modifiers) and AI targeting (prefer targets without cover).

**Primary value:** Positioning matters in combat — units behind cover survive longer, rewarding tactical movement and flanking.

---

### Candidate 36: Area of Effect (AoE) Resolution

**Home module/system:** Extension of `Dia/DiaCombat/` or standalone
**Size:** S (≤1 week)
**Description:** A system for resolving effects applied to spatial areas: circles, cones, lines, rings. Given an AoE shape, origin, and target point, determines which entities are affected and applies effects (damage, status, knockback) with optional falloff (full damage at center, reduced at edges).

Handles friendly fire rules, maximum target counts, and persistent AoE zones (poison cloud that lasts N seconds). Spatial queries use DiaGeometry2D intersection tests.

**Primary value:** Abilities can affect multiple entities spatially — enabling artillery, healing auras, and crowd-control that reward positioning.

---

### Candidate 37: Aggro Range / Engagement Rules

**Home module/system:** Extension of `Dia/DiaCommand/` or `Dia/DiaCombat/`
**Size:** S (≤1 week)
**Description:** Defines when and how entities automatically engage enemies. Rules: aggro range (auto-attack enemies within N cells), leash range (stop chasing beyond M cells), engagement priority (attack nearest vs attack my attacker), and hold-position override (don't aggro, only fire if in range).

Different stance modes: aggressive (seek enemies), defensive (attack if attacked or if enemy enters range), passive (never auto-engage). Stances set per entity or squad and integrate with the command queue.

**Primary value:** Units behave sensibly without micro-management — attacking nearby threats but not chasing across the map.

---

---

### Gameplay — Meta Systems

---

### Candidate 38: Population / Supply Cap System

**Home module/system:** Extension of `Dia/DiaEconomy/`
**Size:** S (≤1 week)
**Description:** Tracks unit population against a cap that can be raised by building supply structures. Each unit type has a supply cost. Training is blocked when at cap. Supply structures that are destroyed reduce the cap (potentially putting the faction over-cap, preventing new units until resolved).

Simple but game-critical for strategy balance — prevents infinite unit spam and creates strategic pressure to expand for supply.

**Primary value:** Unit counts are bounded and contested — creating meaningful decisions about army composition vs expansion.

---

### Candidate 39: Event Trigger / Scripted Sequence System

**Home module/system:** New module: `Dia/DiaTriggerScript/`
**Size:** S–M (1–2 weeks)
**Description:** A data-driven system for scripted game events triggered by conditions. Trigger types: spatial (entity enters region), temporal (time elapsed), state (entity HP below threshold), count (N enemies killed). Actions: spawn entities, play dialogue, change terrain, give resources, fire cutscene, change objective state.

Triggers are defined per-map in JSON. Supports one-shot and repeating triggers. Enables campaign/mission design: "when player crosses the river, spawn ambush from the forest." Works in tandem with DiaObjective for mission scripting.

**Primary value:** Designers can create scripted moments and mission events without C++ code — enabling campaign and narrative content.

---

### Candidate 40: Save / Load Game State Serialization

**Home module/system:** Extension of `Dia/DiaSerializer/` or new `Dia/DiaSaveGame/`
**Size:** M (1–3 weeks)
**Description:** Serialises complete game state (all entity positions, HP, resources, fog state, research progress, command queues, AI state) to a compact binary or JSON format that can be loaded to restore the exact game state. Handles versioning (older saves load in newer game versions with migration).

Every gameplay system must implement a save/load interface. The save system orchestrates the correct order (pause simulation, serialise all systems, resume). Integrates with DiaSerializer for format handling.

**Primary value:** Players can save and resume games — a basic expectation for strategy games with long match durations.

---

## Coverage Map

The 40 candidates span the full stack for a top-down strategy game.

**Genericity** rating:
- **High** = Any game genre could use this (engine primitive)
- **Medium** = Broadly useful but most valuable in strategy/tactics/RPG
- **Low** = Primarily useful in strategy/RTS games specifically

| # | Candidate | Size | Category | Genericity |
|---|-----------|------|----------|------------|
| 1 | Blackboard System | S | AI Decision | High |
| 2 | Utility AI System | M | AI Decision | High |
| 3 | Behaviour Tree Runtime | M | AI Decision | High |
| 4 | HTN Planner | L | AI Decision | Medium |
| 5 | Rule / Condition System | S | AI Decision | High |
| 6 | AI Personality / Difficulty | S | AI Decision | High |
| 7 | Grid Pathfinding (A* + JPS) | M | Navigation | High |
| 8 | Flow Field Navigation | M | Navigation | Medium |
| 9 | Steering Behaviours | S–M | Navigation | High |
| 10 | Dynamic Obstacle Avoidance (RVO) | M | Navigation | Medium |
| 11 | Hierarchical Pathfinding (HPA*) | M | Navigation | Medium |
| 12 | Terrain Analysis / Strategic Points | S–M | Navigation | Medium |
| 13 | Influence Maps | S–M | Spatial Intel | Medium |
| 14 | Fog of War / Visibility | M | Spatial Intel | Low |
| 15 | Command Queue / Action System | S | Command | High |
| 16 | Ability / Cooldown System | S–M | Command | High |
| 17 | Status Effect / Buff System | S | Command | High |
| 18 | Damage / Combat Resolution | S–M | Combat | High |
| 19 | Target Selection & Threat | S | Combat | High |
| 20 | Resource / Economy System | S–M | Economy | Medium |
| 21 | Tech Tree / Upgrade System | S | Economy | Medium |
| 22 | Building / Construction System | M | Economy | Low |
| 23 | Squad / Formation System | M | Coordination | Medium |
| 24 | Rally Point / Waypoint System | S | Coordination | Medium |
| 25 | Communication / Signal System | S | Coordination | Medium |
| 26 | Terrain Effect / Movement Cost | S | World | High |
| 27 | Spawn / Wave System | S | World | High |
| 28 | Objective / Win Condition System | S | World | High |
| 29 | Turn / Phase Manager | S–M | World | Medium |
| 30 | Minimap / Strategic Overview Data | S | World | Medium |
| 31 | AI Budget / Time-Slicing | S | Infrastructure | High |
| 32 | AI Sensor Framework | S | Infrastructure | High |
| 33 | Replay / Deterministic Simulation | M | Infrastructure | High |
| 34 | AI Debug / Inspection Layer | S | Infrastructure | High |
| 35 | Line of Sight / Cover System | S | Combat | Medium |
| 36 | Area of Effect (AoE) Resolution | S | Combat | High |
| 37 | Aggro Range / Engagement Rules | S | Combat | Medium |
| 38 | Population / Supply Cap | S | Meta | Low |
| 39 | Event Trigger / Scripted Sequences | S–M | Meta | High |
| 40 | Save / Load Game State | M | Meta | High |

### Summary

| Genericity | Count | Implication |
|------------|-------|-------------|
| High | 22 | Engine primitives — build first, reusable across all future games |
| Medium | 14 | Broadly useful — valuable for strategy but also RPG, action, etc. |
| Low | 4 | Strategy-specific — build when committed to the genre |

Size distribution: 14×S, 12×S–M, 11×M, 1×L, 0×XL — all individually tractable. The priority question is dependency ordering and "what's the minimum viable set to get a strategy game loop running."

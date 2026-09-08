# Research: Evaluate — AI & Gameplay Tools

**Input:** docs/research/ai_gameplay_tools/ideate.md

## Scoring Criteria

- **Engine Value** (weight 0.25): Improves Dia module reusability or capability across multiple games
- **Game Value** (weight 0.20): Improves CluicheTest as a strategy demo / testbed
- **Implementation Cost** (weight 0.25): Inverse of effort — 5 = very cheap (S-size, builds on existing), 1 = very expensive (L-size, novel)
- **Risk** (weight 0.15): Inverse of uncertainty — 5 = well-understood textbook pattern, 1 = significant design unknowns
- **Cluiche Fit** (weight 0.15): Aligns with module structure and PD-001 through PD-007, builds on existing Dia infrastructure

## Scores

| # | Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|---|-----------|:---:|:---:|:---:|:---:|:---:|:---:|
| 15 | Command Queue / Action System | 5 | 5 | 5 | 5 | 5 | **5.00** |
| 26 | Terrain Effect / Movement Cost | 5 | 5 | 5 | 5 | 5 | **5.00** |
| 27 | Spawn / Wave System | 5 | 5 | 5 | 5 | 5 | **5.00** |
| 28 | Objective / Win Condition | 5 | 5 | 5 | 5 | 5 | **5.00** |
| 36 | Area of Effect Resolution | 5 | 5 | 5 | 5 | 5 | **5.00** |
| 1 | Blackboard System | 5 | 4 | 5 | 5 | 5 | **4.80** |
| 19 | Target Selection & Threat | 4 | 5 | 5 | 5 | 5 | **4.75** |
| 17 | Status Effect / Buff System | 5 | 4 | 5 | 5 | 4 | **4.65** |
| 31 | AI Budget / Time-Slicing | 5 | 4 | 5 | 4 | 5 | **4.65** |
| 32 | AI Sensor Framework | 5 | 4 | 5 | 4 | 5 | **4.65** |
| 39 | Event Trigger / Scripted Sequences | 5 | 5 | 4 | 4 | 5 | **4.60** |
| 5 | Rule / Condition System | 4 | 4 | 5 | 5 | 5 | **4.55** |
| 16 | Ability / Cooldown System | 5 | 4 | 4 | 5 | 5 | **4.55** |
| 37 | Aggro Range / Engagement Rules | 4 | 4 | 5 | 5 | 5 | **4.55** |
| 7 | Grid Pathfinding (A* + JPS) | 5 | 5 | 3 | 5 | 5 | **4.50** |
| 9 | Steering Behaviours | 5 | 4 | 4 | 5 | 4 | **4.40** |
| 34 | AI Debug / Inspection Layer | 4 | 4 | 4 | 5 | 5 | **4.30** |
| 18 | Damage / Combat Resolution | 4 | 5 | 4 | 4 | 4 | **4.20** |
| 2 | Utility AI System | 5 | 4 | 3 | 4 | 5 | **4.15** |
| 24 | Rally Point / Waypoint System | 3 | 4 | 5 | 5 | 4 | **4.15** |
| 21 | Tech Tree / Upgrade System | 3 | 3 | 5 | 5 | 5 | **4.10** |
| 3 | Behaviour Tree Runtime | 5 | 4 | 3 | 4 | 4 | **4.00** |
| 6 | AI Personality / Difficulty | 3 | 3 | 5 | 5 | 4 | **3.95** |
| 25 | Communication / Signal System | 3 | 3 | 5 | 4 | 5 | **3.95** |
| 30 | Minimap / Strategic Overview | 3 | 3 | 5 | 5 | 4 | **3.95** |
| 8 | Flow Field Navigation | 4 | 4 | 3 | 4 | 4 | **3.75** |
| 13 | Influence Maps | 3 | 4 | 4 | 4 | 4 | **3.75** |
| 20 | Resource / Economy System | 3 | 4 | 4 | 4 | 4 | **3.75** |
| 35 | Line of Sight / Cover System | 3 | 4 | 4 | 4 | 4 | **3.75** |
| 38 | Population / Supply Cap | 2 | 3 | 5 | 5 | 4 | **3.70** |
| 12 | Terrain Analysis / Strategic Points | 3 | 4 | 4 | 3 | 4 | **3.60** |
| 40 | Save / Load Game State | 5 | 4 | 2 | 3 | 4 | **3.60** |
| 29 | Turn / Phase Manager | 3 | 3 | 4 | 4 | 4 | **3.55** |
| 33 | Replay / Deterministic Simulation | 5 | 4 | 2 | 2 | 4 | **3.45** |
| 23 | Squad / Formation System | 3 | 4 | 3 | 3 | 4 | **3.35** |
| 11 | Hierarchical Pathfinding (HPA*) | 3 | 3 | 3 | 4 | 4 | **3.30** |
| 14 | Fog of War / Visibility | 2 | 4 | 3 | 4 | 4 | **3.25** |
| 22 | Building / Construction System | 2 | 4 | 3 | 3 | 4 | **3.10** |
| 4 | HTN Planner | 4 | 3 | 2 | 2 | 4 | **3.00** |
| 10 | Dynamic Obstacle Avoidance (RVO) | 3 | 3 | 2 | 3 | 3 | **2.75** |

## Priority Tiers

Rather than a simple "top 3," this research produces a **dependency-ordered implementation roadmap**. The 40 systems form natural tiers where each tier builds on the previous.

---

### Tier 1: Foundations (score ≥ 4.55, all size S, all genericity High)

These are cheap, risk-free, highly generic primitives that enable everything else. Build these first.

| # | System | Score | Depends On |
|---|--------|:-----:|------------|
| 15 | Command Queue / Action System | 5.00 | DiaStateMachine, DiaMailbox |
| 26 | Terrain Effect / Movement Cost | 5.00 | DiaGeometry2D grids |
| 27 | Spawn / Wave System | 5.00 | DiaEntity, DiaCore/Timer |
| 28 | Objective / Win Condition | 5.00 | DiaStreams |
| 36 | Area of Effect Resolution | 5.00 | DiaGeometry2D intersection |
| 1 | Blackboard System | 4.80 | DiaCore containers, StringCRC |
| 19 | Target Selection & Threat | 4.75 | DiaGeometry2D spatial |
| 17 | Status Effect / Buff System | 4.65 | DiaCore/Timer, DiaEntity |
| 31 | AI Budget / Time-Slicing | 4.65 | DiaApplicationFlow (Module) |
| 32 | AI Sensor Framework | 4.65 | DiaGeometry2D, Blackboard (C1) |
| 5 | Rule / Condition System | 4.55 | Blackboard (C1) |
| 16 | Ability / Cooldown System | 4.55 | DiaCore/Timer, Command Queue (C15) |
| 37 | Aggro Range / Engagement Rules | 4.55 | Command Queue (C15), Target Selection (C19) |

**Why these first:** All score ≥4.55 with minimal risk. Most have zero inter-dependencies beyond existing Dia modules. The few that depend on other candidates (C32→C1, C5→C1, C16→C15, C37→C15+C19) form a natural within-tier ordering. After this tier, you have: entities that can receive commands, perceive the world, pick targets, use abilities, take status effects, operate under time budgets, and win/lose — a playable strategy skeleton.

---

### Tier 2: Core AI + Combat (score 4.00–4.50)

Once the foundations exist, these systems build intelligent behaviour and meaningful combat.

| # | System | Score | Depends On |
|---|--------|:-----:|------------|
| 7 | Grid Pathfinding (A* + JPS) | 4.50 | Terrain Effect (C26), DiaGeometry2D grids |
| 9 | Steering Behaviours | 4.40 | Pathfinding (C7) for macro route |
| 39 | Event Trigger / Scripted Sequences | 4.60 | Objective (C28), DiaStreams |
| 34 | AI Debug / Inspection Layer | 4.30 | DiaObservation, Blackboard (C1) |
| 18 | Damage / Combat Resolution | 4.20 | Status Effect (C17), Target Selection (C19) |
| 2 | Utility AI System | 4.15 | Blackboard (C1), AI Budget (C31) |
| 24 | Rally Point / Waypoint System | 4.15 | Command Queue (C15), Pathfinding (C7) |
| 3 | Behaviour Tree Runtime | 4.00 | Blackboard (C1), Command Queue (C15), AI Budget (C31) |

**Why second:** These need Tier 1 foundations. Pathfinding needs terrain costs. Utility/BT need blackboards. Combat resolution needs status effects. Event triggers need objectives. After this tier, units navigate, make decisions, fight with real combat math, and the game has scripted events.

---

### Tier 3: Strategy Layer (score 3.55–3.95)

Systems that make the game specifically a *strategy* game. Build after the core game loop works.

| # | System | Score | Depends On |
|---|--------|:-----:|------------|
| 6 | AI Personality / Difficulty | 3.95 | Utility AI (C2) or BT (C3) |
| 25 | Communication / Signal System | 3.95 | DiaMailbox, Blackboard (C1) |
| 30 | Minimap Data Layer | 3.95 | Visibility (C14) or Influence (C13) |
| 8 | Flow Field Navigation | 3.75 | Pathfinding (C7), Terrain Effect (C26) |
| 13 | Influence Maps | 3.75 | DiaGeometry2D grids |
| 20 | Resource / Economy System | 3.75 | DiaStreams, DiaEntity |
| 35 | Line of Sight / Cover | 3.75 | DiaGeometry2D intersection |
| 21 | Tech Tree / Upgrade System | 4.10 | Economy (C20), DiaCore/DirectedGraph |
| 29 | Turn / Phase Manager | 3.55 | Command Queue (C15) |

**Why third:** These differentiate a generic game loop into a strategy game specifically. Economy, tech trees, influence maps, flow fields for mass movement, cover mechanics. After this tier you have a recognisable strategy game.

---

### Tier 4: Advanced & Genre-Specific (score < 3.55)

Systems that add depth, polish, or enable multiplayer. Build when the game is playable and you know what it needs.

| # | System | Score | Depends On |
|---|--------|:-----:|------------|
| 33 | Replay / Determinism | 3.45 | Pervasive (all systems must be deterministic) |
| 40 | Save / Load Game State | 3.60 | All gameplay systems (serialization interfaces) |
| 23 | Squad / Formation | 3.35 | Pathfinding (C7), Command Queue (C15), DiaEntity hierarchy |
| 14 | Fog of War / Visibility | 3.25 | DiaGeometry2D, Sensors (C32) |
| 22 | Building / Construction | 3.10 | Economy (C20), Pathfinding (C7), DiaEntity |
| 12 | Terrain Analysis | 3.60 | Pathfinding (C7), Influence Maps (C13) |
| 11 | HPA* | 3.30 | Pathfinding (C7) |
| 10 | RVO/ORCA | 2.75 | Steering (C9) |
| 4 | HTN Planner | 3.00 | Blackboard (C1), Command Queue (C15) |
| 38 | Population / Supply Cap | 3.70 | Economy (C20) |

**Why last:** These are either pervasive (replay/save require ALL systems to cooperate), genre-narrow (fog, building, pop cap only matter for committed strategy), or optimisation layers (HPA*, RVO only needed at scale). They're valuable but depend on having a working game first.

---

## Dependency Stacks

Systems that layer on top of each other — building the bottom enables the layers above.

### Stack 1: AI Decision Pipeline

```
Blackboard (C1)           ← foundation: shared knowledge store
  ├─ Rule/Condition (C5)  ← simplest reactive layer
  ├─ Utility AI (C2)      ← strategic scoring
  ├─ Behaviour Tree (C3)  ← tactical sequences
  └─ HTN Planner (C4)     ← full strategic planning
      └─ AI Personality (C6) ← multiplier layer on any of the above
```

All decision systems read/write the same blackboard. Build upward in complexity — rules for simple cases, utility for strategy, BT for tactics, HTN if you need long-range planning.

### Stack 2: Navigation

```
Terrain Effect / Movement Cost (C26)  ← what cells cost
  └─ Grid Pathfinding A* (C7)        ← single-unit routes
      ├─ Flow Fields (C8)            ← multi-unit to same dest
      ├─ HPA* (C11)                  ← large map optimisation
      └─ Steering (C9)              ← local movement below path
          └─ RVO/ORCA (C10)         ← dense crowd handling
```

### Stack 3: Combat

```
Target Selection (C19)                ← who to fight
  └─ Damage / Combat Resolution (C18) ← what happens when you fight
      ├─ Status Effects (C17)         ← modifiers during combat
      ├─ AoE Resolution (C36)         ← multi-target hits
      ├─ Cover / LOS (C35)           ← positional modifiers
      └─ Ability / Cooldown (C16)    ← special combat actions
          └─ Aggro / Engagement (C37) ← auto-combat rules
```

### Stack 4: Command & Coordination

```
Command Queue (C15)                  ← entity receives instructions
  ├─ Rally / Waypoint (C24)         ← persistent movement orders
  ├─ Squad / Formation (C23)        ← group commands
  └─ Engagement Rules (C37)         ← auto-issuing commands
```

### Stack 5: Economy & Building

```
Resource / Economy (C20)             ← currencies exist
  ├─ Tech Tree (C21)                ← spend to unlock
  ├─ Building / Construction (C22)  ← spend to build
  └─ Population Cap (C38)           ← supply limits
```

### Stack 6: World / Meta

```
Spawn / Wave (C27)                   ← entities appear
Objective / Win Condition (C28)      ← game has goals
Event Trigger / Script (C39)         ← scripted moments
  └─ These three compose into mission/campaign design
```

### Stack 7: Infrastructure (crosscutting)

```
AI Budget (C31)                      ← wraps all AI ticking
AI Sensors (C32)                     ← writes to Blackboard (C1)
AI Debug (C34)                       ← reads from everything
Replay / Determinism (C33)           ← constrains everything
Save / Load (C40)                    ← serialises everything
```

### Minimum Viable Set

One layer from each stack produces a playable prototype:

| Stack | Bottom Layer | Why |
|-------|-------------|-----|
| Decision | Blackboard (C1) | Everything reads/writes here |
| Navigation | Terrain Cost (C26) + A* (C7) | Units can move |
| Combat | Target Selection (C19) + Damage (C18) | Units can fight |
| Command | Command Queue (C15) | Units receive orders |
| World | Spawn (C27) + Objective (C28) | Game starts and ends |

These 8 systems = "units spawn, navigate terrain, pick targets, fight, and someone wins."

---

## Recommendation

**The output of this research is not "pick one candidate"** — it's a prioritised build order for an entire gameplay systems stack. The key insight is:

1. **Tier 1 is the research target.** Each of the 13 Tier 1 systems should become its own `/research` → `/spec-feature` → implementation cycle. They're all S-sized, high-genericity, low-risk, and form the minimal viable set for a playable strategy prototype.

2. **The "minimum playable strategy" set is ~8 systems:** Command Queue (C15) + Blackboard (C1) + Grid Pathfinding (C7) + Target Selection (C19) + Damage Resolution (C18) + Spawn/Wave (C27) + Objective (C28) + Terrain Effect (C26). Everything else adds depth but these 8 produce "units spawn, navigate terrain, pick targets, fight, and someone wins."

3. **Genericity tracks priority almost perfectly.** The top 13 candidates (Tier 1) are all High-genericity — they'll serve any future game, not just a strategy game. The strategy-specific systems (Low-genericity: fog, building, pop cap) naturally fall to Tier 4. Build generic first, specialise later.

4. **DiaStateMachine, DiaGeometry2D, DiaMailbox, and DiaStreams already provide significant foundations** — the Tier 1 systems plug into these rather than building from scratch. This validates the existing module investments.

The next step is to pick the first few Tier 1 systems for individual `/research` sessions to nail down their API design before speccing.

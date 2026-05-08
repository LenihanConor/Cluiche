# Research: Evaluate — Visual Debuggers

**Input:** docs/research/visual_debug/ideate.md

---

## Scoring Criteria

| Axis | Weight | Description |
|------|--------|-------------|
| **Engine Value** | 0.25 | Improves Dia module reusability, capability, or coherence |
| **Game Value** | 0.20 | Improves CluicheTest as a demo/testbed; helps debug real gameplay problems |
| **Implementation Cost** | 0.25 | Inverse of effort — 5 = very cheap, 1 = very expensive |
| **Risk** | 0.15 | Inverse of uncertainty — 5 = well-understood, 1 = highly uncertain |
| **Cluiche Fit** | 0.15 | Aligns with module structure, PD-001–PD-008, and existing patterns |

Weighted total = (Engine×0.25) + (Game×0.20) + (Cost×0.25) + (Risk×0.15) + (Fit×0.15)

Deferred candidates (10, 11) are excluded from scoring — their seams are accounted for in Candidate 1 and 7.

---

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | **Total** |
|-----------|:---:|:---:|:---:|:---:|:---:|:---:|
| 1 — DebugLayerManager | 5 | 4 | 3 | 4 | 5 | **4.15** |
| 2 — Options pass (existing debuggers) | 4 | 5 | 5 | 5 | 5 | **4.75** |
| 3 — DiaIK2DVisualDebugger | 3 | 4 | 5 | 5 | 5 | **4.20** |
| 4 — DiaGeometry2DVisualDebugger | 4 | 3 | 3 | 4 | 5 | **3.70** |
| 5 — DiaAnimation2DVisualDebugger | 4 | 4 | 3 | 2 | 5 | **3.55** |
| 6 — TextPrimitive | 5 | 3 | 4 | 3 | 4 | **3.95** |
| 7 — Configurable budget | 4 | 3 | 5 | 5 | 5 | **4.25** |
| 8 — Editor layer panel | 4 | 4 | 4 | 4 | 5 | **4.15** |
| 9 — DiaVisualDebuggerConsole (ImGui) | 3 | 5 | 3 | 3 | 4 | **3.65** |

---

## Score Rationale

**Candidate 1 — DebugLayerManager (4.15)**  
Engine value 5: foundational — every other debugger depends on it. Game value 4: doesn't directly draw anything but unlocks everything else. Cost 3: non-trivial — DiaAPI command registration, sub-layer protocol, broadcast to editor, priority sort, scale, picking seams. Risk 4: well-understood patterns (registry, command dispatch), but the sub-layer broadcast protocol has some design surface. Fit 5: clean PD-001 (StringCRC), PD-002 (Module), PD-004 (DynamicArrayC) alignment.

**Candidate 2 — Options pass (4.75)**  
Engine value 4: improves three existing modules. Game value 5: immediate payoff — developers can suppress noise (statics, sleeping bodies) while focusing on what matters. Cost 5: purely additive struct fields, no architectural change, defaults preserve backward compatibility. Risk 5: zero uncertainty. Fit 5: follows established `VisualDebuggerOptions` pattern. *Highest overall score — cheapest work with most immediate daily-use value.*

**Candidate 3 — DiaIK2DVisualDebugger (4.20)**  
Engine value 3: adds one debugger for one module. Game value 4: IK chains are actively used (foot placement, hand grabs) and impossible to debug blind. Cost 5: follows the exact established pattern; only new work is the `GetWorldTransforms()` accessor on `IKSolver`. Risk 5: fully understood. Fit 5: identical structure to existing debuggers.

**Candidate 4 — DiaGeometry2DVisualDebugger (3.70)**  
Engine value 4: spatial structure visualisation is genuinely useful for broad-phase debugging. Game value 3: less immediately needed than physics/IK — geometry bugs are rarer in day-to-day iteration. Cost 3: two distinct draw modes (shape + spatial structure) add more design surface; shape draw requires overloads for 13 types. Risk 4: design is clear but more implementation volume. Fit 5: clean module pattern.

**Candidate 5 — DiaAnimation2DVisualDebugger (3.55)**  
Engine value 4: animation debugging is critical once DiaAnimation2D ships. Game value 4: blend stack and spring chain debugging will be essential. Cost 3: medium effort when unblocked, but *currently blocked* — DiaAnimation2D is not yet implemented. Risk 2: the block is the key risk; can't be started now, and DiaAnimation2D's API isn't final yet so the debugger design may shift. Fit 5: follows the pattern cleanly. *Score depressed primarily by the blocker.*

**Candidate 6 — TextPrimitive (3.95)**  
Engine value 5: unlocks world-space text for all current and future debuggers — bone names, entity IDs, chain IDs, state labels. Game value 3: high value but indirect — players never see it, developers gain incremental benefit on top of existing shapes. Cost 4: small DiaGraphics change (one union variant + SFML visitor case), fixed-length char buffer keeps it PD-004 compliant. Risk 3: SFML text rendering with built-in font is simple, but the fixed-length buffer design needs care (truncation, encoding). Fit 4: slightly awkward that DiaGraphics gains a rendering dependency on a font (even built-in), but SFML already owns rendering so the dependency is contained.

**Candidate 7 — Configurable budget (4.25)**  
Engine value 4: removes a hard-coded limit and makes overflow diagnosable. Game value 3: rarely noticed until it's a crisis; then very high value. Cost 5: constructor parameter + two query methods — trivial change. Risk 5: no uncertainty. Fit 5: pure DiaGraphics internal change, no API surface impact on callers. *High score for a very small amount of work.*

**Candidate 8 — Editor layer panel (4.15)**  
Engine value 4: makes the editor a first-class control surface for all debug layers. Game value 4: reduces friction significantly — toggle layers without game window focus, see sub-layer tree, get overflow badge. Cost 4: small C++ plugin class + React checkbox tree component; depends on Candidate 1 but not on any other candidate. Risk 4: editor plugin pattern is established (DiaApplicationEditor has 15 features); CEF React component is straightforward. Fit 5: clean `IEditorPlugin` implementation, uses existing WebSocket command path.

**Candidate 9 — DiaVisualDebuggerConsole (3.65)**  
Engine value 3: adds a development convenience tool, not core engine capability. Game value 5: the single highest game value — complete in-game debug control without editor, command input for all DiaAPI commands, log tail, metrics. Cost 3: new external dependency (`imgui-sfml` bridge integration into DiaSFML), ImGui render loop wiring, four distinct panels to implement. Risk 3: ImGui is well-understood but the SFML integration and frame lifecycle (ImGui needs its own render pass) adds complexity; toggling on/off without disrupting the game render is non-trivial. Fit 4: ImGui sits slightly outside the DebugPrimitive/FrameData pattern; it renders via its own pass rather than through `RequestDraw()`.

---

## Top 3 Candidates

### Rank 1: Candidate 2 — VisualDebuggerOptions Pass (4.75)
**Why:** Highest score by a clear margin. Pure additive work on three modules that already exist — no new architecture, no dependencies to resolve, immediate daily-use value for every developer working with physics, rigs, or soft bodies. The `showSleepingBodies` and `showStaticBodies` flags alone will de-noise most debugging sessions. Aligns perfectly with the established `VisualDebuggerOptions` pattern (PD-004 compliant by default). Can ship before Candidate 1 is built since it doesn't require the manager.  
**Watch out for:** The options structs need to be defined in coordination with Candidate 1's sub-layer naming — if flags are named inconsistently now, the manager registration will be messy. Define the sub-layer naming convention first, then write the structs.

### Rank 2: Candidate 7 — Configurable DebugPrimitive Budget (4.25)
**Why:** Tiny effort (constructor param + two methods), zero risk, resolves the silent failure mode that will bite every developer running multiple debuggers simultaneously. `DroppedCount()` flowing into the editor panel overflow badge (Candidate 8) and DiaVisualDebuggerConsole metrics bar (Candidate 9) gives those systems meaningful data to surface. Should be the first DiaGraphics change made — before Candidate 2 generates more primitives.  
**Watch out for:** `DebugFrameData` is constructed in multiple places — find all call sites and verify the default-1024 constructor handles them all before adding the configurable overload.

### Rank 3: Candidate 3 — DiaIK2DVisualDebugger (4.20)
**Why:** Perfect score on cost, risk, and fit — follows the established pattern exactly. IK chains are actively exercised in CluicheTest (foot IK, hand grab) and joint limits + pole vectors are genuinely invisible without this. The only prerequisite is a `GetWorldTransforms()` accessor on `IKSolver`, which is a one-line addition. Delivers immediate diagnostic value for animation-adjacent work happening now, independent of DiaAnimation2D being built.  
**Watch out for:** The `suppressBonesIfRigActive` flag requires the IK debugger to query whether the Rig2D layer is enabled — it needs a read-only reference to the `DebugLayerManager` at draw time. Design this seam carefully so it's optional (nullptr = no suppression) to keep the debugger usable standalone.

---

## Full Ranking

| Rank | Candidate | Score | Key differentiator |
|------|-----------|-------|--------------------|
| 1 | 2 — Options pass | 4.75 | Cheapest, most immediate daily value |
| 2 | 7 — Configurable budget | 4.25 | Tiny effort, unblocks overflow visibility |
| 3 | 3 — DiaIK2DVisualDebugger | 4.20 | Cheapest new debugger, actively needed now |
| 4 | 1 — DebugLayerManager | 4.15 | Foundation for everything, medium effort |
| 4 | 8 — Editor layer panel | 4.15 | High value, depends on Candidate 1 |
| 6 | 6 — TextPrimitive | 3.95 | Unlocks all world-space labels |
| 7 | 4 — DiaGeometry2DVisualDebugger | 3.70 | High value, higher implementation volume |
| 8 | 9 — DiaVisualDebuggerConsole | 3.65 | Highest game value, medium cost + risk |
| 9 | 5 — DiaAnimation2DVisualDebugger | 3.55 | Blocked on DiaAnimation2D |

---

## Recommendation

**Candidate 2 (Options pass) scores highest, but the right answer is that this research covers a complete system, not a single feature.** The scoring reveals a natural build order that maximises value at each step:

1. **Candidate 7** first — budget fix is one hour of work and unblocks overflow visibility for everything that follows
2. **Candidate 2** next — options pass on existing debuggers, immediate daily value, defines sub-layer naming that Candidate 1 will formalise
3. **Candidate 1** — the manager; at this point there are already three debuggers to register, making the value concrete
4. **Candidate 3** — IK debugger slots straight in once the manager exists
5. **Candidates 8 + 6** — editor panel and TextPrimitive together (both S-sized, both serve the same "make debug info readable" goal)
6. **Candidate 9** — DiaVisualDebuggerConsole once the layer system is stable and worth wrapping in a console
7. **Candidate 4** — Geometry2D when spatial debugging becomes a pain point
8. **Candidate 5** — Animation2D when DiaAnimation2D ships

This is a strategy, not a single feature — the right spec structure is one system spec (`DiaVisualDebugger`) covering the manager, palette, and budget, with individual feature specs per debugger and per tool.

# Research: Evaluate — Animation & Rig Layer

**Input:** docs/research/anim_rig_layer/ideate.md

## Scoring Criteria

- **Engine Value (0.25):** How much does this improve Dia module reusability or capability beyond the dragon game?
- **Game Value (0.20):** How much does this improve CluicheTest / COW dragon game directly?
- **Implementation Cost (0.25):** Inverse of effort — 5 = very cheap (days), 1 = very expensive (months).
- **Risk (0.15):** Inverse of uncertainty — 5 = well-understood algorithm/pattern, 1 = highly uncertain.
- **Cluiche Fit (0.15):** Aligns with PD-001 through PD-007, module structure, testing preference.

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|-----------|---------------|-------------|-------------|-------------|------------|-------|
| 1. Minimal Flat Skeleton | 5 | 5 | 5 | 5 | 5 | **5.00** |
| 2. Two-Algorithm IK | 5 | 4 | 4 | 4 | 5 | **4.45** |
| 3. Spine2D JSON Loader | 3 | 5 | 4 | 4 | 4 | **3.95** |
| 4. Damped Spring Chain | 3 | 4 | 5 | 5 | 5 | **4.20** |
| 5. Keyframe Clip Player | 3 | 4 | 4 | 5 | 4 | **3.95** |
| 6. Procedural Locomotion Oscillator | 4 | 5 | 3 | 3 | 5 | **3.90** |
| 7. Look-At Constraint | 4 | 5 | 5 | 5 | 5 | **4.75** |
| 8. Pose Blend Stack | 4 | 4 | 4 | 4 | 5 | **4.15** |
| 9. Skeleton Debug Renderer | 3 | 5 | 5 | 5 | 5 | **4.40** |

## Top 3 Candidates

### Rank 1: Minimal Flat Skeleton (score: 5.00)
**Why:** Perfect score across all axes. It is the unavoidable foundation — nothing else can be built or tested without it. It also directly fixes the known DR-014 performance issue (dirty-flagged cached world transforms) in the same stroke. Fully PD-001 compliant via StringCRC bone names, PD-004 compliant via DiaCore containers, and trivially unit-testable with exact transform assertions.
**Watch out for:** The JSON schema for bone definitions needs to be locked early — changing it later cascades into the Spine2D loader and any authored skeleton files.

---

### Rank 2: Look-At Constraint (score: 4.75)
**Why:** Highest game value per cost of any candidate. A stateless free function in DiaIK — no heap allocation, no state, O(1) — that delivers the single most readable AI-intent signal in the game (where the dragon's head is pointing). Fully deterministic so every test case is a one-liner: known head transform + known target → assert exact output angle. Feeds directly into the Pose Blend Stack as an additive layer.
**Watch out for:** Needs a `LookAtSmoother` wrapper to avoid snapping on rapid target changes; that wrapper introduces dt-dependent state which requires time-step tests.

---

### Rank 3: Skeleton Debug Renderer (score: 4.40)
**Why:** Directly serves the heavy-testing preference — without it, validating that IK solvers, spring chains, and oscillators are producing sane output requires running the full game and eyeballing. With it, integration tests can assert visible bone positions and socket placements. Cost is very low (thin DiaGraphics adapter), risk is near zero (just draw calls), and COW doc marks debug visibility mandatory from Phase 1.
**Watch out for:** Must be strictly gated behind `#ifdef DIA_DEBUG` — unchecked debug rendering in a multi-dragon scene will destroy frame time.

---

## Full Ranking

| Rank | Candidate | Score | Notes |
|------|-----------|-------|-------|
| 1 | Minimal Flat Skeleton | 5.00 | Foundation; unblocks everything |
| 2 | Look-At Constraint | 4.75 | Highest value/cost ratio |
| 3 | Skeleton Debug Renderer | 4.40 | Enables test validation |
| 4 | Two-Algorithm IK | 4.45 | Unblocks locomotion oscillator |
| 5 | Damped Spring Chain | 4.20 | Cheap secondary motion |
| 6 | Pose Blend Stack | 4.15 | Needed once motion systems exist |
| 7 | Spine2D JSON Loader | 3.95 | AI authoring pipeline |
| 8 | Keyframe Clip Player | 3.95 | Signature moments (takeoff etc.) |
| 9 | Procedural Locomotion Oscillator | 3.90 | Most complex; deferred to Wave 3 |

## Recommendation

**Minimal Flat Skeleton** is the clear first candidate — it scored perfectly because it is not optional. Everything in the animation layer depends on a bone hierarchy existing, and building it first with dirty-flagged cached world transforms resolves a known engine bug (DR-014) as a side effect. It aligns with PD-001 (StringCRC bone names), PD-004 (DiaCore containers), and is the most testable artefact in the set: load a known JSON, assert exact bone world transforms, done.

The natural follow-on after the skeleton is **Look-At Constraint** (stateless, pure math, highest readability payoff) and **Two-Algorithm IK** (unblocks the oscillator and all limb contact work). Together these three — Skeleton + IK + Look-At — form a complete, fully testable Wave 1 that delivers visible dragon head tracking and correct limb positioning before any procedural locomotion or clip playback is written.

The **Procedural Locomotion Oscillator** scores lowest not because it is unimportant but because it has the most implementation uncertainty (gait feel requires iteration) and depends on everything else being stable first. It is correctly placed in Wave 3.

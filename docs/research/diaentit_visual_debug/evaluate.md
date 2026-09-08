# Research: Evaluate — diaentitytemplate Visual Debugger & Editor Options

**Input:** docs/research/diaentit_visual_debug/ideate.md

**Note:** Candidate 1 (In-game ImGui Inspector) dropped — it overlaps with Candidate 2 and
would be retired once the editor panel exists. Evaluation covers the remaining 7 candidates.

**Framing:** All candidates are eventually desirable. This evaluation scores for
*sequencing priority* — what to build first. Low cost + high fit = build now.

## Scoring Criteria

- **Engine Value** (0.25) — improves Dia module reusability or capability
- **Game Value** (0.20) — improves CluicheTest as a demo or testbed
- **Implementation Cost** (0.25) — inverse of effort; 5 = very cheap (S), 1 = very expensive (L+)
- **Risk** (0.15) — inverse of uncertainty; 5 = well-understood seam completion
- **Cluiche Fit** (0.15) — aligns with module structure and binding decisions

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | **Total** |
|-----------|:---:|:---:|:---:|:---:|:---:|:---:|
| 3 — Viewport Picking | 5 | 4 | 5 | 5 | 5 | **4.80** |
| 4 — Viewport Overlay | 4 | 4 | 5 | 4 | 5 | **4.40** |
| 2 — Editor Inspector Panel | 4 | 5 | 3 | 3 | 5 | **3.95** |
| 5 — Query Browser Tab | 3 | 4 | 4 | 4 | 5 | **3.90** |
| 6 — Mailbox Traffic Monitor | 4 | 3 | 2 | 3 | 4 | **3.15** |
| 8 — Entity Watch List | 3 | 3 | 2 | 3 | 4 | **2.90** |
| 7 — Blueprint JSON Editor | 3 | 4 | 1 | 2 | 4 | **2.70** |

## Top 3 Candidates

### Rank 1: Candidate 3 — Viewport Picking (score: 4.80)
**Why:** Pure seam completion. `SetSelectedEntityId()`, `GetSelectedEntityId()`, and `debug.pick` are already reserved across three specs. Wiring them costs ≤1 week and unlocks every other candidate. The result is system-agnostic — physics, animation, and entity inspector all read the same `uint32_t` selectedEntityId.
**Watch out for:** Hit-test priority ordering when multiple tagged primitives overlap.

### Rank 2: Candidate 4 — Entity Viewport Overlay (score: 4.40)
**Why:** Follows the `IVisualDebugger` draw class pattern exactly. Text primitive infrastructure (`DebugPrimitiveText2D`) is already Done. The position provider registry is the only novel piece. Together with C3, delivers world-space entity labels with highlight-on-select in under two weeks with zero editor dependency.
**Watch out for:** Entities without a registered position provider are silently skipped — clear contract needed.

### Rank 3: Candidate 2 — CluicheEditor Entity Inspector Panel (score: 3.95)
**Why:** The canonical long-term debugging surface. Establishes the `DiaEntityEditor` plugin that C5, C6, and C8 all extend. WebSocket topic design follows `debug.layer.state` pattern exactly. Tier (b) field edit feeds through `GameConnectionManager::SendUpdate()` with undo backed by `CommandHistory`.
**Watch out for:** Rate-limit the topic push — selection-change-triggered, not frame-tick.

## Recommendation

Two system targets:

**DiaVisualDebugger additions (S+S, ship together):** C3 → C4, both as new feature specs.

**DiaEntityEditor new system (M→S→M→M):** C2 → C5 → C6 → C8 as features in sequence.

**DiaEntityEntityTemplateEditor new system (separate workstream):** C7 + DiaCLI schema export prerequisite.

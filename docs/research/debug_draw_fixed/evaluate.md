# Research: Evaluate — Debug Draw for Fixed/Registered Objects

**Input:** docs/research/debug_draw_fixed/ideate.md (revised 2026-05-03)

---

## Scoring Criteria

- **Engine Value (0.25):** Improves Dia module reusability or capability — 5 = broadly reusable pattern, 1 = narrow one-off
- **Game Value (0.20):** Improves CluicheTest as demo/testbed — 5 = directly enables key debug workflows, 1 = marginal benefit
- **Implementation Cost (0.25):** Inverse of effort — 5 = very cheap (S), 1 = very expensive (XL)
- **Risk (0.15):** Inverse of uncertainty — 5 = well-understood, 1 = high unknowns
- **Cluiche Fit (0.15):** Aligns with PD-001 through PD-007, existing DiaVisualDebugger decisions, non-debug promotion path

---

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|-----------|:---:|:---:|:---:|:---:|:---:|:---:|
| A alone — Render-Thread Fixed Buffer | 4 | 4 | 3 | 4 | 5 | **3.95** |
| A + B — Fixed Buffer + IObjectRenderer | 5 | 5 | 3 | 3 | 5 | **4.20** |
| A + B + C — Full set with DebugDrawGroup | 5 | 5 | 2 | 3 | 5 | **3.95** |

---

## Analysis

### A alone (score: 3.95)
Delivers register-once + toggle at zero FrameData cost. Covers 80% of the stated need immediately. No renderer flexibility — every registered object type has one fixed visual. Straightforward to implement and test. The right thing to build first if you want to prove the thread handoff works before adding renderer complexity.

**Watch out for:** The buffer transfer across the sim/render thread boundary is the one genuinely novel piece. How does the buffer get from sim to render safely? Options: double-buffer swap at frame boundary (safest, zero locking); mutex-protected swap (simple, minor stall); message in the existing frame queue. This needs a decision before implementation.

### A + B (score: 4.20) — **recommended**
Adds `IObjectRenderer` at registration. Same thread safety story as A alone. The only additional cost is the interface definition and one concrete renderer per object type — which you'd be writing anyway (the draw logic has to live somewhere). Delivers all three stated goals: register once, toggle is the stream, custom renderers. Non-debug promotion path is clean — `IObjectRenderer::BuildPrimitives()` is the same signature whether output goes to a debug primitive buffer or a vertex buffer.

**Watch out for:** Two registrations of the same object under different renderer names means two buffers in memory simultaneously. For large structures (128×128 grid = ~1 MB per buffer), having a default + selected + heatmap renderer all registered at once is 3 MB for one grid. Budget this at the system level. Likely fine for debug-only; flag it in the spec.

### A + B + C (score: 3.95)
Adds `DebugDrawGroup` for multi-instance naming. Drops back to 3.95 because the cost of C is real (another class, more registration ceremony, more test surface) and the problem it solves — "I have two grids" — is uncommon in the near term. The SD-DBG-006 assert on name collision is the enforcement; callers can supply distinct names manually for now. Add C when the naming problem is actually felt.

---

## Top 3

### Rank 1: A + B (score: 4.20)
All three user requirements met. Thread safety is bounded to one decision point (buffer handoff mechanism). Renderer strategy is clean and promotion-ready. `IObjectRenderer` is a two-method interface — not over-engineered. Default renderer for each spatial type is a small concrete class. The dynamic overlay (selection highlighting) stays in FrameData as normal — no special handling needed.

### Rank 2: A alone (score: 3.95)
Good starting point. Prove the buffer handoff works, get grids drawing for free, then add B when you want custom renderers. Risk of needing to retrofit B into A later is low — A's registration signature just gains an optional renderer parameter.

### Rank 3: A + B + C (score: 3.95)
Same score as A alone but higher complexity. Defer C until you actually have multiple instances of the same type and the naming collision becomes a real problem.

---

## Recommendation

**A + B** is the right choice. It is the minimum set that fully delivers the stated goals and has a clear non-debug promotion path. The key open decision — how the buffer crosses the thread boundary — should be resolved in the spec before implementation starts. Double-buffer swap at frame boundary is the recommendation: no locking, deterministic, already understood from the FrameData producer/consumer pattern.

`DebugDrawGroup` (C) is deferred. Not because it's a bad idea — it's a good one — but because naming two grids manually is a day-one problem that isn't felt yet. Add it as a follow-on feature spec.

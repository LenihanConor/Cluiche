# Research: Evaluate — Render Backend Swap (replace SFML)

**Input:** docs/research/render_backend_swap/ideate.md

## Scoring Criteria

- **Engine Value (0.25)** — How much it improves Dia module reusability or long-term capability
- **Game Value (0.20)** — How much it improves CluicheTest (and future games) as a demo/testbed for the stated "light 3D" target
- **Implementation Cost (0.25)** — Inverse of effort: 5 = very cheap, 1 = very expensive
- **Risk (0.15)** — Inverse of uncertainty: 5 = well-understood, 1 = highly uncertain
- **Cluiche Fit (0.15)** — Alignment with the module hierarchy and PD-001 through PD-007

## Scores

| # | Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|---|-----------|--------------:|------------:|------------:|------------:|-----------:|------:|
| 10 | bgfx phased (Parity → Light 3D) | 5 | 5 | 3 | 5 | 4 | **4.35** |
| 1 | bgfx (single milestone) | 5 | 4 | 2 | 4 | 4 | **3.75** |
| 3 | Sokol | 3 | 3 | 4 | 3 | 4 | **3.40** |
| 5 | D3D11 native | 3 | 4 | 2 | 4 | 5 | **3.40** |
| 2 | Diligent Engine | 4 | 3 | 2 | 3 | 4 | **3.15** |
| 9 | Magnum | 4 | 3 | 2 | 2 | 3 | **2.85** |
| 6 | D3D12 native | 3 | 3 | 1 | 3 | 5 | **2.80** |
| 8 | SFML 3.0 upgrade + custom 3D layer | 1 | 2 | 5 | 4 | 2 | **2.80** |
| 7 | SDL_GPU | 3 | 3 | 3 | 2 | 2 | **2.70** |
| 4 | Filament | 3 | 4 | 1 | 2 | 2 | **2.40** |

## Top 3 Candidates

### Rank 1: bgfx phased (Parity → Light 3D) (score: 4.35)
**Why:** Matches the user's stated trajectory exactly — *parity first, then light 3D*. Phase 1 (M) buys a verified-on-tests SFML-replacement (sprites, debug primitives, UI overlay, ImGui) before any 3D risk is taken on; Phase 2 (M) adds static mesh, skinned mesh, animation, and a simple forward + shadow shader. bgfx itself is production-proven, has excellent debug-draw ergonomics for `DebugFrameData`, supports multi-view rendering needed for editor + game side-by-side, and respects PD-001/PD-004/PD-005/PD-006/PD-007 cleanly at the public API layer.
**Watch out for:** bgfx's shaderc + `varying.def.sc` shader pipeline is opinionated and must be wired into DiaPipelineEditor. The CMake-built lib needs a prebuild step that DiaCLI orchestrates so vcxproj integration stays clean per PD-006. Phase boundary (parity → 3D) must be enforced as a real ship gate, not a soft milestone.

### Rank 2: bgfx (single milestone) (score: 3.75)
**Why:** Same backend, same long-term win — strong engine value, future-proof multi-backend GPU layer, and a clean DiaGraphics seam. Loses to the phased version only because it accepts more delivery risk by bundling parity and 3D into one large work item.
**Watch out for:** L-sized features without an internal "parity ship gate" tend to drift. If you want this shape, write the spec with explicit `Phase 1 done = SFML deletable from render path` as an in-spec checkpoint.

### Rank 3: D3D11 native (score: 3.40)
**Why:** Highest **Cluiche Fit** score (5) — perfect alignment with PD-005 (Windows-only x64), HLSL natively (no shader transpile needed), and zero third-party renderer surface to maintain. Mature, predictable, and the easiest path to use PIX/RenderDoc productively. Tied with Sokol (3.40) but ranked above it because Fit and Risk are both higher and the long-term cost of a bgfx/Sokol shader pipeline is non-trivial.
**Watch out for:** Every renderer feature is hand-written — sprite batch, debug primitive batch, mesh, skinning, shadow map. Total engineering surface is comparable to bgfx+ours in Phase 2 of Candidate 10, but without the multi-backend insurance. If "Windows is forever" stays true (PD-005 status), this is fine; if not, it's a future migration debt.

## Recommendation

**Adopt the bgfx phased plan (Candidate 10).** It dominates the score table because it converts the largest stated risk — bundling parity and 3D into one undifferentiated push — into two shippable milestones, while still committing to the strongest mid-level backend (Candidate 1). The user's brief — *light 3D within 6–12 months, no photorealism, keep SFML for window/input* — maps directly onto Phase 1 (parity, SFML still owns the window) and Phase 2 (terrain + models + skinned anim + simple lighting), with a clean hand-off to a future SDL window/input migration captured as separate research.

It honours the binding decisions: PD-005 is satisfied (D3D11/D3D12/Vulkan all available via bgfx on Windows x64); PD-004 is preserved by keeping `Graphics::ICanvas` and `FrameData` as the only public surface DiaGraphics consumers see; PD-006 is satisfied by prebuilding bgfx and consuming it via `.vcxproj` references.

The two-phase shape also leaves a clean fallback: if Phase 1 reveals integration problems with SFML's window-context coexistence, the team can pivot to D3D11 (Rank 3) without having committed to Phase 2's 3D scope.

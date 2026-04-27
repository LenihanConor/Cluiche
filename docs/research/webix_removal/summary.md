# Research Summary — Webix Removal

**Session folder:** docs/research/webix_removal/
**Date:** 2026-04-26

## One-Line Answer

Replace Webix with a tiered convention — Alpine.js + DaisyUI for simple self-contained game panels, React + Vite + shadcn/ui for complex multi-panel sim debug UIs — giving zero-cost simple overlays and full shared-state component power where needed.

## Journey

1. **Explored:** Webix is a dormant GPL dependency that was never wired into an active backend; the real question is what JS framework should be canonical for game-facing UI in Cluiche going forward. The DiaUI abstraction (IUISystem, Page, BoundMethod) is already framework-agnostic, so the choice is purely in the web layer.

2. **Ideated:** 7 candidates generated ranging from zero-dependency vanilla JS to full React + Vite, covering the full spectrum of build complexity, reactivity models, and component library availability.

3. **Evaluated:** Three rounds of evaluation — scope was progressively narrowed (all UI → game-only → sim-game context) and two extra scoring axes (Customizability, Performance) were added mid-session. Tiered (React + Alpine) emerged as the top scorer at 4.20 once the sim-game target was confirmed.

4. **Chose:** User confirmed Tiered (React + Alpine) with four pre-spec commitments defining the boundary rule, DaisyUI mandate, upgrade path, and build pipeline separation.

## Chosen Work Item

**Name:** Tiered Game UI Convention — React + Alpine
**Home module:** DiaUI (game-facing pages) / new DiaUIGame system
**Suggested spec type:** System (convention + tooling affecting all game UI pages) or Feature (if scoped to initial panel set)
**Estimated size:** M (1–3 weeks — define convention, create templates for both tiers, remove Webix, create first panels)

## Key Insights from Exploration

- **Webix was never integrated** — removal is purely a cleanup task (delete `External/Webix/`, update `deps.json`); the real work is establishing what replaces it
- **Alpine's ceiling is real for sim games** — shared selection state across panels (entity inspector → resource panel → pathfinding overlay) is exactly where Alpine's `x-data` scoping breaks down; this drove the final recommendation away from Alpine-only
- **DaisyUI is the theming answer** — its `data-theme` attribute + CSS custom properties is the cleanest per-game skinning mechanism found; mandated for the Alpine tier
- **React is already in the repo** — the React tier adds zero new infrastructure; the Vite pipeline is proven in CluicheEditor
- **The boundary rule must be written explicitly** — "self-contained panel → Alpine, cross-panel state → React" is the single most important convention to document; without it developers make inconsistent choices
- **Preact + HTM was considered and rejected** — it would become a redundant middle tier once React is available; the upgrade path is Alpine → React directly

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| C1: React + Vite + shadcn/ui | Build step on every panel including trivial ones; iteration friction on debug tooling |
| C2: Alpine + Tailwind + DaisyUI (alone) | Shared state limitation is a genuine failure mode for sim-game multi-panel UIs |
| C3: Vue 3 + Vite + PrimeVue | Introduces Vue alongside React; fragmentation with no advantage |
| C4: Preact + HTM | HTM syntax LLM pitfall; no component library; redundant once React is available |
| C5: Vanilla JS + Tailwind CDN | No reactivity model; sim-game panels become unmaintainable quickly |
| C6: Lit + Shoelace | Weakest LLM familiarity; Shoelace rebranding adds API churn risk |

## References

- docs/research/webix_removal/explore.md
- docs/research/webix_removal/ideate.md
- docs/research/webix_removal/evaluate.md
- docs/research/webix_removal/choose.md

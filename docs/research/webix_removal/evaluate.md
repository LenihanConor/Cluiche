# Research: Evaluate — Webix Removal

**Input:** docs/research/webix_removal/ideate.md
**Scope:** In-game UI only (CluicheTest debug panels, HUDs, tool overlays). Editor tech stack is fixed as React + TypeScript — that is not a choice here, but React is available and acceptable for game UI too.
**Context:** Target games are sim-style — complex multi-panel debug UIs with shared selection state across panels are expected.

## Scoring Criteria

| Axis | Weight | Description |
|------|--------|-------------|
| **Engine Value** | 0.15 | Improves Dia module reusability or capability for game-facing UI |
| **Game Value** | 0.20 | Improves CluicheTest as a sim-game testbed — complex panels, cross-panel state, debug tools |
| **Implementation Cost** | 0.15 | Inverse of effort — 5 = drop-in single file, 1 = heavy pipeline required |
| **Risk** | 0.10 | Inverse of uncertainty — 5 = well-understood, 1 = highly uncertain for sim-scale UI |
| **Cluiche Fit** | 0.10 | Aligns with module structure and PD decisions; no conflict with editor stack |
| **Customizability** | 0.15 | How easily UI can be skinned/themed per-game (CSS tokens, design system support) |
| **Performance** | 0.15 | Runtime CPU and memory cost inside CEF — lower bundle, lighter runtime = higher score |

Note: C7 Tiered is reframed for game-only scope — React for complex game panels, Alpine for simple game overlays (editor is not in scope).

## Scores

| Candidate | Engine (0.15) | Game (0.20) | Cost (0.15) | Risk (0.10) | Fit (0.10) | Custom (0.15) | Perf (0.15) | **Total** |
|-----------|---------------|-------------|-------------|-------------|------------|---------------|-------------|-----------|
| C7: Tiered (React + Alpine) | 4 | 5 | 3 | 3 | 5 | 5 | 4 | **4.20** |
| C1: React + Vite + shadcn/ui | 4 | 5 | 3 | 5 | 4 | 5 | 3 | **4.15** |
| C4: Preact + HTM | 3 | 5 | 4 | 3 | 4 | 4 | 5 | **4.10** |
| C2: Alpine + Tailwind + DaisyUI | 3 | 3 | 5 | 2 | 4 | 5 | 5 | **3.90** |
| C5: Vanilla JS + Tailwind CDN | 2 | 2 | 5 | 4 | 5 | 4 | 5 | **3.70** |
| C6: Lit + Shoelace | 3 | 3 | 3 | 3 | 4 | 4 | 4 | **3.40** |
| C3: Vue 3 + Vite + PrimeVue | 3 | 4 | 3 | 3 | 3 | 3 | 3 | **3.20** |

### Score Rationale Notes

**C7: Tiered (React + Alpine) — game-only reframe**
- Engine: 4 — establishes a formal two-tier documented convention for all game UI; reusable across future Cluiche games
- Game: 5 — React handles complex sim debug panels (entity inspectors, production chains, pathfinding overlays); Alpine handles zero-cost simple overlays (FPS counter, log stream, mini-map); right tool at every complexity level
- Cost: 3 — React panels need a build step; convention boundary rule adds a decision overhead; two frameworks to maintain
- Risk: 3 — boundary rule ("when does a panel graduate to React?") must be clearly written or developers make inconsistent choices
- Fit: 5 — React already in repo (editor); Alpine panels are asset-only; no new infrastructure for either tier
- Custom: 5 — both tiers use Tailwind + CSS custom properties; a single game theme file propagates to all panels regardless of tier
- Perf: 4 — Alpine panels are near-zero cost; React panels pay virtual DOM overhead only where complexity justifies it

**C1: React + Vite + shadcn/ui**
- Engine: 4 — strong reusable conventions; single framework shared with editor
- Game: 5 — hooks, context, and component composition handle all sim-game panel complexity; shadcn provides datatables, charts, dialogs out of the box
- Cost: 3 — every panel needs a Vite build step; proven pipeline in repo but friction for quick debug panels
- Risk: 5 — most well-understood option; best LLM training data by far; already proven in CluicheEditor
- Fit: 4 — aligns with existing editor stack; PD-006 compliant (build output as assets)
- Custom: 5 — Tailwind + shadcn CSS variables; per-game theming is a single token file
- Perf: 3 — React runtime ~45 KB + bundle; virtual DOM overhead; acceptable but not lightweight

**C4: Preact + HTM**
- Engine: 3 — React-compatible mental model; component conventions transferable to React if needed
- Game: 5 — hooks and `useContext` handle multi-panel shared state cleanly; no build step; 3 KB runtime
- Cost: 4 — single HTML file with CDN import map; no npm; slightly more initial setup than Alpine
- Risk: 3 — HTM template literals occasionally trip LLMs; JSX vs HTM syntax is a known pitfall; manageable with a convention note in prompts
- Fit: 4 — asset-only delivery; no VS project changes; no conflict with editor stack
- Custom: 4 — Tailwind CDN + CSS custom properties; DaisyUI can be loaded alongside; no CSS modules without a build step
- Perf: 5 — Preact 3 KB + HTM <1 KB; near-zero framework overhead

**C2: Alpine + Tailwind + DaisyUI**
- Engine: 3 — lightweight convention for single-purpose panels
- Game: 3 — excellent for simple overlays; **genuine liability for sim-game panels** — shared selection state across panels requires `Alpine.store()` workarounds that degrade quickly and LLMs generate unreliably
- Cost: 5 — single HTML file, CDN tags, no toolchain; fastest possible start
- Risk: 2 — the shared state limitation is a real, expected failure mode for sim games; not a theoretical risk
- Fit: 4 — zero VS project changes; asset-only
- Custom: 5 — DaisyUI `data-theme` is the best theming story of any candidate; 30+ themes, one attribute
- Perf: 5 — 15 KB runtime; near-zero memory and CPU cost

**C5: Vanilla JS + Tailwind CDN**
- Engine: 2 — formalises existing pattern only; no new engine capability
- Game: 2 — no reactivity model; sim-game panels with live cross-panel state need hand-rolled DOM manipulation that becomes unmaintainable fast; AI-generated vanilla JS degrades badly at this complexity level
- Cost: 5 — zero new dependencies; existing panels already use this
- Risk: 4 — maximally understood; risk is known inadequacy for the use case, not uncertainty
- Fit: 5 — perfectly compliant with all PD decisions; zero new dependencies
- Custom: 4 — Tailwind CDN + CSS variables; works but manual discipline required
- Perf: 5 — zero framework overhead

**C6: Lit + Shoelace**
- Engine: 3 — Web Components are portable and future-proof
- Game: 3 — component model handles state better than Alpine/Vanilla; Shoelace widget breadth is good; LLM familiarity is the weakest of the group
- Cost: 3 — no build step but Lit syntax ramp; Shoelace token names less familiar to LLMs
- Risk: 3 — Shoelace recently rebranded to Web Awesome; API churn risk; sparser LLM training data
- Fit: 4 — asset-only delivery; no VS project changes
- Custom: 4 — CSS custom properties throughout Shoelace; clean but requires learning their token system
- Perf: 4 — Lit 6 KB; Shoelace lazy-loaded; solid but heavier than Preact/Alpine

**C3: Vue 3 + Vite + PrimeVue**
- Engine: 3 — full-featured but introduces Vue alongside React; framework fragmentation
- Game: 4 — PrimeVue widget catalog handles sim complexity well; good state management (Pinia)
- Cost: 3 — Vite pipeline same as React but Vue is new to the repo; learning ramp
- Risk: 3 — solid technology but second framework adds ecosystem split; LLM support strong but secondary to React
- Fit: 3 — no existing Vue in repo; fragmentation risk when React already covers the same ground
- Custom: 3 — PrimeVue design token system is heavier to override than Tailwind; less flexible per-game skinning
- Perf: 3 — Vue 3 ~45 KB + PrimeVue; comparable to React tier

---

## Top 3 Candidates

### Rank 1: Tiered (React + Alpine) — game-only (score: 4.20)
**Why:** The only candidate that honestly covers the full range of sim-game panel complexity without compromise. Simple overlays (FPS, log, mini-map) are Alpine single-file panels with zero build cost and near-zero runtime. Complex sim panels (entity inspector, production chain, pathfinding debug) use React with full component composition and shared state. Both tiers share a Tailwind + CSS token theming system so per-game skinning is applied once and propagates everywhere. React is already in the repo — this adds Alpine as a lightweight asset-only second tier, not a second build system.
**Watch out for:** The boundary rule must be written clearly — "if a panel needs state shared with another panel, use React; otherwise Alpine is fine." Without that, developers make inconsistent choices.

### Rank 2: React + Vite + shadcn/ui (score: 4.15)
**Why:** The simplest single-framework answer. No boundary decisions, no cognitive overhead — every panel is React, every developer uses the same mental model, LLM generation is maximally reliable. shadcn's component catalog covers all sim-game UI needs. Extremely close to Rank 1 — the gap is purely the cost of a build step on every panel, including simple ones where Alpine would have been instant.
**Watch out for:** Every panel — even a 10-line FPS counter — needs a Vite build step. That friction adds up when iterating quickly on debug tooling.

### Rank 3: Preact + HTM (score: 4.10)
**Why:** React-compatible shared state and component composition with a 3 KB runtime and no build step. The best of both worlds on paper — but the HTM syntax pitfall is a real daily friction for AI-generated code. A strong choice if the build step of React is genuinely unacceptable for game panels.
**Watch out for:** Always include "use HTM syntax not JSX" in any prompt to Claude. Without it, generated code silently fails in HTM.

## Recommendation

**Tiered (React + Alpine) — game-only** is the recommendation. The sim-game target makes a single lightweight framework (Alpine, Preact) insufficient for complex panels, but a full React-only pipeline is disproportionate for the simple overlays that make up the majority of game debug UI. The tiered convention captures the best of both: Alpine handles the fast, cheap, AI-generatable panels that don't need cross-panel state; React handles the sim-complexity panels that do. React is already proven in the repo (editor stack), and Alpine panels are pure assets — no new build infrastructure is required for the simple tier. Per-game skinning is solved once at the Tailwind design token layer and applies to both tiers automatically, satisfying the customizability requirement cleanly.

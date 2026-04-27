# Research: Choice — Webix Removal

**Date:** 2026-04-26
**Chosen candidate:** Tiered (React + Alpine) — game-only

## Rationale

The sim-game target requires both ends of the complexity spectrum: simple self-contained overlays (FPS counter, log stream, variable watcher) and complex multi-panel debug UIs where a single entity selection drives multiple panels simultaneously. No single lightweight framework covers both well. The tiered approach assigns Alpine to simple panels (zero build cost, zero runtime overhead, AI-generatable in one prompt) and React to complex panels (full component composition, hooks, shared state via context). React is already proven in the repo via CluicheEditor, so no new build infrastructure is required for the React tier. Both tiers share a Tailwind + CSS custom property theming system, satisfying the per-game skinning requirement with a single theme file.

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|------------------|
| C1: React + Vite + shadcn/ui | Build step required even for trivial panels; iteration speed on debug tooling suffers |
| C4: Preact + HTM | HTM syntax pitfall with LLMs; no pre-built component library; would become a third framework once React is also available |
| C2: Alpine + Tailwind + DaisyUI | Shared state across panels is a genuine liability for sim games; wrong ceiling for the target genre |
| C5: Vanilla JS + Tailwind CDN | No reactivity model; sim-game panels become unmaintainable quickly |
| C3: Vue 3 + Vite + PrimeVue | Introduces Vue alongside React; framework fragmentation with no advantage over React |
| C6: Lit + Shoelace | Weakest LLM familiarity; Shoelace rebranding adds churn risk |

## Pre-Spec Commitments

1. **Boundary rule:** "If a panel needs to read or react to state owned by another panel, use React. If it is self-contained, use Alpine." This exact rule must appear in the spec and in any panel authoring guide.

2. **DaisyUI is mandated for all Alpine panels.** DaisyUI's `data-theme` attribute is the per-game skinning mechanism for the Alpine tier — making it optional breaks the theming guarantee. All Alpine panels must load DaisyUI.

3. **The upgrade path is Alpine → React, nothing in between.** Preact + HTM is not introduced as a middle tier. If a panel outgrows Alpine, it is ported to React.

4. **React game panels use a separate Vite project from the CluicheEditor build.** Game panels deploy alongside the game binary; the editor has its own release cycle and output directory. The two build pipelines must remain independent.

## Next Step

Run /spec-system or /spec-feature with this candidate as input.
Suggested parent system: DiaUI (game-facing UI convention) or a new DiaUIGame system spec.

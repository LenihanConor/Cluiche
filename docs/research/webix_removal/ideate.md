# Research: Ideate — Webix Removal

**Input:** docs/research/webix_removal/explore.md

## Candidates

### Candidate 1: React + Vite + shadcn/ui (Unified Build Pipeline)
**Home module/system:** DiaUICEF / DiaEditor (extends existing CluicheEditor pattern)
**Size:** M (1–3 weeks to adopt as canonical standard and port/create initial panels)
**Description:** Standardise on React 18 + TypeScript + Vite across all UI pages — editor panels, debug tools, and any game-facing HUD. shadcn/ui provides copy-paste component primitives (datatables, forms, dialogs, charts via recharts) that live in your repo rather than as a runtime dependency, making them fully AI-editable. This extends the pattern already established in CluicheEditor's React/TypeScript UI layer, so the build pipeline and output conventions are already proven.

The main cost is that every UI page needs an `npm run build` step before it can be loaded by CEF. This is manageable with a single Vite project that compiles multiple pages (via Vite's multi-page mode), or separate sub-projects per major feature area. Hot-module replacement doesn't work inside CEF, so dev workflow requires a build-then-reload cycle.

**Primary value:** Single framework across all Cluiche UI — AI generates consistent, idiomatic React for everything, and developers have one mental model.

---

### Candidate 2: Alpine.js + Tailwind CDN + DaisyUI (No-Build Panel Convention)
**Home module/system:** DiaUICEF / standalone HTML convention (no new Dia module needed)
**Size:** S (≤1 week to define convention and create template; each panel is a self-contained .html file)
**Description:** Alpine.js (15 KB) delivers directive-based reactivity directly in HTML markup — no build step, no npm, no toolchain. A panel is a single `.html` file that loads Alpine and Tailwind via CDN `<script>` tags and is placed in the binary output directory for CEF to load. DaisyUI adds 50+ styled Tailwind component classes (buttons, tables, modals, badges) without requiring JavaScript, keeping the panel lightweight and AI-generatable. The C++ bridge calls `RegisterJSHandler` / `CallJSFunction` as today.

This approach is the fastest path to an AI-authored UI: Claude can generate a complete working debug panel in one prompt with no scaffolding, no build, and no dependencies to install. The weakness is that Alpine becomes painful for complex stateful UIs with many cross-cutting concerns.

**Primary value:** Any debug or tool panel can be created by AI in a single pass with zero build toolchain — ship a new panel in minutes.

---

### Candidate 3: Vue 3 + Vite + PrimeVue (Full-Featured Widget Alternative)
**Home module/system:** DiaUICEF / DiaEditor
**Size:** M (1–3 weeks; new framework choice, no existing Vue in repo)
**Description:** Vue 3 with Vite and the PrimeVue component library offers the richest out-of-the-box widget set of any MIT-licensed framework: DataTable with sorting/filtering/virtual scrolling, TreeView, Chart, PropertyAccordion, Terminal, and more. This directly replaces Webix's widget catalog. Vue's Options API is arguably simpler for LLMs to generate correctly on first attempt than React's hook model. Vite build pipeline is identical to the React path.

The downside is that Vue introduces a second framework alongside React (already used in CluicheEditor), creating a split ecosystem. LLM support is strong but clearly second to React in training data volume.

**Primary value:** Drop-in replacement for Webix's widget library; best pre-built component catalog of any free framework.

---

### Candidate 4: Preact + HTM (React-Compatible, No Build Step)
**Home module/system:** DiaUICEF (any page loaded by CEF)
**Size:** S–M (1 week for adoption; each panel is still a .html file but with React-compatible component model)
**Description:** Preact is a 3 KB React-compatible library. HTM (Hyperscript Tagged Markup) replaces JSX with ES6 tagged template literals — meaning you write React-style components in a plain `.html` or `.js` file with no transpiler or build step. Load Preact + HTM via a CDN import map and write components that look almost identical to React functional components. LLMs that know React can generate valid Preact/HTM with minimal adjustment.

This is the "React without the build step" approach: you get hooks, component composition, and the React mental model in files that CEF can load directly. The tradeoff is that the no-build import-map pattern is less ergonomic for large apps, and the HTM template literal syntax is slightly unfamiliar.

**Primary value:** React-compatible component model with zero build toolchain — best choice if AI-authoring React patterns is the priority but build steps are unwanted.

---

### Candidate 5: Vanilla JS + Tailwind CDN (Formalise the Existing Convention)
**Home module/system:** DiaUICEF (all panels); formalises what outputconsole, home, and gameconnection already do
**Size:** S (≤1 week to write the convention doc, create a starter template, and remove Webix from External/)
**Description:** The existing editor panels (outputconsole, home, gameconnection) are already plain HTML + CSS + vanilla JS, loaded directly by CEF with no framework. This candidate makes that the official, documented convention: define a standard HTML template, a CSS design token file (Tailwind via CDN for utility classes), and a JS bridge pattern for C++ calls. Remove Webix from `External/`, delete the legacy deps from `deps.json`, and document the convention.

LLMs generate vanilla JS trivially. The convention is understandable to anyone who can read HTML. The downside is no reactivity primitives — complex panels with dynamic lists or live data binding need custom DOM manipulation code that degrades quickly in quality as complexity grows.

**Primary value:** Zero new dependencies; formalises and cleans up what already exists; lowest risk path to removing Webix.

---

### Candidate 6: Lit + Shoelace (Web Components, No Build)
**Home module/system:** DiaUICEF / standalone HTML convention
**Size:** M (1–2 weeks; Lit components can be authored without a build step but the pattern is less familiar)
**Description:** Lit is Google's 6 KB Web Components library. Shoelace (now called Web Awesome) is a polished, MIT-licensed component library built on Web Components: buttons, tables, dialogs, trees, select, toast, tabs — equivalent in breadth to Webix's widget set. Both can be loaded via CDN with no build step. Web Components are framework-agnostic, so panels authored this way work in any future renderer (CEF, Ultralight, a standalone browser).

The cost is that Lit's reactive properties and lifecycle (`@property`, `render()`) are moderately familiar to LLMs but less so than React. Shoelace's custom element names (`<sl-button>`, `<sl-data-grid>`) are well-represented in public documentation but less common in LLM training data than React+shadcn equivalents.

**Primary value:** Framework-lock-in-free widget library; direct widget-for-widget replacement for Webix with a richer, maintained, MIT-licensed component set.

---

### Candidate 7: Tiered Convention — React (Editor) + Alpine.js (Tool Panels)
**Home module/system:** DiaEditor (React tier) + DiaUICEF standalone panels (Alpine tier)
**Size:** M (1–3 weeks to document the convention, create templates for both tiers, and establish which panels belong in which tier)
**Description:** Rather than picking one framework for everything, document a formal two-tier convention: complex, stateful editor UIs (CluicheEditor panels, multi-pane layouts, property grids) use React + Vite + shadcn/ui — matching what CluicheEditor already does. Simple, single-concern tool panels and debug overlays (log viewer, variable inspector, performance overlay) use Alpine.js + Tailwind CDN and are single-file HTML — no build step.

The convention document defines the boundary rule (e.g., "if it has more than 3 data bindings or needs cross-panel state, use React"). This approach is the most pragmatic for AI-authoring: simple prompts ("generate an Alpine debug panel") vs. complex prompts ("generate a React editor panel") both have clear, reliable answers. The cost is slightly higher cognitive overhead for developers deciding which tier a new panel belongs to.

**Primary value:** Right tool for the right job — AI generates simple panels instantly with Alpine, complex editors reliably with React, and the convention rule keeps it unambiguous.

---

## Coverage Map

The 7 candidates span all major design axes from explore.md:

| Axis | Covered by |
|------|-----------|
| Build step required | C1 (React/Vite), C3 (Vue/Vite) — Yes; C2 (Alpine), C4 (Preact/HTM), C5 (Vanilla), C6 (Lit) — No; C7 — Both |
| Reactivity model | Component (C1, C3, C4), Directive (C2, C7-simple), Vanilla DOM (C5), Web Components (C6) |
| Dependency weight | Heavy C1/C3, Medium C4/C6/C7, Light C2/C5 |
| LLM familiarity | High C1/C5, High-Medium C2/C3/C4, Medium C6 |
| Widget library | Rich C1 (shadcn), Richest C3 (PrimeVue), Good C6 (Shoelace), Utility-only C2/C5, DIY C4 |
| Size | S: C2, C5; S–M: C4; M: C1, C3, C6, C7 |

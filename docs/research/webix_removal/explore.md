# Research: Explore — Webix Removal

**Session date:** 2026-04-25
**Folder:** docs/research/webix_removal/

## Problem Space Overview

Webix is a commercial/GPL JavaScript UI widget library (datatable, forms, charts, layouts) that exists in `External/Webix/` as versions 2.4.7, 3.1.2, and 5.2.1. The library is effectively dormant — it is not referenced in any C++ source code and was never wired into an active UI backend. The original integration path (Awesomium) was removed, and the modern editor UI has been rebuilt in React/TypeScript. Webix's GPL v3 license also poses a commercial risk if it were ever re-activated.

The broader question is: **what UI framework should be the canonical choice for building tool, editor, debug, and in-engine UI panels in Cluiche going forward?** The DiaUI abstraction layer (`IUISystem`, `Page`, `BoundMethod`) is deliberately backend-agnostic — it loads an HTML file via a URL and bridges JS↔C++ calls. This means the choice of JS framework is orthogonal to the C++ engine and can be swapped without changing engine code.

The constraint from the user is: **free**, **cross-platform capable** (even though Cluiche is Windows-only today per PD-005, the UI web layer runs in a browser context and is inherently portable), and **AI-buildable** (the framework should produce output that LLMs can generate reliably from prompts, since Cluiche's AI-assisted development model means UIs will often be authored or scaffolded by Claude).

## Existing Approaches

- **Webix** — Proprietary widget library; GPL v3 for open source. Very declarative JSON-based API. Heavy, widget-focused. Largely unmaintained community.
- **React + Tailwind** — Component-based with a massive ecosystem. Excellent LLM support (most training data). Requires a build step (Vite/webpack). Already used in CluicheEditor's modern UI layer.
- **Vue 3 + Tailwind** — Similar to React; slightly simpler syntax. Strong LLM support. Requires build step.
- **Svelte / SvelteKit** — Compiled framework with minimal runtime. Excellent DX; growing LLM familiarity. Build step required.
- **Lit (Web Components)** — Google's thin wrapper around native Web Components. No build step needed; very portable. Moderate LLM support.
- **Alpine.js** — Micro-framework that adds reactivity directly to HTML markup. No build step. Works well with Tailwind. Excellent for simple panels. Very LLM-friendly (inline directives readable as plain HTML).
- **Plain HTML + CSS + Vanilla JS** — Zero dependencies; always works. LLMs generate it trivially. No reactivity framework. Already used in some built-in editor panels (outputconsole, home, gameconnection).
- **ImGui (via DearImGui or emscripten)** — Immediate-mode C++ GUI. No browser required. Not web-based; would need a separate rendering path, not via DiaUI's HTML pipeline.
- **Shoelace / shadcn-style web component libraries** — Pre-built accessible components on top of Web Components or React. Good LLM support when combined with React.
- **HTMX** — HTML-over-the-wire; makes HTML elements reactive via server responses. Interesting for debug panels but needs a server. Not a standard fit for embedded game UI.

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| **Rendering pipeline** | CEF (Chromium), Ultralight, ImGui | DiaUICEF and DiaUIUltralight already exist; ImGui would require new C++ plumbing |
| **Build step required** | Yes (React/Vue/Svelte) vs No (Alpine, Lit, Vanilla, HTMX) | No-build options simpler for AI authoring; build options have better tooling |
| **Reactivity model** | Component (React/Vue/Svelte), directive (Alpine), vanilla DOM, immediate (ImGui) | Directive-based (Alpine) is easiest for AI to generate without full project scaffold |
| **Dependency size** | Heavy (React ecosystem ~200 KB+) vs Lightweight (Alpine ~15 KB, Lit ~6 KB) | Matters for Ultralight (smaller memory budget); less for CEF |
| **License** | MIT/Apache (React, Vue, Svelte, Alpine, Lit), GPL (Webix) | All modern frameworks are MIT or Apache — Webix is the odd one out |
| **LLM familiarity** | High (React, Vue, Vanilla), Medium (Svelte, Alpine, Lit), Low (Webix, HTMX) | React is by far the most represented in LLM training data |
| **Component library availability** | Rich (React: shadcn, MUI, Radix), Good (Vue: PrimeVue), Growing (Svelte, Web Components) | Matters for pre-built widgets (datatables, charts, forms) |
| **In-engine debug panel fit** | Simple (Alpine/Vanilla best), Complex editor (React best) | Different UI complexity levels need different answers |

## Known Tradeoffs

- **React power vs. build complexity**: React gives the richest ecosystem and best LLM support, but every page needs a build pipeline (Vite). The CluicheEditor already accepts this cost; simple debug panels do not want it.
- **No-build simplicity vs. raw DOM pain**: Alpine.js and Vanilla JS avoid build steps and load directly from HTML files, but become messy for complex stateful UIs.
- **CEF vs. Ultralight backend**: CEF supports full modern web standards (ES2022, WebGL, etc.) enabling any framework. Ultralight is more constrained (no WebGL, limited CSS) — some heavier frameworks won't work cleanly.
- **Single framework vs. tiered approach**: A single canonical choice simplifies AI authoring conventions; a tiered approach (React for complex editor UIs, Alpine for debug panels) is more pragmatic but adds cognitive overhead.
- **Web Components portability**: Lit/Shoelace components work in any browser renderer; they avoid framework lock-in but sacrifice the reactive DX that LLMs are most familiar with.

## Known Pitfalls (C++ / game engine context)

- CEF's IPC bridge (`CallJSFunction` / `RegisterJSHandler`) imposes a JSON string serialization boundary — framework choice doesn't change this, but overly reactive frameworks can trigger excessive C++↔JS round-trips.
- Ultralight's CSS/JS subset means framework features can silently break; always verify target renderer compatibility before committing.
- React's `useEffect` + StrictMode double-invocation can expose problems with game-engine-side state management if not handled carefully.
- Build artifacts (Vite bundles) must be placed in the correct output directory relative to the binary — currently `Cluiche/bin/Debug/x64/diaapplicationeditor/` — and build step must be part of the developer workflow.
- Hot-reload expectations from web development don't apply inside CEF; UI changes require a full page reload (or a carefully designed HMR proxy).
- LLMs generate plausible-looking but subtly wrong Alpine.js directives for complex two-way binding; always review generated code for `x-model` / `x-bind` misuse.

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| DiaUICEF | Active CEF backend; loads HTML/JS pages; supports any modern JS framework |
| DiaUIUltralight | Lightweight alternative backend; constrained JS/CSS support |
| DiaUI | Platform-agnostic abstraction; Page, IUISystem, BoundMethod |
| DiaEditor | Editor plugin framework; hosts CluicheEditor panels |
| DiaAPI | Command/plugin CLI; could expose endpoints for HTMX-style debug UI |
| DiaLogger | Logging channels; an obvious consumer of a debug panel UI |
| DiaWebSocket | Could serve as a lightweight server for HTMX or live-reload scenarios |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-001 StringCRC | JS framework choice has no direct impact; C++ side of the bridge still uses StringCRC for method IDs |
| PD-004 No STL in public APIs | Applies to C++ bridge code only; JS layer is unaffected |
| PD-005 x64 Windows | Web content runs in CEF/Ultralight (Chromium rendering); JS frameworks are inherently cross-platform in this layer |
| PD-006 VS project files as source of truth | Build artifacts (npm build output) must be excluded from .vcxproj as build inputs; they're assets, not compiled sources |
| PD-007 C++20 required | No direct effect on JS choice; C++ bridge layer already compiles under C++20 |

## Open Questions for Ideation

- Should there be **one canonical framework** for all UI pages (editor panels, debug tools, game HUD) or a **tiered model** (e.g., React for complex editor UIs, Alpine/Vanilla for simple panels)?
- Is the Ultralight backend still actively used, or is CEF the de-facto standard? (If CEF only, more frameworks are viable.)
- Should the new framework ship with a **pre-built component library** (datatables, charts, property editors) to replace the widget functionality Webix offered?
- What is the primary consumer of UI today: the CluicheEditor, debug overlays inside CluicheTest, or both?
- Does the team want a **zero-build-step** convention for simple panels, or is a Vite/npm pipeline acceptable everywhere?
- Should AI-authoring be the primary design criterion (favouring whatever framework Claude generates most reliably), or should developer DX be weighted equally?

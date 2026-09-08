# Research: Ideate — Consistent Plugin UI Styling

**Input:** docs/research/plugin_ui_theme/explore.md

## Candidates

### Candidate 1: Classless CSS Framework (Pico CSS)

**Home module/system:** CluicheEditor (UI assets)
**Size:** S (≤1 week)
**Description:** Adopt Pico CSS — a classless framework that styles semantic HTML elements (`<button>`, `<table>`, `<nav>`, `<article>`) directly. No class names needed. Ship `pico.min.css` as a local asset alongside a small `dia-overrides.css` that sets CSS custom properties for the Cluiche dark palette. Each plugin adds one `<link>` tag and writes plain semantic HTML.
**Primary value:** Plugins get professional styling by writing zero CSS — just use correct HTML elements. Re-theming means editing 10-15 CSS variables in one override file.

### Candidate 2: Custom Design Tokens + Utility Snippets

**Home module/system:** CluicheEditor (UI assets)
**Size:** S (≤1 week)
**Description:** Create a single `dia-theme.css` file defining 30-40 CSS custom properties (colors, spacing scale, typography, border radii) plus 10-15 utility classes for common patterns (`.dia-panel`, `.dia-toolbar`, `.dia-btn`, `.dia-list`). Plugins link this file and use variables + classes. No external dependency — fully hand-authored to match the existing look.
**Primary value:** Tailored exactly to Cluiche's existing aesthetic. Full control with no third-party opinions. Re-theme by changing variables.

### Candidate 3: Shell-Injected Theme (Zero Plugin Configuration)

**Home module/system:** DiaEditor (C++ plugin loader) + CluicheEditor shell
**Size:** M (1–3 weeks)
**Description:** Modify the editor shell's iframe creation code so that when a plugin iframe loads, the shell automatically injects a `<link>` tag pointing to the shared theme CSS. Plugins don't need to reference the theme at all — they get it for free. Combine with either Candidate 1 or 2 for the actual CSS content. Requires a small C++ or JS change in how iframes are initialized.
**Primary value:** Foolproof consistency — impossible for a plugin to forget the theme. New plugins get styling with zero configuration.

### Candidate 4: Web Components Library (dia-* Elements)

**Home module/system:** CluicheEditor (new `components/` JS module)
**Size:** L (1–2 months)
**Description:** Build a library of custom HTML elements: `<dia-button>`, `<dia-panel>`, `<dia-toolbar>`, `<dia-list>`, `<dia-tabs>`, `<dia-badge>`. Each element encapsulates its own Shadow DOM styles but reads theme variables from the host document. Plugins import one JS file and use the custom elements. Styling is locked inside the components — plugins can't accidentally break it.
**Primary value:** Maximum consistency — plugins literally cannot style components incorrectly because styles are encapsulated. Re-theme via CSS variables on `:host`.

### Candidate 5: Tailwind CSS (Utility-First)

**Home module/system:** CluicheEditor (build pipeline addition)
**Size:** M (1–3 weeks)
**Description:** Add Tailwind CSS to the plugin build process. Define a `tailwind.config.js` with the Cluiche color palette, spacing scale, and typography. Plugins use utility classes (`bg-surface-primary`, `text-sm`, `p-4`, `rounded`). Requires adding a build step for plugins (currently they have none).
**Primary value:** Extremely well-documented framework with huge community. Consistent as long as plugins use the defined palette. Easy to learn from examples.

### Candidate 6: MVP.css + Variable Override (Minimal Classless)

**Home module/system:** CluicheEditor (UI assets)
**Size:** S (≤1 week)
**Description:** Similar to Candidate 1 but using MVP.css — an even more minimal classless framework focused on simple app UIs. Override its variables with Cluiche's dark palette. Smaller footprint than Pico, but less component coverage (no accordions, cards, etc.).
**Primary value:** Absolute minimum effort. Works for simple plugins. May not scale to complex UIs like the game connection panel with charts.

### Candidate 7: Style Dictionary Token Pipeline

**Home module/system:** CluicheEditor + DiaCLI (new `dia theme build` command)
**Size:** M (1–3 weeks)
**Description:** Define the design system as a JSON token file (colors, spacing, typography, shadows). Use Style Dictionary (or similar) to compile tokens into CSS custom properties, a reference HTML page, and optionally C++ constants (for native UI if ever needed). `dia theme build` regenerates all outputs. Plugins link the generated CSS.
**Primary value:** Single source of truth in a format anyone can edit (JSON). Generated outputs ensure no drift. The token file is the "easy thing to change."

### Candidate 8: Themed Starter Template + Linting

**Home module/system:** CluicheEditor + DiaCLI (`dia scaffold plugin`)
**Size:** S (≤1 week)
**Description:** Update the `dia scaffold plugin` command to emit a plugin template that already includes the theme `<link>`, correct HTML structure, and example patterns. Add a simple lint check (via `dia check`) that warns if a plugin's `index.html` doesn't include the theme link or uses hardcoded color values. No framework — just the existing hand-written styles extracted into one shared file.
**Primary value:** Guardrails for new plugins + migration path for existing ones. Lint catches drift. Low-tech, low-risk.

## Coverage Map

The candidates span the design axes from explore.md:

- **Delivery:** Linked file (C1, C2, C6, C7, C8), injected (C3), bundled via build (C5), JS import (C4)
- **Abstraction:** Tokens-only (C2, C7), classless framework (C1, C6), utility classes (C5), full components (C4), template+lint (C8)
- **Skill required:** Very low (C1, C3, C6 — just write HTML), low (C2, C8 — copy patterns), medium (C5 — learn utilities), higher (C4 — learn custom elements)
- **Changeability:** Single file (C1, C2, C6, C8), JSON edit + rebuild (C7), variable swap (C4, C5), automatic (C3 inherits from chosen CSS approach)
- **Size:** S (C1, C2, C6, C8), M (C3, C5, C7), L (C4)

Note: Candidate 3 (injection) is an orthogonal delivery mechanism that combines with any of the CSS-content candidates (C1, C2, C6, C7, C8).

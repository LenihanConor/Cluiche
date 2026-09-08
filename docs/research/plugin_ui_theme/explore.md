# Research: Explore — Consistent Plugin UI Styling

**Session date:** 2026-06-03
**Folder:** docs/research/plugin_ui_theme/

## Problem Space Overview

CluicheEditor is a plugin-based editor where each plugin renders as an isolated iframe inside CEF (Chromium Embedded Framework). Currently there are 7 plugins, each with its own inline `<style>` block that hardcodes colors, spacing, and typography. There is no shared stylesheet, no design tokens, and no theming infrastructure.

The result: plugins look *mostly* consistent today (because they copy-paste the same hex codes), but there's no mechanism to keep them aligned as new plugins are added, and changing the look requires editing every plugin individually. The user has limited CSS/design expertise and wants a system that provides consistency by default and allows periodic restyling without touching each plugin.

The editor shell itself is React + Vite + TypeScript with `react-mosaic-component` for docking. Plugins communicate with C++ via `postMessage` but are otherwise fully independent HTML documents.

## Existing Approaches

- **CSS Custom Properties (Design Tokens):** Define `--color-bg-primary`, `--spacing-md`, etc. on `:root`. Plugins link one shared file and use variables instead of hex codes. Re-theming = change one file.
- **Shared Stylesheet / Component Library:** Ship a `.css` file with pre-built classes (`.btn`, `.panel`, `.toolbar`). Similar to lightweight frameworks like Pico CSS, Water.css, or Sakura.
- **CSS Framework (Tailwind, Bootstrap):** Use a utility-class or component-class framework. Heavier, but well-documented and beginner-friendly.
- **Web Components / Custom Elements:** Encapsulate styled components as `<dia-button>`, `<dia-panel>`. Plugins use the elements and get correct styling automatically.
- **Design System Generator:** Tools like Style Dictionary that compile tokens (JSON) into CSS, SCSS, JS variables for multiple platforms.
- **Iframe Theme Injection:** Parent frame injects a `<link>` or `<style>` into each plugin iframe at load time, so plugins don't even need to reference the theme explicitly.

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| **Delivery mechanism** | Linked CSS file, injected by shell, bundled in plugin build | Linked is simplest; injection is most foolproof (plugins can't forget it) |
| **Abstraction level** | Tokens-only (variables) vs tokens + components (classes) vs full framework | Tokens-only is lowest effort but gives least help; components give more guardrails |
| **Authoring skill required** | Raw CSS + variables, utility classes (Tailwind-like), pre-built components (copy-paste HTML patterns) | User is low-skill in CSS — leans toward pre-built patterns |
| **Changeability** | Single file swap, token file re-generation, full rebuild | Single file swap is the gold standard for easy re-theming |
| **Framework dependency** | None (vanilla), lightweight (Pico/MVP.css), medium (Tailwind), heavy (Bootstrap/Material) | Plugins are vanilla JS today; adding a framework has cost |
| **Scope** | Colors only, colors + spacing + typography, full component system | Starting small is safer; can grow over time |

## Known Tradeoffs

- **Tokens-only** gives maximum flexibility but doesn't prevent layout inconsistency — two plugins can use the same colors but look completely different structurally.
- **Pre-built component classes** give more visual consistency but are more work to create and may constrain future plugin designs.
- **Iframe isolation** means CSS doesn't cascade from parent to child automatically — the theme must be explicitly loaded or injected into each iframe.
- **Heavy frameworks** (Bootstrap/Tailwind) add bundle size and learning curve but have excellent documentation for beginners.
- **Classless CSS frameworks** (Pico, Water.css, MVP.css) style semantic HTML tags directly — zero classes needed, but less control over specific layouts.
- **Injection approach** (shell injects theme into iframes) removes the "forgot to include the theme" failure mode but adds coupling between shell and plugin HTML structure.

## Known Pitfalls (C++ / game engine context)

- **CEF version constraints:** CEF ships a specific Chromium version — bleeding-edge CSS features (e.g. `@layer`, `@scope`, container queries) may not be available depending on the CEF version in use.
- **Iframe isolation:** CSS loaded in the parent frame does NOT apply to iframes. Each iframe must load the theme independently.
- **No build tooling for plugins:** Plugins are currently plain HTML files with no bundler/preprocessor. Any solution requiring a build step (Tailwind JIT, SCSS compilation) adds infrastructure.
- **Performance in CEF:** Heavy CSS frameworks can impact render performance in embedded Chromium, especially with many open panels.
- **File serving:** Plugins are loaded from local file paths or a custom scheme (`dia://`). Sharing a CSS file means it must be accessible at a known relative path from every plugin's `index.html`.
- **Offline / no CDN:** Everything must be local — no CDN links for frameworks.

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| DiaEditor | Editor plugin framework — controls how plugins are loaded and iframes are created. Could inject theme at load time. |
| DiaApplicationEditor (CluicheEditor) | The React shell that hosts plugins. Controls the mosaic layout, toolbar, command palette. Already has a dark color scheme. |
| DiaDebugServer | Hosts the WebSocket/HTTP server — if theme is served over HTTP, this is the delivery path. |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-004 No STL in public APIs | Not directly relevant (this is HTML/CSS/JS), but plugin registration APIs in C++ must still use Dia types |
| PD-005 x64 Windows only | CEF version is pinned to Windows x64 — Chromium version is known and stable |
| PD-006 VS project files are source of truth | Theme CSS files need to be included in the vcxproj for proper deployment |
| PD-009 Generated output under Cluiche/out/ | Built/compiled theme output (if any) goes to `out/CluicheEditor/` |

### Current State Summary

| Aspect | Today | Ideal |
|--------|-------|-------|
| Color palette | 8-12 hex codes copy-pasted per plugin | One file defines all colors |
| Typography | `Segoe UI` + `Consolas` hardcoded everywhere | Variables or inherited defaults |
| Spacing | Ad-hoc `14px`, `8px`, `2px` per plugin | Consistent scale (4px grid or similar) |
| Components | Each plugin reinvents buttons, lists, panels | Shared patterns or classes |
| Re-theming | Edit every plugin HTML file | Change one file, everything updates |
| New plugin experience | Copy existing plugin, tweak inline styles | Include one link/import, get correct styles automatically |

## Open Questions for Ideation

- Should the theme be a single CSS file that plugins `<link>` to, or should the editor shell inject it into iframes automatically?
- Is a "classless" CSS approach (style semantic tags directly, no class names needed) viable for the range of plugin UIs needed?
- How much component-level styling (buttons, tabs, panels, lists) is needed vs just tokens (colors, fonts, spacing)?
- Should there be a light theme option, or is dark-only acceptable for now?
- Could a single open-source classless/lightweight framework (Pico CSS, MVP.css) cover 80% of the need?
- What's the mechanism for a user to "re-theme" — edit CSS variables directly, pick from presets, or use a visual tool?

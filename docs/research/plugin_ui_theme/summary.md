# Research Summary — Consistent Plugin UI Styling

**Session folder:** docs/research/plugin_ui_theme/
**Date:** 2026-06-03

## One-Line Answer

Adopt Pico CSS (classless framework) with a dark-palette override file, auto-injected into plugin iframes by the editor shell, so plugins get consistent professional styling by writing plain semantic HTML with zero CSS knowledge.

## Journey

1. **Explored:** CluicheEditor has 7 plugins, each with ~100 lines of inline CSS hardcoding the same dark palette. No shared theme, no tokens, no framework. Plugins are isolated iframes in CEF — theme must be explicitly loaded or injected into each one.
2. **Ideated:** 8 candidates spanning classless frameworks, custom tokens, utility-class systems, web components, and build pipelines. Identified two orthogonal decisions: what CSS to use vs how to deliver it.
3. **Evaluated:** Pico CSS scored highest (4.10) on cost, risk, and skill floor. Tailwind (3.35) and Web Components (3.25) scored lower due to learning curve and build complexity.
4. **Chose:** Pico CSS + shell injection + scaffold template — confirmed by user after discussion of Pico vs Tailwind tradeoffs and a concrete complex-UI example showing where Pico's limits are acceptable.

## Chosen Work Item

**Name:** Pico CSS Plugin Theme System
**Home module:** CluicheEditor (UI assets + shell injection) + DiaEditor (iframe creation) + DiaCLI (scaffold template)
**Suggested spec type:** Feature
**Estimated size:** S–M (Pico + overrides: S; shell injection: small M; migration of 6 plugins: S)

## Key Insights from Exploration

- Plugins are iframes — CSS doesn't cascade from parent. The theme must be injected or linked per-iframe.
- "Classless" means the plugin author writes `<button>` not `<button class="btn btn-primary">` — the framework styles semantic tags directly.
- Pico's variable system (`--pico-primary-background`, etc.) maps cleanly to the existing hardcoded palette, so the visual change can be minimal.
- Complex layouts (grid property panels, multi-section inspectors) will eventually need supplemental CSS on top of Pico — this is expected and acceptable.
- Shell injection makes consistency foolproof: no plugin can accidentally ship without the theme.
- The `dia scaffold plugin` template is the onboarding experience — new plugin authors copy from it and get correct styling with zero effort.

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| Custom tokens + utilities | Requires designing a class system from scratch; higher maintenance burden |
| Tailwind CSS | Build step required, utility class vocabulary to learn, verbose HTML |
| Web Components | Months of work for 7 simple plugins; overkill |
| MVP.css | Too minimal for moderately complex plugins |
| Style Dictionary | Over-engineered JSON→CSS pipeline for a small plugin set |
| Template + lint only | Delivery mechanism without solving the styling content problem |

## References

- docs/research/plugin_ui_theme/explore.md
- docs/research/plugin_ui_theme/ideate.md
- docs/research/plugin_ui_theme/evaluate.md
- docs/research/plugin_ui_theme/choose.md

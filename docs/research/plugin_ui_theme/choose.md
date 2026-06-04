# Research: Choice — Consistent Plugin UI Styling

**Date:** 2026-06-03
**Chosen candidate:** Pico CSS + Shell Injection + Scaffold Template

## Rationale

The user has low CSS/design expertise and wants a system that provides consistency by default with the ability to re-theme periodically. Pico CSS eliminates the need to write CSS classes entirely — plugins just use semantic HTML and look correct. Shell injection removes the possibility of a plugin forgetting to include the theme. The scaffold template ensures new plugins start with the right structure from day one.

This combination won on cost (no build tooling, no design work), risk (well-understood technology, no C++ architecture changes), and skill floor (classless = nothing to learn).

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|------------------|
| Custom tokens + utilities (C2) | Requires designing a token/class system from scratch — more CSS authoring skill needed for maintenance |
| Web Components (C4) | Massive overkill for 7 simple plugins. Months of work for a problem solved in days by Pico. |
| Tailwind CSS (C5) | Requires a build step, learning utility class names, and makes HTML verbose. Higher skill floor. |
| MVP.css (C6) | Too minimal — may not scale to moderately complex plugins like game connection panel |
| Style Dictionary (C7) | Over-engineered for a 7-plugin editor. JSON→CSS pipeline adds tooling burden with minimal benefit. |
| Starter template only (C8) | Doesn't solve the styling content problem — just a delivery mechanism without good CSS behind it |

## Pre-Spec Commitments

- Dark theme only for v1 (no light mode requirement)
- `dia-overrides.css` maps Pico variables to the existing VS Code-ish palette (`#1e1e1e`, `#252526`, `#d4d4d4`, `#0e639c`)
- Shell injection is the primary delivery mechanism; plugins should not need to manually link the theme
- Existing 6 plugins migrated by removing inline styles and ensuring semantic HTML
- `dia scaffold plugin` updated to emit a themed template
- If a future plugin outgrows Pico's capabilities, supplemental custom CSS is acceptable (not a system failure)

## Next Step

Run /spec-feature with this candidate as input.
Suggested parent system: DiaEditor (editor plugin framework) or CluicheEditor application spec

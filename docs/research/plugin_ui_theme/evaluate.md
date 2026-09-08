# Research: Evaluate — Consistent Plugin UI Styling

**Input:** docs/research/plugin_ui_theme/ideate.md

## Scoring Criteria

- **Engine Value (0.25):** Improves Dia module reusability or capability
- **Game Value (0.20):** Improves CluicheTest or editor as a demo/testbed
- **Implementation Cost (0.25):** Inverse of effort — 5 = very cheap, 1 = very expensive
- **Risk (0.15):** Inverse of uncertainty — 5 = well-understood, 1 = highly uncertain
- **Cluiche Fit (0.15):** Aligns with module structure and PD-001 through PD-009

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|-----------|---------------|-------------|-------------|-------------|------------|-------|
| C1: Pico CSS (classless) | 3 | 4 | 5 | 5 | 4 | 4.10 |
| C2: Custom tokens + utilities | 3 | 4 | 4 | 4 | 5 | 3.90 |
| C3: Shell-injected theme | 4 | 4 | 3 | 4 | 4 | 3.75 |
| C4: Web Components library | 5 | 4 | 1 | 2 | 4 | 3.25 |
| C5: Tailwind CSS | 3 | 4 | 3 | 4 | 3 | 3.35 |
| C6: MVP.css (minimal classless) | 2 | 3 | 5 | 4 | 3 | 3.40 |
| C7: Style Dictionary pipeline | 4 | 3 | 2 | 3 | 4 | 3.20 |
| C8: Starter template + lint | 2 | 3 | 4 | 5 | 5 | 3.55 |

## Top 3 Candidates

### Rank 1: Pico CSS (classless) (score: 4.10)
**Why:** Highest cost efficiency — it's a single CSS file download with zero build infrastructure. The classless approach means existing plugins can be migrated by *removing* their inline styles rather than adding new markup. Extremely low skill floor aligns perfectly with the user's stated constraints. Re-theming via CSS variables is straightforward.
**Watch out for:** Complex layouts (entity inspectors, multi-panel UIs) will need supplemental custom CSS. Pico's opinions about spacing/sizing may need overriding to match the compact editor aesthetic.

### Rank 2: Custom tokens + utility classes (score: 3.90)
**Why:** Perfectly tailored to the existing Cluiche look — no compromise with third-party opinions. Gives slightly more layout help than Pico via utility classes. The user owns every line.
**Watch out for:** Requires designing the token set and class library from scratch. Without an existing design system to reference, the initial authoring needs CSS skill (which could be Claude's job, but ongoing maintenance falls to the user).

### Rank 3: Shell-injected theme (score: 3.75)
**Why:** Solves the delivery problem permanently — no plugin can forget the theme. Orthogonal to the CSS content choice; pairs with Rank 1 or 2 to create a foolproof system.
**Watch out for:** Requires a C++ or JS change to iframe creation logic. Adds a coupling between shell and plugin structure. Small but non-zero implementation effort beyond just CSS.

## Recommendation

**Pico CSS (C1) is the clear winner**, beating the field primarily on cost and risk. It requires no build tooling, no design expertise, and no C++ changes. It aligns with the user's constraints: low CSS skill (classless = no classes to learn), easy re-theming (change CSS variables), and instant consistency (all semantic HTML gets styled identically).

The recommended implementation plan combines C1 with elements of C3 and C8:
1. **Pico CSS + dia-overrides.css** as the styling content (C1)
2. **Shell injection** so plugins get the theme automatically (C3)
3. **Scaffold template** so new plugins start with correct structure (C8)

This combination scores well against PD-006 (files tracked in vcxproj), PD-009 (any generated theme output goes to `out/`), and keeps the plugin authoring experience as simple as writing plain HTML.

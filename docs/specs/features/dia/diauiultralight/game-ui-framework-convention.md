# Feature Spec: Game UI Framework Convention

## Status
`Done`

## Traceability

| Level | Spec |
|-------|------|
| Platform | @docs/specs/platform/Cluiche.md |
| Application | @docs/specs/applications/dia.md |
| System | @docs/specs/systems/dia/diauiultralight.md |
| Research | @docs/research/webix_removal/summary.md |

## Problem Statement

Webix (GPL v3, dormant) exists in `External/Webix/` as three versions that are never loaded by any active C++ code. Its GPL license poses a commercial risk and its original integration path (Awesomium) has been removed. There is no documented canonical convention for what JavaScript framework game-facing UI panels should use. This feature removes Webix, establishes a tiered JS framework convention for game UI, and validates that convention against the Ultralight WebKit renderer with a comprehensive widget test suite.

## Summary

Establish a two-tier JS framework convention for all game-facing UI in Cluiche:

- **Alpine tier** — self-contained panels with no cross-panel state (Alpine.js + Tailwind CDN + DaisyUI, single `.html` file, no build step)
- **React tier** — complex panels requiring shared state across multiple panels (React 18 + Vite + shadcn/ui, separate Vite project, separate from CluicheEditor build)

Per-game skinning is handled via a shared Tailwind CSS custom property file applied to both tiers. The upgrade path from Alpine to React is direct — no intermediate frameworks.

## Boundary Rule

> **If a panel needs to read or react to state owned by another panel, use React. If it is self-contained, use Alpine.**

This rule must be cited in any panel authoring guide or AI prompt template.

## Acceptance Criteria

1. `External/Webix/` (all three versions: 2.4.7, 3.1.2, 5.2.1) deleted from the repository
2. Webix entries removed from `deps.json`
3. Boundary rule documented in this spec and in a panel authoring guide
4. Alpine starter template created — single `.html` file, loads Alpine + Tailwind CDN + DaisyUI, includes `window.app` bridge pattern, deployed to game binary output directory
5. React Vite project scaffolded for game panels — separate from CluicheEditor build, output to game binary directory alongside game executable
6. Per-game theming works via a single CSS custom property override file applied to both tiers via DaisyUI `data-theme`
7. Automated bridge contract tests pass: Alpine panel receives C++ data, React panel receives C++ data, C++ callback fires when JS calls `window.app.sendMessage`
8. At least one live example panel exists for each tier, receiving real data from C++ via `window.app` (verified by bridge tests, not visual inspection)

## Bridge Contract Tests

Visual widget rendering is not automatically testable without a rendering harness. The test suite covers the JS↔C++ bridge contract only — these are non-visual, fast, and automatable via the existing Google Test infrastructure with a headless Ultralight instance.

### Required Tests

| # | Test | Tier | What it proves |
|---|------|------|----------------|
| 1 | Alpine panel receives C++ data | Alpine | C++ calls `CallJavaScriptFunction`; Alpine `x-data` value updates correctly |
| 2 | Alpine panel sends data to C++ | Alpine | JS calls `window.app.sendMessage`; registered C++ callback fires with correct payload |
| 3 | React panel receives C++ data | React | C++ calls `CallJavaScriptFunction`; React state updates via `useState` hook |
| 4 | React panel sends data to C++ | React | JS calls `window.app.sendMessage` from a React event handler; C++ callback fires |
| 5 | Cross-panel state (React context) | React | Selection in one component updates state read by a second component via `useContext` |
| 6 | DaisyUI theme switch | Alpine | `data-theme` attribute change does not throw JS errors or break `window.app` bridge |

### Widget Reference Panels (not tested automatically)

The following widget panels are provided as reference implementations for developers and AI authoring. They are not part of the automated test suite — visual correctness is verified by inspection once at implementation time.

**Alpine tier:** stat/badge, progress bar, toast, log stream, minimap, colour swatch, sparkline, dropdown

**React tier:** data table, tree view, modal/dialog, tab panel, chart/graph, property grid, timeline/gantt, accordion

## Tasks

| # | Task | Status | Notes |
|---|------|--------|-------|
| 1 | Add Alpine.js v3, Tailwind CSS, and DaisyUI to `deps.json` and `dia env setup` | Done | Vendored to `External/Alpine/`, `External/Tailwind/`, `External/DaisyUI/` via `single_file` install type. `dia env setup` downloads via `deps_restore.py`. |
| 2 | Delete `External/Webix/` (all 3 versions) and remove from `deps.json` | Done | All three Webix versions removed; `test_deps_json.py` asserts they are absent. |
| 3 | Create Alpine starter template with `window.app` bridge pattern | Done | `Cluiche/CluicheTest/AlpinePanels/template.html` — loads vendored files, wires `window.GameBridge_dispatch` and `window.app` bridge. |
| 4 | Scaffold React Vite project for game panels | Done | `Cluiche/CluicheTest/UI/` — React 18 + TypeScript + Vite, output to `bin/Debug/x64/ui/`, `GameBridge.ts` wires `window.GameBridge_dispatch`. |
| 5 | Create shared CSS custom property theme file for both tiers | Done | `Cluiche/CluicheTest/UI/src/themes/cluichetest.css` — DaisyUI custom property overrides applied via `data-theme="cluichetest"`. |
| 6 | Implement and test all 8 Alpine tier widgets | Done | `AlpinePanels/`: stat-badge, progress-bar, toast, log-stream, minimap, colour-swatch, sparkline, dropdown. Validated by `tests/test_alpine_panels.py` (41 tests: paths, bridge ref, no CDN, vendor scripts). |
| 7 | Implement and test all 8 React tier widgets | Done | `CluicheTest/UI/src/panels/`: DataTable, TreeView, Modal, TabPanel, ChartGraph, PropertyGrid, Timeline, Accordion. Each has a companion `.test.tsx` (44 tests total). |
| 8 | Implement cross-panel state integration test (entity list + detail + resource panels) | Done | `ExamplePanel.tsx` + `ExamplePanel.test.tsx` — `SelectionContext` shared between `EntityList` and `DetailPanel`; 7 tests including cross-panel state re-render. |
| 9 | Implement theming test for both tiers | Done | `Theming.test.tsx` — 6 tests covering `data-theme` attribute, theme switch stability, bridge continuity across theme changes, and CSS custom property presence. |
| 10 | Write panel authoring guide documenting boundary rule, templates, and bridge pattern | Done | `docs/guides/game-ui-authoring.md` — boundary rule, Alpine tier, React tier, bridge patterns, AI prompt templates, theming, upgrade path. |
| 11 | Register feature in DiaUIUltralight system spec features table | Done | Registered in `docs/specs/systems/dia/diauiultralight.md` features table. |

## Data Models / API Shapes

No new C++ types. The existing bridge is used as-is:

```javascript
// Alpine tier — receive data from C++
// C++ calls: page->CallJavaScriptFunction("onDataUpdate", data)
window.onDataUpdate = function(data) { /* update x-data */ };

// React tier — receive data from C++
useEffect(() => {
    window.app.onMessage = function(name, data) {
        if (name === "entityUpdate") setEntities(data);
    };
}, []);

// Both tiers — send data to C++
window.app.sendMessage("entitySelected", { id: entityId });
```

Theme file shape (shared between tiers):
```css
/* themes/<game-name>.css */
[data-theme="my-game"] {
    --p: 259 94% 51%;          /* primary colour */
    --s: 314 100% 47%;         /* secondary colour */
    --b1: 220 13% 10%;         /* base background */
    --bc: 215 20% 91%;         /* base content (text) */
}
```

## Files This Feature Touches

| Path | Change |
|------|--------|
| `External/Webix/` | Deleted entirely |
| `External/Alpine/` | New — Alpine.js v3 vendored via DiaEnv |
| `External/Tailwind/` | New — Tailwind CSS vendored via DiaEnv |
| `External/DaisyUI/` | New — DaisyUI vendored via DiaEnv |
| `deps.json` | Webix entries removed; Alpine, Tailwind, DaisyUI entries added |
| `Cluiche/CluicheTest/UI/` | New directory — Alpine panel `.html` files |
| `Cluiche/CluicheTest/UIReact/` | New directory — React Vite project for game panels |
| `Cluiche/bin/Debug/x64/ui/` | Provisional build output for React panels and Alpine templates — expected to change when deployment strategy is formalised |
| `docs/specs/systems/dia/diauiultralight.md` | Feature registered in features table |
| `docs/guides/game-ui-authoring.md` | New — panel authoring guide with boundary rule and templates |

## Known Open Questions

| # | Question | Status |
|---|----------|--------|
| 1 | Which version of Ultralight is in `External/Ultralight/`? Does it support Alpine.js and React 18 without polyfills? | Open — Task 1 resolves this |
| 2 | Does Ultralight's WebKit support ES module import maps (needed for some React CDN approaches)? If not, Vite bundle is the only option for React. | Open — Task 1 resolves this |
| 3 | Where exactly do Alpine panel `.html` files live relative to the game binary? Is `dia://` scheme used or `file://`? | Open — align with UL-004 (`file://` URL scheme) |

## Binding Decisions Compliance

| Source | ID | Decision | Compliance |
|--------|----|----------|------------|
| Platform | PD-001 | Use StringCRC for all entity/component IDs | Compliant — StringCRC used on C++ side of bridge; JS layer uses plain strings for display only |
| Platform | PD-002 | ProcessingUnit/Phase/Module pattern | Compliant — UI panels are assets loaded by existing `MainUIModule`; no new processing units |
| Platform | PD-003 | Component-based entities | Compliant — this feature is JS/HTML convention only; no entity architecture changes |
| Platform | PD-004 | No STL in public APIs | Compliant — all C++ bridge code uses DiaCore types; JS layer is unaffected |
| Platform | PD-005 | x64 Windows only | Compliant — Ultralight x64 SDK used; JS frameworks run inside Ultralight's WebKit, inherently portable but deployed Windows-only |
| Platform | PD-006 | VS project files as source of truth | Compliant — HTML/JS files are deployed assets, not compiled sources; `.vcxproj` not modified for panel files |
| Platform | PD-007 | C++20 required | Compliant — no new C++ code; JS layer is independent of C++ standard |
| System | UL-001 | CPU renderer only | Compliant — no WebGL or GPU-dependent JS used; all widgets are CSS/DOM based |
| System | UL-002 | Single View per UISystem | Compliant — panels are single-page documents; no multi-view patterns used |
| System | UL-003 | `OnDOMReady` for JS binding | Compliant — Alpine and React both initialise after DOM ready; `window.app` bridge available at that point |
| System | UL-004 | `file://` URL scheme for local assets | Compliant — all panel HTML files loaded via `file://`; CDN links must be replaced with local copies for offline use |
| System | UL-007 | Transparent background by default | Compliant — all panels must use transparent or semi-transparent backgrounds to composite correctly over game scene |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Ultralight compatibility | Does the installed Ultralight version support Alpine.js v3 and React 18 without polyfills? | Resolved — Ultralight 1.3.0 / WebKit 610 (Safari 16.4 era, 2023). Alpine v3 and React 18 are both fully supported. No polyfills needed. |
| 2 | Ultralight compatibility | Does Ultralight's WebKit support ES module import maps? | Resolved — Import maps landed right at WebKit 610; too risky to rely on. React tier must use Vite bundle (not CDN import maps). This is already the chosen approach. |
| 3 | CDN vs local | UL-004 says `file://` scheme — does this mean all CDN `<script>` tags must be replaced with locally vendored copies of Alpine/Tailwind/DaisyUI? | Resolved — yes. Alpine, Tailwind, and DaisyUI vendored to `External/Alpine/`, `External/Tailwind/`, `External/DaisyUI/` via `deps.json` + `dia env setup`. React tier uses Vite bundle (no CDN). |
| 4 | React build output | Where does the React Vite build output go relative to the game binary — same directory, subdirectory? Does it conflict with editor output? | Provisional — `Cluiche/bin/Debug/x64/ui/` for game panels; `diaapplicationeditor/` for editor. Output paths expected to change when deployment strategy is formalised. |
| 5 | Theming | Is a single shared theme file sufficient, or does each game need a full DaisyUI theme config? | Resolved — one `themes/<game-name>.css` per game, containing only CSS custom property overrides of DaisyUI defaults. Loaded via `<link>` tag in every panel. DaisyUI defaults fill anything not overridden. |
| 6 | Widget test harness | Should widget tests be automated (e.g. screenshot comparison) or manual verification? | Resolved — visual widget tests dropped. Automated bridge contract tests only: (1) Alpine panel receives data from C++ via `window.app`, (2) React panel receives data from C++ via `window.app`, (3) C++ callback fires when JS calls `window.app.sendMessage`. No rendering assertions. |
| 7 | Upgrade path | When a panel is ported from Alpine to React, does the Alpine version get deleted or kept? | Resolved — delete the Alpine version. Keeping both creates ambiguity about which is authoritative. |
| 8 | Cross-panel state | The React cross-panel integration test requires 3 panels visible simultaneously — does Ultralight's single View (UL-002) support this, or does each panel need its own View? | Resolved — all panels live in one HTML page as React components. Multi-panel layout is React component composition within a single View. No changes to C++ side required. |

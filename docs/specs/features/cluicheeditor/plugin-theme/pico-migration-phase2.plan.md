# Implementation Plan: Pico Theme Migration Phase 2

**Spec:** @docs/specs/features/cluicheeditor/plugin-theme/plugin-theme-system.md
**Status:** In Progress

## Context

Phase 1 (Done) established the pico theme system: `pico.min.css` + `dia-overrides.css`, CEF injection, and migration of 6 small plugins. Four large standalone editor UIs and the CluicheEditor shell still use hardcoded colors. DiaSceneEditor was migrated as a reference implementation in this session.

## Migration Pattern

Same as Phase 1 (see parent plan's "Plugin Migration Pattern"), adapted for complex panels:

1. Replace all hardcoded colors with `var(--pico-*, fallback)` equivalents
2. Keep layout-only CSS (flex/grid, widths, padding, heights)
3. Keep semantic accent colors for domain icons (layer=teal, camera=purple, etc.) — these are content-specific, not theme colors
4. Expand minified CSS to readable indented format
5. Verify visual parity via `dia run cluicheeditor`

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Migrate `DiaEntityInspector/UI/index.html` — replace ~406 lines of hardcoded CSS with pico vars | `dia run cluicheeditor`; inspector panel renders with correct theme; tabs, fields, watch list styled | Done | sonnet | Purple palette (#13131f) → pico neutral. Domain tag colors (T/P/R/A/S/H/C) and message-type badge colors kept hardcoded (content-semantic). |
| 2 | Migrate `DiaBlueprintEditor/UI/index.html` — replace ~251 lines of hardcoded CSS with pico vars | `dia run cluicheeditor`; blueprint list, accordion, picker, modals styled correctly | Done | sonnet | All hardcoded grays converted to var(). Status bar background now uses --pico-primary. |
| 3 | Migrate `DiaAssetCatalogueEditor/UI/index.html` — replace ~251 lines of hardcoded CSS with pico vars | `dia run cluicheeditor`; asset list, detail panel, toolbar styled correctly | Done | sonnet | Status badge colors (Active/Draft/Deprecated) kept hardcoded. Graph node/edge colors driven by asset type — kept hardcoded in JS TYPE_COLORS map. |
| 4 | Migrate `DiaAssetRuntimeInspector/UI/index.html` — replace ~89 lines of hardcoded CSS with pico vars | `dia run cluicheeditor`; runtime panel renders with theme; connection status dot works | Done | sonnet | Connection dot green kept hardcoded (semantic signal color). |
| 5 | Migrate `CluicheEditor/UI/src/index.html` — replace mosaic theme overrides with pico vars | Editor shell docking frame uses theme colors; window titles, split lines, toolbar styled | Done | haiku | Mosaic selectors preserved; color values switched to var(). Window title color mapped to --pico-ins-color (green). |
| 6 | Migrate `DiaPipelineEditor/UI/src/index.html` — replace hardcoded colors with pico vars | Pipeline editor shell renders with theme background/text | Done | haiku | 3 color declarations replaced with var(). |
| 7 | Visual verification — run editor, open all panels, confirm visual consistency | All editor panels match DiaSceneEditor's new pico-based look; no visual regressions | Not Started | sonnet | Compare against DiaSceneEditor (reference implementation). Check: toolbars, inputs, borders, overlays, selected states, scrollbars. |

## Dependencies

```
[DiaSceneEditor migration] ── already done (reference implementation)
                                    │
    ┌──────────────┬──────────────┬─┴────────────┬──────────────┬──────────┬──────────┐
   [1]            [2]            [3]            [4]            [5]        [6]
 EntityInsp   BlueprintEd   AssetCatEd   RuntimeInsp    Shell     PipelineEd
    └──────────────┴──────────────┴──────────────┴──────────────┴──────────┴──────────┘
                                                    │
                                                   [7] Visual verification
```

Tasks 1-6 are independent — all can run in parallel. Task 7 depends on all others completing.

## Sizing

| File | CSS lines | Total lines | Complexity |
|------|-----------|-------------|------------|
| DiaEntityInspector | ~406 | 998 | High — tabs, tag badges, field tables, watch list, mailbox |
| DiaBlueprintEditor | ~251 | 1011 | Medium — accordion, picker panel, modals |
| DiaAssetCatalogueEditor | ~251 | 1774 | Medium — asset list, filters, detail panel |
| DiaAssetRuntimeInspector | ~89 | 226 | Low — simple panel layout with iframe |
| CluicheEditor shell | ~18 | 31 | Low — mosaic framework overrides only |
| DiaPipelineEditor | ~3 | 16 | Trivial — React shell with 3 color declarations |

## Risks

| Risk | Mitigation |
|------|------------|
| EntityInspector's purple palette (#13131f) was intentionally distinct from neutral gray | Per user direction: align all editors on pico. Purple was pre-theme-system legacy. |
| Mosaic (CluicheEditor shell) uses bp4-dark class selectors that won't respond to pico vars | Map where possible; leave mosaic-specific selectors but use pico var values in them |
| Domain icon colors (teal/purple/gold badges) get washed out if converted to pico vars | Keep these hardcoded — they're content-semantic, not theme colors |
| DiaAssetCatalogueEditor is the largest file; full rewrite risks regression | Mechanical 1:1 color→var replacement, not restructuring. JS/HTML untouched. |

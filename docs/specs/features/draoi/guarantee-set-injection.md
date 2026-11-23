# Feature Spec: DraoiHexWorld Generic Special Sites / POIs (kinds, guarantees and overlay in data; placement injected)

**Parent:** @docs/specs/applications/draoi/draoi.md (step 2 of the Draoi backlog row)
**Status:** `Done` (2026-11-21; full generic system, placement stays an injected hook)
**Origin:** Draoi layer audit, 2026-11-21. Code audit of the hard-coded CoW guarantees.

## Summary
`WorldGenSession`, `WorldOverlayGeometry` and `WorldGenViewHost` name CoW's guarantees and site kinds directly (HearthNexus, DynastySpacing, WorldThrone, DynastyStart). A game that is not CoW cannot reuse the generator without editing them. Direction (user, 2026-11-21): these are **data-driven special sites / points of interest**, not C++ classes. A data file declares the **site kinds** (name, colour, shape, variant label and count), the guarantees on them and the marker overlay; the generator ships three generic guarantee rules that read that data; **placement is injected by the application** (a registered `ISitePlacer` in code, or later a data-described placer). Behaviour-preserving: goldens, JSON `name` values, guarantee order and viewer labels stay identical.

## Goals
- Hearth, Nexus, DynastyStart and WorldThrone are only **names in data** to the generator and viewer: no identifier or include of them in `WorldGenSession`, `WorldOverlayGeometry`, `WorldGenViewHost`.
- Three generic, parameterised guarantee rules replace the three CoW classes (below). CoW's `guarantees.json` instantiates them.
- Site kinds are declared in the same data: a kind needs no C++ declaration to exist, to be listed, drawn in the viewer, saved and loaded.
- Guarantee labels and the marker overlay come from the same data.
- A session with an empty guarantee file validates and generates (Draoi default).

## Non-goals
- Moving files to Draoi (extraction is later; the new rule classes sit in `WorldGeneration/` so the move is a rename).
- Changing `IWorldGuarantee` or the registry plug-in path (`Registry().Guarantees()` still works for exotic C++ guarantees).
- Changing guarantee ids (`CoW.Guarantee.*`), site kind names, or the save format.
- Data-described placement rules. The `ISitePlacer` hook stays the placement seam; CoW registers its dragon-layer placers in code. A data placer can be added later behind the same hook.

## Design
**The three CoW guarantees as generic rules** (all read from data; `kind` = site-kind name, `zone` = zone name from `zones.json`; **Strata:** becomes `stratum` from `strata.json` and `to_zone` becomes `to_stratum` per @docs/specs/features/draoi/generic-strata.md, not yet built):

| Rule type | Meaning | CoW instance (id kept) |
|---|---|---|
| `reach_any` | every site of kind `from` must reach at least one site of kind `to`, or any cell of zone `to_zone`; proof paths recorded | `CoW.Guarantee.HearthNexus`: from `CoW.Site.Hearth` to `CoW.Site.Nexus` |
| `min_spacing` | sites of kind `kind` are pairwise at least `minimum` hexes apart and inside the map; `minimum` is a sidecar parameter | `CoW.Guarantee.DynastySpacing`: kind `CoW.Site.DynastyStart`, minimum 10 |
| `variant_cover` | sites of kind `kind` cover variants `0..variant_count-1` exactly once each, and each reaches any cell of zone `to_zone`; applied only if sites of that kind exist | `CoW.Guarantee.WorldThrone`: kind `CoW.Site.WorldThrone`, variant_count 6, to_zone = last zone |

Diagnostic text and measurement keys move into the rules. To keep goldens byte-identical for the CoW instance, wording that names a CoW concept ("Hearth", "Nexus", "Ancient Interiors", "Apotheosis type") comes from per-rule `noun` fields in the data, and measurement keys (`ThronesOfType`, `AncientInteriorsCells`) are named in data so their CRCs are unchanged.

**Site kinds in data.** A `site_kinds` block lists `{ id, display_name, colour, shape, variant_label, variant_count, protected }, where the optional boolean `protected` (default false) marks a kind whose sites are protected terrain (`TerrainFieldSource::AddProtectedKindsFrom`)`. The loader builds the `SiteKindInfo`s and feeds them to `SiteRegistry` as data-declared kinds (registered with no placer, listed by `ListSiteKinds`, resolvable by `FindInfo`, kind names remembered for save/load). A placer registered from code for a declared id attaches to it; registering a placer for an undeclared id still works exactly as today (plug-in path). Guarantees and the marker overlay refer to kinds by id and fail load if the id is undeclared.

**`guarantees.json`** (new, also holds `site_kinds`; params sidecar-owned like `zones.json` and `terrain_rules`): an ordered list of `{ id, label, type, params... }` plus an optional `marker_overlay { overlay_id, label, kind, spacing_guarantee, fallback_ring }`. Load is all-or-nothing and fails loud naming the entry (unknown rule type, unknown zone, duplicate id, bad param). The session holds the loaded `GuaranteeSpec`; ids absent from the label table keep the `0xXXXXXXXX` fallback.

**Order contract (golden-critical):** spec entries in file order (CoW: HearthNexus, DynastySpacing, WorldThrone-if-sites), then `Registry().Guarantees()`, then `WorldLayout` when regions are fresh. Dedupe by id as today.

**Injection.** `WorldGenSession::SetGuaranteeSpec(const GuaranteeSpec&)`; default empty. The CoW module bootstrap loads `guarantees.json` (catalogue aliases `json.guarantees`, `json.guarantees_schema`) and installs it. Test helpers that default-construct a session install the shipped spec, so existing tests are untouched.

**Marker overlay.** `BuildDynastyStarts` becomes a generic marker builder taking the site kind CRC and spacing evidence; `WorldGenViewHost` reads the marker entry from the spec (overlay id, label, kind, spacing guarantee id, fallback ring) instead of the hard-coded constants.

## Binding Decisions
No binding constraints beyond PD-011..014 (mechanism in the library, content in the application: the rules are mechanism, `guarantees.json` is content). Nothing is added to Dia or Draoi by this feature.

## Acceptance Criteria
1. With the shipped `guarantees.json`, `worldgen_layers.golden.json` and all JSON export goldens are byte-identical; guarantee order, `name` values, diagnostics and measurement keys unchanged. Existing `TestSiteGuarantees` and `TestValidationEdge` assertions pass with only construction changed.
2. An empty spec validates a generated world with only registry guarantees (and WorldLayout) and reports no HearthNexus/DynastySpacing/WorldThrone results.
3. A spec using non-CoW kind names (e.g. `Test.Site.Camp` reaches `Test.Site.Well`, spacing, 3-variant cover) validates a hand-built world, and the viewer shows the configured marker overlay id and label.
4. Load faults (unknown rule type, unknown zone, duplicate id, bad param) fail loud, naming the entry, leaving the prior spec in place.
5. A kind declared only in data (no placer) appears in `ListSiteKinds`, in the viewer sites overlay with its colour/shape, and survives a world JSON save/load round trip; undeclared kinds in a guarantee entry fail load.
6. `WorldGenSession.cpp`, `WorldOverlayGeometry.*` and `WorldGenViewHost.*` no longer include `SiteGuarantees.h` nor contain `Hearth`, `Nexus`, `Throne`, `Dynasty` or `CoW.Guarantee.*`; those strings live only in `guarantees.json` (and test fixtures).
7. Existing CoW suite passes unchanged apart from spec installation in fixtures.

## Tasks
| # | Task | Test | Notes |
|---|------|------|-------|
| 1 | `GuaranteeSpec` type (`site_kinds`, guarantees, marker overlay) + loader + schema + sidecar (`guarantees.json`, catalogue aliases); `SiteRegistry` accepts data-declared kinds | RED-GREEN: load/fault tests (AC 4), data-only kind listed/saved (AC 5) | New files in `WorldGeneration/` |
| 2 | Three generic rules (`reach_any`, `min_spacing`, `variant_cover`) replacing the CoW classes; `DefaultValidationSet` built from the spec | RED-GREEN: non-CoW kinds (AC 3); existing guarantee tests pass with data (AC 1) | Byte-identical diagnostics via data nouns |
| 3 | Session consumes the spec: `SetGuaranteeSpec`, label table, order contract; remove hard-coded names | Empty spec (AC 2), order golden (AC 1) | Fixtures install shipped spec |
| 4 | Generic marker overlay in `WorldOverlayGeometry`/`WorldGenViewHost`; module bootstrap installs the spec | Overlay tests set-driven (AC 3, 5) | UI ids and labels unchanged |
| 5 | Gap audit, edge tests, full suite once | Full CoW suite sharded | No mutation checks |

## Open Design Questions
1. **Data-described placement.** Out of scope here (placers stay code behind `ISitePlacer`). Revisit when a second game needs placement without C++; the dragon-layer placers are the first real consumer and will show what a data placer would have to express.
2. **Rule-type vocabulary.** Three types cover CoW. A second game may need others (e.g. "kind A within N hexes of kind B"); new types are additive. Recommended: ship the three now, they are CoW's existing behaviour.

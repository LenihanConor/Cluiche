# Feature Spec: DraoiHexWorld Guarantee Set Injection (no hard-coded Hearth/Nexus/Throne)

**Parent:** @docs/specs/applications/draoi/draoi.md (step 2 of the Draoi backlog row)
**Status:** `Draft` (2026-11-21; interview answered, awaiting approval)
**Origin:** Draoi layer audit, 2026-11-21. Code audit of the hard-coded CoW guarantees (below).

## Summary
`WorldGenSession`, `WorldOverlayGeometry` and `WorldGenViewHost` name CoW's guarantees and site kinds directly (HearthNexus, DynastySpacing, WorldThrone, DynastyStart). A game that is not CoW cannot reuse the generator without editing them. Introduce an injectable **guarantee set** that the application builds and installs; the generator and viewer read it and name nothing. Behaviour-preserving: goldens, JSON `name` values, guarantee order and viewer labels stay identical.

## Goals
- The session builds its validation set from an injected `WorldGuaranteeSet`, not from inline `HearthNexus/DynastySpacing/WorldThrone` locals.
- Guarantee display names come from the set, not a hard-coded table in `GuaranteeName()`.
- The "Dynasty starts" overlay becomes a configured **marker overlay**: site kind, spacing-guarantee id, label and fallback ring come from the set.
- A session with an empty set validates and generates (Draoi's default).

## Non-goals
- Moving files to Draoi (extraction is later; the set type lives in CoW until then, shaped so the move is a rename).
- Changing `IWorldGuarantee` (no `GetName()`; plug-in guarantees untouched).
- Changing guarantee ids (`CoW.Guarantee.*`), site kind names, or the save format (sites are stored by registry kind name; no guarantee id is persisted).

## Design
**Current state (audit).**
- Inline set built in `WorldGenSession.cpp` ~L2200-2234: `HearthNexusReachabilityGuarantee`, `DynastySpacingGuarantee`, `WorldThroneGuarantee(ZonesOf(mBiomes))`; WorldThrone added only when `SiteKinds::WorldThrone` sites exist; then `Registry().Guarantees()`, then `WorldLayout` when regions are fresh.
- `GuaranteeName()` ~L1972: hard-coded 4-name table feeding JSON `name`, selector matching and `GuaranteeLabel`.
- `WorldOverlayGeometry.cpp` L224: `SiteKinds::DynastyStart()`; `WorldGenViewHost.cpp` L644-648, L911: `CoW.Guarantee.DynastySpacing`, `kDefaultMinimumDynastySpacing`, `MeasurementKeys::MinimumSpacing`, the `dynasty.starts` overlay id/label.
- `DefaultValidationSet` already exists (SiteGuarantees.h) but the session does not use it.

**`WorldGuaranteeSet`** (value type, owned by the application, installed by `SetGuaranteeSet(const WorldGuaranteeSet&)`; session default = the empty set, CoW's module bootstrap installs the CoW set; test helpers that default-construct a session install the CoW set so existing tests are untouched):
- an ordered list of guarantee entries `{ id, label, factory(ValidationBuildContext) -> unique_ptr<IWorldGuarantee> , enabledWhen }`. Factories take the session-owned rules and `ZoneTable` (WorldThrone needs the zones). `enabledWhen` is an optional "only if sites of kind X exist" predicate replacing the WorldThrone `if`.
- one optional **marker overlay** entry `{ overlayId, label, siteKind, spacingGuaranteeId, spacingMeasurementKey, fallbackRing }`.
- the name table is the entries' `{id, label}` pairs; `GuaranteeName()` / `GuaranteeLabel` read it. Unknown ids keep the existing `0xXXXXXXXX` fallback.

**Order contract (golden-critical):** set entries first in declared order (CoW: HearthNexus, DynastySpacing, WorldThrone-if-sites), then `Registry().Guarantees()`, then `WorldLayout` when regions are fresh. Dedupe by id behaves as today.

**`WorldOverlayGeometry::BuildDynastyStarts`** is renamed to a generic marker builder taking the site kind CRC (and spacing evidence) as parameters; **`WorldGenViewHost`** reads the marker entry from the session's set instead of the three constants. `MeasurementKeys::ThronesOfType` stays CoW-side (it is only produced by the WorldThrone guarantee).

## Binding Decisions
No binding constraints beyond PD-011..014 (mechanism in the library, content in the application). Placement of the set type inside CoW until extraction follows the escalation rule: it is not added to Dia or Draoi by this feature.

## Acceptance Criteria
1. With the CoW set installed, `worldgen_layers.golden.json` and all JSON export goldens are byte-identical; guarantee order and `name` values unchanged.
2. A session with the empty set validates a generated world with only registry guarantees (and WorldLayout) and reports no HearthNexus/DynastySpacing/WorldThrone results.
3. A custom set with one non-CoW guarantee and a renamed marker overlay runs and the viewer shows the configured overlay id/label.
4. `WorldGenSession.cpp`, `WorldOverlayGeometry.*` and `WorldGenViewHost.*` no longer include `SiteGuarantees.h` or contain `Hearth`, `Nexus`, `Throne`, `Dynasty` identifiers or the `CoW.Guarantee.*` strings; those live only in the CoW set builder.
5. Existing CoW suite passes unchanged apart from set installation in fixtures.

## Tasks
| # | Task | Test | Notes |
|---|------|------|-------|
| 1 | `WorldGuaranteeSet` type + CoW set builder (moves the three built-ins' wiring out of the session); `SetGuaranteeSet`; session builds validation from the set; `GuaranteeName/Label` from the table | RED-GREEN: empty set (AC 2), CoW set order (AC 1), custom set (AC 3) | Capacity `kMaxValidationGuarantees` checked |
| 2 | Generic marker overlay: `BuildDynastyStarts` generalised, `WorldGenViewHost` reads the marker entry; module bootstrap installs the CoW set | Overlay tests moved to set-driven; AC 3, AC 4 | UI ids/labels unchanged |
| 3 | Gap audit, edge tests, full suite once | Full CoW suite sharded | No mutation checks |

## Open Design Questions
1. **Factory vs instance entries.** Factories are needed because guarantees depend on session-owned rules and zones; if a second game shows a guarantee that needs no context, a plain-instance overload can be added later.
2. **Where the set type lives after extraction.** It is mechanism (Draoi); the CoW builder stays in CoW. Confirm at extraction time.

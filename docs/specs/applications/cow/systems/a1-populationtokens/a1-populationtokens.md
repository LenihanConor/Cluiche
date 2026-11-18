# System Spec: A1 Population Tokens (Layer 2)

**Parent:** @docs/specs/applications/cow/cow.md  
**Extends:** @docs/specs/applications/cow/systems/a1-ecology/a1-ecology.md (Layer 1)  
**Design source:** `Output Docs/GDD/08_SystemsDeepDive/A1_Ecology/` (DesignDocs repo): `A1_EcologySystem.md` §2.2 (Population Token Layer), the migration section ("When prey outflow from a cell exceeds a threshold…"), the event table (Herd migration, Predator wave); `A1_ProductionBacklog.md` A1-29..A1-32  
**Status:** `Approved` (created and approved 2026-11-18, ecology expansion interview; all 6 features Draft, no plan) Amended 2026-11-18 (world-sim review decisions).
**Amended 2026-11-18 (world-sim review, user decisions):** see "World-sim review amendment" below.

**Gameplay Domains:** persistence, debug

## Summary

Layer 2 adds **population tokens** on top of A1's cell layer. There are two kinds:

- **Herd tokens** (persistent, new GDD content). For a species flagged `tokenised` in `species.json`, the herds *are* the population. Each herd has a position, members (juvenile and adult), condition, an origin, a name seed and route memory, so the same herd can be followed through drought, predation, migration and recovery. Herds learn routes: where they were this Season last year pulls them back, so traditional migrations emerge. Dragons will hunt herds, not densities.
- **Front tokens** (transient, as the GDD has them). For species that are not tokenised, a front forms when Layer 1 migration outflow passes a threshold along a long enough connected route, moves with the flow, and dissolves when the flow drops. A front owns no population. It is a named, directed label over cell flow ("the northern deer wave") plus the GDD's trampling effect on the corridor.

Herd tokens reach Layer 1 through a **footprint contract**. Each band, a herd projects its members into a per-cell density for its species over its footprint cells. Layer 1's flow-graph equations then run unchanged (grazing, predation, births, waste, scent), and the resulting deltas are gathered back into the herd. Nothing in Layer 1's equations knows a token exists.

## Problem

Layer 1 makes migration a density gradient. That is right for the analytical model, but nothing persists for the player, D4 or dragons to point at: no herd to name, follow or hunt, and no route that becomes a tradition. The GDD's own token idea (transient fronts) gives direction and naming but not identity. S-4 Dragon needs hunt targets that are things.

## Goals

1. **Identity.** A herd token keeps its id, origin, name seed and history from forming to its end. Events and DiaCellHistory refer to it by id.
2. **Layer 1 unchanged.** Herds couple only through the footprint contract. A1's flow graph, ledger, health index and succession see a tokenised species as ordinary density in its footprint cells.
3. **Conservation.** Members only change through Layer 1's booked causes (births, deaths by cause) and token lifecycle events (split, merge, found, lost), all in the ledger. A test checks that projected density, gathered deltas and token members agree every band.
4. **Learned routes.** A herd's choice of cell is pulled by food, water and its own seasonal memory, and pushed by predator and dragon scent, so repeated migrations emerge from data, not scripts.
5. **The GDD's fronts.** Transient fronts form, move and dissolve on Layer 1 outflow as the GDD describes, for every non-tokenised species.
6. **Deterministic.** Same inputs give bit-identical tokens at any thread count. Draws use a dedicated `A1.tokens` stream in token-id order.
7. **Saved.** Tokens, their memory and the id counter round-trip through E1, and a reload continues bit-identically.
8. **Observable.** Token overlay with trails, a token list and a token detail page, sim-hosted `cow.ecology.token.*` commands, DiaCellHistory events, metrics and health.
9. **Performance.** The Layer 2 Season pass is ≤ 2 ms in Release at Large with 256 herd tokens, on top of A1's 16 ms (reported gate).

## Non-Goals

- **Layer 3** (local instantiation of visible creatures). Deferred. Layer 3 samples tokens and cells.
- **Dragon hunts.** The hunt interface is declared here and built with S-4.
- **Naming and presentation.** D4 turns a name seed and events into a name and a story; V-1 or Layer 3 draws herds for the player. This spec's overlay is debug-only.
- **New ecology equations.** Herds eat, breed and die only through A1's flow graph.
- **Fish runs as tokens.** Runs stay Layer 1 density waves (A1 Seasonal presence). See Open Design Question 3.

## Architecture

### Placement

CoW-Shared, next to A1. Pure logic in `Cluiche/CoW/Shared/Ecology/Tokens/`, test helpers in `Cluiche/CoW/Shared/Ecology/Tokens/Testing/`. Layer 2 runs inside A1's World Step stage `ecology` (no new stage, no new channel), and its state is owned by `EcologySimulation` through a `PopulationTokens` member.

### Data

`species.json` gains an optional block:

```json
"tokenised": {
  "herd_min": 0.02, "herd_max": 0.4,          // members, in map-wide population mass units
  "footprint_radius": 1,                      // hex rings the herd spreads over
  "move_cells": 3,                            // max cells per Season
  "memory_seasons": 8,                        // how many past Seasons of positions it remembers
  "weights": { "food": 1.0, "water": 0.5, "memory": 0.8, "predator": -1.2, "dragon": -2.0, "crowding": -0.5 }
}
```

`ecology_parameters.json` gains the token caps (`max_tokens` 256, `max_tokens_per_species` 64), front thresholds (`front_outflow_threshold`, `front_min_route`, `front_dissolve_seasons`) and `trample_rate`.

### Token state

| Herd token | Front token |
|---|---|
| `id` (monotonic, never reused), `species`, `cell`, `members_juvenile`, `members_adult`, `condition` [0,1], `origin_cell`, `formed_day`, `name_seed`, `state` (`Settled`, `Migrating`, `Fleeing`), `memory` ring (`cell` per past Season, `memory_seasons` long), `seasonal_anchor[4]` (last year's cell per Season) | `id`, `species` or functional group, `kind` (`HerdMigration`, `PredatorWave`; `DroughtFront` and `DiseaseFront` reuse A1's cluster detection), `head_cell`, `direction`, `route` (connected cells), `strength`, `formed_day`, `weak_seasons` |

### Band order inside the `ecology` stage

| When | Step | Effect |
|---|---|---|
| Season, before A1 step 3 | **T1 Project** | For every herd, spread members over its footprint cells, weighted by the species' biome affinity, into the species' Layer 1 juvenile and adult arrays (which hold zero outside footprints) |
| Season and Year, A1 steps 3–7 | Layer 1 runs unchanged | Grazing, predation, births and deaths act on the projected density; the ledger books them per cell |
| After A1 step 7 | **T2 Gather** | For every herd, sum its footprint cells' booked deltas back into members and condition (condition follows the food-satisfaction term), then zero the arrays. Overlapping footprints split each cell's delta by each herd's share of the projected density |
| A1 step 8 (Season) | **T3 Move** | Tokenised species are excluded from Layer 1's migration solver. Each herd scores the cells within `move_cells` with its weights over Layer 1 fields (food reachable through its `eats` edges, `water_gradient`, `predator_scent`, `dragon_scent`, memory and seasonal anchor, crowding by other herds) and moves along the best path. Ties break by direction order 0..5. Herds move against the stable copy, in id order |
| After T3 | **T4 Lifecycle** | Split above `herd_max` (the new herd takes the lower-condition half and a new id); merge two same-species herds sharing a cell if the sum is ≤ `herd_max` (the older id survives); a herd below `herd_min` is lost (members to carrion) unless a same-species herd is adjacent (it joins it); the extinction floor founds a new herd from a lifeboat cell if a tokenised species has none |
| After A1 step 8 | **T5 Fronts** | For each non-tokenised species, connected cells whose outflow passes `front_outflow_threshold` along ≥ `front_min_route` cells form or extend a front; fronts trample their route (`route_memory += trample_rate`, a small producer loss booked as `trample`); a front weak for `front_dissolve_seasons` dissolves |

Seasonal visitors (A1 Seasonal presence) that are also tokenised arrive as herds at the map edge and leave as herds. Their reservoir counts herds' members, not cells.

### Footprint contract

This is the only coupling between the layers, and the place most likely to break, so it is its own feature with its own tests:

- **Projection is exact.** The sum of projected density over a herd's footprint equals its members, to float tolerance.
- **Gather is exact.** The members after gather equal members before + the footprint's booked deltas (births − deaths by cause), so Layer 1's ledger stays the single source of why numbers changed.
- **Layer 1 never stores tokenised density between bands.** Outside T1..T2 the arrays are zero; a save mid-band is impossible (World Step commits whole bands).
- **Predators see herds as prey density**, so a wolf pack (tokenised or not) hunts a deer herd through ordinary `eats` edges.

### Events

`HerdFormed`, `HerdSplit`, `HerdMerged`, `HerdLost{id, cause}`, `HerdMigration{id, from_region, to_region}` (GDD "Herd migration"), `FrontFormed`, `FrontDissolved`, `PredatorWave` (GDD), published at the end of the band and emitted to DiaCellHistory (`kind: event` schema entries) with the token id, so T-1 can draw a herd's life as a track.

### Save/load

`PopulationTokens` serializes as part of A1's save participant: herd and front lists as JSON objects in id order, the next id, and the `A1.tokens` stream position. Floats as `%.17g`. A load validates species ids against the roster.

### Dragon interface (declared, built with S-4)

```cpp
uint32_t GetHerdsInRange(uint32_t cell, uint32_t radius, HerdId* out, uint32_t cap) const;
const HerdToken* GetHerd(HerdId id) const;
void QueueHunt(HerdId id, float adultsKilled, float juvenilesKilled);   // applied at T2, booked as predation by "dragon"
```

### Debug and tools

The commands and their execution model come first; the panel calls the same commands.

- **Commands (sim-hosted, mandatory help):** `cow.ecology.token.list {species?}`, `cow.ecology.token.get {id}` (state, memory, last band's gathered deltas by cause), `cow.ecology.token.spawn {species, cell, members}`, `cow.ecology.token.remove {id}`, `cow.ecology.token.follow {id}` (pins the token in the panel and selects its footprint through `cells.select.*`).
- **Overlay `ecology.tokens`** (draw callback): herd markers sized by members, trails from memory, front arrows along their route; decimates past the geometry budget.
- **Layers:** `ecology.token.footprint` (Category: which herd covers a cell) and `ecology.token.anchor` (seasonal anchor cells for the selected species).
- **Panel card:** token counts by species and kind, cap use, the followed herd's page (members, condition, state, origin, age, route), last band's lifecycle events.
- **Instrumentation:** `DIA_PROFILE_SCOPE` per T step; metrics `cow.ecology.tokens.herds`, `.fronts`, `.lost_per_band`, `.cap_refusals` and `cow.ecology.tokens.pass_ms` with `MetricInfo`; health Degraded when the cap refuses splits for N bands in a row or the conservation check fails.
- **Mockup** before the panel is built (HTML, approved by the user).

## Features

| # | Feature | Description | Spec | Status |
|---|---------|-------------|------|--------|
| 1 | Token Data | `tokenised` block and token parameters in A1's data files; `HerdToken`, `FrontToken`, `PopulationTokens` containers; id allocation; validation. Pure logic, TDD | TBD | Draft |
| 2 | Footprint Contract | T1 projection and T2 gather, overlapping footprints, exclusion of tokenised species from Layer 1 migration, the conservation test against A1's ledger. Pure logic, TDD | TBD | Draft |
| 3 | Herd Movement & Memory | T3 scoring over Layer 1 fields, path within `move_cells`, memory ring, seasonal anchors, `Settled`/`Migrating`/`Fleeing`, tokenised seasonal visitors at edges. Pure logic, TDD | TBD | Draft |
| 4 | Herd Lifecycle | T4 split, merge, lost, extinction-floor founding, caps and refusals, lifecycle events. Pure logic, TDD | TBD | Draft |
| 5 | Front Tokens | T5 formation on outflow, extension, trampling, dissolution; `HerdMigration` and `PredatorWave` events per the GDD. Pure logic, TDD | TBD | Draft |
| 6 | Integration, Save & Tools | Wiring into `EcologySimulation` and the `ecology` World Step stage, save/load, the five commands, overlay, layers, panel card (mockup first), DiaCellHistory events, instrumentation, the dragon interface declaration, the 2 ms gate | TBD | Draft |

**Build order:** 1 → 2 → 3 → 4; 5 after 2; 6 after 4 and 5.

**Proof:** S-3 Ecology, Living mode, gains a "Herds" scenario set (a migration that becomes a tradition over three years, a drought that splits and loses a herd, a wolf wave following a deer herd). S-3 amendment owed when this spec is approved.

## Binding Decisions

| ID | Decision | How it applies |
|----|----------|----------------|
| PD-002 | ProcessingUnit/Phase/Module architecture | Runs on SimPU inside A1's `EcologyModule` and World Step stage; no new module |
| PD-004 | No STL containers in public APIs | Token lists are `DynamicArrayC`; queries fill caller arrays |
| AD-002 | Four-tier dependency model | CoW-Shared, consumed by S-3, S-4, D4, V-1 and Layer 3 |
| AD-003 | GDD owns design intent | Persistent herd tokens are new GDD content (fold-back owed); fronts follow the GDD |
| A1 Goal 1 | Data-only roster and web | Code never names a species; `tokenised` is per-species data |
| A1 Goal 6 | Determinism | Token-id order, stable copy, a dedicated stream |

## Deviations from GDD A1

1. **Persistent herd tokens** for flagged species, owning their population, with learned seasonal routes. The GDD's tokens are all transient. New content for the GDD owner.
2. **Fronts own no population** and are labels plus trampling over Layer 1 flow; this matches the GDD's "dissolves back into cell values" reading for non-tokenised species.
3. **Footprint contract** is Cluiche's mechanism for "tokens modify local cell values as they move".
4. **Disease and drought fronts** reuse A1's existing cluster detection (`EventThresholdCrossed`) rather than becoming new token kinds.

**Folded back 2026-11-18:** DECISION-139 (persistent herd tokens), CHANGE-051; GDD A1 re-locked with user sign-off. GDD backlog A1-29..32 are superseded by this spec, and A1-80 covers herds.

## Dependencies

**Requires:** A1 Ecology Features 1–11 (flow graph, equations, grid passes, ecology state, events, module, save, debug, nutrients, seasonal presence) and S-3 Controlled-mode tuning of Layer 1, so tokens are tuned on a stable base. World Step (the `ecology` stage). DiaCellLayers, DiaCellHistory, DiaValueSchema, `SimQueryHostModule`, `RandomStreamModule`, E1 (all built).

**Required by:** S-4 Dragon (hunt targets), D4 (names and stories), Layer 3 and V-1 (what to draw), T-1 (herd tracks).

## Open Design Questions

1. **Does a footprint average lose too much?** Projection spreads a herd by biome affinity, so a herd on a river edge grazes the dry side as much as the wet side. If S-3 shows herds starving next to food, weight the projection by food available to the species too (a cheap change inside T1).
2. **What happens at the token cap?** At `max_tokens` a split is refused and the herd grows past `herd_max`. That keeps conservation but gives giant herds on big worlds. The alternative is raising `herd_max` per world size; decide after a Large long run.
3. **Should fish runs become tokens later?** A run as one token per river would give "the Greywater run" identity and a better dragon target, but runs fork by flow, which a single token doesn't do. Revisit when S-4 needs river hunting.

## World-sim review amendment (2026-11-18)

User decisions from @docs/research/worldsim_data_driven/summary.md (§0, §0.1), applied through the A1 amendment ([a1-ecology.md](../a1-ecology/a1-ecology.md), "World-sim review amendment"). Each item supersedes any contradicting text in this spec. Bodies are not rewritten. No plan exists; write it at `/implement` against this amendment.

1. **T3 Move scoring uses `ResponseCurve` per term (A12).** The linear `weights` block in Data and the T3 row in "Band order inside the `ecology` stage" (food, water, memory, predator, dragon, crowding) is superseded: each term is a DiaUtilityAI `ResponseCurve` JSON (linear, quadratic, exponential, logistic, sine, step) plus a weight. Curves only, not `UtilitySet`. Supersedes the `weights` shape in Feature 1 and the scoring in Feature 3. Open Design Question 1's "weight the projection by food" stays a T1 change.
2. **Token records go through `JsonDefinitionLoader` plus a sidecar (D6).** The `tokenised` block in `species.json` and any token record files are validated by their sidecars (ranges from DiaValueSchema, not code).
3. **Token parameters go through the DiaAttribute `ParameterFile` (D6, D5).** `max_tokens`, `max_tokens_per_species`, `front_outflow_threshold`, `front_min_route`, `front_dissolve_seasons` and `trample_rate` live in A1's `ecology_parameters.schema.json` (format `cow.ecology_params`), not in `ecology_parameters.json`; that base file no longer exists. Strict load, pack-overridable, set through `cow.ecology.params {action: set}`. Supersedes Data ("`ecology_parameters.json` gains the token caps…") and Feature 1.
4. **Herd perception fields come from `WorldCellFields` (H).** T3 reads food, `water_gradient`, `predator_scent`, `dragon_scent` and crowding through the shared per-cell context (slot `ecology` for A1's committed fields), not by direct array access. Supersedes the T3 row's "over Layer 1 fields" and Feature 3.
5. **Tokens inherit A1's stage and naming.** Layer 2 runs inside A1's `WorldStepStageBase` `ecology` stage (hooks, Failing health, mutation queue; A1 amendment item 2). Token events (`HerdFormed`, `HerdSplit`, `HerdMerged`, `HerdLost`, `HerdMigration`, `FrontFormed`, `FrontDissolved`, `PredatorWave`) are declared in `cow_worldsim.diagamemessages` (supersedes Events, Feature 6). Token save rides A1's kit codec: no in-record version, `GetVersion` + `RegisterMigration`, participant `cow.ecology` (supersedes Save/load). Commands reached through `cow.ecology.params` for parameters (the "five commands" in Feature 6 are recounted at re-plan). Biome affinity uses registry `BiomeIndex` and tags (A1 amendment item 9).

# World-Sim Review: Data-Driven Design and Shared Tooling

**Scope:** S-1 Terrain/World Generation, A4 Weather, A5 Hydrology, A1 Ecology (plus A3 Succession where it touches A1)
**Date:** 2026-11-18
**Question:** How data-driven are the world-sim systems, how much do they share, and what should become data to give more depth and flexibility?

---

## 0. Decisions (user, 2026-11-18)

These supersede the options and recommendations below where they differ.

### Foundations

| # | Decision | Choice |
|---|---|---|
| D1 | Validation policy | **Base files and `set_param` are strict**: any fault fails with the key named and the last good values kept. **Packs are lenient**: unknown keys are dropped with a warning and values are clamped. T-2 flags errors before saving. S-1 moves from clamp to strict when it migrates to ParamSet. |
| D2 | Parameter machinery | **Extend DiaAttribute; no new package** (revised the same day, after comparing it with DiaAttribute):<br>• `AttributeSchema` reads range, default and kind from the DiaValueSchema sidecar, and gains int, bool and enum kinds.<br>• A new parameter-file feature adds strict load with envelope, atomic `ApplyOverrides` for packs, `ToJson` and `Hash`.<br>• `Bind`/`Snapshot` copies resolved values into a C++ struct once per step, for hot loops.<br><br>World parameters gain modifiers (climate cycles and influences as Multiply/Override, gated by DiaCondition), observers (hot reload, UI) and the existing visual debugger. Existing attribute schema JSON and the optional owning entity must keep working. Envelope rules match `dia validate schema`. **AD-001 sign-off given in principle.** S-1 is the proof migration. |
| D3 | Value-range owner | **DiaValueSchema sidecars.** DiaData `AssetTypeDescriptor` keeps identity, required fields and refs, and points at the sidecar for ranges. Amend `diadata/asset-type-framework.md`. |
| D4 | Hex searches | **DiaPathfinding** gets `MultiSourceDistance`, `FloodRegion` and `PriorityFlood` beside `HexTopology`: flat indices, deterministic order, pointer+count APIs (PD-004). **AD-001 sign-off given in principle.** S-1's ~9 copies migrate; A1 and A5 use the helpers from the start. |

| D5 | Where defaults live (from the spec 1 interview) | **The sidecar owns defaults.** There are no separate base parameter files: runtime value = sidecar default ← packs (in order) ← `set` (+ modifiers). "Save as defaults" writes the sidecar's `default` fields in place. A load failure **fails loudly** with no compiled fallback; shipped data is gated by `dia validate schema`. |
| D6 | Parameters vs records | **Parameter** = the schema fixes the key set (scalars, plus maps over a registry) → DiaAttribute `ParameterFile`. **Record** = authors add entries (species, biome rows, rules, flow-graph nodes) → `JsonDefinitionLoader` with sidecar validation. Per-biome tables are **one map per column** (e.g. `hydro.biome.soil_capacity: {Biome.X: v}`). |
| D7 | Cross-field rules | **In the sidecar**, as a top-level `constraints: [{a, op, b}]` checked by C++, the CLI and T-2 (shared fixtures). No C++-only rules. Bands also move into the sidecar, and the old attribute JSON format is deleted. |

Spec 1 is written and **Approved**: [parameter-files.md](../../specs/applications/dia/systems/diaattribute/parameter-files.md) (DiaAttribute Feature 8). The A4, A5 and A1 amendments (spec 6) must drop their base parameter files and descriptor tables accordingly.

### Actions

| # | Decision | Choice |
|---|---|---|
| A1 | S-1 rules not loaded | ~~Wire now, clamp-and-warn.~~ **Superseded:** Parameter Files AC 18 loads `terrain_rules.schema.json`, so the quick fix was dropped. Original choice: The module loads `terrain_rules.json` and `terrain_navigation.json` through catalogue aliases and passes them into `WorldGenSession`. Strict comes with ParamSet. |
| A2a | Biome registry file | **New `Global/World/biomes.json`, owned by S-1 (CoW-Shared).** Holds id, name, palette, tags, generated flag, valid zones, forbidden neighbours, pin and priority, climate envelope. Per-system tables stay in each system's files, keyed by registry id, with coverage checked by `dia validate schema`. Ecology owns seasonal temperature; A4's `baseline_table` becomes a fallback only. |
| A2b | Biome identity | **Roles and tags; the enum becomes a runtime index.** Code asks for roles (`ocean`, `lake`, `volcanic_pin`…) and tags (`land`, `wet`, `cold`, `fuel`, `aquatic`…). A new biome is data only. **This supersedes the 2026-11-13 "one enum, not a StringCRC set" decision** (`a1-ecology.md:81`). It is still one list, but the list now lives in data. |
| A3 | World-sim kit (CoW-Shared) | **All four pieces:** `WorldStepStageBase`, save codec helpers, `ParamCommandHelper`, `CellSelector`. |
| A4 | Naming | **`cow.<sys>` everywhere.** System tokens are worldgen, weather, hydro, ecology, succession. S-1's bare layer ids are renamed, along with contract fixtures and goldens. See §0.1. |
| A5a | Ecology and succession | **All four:** DiaCondition for succession and seasonal-spawn conditions; tags replace biome lists, fuel and named sums; per-pool nutrient rates and the ash target pool as data; an event catalogue, and `flow_graph.json` in reload. |
| A5b | Condition validation | **Dia overload `ConditionExpr::Validate(const IConditionContext&)`.** AD-001 sign-off given in principle. |
| A6 | Hydrology in F-3 and T-2 | **Add** `HydrologyPackParticipant`, the pack kind and a T-2 form route (with the biome table). |
| A7 | Events | **One `cow_worldsim.diagamemessages`** generated with `dia codegen messages`. Migrate S-1's hand registration. |
| A8 | Save versioning | **DiaSaveGame** `GetVersion` and `RegisterMigration`. Drop the in-record version from save blobs. Export files keep the file envelope. |
| A9 | Data I/O | **All four:** catalogue every file and sidecar; resolve paths through `PathStore`; `FileWatcher` reload in dev builds; `JsonDefinitionLoader` for record files (species, biome registry). We are its first production consumer. |
| A10 | Weather depth | **All three:** an influence kind catalogue; the climate cycle as data; westerly band latitudes as parameters shared by S-1 and A4. |
| A11 | Terrain depth | **All three:** stray constants into the rules table; climate scoring per registry entry; **terrain profile table now** (soil multiplier, orographic factor, runoff). |
| A12 | Curves | **DiaUtilityAI `ResponseCurve`** for ecology fit terms and token move scoring. Curves only, not `UtilitySet`. |
| A13 | World summaries for gameplay | **Spec now, reached via ModuleRef.** **Scoped blackboard chain:** agent board → world board (owned by `WorldStepModule`) → game board (owned by the game module). The Dia change (sign-off given in principle): optional parent so `TryGet` falls through, a configurable slot cap (32 today), and optionally value-changed notifications. Off-SimPU readers use a FrameStream snapshot. `GlobalBlackboard` is not used. |
| A14 | Housekeeping | Do all four: A4 command drift, stale `dia-needs.md:22`, a DiaScalarField gap note, and A5 stating `SetHydrologyProvider`. |

### 0.1 Naming convention

| Thing | Pattern | Example |
|---|---|---|
| System token | `worldgen`, `weather`, `hydro`, `ecology`, `succession` | — |
| Commands | `cow.<sys>.<verb>` | `cow.hydro.params set` |
| Param verbs | `params set\|get\|reload\|list` | — |
| Layer ids | `<sys>.<field>` | `hydro.soil_moisture` |
| Schema keys (layers) | `cow.<sys>.layers/<field>` | `cow.hydro.layers/soil_moisture` |
| Schema keys (params) | `cow.<sys>.params/<key>` | `cow.weather.params/snow.melt_threshold` |
| Save ids | `cow.<sys>` | `cow.weather` |
| Formats | `cow.<sys>_params`, `cow.<sys>_state`, `cow.biomes` | — |
| Files | `Global/<System>/<sys>_parameters.json` | — |
| Sidecars | `<same>.schema.json`, next to the file | — |
| Packs | `Global/<System>/Packs/<id>.<sys>.json` | — |
| Events | `<System><Thing>`, declared in `.diagamemessages` | `WeatherUpdated` |
| Unknown biome | Log once, use the registry's `fallback` biome | — |

### 0.2 Spec order

1. DiaAttribute parameter files + sidecar-backed schema (`/spec-feature` under DiaAttribute)
2. Biome registry (CoW-Shared)
3. World-sim kit plus the conventions doc
4. Hex search helpers (DiaPathfinding)
5. Blackboard scopes (DiaBlackboard)
6. Amendments: A1, A3, A4, A5, F-3, T-2, S-1, DiaData, DiaCondition

Action A1 (wire the rules load) and the A14 housekeeping can be done at any time.

---

## 1. Bottom line

- **Values are data everywhere.** Every system puts its tuning constants in a validated JSON file, about 230 constants in all. S-1's X-macro table and A4/A5's descriptor tables are good designs.
- **Content is data in only one place.** That is ecology's food web: species and diet edges in `flow_graph.json`, compiled at load. Everywhere else the *kinds of thing* are code: biomes, zone rules, forbidden neighbours, condition operators, nutrient behaviour, event types. That is where scaling is blocked today.
- **The shared layer is good at the engine level and thin at the game level.** The Dia pieces are built and shared: DiaValueSchema, DiaCellLayers, DiaCellHistory, DiaDeferredStep, CellField and SimQueryHost. Between the CoW systems almost nothing is shared code. The specs share a *pattern*: there are 20+ "mirrors A4" or "same shape as A4" references, so each system will carry its own copy.
- **Dia adoption is uneven (§5b).** Runtime services are reused well: DiaCellLayers, DiaCellHistory, DiaDeferredStep, DiaObservation, DebugServer and jobs. Data and rules packages that would help are unused: **DiaCondition, `.diagamemessages` codegen, DiaValueSchema as the source of parameter ranges, DiaSaveGame versioning, `ResponseCurve`**. About ten other packages were checked and correctly rejected (determinism, per-cell cost, or wrong hex convention). Dia has no hex search helpers, so S-1 has about nine hand-written searches.
- **Now is the cheapest moment to fix it.** Only S-1 is built. A4, A5 and A1 are Approved specs with no code and no started plans. Every duplication below is still on paper.

---

## 2. Status snapshot

| System | Code | Data files on disk |
|---|---|---|
| S-1 Terrain | **Built** (`Cluiche/CoW/Shared/WorldGeneration/`, about 100 files) | `terrain_rules.json`, `terrain_navigation.json`, 3 `.worldshape.json` presets + schema |
| A4 Weather | Specs only (Approved; 14 feature specs, plans not started) | none |
| A5 Hydrology | Specs only (Approved; plans not started) | none |
| A1 Ecology | System spec only (Features 7–9 Draft) | none |
| World Step coordinator | Specs only | none |
| F-3 World Assets | Specs only | none |
| T-1 Inspector / T-2 Data Editor / S-3.5 | Specs only | none |

---

## 3. How data-driven each system is

| | S-1 Terrain | A4 Weather | A5 Hydrology | A1 Ecology |
|---|---|---|---|---|
| **Tuning constants** | ~100 rows, X-macro `TERRAIN_RULES_TABLE` | 41 rows, `ParamDescriptor` | 43 rows, `ParamDescriptor` | `ecology_parameters.json` (no descriptor table specced) |
| **Per-biome table** | Climate centre for **5 of 13** biomes as flat named rules (`kT0_Grass`…) | 13×4 seasonal baseline | 13×3 soil/infiltration/vegetation | `biomes.json` `BiomeProfile` (seasonal temperature again, rainfall, pools, targets) |
| **Content / rules** | Presets, navigation cost table | Presets, influence block | Presets | **Species, food web graph, 17 succession rules, seasonal visitors, tokens** |
| **Hard-coded content** | Biome ids, names, valid zones, 7 forbidden pairs (`BiomeTypes.cpp`); wind band latitudes 30°/60° (`BiomeClimate.cpp:33`); retry/timeout/dynasty spacing outside the table | Field set, passes, 4 seasons | Field set, passes, passability ladder | Node kinds, edge kinds, step lists, N1 order, health formula, rule `source` enum, fire fuel = "grass+shrub+tree", cluster event types, condition operators |
| **Validation policy** | Lenient: clamp + warn, unknown keys ignored | Strict: any fault → whole file falls back to defaults | Strict (same as A4) | Lenient: out-of-range → fallback + warn |
| **Hot reload / packs** | Participant specced | Pack participant | `QueueReload`, **no reload command, no F-3 participant** | Pack + start-state participants; `flow_graph.json` missing from `reload_data` |

**Cost of adding one more…**

| Addition | Cost today |
|---|---|
| Tuning constant (any system) | One table row (good) |
| Species | Data only (good) |
| Succession rule that fits existing fields | Data only |
| New condition field / operator, OR/NOT | Code |
| **Biome** | Enum + 3 switches + 7-pair function + 4 new S-1 rules + S-1 scoring code + rows in 3 JSON files across A1/A4/A5 + validators |
| Nutrient pool with its own behaviour | Code and schema (rates are single global values; ash is pinned to `nutrient.soil`) |
| Weather influence kind, ecology event type | Code |

### Finding: S-1 never loads its rules file

`WorldGenSession.cpp:319` builds from `TerrainRules::Defaults()`. No runtime call to `TerrainRules::LoadFromJson` exists. `terrain_rules.json` is shipped and round-trip tested, but editing it changes nothing in the game. Check whether this is intended (for example, a module load seam that was never wired) before treating it as a bug.

---

## 4. Shared tooling inventory

### Built and genuinely shared (Dia)

| Piece | Gives | Used by |
|---|---|---|
| **DiaValueSchema** | Sidecar `*.schema.json`: type, min/max, default, enum, required, ref, map; `ValueClamper`; ramps; `valueschema.get` | Display layers only (S-1 code; A4/A5/A1/A3 specs), T-1, T-2 |
| `dia validate schema` | CLI data checks against sidecars | T-2 (spec) |
| **DiaCellLayers** | Layer registry, picker, legend, query | S-1 (code), all others (spec) |
| **DiaCellHistory** | Aggregates, rings, events, snapshots | A4, A5, A1, A3, T-1 |
| **DiaDeferredStep** | Async step scheduler, executors, save participant | A4, A5, A1 via World Step |
| **CellField** | GPU hex rendering | S-1, A3 overlays |
| SimQueryHost, `CellLayerGolden.h`, `ISaveable` | Commands, golden tests, saves | All |
| **DiaCondition / DiaRules** | JSON AND/OR/NOT condition trees over a float/bool accessor registry | **Nobody in world-sim** |

### Specced as shared (CoW), not built

| Piece | Gives | Gap |
|---|---|---|
| F-3 `AssetEnvelope` / `OverrideMerge` / `PackFile` | Envelope check, resolve/diff, pack I/O | No `SetParam`, no `ToJson`, no descriptors. **A4 and A5 don't use it** and write their own. **Hydrology is not an F-3 kind**, though A5 refers to "F-3 hydrology packs". |
| `IWorldAssetParticipant` + contract tests | Apply/Capture/IsReady | A5 is missing from the participant list |
| World Step `IWorldStepStage` | Weekly ordering | Interface only, no base class |
| T-2 World Data Editor | Generic table/form editors **generated from DiaValueSchema sidecars** | No hydrology route; sidecars are hand-written |

### The key gap

**DiaValueSchema already holds everything a parameter descriptor holds** (type, range, default, enum). T-2 builds its editors from it. Yet each runtime loader restates the same facts in a C++ table, and F-3 plans a third range source (`IValueRanges`). So ranges live in up to three places per file, and the specs disagree on what an out-of-range value does:

| Who | Out-of-range value |
|---|---|
| S-1 | Clamped |
| F-3 | Clamped |
| A4 / A5 | Whole file rejected |
| T-2 | Flagged but kept |

---

## 5. Duplication hot spots (ranked)

1. **The stage/module skeleton (A4 → A5 → A1).** Active/pending simulations, the revision trio, the 128-entry `MutationQueue`, `SpinUpSlot`, `HoldGeneration`, the IsReady/Capture/Commit discard rule. `weather-module.md:51-73` and `hydrology-module.md:170-263` are nearly word for word, and A1 will be the third copy.
2. **The parameter machinery (four copies, three policies).**
   - S-1: X-macro.
   - A4 and A5: separate `ParamDescriptor` tables, each with its own `ApplyOverrides`, `SetParam`, `ToJson`, params hash and envelope check.
   - A1: unspecified.
   - F-3: a generic merge that nobody calls.
3. **Save codecs.** `weather-save.md:68-111` and `hydrology-save.md:88-143` repeat the same validate checklist, deserialise steps, `-0.0` rule, 17-digit doubles and `{"valid":false}` pending blob. Participant ids are inconsistent: `"weather"`, `"cow.hydrology"`, `"ecology"`.
4. **Debug command plumbing.** S-1 does all-or-nothing rollback inline (`WorldGenerationModule.cpp:531-660`). A4, A5 and A1 each re-spec it. "Set a parameter" has three names:
   - `cow.weather.params {action:set}`
   - `cow.hydro.set_param`
   - `cow.ecology.tuning.set`
5. **Per-biome data and preset selectors.**
   - Per-biome data lives in four places with four keyings and four unknown-id fallbacks (fail / Grassland row / 0.5 / not applicable). Seasonal temperature is authored twice (A4 `baseline_table` and A1 `biomes.json`).
   - A4 and A5 each define a `ResolveSelector` grammar.
6. **Naming conventions.** Layer ids, schema keys and command prefixes:

   | System | Layer ids | Schema keys | Command prefix |
   |---|---|---|---|
   | S-1 | bare | `cow.worldgen.layers/` | `cow.worldgen.*` |
   | A4 | `weather.*` | `weather/` | `cow.weather.*` |
   | A5 | `hydro.*` | `hydro/` | `cow.hydro.*` |
   | A1 | `ecology.*` | `cow.ecology.cell/` | `cow.ecology.*` |

   Sidecar file names `weather_schema.json` and `hydrology_schema.json` won't be found: the loader and T-2 look for `*.schema.json`.

---

## 5b. Dia package audit: are we using what we built?

About 40 Dia packages were checked against the four world-sim systems. For each one: what it gives, whether world-sim uses it (spec or code), and whether it should.

### Already used correctly. Keep.

| Package | Use |
|---|---|
| DiaCellLayers | Debug layer path (S-1 code; A4/A5/A1 specs) |
| DiaCellHistory | History, snapshots, events; A4 drought detection |
| CellField | GPU hex rendering under DiaCellLayers. `dia-needs.md:22` still says "not built", which is stale |
| DiaPathfinding `HexTopology` | The one grid definition (~29 CoW files) |
| DiaDeferredStep, DiaSimTime, DiaThreading, DiaStreams | Step scheduling, budgets, jobs, cross-PU data |
| DiaObservation | Health and metrics (`IHealthReporter`, `MetricInfo`); no reinvention found |
| DiaDebugServer / SimQueryHost | All debug commands |
| DiaAutomation | S-3.5 regression and run modes |
| DiaSaveGame `ISaveable` | A4/A5 save participants (but see version duplication below) |
| DiaAssetCatalogue | `assets.catalogue.json:115-170` tracks S-1's rules, navigation and presets |

### Built but unused, and should be used (Strong fit)

| Package | Use it for | Replaces |
|---|---|---|
| **DiaCondition** (`ConditionExpr`, `IConditionContext`, `ConditionRegistry`) | A3 `succession_rules.json` conditions and A1 `seasonal.spawn.conditions` | Two flat AND-only `{field, op, value}` lists. Gives AND/OR/NOT, shared ops and load errors. Fixes A3's special cases ("any land", "not protected", "unset"). Cost is fine at Year/Decade bands (17 rules × 6.6k cells). |
| **DiaValueSchema sidecars + `ValueClamper`** | The source of every parameter range, default and kind | A4/A5's C++ `ParamDescriptor` tables, S-1's X-macro ranges, F-3's `IValueRanges` adapter. Gets `dia validate schema` and T-2 forms for free. |
| **`dia validate schema`** (envelope + `Packs/` rules in `data_rules.py:16`, `schema_discovery.py:141`) | The one definition of envelope and pack validity | F-3's `AssetEnvelope` / stale-pack rules, which are defined separately |
| **DiaMessageBus `.diagamemessages` + `dia codegen messages`** | One `cow_worldsim.diagamemessages` for `WorldGenerationComplete`, `WeatherUpdated`, `BiomeSuccessionTriggered`… | Hand registration (`WorldGenerationModule.cpp:205-213`, which already has a duplicate-`RegisterType` workaround). Also gets the DiaSchemaBrowser graph. |
| **DiaSaveGame `GetVersion` + `RegisterMigration`** | Save versioning | The in-record `"version":1` checks in `weather-save.md:43,72` and the hydrology equivalent. Export files (`cow.weather_state`) keep the file envelope. |
| **DiaAssetCatalogue registration** | Every new A4/A5/A1/A3 data file | Uncatalogued files (S-1 already does this) |

### Possible fit (use a piece, not the runtime)

| Package | Use | Why only a piece |
|---|---|---|
| DiaUtilityAI `ResponseCurve` | A1 token move weights; ecology fit terms (water, temperature) | Authored non-linear curves for free. `UtilitySet` picks actions, not cells, so use the curves only. |
| DiaData `JsonDefinitionLoader` (built, in DiaAssetCatalogue; DiaData system Approved) | Typed record files: species, biome registry | Replaces hand-written parsers. It has no production callers yet, and no hot reload. |
| DiaFileIO `FileWatcher` | Dev-build auto-trigger of the existing queued `Reload` | Hot reload today is command-only |
| DiaAssetRuntime / `PathStore` | Path resolution | Replaces raw `std::ifstream` (`WorldFiles.cpp:175`). It doesn't decode, layer or reload, so F-3 isn't duplicating it. |
| DiaSerializer | Codec contract (`GetVersion` / `CanMigrate`) | Small gain; the codecs' value is in their field checks |
| DiaBlackboard `GlobalBlackboard` | World summaries for gameplay (season, drought flag, climate phase) for S-4 dragon and NPC AI | Not for per-cell data: no thread safety |
| DiaScalarField | Only A1's scent / `water_gradient` spread-and-decay fields | Needs an odd-q topology adapter; uses hash lookups and a serial `Tick`. A small gather over the shared neighbour table is cheaper. Log the topology gap against DiaScalarField. |

### Checked and correctly not used

| Package | Why not |
|---|---|
| **DiaScalarField for A4/A5 fields** | Wrong hex layout (axial hexagon vs the world's rectangular flat-top odd-q). It looks cells up through an `unordered_map`. Its update only sees `(current, modifier, neighbourSum)`, so it can't do directional transport, upwind or downstream routing. Its `Tick` is serial, which breaks identical-bytes-at-any-range-count. |
| DiaScalarField Inspector / VisualDebugger | DiaCellLayers, DiaCellHistory and T-1 cover them. The overlay only draws axial topology. |
| DiaGeometry2D `Spatial/HexGrid` | **Pointy-top odd-row**: a different convention. Would cause bugs. |
| DiaFlowField | Single goal, outputs direction only, uses hash lookups. Not a router, not multi-source. |
| DiaRules | Actions have no parameters, and the per-rule pass/fail result is debug-only. Succession needs counters per rule. Use `ConditionExpr` directly. |
| DiaAIBudget, DiaBehaviourTree | Wall-clock time slicing breaks determinism |
| DiaEconomy | Linear, real-time income per instance; 6.6k instances is too heavy. `flow_graph.json` already covers the data. |
| DiaAttribute | Too heavy per cell (it fits S-1 band resolution at most) |
| DiaStateMachine | Health states and herd states are trivial enums over arrays |
| DiaObjective, DiaTriggerScript, DiaOrder, DiaHTN, DiaSensor, DiaSteering, DiaEntitySpatial, DiaGridVisibility | Wrong domain: entity, gameplay, planning, latched goals or real-time triggers |

### Gaps where Dia has nothing (shared helper needed)

- **Hex grid searches.** S-1 has about nine hand-written searches over `HexTopology`:
  - multi-source unit-step BFS: `BiomeClimate.cpp:118`, `TerrainLandmass.cpp:217,497`, `TerrainMountains.cpp:210`, `NodesRiver.cpp:61`
  - flood fill: `TerrainLandmass.cpp:410`
  - weighted multi-source Dijkstra: `TerrainDepthZones.cpp:63`, `RegionPartition.cpp:276`, `NavigationQuery.cpp:86`
  - priority flood: `TerrainDrainage.cpp:72`

  A1 needs another for `edge_distance`. A `MultiSourceDistance` / `FloodRegion` / `PriorityFlood` helper beside `HexTopology` would serve all of them. It is generic, so it belongs in DiaPathfinding, which needs AD-001 sign-off.
- **Parameter-set binding.** DiaValueSchema has no C++ struct binding or codegen; `CodeSideCollector.h` covers metrics and commands only. That is the missing piece Option C fills.
- **Two Dia range-metadata sources.** DiaData's `AssetTypeDescriptor` also plans "schema constraints (required fields, value ranges)", which overlaps DiaValueSchema. Settle which one owns value ranges before building `ParamSet`.

---

## 6. What should become data, for depth and flexibility

The rule stays: **values, tables, catalogues and rule lists are data; pass structure and equations are code.** Inside that rule there is a lot of content still in code.

### Cross-system

| Make data | Why it adds depth | Notes |
|---|---|---|
| **Biome registry** (`biomes.json` root): id, display name, `generated` flag, valid depth zones, forbidden neighbours, climate centre/width, palette, tags | A new biome becomes a data change. Zone rules and adjacency become tunable. Every biome gets a climate envelope (today only 5 do). | The enum can stay as a checked mirror while passes switch on it, or move to a runtime index. A4 baseline, A5 biome table and A1 profile key off the registry and are validated for coverage. Seasonal temperature has one owner. |
| **Biome tags** (`wet`, `cold`, `forested`, `fuel`, `aquatic`…) | Rules and passes test tags instead of biome lists. "Any land biome" and fire fuel become tag queries. | Fixes A3's special cases (§7). |
| **Terrain-type modifier table** (extend `terrain_navigation.json` into a terrain profile) | Soil depth by terrain (A5 Open Question 2), orographic strength, runoff | Add once S-2.6 tuning shows biome-only values look wrong |
| **Cross-system constants file** | `glacier.check_week` and `glacier.snow_floor` have one owner instead of being copied A5 → A4 via a struct | Small |

### Terrain (S-1)

- **Wind band latitudes** (30°/60° hard-coded in `WesterlyWeight`) become rules. They also feed A4, so share the value.
- **Placeholder constants** (`kDefaultMaxRetryAttempts`, `kDefaultTimeoutSeconds`, `kDefaultMinimumDynastySpacing`) move into the rules table.
- **Biome climate scoring** becomes general over the registry: each biome optionally carries `{T0, M0, wT, wM}`, not 20 named rules.
- **Pins and fix-up priorities** (water 4, volcanic 3, alpine 2, wetland 1) become registry fields.

### Weather (A4)

- **Climate cycle definition** (period, amplitude, phase) as data, so a world can have a different long-cycle shape. Check `WeatherRunner` when it's built.
- **Influence kinds as a catalogue** (storm, drought, cold snap…). Each kind is a row of pressure/moisture/temperature deltas, falloff and lifetime. A new influence becomes data, not code. Today the `influence` block is global constants.
- **Preset library** already planned. Keep it.

### Hydrology (A5)

- **Passability ladder thresholds** are already parameters. Keep them.
- **Terrain soil multiplier** (above).
- **Ship A5 in F-3 and T-2** so its file gets packs, a form editor and validation like the others. This is a gap rather than new data.

### Ecology (A1 / A3)

| Make data | Why |
|---|---|
| **Succession conditions on DiaCondition** | Gains OR/NOT and a registry of named accessors. A3's "any land biome", "not protected", "`pre_glacial_biome` unset" and elevation-dependent targets stop needing special-case code. The same accessor registry can serve AI rules later. |
| **Per-pool nutrient behaviour** (leach, transport, silt, weathering per pool; ash target pool as data) | A new nutrient becomes truly data-only, as the spec intends |
| **Fire fuel set** as tags/nodes, not "grass+shrub+tree" | New producers can burn |
| **Named aggregates** (`producers_total`) as tag sums | Debug and rules stay correct when nodes are added |
| **Cluster / ecology event types** as a catalogue | New events without code |
| **Health formula weights** already params. Keep the formula in code. | — |
| **`flow_graph.json` hot reload** | Add it to `reload_data` |

### Keep as code

- Pass order and equations: the S-1 stage ladder, wind sweep, A4 passes, A5 routing and topological sort, A1 per-cell equations, the band scheduler.
- Node and edge **kinds** in the food web: these define how the compiler and hot loop work.
- Field sets (structure-of-arrays) and save layouts.
- The 4-season model.

---

## 7. Options

### Option A: converge by convention (lowest cost)

Keep separate implementations and fix the drift.
- One validation policy, one naming convention doc, consistent participant ids.
- Add A5 to F-3/T-2, fix the sidecar names, load `terrain_rules.json`.

**Cost:** about 1–2 days of spec amendments. **Leaves:** four parameter copies, three stage skeletons, three save codecs, and biomes in code.

### Option B: CoW-Shared world-sim kit (recommended core)

Build the shared pieces once in `Cluiche/CoW/Shared/`. No Dia sign-off needed.

| Piece | Replaces |
|---|---|
| `BiomeRegistry` (data) | `BiomeTypes.cpp` switches, S-1 named climate rules, scattered per-biome keying/fallbacks |
| `WorldStepStageBase` (revision/pending/queue/spin-up/hold) | A4/A5/A1 module skeletons |
| `WorldSimSaveCodec` helpers (validate, float arrays, pending blob) | A4/A5/A1 codecs |
| `ParamCommandHelper` (`<prefix>.params set|reload|get`, rollback) | Three command styles, S-1 inline rollback |
| `CellSelector` (one preset selector grammar) | Two `ResolveSelector`s |

A4 builds each piece first as its first consumer. A5 and A1 adopt it.

**Cost:** roughly the same as building A4 alone, plus 2–4 days for extraction and specs. A5 and A1 get cheaper.

### Option C: schema-first parameters in Dia (recommended addition to B)

The audit (§5b) confirms that the pieces exist: sidecars, `ValueClamper`, `dia validate schema` with envelope and pack rules, and T-2 forms generated from sidecars. Only the C++ binding is missing.

Make **DiaValueSchema the single source** for parameter ranges, defaults and kinds. A small Dia `ParamSet` loads a JSON file against its sidecar and provides:
- strict or lenient load
- atomic sparse `ApplyOverrides`
- `SetParam(dottedKey)`
- `ToJson`
- a content hash

C++ binds struct members to schema keys. The binding is generated or written as one line per member, but ranges no longer live in C++. F-3's `OverrideMerge` becomes a thin caller of `ParamSet`, and T-2 already reads the same sidecars.

Rough API, for sign-off:

```cpp
namespace Dia::ValueSchema {
    class ParamSet {                       // one per param file kind
    public:
        bool Bind(StringCRC schemaKey, float* member);           // also int/bool/enum overloads
        bool Load(const Json::Value& root, LoadPolicy, ParamError&);   // envelope + full validate
        bool ApplyOverrides(const Json::Value& sparse, ParamError&);   // atomic
        bool Set(const char* dottedKey, const Json::Value& v, ParamError&);
        Json::Value ToJson() const;
        uint64_t   Hash() const;
    };
}
```

- **Placement:** this is a generic capability any game could use, so under the layer rule it belongs in Dia. **It needs your sign-off (AD-001).** The likely home is DiaValueSchema itself. First settle the overlap with DiaData's `AssetTypeDescriptor` value ranges (§5b).
- **Envelope rules:** take them from `dia validate schema` (`data_rules.py`, `schema_discovery.py`), so the CLI, T-2 and the runtime agree.
- **Cost:** about 3–5 days.
- **Removes:** S-1's X-macro, A4/A5's descriptor tables, F-3's separate range adapter, and three of the four validation policies.

### Option D: adopt the unused Dia packages (recommended, independent of B/C)

These are spec amendments only, with no new engine code:
- **DiaCondition** for succession and seasonal-spawn conditions
- **`.diagamemessages`** codegen for world-sim events
- **DiaSaveGame** versioning and migrations instead of in-record versions
- **Catalogue registration** for every world-sim data file
- **`ResponseCurve`** for authored fit and move curves
- **`FileWatcher`** dev auto-reload

**Cost:** about 2–3 days of spec amendments across A1, A3, A4 and A5. One possible Dia tweak: a `ConditionExpr::Validate` overload that takes an `IConditionContext`, so validation at load time doesn't need stub accessors. That tweak needs sign-off.

### Option E: hex search helpers in DiaPathfinding

`MultiSourceDistance`, `FloodRegion` and `PriorityFlood` beside `HexTopology`. S-1 migrates its ~9 copies; A1 and A5 use the helpers from the start.

**Cost:** 2–3 days, plus the S-1 migration under existing tests. **Needs sign-off (AD-001).**

### Recommendation

Do **B + C + D before A4 Task 1 starts**, along with Option A's naming and drift fixes. Do **E** when convenient. It isn't blocking, but A1 should not add a tenth copy.

---

## 8. Actions

Ordered. **D** = decision for you; **S** = spec work; **C** = code.

| # | Action | Type | Size | Unblocks |
|---|---|---|---|---|
| 1 | Pick the validation policy for param files: strict (A4/A5) or lenient clamp-and-warn (S-1/F-3). Suggest **strict on load and `SetParam`, lenient only for packs** (packs outlive schema changes). | D | — | 4, 5 |
| 2 | Approve or reject Dia `ParamSet` on DiaValueSchema (Option C) | D | — | 5 |
| 3 | Check intent, then wire S-1 to load `terrain_rules.json` at runtime | C | S | Live terrain tuning from file |
| 4 | **Biome registry** spec (CoW-Shared): fields, tags, coverage validation, enum strategy; S-1 migration of `BiomeTypes.cpp` and climate rules | S → C | M | A1/A4/A5 biome tables, A3 tag conditions |
| 5 | `ParamSet` system spec in Dia (`/spec-system`), then migrate S-1 as proof | S → C | M | A4/A5/A1 param features |
| 6 | Amend World Step spec: add `WorldStepStageBase`, save codec helpers, `ParamCommandHelper`, `CellSelector`; amend A4 Features 1/2/4/5/6 to build them and A5 to consume them | S | M | A4 build |
| 7 | World-sim conventions doc: layer ids, schema keys, command verbs, participant ids, sidecar file names, unknown-biome fallback | S | S | All |
| 8 | Amend A1/A3: succession conditions use **DiaCondition** + accessor registry; tags for "any land", fuel and aggregates; per-pool nutrient rates; `flow_graph.json` in reload | S | M | A1 Features 7–9 |
| 9 | Add hydrology to F-3 (pack kind, participant) and T-2 (form route) | S | S | A5 tuning workflow |
| 10 | Weather influence catalogue + climate-cycle data (amend A4 Features 7/9) | S | S | Richer weather variety |
| 11 | Fix spec drift: `cow.weather.reload` and `set_param` still named in `a4-weather.md:308` and `weather-data.md:26`; A5 owes `SetHydrologyProvider` (two-argument, `weather-module.md:80`) | S | XS | — |
| 12 | Terrain profile table (soil multiplier, orographic factor): defer until S-2.6 tuning evidence | S | S | Hydrology realism |
| 13 | Decide who owns value ranges: DiaValueSchema or DiaData `AssetTypeDescriptor` | D | — | 5 |
| 14 | Approve or reject hex search helpers in DiaPathfinding (Option E) | D | — | 19 |
| 15 | One `cow_worldsim.diagamemessages`; migrate S-1's hand registration; amend A4/A5/A1/A3 event sections | S → C | S | Event drift, schema browser |
| 16 | Amend A4/A5 save specs: drop in-record `version`, use `ISaveable::GetVersion` + `RegisterMigration` | S | XS | Save migrations |
| 17 | Register A4/A5/A1/A3 data files in `assets.catalogue.json`; resolve paths through `PathStore` | S | XS | Catalogue, tooling |
| 18 | Amend A1/A3 and tokens to use `ResponseCurve` for fit and move terms; consider `JsonDefinitionLoader` for species and the biome registry | S | S | Authored curves |
| 19 | Hex search helpers in DiaPathfinding; migrate S-1's ~9 searches; A1 `edge_distance` uses them | S → C | M | Less code, one tested search |
| 20 | Fix stale `dia-needs.md:22` (CellField is built) | S | XS | — |
| 21 | Log against DiaScalarField: odd-q rectangular topology, flat-index storage, parallel tick (for future reuse) | S | XS | — |

**Suggested sequence:**
1. Decisions 1, 2, 13 and 14.
2. Quick fixes: actions 3, 7, 11, 16, 17 and 20.
3. Actions 4 and 5 in parallel, with 15 alongside.
4. Action 6.
5. Actions 8 (now including DiaCondition), 9 and 18.
6. Start the A4 build.
7. Action 19 before the A1 build.
8. Actions 10, 12 and 21 alongside tuning.

---

## 9. Sources

- Code: `Cluiche/CoW/Shared/WorldGeneration/` (`TerrainRules.h/.cpp`, `BiomeTypes.cpp`, `BiomeClimate.cpp`, `WorldGenSession.cpp:319`), `Cluiche/Assets/CoW/Global/WorldGen/`
- Specs: `docs/specs/applications/cow/systems/{s1-worldgeneration,a4-weather,a5-hydrology,a1-ecology,a1-populationtokens,a3-biomesuccession,s3-ecology,f3-worldassets,t1-worldsiminspector,t2-worlddataeditor,s35-worldsim,worldstep}/`
- Dia (§5b audit): DiaValueSchema, DiaCondition, DiaRules, DiaUtilityAI, DiaAttribute, DiaEconomy, DiaStateMachine, DiaBlackboard, DiaAIBudget, DiaObjective, DiaOrder, DiaHTN, DiaBehaviourTree, DiaTriggerScript, DiaSensor, DiaScalarField (+Inspector/VisualDebugger), DiaCellLayers, DiaCellHistory, CellField, DiaGridVisibility, DiaFlowField, DiaPathfinding, DiaGeometry2D, DiaEntitySpatial, DiaSteering, DiaAsset, DiaAssetCatalogue, DiaAssetRuntime, DiaJson, DiaSerializer, DiaSaveGame, DiaFileIO, DiaMessageBus, DiaSchemaBrowser, DiaStreams, DiaDeferredStep, DiaSimTime, DiaThreading, DiaObservation, DiaAutomation, DiaDebugServer, DiaCLI `validate` and `codegen`; `docs/specs/applications/dia/systems/diadata/`

## 10. Spec-6 resolutions (2026-11-18, user)

These came up while amending the existing specs. They are recorded in each spec's "Spec-6 resolutions" section.

| # | Point | Choice |
|---|---|---|
| R1 | A record file fails to load | **Fail loudly**, the same as parameters (health Failing, naming the file, record and field). No fallback. |
| R2 | Biomes in saves | **A palette of registry ids plus per-cell indices**, remapped on load through `ResolveOrFallback`. Lives in the kit save codec. |
| R3 | What a pack is | **A sparse document of the parameter format itself**, identified by `Packs/`. Its `basedOn` holds the sidecar schema hash, and a mismatch means stale (load-and-warn). No pack format ids. Bundles gain a hydro part. |
| R4 | Cycles and influences | **They stay per-cell field forcings.** Their definitions become data catalogues: cycles, and influence kinds with `mode: impulse` or `sustained`. Modifiers are used only for scalar parameter effects. |

**Minor rulings** (same principle):
- **Records command:** `cow.<sys>.records {reload|list}`.
- **A5 layer ids:** become field names.
- **Presets:** `cow.<sys>_preset` at `Global/<System>/Presets/<id>.<sys>_preset.json`.
- **Protected cells:** `terrain.is_protected`.
- **Basin kinds:** `hydro.basin_kind` none, lake or dry.
- **Selector limits:** at most 32 selectors and 16 derived fields.
- **Parameter participants:** apply through `ParameterFile::Reload`.
- **A3 glaciation:** two rules (18 in total).
- **F-3 correction:** `terrain_rules.json` is not a kept file.

**Spec 6 is done.** The A1, A3, A4, A5, Population Tokens, F-3, T-2, DiaData, S-2.5, S-3 and S-3.5 specs carry dated amendment sections, and every affected plan is marked "Re-plan required".

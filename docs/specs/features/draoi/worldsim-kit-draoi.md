# Feature Spec: World-Sim Kit, Draoi Half (cell fields, selectors, save codec data half)

**Parent:** @docs/specs/applications/draoi/draoi.md
**Status:** `Approved` (2026-11-21; split approved by the user, recommended answers to the open questions accepted). **Revised 2026-11-23** after the core, terrain and GameSpeed extractions landed: real paths, resolved ODQs, and the `terrain.*` field contribution moved into this half (see "Revision 2026-11-23").
**Sibling:** @docs/specs/applications/cow/systems/worldstep/worldsim-kit.md (the CoW half: stage base, restore sequence, commands, names, wiring)
**Plan:** @docs/specs/features/draoi/worldsim-kit-draoi.plan.md
**Research:** @docs/research/worldsim_data_driven/summary.md (§0: A3, A4, A8)
**Depends on:**
- **Done:** DraoiHexWorld core extracted (2026-11-21: grid and world types, `CellVocabulary.h`, `BiomeRegistry` with open pin-rule ids, `StratumTable`), Strata (@docs/specs/features/draoi/generic-strata.md), generic special sites (@docs/specs/features/draoi/guarantee-set-injection.md), DraoiHexWorldTerrain extracted (2026-11-22, @docs/specs/features/draoi/extract-hexworld-terrain.md), DraoiGameSpeed (2026-11-23). Prerequisites @docs/specs/features/draoi/hexworld-core-prerequisites.md are Done. Nothing blocks this half's tasks.
- DiaPathfinding hex search helpers (`MultiSourceSteps`, `HexNeighbours`, `TreeGraph`), DiaCondition, and the one DiaCondition change below.

## Summary

The kit pieces that carry no CoW content and no scheduling, so any hex-world game gets them. All are **plain libraries in `DraoiHexWorld` core**, plus one registration function in `DraoiHexWorldTerrain` (PD-012: no modules, no bus, no save, command or debug wiring; PD-013: no knowledge of any step chain or processing unit). Three pieces:

1. **`WorldCellFields`**: a per-world registry of per-cell fields, and a cell-cursor `IConditionContext` over it.
2. **`CellSelectorSet`**: cell selectors written in DiaCondition, with derived spatial fields (`distance`, `upstream`).
3. **Save codec, data half**: `SavedField[]`, record header, array writing, Validate, biome-palette write and remap.

Plus the one Dia change both pieces need: `ConditionExpr::Validate(const IConditionContext&)` and the defaulted `IConditionContext::Has`.

## What stays out (CoW half)

`WorldStepStageBase`, `MutationQueue`, `SpinUpSlot`, the 7-step restore, event-order fallback, pending blob, `cow.<sys>` participant ids, `SimCommandSet`, `ParamCommandHelper`, `cow.<sys>.records`, `WorldSimNames`, `worldsim-conventions.md`, the S-1 migration and rename, and all DiaMessageBus wiring. See the CoW half. Draoi code never reads a sidecar file, never touches the bus, and never names SimPU, a processing unit or `StepChain`.

## Design

Namespace `Draoi::HexWorld`, package `DraoiHexWorld` at `Draoi/DraoiHexWorld/`, **flat layout** (the package has no subfolders; headers are included as `<DraoiHexWorld/X.h>`). Test helpers ship in `Draoi/DraoiHexWorld/Testing/` and `Draoi/DraoiHexWorldTerrain/Testing/`; tests live in `Cluiche/Tests/GoogleTests/Draoi/`. New dependency in the module doc: `dia.condition` (`dia.pathfinding` is already listed). No STL in public APIs (PD-004): pointer-and-count lists, `DynamicArrayC`, fixed-size strings.

### 1. `WorldCellFields` (`WorldCellFields.h/.cpp`)

A per-world registry of per-cell fields, keyed `slot.field` (`StringCRC` pairs, the DiaCondition leaf shape).

- **Reads:** each field is a typed reader over a cell index, reading committed state only. Handles are `(slot, field)` CRC pairs resolved at use, not raw pointers (resolved ODQ 2, 2026-11-18).
- **Condition context:** implements `Dia::Condition::IConditionContext` with a **cell cursor** (`SetCell(i)`) and overrides `Has(slot, field)`. A1 succession uses the same context.
- **Registration and rebuild:** `Register(slot, field, reader)`, `Clear()`. Each package contributes its own fields through a plain function that takes the registry (core, `RegisterBiomeFields(registry, const BiomeRegistry&)`: `biome.index`, `biome.tag.<tag>`; terrain package, `RegisterTerrainFields(registry, <generator world>)` in `DraoiHexWorldTerrain`: `terrain.*`, `terrain.is_protected`, `terrain.region`; Weather/Hydrology/Ecology: `weather.*`, `hydro.*`, `ecology.*`). **Who calls `Clear` and rebuilds, and when, is the application's** (CoW does it on `WorldGenerationComplete`); the library has no event knowledge.
- **Threading contract** (thread-neutral): reads touch committed state only. The registry takes an optional "reads are forbidden now" flag pointer; when set and read, it asserts in Debug. It never names `Run` or SimPU.
- **Field vocabulary** (decided in the kit, spec-6 resolutions): `terrain.is_protected` covers corridor sites, `Pass`, `RiverCrossesBarrier`, Hearth, Nexus and world-throne cells and replaces `terrain.is_crossing`; `hydro.basin_kind` takes `none`, `lake` or `dry` (`any` is `!= none`); angular fields are `[0, 2π]` inclusive, normalised into `[0, 2π)` in code. After the generic special sites feature (@docs/specs/features/draoi/guarantee-set-injection.md), "Hearth/Nexus/throne" in `is_protected` are **site kinds named in data** (`guarantees.json`), not C++ identifiers; `is_protected` is built from the data-declared protected kinds plus the two `TerrainFlag` bits.

### 2. Dia change (AD-001 sign-off 2026-11-18, both items)

- `ConditionExpr::Validate(const IConditionContext&)`: validate against a context instead of a concrete `ConditionRegistry`.
- `IConditionContext` gains `virtual bool Has(StringCRC slot, StringCRC field) const { return true; }` (defaulted, existing implementers compile unchanged). The overload rejects unknown pairs through `Has`, naming them.

### 3. `CellSelectorSet` (`CellSelector.h/.cpp`)

Loads one JSON block shared by presets, scenarios and T-2:

```json
"derived": {
  "dist_river":    { "distance": { "from": "river" } },
  "near_crossing": { "upstream": { "of": "crossing", "maxSteps": 6 } }
},
"selectors": {
  "river":   { "op": "==", "slot": "terrain", "field": "is_river", "value": 1 },
  "wet_low": { "and": [ { "op": "<",  "slot": "terrain", "field": "elevation", "value": 0.2 },
                        { "op": "<=", "slot": "derived", "field": "dist_river", "value": 3 },
                        { "not": { "op": "==", "slot": "biome", "field": "tag.cold", "value": 1 } } ] }
}
```

- Selectors are `ConditionExpr` JSON. `all` is predefined. Caps: 32 selectors, 16 derived fields.
- **Derived fields**, computed once at resolve time in dependency order with the DiaPathfinding helpers, under `slot: "derived"`: `distance {from}` (multi-source hex-step distance, unreachable = +infinity) and `upstream {of, maxSteps}` (1 within `maxSteps` along `terrain.flow_direction` toward a matching cell, else 0).
- **Validation at load** (fails, naming the item): unknown slot or field (through `Validate(IConditionContext&)`); unknown selector referenced by a derived field; a cycle between derived fields and selectors; `maxSteps` < 1; a selector or derived-field count over its cap.
- **Resolve:** each selector gives a sorted cell-index list, deterministic (same world, byte-identical lists). "Empty where used" is the consumer's check.
- **Coverage of today's A4/A5 clauses** (proved by fixtures): `elevation`/`latitude {min,max}` as `and` of `>=`/`<=`; `waterSource` as `==` on `terrain.water_source` (enum ordinal); `river` / protected as `==` on `terrain.is_river` / `terrain.is_protected`; `basin lake|dry|any` on `hydro.basin_kind`; `distanceToWater`/`distanceToRiver` as derived `distance` plus comparisons; `upstreamOfCrossing` as derived `upstream == 1`.

### 4. Save codec, data half (`HexWorldSaveCodec.h/.cpp`)

Serialization of hex-world state records with no CoW content and no save-system wiring.

- **Field list.** A system declares `SavedField[]`: name, kind (`F32`, `U8`, `U16`), scope (`PerCell`, `PerBasin`), and a **range supplied by the caller** (`min`/`max`). Draoi never reads a sidecar: CoW reads the layer and state sidecars and passes the ranges in.
- **Record.** Header `ready, width, height, cell_count, step_day`, a caller-supplied `params_hash` value, and system extras (`basin_count`, `world_hash`). Arrays written with `append(double)` at 17 digits, `-0.0` written as `+0.0`. `ready:false` records hold only `ready`. No in-record `version`.
- **Validate** (all-or-nothing, first failure named): required keys; `cell_count == width*height`, non-zero, <= 65,536; extra count caps declared by the system; array lengths equal counts; every value finite and within the caller's range; integer kinds in range; `step_day >= 0`. Angular fields accept `2π` and the caller normalises. A `ValidatePending`-style entry runs the same checks over a blob (the pending format itself is the CoW half's).
- **Biome palette.** A record holding per-cell biomes writes `biome_palette: [ids in registry order]` once, each cell stores an index into it. Load remaps each id to the current `BiomeRegistry` through `ResolveOrFallback` (one warning per unknown id), records `BiomeRegistry::Hash()`, and a hash mismatch with a clean remap is one Info. The codec takes a `const BiomeRegistry&`; it is not a global.

## Acceptance Criteria

Numbers are those of the original kit spec, kept stable so other documents' references still resolve.

5. **Write and validate.** Header, arrays (17 digits, `-0.0` to `+0.0`) and the Validate checklist are shared. Each failure is rejected naming its field: wrong count, wrong length, non-finite, out-of-range, bad integer kind, a cap exceeded, negative `step_day`. A caller-supplied range changes what Validate accepts, with no code change.
10. **`WorldCellFields` (core).** Registered fields read committed state by cell index. As an `IConditionContext` with `SetCell`, it evaluates a `ConditionExpr` per cell to the hand-computed cell set. `Has` agrees with registration; an unknown pair returns 0/false. With the forbid-reads flag set, a read asserts in Debug. *(The rebuild on `WorldGenerationComplete` is the CoW half's AC 10b.)*
10a. **Field contributions (new 2026-11-23).** `RegisterBiomeFields` (core) and `RegisterTerrainFields` (`DraoiHexWorldTerrain`) register `biome.index`, one `biome.tag.<tag>` per registry tag, `terrain.*`, `terrain.is_protected` (data-declared protected site kinds plus the `Pass`/`RiverCrossesBarrier` bits) and `terrain.region`; on a generated Small seed-1 world each field returns the committed layer value (verified by cell data, not pixels). Neither function names a bus, an event or a CoW identifier.
11. **Dia overload.** `ConditionExpr::Validate(const IConditionContext&)` exists with tests; the existing `Validate(ConditionRegistry&)` behaves as before.
12. **Selectors.** `CellSelectorSet` loads `derived` and `selectors`, and fails naming the item for unknown slot or field, unknown selector reference, a cycle, `maxSteps` < 1, a count over cap. It resolves to sorted cell lists, byte-identical for the same world. `distance` and `upstream` match hand-computed answers on a hand-built hex world.
13. **Clause coverage.** A fixture expresses each of today's A4 and A5 clauses; each resolves to the set the clause's spec text defines. `any` equals `lake` union `dry`.
16. **`IConditionContext::Has`.** Defaulted virtual; `Validate(const IConditionContext&)` rejects unknown pairs through it, naming them; a context that does not override it behaves as before; `WorldCellFields` overrides it.
20. **Angular fields.** Declared `[0, 2π]` inclusive; Validate accepts `2π`, a value above is rejected naming the field.
21. **Palette (data half).** `biome_palette` is written once per record; a record written under registry A loads under registry B (row appended or reordered) to the same biome ids; unknown ids load as fallback with one warning per id; the hash is stored; a mismatch with a clean remap is exactly one Info.
24. **Draoi boundary (new).** The package has no include of CoW headers, no `Modules`, no DiaMessageBus, no DiaSaveGame, no DiaDebugServer, and no `StepChain`/`IStepStage`; `dia check deps` passes with the declared module doc dependencies.

## Files Touched

| File | Change |
|---|---|
| `Draoi/DraoiHexWorld/WorldCellFields.h/.cpp`, `CellSelector.h/.cpp`, `HexWorldSaveCodec.h/.cpp`, `BiomeFieldContribution.h/.cpp` | **New** |
| `Draoi/DraoiHexWorldTerrain/TerrainFieldContribution.h/.cpp` | **New** |
| `Draoi/DraoiHexWorld/Testing/` (fixture hex worlds, fake field contributions) | **New** |
| `Dia/DiaCondition/ConditionExpr.h/.cpp`, `IConditionContext.h` | Overload and defaulted `Has` |
| `Cluiche/Tests/GoogleTests/DiaCondition/TestConditionValidateContext.cpp`, Draoi tests | **New** |
| DraoiHexWorld and DraoiHexWorldTerrain module docs and vcxproj | `dia docs vcxproj-add`; module docs edited by hand (`dia scaffold module` is Dia-only); add `dia.condition` to the core doc |

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| PD-012 | Draoi packages are plain libraries | No modules, bus, save, command or debug wiring in any piece |
| PD-013 | Sims and libraries are scheduler-agnostic | Thread-neutral contracts; no SimPU, PU or step-chain names |
| PD-004 | No STL in public APIs | Pointer and count lists, `DynamicArrayC`, fixed-size strings |
| AD-001 | Ask before Dia edits | `Validate(const IConditionContext&)` and `Has` signed off 2026-11-18 |
| Draoi admission test | Two consumers or a credible second | CoW today; A4/A5/A1 sims and any other hex-world game |

## Test Plan

TDD per `.claude/skills/tdd.md`: (1) `Has` and the `Validate` overload; (2) `WorldCellFields`; (3) `CellSelectorSet` load and validate, resolve, derived fields, clause coverage; (4) codec header, arrays, Validate, palette. Hex fixtures are hand-built (a 6 x 4 world and one 3 x 3 flow tree), with answers computed by hand and asserted as literals. Scoped filter: `*CellFields*:*CellSelector*:*SelectorClauses*:*HexWorldSaveCodec*:*Condition*`, `--shards 2`.

## Open Design Questions

1. **Palette and range inputs. (Resolved, recommended answer.)** The codec takes ranges and the registry as arguments. If a caller must also pass a hash for `params_hash`, is that a plain `uint64_t` in the header struct, or should Draoi own a hash type? **Recommended:** plain `uint64_t`; CoW computes it from `ParameterFile::Hash()`.
2. **Where terrain-owned fields register. (Resolved 2026-11-23: the terrain package exists, so the contribution function lives in `DraoiHexWorldTerrain`, which depends on core.)** `terrain.*` readers need the generator's world type. **Recommended:** the contribution function lives in the terrain package (after its extraction); until then it stays in CoW and the CoW half's S-1 task wires it. Confirm the terrain package depends on core only for the registry type.
3. **Package path and scaffolding. (Resolved 2026-11-23: `Draoi/DraoiHexWorld/`, flat.)**

## Revision 2026-11-23

Made when the extractions landed: (1) Depends-on moved from "must land first" to Done; (2) paths made flat and real; (3) the terrain-owned field contribution (the reader half of old CoW Task 29) is now a Draoi task, AC 10a, so CoW keeps only the wiring and the rebuild; (4) ODQs 1-3 closed. No new Dia change; the signed-off DiaCondition items are already Done (D2, D3).

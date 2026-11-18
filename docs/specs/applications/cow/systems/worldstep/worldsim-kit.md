# Feature Spec: World-Sim Kit

**Parent:** @docs/specs/applications/cow/systems/worldstep/worldstep.md (Feature 4)
**Status:** `Approved` (2026-11-18, user)
**Research:** @docs/research/worldsim_data_driven/summary.md (§0: A3, A4, A7, A8; §0.1 naming)
**Depends on:**
- @docs/specs/applications/dia/systems/diaattribute/parameter-files.md (`ParameterFile`, `Hash`, the in-place default writer)
- @docs/specs/applications/cow/systems/s1-worldgeneration/biome-registry.md (biome tags)
- world-sim spec 4, the DiaPathfinding hex search helpers (derived `distance` / `upstream` fields)

## Summary

The shared CoW-Shared building blocks for every world-sim stage, so A4 Weather, A5 Hydrology and A1 Ecology stop copying A4's plumbing. There are five pieces:

1. **`WorldStepStageBase`**: the stage skeleton and its lifecycle, with five hooks.
2. **Save codec helpers**: the record header, arrays, validation, pending blob and restore sequence, with DiaSaveGame versioning.
3. **`ParamCommandHelper` and `SimCommandSet`**: one `cow.<sys>.params` command per system, and command registration with correct rollback.
4. **`WorldCellFields` and `CellSelectorSet`**: one per-cell field context, and cell selectors written in DiaCondition with derived spatial fields.
5. **The world-sim naming conventions**: names are generated from a system token. S-1's layer ids are renamed to match.

## Problem

A4 and A5 spec nearly the same stage skeleton, save codec and debug plumbing, with dozens of "mirrors A4", "same as A4" and "A4's rule" references. A1 specs none of the revision, queue or discard machinery, so it would diverge. In detail:

| Area | Inconsistency today |
|---|---|
| Health | Stages say "Unhealthy"; World Step and DiaObservation say OK/Degraded/Failing |
| Save participant ids | `"weather"` vs `"cow.hydrology"` |
| `ValidatePending` | Missing from `IWorldStepStage`, though both stages implement it |
| Setting a parameter | Three command shapes (`cow.weather.params {action}`, `cow.hydro.set_param`, `cow.ecology.tuning.set`), none with a defined reply |
| Command registration | S-1's rollback calls `UnregisterQuery` on commands |
| Selectors | A4 and A5 each define a grammar (4 shared clauses, 1 A4-only, 5 A5-only), separate from DiaCondition, which A1 succession uses |
| Layer ids | Four naming styles (S-1 bare camelCase; A4/A5/A1 `<sys>.snake`) |
| Schema prefixes | Four styles: `cow.worldgen.layers/`, `weather/`, `hydro/`, `cow.ecology.cell/` |

## Goals

1. Every shared behaviour is written and tested once, on a fake stage. Systems supply only their differences, through named hooks.
2. One vocabulary each for health, versions, ids, commands, selectors and names.
3. Selectors and succession rules use one condition language (DiaCondition) over one per-cell field context.
4. Names can't drift: they are built from the system token, and a test checks them.

## Non-Goals

- **Each system's fields, passes, forcings and mutation entry kinds.** These stay in the systems; the kit is generic over them.
- **Migrating A4, A5 and A1 onto the kit.** That is world-sim spec 6, as amendments. Only S-1 migrates here: commands, the params command, and the layer id rename.
- **F-3 pack save (`pack.save_as`) and pack selection.**
- **The DiaPathfinding helpers themselves** (spec 4) and **DiaCondition's semantics**. The only DiaCondition change is one `Validate` overload.
- **Log channel codes** (`WGEN`, `WTH`, `HYD`, `ECO`). These stay as short codes, listed in the conventions doc.

## Design

### 1. `WorldStepStageBase` (`Cluiche/CoW/Shared/WorldStep/WorldStepStageBase.h/.cpp`)

```cpp
namespace CoW::WorldStep {
    template<class TSim, class TEntry, class TLatch>
    class WorldStepStageBase : public IWorldStepStage {
    protected:
        struct Hooks {
            // init: build TSim for the current world; false + reason -> health Failing (cause code), not ready
            virtual bool InitialiseSim(TSim& sim, const WorldView& world, CauseCode& cause) = 0;
            virtual bool ExtraReady() const { return true; }                 // A5: a water input exists
            virtual bool LatchInputs(TLatch& latch, StepKey key) = 0;        // false -> step invalid (A5)
            virtual void ApplyEntry(TSim& working, TLatch& latch, const TEntry& e) = 0;   // drained queue entry
            virtual void RunStep(TSim& working, const TLatch& latch, IParallelExecutor&) = 0;
            virtual void PublishCommit(const TSim& committed, uint32_t stepDay) = 0;      // system message
            virtual void OnStartExtra() {}   virtual void OnStopExtra() {}   // A5 provider wiring
        };
        // Owned by the base:
        TSim* mActive; TSim* mPending;
        uint32_t mTargetRevision, mActiveRevision, mPendingRevision, mCaptureRevision, mCapturedDay;
        bool mStepValid; uint32_t mVisitGeneration;
        MutationQueue<TEntry, 128> mQueue;   SpinUpSlot<TSim> mSpinUp;   HoldGenerationToken mHold;
    };
}
```

**Behaviour owned by the base** (the union of today's identical A4/A5 text):

| Behaviour | Rule |
|---|---|
| Ready | `IsReady() = ((pending && pendingRev==target) \|\| (active && activeRev==target)) && health != Failing && ExtraReady()` |
| Promotion | Inside `Capture` only |
| Queue | Drained in arrival order inside `Capture`, before `LatchInputs`. A 129th entry is refused: `Enqueue` returns false, logs one Warning and sets Degraded |
| Discard rule | If `mCaptureRevision != mTargetRevision` or `!mStepValid`: no commit, no message, one Info, `cow.<sys>.discarded_steps` += 1. `Run` is a no-op when the step is invalid |
| Spin-up slot | Atomic `Idle/Running/Done/Cancelled` plus revision and sim pointer, polled in `DoUpdate`. A new revision cancels it, checked between weeks |
| `HoldGeneration` | Taken in `DoStart`, released once subscribed |
| `WorldGenerationFailed` | Failing, not ready, own job cancelled, last world not kept |
| Visit generation | Incremented in `DoStart`; stale job results are gated by revision and visit generation |
| `DoStop` | Unregister the stage, cancel and join the job, destroy both sims, clear the queue, release the hold, then `OnStopExtra` |

**Where each A4/A5 difference goes:**

| Difference (sweep 2026-11-18) | Hook |
|---|---|
| A5 needs a water input to be ready | `ExtraReady` |
| A5 `mStepValid` / invalid input view | `LatchInputs` returns false |
| Init trigger (A4 EC-01 no water source; A5 `Initialise` fails) | `InitialiseSim` with a cause code |
| Mutation kinds (A4 9 kinds, A5 4 so far) | `TEntry` + `ApplyEntry` |
| Commit payload (`WeatherUpdated{week, season}` vs `HydrologyUpdated{week}`; A4 `Age`) | `PublishCommit` (A4 ages inside it) |
| A5 provider and water-source wiring, its `DoStop` extras | `OnStartExtra` / `OnStopExtra` |
| Coupled spin-up (A4 runs, A5 hosts `IHydrologySpinUpHost`) | System code on top of `SpinUpSlot`; the base's slot rules still apply |

**Interface amendment.** `IWorldStepStage` gains `virtual bool ValidatePending(Dia::SaveGame::LoadContext&) const = 0;`. Today it's in neither the interface nor its doc, though both stages implement it (`step-chain.md` amended). **Stage ids are the system tokens:** `weather`, `hydro`, `ecology`. The World Step `order` becomes `["weather", "hydro", "ecology"]`.

**Health.** `Dia::Observation::HealthStatus` (`kOK`/`kDegraded`/`kFailing`). Each system declares cause codes, an enum plus text, reported through one `IHealthReporter` held by the base. "Unhealthy" maps to `kFailing`. **Metrics:** the base registers `cow.<sys>.step_ms` and `cow.<sys>.discarded_steps` as `MetricInfo`.

### 2. Save codec helpers (`Cluiche/CoW/Shared/WorldStep/WorldSimSaveCodec.h/.cpp`)

- **Field list.** A system declares its saved arrays as a `SavedField[]`: name, kind (`f32`, `u8`, `u16`), per-cell or per-basin, and the layer sidecar key (`cow.<sys>.layers/<field>`). Validation reads that entry's `min`/`max`, so **a field's range lives once, in the layer sidecar**, shared by the legend and the save check. A field with no layer gives its range in the system's state sidecar.
- **Record.** The header is `ready, width, height, cell_count, step_day` plus `params_hash = ParameterFile::Hash()` (which covers per-biome maps, replacing A5's "descriptor order, then biome table" hash), plus system extras (`basin_count`, `world_hash`). Arrays are written with `append(double)` at 17 digits, with `-0.0` written as `+0.0`. `ready:false` records hold only `ready`.
- **No in-record `version`.** The version is `ISaveable::GetVersion()` (`kFormatVersion` per system), and upgrades go through `SaveRegistry::RegisterMigration`. Export files (`cow.<sys>_state`) keep the F-3 file envelope (`format` + `version`) because they are files.
- **Validate** (all-or-nothing):
  - required keys present;
  - `cell_count == width*height`, non-zero, ≤ 65,536;
  - extra count caps declared by the system (A5 basins ≤ 4,096);
  - every array length equals its count;
  - every value finite and within the field's sidecar range;
  - integer kinds in range;
  - `step_day ≥ 0`.

  The first failure is named.
- **Restore sequence** (shared; system hooks in brackets):
  1. Cancel the job and drop pending (ignore a stale spin-up completion).
  2. `ready:false` or no world: stop.
  3. Dimension or extra-count mismatch: Warning, discard, spin up.
  4. [`InitialiseSim`], then [`RestoreCommitted(record)`]; if Init fails, discard.
  5. active = target = current revision, set `mRestoredRevision`, ready at once.
  6. `params_hash` mismatch: one Warning, Degraded until a reload or a pack changes the values.
  7. A re-announce at `mRestoredRevision` is ignored.

  **Event-order fallback:** if the world isn't there yet, stash the record and apply it on the next matching `WorldGenerationComplete`.
- **Pending blob.** `{pending_version: 1, valid, step_day, cell_count, …system fields}`. A discarded step writes `{pending_version: 1, valid: false}`. `ValidatePending` runs the same checks as Validate. If a blob can't be restored, the stage is not ready, and `Apply` tolerates `Commit()==false`.
- **Participant ids** are `cow.<sys>`.

### 3. Commands

**`SimCommandSet`** (`Cluiche/CoW/Shared/Debug/SimCommandSet.h/.cpp`) registers a list of `{name, kind: command|query, help, handler}` on the `SimQueryHost`, all-or-nothing. On any failure, each registered entry is rolled back with **its own kind's** unregister, an Error naming the failed entry is logged, and the call returns false. S-1's 18 names migrate to it, which removes the inline code at `WorldGenerationModule.cpp:531-660` and its `UnregisterQuery`-on-commands bug.

**`ParamCommandHelper`** (`Cluiche/CoW/Shared/Debug/ParamCommandHelper.h/.cpp`) adds one sim-hosted command `cow.<sys>.params` with mandatory `SimHelpInfo`, bound to a stage's `ParameterFile` and mutation queue:

| `action` | Args | Effect | Reply |
|---|---|---|---|
| `set` | `key`, `value` | Strict validation now. Queued as a mutation, applied at the next `Capture` | `{queued: true, key, value, effectiveDay}` |
| `get` | `key` | — | `{key, value, default, min, max, kind, source}`, where `source` is `default`, `pack:<id>` or `set` |
| `list` | `prefix`? | — | `{params: [ {key, value, default, min, max, kind, source} … ]}` in schema order |
| `reload` | — | Queued: re-read the sidecar and the active packs (`ParameterFile::Reload`) | `{queued: true, packs: [ids], warnings: [...]}` |
| `save_defaults` | — | **Dev builds only.** Writes current base values into the sidecar's `default` fields with the in-place writer; refused in shipping builds with an error | `{written: path, changed: [keys]}` |

Errors name the key, queue nothing, and use the SimQueryHost error shape. System-specific actions (A4's `cycle`) are passed to a system handler registered with the helper, under the same command name. This keeps one name per system, which matters under the 192-handler cap. S-1's `cow.worldgen.params` query becomes this command.

### 4. `WorldCellFields` and `CellSelectorSet`

**`WorldCellFields`** (`Cluiche/CoW/Shared/World/WorldCellFields.h/.cpp`) is a per-world registry of per-cell fields, keyed `slot.field` (`StringCRC` pairs, the DiaCondition leaf shape).

- **Who contributes what:**
  - S-1: `terrain.*` (elevation, latitude, water source, terrain type, river, crossing, flow direction…), `biome.index` and `biome.tag.<tag>` (bool, from the registry).
  - A4, A5 and A1: their own committed fields (`weather.*`, `hydro.*`, `ecology.*`), when present.
- **Reads:** each field is a typed reader over a cell index, reading committed state only.
- **Condition context:** it implements `Dia::Condition::IConditionContext` with a **cell cursor** (`SetCell(i)`). A1 succession (spec 6) uses the same context.
- **Threading:** SimPU-only; it is never read during `Run`.

**`ConditionExpr::Validate(const IConditionContext&)`** is the one Dia change: validating against a context rather than a concrete `ConditionRegistry`. Sign-off was given 2026-11-18 (research §0 A5b).

**`CellSelectorSet`** (`Cluiche/CoW/Shared/World/CellSelector.h/.cpp`) loads one block, shared by presets, scenarios and T-2:

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

- **Selectors** are `ConditionExpr` JSON (DiaCondition's own format). `all` is predefined.
- **Derived fields** are computed once at resolve time, in dependency order, with the DiaPathfinding helpers:
  - `distance {from: <selector>}` is a multi-source hex-step distance (unreachable = +∞);
  - `upstream {of: <selector>, maxSteps}` is 1 within `maxSteps` along `terrain.flow_direction` toward a matching cell, else 0.

  They appear under `slot: "derived"`.
- **Validation at load** (fails, naming the item):
  - an unknown slot or field (through `Validate(IConditionContext&)`);
  - an unknown selector referenced by a derived field;
  - a cycle between derived fields and selectors;
  - `maxSteps` < 1.
- **Resolve** gives each selector a sorted cell-index list, deterministically. "Empty where used" is the consumer's check, made when a fill or forcing uses the selector (A4 and A5 rule).

**Coverage of today's clauses** (proved by fixtures):

| Today | Expression |
|---|---|
| `elevation`/`latitude {min, max}` | `and` of `>=` / `<=` on `terrain.elevation` / `terrain.latitude` |
| `waterSource` | `==` on `terrain.water_source` (enum ordinal) |
| `river` / `crossing` | `==` on `terrain.is_river` / `terrain.is_crossing` |
| `basin lake\|dry\|any` | `==` on `hydro.basin_kind` (A5 contributes it) |
| `distanceToWater` / `distanceToRiver {min, max}` | derived `distance {from: water}` / `{from: river}` plus `>=` / `<=` |
| `upstreamOfCrossing {maxSteps}` | derived `upstream {of: crossing, maxSteps}` `== 1` |

### 5. Naming conventions

`docs/specs/applications/cow/worldsim-conventions.md` (new, a reference doc) records the §0.1 table:
- the system tokens `worldgen`, `weather`, `hydro`, `ecology`, `succession`;
- the patterns for commands, layer ids, schema keys, save ids, formats, files, packs, events and stage ids;
- the log channel codes;
- the unknown-biome rule.

`WorldSimNames.h` builds every name from the token: `LayerId(sys, field)` → `"<sys>.<field>"`, `LayerSchemaKey`, `ParamSchemaKey`, `CommandName(sys, verb)`, `SaveId`, `StateFormat`, `ParamsFormat`. Kit code uses only these. Field names are `snake_case`.

**S-1 rename.** The 35 layer ids in `WorldLayerSource.cpp:122-159` become `worldgen.<snake_case>` (`flowAccumulation` → `worldgen.flow_accumulation`), with schema keys `cow.worldgen.layers/<snake_case>`. The golden file `worldgen_layers.golden.json` has its keys renamed with **identical hashes**, by a scripted rename. `TestWorldLayerSource`, `TestWorldGenSchema`, `TestWorldGenLayerGoldens` and the S-1 stage sidecar are updated. No file under `Cluiche/Tests/Contracts/` uses these ids (checked 2026-11-18).

## Acceptance Criteria

### Stage base
1. **Shared behaviour.** `WorldStepStageBase` implements `IWorldStepStage`, and a fake-stage test suite proves once:
   - readiness;
   - promotion only in `Capture`;
   - queue drain order, and refusal of the 129th entry (Warning + Degraded);
   - the discard rule (revision mismatch or invalid step: no commit, no message, one Info, counter += 1);
   - spin-up slot cancellation on a new revision;
   - the `HoldGeneration` take and release;
   - `WorldGenerationFailed` handling;
   - the stale-result guard (revision and visit generation);
   - the `DoStop` order.
2. **Hooks.** The five hooks (`InitialiseSim`, `ExtraReady`, `LatchInputs`, `RunStep` with `ApplyEntry`, `PublishCommit`) plus `OnStartExtra` and `OnStopExtra` exist. The spec's difference table maps every A4/A5 difference found in the 2026-11-18 sweep to a hook. A fake stage that sets `ExtraReady=false` or makes `LatchInputs` fail behaves as A5 specs.
3. **Interface and ids.** `IWorldStepStage::ValidatePending` is added, and `step-chain.md` is amended. Stage ids are `weather`, `hydro`, `ecology`, and the World Step order and docs use them.
4. **Health and metrics.** Health is `HealthStatus` with per-system cause codes; no spec text says "Unhealthy" after the spec-6 amendments. `cow.<sys>.step_ms` and `cow.<sys>.discarded_steps` are registered by the base.

### Save codec
5. **Write and validate.**
   - The header, arrays (17 digits, `-0.0` → `+0.0`) and the validate checklist are shared.
   - Each validate failure is rejected, naming its field: wrong count, wrong length, non-finite value, out-of-range value, bad integer kind, a cap exceeded, negative `step_day`.
   - Ranges come from the field's layer sidecar entry: changing a sidecar `max` changes what Validate accepts, with no code change.
6. **Versioning and ids.** No in-record `version`. `GetVersion` plus a registered migration upgrades a v1 fixture to v2 in a test. Participant ids are `cow.<sys>`.
7. **Restore.**
   - The 7-step restore and the event-order fallback are shared and tested with a fake stage.
   - The `{valid: false}` pending form round-trips.
   - A `params_hash` mismatch gives one Warning and Degraded health, cleared by a reload or a pack.
   - Each restore step's failure behaves as listed in the Design.

### Commands
8. **Params command.** `cow.<sys>.params` supports `set`, `get`, `list`, `reload` and `save_defaults` with the reply shapes above.
   - `set` validates strictly, queues, and the value applies at the next `Capture`.
   - Errors name the key and queue nothing.
   - `save_defaults` writes only the `default` fields (other bytes unchanged) in a dev build, and is refused in a shipping build.
   - A system action handler (a fake `cycle`) is reachable under the same command.
9. **Command set.** `SimCommandSet` registers all-or-nothing. A forced failure on the n-th entry unregisters entries 1..n-1 with their own kind's unregister and logs one Error. S-1's 18 names use it, and `cow.worldgen.params` becomes the params command. Every S-1 command and E2E test still passes (renamed where the params query changed).

### Cell fields and selectors
10. **`WorldCellFields`.** S-1 registers `terrain.*`, `biome.index` and `biome.tag.<tag>` (one per registry tag). Each field reads committed state by cell index. As an `IConditionContext` with `SetCell`, it evaluates a `ConditionExpr` per cell. Reading during `Run` asserts in Debug.
11. **Dia overload.** `ConditionExpr::Validate(const IConditionContext&)` exists in DiaCondition with tests. The existing `Validate(ConditionRegistry&)` behaves as before.
12. **Selectors.** `CellSelectorSet` loads `derived` and `selectors`, and fails, naming the item, for:
    - an unknown slot or field;
    - an unknown selector reference;
    - a cycle;
    - `maxSteps` < 1.

    It resolves deterministically to sorted cell lists: the same world gives byte-identical lists. `distance` and `upstream` match hand-computed answers on a hand-built hex world.
13. **Clause coverage.** A fixture expresses each of today's A4 and A5 clauses (the table above). Each resolves to the cell set the clause's spec text defines, on a hand-built world.

### Naming
14. **Conventions.** `worldsim-conventions.md` exists. `WorldSimNames.h` builds every name, and a test asserts that every layer id, command name, schema key, save id and stage id registered by S-1 (and later by each kit user) matches its pattern.
15. **S-1 rename.**
    - S-1's 35 layer ids are `worldgen.<snake_case>` and their schema keys are `cow.worldgen.layers/<snake_case>`.
    - `worldgen_layers.golden.json` keys are renamed, and **every hash is unchanged** (the golden test passes without `DIA_UPDATE_GOLDEN`).
    - The S-1 stage sidecar and tests are updated, and T-1/T-2 spec mentions of S-1 layer ids are aligned.

### Plan-gap additions (added 2026-11-18, plan-gap resolution)
16. **`IConditionContext::Has`.** (added 2026-11-18, plan-gap resolution) `IConditionContext` has `virtual bool Has(StringCRC slot, StringCRC field) const { return true; }`. `ConditionExpr::Validate(const IConditionContext&)` rejects unknown slot/field pairs through `Has`, naming them. A context that doesn't override `Has` behaves as before, and `WorldCellFields` overrides it.
17. **`cow.worldgen.params` and `.status`.** (added 2026-11-18, plan-gap resolution) `cow.worldgen.params` is S-1's ParameterFile command (terrain rules plus terrain profile), the same shape as every other system. The world-shape sliders, seed and size it returned move to `cow.worldgen.status` (query) and the existing `cow.worldgen.set` (command). Every S-1 test and E2E that read shape params from `cow.worldgen.params` reads `cow.worldgen.status` instead.
18. **S-1 log channel.** (added 2026-11-18, plan-gap resolution) S-1 logs use channel `WGEN`, not `"CoW"`.
19. **State sidecar.** (added 2026-11-18, plan-gap resolution) A state field with no layer entry takes its Validate range from `Global/<System>/<sys>_state.schema.json` (format `cow.<sys>_state`). Changing a `max` there changes the verdict with no code change.
20. **Angular fields.** (added 2026-11-18, plan-gap resolution) Angular fields (wind direction and similar) are declared `[0, 2π]` inclusive; Validate accepts `2π` and code normalises into `[0, 2π)`.
21. **Palette once per record.** (added 2026-11-18, plan-gap resolution) `biome_palette` is written once per save record. The pending blob does not repeat it, and is decoded against its committed record's palette.
22. **Preset pack layer.** (added 2026-11-18, plan-gap resolution) A preset's parameter values form their own pack layer, applied last. The next `ApplyPreset` replaces that layer; user packs are kept.
23. **Region selection.** (added 2026-11-18, plan-gap resolution) `set_region`-style region selection reads the S-1 field `terrain.region` registered in `WorldCellFields`.

## Files Touched

| File | Change |
|---|---|
| `Cluiche/CoW/Shared/WorldStep/WorldStepStageBase.h/.cpp`, `MutationQueue.h`, `SpinUpSlot.h`, `WorldSimSaveCodec.h/.cpp`, `Testing/FakeWorldStage.h` | **New** |
| `Cluiche/CoW/Shared/WorldStep/IWorldStepStage.h` (when World Step F1 is built) | `ValidatePending` |
| `Cluiche/CoW/Shared/Debug/SimCommandSet.h/.cpp`, `ParamCommandHelper.h/.cpp` | **New** |
| `Cluiche/CoW/Shared/World/WorldCellFields.h/.cpp`, `CellSelector.h/.cpp`, `WorldSimNames.h` | **New** |
| `Dia/DiaCondition/ConditionExpr.h/.cpp` | `Validate(const IConditionContext&)` overload |
| `Dia/DiaCondition/IConditionContext.h` | Defaulted `virtual bool Has(StringCRC, StringCRC) const` (plan-gap resolution) |
| `Cluiche/Assets/CoW/Global/<System>/<sys>_state.schema.json` | **New** per system, state-field ranges |
| `Cluiche/CoW/Modules/WorldGenerationModule.h/.cpp` | `SimCommandSet`, params command, `WorldCellFields` contribution |
| `Cluiche/CoW/Shared/WorldGeneration/WorldLayerSource.h/.cpp` | Ids renamed |
| `Cluiche/Assets/CoW/Stages/S1WorldGenerationTestbedStage/worldgen.schema.json` | Keys renamed |
| `Cluiche/Tests/GoogleTests/CoW/Golden/worldgen_layers.golden.json` | Keys renamed, hashes unchanged |
| `Cluiche/Tests/GoogleTests/CoW/*` (layer, schema and golden tests) plus new kit tests | Updated / new |
| `docs/specs/applications/cow/worldsim-conventions.md` | **New** |
| `docs/specs/applications/cow/systems/worldstep/step-chain.md`, `worldstep.md` | `ValidatePending`; stage ids; Feature 4 row |
| T-1/T-2 specs naming S-1 layer ids | Aligned |
| `CoW.vcxproj`, `GoogleTests.vcxproj` (+ filters) | Via `dia docs vcxproj-add` |

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| World Step SD (step-chain threading contract) | Everything except `Run` is SimPU-only; `Run` on one worker; readiness is never written during `Run` | The base enforces it: `Run` touches only the working sim and latch, and readiness and queue are SimPU-only. `WorldCellFields` asserts if read during `Run`. |
| PU boundaries are streams (project rule) | No mutex-guarded cross-PU reads | All kit state is SimPU. Off-SimPU readers use the existing snapshots and streams. |
| SD-S1-002 | Every editor action is a DiaAPI command | Params and S-1 commands stay sim-hosted DiaAPI commands. The panel calls the same commands. |
| Sim-hosted commands need mandatory help; 192-handler cap | — | One `cow.<sys>.params` per system, with `SimHelpInfo`. System actions share that name instead of adding handlers. |
| AD-001 (CoW) | Ask before Dia edits | `ConditionExpr::Validate(const IConditionContext&)` was signed off 2026-11-18. The defaulted `IConditionContext::Has` virtual (Plan-gap resolutions) was also signed off by the user on 2026-11-18. |
| PD-004 | No STL in public APIs | Pointer and count lists, `DynamicArrayC` queues, fixed-size strings. |
| D1, D5 (research) | Strict `set`; the sidecar owns defaults | `set` is strict; `save_defaults` writes the sidecar in place. |

## Test Plan

TDD per `.claude/skills/tdd.md`, in this order:
1. `MutationQueue` and `SpinUpSlot`.
2. `WorldStepStageBase` on `FakeWorldStage`, covering every row of the behaviour table and both A5-style hooks.
3. The save codec: validate checklist, restore steps, pending blob, migration.
4. `SimCommandSet` rollback.
5. `ParamCommandHelper` actions over a fixture `ParameterFile`.
6. The `ConditionExpr::Validate` overload.
7. `WorldCellFields`.
8. `CellSelectorSet` (derived fields, cycles, clause coverage).
9. `WorldSimNames` and the pattern test.
10. The S-1 migration: commands, params, cell fields, rename with golden keys only.

Scoped runs use `--filter="*WorldStep*:*WorldSim*:*CellSelector*:*CellFields*:*SimCommandSet*:*ParamCommand*:*Condition*:*WorldGen*:*WorldLayer*"` with `--shards 2`, plus the S-1 E2E stage. There is one full suite run at close-out. The gap audit and edge pass is the last plan task.

## Open Design Questions

1. **Template base versus composition.** `WorldStepStageBase<TSim, TEntry, TLatch>` assumes every stage has one sim type, one entry type and one latch. A1 steps several bands per kick with an accumulator, which may fit awkwardly. **Recommended:** keep the template, and let A1's `TLatch` carry its due bands. Check while writing the A1 amendment (spec 6), and split the base into composable parts only if A1 can't fit.
2. **`WorldCellFields` across worlds and saves.** Fields are registered per world. After a reload or regenerate, consumers holding field handles must re-resolve. **Recommended:** handles are `(slot, field)` CRCs resolved at use, not raw pointers, and the registry is rebuilt on `WorldGenerationComplete`. Confirm the cost is fine for A1 succession's per-cell evaluation (17 rules × 6.6k cells, Year/Decade bands). If it isn't, cache resolved readers per band.
3. **Does `effectiveDay` in the `set` reply hold under deferral?** With DiaDeferredStep, a value queued now applies at the next `Capture`, but its effect commits at day N+k. **Recommended:** `effectiveDay` is the capture day, with a separate `visibleDay` (capture + k) when deferred, so tools don't promise an effect early.

## Resolved Design Questions (2026-11-18, user)

All three are decided as recommended:
1. **Template base.** Keep `WorldStepStageBase<TSim, TEntry, TLatch>`. A1's `TLatch` carries its due bands; the A1 amendment (spec 6) checks the fit, and the base is split into composable parts only if A1 can't fit.
2. **Field handles.** `WorldCellFields` handles are `(slot, field)` CRCs resolved at use, and the registry is rebuilt on `WorldGenerationComplete`. Measure A1 succession's per-cell cost at its first build, and cache resolved readers per band if needed.
3. **The `set` reply.** It returns `effectiveDay` (the capture day) and, when the step is deferred, `visibleDay` (capture + k). **AC 8's reply shape includes both.**

Applied on approval: `IWorldStepStage::ValidatePending` was added to `step-chain.md`. `save-and-history.md` now has the module call each stage's `ValidatePending`. The stage id is `hydro` in `step-chain.md` and `worldstep.md`. The `"hydrology"` stage-id mentions in A1, A4, A5 and S-3 are left for the spec-6 amendments.


## Spec-6 resolutions (2026-11-18, user)

These settle open points raised while amending. Each item supersedes contradicting text above, including the world-sim review amendment.

1. **Biome palette in saves (R2).**
   - Every save record that holds per-cell biomes writes the registry id list once (`biome_palette: [ids...]`), with each cell storing an index into that palette.
   - On load, palette ids are remapped to the current registry through `ResolveOrFallback`, with one warning per unknown id.
   - The registry hash is also recorded. A mismatch with a successful remap is one Info.
   - This is part of the save codec, so A4, A5 and A1 get it. It adds to AC 5.
2. **Records command.** Next to `params`, the kit provides `cow.<sys>.records {action: reload|list}` for record files (species, flow graph, succession rules, tokens).
   - Reload is queued and strict. A bad record fails loudly (R1): health Failing, naming the file, record and field.
   - Registered through `SimCommandSet`.
3. **Selector limits and fields.**
   - `CellSelectorSet` caps a set at 32 selectors and 16 derived fields.
   - Today's `elevation`, `latitude` and `waterSource` clauses map as in the coverage table.
   - The protected-cell field is `terrain.is_protected`, the GDD name, covering corridor sites, Pass, RiverCrossesBarrier, Hearth, Nexus and world-throne cells. It replaces `terrain.is_crossing` in the coverage table.
   - `hydro.basin_kind` takes `none`, `lake` or `dry`; A5's `any` is `!= none`.
4. **Preset naming.** Preset files use format `cow.<sys>_preset` at `Global/<System>/Presets/<id>.<sys>_preset.json`. This replaces `.weatherpreset.json`, `.hydropreset.json` and `cow.hydrology_preset`. S-1's `cow.worldshape` presets are a different concept and are unchanged.

## Plan-gap resolutions (2026-11-18, user)

Each item supersedes contradicting text above.

1. **`IConditionContext::Has`.** DiaCondition's `IConditionContext` gains `virtual bool Has(StringCRC slot, StringCRC field) const { return true; }`; it is defaulted, so existing implementers compile unchanged. `ConditionExpr::Validate(const IConditionContext&)` uses `Has` to reject unknown slot/field pairs, naming them; `WorldCellFields` overrides it. AC 16. This unblocks the plan's Validate task and its dependents.
2. **`cow.worldgen.params`** is S-1's ParameterFile command (terrain rules plus terrain profile, from the Parameter Files migration), the same shape as every other system. The world-shape sliders, seed and size it returned move to `cow.worldgen.status` (query) and `cow.worldgen.set` (command). The S-1 migration in AC 9 and any S-1 test or E2E that read shape params from `cow.worldgen.params` move to `cow.worldgen.status`. AC 17.
3. **Minor rulings:**
   - S-1 logs move from channel `"CoW"` to `WGEN`, per the conventions doc (AC 18).
   - State fields with no layer entry take their Validate range from a per-system **state sidecar**, format `cow.<sys>_state`, at `Global/<System>/<sys>_state.schema.json` (AC 19).
   - Angular fields such as wind direction are declared `[0, 2π]` inclusive and normalised into `[0, 2π)` in code (AC 20).
   - The `biome_palette` is written once per save record. The pending blob doesn't repeat it, and a blob is decoded against its committed record's palette (AC 21).
   - A preset's parameter values form their own pack layer, applied last and replaced by the next `ApplyPreset`; user packs are kept (AC 22).
   - `set_region`-style region selection reads the S-1 field `terrain.region` in `WorldCellFields` (AC 23).

## Status

**Status:** `Approved` (2026-11-18, user). No plan yet; write one at `/implement`, after specs 1, 2 and 4. Interview done with the user on 2026-11-18 (decisions: all four kit pieces including `WorldStepStageBase`; DiaCondition selectors with derived spatial fields over a shared `WorldCellFields` context; `save_defaults` as a dev-only params action; 15 acceptance criteria accepted).

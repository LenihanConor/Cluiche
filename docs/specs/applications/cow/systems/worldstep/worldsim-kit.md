# Feature Spec: World-Sim Kit, CoW Half (stage base, restore, commands, names, wiring)

**Parent:** @docs/specs/applications/cow/systems/worldstep/worldstep.md (Feature 4)
**Status:** `Done` (2026-11-23; was Approved 2026-11-21, split approved by the user, recommended answers to the open questions accepted). The original kit spec was `Approved` 2026-11-18 and its Draoi layer amendment 2026-11-20; this document is the reduced CoW-owned half after the split.
**Draoi half:** @docs/specs/features/draoi/worldsim-kit-draoi.md (`WorldCellFields`, `CellSelectorSet`, codec data half, the DiaCondition overload)
**Plan:** @docs/specs/applications/cow/systems/worldstep/worldsim-kit.plan.md
**Research:** @docs/research/worldsim_data_driven/summary.md (§0: A3, A4, A7, A8; §0.1 naming)
**Depends on:**
- @docs/specs/applications/dia/systems/diadeferredstep/step-chain.md (Done): Dia `StepChain`, `IStepStage`, `StepChainConfig`, `IStepChainListener`. CoW's own chain types no longer exist; `WorldStepModule` stays in CoW and uses the Dia types.
- @docs/specs/applications/dia/systems/diaattribute/parameter-files.md (`ParameterFile`, `Hash`, the in-place default writer)
- @docs/specs/applications/cow/systems/s1-worldgeneration/biome-registry.md (biome tags)
- The Draoi half (@docs/specs/features/draoi/worldsim-kit-draoi.md). The DraoiHexWorld core, terrain and GameSpeed extractions are Done (2026-11-21 to 2026-11-23), so nothing gates it.

## Summary

The CoW-owned building blocks for every world-sim stage, so A4 Weather, A5 Hydrology and A1 Ecology stop copying A4's plumbing. These are **application wiring**: they react to CoW module events, DiaAPI commands, save participants and the step chain. The pure parts live in Draoi (see the Draoi half). Four pieces:

1. **`WorldStepStageBase`** (with `MutationQueue`, `SpinUpSlot`): the stage skeleton and its lifecycle, implementing Dia `IStepStage`, with five hooks.
2. **Save codec, restore half**: the 7-step restore, event-order fallback, pending blob, `cow.<sys>` participant ids and DiaSaveGame versioning. It calls the Draoi codec data half.
3. **`ParamCommandHelper`, `SimCommandSet`, `cow.<sys>.records`**: one params command per system, and command registration with correct rollback.
4. **The world-sim naming conventions** (`WorldSimNames`, `worldsim-conventions.md`), and the S-1 layer id rename.

Plus the **CoW wiring of the Draoi pieces**: S-1's contribution to `WorldCellFields`, rebuilt on `WorldGenerationComplete`; sidecar ranges fed to the codec; the `WorldStepModule` use of Dia `StepChain`.

## Problem

A4 and A5 spec nearly the same stage skeleton, save codec and debug plumbing, with dozens of "mirrors A4", "same as A4" and "A4's rule" references. A1 specs none of the revision, queue or discard machinery, so it would diverge. In detail:

| Area | Inconsistency today |
|---|---|
| Health | Stages say "Unhealthy"; World Step and DiaObservation say OK/Degraded/Failing |
| Save participant ids | `"weather"` vs `"cow.hydrology"` |
| `ValidatePending` | Missing from `IStepStage` when first specced, though both stages implement it (now in Dia's `IStepStage`, see step-chain.md) |
| Setting a parameter | Three command shapes (`cow.weather.params {action}`, `cow.hydro.set_param`, `cow.ecology.tuning.set`), none with a defined reply |
| Command registration | S-1's rollback calls `UnregisterQuery` on commands |
| Selectors | Separate grammars per stage (now the Draoi half's `CellSelectorSet`) |
| Layer ids | Four naming styles (S-1 bare camelCase; A4/A5/A1 `<sys>.snake`) |
| Schema prefixes | Four styles: `cow.worldgen.layers/`, `weather/`, `hydro/`, `cow.ecology.cell/` |

## Goals

1. Every shared stage behaviour is written and tested once, on a fake stage. Systems supply only their differences, through named hooks.
2. One vocabulary each for health, versions, ids, commands and names.
3. Names can't drift: they are built from the system token, and a test checks them.
4. The CoW adapters are thin: Draoi sims expose `Step` and `Capture`/`Run`/`Commit`; the base's hooks call them.

## Non-Goals

- **Each system's fields, passes, forcings and mutation entry kinds.** These stay in the systems (the sims go to Draoi, the adapters stay here).
- **The Draoi half's pieces** (cell fields, selectors, codec data half, the DiaCondition change).
- **Migrating A4, A5 and A1 onto the kit.** That is world-sim spec 6, as amendments. Only S-1 migrates here: commands, the params command, and the layer id rename.
- **F-3 pack save (`pack.save_as`) and pack selection.**
- **Log channel codes** (`WGEN`, `WTH`, `HYD`, `ECO`). These stay as short codes, listed in the conventions doc.
- **The step chain itself** (Dia, Done). **Speculative moves to Dia** of `MutationQueue` and `SpinUpSlot`: one consumer today; revisit when a second game needs them.

## Design

### 1. `WorldStepStageBase` (`Cluiche/CoW/Shared/WorldStep/WorldStepStageBase.h/.cpp`)

```cpp
namespace CoW::WorldStep {
    // Implements Dia::DeferredStep::IStepStage (stage of a Dia StepChain)
    template<class TSim, class TEntry, class TLatch>
    class WorldStepStageBase : public IStepStage {
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

`TSim` is a **Draoi sim** (`WeatherSimulation` and so on). Each Draoi sim exposes a synchronous `Step(...)` and a split `Capture` / `Run` / `Commit` form; the sim owns its working copy and swaps on `Commit`. `InitialiseSim`, `ApplyEntry`, `RunStep` and `RestoreCommitted` call those methods; `LatchInputs`, `PublishCommit` and the extras are adapter code. The adapter fills each sim's inputs once per step during `Capture`, as whole arrays. No per-cell virtual call crosses a package boundary inside `Run`. Feedback inputs (hydrology lakes into weather, and so on) are plain per-cell structs owned by the upstream Draoi sim, with the fill function in the downstream package; they replace the `IBiomeSeasonalBaseline`, `IWaterInputSource` and `IHydrologyInputsProvider` interfaces.

`IParallelExecutor` is the DiaThreading type (signed off 2026-11-20; the move is part of the Dia step-chain work, confirm its current location at build time).

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

**Interface.** `IStepStage::ValidatePending(Dia::SaveGame::LoadContext&) const` is already in Dia's `IStepStage` (step-chain.md). The base implements it through the codec. **Stage ids are the system tokens:** `weather`, `hydro`, `ecology`; the `StepChainConfig` order used by `WorldStepModule` is `["weather", "hydro", "ecology"]`. `WorldStepModule` stays in CoW ("this game's clock") and owns the manifest wiring, key source and cadence of a Dia `StepChain`.

**Health.** `Dia::Observation::HealthStatus` (`kOK`/`kDegraded`/`kFailing`). Each system declares cause codes, an enum plus text, reported through one `IHealthReporter` held by the base. "Unhealthy" maps to `kFailing`. **Metrics:** the base registers `cow.<sys>.step_ms` and `cow.<sys>.discarded_steps` as `MetricInfo`.

**Bus.** The bus carries notifications, not sim data. `WeatherUpdated`, `HydrologyUpdated`, generation complete or failed and health changes stay on DiaMessageBus, declared in `cow_worldsim.diagamemessages` and published by the CoW adapters from `PublishCommit`. Draoi sims never touch the bus; per-cell data never goes through it (size, and delivery timing would break deterministic in-step ordering, F-2).

### 2. Save codec, restore half (`Cluiche/CoW/Shared/WorldStep/WorldSimSaveCodec.h/.cpp`)

The data half (header, arrays, Validate, palette) is in Draoi. This half adds the CoW wiring:

- **Ranges.** CoW reads each field's `min`/`max` from the layer sidecar entry (`cow.<sys>.layers/<field>`) and passes them to the Draoi codec, so **a field's range lives once, in the layer sidecar**, shared by the legend and the save check. A field with no layer takes its range from the system's state sidecar (`Global/<System>/<sys>_state.schema.json`, format `cow.<sys>_state`).
- **`params_hash`** is `ParameterFile::Hash()` (covers per-biome maps), passed into the Draoi header.
- **Versioning.** No in-record `version`. The version is `ISaveable::GetVersion()` (`kFormatVersion` per system); upgrades go through `SaveRegistry::RegisterMigration`. Export files (`cow.<sys>_state`) keep the F-3 file envelope (`format` + `version`).
- **Restore sequence** (shared; system hooks in brackets):
  1. Cancel the job and drop pending (ignore a stale spin-up completion).
  2. `ready:false` or no world: stop.
  3. Dimension or extra-count mismatch: Warning, discard, spin up.
  4. [`InitialiseSim`], then [`RestoreCommitted(record)`]; if Init fails, discard.
  5. active = target = current revision, set `mRestoredRevision`, ready at once.
  6. `params_hash` mismatch: one Warning, Degraded until a reload or a pack changes the values.
  7. A re-announce at `mRestoredRevision` is ignored.

  **Event-order fallback:** if the world isn't there yet, stash the record and apply it on the next matching `WorldGenerationComplete`.
- **Pending blob.** `{pending_version: 1, valid, step_day, cell_count, …system fields}`. A discarded step writes `{pending_version: 1, valid: false}`. `ValidatePending` runs the same checks as Validate (via the Draoi codec). The blob does not repeat `biome_palette` and is decoded against its committed record's palette; a blob with no committed record is rejected naming the field. If a blob can't be restored, the stage is not ready, and `Apply` tolerates `Commit()==false`.
- **Participant ids** are `cow.<sys>`.

### 3. Commands

**`SimCommandSet`** (`Cluiche/CoW/Shared/Debug/SimCommandSet.h/.cpp`) registers a list of `{name, kind: command|query, help, handler}` on the `SimQueryHost`, all-or-nothing. On any failure, each registered entry is rolled back with **its own kind's** unregister, an Error naming the failed entry is logged, and the call returns false. S-1's 18 names migrate to it, which removes the inline code at `WorldGenerationModule.cpp:531-660` and its `UnregisterQuery`-on-commands bug.

**`ParamCommandHelper`** (`Cluiche/CoW/Shared/Debug/ParamCommandHelper.h/.cpp`) adds one sim-hosted command `cow.<sys>.params` with mandatory `SimHelpInfo`, bound to a stage's `ParameterFile` and mutation queue:

| `action` | Args | Effect | Reply |
|---|---|---|---|
| `set` | `key`, `value` | Strict validation now. Queued as a mutation, applied at the next `Capture` | `{queued: true, key, value, effectiveDay}` plus `visibleDay` (capture + k) when the step is deferred |
| `get` | `key` | — | `{key, value, default, min, max, kind, source}`, where `source` is `default`, `pack:<id>` or `set` |
| `list` | `prefix`? | — | `{params: [ {key, value, default, min, max, kind, source} … ]}` in schema order |
| `reload` | — | Queued: re-read the sidecar and the active packs (`ParameterFile::Reload`) | `{queued: true, packs: [ids], warnings: [...]}` |
| `save_defaults` | — | **Dev builds only.** Writes current base values into the sidecar's `default` fields with the in-place writer; refused in shipping builds with an error | `{written: path, changed: [keys]}` |

Errors name the key, queue nothing, and use the SimQueryHost error shape. System-specific actions (A4's `cycle`) are passed to a system handler registered with the helper, under the same command name (one name per system, for the 192-handler cap). S-1's `cow.worldgen.params` query becomes this command. A preset's parameter values form their own pack layer, applied last and replaced by the next `ApplyPreset`; user packs are kept.

**`cow.<sys>.records {action: reload|list}`** serves record files (species, flow graph, succession rules, tokens). Reload is queued and strict: a bad record fails loudly (R1), health Failing, naming the file, record and field. Registered through `SimCommandSet`.

### 4. CoW wiring of the Draoi cell fields and selectors

- S-1 contributes `terrain.*`, `biome.index`, `biome.tag.<tag>`, `terrain.is_protected` and `terrain.region` by calling the Draoi registration functions (`RegisterBiomeFields`, `RegisterTerrainFields`, Draoi half AC 10a); the readers themselves live in Draoi, not CoW. CoW's part is the call and the `Clear` and rebuild on `WorldGenerationComplete`.
- The `WorldCellFields` registry is cleared and rebuilt on `WorldGenerationComplete`. A4, A5 and A1 contribute `weather.*`, `hydro.*`, `ecology.*` when present.
- The forbid-reads flag is set by the base while `Run` is active, so a read during `Run` asserts in Debug.
- `CellSelectorSet` is used by stages and presets as the Draoi half defines it; this half only supplies the loaded JSON block and the cell-field context.

### 5. Naming conventions

`docs/specs/applications/cow/worldsim-conventions.md` (new, a reference doc) records the §0.1 table:
- the system tokens `worldgen`, `weather`, `hydro`, `ecology`, `succession`;
- the patterns for commands, layer ids, schema keys, save ids, formats, files, packs, events and stage ids;
- the log channel codes;
- the unknown-biome rule, and preset naming (`cow.<sys>_preset` at `Global/<System>/Presets/<id>.<sys>_preset.json`; S-1's `cow.worldshape` presets are a different concept and unchanged).

`WorldSimNames.h` builds every name from the token: `LayerId(sys, field)` → `"<sys>.<field>"`, `LayerSchemaKey`, `ParamSchemaKey`, `CommandName(sys, verb)`, `SaveId`, `StateFormat`, `ParamsFormat`, `PresetFormat`, `PresetPath`, `StageId`, `MetricName`. Kit code uses only these. Field names are `snake_case`.

**S-1 rename.** The 35 layer ids in `Draoi/DraoiHexWorldTerrain/WorldLayerSource.cpp` (moved there by the terrain extraction; the old `Cluiche/CoW/Shared/WorldGeneration/` copy is gone) become `worldgen.<snake_case>` (`flowAccumulation` → `worldgen.flow_accumulation`), with schema keys `cow.worldgen.layers/<snake_case>`. The golden file `worldgen_layers.golden.json` has its keys renamed with **identical hashes**, by a scripted rename. `TestWorldLayerSource`, `TestWorldGenSchema`, `TestWorldGenLayerGoldens` and the S-1 stage sidecar are updated. No file under `Cluiche/Tests/Contracts/` uses these ids (checked 2026-11-18). S-1 logs in the CoW module and shared files move from channel `"CoW"` to `WGEN` (82 sites in `Cluiche/CoW/Modules/WorldGen` and `Shared`); the Draoi generator already logs `"HexWorld"` and is not renamed.

**Placement of the rename (raised 2026-11-23, needs a user ruling before Tasks 33-34).** The layer ids and the schema constant `kWorldLayerSchemaFor = "cow.worldgen.layers"` now live in `DraoiHexWorldTerrain`, which must carry no CoW strings. **Recommended:** rename the ids in Draoi to the neutral `worldgen.<snake_case>` (no `cow.` in the id), and have CoW inject the schema prefix `cow.worldgen.layers/` the way it injects seed names and node kinds; the golden file and the CoW tests change as before.

**Coupling with the generic special sites feature.** `terrain.is_protected` and the site-kind names are data-declared (`guarantees.json`, @docs/specs/features/draoi/guarantee-set-injection.md); the S-1 rename task does not touch site kind names or `CoW.Guarantee.*` ids.

## Acceptance Criteria

Numbers are those of the original kit spec, kept stable. ACs 5, 11, 12, 13, 16, 20 and the core of 10 and 21 moved to the Draoi half.

### Stage base
1. **Shared behaviour.** `WorldStepStageBase` implements Dia `IStepStage`, and a fake-stage test suite proves once: readiness; promotion only in `Capture`; queue drain order and refusal of the 129th entry (Warning + Degraded); the discard rule; spin-up slot cancellation on a new revision; the `HoldGeneration` take and release; `WorldGenerationFailed` handling; the stale-result guard (revision and visit generation); the `DoStop` order.
2. **Hooks.** The five hooks plus `OnStartExtra`/`OnStopExtra` exist. The difference table maps every A4/A5 difference from the 2026-11-18 sweep to a hook. A fake stage with `ExtraReady=false` or failing `LatchInputs` behaves as A5 specs.
3. **Interface and ids.** The base implements Dia `IStepStage::ValidatePending`. Stage ids are `weather`, `hydro`, `ecology`; `WorldStepModule`'s `StepChainConfig` order and the docs use them.
4. **Health and metrics.** Health is `HealthStatus` with per-system cause codes; no spec text says "Unhealthy" after the spec-6 amendments. `cow.<sys>.step_ms` and `cow.<sys>.discarded_steps` are registered by the base.

### Save codec (CoW half)
5b. **Ranges from sidecars.** Changing a layer-sidecar or state-sidecar `max` changes what the Draoi Validate accepts when called through the CoW wiring, with no code change (AC 19). The Draoi half owns the validate checklist (its AC 5).
6. **Versioning and ids.** No in-record `version`. `GetVersion` plus a registered migration upgrades a v1 fixture to v2 in a test. Participant ids are `cow.<sys>`.
7. **Restore.** The 7-step restore and the event-order fallback are shared and tested with a fake stage. The `{valid: false}` pending form round-trips. A `params_hash` mismatch gives one Warning and Degraded, cleared by a reload or a pack. Each restore step's failure behaves as listed.
19. **State sidecar.** A state field with no layer entry takes its Validate range from `Global/<System>/<sys>_state.schema.json` (format `cow.<sys>_state`).
21b. **Pending blob and palette.** The pending blob has no `biome_palette` and is decoded against its committed record's palette; a blob with no committed record is rejected naming the field.

### Commands
8. **Params command.** `cow.<sys>.params` supports `set`, `get`, `list`, `reload` and `save_defaults` with the reply shapes above. `set` validates strictly, queues, and applies at the next `Capture`; errors name the key and queue nothing; `save_defaults` writes only the `default` fields in a dev build and is refused in a shipping build; a system action handler (a fake `cycle`) is reachable under the same command. The `set` reply includes `effectiveDay` and, when deferred, `visibleDay`.
9. **Command set.** `SimCommandSet` registers all-or-nothing. A forced failure on the n-th entry unregisters entries 1..n-1 with their own kind's unregister and logs one Error. S-1's 18 names use it, and `cow.worldgen.params` becomes the params command. Every S-1 command and E2E test still passes (renamed where the params query changed). The `cow.<sys>.records` command is registered through the set, fails loudly on a bad record and keeps the previous records.
17. **`cow.worldgen.params` and `.status`.** `cow.worldgen.params` is S-1's ParameterFile command (terrain rules plus terrain profile). The world-shape sliders, seed and size move to `cow.worldgen.status` (query) and the existing `cow.worldgen.set`. Every S-1 test and E2E that read shape params from `params` reads `status`.
22. **Preset pack layer.** A preset's values form their own pack layer, applied last; the next `ApplyPreset` replaces it; user packs are kept.

### Cell-field wiring
10b. **S-1 contribution.** S-1 calls the Draoi contributions (`terrain.*`, `biome.index`, `biome.tag.<tag>` per registry tag, `terrain.is_protected`, `terrain.region`; AC 23, Draoi AC 10a); each field reads committed state by cell index; the registry is rebuilt on `WorldGenerationComplete`; reading during `Run` asserts in Debug.
23. **Region selection.** `set_region`-style selection reads `terrain.region`.

### Naming
14. **Conventions.** `worldsim-conventions.md` exists. `WorldSimNames.h` builds every name, and a test asserts that every layer id, command name, schema key, save id and stage id registered by S-1 (and later by each kit user) matches its pattern.
15. **S-1 rename.** S-1's 35 layer ids are `worldgen.<snake_case>` and schema keys `cow.worldgen.layers/<snake_case>`. `worldgen_layers.golden.json` keys are renamed and **every hash is unchanged** (the golden test passes without `DIA_UPDATE_GOLDEN`). The S-1 stage sidecar and tests are updated, and T-1/T-2 spec mentions of S-1 layer ids are aligned.
18. **S-1 log channel.** S-1 logs in the CoW module and shared files use channel `WGEN`, not `"CoW"`. The Draoi generator keeps `"HexWorld"` (already built).

## Files Touched

| File | Change |
|---|---|
| `Cluiche/CoW/Shared/WorldStep/WorldStepStageBase.h/.cpp`, `MutationQueue.h`, `SpinUpSlot.h`, `WorldSimSaveCodec.h/.cpp`, `Testing/FakeWorldStage.h` | **New** |
| `Cluiche/CoW/Shared/Debug/SimCommandSet.h/.cpp`, `ParamCommandHelper.h/.cpp` | **New** |
| `Cluiche/CoW/Shared/World/WorldSimNames.h` | **New** |
| `Cluiche/Assets/CoW/Global/<System>/<sys>_state.schema.json` | **New** per system, state-field ranges |
| `Cluiche/CoW/Modules/WorldGen/WorldGenerationModule.h/.cpp` | `SimCommandSet`, params command, cell-field contribution |
| `Draoi/DraoiHexWorldTerrain/WorldLayerSource.h/.cpp` | Ids renamed; schema prefix injected (see Placement of the rename) |
| `Cluiche/Assets/CoW/Stages/S1WorldGenerationTestbedStage/worldgen.schema.json` | Keys renamed |
| `Cluiche/Tests/GoogleTests/CoW/Golden/worldgen_layers.golden.json` | Keys renamed, hashes unchanged |
| `Cluiche/Tests/GoogleTests/CoW/*` (layer, schema, golden tests) plus new kit tests | Updated / new |
| `docs/specs/applications/cow/worldsim-conventions.md` | **New** |
| `docs/specs/applications/cow/systems/worldstep/worldstep.md`, T-1/T-2 specs naming S-1 layer ids | Feature 4 row; ids aligned |
| `CoW.vcxproj`, `GoogleTests.vcxproj` (+ filters) | Via `dia docs vcxproj-add` |

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| World Step SD (step-chain threading contract) | Everything except `Run` is SimPU-only; `Run` on one worker; readiness is never written during `Run` | The base enforces it: `Run` touches only the working sim and latch; readiness and queue are SimPU-only |
| PD-011 to PD-014 (Draoi) | Draoi is plain libraries; wiring and execution order stay in the application | Stage base, restore, commands, names and bus publishing stay in CoW; pure parts are in the Draoi half |
| PU boundaries are streams (project rule) | No mutex-guarded cross-PU reads | All kit state is SimPU. Off-SimPU readers use the existing snapshots and streams |
| SD-S1-002 | Every editor action is a DiaAPI command | Params and S-1 commands stay sim-hosted DiaAPI commands |
| Sim-hosted commands need mandatory help; 192-handler cap | — | One `cow.<sys>.params` per system, with `SimHelpInfo` |
| AD-001 (CoW) | Ask before Dia edits | This half edits no Dia file (the DiaCondition change is the Draoi half's, signed off 2026-11-18) |
| PD-004 | No STL in public APIs | Pointer and count lists, `DynamicArrayC` queues, fixed-size strings |
| D1, D5 (research) | Strict `set`; the sidecar owns defaults | `set` is strict; `save_defaults` writes the sidecar in place |

## Test Plan

TDD per `.claude/skills/tdd.md`, in this order: (1) `MutationQueue` and `SpinUpSlot`; (2) `WorldStepStageBase` on `FakeWorldStage`; (3) the restore half, pending blob, migration; (4) `SimCommandSet` rollback; (5) `ParamCommandHelper`; (6) `WorldSimNames` and the pattern test; (7) the S-1 migration: commands, params, cell-field contribution, rename with golden keys only. Scoped runs use `--filter="*WorldStep*:*WorldSim*:*SimCommandSet*:*ParamCommand*:*WorldGen*:*WorldLayer*:*MutationQueue*:*SpinUpSlot*"` with `--shards 2`, plus the S-1 E2E stage. One full suite run at close-out. The gap audit and edge pass is the last plan task.

## Open Design Questions

1. **Template base versus composition** (resolved 2026-11-18: keep the template; A1's `TLatch` carries its due bands; split only if A1 can't fit). Still open after the split: does the base compile against a Draoi sim exposing `Step`/`Capture`/`Run`/`Commit` with the sim owning its working copy, given the base currently owns `mActive`/`mPending` sim pointers? **Recommended:** the base keeps owning the sim instances, and the sim's `Commit` swaps its internal working copy; confirm in the first Weather amendment.
2. **Where the S-1 cell-field readers live. (Resolved 2026-11-23: in `DraoiHexWorldTerrain`, Draoi half task D4b; CoW only calls them.)**
3. **Placement of the layer-id rename.** See "Placement of the rename" under the S-1 rename; needs a user ruling before Tasks 33-34.
3. **`MutationQueue` and `SpinUpSlot` placement.** CoW beside the base today (one consumer). **Recommended:** keep in CoW; promote to Dia only when a second game appears (admission test).

## Resolved Design Questions (2026-11-18, user)

1. **Template base.** Keep `WorldStepStageBase<TSim, TEntry, TLatch>`; A1's `TLatch` carries its due bands.
2. **Field handles.** `WorldCellFields` handles are `(slot, field)` CRCs resolved at use, rebuilt on `WorldGenerationComplete`. Measure A1 succession's per-cell cost at first build, and cache resolved readers per band if needed.
3. **The `set` reply.** Returns `effectiveDay` (the capture day) and, when deferred, `visibleDay` (capture + k).

`save-and-history.md` has the module call each stage's `ValidatePending`. The stage id is `hydro` in `worldstep.md`; the `"hydrology"` stage-id mentions in A1, A4, A5 and S-3 are left for the spec-6 amendments.

## Spec-6 resolutions (2026-11-18, user)

1. **Biome palette in saves.** Written once per record; remapped on load (Draoi half, AC 21); the CoW half covers the pending-blob side (AC 21b).
2. **Records command.** `cow.<sys>.records {action: reload|list}` (above).
3. **Selector limits and fields.** Caps (32 selectors, 16 derived), `terrain.is_protected` and `hydro.basin_kind` are the Draoi half's.
4. **Preset naming.** Files use format `cow.<sys>_preset` at `Global/<System>/Presets/<id>.<sys>_preset.json`, replacing `.weatherpreset.json`, `.hydropreset.json` and `cow.hydrology_preset`.

## Plan-gap resolutions (2026-11-18, user)

1. **`IConditionContext::Has`**: Draoi half (AC 16).
2. **`cow.worldgen.params`** is S-1's ParameterFile command; shape params, seed and size move to `cow.worldgen.status` / `cow.worldgen.set` (AC 17).
3. **Minor rulings:** S-1 log channel `WGEN` (AC 18); state sidecar ranges (AC 19); angular fields (Draoi AC 20); palette once per record (Draoi AC 21 and AC 21b); preset pack layer (AC 22); `terrain.region` (AC 23).

## Draoi layer split (2026-11-20 amendment, carried into 2026-11-21 split)

The platform gained the Draoi layer (PD-011 to PD-014). Division of the original kit:

| Kit piece | Goes to |
|---|---|
| `WorldStepStageBase`, `MutationQueue`, `SpinUpSlot` | **CoW** (this document) |
| Save codec restore half, pending blob, participant ids | **CoW** |
| `SimCommandSet`, `ParamCommandHelper`, `cow.<sys>.records` | **CoW** |
| `WorldSimNames`, `worldsim-conventions.md` | **CoW** |
| Save codec data half (`SavedField[]`, header, arrays, Validate, palette) | **DraoiHexWorld** (Draoi half) |
| `WorldCellFields`, `CellSelectorSet` (derived fields) | **DraoiHexWorld** (Draoi half) |
| `ConditionExpr::Validate(const IConditionContext&)`, `IConditionContext::Has` | Dia, delivered by the Draoi half |

Package dependencies follow the data flow one way: `DraoiHexWorld <- DraoiHexWorldWeather <- DraoiHexWorldHydrology <- DraoiHexWorldEcology`. A package includes the ones before it directly; there are no injected interfaces between sims. A game can stop the chain at any point but cannot skip a middle link. A1, A4 and A5 need matching amendments (sim in Draoi, adapter in CoW, the input structs); they are owed separately.

## Status

**Status:** `Done` (2026-11-23; Approved 2026-11-21), split approved. Plan complete, see `worldsim-kit.plan.md`. Carried-over decisions are unchanged from the 2026-11-18 interview and the 2026-11-20 Draoi amendment. Plan: `worldsim-kit.plan.md` (revised 2026-11-23, Not Started). Build order: all prerequisites are Done (step-chain, Parameter Files, biome registry, special sites, Strata, core, terrain and GameSpeed extractions); the Draoi half's D4, D4b, D5 and D6 come first, then this half's Draoi-dependent tasks. **Revision 2026-11-23:** terrain field readers moved to the Draoi half (D4b), the rename and log-channel notes updated for the extracted terrain package.

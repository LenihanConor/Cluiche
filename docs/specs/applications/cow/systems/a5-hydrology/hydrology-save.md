# Feature Spec: A5 Hydrology Save/Load

**Parent:** @docs/specs/applications/cow/systems/a5-hydrology/a5-hydrology.md (Feature 4)
**Status:** Draft (2026-11-17)
**Design source:** GDD `08_SystemsDeepDive/A5_Hydrology/` (drafted 2026-11-17, **not locked**; the GDD session `cow-68` has it). This spec cannot be Approved before the GDD is locked or accepts the differences listed below (AD-003).
**Depends on:**
- A5 Features 1, 2 and 3 (`HydrologyFields`, `BasinState`, `HydrologySimulation`, `HydrologyModule`, the stage).
- World Step Feature 3 (the coordinator delegates `SerializePending`/`DeserializePending` to this stage and calls `ValidatePending`).
- DiaDeferredStep Feature 3 (`DeferredStepSaveParticipant`).
- E1 `SaveLoadModule::RegisterParticipant` and `CaptureState()` (built).

**Mirrors:** A4 Feature 4 (@docs/specs/applications/cow/systems/a4-weather/weather-save.md). Same encoding, same checks, same ordering rules.
**Consumed by:** S-3.5 R2/R3 (save/load determinism), every stage that saves with hydrology running.

## Summary

Two things, both data with no new mechanics:

1. **`HydrologySaveParticipant`**, an E1 `ISaveable` (`cow.hydrology`) that writes the committed state as JSON text arrays and restores it into a fresh simulation on load **without re-running the spin-up**.
2. The stage's **`SerializePending` / `DeserializePending` / `ValidatePending`**, which write and restore a completed-but-unapplied working set. A save taken mid-step then reloads identically to the uninterrupted run (DFS-005).

The round trip is bit-identical for finite floats. Terrain is never saved. It comes back from S-1.

## Goals

1. Save, load, continue for N weeks gives committed fields bit-identical to a run that never saved.
2. A save taken mid-step restores the pending working set and applies it at its original key.
3. A corrupt or inconsistent hydrology save aborts the load in `Validate`, before any state changes.
4. A save that does not match the loaded world never corrupts A5. It is discarded and the world loads fresh.
5. Loading never re-runs the spin-up when the save matches the world.
6. Tuning that differs from the saved run is **visible**, not silent (A4's rule).
7. Every reader sees the same committed values after a load as it would have seen without one: A4 (at its next `Capture`), A1, A3 and V-1.

## Non-Goals

- The scheduler's drain, the pending-record envelope, stage-id tagging and the missing-blob refusal (DiaDeferredStep Feature 3, World Step Feature 3).
- Saving parameters. They live in the file and F-3 packs.
- Saving the **latched step inputs**: sink arrays, the influence array, the source view and the fixed driver's position. A pending step is a *completed* working set (see Design), so nothing re-runs and nothing needs them. After a load the next kick latches fresh ones.
- Saving the mutation queue or presets. Debug tools, as in A4.
- A binary format. JSON text only (the 2026-10-15 decision).
- Saving A4's or A1's state. Each owns its own participant.

## Design

### Participant registration

`SaveLoadModule::RegisterParticipant(StringCRC("cow.hydrology"), &mParticipant)` in `HydrologyModule::DoStart`, unregistered in `DoStop`. `SaveLoadModule` is a **soft** dependency. Absent (S-2.6 without saves), A5 runs and does not participate.

Registration order is serialization order:
- **After S-1's participant.** The world must be restored before hydrology is rebuilt.
- **Before DiaDeferredStep's participant.** The pending result applies on top of committed state. That participant attaches on `DeferredStepModule`'s first `DoUpdate` (World Step Feature 3 amendment 5), so this always holds.
- A5's committed state does not depend on A4's, so their relative order does not matter.

### Record (version 1)

```json
"cow.hydrology": {
  "version": 1,
  "ready": true,
  "width": 104, "height": 64, "cell_count": 6656, "basin_count": 9,
  "week": 412,
  "world_hash": "9f3c...",                  // present only when the world exposes a content hash
  "params_hash": 7712930193384,
  "surface_water": [...], "soil_moisture": [...], "ground_water": [...], "river_store": [...],
  "mean_flow": [...], "ice": [...], "glacier": [...], "season_min_snow": [...],
  "river_flow": [...],
  "inundated_weeks": [...], "exposed_weeks": [...], "snow_years": [...],
  "glacier_eligible": [...], "flooded": [...],
  "biome": [2, 2, 4, ...],
  "basin_volume": [...]
}
```

- **Saved fields:** exactly Feature 1's "saved" list: `surfaceWater`, `soilMoisture`, `groundWater`, `riverStore`, `meanFlow`, `ice`, `glacier`, `seasonMinSnow`, `inundatedWeeks`, `exposedWeeks`, `snowYears`, `glacierEligible`; plus per-basin `volume`.
- **Three additions to that list, each needed for the round trip:**
  - **`biome`**, the A5-cached biome per cell as `uint8_t`. A3's succession changes reach A5 through `BiomeTransitionComplete`. S-1's `Cell::biome` is only the ingest source, so without this array a load would use pre-succession capacities. It is authoritative on load, the same rule as A4.
  - **`riverFlow`** and **`flooded`**. They are derived every week from the cascade, but no later pass can rebuild them from the saved stores, and readers use them between a commit and the next step (A1's seasonal reads, A3's counters, V-1). Without them a save taken between two kicks would read zero flow and no floods until the next commit. Open Design Question 1.
- **Re-derived on load, not saved:** basin `level`, `openWater`, `passability`. They are pure functions of the saved stores. `RederiveCommitted()` (the same code as `Initialise`'s final derivation, Feature 1) computes them. A4 reads `openWater` and `surfaceWetness` at its next `Capture`, so a test checks that they equal the uninterrupted run's values.
- `week` is the last applied week (`WeekOf` of the last committed step day) as `int64_t`.
- `ready: false` writes only `version` and `ready` (a save taken before the first world is spun up). It restores as a no-op.
- `params_hash` is a 64-bit FNV-1a over the **bit patterns** of the effective `HydrologyParameters` in descriptor-table order, then the biome table in enum order (`HashHydrologyParameters`, pure, new here).
- `world_hash` is optional. `GetWorldContentHash()` is a pending F-3 seam and is not built. The field is written and checked only when the world exposes one.
- Arrays are written with `BeginArray`/`CurrentNode().append(double)` because `SaveContext` has no array-element writer. Floats are written as doubles at 17 significant digits, so every `float` round-trips exactly (A4's rule). `uint8_t`/`uint16_t` are written as integers. `-0.0` is written as `+0.0`.
- Non-finite values never occur (Feature 2 clamps every field), so JSON never has to carry them.
- **Size at Large** (6,656 cells): about 9 float arrays × ~32 bytes plus the integer arrays is about 2.5 MB. Committed plus a pending blob stays under 8 MB, inside `SaveContext::kBufferSize`. Reported, not gated.

### `Validate` (all-or-nothing, mutates nothing)

Runs before any participant deserializes, so it checks **internal consistency only**. The live world is still the old one. It returns false, logging an Error with the key, when:

- `version` is missing, `0`, or greater than 1;
- `ready` is true and `width`, `height`, `cell_count`, `basin_count`, `week`, `params_hash`, or any array is missing or the wrong type;
- `cell_count != width * height`, `width` or `height` is 0, or `cell_count` exceeds 65,536 (a corrupt-count guard);
- `basin_count` exceeds 4,096 (a corrupt-count guard);
- a per-cell array's length is not `cell_count`, or `basin_volume`'s is not `basin_count`;
- a float is non-finite or negative, or `season_min_snow` is outside `[0,1]`;
- a counter is outside `0..65535`, `flooded` or `glacier_eligible` is not 0 or 1, or `biome >= 13`;
- `week < 0`.

Stores are **not** range-checked against per-biome capacities. Parameters may have changed since the save, and Feature 2's clamp pushes any excess into the next store on the next step (water is not destroyed). `ready: false` is valid with nothing else present.

### `Deserialize`

SimPU, after every participant validated, in this order:

1. **Cancel** any running own spin-up job and drop `mPending`. Any coupled ticket held by A4's job is A4's to destroy; `CompleteSpinUp` for it is ignored if it arrives (its revision is not `mTargetRevision`, or equals `mRestoredRevision`).
2. If `ready` is false, or `GetCurrentWorld() == nullptr`, do nothing more. The ordinary `WorldGenerationComplete` path ingests and spins up.
3. **World identity check.** Build a `HydrologyTerrain` from the loaded world. If `width`, `height` or `basin_count` differs from the save, or both sides have a content hash and they differ, **discard the saved state**: one Warning naming both values, then the ordinary fresh path for the current revision. The load succeeds. This is A4's rule (decided 2026-11-16), and the same choice A4 makes for a dimension mismatch. A save is never allowed to corrupt a world it does not belong to.
4. `sim.Initialise(terrain, params)` then `sim.RestoreCommitted(week, record)`. `RestoreCommitted` overwrites the stores, the biome array and the basin volumes, then calls `RederiveCommitted()`. A failing `Initialise` takes step 3's discard path.
5. `mActive = sim`; `mActiveRevision = mTargetRevision = the current world revision`; `mRestoredRevision` = that revision. `IsReady(revision)` is true at once, with **no spin-up**.
6. Compare `params_hash` with the current effective parameters. A mismatch logs one Warning ("saved under different hydrology parameters") and sets Degraded until the next `QueueParameterOverrides` or reload applies. The run continues under the **current** tuning. Water already stored is not rescaled. A stricter option is not built (Open Design Question 2).
7. **Idempotent re-announce.** A later `WorldGenerationComplete` whose revision equals `mRestoredRevision` is ignored (the load itself caused it). A greater revision is a real regeneration and ingests and spins up as usual. If the shared E1 + S-1 event-order spike (A4 Feature 4's first plan task) finds the bad order, this feature takes the same committed fallback: stash the parsed, validated record and apply it on the next matching `WorldGenerationComplete`. The two participants share one test.

After a restore the committed state is exactly what was saved. `GetSnapshot().GetWeek()` is `week`. The stage's next step takes its week from the coordinator's key as usual.

**Two participants, one world.** A4 and A5 restore independently, so three mixed cases exist. All are defined, none corrupt either system:

| A4 | A5 | What happens |
|---|---|---|
| in the save | in the save | Both restore. No spin-up for either |
| in the save | **absent** from the save (an older save, or hydrology was not hosted) | A5 loads **fresh** from the world (A4's rule for an absent participant). A4 is already restored, so there is no coupled job: A5 takes Feature 3's seed-only path and goes Degraded |
| absent | in the save | A5 restores. A4 starts its job; `BeginSpinUp(revision)` returns null because `mRestoredRevision == revision`, so A4 spins up **alone** |

### Pending result

The stage implements `SerializePending(ctx)` (called after the scheduler's `DrainAll`, so `Run` has finished) and `DeserializePending(ctx)`. The module wraps this stage in its own `stage_N` object (World Step Feature 3 amendment 4), so the stage writes at the current node:

```json
{ "pending_version": 1, "valid": true, "step_day": 412,
  "cell_count": 6656, "basin_count": 9,
  "totals": { "in": 0.0, "sea_out": 0.0, "et_out": 0.0, "draw_out": 0.0, "draw_unmet": 0.0,
              "basin_evap_out": 0.0, "fixed_lake_makeup": 0.0, "storage": 0.0, "error": 0.0 },
  "surface_water": [...], ..., "biome": [...], "basin_volume": [...] }
```

- It is the sim's **working** set (`WorkingFields()`, the working biome array and basin volumes, plus the step's `WaterTotals`), exactly what `Commit` would swap in. The encoding is the same codec as the committed record, so there is one code path. The apply key and ordering are the scheduler's and the coordinator's.
- **`step_day`** is the day the step represents. The stage's `Commit()` takes no argument, so it needs the day to set `WeekOf(stepDay)`. The coordinator keeps its own copy, but the stage cannot read it. This is why the stage saves its own.
- **Totals** (`WaterTotals`, doubles) go with it so the balance gauge and health after the restored commit match the uninterrupted run. Integer counters in `WaterTotals` are written as integers.
- **No latched inputs are saved.** The pending set is a completed result. It is applied, not re-run (DFS-005, DiaDeferredStep AC 21). The effects of the sinks, influences and source view are already in the arrays.
- **A discarded or invalid step still writes a blob.** If the stage has no valid completed set (the step's input view was invalid, or its revision no longer matches the active revision), `SerializePending` writes `{ "pending_version": 1, "valid": false }`. World Step's `Validate` refuses a save that lacks a blob for any stage in `order`, so a stage that wrote nothing would make its own save unloadable. `DeserializePending` of a `valid: false` blob restores the "discard" marker, so the restored `Apply()` is a logged no-op with no `HydrologyUpdated`.
- `DeserializePending` runs after this participant's `Deserialize`, so `mActive` exists. It validates `cell_count` and `basin_count` against `mActive`, applies `RestoreCompleted(record)` (which re-derives the working set's level, `openWater` and `passability`), sets `mCapturedDay = step_day`, `mStepValid = true`, and `mCaptureRevision = mActiveRevision`.
- `ValidatePending` delegates to the same consistency checks as `Validate` (no live-state comparison).
- **If the pending result cannot be restored** (no `mActive` because step 3 discarded the save, or a count mismatch): the stage returns not-ready, logs an Error, and the coordinator's `RestoreRan` fails. The scheduler's channel still reaches its apply key, and `WorldStepModule::Apply` tolerates `chain.Commit() == false` (World Step Feature 3 AC 20a): one log, no signal.

### New simulation surface

`RestoreCommitted(uint64_t week, const HydrologyStateRecord&)`, `RestoreCompleted(const HydrologyStateRecord&, const WaterTotals&)`, `RederiveCommitted()`, `GetWorkingBasins()`, `GetWorkingTotals()`. All validate sizes and return false on a mismatch without changing state.

## Public API

```cpp
namespace CoW::Hydrology {

    struct HydrologyStateRecord {                 // the decoded form of the JSON above
        uint16_t width = 0, height = 0;  uint32_t cellCount = 0, basinCount = 0;
        int64_t  week = 0;               uint64_t paramsHash = 0;
        bool     hasWorldHash = false;   uint64_t worldHash = 0;
        HydrologyFields         fields;           // the saved set + riverFlow + flooded
        DynamicArrayC<uint8_t>  biome;
        DynamicArrayC<float>    basinVolume;
    };

    // Pure codec. Files: Cluiche/CoW/Shared/Hydrology/HydrologyStateCodec.h/.cpp
    bool     EncodeState(const HydrologyStateRecord&, Json::Value& out);
    bool     DecodeState(const Json::Value&, HydrologyStateRecord& out, Dia::Core::Containers::String256& err);  // all checks
    bool     EncodePending(const HydrologyStateRecord&, const WaterTotals&, uint64_t stepDay, Json::Value& out);
    bool     DecodePending(const Json::Value&, HydrologyStateRecord& out, WaterTotals& totals,
                           uint64_t& stepDay, bool& valid, Dia::Core::Containers::String256& err);
    uint64_t HashHydrologyParameters(const HydrologyParameters&);

    // Cluiche/CoW/Modules/HydrologySaveParticipant.h/.cpp: a thin ISaveable over the codec and HydrologyModule
    class HydrologySaveParticipant final : public Dia::SaveGame::ISaveable {
    public:
        static const uint32_t kFormatVersion = 1;
        void     Serialize  (Dia::SaveGame::SaveContext&) const override;
        void     Deserialize(Dia::SaveGame::LoadContext&) override;
        uint32_t GetVersion () const override;
        bool     Validate   (Dia::SaveGame::LoadContext&) const override;
    };
}
```

The JSON dependency stays at the codec boundary. The record uses `DynamicArrayC`, no STL (PD-004).

## Acceptance Criteria

### Codec (pure)
1. A `HydrologyStateRecord` encodes and decodes **bit-identically** for random finite floats, for `0`, for denormals, for values at each range bound, and for `float::max()`-scale values. `-0.0` encodes as `+0.0`. `uint8_t` and `uint16_t` arrays round-trip at their limits.
2. Decoding rejects (false, nothing written), naming the key: wrong array length, a missing array, a non-number element, a NaN/Inf literal, a negative float, `season_min_snow` outside `[0,1]`, a counter outside `0..65535`, a flag not 0 or 1, a biome ≥ 13, `cell_count != width*height`, `cell_count` > 65,536, `basin_count` > 4,096, `basin_volume` length mismatch, `week` < 0, `version` 0 and 2.
3. `HashHydrologyParameters` is deterministic, changes when any one scalar or any one biome-table value changes by one ULP, and is identical for two equal sets.
4. `EncodePending`/`DecodePending` round-trip the working set, the `WaterTotals` (all doubles bit-exact, counters exact) and `step_day`. A `valid: false` blob decodes with `valid == false` and nothing else required.

### Participant
5. `ready: false` saves and loads as a no-op, and the next `WorldGenerationComplete` ingests and spins up normally.
6. Save, load into a fresh module with the same world, run N weeks: committed fields bit-identical to an uninterrupted run, checked at N = 1, 13 and 52. Covers the fixed driver, and a fake A4 source with scripted outputs.
7. A successful restore starts **no** spin-up job and `IsReady(rev)` is true immediately.
8. `Validate` returns false for every defect in criterion 2, plus a missing top-level key and a negative store, and **mutates nothing** (fields, week, active sim, queue unchanged).
9. A failed `Validate` makes `SaveManager::Load` abort with a deserialize error. The live hydrology state is exactly as before.
10. A dimension mismatch, a `basin_count` mismatch or (when both exist) a content-hash mismatch against the loaded world discards the saved state. One Warning names both values, a fresh ingest and spin-up start for the current revision, and the load succeeds.
11. A save from a world whose terrain `Initialise` rejects takes the discard path, not a crash.
12. `params_hash` equal: no warning. Unequal: exactly one Warning, Degraded, the run continues under the current parameters, and a following parameter apply clears Degraded. Stored water is not rescaled.
13. The `WorldGenerationComplete` caused by the load itself (revision equal to `mRestoredRevision`) is ignored. A greater revision ingests and spins up.
14. Without `SaveLoadModule` the module runs and registers nothing. With it, the participant is registered after S-1's and before DeferredStep's (an ordering assertion on a fake registry).
15. The restored week is the week saved. The first step after the load publishes `WeekOf(stepDay)` from the coordinator's key.
16. **Readers after a load.** After a restore, the snapshot's `RiverFlow`, `Flooded`, `OpenWater`, `PassabilityAt`, `SoilMoisture`, `SurfaceWetness`, `GlacierEligible` and basin levels equal the values the uninterrupted run holds at the same point. A4's `IHydrologyInputsProvider` copy equals too.
17. The biome array comes from the save. A cell A3 changed from Grassland to Wetland loads as Wetland, and the next step uses Wetland's capacities, even though S-1's `Cell::biome` says Grassland.
18. **Mixed cases.** A save with A5 absent loads A5 fresh. With a fake restored A4 it takes Feature 3's seed-only path and is Degraded. A save with A5 present and A4 absent restores A5, and `BeginSpinUp` returns null for that revision.
19. A restore while a coupled ticket from A4's job is outstanding: the later `CompleteSpinUp` for the old ticket is ignored and the restored state is untouched.

### Pending result
20. Save mid-step (after `Run`, before `Commit`), load, apply at the original key: committed fields after the apply, and after N further weeks, are bit-identical to the uninterrupted run. This is the DFS-005 test, covered once at stage level and once through the real coordinator with a fake `weather` stage (and with a fake `ecology` stage after A5, to prove A5's blob sits correctly between them).
21. The restored step publishes `HydrologyUpdated` with `WeekOf(step_day)`. `step_day` is restored from the blob, not from live state.
22. `SerializePending` writes the working set after `DrainAll`, and the committed set is unchanged by it.
23. A pending blob with a wrong `cell_count` or `basin_count`, a non-finite value, a bad biome, or a missing `step_day` fails `ValidatePending` and nothing changes.
24. A stage with no valid completed step (invalid input view, or revision no longer matching) writes `{"pending_version":1,"valid":false}`. The save passes World Step's missing-blob check. After a load, `Apply()` is a logged no-op with no `HydrologyUpdated`, and the next kick works.
25. With no `mActive` (the discard path), `DeserializePending` returns not-ready with an Error. The later `Apply()` of the restored channel is a logged no-op, with no crash, and the next kick works.
26. A save with a step in flight whose pending blob came from sinks or influences latched at `Capture` loads and applies the same result with **no** provider, source or influence present at load time (nothing latched is needed).
27. The save taken with the hydrology stage not in `order` (S-2.5) writes no hydrology blob. A save taken with it and loaded where it is not in `order` skips the blob with World Step's single Warning (the coordinator's rule; one test through the real coordinator).

### Size and budget
28. At Large (6,656 cells) the hydrology record plus a pending blob is under 8 MB of JSON. A full `CaptureState()` of hydrology is reported in Release and not gated in Debug.

## Files Touched

| File | Change |
|---|---|
| `Cluiche/CoW/Shared/Hydrology/HydrologyStateCodec.h/.cpp` | New (pure) |
| `Cluiche/CoW/Modules/HydrologySaveParticipant.h/.cpp` | New |
| `Cluiche/CoW/Shared/Hydrology/HydrologySimulation.h/.cpp` | Edit: `RestoreCommitted`, `RestoreCompleted`, `RederiveCommitted`, `GetWorkingBasins`, `GetWorkingTotals` |
| `Cluiche/CoW/Modules/HydrologyModule.h/.cpp`, `HydrologyStage.h/.cpp` | Edit: registration, `OnLoadRestore`, `mRestoredRevision`, `SerializePending`/`DeserializePending`/`ValidatePending` |
| `Cluiche/Tests/GoogleTests/CoW/Hydrology/{TestHydrologyStateCodec,TestHydrologySaveParticipant,TestHydrologyPendingSave}.cpp` | New |
| `CoW.vcxproj`, `GoogleTests.vcxproj` (+ filters) | Via `dia docs vcxproj-add` |

**Spec edits owed on approval:**
- A5 Feature 1: `biome`, `riverFlow` and `flooded` join the "saved" list that Feature 4 iterates. The constant list now has 15 per-cell entries.
- The parent spec's Features table row 4.
- World Step Feature 3: the third stage blob (already noted there).
- A4 Feature 4: see "Uncertain items" in Status (it has the same `step_day` gap and the discard-blob conflict).

**No Dia files are added or edited, so no AD-001 sign-off.** E1 needs no change.

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| PD-004 | No STL containers in public APIs | The record and codec use `DynamicArrayC`. jsoncpp is behind the codec boundary and `SaveContext` |
| PD-005 | x64 is the only target | Saves are JSON text with 17-digit floats, independent of byte order |
| AD-001 | Ask permission before Dia edits | None. `ISaveable`, `SaveContext` array writes through `CurrentNode`, `RegisterParticipant` and `CaptureState` are used as built |
| AD-003 | GDD owns design intent | Draft until GDD A5 is locked. The save format is an engineering consequence, with the differences below |
| DFS-005 | Saves drain, then store pending results with apply keys; steps are not re-run on load | The stage's blob is the completed working set; the scheduler stores the keys; ACs 20–26 prove no re-run and no dependence on latched inputs |
| DFS-002 | `Run` reads only the captured working set | `DeserializePending` restores a finished set. No source or provider is touched at load |
| E1 | Participant order is serialization order | Registered after S-1's and before DiaDeferredStep's; AC 14 |
| World Step Feature 3 | A missing blob for a stage in `order` refuses the load | A stage that has nothing to store still writes a `valid: false` blob (AC 24) |

## GDD differences

| Topic | GDD draft | This spec |
|---|---|---|
| Save contents | `serialize_state()` of cells, basins, counters and the pending working set | The Feature 1 saved list plus `biome`, `riverFlow`, `flooded` and basin volumes. No `channel_store`, `pending_spill` or basin `ice_fraction` exist in this design. `low_flow_weeks` and `glacier_weeks` are not in Features 1–2 (see Status) |
| Entry points | `serialize_state()` / `restore_state()`, `GameLoaded` | An E1 `ISaveable` participant `cow.hydrology`; the pending blob through DiaDeferredStep |
| Changed parameters (EC-22) | State loads as saved; a changed `BASIN_DEPTH_SCALE` recomputes level; volume above capacity is handled | `params_hash` mismatch warns and Degrades. Volume above a changed capacity is Feature 2's concern at the next step (it spills at the spill height). No rescale |
| Old world files (EC-15) | Rejected | A save is matched to the loaded world. A world file of version 1 works with `basinsFromLakesOnly` (parent decision) |
| Latched state | The pending working set | The same. No latched inputs are saved (they are not needed once the step has run) |

## Test Plan

TDD, RED then GREEN, in this order:
1. Pure codec tests (no Dia save machinery): round trip, rejections, the hash.
2. `Validate` against a real `SaveContext`/`LoadContext` round trip through `Flush` and parse. The float-fidelity risk is in the text, so no mocks there.
3. `Deserialize` happy path.
4. Identity, mismatch and hash paths, and the mixed A4 cases with a fake source.
5. Reader equality after a load (AC 16–17).
6. Pending: stage level, then through the real coordinator with fake neighbours, and the `valid: false` blob.
7. One test through a real `SaveManager` with `SaveLoadModule::CaptureState()`.
8. Size.

Layer goldens are not used here; state identity is asserted bitwise. Scoped `--filter=*Hydrology*` while iterating, one full `--shards 2` run at the end. No mutation checks. The sanitizer runs only if asked. Gap audit and edge pass is the last plan task. **Observation scan** at verify (expected finds: a save-duration trace zone, a Warning on mismatch).

## Open Design Questions

1. **Saving `riverFlow` and `flooded` goes beyond "the saved list".** The alternative is to save only the stores and let readers see zero flow and no floods until the first step after a load. That is smaller (two arrays) but breaks "readers see the same values after a load" (Goal 7) for any save between two kicks, and it makes A1's seasonal means differ from the uninterrupted run. Chosen: save them. If you would rather keep Feature 1's list exactly, AC 16 must be relaxed to "after the first applied step".
2. **Is a parameter-hash warning enough for S-3.5 R2?** The same question as A4. R2 loads under whatever tuning the bundle supplies, so its bit-identity claim holds only under matching tuning. A `strict` load option that refuses on a mismatch is the upgrade. Not built now.
3. **Discard versus refuse on a world mismatch.** The request said "refuse". A4's rule is to discard the saved state, spin up fresh and let the load succeed, and this spec follows A4. The cost is a silent-looking reset of water (a Warning is logged). The alternative is to abort the whole load, which also throws away S-1's and A4's matching state. If you want the stricter behaviour, it is a one-line change in step 3 plus AC 10.

## Status

**Status:** Draft (2026-11-17). Decisions encoded from the user session the same day:
- E1 `ISaveable` participant `cow.hydrology`; JSON text arrays (floats as 17-digit doubles, as A4).
- It saves Feature 1's saved list, basin volumes and the counters, plus the three additions above.
- The pending working set goes through `SerializePending`/`DeserializePending`, with the scheduler's apply key (DFS-005), so a reload continues bit-identically including a step in flight.
- A world identity check on dimensions, basin count and content hash (when available).
- A version field. An unknown version is refused with a reason.
- A save with hydrology absent loads hydrology fresh from the world, and World Step's missing-blob refusal applies.

**Uncertain items found while writing** (not guessed; see the report):
- The request says to save "the latched sink/influence state needed to re-run a pending step". DFS-005 forbids re-running, and a pending step is already complete, so this spec saves **none** of it (Non-Goals, AC 26).
- A pending blob needs `step_day` so `Commit` can set the week. A4's `weather-save.md` pending record has no such field, and its `Commit()` has the same need.
- A4 Feature 4 AC 19 ("a step whose revision no longer matches is not written as pending") conflicts with World Step's missing-blob refusal. This spec writes a `valid: false` blob instead.
- `world_hash` depends on an unbuilt F-3 seam. Dimensions and `basin_count` carry the check until then.
- `low_flow_weeks` and `glacier_weeks` (the GDD's counters for the dry-riverbed and glaciation rules) are in neither Feature 1 nor Feature 2. If A3 needs them from A5, they must join the saved list.

Approval waits on GDD A5's lock. No plan yet.

# Feature Spec: A4 Weather Save/Load

**Parent:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.md (Feature 4)
**Status:** Approved (2026-11-16)
**Depends on:** A4 Features 1, 2, 3 (`WeatherFields`, `WeatherSimulation`, `WeatherModule`, the stage); World Step Feature 3 (the coordinator delegates `SerializePending`/`DeserializePending` to this stage and calls `ValidatePending`); DiaDeferredStep Feature 3; E1 `SaveLoadModule::RegisterParticipant` and `CaptureState()` (built).
**Consumed by:** S-3.5 R2/R3 (save/load determinism, snapshot restore), every stage that saves with weather running.

## Summary

Two things, both pure data with no new mechanics:

1. **`WeatherSaveParticipant`**, an E1 `ISaveable` that writes the committed `WeatherFields`, the cached biome array and the applied week as JSON, and restores them into a fresh simulation on load without re-running the spin-up.
2. **The stage's `SerializePending` / `DeserializePending`**, which write and restore a completed-but-unapplied working set so a save taken mid-step reloads identically to the uninterrupted run (DFS-005).

Round trip is bit-identical for finite floats. Terrain is never saved: it comes back from S-1.

## Goals

1. Save, load, continue for N weeks gives committed fields bit-identical to a run that never saved (with no active forcings).
2. A save taken mid-step restores the pending working set and applies it at its original key.
3. A corrupt or inconsistent weather save aborts the load in `Validate`, before any state changes.
4. A save that does not match the loaded world never corrupts A4: it is discarded and the world spins up fresh.
5. Loading never re-runs the spin-up when the save matches the world.
6. Tuning that differs from the saved run is **visible** (decided 2026-11-16), not silent.

## Non-Goals

- The scheduler's drain, the pending-record envelope, stage-id tags and `ValidatePending` plumbing (DiaDeferredStep Feature 3, World Step Feature 3).
- Saving forcings, wind overrides, the mutation queue or presets. They are debug tools (system spec). A run with an active forcing at save time is **not** guaranteed bit-identical after load, and the spec says so.
- Saving parameters. They live in the file and F-3 packs and bundles.
- Binary format (`SaveFormat::Binary` does not exist; the 2026-10-15 decision rejected a blob).

## Design

### Participant registration

`SaveLoadModule::RegisterParticipant(StringCRC("weather"), &mParticipant)`, with `WeatherModule` holding `SaveLoadModule` as a **soft** dependency: absent (S-2.5), A4 runs and does not participate. Registration order is serialization order: the participant registers **after** S-1's (the world must be restored before weather is rebuilt) and **before** DiaDeferredStep's participant (the pending result applies on top of committed state; see World Step Feature 3). `WeatherModule` registers in `DoStart` and unregisters in `DoStop`.

### Record (version 1)

```json
"weather": {
  "version": 1,
  "ready": true,
  "width": 104, "height": 64, "cell_count": 6656,
  "week": 412,
  "params_hash": 7712930193384,
  "moisture": [0.41, ...], "temperature": [...], "pressure": [...],
  "wind_direction": [...], "wind_speed": [...], "precipitation": [...],
  "cloud_cover": [...], "water_to_ground": [...], "snow_pack": [...],   // ground_water renamed: amended 2026-11-17, A5 Hydrology
  "biome": [2, 2, 4, ...]
}
```

- Nine float arrays (`water_to_ground` replaces `ground_water`; `glacierInput` and A5's latched inputs are not saved; amended 2026-11-17, A5 Hydrology), one entry per cell, row-major (`index = y * width + x`), written with `BeginArray`/`CurrentNode().append(double)` because `SaveContext` has no array-element writer. `Write(float)` and the `double` form both round-trip a `float` exactly (`StyledWriter` emits 17 significant digits). A test proves the bit-identity.
- `biome` is the **A4-cached** biome per cell as `uint8_t`, including A3's succession changes, so it is authoritative for A4 on load (the S-1 `Cell::biome` is only the ingest source).
- `week` is the last applied week (`WeekOf` of the last committed step day) as `int64_t`.
- `ready: false` writes only `version` and `ready` (a save taken before the first world is spun up). It restores as a no-op.
- `params_hash` is a 64-bit FNV-1a over the **bit patterns** of the effective `WeatherParameters` in descriptor-table order (`HashWeatherParameters`, new in this feature, pure).
- Ground water itself is saved by A5's participant, not here. No migration is needed: A4 is not built yet, so no `ground_water` save exists. Size at Large: about 2 MB (9 × 6,656 floats at roughly 32 bytes of styled JSON each). Well inside `SaveContext::kBufferSize`.
- Floats are never NaN/Inf (Finish clamps, EC-20), so JSON never has to carry them. `-0.0` is normalised to `+0.0` on write (jsoncpp reads `-0` as the integer 0).

### `Validate` (all-or-nothing, mutates nothing)

Runs before any participant deserializes, so it checks **internal consistency only**: the live world is still the old one and cannot be compared. Returns false, logging an Error with the key, when:

- `version` is missing or greater than 1;
- `ready` is true and any of `width`, `height`, `cell_count`, `week`, `params_hash`, or an array is missing or the wrong type;
- `cell_count != width * height`, `width` or `height` is 0, or `cell_count` exceeds 65,536 (a corrupt-count guard);
- any of the ten arrays has a length other than `cell_count`;
- a float is non-finite or outside its field range (`[0,1]`, `windDirection` in `[0, 2π)`);
- a `biome` value is `>= 13`;
- `week < 0`.

`ready: false` is valid with nothing else present.

### `Deserialize`

SimPU, after every participant validated, in this order:

1. **Cancel** any running spin-up job and drop `mPending` (the module state from before the load is dead).
2. If `ready` is false, or the loaded world is missing (`GetCurrentWorld() == nullptr`), do nothing more: the ordinary `WorldGenerationComplete` path spins up.
3. Build a `WeatherTerrain` from the loaded world. If its `width`/`height` differ from the save's, **discard the saved state** (decided 2026-11-16): log a Warning naming both sizes, and start the ordinary spin-up for the current revision. The load succeeds.
4. `sim.Initialise(terrain, params, baseline)` then `sim.RestoreCommitted(week, fields, biomes)` (this feature adds the biome argument). If `Initialise` fails (EC-01), treat as step 3's discard.
5. `mActive = sim`, `mActiveRevision = mTargetRevision = current world revision`, `mRestoredRevision = that revision`. `IsReady(revision)` is true at once, with no spin-up.
6. Compare `params_hash` with the current effective parameters. A mismatch logs one Warning ("saved under different weather parameters") and sets Degraded until the next `QueueParameterOverrides`/reload is applied; the run continues under the **current** tuning (decided 2026-11-16).
7. **Idempotent re-announce.** A later `WorldGenerationComplete` whose revision equals `mRestoredRevision` is ignored (the load itself caused it). A greater revision is a real regeneration and spins up as usual. The first plan task proves the real E1 + S-1 event order (Open Design Question 1).

After a restore the committed state is exactly what was saved, `GetSnapshot().GetWeek()` is `week`, and the stage's next step takes its season and week from the coordinator's key as usual.

### Pending result

The stage implements `SerializePending(ctx)` (called after the scheduler's `DrainAll`, so the step has completed and `Run` has finished) and `DeserializePending(ctx)`:

```json
{ "pending_version": 1, "cell_count": 6656,
  "moisture": [...], ... 9 arrays, "biome": [...] }
```

- Amended 2026-11-17 (A5 Hydrology): the pending record holds the nine `WeatherFields` arrays only. `glacierInput` is not saved; A5's own pending record carries its consequence, and both are drained together as one world step.
- It is the sim's **working** set (`GetWorkingFields()`/`GetWorkingBiomes()`, new read accessors), exactly what `Commit` would swap in. The step day, apply key and ordering are the coordinator's and the scheduler's.
- `DeserializePending` runs after the weather participant's `Deserialize` (ordering rule above), so `mActive` exists. It validates `cell_count == mActive cell count` and ranges, then calls `sim.RestoreCompleted(fields, biomes)`, which puts the sim in the "Run finished" state so `Commit` works, and sets `mCaptureRevision = mActiveRevision`.
- `ValidatePending` (World Step Feature 3) delegates here for the same checks as `Validate` (consistency only, no live-state comparison).
- **If the pending result cannot be restored** (no `mActive` because step 3 discarded the save, or a cell-count mismatch): the stage returns not-ready, logs an Error, and the coordinator's `RestoreRan` fails. The scheduler's restored channel still reaches its apply key, and `WorldStepModule::Apply` must tolerate `chain.Commit() == false` (log once, no signal, no message). **Amendment to World Step Feature 3.**
- Forcings latched into the captured step were already applied inside `Run`; the committed forcing timers are not saved, so after load no forcing remains (Non-Goals).

### New simulation surface

`RestoreCommitted(uint64_t week, const WeatherFields&, const DynamicArrayC<uint8_t>& biomes)` (extends the system spec's two-argument form), `RestoreCompleted(const WeatherFields&, const DynamicArrayC<uint8_t>& biomes)`, `GetWorkingFields()`, `GetWorkingBiomes()`. All validate sizes and return false on mismatch without changing state.

## Public API

New: `Cluiche/CoW/Shared/Weather/WeatherStateCodec.h/.cpp` (pure: fields and biomes to and from a `Json::Value`, the range and consistency checks, `HashWeatherParameters`), `Cluiche/CoW/Modules/WeatherSaveParticipant.h/.cpp` (thin `ISaveable` over the codec and `WeatherModule`). Edited: `WeatherSimulation` (the four methods above), `WeatherModule` (registration, `OnLoadRestore`, `mRestoredRevision`), `WeatherStage` (`SerializePending`/`DeserializePending`/`ValidatePending`).

## Acceptance Criteria

### Codec (pure)
1. A `WeatherFields` + biomes encode/decode round trip is **bit-identical** for random finite floats, for `0`, for denormals, for values at each range bound, and for `float::max()`-scale values; and `-0.0` encodes as `+0.0`.
2. Decoding rejects (false, nothing written) each of: wrong array length, a missing array, a non-number element, a NaN/Inf literal, a float outside its field range, a biome ≥ 13, `cell_count != width*height`, `cell_count` > 65,536, `week` < 0, `version` 0 and 2.
3. `HashWeatherParameters` is deterministic, changes when any one parameter changes by one ULP, and is identical for two equal sets.

### Participant
4. `ready: false` saves and loads as a no-op, and the next `WorldGenerationComplete` spins up normally.
5. Save, load into a fresh module with the same world, run N weeks: committed fields bit-identical to an uninterrupted run (no forcings), checked at N = 1, 13 and 52.
6. A successful restore does **not** start a spin-up job and `IsReady(rev)` is true immediately.
7. `Validate` returns false for every defect in criterion 2 and for a missing top-level key, **and mutates nothing** (fields, week, active sim, queue unchanged).
8. A failed `Validate` makes `SaveManager::Load` abort with a deserialize error and the live weather state is exactly as before.
9. A dimension mismatch against the loaded world discards the saved state, logs one Warning naming both sizes, starts a spin-up for the current revision, and the load succeeds.
10. A save from a world with no water-source cell in the loaded terrain takes the discard path (EC-01), not a crash.
11. `params_hash` equal: no warning. Unequal: exactly one Warning, Degraded, run continues under the current parameters; a following parameter apply clears Degraded.
12. The `WorldGenerationComplete` caused by the load itself (revision equal to `mRestoredRevision`) is ignored; one with a greater revision spins up.
13. Without `SaveLoadModule` the module runs and registers nothing; with it, the participant is registered after S-1's and before DeferredStep's (an ordering assertion on a fake registry).
14. The week restored is the week saved; the first step after the load publishes `WeekOf(stepDay)` from the coordinator's key and the history signal is unaffected.

### Pending result
15. Save mid-step (after `Run`, before `Commit`), load, apply at the original key: committed fields after the apply and after N further weeks are bit-identical to the uninterrupted run (the DFS-005 test, covered once at stage level and once through the real coordinator with fake neighbours).
16. `SerializePending` writes the working set after `DrainAll` and the committed set is unchanged by it.
17. A pending blob with a wrong `cell_count`, a non-finite value or a bad biome fails `ValidatePending` and nothing changes.
18. With no `mActive` (the discard path), `DeserializePending` returns not-ready with an Error and the later `Apply()` of the restored channel is a logged no-op: no `WeatherUpdated`, no crash, and the next kick works.
19. A save with a step in flight whose revision no longer matches the active revision is not written as pending (the discard guard from Feature 3 applies at save time too).

### Size and budget
20. At Large (6,656 cells) the weather record plus a pending blob is under 5 MB of JSON, and a full `CaptureState()` of weather runs in under 150 ms in Release (reported, not gated in Debug).

## Files Touched

New: the codec and participant pairs; `Tests/GoogleTests/CoW/Weather/{TestWeatherStateCodec,TestWeatherSaveParticipant,TestWeatherPendingSave}.cpp`. Edited: `WeatherSimulation`, `WeatherModule`, `WeatherStage`, vcxproj via `dia docs vcxproj-add`. **Spec edits on approval:** World Step Feature 3 (`Apply` tolerates `chain.Commit() == false`; it already lists the participant-order rule, which this feature confirms); the system spec's Save/load section (replace the stale parameter and spin-up text with this record and the mismatch and hash rules). No Dia files are added or edited, and E1 needs no change.

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| PD-004 | No STL in public APIs | The codec uses `DynamicArrayC` and jsoncpp only at the boundary |
| PD-005 | x64 is the only target | Saves are JSON text with 17-digit floats, little-endian-independent |
| AD-001 | Ask permission before Dia edits | None. `ISaveable`, `SaveContext` array writes through `CurrentNode`, `RegisterParticipant` and `CaptureState` are used as built |
| DFS-005 | Saves drain, then store pending results with their apply keys | The stage's blob is the working set; the scheduler stores keys; criteria 15–18 |
| E1 | Participant order is serialization order | Registered after S-1 and before DiaDeferredStep; asserted in criterion 13 |

## Test Plan

Pure codec tests first (no Dia save machinery), then the participant against a real `SaveContext`/`LoadContext` round trip through `Flush` and parse (the float-fidelity risk is in the text, so no mocks there), then the module paths with the fake world source and synchronous launcher from Feature 3, then one test through a real `SaveManager` with `SaveLoadModule::CaptureState()`. TDD order: codec, validate, deserialize happy path, mismatch and hash paths, pending, size. Layer goldens are not used here (state identity is asserted bitwise). Scoped `--filter` while iterating, one full `--shards >= 2` run at the end. No mutation checks. Gap audit and edge pass is the last plan task. **Observation scan** at verify (a Save-duration trace zone and a Warning on mismatch are the expected finds).

## Open Design Questions

1. **What order do E1's load and S-1's `WorldGenerationComplete` really arrive in?** Criterion 12 and step 7 assume the load causes at most one completion at a revision `A4` can see when it deserializes. If S-1 publishes its completion **after** A4's `Deserialize` with a revision A4 has not recorded, A4 will re-spin-up and overwrite the restored state, silently. If it arrives before, the stale-revision guard works. The first plan task is an integration test over the real E1 + S-1 sequence; if the order is the bad one, the fallback is to have the weather participant defer restoring until the completion for the loaded revision arrives (stash the parsed state, apply on completion).
2. **Is a parameter-hash warning enough for S-3.5 R2?** R2 (save/load determinism) loads under whatever tuning the bundle supplies; a deliberate pack change between save and load will warn, but R2's bit-identity claim holds only under matching tuning. If S-3.5 needs the stronger guarantee, add a `strict` load option that refuses on mismatch. Not built now.
3. **Should the pending blob be skipped when the committed state is identical to the pending one?** At weekly cadence the pending working set differs from committed on every cell, so there is nothing to skip; mentioned only because the combined record is about 4 MB at Large and a faster cadence would double it.

## Gap-pass amendments (2026-11-16)

Found by the plan-writing pass; each item below **supersedes** any contradicting text above. Numbering is stable.

1. **`Validate` and `version`.** `version` missing, `0` or greater than 1 fails. `week` is `int64_t` in the record and is range-checked (`>= 0`) in the codec; the codec casts to `uint64_t` only after that check, and passes the unsigned value to `SetCommittedWeek`.
2. **`cow.weather_state` envelope (also used by Feature 5's `state export`/`import`).** `{"format":"cow.weather_state","version":1, ...}` followed by the **same fields as the save record** (`ready` is always true; `width`, `height`, `cell_count`, `week`, `params_hash`, the nine arrays, `biome`). The codec exposes `EncodeState(sim, out)` and `DecodeState(json, record)` for both the participant and the commands. Import additionally requires matching dimensions against the live world.
3. **Event-order fallback is the committed design if the first-task spike finds the bad order.** The participant stashes the parsed, validated record and applies it on the **next** `WorldGenerationComplete` after the load (instead of spinning up) provided its dimensions match; a mismatching completion discards the stash with a Warning and spins up. The stash is dropped on `DoStop` and on any later regeneration.
4. **Participant order.** The weather participant registers in `WeatherModule::DoStart`. The DeferredStep participant attaches on `DeferredStepModule`'s first `DoUpdate` (World Step Feature 3 amendment 5), so it always follows every stage participant regardless of manifest order. Criterion 13 asserts the resulting order.
5. **`SaveContext` sub-objects for the pending blob** are the coordinator's (`BeginObject("stage_N")` around each stage); this stage writes into the current node.

## Status

Amended 2026-11-17 (user decisions: A5 hand-off, Features 7–9): `ground_water` array renamed `water_to_ground`; see above. Feature 7 (influences) and Feature 8 (cycle phases) will add their own saved records, to be specced with those features.

**Status:** `Approved` (2026-11-16). Plan: [weather-save.plan.md](weather-save.plan.md) (written 2026-11-16, not started).

# Plan: A4 Weather Save/Load (A4 Weather Feature 4)

**Spec:** @docs/specs/applications/cow/systems/a4-weather/weather-save.md
**Status:** Not Started
**System plan:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.plan.md (row 4)

Rebuilt 2026-11-18 against the world-sim review amendment and the spec-6 resolutions. The save codec is now the
World-Sim Kit's (`WorldSimSaveCodec`: record header, arrays at 17 digits with `-0.0` normalised, the validate
checklist, the shared 7-step restore with the event-order fallback, the pending blob shape, the biome palette).
This plan therefore **deletes** the A4-private validate checklist, `HashWeatherParameters`, the hand-ordered
`Deserialize`, the in-record `version`, the A4 stash fallback and the `WorldStepModule::Apply` edit. It adds the
parts that are A4's own: the `SavedField` list, the restore hooks, the `influences` and `climate` blocks, the
`step_day` and `valid:false` pending content, and the `cow.weather_state` export file.

**Start conditions.** Needs these plans Done:
- `weather-module.plan.md` (the stage, `WeatherStateRecord.h`, `StateReplace` entry).
- `worldsim-kit.plan.md` (World Step): the save codec helpers, `SavedField`, the biome palette, `Testing/FakeWorldStage.h`,
  `WorldSimNames.h`.
- `save-and-history.plan.md` (World Step): the coordinator's per-stage pending blob, `ValidatePending` delegation,
  and the `Apply` tolerance for `Commit() == false`.
- `parameter-files.plan.md` (Dia): `ParameterFile::Hash()`.
- `biome-registry.plan.md` (S-1): `BiomeRegistry::ResolveOrFallback`, `Hash()`.
- DiaDeferredStep Feature 3 (participant, `RestorePending`); E1 `SaveLoadModule` and S-1 are built.
- For the real field ranges, the `cow.weather.layers` sidecar entries from `weather-debug.plan.md` (see open item 1).

`grid-search-helpers.plan.md` and `blackboard-scopes.plan.md` are not needed. **Startable earlier on fakes:** the
`SavedField` list, the encode and decode of A4's fields and the palette tests (Tasks 3 to 7) run on the kit's
`FakeWorldStage` and a fixture sidecar as soon as the kit's codec and A4 Feature 1 exist. Task 1 (the real E1 + S-1
order test) needs only E1 and S-1.

No Dia files are added or edited. If a task would need one, mark it `AD-001 ask first`.

## Implementation Patterns

### Files and layout
- New: `Cluiche/CoW/Shared/Weather/WeatherSaveFields.h/.cpp` (the `SavedField[]`, the encode and decode of A4's
  fields over the kit codec, the `influences` and `climate` block codecs, `EncodeState`/`DecodeState`),
  `Cluiche/CoW/Modules/WeatherSaveParticipant.h/.cpp` (thin `ISaveable` registered as `cow.weather`).
- Edited: `WeatherSimulation` (restore surface), `WeatherModule` (registration, restore hooks), `WeatherStage`
  (pending blob content).
- Tests: `Tests/GoogleTests/CoW/Weather/{TestWeatherSaveFields,TestWeatherSaveParticipant,TestWeatherPendingSave}.cpp`.
- `vcxproj` through `dia docs vcxproj-add` only.

### What the kit owns (not rebuilt, not re-tested here)
The header (`ready, width, height, cell_count, step_day, params_hash`), array writing, the generic validate
checklist, the shared restore sequence, the pending blob envelope, `ISaveable::GetVersion` and
`SaveRegistry::RegisterMigration`, and the biome-palette remap. A4 tests only **its own fields** (amendment 5).

### The A4 save record (per the amendment)
- Participant id `cow.weather` from `WorldSimNames.h`. `GetVersion()` returns `kFormatVersion` = 1. There is **no
  in-record `"version"`**; `pending_version` stays in the pending blob.
- Header extras for A4: none beyond the kit's. `params_hash` is `ParameterFile::Hash()`.
- A4's `SavedField[]`: ten `f32` per-cell arrays (`moisture, temperature, pressure, wind_direction, wind_speed,
  precipitation, cloud_cover, water_to_ground, snow_pack, pressure_anomaly`), each pointing at
  `cow.weather.layers/<field>` for its range; any field with no layer entry gives its range in the weather
  state sidecar. `glacierInput`, `influenceWater` and A5's latched inputs are not saved.
- Biomes: the kit's `biome_palette: [ids...]` plus one palette index per cell, remapped on load through
  `ResolveOrFallback` (one Warning per unknown id). The registry hash is recorded; a mismatch with a successful
  remap is one Info. EC-19's 0.5 applies only to `kNoBiome`.
- Blocks A4 adds (kept): `influences { next_id, items[] }` with each item's kind **by catalogue id** (Feature 7), and
  `climate { cycles_hash, cycles[] { id, enabled, phase_days, pinned_phase_days? } }`. Absent blocks load clean
  with no warning. Cycle state is saved as cycle state, not as modifier state (spec-6 resolution 2).
- Validation of those two blocks runs through the same all-or-nothing path; influence validation uses the
  catalogue (an unknown kind id is a defect) and the sidecar defaults of the generic caps, not live parameters.
- Pending blob: `{pending_version: 1, valid, step_day, cell_count, ten arrays, biome palette indices}`; a stage with
  no valid completed step writes `{pending_version: 1, valid: false}`. `step_day` restores `mCapturedDay`.
- Export file `cow.weather_state` keeps the F-3 file envelope (`format`, `version`) because it is a file; its
  fields are the save record's without the `influences` and `climate` blocks. `EncodeState`/`DecodeState` are shared
  by the participant and Feature 5's `state export`/`import`.

### Restore hooks (the kit runs the sequence)
A4 supplies `InitialiseSim` (Feature 3) and `RestoreCommitted(record)`; plus the A4-only restore of the influence
book and the cycle phases. `WeatherSimulation` gains `RestoreCommitted(week, fields, biomes)`, `RestoreCompleted(
fields, biomes)`, `GetWorkingFields()`, `GetWorkingBiomes()`, each validating sizes and returning false with no
state change.

### Verification cadence
- Per task: narrow `dia run googletest --filter="<suite under work>" --shards 2` (always `--shards 2`); the
  three-suite filter runs only in the scoped-verification task. The full suite runs once in worldstep.plan.md,
  Weather-stack close-out. No sanitizer unless asked. No `SLOW_` suites. No mutation checks. The AC 20 timing is a
  Debug ballpark only. Float fidelity is tested through real JSON text, never mocks.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Prerequisite check, read only: the kit's `WorldSimSaveCodec` (`SavedField`, `WriteRecord`, `Validate`, restore driver, pending blob helpers, palette remap), `ISaveable::GetVersion` plus `SaveRegistry::RegisterMigration`, `ParameterFile::Hash()`, `BiomeRegistry::Hash`/`ResolveOrFallback`, `FakeWorldStage`, `WorldSimNames.h` (save id builder), `ValidatePending` on `IWorldStepStage`, the `Apply` tolerance for `Commit() == false`, `WeatherStateRecord.h`. Record which `cow.weather.layers/<field>` sidecar entries exist (and which of the ten fields have none) | Findings recorded in notes | Not Started | haiku | Read-only, TDD-exempt. If a named API is missing, report which plan is not Done; do not reimplement it |
| 1 | **Event-order integration test.** Over the real E1 `SaveManager::Load` and S-1 world publication, record the order and revisions of probe `Validate`, probe `Deserialize` and `WorldGenerationComplete`, and what `GetCurrentWorld()` returns at `Deserialize` time; assert the observed order | A named integration test that fails loudly if E1 or S-1 reorders | Not Started | sonnet | Resolves system open item F4 ODQ 1. The stash-then-apply fallback is the kit's shared restore; this test records which branch A4 really takes. Ask before escalating past sonnet |
| 2 | Scaffold: empty `WeatherSaveFields`, `WeatherSaveParticipant` and the three test files; `dia docs vcxproj-add CoW <file>` and `dia docs vcxproj-add GoogleTests <file>` for each | Project compiles | Not Started | haiku | Mechanical, TDD-exempt |
| 3 | RED then GREEN: A4's `SavedField[]` and its encode and decode over the kit codec (ten float arrays, field names from `WorldSimNames.h`) | AC 1 (A4's ten arrays and the palette round trip bit-identical through real JSON text: random finite floats, zero, denormals, each range bound; `-0.0` encodes as `+0.0`), AC 21 (`pressure_anomaly` round trip including values at +/-0.5) | Not Started | sonnet | Do not re-test the kit's array writer. The test iterates the `SavedField[]`, so a field added later is covered |
| 4 | RED then GREEN: A4 field ranges come from the layer sidecar | AC 2 and 21 for A4's fields only: per field, an out-of-range value, a wrong length, a missing array and a non-finite value are rejected naming the field; changing a fixture sidecar `max` changes what is accepted with no code change; `pressure_anomaly` accepts [-0.5, 0.5] and rejects beyond | Not Started | sonnet | Ranges come from `cow.weather.layers/<field>` (or the state sidecar for a field with no layer). Uses a fixture sidecar until `weather-debug.plan.md` ships the real one |
| 5 | RED then GREEN: biome palette in A4's record and pending blob | New, from spec-6 resolution 1 (it adds to the kit's AC 5): the record writes `biome_palette` and per-cell palette indices; a load remaps through `ResolveOrFallback`; an unknown id becomes the fallback biome with one Warning per id; a registry-hash mismatch with a successful remap is one Info; a palette index past the palette length is rejected naming the field; the extra-biome fixture registry saves and loads an appended biome with no code change | Not Started | sonnet | Replaces the old "biome >= 13" rule. Uses `BiomeTestRegistry.h` |
| 6 | RED then GREEN: `params_hash` is `ParameterFile::Hash()`, and the mismatch behaviour | AC 11: equal gives no warning; unequal gives exactly one Warning and Degraded, the run continues under the current parameters, and a following parameter apply or reload clears Degraded | Not Started | sonnet | `HashWeatherParameters` is deleted; the one-ULP property belongs to `ParameterFile` Feature 8. Test that the saved value is the file's `Hash()` and that a changed per-biome map value changes it |
| 7 | RED then GREEN: `cow.weather_state` export file: `EncodeState(sim, out)` and `DecodeState(json, record)` | The F-3 envelope (`format` `cow.weather_state`, `version` 1) then the save record's fields without `influences`/`climate`; round trip bit-identical through text; wrong `format`, missing or wrong `version` and every Task 4 defect rejected naming the key; `DecodeState` mutates nothing on failure | Not Started | sonnet | Shared by the participant and Feature 5's `state export`/`import` (Feature 5 adds the live-dimension match on import). Format name from `WorldSimNames.h` |
| 8 | RED then GREEN: `WeatherSimulation` surface: `RestoreCommitted` (with biomes), `RestoreCompleted`, `GetWorkingFields`, `GetWorkingBiomes`; size mismatch returns false with no state change | Part of AC 5, 15, 17: restore then read back equals input; mismatched sizes leave state unchanged; the working set equals what `Commit` would swap in | Not Started | sonnet | Extends Feature 2's class; read accessors const and allocation-free |
| 9 | RED then GREEN: the participant: id `cow.weather`, `GetVersion`, `ready: false`, `Validate` mutates nothing, failed `Validate` aborts `Load` | AC 4 (`ready: false` writes only `ready`, loads as a no-op, the next `WorldGenerationComplete` spins up), AC 7 (every defect and a missing top-level key returns false and fields, week, active sim and queue are unchanged), AC 8 (a failed `Validate` makes `SaveManager::Load` abort with a deserialize error and live state is exactly as before), a v1 fixture loads under `GetVersion() == 1` and a registered v1-to-v2 migration stub is invoked for a v0 fixture | Not Started | sonnet | Snapshot-compare module state before and after. No `version` key in the record. The generic migration test is the kit's; A4 asserts the id and version it declares |
| 10 | RED then GREEN: A4's restore hooks over the kit's sequence (`InitialiseSim`, `RestoreCommitted(record)`), no spin-up | AC 5 (save, load into a fresh module with the same world, N = 1, 13, 52 further weeks bit-identical to an uninterrupted run, no forcings), AC 6 (no spin-up job started and `IsReady(rev)` true at once), AC 9 (dimension mismatch: state discarded, one Warning naming both sizes, spin-up for the current revision, load succeeds), AC 10 (a save from a world with no water-source cell takes the discard path, no crash), AC 12 (the completion caused by the load is ignored; a greater revision spins up), AC 14 (restored week is the saved week; the first step publishes `WeekOf(stepDay)`) | Not Started | sonnet | The 7-step order, the stash fallback and the idempotent re-announce are the kit's and are proved on its fake stage; these tests prove A4's hooks and outcomes only. If Task 1's finding differs from the kit's assumption, record it as a spec gap |
| 11 | RED then GREEN: apply the `StateReplace` queue entry at `Capture` | A decoded `WeatherStateRecord` queued with `QueueStateReplace` is applied at the next `Capture` like a preset (reseed, then `RestoreCommitted`); committed fields and week equal the record only after that step commits; a record with other dimensions is rejected at apply with one Warning and state unchanged; a `ClearWindOverrides` entry removes every wind override at the same point and leaves other forcings | Not Started | sonnet | Feature 3 owns the entry type and the drain; this task adds the apply branch (needs Task 8 and Feature 6's `Reseed`). Feature 5's `state import` depends on it |
| 12 | RED then GREEN: `influences` and `climate` blocks in the record, their validation and restore | AC 22 (book bit-identical at N = 1, 13, 52 with pending, in-flight and active entries; a mid-step save restores in-flight entries as in flight; each Feature 7 validation defect, including an unknown kind id against the catalogue, is rejected and mutates nothing; a dimension mismatch discards the book), AC 23 (cycle `enabled`, `phase_days`, `pinned_phase_days` round trip and the continued run is bit-identical with a seeded start and a pin both set; a `cycles_hash` mismatch warns and keeps the saved phases for matching ids; a block absent loads clean with no warning) | Not Started | sonnet | Needs Features 7 and 8's pure types and catalogues; the codec half runs on their pure classes and test fakes until they are built. Cycle state is not modifier state |
| 13 | RED then GREEN: registration and ordering | AC 13 (without `SaveLoadModule` the module runs and registers nothing; with it, `cow.weather` registers after S-1's and before DeferredStep's, asserted on a fake registry that records arrival order) | Not Started | sonnet | Order is by attach time: weather registers in `DoStart`, DeferredStep's participant attaches on its first `DoUpdate` |
| 14 | RED then GREEN: stage pending content: `SerializePending`, `ValidatePending`, `DeserializePending` over the kit's blob helpers | AC 16 (working set written after `DrainAll`, committed set unchanged), AC 17 (a wrong `cell_count`, non-finite value or bad palette index fails `ValidatePending` and nothing changes), AC 19 (a step in flight with a stale revision writes `{pending_version:1, valid:false}`, `ValidatePending` accepts it, the later `Apply()` is a logged no-op), AC 19a (`step_day` written; a `valid:true` blob without a valid `step_day` fails; the applied step publishes `WeekOf(step_day)` and the season of `step_day` with no live state read), AC 21 for the pending blob | Not Started | sonnet | Keys, apply key and ordering are the scheduler's. `ValidatePending` runs the same checks as `Validate` (consistency only) |
| 15 | RED then GREEN: pending restore failure and the `Apply` tolerance as seen from A4 | AC 18 (with no `mActive`, `DeserializePending` returns not-ready with an Error, the later `Apply()` of the restored channel is a logged no-op: no `WeatherUpdated`, no crash, the next kick works) | Not Started | sonnet | The `chain.Commit() == false` tolerance in `WorldStepModule::Apply` is delivered by `save-and-history.plan.md`; this task asserts it from A4's side and adds no `WorldStepModule` edit |
| 16 | RED then GREEN: stage-level DFS-005 test: save mid-step (after `Run`, before `Commit`), load, apply at the original key | AC 15, stage half: committed fields after the apply and after N further weeks bit-identical to the uninterrupted run | Not Started | sonnet | `SerialExecutor` and a driver, no coordinator |
| 17 | Through the real coordinator with fake neighbours: DFS-005 end to end (real `DeferredStepSaveParticipant`) | AC 15, coordinator half | Not Started | sonnet | Needs the World Step plans and DiaDeferredStep Feature 3. If either is missing, mark Blocked with the reason; do not fake the scheduler |
| 18 | Real `SaveManager` test with `SaveLoadModule::CaptureState()`; size and budget at Large (6,656 cells) | AC 20 (record plus pending under 5 MB; capture duration is a Debug ballpark, reported not gated), AC 8 end to end | Not Started | sonnet | Add a Save-duration `DIA_TRACE_ZONE` here if absent. Release timing is the Weather-stack close-out's |
| 19 | Exhaustive test pass: gap audit of every AC (the amended set, see the coverage note) against the written tests, then edge cases | Gap list in notes. Edge cases: empty and 1-cell world, `cell_count` exactly 65,536 and 65,537, denormal and `-0.0` in every array, a save before the first world is ready then a load into a ready world, load twice in a row, save during spin-up, a pending blob present with `ready: false`, week 0 and a very large week, a palette of one id, a palette larger than the registry | Not Started | sonnet | No mutation check. Tests only; production bugs go to a separate fix task |
| 20 | Observation Opportunity Scan over every touched file across the five DiaObservation pillars | Findings listed in notes as suggestions | Not Started | haiku | Suggestions only. Expected finds: a Save-duration trace zone, a mismatch Warning metric, a save size gauge |
| 21 | Scoped verification: `dia run googletest --filter="WeatherSaveFields*:WeatherSaveParticipant*:WeatherPendingSave*" --shards 2`; `dia check deps`; `dia docs precommit` | Pass/fail and failing names only; all amended AC green or listed | Not Started | haiku | The full suite is in worldstep.plan.md Weather-stack close-out |
| 22 | Close-out: `dia docs plan` rows to Done with notes, then `dia docs spec-done docs/specs/applications/cow/systems/a4-weather/weather-save.md` | Plan and spec statuses updated | Not Started | haiku | **Do not run until all rows above are Done.** |

**AC coverage (old plan 28 rows, 0-26 plus 17a; now 23 rows, 0-22).** AC 1: Task 3. AC 2: Task 4 (A4 fields), kit (generic). AC 3 (`HashWeatherParameters`): deleted, replaced by Task 6. AC 4, 7, 8: Task 9. AC 5, 6, 9, 10, 12, 14: Task 10. AC 11: Task 6. AC 13: Task 13. AC 15: Tasks 16, 17. AC 16, 17, 19, 19a: Task 14. AC 18: Task 15. AC 20: Task 18. AC 21: Tasks 3, 4, 14. AC 22, 23: Task 12. New (spec-6 resolution 1, palette): Task 5. Old Tasks 4 and 5 (codec rejection table and one-ULP hash), 10's hand order, 14's own re-announce rule and 19's `WorldStepModule` edit are deleted.

## Open items

1. **Who authors the `cow.weather.layers` sidecar ranges.** The kit reads each saved field's range from its layer sidecar entry, but the weather layers are `weather-debug.plan.md`'s (Feature 5) and `waterToGround` and `pressureAnomaly` may have no layer at all. The spec says a field with no layer gives its range "in the system's state sidecar" but names no file or format. Tasks 3 and 4 run on a fixture sidecar; the real entries need that plan or a state sidecar defined.
2. **AC 2's "version 0 and 2" cases** are gone with the in-record version; the migration check (Task 9) uses a fixture because A4 has no real v0. The spec does not say what a missing `GetVersion` upgrade should assert.
3. **Per-field ranges for `wind_direction` `[0, 2pi)`** must be expressible as a sidecar `[min, max]`; the half-open upper bound is not (clamped at `2pi`). Confirm with the layer sidecar author.
4. **Biome palette inside the pending blob.** The spec-6 resolution says "every save record that holds per-cell biomes" writes a palette; whether the pending blob repeats the palette or reuses the committed record's is not stated. The plan repeats it (Task 14) so the blob is self-contained.
5. **`influences` blocks name kinds by catalogue id**, but the catalogue's file location (sidecar or record file) is open (see `weather-influence.plan.md`); an unknown id on load is planned as a Validate defect, not a drop-with-warning.
6. **Spec text still cites** "version 1" in the record, `HashWeatherParameters`, "biome >= 13", `ground_water`, and a World Step Feature 3 edit; the amendment wins and this plan follows it.
7. **Open Design Question 2** (a `strict` load option for S-3.5 R2) stays unbuilt, as the spec says.
- **Review 2026-11-16 (decision 5):** `WeatherStateRecord.h` is created by Feature 3; this feature fills it.

# Feature Spec: F-3 WorldAssetsModule and Bundle Load

**Parent:** @docs/specs/applications/cow/systems/f3-worldassets/f3-worldassets.md
**Builds on:** @docs/specs/applications/cow/systems/f3-worldassets/envelope-and-override-merge.md (Feature 1)

**Gameplay Domains:** debug

## Summary

The module that turns an F-3 file into a running world. `WorldAssetsModule` (SimPU, CoW-Shared) reads a **world bundle** (`.cowworld`), prepares and validates every file it names, applies the weather and ecology parameters to the systems that own them, starts the terrain, waits until the systems report ready, applies the start state, and announces the result. It also loads single packs and start states live, picks the initial bundle for a stage (session hint, stage config, or the game default), and exposes `cow.world.status`.

Systems never read asset paths themselves: each implements `IWorldAssetParticipant`, and F-3 hands them resolved values.

## Problem

A tuned world is only useful if New Game, a testbed and a relaunch all load the same one the same way. Today S-1, A4 and A1 each read their own files and each initialise themselves when terrain completes. F-3 has to load a bundle consistently without making any system initialise twice, without half-applying a bundle that turns out to be broken, and without breaking the systems' own regenerate-and-reinitialise behaviour.

## Goals

1. **One loader.** A bundle id becomes a running world through one code path, in dev and deployed builds.
2. **Prepare, then apply.** Every file is read, checked and resolved before anything changes, so a bad bundle leaves the current world untouched.
3. **One initialisation.** Parameters are applied before terrain, so A1 and A4 initialise once, with the right values, when terrain completes.
4. **Honest partial results.** A failure after the new terrain is accepted is reported as partial, never as "previous world kept".
5. **Live packs.** A single weather or ecology pack can be applied or cleared on a running world.
6. **Right bundle at the right time.** The initial bundle comes from a fresh session hint, the stage's config, or the game default, in that order.
7. **Decoupled.** Participants are found by module reference; a stage without a system (for example weather-only S-2.5) simply has no participant for it.

## Non-Goals

- Writing files, `save_as`, bake and `set_default` (Feature 3).
- Aliases, deploy and moving existing files to the Asset Table paths (Feature 4).
- The panel card (Feature 5).
- Implementing the participants inside S-1, A4 and A1 (Feature 6, per consumer).
- Hot-swapping a bundle mid-simulation without a reset: loading a bundle resets the world.
- Savegames (E1).

## Architecture

`Cluiche/CoW/Shared/WorldAssets/` (AD-002, CoW-Shared), namespace `CoW::WorldAssets`, on SimPU. It uses Feature 1's `CheckEnvelope`, `ParsePack`, `Resolve` and `EvaluateStale`.

```
WorldAssetsModule
  ├─ BundleResolver       .cowworld → WorldBundleSpec (paths resolved through Feature 4's `AssetLocator`: `source` first in a dev launch, else the `world` alias)
  ├─ BundleLoader         the multi-frame load state machine, with rollback
  ├─ PackManager          single pack load/clear, list, stale info
  ├─ SessionHint          read-and-delete / write of out/CoW/session/next_bundle.json
  ├─ InitialBundlePicker  session hint > stage config > game default
  └─ participants (ModuleRef, no singletons): terrain, weather, ecology, ecology_start
```

### `IWorldAssetParticipant`

```cpp
class IWorldAssetParticipant {
public:
    virtual ~IWorldAssetParticipant() = default;
    virtual Dia::Core::StringCRC Kind() const = 0;                       // terrain | weather | ecology | ecology_start
    virtual bool                 HasDefaults() const = 0;                // weather and ecology: true
    virtual EnvelopeSpec         DefaultsSpec() const = 0;               // format + current version of the defaults file
    virtual const Json::Value&   Defaults() const = 0;                   // loaded defaults
    virtual bool                 Apply(const Json::Value& resolved, ApplyReport&) = 0;   // parameters: live, no initialisation
    virtual Json::Value          Capture() const = 0;                    // current values (also used for rollback and, in Feature 3, diffs)
    virtual bool                 IsReady(uint32_t worldRevision) const = 0;
};
```

- **Weather and ecology** `Apply` set the resolved parameters; the next band uses them. `IsReady(r)` is true once the system has initialised for world revision `r` (A4 after spin-up, A1 after `Initialise`).
- **Terrain** `Apply` takes the bundle's terrain object, either `{ "baked": path }` or `{ "preset": id, "seed": n, "size": "Small|Standard|Large" }`, and starts an import or a generation. Completion is `WorldGenerationComplete`; a failure is `WorldGenerationFailed`. `Capture` describes the current terrain (baked id, preset and seed, or "edited").
- **Ecology start state** `Apply` takes the sparse cell overlay and applies it after A1 has initialised; it refuses before that.
- Participants register by being named in the module's config; a bundle part with no participant is skipped (see Missing participants).

### Module config (`.diaapp`)

```json
{ "participants": [ { "kind": "terrain", "module": "WorldGenerationModule" },
                    { "kind": "weather", "module": "WeatherModule" },
                    { "kind": "ecology", "module": "EcologyModule" },
                    { "kind": "ecology_start", "module": "EcologyModule" } ],
  "terrainTimeoutSeconds": 120, "readyTimeoutSeconds": 60 }
```

### Bundle load

`cow.world.bundle.load <id>` (and the initial load) runs a state machine, one step per frame at most:

| Step | What happens | Failure |
|---|---|---|
| **1. Prepare** | (implemented as `CheckBundle` in `Core/`, shared with the T-2 editor; amended 2026-10-20) Read the bundle, `CheckEnvelope`; read each referenced file through the `world` alias; check envelopes; `ParsePack` and `Resolve` the weather and ecology packs against their participant's defaults; confirm the terrain file or preset exists; check the start state's terrain hash against the baked world | Error: nothing changed, the previous world stays. A missing file, a too-new version, a wrong format, or a start state on non-baked terrain fails the load |
| **2. Snapshot** | `Capture()` the current weather and ecology parameters for rollback | none |
| **3. Apply parameters** | `Apply(resolved)` to the weather and ecology participants | A participant that refuses: restore the snapshot, Error, previous world stays |
| **4. Terrain** | `terrain.Apply(spec)`; wait for `WorldGenerationComplete` (or `Failed`), with a watchdog | `Failed` or timeout: restore the snapshot, Error, previous world stays (S-1 keeps the previous world until a new one is accepted) |
| **5. Ready** | Poll `IsReady(worldRevision)` of the weather and ecology participants each frame | Timeout (`readyTimeoutSeconds`): partial (below) |
| **6. Start state** | `ecology_start.Apply(cells)` | Refused or failed: partial (below) |
| **7. Done** | Broadcast `WorldBundleLoaded{bundleId}`; status becomes the loaded bundle | none |

**Order.** Parameters are applied **before** terrain starts, so A1 and A4, which initialise themselves on `WorldGenerationComplete`, initialise once with the pack's values. This amends the parent's load order (terrain first). Systems keep their own regenerate behaviour untouched.

**Start state hash.** A start state whose terrain hash differs from the baked world's is refused: the bundle loads without a start state and logs a Warning (GDD S-3.5 CH-S35-04). A start state on preset-plus-seed terrain makes the bundle invalid (Prepare error).

**Partial result.** Once terrain has been accepted, the previous world cannot be restored. A later failure (not ready in time, start state failed) leaves the new world and its parameters in place, skips the start state, logs an Error, publishes `WorldBundleFailed{bundleId, step, message}` and sets the status to `partial`. This replaces the parent's blanket "keeps the previous world", which holds only for failures before the terrain is accepted.

**One load at a time.** A second load request while one is running is refused ("a bundle is loading").

### Missing participants

A bundle part whose participant is not present is skipped with one Warning ("no participant for ecology: pack ignored") and recorded in the status. A missing **terrain** participant, or a terrain part with no participant, is an Error (a bundle always needs a world).

### Single pack and start state commands

| Command | Does |
|---|---|
| `cow.world.status` | Loaded bundle id (or `custom`, or `none`), terrain source, weather pack, ecology pack and start state with stale flags, load state (`idle`, a step name, `partial`, `failed`), the last error, participants present, and an `actions` block giving `{enabled, reason}` for each load and save action, using the same reason strings the commands return when refused (added 2026-10-20, Feature 5) |
| `cow.world.bundle.list` | Bundles in `Global/Worlds/` with display names |
| `cow.world.terrain.list` / `terrain.load <id>` | Baked worlds with origin and size; load one with the current parameters applied (the terrain and ready steps of the loader). A start state whose terrain hash differs is cleared with a Warning (added 2026-10-20, Feature 5) |
| `cow.world.bundle.load <id>` | The state machine above |
| `cow.world.pack.list <weather\|ecology>` | Packs in `Global/<System>/Packs/` with display names and stale flags (header only) |
| `cow.world.pack.load <kind> <id>` | Prepare (parse, resolve), then `Apply` live; no initialisation. The status becomes `custom` and the part shows the pack |
| `cow.world.pack.clear <kind>` | Apply the defaults |
| `cow.world.startstate.load <id>` | Prepare (envelope, hash check against the loaded baked terrain), then apply the cells on top of the current state (a live edit like A1's paint). Refused if the system is not ready or the terrain is not baked |
| `cow.world.startstate.clear` | Forget which start state the world has; the cells already applied stay (a later `bundle.save_as` then records none) |

Live pack changes take effect at the system's next band or week, as its own tuning does.

### Initial bundle

At the first update after the participants exist, `InitialBundlePicker` chooses, in order:
1. a **session hint** `out/CoW/session/next_bundle.json` (`{bundle, from_stage}`), if it is no more than 60 seconds old; it is deleted when read, and an older one is deleted unread;
2. the stage's `.diastage` config `world_bundle`, where the value `"@default"` means `cow.diagame` `config.default_world`;
3. nothing: the stage loads its own world as S-1 does (SD-S1-010).

`WorldAssetsModule` also writes the hint for launch-outs: `cow.world.session.hint <bundleId>` (and the same call from code) writes `next_bundle.json` with the current stage as `from_stage`.

### Events and logging

- **Events** (small POD): `WorldBundleLoaded{ bundleId }`, `WorldBundleFailed{ bundleId, step, message[96] }`. `WorldAssetSaved` belongs to Feature 3.
- **Channel `WAS`.** Info: each step, the bundle loaded. Warning: entries of every `MergeReport` (dropped, clamped, stale), skipped participants, refused start state. Error: any failure above.
- **Traces / Profiling:** `DIA_TRACE_ZONE` per step; `DIA_PROFILE_SCOPE` on Prepare.
- **Metrics:** bundle loads, failures, partials, load milliseconds per step, packs applied.
- **Health:** `IHealthReporter` Degraded while the status is `partial` or `failed`.

## Acceptance Criteria

1. With fake participants, `cow.world.bundle.load` runs Prepare, Snapshot, Apply parameters, Terrain, Ready, Start state and Done in that order, and publishes `WorldBundleLoaded{bundleId}` once.
2. Weather and ecology parameters are applied **before** the terrain participant is asked to start.
3. A missing file, an envelope error (too new, wrong format), or a start state on non-baked terrain fails in Prepare; no participant's `Apply` is called and the previous world and parameters are unchanged.
4. A participant that refuses `Apply` in step 3, or a terrain failure or timeout in step 4, restores the captured parameters; the previous world stays; an Error and `WorldBundleFailed` are produced.
5. A start state whose terrain hash does not match the baked world is refused with a Warning and the bundle loads without a start state.
6. A failure in step 5 or 6 leaves the new world and parameters, skips the start state, sets status `partial`, logs an Error, publishes `WorldBundleFailed` and makes the health reporter Degraded.
7. A second load request during a load is refused with a message.
8. A bundle part with no participant is skipped with one Warning and listed in the status; a missing terrain participant fails the load.
9. Unknown, mistyped, clamped and stale pack entries are reported as Warnings from the Feature 1 `MergeReport`, one per entry, and a stale pack still loads.
10. Loading the same bundle twice gives identical resolved parameters for every participant.
11. `cow.world.pack.load` applies a resolved pack live without terrain or initialisation, sets the status to `custom`, and `pack.clear` applies the defaults; `pack.list` and `bundle.list` return display names and stale flags.
12. `cow.world.startstate.load` refuses when the system is not ready or the terrain is not baked or the hash differs; `startstate.clear` changes no cells and clears the recorded start state.
13. The initial bundle comes from a fresh session hint, else the stage's `world_bundle` (`"@default"` resolves to `default_world`), else nothing; a hint older than 60 seconds is deleted unused; a read hint is deleted.
14. `cow.world.session.hint <id>` writes the hint with the current stage as `from_stage`.
15. `cow.world.status` reports the loaded bundle (or `custom` / `none`), each part with its stale flag, the load state, the last error and which participants exist.
16. The module uses module references only: no singletons, no globals reaching into S-1, A4 or A1 (checked by `dia check deps` and by the fakes-only tests).

## Test Strategy

- **GoogleTests (TDD)** with fakes in `CoW/Shared/WorldAssets/Testing/`:
  - `FakeParticipant` (records calls, can refuse `Apply`, can delay readiness), `FakeTerrain` (completes or fails after N frames), a fake message bus and clock, and an in-memory file source for the `world` alias.
  - Each state-machine path: success; Prepare failures of each kind; refusal in step 3; terrain failure and timeout; ready timeout; start-state refusal and failure; second load during a load; missing participants.
  - Rollback restores exactly the captured values.
  - Pack list, load, clear; start state load and clear; initial bundle selection (hint fresh, hint stale, stage value, `@default`, none); hint write.
  - Reproducibility of resolved parameters over two loads.
- **Integration** (once consumers exist, Feature 6): with real S-1, A4 and A1 at Small and Large, loading the same bundle twice gives the same A1 state hash after the start state; this is S-3.5's R-checks.
- No UI or E2E here: the panel is Feature 5.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| AD-002 | Four-tier placement | CoW-Shared |
| AD-005 | One stage per testbed, modes in the panel | No new stage; commands only |
| PD-009 | Generated output under `Cluiche/out/<App>/` | Session hint under `out/CoW/session/` |
| PD-010 | `.diagame` is the root | `default_world` read from `cow.diagame` |
| PD-001 | StringCRC ids | Participant kinds and ids are `StringCRC` |
| No singletons without sign-off | Use module references | Participants found by module reference from config |
| Game logic on SimPU | The module runs on SimPU | Same PU as S-1, A4 and A1 |
| Parent version table | Load and warn | Through Feature 1's reports |
| GDD S-3.5 CH-S35-04 | Start-state hash mismatch is refused, the bundle loads without it | Step 6 and AC 5 |

No conflicts. Two parent statements are amended by this spec and updated in the parent: the load order (parameters before terrain) and the failure rule ("keeps the previous world" holds before the terrain is accepted).

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Feature 1 | Envelope, packs, merge | Approved |
| Feature 4 | The `world` alias and deploy so paths resolve in every build | Draft |
| DiaMessageBus, DiaAPI `CommandRegistry`, `PathStore` | Events, commands, aliases | Done |
| S-1 / A4 / A1 participants | Implemented in Feature 6 | Specced, not built |
| **Consumer requirements (Feature 6)** | A1 and A4 must read their parameters when they initialise, not cache them at module start, and expose `IsReady(revision)`. A1 and A4 keep initialising themselves on `WorldGenerationComplete` | To be added to their specs with Feature 6 |

## Open Design Questions

_None remaining._ Resolved: the load-order amendment (parameters before terrain, readiness polling), the partial-result rule, and the missing-participant rule are decided above.

## Status

`Approved` (2026-10-20). Plan not yet created. Needs Feature 1; the `world` alias comes from Feature 4.

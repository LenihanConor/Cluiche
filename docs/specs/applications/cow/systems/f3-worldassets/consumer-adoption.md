# Feature Spec: F-3 Consumer Adoption

**Parent:** @docs/specs/applications/cow/systems/f3-worldassets/f3-worldassets.md
**Builds on:** @docs/specs/applications/cow/systems/f3-worldassets/world-assets-module-and-bundle-load.md (Feature 2, the `IWorldAssetParticipant` interface), @docs/specs/applications/cow/systems/f3-worldassets/save-as.md (Feature 3)

**Gameplay Domains:** debug, simulation

## Summary

What S-1, A4 and A1 must provide so F-3 can load, apply and capture their assets, and what the other consumers (A3, S-2.5, S-3, S-3.5) need. It fixes one participant contract, replaces the thinner interface that A4 and A1 declared earlier, and records each consumer's adoption as a task in that consumer's own plan, built when its module comes into existence. It also ships a shared contract test that every participant must pass.

The consumer specs are amended in place with this feature (listed under Amendments).

## Problem

F-3's loader, saver and card are only useful if the systems behind them behave the same way. Today the three participants are described three ways: A4 and A1 declare `Defaults()` by value, a void `Apply` and no readiness; A1 puts two asset kinds on one class, which a single `Kind()` cannot express; S-1 declares no participant at all and has no hash for the start states that key to its worlds; and A1's "edited cells" is undefined, so a start state saved after loading another one could lose the first one's cells. These need one agreed answer before any module is built.

## Goals

1. **One contract.** A single `IWorldAssetParticipant` (Feature 2), implemented the same way by every system.
2. **No double initialisation.** Each system reads its parameters when it initialises, so a pack applied before terrain is the one it initialises with.
3. **Exact edit sets.** A1's start-state capture is cumulative, so saving always reproduces the whole edited world.
4. **Terrain that can be keyed.** S-1 provides a stable content hash and says whether its current world is baked, from a preset, or edited.
5. **Proven.** Every participant passes the same contract test.
6. **Timed with the systems.** Each adoption is a task in its consumer's plan; none blocks another.

## Non-Goals

- The F-3 module, loader and saver (Features 2 to 4).
- The tuning content of any system (what the parameters mean).
- Species and biome record packs (Feature 1 decision: parameter files only).
- A3 as a participant: its rules file is a defaults file only in v1.
- Migrating old file formats (none exist yet).

## Architecture

### The participant contract

`Cluiche/CoW/Shared/WorldAssets/IWorldAssetParticipant.h` (CoW-Shared). It is the Feature 2 interface, restated here as the single definition:

```cpp
struct ApplyReport { std::string message; /* set when Apply returns false */ };

class IWorldAssetParticipant {
public:
    virtual ~IWorldAssetParticipant() = default;
    virtual Dia::Core::StringCRC Kind() const = 0;                 // terrain | weather | ecology | ecology_start
    virtual bool                 HasDefaults() const = 0;          // true for weather and ecology
    virtual EnvelopeSpec         DefaultsSpec() const = 0;         // format id + current version of the defaults file
    virtual const Json::Value&   Defaults() const = 0;             // the loaded defaults file content
    virtual bool                 Apply(const Json::Value& resolved, ApplyReport&) = 0;
    virtual Json::Value          Capture() const = 0;
    virtual bool                 IsReady(uint32_t worldRevision) const = 0;
};
```

**Rules for every participant:**
1. **One class per kind.** A module owns one small adapter object per kind it provides (`WeatherPackParticipant`, `EcologyPackParticipant`, `EcologyStartStateParticipant`, `TerrainParticipant`) and exposes them to F-3 by reference. A module does not inherit the interface itself, so a module with two kinds (A1) needs no workaround.
2. **Read at initialise.** `Apply` replaces the system's live parameters; the system's `Initialise` (run when `WorldGenerationComplete` arrives) uses those live parameters, never the file again. Hot reload of a defaults file refreshes `Defaults()` and re-applies the currently loaded pack on top.
3. **`Apply` is atomic.** It validates the whole resolved value, then swaps it in. On any problem it returns false with a message and changes nothing.
4. **Effective at the next band.** A live `Apply` after initialisation takes effect at the system's next band or week, the same as its own tuning commands.
5. **`Capture` mirrors `Apply`.** `Apply(x)` then `Capture()` gives `x` (within Feature 1's tolerance), in the shape of the defaults file.
6. **`IsReady` is cheap and monotonic.** It is true when the system has fully initialised for that world revision (including a spin-up), and stays true until a newer revision arrives.
7. **SimPU only.** All calls come from the F-3 module on the simulation thread.

### S-1: `TerrainParticipant` (kind `terrain`)

- `HasDefaults()` is false (`DefaultsSpec` and `Defaults` are unused).
- **`Apply(spec)`** takes the bundle's terrain object:
  - `{ "baked": path }`: read the file through F-3's locator, `ImportWorld`, accept it. Opening it **locks** the world (SD-S1-009). A parse or validation failure returns false.
  - `{ "preset": id, "seed", "size" }`: load the preset's `WorldShapeParams`, set the dimensions from `size`, and request a regeneration with that seed.
- **Import publishes `WorldGenerationComplete`.** An imported world now publishes the same message as a generated one (seed taken from the file's provenance, `retryAttempts` 0), so A1, A4 and the others initialise from it. (S-1's text already increments the world revision on import; the message was not stated.)
- **`Capture()`** returns the current terrain source: `{ "source": "baked", "id", "hash" }` for a baked or locked-imported world; `{ "source": "preset", "preset", "seed", "size" }` for a generated world whose parameters still equal the preset's; `{ "source": "edited" }` once sliders or placements have changed from either. Bundles save only the first two forms (Feature 3).
- **Content hash.** S-1 exposes `GetWorldContentHash()`: a 64-bit FNV-1a hash of the world's canonical export text (sorted keys, no whitespace), written as 16 lower-case hex digits. It is what start states record (`world.hash`) and what bundle loading compares. It depends only on the world, not on the file's formatting or location.
- **Bake.** `ITerrainBaker::Bake(absolutePath)` calls `ExportWorld` for Feature 3; after a bake the world counts as baked with that id.
- **`IsReady(rev)`:** `GetWorldRevision() >= rev`.
- **Validate.** `cow.worldgen.validate <file>` reports its result to F-3 (`WorldAssetsModule::SetWorldValidated(relPath, ok)`, by module reference), which updates the record's `validated` flag (Feature 3).

### A4: `WeatherPackParticipant` (kind `weather`)

- Defaults file `Global/Weather/weather_parameters.json`, format `cow.weather_params`.
- `Apply` validates against A4's own parameter rules (the 20 constants and their ranges) and swaps `mParams`; it takes effect on the next weekly step. `Capture()` returns `mParams` in the defaults file's shape.
- `IsReady(rev)` is true after `Initialise` **and spin-up** for revision `rev` (spin-up is the 52 steps that run once after `WorldGenerationComplete`).
- The five named presets stay A4 Feature 6's question (packs or scenarios); F-3 supports either.

### A1: two adapters

- **`EcologyPackParticipant` (kind `ecology`):** defaults `Global/Ecology/ecology_parameters.json`, format `cow.ecology_params`. The same rules as A4's. Species and biome records are **not** part of a pack; their tuning keeps `cow.ecology.tuning.save`.
- **`EcologyStartStateParticipant` (kind `ecology_start`):** no defaults file. `Apply(cells)` runs only after A1 is ready for the current revision; it applies each edited cell through A1's own edit path (`set_field`, `set_species`, biome paint), so applying a start state and painting by hand are the same code. It returns false if A1 is not ready or a cell or field name is unknown.
- **The edit set is cumulative.** A1 keeps, per cell, the fields whose values differ from the freshly initialised state, including cells set by a loaded start state and any later painting. `Capture()` returns that whole sparse overlay, sorted by row then column, using A1's `set_field` names. A re-`Initialise` clears it. Saving a start state after loading another and painting three more cells therefore stores the union, never only the three.
- **Unsaved detection.** F-3 compares `Capture()` with the loaded start state's cell set to decide whether there are unsaved cell edits (Feature 3's bundle rule).
- `IsReady(rev)` is true after `Initialise` for `rev`.

### A3 (no participant)

`succession_rules.json` moves to `Global/Succession/` with the envelope `cow.succession_rules` and is a hand-authored defaults file in v1. A1 reads it as it does today; A3 owns its contents. A3 gets no participant and no pack until a testbed asks for tunable rules.

### Testbeds

S-2.5 and S-3 already read the session hint and `world_bundle` (amended 2026-10-19). What remains: S-2.5's stage loads a terrain-only bundle without error when no ecology participant exists (Feature 2's missing-participant rule), its scenario files carry the envelope, and S-3 exposes the authoring loop through F-3's card (Feature 5). S-3.5 consumes everything above and is amended only where it names commands.

### The shared contract test

`Cluiche/CoW/Shared/WorldAssets/Testing/ParticipantContractTests.h` is a GoogleTest fixture parameterised by a factory that builds a participant with a small fixture world. Every adapter passes the same cases:
- `Apply(Defaults())` changes nothing observable.
- `Apply(x)` then `Capture()` equals `x`.
- A bad value makes `Apply` return false with a message and leaves `Capture()` unchanged.
- `IsReady(rev)` is false before initialisation for `rev`, true after, and stays true.
- A second `Apply` replaces the first completely.
- (Terrain) `Capture()` reports `baked`, `preset` or `edited` correctly after import, generate and slider change, and `GetWorldContentHash()` is identical for the same world exported twice and different after a one-cell change.
- (Ecology start state) apply, paint, `Capture()` returns the union; re-initialise clears it.

## Acceptance Criteria

1. `IWorldAssetParticipant` exists once, in CoW-Shared, with the members above; A4's and A1's earlier declarations are replaced and no module inherits the interface directly.
2. Each of S-1, A4 and A1 provides its adapter objects, and A1 provides two with distinct kinds.
3. Each participant passes the shared contract test with its own factory.
4. A pack applied before terrain is the one A4 and A1 initialise with; neither re-reads its defaults file at initialise.
5. `Apply` is atomic and returns false with a message on an invalid value for all four parameter and cell participants.
6. S-1's import publishes `WorldGenerationComplete`, and A1 and A4 initialise from an imported world exactly as from a generated one.
7. S-1 reports the terrain source as `baked`, `preset` or `edited` correctly, locks an imported world, and exposes `GetWorldContentHash()` with the stability properties above.
8. `cow.worldgen.validate` updates the world record's `validated` flag through F-3.
9. A1's edit set is cumulative: loading a start state then painting returns the union; a re-initialise clears it; `Capture()` is sorted and uses `set_field` names.
10. `EcologyStartStateParticipant::Apply` refuses before readiness and on unknown names, changing nothing.
11. `IsReady` for A4 includes the spin-up, and for A1 includes `Initialise`.
12. The consumer specs carry the amendments listed below.

## Amendments (applied with this spec)

- **S-1** (`worldgenerationpipeline.md`): the terrain participant, import publishes `WorldGenerationComplete`, `GetWorldContentHash()`, terrain source states, validate hook, Bake interface.
- **A4** (`a4-weather.md`): `WeatherPackParticipant` replaces the inherited declaration; read-at-initialise; `IsReady` includes spin-up.
- **A1** (`a1-ecology.md`): two adapters replace the inherited declaration; cumulative edit set; read-at-initialise; species and biome tuning stays `tuning.save`.
- **A3** (`a3-biomesuccession.md`): the rules file moves to `Global/Succession/` with its envelope; no participant in v1.
- **S-2.5, S-3, S-3.5:** no further change beyond what their 2026-10-19 and 2026-10-20 amendments state.

## Test Strategy

- **GoogleTests:** the shared contract suite run once per participant (terrain, weather, ecology, ecology start) with fixture worlds at Small size; S-1's content-hash tests; A1's edit-set tests (load, paint, union, reinitialise); the import-publishes-completion test (A1 and A4 fakes initialise on it); the validate-hook test.
- **Integration (after the modules exist):** load the same bundle twice at Small and Large and compare the A1 state hash after the start state (S-3.5's R-checks); a pack applied before terrain produces one initialisation per system (count `Initialise` calls).
- No UI tests here.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| AD-002 | Four-tier placement | The interface and tests are CoW-Shared; each adapter sits with its module |
| No singletons without sign-off | Use module references | F-3 gets adapters by reference; S-1 reaches F-3 by module reference for validate |
| Game logic on SimPU | Participants on SimPU | Rule 7 |
| Parent version table / Feature 1 | Load and warn; packs are parameter files only | A1's species and biome tuning stays out of packs |
| SD-S1-009 | Imported worlds open locked | The terrain adapter locks on import |
| SD-S1-011 | Presets are authored assets | Presets load through the locator |
| CH-S35-02 / CH-S35-04 | Saves are self-contained; a hash mismatch refuses the start state | The content hash is the value compared |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Features 1 to 4 | Contract, loader, saver, locator | Approved |
| S-1 Feature 4 (JSON export/import) and preset support | Import, hash, presets | Specced, not built |
| A4 Feature 3 (WeatherModule) | The weather adapter | Specced, not built |
| A1 Features 2 and 7 (cell state, EcologyModule) | The ecology adapters and edit set | Specced, not built |
| A3 Feature 1 | Creates the rules file at its new path | Specced, not built |

## Resolved Decisions

1. **Adapter objects, not inheritance** (2026-10-20). A module with two kinds cannot implement one `Kind()`; one adapter per kind also keeps each kind's tests separate.
2. **The edit set is cumulative** (2026-10-20), so a start state saved after loading another keeps the first one's cells. The alternative (edits since load) would silently lose them.
3. **Import publishes `WorldGenerationComplete`** (2026-10-20), so everything that initialises on it works for imported worlds without a second code path.
4. **The content hash is FNV-1a 64 over canonical export text** (2026-10-20): stable across machines, independent of file formatting, and cheap enough to compute at bake and at load.
5. **A3 has no participant in v1** (2026-10-20); its rules file is a defaults file, not a tuning surface.

## Open Design Questions

_None remaining._

## Status

`Approved` (2026-10-20). Plan not yet created. Each adoption is a task in its consumer's plan.

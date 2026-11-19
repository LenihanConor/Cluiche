# Feature Spec: Launch Presets

**Parent:** @docs/specs/applications/dia/systems/stagecatalogue/stagecatalogue.md (Feature 2)
**Status:** Approved (2026-11-18)

## Summary

A stage can declare named **presets** in its `.diastage`. Each preset is a set of config overrides for modules scoped to that stage, for example S-1 "alpine42" = `WorldGenerationModule {preset: alpine, seed: 42, size: Small}`.

There are three ways to launch a stage with a preset, all through one request:

- a sub-row in the boot menu,
- `stages.launch {stage, preset}`,
- `dia run|launch <target> --stage <S> --preset <P>`.

Modules get their config only once, at boot, so DiaApplicationFlow gains an opt-in `Module::OnStageLaunch(const char* overridesJson)` hook. The application calls it on stage entry, before `BeginStart`, and only on modules that start for that stage. PD-010 was amended on 2026-11-18 to allow these launch parameters: they never change which modules are built.

## Goals

- One click, or one command, takes you into a tool stage in a known configuration.
- Scripts and AI agents can drive testbeds into a known state without menus.
- Modules that don't support presets are untouched.

## Acceptance Criteria

### Format and validation

1. `.diastage` gains an optional `presets` field:
   - an array of at most 16 entries `{name, description?, modules}`;
   - `name` is unique within the stage, lowercase kebab-case, ≤ 31 characters;
   - `description` is ≤ 120 characters;
   - `modules` is an object keyed by module instance id, each value an object.
2. `dia validate manifest`:
   - Each `modules` key must be a module instance declared in the stage's own `.diaapp` and scoped to that stage. An `all`-scoped module is not restarted on stage entry, so it can't take an override; naming one is an error that says so.
   - A preset JSON larger than 2 KB is an error.
3. `JsonDiaStageSerializer` round-trips `presets`. Override objects are kept as JSON text, so editor saves never drop or reorder them semantically.

### Runtime (DiaApplicationFlow)

4. `ManifestComposerV2::MergeStage` reads `presets` into the stage's `StageDeclaration`: a list of `{name StringCRC, description, per-module {instanceId, overridesJson}}`, with the caps from AC 1-2. A malformed `presets` field is logged and ignored, and the stage still loads.
5. `IApplicationControl` gains `TransitionTo(const StringCRC& stage, const StringCRC& preset)`.
   - The existing `TransitionTo(stage)` means "no preset".
   - An unknown preset logs an Error and the transition does **not** happen.
6. `Module` gains `virtual void OnStageLaunch(const char* overridesJson) {}`.
   - During a transition, before `BeginStart`, the application calls it on every module that starts for the new stage, as at `Application.cpp:1129`.
   - The argument is that module's override from the chosen preset, or `""` when there is no preset or no override for it.
   - The call runs on the same thread and at the same point as `BeginStart`.
7. `IApplicationControl::GetCurrentPreset()` returns the preset of the running stage, or empty.

### Launch path

8. `RenderToSimNavRequest` gains `StringCRC preset`, empty by default, so HUD navigation is unchanged.
   - `SimNavigationHandlerModule` passes it to `TransitionTo(stage, preset)`.
   - `AutomationService::ReleaseNavigationHold` gains an optional preset argument that it passes through.
9. **`stages.launch {stage, preset?}`** (`StageCatalogueModule`, MainPU):
   - Validates against the catalogue.
   - Refuses with `{error}` unless the current stage is Boot.
   - On success, sends a `StageLaunchRequest {stage, preset}` on a new EventStream (MainPU → SimPU, capacity 4) that `SimNavigationHandlerModule` reads.
   - Replies `{ok: true}` once the request is queued.
10. **Boot menu** (Feature 4's view): a stage with presets shows them as indented sub-rows under each of its rows, with the preset description as a tooltip.
    - Clicking a sub-row launches that preset.
    - Clicking the stage row launches it with no preset.
    - `BootMenuLogic` exposes preset rows so they are unit-tested.
11. The catalogue (Feature 1) carries preset names and descriptions per entry into the snapshot and into `stages.list`/`stages.get`. It does not carry the override JSON, which only the runtime needs.

### CLI (`--stage` / `--preset`)

12. `dia run <target> --stage <S> [--preset <P>]` and `dia launch` do the same:
    1. Validate the stage and preset against the game's files first, before building or launching. A typo exits non-zero and lists the valid names.
    2. Pass `--stage S [--preset P]` to the exe.
    3. Reuse DiaViewLaunch Feature 4's `dia launch` argument passthrough if it exists, or build it here (the same design) if it doesn't.
    4. `--preset` without `--stage` is an error.
    5. The flag is `--preset` on `run`/`launch` only. `dia preset`, the existing DiaConsole command-preset command, is unrelated and unchanged.
13. **Game entry:** the exe parses `--stage`/`--preset` into `Application::SetInitialLaunch(stage, preset)`.
    - When Boot is ready, `SimNavigationHandlerModule` issues that transition once, unless an automation navigation hold is active, in which case the hold wins.
    - An unknown stage at runtime logs an Error and stays at Boot.

### First adopter

14. S-1 `WorldGenerationModule` overrides `OnStageLaunch`. It applies `preset`, `seed` and `size` from the override on top of its manifest config, using the same fields as its existing config.
    - `s1world_generation_testbed_stage.diastage` declares at least two presets.
    - After `dia run cow --stage S1WorldGenerationTestbedStage --preset <p>`, the `cow.worldgen` state query reports that preset's seed and size.

### Observation

15. Logs:
    - preset launch requested, with source menu, command or CLI;
    - `OnStageLaunch` delivered (module, bytes);
    - unknown preset refused;
    - initial launch issued or skipped because of a hold.

## Files Touched

- **DiaApplicationFlow:**
  - `Manifest/ApplicationManifestV3.h` and `Manifest/ManifestComposerV2.cpp`
  - `IApplicationControl.h` and `Application.*`
  - `Module.h`
  - module doc
- **DiaAutomation:** `AutomationService.*`, for the hold passthrough.
- **DiaGame:** serializer, catalogue entry, snapshot, `StageCatalogueModule` (`stages.launch`).
- **CluicheGameBaseline:**
  - `SimNavigationHandlerModule.*` and the nav request type
  - `BootMenuLogic`/`BootMenuModule` (preset rows)
  - the exe entry argument parse (CoW and CluicheTest `main`)
- **DiaCLI:** `cli/run.py`, `cli/launch.py`, `cli/cli_validate.py`, tests.
- **CoW:** `WorldGenerationModule`, the S-1 `.diastage`, `cow_main.diaapp` (new stream).

## Binding Decisions

| Decision | How this feature complies |
|---|---|
| PD-010 (amended 2026-11-18): `.diastage` may carry launch parameters applied through the opt-in hook | Presets only change config through `OnStageLaunch`, and module construction is untouched (AC 4-6) |
| SC-007: presets inline, per module instance | AC 1-2 |
| SC-009: one launch path | Menu, `stages.launch` and the CLI all end at `TransitionTo(stage, preset)` through `SimNavigationHandlerModule` (AC 8-10, 13) |
| SC-010: menu actions are commands | `stages.launch` does exactly what the menu's sub-row does |
| PD-002 / AD-004: PU boundaries are streams | `stages.launch` (MainPU) reaches SimPU by EventStream; the menu (RenderPU) uses the existing nav EventStream (AC 8-9) |
| PD-004 / AD-002: no STL in public APIs | Override JSON crosses the API as `const char*`, like `OnConfigure`; fixed-cap declaration lists (AC 4, 6) |
| AD-001: sign-off for engine API changes | **Sign-off needed:** `Module::OnStageLaunch`, `TransitionTo(stage, preset)`, `GetCurrentPreset`, `SetInitialLaunch`, `ReleaseNavigationHold` preset argument |
| DiaCLI rule: never call the exe directly | `--stage/--preset` go through `dia run`/`dia launch` passthrough (AC 12) |

## Test Plan

- **GoogleTests:**
  - Composer reads presets; a malformed field is ignored and the stage still loads.
  - `OnStageLaunch` on a fake app:
    - called only for modules starting for the stage, before `BeginStart`;
    - not called for an `all` module already running;
    - `""` with no preset;
    - the right override per module.
  - Unknown preset: no transition.
  - `GetCurrentPreset`.
  - Nav request preset passthrough, including through an automation hold.
  - `stages.launch`: refused outside Boot, unknown stage or preset refused, request queued.
  - Initial launch issued once and skipped under a hold.
  - `BootMenuLogic` preset rows.
- **pytest:**
  - Validator: an `all`-scoped instance is an error, an undeclared instance is an error, duplicate names, 2 KB cap.
  - `--stage/--preset` validation errors before launch, with the valid-name list.
  - `--preset` without `--stage` is an error.
- **E2E:**
  1. `dia run cow --stage S1WorldGenerationTestbedStage --preset <p>` reaches the stage.
  2. A `cow.worldgen` query shows the preset's seed and size.
  3. `dia query stages.launch` from Boot does the same.
- **Manual:** clicking a preset sub-row in the CoW menu opens S-1 with that world.

## Open Design Questions

1. **Shallow vs deep merge.** AC 14 assumes a shallow merge: top-level keys in the override replace those in the manifest config. Nested config objects (for example `path_aliases`) would be replaced whole.
   - Deep merge is friendlier, but harder to reason about.
   - Shallow merge is the spec default. Revisit if a second adopter needs nested overrides.
2. **Who merges.** The spec passes only the override, and each module merges it onto its own config. That keeps DiaApplicationFlow ignorant of config shape, but every adopter repeats a little merge code.
   - Alternative: the application passes `merged = shallowMerge(configJson, override)` so modules just re-read their config.
   - Decide when building AC 6.
3. **Presets for a stage that is already running.** Re-launching a preset of the current stage means going through Boot. Hot re-apply is out of scope; flag it if testbeds want it.

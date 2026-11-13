**Spec:** @docs/specs/applications/cow/systems/s1-worldgeneration/worldgenerationpipeline.md (SD-S1-010, SD-S1-009)
**Status:** In Progress

System plan row 16b. There is no feature spec: SD-S1-010 in the system spec is the contract, and this plan adds a short "World hand-off" section under it when built. Code in `Cluiche/CoW/Modules/WorldGenerationModule.*`, tests in `Cluiche/Tests/GoogleTests/CoW/TestWorldGenCommands.cpp` (reuses its module `Harness`).

**Scope decisions (2026-11-12):**
- The `"world_bundle": "<id>"` form belongs to F-3 World Assets, which is spec-only (no code). It is NOT built: the config parser rejects it with a clear Error log and a failing module health, and leaves the seam (`GetHandoffError()` names it).
- No schema file enumerates module config keys (`config` is a free-form object in the manifest schemas), so there is nothing to extend; the contract is documented in the system spec.
- No new shipped stage: the S1 testbed stage keeps its current config (= today's behaviour). Test configs are built inline by the module `Harness`.

## Implementation Patterns

**Config keys** (module `config` object in the stage's `.diaapp`/`.diastage` module entry; read in `OnConfigure`, applied as below):

| Key | Meaning | When applied |
|---|---|---|
| `world_file` | `"<name>"` of a `<name>.wld.json` found in `out_worlds_dir`, then `worlds_dir`. Imported all-or-nothing, opens locked (SD-S1-009) | In `DoStart`, after the bus and commands are up (the import publishes `WorldGenerationComplete`) |
| `preset` | Preset id; with the existing `seed` and `size` keys. `PresetLoad` then a Regenerate (regions and sites placed, deterministic) | At the end of `OnConfigure`, after the preset dirs are set; the first generation still waits for the start deferral and holds |
| `world_bundle` | F-3 bundle id. Not built: rejected | Parse time |

`seed`/`size` alone (no `preset`, no `world_file`) keep today's meaning (initial seed and size of the auto-generated world). With `world_file`, `seed`/`size` are ignored with a Warning (the file carries its own provenance).

**Failure** (any of: more than one source key, `world_bundle`, a key of the wrong type, unknown preset, missing/invalid/corrupt world file, invalid world name): `GetHandoffError()` holds the message; `DoStart` logs it at Error, sets health Failing (`world_handoff_failed`) and increments the health error count; automatic generation is suppressed (`mHandoffFailed`), so no world exists and `GetWorldRevision()` stays 0. An explicit action (`regenerate`, `set`, ...) still works afterwards (the existing `mActionSeen` rule). A failed import changes nothing (ImportWorld is all-or-nothing).

**Reading the world:** `GetCurrentWorld()` (nullptr until revision > 0) and `GetWorldRevision()` exist; add `IsReady(rev)` (`GetWorldRevision() >= rev`) from the F-3 participant text. Dependants poll `IsReady` or subscribe to `WorldGenerationComplete`.

**Test shapes:** a pure `WorldGenSession` produces the expected world (preset form) or the exported file (file form); the module under the `Harness` must hold a byte-identical world with revision > 0.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | **Config parser, preset form and failure handling.** Parse `world_file`/`preset`/`world_bundle`, validate types and exclusivity, apply the preset form, `GetHandoffError()`, `IsReady()`, failure state (health, log, suppressed auto-generation) | RED-GREEN in `TestWorldGenCommands.cpp`: preset form matches a pure-session world and carries seed/size; unknown preset, two sources, `world_bundle` and non-string keys fail with revision 0 and Failing health; no hand-off keys unchanged; a manual regenerate works after a failure | Done | sonnet | 9 WorldHandoff tests (preset form == pure-session world incl. regions/sites, size key, no keys unchanged, unknown/invalid preset, >1 source, world_bundle rejected, wrong types, manual regenerate after failure); WorldGen*:WorldHandoff* 169+168 pass. RED = compile fail (IsReady/GetHandoffError missing). Keys world_file recognised and type-checked, import is task 2 |
| 2 | **World file form.** Import `world_file` in `DoStart`, publish through `OnWorldImported`, locked, seed/size ignored with a Warning | RED-GREEN: file exported by a pure session loads byte-identical, locked, revision > 0, seed from provenance; missing, corrupt, truncated and invalid-name files fail with revision 0 and no half world | Pending | sonnet | |
| 3 | **Contract docs and close.** SD-S1-010 "World hand-off" section in the system spec, A4 pointer if needed, gap audit, edge tests, one full `dia run googletest --shards 4`, SLOW suites for the new tests, Observation scan, system plan row 16b Done | Full run; `dia docs precommit` is not required (docs plus tests only) | Pending | sonnet | No mutation checks (user instruction 2026-11-12) |

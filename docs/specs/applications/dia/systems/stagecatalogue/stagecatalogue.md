# System Spec: Stage Catalogue

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** debug, tools

**Code home (amended 2026-11-18):** `Cluiche/CluicheGameBaseline` by default, and `Dia/DiaGame` only for what must be engine. DiaGame keeps the `.diastage` format: the new manifest fields, the serializer and the kind vocabulary (`StageKinds`, `stage_kinds.json`), because DiaGame owns the format and Dia tooling (the Python validator) reads the vocabulary. The catalogue model, loader, results store, `StageCatalogueModule`, snapshot and commands live in the Baseline, which CluicheTest and CoW already share and which already hosts `BootMenuModule`, `StageStatusModule` and `DebugServerHostModule`. Feature 2's `Module::OnStageLaunch` and `StageLaunchContext` stay in DiaApplicationFlow (the `Module` base class is engine). Supersedes the 2026-11-18 decision to extend DiaGame (SC-001).

**Related:** @docs/specs/applications/dia/systems/diaviewlaunch/diaviewlaunch.md (Feature 4 plans `dia launch` argument passthrough, which Feature 2 here reuses).

## Purpose

A game's boot menu today is a flat list of raw stage names taken from Boot's `transitions`. In CoW that list mixes E2E tests, showcases and tool testbeds, and nothing says which is which. The only extra is a pass/fail badge, and only CluicheTest has it.

The Stage Catalogue makes the stage list a **typed model** that every consumer reads:

- the in-game boot menu (CluicheGameBaseline),
- the CLI (`dia stages list --tag tool`, `dia run cow --stage <S> --preset <P>`),
- DiaAPI / DebugServer clients (CluicheEditor, scripts, AI agents),
- generated docs and `dia check`.

Each stage carries **tags**, a **description**, named **launch presets** and the **specs it proves**. Each stage also keeps a **last result** that survives the process, so a stage run from the menu and one run by `dia run e2e` show the same badge.

## Responsibilities

- **Stage metadata format:** `.diastage` gains optional `tags`, `description`, `presets` and `proves`. DiaGame's serializer reads and writes them, and the `dia validate manifest` schema checks them.
- **Kind vocabulary:** a fixed list of kind tags, `showcase`, `tool` and `test`. Validation rejects unknown spellings that are close to a kind tag (`tools`, `Tool`); all other tags are free-form.
- **Catalogue model:** a pure `StageCatalogue` built from the `.diagame` and its stage imports, holding entries in Boot-transition order. It answers list, filter-by-tag, find and group-by-kind.
- **Results store:** the last result per stage (severity, label, UTC time, session id, session-log path), held by a module and persisted to `Cluiche/out/<App>/stage_results.json`.
- **Commands:** `stages.list`, `stages.get`, `stages.launch` and `stages.results` as DiaAPI commands, which the menu and CLI both use.
- **Launch with a preset:** one launch request type, `{stage, preset}`, used by the menu, `stages.launch` and the CLI's `--stage`/`--preset`.
- **Traceability:** `proves` lists spec file paths; `dia check stage-proves` reports broken links and specs that nothing proves.
- **Generated registry doc:** `dia docs stage-registry` writes one table per game (stage, kinds, tags, description, presets, proves).

## Non-Responsibilities

- **Drawing the menu.** `BootMenuModule` stays in CluicheGameBaseline and becomes a view over the catalogue snapshot (layer rule: the wiring and the UI stay in the application layer).
- **Deciding what a stage's result is.** Stages (or a game's test runner) report results; the catalogue only stores and shows them.
- **Test playlists, run history, readiness checks, recent and favourite stages.** These are deferred (backlog row). The results file is designed so history can be added later without changing the format.
- **Scenario files.** CoW-Shared `.scenario.json` (S-2.5 Feature 4) are game content. A CoW preset may name a scenario through ordinary module config, but the catalogue does not know about them.
- **Arbitrary key-value metadata.** Rejected 2026-11-18. Each new field must name a consumer.
- **Changing stage transitions or flow.** Boot's `transitions` stays the source of which stages exist and in what order.

## Design

### Shape

```
.diagame ─imports─► *.diastage  (name, manifest, tags, description, presets, proves)
     │                    │
     │   StageCatalogueLoader (Baseline, pure, any thread)
     ▼                    ▼
 StageCatalogue (immutable after load) ── order from Boot transitions
     │
 StageCatalogueModule (MainPU) ── owns results store ──► out/<App>/stage_results.json
     │   ▲ StageResultEvent (EventStream, SimPU → MainPU)
     │   │      reported by test stages / game test runners
     │   └─ DiaAPI: stages.list / get / results / launch
     ▼
 StageCatalogueSnapshot (FrameStream, MainPU → RenderPU)
     ▼
 BootMenuModule (Baseline, RenderPU): groups, filters, badges
     │ StageLaunchRequest {stage, preset} (EventStream, RenderPU → SimPU)
     ▼
 SimNavigationHandlerModule (Baseline) → QueueStageTransition(stage, launchContext)
```

### `.diastage` additions

```json
{
  "name": "S1WorldGenerationTestbedStage",
  "manifest": "Stages/S1WorldGenerationTestbedStage/misc/ApplicationFlow/s1world_generation_testbed_stage.diaapp",
  "tags": ["tool", "worldgen"],
  "description": "Live terrain, biome and region editor",
  "presets": [
    {
      "name": "alpine42",
      "description": "Alpine-heavy Small world, seed 42",
      "modules": {
        "WorldGenerationModule": { "preset": "alpine", "seed": 42, "size": "Small" }
      }
    }
  ],
  "proves": ["docs/specs/applications/cow/systems/s1-worldgeneration/s1-worldgeneration.md"],
  "config": { "path_aliases": { "stage_root": "." } }
}
```

- Every new field is optional. A stage with none of them behaves exactly as it does today and is listed under **Other**.
- **`tags`:** a string array with no duplicates. Strings are kept for display and hashed to `StringCRC` for lookup (PD-001).
- **`description`:** one line, at most 120 characters. The menu shows it as a hover tooltip and a dimmed second line.
- **`presets[].modules`:** keyed by module **instance id** in the stage's `.diaapp`. Each value is a JSON object merged shallowly onto that module's manifest `config`. An instance id that the stage does not declare is a validation error.
- **`proves`:** repo-relative spec paths. The runtime ignores this field; only tools read it.

### Kind vocabulary

- `showcase`: something to show a person (a demo scene, front end, a playable slice).
- `tool`: an interactive testbed for understanding or tuning a system (S-1, S-2.5, S-3...).
- `test`: has a pass/fail result (E2E stages and runner-driven stages).
- A stage may carry several kinds. The menu lists it **under every kind it has** (user decision 2026-11-18).
- The vocabulary is a constant in DiaGame (`StageKinds`) and is mirrored in the Python validator from one exported list, so the two cannot drift.

### Catalogue model (Baseline, pure)

- `StageCatalogueLoader::Load(diagamePath, outCatalogue)` reads the `.diagame`, each stage import and Boot's `transitions` from the root manifest. It has no module machinery and is safe on any thread. Its tests use fixture files.
- `StageCatalogue` holds up to `kMaxStages = 64` entries. This matches today's `BootMenuLogic` and `StageStatusModule` caps; going over the cap is a logged error, and the extra stages are dropped and counted.
- **Order:** Boot-transition order. A stage imported but missing from Boot's transitions is not listed, which keeps today's behaviour.
- Queries:
  - `Find(StringCRC)`
  - `Filter(tags[], match = all)`
  - `Kinds(entry)`
  - `GroupByKind()`, which returns rows `{groupKind, entryIndex}`. A stage appears in a row once per kind; untagged stages and stages with only free-form tags go to `other`.
- **Rows and stages are separate.** Selection, visited state, counts and the result all key on the stage; rows are only a view. "N stages" and the `boot_menu.stage_count` gauge count unique stages.

### Results store

- `StageResult`: `{severity: none|ok|warn|fail, label (≤ 16 chars, e.g. PASS/FAIL/TIMEOUT), utc, session_id, log_path}`.
- **Reporting:** any module sends `StageResultEvent {stage, result}` on the `StageResult` EventStream (SimPU → MainPU). CluicheTest's `TestStatusBridgeModule` becomes an adapter that turns its `TestResultsRegistry` results into these events. CoW's `F2TestRunnerModule` sends them directly.
- **Persistence:** `StageCatalogueModule` (MainPU) loads `Cluiche/out/<App>/stage_results.json` on start. It writes the file atomically (temp file then rename) each time a result arrives, at most once per frame. Format: `{format: "dia.stage_results", version: 1, results: {<stage>: StageResult}}`.
- **Same badge from either path.** `dia run e2e` runs the same game process, so a test reports through the same event whether a person picked the stage or the harness drove it. The CLI does not write results itself.
- **Visited** stays session-only menu state. It is not a result: a test stage that was visited but has no result shows as pending.
- `StageStatusModule` (Baseline) is retired once the menu reads the snapshot. Its generic label-and-severity table is what the results store replaces.

### Snapshot to the menu

- `StageCatalogueModule` publishes a `StageCatalogueSnapshot` FrameStream (MainPU → RenderPU): the immutable entries (built once) plus the results and a revision number. RenderPU never reads MainPU state directly; PU boundaries are always streams.
- The FrameStream is single-writer, and the snapshot is rebuilt only when the revision changes.

### Commands

| Command | Args | Returns |
|---|---|---|
| `stages.list` | `tag?` (repeatable, all must match), `kind?` | entries: name, kinds, tags, description, preset names, last result |
| `stages.get` | `stage` | full entry, including presets and `proves` |
| `stages.results` | `stage?` | last result(s) |
| `stages.launch` | `stage`, `preset?` | `{ok}`; error if the stage or preset is unknown, or the game is not at Boot |

- Registered by `StageCatalogueModule` on MainPU, with mandatory help text.
- `stages.launch` sends the same `StageLaunchRequest` as the menu. There is one launch path.
- `dia stages list|get|results` always reads the files: the catalogue sources and `stage_results.json`, which the game writes on every result. They give the same answer as the live commands. Live data is `dia query stages.list` (simplified 2026-11-18 while writing Feature 1).

### Launch with a preset (engine hook)

- Modules get their config **once, at boot** (`Application::BuildFromManifest` → `OnConfigure`). A preset picked from the menu partway through a run therefore needs a per-launch hook:
  - DiaApplicationFlow gains `QueueStageTransition(stage, const StageLaunchContext&)`.
  - It also gains a `Module::OnStageLaunch(const char* overridesJson)` virtual. The default is a no-op.
  - The application calls `OnStageLaunch` on each module scoped to the target stage, on the main thread, before that module's `DoStart` for the stage. The argument is that module's merged override, or `""` when there is no preset.
- Modules that support presets read the override in `OnStageLaunch`. Modules that don't support them are unaffected.
- **CLI:** `dia run|launch <target> --stage <S> [--preset <P>]` passes `--stage`/`--preset` to the exe through the argument passthrough planned in DiaViewLaunch Feature 4. The application then queues Boot → S with that preset once Boot is ready. Validation (`stage exists`, `preset exists`) runs in Python first, so mistakes fail fast with a clear error.
- **Menu:** a stage with presets shows them as indented sub-rows. Clicking the stage row launches it with no preset.

### Traceability (`proves`)

- `dia check stage-proves`:
  - Resolves every `proves` path in every game's stages. A missing file is an error.
  - Reads each spec's Status.
  - Reports **unproven specs**: specs that opt in with `**Testbed:** required` in their header and are Approved, In Progress or Done, but are named by no stage.
  - Writes `out/check/stage-proves.md` and exits non-zero on broken links only.
- Opt-in is deliberate. Most Dia systems (DiaMaths, DiaCore) are proved by GoogleTests, not by stages, so flagging them all would make the report useless.

### Menu (Baseline, view only)

- Kind groups as collapsible headers in the order Showcase, Tools, Tests, Other. Tests is collapsed by default, and its header shows a passed/failed/pending rollup.
- A filter bar of tag chips (union of all tags, kinds first) plus a text search over name and description.
- The pass/fail badge is shown only on `test` stages, on each of their rows.
- E2E stages are launchable from the menu (user decision 2026-11-18).
- **Mockup gate:** an approved HTML mockup of the grouped menu is required before the menu is rebuilt.

### Logging and observation

- **Logs:** `STAGES` log channel. It covers catalogue loaded (count, dropped), validation warnings, result received and persisted, file write failure, launch requested (stage, preset), unknown stage or preset refused.
- **Metrics:**
  - `stages.catalogue.count` gauge.
  - `stages.results.write_fail` counter.
  - `boot_menu.stage_count` keeps counting unique stages.
- **Health:** `StageCatalogueModule` reports degraded when the results file can't be written.

## Features

| # | Feature | Notes | Status | Spec |
|---|---|---|---|---|
| 1 | Catalogue, tags and results | `.diastage` `tags`/`description` + serializer + validator; `StageKinds` + `stage_kinds.json`; pure `StageCatalogue` + loader (C++ and Python, shared fixtures); `StageCatalogueModule` (MainPU): results store, `stage_results.json` (merge-before-write), `StageResult` EventStream in, snapshot FrameStream out, `stages.list/get/results`; CluicheTest `TestResultReporterModule` (SimPU) + CoW F2 runner send results; tag all stages; `dia stages`; `dia docs stage-registry`. Menu unchanged. AD-001 sign-off | Approved | [catalogue-and-results.md](catalogue-and-results.md) |
| 2 | Launch presets | `presets` (stage-scoped module instances only); `ManifestComposerV2` reads them; `Module::OnStageLaunch` + `TransitionTo(stage, preset)` + `SetInitialLaunch` (AD-001 sign-off); nav request and automation hold carry a preset; `stages.launch` over a MainPU → SimPU stream; menu preset sub-rows; `dia run/launch --stage --preset`; S-1 `WorldGenerationModule` adopts first | Approved | [launch-presets.md](launch-presets.md) |
| 3 | `proves` traceability | `proves` (spec paths) + validation; `**Testbed:** required` opt-in; `dia check stage-proves` (broken links exit 1, `--strict` for unproven) + report; registry column; precommit hook; CoW adoption baseline | Approved | [proves-traceability.md](proves-traceability.md) |
| 4 | Boot menu view | **Mockup gate first.** `BootMenuModule` rebuilt on the snapshot: kind groups, multi-group rows, chips (AND), search, descriptions, badges on `test` only, Tests collapsed with rollup, fallback flat list; `StageStatusModule` + `TestStatusBridgeModule` retired (split out of Feature 1, user decision 2026-11-18) | Approved | [boot-menu-view.md](boot-menu-view.md) |

**Build order:** 1 first. Then 3 and 4 can be built in parallel (3 needs only 1's format work and loader; 4 needs 1's snapshot and its mockup). 2 comes last, because its menu sub-rows need 4. Its runtime and CLI parts need only 1, so they can start earlier if wanted.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| DiaGame | `.diastage` format, serializer, `StageKinds`, `DiaGameManifestLoader` (extended here) | Done (extended) |
| CluicheGameBaseline | Catalogue, loader, results, `StageCatalogueModule`, snapshot | Gains new code (app layer) |
| DiaApplicationFlow | `GetStageTransitions`, `QueueStageTransition`, module lifecycle; gains `StageLaunchContext` + `OnStageLaunch` in Feature 2 | Done (extended in F2) |
| DiaStreams | `StageResult` EventStream, `StageCatalogueSnapshot` FrameStream, `StageLaunchRequest` EventStream | Done |
| DiaAPI / DiaDebugServer | `stages.*` commands, reachable through `dia query` | Done |
| DiaObservation | Session id and log path for results; logs, metrics, health | Done |
| DiaCLI | `dia stages`, `dia validate manifest`, `dia check stage-proves`, `dia docs stage-registry`, `--stage/--preset` | Done (extended) |
| DiaViewLaunch Feature 4 | `dia launch` argument passthrough for `--stage/--preset` | Not started; F2 builds the passthrough itself if it hasn't landed |
| CluicheGameBaseline | `BootMenuModule`, `SimNavigationHandlerModule` (consumers, rebuilt in F1/F2) | Done (rebuilt here) |

## Decisions

| ID | Decision | Status | Binding |
|---|---|---|---|
| SC-001 | Baseline by default, DiaGame only where it must be engine: the `.diastage` format fields, serializer and kind vocabulary stay in DiaGame; catalogue, loader, results, module and snapshot live in CluicheGameBaseline (user decision 2026-11-18, amended the same day) | Accepted | Yes |
| SC-002 | Stage metadata is tags plus named fields (`description`, `presets`, `proves`); no key-value store. A new field needs a named consumer (user decision 2026-11-18) | Accepted | Yes |
| SC-003 | Kind tags are a fixed, validated vocabulary (`showcase`, `tool`, `test`); other tags are free-form. One exported list feeds C++ and Python | Accepted | Yes |
| SC-004 | A stage appears under every kind group it has; selection, visited, counts and results key on the stage, never the row (user decision 2026-11-18) | Accepted | Yes |
| SC-005 | E2E stages are launchable from the menu (user decision 2026-11-18) | Accepted | Yes |
| SC-006 | The last result per stage persists in `Cluiche/out/<App>/stage_results.json`; written only by the game process, so menu and CLI runs share one badge (user decision 2026-11-18) | Accepted | Yes |
| SC-007 | Presets live inline in `.diastage` as per-module-instance config overrides (user decision 2026-11-18) | Accepted | Yes |
| SC-008 | `proves` names repo-relative spec file paths; specs opt in to "must be proved" with `**Testbed:** required` (user decision 2026-11-18 for paths) | Accepted | Yes |
| SC-009 | One launch path: menu, `stages.launch` and CLI all produce `StageLaunchRequest {stage, preset}` | Accepted | Yes |
| SC-010 | The menu is a view over a typed model; every action it offers is a `stages.*` command (UI/business separation, editor actions via DiaAPI) | Accepted | Yes |

## Inherited Binding Decisions

| ID | Decision | How it constrains Stage Catalogue |
|---|---|---|
| PD-001 | StringCRC for IDs | Stage names, tags, kinds and preset names are `StringCRC` in C++; strings kept for display and the file format |
| PD-002 / AD-004 | ProcessingUnit/Phase/Module | `StageCatalogueModule` is a MainPU module; results arrive and leave by streams (`StageResult` in, snapshot out, `StageLaunchRequest` to SimPU). No cross-PU reads, including of the immutable catalogue: the menu gets it in the snapshot |
| PD-004 / AD-002 | No STL in public APIs | `StageCatalogue`, `StageResult`, `StageLaunchContext` use Dia containers; Json values cross the API as `const char*` like `OnConfigure` |
| PD-009 | Output under `Cluiche/out/<AppName>/` | `stage_results.json` in `out/<App>/`; check report in `out/check/` |
| PD-010 (amended 2026-11-18) | `.diagame` is root; `.diastage` is tooling metadata, and may also carry launch parameters applied through the opt-in `OnStageLaunch` hook | Tags, description and `proves` are pure metadata. Presets are launch parameters: they change stage-scoped module config at stage entry and never change which modules are built |
| AD-001 | Module docs; sign-off for engine API changes | F1 engine edits are the DiaGame format fields, serializer and `StageKinds` (sign-off given 2026-11-18); the runtime module now lives in the Baseline and needs none. F2 needs DiaApplicationFlow `OnStageLaunch` + `StageLaunchContext` (sign-off given 2026-11-18) |
| AD-003 | `Dia::<Module>::` namespaces | `Dia::Game::` for the format and `StageKinds` only; the Baseline types use the Baseline's `Cluiche::AppFlow::` namespace |
| DDS-004 | DebugServer is Debug-only | `stages.*` via `dia query` works on Debug builds; the in-game menu and the file-based `dia stages` work in Release too |

## Test Strategy

- **GoogleTests:**
  - Serializer round trip with and without each new field.
  - Unknown near-kind tag rejected; duplicate tag rejected.
  - Loader against fixture `.diagame`/`.diastage` sets: order follows Boot transitions; an imported stage missing from the transitions is not listed; 65 stages drops one and counts it.
  - `GroupByKind`: one stage with two kinds gives two rows and one stage; free-form-only tags go to `other`.
  - `Filter` with all-match.
  - Results store: load missing, corrupt or old-version file (start empty, log); atomic write; event updates the revision once per frame.
  - Snapshot carries results to a fake reader.
  - **F2:**
    - `OnStageLaunch` is called before `DoStart`, only for modules scoped to the target stage, with the merged override.
    - A launch with no preset passes `""`.
    - An unknown preset is refused.
  - `stages.*` command replies and errors.
- **Pure `BootMenuLogic` tests:**
  - Rows derived from the snapshot.
  - Visited and selection shared across a stage's rows.
  - Rollup counts unique test stages.
  - Badge only on `test`.
- **pytest (DiaCLI):**
  - Schema validation of the new fields.
  - Python loader matches C++ on the same fixtures (shared fixture set).
  - `dia stages list --tag` against files.
  - `--stage/--preset` validation errors before launch.
  - `dia check stage-proves` against fixture specs: broken link, unproven opt-in spec, proven spec.
  - Registry doc generation.
- **E2E:**
  1. `dia run e2e` on a CluicheTest stage writes `stage_results.json`.
  2. Launching CluicheTest afterwards shows that badge in the menu snapshot (`stages.results`).
  3. `dia run cow --stage S1WorldGenerationTestbedStage --preset <p>` reaches the stage with the preset applied (checked through a worldgen query).
- **Manual proof:**
  1. `dia run cow` shows Showcase, Tools, Tests and Other groups.
  2. A multi-kind stage appears in both of its groups.
  3. A tag chip filters the list.
  4. A test stage run from the menu shows its badge after returning to Boot and after restarting the game.

## Resolved Decisions

1. **Presets and PD-010** (user decision 2026-11-18): PD-010 is amended in `Cluiche.md`. A `.diastage` may carry launch parameters applied through the opt-in `Module::OnStageLaunch` hook, and these never change which modules are built. Presets stay inline (SC-007).
2. **Module declaration** (user decision 2026-11-18): `StageCatalogueModule` is declared per app in its `.diaapp` (CoW, CluicheTest). Nothing is added automatically.
3. **Results-file contention** (user decision 2026-11-18): last write wins. Each write first merges with the file on disk, keeping the newer result per stage, then writes through a temp file and a rename. Only two writes for the same stage race.
4. **Menu split out** (user decision 2026-11-18): the menu rebuild is Feature 4, gated on its mockup, so Feature 1 can be built while the mockup is reviewed.

## Open Design Questions

_None open at system level._ Feature-level questions are in each feature spec.

## Status

`Approved` (2026-11-18, user sign-off). All four feature specs Approved 2026-11-18. Next: `/implement` Feature 1, and the Feature 4 mockup in parallel. System questions resolved. AD-001 sign-offs are asked when Features 1 and 2 are built.

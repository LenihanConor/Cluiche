# Plan: Catalogue, Tags and Results (Stage Catalogue Feature 1)

**Spec:** @docs/specs/applications/dia/systems/stagecatalogue/catalogue-and-results.md
**Status:** Not Started
**Order:** this plan first. Then `proves-traceability.plan.md` (F3) and the F4 mockup in parallel, then `launch-presets.plan.md` (F2). Those plans are written when each starts.

## Sign-offs (AD-001)

| Change | Where | Status |
|---|---|---|
| `.diastage` fields, serializer, `StageKinds` + `stage_kinds.json`, DiaGame module doc | DiaGame | Given 2026-11-18 |
| Placement: Baseline by default; only the format and vocabulary in DiaGame (SC-001 amended) | Escalation rule | Confirmed 2026-11-18 |

## Parallel structure

Task 1 (fixtures) is the contract for both tracks. After it, two tracks run concurrently as separate subagents because their files are disjoint:

- **Track P (Python, tasks 2-5):** touches only `Dia/DiaCLI`. No vcxproj.
- **Track C (C++, tasks 6-13):** touches DiaGame (format only), CluicheGameBaseline, CluicheTest, CoW and vcxproj files. Tasks inside Track C run in order (shared vcxproj and headers).
- **Track M (mechanical, task 14):** tagging every `.diastage`. Touches only `.diastage` files; runs after Task 2 (the validator) so each file is checked as it is edited.
- Join at task 15 (E2E) and 16 (gap audit).

## Resolved before planning (2026-11-18)

- Snapshot omits `logPath` (spec AC 14). `static_assert` on snapshot size stays.
- RUNNING is persisted as specced.

## Implementation Patterns

- Namespace `Dia::Game::` for the DiaGame format and `StageKinds`; `Cluiche::AppFlow::` for everything in the Baseline. Fixed-size Dia strings and `DynamicArrayC`; no STL in public headers. Never `str = cstr` on `String<N>` (it formats); use a bounded copy helper.
- Names, tags, kinds and presets hash to `StringCRC`; strings kept for display and the file format.
- `StageKinds.h` and `stage_kinds.json` are the single vocabulary; the Python validator reads the JSON.
- Pure first: `StageCatalogue`, `StageCatalogueLoader`, `StageResultsFile` have no module machinery and are tested on fixtures. The module is a thin wrapper tested with fake streams.
- Results file write: read disk, keep newer `utcMs` per stage, write `<path>.tmp`, rename.
- Module follows `ValueSchemaModule` for the optional `DebugServerService` and `DebugServerHostModule` for `diagame_path`.
- Test helpers for the catalogue live in `Cluiche/CluicheGameBaseline/Testing/` (ship with the library); tests in `Tests/GoogleTests/CluicheGameBaseline/`.
- Add files with `dia docs vcxproj-add` from PowerShell or with literal paths (bash-loop bug).
- Scoped runs while iterating: `--filter="*StageCatalogue*:*StageKinds*:*StageResults*:*JsonDiaStage*"`, always `--shards 2` or more. One full run at the end (Task 16).

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Placement is confirmed (Baseline by default). Record baseline: one scoped GoogleTests run of `*DiaGame*:*Manifest*`, and `dia validate manifest` result | — | Done | haiku | No code. Gate for later tasks; Placement confirmed. Baseline scoped run (DiaGame/Manifest/JsonDia): 1 pre-existing failure ManifestSaver.RoundTrip_PreservesOverflowAndMultiWriter, unrelated |
| 1 | **Shared fixtures (the contract).** `Cluiche/Tests/Fixtures/StageCatalogue/`: a `.diagame` with Boot transitions, stages with tags/description, a transition with no import, an imported stage missing from transitions, a 65-stage set, malformed `.diagame`; plus `expected_catalogue.json` (order, group rows, omitted and dropped counts) | Fixtures load by hand; expected JSON reviewed against AC 5-6 | Done | sonnet | Both tracks assert against this expected file (AC 7); Fixtures committed 7f7cbaa1. Pinned conventions: rows ascending entry index within group; tags as written, kinds canonical order; absent description is empty string |
| 2 | **P:** RED → GREEN: `stage_kinds.json`; validator rules in `cli_validate.py` (kebab-case, unique, 16 max, near-kind error naming the kind, description ≤ 120) | AC 3: pytest cases for each rule and message | Done | sonnet | JSON is created here; Task 6 adds the C++ twin; 32 pytest pass (RED: import error). Damerau edit distance so tset->test. Fixtures dir excluded from repo-wide validate scans. dia validate manifest: 80 checked, 0 errors |
| 3 | **P:** RED → GREEN: `utils/stage_catalogue.py` loader (order, no-import, omitted, dropped, queries, `GroupByKind`) | AC 7: output equals `expected_catalogue.json` | Done | sonnet | 10 pytest pass (RED: module missing). Contract for C++ twin: first manifest import is root, Boot stage transitions; unreadable imported .diastage => Malformed; filter exact case-sensitive, empty filter returns all |
| 4 | **P:** RED → GREEN: `dia stages list/get/results` (`--game`, `--tag`, `--kind`, `--json`) reading files only | AC 21: tag/kind/json against fixtures; missing results file | Done | sonnet | JSON matches the `stages.*` reply shapes; 13 pytest (RED: collection error). Loader now falls back to unique file-name match for source layouts (CoW cow_main.diaapp is flat). dia stages list --game cow/cluichetest work |
| 5 | **P:** RED → GREEN: `dia docs stage-registry` writes `docs/reference/registry/stage-registry.md` | AC 22: generated table on fixtures | Done | haiku | Register in `cli/docs.py`; 10 pytest (RED: module missing). Generated file not committed until tagging lands |
| 6 | **C:** RED → GREEN: `DiaStageManifest` `tags`/`description`; `JsonDiaStageSerializer` load/save (omit when empty, wrong-type message names the field); `StageKinds.h` + GoogleTest that matches `stage_kinds.json` | AC 1-2: round trip with and without fields, wrong type, JSON/C++ table equal | Done | sonnet | `dia docs vcxproj-add` DiaGame + GoogleTests; RED 5 failed/4 pass; GREEN 17 new tests; regression filter 110/shard 0 fail. Sized-string assign uses String("%s",x) pattern (operator= formats). Over-limit values fail, never truncate |
| 7 | **C:** RED → GREEN: `StageCatalogueEntry`, `StageCatalogue`, `StageCatalogueLoader::Load`, queries (`Find`, `Kinds`, `Filter`, `GroupByKind`); `Testing/` fixture helper | AC 4-6: fixtures; output equals `expected_catalogue.json`; 65 stages → dropped 1; missing and malformed `.diagame` | Done | sonnet | Same expected file as Task 3; RED 21/22 fail; GREEN 22 pass; regression 69 pass. Loader parses .diagame/.diaapp with Json::Reader (JsonDiaGameSerializer caps imports at 16). Baseline gained a DiaGame ProjectReference. GOTCHA: String128("%s", x.AsCStr()) crashed (UB, root cause unknown) so use AppendAsMuchAsCan; follow-up for Loose Ends |
| 8 | **C:** RED → GREEN: `StageResult`, `StageResultEvent`, `StageResultsFile::Read/Write` with merge-before-write and atomic rename | AC 8-10: missing, malformed, wrong format, version 2; newer-wins merge; temp renamed | Done | sonnet | RED 12 fail; GREEN 11 C++ + 2 pytest; regression 20/shard. Deviations: no String16 in Dia (label is String32 capped at 16 via SetLabel); String256 holds 255 so logPath truncates at 255 |
| 9 | **C:** RED → GREEN: `StageCatalogueSnapshot` type (no `logPath`, `static_assert` ≤ 48 KB) and `StageCatalogueModule`: config, DoStart load, event → table → one write per frame, publish on revision change only, health Degraded on write failure, session id/log path fill-in | AC 11-14, 16: fake streams; two events in one frame give one write; snapshot only on revision change; no persistence path logs one Warning | Not Started | sonnet | Metrics `stages.catalogue.count`, `stages.results.write_fail`; `STAGES` log channel |
| 10 | **C:** RED → GREEN: `stages.list/get/results` queries with help text; unknown stage `{error}`; absent `DebugServerService` runs without them | AC 15: replies and errors against a fake service | Not Started | sonnet | |
| 11 | **C:** CluicheTest `TestResultReporterModule` (SimPU): state change → `StageResultEvent` (PASS/FAIL/TIMEOUT/RUNNING) | AC 18: fake registry states map to the right events | Not Started | sonnet | Reads the singleton on its own PU (ODQ 1 stays open) |
| 12 | **C:** CoW `F2TestRunnerModule` sends PASS/FAIL on run completion | AC 19: all-pass and one-fail cases | Not Started | sonnet | |
| 13 | **C:** Manifests: `StageCatalogueModule`, `StageResult` EventStream (drop-oldest, 16) and snapshot FrameStream in `cow_main.diaapp` and `cluiche_main.diaapp`; DiaGame module doc; `dia validate manifest` | AC 17, 23-24: validate passes for both; a game without the module and a `.diastage` with no new fields behave as before | Not Started | haiku | |
| 14 | **M:** Tag and describe every `.diastage`: CoW (`FrontEnd` showcase, `F2CoreRuntime` test, `S1WorldGenerationTestbedStage` tool+worldgen) and every CluicheTest stage (`test` + one domain tag) | AC 20: `dia validate manifest` passes; `dia stages list --game cow` and `--game cluichetest` show every stage tagged | Done | haiku | After Task 2. Pure editing; split per app if parallel; 32 .diastage files tagged (3 CoW, 29 CluicheTest); validate manifest 80 checked 0 errors. JobSystemServiceTestStage is in Boot transitions with no .diastage import so it lists untagged (Other): add a .diastage if it should be a test |
| 15 | E2E: `dia run e2e` on a CluicheTest stage writes `out/CluicheTest/stage_results.json` with PASS; second launch answers `stages.results`; `dia stages results --game cluichetest` prints it | The three E2E steps and the manual `dia stages list --game cow` | Not Started | sonnet | Kill leftover `CluicheTest.exe` first; `dia test e2e` does not build, so run compile-code first |
| 16 | Close-out gap audit and edge tests (no mutation checks). One full `dia run googletest --shards 2`. Observation scan. Spec Status and backlog row updated | Full suite pass; gaps listed and covered | Not Started | sonnet | Sanitizer only on request. Update the plan row with `dia docs plan` |

## Notes

- **Project per task:** Task 6 is DiaGame (+ GoogleTests). Tasks 7-10 are CluicheGameBaseline (+ GoogleTests): `dia docs vcxproj-add CluicheGameBaseline ...`. Task 11 is CluicheTest, Task 12 CoW.
- Report results only (pass/fail, failing names, errors) per the build-output rule.
- Commit after each task; update this plan in the same commit.

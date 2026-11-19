# Plan: `proves` Traceability (Stage Catalogue Feature 3)

**Spec:** @docs/specs/applications/dia/systems/stagecatalogue/proves-traceability.md
**Status:** In Progress
**Prereq:** Feature 1 plan (`catalogue-and-results.plan.md`) is Done. Reuses its Python loader (`utils/stage_catalogue.py`), `stage_registry_cmd.py` and the `.diastage` validator.

## Sign-offs

| Change | Where | Status |
|---|---|---|
| `proves` field on `DiaStageManifest` + serializer (existing headers, no new files; spec names DiaGame) | DiaGame | Covered by approved spec (SC-002, PD-010) |

## Parallel structure

- **Track C (C++):** Task 1 only — DiaGame + GoogleTests.
- **Track P (Python):** Tasks 2-6, in order (shared `check.py`, `cli_validate.py`).
- Tasks 1 and 2 touch disjoint files and may run concurrently. Task 7 (adoption) needs 1, 2 and 3.
- The spec has no Tasks section; tasks below are derived from its ACs.

## Implementation Patterns

- C++: `Dia::Game::`, fixed-size Dia strings, `DynamicArrayC<String256, 16>`. Never `str = cstr` on `String<N>` (it formats). Over-limit values fail, never truncate (F1 Task 6 precedent). Add files with `dia docs vcxproj-add` (PowerShell or literal paths).
- Python: new check in `commands/check/stage_proves.py`; registered in `cli/check.py` as `stage-proves`. Use the F1 loader to read stages.
- Scoped runs: `--filter="*JsonDiaStage*:*DiaStageManifest*:*Manifest*"`, `--shards 2` or more. One full run at the end only.
- Report output under `Cluiche/out/check/`.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | **C:** RED → GREEN: `DiaStageManifest::proves` (`DynamicArrayC<String256, 16>`); `JsonDiaStageSerializer` load/save (omit when empty; wrong-type message names the field; >16 or over-length fails); `ManifestComposerV2` ignores it | AC 1, 3: round trip with and without `proves`, wrong type, 17 entries, composer output unchanged | Pending | sonnet | RED-GREEN. Solo verify. Update DiaGame module doc if it lists stage fields |
| 2 | **P:** RED → GREEN: `.diastage` `proves` shape validation in `cli_validate.py` (array of strings, `docs/specs/` prefix, `.md` suffix, no duplicates, ≤16; missing file = warning) | AC 1-2: pytest per rule and message; missing path is a warning, not an error | Done | sonnet | RED-GREEN. Solo verify; RED import error; GREEN 75 pytest pass; validate manifest 80/0 errors. _validate_diastage_full returns (errors, warnings) |
| 3 | **P:** RED → GREEN: `commands/check/stage_proves.py` core: spec marker parse (`**Testbed:** required`, header block before first `## `, case-insensitive), status parse (`## Status` first backticked word, else `**Status:**` header), broken-link and unproven detection, coverage map | AC 4-6: fixture repo tree; Draft not flagged; non-opt-in never flagged; marker case; both status sources; feature-spec proves counts as coverage | Done | sonnet | RED-GREEN. Pure functions over a repo root; no click yet; RED import error; GREEN 59 pytest pass. StageEntry.proves + StageCatalogue.imported added to loader |
| 4 | **P:** RED → GREEN: `dia check stage-proves [--game] [--strict] [--json]` in `cli/check.py`; console summary, `Cluiche/out/check/stage-proves.md` (Broken links, Unproven specs, Coverage), exit codes, `--help` documents marker | AC 5, 7, 8: exit 1 on broken link; unproven exit 0, exit 1 with `--strict`; report and `--json` content | Pending | sonnet | RED-GREEN. `check.py` has unrelated uncommitted edits: stage only this task's hunks |
| 5 | **P:** RED → GREEN: stage-registry gains "Proves" column with links | AC 10: generated table on fixtures | Pending | haiku | Edit `stage_registry_cmd.py` |
| 6 | **P:** RED → GREEN: `dia docs precommit` runs the check without `--strict`; broken links fail precommit | AC 9: precommit integration test | Pending | sonnet | `precommit_cmd.py` |
| 7 | **Adoption:** add `**Testbed:** required` to S-1, S-2.5, S-3, S-3.5 testbed system specs and CoW world-layer system specs (S-1 worldgen, A4, A1, A3); `proves` on `S1WorldGenerationTestbedStage` (S-1) and `F2CoreRuntime` (F-2); run `dia validate manifest` and `dia check stage-proves`; record the baseline output in Notes | AC 11: validate passes; check output equals the expected unproven list | Pending | haiku | Spec files edited are header lines only. Baseline recorded below |
| 8 | Close-out gap audit and edge tests (no mutation checks). One full `dia run googletest --shards 2`. Observation scan. Spec Status Done, backlog updated | Full suite pass; gaps listed and covered | Pending | sonnet | Sanitizer only on request. Use `dia docs spec-done` |

## Baseline (Task 7)

_Recorded when Task 7 completes._

## Notes

- Report results only (pass/fail, failing names, errors) per the build-output rule.
- Commit after each task; update this plan in the same commit.

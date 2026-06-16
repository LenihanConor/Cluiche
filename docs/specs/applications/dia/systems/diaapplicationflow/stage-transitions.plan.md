# Plan: Stage Transitions (Manifest v3)

## Spec
[stage-transitions.md](stage-transitions.md) — Approved 2026-05-20

## Session Notes

### Spec decisions summary

DiaApplicationFlow's `.diaapp` manifest bumps from v2 to v3 to support **branching stage graphs**. v2's `stages: string[]` + top-level `auto_stages: string[]` can only encode linear flows; per-stage objects with explicit `transitions[]` and `auto_advance: bool` express branching natively.

**Binding decisions in scope:** PD-001 (StringCRC for IDs — stage names + transition targets stay CRC-keyed in memory, strings on disk), PD-007 (C++20 — DynamicArrayC + designated initializers), AD-003 (`Dia::ApplicationFlow::` namespace — all new code), SD-001 (config sole source of truth — transitions explicit, no implicit "next-in-array"), SD-002 (Stages replace Phases — reinforced), SD-005 (transitions queued — auto-advance still queues `TransitionTo`), SD-014 (full validation at load — 4 new rules tighten the guarantee), SD-017 (clean break, no v1 compat — extended: no v2 compat either), **SD-019 (introduced by this spec — manifest v3 with per-stage transitions)**.

**Hard cutover:** loader rejects v2 with `kVersionMismatch`. The ~5 source `.diaapp` files migrate by hand. No shim, no compat path. Worktree copies and `bin/` artifacts regenerate.

**Struct rename:** `ApplicationManifestV2` → `ApplicationManifestV3` (file, struct, loader class, validator class). All callers updated in one pass.

**Validator additions:** `TRANSITION_TARGET_INVALID` (E), `AUTO_ADVANCE_AMBIGUOUS` (E), `TRANSITION_SELF_LOOP` (W), `STAGE_UNREACHABLE` (W). The existing `UNKNOWN_STAGE` rule loses its `autoStages` arm.

**Runtime change is narrow:** the auto-advance block in `Application.cpp::ApplyPendingTransition` (today: scan `autoStages`, advance to next array index) replaced with: find StageDeclaration for `newStage`, if `autoAdvance && transitions.Size()==1`, `TransitionTo(transitions[0])`. Defensive log if `autoAdvance && transitions.Size()!=1` (validator should have caught it).

**Editor adoption is out of scope** — handled in Step 8b (`stages-tab` revision + `StageConfiguration` sidebar transitions chip-list editor). This spec is engine-side only.

### Key file map

- `Dia/DiaApplicationFlow/Manifest/ApplicationManifestV2.h` → **rename** `ApplicationManifestV3.h`
- `Dia/DiaApplicationFlow/Manifest/ApplicationManifestV2.cpp` → **rename** `ApplicationManifestV3.cpp`
- `Dia/DiaApplicationFlow/Manifest/ApplicationManifestLoaderV2.h/cpp` → **rename** `ApplicationManifestLoaderV3.h/cpp`
- `Dia/DiaApplicationFlow/Manifest/ManifestValidatorV2.h/cpp` → **rename** `ManifestValidatorV3.h/cpp`
- `Dia/DiaApplicationFlow/Application.cpp` — auto-advance block (lines ~737–758)
- `Dia/DiaApplicationFlow/Application.h` — caller of manifest type
- `Dia/DiaApplicationFlow/DiaApplicationFlow.vcxproj` + `.filters` — file renames
- `GoogleTests/DiaApplicationFlowTests/...` — fixture rebuilds + new validator test cases
- **Source manifests to migrate by hand:**
  - `Cluiche/Assets/CluicheTest/Global/Misc/ApplicationFlow/cluiche_main.diaapp`
  - `Cluiche/Assets/Stages/DummyStage/Misc/ApplicationFlow/dummy_stage.diaapp`
  - `Cluiche/CluicheEditor/Data/editor.diaapp`
  - `Cluiche/CluicheTest/ApplicationFlow/example_app.diaapp` (verify if source-of-truth)
  - `Cluiche/CluicheTest/ApplicationFlow/test_manifest.diaapp` (verify if source-of-truth)
- **Editor (deferred to Step 8b, not this plan):**
  - `Dia/DiaApplicationEditor/UI/src/v2/types.ts` — StageV2 shape change
  - `Dia/DiaApplicationEditor/...` — JS payload + StageConfiguration sidebar
- `docs/specs/systems/dia/diaapplicationflow.md` — SD-019 added (already done at spec approval); Stage System feature row note already updated

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Rename `ApplicationManifestV2.{h,cpp}` → `V3`; rename struct + manifest type; add `transitions: DynamicArrayC<StringCRC, 8>` + `autoAdvance: bool` to `StageDeclaration`; remove `autoStages` from `ApplicationManifestV3`; update `.vcxproj` + `.filters`; update direct callers in `Application.h/.cpp` to compile | `dia pipeline --target cluichetest` reaches link stage (test fixtures may still fail — fixed in later tasks) | Todo | sonnet | Plain rename + field add. Touches Application.cpp, Application.h, ApplicationManifestLoaderV2.cpp, ManifestValidatorV2.cpp. No logic change yet. Existing tests will break — that's expected, fixed in tasks 2–6 |
| 2 | Rename `ApplicationManifestLoaderV2.{h,cpp}` → `V3`; bump expected version to 3 in `ParseJson`; rewrite `stages` parsing to require object form (read `name`, `transitions[]`, `auto_advance`, optional `manifestPath`); drop `auto_stages` reading entirely | New GoogleTest in `LoaderV3Test.cpp`: round-trips a v3 manifest with branching transitions; rejects a v2-shaped manifest with `LoadResult::kVersionMismatch` | Todo | sonnet | String-form stage entries log warning + skip (defensive — version check rejects file first) |
| 3 | Rename `ManifestValidatorV2.{h,cpp}` → `V3`; update `CheckStageReferences` to drop the `autoStages` loop; existing rule still fires for `initialStage` and module stage refs | Existing validator tests pass after fixture rebuild for v3 shape; one test updated for v3 | Todo | sonnet | Mechanical removal — drop ~5 lines |
| 4 | Implement `CheckTransitionTargets` (`TRANSITION_TARGET_INVALID`, Error) + `CheckAutoAdvanceConsistency` (`AUTO_ADVANCE_AMBIGUOUS`, Error); wire both into `Validate()` | New GoogleTest cases: each rule fires on synthetic bad manifest (missing target / auto_advance with !=1 transitions), doesn't fire on good | Todo | sonnet | |
| 5 | Implement `CheckTransitionSelfLoops` (`TRANSITION_SELF_LOOP`, Warning) + `CheckStageReachability` (`STAGE_UNREACHABLE`, Warning); wire into `Validate()` | New GoogleTest cases: self-loop fires on a→a; reachability fires on isolated stage; neither fires on clean manifest | Todo | sonnet | Reachability builds the union set of all `transitions` then checks each non-initial stage |
| 6 | Update `Application.cpp` auto-advance block (currently lines ~737–758): replace "scan autoStages, advance to array[index+1]" with "find StageDeclaration for newStage; if `autoAdvance && transitions.Size()==1`, TransitionTo(transitions[0])"; add defensive WARNING log for `autoAdvance && transitions.Size()!=1` | CluicheTest auto-advance integration test passes with a v3 manifest where stage A has `auto_advance: true, transitions: [B]` — boots A and advances to B | Todo | sonnet | Keep the queued `TransitionTo` mechanic (SD-005 unchanged) |
| 7 | Migrate source `.diaapp` files to v3 by hand: bump `version` to 3, convert `stages` array of strings to objects with `name` + `transitions[]` + `auto_advance: false`, remove `auto_stages` field. Files: `cluiche_main.diaapp`, `dummy_stage.diaapp`, `editor.diaapp`. Verify and migrate `example_app.diaapp` + `test_manifest.diaapp` if source-of-truth (else skip) | `dia run cluichetest` boots and shuts down cleanly with quoted output | Todo | haiku | Mechanical JSON edit. Boot's transitions = ["DummyStage"]; DummyStage's transitions = ["Boot"] (per spec design intent, even though current runtime never uses the back-edge) |
| 8 | Run full pipeline + tests: `dia pipeline --target cluichetest`, `dia run cluichetest`, `dia run googletest --filter="*Manifest*"`, `dia run googletest --filter="*Validator*"`, `dia run googletest --filter="*ApplicationFlow*"` | All quoted: pass/fail summary, any failing test names | Todo | sonnet | Verification gate per CLAUDE.md verify skill — fresh run, quoted output |
| 9 | Update `dia.applicationflow.architecture.module.md` YAML frontmatter to mention v3 schema; verify SD-019 added to `docs/specs/systems/dia/diaapplicationflow.md` (already done at approval); verify Stage System feature row note updated (already done at approval) | Doc only | Todo | haiku | |
| 10 | Commit with conventional message: "Bump .diaapp to v3 — per-stage transitions + auto_advance, replaces v2 auto_stages" | `git status` clean | Todo | haiku | After tasks 1–9 |

## Risks

- **Cross-file rename blast radius** — V2 → V3 rename touches the loader, validator, manifest struct file, Application.h/.cpp, every test fixture, and the `.vcxproj` + `.filters`. Task 1 deliberately stops at "compiles" rather than "all tests green" because the test fixtures need their own rebuild in subsequent tasks. If the rename is partial and the build fails mid-task, fix at the rename site (single normalisation point), not by reverting.
- **Test fixture churn** — every GoogleTest that constructs a `StageDeclaration` or `ApplicationManifest*` literal needs the new fields populated. Mostly mechanical (`autoAdvance = false; transitions = {};`) but high-volume — likely 10+ test files. No way around it; SD-017's clean-break is the chosen path.
- **`example_app.diaapp` / `test_manifest.diaapp` provenance unclear** — spec lists them as "verify if source-of-truth." If they're test fixtures generated programmatically, they don't need hand-migration; if they're checked-in inputs, they do. Task 7 explicitly checks `git log` on these files to determine status.
- **Editor breaks until Step 8b** — the editor's JS payload assumes the v2 shape (`stages: StageV2[]` with name + manifestPath; separate `autoStages: string[]`). Once the engine emits v3, the editor's parser will throw on load. **Mitigation:** Step 8b is sequenced immediately after this spec; in the gap, the editor is non-functional but the engine + CluicheTest work correctly. Acceptable per user direction ("yeah line it up after this").
- **Defensive runtime check creates dead code if validator is correct** — AC #8's defensive `WARNING` log for `autoAdvance && transitions.Size()!=1` is unreachable if `AUTO_ADVANCE_AMBIGUOUS` correctly catches it at load. Per AI Review Q4, we keep the check anyway (cost: one `if`; benefit: robustness against future skipped-validator paths).

## Decisions log

(to be filled as tasks execute)

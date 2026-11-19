# Cleanup bundle plan (2026-11-19)

**Status:** In Progress
**Scope:** 7 backlog Task rows fixed together, verified, then struck from `docs/BACKLOG.md` (moved to BACKLOG-HISTORY.md via `dia docs backlog move`).
**Rules:** scoped `--filter` while iterating, `--shards >= 2`, one full `dia run googletest` at the end. No sanitizer, no mutation checks, no Release benchmarking. Commit after each task. Models are sonnet/haiku only (no opus without asking).

## Decisions (user, 2026-11-19)

| # | Decision |
|---|---|
| D1 | Zone shortfall: **keep the total-only rule** (AC4 amendment stands). Fix the SLOW_ test assertion, no RegionRepair code change. |
| D2 | Subject-name table: **raise the cap only** (256 -> 1024). No unregister API. |
| D3 | Describe: add describes to existing CluicheTest/CoW components **and move `SchemaReader`** to a shared Dia module. |

## Constraints to check first

- Working tree has uncommitted grid-search edits (`GridSearch.inl`, `GridSearchScratch.h`, `TestGridSearchPerf.cpp`, `BACKLOG.md`). Do not touch or stage them; stage only each task's own files.
- Retire placeholder was marked "in progress elsewhere" but the code is untouched (module, vcxproj and `cow_main.diaapp` entries all still present). Confirm no other session is editing before Task 1.
- Task 8 (SchemaReader move) adds a header to a Dia project from a game-adjacent task: **AD-001 sign-off was given in D3**, placement to be confirmed in Task 8 step 1 before moving.
- S-1 tasks (3, 4) touch `Cluiche/CoW/Shared/WorldGeneration/`; Biome Registry (backlog) will also, so land these first.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Retire `JobSystemPlaceholderConsumerModule`: add `DeferredStepModule` (`consumes JobSystemService`, `worker_cap: auto`, `ranges_per_worker: 4`, copy from CluicheTest stage) to `cow_main.diaapp` SimPU list in place of the placeholder; delete `.h/.cpp` under `CoW/Modules/`; remove vcxproj + filters entries (`dia docs vcxproj-add` counterpart / manual); update stale doc mentions only where they claim it exists | `dia validate manifest` (no `SERVICE_STREAM_ORPHAN_PROVIDER`); `dia run googletest --filter="*CoW*"` builds and passes; CoW launches (`dia run cow` smoke if available) | Not Started | sonnet | One "no key source" warning per run is expected until WorldStepModule exists |
| 2 | Profile subject-name cap: `kMaxSubjectNames` 256 -> 1024 in `Profiler.h`; check `sSubjects` static size and any file-sink/trip-record format assumption of the cap (`ProfileFileSink.cpp`, `TripRecord.cpp`); update `TestProfiler`/`TestJobObserver` fillers (they loop to the constant, so should adapt); update module doc | `--filter="Profiler*:JobObserver*:*Trip*:*ProfileFileSink*"` | Not Started | haiku | If any serialized format hardcodes 256, stop and report |
| 3 | S-1 zone shortfall: amend `TestRegionRepair.GeneratedWorldsThreePresetsTwentySeedsCountsAndAbsorbLimits` to assert the total-only rule (mainland total >= 10 or `limitedBy` set); drop the per-zone minimum assertion, keep a comment pointing at the AC4 amendment; note the decision in `regions-and-sites.md` | `dia run googletest --filter="*RegionRepair*" --slow` (SLOW_ included) | Not Started | sonnet | No code change in RegionRepair |
| 4 | S-1 panel `busy` / unavailable layers: **check first** the panel UI for (a) last Regenerate outcome shown next to world, (b) `busy` handling on slider drags, (c) "layer unavailable" for layers not rebuilt after import. Fix whichever is missing in the panel and its sim-hosted commands; follow the typed-command rule (UI calls DiaAPI commands only) | New/extended panel tests (Vitest or GoogleTest, whichever the panel already uses) for each fixed behaviour; `dia pipeline --target cow` if UI changes | Not Started | sonnet | If all three already work, record that and close with a test pinning it. Any UI visual change needs mockup sign-off: stop and ask |
| 5 | S-1 review leftovers: (#4) cache `cow.worldgen` state JSON by revision; (#5/#8) AC10/AC6 draw-count tests for the new drawers; minors #9-#14 per plan row 18 notes; archipelago preset Plateau barrier: record as accepted (decide keep, no revisit row) | `--filter="*WorldGen*:*TerrainDraw*"`; golden file `worldgen_layers.golden.json` must stay bit-identical | Not Started | sonnet | Read plan row 18 notes first; minors #9-14 listed there |
| 6 | DiaDeferredStep leftovers (1): `requires_tick_after` across manifests composed in a `.diagame` (extend `dia validate manifest` rule + test); check whether `ManifestSaver` drops the key on save and fix with a round-trip test | pytest for validator; `--filter="*ManifestSaver*"` | Not Started | sonnet | AD-001: CLI rule extension already covered by the 2026-11-18 manifest-ordering sign-off; confirm |
| 7 | DiaDeferredStep leftovers (2): stream write-path perf pass. Measure Debug ballpark only (no Release benchmarking per rule); if ballpark-OK, **raise the AC1 5 ns target in the spec to the measured ceiling and say so**; otherwise optimise the obvious hot spot | `--filter="*FrameStream*:*EventStream*"` perf tests | Not Started | sonnet | Per quick-and-dirty rule: if ballpark-OK, leave code, amend spec |
| 8 | Describe existing components: (a) move `SchemaReader` from `DiaEntityTemplateEditor` to a shared module (propose placement, confirm before moving; update vcxproj, includes in DiaSceneEditor / PropertyInspectorController / BlueprintPropertyController, module docs + registry, `dia check deps`); (b) add `DIA_ATTR_DESCRIBE` to every CluicheTest and CoW component field | `--filter="*SchemaReader*:*PropertyInspector*:*BlueprintProperty*:*DescribedComponent*"`; `dia validate schema` lists no ranged field without describe; `dia check deps` | Not Started | sonnet | Split 8a (move) and 8b (describes) into two commits; list components first (grep `DIA_COMPONENT`) |
| 9 | Close-out: full `dia run googletest --shards 2` (one run), `dia check deps`, `dia validate manifest`, `dia docs precommit`; observation opportunity scan on touched files (High findings implemented separately, Low noted here) | Full suite pass | Not Started | haiku | Report pass/fail and failing names only |
| 10 | Backlog: `dia docs backlog move` for the 7 rows (retire placeholder, zone shortfall, panel busy, review leftovers, DeferredStep leftovers, subject-name cap, describe existing components); fix cross-references (World Step plan note, A4 weather docs mentioning the placeholder if now stale); commit | `dia docs precommit` | Not Started | haiku | Do not stage the unrelated dirty `BACKLOG.md` grid-search edits blindly; stage hunks only |

## Order

1 -> 2 -> 3 -> 5 -> 4 -> 6 -> 7 -> 8a -> 8b -> 9 -> 10. Tasks 1, 2, 3, 6 are independent and could run as parallel subagents, but all touch vcxproj or shared build output, so run sequentially.

## Gap handling

Anything found during a task ends as: fix now (small), promote to a backlog row with a deliverable, or drop. No Loose Ends rows.

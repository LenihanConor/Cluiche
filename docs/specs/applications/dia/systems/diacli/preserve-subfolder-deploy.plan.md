**Spec:** @docs/specs/applications/dia/systems/diacli/preserve-subfolder-deploy.md
**Status:** Done

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `layout.py`: `compute_world_subfolder` + `world`-tag branch in `resolve_deploy_path`; stage scope wins over `world` (AC4, Resolved Decision 1); Warning (validate + build-assets) for `world` record outside `Global/` | RED-GREEN: pytest AC1-AC5 (`test_layout_world_deploy.py`) | Done | sonnet | 4 new tests + 40 asset regression tests pass. Stage wins over `world` (AC4). `dia validate manifest` also warns on `world` records outside `Global/` (reads the `.diagame`'s `asset_catalogue`; shared `world_record_outside_global` predicate; 3 tests) |
| 2 | `cow.diagame`: add `"world": "./global/world"` to `path_aliases` | `dia validate manifest`; manual AC6 | Done | haiku | Alias added; JSON valid. `dia validate manifest` shows only the pre-existing `JobSystemService` orphan-provider error. AC6 runtime resolution (`dia launch cow`) not run |
| 3 | Catalogue protection: confirm append-only, add regression test that `author: game` record survives pipeline with source missing | pytest AC7 (`test_catalogue_protection.py`) | Done | sonnet | Verified: no code path in `commands/asset` removes or rewrites catalogue records. Characterization test passes (5 with integration suite); no RED applies |

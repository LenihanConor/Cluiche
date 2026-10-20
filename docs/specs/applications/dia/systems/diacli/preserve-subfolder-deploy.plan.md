**Spec:** @docs/specs/applications/dia/systems/diacli/preserve-subfolder-deploy.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `layout.py`: `compute_world_subfolder` + `world`-tag branch in `resolve_deploy_path`; stage scope wins over `world` (AC4, Resolved Decision 1); Warning (validate + build-assets) for `world` record outside `Global/` | RED-GREEN: pytest AC1-AC5 (`test_layout_world_deploy.py`) | Done | sonnet | 4 new tests + 40 asset regression tests pass. Stage wins over `world` (AC4). **Gap:** warning fires from build-assets only; the `dia validate manifest` half of AC5 is not done (no record-level hook checked yet) |
| 2 | `cow.diagame`: add `"world": "./global/world"` to `path_aliases` | `dia validate manifest`; manual AC6 | Pending | haiku | |
| 3 | Catalogue protection: confirm append-only, add regression test that `author: game` record survives pipeline with source missing | pytest AC7 (`test_catalogue_protection.py`) | Pending | sonnet | Spec says protection is automatic; test proves it |

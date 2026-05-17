# Feature Spec: DummyStage Asset Deployment

## Parent System
@docs/specs/systems/cluichetest/async-asset-loading.md

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | CluicheTest | @docs/specs/applications/cluichetest.md |
| System | Async Stage Asset Loading | @docs/specs/systems/cluichetest/async-asset-loading.md |
| Feature | DummyStage Asset Deployment | (this file) |

## Problem Statement

The asset catalogue (`Assets/CluicheTest/assets.catalogue.json`) and stage manifest (`Assets/Stages/DummyStage/dummy_stage.diastage`) already declare DummyStage's three test textures with `"scope": "stage", "stage_name": "DummyStage"` pointing at `Stages/DummyStage/World/Textures/test_*.png`. However:

1. The PNG files are **not on disk** at the declared paths (verified during planning)
2. The `dummy_stage.diaapp` file referenced from `dummy_stage.diastage` line 3 **does not exist**
3. `AssetServiceModule::DoStart` currently pre-loads these textures into `stage.global` instead of letting DummyStage own them

This feature closes those gaps so DummyStage's stage-driven async load (sibling feature) has something real to load.

## Acceptance Criteria

1. Three PNG files exist at `Cluiche/Assets/Stages/DummyStage/World/Textures/test_red.png`, `test_blue.png`, `test_green.png`. Source: copy from existing global location, or generate trivial 64x64 solid-color PNGs if no canonical source exists
2. `Cluiche/Assets/Stages/DummyStage/misc/ApplicationFlow/dummy_stage.diaapp` exists, declaring DummyStage modules per ApplicationFlow SD-005 (stage-specific modules from `.diastage`); content matches the existing applicationflow system spec's `dummy_stage.diaapp` example, with `DummyLevel`'s dependencies updated by the consumer feature
3. `AssetServiceModule::DoStart` no longer pre-loads `texture.test_red/blue/green` as part of `stage.global`; only true global assets remain in the global pre-load (per SD-007 of Async system)
4. `assets.catalogue.json` continues to declare the 3 textures as stage-scoped to `DummyStage` (no change — already correct per existing asset-pipeline)
5. `dia pipeline --target cluichetest` runs cleanly and deploys the new files into the bin output structure
6. `dia run cluichetest` succeeds (regression check — feature alone doesn't enable async path; consumer feature does)

## Files Touched

| File | Action |
|------|--------|
| `Cluiche/Assets/Stages/DummyStage/World/Textures/test_red.png` | New (or move from global) |
| `Cluiche/Assets/Stages/DummyStage/World/Textures/test_blue.png` | New (or move from global) |
| `Cluiche/Assets/Stages/DummyStage/World/Textures/test_green.png` | New (or move from global) |
| `Cluiche/Assets/Stages/DummyStage/misc/ApplicationFlow/dummy_stage.diaapp` | New |
| `Cluiche/CluicheGameBaseline/Modules/AssetServiceModule.cpp` | Update — narrow `DoStart` to drop DummyStage textures from global pre-load |
| `Cluiche/Assets/CluicheTest/Global/.../<global-stage-manifest>` | Update if removed entries leave stage.global empty/dangling — verify and adjust |

## Discovery Tasks (before editing)

| # | Task |
|---|------|
| 1 | Locate the 3 source PNGs in the current global asset deployment (where they're served from today) |
| 2 | Verify `dummy_stage.diastage` line 3's referenced manifest path matches what the runtime actually expects |
| 3 | Read `AssetServiceModule::DoStart` (line 75 onward) and identify exactly which calls/strings reference the 3 textures so the narrowing is clean |
| 4 | Check whether `assets.catalogue.json` paths use forward slashes or back-slashes; match local convention |

## Dependencies

- **Asset Pipeline system** — `dia pipeline --target cluichetest` deploys assets per the existing pipeline rules
- **DiaAssetCatalogue** — already understands `"scope": "stage"`
- **DiaAssetRuntime** — already understands `RequestStageLoad("stage.global")` and per-stage manifests

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-010 | Platform | `.diagame` root, `.diastage` for stages, typed imports | Compliant — using existing `dummy_stage.diastage`, no schema changes |
| AD-005 | App | App is engine testbed | Compliant — DummyStage is the testbed reference |
| SD-005 (CT AppFlow) | System | Stage-specific modules from `.diastage` files | Compliant — creating the missing `dummy_stage.diaapp` |
| SD-005 (Async) | System | Stages declare assets via existing catalogue, no new manifest format | Compliant — catalogue already declares them; this feature only adds the missing files |
| SD-007 (Async) | System | `AssetServiceModule::DoStart` no longer pre-loads DummyStage textures | This feature **is** that change |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Asset source | Where do `test_red.png` etc. live today? | Discovery Task 1. Likely in a legacy global texture folder. If found: copy. If not: generate trivial 64x64 solid colour PNGs (acceptable since they're testbed sprites, not art). |
| 2 | Sequencing | Can this feature land before the consumer (DummyStage Async Load Consumer) feature? | Yes. Removing DummyStage textures from global pre-load + adding the files in their stage location does NOT break anything: `assets.catalogue.json` already says they're stage-scoped, so global-stage load won't try to load them after the narrowing. DummyStage will still draw correctly because it currently looks them up by CRC at draw time — they're loaded by global today. **Caveat:** there's a window where this feature has landed but the consumer hasn't, so DummyStage textures aren't loaded at all → `redId/blueId/greenId == 0` → sprites silently don't render. Mitigation: ship features 3 and 4 in the same plan execution, or land 4 first if possible. |
| 3 | Manifest content | What modules does `dummy_stage.diaapp` declare? | Just `DummyLevel`, matching the existing `applicationflow.md` spec sketch (lines 146–168). The consumer feature adds the `AssetService` dependency; this feature creates the file with the dependencies it has *today*, the consumer feature updates it. |
| 4 | Pipeline | Does `dia pipeline` need any config update to pick up the new texture directory? | Discovery Task — likely no, since the existing pipeline already deploys `Assets/Stages/<StageName>/...`. Verify by running pipeline before and after the file additions. |
| 5 | Webix | The asset-restructure memory mentions dropping Webix; should this feature do that? | No — out of scope. Webix removal is a separate concern in the existing asset-restructure plan. This feature is narrow: deploy DummyStage assets and narrow global pre-load. |
| 6 | Source PNG location | If the 3 PNGs already exist somewhere in the repo and are deployed to bin today, do we leave the source copy or move it? | Move (delete the global source copy after copying to the new stage location). Two source copies risks divergence. Verify nothing else references the global path before deleting. |

## Open Questions

- Source location of existing `test_*.png` files (resolved by Discovery Task 1).
- Whether moving the PNGs and narrowing `DoStart` together leaves `stage.global` entry empty — if so, what does the runtime do with an empty stage? Verify before merging.

## Status

`Approved` — 2026-05-17

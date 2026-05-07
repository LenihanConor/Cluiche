# Feature Spec: Directory Structure & Asset Migration

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | CluicheTest | @docs/specs/applications/cluichetest.md |
| System | CluicheTest Asset Pipeline | @docs/specs/systems/cluichetest/asset-pipeline.md |
| Feature | Directory Structure & Asset Migration | (this document) |

## Summary

Create the `Assets/CluicheTest/` raw asset directory in the `Global/` + `Stages/<Name>/` layout, move all existing CluicheTest assets into their correct locations, and delete legacy data. This establishes the canonical source tree that DiaAssetPipeline reads and DiaAssetCatalogueEditor discovers from.

## Problem

CluicheTest assets are currently scattered across three locations (`Cluiche/Data/`, `Cluiche/CluicheTest/Data/Manifests/`, `Cluiche/Assets/Textures/`) with no consistent structure. Legacy data (Webix 3.4, DiaGraphicWithUITest, UnitTestLevel) is mixed in. There is no `.diagame`/`.diastage` root to anchor tool discovery or pipeline processing. The asset pipeline needs a single, structured source tree.

## Acceptance Criteria

1. `Assets/CluicheTest/Global/` directory exists with `cluichetest.diagame` at its root
2. `Assets/CluicheTest/Global/Manifests/` contains `cluiche_main.diaapp`, `cluiche_render.diaapp`, `cluiche_sim.diaapp`
3. `Assets/CluicheTest/Global/Presentation/ui.frag` exists
4. `Assets/CluicheTest/Global/Presentation/UI/BootStrap/` contains `bootscreen.html`, `launchMenuData.json`, `launchMenuData.csv`, `testdata.js`
5. `Assets/CluicheTest/Global/Presentation/UI/Webix/5.2.1/` contains the full Webix 5.2.1 library
6. `Assets/CluicheTest/Global/Misc/Resources/` contains `cacert.pem` and `icudt67l.dat`
7. `Assets/CluicheTest/Stages/DummyStage/dummy_stage.diastage` exists at the stage root
8. `Assets/CluicheTest/Stages/DummyStage/Manifests/dummy_stage.diaapp` exists
9. `Assets/CluicheTest/Stages/DummyStage/Presentation/UI/dummyStage.html` exists
10. `Assets/CluicheTest/Stages/DummyStage/World/Textures/` contains `test_red.png`, `test_blue.png`, `test_green.png`
11. Legacy data deleted: `Cluiche/Data/UI_Common/Webix/3.4/`, `Cluiche/Data/DiaGraphicWithUITest/`, `Cluiche/Data/UnitTestLevel/`
12. Old source locations emptied/removed: `Cluiche/Data/` directories that moved, `Cluiche/CluicheTest/Data/Manifests/` contents that moved, `Cluiche/Assets/Textures/` contents that moved
13. `.diagame` file includes a `config.path_aliases` section (structure only — runtime loading is Feature 4)
14. `.diastage` file includes a `config.path_aliases` section (structure only — runtime loading is Feature 4)
15. `pathStoreConfig.json` is NOT deleted yet (that happens in Feature 4 when the replacement is wired up)

## Design

### Source File Mapping

| Current Location | New Location |
|------------------|-------------|
| `Cluiche/CluicheTest/Data/Manifests/cluichetest.diagame` | `Assets/CluicheTest/Global/cluichetest.diagame` |
| `Cluiche/CluicheTest/Data/Manifests/cluiche_main.diaapp` | `Assets/CluicheTest/Global/Manifests/cluiche_main.diaapp` |
| `Cluiche/CluicheTest/Data/Manifests/cluiche_render.diaapp` | `Assets/CluicheTest/Global/Manifests/cluiche_render.diaapp` |
| `Cluiche/CluicheTest/Data/Manifests/cluiche_sim.diaapp` | `Assets/CluicheTest/Global/Manifests/cluiche_sim.diaapp` |
| `Cluiche/CluicheTest/Data/Manifests/stages/dummy_stage.diastage` | `Assets/CluicheTest/Stages/DummyStage/dummy_stage.diastage` |
| `Cluiche/CluicheTest/Data/Manifests/stages/dummy_stage.diaapp` | `Assets/CluicheTest/Stages/DummyStage/Manifests/dummy_stage.diaapp` |
| `Cluiche/Data/BootStrap/bootscreen.html` | `Assets/CluicheTest/Global/Presentation/UI/BootStrap/bootscreen.html` |
| `Cluiche/Data/BootStrap/launchMenuData.json` | `Assets/CluicheTest/Global/Presentation/UI/BootStrap/launchMenuData.json` |
| `Cluiche/Data/BootStrap/launchMenuData.csv` | `Assets/CluicheTest/Global/Presentation/UI/BootStrap/launchMenuData.csv` |
| `Cluiche/Data/BootStrap/testdata.js` | `Assets/CluicheTest/Global/Presentation/UI/BootStrap/testdata.js` |
| `Cluiche/Data/Render_Common/ui.frag` | `Assets/CluicheTest/Global/Presentation/ui.frag` |
| `Cluiche/Data/UI_Common/Webix/5.2.1/` | `Assets/CluicheTest/Global/Presentation/UI/Webix/5.2.1/` |
| `Cluiche/Data/DummyStage/dummyStage.html` | `Assets/CluicheTest/Stages/DummyStage/Presentation/UI/dummyStage.html` |
| `Cluiche/Assets/Textures/test_red.png` | `Assets/CluicheTest/Stages/DummyStage/World/Textures/test_red.png` |
| `Cluiche/Assets/Textures/test_blue.png` | `Assets/CluicheTest/Stages/DummyStage/World/Textures/test_blue.png` |
| `Cluiche/Assets/Textures/test_green.png` | `Assets/CluicheTest/Stages/DummyStage/World/Textures/test_green.png` |
| `Cluiche/bin/.../resources/cacert.pem` | `Assets/CluicheTest/Global/Misc/Resources/cacert.pem` |
| `Cluiche/bin/.../resources/icudt67l.dat` | `Assets/CluicheTest/Global/Misc/Resources/icudt67l.dat` |

### Deleted (not moved)

| Location | Reason |
|----------|--------|
| `Cluiche/Data/UI_Common/Webix/3.4/` | Superseded by 5.2.1 |
| `Cluiche/Data/DiaGraphicWithUITest/` | Legacy test remnant |
| `Cluiche/Data/UnitTestLevel/` | Legacy test remnant |

### Updated `.diagame` Structure

```json
{
    "name": "CluicheTest",
    "version": "0.1.0",
    "imports": [
        { "type": "manifest", "path": "Manifests/cluiche_main.diaapp" }
    ],
    "stages": [
        { "type": "stage", "path": "../Stages/DummyStage/dummy_stage.diastage" }
    ],
    "config": {
        "path_aliases": {
            "assets_root": ".",
            "ui_common": "./Presentation/UI"
        },
        "ultralight": {
            "resource_path_prefix": "Misc/Resources/"
        }
    }
}
```

### Updated `.diastage` Structure

```json
{
    "name": "DummyStage",
    "manifest": "Manifests/dummy_stage.diaapp",
    "config": {
        "path_aliases": {
            "stage_ui": "./Presentation/UI"
        }
    }
}
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Create directory structure | Create all directories under `Assets/CluicheTest/Global/` and `Assets/CluicheTest/Stages/DummyStage/` |
| 2 | Move manifest files | Move `.diagame`, `.diaapp`, `.diastage` to new locations. Update `.diagame` content with new relative paths and `config` section. Update `.diastage` content. |
| 3 | Move presentation assets | Move bootscreen HTML + data, Webix 5.2.1, ui.frag, dummyStage.html to new locations |
| 4 | Move textures | Move test_red/blue/green.png to `Stages/DummyStage/World/Textures/` |
| 5 | Move Ultralight resources | Copy `cacert.pem` and `icudt67l.dat` to `Global/Misc/Resources/` (keep originals in bin for now — Feature 2 handles deploy) |
| 6 | Delete legacy data | Remove Webix 3.4, DiaGraphicWithUITest/, UnitTestLevel/ |
| 7 | Clean up empty source directories | Remove emptied directories from old locations |
| 8 | Update .gitignore if needed | Ensure the new `Assets/CluicheTest/` tree is tracked and deployed assets in `bin/` are ignored |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| PD-010 (.diagame/.diastage format) | File format for root manifests |
| Existing file locations | Source data to migrate |

## Files

| File | Action |
|------|--------|
| `Assets/CluicheTest/Global/cluichetest.diagame` | Create (move + update content) |
| `Assets/CluicheTest/Global/Manifests/*.diaapp` | Create (move) |
| `Assets/CluicheTest/Global/Presentation/ui.frag` | Create (move) |
| `Assets/CluicheTest/Global/Presentation/UI/BootStrap/*` | Create (move) |
| `Assets/CluicheTest/Global/Presentation/UI/Webix/5.2.1/` | Create (move) |
| `Assets/CluicheTest/Global/Misc/Resources/cacert.pem` | Create (copy) |
| `Assets/CluicheTest/Global/Misc/Resources/icudt67l.dat` | Create (copy) |
| `Assets/CluicheTest/Stages/DummyStage/dummy_stage.diastage` | Create (move + update content) |
| `Assets/CluicheTest/Stages/DummyStage/Manifests/dummy_stage.diaapp` | Create (move) |
| `Assets/CluicheTest/Stages/DummyStage/Presentation/UI/dummyStage.html` | Create (move) |
| `Assets/CluicheTest/Stages/DummyStage/World/Textures/test_*.png` | Create (move) |
| `Cluiche/Data/UI_Common/Webix/3.4/` | Delete |
| `Cluiche/Data/DiaGraphicWithUITest/` | Delete |
| `Cluiche/Data/UnitTestLevel/` | Delete |
| `Cluiche/CluicheTest/Data/Manifests/` | Delete (contents moved) |
| `Cluiche/Data/BootStrap/` | Delete (contents moved) |
| `Cluiche/Data/DummyStage/` | Delete (contents moved) |
| `Cluiche/Data/Render_Common/` | Delete (contents moved) |
| `Cluiche/Assets/Textures/test_*.png` | Delete (moved) |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-010 | .diagame is project root; .diastage declares stage metadata | **Compliant.** `cluichetest.diagame` at `Global/` root; `dummy_stage.diastage` at stage root. |
| SD-CTAP-001 | Raw layout uses Global/ + Stages/<Name>/ | **Compliant.** This feature establishes that layout. |
| SD-CTAP-006 | Ultralight resources in asset tree under Global/Misc/Resources/ | **Compliant.** cacert.pem and icudt67l.dat placed there. |
| SD-CTAP-007 | .diagame at root of Global/; .diastage at root of each stage | **Compliant.** Both placed at their respective roots. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Migration | Should this be a git move (preserving history) or copy+delete? | Git move (`git mv`) where possible to preserve history. For files that need content changes (diagame, diastage), move then edit in a separate commit. |
| 2 | Build breakage | Moving assets will break the current runtime (hardcoded paths in code). How do we handle the interim? | This feature moves files only. Code paths are NOT updated here — that happens in Features 3 and 4. The application will be broken between Feature 1 and Feature 3 completion. This is acceptable for a development branch. |
| 3 | Shared data | `Cluiche/Data/` is currently shared across apps (paths resolve from pathStoreConfig). Will other apps break? | Only CluicheTest uses these paths today. CluicheEditor has its own assets. GoogleTests doesn't load game data. No other app breaks. |
| 4 | Webix 5.2.1 size | ~30MB of library in the raw tree. Should it be in `.gitignore` or an external package? | Keep it tracked in git for now — it's a dependency. Replacing with ImGui (backlog item) will remove it entirely. |
| 5 | Resources source | cacert.pem and icudt67l.dat currently exist only in bin/ (deployed by Ultralight setup). Where is the authoritative source? | Ultralight ships them in its SDK. Copy from `External/Ultralight/` or existing bin location into the asset tree. They become part of the asset pipeline going forward. |

## Status

`Approved`

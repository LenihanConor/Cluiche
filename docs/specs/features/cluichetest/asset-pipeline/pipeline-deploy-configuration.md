# Feature Spec: DiaAssetPipeline Deploy Configuration

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | CluicheTest | @docs/specs/applications/cluichetest.md |
| System | CluicheTest Asset Pipeline | @docs/specs/systems/cluichetest/asset-pipeline.md |
| Feature | DiaAssetPipeline Deploy Configuration | (this document) |

## Summary

Configure `pipeline.toml` for CluicheTest, create the `assets.catalogue.json` manifest for all migrated assets, and verify that `dia asset build --target cluichetest` successfully deploys assets to `bin/CluicheTest/Debug/x64/assets/` with the mirrored directory structure and generates a valid `assets.runtime.json`.

## Problem

After Feature 1 moves raw assets into the structured tree, there is no build-time mechanism to deploy them to the bin directory. The DiaAssetPipeline system spec and its features are approved but the CluicheTest-specific configuration does not exist. Without `pipeline.toml` target config and `assets.catalogue.json`, the asset pipeline cannot process CluicheTest assets.

## Acceptance Criteria

1. `pipeline.toml` contains a `[targets.cluichetest]` section with `catalogue_manifest`, `asset_stages`, and `deploy_root`
2. `Assets/CluicheTest/assets.catalogue.json` exists and lists all assets with correct IDs, types, source paths, scope, and stage membership
3. `dia asset build --target cluichetest` completes with exit code 0
4. Deployed directory at `bin/CluicheTest/Debug/x64/assets/` mirrors the raw structure: `global/` + `stages/DummyStage/`
5. `assets.runtime.json` is generated at `bin/CluicheTest/Debug/x64/assets/assets.runtime.json`
6. `assets.runtime.json` contains a `stage.global` entry listing all global-scoped assets
7. `assets.runtime.json` contains a `stage.dummy_stage` entry listing all DummyStage-scoped assets
8. All asset IDs in `assets.runtime.json` follow the `type.name` convention (e.g., `folder.webix_5_2_1`, `file.bootscreen_html`, `texture.test_red`)
9. Deploy paths in `assets.runtime.json` are relative to the `assets/` deploy root
10. Folder assets (Webix, BootStrap) deploy their full directory tree recursively
11. `dia asset validate --target cluichetest` passes with no errors
12. Release config also works: `dia asset build --target cluichetest --config Release`

## Design

### pipeline.toml Target

```toml
[targets.cluichetest]
catalogue_manifest = "Assets/CluicheTest/assets.catalogue.json"
asset_stages = ["stage.global", "stage.dummy_stage"]
deploy_root = "bin/CluicheTest/{config}/{platform}/assets"
```

### Asset Catalogue Manifest

`Assets/CluicheTest/assets.catalogue.json` — authored initially by hand (DiaAssetCatalogueEditor will manage it once wired up):

```json
{
    "assets": [
        {
            "id": "manifest.cluiche_main",
            "type": "manifest",
            "source_path": "Global/Manifests/cluiche_main.diaapp",
            "scope": "global",
            "tags": ["Manifests"]
        },
        {
            "id": "manifest.cluiche_render",
            "type": "manifest",
            "source_path": "Global/Manifests/cluiche_render.diaapp",
            "scope": "global",
            "tags": ["Manifests"]
        },
        {
            "id": "manifest.cluiche_sim",
            "type": "manifest",
            "source_path": "Global/Manifests/cluiche_sim.diaapp",
            "scope": "global",
            "tags": ["Manifests"]
        },
        {
            "id": "shader.ui_frag",
            "type": "shader",
            "source_path": "Global/Presentation/ui.frag",
            "scope": "global",
            "tags": ["Presentation"]
        },
        {
            "id": "folder.bootstrap_ui",
            "type": "folder",
            "source_path": "Global/Presentation/UI/BootStrap/",
            "scope": "global",
            "tags": ["Presentation/UI"]
        },
        {
            "id": "folder.webix_5_2_1",
            "type": "folder",
            "source_path": "Global/Presentation/UI/Webix/5.2.1/",
            "scope": "global",
            "tags": ["Presentation/UI"]
        },
        {
            "id": "folder.ultralight_resources",
            "type": "folder",
            "source_path": "Global/Misc/Resources/",
            "scope": "global",
            "tags": ["Misc"]
        },
        {
            "id": "manifest.dummy_stage",
            "type": "manifest",
            "source_path": "Stages/DummyStage/Manifests/dummy_stage.diaapp",
            "scope": "stage",
            "stage": "DummyStage",
            "tags": ["Manifests"]
        },
        {
            "id": "folder.dummy_stage_ui",
            "type": "folder",
            "source_path": "Stages/DummyStage/Presentation/UI/",
            "scope": "stage",
            "stage": "DummyStage",
            "tags": ["Presentation/UI"]
        },
        {
            "id": "texture.test_red",
            "type": "texture",
            "source_path": "Stages/DummyStage/World/Textures/test_red.png",
            "scope": "stage",
            "stage": "DummyStage",
            "tags": ["World"]
        },
        {
            "id": "texture.test_blue",
            "type": "texture",
            "source_path": "Stages/DummyStage/World/Textures/test_blue.png",
            "scope": "stage",
            "stage": "DummyStage",
            "tags": ["World"]
        },
        {
            "id": "texture.test_green",
            "type": "texture",
            "source_path": "Stages/DummyStage/World/Textures/test_green.png",
            "scope": "stage",
            "stage": "DummyStage",
            "tags": ["World"]
        }
    ],
    "stages": [
        {
            "id": "stage.global",
            "assets": [
                "manifest.cluiche_main",
                "manifest.cluiche_render",
                "manifest.cluiche_sim",
                "shader.ui_frag",
                "folder.bootstrap_ui",
                "folder.webix_5_2_1",
                "folder.ultralight_resources"
            ]
        },
        {
            "id": "stage.dummy_stage",
            "assets": [
                "manifest.dummy_stage",
                "folder.dummy_stage_ui",
                "texture.test_red",
                "texture.test_blue",
                "texture.test_green"
            ]
        }
    ]
}
```

### Expected Deploy Output

```
bin/CluicheTest/Debug/x64/assets/
├── assets.runtime.json
├── global/
│   ├── Manifests/
│   │   ├── cluiche_main.diaapp
│   │   ├── cluiche_render.diaapp
│   │   └── cluiche_sim.diaapp
│   ├── Presentation/
│   │   ├── ui.frag
│   │   └── UI/
│   │       ├── BootStrap/
│   │       │   ├── bootscreen.html
│   │       │   ├── launchMenuData.json
│   │       │   ├── launchMenuData.csv
│   │       │   └── testdata.js
│   │       └── Webix/
│   │           └── 5.2.1/...
│   └── Misc/
│       └── Resources/
│           ├── cacert.pem
│           └── icudt67l.dat
└── stages/
    └── DummyStage/
        ├── Manifests/
        │   └── dummy_stage.diaapp
        ├── Presentation/
        │   └── UI/
        │       └── dummyStage.html
        └── World/
            └── Textures/
                ├── test_red.png
                ├── test_blue.png
                └── test_green.png
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Add CluicheTest target to pipeline.toml | Create or update `pipeline.toml` with `[targets.cluichetest]` section |
| 2 | Create assets.catalogue.json | Hand-author the catalogue manifest listing all assets with IDs, types, source paths, scope, tags, and stage membership |
| 3 | Verify DiaAssetPipeline features are implemented | Confirm handler registry, deploy layout engine, built-in handlers, and CLI are functional. If not, this feature is blocked. |
| 4 | Run `dia asset build --target cluichetest` | Execute the pipeline end-to-end; verify exit code 0 |
| 5 | Validate deployed directory structure | Check that `bin/CluicheTest/Debug/x64/assets/` matches expected layout |
| 6 | Validate assets.runtime.json content | Verify all assets present, IDs correct, deploy paths relative, stages populated |
| 7 | Test Release config | Run `dia asset build --target cluichetest --config Release` and verify |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| Feature 1 (Directory Structure) | Raw assets must be in place before pipeline can process them |
| DiaAssetPipeline (all 4 features) | Handler registry, built-in handlers, deploy layout engine, CLI must be implemented |
| DiaPipeline | `build-assets` stage invocation |

## Files

| File | Action |
|------|--------|
| `pipeline.toml` (repo root or DiaCLI config location) | Modify — add `[targets.cluichetest]` |
| `Assets/CluicheTest/assets.catalogue.json` | Create — full catalogue manifest |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-008 | Directory.Build.props owns OutDir | **Compliant.** Deploy root uses `bin/CluicheTest/{config}/{platform}/` which matches centralised conventions. |
| PD-009 | Generated output under Cluiche/out/ | **Compliant.** Pipeline logs go to `out/CluicheTest/`. Deployed assets go to `bin/`. |
| SD-CTAP-001 | Raw layout uses Global/ + Stages/<Name>/ | **Compliant.** Deploy mirrors this as `global/` + `stages/<Name>/`. |
| SD-APIPE-001 | Pipeline reads catalogue, generates runtime manifest | **Compliant.** Reads `assets.catalogue.json`, generates `assets.runtime.json`. |
| SD-APIPE-003 | Copy-as-is default transform | **Compliant.** All CluicheTest assets use copy-as-is (no custom transforms). |
| SD-APIPE-004 | Deploy layout driven by scope and tags | **Compliant.** Scope determines `global/` vs `stages/`; tags determine subdirectory. |
| SD-CAT-001 | Asset IDs are type.name composites | **Compliant.** All IDs follow `type.name` convention. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Blocking | What if DiaAssetPipeline features aren't implemented yet? | This feature is blocked until DiaAssetPipeline Features 1-4 are Done. It cannot be completed until the pipeline CLI is functional. If blocked, mark as such and proceed to Feature 3 (AssetServiceModule) using a manually-deployed asset tree. |
| 2 | Catalogue authoring | Who maintains `assets.catalogue.json` after initial creation? | DiaAssetCatalogueEditor is the long-term owner. For V1, it's hand-authored. Any asset addition/removal requires manual edit until the editor is wired up. |
| 3 | diagame/diastage | Should `.diagame` and `.diastage` be listed as assets in the catalogue? | No. They are discovery roots, not deployable assets. The pipeline reads them for structure but does not deploy them as assets. They stay in the raw tree only. |
| 4 | Stage.global | Is `stage.global` a real stage or a synthetic construct? | Synthetic. The pipeline creates it by collecting all `scope: "global"` assets into a stage entry. DiaAssetRuntime treats it as a regular stage for loading purposes. |
| 5 | Deploy root case | `global/` is lowercase but `Stages/DummyStage/` has mixed case in raw. What case does deploy use? | Deploy uses lowercase `global/` and `stages/` as directory names (matching DiaAssetPipeline spec), but preserves stage name casing: `stages/DummyStage/`. Tag-driven subdirectories preserve their casing (`Manifests/`, `Presentation/`, `World/`). |

## Status

`Approved`

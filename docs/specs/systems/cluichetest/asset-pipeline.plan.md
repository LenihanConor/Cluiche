# Plan: CluicheTest Asset Pipeline

**Spec:** @docs/specs/systems/cluichetest/asset-pipeline.md  
**Status:** In Progress  
**Started:** 2026-05-07  
**Last Updated:** 2026-05-07

## Implementation Patterns

### Feature 1: Directory Structure & Asset Migration
- Pure file operations: `git mv` for tracked files, manual copy for binary resources
- `.diagame` and `.diastage` use the JSON format defined in PD-010
- No code changes — only file moves and manifest content updates
- Application will be broken between Feature 1 and Feature 3 (acceptable on dev branch)

### Feature 2: DiaAssetPipeline Deploy Configuration
- `pipeline.toml` uses existing DiaCLI config format (TOML with `[targets.<name>]` sections)
- `assets.catalogue.json` follows DiaAssetCatalogue's manifest schema (see asset-system-overview.md)
- Verify via `dia asset build --target cluichetest` CLI command
- Deploy output under `bin/CluicheTest/Debug/x64/assets/`

### Feature 3: AssetServiceModule & Phase Gating
- Module class: `Cluiche::Main::AssetServiceModule` inheriting `Dia::Application::Module` + `Dia::AssetRuntime::IAssetStateListener`
- Located in `CluicheTest/CluicheKernel/ApplicationFlow/Modules/` (same as existing modules)
- `kUniqueId` pattern matching existing modules (StringCRC constant)
- Phase gating: use existing phase condition/transition mechanism (check `CanTransition()` pattern)
- V1 acknowledgement: auto-acknowledge in `OnAssetReady`/`OnAssetUnloading` (no content consumers yet)
- Texture loading in SimPU: replace hardcoded paths with `AssetRuntime::ResolveAssetPath()`

### Feature 4: Manifest Path Alias Integration
- Read `.diagame` config section via existing DiaApplication JSON loader
- Register aliases in DiaCore PathStore using existing `AddAlias` API
- Stage aliases registered on load, unregistered on unload
- Update `UltralightUISystem` config init to use `resource_path_prefix` from manifest
- Update `LaunchUIPage.cpp` and `DummyUIPage.cpp` path strings

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create directory structure under Assets/CluicheTest/ | Directories exist | Not Started | haiku | mkdir only |
| 2 | Move manifest files (.diagame, .diaapp, .diastage) | Files at new paths | Not Started | haiku | git mv + content update |
| 3 | Move presentation assets (HTML, Webix, shader) | Files at new paths | Not Started | haiku | git mv |
| 4 | Move textures to DummyStage/World/Textures/ | Files at new paths | Not Started | haiku | git mv |
| 5 | Copy Ultralight resources to Global/Misc/Resources/ | cacert.pem + icudt67l.dat present | Not Started | haiku | copy from bin/ |
| 6 | Delete legacy data (Webix 3.4, DiaGraphicWithUITest, UnitTestLevel) | Directories gone | Not Started | haiku | git rm -r |
| 7 | Clean up empty source directories | No empty dirs remain | Not Started | haiku | git rm -r |
| 8 | Update .diagame content with new paths and config section | Valid JSON, correct relative paths | Not Started | sonnet | JSON content |
| 9 | Update .diastage content with config section | Valid JSON | Not Started | sonnet | JSON content |
| 10 | Add pipeline.toml CluicheTest target | [targets.cluichetest] section exists | Not Started | sonnet | — |
| 11 | Create assets.catalogue.json | Valid JSON, all assets listed | Not Started | sonnet | Hand-author |
| 12 | Run dia asset build --target cluichetest | Exit code 0, assets deployed | Not Started | sonnet | Verify deployed structure |
| 13 | Create AssetServiceModule header | Compiles | Not Started | sonnet | .h file |
| 14 | Create AssetServiceModule implementation | Compiles, links | Not Started | sonnet | .cpp file |
| 15 | Register AssetServiceModule on MainProcessingUnit | Module starts at boot | Not Started | sonnet | Modify MainProcessingUnit.cpp |
| 16 | Wire boot load phase to RequestGlobalLoad + gate | Phase gates correctly | Not Started | sonnet | Modify boot phase |
| 17 | Wire DummyStage MainLoadPhase to RequestStageLoad + gate | Phase gates correctly | Not Started | sonnet | Modify MainLoadPhase.cpp |
| 18 | Wire stage exit to RequestStageUnload | Unload fires on exit | Not Started | sonnet | — |
| 19 | Update SimProcessingUnit texture loading | Textures render from new paths | Not Started | sonnet | Replace hardcoded paths |
| 20 | Update CluicheTest.vcxproj + .filters | Build succeeds | Not Started | haiku | Add new files |
| 21 | Parse path_aliases from .diagame in AssetServiceModule | Aliases registered | Not Started | sonnet | — |
| 22 | Parse path_aliases from .diastage in RequestStageLoad | Stage aliases registered | Not Started | sonnet | — |
| 23 | Wire UltralightUISystem resource_path_prefix | Ultralight finds resources | Not Started | sonnet | Modify UI system init |
| 24 | Update LaunchUIPage + DummyUIPage paths | UI pages load | Not Started | haiku | String changes |
| 25 | Remove pathStoreConfig.json loading from MainKernelModule | No standalone config loaded | Not Started | haiku | Delete code |
| 26 | Delete pathStoreConfig.json | File gone | Not Started | haiku | git rm |
| 27 | Full end-to-end verification | CluicheTest boots, renders, DummyStage works | Not Started | opus | — |

## Session Notes

### 2026-05-07
- Plan created. All 4 feature specs Approved. DiaAssetRuntime and DiaAssetPipeline code exists.
- Build order: Tasks 1-9 (Feature 1) → Tasks 10-12 (Feature 2) → Tasks 13-20 (Feature 3) → Tasks 21-26 (Feature 4) → Task 27 (verify)

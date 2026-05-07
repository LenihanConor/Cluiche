# Feature Spec: Manifest Path Alias Integration

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | CluicheTest | @docs/specs/applications/cluichetest.md |
| System | CluicheTest Asset Pipeline | @docs/specs/systems/cluichetest/asset-pipeline.md |
| Feature | Manifest Path Alias Integration | (this document) |

## Summary

Move path alias configuration from the standalone `pathStoreConfig.json` into the `.diagame` and `.diastage` manifest files. `AssetServiceModule` reads aliases from the loaded manifests and registers them in DiaCore's PathStore at the appropriate lifecycle points (global aliases at boot, stage aliases on stage entry, unregister on stage exit). Also configure Ultralight's `resource_path_prefix` from the `.diagame` config. Delete `pathStoreConfig.json` and remove its loading code from `MainKernelModule`.

## Problem

`pathStoreConfig.json` is a standalone file loaded by `MainKernelModule` that maps path aliases to absolute disk paths. This creates several issues: (1) aliases are disconnected from the assets they reference, (2) the file uses hardcoded absolute paths tied to developer machine layout, (3) there's no stage-scoping of aliases, and (4) Ultralight's resource path is implicitly `"resources/"` with no configuration. Moving aliases into manifests makes them travel with the asset tree and enables stage-specific alias scoping.

## Acceptance Criteria

1. `cluichetest.diagame` contains a `config.path_aliases` object mapping alias names to paths relative to the `.diagame` file's directory
2. `dummy_stage.diastage` contains a `config.path_aliases` object mapping alias names to paths relative to the `.diastage` file's directory
3. `cluichetest.diagame` contains `config.ultralight.resource_path_prefix` pointing to the Ultralight resources within the deployed asset tree
4. `AssetServiceModule::DoStart()` reads the `.diagame` config and registers global path aliases in DiaCore PathStore
5. `AssetServiceModule::RequestStageLoad()` reads the `.diastage` config and registers stage-specific aliases
6. `AssetServiceModule::RequestStageUnload()` unregisters stage-specific aliases
7. All registered paths are absolute (resolved from the deploy directory + relative path in manifest)
8. `pathStoreConfig.json` is deleted from the repository
9. `MainKernelModule` no longer loads `pathStoreConfig.json` (loading code removed)
10. `UltralightUISystem` reads `resource_path_prefix` from the `.diagame` config (or AssetServiceModule provides it)
11. Existing PathStore consumers (LaunchUIPage, DummyUIPage) continue to work with the new alias source
12. CluicheTest boots and renders correctly with the new path resolution

## Design

### Manifest Config Format

```json
// cluichetest.diagame — config section
{
    "config": {
        "path_aliases": {
            "root": ".",
            "ui_common": "./Presentation/UI"
        },
        "ultralight": {
            "resource_path_prefix": "global/Misc/Resources/"
        }
    }
}
```

```json
// dummy_stage.diastage — config section
{
    "config": {
        "path_aliases": {
            "stage_root": ".",
            "stage_ui": "./Presentation/UI"
        }
    }
}
```

### Path Resolution

Aliases use paths relative to the manifest file's deployed location:
- `.diagame` is at `assets/global/cluichetest.diagame` (deploy) → `"root": "."` resolves to the `assets/global/` absolute path
- `.diastage` is at `assets/stages/DummyStage/dummy_stage.diastage` (deploy) → `"stage_root": "."` resolves to `assets/stages/DummyStage/` absolute path

`AssetServiceModule` resolves relative paths against the known deploy root to produce absolute paths for PathStore registration.

### Integration with Existing Code

Current consumers of PathStore aliases:
- `LaunchUIPage.cpp` — loads `"root/BootStrap/bootscreen.html"` (alias `root` + relative path)
- `DummyUIPage.cpp` — loads `"root/DummyStage/dummyStage.html"` (alias `root` + relative path)

After migration:
- `LaunchUIPage.cpp` — loads `"root/Presentation/UI/BootStrap/bootscreen.html"` (updated path within deployed global tree)
- `DummyUIPage.cpp` — loads `"stage_ui/dummyStage.html"` (uses stage-scoped alias)

### Ultralight Resource Path

```cpp
// UltralightUISystem.cpp — during init
::ultralight::Config config;
config.resource_path_prefix = assetServiceModule->GetUltralightResourcePrefix();
// Resolves to "assets/global/Misc/Resources/" or absolute path
```

### AssetServiceModule Extensions

```cpp
class AssetServiceModule : public Dia::Application::Module
{
public:
    // Path alias management
    void RegisterGlobalAliases();    // Called from DoStart after manifest load
    void RegisterStageAliases(const Dia::Core::StringCRC& stageId);
    void UnregisterStageAliases();

    // Ultralight config
    const Dia::Core::Containers::String512& GetUltralightResourcePrefix() const;

private:
    // Parsed from .diagame config
    struct PathAliasEntry
    {
        Dia::Core::StringCRC mAlias;
        Dia::Core::Containers::String512 mResolvedPath;
    };
    Dia::Core::Containers::DynamicArrayC<PathAliasEntry, 16> mGlobalAliases;
    Dia::Core::Containers::DynamicArrayC<PathAliasEntry, 16> mStageAliases;
    Dia::Core::Containers::String512 mUltralightResourcePrefix;
};
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Parse path_aliases from .diagame in AssetServiceModule::DoStart | Read the diagame manifest's config.path_aliases, resolve relative paths to absolute using deploy root, store in mGlobalAliases |
| 2 | Register global aliases in PathStore | Iterate mGlobalAliases, call PathStore::AddAlias for each |
| 3 | Parse path_aliases from .diastage in RequestStageLoad | Read the diastage config.path_aliases, resolve to absolute, store in mStageAliases |
| 4 | Register/unregister stage aliases | Register on stage load, unregister on stage unload. PathStore needs an UnregisterAlias method (or clear-and-re-register pattern). |
| 5 | Parse ultralight config from .diagame | Read config.ultralight.resource_path_prefix, resolve to absolute, store in mUltralightResourcePrefix |
| 6 | Wire UltralightUISystem to use configured resource prefix | Modify UltralightUISystem init to get resource_path_prefix from AssetServiceModule instead of using default |
| 7 | Update LaunchUIPage path | Change from `"root/BootStrap/bootscreen.html"` to `"root/Presentation/UI/BootStrap/bootscreen.html"` |
| 8 | Update DummyUIPage path | Change from `"root/DummyStage/dummyStage.html"` to `"stage_ui/dummyStage.html"` |
| 9 | Remove pathStoreConfig.json loading from MainKernelModule | Delete the SerializedFileLoad call and related pathstore config code |
| 10 | Delete pathStoreConfig.json | Remove from repository and any deploy scripts |
| 11 | Verify end-to-end | Boot CluicheTest — confirm boot screen loads, DummyStage UI loads, Ultralight resources found |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| Feature 3 (AssetServiceModule) | Module must exist and be wired up before adding alias management |
| Feature 1 (Directory Structure) | .diagame and .diastage must have config sections |
| DiaCore PathStore | Alias registration API |
| DiaApplication manifest loader | Reading .diagame/.diastage JSON |
| Ultralight integration | resource_path_prefix configuration |

## Files

| File | Action |
|------|--------|
| `Cluiche/CluicheTest/CluicheKernel/ApplicationFlow/Modules/AssetServiceModule.h` | Modify — add alias management methods and storage |
| `Cluiche/CluicheTest/CluicheKernel/ApplicationFlow/Modules/AssetServiceModule.cpp` | Modify — implement alias parsing, registration, ultralight config |
| `Cluiche/CluicheTest/CluicheKernel/ApplicationFlow/Modules/MainKernelModule.cpp` | Modify — remove pathStoreConfig.json loading |
| `Cluiche/CluicheTest/Source/LaunchUIPage.cpp` | Modify — update path string |
| `Cluiche/CluicheTest/Stages/DummyStage/UI/DummyUIPage.cpp` | Modify — update path string |
| `Dia/DiaGraphics/UltralightUISystem.cpp` (or equivalent) | Modify — read resource_path_prefix from config |
| `Assets/CluicheTest/Global/cluichetest.diagame` | Modify — ensure config.path_aliases and config.ultralight are populated |
| `Assets/CluicheTest/Stages/DummyStage/dummy_stage.diastage` | Modify — ensure config.path_aliases is populated |
| `pathStoreConfig.json` (in deploy/repo) | Delete |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | **Compliant.** Alias names stored as StringCRC keys in PathStore. |
| PD-004 | No STL in public APIs | **Compliant.** String512 and DynamicArrayC used for path storage. |
| PD-010 | .diagame is project root; .diastage declares stage metadata | **Compliant.** Path aliases are config within these root files. |
| SD-CTAP-003 | Path aliases in .diagame/.diastage, not standalone config | **Compliant.** This feature implements that decision. |
| SD-CTAP-006 | Ultralight resources in asset tree | **Compliant.** resource_path_prefix points to deployed location within asset tree. |
| SD-ARUN-005 | Asset IDs registered as path aliases | **Compliant.** Coexists — DiaAssetRuntime registers asset ID aliases; this feature registers named aliases from manifests. Both are additive to PathStore. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | PathStore | Does PathStore currently support UnregisterAlias? | Needs verification. If not, either add it (simple — remove from internal hash table) or use a clear-and-re-register pattern where global aliases are always re-registered alongside stage aliases. |
| 2 | Ordering | AssetServiceModule registers aliases in DoStart. But MainKernelModule currently loads pathStoreConfig in its DoStart. Who starts first? | Module start order is determined by GenerateModuleDependecyGraph. AssetServiceModule should have no dependency on KernelModule for this — it registers its own aliases independently. Remove the pathStoreConfig loading from KernelModule. |
| 3 | Relative paths | What's the base directory for resolving relative paths in manifests? | The deploy root for the application: `bin/CluicheTest/<Config>/<Platform>/assets/`. Manifests reference paths relative to their own location within this tree. AssetServiceModule knows the deploy root and resolves accordingly. |
| 4 | Existing code | Are there other consumers of PathStore aliases beyond LaunchUIPage and DummyUIPage? | Need to grep for PathStore usage. If others exist, they need path updates too. The alias names (`"root"`, `"ui_common"`) can stay the same — only the resolved paths change. |
| 5 | Ultralight timing | UltralightUISystem::Init is called before or after AssetServiceModule::DoStart? | Needs verification. If before: the resource prefix must be available earlier (e.g., hardcoded as a fallback or read from .diagame directly in UIModule). If after: AssetServiceModule provides it via GetUltralightResourcePrefix(). |
| 6 | Deploy vs source | At runtime, aliases resolve to deployed paths (under bin/). At tool-time, should they resolve to raw paths (under Assets/)? | Yes — but that's DiaAssetCatalogueEditor's concern. The editor resolves against the raw tree; the runtime resolves against the deployed tree. This feature only handles runtime. |

## Status

`Approved`

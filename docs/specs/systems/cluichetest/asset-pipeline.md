# System Spec: CluicheTest Asset Pipeline

## Parent Application
@docs/specs/applications/cluichetest.md

## Asset System Context
@docs/specs/systems/dia/asset-system-overview.md

## Summary

CluicheTest Asset Pipeline is the end-to-end asset integration for the CluicheTest application. It defines:
1. The raw asset directory structure (`Assets/CluicheTest/`) using a `Global/` + `Stages/<Name>/` layout
2. DiaAssetPipeline configuration to deploy raw assets to `bin/CluicheTest/<Config>/<Platform>/assets/`
3. An `AssetServiceModule` on the Main ProcessingUnit that owns a DiaAssetRuntime instance, loads global assets at boot, and loads/unloads stage assets during phase transitions
4. Manifest integration — path alias configuration folded into `.diagame`/`.diastage` files, replacing the standalone `pathStoreConfig.json`

This system ties together the Dia-level asset subsystems (DiaAssetCatalogue, DiaAssetPipeline, DiaAssetRuntime) into a working pipeline for a concrete application.

## Responsibilities

**Owns:**
- Raw asset directory structure for CluicheTest (`Assets/CluicheTest/Global/`, `Assets/CluicheTest/Stages/`)
- `pipeline.toml` configuration for DiaAssetPipeline targeting CluicheTest
- `AssetServiceModule` — a `Dia::Application::Module` on the Main PU that wraps DiaAssetRuntime
- Phase gating for asset loads — load phases block until assets are ready
- Global asset loading during application boot (permanent, never unloaded)
- Stage asset loading/unloading during stage transitions
- Path alias definitions within `.diagame` and `.diastage` manifests
- Removal of standalone `pathStoreConfig.json`
- Ultralight `resource_path_prefix` configuration to point into the asset tree

**Does NOT own:**
- DiaAssetRuntime internals (state machine, ref counting, events) — owned by DiaAssetRuntime spec
- DiaAssetPipeline handler logic (validate, transform, deploy) — owned by DiaAssetPipeline spec
- DiaAssetCatalogue data model — owned by DiaAssetCatalogue spec
- Asset content loading (textures, audio) — owned by consuming systems (DiaGraphics, etc.)
- DiaAssetCatalogueEditor UI — owned by DiaAssetCatalogueEditor spec

## Asset Directory Structure

### Raw (Source) Layout

```
Assets/CluicheTest/
├── Global/
│   ├── cluichetest.diagame              ← root game manifest (includes path alias config)
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
│   │           └── 5.2.1/              ← kept until ImGui replacement (backlog)
│   └── Misc/
│       └── Resources/
│           ├── cacert.pem
│           └── icudt67l.dat
└── Stages/
    └── DummyStage/
        ├── dummy_stage.diastage         ← stage root (includes stage-specific path aliases)
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

### Deployed Layout

Mirrors raw structure under `bin/CluicheTest/<Config>/<Platform>/assets/`:

```
bin/CluicheTest/Debug/x64/assets/
├── assets.runtime.json
├── global/
│   ├── Manifests/
│   ├── Presentation/
│   │   ├── ui.frag
│   │   └── UI/
│   │       ├── BootStrap/
│   │       └── Webix/5.2.1/
│   └── Misc/Resources/
└── stages/
    └── DummyStage/
        ├── Manifests/
        ├── Presentation/UI/
        └── World/Textures/
```

### Category Mapping

| Directory | Content |
|-----------|---------|
| `Manifests/` | `.diaapp` files |
| `Presentation/` | Shaders, presentation config |
| `Presentation/UI/` | HTML UI, Webix (→ ImGui later) |
| `World/` | Textures, environments, level geometry |
| `Gameplay/` | Game logic data (future) |
| `Character/` | Character data (future) |
| `Misc/` | Runtime resources, uncategorised |

## AssetServiceModule

### Role

Thin `Dia::Application::Module` on the Main ProcessingUnit. Owns a `Dia::AssetRuntime::AssetRuntime` instance. Provides the integration point between Dia's ProcessingUnit/Phase architecture and the asset lifecycle.

### Lifecycle

```
MainProcessingUnit starts
    │
    ├─ AssetServiceModule::DoStart()
    │      ├─ Create AssetRuntime instance
    │      ├─ LoadManifest(assets.runtime.json)
    │      └─ Register listeners (SimPU, RenderPU asset consumers)
    │
    ├─ BootLoadPhase
    │      ├─ AssetServiceModule::RequestGlobalLoad()
    │      │      └─ RequestStageLoad("global") → DiaAssetRuntime
    │      └─ Phase gates on: all global assets in Loaded state
    │
    ├─ [Stage transition: enter DummyStage]
    │      ├─ StageLoadPhase
    │      │      ├─ AssetServiceModule::RequestStageLoad("DummyStage")
    │      │      │      └─ RequestStageLoad(stageId) → DiaAssetRuntime
    │      │      └─ Phase gates on: all stage assets in Loaded state
    │      │
    │      ├─ [Stage running — assets immutable, safe cross-thread reads]
    │      │
    │      └─ StageUnloadPhase
    │             └─ AssetServiceModule::RequestStageUnload("DummyStage")
    │                    └─ RequestStageUnload(stageId) → DiaAssetRuntime
    │
    └─ AssetServiceModule::DoEnd()
           └─ runtime.Reset()
```

### Threading Model

- **Load/unload calls** — always on Main PU thread (during phase transitions)
- **Read access** (ResolveAssetPath, IsAssetReady, GetAssetState) — safe from any thread once assets are in `Loaded` state. Guaranteed because:
  - Assets transition to `Loaded` during a blocking load phase
  - No state changes occur until the next unload phase
  - Between load and unload, asset data is immutable
- **Request from other threads** (Sim/Render asking for a load) — not V1. API is async-ready (request returns handle, phase gates on completion) so the wiring supports it when async loading is added later
- **Future: async load thread** — AssetServiceModule dispatches loads to a background thread; phase still gates on completion. Consumer code unchanged.

### Phase Gating

Load phases use the existing phase transition condition mechanism:
- Phase registers a condition: "all requested assets are Loaded"
- AssetServiceModule exposes `IsLoadComplete()` — queries DiaAssetRuntime state
- Phase transitions to next phase only when condition is met
- In V1 this is instant (loads are blocking), but the gating structure is already correct for async

### API (Public to CluicheTest code)

```cpp
namespace Cluiche::Main
{
    class AssetServiceModule : public Dia::Application::Module
    {
    public:
        static const Dia::Core::StringCRC kUniqueId;

        // Called by load phases
        void RequestGlobalLoad();
        void RequestStageLoad(const Dia::Core::StringCRC& stageId);
        void RequestStageUnload(const Dia::Core::StringCRC& stageId);

        // Called by phase gating conditions
        bool IsLoadComplete() const;

        // Cross-thread safe once loaded
        const Dia::AssetRuntime::AssetRuntime& GetRuntime() const;

    protected:
        void DoStart() override;
        void DoEnd() override;
    };
}
```

## Manifest Integration

### Path Aliases in `.diagame`

Currently `pathStoreConfig.json` defines path aliases loaded by `MainKernelModule`. This system moves alias definitions into the `.diagame` and `.diastage` files:

```json
// cluichetest.diagame
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

```json
// dummy_stage.diastage
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

`AssetServiceModule::DoStart()` reads path aliases from the loaded diagame and registers them in DiaCore's PathStore. Stage-specific aliases are registered/unregistered during stage transitions.

### Removed

- `pathStoreConfig.json` — deleted. Aliases now live in manifests.
- `Webix/3.4/` — deleted.
- `DiaGraphicWithUITest/` data — deleted.
- `UnitTestLevel/` data — deleted.

## DiaAssetCatalogue Integration

At tool-time, DiaAssetCatalogue reads the raw directory structure:
- Loading a `.diagame` → enumerates `Global/` + all `Stages/*/`, returns all assets tagged by provenance (global vs stage name)
- Loading a `.diastage` → enumerates only that stage's directory, returns stage-scoped assets

This gives the editor a "what exists" view with provenance tagging, matching the existing green (global) / blue (stage) badge pattern.

## DiaAssetPipeline Integration

`pipeline.toml` target configuration:

```toml
[targets.cluichetest]
catalogue_manifest = "Assets/CluicheTest/assets.catalogue.json"
asset_stages = ["stage.dummy_stage"]
deploy_root = "bin/CluicheTest/{config}/{platform}/assets"
```

For V1, all handlers are copy-as-is (no-op transform). The pipeline:
1. Reads `assets.catalogue.json`
2. Copies each asset to the deploy directory preserving the `global/` / `stages/<Name>/` layout
3. Generates `assets.runtime.json` at the deploy root

## Dependencies

| Dependency | What this system uses |
|------------|----------------------|
| DiaAssetRuntime | AssetRuntime instance (load manifest, request stage load/unload, path resolution, state queries) |
| DiaAssetPipeline | Build-time deploy of raw → bin (configured via pipeline.toml) |
| DiaAssetCatalogue | Tool-time registry (editor uses to discover and display assets) |
| DiaApplication | Module base class, ProcessingUnit, Phase gating |
| DiaCore PathStore | Runtime path alias registration |
| Ultralight | `resource_path_prefix` configuration |

## Features

| # | Feature | Size | Description | Spec | Status |
|---|---------|------|-------------|------|--------|
| 1 | Directory Structure & Asset Migration | M | Create `Assets/CluicheTest/Global/` + `Stages/` layout, move all existing assets into it, delete legacy data (Webix 3.4, DiaGraphicWithUITest, UnitTestLevel, pathStoreConfig.json) | @docs/specs/features/cluichetest/asset-pipeline/directory-structure-migration.md | Approved |
| 2 | DiaAssetPipeline Deploy Configuration | S | Create `pipeline.toml` target for CluicheTest, configure copy-as-is handlers, generate `assets.runtime.json` | @docs/specs/features/cluichetest/asset-pipeline/pipeline-deploy-configuration.md | Approved |
| 3 | AssetServiceModule & Phase Gating | M | Create `AssetServiceModule` on Main PU, integrate with DiaAssetRuntime, implement load phases with gating conditions, wire global + stage load/unload | @docs/specs/features/cluichetest/asset-pipeline/asset-service-module.md | Approved |
| 4 | Manifest Path Alias Integration | S | Move path alias config from `pathStoreConfig.json` into `.diagame`/`.diastage`, update PathStore loading in AssetServiceModule, configure Ultralight resource prefix | @docs/specs/features/cluichetest/asset-pipeline/manifest-path-alias-integration.md | Approved |

**Build order:** 1 → 2 → 3 → 4 (directory structure first; pipeline needs it; module needs pipeline output; aliases need module)

## Design Constraints

- **AssetServiceModule is a thin shim.** All logic lives in DiaAssetRuntime. The module only owns lifecycle wiring and phase gating.
- **Loads are blocking in V1.** Phase gates are present but resolve immediately. Async load thread is a later feature.
- **Cross-thread reads are safe by design.** Assets are immutable between load and unload phases. No runtime locks on read path.
- **Deploy mirrors raw.** No structural transformation. Future binary conversion may change this (DiaAssetPipeline handles it, not this system).
- **Global assets are permanent.** Loaded once at boot, never unloaded during application lifetime.
- **Stage assets are transient.** Exactly one stage loaded at a time. Unload-then-load, no overlap.
- **Webix retained until ImGui replacement.** Kept under `Presentation/UI/Webix/5.2.1/` — replacement is a separate backlog item.

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-CTAP-001 | Raw layout uses `Global/` + `Stages/<Name>/` with category subdirectories | Matches DiaAssetPipeline deploy layout; clear provenance; stage isolation | All features | Accepted | Yes |
| SD-CTAP-002 | AssetServiceModule lives on Main PU, persists across all stages | Needs to outlive any individual stage; Main PU is the coordination thread | Features 3, 4 | Accepted | Yes |
| SD-CTAP-003 | Path aliases defined in `.diagame`/`.diastage`, not standalone config | Single source of truth per manifest; aliases travel with the asset tree; eliminates pathStoreConfig.json | Feature 4 | Accepted | Yes |
| SD-CTAP-004 | Loads are blocking in V1; API is async-ready (request + gate) | Simplest correct implementation now; async load thread added later without consumer code changes | Feature 3 | Accepted | Yes |
| SD-CTAP-005 | One stage loaded at a time; unload-then-load, no overlap | Simplest memory model; DummyStage is the only stage today; crossfade/streaming is a future concern | Feature 3 | Accepted | Yes |
| SD-CTAP-006 | Ultralight resources live in asset tree under `Global/Misc/Resources/` | Keeps all runtime data in one tree; configurable via `resource_path_prefix` | Feature 1 | Accepted | Yes |
| SD-CTAP-007 | `.diagame` sits at root of `Global/`; `.diastage` sits at root of each stage directory | Natural discovery point; DiaAssetCatalogue enumerates from these roots | Feature 1 | Accepted | Yes |

## Inherited Binding Decisions

| ID | Decision | Source | Implication for this system |
|----|----------|--------|-----------------------------|
| PD-001 | StringCRC for all IDs | Platform | Stage IDs, asset IDs, module IDs all StringCRC |
| PD-002 | ProcessingUnit/Phase/Module architecture | Platform | AssetServiceModule is a `Dia::Application::Module`; phase gating uses standard phase condition mechanism |
| PD-004 | No STL in public APIs | Platform | AssetServiceModule public API uses DiaCore types |
| PD-006 | VS project files are source of truth | Platform | Any new source files added to CluicheTest.vcxproj manually |
| PD-008 | Directory.Build.props owns OutDir | Platform | Deploy path (`bin/CluicheTest/<Config>/<Platform>/`) follows centralised conventions |
| PD-009 | Generated output under Cluiche/out/ | Platform | Pipeline logs go to `out/CluicheTest/`, deployed assets go to `bin/` |
| PD-010 | `.diagame` is project root; `.diastage` declares stage metadata | Platform | `cluichetest.diagame` is the entry point; stages discovered via typed imports |
| AD-001 | Three ProcessingUnits (Main/Render/Sim) | CluicheTest | AssetServiceModule on Main PU; Sim and Render are read-only consumers |
| AD-005 | Application is testbed, not production | CluicheTest | Blocking loads are acceptable; no streaming requirements |
| SD-ARUN-001 | DiaAssetRuntime has no DiaApplication dependency | DiaAssetRuntime | AssetServiceModule wraps DiaAssetRuntime; the library doesn't know about Modules |
| SD-ARUN-002 | Stage is unit of load/unload | DiaAssetRuntime | AssetServiceModule calls `RequestStageLoad/Unload` — never individual asset loads |
| SD-ARUN-004 | Consumers acknowledge load/unload | DiaAssetRuntime | AssetServiceModule (or downstream listeners) must call `AcknowledgeAssetLoaded` |
| SD-ARUN-006 | No content loading in DiaAssetRuntime | DiaAssetRuntime | Consuming systems (DiaGraphics, etc.) load content in response to `OnAssetReady` |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Phase gating | How does the load phase know all assets are loaded if acknowledgement comes from content consumers (DiaGraphics, etc.) and not from AssetServiceModule itself? | AssetServiceModule registers as an `IAssetStateListener`. Content consumers register separately. The gating condition checks `DiaAssetRuntime::IsAssetReady()` for all assets in the requested stage — which requires consumers to have called `AcknowledgeAssetLoaded`. The load phase doesn't transition until all assets reach `Loaded` state. |
| 2 | Threading | What if a stage unload is triggered while Sim/Render are mid-frame reading asset data? | Unload only happens during a phase transition. Phase transitions on Main PU synchronize with Sim/Render PU phase boundaries. Assets are guaranteed immutable during active gameplay phases. |
| 3 | Global as stage | Global assets are loaded via `RequestStageLoad("global")` — does DiaAssetRuntime need a special "global" stage entry in `assets.runtime.json`? | Yes. The pipeline creates a synthetic `stage.global` entry in `assets.runtime.json` listing all global-scoped assets. DiaAssetRuntime treats it as a regular stage with ref-counted assets. |
| 4 | Manifest paths | `.diagame` uses relative paths to `.diaapp` imports. After deploy, these paths change (assets are under `assets/global/Manifests/`). How is this resolved? | The `.diagame` and `.diastage` are deployment configuration — they tell AssetServiceModule what to load. The actual file resolution uses DiaAssetRuntime's path resolution (asset ID → absolute deploy path). Manifest imports in `.diagame` are for the DiaApplication loader (existing system), not for asset resolution. |
| 5 | DiaAssetCatalogue | When enumerating from `.diagame` at tool-time, how does DiaAssetCatalogue discover assets if there's no `assets.catalogue.json` yet? | Feature 1 creates the directory structure. DiaAssetCatalogueEditor's file discovery (existing feature) scans the raw directory tree to discover assets and create/update `assets.catalogue.json`. The `.diagame` provides the root; the editor walks `Global/` and `Stages/`. |
| 6 | pathStoreConfig removal | Other applications (CluicheEditor, GoogleTests) may also use pathStoreConfig.json — does removing it break them? | Each application gets its own asset tree and manifest. CluicheEditor and GoogleTests would need their own `.diagame` with path aliases. This migration is scoped to CluicheTest only. |
| 7 | Webix retention | Webix 5.2.1 is ~30MB of library files. Does it deploy as a folder asset or individual files? | Folder asset (`folder.webix_5_2_1`). DiaAssetPipeline's folder handler deploys the entire directory tree recursively (SD-CAT-013). A single asset ID resolves to the folder root. |
| 8 | Async readiness | The API is "async-ready" — what specifically needs to change when async loading is added? | Only DiaAssetRuntime internals: add a load thread, make `RequestStageLoad` non-blocking (post to queue), fire `OnAssetReady` from load thread. AssetServiceModule's phase gating already polls `IsLoadComplete()` — no change. Consumer code unchanged. |

## Status

`Approved` — 4 features identified, all Draft. Pending feature specs.

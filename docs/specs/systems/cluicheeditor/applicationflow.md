# System Spec: CluicheEditor ApplicationFlow

## Parent Application
@docs/specs/applications/cluicheeditor.md

## Purpose

CluicheEditor ApplicationFlow is the thin application host layer that wires CluicheEditor's DiaEditor library classes into the DiaApplicationFlow v2 framework. It owns the `Application` bootstrap, the `ProcessingUnit`, all `Module` subclasses, and the `.diaapp` v2 manifest that declares stage membership for each module. DiaEditor remains a pure library with no DiaApplicationFlow dependency — this system is the seam where the library meets the framework.

This system supersedes the original v1 wiring (v1 `ProcessingUnit` subclass + three `Phase` subclasses + v1 `Module` subclasses + `.diaapp` v1 manifest).

## Responsibilities

- Own the CluicheEditor `Application` bootstrap entry point (replace v1 `ProcessingUnit::Start/Update/Stop` call site)
- Provide v2 `Module` subclasses wrapping each DiaEditor library object (EditorModel, CommandHistory, EditorView, EditorViewController, GameConnection)
- Declare stage membership via `.diaapp` v2 manifest (Boot and Running stages)
- Delete v1 Phase classes (CluicheEditorBootPhase, CluicheEditorRunningPhase, CluicheEditorShutdownPhase)
- Update CluicheEditor.vcxproj: remove DiaApplication reference, add DiaApplicationFlow reference

## Non-Responsibilities

- **DiaEditor library internals** — modules delegate to library objects; no business logic in wrappers
- **Plugin loading** — handled by DiaEditor library (EditorModel, PluginLoader)
- **CEF subprocess guard** — stays in `wWinMain`, above Application bootstrap
- **Editor UI layout** — owned by DiaEditor/DiaUICEF

## Public Interfaces

### Module Classes

```cpp
namespace Cluiche::Editor {
    // Each module follows identical pattern:
    class EditorModelModule : public Dia::ApplicationFlow::Module {
    public:
        static constexpr Dia::Core::StringCRC kTypeId{"EditorModelModule"};
    protected:
        StartResult DoStart() override;
        void DoUpdate(float deltaTime) override;
        StopResult DoStop() override;
    };
    DIA_MODULE(EditorModelModule);
    // ... CommandHistoryModule, EditorViewModule,
    //     EditorViewControllerModule, GameConnectionModule
}
```

### Bootstrap (wWinMain)

```cpp
int APIENTRY wWinMain(...) {
    // CEF subprocess guard MUST come first
    CefMainArgs mainArgs(hInstance);
    int exitCode = CefExecuteProcess(mainArgs, nullptr, nullptr);
    if (exitCode >= 0) return exitCode;

    Dia::ApplicationFlow::TypeRegistry registry;
    auto manifest = Dia::ApplicationFlow::ApplicationManifestLoaderV2::Load("editor.diaapp");
    Dia::ApplicationFlow::Application app(manifest, registry);

    if (!app.Start()) return 1;
    while (app.Update(deltaTime)) { /* frame loop */ }
    return 0;
}
```

### Manifest (editor.diaapp v2)

```json
{
    "version": 2,
    "stages": ["Boot", "Running"],
    "initial_stage": "Boot",
    "auto_stages": ["Boot"],
    "processing_units": [
        {
            "instance_id": "EditorPU",
            "frequency_hz": 60,
            "dedicated_thread": false,
            "modules": [
                { "instance_id": "EditorModel",          "type_id": "EditorModelModule",          "stages": ["Boot", "Running"] },
                { "instance_id": "CommandHistory",       "type_id": "CommandHistoryModule",       "stages": ["Boot", "Running"] },
                { "instance_id": "SplashScreen",         "type_id": "SplashScreenModule",         "stages": ["Boot"] },
                { "instance_id": "EditorView",           "type_id": "EditorViewModule",           "stages": ["Running"] },
                { "instance_id": "EditorViewController", "type_id": "EditorViewControllerModule", "stages": ["Running"] },
                { "instance_id": "PluginLoader",         "type_id": "PluginLoaderModule",         "stages": ["Running"] },
                { "instance_id": "GameConnection",       "type_id": "GameConnectionModule",       "stages": ["Running"] }
            ]
        }
    ]
}
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| CluicheEditorApplicationFlowMigration | Migrate CluicheEditor from DiaApplication v1 to DiaApplicationFlow v2 | [cluicheapplicationflowmigration.md](../../features/cluicheeditor/applicationflow/cluicheapplicationflowmigration.md) | Draft |

## Dependencies

**Required:**
- **Dia.DiaApplicationFlow** — ProcessingUnit, Module base classes, Application, DIA_MODULE macro
- **Dia.DiaEditor** — Library objects that modules wrap
- **Dia.DiaUICEF** — CEF-based UI (EditorViewModule)
- **Dia.DiaCore** — StringCRC, containers

**Removes:**
- **Dia.DiaApplication (v1)** — ProcessingUnit/Phase/Module v1 base classes (dropped)

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SCED-001 | Phases deleted, not migrated | SD-002 (DiaApplicationFlow) removes Phase concept entirely; Boot→Running is expressed as a stage transition with auto_stages on Boot; no Phase subclasses survive | All features | Accepted | Yes |
| SCED-002 | Boot stage holds EditorModel, CommandHistory, and SplashScreen | CEF (EditorViewModule) must not initialise until Boot completes; SplashScreen shows during Boot; matches original BootPhase intent | All features | Accepted | Yes |
| SCED-003 | Shutdown is via RequestShutdown, not a Shutdown phase | SD-011 (DiaApplicationFlow): shutdown is framework lifecycle, not a stage. EditorViewModule::DoStop handles CEF teardown; EditorView::SaveLayoutToDisk called in DoStop | All features | Accepted | Yes |
| SCED-004 | PluginLoaderModule kept as own v2 Module (revised) | PluginLoaderModule implements IPluginLoader (~200 lines of plugin lifecycle management). Too substantial to absorb; kept as a dedicated Running-stage v2 Module. | All features | Accepted | Yes |
| SCED-005 | LoggerModule absorbed into EditorModelModule | Logger/sink init is a few calls in DoStart; a separate module adds boilerplate for no modularity gain | All features | Accepted | Yes |
| SCED-006 | EditorConsoleSinkModule absorbed into EditorViewModule | Console sink registration is one call in DoStart; kept co-located with the view | All features | Accepted | Yes |
| SCED-007 | SplashScreenModule kept as own v2 Module | Has real Win32 splash window behavior (DoStart creates Win32 splash, Dismiss called from EditorViewModule on shell_ready). Belongs in Boot stage. | All features | Accepted | Yes |

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|-----------------------------|
| PD-001 | Platform | StringCRC for all IDs | Module instance IDs and type IDs are StringCRC |
| PD-002 | Platform | PU/Phase/Module architecture (updating to PU/Stage/Module) | This system adopts PU/Stage/Module via DiaApplicationFlow |
| PD-004 | Platform | No STL containers in public APIs | Module public APIs use DiaCore containers |
| PD-006 | Platform | Visual Studio project files source of truth | CluicheEditor.vcxproj updated manually |
| PD-007 | Platform | C++20 required | constexpr StringCRC kTypeId in each module |
| AED-001 | CluicheEditor App | CluicheEditor owns all application flow; DiaEditor is a pure library | This system is the wiring layer — modules are thin; DiaEditor has no DiaApplicationFlow dependency |
| SD-001 | DiaApplicationFlow | Config is sole source of truth for structural wiring | editor.diaapp v2 manifest declares all modules and stage membership |
| SD-002 | DiaApplicationFlow | Phase concept removed, replaced by Stages | No Phase subclasses; Boot and Running are config-declared stages |
| SD-003 | DiaApplicationFlow | ModuleRef<T> is the sole module access pattern | Cross-module dependencies resolved via ModuleRef<T> in DoStart |
| SD-008 | DiaApplicationFlow | DIA_MODULE one-liner registration | Every module uses DIA_MODULE macro |
| SD-017 | DiaApplicationFlow | Clean break — no v1 backward compatibility | DiaApplication.vcxproj reference removed from CluicheEditor.vcxproj |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Shutdown | How does CluicheEditor signal shutdown in v2? | EditorModel sets a close-requested flag; EditorViewModule::DoUpdate polls `EditorModel::IsCloseRequested()` and calls `GetApplication()->RequestShutdown()` |
| 2 | Module count | Is reducing 8 v1 modules to 5 v2 modules (via absorption) safe? | Yes — PluginLoader, Logger, and ConsoleSink are one-liner initialisations; absorbing them follows SCED-004/005/006 |
| 3 | CEF guard | Does the CEF subprocess guard still work with the v2 bootstrap? | Yes — `CefExecuteProcess` runs before any Dia initialisation; v2 bootstrap follows immediately after the guard |

## Status

`Approved` — 2026-05-09

**Plan:** [applicationflow.plan.md](applicationflow.plan.md)

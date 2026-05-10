# System Spec: CluicheEditor ApplicationFlow

## Parent Application
@docs/specs/applications/cluicheeditor.md

## Purpose

CluicheEditor ApplicationFlow is the thin application host layer that wires CluicheEditor's DiaEditor library classes into the DiaApplicationFlow v2 framework. It owns the `Application` bootstrap, a single `EditorPU` processing unit, all `Module` subclasses, and the `.diaapp` v2 manifest. DiaEditor remains a pure library with no DiaApplicationFlow dependency — this system is the seam where the library meets the framework.

The editor runs entirely within a single `Running` stage. v2's array-order dependency guarantee (modules start in manifest order) replaces the v1 Boot→Running phase gate: EditorModel starts before EditorView without needing a separate stage. There is no shutdown stage — `Application::RequestShutdown()` is called from `EditorViewModule::DoUpdate` when the close button is clicked, and the framework tears all modules down in reverse order.

This system supersedes the original v1 wiring (v1 `ProcessingUnit` subclass + three `Phase` subclasses + v1 `Module` subclasses + `.diaapp` v1 manifest).

## Responsibilities

- Own the CluicheEditor `Application` bootstrap entry point (replace v1 `ProcessingUnit::Start/Update/Stop` call site)
- Provide v2 `Module` subclasses wrapping each DiaEditor library object (EditorModel, CommandHistory, SplashScreen, EditorView, EditorViewController, PluginLoader, GameConnection)
- Declare a single `Running` stage in `Data/editor.diaapp` (v2 manifest); rely on manifest array order to sequence module startup
- Parse the project-path command-line argument inside `EditorModelModule::DoStart` via `GetCommandLineW()` (no coupling to Main.cpp)
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
    // CEF subprocess guard MUST come first — runs in renderer/helper processes.
    CefMainArgs mainArgs(hInstance);
    CefRefPtr<Dia::UICEF::CEFProcessHandler> cefApp = new Dia::UICEF::CEFProcessHandler("");
    int exitCode = CefExecuteProcess(mainArgs, cefApp.get(), nullptr);
    if (exitCode >= 0) return exitCode;

    Dia::ApplicationFlow::ApplicationManifestV2 manifest;
    if (Dia::ApplicationFlow::ApplicationManifestLoaderV2::LoadFromFile(
            "assets/configs/editor.diaapp", manifest) != LoadResult::kSuccess)
        return 1;

    Dia::ApplicationFlow::Application app(manifest, Dia::ApplicationFlow::TypeRegistry::Global());
    if (!app.Start()) return 1;

    // Frame-paced main loop at 120 Hz.  The editor is GUI-only; an uncapped
    // loop wastes CPU on a spin without matching renderer output.
    const float kFrameTimeSec = 1.0f / 120.0f;
    while (app.Update(kFrameTimeSec)) { /* sleep to frame budget */ }
    return 0;
}
```

### Manifest (editor.diaapp v2)

```json
{
    "version": 2,
    "stages": ["Boot", "Running"],
    "initial_stage": "Running",
    "auto_stages": [],
    "processing_units": [
        {
            "instance_id": "EditorPU",
            "frequency_hz": 120,
            "dedicated_thread": false,
            "modules": [
                { "instance_id": "EditorModelModule",          "type_id": "EditorModelModule",          "stages": ["Running"] },
                { "instance_id": "CommandHistoryModule",       "type_id": "CommandHistoryModule",       "stages": ["Running"] },
                { "instance_id": "SplashScreenModule",         "type_id": "SplashScreenModule",         "stages": ["Running"] },
                { "instance_id": "EditorViewControllerModule", "type_id": "EditorViewControllerModule", "stages": ["Running"] },
                { "instance_id": "EditorViewModule",           "type_id": "EditorViewModule",           "stages": ["Running"] },
                { "instance_id": "PluginLoaderModule",         "type_id": "PluginLoaderModule",         "stages": ["Running"] },
                { "instance_id": "GameConnectionModule",       "type_id": "GameConnectionModule",       "stages": ["Running"] }
            ]
        }
    ]
}
```

**Array order is startup order.** EditorViewController (index 3) starts before EditorView (index 4) so `ModuleRef<EditorViewControllerModule>` resolves at EditorView's `DoStart`. PluginLoader (index 5) comes after EditorView because it registers plugins with the view. Manifest `instance_id` matches `type_id` so `ModuleRef<T>`'s default lookup (`instanceId = T::kTypeId`) resolves without explicit overrides.

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| CluicheEditorApplicationFlowMigration | Migrate CluicheEditor from DiaApplication v1 to DiaApplicationFlow v2 | [cluicheapplicationflowmigration.md](../../features/cluicheeditor/applicationflow/cluicheapplicationflowmigration.md) | Done |

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
| SCED-001 | Phases deleted, not migrated | SD-002 (DiaApplicationFlow) removes Phase concept entirely; v2's array-order startup replaces the phase gate | All features | Accepted | Yes |
| SCED-002 | Single `Running` stage (no Boot stage) | v2 `auto_stages` only fires via `ApplyPendingTransition`; the initial stage is entered directly in `Application::Start` and does not trigger auto-advance. A Boot→Running split would hang at startup. Array-order manifest entries give the same startup-order guarantee without needing two stages. | All features | Accepted | Yes |
| SCED-003 | Shutdown is via RequestShutdown, not a Shutdown phase | SD-011 (DiaApplicationFlow): shutdown is framework lifecycle, not a stage. EditorViewModule::DoStop handles CEF teardown; EditorView::SaveLayoutToDisk called in DoStop | All features | Accepted | Yes |
| SCED-004 | PluginLoaderModule kept as own v2 Module (revised) | PluginLoaderModule implements IPluginLoader (~200 lines of plugin lifecycle management). Too substantial to absorb; kept as a dedicated v2 Module. | All features | Accepted | Yes |
| SCED-005 | LoggerModule absorbed into EditorModelModule | Logger/sink init is a few calls in DoStart; a separate module adds boilerplate for no modularity gain | All features | Accepted | Yes |
| SCED-006 | EditorConsoleSinkModule absorbed into EditorViewModule | Console sink registration is one call in DoStart; kept co-located with the view | All features | Accepted | Yes |
| SCED-007 | SplashScreenModule kept as own v2 Module | Has real Win32 splash window behavior (DoStart creates Win32 splash, Dismiss called from EditorViewModule on shell_ready) | All features | Accepted | Yes |
| SCED-008 | Manifest `instance_id` matches `type_id` for all modules | `ModuleRef<T>` defaults to looking up `T::kTypeId` as the instance id. Using different instance/type names would require every ModuleRef constructor to pass an explicit instance id — error-prone. Matching them lets the default resolve correctly. | All features | Accepted | Yes |
| SCED-009 | Frame pacing at 120 Hz | GUI-only editor — uncapped loop wastes CPU spinning at >30,000 FPS with no output. 120 Hz matches high-refresh displays and CEF's default paint cadence. Main loop sleeps to the frame budget. | All features | Accepted | Yes |

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
| SD-002 | DiaApplicationFlow | Phase concept removed, replaced by Stages | No Phase subclasses; a single `Running` stage holds every module. See SCED-002 for why Boot was not kept as a separate stage. |
| SD-003 | DiaApplicationFlow | ModuleRef<T> is the sole module access pattern | Cross-module dependencies resolved via ModuleRef<T> in DoStart |
| SD-008 | DiaApplicationFlow | DIA_MODULE one-liner registration | Every module uses DIA_MODULE macro |
| SD-017 | DiaApplicationFlow | Clean break — no v1 backward compatibility | DiaApplication.vcxproj reference removed from CluicheEditor.vcxproj |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Shutdown | How does CluicheEditor signal shutdown in v2? | EditorModel sets a close-requested flag; EditorViewModule::DoUpdate polls `EditorModel::IsCloseRequested()` and calls `GetProcessingUnit()->GetApplication()->RequestShutdown()` |
| 2 | Module count | Is reducing 8 v1 modules to 7 v2 modules (Logger + ConsoleSink absorbed) safe? | Yes — Logger/sink init is a few calls in DoStart. PluginLoader was originally flagged for absorption (SCED-004 "old") but is too substantial (~200 lines, implements IPluginLoader). Revised SCED-004 keeps it. |
| 3 | CEF guard | Does the CEF subprocess guard still work with the v2 bootstrap? | Yes — `CefExecuteProcess` runs before any Dia initialisation; v2 bootstrap follows immediately after the guard |
| 4 | Startup order | How does v2 guarantee EditorModel starts before EditorView without a Boot stage? | Manifest array order is startup order (engine-enforced by `ManifestValidatorV2` DEPENDENCY_ORDER rule). EditorModelModule is at index 0, EditorViewModule at index 4; `ModuleRef<EditorModelModule>::Get()` returns non-null by the time EditorView's `DoStart` runs in the same frame. |
| 5 | CEF async init | Should EditorView's `DoStart` return `kLoading` until CEF is ready? | No — `IUISystem` does not expose `IsReady()`. CEF's `CefInitialize` is synchronous (renderer/GPU helpers spawn asynchronously but the host-side `IUISystem` is usable immediately). `DoStart` returns `kReady` after `CreateEditorUISystem`, matching v1 behaviour. |
| 6 | Frame pacing | Why 120 Hz and not 60 Hz? | 120 Hz matches high-refresh displays and CEF's default paint cadence. Main loop sleeps to the frame budget so CPU is only used when input/rendering needs it. |

## Status

`Approved` — 2026-05-09

**Plan:** [applicationflow.plan.md](applicationflow.plan.md)

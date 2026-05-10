# Implementation Plan: CluicheEditor ApplicationFlow Migration

**Spec:** [applicationflow.md](applicationflow.md)
**Feature:** [cluicheapplicationflowmigration.md](../../features/cluicheeditor/applicationflow/cluicheapplicationflowmigration.md)
**Status:** Not Started

---

## Session Notes

**Spec decisions summary (Platform → App → System → Feature):**

CluicheEditor is migrating from DiaApplication v1 (ProcessingUnit/Phase/Module) to DiaApplicationFlow v2 (Application/Stage/Module). Key constraints:

- **SD-017 (DiaApplicationFlow):** Clean break — DiaApplication.vcxproj removed from CluicheEditor.vcxproj. No v1 headers survive.
- **SD-002 (DiaApplicationFlow):** Phase concept removed entirely. Boot/Running/Shutdown phases are deleted; Boot and Running are config-declared stages in `Data/editor.diaapp`. `auto_stages: ["Boot"]` drives the Boot→Running transition automatically.
- **SD-001:** Config is sole source of truth — all module stage membership in manifest only.
- **SD-003:** `ModuleRef<T>` is the only cross-module access pattern; replaces `GetModule<T>()` calls.
- **SD-008:** `DIA_MODULE(ClassName)` one-liner macro at file scope in each `.cpp`.
- **AED-001 (CluicheEditor App):** DiaEditor is a pure library — module wrappers are thin (DoStart inits, DoUpdate ticks, DoStop shuts down). No business logic in modules.
- **SCED-002:** Boot stage holds only EditorModel + CommandHistory. EditorView/ViewController/GameConnection are Running-stage only.
- **SCED-003:** Shutdown via `Application::RequestShutdown()` called from `EditorViewModule::DoUpdate` when `EditorModel::IsCloseRequested()`.
- **SCED-004/005/006:** PluginLoaderModule + LoggerModule absorbed into EditorModelModule; EditorConsoleSinkModule absorbed into EditorViewModule. SplashScreenModule dropped.
- **AI Review Q1:** `EditorViewModule::DoStart` returns `kLoading` until `CEFUISystem::IsReady()`, then `kReady`.
- **wWinMain note:** v1 had command-line project path parsing (`SetProjectPath`) — this must be preserved in the v2 bootstrap, passed to EditorModelModule before `app.Start()` or via module config in the manifest.

**v1 → v2 file mapping:**

| v1 files (delete) | v2 files (create/rewrite) |
|---|---|
| `CluicheEditorProcessingUnit.h/.cpp` | — (replaced by Application in Main.cpp) |
| `Phases/CluicheEditorBootPhase.h/.cpp` | — (deleted, no replacement) |
| `Phases/CluicheEditorRunningPhase.h/.cpp` | — (deleted, no replacement) |
| `Phases/CluicheEditorShutdownPhase.h/.cpp` | — (deleted, no replacement) |
| `Modules/EditorModelModule.h/.cpp` | `Modules/EditorModelModule.h/.cpp` (rewritten) |
| `Modules/CommandHistoryModule.h/.cpp` | `Modules/CommandHistoryModule.h/.cpp` (rewritten) |
| `Modules/EditorViewModule.h/.cpp` | `Modules/EditorViewModule.h/.cpp` (rewritten) |
| `Modules/EditorViewControllerModule.h/.cpp` | `Modules/EditorViewControllerModule.h/.cpp` (rewritten) |
| `Modules/SplashScreenModule.h/.cpp` | — (deleted, no replacement) |
| `Modules/PluginLoaderModule.h/.cpp` | — (absorbed into EditorModelModule) |
| `Modules/LoggerModule.h/.cpp` | — (absorbed into EditorModelModule) |
| `Modules/EditorConsoleSinkModule.h/.cpp` | — (absorbed into EditorViewModule) |
| `Data/test-editor-plugins.diaapp` (v1) | `Data/editor.diaapp` (v2) |
| `Main.cpp` (v1 bootstrap) | `Main.cpp` (v2 bootstrap, rewritten) |

---

## Implementation Patterns

### v2 Module pattern (all 5 modules follow this)

```cpp
// EditorModelModule.h
#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaEditor/MVC/EditorModel.h>

namespace Cluiche::Editor {
    class EditorModelModule : public Dia::ApplicationFlow::Module {
    public:
        static constexpr Dia::Core::StringCRC kTypeId{"EditorModelModule"};
        EditorModelModule();
        Dia::Editor::EditorModel& GetModel() { return mModel; }
    protected:
        StartResult DoStart() override;
        void DoUpdate(float deltaTime) override;
        StopResult DoStop() override;
    private:
        Dia::Editor::EditorModel mModel;
    };
}

// EditorModelModule.cpp
#include "EditorModelModule.h"
DIA_MODULE(Cluiche::Editor::EditorModelModule);
```

Key changes from v1:
- Constructor takes no `ProcessingUnit*` argument — framework injects PU
- `kTypeId` is `static constexpr StringCRC{...}` (not `static const` with cpp-side definition)
- `DoStart()` returns `StartResult` (`kReady`/`kLoading`/`kFailed`)
- `DoUpdate(float deltaTime)` — receives delta time from framework
- `DoStop()` returns `StopResult` (`kDone`/`kStopping`)
- `DIA_MODULE(ClassName)` at file scope in `.cpp`
- No `IBuildDependencies` — cross-module deps use `ModuleRef<T>` in `DoStart`

### EditorViewModule async CEF pattern

```cpp
StartResult EditorViewModule::DoStart() override {
    if (!mInitStarted) {
        // Create window + start CEF init (async)
        mWindow = ...; // Win32Window
        mWindow->SetCloseCallback([this]{ /* EditorModel::RequestClose */ });
        mUISystem = ...; // CEFUISystem init
        mInitStarted = true;
    }
    return mUISystem->IsReady() ? StartResult::kReady : StartResult::kLoading;
}
```

### Shutdown pattern

```cpp
void EditorViewModule::DoUpdate(float deltaTime) override {
    mView.Update(deltaTime);
    if (/* EditorModel via ModuleRef */.GetModel().IsCloseRequested())
        TransitionTo(/* RequestShutdown equivalent */); // or GetApplication()->RequestShutdown()
}
```

### Cross-module access via ModuleRef

```cpp
// EditorViewControllerModule.h
ModuleRef<EditorModelModule> mModelRef{this};  // resolved in DoStart
ModuleRef<EditorViewModule>  mViewRef{this};

StartResult EditorViewControllerModule::DoStart() {
    mViewController.Init(mModelRef->GetModel(), mViewRef->GetView());
    return StartResult::kReady;
}
```

### v2 Manifest (`Data/editor.diaapp`)

```json
{
    "version": 2,
    "stages": ["Boot", "Running"],
    "initial_stage": "Boot",
    "auto_stages": ["Boot"],
    "processing_units": [{
        "instance_id": "EditorPU",
        "frequency_hz": 30,
        "dedicated_thread": false,
        "modules": [
            { "instance_id": "EditorModel",          "type_id": "EditorModelModule",          "stages": ["Boot", "Running"] },
            { "instance_id": "CommandHistory",        "type_id": "CommandHistoryModule",       "stages": ["Boot", "Running"] },
            { "instance_id": "EditorView",            "type_id": "EditorViewModule",           "stages": ["Running"] },
            { "instance_id": "EditorViewController",  "type_id": "EditorViewControllerModule", "stages": ["Running"] },
            { "instance_id": "GameConnection",        "type_id": "GameConnectionModule",       "stages": ["Running"] }
        ]
    }]
}
```

### v2 Bootstrap (`Main.cpp`)

CEF subprocess guard and project-path argument parsing preserved from v1:

```cpp
int APIENTRY wWinMain(HINSTANCE hInstance, ...) {
    CefMainArgs mainArgs(hInstance);
    CefRefPtr<Dia::UICEF::CEFProcessHandler> cefApp = new Dia::UICEF::CEFProcessHandler("");
    int exitCode = CefExecuteProcess(mainArgs, cefApp.get(), nullptr);
    if (exitCode >= 0) return exitCode;

    // Parse project path arg (existing logic preserved)
    char projectPath[260] = {};
    // ... wide-to-utf8 conversion ...

    Dia::ApplicationFlow::TypeRegistry registry;
    // Register all 5 module types
    // ... or rely on DIA_MODULE static registration

    auto manifest = Dia::ApplicationFlow::ApplicationManifestLoaderV2::Load("Data/editor.diaapp");
    Dia::ApplicationFlow::Application app(manifest, registry);
    // Inject project path via module config or pre-start call

    if (!app.Start()) return 1;
    const float kDeltaTime = 1.0f / 30.0f;
    while (app.Update(kDeltaTime)) {}
    return 0;
}
```

**Open implementation question for Task 3:** How does `projectPath` reach `EditorModelModule`? Options: (a) module config field in manifest, (b) a pre-Start injection API on the module, (c) command-line accessible via a global. Resolve at implementation time by checking how DiaApplicationFlow module config is passed.

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Delete v1 Phase classes (Boot, Running, Shutdown — 5 files total: 3 `.h` + 2 `.cpp`, ShutdownPhase has only `.cpp`) | Build passes with no reference errors | Done | haiku | |
| 2 | Rewrite 7 v2 Module classes (EditorModel, CommandHistory, SplashScreen, EditorView, EditorViewController, PluginLoader, GameConnection) | Modules compile; DIA_MODULE macro present in each .cpp | Done | sonnet | Logger absorbed into EditorModel; ConsoleSink absorbed into EditorView. kTypeId must be `static const` not `constexpr` (StringCRC has no constexpr ctor). ModuleRef members with forward-declared types must be init'd in .cpp constructor, not header brace-init. |
| 3 | Rewrite Main.cpp bootstrap (remove v1 PU, add Application + ManifestLoaderV2; preserve CEF guard + project path arg) | Main.cpp compiles | Done | sonnet | Project path injected via GetCommandLineW() inside EditorModelModule::DoStart |
| 4 | Write Data/editor.diaapp v2 manifest | Manifest validates against v2 schema | Done | haiku | |
| 5 | Update CluicheEditor.vcxproj: swap DiaApplication → DiaApplicationFlow ProjectReference; remove DiaApplication AdditionalIncludeDirectories; add/remove source file entries | vcxproj loads in VS without errors | Done | haiku | DiaApplicationFlow ref was already present; no DiaApplication ref to remove |
| 6 | Update CluicheEditor.vcxproj.filters: remove deleted files, add new Module files | Filters file consistent with vcxproj | Done | haiku | |
| 7 | Check GoogleTests/ApplicationFlow/ for CluicheEditor tests; update any found to v2 API | Affected tests compile and pass | Done | haiku | No CluicheEditor-specific tests found |
| 8 | Verify: `dia pipeline --target cluicheeditor --config Debug` passes | AC-7 — build passes | Done | haiku | `✓ pipeline complete  3 passed · 0 failed · 21.4s` |

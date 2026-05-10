# Feature Spec: CluicheEditorApplicationFlowMigration

## Traceability

| Level | Spec |
|-------|------|
| Platform | @docs/specs/platform/Cluiche.md |
| Application | @docs/specs/applications/cluicheeditor.md |
| System | @docs/specs/systems/cluicheeditor/applicationflow.md |
| Feature | this file |

## Summary

Migrate CluicheEditor from the deprecated DiaApplication v1 API to DiaApplicationFlow v2. The three v1 Phase classes are deleted (phases are removed in v2). The eight v1 Module classes are rewritten as five v2 Module subclasses (three absorbed per SCED-004/005/006). The v1 ProcessingUnit subclass is replaced by a v2 `Application` bootstrap in `wWinMain`. The `.diaapp` manifest is upgraded from v1 to v2 format. The DiaApplication.vcxproj reference is dropped from CluicheEditor.vcxproj and replaced with DiaApplicationFlow.vcxproj.

## Goals

- CluicheEditor compiles and runs with zero DiaApplication v1 dependencies
- All editor functionality (boot, running, CEF UI, plugin loading, shutdown) preserved
- DIA_MODULE macro registers all five modules; manifest declares stage membership
- Hard cut — no v1 shim retained

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC-1 | `CluicheEditorProcessingUnit.h/.cpp` deleted; `Application` bootstrapped in `wWinMain` using `ApplicationManifestLoaderV2` |
| AC-2 | All three v1 Phase classes (`CluicheEditorBootPhase`, `CluicheEditorRunningPhase`, `CluicheEditorShutdownPhase`) deleted |
| AC-3 | Seven v2 Module subclasses created: `EditorModelModule`, `CommandHistoryModule`, `SplashScreenModule`, `EditorViewModule`, `EditorViewControllerModule`, `PluginLoaderModule`, `GameConnectionModule`; each uses `DIA_MODULE` registration macro (Logger absorbed into EditorModel; ConsoleSink absorbed into EditorView) |
| AC-4 | `editor.diaapp` manifest upgraded to v2 schema (version: 2, single `Running` stage, `frequency_hz: 120`, `instance_id` == `type_id` for each module, `stages: ["Running"]` per module) deployed to `assets/configs/editor.diaapp` |
| AC-5 | `CluicheEditor.vcxproj` references `DiaApplicationFlow.vcxproj` and removes reference to `DiaApplication.vcxproj` |
| AC-6 | `CluicheEditor.vcxproj` and `.vcxproj.filters` updated to reflect deleted/added source files |
| AC-7 | `CluicheEditor.exe` launches, reaches Running stage, CEF initialises (DevTools listening on port 9222), and 6 CEF helper processes spawn alongside the main process |

## Tasks

| # | Task | AC | Status |
|---|------|----|--------|
| 1 | Delete v1 Phase classes (3 files: Boot, Running, Shutdown) | AC-2 | Not Started |
| 2 | Rewrite 5 v2 Module classes (EditorModel, CommandHistory, EditorView, EditorViewController, GameConnection) with DIA_MODULE registration | AC-3 | Not Started |
| 3 | Replace wWinMain bootstrap: remove v1 PU, add Application + ManifestLoaderV2 | AC-1 | Not Started |
| 4 | Upgrade editor.diaapp manifest to v2 schema | AC-4 | Not Started |
| 5 | Update CluicheEditor.vcxproj: swap DiaApplication → DiaApplicationFlow, add/remove source files | AC-5, AC-6 | Not Started |
| 6 | Update CluicheEditor.vcxproj.filters to reflect added/removed files | AC-6 | Not Started |
| 7 | Verify: `dia run cluicheeditor` boots to Running stage | AC-7 | Not Started |

## Binding Decisions Compliance

| Decision ID | Source | Decision (summary) | Compliance |
|-------------|--------|--------------------|------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | All module `kTypeId` and instance IDs are `constexpr StringCRC`. Config strings resolve to CRC at load time. |
| PD-002 | Platform | PU/Phase/Module architecture (updated to PU/Stage/Module) | Adopts PU/Stage/Module via DiaApplicationFlow v2. Phase concept removed per SD-002. |
| PD-004 | Platform | No STL containers in public APIs | Module public APIs use DiaCore containers only. |
| PD-006 | Platform | Visual Studio project files are source of truth | CluicheEditor.vcxproj updated manually; no generated project files. |
| PD-007 | Platform | C++20 required | `static const Dia::Core::StringCRC kTypeId` in each module (StringCRC has no constexpr ctor, so `constexpr` was revised to `const` during implementation — pattern matches CluicheTest). |
| AED-001 | CluicheEditor App | CluicheEditor owns all application flow; DiaEditor is pure library | Modules are thin wrappers — all business logic remains in DiaEditor library classes. DiaEditor has no DiaApplicationFlow include. |
| SD-001 | DiaApplicationFlow | Config is sole source of truth for structural wiring | `editor.diaapp` v2 manifest is the single declaration of modules and the EditorPU. No code-side wiring. |
| SD-002 | DiaApplicationFlow | Phase concept removed, replaced by Stages | All three v1 Phase classes deleted. CluicheEditor uses a single `Running` stage (see SCED-002) — array order replaces the Boot→Running gate. |
| SD-003 | DiaApplicationFlow | ModuleRef<T> is sole module access pattern | Any cross-module dependency (e.g., EditorViewController needing EditorModel) resolved via `ModuleRef<EditorModelModule>` member + init in constructor. |
| SD-008 | DiaApplicationFlow | DIA_MODULE one-liner registration | Every module file ends with `DIA_MODULE(XxxModule);`. |
| SD-017 | DiaApplicationFlow | Clean break — no v1 backward compatibility | No v1 headers included anywhere in CluicheEditor. DiaApplicationFlow vcxproj was already the project reference. |
| SCED-001 | CluicheEditor AppFlow | Phases deleted, not migrated | Compliant — all three Phase files deleted. |
| SCED-002 | CluicheEditor AppFlow | Single Running stage | Compliant — manifest has `stages: ["Running"]` and all 7 modules belong to it. |
| SCED-003 | CluicheEditor AppFlow | Shutdown via RequestShutdown, not Shutdown phase | Compliant — EditorViewModule::DoUpdate polls `IsCloseRequested()` and calls `GetProcessingUnit()->GetApplication()->RequestShutdown()`. No Shutdown phase. |
| SCED-004 | CluicheEditor AppFlow | PluginLoaderModule kept as own v2 Module | Compliant — PluginLoaderModule class exists; full IPluginLoader impl ported. |
| SCED-005 | CluicheEditor AppFlow | LoggerModule absorbed into EditorModelModule | Compliant — logger/sink setup is a few calls in `EditorModelModule::DoStart`. `editor-logger.json` loaded at start. |
| SCED-006 | CluicheEditor AppFlow | EditorConsoleSinkModule absorbed into EditorViewModule | Compliant — console sink registration inline in `EditorViewModule::DoStart`. |
| SCED-007 | CluicheEditor AppFlow | SplashScreenModule kept as own v2 Module | Compliant — Win32 splash preserved, dismissed via `shell_ready` handler in EditorViewModule. |
| SCED-008 | CluicheEditor AppFlow | Manifest `instance_id` matches `type_id` | Compliant — all 7 modules use the same string for both fields. |
| SCED-009 | CluicheEditor AppFlow | Frame pacing at 120 Hz | Compliant — `kFrameTimeSec = 1.0f / 120.0f` in Main.cpp's loop, with sleep to frame budget. |

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Module: EditorView | EditorViewModule::DoStart creates the Win32Window and initialises CEF — does DoStart returning `kLoading` for async CEF init need to be handled? | Yes — CEF init is async; DoStart should return `kLoading` until `CEFUISystem::IsReady()`, then `kReady` | Yes — kLoading until IsReady(), then kReady. |
| 2 | Module: EditorView | Where does the close callback (`EditorModel::RequestClose`) get registered in v2? | In `EditorViewModule::DoStart` after Win32Window creation — same as v1, just moved from Phase to Module | EditorViewModule::DoStart, same as v1 but moved from Phase to Module. |
| 3 | Manifest | Does `editor.diaapp` live alongside `wWinMain` or in a `Data/` subdirectory? | `Data/editor.diaapp` — mirrors existing `Data/test-editor-plugins.diaapp` location | `Data/editor.diaapp`. |
| 4 | vcxproj | Are there any include-path entries in CluicheEditor.vcxproj pointing to DiaApplication headers that also need removing? | Yes — any `<AdditionalIncludeDirectories>` entries referencing `Dia/DiaApplication` must be removed and `Dia/DiaApplicationFlow` added | Yes — remove all DiaApplication include paths, add DiaApplicationFlow. |
| 5 | Testing | Is there an existing GoogleTest covering CluicheEditor boot that needs updating? | Check `Cluiche/Tests/GoogleTests/ApplicationFlow/` — if CluicheEditor tests exist, update to v2 API | Check and update if found. |

## Status

`Done` — 2026-05-09

**Plan:** [applicationflow.plan.md](../../../systems/cluicheeditor/applicationflow.plan.md)

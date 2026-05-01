# Feature Spec: Per-App Bin Directory Layout

## Status
`Done`

## Traceability

| Level | Spec |
|-------|------|
| Platform | @docs/specs/platform/Cluiche.md |
| Application | @docs/specs/applications/dia.md |
| System | @docs/specs/systems/dia/diapipeline.md |
| Research | @docs/research/bin_dir_deploy/summary.md |

## Problem Statement

All build outputs for every project land in a single flat `Cluiche/bin/Debug/x64/` directory, making it impossible to tell which files belong to which application and preventing a clean self-contained runnable folder per app.

## Goals

1. Each application target gets its own self-contained runnable folder under `Cluiche/bin/<AppName>/$(Configuration)/$(Platform)/`
2. Static and dynamic libraries land in a shared linker-visible pool (`Cluiche/bin/sharedlibs/$(Configuration)/$(Platform)/`)
3. `path_resolver.py` derives the correct per-app `OutDir` from the pipeline target name so DIA CLI deploys data files into the right folder
4. Deploy remains fully automated via existing `.vcxproj` post-build events — no manual step required
5. No per-project `.vcxproj` changes required

## Acceptance Criteria

1. `Directory.Build.props` branches `OutDir` on `$(ConfigurationType)`: `Application` → `Cluiche/bin/$(MSBuildProjectName)/$(Configuration)/$(Platform)/`; all other types → `Cluiche/bin/sharedlibs/$(Configuration)/$(Platform)/`. No `.vcxproj` file may override `OutDir`.
2. `path_resolver.py` `resolve_out_dir` accepts an optional `app_name` parameter; when provided it returns `Cluiche/bin/<app_name>/<config>/<platform>/`; when `None` it returns `Cluiche/bin/sharedlibs/<config>/<platform>/` (backward compatible)
3. `resolve_variables` passes `app_name` through so `$(OutDir)` in `pipeline.toml` rules resolves to the per-app path
4. `package_stage.py` passes the pipeline target name as `app_name` when calling `resolve_variables`
5. After `dia pipeline deploy <target>`, the app's folder contains everything needed to launch it (exe, PDB, all runtime DLLs, all data files) with no manual steps
6. `GoogleTests.vcxproj` post-build event uses `$(TargetDir)` not `$(OutDir)` to reference the exe output directory
7. `test_dia_pipeline.py` tests updated to cover per-app and shared-pool (`sharedlibs/`) path resolution
8. A clean build produces runnable folders: `bin/CluicheEditor/Debug/x64/`, `bin/CluicheTest/Debug/x64/`, `bin/GoogleTests/Debug/x64/`

## CluicheEditor Output Layout

The CluicheEditor runnable folder follows a structured layout:

```
bin/CluicheEditor/<Config>/<Platform>/
  CluicheEditor.exe
  CluicheEditor.pdb
  libcef.dll  chrome_elf.dll  d3dcompiler_47.dll  dxcompiler.dll
  dxil.dll  libEGL.dll  libGLESv2.dll  vk_swiftshader.dll  vulkan-1.dll
  cef/
    DiaUICEF_TestSubprocess.exe
    DiaUICEF_TestSubprocess.pdb
    chrome_100_percent.pak  chrome_200_percent.pak
    icudtl.dat  resources.pak  v8_context_snapshot.bin
    vk_swiftshader_icd.json
    locales/
  plugins/
    home/  outputconsole/  gameconnection/  pluginbrowser/  stub/  hello/
    diaapplicationeditor/
    diapipelineeditor/
  assets/
    configs/     ← editor-*.json, .diaapp, .cluicheproj
    ui/          ← main editor shell React app
    icons/       ← icon.ico, splash-logo.png, etc.
```

CEF DLLs must remain at root — Windows DLL loader constraint.

## Out of Scope

- Moving `bin/intermediate/` or `bin/log/` — these are already per-project and unaffected
- Deleting the stale `bin/exe/` directory — separate cleanup task
- Changing `pipeline.toml` deploy rules (file paths are relative; `$(OutDir)` resolution handles routing)

## Tasks

| # | Task | Status | Notes |
|---|------|--------|-------|
| 1 | Update `Directory.Build.props` | Done | Conditional on `$(ConfigurationType)`; Application → per-app, all else → sharedlibs |
| 2 | Update `path_resolver.py` | Done | `app_name=None` returns sharedlibs path |
| 3 | Update `package_stage.py` | Done | Passes target name as `app_name` |
| 4 | Fix `GoogleTests.vcxproj` | Done | Uses `$(TargetDir)` in post-build event |
| 5 | Update `test_dia_pipeline.py` | Done | Covers per-app and sharedlibs paths |
| 6 | Clean CluicheEditor output layout | Done | cef/, plugins/, assets/ structure; pipeline.toml and C++ paths updated |

## Binding Decisions Compliance

| Decision | Summary | Compliance |
|----------|---------|------------|
| PD-001 StringCRC | Use StringCRC for all entity/component IDs | Not applicable — this feature is Python + MSBuild only; no C++ entity IDs involved |
| PD-002 ProcessingUnit/Phase/Module | Use ProcessingUnit architecture for app structure | Not applicable — no C++ application framework changes |
| PD-003 Component-based entities | Use IComponent/IComponentObject for entities | Not applicable — no C++ entity changes |
| PD-004 No STL in public APIs | Use DiaCore containers in public APIs | Not applicable — Python only; no C++ public API changes |
| PD-005 x64 Windows only | x64 is the sole build target | Compliant — `Directory.Build.props` change applies to x64 only; no platform conditional required |
| PD-006 VS project files are source of truth | MSBuild project files drive all builds | Compliant — `Directory.Build.props` is a VS MSBuild file; no build logic moved outside it |
| PD-007 C++20 required | All projects compile under `/std:c++20` | Compliant — `Directory.Build.props` change does not touch `LanguageStandard` |
| PD-008 `Directory.Build.props` owns OutDir/IntDir | No per-project OutDir overrides | Compliant — OutDir conditional lives in `Directory.Build.props` only; all four per-project `.vcxproj` overrides removed |
| PD-009 Non-binary output under `Cluiche/out/<AppName>/` | Generated non-binary output in `out/` | Compliant — `bin/` is binary output; this feature does not change `out/` layout |
| AD-001 Module system with YAML frontmatter | Each module has architecture doc | Not applicable — no new C++ module introduced |
| AD-002 No STL in public APIs | Use Dia containers | Not applicable — Python only |
| AD-003 Namespace `Dia::<Module>::` | Namespace convention | Not applicable — Python only |
| SD-PIPE-001 `pipeline.toml` is single source of truth | All pipeline config in `pipeline.toml` | Compliant — deploy rules updated to reflect new folder layout; `$(OutDir)` resolution consistent |
| SD-PIPE-002 Fixed stage ordering | compile-code → build-assets → deploy | Compliant — stage order unchanged |
| SD-PIPE-003 `$(OutDir)` resolved from `Directory.Build.props` conventions at runtime | Path resolution consistent with MSBuild | Compliant — `path_resolver.py` mirrors the new `Directory.Build.props` convention including sharedlibs pool |
| SD-PIPE-006 Deploy coexists with `.vcxproj` post-build events | No forced xcopy removal | Compliant — all post-build events already call DIA CLI; no xcopy present |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | `Directory.Build.props` | How does `$(ConfigurationType)` behave for projects that produce both a `.lib` and a `.exe` (none currently, but could occur)? | N/A — all current projects are either `Application` or `StaticLibrary`, never both. Not a real risk. |
| 2 | `path_resolver.py` | How does `resolve_out_dir` map a pipeline target name (e.g. `cluicheeditor`) to the MSBuild project name (e.g. `CluicheEditor`)? Convention-based capitalisation, explicit map, or a new field in `pipeline.toml`? | Derive from the `project` field in `pipeline.toml` — strip the `.vcxproj` extension and take the filename: e.g. `Cluiche/CluicheEditor/CluicheEditor.vcxproj` → `CluicheEditor`. Zero-config; always consistent with the MSBuild project name. |
| 3 | Linker paths | After moving Application outputs to per-app subfolders, are there any `.vcxproj` files that hardcode `bin/Debug/x64` as an `AdditionalLibraryDirectories` path that would break? | No — grep confirms zero hardcoded `bin/Debug` or `bin/Release` paths across all `.vcxproj` files. All use `$(OutDir)` or `$(LibOutDir)` macros and follow the new layout automatically. |
| 4 | Acceptance criteria 8 | `bin/GoogleTests/` — the MSBuild project is named `GoogleTests`; the pipeline target is `googletest` (lowercase). Does the per-app folder name follow the MSBuild project name? | Yes — MSBuild project name, derived from the `.vcxproj` filename. Folders are `bin/GoogleTests/`, `bin/CluicheEditor/`, `bin/CluicheTest/`. |
| 5 | `test_dia_pipeline.py` | Should existing resolver tests be updated in-place or kept alongside new per-app tests? | Existing tests kept and remain valid — `app_name=None` preserves the sharedlibs path. New per-app test cases added alongside them. |
| 6 | `DiaUICEF_TestSubprocess.exe` | Which folder should it land in? | `cef/` subfolder — deployed via pipeline.toml; subprocess path in `EditorViewModule.cpp` updated to `"cef/DiaUICEF_TestSubprocess.exe"`. |
| 7 | New projects | How do new Application projects get the correct output folder automatically? | `Directory.Build.props` conditional on `$(ConfigurationType)=='Application'` handles it automatically — no per-project action needed. Adding a per-project `<OutDir>` override would violate PD-008. |

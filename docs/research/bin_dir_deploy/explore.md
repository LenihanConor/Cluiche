# Research: Explore — Bin Directory Layout & Clean Deploy

**Session date:** 2026-04-27
**Folder:** docs/research/bin_dir_deploy/

## Problem Space Overview

Today `Cluiche/bin/<Config>/<Platform>/` is a flat soup: every `.lib`, `.pdb`, `.bsc`, `.idb`, and `.exe` from all 20+ projects lands in the same directory, mixed together with deployed runtime data (CEF DLLs, SFML DLLs, UI dist folders, JSON configs, Python DLLs). There is no isolation between applications. Running `CluicheEditor.exe` and `CluicheTest.exe` from the same folder means runtime artifacts for both are interleaved, making it hard to tell which files belong to which app.

The `out/` tree already demonstrates a cleaner model: `Cluiche/out/<AppName>/` gives each application its own rooted output space. The goal of this research is to define an equivalent per-app structure for `bin/`, ensure that every file needed to launch an app is co-located under that app's bin folder, and route all data-file deployment through DIA CLI's package stage so it is auditable and consistent.

The secondary pain is deploy correctness: currently data files (DLLs, assets, configs, UI dist) are copied into `OutDir` by MSBuild post-build xcopy steps in individual `.vcxproj` files OR by `dia pipeline deploy`. The two mechanisms are not always in sync. A clean design should make DIA CLI the sole authority for deploying non-binary files, with `Directory.Build.props` the sole authority for binary output paths.

## Existing Approaches

- **Unity / Unreal model** — each target produces a self-contained folder: `Build/<App>/<Config>/`. All runtime dependencies (DLLs, assets) are copied in by the build system. Running the app = just run the exe in that folder.
- **CMake INSTALL target** — binaries go to `bin/`, shared libs to `lib/`, data to `share/<app>/`. Separation is semantic but flat across apps.
- **Cargo / Rust** — `target/debug/` contains the binary + all dylibs needed at runtime. No per-app subfolder by default, but workspaces allow per-crate outputs.
- **Makefile staging** — explicit `dist/<app>/` staging step copies only what is needed; the "dist" folder is the only thing you ship or run from.
- **MSBuild OutputPath override** — each `.vcxproj` sets its own `OutDir`; shared libs produce duplicate copies but each app is fully self-contained.

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| **Binary isolation** | Flat shared (`bin/Debug/x64/`) · Per-app (`bin/<App>/Debug/x64/`) · Per-app per-config (`bin/<App>/<Config>/`) | Current = flat shared |
| **Lib/exe separation** | All in same OutDir · Libs in `bin/lib/`, exes in `bin/<App>/` · Libs only in IntDir | Libs don't need to be runnable; mixing them with exes is the core noise problem |
| **Data deploy authority** | MSBuild xcopy · DIA CLI only · Hybrid | User wants DIA CLI only |
| **Deploy timing** | Always (every build) · On-demand (`dia pipeline deploy`) · Incremental (mtime check) | Current DIA CLI already supports incremental mtime |
| **Shared vs per-app data** | Each app gets its own copy of DLLs · Shared DLL pool · Symlinks (not viable on Win) | CEF alone is ~150 MB; per-app copies are expensive but simple |
| **OutDir in props vs vcxproj** | `Directory.Build.props` (PD-008 mandated) · Per-project override | PD-008 forbids per-project overrides |

## Known Tradeoffs

- **Per-app bin folders → bigger disk usage**: CEF, SFML, Python DLLs duplicated per app that needs them. Acceptable for a dev machine; problematic for CI artifact storage.
- **Per-app bin folders → simpler runtime**: no need for PATH tricks or working-directory games; just `cd bin/CluicheEditor/Debug/x64 && CluicheEditor.exe`.
- **Centralising deploy in DIA CLI → one source of truth**: removes xcopy drift, but means you must run `dia pipeline deploy` after every build to get a runnable state (unless it is wired as a post-build event).
- **`Directory.Build.props` change → all 20 projects rebuild**: changing `OutDir` in `Directory.Build.props` invalidates all incremental build state. One-time cost.
- **Lib pollution**: `.lib` and `.pdb` for intermediate libs (DiaCore, DiaMaths, etc.) do not need to be in the runnable app folder. Separating them reduces clutter.

## Known Pitfalls (C++ / game engine context)

- **DLL load path**: Windows resolves DLLs from the exe's directory first. If CEF DLLs are not in the same folder as `CluicheEditor.exe`, the app will fail at launch with a cryptic error.
- **CEF subprocess**: `CluicheEditor.exe` spawns a renderer subprocess that must also be in the same directory; CEF DLLs must be co-located.
- **Post-build event race**: if xcopy post-build events survive in any `.vcxproj`, they will fight with DIA CLI deploy, causing partial copies or overwrites in wrong order.
- **PDB lookup**: Visual Studio locates `.pdb` files relative to the `.exe`. If binaries move to a per-app folder, PDB paths in the binary may break unless PDB is also deployed there.
- **Incremental build**: changing `OutDir` mid-project causes `.lib` link targets to not be found unless all dependents are rebuilt.
- **`.gitignore` coverage**: `Cluiche/bin/` is presumably gitignored; per-app subdirs must remain gitignored.

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| `Directory.Build.props` | Currently sets `OutDir = Cluiche/bin/$(Configuration)/$(Platform)/` — this is the single change point for binary layout |
| `pipeline.toml` | Already describes per-target deploy rules; natural place to add per-target `out_dir` override |
| `path_resolver.py` | Hardcodes `resolve_out_dir` to `Cluiche/bin/<config>/<platform>` — must update if OutDir changes |
| `package_stage.py` | Already handles `$(OutDir)` substitution and mtime-based incremental copy |
| `DiaCLI / dia pipeline deploy` | Already called from `.vcxproj` post-build events for editor; can be generalised |
| `Cluiche/out/<AppName>/` | Pattern to mimic: per-app rooted output space |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-006 Visual Studio project files are source of truth | OutDir must be set via MSBuild properties, not scripts |
| PD-008 `Directory.Build.props` owns OutDir and IntDir | Any OutDir change MUST go through `Directory.Build.props`; no per-project overrides allowed |
| PD-009 All generated non-binary output lives under `Cluiche/out/<AppName>/` | Reinforces the per-app model; `bin/` should mirror this pattern |

## Open Questions for Ideation

- Should `.lib` files for intermediate modules (DiaCore, DiaMaths, etc.) move out of `bin/` entirely (e.g. into IntDir), or stay in a shared `bin/lib/` pool?
- Should `OutDir` be per-app (requiring each app `.vcxproj` to know its app name, violating PD-008) or should `Directory.Build.props` use an MSBuild condition or a property that each exe project sets?
- How does DIA CLI discover which `OutDir` to deploy into — does it read `Directory.Build.props`, receive it as a CLI argument, or compute it from a convention?
- Should the deploy step be wired as a `.vcxproj` post-build event (runs on every build) or remain a manual `dia pipeline deploy` (developer-triggered)?
- What is the right granularity for "app" — `cluicheeditor`, `cluichetest`, `googletest` as today, or should shared executables like `DiaUICEF_TestSubprocess.exe` be under the app that owns them?
- Is it acceptable for all `.lib` / `.pdb` for static libs to remain in a shared `bin/lib/Debug/x64/` so dependent projects can still link them without per-project path changes?

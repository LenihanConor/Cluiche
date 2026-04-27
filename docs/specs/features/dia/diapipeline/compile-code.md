# Feature Spec: compile-code

## Parent System
@docs/specs/systems/dia/diapipeline.md

## Status
`Done`

## Summary

Implement `dia pipeline --stage compile-code` — runs `msbuild` on the per-target `.vcxproj` (or the full solution) for the active configuration and platform. Supports `Debug|x64`, `Release|x64`, and `Both` (sequential). Highest-value stage; unblocks `dia test env-integration`.

## Problem

Building any Cluiche target currently requires opening Visual Studio or knowing the exact `msbuild` invocation. `compile-code` makes the build reproducible from the CLI and inside the Docker container, with a consistent exit code surface.

## Goals

- Invoke `msbuild` on the `.vcxproj` path specified by `pipeline.toml` for the active target
- Support `Debug|x64`, `Release|x64`, and `Both` (Debug then Release, sequentially)
- Stream `msbuild` output to stdout in real time (not buffered)
- Exit 1 on any `msbuild` non-zero exit; propagate msbuild error output
- Support building the full solution (`Cluiche/Cluiche.sln`) as an alternative target when `project` is `"solution"` in `pipeline.toml`
- Locate `msbuild.exe` via `vswhere.exe` if not on PATH

## Non-Goals

- Parallel `msbuild` invocations for `--config Both` (SD-PIPE-007: sequential to avoid intermediate file conflicts)
- Running tests after compile — owned by DiaTest
- Modifying `.vcxproj` files (PD-006)
- Installing or verifying VS 2022 workloads — owned by DiaEnv
- Downloading or extracting the CEF binary distribution — owned by DiaEnv (`dia env deps-restore`)

## Build Dependencies (Pre-MSBuild)

Targets may declare build dependencies in `pipeline.toml` under `[targets.<name>.build_deps]`. These are built **before** MSBuild runs, as sub-steps of compile-code. Each sub-step is skippable — it checks whether its output already exists and only runs when needed.

### Protobuf Code Generation (`protobuf = true`)

Runs `protoc` on `debug_protocol.proto` to generate C++ headers (`debug_protocol.pb.h`, `debug_protocol.pb.cc`). This was previously a standalone `proto-compile` stage but is now a build_dep since it is a pre-requisite for compilation, not a separate pipeline concern.

**Trigger:** `build_deps.protobuf = true` in `pipeline.toml` AND sentinel file `.diaenv/proto/debug_protocol.sentinel` does not exist (or `--force` is set).

**Skip:** If sentinel exists and `--force` is not set, logged as "protobuf up to date (sentinel present)".

**Steps:**
1. Locate `protoc.exe` at `External/protobuf/protobuf-25.6/install-x64/bin/protoc.exe` (or `pipeline.toml [proto] protoc_path` override).
2. Run `protoc --proto_path=<proto_dir> --cpp_out=<output_dir> debug_protocol.proto`.
3. On success, write sentinel file; on failure, exit 1 immediately.

**Configuration:** In `pipeline.toml`:
```toml
[targets.googletest.build_deps]
protobuf = true
```

### CEF Wrapper (`cef_wrapper = true`)

The `cluicheeditor` target depends on `libcef_dll_wrapper.lib`, a ~150MB static library built from the CEF binary distribution's CMake project. This lib is gitignored and must be built locally.

**Trigger:** `build_deps.cef_wrapper = true` in `pipeline.toml` AND `External/CEF/lib/<config>/libcef_dll_wrapper.lib` does not exist.

**Skip:** If the wrapper lib already exists, the sub-step is skipped entirely (logged as "already exists — skipping").

**Steps:**
1. Check if `External/CEF/CMakeLists.txt` exists (from CEF binary distribution, restored by `dia env deps-restore`). If missing → exit 1 with message pointing to `dia env deps-restore`.
2. Find CMake via vswhere (`-requires Microsoft.VisualStudio.Component.VC.CMake.Project`). If missing → exit 1 with install instructions.
3. Run `cmake -G "Visual Studio 17 2022" -A x64 -DCEF_RUNTIME_LIBRARY_FLAG=/MD <cef_dir>` in `External/CEF/build/`.
4. Run `cmake --build <build_dir> --config <Debug|Release> --target libcef_dll_wrapper`.
5. Copy built lib from `build/libcef_dll_wrapper/<config>/libcef_dll_wrapper.lib` to `External/CEF/lib/<config>/`.

**Configuration:** In `pipeline.toml`:
```toml
[targets.cluicheeditor.build_deps]
cef_wrapper = true
```

**Error handling:** Any CMake failure (configure or build) returns exit 1 immediately; MSBuild is not attempted.

## MSBuild Invocation

```python
solution_dir = str(repo_root / "Cluiche").replace("/", "\\") + "\\"

msbuild_args = [
    msbuild_exe,
    str(project_path),
    f"/p:Configuration={build_config}",
    f"/p:Platform={platform}",
    f"/p:SolutionDir={solution_dir}",
    "/m",           # parallel build (within msbuild, not across invocations)
    "/v:minimal",   # minimal verbosity; errors and warnings only
    "/nologo",
]
```

`/p:SolutionDir` is passed explicitly because when MSBuild runs on a standalone `.vcxproj` (rather than via `.sln`), `$(SolutionDir)` defaults to the project directory. PostBuildEvent commands that use `$(SolutionDir)` (e.g., `cd "$(SolutionDir)..\Dia\DiaCLI"`) would resolve incorrectly without this override.

For `--config Both`, the stage runs twice: first `Debug`, then `Release`. If Debug fails, Release is not attempted.

## MSBuild Discovery

`msbuild.exe` is located in one of these ways, in order:

1. `vswhere.exe -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`
2. `pipeline.toml [global] msbuild_path` override (optional field)
3. Exit 1 with "msbuild not found — run `dia env verify` to check your VS 2022 installation"

`vswhere.exe` is at `%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe` on a standard VS 2022 install.

## Implementation

### Files introduced

```
Dia/DiaCLI/
└── dia_cli/
    └── commands/
        └── pipeline/
            └── stages/
                └── compile_code_stage.py   # Stage handler
```

### `compile_code_stage.py` responsibilities

```python
def run(config: PipelineConfig, target: str, build_config: str, force: bool, repo_root: Path) -> int:
    """Returns 0 on success, 1 on failure."""
```

1. Check `build_deps` for the target — if `protobuf` is true, run `_build_protobuf()` (protoc code generation); exit 1 on failure
2. Check `build_deps` — if `cef_wrapper` is true, call `_build_cef_wrapper()` (see Build Dependencies section above); exit 1 on failure
3. Resolve `msbuild.exe` path (vswhere → toml override → exit 1)
3. Resolve project path from `config.targets[target].project` relative to `repo_root`
4. Run `msbuild` with `subprocess.run(... stdout=sys.stdout, stderr=sys.stderr)` for real-time output
5. Return msbuild exit code directly

For `--config Both`, `pipeline_runner.py` calls `compile_code_stage.run` twice.

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| `pipeline-config` feature | Hard | `TargetConfig.project`, `TargetConfig.build_deps`, and `msbuild_path` override |
| `msbuild.exe` | System tool | Installed by `dia env setup --toolchain`; discovered via vswhere |
| `vswhere.exe` | System tool | Ships with VS 2022 installer |
| `protoc.exe` | System tool | From `External/protobuf/` via `dia env deps-restore`. Only needed when `build_deps.protobuf = true` |
| `cmake.exe` | System tool | Bundled with VS 2022 "C++ CMake tools" component; discovered via vswhere. Only needed when `build_deps.cef_wrapper = true` |
| CEF binary distribution | External dep | `External/CEF/CMakeLists.txt` from `dia env deps-restore`; only needed when building CEF wrapper |
| `subprocess` (stdlib) | Python | |
| `shutil` (stdlib) | Python | Used for copying built wrapper lib |

## Acceptance Criteria

1. `dia pipeline --stage compile-code --target googletest --config Debug` builds `GoogleTests.vcxproj` in `Debug|x64` and exits 0
2. `dia pipeline --stage compile-code --target googletest --config Release` builds `GoogleTests.vcxproj` in `Release|x64` and exits 0
3. `dia pipeline --stage compile-code --config Both` builds Debug then Release sequentially; if Debug fails, Release is not attempted
4. `msbuild` output streams to stdout in real time (not held until completion)
5. A compilation error causes the stage to exit 1; the msbuild error lines are visible in the output
6. If `msbuild.exe` cannot be found, exits 1 with "msbuild not found — run `dia env verify`"
7. `dia pipeline --stage compile-code --target cluichetest` and `--target cluicheeditor` each build their respective projects
8. When `cef_wrapper = true` and the wrapper lib exists, compile-code logs "already exists — skipping" and proceeds to MSBuild
9. When `cef_wrapper = true` and the wrapper lib is missing but `CMakeLists.txt` is present, compile-code builds the wrapper via CMake before MSBuild
10. When `cef_wrapper = true` and `CMakeLists.txt` is missing, compile-code exits 1 with a message pointing to `dia env deps-restore`
11. Targets without `build_deps` (e.g., googletest) are unaffected by the build_deps logic

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaPipeline | @docs/specs/systems/dia/diapipeline.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | Use StringCRC for all identifiers | Compliant — Python tooling only |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | Compliant — tooling |
| PD-003 | Platform | Component-based entities | Compliant — tooling |
| PD-004 | Platform | No STL containers in public APIs | Compliant — Python only |
| PD-005 | Platform | x64 Windows only | Compliant — `/p:Platform=x64` hardcoded |
| PD-006 | Platform | VS project files are source of truth | Compliant — `msbuild` reads existing `.vcxproj` files; nothing modifies them |
| PD-007 | Platform | C++20 required | Compliant — C++20 is set in `Directory.Build.props`; DiaPipeline does not override it |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | Compliant — `msbuild` inherits these from `Directory.Build.props`; `SolutionDir` is set to match what `.sln` would provide, but OutDir/IntDir are not overridden |
| AD-001 | Dia App | Module system with YAML frontmatter | Compliant — Python tooling only |
| SD-CLI-001 | DiaCLI | MDK CLI architecture | Compliant |
| SD-CLI-002 | DiaCLI | Python-based implementation | Compliant |
| SD-CLI-008 | DiaCLI | Exit codes follow Unix conventions | Compliant — propagates msbuild exit code (0 success, 1 failure) |
| SD-PIPE-001 | DiaPipeline | `pipeline.toml` is single source of truth | Compliant — project path read from `pipeline.toml` |
| SD-PIPE-002 | DiaPipeline | Stage ordering fixed | Compliant — compile-code is stage 1; protobuf generation is now a build_dep sub-step within compile-code |
| SD-ENV-010 | DiaEnv | Python 3.11 | Compliant |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | `/m` flag | Does `/m` (parallel MSBuild) conflict with `--config Both` running two sequential builds? | No conflict. `/m` parallelises the build graph within a single `msbuild` invocation. The two sequential invocations (Debug then Release) do not overlap. |
| 2 | Solution vs project | When should `project` be `"solution"` vs a specific `.vcxproj`? | Use a specific `.vcxproj` for targeted builds (faster). `"solution"` is a convenience for building everything. The `googletest`, `cluichetest`, and `cluicheeditor` targets all use specific project paths. |
| 3 | msbuild verbosity | `/v:minimal` — will this show enough output to diagnose build failures? | Yes — minimal verbosity shows errors and warnings. For deeper diagnosis, the developer can re-run `msbuild` directly with `/v:normal` or `/v:detailed`. The pipeline's job is to surface the failure; detailed diagnosis is out of scope. |
| 4 | vswhere fallback | What if vswhere returns multiple VS installations? | `vswhere -latest` returns only the newest installation. This is the correct behaviour for a project requiring VS 2022. |
| 5 | SolutionDir override | Why is `/p:SolutionDir` passed explicitly to MSBuild? | When MSBuild runs on a standalone `.vcxproj` (not via `.sln`), `$(SolutionDir)` defaults to the project directory. PostBuildEvent commands that reference `$(SolutionDir)` (e.g., `cd "$(SolutionDir)..\Dia\DiaCLI"` for the deploy step) would resolve to the wrong path. The override ensures `$(SolutionDir)` points to `Cluiche\` — matching the `.sln` layout. |

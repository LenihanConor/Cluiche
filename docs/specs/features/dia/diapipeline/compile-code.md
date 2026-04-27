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

## MSBuild Invocation

```python
msbuild_args = [
    msbuild_exe,
    str(project_path),
    f"/p:Configuration={build_config}",
    f"/p:Platform={platform}",
    "/m",           # parallel build (within msbuild, not across invocations)
    "/v:minimal",   # minimal verbosity; errors and warnings only
    "/nologo",
]
```

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

1. Resolve `msbuild.exe` path (vswhere → toml override → exit 1)
2. Resolve project path from `config.targets[target].project` relative to `repo_root`
3. Run `msbuild` with `subprocess.run(... stdout=sys.stdout, stderr=sys.stderr)` for real-time output
4. Return msbuild exit code directly

For `--config Both`, `pipeline_runner.py` calls `compile_code_stage.run` twice.

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| `pipeline-config` feature | Hard | `TargetConfig.project` and `msbuild_path` override |
| `msbuild.exe` | System tool | Installed by `dia env setup --toolchain`; discovered via vswhere |
| `vswhere.exe` | System tool | Ships with VS 2022 installer |
| `subprocess` (stdlib) | Python | |

## Acceptance Criteria

1. `dia pipeline --stage compile-code --target googletest --config Debug` builds `GoogleTests.vcxproj` in `Debug|x64` and exits 0
2. `dia pipeline --stage compile-code --target googletest --config Release` builds `GoogleTests.vcxproj` in `Release|x64` and exits 0
3. `dia pipeline --stage compile-code --config Both` builds Debug then Release sequentially; if Debug fails, Release is not attempted
4. `msbuild` output streams to stdout in real time (not held until completion)
5. A compilation error causes the stage to exit 1; the msbuild error lines are visible in the output
6. If `msbuild.exe` cannot be found, exits 1 with "msbuild not found — run `dia env verify`"
7. `dia pipeline --stage compile-code --target cluichetest` and `--target cluicheeditor` each build their respective projects

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
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | Compliant — `msbuild` inherits these from `Directory.Build.props`; no override in the invocation |
| AD-001 | Dia App | Module system with YAML frontmatter | Compliant — Python tooling only |
| SD-CLI-001 | DiaCLI | MDK CLI architecture | Compliant |
| SD-CLI-002 | DiaCLI | Python-based implementation | Compliant |
| SD-CLI-008 | DiaCLI | Exit codes follow Unix conventions | Compliant — propagates msbuild exit code (0 success, 1 failure) |
| SD-PIPE-001 | DiaPipeline | `pipeline.toml` is single source of truth | Compliant — project path read from `pipeline.toml` |
| SD-PIPE-002 | DiaPipeline | Stage ordering fixed | Compliant — compile-code is stage 2; only runs after proto-compile |
| SD-ENV-010 | DiaEnv | Python 3.11 | Compliant |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | `/m` flag | Does `/m` (parallel MSBuild) conflict with `--config Both` running two sequential builds? | No conflict. `/m` parallelises the build graph within a single `msbuild` invocation. The two sequential invocations (Debug then Release) do not overlap. |
| 2 | Solution vs project | When should `project` be `"solution"` vs a specific `.vcxproj`? | Use a specific `.vcxproj` for targeted builds (faster). `"solution"` is a convenience for building everything. The `googletest`, `cluichetest`, and `cluicheeditor` targets all use specific project paths. |
| 3 | msbuild verbosity | `/v:minimal` — will this show enough output to diagnose build failures? | Yes — minimal verbosity shows errors and warnings. For deeper diagnosis, the developer can re-run `msbuild` directly with `/v:normal` or `/v:detailed`. The pipeline's job is to surface the failure; detailed diagnosis is out of scope. |
| 4 | vswhere fallback | What if vswhere returns multiple VS installations? | `vswhere -latest` returns only the newest installation. This is the correct behaviour for a project requiring VS 2022. |

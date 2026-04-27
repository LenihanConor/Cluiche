# Feature Spec: pipeline-config

## Parent System
@docs/specs/systems/dia/diapipeline.md

## Status
`Done`

## Summary

Implement `pipeline.toml` parsing, CLI skeleton for `dia pipeline`, and the runtime resolution of `$(OutDir)` / `$(Configuration)` variables. This is the foundational feature — all other DiaPipeline features read the config and use the resolved paths.

## Problem

Every other DiaPipeline feature needs to know: which target is active, which config (Debug/Release/Both), which stages to run, and where the output directory resolves to. Without a shared config layer there is no way to run stages consistently across invocations.

## Goals

- Parse `pipeline.toml` from the repo root into a typed Python dataclass
- Resolve `$(OutDir)` to `Cluiche/bin/<Configuration>/<Platform>/` and `$(Configuration)` to `Debug` or `Release` at runtime
- Implement the top-level `dia pipeline` Click group with `--config`, `--target`, `--stage`, `--force`, `--docker` flags
- Validate CLI arguments against the parsed config (unknown target → exit 2, unknown stage → exit 2)
- Exit 2 with a clear message if `pipeline.toml` is missing
- Emit `OnPipelineCompleted` event after all stages finish

## Non-Goals

- Running any actual stage (proto-compile, compile-code, etc.) — those are their own features
- Writing or modifying `pipeline.toml` — config is read-only from Python's perspective
- Cross-platform path resolution — Windows x64 only (PD-005)

## CLI Interface

```bash
# Run all stages for the default target (from pipeline.toml [global])
dia pipeline

# Select configuration
dia pipeline --config Debug
dia pipeline --config Release
dia pipeline --config Both

# Select target
dia pipeline --target googletest
dia pipeline --target cluichetest
dia pipeline --target cluicheeditor

# Select specific stages
dia pipeline --stage proto-compile
dia pipeline --stage compile-code,package

# Force re-run even if stage sentinel present
dia pipeline --force

# Run inside Docker
dia pipeline --docker
```

## `pipeline.toml` Schema

See system spec for full schema. The config dataclass mirrors the TOML structure:

```python
@dataclass
class GlobalConfig:
    default_config: str       # "Debug" | "Release" | "Both"
    default_platform: str     # "x64"
    default_target: str       # "googletest" | "cluichetest" | "cluicheeditor"

@dataclass
class ProtoConfig:
    proto_dir: str
    output_dir: str
    language: str             # "cpp" only

@dataclass
class PackageFile:
    src: str
    dest: str

@dataclass
class PackageConfig:
    files: list[PackageFile]

@dataclass
class TargetConfig:
    project: str
    stages: list[str]
    package: PackageConfig

@dataclass
class PipelineConfig:
    global_cfg: GlobalConfig
    proto: ProtoConfig
    targets: dict[str, TargetConfig]
```

## `$(OutDir)` Resolution

`$(OutDir)` resolves to `<repo_root>/Cluiche/bin/<Configuration>/<Platform>/` per `Directory.Build.props` convention. Resolution logic:

```python
def resolve_out_dir(repo_root: Path, config: str, platform: str) -> Path:
    return repo_root / "Cluiche" / "bin" / config / platform

def resolve_variables(path: str, config: str, platform: str, repo_root: Path) -> str:
    out_dir = str(resolve_out_dir(repo_root, config, platform)) + "/"
    return (path
        .replace("$(OutDir)", out_dir)
        .replace("$(Configuration)", config)
        .replace("$(Platform)", platform))
```

## Implementation

### Files introduced

```
Dia/DiaCLI/
└── dia_cli/
    ├── cli/
    │   └── pipeline_cmd.py          # Click group: dia pipeline (top-level flags)
    └── commands/
        └── pipeline/
            ├── __init__.py
            ├── pipeline_config.py   # PipelineConfig dataclass + load_pipeline_config()
            ├── pipeline_runner.py   # Stage orchestration loop + event emission
            └── path_resolver.py     # $(OutDir) / $(Configuration) resolution
```

### `pipeline_config.py` responsibilities

- `load_pipeline_config(repo_root: Path) -> PipelineConfig` — reads `pipeline.toml`, raises `PipelineConfigError` (exit 2) if missing or malformed
- Validates that all stage names in `targets[*].stages` are in `{"proto-compile", "compile-code", "asset-build", "package"}`

### `pipeline_runner.py` responsibilities

- `run_pipeline(config, target, stages, build_config, force, docker)` — iterates the active stage list, delegates to stage handlers (stubs for now), emits events
- On stage failure: emit `OnStageFailed`, log error, return exit 1
- After all stages: emit `OnPipelineCompleted` with pass/fail counts and total duration

### `pipeline_cmd.py` responsibilities

- `@click.group` with `--config`, `--target`, `--stage`, `--force`, `--docker` options
- Loads config, validates args, calls `pipeline_runner.run_pipeline`
- `--stage` accepts comma-separated list; unknown stage names → exit 2
- `--config Both` → runs Debug then Release sequentially

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| `toml` | Python | Already in DiaCLI `pyproject.toml` |
| `click` | Python | Already in DiaCLI |
| `loguru` | Python | Already in DiaCLI |
| `pipeline.toml` | Config file | Must exist at repo root; committed to version control |

## Acceptance Criteria

1. `dia pipeline --help` shows all flags with descriptions
2. `dia pipeline --target googletest` loads config and prints "No stages run — stub" without error
3. `dia pipeline --target unknown` exits 2 with "unknown target: unknown"
4. `dia pipeline --stage unknown-stage` exits 2 with "unknown stage: unknown-stage"
5. Missing `pipeline.toml` exits 2 with "pipeline.toml not found at <path>"
6. `$(OutDir)` in a package file rule resolves to `Cluiche/bin/Debug/x64/` for `--config Debug`
7. `$(OutDir)` resolves to `Cluiche/bin/Release/x64/` for `--config Release`
8. `--config Both` triggers two pipeline runs (Debug then Release) sequentially
9. `OnPipelineCompleted` is logged with pass/fail counts and duration

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaPipeline | @docs/specs/systems/dia/diapipeline.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | Use StringCRC for all identifiers | Compliant — Python tooling only; no C++ identifiers |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | Compliant — tooling; no runtime components |
| PD-003 | Platform | Component-based entities | Compliant — tooling |
| PD-004 | Platform | No STL containers in public APIs | Compliant — Python only |
| PD-005 | Platform | x64 Windows only | Compliant — `$(Platform)` hardcoded to `x64`; no cross-platform path logic |
| PD-006 | Platform | VS project files are source of truth | Compliant — config reads `.vcxproj` paths from `pipeline.toml`; does not modify them |
| PD-007 | Platform | C++20 required | Compliant — tooling; no compiler configuration |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | Compliant — `$(OutDir)` resolution mirrors `Directory.Build.props` convention; does not override it |
| AD-001 | Dia App | Module system with YAML frontmatter | Compliant — Python tooling only |
| SD-CLI-001 | DiaCLI | MDK CLI architecture | Compliant — two-file plugin pattern (`cli/pipeline_cmd.py` + `commands/pipeline/`) |
| SD-CLI-002 | DiaCLI | Python-based implementation | Compliant |
| SD-CLI-006 | DiaCLI | Click framework | Compliant — Click group and decorators |
| SD-CLI-008 | DiaCLI | Exit codes follow Unix conventions | Compliant — 0 all-pass, 1 stage failure, 2 invalid args/missing config |
| SD-PIPE-001 | DiaPipeline | `pipeline.toml` is single source of truth | Compliant — this feature implements `pipeline.toml` parsing |
| SD-PIPE-002 | DiaPipeline | Stage ordering fixed | Compliant — `pipeline_runner.py` enforces proto-compile → compile-code → asset-build → package |
| SD-PIPE-003 | DiaPipeline | `$(OutDir)` / `$(Configuration)` resolved at runtime | Compliant — `path_resolver.py` implements this |
| SD-ENV-010 | DiaEnv | Python 3.11 | Compliant |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | `pipeline.toml` | Should `pipeline.toml` be committed as part of this feature or written manually? | Committed — the file is the per-repo record of how to build each target. It is written once and version-controlled. A valid `pipeline.toml` with all three targets must be committed as part of this feature. |
| 2 | `--config Both` | Does `--config Both` run Debug then Release in the same process, or spawn two subprocess calls? | Same process — `pipeline_runner.run_pipeline` is called twice sequentially with `config="Debug"` then `config="Release"`. No subprocess spawning needed at this layer. |
| 3 | Variable resolution | What happens if `pipeline.toml` contains `$(OutDir)` in a src path (not just dest)? | Supported — `resolve_variables` is applied to both `src` and `dest` fields. |
| 4 | Unknown stage in `--stage` | Should unknown stage names be caught by CLI validation or by the runner? | CLI layer — validate against the fixed stage list before calling the runner, so the exit 2 path is clear and doesn't depend on runner state. |

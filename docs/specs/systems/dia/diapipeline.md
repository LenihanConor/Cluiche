# System Spec: DiaPipeline

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaPipeline owns the `dia pipeline` command surface — a 3-stage build pipeline that compiles the Cluiche solution (with pre-requisite build dependencies), processes assets, and deploys runtime dependencies for each executable. It runs on either the host machine or inside the Docker container (via `--docker`), and is driven by a two-level config file (`pipeline.toml`) that defines global defaults and per-target overrides.

The core problem DiaPipeline solves: building and staging Cluiche for any executable (GoogleTests, CluicheTest, CluicheEditor) requires knowing exactly which stages to run, which configuration to use, and which runtime files to copy alongside the binary. Currently this is scattered across `xcopy` commands hardcoded in `.vcxproj` post-build events. DiaPipeline centralises this into a single, config-driven command.

DiaPipeline lives entirely in `Dia/DiaCLI/` as a set of `dia pipeline` sub-commands and a repo-root `pipeline.toml`.

## Responsibilities

- **Pipeline Config** — Own `pipeline.toml` at the repo root: global defaults (configuration, platform, target) and per-target stage overrides
- **Compile-Code Stage** — Build pre-requisites (protobuf codegen, CEF wrapper) via per-target `build_deps`, then run `msbuild` on the solution or named target; support `Debug|x64` and `Release|x64`
- **Build-Assets Stage** — Run the asset pipeline for a named target (stub for now; no-op until asset system is specced)
- **Deploy Stage** — Copy runtime dependencies (DLLs, data files, manifests) alongside the built executable; reads per-target copy rules from `pipeline.toml`; eventual replacement for `xcopy` post-build events in `.vcxproj` files
- **Execution Context** — Run on host machine or inside Docker container (`--docker`)

## Non-Responsibilities

- **Test execution** — owned by DiaTest (`dia test`)
- **Environment provisioning** — owned by DiaEnv (`dia env`)
- **Asset authoring or conversion tools** — asset-build stage calls the asset pipeline but does not own it
- **Deployment to remote targets** — deploy stage stages files locally only
- **CI/CD pipeline configuration** — local-only for now
- **Removing post-build xcopy from `.vcxproj` files** — this is a future cleanup task; DiaPipeline and `.vcxproj` post-build events will coexist until explicitly migrated

## Public Interfaces

### CLI Commands

```bash
# Run all pipeline stages for the default target
dia pipeline

# Run inside Docker container
dia pipeline --docker

# Select build configuration
dia pipeline --config Debug
dia pipeline --config Release
dia pipeline --config Both          # runs Debug then Release

# Select target (executable)
dia pipeline --target googletest
dia pipeline --target cluichetest
dia pipeline --target cluicheeditor

# Run specific stages only
dia pipeline --stage compile-code
dia pipeline --stage compile-code,deploy
dia pipeline --stage compile-code --config Release --target googletest

# Force re-run of a stage even if already complete
dia pipeline --stage compile-code --force
```

### Stage Definitions

| # | Stage | What it does | Default enabled | Skippable |
|---|-------|-------------|----------------|-----------|
| 1 | `compile-code` | Run per-target `build_deps` (protobuf codegen, CEF wrapper) then `msbuild` | Yes | No |
| 2 | `build-assets` | Run asset pipeline for target (no-op stub until specced) | Yes | Yes |
| 3 | `deploy` | Copy runtime DLLs/data/manifests/UI to `OutDir` per target | Yes | Yes — skips if already staged |

### `pipeline.toml` Schema

```toml
# pipeline.toml (repo root)

[global]
default_config = "Debug"       # Debug | Release | Both
default_platform = "x64"
default_target = "googletest"  # which target runs when no --target specified

[proto]
proto_dir = "Dia/DiaDebugProtocol/proto"
output_dir = "Dia/DiaDebugProtocol/proto/generated"
language = "cpp"               # cpp only for now

[targets.googletest]
project = "Cluiche/Tests/GoogleTests/GoogleTests.vcxproj"
stages = ["compile-code", "build-assets", "deploy"]

[targets.googletest.build_deps]
protobuf = true

[targets.googletest.deploy]
files = [
  { src = "External/Python311/python311.dll", dest = "$(OutDir)" },
  { src = "Cluiche/CluicheTest/Data/Manifests/*.diaapp", dest = "$(OutDir)Data/Manifests/" }
]

[targets.cluichetest]
project = "Cluiche/CluicheTest/CluicheTest.vcxproj"
stages = ["compile-code", "build-assets", "deploy"]

[targets.cluichetest.build_deps]
protobuf = true

[targets.cluichetest.deploy]
files = [
  { src = "Cluiche/pathStoreConfig.json", dest = "$(OutDir)" },
  { src = "External/SFML/Current-x64/bin/*.dll", dest = "$(OutDir)" },
  { src = "Cluiche/CluicheTest/Data/Manifests/*.diaapp", dest = "$(OutDir)Data/Manifests/" }
]

[targets.cluicheeditor]
project = "Cluiche/CluicheEditor/CluicheEditor.vcxproj"
stages = ["compile-code", "build-assets", "deploy"]

[targets.cluicheeditor.build_deps]
protobuf = true
cef_wrapper = true

[targets.cluicheeditor.deploy]
files = [
  { src = "External/CEF/Resources/*", dest = "$(OutDir)" },
  { src = "External/CEF/Resources/locales/**", dest = "$(OutDir)locales/" },
  { src = "External/CEF/bin/$(Configuration)/*", dest = "$(OutDir)" }
]
```

**`$(OutDir)` and `$(Configuration)` are resolved at runtime** from `Directory.Build.props` conventions and the active `--config` flag.

### Events Emitted

- **OnStageStarted(stageName, target, config)** — before each stage begins
- **OnStageCompleted(stageName, target, config, durationMs)** — after stage succeeds
- **OnStageFailed(stageName, target, config, error)** — on stage failure
- **OnPipelineCompleted(passCount, failCount, totalDurationMs)** — after all stages finish

### Exit Codes

| Code | Meaning |
|------|---------|
| `0` | All stages passed |
| `1` | One or more stages failed |
| `2` | Invalid arguments or missing `pipeline.toml` |
| `3` | Docker container not available (when `--docker` used) |

## Features

| Feature | Description | Key Capabilities | Spec | Effort | Status |
|---------|-------------|------------------|------|--------|--------|
| pipeline-config | `pipeline.toml` schema + CLI parsing | Two-level config, target definitions, stage enable/disable, `$(OutDir)` resolution, `build_deps` per-target | [pipeline-config.md](../../features/dia/diapipeline/pipeline-config.md) | 2 days | Done |
| compile-code | `dia pipeline --stage compile-code` | `build_deps` pre-steps (protobuf codegen, CEF wrapper), `msbuild` invocation, Debug/Release/Both, per-target project | [compile-code.md](../../features/dia/diapipeline/compile-code.md) | 3 days | Done |
| build-assets | `dia pipeline --stage build-assets` | No-op stub; logs "build-assets: skipped (not yet implemented)" | [asset-build.md](../../features/dia/diapipeline/asset-build.md) | 1 day | Done |
| deploy | `dia pipeline --stage deploy` | File copy rules from `pipeline.toml`, UI builds, glob support, `$(OutDir)`/`$(Configuration)` resolution, skip-if-staged guard | [package.md](../../features/dia/diapipeline/package.md) | 3 days | Done |
| docker-execution | `dia pipeline --docker` | Run all stages inside Docker container via `docker run` with volume-mounted repo | [docker-execution.md](../../features/dia/diapipeline/docker-execution.md) | 2 days | Done |

**Total Effort Estimate:** ~11 days

**Recommended Implementation Order:**
1. `pipeline-config` (2d) — foundational; all other features read it
2. `compile-code` (3d) — highest value; includes protobuf and CEF build_deps
3. `deploy` (3d) — required for executables to run after build
4. `build-assets` (1d) — stub; low effort, completes the stage surface
5. `docker-execution` (2d) — wraps all stages in container; required for `dia test env-integration`

## Platform Primitives Used

**Python Packages:**
- **click** — CLI argument parsing (existing DiaCLI dependency)
- **toml** — `pipeline.toml` parsing (existing DiaCLI dependency)
- **loguru** — internal/debug logging (existing DiaCLI dependency)
- **glob** / **shutil** (stdlib) — file copy rules in deploy stage
- **OutputContext** (`dia_cli/utils/dia_output.py`) — structured terminal output and NDJSON event log; all `OnStageStarted`/`OnStageCompleted`/`OnStageFailed`/`OnRunCompleted` events emitted via this layer (see `cli-output` feature)

**System Tools:**
- **msbuild** — C++ compilation (`compile-code` stage)
- **protoc** — protobuf compiler (`compile-code` build_dep); lives in `External/protobuf/`
- **cmake** — CEF wrapper build (`compile-code` build_dep); bundled with VS 2022
- **docker** — container execution (`--docker` flag)

## Dependencies on Other Systems

| System | Type | Notes |
|--------|------|-------|
| DiaEnv | Hard | `docker-build-env` provides the container; `deps-manifest` ensures `External/` is populated; `msbuild-restore` provides the MSBuild safety net |
| DiaTest | Consumer | `dia test env-integration` calls `dia pipeline` as stage 2 of its loop |
| DiaCLI | Hard | All commands are DiaCLI plugins |
| DiaCLI `cli-output` | Hard | `OutputContext` is the sole terminal + NDJSON output layer for all pipeline events |

## Out of Scope

- Removing `xcopy` post-build events from `.vcxproj` files — coexistence is acceptable until a future cleanup sprint
- Asset authoring, conversion, or optimisation tools — owned by a future asset pipeline system
- Code signing, notarisation, or distribution packaging
- Non-Windows build targets (PD-005)

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-PIPE-001 | `pipeline.toml` is the single source of truth for pipeline configuration | Replaces scattered `xcopy` commands; enables any target to be built with one command; version-controlled | All features | Accepted | Yes |
| SD-PIPE-002 | Stage ordering is fixed: compile-code → build-assets → deploy | Build deps (protobuf, CEF) run as sub-steps within compile-code; assets follow compile; deploy is always last | All features | Accepted | Yes |
| SD-PIPE-003 | `$(OutDir)` and `$(Configuration)` resolved from `Directory.Build.props` conventions at runtime | Keeps path resolution consistent with MSBuild; no duplicate path logic | deploy, compile-code | Accepted | Yes |
| SD-PIPE-004 | `--docker` runs all stages inside the container via `docker run` with volume-mounted repo | Consistent with SD-TEST-001; clean environment guarantee | docker-execution | Accepted | Yes |
| SD-PIPE-005 | build-assets stage is a no-op stub until the asset pipeline system is specced | Reserves the stage surface; prevents future breaking change to stage ordering | build-assets | Accepted | Yes |
| SD-PIPE-006 | Deploy stage coexists with `.vcxproj` post-build xcopy events until explicit migration | Avoids blocking pipeline implementation on a separate cleanup task | deploy | Accepted | Yes |
| SD-PIPE-007 | Pre-requisites (protobuf, CEF wrapper) are `build_deps` sub-steps of compile-code, not separate stages | Simplifies the top-level API to 3 stages; build_deps are per-target config in pipeline.toml | compile-code | Accepted | Yes |

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-005 | Platform | x64 Windows only | All `msbuild` invocations target `x64`; Docker container is Windows x64; no cross-platform support |
| PD-006 | Platform | VS project files are source of truth | `compile-code` stage invokes `msbuild` on `.vcxproj` files; DiaPipeline does not replace or modify them |
| PD-007 | Platform | C++20 required | `msbuild` inherits `/std:c++20` from `Directory.Build.props`; DiaPipeline does not override language settings |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | `$(OutDir)` resolution reads `Directory.Build.props` conventions; DiaPipeline does not set these properties |
| PD-009 | Platform | All generated non-binary output under `Cluiche/out/<AppName>/` | Pipeline NDJSON event log written to `Cluiche/out/DiaCLI/logs/pipeline/last-run.ndjson` via `OutputContext`. Compliant. |
| AD-001 | Dia App | Module system with YAML frontmatter | Python tooling only; no new C++ module introduced |
| SD-CLI-001 | DiaCLI | MDK CLI architecture | All `dia pipeline` commands follow the two-file plugin pattern |
| SD-CLI-002 | DiaCLI | Python-based implementation | All pipeline commands are Python |
| SD-CLI-006 | DiaCLI | Click framework | All commands use Click |
| SD-CLI-008 | DiaCLI | Exit codes follow Unix conventions | Exit codes follow the table above |
| SD-ENV-008 | DiaEnv | Docker scope is headless build + test only | `--docker` mode is headless; no GPU/GUI stages |
| SD-ENV-010 | DiaEnv | Python 3.11 | All DiaCLI pipeline code runs on Python 3.11 |
| SD-TEST-001 | DiaTest | All tests run inside Docker | `dia test env-integration` calls `dia pipeline --docker`; container execution is the integration test path |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | proto-compile | `protoc` is in `External/protobuf/protobuf-25.6` — is the binary at a known path, or does it need to be on PATH? | Path resolved from `External/protobuf/protobuf-25.6/bin/protoc.exe`; no PATH requirement. `pipeline.toml` can override with a `protoc_path` field if needed. |
| 2 | compile-code | Should `--config Both` run Debug and Release sequentially or in parallel? | Sequentially — parallel MSBuild invocations on the same solution can conflict on intermediate files. |
| 3 | package | `$(OutDir)` in `pipeline.toml` — is this resolved to `Cluiche/bin/Debug/x64/` per `Directory.Build.props`? | Yes — `OutDir` is `$(MSBuildThisFileDirectory)Cluiche\bin\$(Configuration)\$(Platform)\` per the existing `Directory.Build.props`. DiaPipeline resolves this at runtime from the config + platform. |
| 4 | package | Should the package stage remove stale files from `OutDir` before copying, or only copy? | Copy-only for now; no clean step. A `--clean-package` flag can be added later. Matches current `xcopy /Y /D` behaviour (overwrite if newer). |
| 5 | docker-execution | When `--docker` is used, does `dia pipeline` need to rebuild the Docker image first, or assume it exists? | Assume it exists (built by `dia env docker image`); exit 3 with clear message if not found. Consistent with `dia test cli` behaviour. |
| 6 | pipeline.toml | Should `pipeline.toml` be committed with sensible defaults, or generated by `dia env setup`? | Committed — it is a project configuration file, not a machine-specific file. It is the per-repo record of how to build each target. |
| 7 | default target | What is the default target when `--target` is not specified? | `googletest` — it is the fastest build and the most commonly run target during development. Overridable in `pipeline.toml` `[global] default_target`. |

## Status

`Done` - All 5 features implemented. Pipeline consolidated to 3 stages (compile-code, build-assets, deploy) with protobuf and CEF wrapper as build_deps sub-steps of compile-code.

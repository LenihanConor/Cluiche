# Feature Spec: docker-build-env

## Parent System
@docs/specs/systems/dia/diaenv.md

## Status
`Done`

**Research:** @docs/research/docker_hyperv/summary.md

## Summary

Provide a Docker Windows Container that serves as the reproducible build environment infrastructure for the Cluiche platform. The container encapsulates MSVC Build Tools and the Windows SDK; the repo is volume-mounted from the host. Three `dia env docker` sub-commands manage the container lifecycle: `image` builds/pulls the image, `deps` restores `External/` dependencies inside the container using `deps.json`, and `paths` configures PATH inside the container. This infrastructure is the foundation that `dia pipeline` and `dia test` will consume — it does not itself run builds or tests.

## Problem

Without an isolated build environment, every developer must manually install and maintain matching versions of MSVC, the Windows SDK, and all `External/` dependencies. Version drift between machines causes hard-to-reproduce build failures. There is currently no mechanism to guarantee that two developers are compiling with the same toolchain.

## Goals

- Reproducible compiler and Windows SDK version across all developer machines
- Single command to provision the container image from scratch
- Restore `External/` dependencies inside the container from `deps.json`
- Configure PATH inside the container so MSBuild, Python, and Git are accessible
- Repo volume-mounted from host — no source files baked into the image
- Both `Debug|x64` and `Release|x64` configurations supported

## Non-Goals

- Running `msbuild` (owned by `dia pipeline`)
- Running `GoogleTests.exe` or any other test runner (owned by `dia test`)
- Asset pipeline execution (owned by `dia pipeline`)
- Protobuf compilation (owned by `dia pipeline`)
- GPU access, windowed game execution, or any interactive UI inside the container
- Hosted CI configuration (local-only; hosted CI is a future concern)
- Git clone inside the container (out of scope; `--clone` flag reserved for future use)

## CLI Interface

```bash
# Build or pull the Docker image (bakes MSVC + Windows SDK)
dia env docker image

# Restore External/ dependencies inside the container via deps.json
dia env docker deps

# Configure PATH inside the container (MSBuild, Python, Git)
dia env docker paths

# Run all three stages in order
dia env docker

# Future (reserved, not implemented by this spec)
dia env docker image --clone   # clone repo inside container instead of volume-mount
```

## Stages

| Stage | Command | What it does | Slow? | Prerequisite |
|-------|---------|-------------|-------|-------------|
| `image` | `dia env docker image` | Build Docker image with MSVC Build Tools + Windows SDK baked in; pin base image tag | One-time, slow | Docker Desktop in Windows Containers mode |
| `deps` | `dia env docker deps` | Run `deps.json` restore inside the container; populate `External/` on the volume-mounted repo | Medium | `image` stage complete; `deps.json` exists (deps-manifest feature) |
| `paths` | `dia env docker paths` | Verify PATH is correctly configured in the image via `ENV` instructions; rebuilds image if PATH entries are missing | Fast | `image` stage complete |

Running `dia env docker` with no arguments executes `image → deps → paths` in order. If a stage has already been run and its sentinel is present, it is skipped unless `--force` is passed.

## Files Introduced

```
build-env/
├── Dockerfile                  # Windows Container image definition
├── docker-compose.yml          # Stage orchestration
└── scripts/
    ├── restore-deps.ps1        # deps stage: reads deps.json, downloads/unzips External/
    └── configure-paths.ps1     # paths stage: sets PATH for MSBuild, Python, Git
```

```
Dia/DiaCLI/
└── dia_cli/
    ├── cli/
    │   └── env/
    │       └── docker_cmd.py   # Click group: dia env docker [image|deps|paths]
    └── commands/
        └── env/
            └── docker/
                ├── image_cmd.py
                ├── deps_cmd.py
                └── paths_cmd.py
```

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| `deps-manifest` feature (DiaEnv) | Hard | `deps.json` must exist before `dia env docker deps` can run; this feature must be implemented first |
| Docker Desktop (Windows Containers mode) | Runtime | Developer machine prerequisite; `dia env verify` reports its presence |
| `dia env verify` (`env-verify-cmd` feature) | Integration | Should report Docker Desktop status as part of environment health check |

## Acceptance Criteria

1. `dia env docker image` builds successfully from `build-env/Dockerfile` on a host with Docker Desktop in Windows Containers mode
2. The Dockerfile pins the Windows base image tag and MSVC Build Tools version explicitly (no `latest` tags)
3. `dia env docker deps` restores all `External/` dependencies inside the container using `deps.json`; exits non-zero if `deps.json` is absent or a dep fails SHA-256 verification
4. `dia env docker paths` configures MSBuild, Python, and Git on PATH inside the container
5. The repo root is volume-mounted into the container at a consistent path; no source files are copied into the image
6. Running `dia env docker` with no arguments runs all three stages in order and exits 0 on success
7. `dia env verify` reports whether Docker Desktop is installed and whether it is in Windows Containers mode
8. `--force` flag on any stage bypasses sentinel-file skip logic and re-runs the stage
9. `--clone` flag is reserved (documented as future, exits with a clear "not implemented" message if used)
10. Both `Debug|x64` and `Release|x64` build configurations are reachable after `deps` and `paths` stages complete (verified by a smoke-test `msbuild /t:Clean` that does not require the full build)

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaEnv | @docs/specs/systems/dia/diaenv.md |
| Research | Docker vs Hyper-V | @docs/research/docker_hyperv/summary.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | Use StringCRC for all identifiers | Compliant — this feature is Python tooling only; no C++ identifiers introduced |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | Compliant — development tooling; no application runtime components |
| PD-003 | Platform | Component-based entities | Compliant — development tooling; no entity system used |
| PD-004 | Platform | No STL containers in public APIs | Compliant — Python only; no C++ public API surface |
| PD-005 | Platform | x64 Windows only | Compliant — Docker Windows Containers target x64 Windows exclusively; Linux containers explicitly excluded |
| PD-006 | Platform | VS project files are source of truth | Compliant — Dockerfile installs MSVC Build Tools to match the same toolset the `.vcxproj` files require; does not replace MSBuild |
| PD-007 | Platform | C++20 required | Compliant — MSVC Build Tools version pinned in Dockerfile must include the v143 toolset (VS 2022) which supports `/std:c++20` |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | Compliant — repo is volume-mounted; `Directory.Build.props` is present on the mount; container does not override any build output properties |
| AD-001 | Dia App | Module system with YAML frontmatter | Compliant — Python tooling only; no new C++ module introduced |
| SD-CLI-001 | DiaCLI | MDK CLI architecture | Compliant — `docker_cmd.py` follows the same two-file plugin pattern used by all DiaCLI commands |
| SD-CLI-002 | DiaCLI | Python-based implementation | Compliant — all DiaCLI surface is Python; PowerShell scripts are container-internal only |
| SD-CLI-003 | DiaCLI | Separate from C++ DiaAPI | Compliant — no C++ DiaAPI commands introduced |
| SD-CLI-006 | DiaCLI | Click framework for argument parsing | Compliant — `docker_cmd.py` uses Click groups and decorators |
| SD-CLI-008 | DiaCLI | Exit codes follow Unix conventions | Compliant — exits 0 on success, 1 on hard failure |
| SD-ENV-001 | DiaEnv | `deps.json` is single source of truth for binary SDKs | Compliant — `deps` stage reads `deps.json`; no parallel dep list inside the Dockerfile |
| SD-ENV-002 | DiaEnv | SHA-256 verification required for all downloaded deps | Compliant — `restore-deps.ps1` verifies SHA-256 for every dep entry before unzipping |
| SD-ENV-008 | DiaEnv | Docker scope is headless build + test only | Compliant — no GPU, no GUI, no SFML/Awesomium runtime in the container |
| SD-ENV-009 | DiaEnv | Docker files live under `build-env/` | Compliant — `Dockerfile`, `docker-compose.yml`, and scripts all live under `build-env/` |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | image stage | Which Windows base image should the Dockerfile use — `mcr.microsoft.com/windows/servercore` or `mcr.microsoft.com/windows`? | `servercore` is smaller (~5 GB vs ~20 GB) and sufficient for MSVC builds; no GUI/desktop shell needed. Use `servercore`. |
| 2 | image stage | How should MSVC Build Tools be installed inside the Dockerfile — `winget`, VS installer EXE, or a pre-downloaded offline layout? | VS Build Tools bootstrapper (`vs_buildtools.exe`) with `--quiet --wait` flags; download in the Dockerfile `RUN` step. This is the most reliable silent-install path. |
| 3 | deps stage | What happens if `deps.json` does not exist when `dia env docker deps` is run? | Exit code 3 (manifest not found), per SD-ENV exit code table. Clear error message naming the missing file and pointing to the `deps-manifest` feature. |
| 4 | paths stage | PATH configuration inside the container — is this done via Docker `ENV` instruction in the Dockerfile, or at runtime via `configure-paths.ps1`? | Resolved — `ENV` instructions baked into the Dockerfile. `configure-paths.ps1` reserved for edge-case overrides only. |
| 5 | image stage | Should the image be pushed to a registry (Docker Hub, GHCR) or only built locally? | Local only for now. Future concern when team grows or hosted CI is adopted. |
| 6 | deps stage | Awesomium SDK has a non-interactive installer — does `restore-deps.ps1` handle arbitrary installer types, or only zip archives? | `deps.json` schema should include an `install_type` field (`zip` or `exe`); `restore-deps.ps1` dispatches accordingly. Spec the `exe` variant in the `deps-manifest` feature. |
| 7 | sentinel files | Where are sentinel files written — inside the container, on the host volume, or both? | Host volume (repo root under `.docker-env/`); this allows `dia env docker` to detect completed stages across container restarts. |
| 8 | `--force` flag | Should `--force` apply to a single named stage or cascade to all dependent stages? | Named stage only; caller is responsible for re-running dependents if needed. |
| 9 | verify integration | Should `dia env verify` fail (exit 1) or warn (exit 2) if Docker Desktop is absent? | Warn (exit 2) — Docker is not required to build the project on the host; its absence is non-blocking. |
| 10 | git clone stub | Should `--clone` silently no-op or print a "not implemented" error? | Print a clear "not implemented" error with a pointer to the future feature; exit 1. Prevents silent misuse. |

## Open Questions

| # | Question | Resolution |
|---|----------|------------|
| 1 | PATH configuration: `ENV` in Dockerfile vs runtime `configure-paths.ps1`? | Resolved — bake PATH into the image via `ENV` instructions. `configure-paths.ps1` reserved for edge-case overrides only. The `paths` stage writes `ENV` entries into the Dockerfile and rebuilds the image if needed. |

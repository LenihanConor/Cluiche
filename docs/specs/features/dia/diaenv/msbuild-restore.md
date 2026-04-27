# Feature Spec: msbuild-restore

## Parent System
@docs/specs/systems/dia/diaenv.md

## Status
`Done`

## Summary

Add a `RestoreExternals` MSBuild target in a new `Directory.Build.targets` file at the repo root. The target reads `deps.json` and downloads any missing binary SDK entry before compilation begins — a no-op when all deps are already present (sentinel-file guard). This ensures that opening the solution in Visual Studio and hitting Build always produces a complete `External/` without requiring the developer to remember `dia env deps` first.

## Problem

A developer who clones the repo and opens the solution in Visual Studio will get a build failure because `External/` is empty. They must know to run `dia env deps` first. The MSBuild restore target makes this automatic — the build system restores missing deps as part of the build, with zero overhead when everything is already in place.

## Goals

- `Directory.Build.targets` defines a `RestoreExternals` target that runs before `Build`
- Target reads `deps.json` and checks sentinel files; downloads only missing deps
- No-op fast path: if all sentinels are present, the target completes in < 1 second with no network I/O
- Works in both Debug and Release configurations
- Does not override any properties owned by `Directory.Build.props` (PD-008)
- Invokes the same `deps_restore.py` logic used by `dia env deps` — no duplicated download logic

## Non-Goals

- Toolchain install (VS, Python, Git) — that requires elevation and is a one-time operation
- Submodule init — MSBuild cannot run `git submodule update`; developer must do this manually or via `dia env setup`
- Replacing `dia env deps` — the MSBuild target is a safety net, not a replacement
- Running during `Clean` or `Rebuild` — restore runs before `Build` only

## MSBuild Target Design

### `Directory.Build.targets` (repo root)

```xml
<?xml version="1.0" encoding="utf-8"?>
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">

  <Target Name="RestoreExternals" BeforeTargets="Build">
    <PropertyGroup>
      <_DepsJson>$(MSBuildThisFileDirectory)deps.json</_DepsJson>
      <_DiaCLI>$(MSBuildThisFileDirectory)Dia\DiaCLI\dia</_DiaCLI>
      <_SentinelDir>$(MSBuildThisFileDirectory).diaenv\deps</_SentinelDir>
    </PropertyGroup>

    <!-- Skip entirely if deps.json does not exist -->
    <Warning
      Condition="!Exists('$(_DepsJson)')"
      Text="DiaEnv: deps.json not found — skipping RestoreExternals. Run `dia env deps` to initialise." />

    <!-- Run dep restore via DiaCLI Python script -->
    <Exec
      Condition="Exists('$(_DepsJson)')"
      Command="python &quot;$(_DiaCLI)&quot; env deps --quiet"
      ConsoleToMSBuild="true"
      WorkingDirectory="$(MSBuildThisFileDirectory)" />
  </Target>

</Project>
```

### Sentinel guard

`dia env deps --quiet` internally skips any dep whose sentinel file (`.diaenv/deps/<id>.restored`) is already present. If all sentinels exist, the Python process starts and exits in under a second with no network I/O. The `BeforeTargets="Build"` hook pays only the Python startup cost on already-provisioned machines.

### Performance characteristics

| Scenario | Expected overhead |
|----------|------------------|
| All deps present (sentinels valid) | ~0.5–1s (Python startup + sentinel checks) |
| One dep missing | Download time for that dep (one-time cost) |
| `deps.json` absent | MSBuild warning, no failure, build continues |
| DiaCLI not installed | MSBuild error (Python not found); actionable message |

## Files Introduced / Modified

| File | Change |
|------|--------|
| `Directory.Build.targets` (repo root) | New — defines `RestoreExternals` target |
| `Dia/DiaCLI/dia_cli/cli/env/deps_cmd.py` | Add `--quiet` flag (suppress progress, print errors only) |

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| `deps-manifest` feature | Hard | `deps.json` and `deps_restore.py` must exist |
| Python 3.11 on PATH | Runtime | `python` must be resolvable by MSBuild `Exec` task |
| `Directory.Build.props` | Existing | Must not be modified; `Directory.Build.targets` is a separate file |

## Acceptance Criteria

1. `Directory.Build.targets` exists at repo root and defines a `RestoreExternals` target that runs `BeforeTargets="Build"`
2. Building the solution in Visual Studio on a machine with all sentinels present adds < 2 seconds to build time
3. Building on a machine with a missing dep triggers an automatic download and unpack before compilation proceeds
4. If `deps.json` is absent, `RestoreExternals` emits a MSBuild warning and the build continues (does not fail)
5. If `python` is not on PATH, `RestoreExternals` emits a clear MSBuild error naming the fix (`dia env setup --toolchain`)
6. `Directory.Build.targets` does not set or override `OutDir`, `IntDir`, `PlatformToolset`, `WindowsTargetPlatformVersion`, or `LanguageStandard` (PD-008)
7. Target runs in both `Debug|x64` and `Release|x64` configurations
8. `dia env deps --quiet` flag suppresses progress output and prints only errors

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaEnv | @docs/specs/systems/dia/diaenv.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | Use StringCRC for all identifiers | Compliant — no C++ identifiers introduced; MSBuild XML only |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | Compliant — build tooling; no runtime components |
| PD-003 | Platform | Component-based entities | Compliant — build tooling; no entity system used |
| PD-004 | Platform | No STL containers in public APIs | Compliant — no C++ public API surface |
| PD-005 | Platform | x64 Windows only | Compliant — MSBuild target is Windows-only; consistent with PD-005 |
| PD-006 | Platform | VS project files are source of truth | Compliant — `Directory.Build.targets` extends the build without modifying any `.vcxproj`; MSBuild imports it automatically |
| PD-007 | Platform | C++20 required | Compliant — no compiler settings modified |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir/PlatformToolset/LanguageStandard | Compliant — `Directory.Build.targets` is a separate file and sets none of these properties; explicitly verified in acceptance criteria 6 |
| AD-001 | Dia App | Module system with YAML frontmatter | Compliant — no new C++ module introduced |
| SD-CLI-002 | DiaCLI | Python-based implementation | Compliant — target invokes DiaCLI Python script; no new C++ code |
| SD-ENV-001 | DiaEnv | `deps.json` is single source of truth | Compliant — target reads `deps.json` via `deps_restore.py`; no parallel dep list in the target |
| SD-ENV-002 | DiaEnv | SHA-256 verification required | Compliant — SHA-256 is enforced inside `deps_restore.py`; target does not bypass it |
| SD-ENV-005 | DiaEnv | MSBuild RestoreExternals uses sentinel files to guard against re-download | Compliant — this is the implementing feature of SD-ENV-005 |
| SD-ENV-010 | DiaEnv | Python 3.11 is the single Python version | Compliant — `Exec` task invokes `python` which resolves to Python 3.11 per `winget-manifest` |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Python PATH | MSBuild `Exec` uses the system PATH at build time — is Python 3.11 guaranteed to be on PATH after `dia env setup --toolchain`? | Yes — `winget import` for `Python.Python.3.11` adds Python to the system PATH. `dia env verify` checks this. If not on PATH, the MSBuild error message names the fix. |
| 2 | Build performance | Should the `RestoreExternals` target be skipped entirely during `Rebuild` (which calls `Clean` then `Build`)? | No — `Rebuild` calls `Build` which triggers `BeforeTargets="Build"`; this is correct. Deps should be restored before a rebuild compiles. Sentinels ensure it's fast when already present. |
| 3 | DiaCLI path | The target hardcodes `Dia\DiaCLI\dia` as the CLI entrypoint — is this path stable? | Yes — DiaCLI lives at `Dia/DiaCLI/` per the module structure; this path is repo-root-relative and stable. If DiaCLI moves, the target must be updated. |
| 4 | Parallel builds | MSBuild can run project builds in parallel (`/m` flag) — could multiple projects trigger `RestoreExternals` simultaneously? | `BeforeTargets="Build"` runs per-project. Multiple projects could invoke `dia env deps --quiet` concurrently. `deps_restore.py` must be safe to run concurrently — sentinel writes should be atomic (temp file rename pattern from `deps-manifest` spec satisfies this). |
| 5 | `deps.json` absent warning | Should a missing `deps.json` be a warning or a no-op? | Warning — a developer who has never run `dia env deps` and has no `deps.json` should be informed, but the build should not fail outright. The warning text names the fix. |

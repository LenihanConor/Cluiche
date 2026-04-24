# Research: Explore — Portable Build Setup & Development Environment

**Session date:** 2026-04-24
**Folder:** docs/research/portabl_build_setup/

## Problem Space Overview

The Cluiche project is a Windows-only C++20 game engine with a significant dependency surface: SFML, Ultralight (CEF-based HTML UI), Protobuf, Python 3.11, pybind11, WebSocket++, Asio, GoogleTest, jsoncpp, and the DiaCLI Python toolchain. None of these are vendored in the repository in a state that a fresh machine could consume directly — the `.gitignore` explicitly excludes large SDKs (Ultralight, Python311, pre-built Protobuf). A developer cloning the repo starts with a source tree that cannot build without a separate, manual setup ritual.

The problem has two entangled dimensions. First, **toolchain portability**: Visual Studio 2022 with specific workloads (C++ Desktop, Windows SDK 10.0.22621), MSBuild, and Python 3.11 must be present. Second, **dependency portability**: the `External/` directory must be populated with binary SDKs that currently live nowhere in version control. A partial solution to one without the other leaves the repo still unbuildable.

There is also a third dimension unique to this project: **AI context portability**. The `.claude/` directory contains settings, steering docs, memory, skills, commands, and hooks that represent accumulated working knowledge between the user and the AI assistant. Losing this on a machine transition means starting the collaborative relationship from scratch. A complete "move to another machine" solution must carry this context along with the code and toolchain.

## Existing Approaches

- **Manual README-driven setup**: Document every manual step; developer follows the guide. High maintenance cost, frequently goes stale, error-prone.
- **Chocolatey / winget scripts**: Windows package managers can automate VS, Python, and SDK installs. Reproducible but still imperative; no isolation.
- **Docker on Windows**: Windows Containers can run MSVC/MSBuild. Requires Docker Desktop, Windows Container mode (not Linux containers). Build-only; no GPU/display for running the game. Good for headless CI builds.
- **Dev Containers (VS Code)**: `.devcontainer/` spec drives Docker image creation; VS Code Remote-Containers attaches. Well-supported for Linux stacks; Windows container support exists but is heavier.
- **Git LFS / Git submodules**: Binary SDKs stored in LFS or as submodules. Solves the External/ gap cleanly; integrates with normal `git clone`. No toolchain solution.
- **PowerShell bootstrap script**: Single `bootstrap.ps1` installs winget packages, downloads SDKs, sets environment variables. Runs in <5 minutes on a fresh machine. Simple and inspectable.
- **Package manifest (winget export)**: `winget export` can serialise an installed-software state to JSON; `winget import` restores it. Covers VS workloads, Python, tools.
- **DiaCLI `setup` command**: The existing DiaCLI Python CLI already has a `setup_cmd` and a `software_installer` that downloads and unzips SDKs from URLs. This is the natural extension point.
- **Symbolic links / junction points**: Map `External/` to a shared network location or cloud-synced folder. Low ceremony but fragile — requires the other machine to have access to the same share.
- **Dotfiles-style `.claude/` sync**: Copy `.claude/` as part of the environment setup. Conceptually simple; the directory is just files.

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| **Isolation model** | None (bare metal), Docker container, Dev Container | Docker adds overhead; bare metal is simpler for a single-dev project |
| **Dependency delivery** | Manual download, bootstrap script, DiaCLI command, Git LFS, package registry | DiaCLI already exists and has download+unzip primitives |
| **Toolchain automation** | None, winget/choco script, Dev Container image | winget is lowest friction on Windows |
| **AI context portability** | Manual copy, part of git repo, symlink, DiaCLI command | `.claude/` is already in the repo root — it travels with the clone |
| **Scope of "environment"** | Just build deps, build deps + toolchain, build + toolchain + AI context | User explicitly wants all three |
| **Trigger mechanism** | Manual script, DiaCLI command, VS task, git hook | DiaCLI command is the stated preference |
| **Sync direction** | One-way push, bidirectional | For single-dev, one-way is sufficient |

## Known Tradeoffs

- **Docker for MSVC**: Windows containers require "Process isolation" mode on Windows 11 and a matching OS build number. Container images with VS Build Tools are ~20–30 GB. Build-only; you cannot run graphical applications (SFML window) inside a container without additional plumbing (virtual display or WSL2 + X server).
- **Git LFS**: Solves the binary versioning problem cleanly but adds LFS bandwidth costs and requires LFS to be installed on every clone machine. Binary SDKs like Ultralight (~300 MB) are large.
- **winget reliability**: `winget import` can silently fail on individual packages; VS workload installs require elevated privileges and a reboot. Not fully scriptable without workarounds.
- **DiaCLI download approach**: DiaCLI's `software_installer` already downloads from URLs. The missing piece is knowing the authoritative URL for each SDK — this must be maintained in a manifest file.
- **AI context in git**: Putting `.claude/` in the repo means AI memory and settings travel with every `git clone` or `git pull`. This is the desired behaviour, but it means memory is shared/overwritten on every push if two machines diverge.

## Known Pitfalls (C++ / game engine context)

- SDK version pinning: if the download URL for e.g. Ultralight is not versioned, a fresh install may pull a newer incompatible SDK.
- Path-absolute assumptions: `.vcxproj` files use `$(SolutionDir)` and relative paths, which is correct — but if `External/` is populated in a non-standard location, include paths break silently.
- Python version mismatch: DiaCLI and DiaPython both rely on Python 3.11 specifically. A fresh machine with Python 3.12 may break quietly.
- Windows SDK version: `Directory.Build.props` pins `WindowsTargetPlatformVersion`. A machine without that exact SDK version installed will fail at link time.
- Visual Studio workload drift: VS installer is not idempotent for workloads — adding a missing workload requires VS to be closed.
- `.claude/settings.local.json` contains machine-specific settings (e.g., allowed tools, hooks). Sharing it verbatim across machines may not be desirable.

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| DiaCLI | Python-based CLI already has `setup_cmd`, `software_installer` (download+unzip), and `dia_cli_config` — the natural home for a `dia env setup` command |
| DiaAPI | C++ plugin CLI framework — less relevant for setup (Python is better for OS-level provisioning) |
| DiaCore / DiaMaths | No setup relevance, but they are the first things that need to build after setup |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-005 x64 Windows only | Simplifies toolchain matrix — no cross-platform Docker Linux images needed; can target exactly Windows 11 x64 |
| PD-006 VS project files are source of truth | Bootstrap must install VS with the correct workloads and MSBuild; cannot replace the build system |
| PD-007 C++20 required | Must install VS 2022 (v143 toolset) specifically — VS 2019 does not fully support C++20 |
| PD-008 Directory.Build.props owns OutDir/IntDir | No path changes needed for build output; `External/` population is the only gap |
| PD-001 StringCRC | No setup relevance |
| PD-004 No STL in public APIs | No setup relevance |

### Existing `.claude/` Structure

The `.claude/` directory is already tracked in git (modulo `settings.local.json`). It contains:
- `settings.json` / `settings.local.json` — Claude Code configuration
- `steering/` — tech and structure docs loaded into AI context
- `commands/` — custom slash commands
- `skills/` — reusable skill definitions
- `projects/.../memory/` — persistent AI memory files

This means **AI context portability is already solved for most of the `.claude/` tree** — it travels with the repo. The only gap is `settings.local.json` (machine-specific, gitignored) and the memory directory path, which is user-profile-based (`C:\Users\<user>\.claude\projects\...`).

## Open Questions for Ideation

- Should the `External/` SDK manifest be a JSON file (extending DiaCLI's existing config pattern) or something simpler like a `deps.txt` with URL+hash?
- Is Docker worthwhile at all for this project given PD-005 (Windows only) and the need to run graphical applications? Or should it be scoped strictly to headless CI builds?
- Should `dia env setup` be a single command that does everything (toolchain + deps + AI context check), or a composable set of sub-commands?
- How should SDK version pinning work — hash verification, semver constraints, or exact version locks?
- What is the right behaviour when `External/` already has some (but not all) dependencies — skip, overwrite, or prompt?
- Should AI memory migration (user-profile path) be in scope, or is it out of scope since memory is inherently per-user?
- Is there value in a `dia env export` command that produces a snapshot of the current environment for transfer, vs. always re-downloading from canonical URLs?

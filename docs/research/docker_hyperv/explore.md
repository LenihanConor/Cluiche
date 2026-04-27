# Research: Explore — Docker vs Hyper-V for Dev Environment Isolation

**Session date:** 2026-04-25
**Folder:** docs/research/docker_hyperv/

## Problem Space Overview

Dev environment isolation solves the "works on my machine" problem by ensuring every contributor builds and runs the project against a consistent set of tools, compilers, SDKs, and dependencies. For Cluiche, the relevant toolchain is heavy and Windows-only: MSVC, Windows SDK, MSBuild, SFML, Awesomium, jsoncpp, and a specific Visual Studio toolset. Mismatched compiler versions or SDK paths are a common source of hard-to-reproduce bugs.

The two dominant isolation strategies on Windows are **Docker Desktop (Windows Containers)** and **Hyper-V VMs**. Docker provides lightweight OS-level containerisation; Hyper-V provides full hardware virtualisation. Between them they cover the entire isolation spectrum from "share the kernel, isolate the filesystem" to "completely separate OS instance with its own kernel and device stack".

For a native Windows C++ game engine project, the choice is non-trivial. Both Docker Windows Containers and Hyper-V VMs can run MSVC and MSBuild, but they differ sharply on GPU access, GUI support, image size, startup time, and operational complexity — all of which matter for a project that needs a renderer (SFML/DirectX) and an editor (CluicheEditor, CEF/Awesomium).

## Existing Approaches

- **Docker Windows Containers (process isolation)** — Share the host Windows kernel; lightweight; images can host MSVC build tools via `mcr.microsoft.com/dotnet/framework/sdk` or custom MSVC images. No GPU passthrough in process-isolation mode.
- **Docker Windows Containers (Hyper-V isolation)** — Each container gets a minimal Hyper-V utility VM. Slightly heavier than process isolation; still no GPU by default.
- **WSL 2 + Docker Desktop** — Linux containers on Windows via WSL 2 kernel; not applicable to MSVC/Windows builds.
- **Hyper-V VMs (manual)** — Full Windows VM; can be snapshotted, cloned, exported. GPU passthrough via RemoteFX (deprecated) or GPU-P (Windows 11 host). Manual setup and maintenance burden.
- **Vagrant + Hyper-V provider** — Automates Hyper-V VM lifecycle; boxes versioned in source control; provisioning via PowerShell/Ansible.
- **Dev Containers (VS Code / GitHub Codespaces)** — JSON-defined dev environment; typically Docker-backed but Hyper-V backend is possible with Vagrant. Strong IDE integration story.
- **Sandbox / Windows Sandbox** — Lightweight throwaway VM using Hyper-V; GUI support; no persistent state without extra scripting. Not suitable for long-lived dev envs.
- **Packer-built golden images** — Immutable VM/container images baked with all toolchain dependencies; used as base for VMs or containers; separates "toolchain provisioning" from "daily use".

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| Isolation level | Process (container) → OS (Hyper-V VM) | More isolation = heavier startup, more resource use |
| GPU access | None / GPU-P (Hyper-V) / host passthrough | Needed for SFML renderer and CluicheEditor |
| GUI / windowing | None / RDP / native | Game and editor require a real window; containers have no display by default |
| Image lifecycle | Mutable VM / Immutable container image | Immutable is easier to reason about; mutable is easier to develop against |
| IDE integration | VS Code Dev Containers / VS remote / RDP | Strong preference for Visual Studio on Windows |
| Startup time | Seconds (container) → Minutes (VM) | Affects daily dev friction |
| Disk footprint | ~10–20 GB container image vs ~40–80 GB VM | Windows container base images are large |
| Host OS requirement | Windows 10/11 Pro/Enterprise (Hyper-V) | Hyper-V not available on Windows Home |

## Known Tradeoffs

- **Docker Windows Containers** are fast and scriptable but have no native GPU access and no windowed GUI without significant workarounds (VNC, RDP sidecar, virtual display). This is a hard blocker for any build step that exercises the renderer or editor.
- **Hyper-V VMs** support GPU-P on Windows 11 hosts, making GPU-accelerated rendering possible, but setup is complex and RemoteFX (the older path) is removed from Windows 11.
- Container images for Windows + MSVC are large (15–25 GB base before adding SDKs). Pull time and local disk use are significant compared to Linux containers.
- Hyper-V VMs can be snapshotted before risky operations; containers cannot hold in-progress state across image rebuilds without a volume mount.
- **Vagrant + Hyper-V** adds a clean lifecycle CLI (`vagrant up/halt/destroy`) and Vagrantfile versioning, but adds another toolchain layer to maintain.
- WSL 2 (Linux) cannot run MSVC or produce Windows PE binaries without Wine hacks — ruled out for this project.

## Known Pitfalls (C++ / game engine context)

- Windows container base images must match the host OS build number exactly for process-isolation mode; version drift breaks containers silently.
- MSVC in a container requires a full Build Tools installer (~5–7 GB) baked into the image; re-baking on VS updates is slow.
- Awesomium SDK is old and may have installer quirks in a non-interactive container build context.
- SFML links against system OpenGL/DirectX; in a container without GPU these link steps may succeed but runtime will fail.
- Hyper-V and VirtualBox/VMware cannot run simultaneously on the same host (Hyper-V takes the hypervisor layer). Docker Desktop on Windows uses Hyper-V or WSL 2 as its backend — running Docker Desktop alongside a manual Hyper-V VM is fine, but running Docker Desktop alongside VirtualBox is not.
- GPU-P in Hyper-V is a paravirtualised interface; not all GPU operations are supported (notably DirectX 12 compute and Vulkan have gaps).

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| DiaSFML | Requires OpenGL at runtime; won't run headless without a virtual display or GPU passthrough |
| DiaGraphics | Abstraction layer over rendering; if a software renderer backend existed, container builds could test non-GPU paths |
| DiaUIAwesomium / DiaUICEF | Browser-based UI requires a windowing system; hard to run inside a headless container |
| DiaAPI | CLI/plugin framework — pure build-time, no GPU dependency; ideal candidate for container-based build/test |
| GoogleTests | Unit tests that don't exercise the renderer can run headlessly inside a container |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-005 x64 Windows only | Both Docker Windows Containers and Hyper-V VMs must be x64 Windows — no Linux containers; no ARM |
| PD-006 VS project files are source of truth | Isolated environment must have MSBuild + MSVC + the same Windows SDK version; version pinning is critical |
| PD-007 C++20 required | MSVC toolset version must be ≥ 19.29 (VS 2019 16.9+) or VS 2022; image/VM must pin this |
| PD-008 Directory.Build.props owns OutDir/IntDir | Build paths are repo-relative; volume mounts or cloned repos inside the VM/container must preserve relative paths |

## Open Questions for Ideation

- Is the primary goal **headless build + unit test isolation** (no GPU needed) or **full interactive dev environment** (GPU + GUI needed)?
- Should the isolated environment be used by **one developer** or shared across a **team / CI pipeline**?
- How important is **Visual Studio IDE** inside the environment vs a command-line build workflow?
- Is there a tolerance for the complexity of Vagrant + Hyper-V provisioning scripts, or is a simpler (even if less reproducible) approach preferred?
- Should the environment support **running the game / editor** (requires GPU + window) or only **building and running unit tests**?
- Would a **hybrid approach** (Docker for headless build/test, Hyper-V VM for interactive dev) be acceptable operational overhead?

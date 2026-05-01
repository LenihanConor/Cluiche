# Research: Choice — Docker vs Hyper-V for Dev Environment Isolation

**Date:** 2026-04-25
**Chosen candidate:** Docker Windows Container — Headless Build + Test Only

## Rationale

Chosen because it is the cheapest, lowest-risk way to achieve reproducible build and test environments. It directly supports the GoogleTests pipeline, pins the MSVC toolset and Windows SDK version (honouring PD-006 and PD-007), and requires no GPU or GUI — avoiding the hardest Windows container problems entirely. The user agreed with the automated recommendation.

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|------------------|
| Docker Compose Multi-Stage Pipeline | Natural follow-on to the chosen candidate; premature before the base image is stable |
| Hybrid Docker + Hyper-V | Adds significant complexity; not needed until interactive GPU/GUI isolation is a requirement |
| Vagrant + Hyper-V Full VM | Higher cost and operational overhead; GPU/GUI capability not currently needed |
| VS Code Dev Containers | VS Code-only workflow; Visual Studio is the primary IDE for this project |
| Packer Golden VM Image | Highest implementation cost; premature for a solo/small team |
| Windows Sandbox + Script | Useful for smoke testing only; not a repeatable daily dev environment |
| Docker + WARP Renderer | Good follow-on for renderer regression testing; out of scope for this iteration |

## Pre-Spec Commitments

- **Base image version:** No pinned Windows Server version required; image should document the version used but developer machines are not constrained to a specific host OS build number.
- **CI target:** Local-only for now; invoked via DiaCLI (DiaAPI plugin), not a hosted CI system.
- **Build configurations:** Both `Debug|x64` and `Release|x64` must be supported; the container must be able to build either configuration on demand.
- **File location:** All Docker-related files live under `build-env/` in the repo root (e.g. `build-env/Dockerfile`, `build-env/docker-compose.yml`).

## Next Step

Run /spec-feature or /spec-system with this candidate as input.
Suggested parent system: DiaAPI (DiaCLI plugin surface) + GoogleTests pipeline

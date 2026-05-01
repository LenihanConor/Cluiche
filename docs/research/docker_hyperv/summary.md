# Research Summary — Docker vs Hyper-V for Dev Environment Isolation

**Session folder:** docs/research/docker_hyperv/
**Date:** 2026-04-25

## One-Line Answer

A Docker Windows Container image baked with MSVC Build Tools and all Cluiche dependencies gives every developer a reproducible, version-pinned build and test environment invocable via a DiaCLI plugin — with zero GPU/GUI complexity.

## Journey

1. **Explored:** Docker Windows Containers are viable for headless MSVC builds but have no GPU/GUI support; Hyper-V VMs cover GPU/GUI but are heavier and more complex. PD-005 (x64 Windows only) rules out Linux containers entirely.
2. **Ideated:** 8 candidates generated, ranging from a minimal headless Docker image (S) to a Packer-built golden VM (L), covering the full isolation spectrum.
3. **Evaluated:** Docker Headless Build+Test scored highest (3.40) on the strength of low cost, low risk, and direct fit with the GoogleTests pipeline; GPU/GUI candidates scored lower due to implementation cost and uncertainty.
4. **Chose:** Docker Windows Container — Headless Build+Test; user confirmed the recommendation with no modifications to the top candidate.

## Chosen Work Item

**Name:** Docker Windows Container — Headless Build + Test  
**Home module:** DiaAPI (DiaCLI plugin) + GoogleTests pipeline  
**Suggested spec type:** Feature (under the GoogleTests or DiaAPI system spec)  
**Estimated size:** S (≤1 week)

## Key Insights from Exploration

- Windows container base images must match the host OS build number for process-isolation mode — pin and document the image tag explicitly.
- MSVC Build Tools installer is ~5–7 GB; baking it into the image is a one-time cost but re-baking on VS updates is slow; use multi-stage builds to keep the runtime layer lean.
- Awesomium SDK has a non-interactive installer that will need a silent-install PowerShell wrapper for use inside a container build step.
- Both `Debug|x64` and `Release|x64` must be supported; the DiaCLI plugin should accept a `--config` argument to select the configuration.
- Docker Desktop's Hyper-V backend coexists fine with manually managed Hyper-V VMs — the Hybrid candidate (Candidate 3) remains a viable future upgrade path.
- PD-008 (`Directory.Build.props` owns OutDir/IntDir) means output paths are repo-relative; volume-mounting the repo root into the container at a consistent path is all that's needed to preserve this.

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| Docker Compose Multi-Stage | Good follow-on; premature before base image is stable |
| Hybrid Docker + Hyper-V | GPU/GUI not currently needed; adds two environments to maintain |
| Vagrant + Hyper-V Full VM | Higher cost; interactive dev not the current goal |
| VS Code Dev Containers | VS Code-only; Visual Studio is the primary IDE |
| Packer Golden VM Image | Highest cost; premature for current team size |
| Windows Sandbox + Script | Smoke-test utility only; not a daily dev environment |
| Docker + WARP Renderer | Good renderer regression candidate; out of scope this iteration |

## References

- docs/research/docker_hyperv/explore.md
- docs/research/docker_hyperv/ideate.md
- docs/research/docker_hyperv/evaluate.md
- docs/research/docker_hyperv/choose.md

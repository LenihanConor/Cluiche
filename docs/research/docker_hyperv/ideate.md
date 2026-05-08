# Research: Ideate — Docker vs Hyper-V for Dev Environment Isolation

**Input:** docs/research/docker_hyperv/explore.md

## Candidates

### Candidate 1: Docker Windows Container — Headless Build + Test Only
**Home module/system:** CI / GoogleTests pipeline
**Size:** S
**Description:** A Docker Windows Container image baked with MSVC Build Tools, Windows SDK, and all Cluiche third-party headers/libs. Used exclusively for `msbuild` + `UnitTests.exe` runs — no renderer, no GUI. The image is version-pinned to a specific VS toolset and Windows SDK build number. A `Dockerfile` and a `docker-compose.yml` live in the repo root; developers run `docker compose run build` to get a clean build and `docker compose run test` to run GoogleTests. Source is volume-mounted so no git clone is needed inside the container.
**Primary value:** Any developer (or CI runner) gets a byte-for-byte identical compiler and SDK with a single command; eliminates "wrong toolset" build failures permanently.

---

### Candidate 2: Vagrant + Hyper-V — Full Interactive Dev VM
**Home module/system:** Dev environment / all applications
**Size:** M
**Description:** A `Vagrantfile` in the repo root defines a Windows Server or Windows 11 Hyper-V VM pre-provisioned with Visual Studio 2022, all External/ dependencies, and repo cloned inside. Developers run `vagrant up` to spin up the VM; they connect via RDP for a full GUI experience including running CluicheTest and CluicheEditor. The Vagrantfile and a PowerShell provisioning script are versioned in source control. VM snapshots can be taken before risky dependency upgrades.
**Primary value:** Every developer gets an identical, fully interactive Windows dev environment with Visual Studio and all dependencies pre-installed; no manual setup required.

---

### Candidate 3: Hybrid — Docker for Build/Test + Hyper-V VM for Interactive Dev
**Home module/system:** CI pipeline + dev environment
**Size:** M
**Description:** Two isolated environments maintained in parallel: (1) a Docker Windows Container image for headless build and GoogleTests (Candidate 1), and (2) a lightweight Hyper-V VM (manually created or Vagrant-managed) for running the game/editor interactively. Developers use the container daily for compile/test loops and drop into the VM only when they need to exercise the renderer or UI. A shared `build-env/` directory in the repo contains both the `Dockerfile` and the `Vagrantfile`.
**Primary value:** Fast headless iteration via Docker plus full GPU/GUI capability via VM, without forcing developers to maintain one heavyweight environment for everything.

---

### Candidate 4: Windows Dev Container (VS Code Dev Containers extension)
**Home module/system:** Dev environment / VS Code workflow
**Size:** S
**Description:** A `.devcontainer/devcontainer.json` config in the repo root defines a Windows Container-backed Dev Container with MSVC Build Tools. Developers open the repo in VS Code and click "Reopen in Container"; VS Code handles the Docker lifecycle transparently. Build tasks run inside the container via the VS Code terminal. No GUI game execution — this is a build/edit/test workflow only. Relies on Docker Desktop with Windows Containers mode enabled.
**Primary value:** Zero-friction onboarding for VS Code users; the dev environment is defined entirely in source control and activated with one click.

---

### Candidate 5: Packer-Built Golden VM Image (Hyper-V backend)
**Home module/system:** Dev environment / team onboarding
**Size:** L
**Description:** HashiCorp Packer scripts (stored in `build-env/packer/`) automate the creation of a Hyper-V VM image pre-loaded with Windows 11, Visual Studio 2022, all External/ SDKs, and repo prerequisites. The resulting `.vhdx` is hosted on a shared network drive or artifact store. New developers download the image, import it into Hyper-V, and have a working dev environment in minutes. The Packer script is re-run on toolchain updates to produce a new versioned image. GPU-P is configured in the image.
**Primary value:** Eliminates provisioning time entirely for new team members; the image is the single source of truth for the dev environment, versioned and auditable.

---

### Candidate 6: Windows Sandbox + Setup Script — Lightweight Throwaway Env
**Home module/system:** Dev environment / quick validation
**Size:** S
**Description:** Windows Sandbox (built-in Hyper-V-backed throwaway VM) combined with a PowerShell setup script (`build-env/sandbox-setup.ps1`) that installs Build Tools, clones the repo, and runs a build+test cycle. Used for validating that a clean environment can build from scratch — not for daily development. Configuration defined via a `.wsb` (Windows Sandbox config) file in the repo. Destroyed after each session; no persistence needed.
**Primary value:** A cheap, always-clean smoke test that proves the repo builds from zero on a pristine Windows install; catches "missing from docs" dependency problems immediately.

---

### Candidate 7: Docker + Xvfb Virtual Display — Headless Renderer Testing
**Home module/system:** DiaSFML / DiaGraphics / GoogleTests
**Size:** M
**Description:** A Docker Windows Container extended with a virtual framebuffer (software OpenGL via Mesa or WARP — the Windows Advanced Rasterisation Platform software renderer) so that SFML and DiaGraphics can initialise a window and render without a physical GPU. Used for automated renderer smoke tests and screenshot-diff regression tests. WARP is built into Windows and available inside Windows Containers. Requires wrapping SFML init to accept a software device.
**Primary value:** Enables GPU-free automated rendering tests inside CI containers; catches rendering regressions without needing a physical machine with a GPU.

---

### Candidate 8: Docker Compose Multi-Stage — Separate Build, Test, and Package Stages
**Home module/system:** CI pipeline / DiaAPI / GoogleTests
**Size:** S
**Description:** A `docker-compose.yml` with three named services — `build` (compiles the solution), `test` (runs `UnitTests.exe`), and `package` (copies binaries to an output volume) — all using the same base Windows Container image. Each stage can be run independently or chained. Output artifacts are written to a host-mounted `dist/` directory. Designed to slot into any CI system (GitHub Actions, Jenkins, TeamCity) as a portable build pipeline.
**Primary value:** Portable, self-describing build pipeline that any CI system can drive with a single `docker compose run` command; no CI-specific configuration needed.

---

## Coverage Map

The eight candidates span the full design-axis range from the explore stage:

- **Isolation level**: Candidates 1, 4, 7, 8 sit at the lightweight container end; Candidates 2, 5 sit at the full Hyper-V VM end; Candidate 3 spans both; Candidate 6 is a throwaway middle ground.
- **GPU / GUI**: Candidates 2, 3, 5 provide full GPU + GUI; Candidate 7 provides software-rasterised GPU; all others are headless.
- **Size range**: Three S candidates (1, 4, 6, 8 — 8 is also S), two M candidates (2, 3, 7), one L candidate (5) — covers the full scope spectrum.
- **Primary use case**: Build/test automation (1, 4, 7, 8), interactive dev (2, 5), hybrid (3), smoke testing (6).
- **Operational complexity**: 1, 4, 6, 8 are low; 2, 3, 7 are medium; 5 is high.

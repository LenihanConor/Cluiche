# Research: Evaluate — Docker vs Hyper-V for Dev Environment Isolation

**Input:** docs/research/docker_hyperv/ideate.md

## Scoring Criteria

- **Engine Value** (weight 0.25): Improves Dia module reusability or capability — does this make the engine easier to build, test, or validate?
- **Game Value** (weight 0.20): Improves CluicheTest as a demo or testbed — does this make running or iterating on the game easier?
- **Implementation Cost** (weight 0.25): Inverse of effort — 5 = very cheap to set up and maintain, 1 = very expensive
- **Risk** (weight 0.15): Inverse of uncertainty — 5 = well-understood, low chance of surprises; 1 = highly uncertain
- **Cluiche Fit** (weight 0.15): Aligns with module structure and PD-001 through PD-007 — does this slot naturally into the existing project shape?

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|-----------|---------------|-------------|-------------|-------------|------------|-------|
| 1. Docker Headless Build+Test | 4 | 1 | 5 | 3 | 4 | 3.40 |
| 2. Vagrant + Hyper-V Full VM | 3 | 4 | 2 | 3 | 3 | 3.00 |
| 3. Hybrid Docker + Hyper-V | 4 | 4 | 2 | 2 | 4 | 3.20 |
| 4. VS Code Dev Containers | 3 | 1 | 4 | 3 | 3 | 2.90 |
| 5. Packer Golden VM Image | 3 | 4 | 1 | 2 | 3 | 2.60 |
| 6. Windows Sandbox + Script | 2 | 1 | 5 | 4 | 3 | 3.00 |
| 7. Docker + WARP Renderer | 5 | 2 | 2 | 2 | 4 | 3.20 |
| 8. Docker Compose Multi-Stage | 4 | 1 | 4 | 4 | 4 | 3.35 |

_Weighted totals: (Engine×0.25) + (Game×0.20) + (Cost×0.25) + (Risk×0.15) + (Fit×0.15)_

## Top 3 Candidates

### Rank 1: Docker Headless Build+Test (score: 3.40)
**Why:** The highest-scoring candidate because it is cheap to implement (S-sized, a single Dockerfile + docker-compose.yml), eliminates the most common class of dev environment bugs (wrong MSVC toolset or Windows SDK version), and fits naturally into the existing repo shape alongside the GoogleTests pipeline. It does not require GPU or GUI, which sidesteps the hardest Windows container problems entirely. PD-006 and PD-007 compliance is encoded directly in the image — the toolset version is pinned and reproducible.
**Watch out for:** Windows container base images must match the host OS build number for process-isolation mode; version drift between developer machines and the image will silently break containers. Awesomium SDK's non-interactive installer may need wrapping in a PowerShell silent-install script.

---

### Rank 2: Docker Compose Multi-Stage Pipeline (score: 3.35)
**Why:** Extends Candidate 1 with a clean three-stage structure (build → test → package) that maps directly to the existing MSBuild + UnitTests.exe workflow. Low cost, low risk, and the self-describing compose file slots into any CI system without CI-specific configuration. The separate `package` stage is immediately useful for producing distributable builds of CluicheTest.
**Watch out for:** Still headless-only — the same GPU/GUI limitation as Candidate 1 applies. The added value over Candidate 1 is primarily organisational; if the team is small and CI is not yet a priority, the extra compose complexity may not pay off yet.

---

### Rank 3: Hybrid Docker + Hyper-V (score: 3.20, tied with Candidate 7)
**Why:** The only candidate that covers both the headless build/test loop and interactive GPU dev. Candidate 3 is the natural evolution path if the team grows or if running CluicheEditor or CluicheTest inside the isolated environment becomes a requirement. Scores well on Engine Value and Game Value simultaneously.
**Watch out for:** Two environments to maintain instead of one; operational overhead is real. Risk score is lower (2) because Windows container + Hyper-V VM coexistence has some known friction points (Docker Desktop's Hyper-V backend can interfere with manually managed VMs). Best adopted after Candidate 1 is already stable.

---

## Recommendation

**Candidate 1 — Docker Windows Container (Headless Build + Test)** is the clear first step. It solves the most pressing dev environment isolation problem (reproducible compiler + SDK), costs the least to implement, and has the best risk profile of any GPU-free option. It directly supports the GoogleTests pipeline and respects PD-006 (MSBuild as source of truth) and PD-007 (MSVC toolset version pinned in the image). Candidate 8 (Multi-Stage) is the natural follow-on once Candidate 1 is working — they share the same Docker image and differ only in compose structure. Candidate 3 (Hybrid) is the right upgrade path if interactive game/editor isolation later becomes a requirement, but is premature before the headless foundation exists.

# Research: Evaluate — Portable Build Setup & Development Environment

**Input:** docs/research/portabl_build_setup/ideate.md

## Scoring Criteria

- **Engine Value** (0.25): Improves Dia module reusability or capability for engine consumers
- **Game Value** (0.20): Improves CluicheTest as a demo or testbed — or unblocks getting there
- **Implementation Cost** (0.25): Inverse of effort — 5 = very cheap (days), 1 = very expensive (months)
- **Risk** (0.15): Inverse of uncertainty — 5 = well-understood technique, 1 = highly uncertain
- **Cluiche Fit** (0.15): Aligns with module structure and platform decisions PD-001 through PD-008

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|-----------|---------------|-------------|-------------|-------------|------------|-------|
| 1. `dia env setup` full bootstrap | 3 | 2 | 3 | 3 | 5 | 3.10 |
| 2. `dia env verify` health checker | 2 | 1 | 5 | 5 | 4 | 3.30 |
| 3. SDK `deps.json` manifest | 3 | 2 | 5 | 4 | 5 | 3.75 |
| 4. winget toolchain manifest | 2 | 1 | 5 | 3 | 4 | 3.00 |
| 5. `dia env export` snapshot zip | 2 | 1 | 3 | 3 | 3 | 2.35 |
| 6. `External/` submodule migration | 3 | 2 | 4 | 4 | 4 | 3.35 |
| 7. `.claude/` AI context hardening | 1 | 1 | 5 | 5 | 3 | 2.90 |
| 8. `dia env doctor` monitoring | 2 | 1 | 2 | 2 | 4 | 2.10 |
| 9. MSBuild `RestoreExternals` target | 3 | 3 | 4 | 3 | 4 | 3.40 |

## Top 3 Candidates

### Rank 1: SDK `deps.json` Manifest (score: 3.75)
**Why:** Highest score because it delivers the most leverage per unit of effort — a few days of work that unlocks Candidates 1, 8, and 9 as follow-on features. It fits DiaCLI's existing config pattern perfectly (extending `dia_cli_config` and `software_installer` which already exist), satisfies PD-006 by making the dependency contract explicit and machine-readable, and eliminates the tribal knowledge problem (the "go find the right version of Ultralight" ritual) permanently. It is also the most atomic unit of work: it can ship alone and be immediately useful.
**Watch out for:** URL stability — if a download URL for a dep changes (SDK vendor moves their CDN), the manifest breaks. SHA-256 hashing catches corruption but not 404s. A fallback or mirror strategy should be considered early.

### Rank 2: MSBuild `RestoreExternals` Target (score: 3.40)
**Why:** The highest *game value* of any candidate (3) because it makes the build self-healing — a developer who forgets to run any setup command still gets a working build rather than a cryptic linker error. It aligns with PD-006 (MSBuild is the source of truth) by making dependency restoration part of the build pipeline. Requires Candidate 3 (deps.json) as its data source, so it's a natural second step. The implementation is a PowerShell inline MSBuild task, a well-documented pattern.
**Watch out for:** Inline MSBuild PowerShell tasks have quirks around error handling and output capture. Restore should only trigger on missing deps (not re-download on every build), requiring a simple sentinel-file check. Build times must not regress when all deps are present.

### Rank 3: `External/` Submodule Migration (score: 3.35)
**Why:** Solves a distinct class of the problem (source-only, header-only deps like googletest, pybind11, websocketpp, asio) with zero ongoing maintenance after setup — git handles population automatically on `git clone --recurse-submodules`. Does not overlap with Candidates 3/9, which handle binary SDKs. Complements the manifest approach cleanly by splitting `External/` into two categories: submodules (auto-populated) and downloaded (populated by the manifest).
**Watch out for:** Submodule UX is a known developer pain point — forgetting `--recurse-submodules` or `git submodule update --init` is common. Mitigate by adding a `git clone` note to the README and wiring `dia env verify` to detect uninitialised submodules.

## Recommendation

Start with the **SDK `deps.json` Manifest** (Candidate 3). It is the foundational data layer the whole environment story depends on, it costs almost nothing to build (DiaCLI's `software_installer` already exists), and it delivers immediate value by replacing manual SDK hunting with a single `dia env setup --deps` command. It also satisfies the stated user goal — moving to another machine — more directly than any other single candidate, since binary SDKs are the largest missing piece on a fresh clone. PD-006 explicitly makes the build system the source of truth; a `deps.json` manifest extends that principle to the dependency graph. Once the manifest exists, the MSBuild `RestoreExternals` target (Rank 2) and the full bootstrap command (Candidate 1) become straightforward follow-ons rather than open design problems.

# Research: Evaluate — Editor Workspace / Persona

**Input:** docs/research/editor_workspac/ideate.md

## Scoring Criteria

- **Engine Value (0.25):** Improves DiaEditor as a reusable framework — does it benefit any future editor built on DiaEditor?
- **Game Value (0.20):** Improves CluicheEditor as a productive tool for building games (CoW, CluicheTest)
- **Implementation Cost (0.25):** Inverse of effort — 5 = trivial, 1 = very expensive. Considers C++, JS, manifest, and test cost.
- **Risk (0.15):** Inverse of uncertainty — 5 = well-understood, 1 = novel/unknowns. Considers CEF iframe state, plugin lifecycle, migration.
- **Cluiche Fit (0.15):** Alignment with PD-001–PD-009, SED decisions, existing patterns (.diaapp, .cluicheproj, out/).

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|-----------|---------------|-------------|-------------|-------------|------------|-------|
| 1 — Auto-Save Session | 3 | 3 | 5 | 5 | 4 | 3.85 |
| 2 — Named Workspace Profiles | 4 | 5 | 3 | 3 | 4 | 3.80 |
| 3 — Auto-Save + User-Global Presets | 4 | 4 | 3 | 3 | 3 | 3.45 |
| 4 — Manifest Overlays | 3 | 4 | 3 | 3 | 5 | 3.50 |
| 5 — Plugin Pinning + Layout Snapshots | 3 | 3 | 4 | 4 | 4 | 3.55 |
| 6 — Plugin Groups | 3 | 4 | 3 | 3 | 4 | 3.35 |
| 7 — Full Persona System | 5 | 4 | 1 | 2 | 3 | 3.05 |
| 8 — .cluicheproj State Extension | 2 | 3 | 5 | 4 | 5 | 3.65 |

## Top 3 Candidates

### Rank 1: Candidate 1 — Auto-Save Session (score: 3.85)

**Why:** Highest total because it's dead simple, zero risk, and solves the most common frustration (losing state on restart) immediately. No new UI concepts, no migration needed. Builds on existing `DockingLayout::SaveLayout` and `PluginLoaderModule::RestoreLayoutPlugins`. The `.session.json` in `out/` follows PD-009 exactly. Doesn't preclude any future workspace system — it's the natural foundation layer.

**Watch out for:** Doesn't address the named-switching use case ("Editor Dev" vs "Build CoW"). If that's the primary need, this is necessary-but-not-sufficient.

### Rank 2: Candidate 2 — Named Workspace Profiles (score: 3.80)

**Why:** Directly addresses the stated use case — switching between "develop editor" and "build CoW" with one click. High game value because it makes CluicheEditor a serious multi-project tool. Profiles stored in project directory are version-controllable and team-shareable. Framework-level concept (WorkspaceManager in DiaEditor) makes it reusable.

**Watch out for:** Plugin unload/reload during switch is the risky part — CEF iframe state loss (SED-018), plugin save/restore coordination, potential for partially-loaded states if a referenced plugin is missing. Medium effort: needs C++ WorkspaceManager, JS dropdown UI, save/restore logic, and plugin notification protocol.

### Rank 3: Candidate 8 — .cluicheproj State Extension (score: 3.65)

**Why:** Elegantly minimal — just make the existing `editor_state` block do what it already claims to. No new file formats or concepts. The `.cluicheproj.user` sidecar pattern is well-understood (Unity `.meta`, VS `.user` files). Excellent Cluiche fit since it doubles down on SED-014 (`.cluicheproj` as top-level file).

**Watch out for:** Couples editor state to the project file — if you share `.cluicheproj` via version control, layout/plugin state goes with it (may not be desired). The `.user` sidecar mitigates this but adds another file to manage. Doesn't solve switching between configurations within the same project.

## Recommendation

**Candidate 1 (Auto-Save Session)** wins on total score and is the clearest first step. It's cheap, risk-free, follows PD-009, and directly extends the existing `RestoreLayoutPlugins` pattern without new concepts. Critically, it doesn't foreclose any future direction — if named profiles (Candidate 2) are needed later, the auto-save mechanism becomes the "save current state" half of the switch operation.

The pragmatic path is: **ship Candidate 1 first** (get restore-on-reopen working), then evaluate whether named switching is actually needed in practice. If it is, Candidate 2 layers on top naturally. Candidate 4 (Manifest Overlays) is worth noting as the "zero new abstractions" alternative to Candidate 2 if switching is needed but you want to avoid a WorkspaceManager class.

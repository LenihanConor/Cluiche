# Research: Evaluate — CMake Migration & Dia Architecture

**Input:** docs/research/migrat_cmake/ideate.md

## Scoring Criteria

- **Engine Value (0.25):** Improves Dia module reusability, enforceability, or long-term maintainability
- **Game Value (0.20):** Improves CluicheTest/CluicheEditor development velocity or unlocks blocked features
- **Implementation Cost (0.25):** Inverse of effort — 5 = very cheap, 1 = very expensive
- **Risk (0.15):** Inverse of uncertainty — 5 = well-understood, 1 = highly uncertain
- **Cluiche Fit (0.15):** Aligns with module structure, PD decisions, and existing workflow

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|-----------|---------------|-------------|-------------|-------------|------------|-------|
| C1: Architecture audit tool | 3 | 2 | 5 | 5 | 5 | **3.75** |
| C2: Foundation CMake pilot | 4 | 3 | 4 | 4 | 4 | **3.80** |
| C3: Layered INTERFACE model | 5 | 3 | 2 | 3 | 4 | **3.50** |
| C4: YAML → CMake generator | 3 | 1 | 5 | 4 | 5 | **3.40** |
| C5: Full migration / Open Folder | 5 | 4 | 1 | 2 | 3 | **3.10** |
| C6: Dual-track analysis overlay | 3 | 3 | 3 | 3 | 3 | **3.00** |
| C7: Layer formalisation in YAML | 4 | 2 | 5 | 5 | 5 | **3.85** |

## Top 3 Candidates

### Rank 1: C7 — Layer Formalisation in YAML (score: 3.85)
**Why:** Zero build-system risk, maximum architecture signal. Adding a `layer:` field to all 55+ module docs formalises the intended architecture without touching any `.vcxproj`. The Python audit tool (C1) can immediately use it to enforce ordering — a `foundation` module depending on a `tooling` module becomes a CI failure. This work is also the prerequisite metadata for C4 (generator) and C3 (INTERFACE model), so it pays off in every subsequent step. It aligns perfectly with the existing YAML-first module documentation culture.

**Watch out for:** The `layer` assignments will surface disagreements about where some modules belong — DiaObservation could reasonably be `foundation` or `engine_services`. These are real architecture decisions that need user confirmation, not just mechanical labelling.

---

### Rank 2: C2 — Foundation Layer CMake Pilot (score: 3.80)
**Why:** Adding `CMakeLists.txt` for DiaCore, DiaMaths, and DiaGeometry2D is low-risk because these modules have zero Dia dependencies — no forbidden-dep violations possible. It immediately yields `compile_commands.json` for the highest-value Clang-Tidy targets. The VS workflow is unchanged (`.vcxproj` stays). It proves the CMake pattern, validates `CMakePresets.json` layout, and uncovers real friction before committing to 55 modules. DiaCore is the most-included module in the project — getting Clang-Tidy and clangd working on it has outsized value.

**Watch out for:** The Ninja preset for `compile_commands.json` must be kept passing as a CI check, otherwise it silently drifts. Someone must own the "add file → update both `.vcxproj` and `CMakeLists.txt`" discipline during the pilot period.

---

### Rank 3: C1 — Architecture Audit Tool (score: 3.75)
**Why:** A Python `#include` graph parser against the YAML `dependencies.forbidden` list is a small script that produces immediate, actionable output: which modules have violations today. This report is required before any CMake migration — you cannot structure a layered CMake tree if you don't know the current violation state. It also becomes a permanent CI gate that prevents future regressions independent of the build system. The implementation cost is very low.

**Watch out for:** The audit tool's false positive rate matters — if it flags too many false violations (e.g. conditional includes, platform-specific paths) the team will learn to ignore it. Needs careful design around system headers and `External/` includes.

---

## Recommendation

**Start with C7 + C1 together, then C2.** The layered YAML formalisation (C7) and the architecture audit tool (C1) are both S-sized, have zero build-system risk, and together produce a documented + CI-enforced architecture model by end of week. C7 answers the question "what should our architecture look like?" C1 answers "what violations exist today?" Armed with both, the Foundation CMake pilot (C2) can be designed with full knowledge of the actual dependency state — not guessed.

The sequencing C7 → C1 → C2 maps cleanly to feature specs under a new `DiaArchitecture` system. C3 (layered INTERFACE model) follows naturally from C2 once the foundation is proven; C4 (generator) accelerates C3 if the full migration is chosen. C5 (full cutover) is the endgame but is not urgent — the value of enforced architecture can be captured at C3 without retiring the `.sln`.

C6 (dual-track) is the worst long-term option — it delivers the same Clang-Tidy benefit as C2 but with ongoing dual-maintenance cost and no architecture enforcement upside. Skip it.

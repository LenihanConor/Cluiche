# Research: Evaluate — Google Test Suite Speed

**Input:** docs/research/google_test_suite_speed/ideate.md
**Scope:** Quick wins + parallelism path — C1, C2, C3, C4, C6 only.

## Scoring Criteria

- **Engine Value (0.25):** Improves Dia module reusability or long-term engine capability
- **Game Value (0.20):** Improves CluicheTest as a demo or testbed (proxy: improves dev velocity)
- **Implementation Cost (0.25):** Inverse of effort — 5 = very cheap, 1 = very expensive
- **Risk (0.15):** Inverse of uncertainty — 5 = well-understood, 1 = highly uncertain
- **Cluiche Fit (0.15):** Aligns with module structure and PD decisions

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|-----------|---------------|-------------|-------------|-------------|------------|-------|
| C1 — PCH for gtest.h | 2 | 3 | 5 | 5 | 4 | **3.70** |
| C2 — Fast/Slow tag + DiaCLI filter | 2 | 5 | 5 | 4 | 5 | **4.05** |
| C3 — Shard runner in DiaCLI | 3 | 5 | 3 | 3 | 4 | **3.55** |
| C4 — Release config for tests | 2 | 4 | 5 | 5 | 4 | **3.80** |
| C6 — Fixture amortisation (SetUpTestSuite) | 3 | 4 | 3 | 4 | 4 | **3.55** |

## Top 3 Candidates

### Rank 1: C2 — Fast/Slow Tag Convention + DiaCLI Default Filter (4.05)
**Why:** The highest-impact change with the lowest effort. Changing the default `dia run googletest`
to filter out slow suites immediately gives every developer a sub-2-minute run with no build changes
and no .vcxproj maintenance. It also creates the labelling infrastructure that makes C3 and C7
more effective later. Aligns well with PD-009 (DiaCLI as the canonical runner entry point) and
the existing `--filter` flag already wired through.
**Watch out for:** The slow-test audit is the real work — someone needs to identify which suites
in RigidBody2D, SoftBody2D, Python, WebSocket, and Animation2D are the actual offenders. If
the slow prefix isn't applied consistently, the default filter gives false confidence.

---

### Rank 2: C4 — Release-with-Debug-Info Config (3.80)
**Why:** Zero code changes — just a documented CLI flag (`--config Release`) or a CI default
change. Physics, math, and geometry simulation loops are O2-sensitive; this is likely a 3–5x
runtime reduction for the heaviest test folders. The existing Release|x64 config is already in
the .vcxproj. Risk is minimal — ASAN/UBSan configs are separate and unaffected.
**Watch out for:** Release builds can mask bugs that only surface under Debug (uninitialised
variable reads, iterator invalidation). Should remain opt-in for local dev; make it the CI
default for the full-suite run, not the debug-loop run.

---

### Rank 3: C1 — Precompiled Header for gtest.h (3.70)
**Why:** Pure compile-time win with no correctness risk. With 404 files all parsing gtest.h on
every incremental build, a single PCH saves meaningful time per edit-compile cycle. MSVC PCH is
well-understood and the config belongs in Directory.Build.props per PD-008, keeping it off the
per-project files.
**Watch out for:** PCH setup requires care to include headers that are stable across all test
files. If a PCH header changes (unlikely for gtest.h), all 404 files recompile. Should cover
only gtest.h and a small set of universally-included Dia headers.

## Recommendation

**C2 (tag + filter) should be the first thing implemented.** It costs one DiaCLI change and a
naming audit, delivers the biggest daily-loop improvement immediately, and provides the category
structure that makes everything else more effective. C4 follows as a zero-code CI config change.
C1 closes out the compile-side savings. C3 (sharding) and C6 (fixture amortisation) are solid
M-sized follow-ons once the S items are shipped — together the full set should cut the 5-minute
run to well under 90 seconds for the daily fast path and under 2 minutes for the full CI run on
a multi-core machine, all without touching a single .vcxproj structurally.

C3 scores lower than C1/C4 not because it's less valuable, but because it requires new Python
multiprocessing + XML merge work in DiaCLI and carries more uncertainty around shard-ordering
and flaky test interactions. It's the right next step after the S items are proven.

# Research Summary — Google Test Suite Speed

**Session folder:** docs/research/google_test_suite_speed/
**Date:** 2026-06-06

## One-Line Answer

Cut the 5-minute GoogleTests run to under 90 seconds by tagging slow suites, running tests in
Release config, adding a PCH, sharding across cores, and amortising expensive fixture setup.

## Journey

1. **Explored:** The suite has grown to 404 test files across 59 folders in a single binary linking
   60+ Dia libraries; slow runtime, no parallelism, and repeated compile cost are the three root causes.
2. **Ideated:** 8 candidates generated spanning S–XL, covering compile time, runtime categorisation,
   parallelism, build restructuring, and fixture-level optimisation.
3. **Evaluated:** C2 (tag + filter) scored highest (4.05); C4 (Release config) and C1 (PCH) are
   near-free follow-ons; C3 (sharding) and C6 (fixture amortisation) are solid M-sized complements.
4. **Chose:** Full Quick wins + parallelism bundle (C1+C2+C3+C4+C6) as a single system spec;
   heavier restructuring options (C5, C7, C8) deferred or ruled out.

## Chosen Work Item

**Name:** GoogleTests Speed — Quick Wins + Parallelism
**Home module:** GoogleTests.vcxproj + DiaCLI test runner
**Suggested spec type:** System
**Estimated size:** M (3×S + 2×M tasks)

## Key Insights from Exploration

- **C2 is the keystone.** The tag convention (slow suite prefix + DiaCLI default filter) is both the
  cheapest change and the one everything else builds on — sharding works better with categorised
  suites; dirty tracking (future C7) needs categories to be useful.
- **Release config is a free win.** `dia run googletest --config Release` already works today.
  Physics/math/geometry tests are O2-sensitive; 3–5× runtime reduction for those folders costs zero
  code changes. Should become the CI full-suite default.
- **PCH scope must stay narrow.** gtest.h + a small set of universally-included Dia headers only.
  A PCH that includes unstable headers causes all 404 files to recompile on any header change.
- **Python `Py_Initialize` is the biggest single fixture offender.** Every TEST_F that calls it per
  test-case adds ~200ms. A single `SetUpTestSuite` interpreter shared across the Python/ suite
  is the highest-value C6 target.
- **C8 (per-module binaries) is the right long-term answer but wrong now.** PD-006 requires
  hand-maintained .vcxproj files; with 20+ modules that's significant ongoing cost. Revisit when
  the module count stabilises or `dia scaffold` can generate test projects automatically.
- **ASAN/UBSan configs are sacrosanct.** All five candidates must leave the sanitiser configs
  untouched — they catch real bugs and are already wired into CI.

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| C5 — Slow-suite quarantine binary | Superseded by C2+C3; adds .vcxproj maintenance without proportional gain |
| C7 — Dirty-module tracking | L-sized complexity; deferred until C2 labelling is in place |
| C8 — Per-module binaries | XL; PD-006 maintenance cost too high at current module count |

## References

- docs/research/google_test_suite_speed/explore.md
- docs/research/google_test_suite_speed/ideate.md
- docs/research/google_test_suite_speed/evaluate.md
- docs/research/google_test_suite_speed/choose.md

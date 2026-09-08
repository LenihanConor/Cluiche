# Research: Choice — Google Test Suite Speed

**Date:** 2026-06-06
**Chosen candidate:** Full bundle — C1 + C2 + C3 + C4 + C6 (Quick wins + parallelism)

## Rationale

All five candidates target the same system (GoogleTests binary + DiaCLI runner) and are orthogonal
to each other — they compound without conflict. The three S-sized items (C1, C2, C4) deliver
immediate wins with minimal risk; the two M-sized items (C3, C6) follow once the quick wins are
proven. Implementing them as a single system spec avoids five separate spec ceremonies for
changes that are tightly coupled in purpose.

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|------------------|
| C5 — Slow-suite quarantine binary | Superseded by C2 (tag convention) + C3 (sharding); two-binary split adds .vcxproj maintenance cost without proportional gain |
| C7 — DiaCLI dirty-module tracking | L-sized; deferred — C2 + C3 cover the daily-loop problem without the complexity of a dep-graph cache |
| C8 — Per-module test binaries | XL-sized; violates PD-006 spirit (hand-maintained .vcxproj per module); C3 delivers most of the parallelism benefit without the restructuring |

## Pre-Spec Commitments

- Implementation order: C2 → C4 → C1 → C3 → C6 (cheapest/highest-impact first)
- C2 naming convention must be applied to existing slow suites in RigidBody2D, SoftBody2D, Python,
  WebSocket, and Animation2D folders — not just new tests
- C4 is a documentation + CI config change only; Release config must remain opt-in for local dev
- C3 shard runner lives entirely in DiaCLI Python; zero changes to .vcxproj files
- C6 targets Python/ and WebSocket/ fixtures first (highest setup cost per test)
- ASAN and UBSan configs must remain unaffected by all changes

## Next Step

Run /spec-system with this bundle as input.
Suggested parent: GoogleTests application spec + DiaCLI

# Research: Choice — Animation & Rig Layer

**Date:** 2026-04-26
**Chosen candidate:** Full 9-candidate set as a layered system (DiaRig / DiaIK / DiaAnimation + game driver)

## Rationale

All 9 candidates are complementary building blocks of a single coherent animation layer — not competing alternatives. The research process confirmed the architecture decomposition rather than selecting one winner. The chosen direction is:

```
DiaRig        — skeleton data, bone hierarchy, rest pose, sockets, debug renderer
DiaIK         — two-bone analytical, FABRIK, look-at constraint (pure math)
DiaAnimation  — damped spring chain, keyframe clip player, locomotion oscillator, pose blend stack
[game code]   — DragonAnimDriver (reads AI intent / genetics / world → drives DiaAnimation)
```

This matches the COW animation spec recommendation (Option C: hybrid procedural 2D rig) and the user's stated constraints: simple, procedural-first, AI-generated animation input, industry-standard formats, heavy testing.

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|------------------|
| Full keyframe pipeline only | Content cost explodes with genetic variance; no animator on team |
| Fully procedural (no clips) | Too risky for stylised signature moments (takeoff, roar) |
| Single monolithic module | Untestable as a unit; IK solvers need isolation to test exhaustively |
| DiaSoftBody2D Rope as animation secondary motion | PBD Rope is the physics layer tool for tail/terrain collision; Damped Spring Chain is the animation layer tool for visual follow-through — both needed, different jobs |

## Build Waves

| Wave | Candidates | Modules | Gate |
|------|-----------|---------|------|
| 1 — Foundations | Minimal Flat Skeleton, Two-Algorithm IK, Look-At Constraint, Skeleton Debug Renderer | DiaRig + DiaIK | All Wave 2+ blocked until this is tested and passing |
| 2 — Data + Motion Basics | Spine2D JSON Loader, Damped Spring Chain, Keyframe Clip Player | DiaRig + DiaAnimation | Wave 1 done |
| 3 — Locomotion | Procedural Locomotion Oscillator | DiaAnimation | Wave 2 done; oscillator needs its own research session before speccing (gait-feel iteration risk) |
| 4 — Integration | Pose Blend Stack | DiaAnimation | Wave 3 done |

## Pre-Spec Commitments

- **PD-001:** All bone names use StringCRC — no raw string maps anywhere in any module
- **PD-004:** All public API arrays use DiaCore containers (DynamicArrayC etc.) — no STL vectors in headers
- **PD-007:** C++20 throughout — use std::span for clip data access, concepts to constrain IK solver templates where appropriate
- **DR-014 fix:** Skeleton must implement dirty-flagged cached world transforms — this resolves the known GetWorldMatrix() performance issue as a side effect of building Candidate 1
- **Debug gating:** All SkeletonDebugRenderer code behind `#ifdef DIA_DEBUG` — must not regress frame time in Release builds
- **Spine2D format:** Industry-standard Spine2D v4 JSON is the canonical AI-generated animation input format; loaded via DiaCore/Json (jsoncpp), no new parser dependency
- **Secondary motion split:** Damped Spring Chain (DiaAnimation) for visual follow-through on frills/sails/small appendages; DiaSoftBody2D Rope for physically-accurate tail collision with terrain — both needed, not interchangeable
- **Oscillator deferred:** Candidate 6 (Procedural Locomotion Oscillator) requires its own research session before speccing — gait feel requires iteration and the algorithm has more uncertainty than the rest of the set
- **Testing approach:** All IK solvers tested with canonical fixed inputs (known skeleton, known target → assert exact output angles); spring chain tested with fixed dt and known stiffness/damping → assert exact angle sequence; no visual-only validation

## Next Step

Run `/spec-system` for the animation layer system, covering all three modules.
Suggested system name: **Dia Animation Layer**
Suggested parent application spec: `docs/specs/applications/dia.md`
Attach `summary.md` as context.

Wave 1 features to spec immediately:
1. `/spec-feature` — DiaRig: Minimal Flat Skeleton
2. `/spec-feature` — DiaRig: Skeleton Debug Renderer
3. `/spec-feature` — DiaIK: Two-Algorithm IK (TwoBone + FABRIK)
4. `/spec-feature` — DiaIK: Look-At Constraint

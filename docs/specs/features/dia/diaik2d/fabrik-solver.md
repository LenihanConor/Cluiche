# Feature Spec: FABRIK Solver

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | — |
| Application | @docs/specs/applications/dia.md | — |
| System | @docs/specs/systems/dia/diaik2d.md | **fabrik-solver** |

**Status:** `Approved`

---

## Problem Statement

Spines, tails, tentacles, and multi-segment chains need IK that works for any joint count and produces natural-looking results without the base-heavy twisting of CCD. FABRIK (Forward And Backward Reaching IK) converges quickly (10–20 iterations for most game chains), is straightforward to implement, and handles joint angle limits well via per-iteration projection.

---

## Solution Overview

`IKSolver::SolveFABRIK()` implements the FABRIK iterative solver:

1. Snapshot local rotations of all bones in the chain — for reach weight blending later.
2. Extract world-space positions of all chain joints from the skeleton's current FK state.
3. Check reachability: if total chain length < distance(root, target), fully extend toward target and return.
4. Iterate up to `IKChainDef::maxIterations`:
   - **Backward pass** (end → root): move end effector to target; for each bone from end to start, position it along the direction from next joint, at `bone.length` distance.
   - **Forward pass** (root → end): pin root bone world position; for each bone from start to end, position it along the direction from previous joint, at `bone.length` distance.
   - **Apply joint angle limits** per joint after each pass: convert world-space joint positions back to local angles, clamp to `[minAngle, maxAngle]`, recompute downstream positions (SD-005).
   - Check convergence: if distance(end effector, target) < `IKChainDef::tolerance`, break early.
5. If not converged after `maxIterations`, emit `DIA_LOG_WARNING("Rig2D", ...)` in debug; use best-effort result.
6. Convert solved world-space joint positions back to local bone rotations.
7. Apply reach weight: lerp each bone's local rotation between pre-solve snapshot and FABRIK result (shortest-arc, SD-003).
8. Write modified local rotations back into `Pose` via `Pose::GetLocalTransform(i).rotation` (SD-013).
9. Re-propagate FK from the start bone to end of the skeleton using the stored root transform (SD-002).

The root bone's world-space position is pinned throughout (both passes preserve it). This is standard FABRIK — allowing root drift would break character grounding.

---

## Acceptance Criteria

1. `SolveFABRIK(chainId, target)` returns `true` always (even on non-convergence). Returns `false` only if `chainId` is not registered.
2. For a reachable target on a 3-joint chain (4 bones), end effector reaches within `tolerance` of target in ≤ `maxIterations` iterations under typical conditions (no tight joint limits).
3. For an unreachable target (total chain length < distance to target), all bones are aligned toward target. No assert; returns `true`.
4. Root bone world-space position is unchanged after the solve (pinned).
5. Joint limits applied per iteration: a joint clamped to `[0°, 90°]` cannot exceed 90° in the final pose.
6. Non-convergence emits `DIA_LOG_WARNING` in debug builds only (inside `#ifndef NDEBUG`). Does not assert.
7. Reach weight `0.0` leaves all bone rotations unchanged. Reach weight `1.0` applies full FABRIK result. Intermediate values lerp with shortest-arc interpolation.
8. FK propagated from chain start bone to end of skeleton after writing rotations.
9. No heap allocation during solve (all working arrays pre-allocated at `IKSolver` construction or on stack for chains ≤ max supported length).
10. Works for chains with 1 joint (2 bones) and up to the skeleton's bone count.

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | `chainId` parameter is `StringCRC`. ✅ |
| PD-007 | Platform | C++20 required | Implementation compiled under `/std:c++20`. ✅ |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | Implementation in `Dia::IK2D::`. ✅ |
| RD-002 | DiaRig2D | 2D only: scalar rotation, Vector2D | FABRIK operates on Vector2D joint positions and scalar bone rotations. ✅ |
| RD-010 | DiaRig2D | SRT transform concatenation order | FK re-propagation uses SRT order. ✅ |
| RD-012 | DiaRig2D | Bone length stored on Bone | Solver reads `Bone::length` per bone — no recomputation. ✅ |
| SD-001 | DiaIK2D | Post-process: reads/writes `Pose` local rotations in-place | Reads `Pose` local rotations and IKSolver's world-transform cache; writes modified rotations back to `Pose`. `Skeleton` is never mutated. ✅ |
| SD-013 | DiaIK2D | IKSolver writes `Pose`, not `Skeleton` | All rotation writes target `Pose::GetLocalTransform(i).rotation`. ✅ |
| SD-002 | DiaIK2D | Trigger FK propagation after solve | Re-propagates FK from start bone to end of skeleton. ✅ |
| SD-003 | DiaIK2D | Reach weight blends pre-solve FK and IK result | Snapshots before, lerps after with shortest-arc. ✅ |
| SD-005 | DiaIK2D | Joint limits applied during solver iterations | Limits projected per iteration in both passes. ✅ |
| SD-006 | DiaIK2D | FABRIK over CCD | Implemented as FABRIK. ✅ |
| SD-009 | DiaIK2D | Bone IDs resolved at RegisterChain time | Works with pre-resolved bone indices. ✅ |
| SD-010 | DiaIK2D | No STL in public APIs | No STL in method signatures. ✅ |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Working arrays | FABRIK needs a temporary world-position array per solve call. How is this allocated? | Pre-allocated in `IKSolver` as a `DynamicArrayC<Vector2D>` sized to `skeleton.GetBoneCount()` at construction. Reused across solve calls. No per-call heap allocation. |
| 2 | Joint limits in FABRIK | Applying joint limits mid-iteration requires converting FABRIK's world-position updates to local angles. Is this expensive? | Yes, it adds a position→angle conversion per joint per iteration. For typical 2D chains (< 20 joints, 20 iterations), this is ~400 atan2 calls per frame — acceptable. Document as a performance note. Callers with very long chains and tight limits should set `maxIterations` conservatively. |
| 3 | Reach weight and limits | Does reach weight interact correctly with joint limits? | Yes: pre-solve snapshot is taken before limits. IK result (blend source) was produced under limit constraints. FK result (the other blend source) has no guarantees about limits — that is the caller's responsibility. |
| 4 | Single-joint chain | Is FABRIK valid for a 1-joint chain (2 bones)? | Degenerate but valid. The backward pass moves the end to target, the forward pass pins the root and extends toward the end. Result is equivalent to Look-At for a single bone. `SolveFABRIK` handles it without special-casing. |
| 5 | Non-convergence tolerance | `tolerance` compares end-effector distance to target. Should it also check velocity (change between iterations)? | Distance-only tolerance is standard and sufficient. Velocity check would catch cycles (oscillation near limits) but adds complexity. Document that very tight joint limits may cause oscillation without convergence — callers should cap `maxIterations` in those cases. |

---

## Status

`Approved`

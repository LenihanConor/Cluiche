# Feature Spec: Two-Bone Solver

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | — |
| Application | @docs/specs/applications/dia.md | — |
| System | @docs/specs/systems/dia/diaik2d.md | **two-bone-solver** |

**Status:** `Approved`

---

## Problem Statement

Limb animation (arms, legs) requires a precise, fast IK solve for exactly 2-joint chains (shoulder→elbow→wrist, hip→knee→ankle). Iterative solvers converge slowly for this case and can jitter near singular configurations. A closed-form analytic solution gives an exact, stable result every frame with no iteration, and a pole vector controls which way the elbow or knee bends.

---

## Solution Overview

`IKSolver::SolveTwoBone()` implements the analytic two-bone IK solve:

1. Snapshot local rotations of the 3 bones (start, middle, end) before solving — for reach weight blending later.
2. Read bone lengths from `Skeleton::GetBone()` (stored at skeleton construction per RD-012).
3. Compute the angle at the middle joint using the law of cosines: `cos(θ) = (l1² + l2² − d²) / (2·l1·l2)`, where `d` = distance from start bone to target.
4. If `d ≥ l1 + l2` (unreachable): fully extend both bones toward the target (SD-007). Apply pole vector for bend orientation.
5. Compute the angle at the start joint using atan2 to orient the chain at the target, adjusted by the pole vector.
6. Apply joint angle limits by clamping computed angles to `[minAngle, maxAngle]` for each joint (SD-005).
7. Apply reach weight: lerp each bone's local rotation between the pre-solve snapshot and the IK result (SD-003). Shortest-arc angle lerp to avoid wrap-around artefacts.
8. Write modified local rotations back into `Pose` via `Pose::GetLocalTransform(i).rotation` (SD-013).
9. Re-propagate FK from start bone to end of skeleton using the stored root transform (SD-002).

The pole vector biases the bend direction: the middle joint is pushed toward the world-space point `startBoneWorldPos + poleVector.direction`. Its `weight` is applied as a lerp between no-pole orientation and full-pole orientation.

---

## Acceptance Criteria

1. `SolveTwoBone(chainId, target, poleVector)` returns `true` on success, `false` if chain not found or chain does not span exactly 2 joints (3 bones). `DIA_ASSERT` in debug for wrong joint count.
2. Given a reachable target, the end effector position (world-space position of end bone) matches `target` within `1e-4` world units after FK propagation.
3. Given an unreachable target (`d ≥ l1 + l2`), all bones are aligned toward the target (full extension). No assert; returns `true`.
4. Pole vector = `nullptr` falls back to current chain orientation (no bend bias change).
5. Joint limits: if the middle joint limit is `[0, π]`, the elbow cannot fold past straight or bend past 180°. DIA_ASSERT if limit min > max.
6. Reach weight `0.0` leaves all bone rotations unchanged. Reach weight `1.0` applies the full IK result. Values in between lerp linearly with shortest-arc angle interpolation.
7. FK is propagated from the chain's start bone to end bone (inclusive) after the solve writes bone rotations.
8. The solve reads bone lengths from `Skeleton::GetBone(index).length` — no recomputation per frame.
9. No heap allocation during solve.

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | `chainId` parameter is `StringCRC`. ✅ |
| PD-007 | Platform | C++20 required | Implementation compiled under `/std:c++20`. ✅ |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | Implementation in `Dia::IK2D::`. ✅ |
| RD-002 | DiaRig2D | 2D only: scalar rotation (radians), Vector2D position/scale | Solver works with scalar rotation and Vector2D; no 3D math. ✅ |
| RD-010 | DiaRig2D | SRT transform concatenation order | FK re-propagation after solve uses DiaRig2D's own FK path (or replicates SRT order). ✅ |
| RD-012 | DiaRig2D | Bone length stored explicitly on Bone | Solver reads `Bone::length` — no recomputation. ✅ |
| SD-001 | DiaIK2D | Post-process: reads/writes `Pose` local rotations in-place | Reads `Pose` local rotations, writes modified rotations back to `Pose`. `Skeleton` is never mutated. ✅ |
| SD-013 | DiaIK2D | IKSolver writes `Pose`, not `Skeleton` | All rotation writes target `Pose::GetLocalTransform(i).rotation`. ✅ |
| SD-002 | DiaIK2D | Trigger FK propagation on modified bone range after each solve | Re-propagates FK from start to end bone after writing rotations. ✅ |
| SD-003 | DiaIK2D | Reach weight per chain blends pre-solve FK and IK result | Snapshots before, lerps after. ✅ |
| SD-004 | DiaIK2D | Pole vector is a per-call argument, not persisted state | `poleVector` is a const pointer parameter on `SolveTwoBone`. ✅ |
| SD-005 | DiaIK2D | Joint limits applied during solver iterations | Computed angles clamped before writing back. ✅ |
| SD-007 | DiaIK2D | Unreachable targets: fully extend chain | Handled explicitly. ✅ |
| SD-009 | DiaIK2D | Bone IDs resolved at RegisterChain time | Solver works with pre-resolved bone indices from `IKSolver` internals. ✅ |
| SD-010 | DiaIK2D | No STL in public APIs | No STL in method signatures or data returned. ✅ |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Singular config | What happens when start bone and target are nearly coincident (d ≈ 0)? | The chain folds to minimum length. The angle at the start joint is ill-defined — use the current start bone orientation as fallback. DIA_LOG_WARNING in debug if `d < 1e-5`. |
| 2 | Pole vector normalisation | Should the solver normalise the pole vector direction, or DIA_ASSERT it's already unit-length? | Normalise defensively inside the solver. Cost is one `sqrt` per solve call — negligible. No assert needed since pole vectors often come from raw world-space offsets. |
| 3 | Reach weight lerp | Shortest-arc angle lerp across `[-π, π]` — which utility handles this? | Use `Dia::Maths::ShortestArcLerp(float a, float b, float t)` if it exists, otherwise implement inline as `a + t * atan2(sin(b-a), cos(b-a))`. Document the utility dependency. |
| 4 | Joint count check | Should `SolveTwoBone` accept a chain of any length registered under a given ID, or is the 2-joint check purely runtime? | Runtime check in `SolveTwoBone`. `DIA_ASSERT` + return `false` if the chain doesn't span exactly 2 joints. Callers are responsible for registering the right chain with the right solver. |
| 5 | FK re-propagation scope | Should FK propagation extend past the chain's end bone (to children of the end bone)? | Yes — children of the end bone depend on the end bone's world transform. FK must propagate to the end of the skeleton's topological array starting from the start bone index, not just the chain's range. Actually: propagate from start bone index to end of skeleton (full downstream). |

---

## Status

`Approved`

# Feature Spec: Look-At Constraint

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | — |
| Application | @docs/specs/applications/dia.md | — |
| System | @docs/specs/systems/dia/diaik2d.md | **look-at-constraint** |

**Status:** `Approved`

---

## Problem Statement

Many game objects need a bone to track a world-space target each frame — eyes following a character, a gun barrel pointing at a cursor, a head turning toward a sound source, a turret aiming at an enemy. This is a degenerate IK case (single bone, single target) that does not require a chain registration or iterative solver, but needs the same reach weight blend and axis offset support as the full solvers.

---

## Solution Overview

`IKSolver::SolveLookAt()` rotates a single bone so its forward axis points at a 2D world-space target:

1. Snapshot the bone's local rotation before solving — for reach weight blending.
2. Compute the bone's current world-space position from the skeleton's FK state.
3. Compute `angle = atan2(target.y - boneWorldPos.y, target.x - boneWorldPos.x)`.
4. Apply `axisAngleOffset`: subtract from `angle` to correct for bones whose bind-pose forward is not +X. For example, a bone drawn pointing up (+Y) needs `axisAngleOffset = π/2`.
5. Convert the world-space angle to a local rotation relative to the parent bone's world rotation.
6. Apply reach weight: lerp between pre-solve local rotation and the IK local rotation using shortest-arc interpolation.
7. Write the result back to the bone's local rotation in `Pose` via `Pose::GetLocalTransform(i).rotation` (SD-013).
8. Re-propagate FK from the modified bone to end of skeleton using the stored root transform (SD-002).

No chain registration required. Bone lookup is by `StringCRC` at call time — re-resolved each call via `IKSolver`'s internal bone index cache (pre-resolved at first use, or passed as a cached index by advanced callers).

---

## Acceptance Criteria

1. `SolveLookAt(boneId, target, weight, axisAngleOffset)` returns `true` on success, `false` if `boneId` is not found in the skeleton.
2. Given a target directly to the right (+X) of the bone and `axisAngleOffset = 0`, the bone's local rotation after solve points the +X axis at the target (within `1e-4` radians).
3. Given `axisAngleOffset = π/2`, a bone that was drawn pointing up (+Y) points its +Y axis at the target.
4. Reach weight `0.0` leaves the bone rotation unchanged. Reach weight `1.0` applies full look-at. Intermediate values lerp with shortest-arc angle interpolation.
5. FK is propagated from the modified bone to the end of the skeleton after the solve.
6. No chain registration needed — `SolveLookAt` takes `boneId` directly.
7. No heap allocation during solve.
8. Bone not found in skeleton: returns `false`, `DIA_ASSERT` in debug, no mutation of skeleton.

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | `boneId` parameter is `StringCRC`. ✅ |
| PD-007 | Platform | C++20 required | Implementation compiled under `/std:c++20`. ✅ |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | Implementation in `Dia::IK2D::`. ✅ |
| RD-002 | DiaRig2D | 2D only: scalar rotation, Vector2D | Operates on scalar rotation and Vector2D. ✅ |
| RD-010 | DiaRig2D | SRT transform concatenation order | FK re-propagation respects SRT order. ✅ |
| SD-001 | DiaIK2D | Post-process: reads/writes `Pose` local rotations in-place | Reads bone world position from IKSolver's world-transform cache; writes modified rotation to `Pose`. `Skeleton` is never mutated. ✅ |
| SD-013 | DiaIK2D | IKSolver writes `Pose`, not `Skeleton` | Write targets `Pose::GetLocalTransform(i).rotation`. ✅ |
| SD-002 | DiaIK2D | Trigger FK propagation after solve | Re-propagates FK from modified bone to end of skeleton. ✅ |
| SD-003 | DiaIK2D | Reach weight blends pre-solve FK and IK result | Snapshots before, lerps with shortest-arc. ✅ |
| SD-009 | DiaIK2D | Bone IDs resolved at registration time | Look-At has no chain registration; bone ID resolved at call time via `IKSolver`'s skeleton reference. Not a registration-based solver — lookup is by ID per call. Acceptable for single-bone operations. ✅ |
| SD-010 | DiaIK2D | No STL in public APIs | No STL in method signatures. ✅ |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | No chain registration | Look-At bypasses chain registration. How is the bone found at call time? | `IKSolver` holds a reference to the `Skeleton`. `SolveLookAt` calls `skeleton.FindBoneIndex(boneId)` at call time (O(n) scan). For high-frequency look-at (called every frame for many bones), callers should cache the index and use an overload that takes `int boneIndex` directly. Provide both overloads. |
| 2 | Parent world rotation | Computing local rotation requires knowing the parent bone's world rotation. How is this obtained? | From the FK world transform output. `IKSolver` maintains (or re-runs FK to obtain) world transforms. For Look-At, run partial FK up to the parent bone only if world transforms are not already current. Document that callers should run full FK before any `IKSolver` solve call. |
| 3 | Target coincident with bone | What if the target is at the same world position as the bone (`d ≈ 0`)? | Direction is undefined. Keep the current bone rotation unchanged (do not modify) and emit `DIA_LOG_WARNING("Rig2D", ...)` in debug. Equivalent to reach weight = 0 for that call. |
| 4 | axisAngleOffset convention | Is `axisAngleOffset` additive or subtractive relative to the computed angle? | Subtractive: `localAngle = computedWorldAngle - axisAngleOffset - parentWorldAngle`. This means "my bind-pose forward is at +offset radians from +X, so subtract to correct". A bone pointing up (+Y at bind) needs `axisAngleOffset = π/2`. Document with worked examples. |
| 5 | int boneIndex overload | Should a second overload `SolveLookAt(int boneIndex, ...)` be provided for callers that cache indices? | Yes — add it as a convenience for high-frequency use. The `StringCRC` overload calls through to the `int` overload after resolving. Both overloads are in scope for this feature. |

---

## Status

`Approved`

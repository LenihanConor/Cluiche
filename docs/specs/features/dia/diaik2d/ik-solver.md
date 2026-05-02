# Feature Spec: IK Solver

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | — |
| Application | @docs/specs/applications/dia.md | — |
| System | @docs/specs/systems/dia/diaik2d.md | **ik-solver** |

**Status:** `Approved`

---

## Problem Statement

The three IK solvers (two-bone, FABRIK, look-at) need a single coordinator that owns the registered chain table, resolves bone IDs at registration time, dispatches to the right solver, manages the pre-solve FK world-transform state, and handles `Rig2D`-channel logging. Without a coordinator, callers would need to manage bone index resolution, FK state, and working arrays themselves.

---

## Solution Overview

`IKSolver` is a non-owning wrapper around a `Skeleton&` and a `Pose&`. It owns:
- A registered chain table (keyed by `StringCRC`, storing resolved bone index ranges and cached `IKChainDef` copies)
- A stored root `BoneTransform` (set by `SetRootTransform` each frame)
- A working world-transform array (pre-allocated to `skeleton.GetBoneCount()`, reused per-frame)
- A working position array for FABRIK (same size)

**Pose vs Skeleton:** `Skeleton` is the immutable definition (bind pose, hierarchy). All IK reads and writes target `Pose` local rotations via `Pose::GetLocalTransform(i).rotation`. `Skeleton` data is never mutated (SD-013).

**Per-frame contract:** Caller must call `SetRootTransform(rootTransform)` once before any `Solve*` calls each frame. `SetRootTransform` stores the root transform and runs a full FK pass via `Pose::ComputeWorldTransforms()` to populate the world-transform cache. All subsequent `Solve*` calls in that frame read from the cache and update it incrementally after each solve (SD-012).

**Registration:** `RegisterChain(IKChainDef)` resolves `startBoneId` and `endBoneId` to indices, validates that start precedes end in topological order, validates joint count requirements, and stores the resolved chain. `DIA_ASSERT` in debug for bones not found or invalid ordering.

**Solve dispatch:** Each `Solve*` call looks up the registered chain, calls the solver implementation, writes back to `Pose`, then re-propagates FK from the modified start bone to end of skeleton — updating the world-transform cache for subsequent solve calls.

**Logging:** `Rig2D`-channel `DIA_LOG_WARNING` for: `SetRootTransform` not called before first solve (debug only), unreachable target (two-bone and FABRIK), FABRIK non-convergence, joint limit violations, bone not found, look-at target coincident with bone. All inside `#ifndef NDEBUG`.

---

## Acceptance Criteria

1. `IKSolver(Skeleton& skeleton, Pose& pose)` construction pre-allocates world-transform and working arrays sized to `skeleton.GetBoneCount()`. No allocation during solve calls.
2. `SetRootTransform(const BoneTransform& rootTransform)` stores the root transform and runs full FK to populate the world-transform cache. Must be called once per frame before any `Solve*` call. `DIA_LOG_WARNING` in debug if a `Solve*` is called without a prior `SetRootTransform` in the current frame.
3. `RegisterChain(IKChainDef)` resolves bone IDs to indices. `DIA_ASSERT` if `startBoneId` not found, `endBoneId` not found, or `startIndex >= endIndex` (topological order violation).
4. `UnregisterChain(StringCRC)` removes the chain. No-op if chain not found (silent).
5. `HasChain(StringCRC)` returns `true` if the chain is registered, `false` otherwise.
6. `SolveTwoBone(chainId, target, poleVector)` returns `false` + `DIA_ASSERT` in debug if chain not found or chain spans != 2 joints.
7. `SolveFABRIK(chainId, target)` returns `false` if chain not found; `true` always otherwise (even non-convergence).
8. `SolveLookAt(StringCRC boneId, target, weight, axisAngleOffset)` and `SolveLookAt(int boneIndex, target, weight, axisAngleOffset)` both supported.
9. After any `Solve*` call, `Pose` local rotations are updated and world transforms are re-propagated from the modified bone range to the end of the skeleton, so subsequent calls within the same frame see correct state.
10. All warning logs emitted via `DIA_LOG_WARNING("Rig2D", ...)` under `#ifndef NDEBUG`.
11. `GetSkeleton()` returns `const Skeleton&`. `GetPose()` returns `const Pose&`.
12. `IKSolver` is moveable, not copyable (stores references — copy would alias).
13. Multiple `IKSolver` instances may wrap the same `Skeleton`/`Pose` — caller is responsible for ordering.

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | `chainId`, `boneId` parameters are `StringCRC`. Registered chain table keyed by `StringCRC`. ✅ |
| PD-004 | Platform | No STL in public APIs | Chain table uses `DiaCore::DynamicArrayC` or `HashTable`; working arrays use `DynamicArrayC`. ✅ |
| PD-007 | Platform | C++20 required | Implementation compiled under `/std:c++20`. ✅ |
| AD-002 | Dia App | No STL in public APIs | Reinforces PD-004. ✅ |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | `IKSolver` in `Dia::IK2D::`. ✅ |
| SD-001 | DiaIK2D | Post-process: reads/writes `Pose` local rotations in-place | `IKSolver` holds `Pose&`; all solvers read/write `Pose`. `Skeleton` is the immutable definition and is never mutated. ✅ |
| SD-012 | DiaIK2D | `SetRootTransform()` called once per frame before solve batch | `IKSolver::SetRootTransform()` runs full FK, populates world-transform cache. All subsequent solve calls use cached transforms. ✅ |
| SD-013 | DiaIK2D | IKSolver writes `Pose`, not `Skeleton` | `RegisterChain`, `Solve*` all target `Pose` for read/write. ✅ |
| SD-002 | DiaIK2D | Trigger FK propagation after each solve | `IKSolver` re-propagates FK after every `Solve*` call. ✅ |
| SD-008 | DiaIK2D | `IKSolver` wraps a single `Skeleton&`, non-owning | Constructor takes `Skeleton&`, stores reference. Skeleton lifetime managed by caller. ✅ |
| SD-009 | DiaIK2D | Bone IDs resolved at RegisterChain time | `RegisterChain` resolves IDs to indices; solve calls use cached indices. ✅ |
| SD-010 | DiaIK2D | No STL in public APIs | No STL in any public method signature. ✅ |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Chain table storage | What container stores registered chains? | `DynamicArrayC<ResolvedChain>` where `ResolvedChain` stores the `IKChainDef` copy plus resolved `startIndex`/`endIndex`. Linear scan by `StringCRC` — typical registration count is < 20 chains, so O(n) is negligible. |
| 2 | FK world transform source | Where does `IKSolver` get bone world transforms for FABRIK and look-at? | `IKSolver` owns `DynamicArrayC<BoneTransform> mWorldTransforms` pre-allocated to `skeleton.GetBoneCount()`. `SetRootTransform()` calls `Pose::ComputeWorldTransforms(skeleton, rootTransform, mWorldTransforms)` to populate it. After each solve, FK is re-propagated to update the cache for downstream solves (SD-012). |
| 3 | Pose access | `IKSolver` reads/writes `Pose` local rotations. `Skeleton` is never mutated. | Confirmed — constructor takes `Skeleton& skeleton` (for bone count, lengths, hierarchy) and `Pose& pose` (for local rotation reads/writes). SD-013 documents this contract. |
| 4 | Multiple IKSolvers on same skeleton | Two `IKSolver` instances wrapping the same skeleton could interleave FK invalidations. Is this documented? | Documented as caller responsibility. Ordering: run all solvers' `Solve*` calls in a defined sequence. No runtime detection of overlap. |
| 5 | Logging channel | `IKSolver` logs to the `Rig2D` channel. Should it use a dedicated `IK` channel instead? | Use the `Rig2D` channel — it's already established and IK is part of the rig system. A separate `IK` channel can be split in a future feature spec if log volume warrants it. |

---

## Status

`Approved`

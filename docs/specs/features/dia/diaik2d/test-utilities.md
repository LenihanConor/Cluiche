# Feature Spec: Test Utilities

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | — |
| Application | @docs/specs/applications/dia.md | — |
| System | @docs/specs/systems/dia/diaik2d.md | **test-utilities** |

**Status:** `Approved`

---

## Problem Statement

Every DiaIK2D test needs a skeleton to operate on. Without shared helpers, each test file would repeat the same boilerplate: build a skeleton, set up bone lengths, create a pose, construct an `IKSolver`. This duplication slows test authoring and makes tests brittle when the `Skeleton` API changes.

---

## Solution Overview

`DiaIK2D/Testing/` contains opt-in header-only helpers (no `.cpp`, not compiled into `DiaIK2D.lib`):

- **`IKTestHelpers.h`** — `BuildLimbSkeleton(int boneCount)` (returns `Skeleton` + `Pose` pair for a straight vertical chain), `BuildTestIKSolver(Skeleton&, Pose&)`, bone rotation assertion helpers
- **`IKAssertions.h`** — `AssertBoneRotation`, `AssertEndEffectorPosition`, `AssertBoneUnchanged`

These are consumed by `#include <DiaIK2D/Testing/IKTestHelpers.h>` in GoogleTest files. They depend on DiaRig2D test utilities (`DiaRig2D/Testing/`) where applicable.

---

## Acceptance Criteria

1. `BuildLimbSkeleton(int boneCount, float boneLength)` returns a `Skeleton` with `boneCount` bones in a straight vertical chain, each bone of length `boneLength`, parent-child in topological order.
2. `BuildTestIKSolver(Skeleton&, Pose&)` returns a fully constructed `IKSolver` ready for chain registration.
3. `AssertBoneRotation(skeleton, pose, boneId, expectedAngle, toleranceRad)` asserts that the named bone's local rotation matches `expectedAngle` within `toleranceRad`. Uses GoogleTest `EXPECT_NEAR`.
4. `AssertEndEffectorPosition(skeleton, pose, rootTransform, boneId, expectedPos, toleranceUnits)` runs FK and asserts world-space end effector position.
5. `AssertBoneUnchanged(skeleton, pose, boneId, snapshotRotation)` asserts rotation equals `snapshotRotation` — used to verify reach weight 0.0 leaves bones untouched.
6. All helpers live in `namespace Dia::IK2D::Testing`.
7. Headers are not compiled into `DiaIK2D.vcxproj` (Testing/ excluded from the library build). GoogleTests.vcxproj includes the `DiaIK2D/Testing/` directory via its include paths.
8. No STL in helper function signatures (uses `DiaCore::StringCRC`, `DiaMaths::Vector2D`, plain floats).

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | `boneId` parameters in assertion helpers use `StringCRC`. ✅ |
| PD-004 | Platform | No STL in public APIs | Helper signatures use DiaCore/DiaMaths types only. ✅ |
| PD-007 | Platform | C++20 required | Headers compiled under `/std:c++20`. ✅ |
| AD-002 | Dia App | No STL in public APIs | Reinforces PD-004. ✅ |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All helpers in `Dia::IK2D::Testing`. ✅ |
| SD-011 | DiaIK2D | Test utilities ship in `DiaIK2D/Testing/` | Implemented exactly as specified. ✅ |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | DiaRig2D dependency | `BuildLimbSkeleton` produces a `Skeleton`. Does it depend on `DiaRig2D/Testing/` helpers? | It can use `DiaRig2D`'s public API directly (`SkeletonDef`, `Skeleton` constructor). No dependency on `DiaRig2D/Testing/` — that keeps DiaIK2D's test utilities self-contained. If DiaRig2D/Testing provides a useful `BuildBone()` helper, import it via include but don't depend on unstable internal helpers. |
| 2 | GoogleTest dependency | Assertion helpers use `EXPECT_NEAR`. Should they ASSERT_ or EXPECT_? | Use `EXPECT_*` (non-fatal) so a single test can report multiple bone failures. Callers that want fatal failure can wrap in `ASSERT_NO_FATAL_FAILURE`. |
| 3 | Bone count for BuildLimbSkeleton | What is the default `boneCount` for `BuildLimbSkeleton`? | No default — require explicit count. Callers should be intentional. Typical values: 3 (two-bone chain), 5 (FABRIK spine). |
| 4 | Pose returned by BuildLimbSkeleton | Should the helper return a `Pose` pre-set to the bind pose? | Yes — return a `{Skeleton, Pose}` pair where `Pose` is initialised to the skeleton's bind pose via `Pose(skeleton)`. Tests can then modify the pose before constructing `IKSolver`. |

---

## Status

`Approved`

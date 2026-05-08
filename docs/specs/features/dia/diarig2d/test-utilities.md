# Feature Spec: Test Utilities

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diarig2d.md | **test-utilities** |

**Status:** `Approved`

---

## Problem Statement

Every DiaRig2D feature needs unit tests that construct skeletons, create poses, and compare results. Without shared test helpers, each test file duplicates skeleton construction boilerplate, uses inconsistent test skeletons, and writes ad-hoc floating-point comparisons. This is the same problem solved by `DiaRigidBody2D/Testing/` and `DiaSoftBody2D/Testing/`.

---

## Solution Overview

Provide test utilities in `Dia/DiaRig2D/Testing/` that ship with the DiaRig2D library (not in GoogleTests). This follows the established pattern: test helpers live in `<Module>/Testing/`, not in the test project (per feedback_testing_utilities memory).

### Skeleton Builders

```cpp
namespace Dia::Rig2D::Testing {
    SkeletonDef MakeSimpleChain(int boneCount);
    SkeletonDef MakeHumanoid();
    SkeletonDef MakeBranching();
}
```

- **MakeSimpleChain(n)**: Linear chain of n bones — root at origin, each child offset along +Y. Useful for FK and IK tests.
- **MakeHumanoid()**: Canonical 2D humanoid skeleton (root, spine, head, 2 arms, 2 legs) — ~10-15 bones. Useful for integration tests and visual debugger testing.
- **MakeBranching()**: Skeleton with branches (one parent with 3+ children) — tests that FK handles branching correctly.

### Pose Comparison Helpers

```cpp
namespace Dia::Rig2D::Testing {
    bool PosesAreEqual(const Pose& a, const Pose& b, float tolerance = 0.001f);
    bool BoneTransformsAreEqual(const BoneTransform& a, const BoneTransform& b, float tolerance = 0.001f);
}
```

Floating-point comparison with configurable tolerance. Rotation comparison uses shortest-arc difference (handles -pi/+pi wrapping).

### JSON Test Fixtures

Pre-written JSON skeleton files in `Cluiche/Tests/GoogleTests/Rig/Fixtures/`:
- `simple_chain_3.json` — 3-bone chain
- `humanoid.json` — canonical humanoid
- `invalid_parent.json` — invalid parent reference (for error testing)
- `missing_position.json` — missing required field (for error testing)

### Files

| File | Purpose |
|------|---------|
| `Dia/DiaRig2D/Testing/SkeletonBuilders.h` | Builder function declarations |
| `Dia/DiaRig2D/Testing/SkeletonBuilders.cpp` | Builder implementations |
| `Dia/DiaRig2D/Testing/PoseComparison.h` | Comparison helper declarations |
| `Dia/DiaRig2D/Testing/PoseComparison.cpp` | Comparison implementations |
| `Cluiche/Tests/GoogleTests/Rig/Fixtures/*.json` | JSON test fixture files |

---

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| 1 | `MakeSimpleChain(n)` returns a valid SkeletonDef with n bones in a chain | Unit test: construct Skeleton from it, IsValid() == true, GetBoneCount() == n |
| 2 | `MakeHumanoid()` returns a valid SkeletonDef with expected bone names (root, spine, head, etc.) | Unit test: construct Skeleton, FindBoneIndex for each expected name |
| 3 | `MakeBranching()` returns a valid SkeletonDef with at least one bone having 3+ children | Unit test: verify branching structure |
| 4 | `PosesAreEqual` returns true for identical poses | Unit test |
| 5 | `PosesAreEqual` returns false for poses differing by more than tolerance | Unit test |
| 6 | `PosesAreEqual` handles rotation wrapping (e.g., -pi and +pi are equal) | Unit test |
| 7 | `BoneTransformsAreEqual` compares position, rotation, scale with tolerance | Unit test |
| 8 | Test helpers live in `Dia/DiaRig2D/Testing/`, not in `Cluiche/Tests/GoogleTests/` | File location review |
| 9 | JSON fixture files exist and are parseable by LoadSkeletonDef | Unit test: load each fixture |
| 10 | Invalid JSON fixtures correctly trigger error paths | Unit test: load invalid fixtures, verify LoadSkeletonDef returns false |

---

## Tasks

| # | Task | Depends On | Notes |
|---|------|------------|-------|
| 1 | Implement `SkeletonBuilders.h` / `SkeletonBuilders.cpp` | Flat Skeleton feature | |
| 2 | Implement `PoseComparison.h` / `PoseComparison.cpp` | Pose & Pose Blending feature | |
| 3 | Create JSON fixture files | JSON Skeleton Definitions feature | |
| 4 | Unit tests for builders and comparison helpers | 1, 2 | |

---

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-004 | Platform | No STL in public APIs | Compliant — builders return SkeletonDef (Dia types) |
| AD-003 | Dia App | Namespace Dia::\<Module\>:: | Compliant — Dia::Rig2D::Testing:: |
| RD-001 | DiaRig2D | Flat bone array in topological order | Compliant — all builders produce topologically ordered bone arrays |
| RD-002 | DiaRig2D | 2D only | Compliant — all test skeletons use 2D transforms |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Location | Why `DiaRig2D/Testing/` instead of `GoogleTests/`? | Established project pattern — test helpers ship with the library so they're available to any test project and future consumers. `GoogleTests/` contains test executables, not reusable utilities. |
| 2 | MakeHumanoid | Should the humanoid skeleton be configurable (e.g., number of spine bones, arm segments)? | No — it's a fixed canonical skeleton for testing. Tests that need specific topologies use MakeSimpleChain or construct SkeletonDefs directly. MakeHumanoid is a convenience, not a generator. |
| 3 | Tolerance | Default tolerance of 0.001f — is this appropriate? | Yes for unit tests with small bone counts and short chains. Deeply nested chains with many matrix multiplications might need looser tolerance. Callers can pass a custom tolerance. |

---

## Open Questions

None — all resolved above.

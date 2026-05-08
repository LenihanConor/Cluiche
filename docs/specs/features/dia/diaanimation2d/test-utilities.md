# Feature Spec: Test Utilities

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diaanimation2d.md | **test-utilities** |

**Status:** `Approved`

---

## Problem Statement

Testing DiaAnimation2D features (spring chains, clip players, blend stacks, evaluator) requires building skeletons, creating clips, configuring spring chains, and comparing pose output. Without shared test helpers, every test file duplicates this boilerplate, making tests harder to write and maintain.

---

## Solution Overview

Provide test helpers in `Dia/DiaAnimation2D/Testing/` under namespace `Dia::Animation2D::Testing`. Ships with the library — consumer opt-in via include (AND-011). Not compiled into the main library object. This follows the established pattern: test helpers live in `<Module>/Testing/`, not in the test project.

### Clip Builders

```cpp
namespace Dia::Animation2D::Testing {
    // Build a simple AnimClipDef with N keyframes per bone, linear ramp
    AnimClipDef BuildTestClip(const Dia::Core::StringCRC& id, float duration,
                              const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC>& boneIds,
                              int keyframesPerTrack = 2);

    // Build a single-track clip for one bone
    AnimClipDef BuildSingleBoneClip(const Dia::Core::StringCRC& clipId,
                                     const Dia::Core::StringCRC& boneId,
                                     float duration,
                                     float startRotation, float endRotation);
}
```

- **BuildTestClip**: Creates an `AnimClipDef` with a track per bone. Each track has `keyframesPerTrack` keyframes evenly spaced over `duration`, with rotation linearly ramping from 0 to 1 radian. Useful for blend stack and evaluator tests.
- **BuildSingleBoneClip**: Creates a minimal single-track clip targeting one bone, with two keyframes at `startRotation` and `endRotation`. Useful for clip player and sampling tests.

### Spring Chain Builders

```cpp
namespace Dia::Animation2D::Testing {
    // Build a SpringChainDef for a contiguous bone range
    SpringChainDef BuildTestSpringChain(const Dia::Core::StringCRC& id,
                                         const Dia::Rig2D::Skeleton& skeleton,
                                         int startBoneIndex, int endBoneIndex,
                                         float stiffness = 50.0f, float damping = 5.0f);
}
```

- **BuildTestSpringChain**: Creates a `SpringChainDef` covering a contiguous bone range `[startBoneIndex, endBoneIndex]` in the provided skeleton. Uses the skeleton's bone IDs and sets uniform stiffness/damping across all nodes. Useful for spring chain simulation tests.

### Pose Comparison

```cpp
namespace Dia::Animation2D::Testing {
    // Assert all bone transforms match within tolerance
    void AssertPosesEqual(const Dia::Rig2D::Pose& expected, const Dia::Rig2D::Pose& actual,
                          float tolerance = 1e-5f);

    // Assert a specific bone's transform matches
    void AssertBoneTransformEqual(const Dia::Rig2D::BoneTransform& expected,
                                  const Dia::Rig2D::BoneTransform& actual,
                                  float tolerance = 1e-5f);

    // Assert a bone's local rotation is within tolerance of expected value
    void AssertBoneRotation(const Dia::Rig2D::Pose& pose, int boneIndex,
                            float expectedRotation, float tolerance = 1e-5f);
}
```

Assertions use GoogleTest `EXPECT_NEAR` (non-fatal) so a single test can report multiple bone failures. Rotation comparison uses shortest-arc difference to handle -pi/+pi wrapping.

### Evaluator Builders

```cpp
namespace Dia::Animation2D::Testing {
    // Build an AnimationEvaluator with a test skeleton, ready to register sources.
    // Skeleton and pose are owned by the returned struct.
    struct TestEvaluatorFixture {
        Dia::Rig2D::Skeleton skeleton;
        Dia::Rig2D::Pose pose;
        AnimationEvaluator evaluator;
    };
    TestEvaluatorFixture BuildTestEvaluator(int boneCount = 5);
}
```

- **BuildTestEvaluator**: Creates a `Skeleton` (simple chain of `boneCount` bones via DiaRig2D), a `Pose` initialised to bind pose, and an `AnimationEvaluator` wired to both. Returns a self-contained fixture ready for source registration and evaluation.

### Files

| File | Purpose |
|------|---------|
| `Dia/DiaAnimation2D/Testing/AnimClipTestHelpers.h` | Clip builder functions |
| `Dia/DiaAnimation2D/Testing/SpringChainTestHelpers.h` | Spring chain builder functions |
| `Dia/DiaAnimation2D/Testing/PoseAssertions.h` | Pose comparison assertions |
| `Dia/DiaAnimation2D/Testing/EvaluatorTestHelpers.h` | Evaluator fixture builder |

---

## Key Behaviours

1. All helpers are header-only in `Testing/` directory — not compiled into `DiaAnimation2D.lib`
2. Consumer opt-in via `#include <DiaAnimation2D/Testing/...>`
3. Uses DiaRig2D public API for skeleton building (depends on `DiaRig2D` types but not `DiaRig2D/Testing/`)
4. Assertions use GoogleTest `EXPECT_*` macros (non-fatal) for multi-failure reporting

---

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| 1 | `BuildTestClip` produces a valid `AnimClipDef` with correct track count and keyframe count per track | Unit test: construct `AnimClip` from the def, verify `GetTrackCount()` and keyframe sampling |
| 2 | `BuildSingleBoneClip` produces an `AnimClipDef` with one track, correct duration, and correct start/end rotation keyframes | Unit test: sample clip at t=0 and t=duration, verify rotation values |
| 3 | `BuildTestSpringChain` produces a valid `SpringChainDef` for the contiguous bone range with correct stiffness and damping | Unit test: construct `SpringChain` from the def, verify `GetNodeCount()` matches bone range |
| 4 | `AssertPosesEqual` passes for identical poses | Unit test: create two identical poses, verify assertion passes |
| 5 | `AssertPosesEqual` fails for poses differing by more than tolerance | Unit test: modify one bone, verify assertion reports failure |
| 6 | `AssertBoneRotation` checks the correct bone index and reports failure for mismatched rotation | Unit test: set known rotation, verify pass/fail at correct bone |
| 7 | `TestEvaluatorFixture` is functional — can register clip players and spring chains, call `Evaluate`, and read output pose | Unit test: register a clip player, evaluate one frame, verify pose changed |
| 8 | All helpers compile and link correctly when included from GoogleTests | Build: include all four headers from a test file, compile and link successfully |

---

## Tasks

| # | Task | Depends On | Notes |
|---|------|------------|-------|
| 1 | Create `Dia/DiaAnimation2D/Testing/` directory | - | |
| 2 | Implement `AnimClipTestHelpers.h` — clip builder functions | Keyframe Clip Player feature | |
| 3 | Implement `SpringChainTestHelpers.h` — spring chain builder functions | Damped Spring Chain feature | |
| 4 | Implement `PoseAssertions.h` — pose comparison assertions | DiaRig2D Pose types | |
| 5 | Implement `EvaluatorTestHelpers.h` — evaluator fixture builder | Animation Evaluator feature | |
| 6 | Unit tests for all helpers in GoogleTests | 2, 3, 4, 5 | Verify acceptance criteria |

---

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| AD-003 | Dia App | Namespace `Dia::<Module>::` | Compliant — all helpers in `Dia::Animation2D::Testing::` |
| AND-010 | DiaAnimation2D | No STL in public APIs | Compliant — helper signatures use DiaCore/DiaMaths/DiaRig2D types only |
| AND-011 | DiaAnimation2D | Test utilities ship in `DiaAnimation2D/Testing/` | Compliant — implemented exactly as specified, consumer opt-in via include |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | DiaRig2D dependency | Should test helpers depend on `DiaRig2D/Testing/` helpers or be self-contained? | Self-contained. `BuildTestEvaluator` uses DiaRig2D's public API directly (`SkeletonDef`, `Skeleton` constructor) rather than depending on `DiaRig2D/Testing/` builders. This avoids coupling to another module's test utilities. If DiaRig2D/Testing offers a convenient builder, prefer copying the pattern rather than importing. |
| 2 | Clip builders | Should `BuildTestClip` support configurable interpolation patterns (not just linear ramp)? | No — linear ramp is sufficient for test determinism. Tests that need specific interpolation patterns should construct `AnimClipDef` directly. `BuildTestClip` is a convenience for the common case, not a general-purpose generator. |
| 3 | Spring chain helpers | Should there be a `RunSpringChainSteps` helper that advances N steps and returns final state? | Deferred. A step-runner helper adds convenience but also hides the integration loop, making tests less transparent. If multiple test files end up writing the same `for` loop, factor it out then. For now, explicit loops in tests are clearer. |
| 4 | Tolerance defaults | Is `1e-5f` appropriate for all comparisons (position, rotation, scale)? | Appropriate for unit tests with small bone counts and short chains. Semi-implicit Euler integration is deterministic for identical inputs, so tight tolerance catches real bugs without false positives. For deeply nested chains or long simulation runs, callers can pass a looser tolerance via the parameter. |

---

## Open Questions

None — all resolved above.

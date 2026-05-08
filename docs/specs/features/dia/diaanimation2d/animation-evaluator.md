# Feature Spec: Animation Evaluator

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diaanimation2d.md | **animation-evaluator** |

**Status:** `Approved`

---

## Problem Statement

The animation pipeline has a strict evaluation order (FK -> clip sample -> spring update -> blend) and requires intermediate Pose objects per source to avoid the data flow contradiction where spring chains write in-place but the blend stack needs separate input poses. Without an orchestrator, every game must independently implement this pipeline correctly, allocate intermediate poses, and avoid aliasing bugs. This is a documented footgun.

---

## Solution Overview

One class in `Dia::Animation2D`:

```cpp
namespace Dia::Animation2D {
    class AnimationEvaluator {
    public:
        explicit AnimationEvaluator(Dia::Rig2D::Skeleton& skeleton, Dia::Rig2D::Pose& pose);

        // Source registration -- evaluator allocates and owns intermediate Poses.
        // Returns a non-owning pointer to the evaluator-owned Pose for the source.
        Dia::Rig2D::Pose* RegisterClipPlayer(Dia::Core::StringCRC sourceId,
                                              AnimClipPlayer& player,
                                              int blendPriority,
                                              const BoneMask* boneMask = nullptr);

        Dia::Rig2D::Pose* RegisterSpringChain(Dia::Core::StringCRC sourceId,
                                               SpringChain& chain,
                                               int blendPriority,
                                               const BoneMask* boneMask = nullptr);

        void UnregisterSource(Dia::Core::StringCRC sourceId);
        void SetSourceWeight(Dia::Core::StringCRC sourceId, float weight);
        void SetSourcePriority(Dia::Core::StringCRC sourceId, int priority);

        // Run the full animation pipeline for this frame:
        //   1. FK (compute world transforms from current pose + root transform)
        //   2. Each clip player: Update(dt), Sample into evaluator-owned pose
        //   3. Each spring chain: Update(dt, evaluator-owned pose, world transforms)
        //   4. PoseBlendStack: Evaluate all sources into the target pose
        // Caller runs DiaIK2D post-process and final FK after this returns.
        void Evaluate(float dt, const Dia::Rig2D::BoneTransform& rootTransform);

        // Direct access for advanced use cases.
        PoseBlendStack&       GetBlendStack();
        const PoseBlendStack& GetBlendStack() const;
    };
}
```

### Key Behaviours

1. **Owns intermediate Poses** -- one per registered source, allocated at registration, reused across frames (AND-021).
2. **RegisterClipPlayer / RegisterSpringChain** returns a non-owning pointer to the evaluator-owned Pose (for inspection/debug; callers should not write to it).
3. **Evaluate(dt, rootTransform)** runs the full pipeline:
   - a. FK: `ComputeWorldTransforms` from current pose + rootTransform into an internal worldTransforms buffer.
   - b. For each registered clip player: `player.Update(dt)`, `player.Sample(skeleton, evaluator-owned pose)`.
   - c. For each registered spring chain: `chain.Update(dt, evaluator-owned pose, worldTransforms)`.
   - d. `PoseBlendStack.Evaluate(target pose)` -- blends all sources into the target pose passed at construction.
4. **Clip players evaluated before spring chains** -- clips produce base poses; springs react to them.
5. **Target pose** (passed at construction) is what game code reads after `Evaluate` returns. Caller then runs DiaIK2D and final FK.
6. **Non-owning references** to Skeleton, Pose, AnimClipPlayers, SpringChains -- caller manages lifetimes.
7. **DIA_ASSERT if sourceId already registered** -- duplicate registration is a programming error.
8. **UnregisterSource** deallocates the evaluator-owned Pose for that source.
9. **GetBlendStack()** exposes the internal PoseBlendStack for advanced layer manipulation.
10. **Output alias protection guaranteed by construction** -- each source has its own Pose, blend stack output is the target pose which is distinct.

### Files

| File | Purpose |
|------|---------|
| `Dia/DiaAnimation2D/AnimationEvaluator.h` | AnimationEvaluator class declaration |
| `Dia/DiaAnimation2D/AnimationEvaluator.cpp` | Implementation |

---

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| 1 | Constructor stores non-owning references to Skeleton and target Pose | Code review |
| 2 | `RegisterClipPlayer` allocates an evaluator-owned Pose and returns a non-null pointer | Unit test: register a clip player, verify returned pointer is non-null |
| 3 | `RegisterSpringChain` allocates an evaluator-owned Pose and returns a non-null pointer | Unit test: register a spring chain, verify returned pointer is non-null |
| 4 | Duplicate sourceId on registration triggers DIA_ASSERT | Unit test: register same sourceId twice, DIA_ASSERT fires |
| 5 | `UnregisterSource` frees the evaluator-owned Pose for that source | Unit test: register then unregister, verify source is removed |
| 6 | `Evaluate` runs FK first -- world transforms are computed before clip/spring processing | Unit test: verify world transforms buffer is populated after Evaluate |
| 7 | `Evaluate` calls clip player `Update(dt)` and `Sample(skeleton, pose)` for each registered clip player | Unit test: mock/verify clip player receives Update and Sample calls |
| 8 | `Evaluate` calls spring chain `Update(dt, pose, worldTransforms)` with the world transforms buffer | Unit test: verify spring chain receives Update call with correct parameters |
| 9 | `Evaluate` calls `PoseBlendStack::Evaluate` to blend all sources into the target pose | Unit test: verify target pose is modified after Evaluate with active sources |
| 10 | Pipeline order is FK -> clip -> spring -> blend (clips before springs, blend last) | Unit test: verify clips are sampled before springs update, both before blend |
| 11 | `SetSourceWeight` propagates to the corresponding PoseBlendStack layer | Unit test: set weight, verify blend stack layer weight matches |
| 12 | `SetSourcePriority` propagates to the corresponding PoseBlendStack layer | Unit test: set priority, verify blend stack layer priority matches |
| 13 | Zero registered sources: `Evaluate` is a no-op, target pose unchanged | Unit test: call Evaluate with no sources, verify target pose is identical before and after |
| 14 | All public API uses Dia containers (no STL) | Code review |
| 15 | All code in `Dia::Animation2D::` namespace | Code review |

---

## Tasks

| # | Task | Depends On | Notes |
|---|------|------------|-------|
| 1 | Implement `AnimationEvaluator.h` / `AnimationEvaluator.cpp` -- source registration, pose allocation, pipeline execution | - | Add to `DiaAnimation2D.vcxproj` and `.vcxproj.filters` |
| 2 | Unit tests in `Cluiche/Tests/GoogleTests/Animation2D/` -- pipeline ordering, pose ownership, source lifecycle | 1 | Test all acceptance criteria |

---

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Compliant -- sourceId parameters are StringCRC; blend stack layer IDs are StringCRC |
| PD-004 | Platform | No STL containers in public APIs | Compliant -- internal collections use DynamicArrayC; no std::vector, std::string, etc. |
| PD-007 | Platform | C++20 required | Compliant -- compiled under /std:c++20 |
| AD-002 | Dia App | No STL in public APIs | Compliant -- reinforces PD-004 |
| AD-003 | Dia App | Namespace Dia::\<Module\>:: | Compliant -- all code in Dia::Animation2D:: |
| AND-010 | DiaAnimation2D | No STL in public APIs | Compliant -- reinforces PD-004/AD-002 |
| AND-015 | DiaAnimation2D | SpringChain requires FK to have been run before Update | Compliant -- AnimationEvaluator runs FK as pipeline step 1 before any spring chain Update calls |
| AND-021 | DiaAnimation2D | AnimationEvaluator orchestrates full pipeline and owns intermediate poses | Compliant -- this feature directly implements AND-021; one Pose allocated per registered source |
| AND-024 | DiaAnimation2D | PoseBlendStack::Evaluate output must not alias any PoseLayer::pose input | Compliant -- guaranteed by construction; each source has its own evaluator-owned Pose, target pose is distinct from all source poses |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Clip Player Lifecycle | What if a registered clip player's AnimClip is changed between frames (e.g. `Play()` called with a different clip)? | Safe -- AnimClipPlayer internally tracks its current clip via a non-owning pointer. `Play()` restarts from time 0 with the new clip (AND-023). The evaluator calls `player.Update(dt)` then `player.Sample()` each frame, which operates on whatever clip the player currently references. The evaluator does not cache or depend on which clip is being played. |
| 2 | Pose Reset | Should Evaluate reset evaluator-owned poses to bind pose before each frame? | Yes -- each evaluator-owned Pose should be reset to the skeleton's bind pose at the start of `Evaluate` before clip sampling or spring updating. This ensures that bones not covered by a clip track or spring chain revert to bind pose rather than retaining stale data from the previous frame. This is consistent with AND-022 (AnimClip::Sample is a pure write using bind pose for omitted fields). |
| 3 | Skeleton Mutation | What if the skeleton's bone count changes after construction? | It should not -- `Skeleton` is immutable after construction (flat-skeleton spec). The evaluator stores a non-owning reference to the Skeleton provided at construction. If the caller destroys the Skeleton and creates a new one, the evaluator's reference is dangling -- this is a caller lifetime error, not an evaluator concern. Document that the Skeleton must outlive the AnimationEvaluator. |
| 4 | Bone Overlap | Can the evaluator support a mix of clip players and spring chains on overlapping bones? | Yes -- each source writes to its own evaluator-owned Pose independently. Overlapping bones are resolved by the PoseBlendStack during the blend step, using layer priority and per-bone BoneMask weighting. This is the intended arbitration mechanism (same pattern as DiaIK2D chain overlap). For best results, callers should use BoneMasks to partition bones across sources, but overlapping without masks will produce a blended result based on priority and weight. |
| 5 | Source Limits | Should there be a max source count? | A reasonable static limit (e.g., 16 or 32 sources) is appropriate for a 2D game engine. This bounds memory allocation for intermediate Poses and blend stack layers. DIA_ASSERT if the limit is exceeded at registration time. The limit can be a `static constexpr` on AnimationEvaluator (e.g., `kMaxSources = 16`). Typical usage is 2-5 sources (base clip + 1-2 overlays + 1-2 spring chains). |
| 6 | Zero Delta Time | What happens if Evaluate is called with dt=0? | Safe -- clip players advance by 0 (no time change, same sample), spring chains sub-step with dt=0 (no velocity/position change), FK and blend proceed normally. The result is identical to the previous frame's output. This is expected during paused gameplay or when the game intentionally calls Evaluate without time progression. |
| 7 | Unregister During Evaluate | What if UnregisterSource is called from within a callback triggered during Evaluate? | Not supported -- `Evaluate` is not re-entrant and does not invoke user callbacks. All registration/unregistration must happen outside of `Evaluate`. DIA_ASSERT if mutation is attempted during evaluation (optional guard via a bool flag set during Evaluate). |
| 8 | World Transforms Buffer | Should the internal world transforms buffer be exposed for callers who need world-space bone positions after Evaluate? | Not in v1 -- the world transforms buffer is an internal implementation detail used only to feed spring chain Update calls. After Evaluate returns, the caller runs their own FK pass (potentially after IK post-process), which produces the authoritative world transforms. Exposing the internal buffer would create confusion about which world transforms are "final". |

---

## Open Questions

None -- all resolved above.

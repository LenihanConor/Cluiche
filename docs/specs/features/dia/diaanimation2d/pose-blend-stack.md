# Feature Spec: Pose Blend Stack

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diaanimation2d.md | **pose-blend-stack** |

**Status:** `Approved`

---

## Problem Statement

Multiple animation sources (clip players, spring chains, procedural systems) produce poses for different bone groups. Without a blending mechanism, these systems stomp each other's bone writes. The engine needs a way to combine poses from different sources with per-bone control and artist-friendly weight semantics.

---

## Solution Overview

Provide three types — `BoneMask`, `PoseLayer`, and `PoseBlendStack` — in the `Dia::Animation2D` namespace.

**BoneMask** defines which bones a layer affects:

```cpp
namespace Dia::Animation2D {
    struct BoneMask {
        Dia::Core::StringCRC                                        id;
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC>  boneIds;
    };
}
```

**PoseLayer** is a single blend layer:

```cpp
namespace Dia::Animation2D {
    struct PoseLayer {
        Dia::Core::StringCRC    id;
        const Dia::Rig2D::Pose* pose;       // Non-owning; source pose for this layer
        float                   weight;     // [0, 1]; 0-weight layers are skipped during Evaluate
        int                     priority;   // Lower = evaluated first (base); higher = evaluated later (override)
        const BoneMask*         boneMask;   // nullptr = affects all bones
    };
}
```

**PoseBlendStack** is the blender:

```cpp
namespace Dia::Animation2D {
    class PoseBlendStack {
    public:
        explicit PoseBlendStack(const Dia::Rig2D::Skeleton& skeleton);

        // Add/remove layers. Layers are sorted by priority for evaluation.
        void AddLayer(const PoseLayer& layer);
        void RemoveLayer(Dia::Core::StringCRC layerId);
        void SetLayerWeight(Dia::Core::StringCRC layerId, float weight);
        void SetLayerPriority(Dia::Core::StringCRC layerId, int priority);

        // Evaluate: blend all layers in priority order via cascading lerp.
        // result = lerp(result, layer[i], weight[i]) per layer, low priority first.
        // Zero-weight layers are skipped. outPose must NOT alias any PoseLayer::pose.
        void Evaluate(Dia::Rig2D::Pose& outPose) const;

        int  GetLayerCount() const;
        bool HasLayer(Dia::Core::StringCRC layerId) const;
        void Clear();
    };
}
```

### Key Behaviours

1. **Cascading lerp evaluation**: `result = lerp(result, layer[i], weight[i])` per layer in priority order (AND-016). Weight 1.0 = full override. Weights do not need to sum to 1.
2. **Explicit integer priority, not push-order** (AND-025). Lower = evaluated first (base). Same priority = registration order.
3. **Zero-weight layers skipped** during Evaluate (AND-026) — documented fast-path.
4. **Output pose must NOT alias any PoseLayer::pose** — DIA_ASSERT in debug (AND-024).
5. **BoneMask bone IDs resolved to indices at AddLayer time** (AND-009). Invalid bone IDs produce DIA_LOG_WARNING and are excluded from the resolved set (AND-020).
6. **nullptr boneMask** = layer affects all bones.
7. **Zero layers** = Evaluate is no-op, outPose unchanged.
8. **Rotation blending** uses shortest-arc angle lerp (consistent with DiaRig2D::BlendPoses).
9. **Per-bone evaluation**: for each bone, iterate layers that include it, apply cascading lerp.

### Files

| File | Purpose |
|------|---------|
| `Dia/DiaAnimation2D/BoneMask.h` | BoneMask struct |
| `Dia/DiaAnimation2D/PoseLayer.h` | PoseLayer struct |
| `Dia/DiaAnimation2D/PoseBlendStack.h` | PoseBlendStack class declaration |
| `Dia/DiaAnimation2D/PoseBlendStack.cpp` | PoseBlendStack implementation |

---

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| 1 | Single layer with weight 1.0 produces exact copy of source pose | Unit test: add one layer at weight 1.0, Evaluate, compare output to source pose bone-by-bone |
| 2 | Two layers with cascading lerp produce correct blended output | Unit test: two layers at different priorities, verify result matches manual lerp calculation |
| 3 | Zero-weight layer is skipped: output identical with and without it | Unit test: evaluate with a zero-weight layer present, prove output matches evaluation without that layer |
| 4 | Priority ordering is respected: higher priority overrides lower | Unit test: two layers at different priorities, swap priorities, verify output changes accordingly |
| 5 | Same-priority layers respect registration order | Unit test: add two layers at same priority, verify first-added is evaluated as base |
| 6 | BoneMask restricts which bones a layer affects | Unit test: mask a layer to only affect bone "A", verify bone "B" is unchanged by that layer |
| 7 | Invalid bone IDs in BoneMask are logged and excluded | Unit test: add layer with mask containing a non-existent bone ID, verify DIA_LOG_WARNING and bone is excluded from resolved set |
| 8 | nullptr BoneMask means layer affects all bones | Unit test: add layer with nullptr mask, verify all bones are affected |
| 9 | Zero layers = Evaluate is no-op, outPose unchanged | Unit test: call Evaluate on empty stack, verify outPose retains its original values |
| 10 | Output alias DIA_ASSERT fires when outPose aliases a layer's source pose | Unit test: pass a PoseLayer's source pose as outPose, verify DIA_ASSERT fires in debug |
| 11 | Rotation shortest-arc is correct across -pi/+pi boundary | Unit test: one layer at rotation -3.0 rad, another at +3.0 rad, verify blend goes through the short arc (not the long way around) |
| 12 | AddLayer correctly adds a layer and GetLayerCount reflects it | Unit test: add N layers, verify GetLayerCount == N |
| 13 | RemoveLayer correctly removes a layer by ID | Unit test: add layer, remove by ID, verify HasLayer returns false and GetLayerCount decremented |
| 14 | SetLayerWeight updates the weight of an existing layer | Unit test: add layer at weight 1.0, set weight to 0.5, evaluate, verify blended output reflects 0.5 weight |
| 15 | SetLayerPriority updates the priority of an existing layer | Unit test: add two layers, change priority of one, verify evaluation order changes |
| 16 | HasLayer returns true for existing layers, false for non-existent | Unit test: add layer, verify HasLayer(id) == true; verify HasLayer(unknownId) == false |
| 17 | Clear empties the stack and GetLayerCount returns 0 | Unit test: add layers, call Clear, verify GetLayerCount == 0 |
| 18 | All public APIs use Dia containers (DynamicArrayC), no STL | Code review |
| 19 | All code in `Dia::Animation2D::` namespace | Code review |

---

## Tasks

| # | Task | Depends On | Notes |
|---|------|------------|-------|
| 1 | Create `Dia/DiaAnimation2D/BoneMask.h` and `Dia/DiaAnimation2D/PoseLayer.h` — data structs | - | BoneMask with StringCRC id + DynamicArrayC of bone IDs; PoseLayer with id, pose ptr, weight, priority, mask ptr |
| 2 | Implement `Dia/DiaAnimation2D/PoseBlendStack.h` / `PoseBlendStack.cpp` — layer management, bone mask resolution at AddLayer time, cascading lerp evaluate with per-bone iteration | 1 | Resolve BoneMask bone IDs to indices at AddLayer; DIA_ASSERT alias check in Evaluate; skip zero-weight layers |
| 3 | Unit tests in `Cluiche/Tests/GoogleTests/Animation2D/` | 2 | Cover all 19 acceptance criteria |

---

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Compliant — BoneMask.id, PoseLayer.id, BoneMask.boneIds all use StringCRC |
| PD-004 | Platform | No STL containers in public APIs | Compliant — BoneMask.boneIds is DynamicArrayC; internal layer storage uses Dia containers |
| PD-007 | Platform | C++20 required | Compliant — compiled under /std:c++20 |
| AD-002 | Dia App | No STL in public APIs | Compliant — reinforces PD-004 |
| AD-003 | Dia App | Namespace Dia::\<Module\>:: | Compliant — all code in Dia::Animation2D:: |
| AND-003 | DiaAnimation2D | PoseBlendStack is a flat priority-ordered stack, not a blend tree | Compliant — this feature implements the flat stack design |
| AND-009 | DiaAnimation2D | Per-bone BoneMask uses StringCRC bone IDs, resolved to indices at AddLayer time | Compliant — bone ID resolution happens once at AddLayer, not per-frame in Evaluate |
| AND-010 | DiaAnimation2D | No STL in public APIs | Compliant — reinforces PD-004/AD-002 |
| AND-016 | DiaAnimation2D | Cascading lerp evaluation | Compliant — Evaluate uses `result = lerp(result, layer[i], weight[i])` per layer bottom-up |
| AND-020 | DiaAnimation2D | BoneMask with invalid bone IDs: DIA_LOG_WARNING and skip at AddLayer time | Compliant — invalid bone IDs logged and excluded from resolved index set |
| AND-024 | DiaAnimation2D | PoseBlendStack::Evaluate output must not alias any PoseLayer::pose input | Compliant — DIA_ASSERT in debug that outPose address differs from all layer pose pointers |
| AND-025 | DiaAnimation2D | Layers use explicit integer priority, not push-order | Compliant — PoseLayer.priority is an int; layers sorted by priority for evaluation |
| AND-026 | DiaAnimation2D | Skips layers with weight == 0.0f during Evaluate | Compliant — zero-weight layers produce no per-bone lerp; documented fast-path |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Layer Management | What happens if the same layer ID is added twice via AddLayer? | DIA_ASSERT in debug. Layer IDs must be unique within a PoseBlendStack. Callers who want to update an existing layer should use SetLayerWeight/SetLayerPriority, or RemoveLayer + AddLayer. Silent replacement would hide bugs where two systems accidentally choose the same ID. |
| 2 | Weight Semantics | What if SetLayerWeight is called with a value outside [0, 1]? | Clamp to [0, 1] with a DIA_LOG_WARNING. Values outside this range have no meaningful interpretation in the cascading lerp model (negative weights would extrapolate, >1 would overshoot). Clamping is safer than asserting since weight may come from gameplay math that transiently overshoots. |
| 3 | Zero Layers | Should Evaluate output bind pose as the starting base, or leave outPose as-is for zero layers? | Leave outPose as-is. The caller decides the starting state — typically bind pose or the previous frame's pose. This keeps PoseBlendStack a pure blending utility with no hidden state. For non-zero layers, the first layer (lowest priority) with weight 1.0 fully sets the base. If the first layer has weight < 1.0, it blends with whatever was already in outPose, which is the correct behavior for partial overrides on top of a caller-provided base. |
| 4 | Lifetime Safety | What if a layer's source Pose is destroyed between AddLayer and Evaluate? | Undefined behavior — PoseLayer holds a non-owning pointer. This follows the same contract as AnimClipPlayer's non-owning clip pointer (AND-006) and DiaIK2D's non-owning Skeleton reference (SD-008). The caller owns lifetime. In practice, AnimationEvaluator manages this by owning intermediate poses. For raw API users, this is documented as caller responsibility. |
| 5 | Scalability | Should there be a max layer count? | No hard limit in v1. Practical use cases have 2-5 layers (base pose, IK overlay, one-shot clip, facial). DynamicArrayC handles growth. If profiling shows the layer sort is a bottleneck (unlikely at <10 layers), a fixed-capacity StaticArrayC could be added later without API changes. |
| 6 | Blending Model | Does cascading lerp produce different results from normalized weighted average? Show an example. | Yes. Consider two layers: base (weight 1.0, priority 0) and overlay (weight 0.5, priority 1). For a bone with base rotation 0 and overlay rotation 1.0 rad: **Cascading lerp**: `result = lerp(0, 1.0, 0.5) = 0.5 rad`. **Normalized average**: weights sum to 1.5, normalized to (0.667, 0.333), result = `0.667 * 0 + 0.333 * 1.0 = 0.333 rad`. Different result. Cascading lerp is more intuitive: the overlay says "blend me 50% on top of whatever is below." This matches Unity/Unreal layer conventions. |
| 7 | Rotation | Is shortest-arc angle lerp sufficient for all 2D rotation blending, or are there edge cases? | Shortest-arc angle lerp handles the -pi/+pi wraparound correctly. The only edge case is exactly pi difference (180 degrees), where both arcs are equal length. In this case either direction is valid — the implementation should pick one deterministically (e.g., always positive direction). This is consistent with DiaRig2D::BlendPoses behavior. |
| 8 | Performance | For per-bone evaluation, is iterating all layers per bone better than iterating all bones per layer? | Iterating layers per bone (the specified approach) produces the correct cascading lerp result and is cache-friendly for the output pose. Iterating bones per layer would require a temporary accumulator per bone. With typical counts (20-50 bones, 2-5 layers), both approaches are fast. The per-bone approach avoids temporary storage and is simpler to implement correctly. |

---

## Open Questions

None — all resolved above.

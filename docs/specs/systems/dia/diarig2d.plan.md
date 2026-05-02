# Plan: DiaRig2D

**Spec:** @docs/specs/systems/dia/diarig2d.md  
**Status:** Done  
**Started:** 2026-05-01  
**Last Updated:** 2026-05-01

## Implementation Order Rationale

The dependency chain is linear: Bone/Skeleton (data foundation) -> Pose/FK/Blend (transform operations on skeletons) -> JSON loader (data-driven construction) -> SkeletonComponent (engine integration) -> Test Utilities (shared helpers for all tests) -> Debug Renderer (visual verification, separate project).

Test utilities (Task 7) are placed after SkeletonComponent but before the debug renderer because the builders and comparison helpers are needed by all feature tests. However, each feature's tests are written alongside the feature (Tasks 3, 5, 6, 8) using inline skeleton construction first, then refactored to use test utilities in Task 7.

The debug renderer is last because it's a separate .vcxproj with a DiaGraphics dependency, making it the most isolated deliverable.

## Dependencies (External to This System)

DiaRig2D depends on:
- **DiaCore** — StringCRC, DynamicArrayC, DIA_ASSERT, IComponent, IComponentFactory, ComponentFactoryRegistry, DynamicComponentFactory, Json (jsoncpp wrapper), FilePath
- **DiaMaths** — Vector2D (position, scale), float angle math, `Vector2D::Length()` for bone length computation
- **DiaLogger** — `DIA_LOG_WARNING` / `DIA_LOG_DEBUG` on `Rig2D` channel

**Pre-implementation check:** Verify these APIs exist before starting:
- `DiaCore::StringCRC` constructor from string literal (for `kUniqueId`)
- `DiaCore::Containers::DynamicArrayC<T>` — `Add()`, `GetSize()`, `operator[]`, `Reserve()`
- `DiaCore::IComponent` — base class with `GetUniqueId()` virtual
- `DiaCore::DynamicComponentFactory<T>` — template factory
- `DiaCore::ComponentFactoryRegistry` — `Register()` and singleton access
- `DiaMaths::Vector2D` — `Length()`, arithmetic operators, `x`/`y` members
- DiaCore/Json — jsoncpp `Json::Value` parsing, `Json::Reader`
- DiaLogger channel registration and `DIA_LOG_WARNING` macro

## Implementation Patterns

### Task 1 — Project Scaffolding
- **vcxproj**: `StaticLibrary`, x64 only, include dirs `./;./../;`, `ProjectReference` to DiaCore, DiaMaths, DiaLogger with `ReferenceOutputAssembly=false`. NO OutDir/IntDir/PlatformToolset/LanguageStandard overrides (PD-008).
- **sln**: Register under Library solution folder, same pattern as DiaSoftBody2D entry.
- **Filters**: Top-level filters for `Docs`, `Testing`, source root.
- **Architecture doc**: `Docs/dia.rig2d.architecture.module.md` with `schema: dia.module.v1`, `module_id: dia.rig2d`, `parent_module_id: dia.root`, dependencies on `dia.core`, `dia.maths`, `dia.logger`.

### Task 2-3 — Bone + Skeleton (Flat Skeleton feature)
- **Bone.h**: Plain struct in `Dia::Rig2D::`. Default-initialised: `name{}`, `parentIndex{-1}`, `localPosition{0,0}`, `localRotation{0}`, `localScale{1,1}`, `length{0}`. No `.cpp` needed.
- **Skeleton.h/cpp**: `SkeletonDef` struct (StringCRC id + DynamicArrayC<Bone>). `Skeleton` class with `static constexpr int kMaxBones = 256`. Constructor validates: `DIA_ASSERT(boneCount <= kMaxBones)`, `DIA_ASSERT(exactly one root)`, `DIA_ASSERT(unique names)`, `DIA_ASSERT(topological order)`. Computes `length = localPosition.Length()` for non-root bones where `length == 0` in the def. `FindBoneIndex()` — silent, returns -1. `GetRequiredBoneIndex()` — DIA_ASSERT + Rig2D channel warning on miss.
- **StringCRC note**: Use `static const` (not `constexpr`) for `kUniqueId` if StringCRC constructor isn't constexpr (same as DiaSoftBody2D pattern).

### Task 4-5 — Pose + FK + Blend (Pose & Pose Blending feature)
- **BoneTransform.h**: Plain struct. Defaults: `position{0,0}`, `rotation{0}`, `scale{1,1}`.
- **Pose.h/cpp**: Internal `DynamicArrayC<BoneTransform>` sized at construction. Does NOT store Skeleton reference. `ComputeWorldTransforms()`: single forward pass, compose 3x3 affine matrix per bone as `worldParent * T * R * S`, decompose back to BoneTransform. DIA_ASSERT on bone count mismatch and output capacity.
- **BlendPoses.h/cpp**: Free function. Clamp t to [0,1]. Per-bone: lerp position, shortest-arc angle lerp for rotation (normalize diff to [-pi,pi]), lerp scale. DIA_ASSERT on bone count mismatch.
- **Matrix composition**: Use inline helper (not a full Matrix3x3 class) — compose S*R*T as 6 floats (2x3 affine), multiply, decompose. This avoids depending on a matrix class that may not exist in DiaMaths for 2D.

### Task 6 — JSON Loader (JSON Skeleton Definitions feature)
- **SkeletonJson.h/cpp**: Uses `DiaCore/Json/` (jsoncpp). `LoadSkeletonDef` returns `bool` (not DIA_ASSERT) for data errors. Logs to Rig2D channel on error. Parent name resolution: linear scan of preceding bones. `SaveSkeletonDef` writes parent as bone name string.
- **LoadSkeletonDefFromString**: Parses from `const char*` — used by tests.

### Task 7 — Test Utilities
- **Testing/SkeletonBuilders.h/cpp**: `MakeSimpleChain(n)` — root + (n-1) children along +Y with 1.0 unit spacing. `MakeHumanoid()` — ~12 bones (root, hips, spine, chest, head, L/R upper arm, L/R lower arm, L/R upper leg, L/R lower leg). `MakeBranching()` — root with 3 children, one of which has 2 children.
- **Testing/PoseComparison.h/cpp**: `PosesAreEqual` / `BoneTransformsAreEqual` with tolerance. Rotation comparison wraps via `fmod` to [-pi,pi] before diff.

### Task 8 — Skeleton Component
- **SkeletonComponent.h/cpp**: Inherits `IComponent`. Owns `Skeleton` + `Pose`. Constructor takes `SkeletonDef`, builds Skeleton, inits Pose to bind pose. `ResetToBindPose()` delegates to `Pose::SetToBindPose()`.
- **Factory**: `DynamicComponentFactory<SkeletonComponent>` — registration helper function or inline in test setup.

### Task 9 — Debug Renderer (separate project)
- **DiaRig2DVisualDebugger/**: New directory with own `.vcxproj` (StaticLibrary, refs DiaRig2D + DiaGraphics). `VisualDebugger.h/cpp`. Takes pre-computed world transforms array + Skeleton + FrameData. Draws lines (bones), circles (joints), optional text (names).
- **Architecture doc**: `dia.rig2dvisualdebugger.architecture.module.md`.

## Tasks

| # | Task | Spec | Status | Notes |
|---|------|------|--------|-------|
| 1 | **Project scaffolding** — Create `Dia/DiaRig2D/` directory, `DiaRig2D.vcxproj` (static lib, x64, refs DiaCore/DiaMaths/DiaLogger), `.vcxproj.filters`, register in `Cluiche.sln`, create `Docs/dia.rig2d.architecture.module.md` | System | Done | GUID {E4F5A6B7-C8D9-0123-ABCD-456789ABCDE0} |
| 2 | **Bone** — `Bone.h`: Bone struct with defaults, in `Dia::Rig2D::` namespace | [flat-skeleton.md](../../features/dia/diarig2d/flat-skeleton.md) | Done | Extended with MetadataValue variant + MetadataEntry bag (SetMetadata/FindMetadata) |
| 3 | **Skeleton** — `Skeleton.h` / `Skeleton.cpp`: SkeletonDef struct, Skeleton class with constructor validation (topology, single root, unique names, max bones), bone length computation, FindBoneIndex, GetRequiredBoneIndex | [flat-skeleton.md](../../features/dia/diarig2d/flat-skeleton.md) | Done | kMaxBones = 128 (reduced from 256 to avoid stack overflow) |
| 4 | **BoneTransform + Pose** — `BoneTransform.h`, `Pose.h` / `Pose.cpp`: Pose class with bind pose init, GetLocalTransform, ComputeWorldTransforms (FK with SRT, negative scale propagation) | [pose-blending.md](../../features/dia/diarig2d/pose-blending.md) | Done | Uses inline Affine2x3 helpers in anonymous namespace |
| 5 | **BlendPoses** — `BlendPoses.h` / `BlendPoses.cpp`: shortest-arc rotation lerp, clamped t, bone count validation | [pose-blending.md](../../features/dia/diarig2d/pose-blending.md) | Done | |
| 6 | **JSON loader** — `ISkeletonLoader.h` interface + `SkeletonJson.h/cpp` default impl. Bool return for data errors, Rig2D channel logging. | [json-skeleton-definitions.md](../../features/dia/diarig2d/json-skeleton-definitions.md) | Done | Loader behind ISkeletonLoader interface per user request; metadata in explicit "metadata" JSON block |
| 7 | **Test utilities** — `Testing/SkeletonBuilders.h/cpp`, `Testing/PoseComparison.h/cpp`. MakeSimpleChain, MakeHumanoid, MakeBranching, PosesAreEqual, BoneTransformsAreEqual. | [test-utilities.md](../../features/dia/diarig2d/test-utilities.md) | Done | Ships with DiaRig2D library |
| 8 | **Skeleton component** — `SkeletonComponent.h` / `SkeletonComponent.cpp`: IComponent wrapper owning Skeleton + Pose, DynamicComponentFactory registration, ResetToBindPose | [skeleton-component.md](../../features/dia/diarig2d/skeleton-component.md) | Done | COMPONENT_DECLARATION(0x52494730) |
| 9 | **Unit tests** — 6 test files (44 tests): TestBone(4), TestSkeleton(9), TestPose(7), TestBlendPoses(7), TestSkeletonJson(10), TestSkeletonComponent(7). JSON fixtures in Fixtures/. | All features | Done | All 44 pass Debug + Release x64 |
| 10 | **Debug renderer project** — `Dia/DiaRig2DVisualDebugger/` with own vcxproj, refs DiaRig2D + DiaGraphics | [skeleton-debug-renderer.md](../../features/dia/diarig2d/skeleton-debug-renderer.md) | Done | GUID {F5A6B7C8-D9E0-1234-BCDE-56789ABCDEF1} |
| 11 | **Debug renderer implementation** — `VisualDebugger.h/cpp`: bones as white lines, root as green circle, joints as yellow circles. On/off + bone names toggle. | [skeleton-debug-renderer.md](../../features/dia/diarig2d/skeleton-debug-renderer.md) | Done | Text rendering deferred (FrameData may not support text primitives) |
| 12 | **Build verification** — Debug + Release x64 build and test. | All features | Done | Both configs build clean; all 44 Rig2D tests pass in both |

## File Layout (Target)

```
Dia/DiaRig2D/
  Bone.h
  BoneTransform.h
  Skeleton.h
  Skeleton.cpp
  Pose.h
  Pose.cpp
  BlendPoses.h
  BlendPoses.cpp
  SkeletonJson.h
  SkeletonJson.cpp
  SkeletonComponent.h
  SkeletonComponent.cpp
  Testing/
    SkeletonBuilders.h
    SkeletonBuilders.cpp
    PoseComparison.h
    PoseComparison.cpp
  Docs/
    dia.rig2d.architecture.module.md
  DiaRig2D.vcxproj
  DiaRig2D.vcxproj.filters

Dia/DiaRig2DVisualDebugger/
  VisualDebugger.h
  VisualDebugger.cpp
  Docs/
    dia.rig2dvisualdebugger.architecture.module.md
  DiaRig2DVisualDebugger.vcxproj
  DiaRig2DVisualDebugger.vcxproj.filters

Cluiche/Tests/GoogleTests/Rig/
  TestBone.cpp
  TestSkeleton.cpp
  TestPose.cpp
  TestBlendPoses.cpp
  TestSkeletonJson.cpp
  TestSkeletonComponent.cpp
  Fixtures/
    simple_chain_3.json
    humanoid.json
    branching.json
    invalid_parent.json
    missing_position.json
```

## Session Notes

### 2026-05-01
- Plan created. All 6 feature specs are Approved. No code exists yet.
- DiaSoftBody2D project structure reviewed as reference for vcxproj patterns (include dirs, ProjectReference, filters, sln registration).
- All 12 tasks implemented and verified in a single session.
- Design changes during implementation:
  - JSON loader placed behind `ISkeletonLoader` interface (user request for pluggable serialization)
  - Bone struct extended with `MetadataValue` variant (bool/int/float/string) and `MetadataEntry` bag (key-value pairs with StringCRC keys) for cross-system extensibility
  - Metadata stored in explicit `"metadata"` JSON block (not flat on bone node)
  - `kMaxBones` reduced from 256 to 128 and `kMaxMetadataPerBone` from 16 to 8 to avoid stack overflow (StringCRC is 64 bytes, making large DynamicArrayC<Bone> exceed 1MB stack)
- RGBA static constants fix: `RGBA::Green` not `RGBA::Green()` — these are `static const` members, not functions
- FK uses inline `Affine2x3` struct (anonymous namespace) to avoid dependency on a full Matrix class
- All 44 Rig2D unit tests pass in both Debug and Release x64
- Text rendering in VisualDebugger deferred — toggle exists but FrameData may not support text primitives

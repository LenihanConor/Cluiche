# DiaCamera3D — Implementation Plan

**Spec:** @docs/specs/systems/dia/diacamera3d.md
**Status:** In Progress

---

## Project Structure

```
Dia/DiaCamera3D/
├── Camera3D.h                            ← value types: Camera3D, PerspectiveParams, OrthoParams, ProjectionType
├── ViewportTransform3D.h / .cpp          ← view+proj matrices, WorldToScreen, ScreenToWorldRay, ExtractFrustum
├── Registry/
│   ├── CameraRegistry3D.h / .cpp         ← named registry, active tracking, behaviour management, UpdateAll
│   ├── ICameraBehaviour3D.h              ← Update(Camera3D&, float dt) contract
│   └── CameraBehaviourRegistry3D.h/.cpp  ← self-registering factory
├── Behaviours/
│   ├── Follow3D.h / .cpp
│   ├── SmoothDamp3D.h / .cpp
│   ├── BoundsClamp3D.h / .cpp
│   ├── ScreenShake3D.h / .cpp
│   ├── Orbit.h / .cpp
│   └── Flythrough.h / .cpp
├── Health/
│   └── CameraRegistryHealth3D.h / .cpp
├── Testing/
│   └── CameraBuilder3D.h                 ← fluent test helper
├── dia.camera3d.architecture.module.md
└── DiaCamera3D.vcxproj / .vcxproj.filters
```

## Implementation Patterns

### Namespace
All types in `Dia::Camera3D::`. Matches AD-003.

### Registry Pattern
Mirrors `CameraRegistry2D`. Fixed-capacity internal storage:
- `HashTable<StringCRC, CameraSlot>` (max 8 cameras)
- `CameraSlot` holds `Camera3D` + `DynamicArrayC<ICameraBehaviour3D*, 8>`
- `UpdateAll()` ticks behaviours in attachment order
- `GetActive()` asserts if no active camera set

### Behaviour Factory
Self-registration via static initialiser in each `.cpp`:
```cpp
static bool sRegistered = [] {
    Dia::Camera3D::CameraBehaviourRegistry3D::Get().Register(
        "Follow3D"_crc,
        [](const void* cfg) -> ICameraBehaviour3D* { return new Follow3D(); });
    return true;
}();
```

### ViewportTransform3D
Constructed from `(Camera3D&, Vector2D windowSize)`. Aspect derived from windowSize.
- View matrix: built from position + quaternion (identity = -Z forward, +Y up)
- Projection matrix: perspective (fovY, aspect, near, far) or ortho (width, height, near, far)
- `WorldToScreen`: NDC → pixel; returns false if w ≤ 0 (behind camera)
- `ScreenToWorldRay`: unproject pixel → origin + normalised direction

### Orbit / Flythrough Input
Both expose setters only (`SetInput(yaw, pitch, radiusDelta)` / `SetLookInput`, `SetMoveInput`). Never read DiaInput directly — application drives via game input handlers.

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `Dia/DiaCamera3D/` library: `DiaCamera3D.vcxproj` (static lib, refs DiaCore+DiaMaths+DiaGeometry3D+DiaObservation), `.vcxproj.filters`, `dia.camera3d.architecture.module.md`; register in `Cluiche.sln` | Project builds (no source files yet) | Done | haiku | 6194 tests pass |
| 2 | Implement `Camera3D.h` value type: `PerspectiveParams`, `OrthoParams`, `ProjectionType` enum, `Camera3D` struct (position, orientation, projectionType, params) — header only | Header compiles cleanly | Done | haiku | 6194 tests pass |
| 3 | Implement `ViewportTransform3D.h/.cpp`: constructor, `GetViewMatrix`, `GetProjectionMatrix`, `WorldToScreen`, `ScreenToWorldRay`, `ExtractFrustum` | `TestViewportTransform3D.cpp` — round-trip, behind-camera false, frustum corners | Done | sonnet | 6194 tests pass; Angle::FromDegrees (not Degrees) |
| 4 | Implement `ICameraBehaviour3D.h` + `CameraBehaviourRegistry3D.h/.cpp` (self-registering factory, kMaxBehaviourTypes=16) | `TestCameraBehaviourRegistry3D.cpp` — register, create, unknown type returns nullptr | Done | sonnet | 6194 tests pass |
| 5 | Implement `CameraRegistry3D.h/.cpp` (Register, Unregister, Has, AttachBehaviour, DetachBehaviour, SetActive, GetActive/Id, Get const+mut, UpdateAll) | `TestCameraRegistry3D.cpp` — CRUD, active camera, behaviour dispatch order | Done | sonnet | 6194 tests pass |
| 6 | Implement `Follow3D` + `SmoothDamp3D` behaviours; self-register on startup | Tests in `TestBehaviours3D.cpp` — target tracking, smoothing convergence | Not Started | sonnet | |
| 7 | Implement `BoundsClamp3D` (AABB clamp, no-op for zero-volume) + `ScreenShake3D` (trauma-based additive position + orientation, attach last) | Tests — clamp boundary, shake decays to zero | Not Started | sonnet | |
| 8 | Implement `Orbit` (yaw/pitch/radius, SetInput) + `Flythrough` (SetLookInput/SetMoveInput free-look) behaviours | Tests — orbit wraps yaw, pitch clamped, flythrough position integrates | Not Started | sonnet | |
| 9 | Create `Testing/CameraBuilder3D.h` fluent helper (`WithPosition`, `WithQuaternion`, `Perspective`, `Orthographic`, `Build`) | Used by registry and behaviour tests | Not Started | haiku | |
| 10 | Write full GoogleTest suite: `TestCamera3D.cpp`, `TestViewportTransform3D.cpp`, `TestCameraRegistry3D.cpp`, `TestBehaviours3D.cpp`; add files + `DiaCamera3D.lib` linker dep + ProjectReference to `GoogleTests.vcxproj` | `dia run googletest --filter="DiaCamera3D*"` all pass | Not Started | sonnet | |
| 11 | Implement `CameraRegistryHealth3D` (Degraded if no active camera, Failing if UpdateAll ticked with zero cameras); add DiaObservation logs + traces + metrics (`dia.camera3d.count`, `dia.camera3d.behaviour_ticks`) | Health state transitions correct in tests | Not Started | sonnet | |
| 12 | Run `dia docs registry` to regenerate module-registry.md; run `dia docs precommit` to verify all invariants | No violations reported | Not Started | haiku | |

# System Spec: DiaCamera3D

## Parent Application
@docs/specs/applications/dia/dia.md

**Status:** `Done`

---

## Purpose

DiaCamera3D is the engine library for 3D camera management. It owns the `Camera3D` value type (position + quaternion orientation + projection), `ViewportTransform3D` (world↔screen conversion + frustum extraction), `CameraRegistry3D` (named registry with composable per-camera behaviours), `ICameraBehaviour3D` interface, `CameraBehaviourRegistry3D` factory, and 6 engine-provided behaviours.

DiaCamera3D is an **independent peer of DiaScene3D** — the registry is fully usable without any scene file. Scene loading is an optional data-driven path that populates the registry; the programmatic path always works.

**Dependency chain:**
`DiaCamera3D → DiaGeometry3D → DiaMaths → DiaCore`

---

## Responsibilities

- Own `Camera3D` value type: position (Vector3D), orientation (Quaternion), projection (Perspective or Orthographic)
- Own `ViewportTransform3D`: view + projection matrix pair, WorldToScreen, ScreenToWorldRay, frustum extraction
- Own `CameraRegistry3D`: named registry for Camera3D instances, active camera tracking, composable per-camera behaviours
- Own `ICameraBehaviour3D` interface: `Update(Camera3D&, float dt)` tick contract
- Own `CameraBehaviourRegistry3D`: self-registering factory for behaviour types
- Provide 6 engine behaviours: Follow3D, SmoothDamp3D, BoundsClamp3D, ScreenShake3D, Orbit, Flythrough
- Provide `CameraBuilder3D` test helper in `Dia/DiaCamera3D/Testing/`
- Provide `CameraRegistryHealth3D` health reporter
- Provide `DiaCamera3D.vcxproj` static library project registered in `Cluiche.sln`
- Provide `dia.camera3d.architecture.module.md` YAML module documentation

## Non-Responsibilities

- Scene loading or entity hydration (DiaScene3D)
- Frustum culling (DiaScene3D — it receives the frustum from ViewportTransform3D)
- Euler angle helpers — orientation is Quaternion; Euler authoring tools are a future convenience, not v1
- Rendering / draw calls (DiaBgfx3D)
- Window management (DiaWindow / DiaSDL)
- Input handling (DiaInput) — behaviours expose setters driven by application input, they never read input directly
- 2D cameras (DiaCamera2D — independent peer)
- PU/Module integration (application-side)

---

## Public Interfaces

### `Camera3D`

```cpp
namespace Dia::Camera3D
{
    struct PerspectiveParams
    {
        float fovY  = 60.0f;    // degrees
        float nearZ = 0.1f;
        float farZ  = 1000.0f;
    };

    struct OrthoParams
    {
        float width  = 10.0f;
        float height = 10.0f;
        float nearZ  = -100.0f;
        float farZ   =  100.0f;
    };

    enum class ProjectionType : uint8_t { Perspective, Orthographic };

    struct Camera3D
    {
        Dia::Maths::Vector3D   position;
        Dia::Maths::Quaternion orientation;    // identity = looking down -Z, up is +Y
        ProjectionType         projectionType = ProjectionType::Perspective;
        PerspectiveParams      perspective;    // active when projectionType == Perspective
        OrthoParams            ortho;          // active when projectionType == Orthographic
    };
}
```

### `ViewportTransform3D`

```cpp
namespace Dia::Camera3D
{
    class ViewportTransform3D
    {
    public:
        // windowSize drives aspect ratio for Perspective cameras.
        ViewportTransform3D(const Camera3D& camera, Dia::Maths::Vector2D windowSize);

        // Project world point to screen pixel. Returns false if behind the camera.
        bool WorldToScreen(Dia::Maths::Vector3D world,
                           Dia::Maths::Vector2D& outScreen) const;

        // Unproject screen pixel to world-space ray (normalised direction).
        void ScreenToWorldRay(Dia::Maths::Vector2D screen,
                              Dia::Maths::Vector3D& outOrigin,
                              Dia::Maths::Vector3D& outDirection) const;

        // Extract view frustum for callers that need it (e.g. DiaScene3D culling).
        Dia::Geometry3D::Frustum ExtractFrustum() const;

        const Dia::Maths::Matrix44& GetViewMatrix()       const;
        const Dia::Maths::Matrix44& GetProjectionMatrix() const;
    };
}
```

### `CameraRegistry3D`

```cpp
namespace Dia::Camera3D
{
    class CameraRegistry3D
    {
    public:
        static constexpr unsigned int kMaxCameras    = 8;
        static constexpr unsigned int kMaxBehaviours = 8;

        // Registration
        bool Register(Dia::Core::StringCRC id, const Camera3D& camera);
        void Unregister(Dia::Core::StringCRC id);
        bool Has(Dia::Core::StringCRC id) const;

        // Behaviour management (registry owns behaviours)
        bool AttachBehaviour(Dia::Core::StringCRC cameraId, ICameraBehaviour3D* behaviour);
        void DetachBehaviour(Dia::Core::StringCRC cameraId, Dia::Core::StringCRC behaviourTypeId);

        // Active camera
        void                  SetActive(Dia::Core::StringCRC id);
        const Camera3D&       GetActive() const;
        Dia::Core::StringCRC  GetActiveId() const;

        // Direct access
        const Camera3D&       Get(Dia::Core::StringCRC id) const;
        Camera3D&             Get(Dia::Core::StringCRC id);

        // Tick all cameras' behaviours in attachment order
        void UpdateAll(float dt);
    };
}
```

### `ICameraBehaviour3D`

```cpp
namespace Dia::Camera3D
{
    class ICameraBehaviour3D
    {
    public:
        virtual Dia::Core::StringCRC GetTypeId() const = 0;
        virtual void Update(Camera3D& camera, float dt) = 0;
    };
}
```

### `CameraBehaviourRegistry3D`

```cpp
namespace Dia::Camera3D
{
    class CameraBehaviourRegistry3D
    {
    public:
        using FactoryFn = ICameraBehaviour3D* (*)(const void* config);

        static constexpr unsigned int kMaxBehaviourTypes = 16;

        static CameraBehaviourRegistry3D& Get();

        void                Register(Dia::Core::StringCRC typeId, FactoryFn factory);
        ICameraBehaviour3D* Create(Dia::Core::StringCRC typeId, const void* config = nullptr) const;
        bool                IsRegistered(Dia::Core::StringCRC typeId) const;
    };
}
```

### Engine Behaviours (v1)

| Class | 2D Analogue | Description |
|---|---|---|
| `Follow3D` | FollowBehaviour | Track a target Vector3D with configurable offset |
| `SmoothDamp3D` | SmoothDampBehaviour | Exponential position smoothing toward a target |
| `BoundsClamp3D` | BoundsClampBehaviour | Clamp camera position within an AABB; no-op if zero-volume |
| `ScreenShake3D` | ScreenShakeBehaviour | Trauma-based additive position + orientation shake; attach last |
| `Orbit` | — | Rotate around a target point: yaw, pitch, radius; SetInput(yaw, pitch, radiusDelta) |
| `Flythrough` | — | Free-look: SetLookInput(yaw, pitch) drives orientation; SetMoveInput(Vector3D) drives position |

Orbit and Flythrough have no 2D analogues. Both expose setter methods for application-driven input — they never read DiaInput directly.

---

## Dependencies

| Dependency | What's used |
|---|---|
| DiaMaths | Vector3D, Quaternion, Matrix44 |
| DiaGeometry3D | Frustum (ViewportTransform3D), AABB (BoundsClamp3D) |
| DiaCore | StringCRC, DynamicArrayC, HashTable |
| DiaObservation | Health (CameraRegistryHealth3D), Metrics (counter/gauge for camera count, behaviour ticks) |

### Does NOT depend on

- DiaGraphics / DiaGraphics3D (no FrameData, no rendering concepts)
- DiaBgfx / DiaBgfx3D (no renderer code)
- DiaScene3D (registry usable without scene)
- DiaEntity (no entity coupling)
- DiaApplicationFlow (no PU/Module coupling)
- DiaInput (behaviours receive input via setters, not directly)

---

## Features

| Feature | Description | Spec |
|---|---|---|
| `camera3d-types` | Camera3D value type, PerspectiveParams, OrthoParams, ViewportTransform3D | TBD |
| `camera3d-registry` | CameraRegistry3D, ICameraBehaviour3D, CameraBehaviourRegistry3D | TBD |
| `camera3d-behaviours` | 6 engine behaviours: Follow3D, SmoothDamp3D, BoundsClamp3D, ScreenShake3D, Orbit, Flythrough | TBD |

---

## Inherited Binding Decisions

| ID | Decision | How it applies |
|---|---|---|
| PD-001 | StringCRC for all IDs | Camera IDs, behaviour type IDs |
| PD-004 | No STL containers in public APIs | All arrays are DynamicArrayC; registry uses fixed-capacity internal storage |
| PD-005 | x64 only | Single build target |
| PD-007 | C++20 required | Module uses C++20 features |
| PD-008 | Directory.Build.props owns build paths | Library output to `bin/sharedlibs/<Config>/<Platform>/` |
| AD-001 | Module system with YAML frontmatter | `dia.camera3d.architecture.module.md` required |
| AD-002 | No STL in public APIs | Same as PD-004 |
| AD-003 | Namespace convention `Dia::<Module>::` | `Dia::Camera3D::` namespace |

---

## Resolved Design Questions

1. **Quaternion orientation** — Camera3D stores a Quaternion (identity = looking down -Z, up is +Y). Euler-angle helpers are a future tooling convenience and not in v1 scope; the struct is forward-compatible with a utility layer.

2. **Both Perspective and Orthographic** — Projection is a tagged pair (ProjectionType enum + PerspectiveParams/OrthoParams). Orthographic is needed for debug overlays, isometric views, and UI cameras. Only one set of params is active at a time.

3. **Aspect ratio is caller-supplied to ViewportTransform3D** — Camera3D stores fovY/near/far but not aspect, because aspect is window-derived. ViewportTransform3D accepts windowSize and computes aspect from it.

4. **ScreenToWorldRay returns a ray, not a point** — Unlike 2D where you unproject to a world point, 3D unprojection produces a ray (origin + normalised direction). WorldToScreen projects 3D → 2D with a validity bool for the behind-camera case.

5. **Registry is independent of DiaScene3D** — Same policy as DiaCamera2D. The registry is fully usable without any scene file; DiaScene3D populates it at load time as an optional data-driven path.

6. **Behaviours receive input via setters** — Flythrough and Orbit expose `SetLookInput(yaw, pitch)` and `SetMoveInput(Vector3D)`. They never read DiaInput directly.

---

## Resolved Design Questions (continued)

11. **One active camera** — DiaCamera3D supports exactly one active camera at a time, matching DiaCamera2D. No split-screen or picture-in-picture in v1.

12. **Two separate Camera3D types — no aliasing** — `DiaCamera3D::Camera3D` (live sim type: position + quaternion + projection params) and `DiaGraphics3D::Camera3D` (render snapshot: pre-baked view + projection matrices) serve different purposes and belong in different layers. DiaCamera3D does not depend on DiaGraphics3D. The bridge is `ViewportTransform3D`: it produces `Matrix44` view and projection matrices from the live camera, and the frame-building layer (DiaScene3D or a sim module) copies them into `DiaGraphics3D::Camera3D` before calling `Mesh3DFrameData::SetCamera()`. This keeps the type-seam isolation of DiaGraphics3D intact — 2D-only games never pull in DiaCamera3D.

---

## Status

`Approved` — plan: @docs/specs/applications/dia/systems/diacamera3d/diacamera3d.plan.md

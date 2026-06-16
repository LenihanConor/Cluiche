# System Spec: DiaCamera2D

## Parent Application
@docs/specs/applications/dia/dia.md

**Status:** `Done`

**Research:** @docs/research/diascene2d/design-decisions.md (sections 5, 9)

---

## Purpose

DiaCamera2D is the engine library for 2D camera management. It owns the Camera2D value type, a named camera registry, the camera behaviour interface with self-registering factory, and a set of engine-provided behaviours (Follow, SmoothDamp, Deadzone, BoundsClamp, ScreenShake, ZoomToFit, Pan, Zoom).

The system is a **library** — it has no knowledge of ProcessingUnits or Modules. Application-side code (e.g., `Camera2DModule` in CluicheGameBaseline) owns the registry instance, ticks it per frame, and exposes it to other modules.

### Key design principle

The registry is usable without any scene file or DiaScene2D dependency. Code can construct, register, and tick cameras programmatically. DiaScene2D is an optional data-driven path that populates the same registry from a `.diascene` file.

```
DiaCamera2D (engine library)
    ↑ used by
Camera2DModule (application-side PU module — owns registry, calls UpdateAll)
    ↑ used by
DiaScene2D SceneLoader (optional — populates registry from file)
```

**Dependency chain:**
`DiaCamera2D → DiaGeometry2D → DiaMaths → DiaCore`

---

## Responsibilities

- Own `Camera2D` value type (position, zoom, rotation)
- Own `ViewportTransform` (screen↔world coordinate conversion)
- Provide `CameraRegistry2D` — named camera storage with active-camera management
- Define `ICameraBehaviour` interface with `Update(Camera2D&, float dt)`
- Provide `CameraBehaviourRegistry` — self-registering factory for behaviour creation
- Provide `CameraRegistry2D::UpdateAll(float dt)` — ticks all registered cameras' behaviours
- Ship 8 engine-provided behaviours: Follow, SmoothDamp, Deadzone, BoundsClamp, ScreenShake, ZoomToFit, Pan, Zoom
- Provide `DiaCamera2D.vcxproj` static library project registered in `Cluiche.sln`
- Provide `dia.camera2d.architecture.module.md` YAML module documentation

## Non-Responsibilities

- PU/Module integration (application-side Camera2DModule)
- Input handling or input→behaviour wiring (application-side)
- Scene file loading (DiaScene2D)
- 3D cameras (future DiaCamera3D — independent peer, not child)
- Rendering (DiaBgfx consumes Camera2D via FrameData)
- Window size management (application-side)

---

## Public Interfaces

### Camera2D (migrated from DiaGraphics)

```cpp
namespace Dia::Camera2D
{
    class Camera2D
    {
    public:
        Camera2D();
        explicit Camera2D(Dia::Maths::Vector2D position, float zoom = 1.0f, float rotation = 0.0f);

        const Dia::Maths::Vector2D& GetPosition() const;
        float GetZoom() const;
        float GetRotation() const;

        void SetPosition(const Dia::Maths::Vector2D& position);
        void SetZoom(float zoom);
        void SetRotation(float rotation);

    private:
        Dia::Maths::Vector2D mPosition;
        float mZoom;
        float mRotation;
    };
}
```

### ViewportTransform (migrated from DiaGraphics)

```cpp
namespace Dia::Camera2D
{
    class ViewportTransform
    {
    public:
        ViewportTransform(const Camera2D& camera, const Dia::Maths::Vector2D& windowSize);

        Dia::Maths::Vector2D ScreenToWorld(const Dia::Maths::Vector2D& pixel) const;
        Dia::Maths::Vector2D WorldToScreen(const Dia::Maths::Vector2D& world) const;
        Dia::Geometry2D::AARect GetWorldBounds() const;
    };
}
```

### CameraRegistry2D

```cpp
namespace Dia::Camera2D
{
    class CameraRegistry2D
    {
    public:
        void Register(Dia::Core::StringCRC id, Camera2D camera);
        void Unregister(Dia::Core::StringCRC id);

        void AttachBehaviour(Dia::Core::StringCRC cameraId, ICameraBehaviour* behaviour);
        void DetachBehaviour(Dia::Core::StringCRC cameraId, Dia::Core::StringCRC behaviourTypeId);

        void SetActive(Dia::Core::StringCRC id);
        Camera2D& GetActive();
        const Camera2D& GetActive() const;
        Camera2D& Get(Dia::Core::StringCRC id);

        void UpdateAll(float dt);
    };
}
```

### ICameraBehaviour

```cpp
namespace Dia::Camera2D
{
    class ICameraBehaviour
    {
    public:
        virtual ~ICameraBehaviour() = default;
        virtual Dia::Core::StringCRC GetTypeId() const = 0;
        virtual void Update(Camera2D& camera, float dt) = 0;
    };
}
```

### CameraBehaviourRegistry (factory)

```cpp
namespace Dia::Camera2D
{
    using BehaviourFactory = ICameraBehaviour* (*)(const ReflectedConfig& config);

    class CameraBehaviourRegistry
    {
    public:
        static CameraBehaviourRegistry& Get();

        void Register(Dia::Core::StringCRC typeId, BehaviourFactory factory);
        ICameraBehaviour* Create(Dia::Core::StringCRC typeId, const ReflectedConfig& config);
    };
}
```

### Engine-provided behaviours

| Behaviour | Setter | What it does |
|---|---|---|
| `FollowBehaviour` | `SetTarget(Vec2)` | Track a target position with configurable offset |
| `SmoothDampBehaviour` | — (automatic) | Exponential smoothing on position changes |
| `DeadzoneBehaviour` | — (automatic) | Don't move until target leaves central region |
| `BoundsClampBehaviour` | `SetBounds(AARect)` | Clamp camera within world bounds |
| `ScreenShakeBehaviour` | `Trigger(float trauma)` | Additive trauma-based decaying shake |
| `ZoomToFitBehaviour` | `SetTargets(Span<Vec2>)` | Auto-zoom to keep N targets in view |
| `PanBehaviour` | `SetDelta(Vec2)` | Move camera position by a delta |
| `ZoomBehaviour` | `SetInput(float scrollDelta)` | Scale zoom by factor, clamp to min/max |

All behaviours:
- Self-register their factory via static initialization
- Are composable and ordered (Follow runs before SmoothDamp, ScreenShake is additive last)
- Don't read input directly — expose setters that the application feeds

---

## Dependencies

| Dependency | What's used |
|---|---|
| DiaMaths | Vector2D, math utilities |
| DiaGeometry2D | AARect (for ViewportTransform::GetWorldBounds, BoundsClamp) |
| DiaCore | StringCRC, containers (HashTable, DynamicArrayC) |

### Does NOT depend on

- DiaGraphics (Camera2D *moves out* of DiaGraphics into this module)
- diaentitytemplate
- DiaScene2D
- DiaApplicationFlow
- DiaBgfx

---

## Migration

Camera2D and ViewportTransform currently live in `Dia/DiaGraphics/Camera/`. They move to `Dia/DiaCamera2D/`. DiaGraphics will depend on DiaCamera2D for the Camera2D type (used in FrameData).

Existing code referencing `Dia::Graphics::Camera2D` and `Dia::Graphics::ViewportTransform` will need namespace updates to `Dia::Camera2D::Camera2D` and `Dia::Camera2D::ViewportTransform`.

---

## Features

| Feature | Description | Spec |
|---|---|---|
| camera2d-system | Camera2D type, registry, behaviour interface/factory, 8 engine behaviours, ViewportTransform migration | TBD |

**Plan:** [diacamera2d.plan.md](diacamera2d.plan.md)

---

## Inherited Binding Decisions

| ID | Decision | How it applies |
|---|---|---|
| PD-001 | StringCRC for all IDs | Camera IDs, behaviour type IDs are all StringCRC |
| PD-004 | No STL containers in public APIs | Registry uses DynamicArrayC/HashTable |
| PD-005 | x64 only | Single build target |
| PD-007 | C++20 required | Module uses C++20 features |
| PD-008 | Directory.Build.props owns build paths | Library outputs to `bin/sharedlibs/<Config>/<Platform>/` |
| AD-001 | Module system with YAML frontmatter | `dia.camera2d.architecture.module.md` required |
| AD-002 | No STL in public APIs | Same as PD-004 |
| AD-003 | Namespace convention `Dia::<Module>::` | `Dia::Camera2D::` namespace |

---

## Resolved Design Questions

1. **FrameData dependency** — DiaGraphics depends on DiaCamera2D for the Camera2D type. FrameData continues embedding Camera2D directly (`#include <DiaCamera2D/Camera2D.h>`). DiaCamera2D's dependency chain is lightweight (DiaMaths, DiaGeometry2D) so this adds no heavyweight pull.

2. **Behaviour ordering** — Defined by attachment order. Behaviours tick in the order they were attached to a camera. Simpler and deterministic for the common case. If detach/re-attach reordering is needed, caller detaches all and re-attaches in desired order.

3. **Behaviour ownership** — Registry owns behaviours. `Unregister(cameraId)` and registry destruction delete all attached behaviours. Caller creates via factory, registry takes ownership.

---

## Status

`Approved`

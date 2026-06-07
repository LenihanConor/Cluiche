# System Spec: DiaLighting3D

## Parent Application
@docs/specs/applications/dia.md

**Status:** `Draft`

---

## Purpose

DiaLighting3D is the engine library for 3D light management. It owns four light value types (`PointLight3D`, `DirectionalLight3D`, `SpotLight3D`, `AmbientLight3D`), `LightRegistry3D` (named registry with per-type storage and query), `ILightBehaviour3D` interface, `LightBehaviourRegistry3D` factory, and 3 engine behaviours (Flicker, Pulse, ColorCycle).

DiaLighting3D is an **independent peer of DiaScene3D** — the registry is fully usable without any scene file. Scene loading is an optional data-driven path that populates the registry; the programmatic path always works.

No layer masks — 3D has no parallax layer system. Lights affect all geometry unless the renderer implements culling (that belongs in DiaBgfx3D, not here).

**Dependency chain:**
`DiaLighting3D → DiaMaths → DiaCore`

---

## Responsibilities

- Own four light value types: `PointLight3D`, `DirectionalLight3D`, `SpotLight3D`, `AmbientLight3D`
- Own `LightRegistry3D`: named registry with per-type storage, indexed iteration, query by type
- Own `ILightBehaviour3D` interface: `Update(float dt)` tick contract (behaviours mutate their owning light slot)
- Own `LightBehaviourRegistry3D`: self-registering factory for behaviour types
- Provide 3 engine behaviours: Flicker, Pulse, ColorCycle
- Provide `LightBuilder3D` test helper in `Dia/DiaLighting3D/Testing/`
- Provide `DiaLighting3D.vcxproj` static library project registered in `Cluiche.sln`
- Provide `dia.lighting3d.architecture.module.md` YAML module documentation

## Non-Responsibilities

- Rendering / shadow maps / light volumes — DiaBgfx3D
- Light culling or spatial queries — DiaBgfx3D or DiaScene3D
- Scene loading or entity hydration — DiaScene3D
- Layer mask affinity — dropped (no parallax layers in 3D)
- PU/Module integration (application-side)
- 2D lights (DiaLighting2D — independent peer)

---

## Public Interfaces

### Light Value Types

```cpp
namespace Dia::Lighting3D
{
    struct PointLight3D
    {
        Dia::Maths::Vector3D position;
        float                radius    = 10.0f;
        Dia::Core::RGBA      colour    = { 255, 255, 255, 255 };
        float                intensity = 1.0f;
        bool                 enabled   = true;
    };

    struct DirectionalLight3D
    {
        Dia::Maths::Vector3D direction  = { 0.0f, -1.0f, 0.0f };  // unit vector, world space
        Dia::Core::RGBA      colour     = { 255, 255, 230, 255 };  // warm white default
        float                intensity  = 1.0f;
        bool                 enabled    = true;
    };

    struct SpotLight3D
    {
        Dia::Maths::Vector3D position;
        Dia::Maths::Vector3D direction  = { 0.0f, -1.0f, 0.0f };  // unit vector
        float                innerAngle = 15.0f;  // degrees — full-intensity cone
        float                outerAngle = 30.0f;  // degrees — falloff edge
        float                range      = 20.0f;
        Dia::Core::RGBA      colour     = { 255, 255, 255, 255 };
        float                intensity  = 1.0f;
        bool                 enabled    = true;
    };

    struct AmbientLight3D
    {
        Dia::Core::RGBA colour    = { 30, 30, 50, 255 };  // dim cool default
        float           intensity = 0.1f;
        bool            enabled   = true;
    };
}
```

### `LightRegistry3D`

```cpp
namespace Dia::Lighting3D
{
    class LightRegistry3D
    {
    public:
        static constexpr unsigned int kMaxPointLights       = 16;
        static constexpr unsigned int kMaxDirectionalLights =  4;
        static constexpr unsigned int kMaxSpotLights        = 16;
        static constexpr unsigned int kMaxBehaviours        =  4;  // per light slot

        // Registration — each type has its own namespace of IDs
        bool RegisterPoint      (Dia::Core::StringCRC id, const PointLight3D& light);
        bool RegisterDirectional(Dia::Core::StringCRC id, const DirectionalLight3D& light);
        bool RegisterSpot       (Dia::Core::StringCRC id, const SpotLight3D& light);
        void SetAmbient         (const AmbientLight3D& light);  // single ambient; always present

        void Unregister(Dia::Core::StringCRC id);   // works across all types
        bool Has       (Dia::Core::StringCRC id) const;

        // Direct access
        PointLight3D&       GetPoint      (Dia::Core::StringCRC id);
        DirectionalLight3D& GetDirectional(Dia::Core::StringCRC id);
        SpotLight3D&        GetSpot       (Dia::Core::StringCRC id);
        AmbientLight3D&     GetAmbient    ();

        // Indexed iteration (for renderer consumption)
        unsigned int           GetPointCount()                    const;
        const PointLight3D&    GetPointByIndex(unsigned int i)    const;
        unsigned int           GetDirectionalCount()              const;
        const DirectionalLight3D& GetDirectionalByIndex(unsigned int i) const;
        unsigned int           GetSpotCount()                     const;
        const SpotLight3D&     GetSpotByIndex(unsigned int i)     const;

        // Behaviour management (registry owns behaviours)
        bool AttachBehaviour(Dia::Core::StringCRC lightId, ILightBehaviour3D* behaviour);
        void DetachBehaviour(Dia::Core::StringCRC lightId, Dia::Core::StringCRC behaviourTypeId);

        // Tick all lights' behaviours
        void UpdateAll(float dt);
    };
}
```

### `ILightBehaviour3D`

```cpp
namespace Dia::Lighting3D
{
    class ILightBehaviour3D
    {
    public:
        virtual Dia::Core::StringCRC GetTypeId() const = 0;
        // dt in seconds; implementation mutates the light via a typed setter
        virtual void Update(float dt) = 0;
    };
}
```

Behaviours hold a pointer to their owning light (set at attach time). They mutate the light's `intensity` and/or `colour` directly. Attachment is type-agnostic — Flicker works on any light type that has `intensity`.

### `LightBehaviourRegistry3D`

```cpp
namespace Dia::Lighting3D
{
    class LightBehaviourRegistry3D
    {
    public:
        using FactoryFn = ILightBehaviour3D* (*)(const void* config);

        static constexpr unsigned int kMaxBehaviourTypes = 16;

        static LightBehaviourRegistry3D& Get();

        void                Register(Dia::Core::StringCRC typeId, FactoryFn factory);
        ILightBehaviour3D*  Create  (Dia::Core::StringCRC typeId, const void* config = nullptr) const;
        bool                IsRegistered(Dia::Core::StringCRC typeId) const;
    };
}
```

### Engine Behaviours (v1)

| Class | Description |
|---|---|
| `FlickerBehaviour3D` | Random intensity jitter within a range; configurable frequency and magnitude. `SetRange(min, max)`, `SetFrequency(hz)` |
| `PulseBehaviour3D` | Smooth sinusoidal intensity oscillation. `SetRange(min, max)`, `SetPeriod(seconds)` |
| `ColorCycleBehaviour3D` | Lerps colour through a fixed palette in sequence. `SetPalette(colours[], count)`, `SetPeriod(seconds)` |

---

## Dependencies

| Dependency | What's used |
|---|---|
| DiaMaths | Vector3D (positions, directions) |
| DiaCore | StringCRC, DynamicArrayC, RGBA |
| DiaObservation | Metrics (counter/gauge for light counts, behaviour ticks), Health reporter |

### Does NOT depend on

- DiaGraphics / DiaGraphics3D (no FrameData — renderer queries the registry directly)
- DiaBgfx / DiaBgfx3D
- DiaGeometry3D (no spatial queries — culling belongs in renderer)
- DiaScene3D (registry usable without scene)
- DiaEntity
- DiaApplicationFlow

---

## Relationship with DiaGraphics3D

DiaGraphics3D defines `DirectionalLight` and `PointLight` as **render snapshots** (flat colour + intensity + direction/position — no ID, no enabled flag, no behaviour). These are separate from `DiaLighting3D`'s live sim types for the same reason `DiaGraphics3D::Camera3D` is separate from `DiaCamera3D::Camera3D`.

The frame-building layer (DiaScene3D or a sim module) copies enabled lights from `LightRegistry3D` into `Mesh3DFrameData` before submission. `DiaLighting3D` has no dependency on `DiaGraphics3D`.

Note: DiaGraphics3D has no `SpotLight` or `AmbientLight` snapshot type yet. Those will need to be added to DiaGraphics3D when DiaBgfx3D implements spot/ambient support.

---

## Features

| Feature | Description | Spec |
|---|---|---|
| `lighting3d-types` | Four light value types, `LightRegistry3D`, `ILightBehaviour3D`, `LightBehaviourRegistry3D` | TBD |
| `lighting3d-behaviours` | 3 engine behaviours: FlickerBehaviour3D, PulseBehaviour3D, ColorCycleBehaviour3D | TBD |

---

## Inherited Binding Decisions

| ID | Decision | How it applies |
|---|---|---|
| PD-001 | StringCRC for all IDs | Light IDs, behaviour type IDs |
| PD-004 | No STL containers in public APIs | All arrays are DynamicArrayC; registry uses fixed-capacity internal storage |
| PD-005 | x64 only | Single build target |
| PD-007 | C++20 required | Module uses C++20 features |
| PD-008 | Directory.Build.props owns build paths | Library output to `bin/sharedlibs/<Config>/<Platform>/` |
| AD-001 | Module system with YAML frontmatter | `dia.lighting3d.architecture.module.md` required |
| AD-002 | No STL in public APIs | Same as PD-004 |
| AD-003 | Namespace convention `Dia::<Module>::` | `Dia::Lighting3D::` namespace |

---

## Resolved Design Questions

1. **No layer masks** — 3D has no parallax layer system. Layer masks are dropped entirely. Lights affect all geometry; selective influence (e.g. indoor vs outdoor) is the renderer's concern (light volumes, shadow masks in DiaBgfx3D).

2. **Four light types** — PointLight3D (omni), DirectionalLight3D (sun/sky), SpotLight3D (cone), AmbientLight3D (global fill). AmbientLight3D is a singleton on the registry (single ambient per scene); the others are named and registered.

3. **Two separate light type families** — DiaLighting3D sim types and DiaGraphics3D render snapshots are distinct, same pattern as DiaCamera3D / DiaGraphics3D. Frame-building layer bridges them. DiaLighting3D has no dep on DiaGraphics3D.

4. **Behaviours in v1** — Three example behaviours shipped: Flicker, Pulse, ColorCycle. Behaviours hold a typed pointer to their light and mutate `intensity` and/or `colour` directly.

5. **SpotLight/AmbientLight not in DiaGraphics3D yet** — DiaBgfx3D will need them as render snapshot types. Adding them to DiaGraphics3D is a follow-on feature spec when DiaBgfx3D implements those light types.

6. **Direction is world-space unit vector** — `DirectionalLight3D::direction` and `SpotLight3D::direction` are normalised world-space vectors. The convention (points away from surface, same as DiaGraphics3D::DirectionalLight) is documented on the field.

---

## Resolved Design Questions (continued)

7. **`Dia::Core::RGBA` for colour** — Light value types use `Dia::Core::RGBA`, not `Dia::Graphics::RGBA`. This keeps DiaLighting3D free of any DiaGraphics dependency — consistent with the sim/render split and the DiaLighting2D precedent.

---

## Status

`Approved`

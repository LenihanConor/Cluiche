# Feature Spec: LightPathBehaviour3D

## Parent System
@docs/specs/applications/dia/systems/dialighting3d/dialighting3d.md

**Status:** `Done`

---

## Summary

Add a `LightPathBehaviour3D` to DiaLighting3D — a behaviour that drives a light's world-space **position** along a 3D Catmull-Rom or B-Spline path at a configurable speed. Works with `PointLight3D` and `SpotLight3D` (both have a `position` field). Directional lights have no position; a companion `LightDirectionPathBehaviour3D` for sweeping `direction` is tracked as a future feature.

The behaviour conforms to the existing `ILightBehaviour3D` contract — attach it via `LightRegistry3D::AttachBehaviour()`, tick it with `UpdateAll(float dt)`. The speed of travel and loop mode (loop, ping-pong, once) are configurable at attach time.

---

## Goals

1. `LightPathBehaviour3D` class implementing `ILightBehaviour3D`
2. Follows a `Dia::Geometry3D::Spline3D` path (Catmull-Rom or B-Spline — caller chooses at construction)
3. Typed setter for position: works on `PointLight3D*` and `SpotLight3D*` — resolved at attach time via template helpers; compile-time error if attached to `DirectionalLight3D` or `AmbientLight3D`
4. Loop modes: `Loop` (restart at t=0), `PingPong` (reverse direction), `Once` (stop at t=1)
5. Self-registers with `LightBehaviourRegistry3D` on startup under `StringCRC("LightPathBehaviour3D")`
6. Lives in `Dia/DiaLighting3D/Behaviours/LightPathBehaviour3D.h+cpp`
7. Add new dependency: `DiaLighting3D → DiaGeometry3D` (for `Spline3D`)
8. GoogleTest coverage: position advances along curve, loop wraps, ping-pong reverses, type-mismatch rejected

## Non-Goals

- Driving `direction` along a path (future `LightDirectionPathBehaviour3D`)
- Arclength re-parameterisation (constant world-speed) — raw `t` advance at configurable rate
- Editor integration (DiaLighting3DVisualDebugger handles path preview)
- JSON/data-driven path loading — paths constructed in code for now

---

## Public API

```cpp
namespace Dia::Lighting3D
{
    class LightPathBehaviour3D : public ILightBehaviour3D
    {
    public:
        enum class LoopMode { Loop, PingPong, Once };

        struct Config
        {
            Dia::Geometry3D::Spline3D spline;
            float                     speed    = 0.5f;  // t units per second (full path in 2s at default)
            LoopMode                  loopMode = LoopMode::Loop;
        };

        // Constructors for supported light types
        explicit LightPathBehaviour3D(PointLight3D* light, const Config& config);
        explicit LightPathBehaviour3D(SpotLight3D*  light, const Config& config);

        // ILightBehaviour3D
        Dia::Core::StringCRC GetTypeId() const override;
        void Update(float dt) override;

        float    GetT()        const;           // current parameter [0,1]
        LoopMode GetLoopMode() const;

        static Dia::Core::StringCRC kTypeId;    // StringCRC("LightPathBehaviour3D")
    };
}
```

---

## Tasks

| # | Task | Test |
|---|------|------|
| 1 | Update `DiaLighting3D.vcxproj` ProjectReference to add `DiaGeometry3D`; update `dia.lighting3d.architecture.module.md` dependent_modules | Build passes, no circular dep |
| 2 | Implement `LightPathBehaviour3D` in `Dia/DiaLighting3D/Behaviours/LightPathBehaviour3D.h+cpp`; add to vcxproj | Build passes |
| 3 | Write `Cluiche/Tests/GoogleTests/DiaLighting3D/TestLightPathBehaviour3D.cpp`; add to `GoogleTests.vcxproj` | `dia run googletest --filter="DiaLighting3D_PathBehaviour*"` all pass |

---

## Dependencies Added

| Dependency | What's used |
|---|---|
| DiaGeometry3D | `Spline3D`, `SplineFactory3D` (for path evaluation) |

This is the only new dependency added to DiaLighting3D by this feature.

---

## Resolved Design Questions

1. **Typed position setter** — `ILightBehaviour3D::Update` takes only `float dt`, so the behaviour must hold a typed pointer set at construction. The two valid pointer types (`PointLight3D*`, `SpotLight3D*`) are resolved via two constructors (no virtual dispatch overhead). If a third positional light type is added later, a third constructor is needed — accepted right tradeoff vs a generic void*.

2. **t-rate vs world-speed** — `speed` is in `t` units per second, not world units per second. Fast/slow segments appear where control points are dense/sparse. Arclength re-parameterisation is the fix but adds cost. Deferred; flag in task notes if constant-speed is needed by a consumer.

3. **Scene is the source of truth.** Path control points, speed, and loop mode are declared in `.diascene` alongside the light entry. `SceneLoader3D` constructs the `LightPathBehaviour3D` and attaches it to `LightRegistry3D` at hydration time. Programmatic construction via the public constructors is also valid (e.g. test stages).

4. **`LightRegistry3D` exposes `GetPathBehaviour()`.** Add `LightPathBehaviour3D* GetPathBehaviour(Dia::Core::StringCRC lightId)` to `LightRegistry3D` so the visual debugger can query the attached behaviour for arc sampling without knowing the attachment internals.

---

## Inherited Binding Decisions

| ID | Decision | How it applies |
|---|---|---|
| PD-001 | StringCRC for IDs | `kTypeId = StringCRC("LightPathBehaviour3D")` |
| PD-004 | No STL in public API | Config passes Spline3D by value (fixed-size internal array) |
| AD-003 | Namespace `Dia::<Module>::` | `Dia::Lighting3D::LightPathBehaviour3D` |

---

## Status

`Approved`

# System Spec: DiaGraphics

## Parent Application
@docs/specs/applications/dia.md

**Status:** `Approved`

---

## Purpose

DiaGraphics is the rendering abstraction layer for the Dia engine. It defines the data structures and interfaces that bridge simulation-side frame construction with renderer-side frame consumption. The system is explicitly NOT a renderer — it owns no platform-specific code. Instead it defines the contracts (`ICanvas`, `FrameData`, draw command types) that renderer implementations (DiaSFML, future Vulkan etc.) must fulfill.

DiaGraphics sits between the simulation and the renderer:

```
Simulation (SimProcessingUnit, VisualDebuggers)
    ↓  fills FrameData
DiaGraphics (FrameData, DebugFrameData, EntityFrameData, UIFrameData)
    ↓  copied into frame stream
DiaSFML / future renderer (DebugFrameRendererVisitor, SpriteRenderer)
```

**Dependency chain:**
`DiaGraphics -> DiaMaths -> DiaCore`

---

## Responsibilities

- Define `FrameData` as a composable per-frame data packet (inherits `DebugFrameData`, `EntityFrameData`, `UIFrameData`)
- Define `DebugFrameData` — debug geometry storage with visitor dispatch
- Define `DebugPrimitive` — the canonical value type for all debug draw requests (tagged union; no heap allocation)
- Define `DebugFrameDataVisitor` — the abstract interface renderers implement to consume debug primitives
- Define `EntityFrameData` — sprite draw command storage (`SpriteDrawCommand`)
- Define `UIFrameData` — UI frame data storage
- Define `ICanvas` — abstract rendering surface interface
- Define `RGBA` — colour type used across all drawing APIs
- Own the `FrameStream` cross-thread transport mechanism for `FrameData`
- Provide `DiaGraphics.vcxproj` static library project registered in `Cluiche.sln`
- Provide `dia.graphics.architecture.module.md` YAML module documentation

## Non-Responsibilities

- Platform-specific rendering — DiaSFML, future Vulkan backend
- Window creation and management — DiaWindow
- Input handling — DiaInput
- UI system logic — DiaUI
- Physics or simulation state — DiaRigidBody2D / DiaSoftBody2D
- Persistent debug overlays across multiple frames — future DiaVisualDebuggerDraw system

---

## Public Interfaces

### FrameData

```cpp
namespace Dia::Graphics {
    class FrameData : public DebugFrameData, public UIFrameData, public EntityFrameData {
    public:
        FrameData();
        FrameData& operator=(const FrameData& rhs);
        void Clear();
        void Copy(const FrameData& rhs);
    };
}
```

### DebugFrameData

```cpp
namespace Dia::Graphics {
    class DebugFrameData {
    public:
        void ClearDebugBuffer();
        void CopyDebugBuffer(const DebugFrameData& rhs);

        void RequestDraw(const DebugFrameDataCircle2D& object);
        void RequestDraw(const DebugFrameDataLine2D& object);
        // ... one overload per primitive type

        void AcceptVisitor(const DebugFrameDataVisitor& visitor) const;
    };
}
```

### DebugFrameDataVisitor

```cpp
namespace Dia::Graphics {
    class DebugFrameDataVisitor {
    public:
        virtual void Visit(const DebugPrimitive& primitive) const = 0;
        virtual void Visit(const DebugFrameData& frameData) const = 0;
    };
}
```

---

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| debug-primitive-tagged-union | Replace per-type debug buffers with a single tagged-union `DebugPrimitive` buffer | [debug-primitive-tagged-union.md](../../features/dia/diagraphics/debug-primitive-tagged-union.md) | Done |
| texture-handle-stringcrc | Refactor `ITexture` to canonical asset-aware handle (StringCRC asset id + atomic ready state); `SpriteDrawCommand::textureId` (unsigned int) → `texture` (ITexture*); add `Dia::SFML::SfmlTexture` impl; coordinates with async-asset-loading per RB-009 | [texture-handle-stringcrc.md](../../features/dia/diagraphics/texture-handle-stringcrc.md) | Approved |
| graphics-3d-types | Phase 2 — moved to DiaGraphics3D system (G3D-001); `Camera3D`, lights, `Mesh3DDrawCommand`, `Mesh3DFrameData`, `FrameData3D` now live in `Dia/DiaGraphics3D/` under `Dia::Graphics3D::`. DiaGraphics::FrameData unchanged. | [graphics-3d-types.md](../../features/dia/diagraphics3d/graphics-3d-types.md) | Approved (re-homed) |

---

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| GD-001 | `DebugPrimitive` is a hand-rolled tagged union, not `std::variant` | PD-004 forbids STL in public APIs; tagged union is trivially copyable and has zero heap allocation | DiaGraphics debug subsystem | Accepted | Yes |
| GD-002 | `FrameData` copy must be trivially correct — no pointer members in debug buffers | `FrameData` is copied across the frame stream every tick; pointer-based designs are excluded | DiaGraphics | Accepted | Yes |
| GD-003 | Debug renderer is a separate concern — `DebugFrameData` stores data only, rendering is in DiaSFML | Keeps DiaGraphics platform-independent | DiaGraphics | Accepted | Yes |
| GD-004 | Debug primitives are stored in insertion order; renderer visits them in push order | Correct overlay ordering without per-type sorting passes | DiaGraphics debug subsystem | Accepted | Yes |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Scope | Should `ICanvas` be documented as a feature within this system spec? | Deferred — ICanvas is stable and unchanged by current work; add as a feature when it is next modified |
| 2 | FrameStream | Should the cross-thread FrameStream transport be a separate feature? | Deferred — it is unchanged by current work; capture as a feature when it is next modified |
| 3 | Future | Will a 3D debug primitive set be needed? | Not in scope for this system; DiaGraphics is 2D-focused today |

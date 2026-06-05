# Feature Spec: 2D Picking Service

**Parent:** @docs/specs/systems/dia/diapicking.md

**Status:** `Approved`
**Plan:** @docs/specs/features/dia/diapicking/geometry2d-picking.plan.md

---

## Problem Statement

The engine currently has no decoupled way to resolve "what 2D spatial object is at this world position?" Input handling, spatial query, result storage, and visual feedback are all collapsed into debug drawer classes (`HexGridDrawer`, `SpatialGridDrawer`), making them untestable, non-reusable, and broken (ImGui context dependency). Game systems (selection, targeting, tooltips, drag) all need the same fundamental query but currently have no engine-level service to call.

---

## Solution Overview

Introduce `DiaGeometry2DPicking` — a new vcxproj that provides:
1. **`IPickable2D`** — interface for anything that can answer "is this world point inside me?"
2. **`PickHit2D`** — typed result describing what was hit (hex cell, grid cell, object, entity)
3. **`PickingService2D`** — stateless query service that traverses registered pickables and returns sorted results
4. **Concrete adapters** — `HexGridPickable`, `SpatialGridPickable`, `ShapePickable` wrapping existing geometry types

Introduce `DiaPicking` — the picking protocol layer:
5. **`PickResult<THit>`** — sorted hit container (dimension-agnostic)
6. **`PickEvent<THit>`** — the message type sent through DiaMailbox
7. **`PickTrigger`** — what caused the pick (click, hover, right-click, area-select)
8. **`PickRouter`** — `IMailboxRouter` that routes events to subscribers by trigger type
9. **`PickAddress`** — helper to build DiaMailbox addresses for picking

At the application layer (CluicheGameBaseline):
10. **`Camera2DModule`** — non-debug owner of Camera2D + window size (extracted from VisualDebuggerModule)
11. **`InputStreamModule` extended** — mouse button state (pressed/released/down)
12. **`PickingModule`** — thin orchestrator: reads input, transforms screen→world, queries service, sends PickEvent via DiaMailbox

Consumers (test stages, future game modules) register pickables on start, subscribe to triggers via DiaMailbox, drain events and react.

---

## Acceptance Criteria

| ID | Criterion | Verification |
|---|---|---|
| AC1 | `DiaPicking.vcxproj` builds clean with `PickResult<T>`, `PickEvent<T>`, `PickTrigger`, `PickLayer`, `PickRouter`, `PickAddress` | Build check |
| AC2 | `DiaGeometry2DPicking.vcxproj` builds clean with `IPickable2D`, `PickHit2D`, `PickingService2D` | Build check |
| AC3 | `HexGridPickable<T,Max>` wraps `HexGrid` and correctly resolves point → hex coord | Unit test |
| AC4 | `SpatialGridPickable<T,Max>` wraps `SpatialGrid` and correctly resolves point → cell (x,y) | Unit test |
| AC5 | `ShapePickable` wraps Circle/AARect/ConvexPolygon and resolves point-in-shape | Unit test |
| AC6 | `PickingService2D::Pick(worldPos, mask)` returns hits sorted by priority (highest first) | Unit test |
| AC7 | `PickingService2D::PickArea(AARect, mask)` returns all pickables overlapping the rect | Unit test |
| AC8 | Layer mask filtering excludes pickables whose layer doesn't match | Unit test |
| AC9 | `PickEvent` sent via DiaMailbox is received by all subscribers to that trigger | Unit test |
| AC10 | `InputStreamModule` exposes `WasMouseButtonPressed`, `IsMouseButtonDown`, `WasMouseButtonReleased` | Unit test |
| AC11 | `Camera2DModule` provides `GetViewportTransform()` in non-debug builds | Build check (Release) |
| AC12 | `PickingModule` sends `PickEvent<PickHit2D>` to DiaMailbox on left-click | Integration test |
| AC13 | `Geometry2DTestStageModule` drains click events, stores `mSelectedHex`, passes to drawer | Visual verification |
| AC14 | `HexGridDrawer` no longer owns selection state or reads ImGui input | Code review |
| AC15 | Full solution builds clean (Debug x64) | `dia run googletest` |

---

## Public API

### IPickable2D

```cpp
// Dia/DiaGeometry2DPicking/IPickable2D.h
namespace Dia::Geometry2DPicking {

class IPickable2D {
public:
    virtual ~IPickable2D() = default;
    virtual Dia::Core::StringCRC GetPickableId() const = 0;
    virtual int GetPriority() const = 0;
    virtual Dia::Picking::PickLayer GetLayer() const = 0;

    virtual bool TryPick(const Dia::Maths::Vector2D& worldPos, PickHit2D& out) const = 0;
    virtual void PickArea(const Dia::Geometry2D::AARect& worldRect,
                          Dia::Core::Containers::DynamicArrayC<PickHit2D, kMaxAreaHits>& out) const = 0;
};

} // namespace Dia::Geometry2DPicking
```

### PickHit2D

```cpp
// Dia/DiaGeometry2DPicking/PickHit2D.h
namespace Dia::Geometry2DPicking {

struct PickHit2D {
    enum class Kind : unsigned int { kHexCell, kSpatialCell, kObject, kEntity };

    Dia::Core::StringCRC pickableId;
    Kind                 kind;
    int                  priority;
    Dia::Maths::Vector2D worldPos;

    union {
        Dia::Geometry2D::HexCoord hexCell;
        struct { int x; int y; }  spatialCell;
        unsigned int              objectIdx;
        unsigned int              entityId;
    };
};

} // namespace Dia::Geometry2DPicking
```

### PickingService2D

```cpp
// Dia/DiaGeometry2DPicking/PickingService2D.h
namespace Dia::Geometry2DPicking {

class PickingService2D {
public:
    static constexpr unsigned int kMaxPickables = 64;

    void Register(IPickable2D* pickable);
    void Unregister(IPickable2D* pickable);

    Dia::Picking::PickResult<PickHit2D> Pick(
        const Dia::Maths::Vector2D& worldPos,
        Dia::Picking::PickLayerMask mask = static_cast<unsigned int>(Dia::Picking::PickLayer::kAll)) const;

    Dia::Picking::PickResult<PickHit2D> PickArea(
        const Dia::Geometry2D::AARect& worldRect,
        Dia::Picking::PickLayerMask mask = static_cast<unsigned int>(Dia::Picking::PickLayer::kAll)) const;
};

} // namespace Dia::Geometry2DPicking
```

### Adapters

```cpp
// Dia/DiaGeometry2DPicking/Adapters/HexGridPickable.h
namespace Dia::Geometry2DPicking {

template<typename T, unsigned int MaxObjects>
class HexGridPickable : public IPickable2D {
public:
    HexGridPickable(const Dia::Geometry2D::HexGrid<T, MaxObjects>& grid,
                    Dia::Core::StringCRC id,
                    int priority = 0,
                    Dia::Picking::PickLayer layer = Dia::Picking::PickLayer::kDefault);

    bool TryPick(const Dia::Maths::Vector2D& worldPos, PickHit2D& out) const override;
    void PickArea(const Dia::Geometry2D::AARect& worldRect,
                  Dia::Core::Containers::DynamicArrayC<PickHit2D, kMaxAreaHits>& out) const override;
};

// Similarly: SpatialGridPickable<T,Max>, ShapePickable

} // namespace Dia::Geometry2DPicking
```

---

## Dependencies

```
DiaPicking
  depends on: DiaCore, DiaMailbox

DiaGeometry2DPicking
  depends on: DiaPicking, DiaGeometry2D, DiaMaths, DiaCore

PickingModule (CluicheGameBaseline)
  depends on: DiaGeometry2DPicking, DiaInput, DiaGraphics (ViewportTransform), DiaMailbox
```

---

## Tasks

| # | Task | Scope |
|---|---|---|
| 1 | Create `DiaPicking` vcxproj — `PickResult<T>`, `PickEvent<T>`, `PickTrigger`, `PickLayer`, `PickAddress`, `PickRouter` | Dia |
| 2 | Create `DiaGeometry2DPicking` vcxproj — `IPickable2D`, `PickHit2D`, `PickingService2D` | Dia |
| 3 | Implement `HexGridPickable` adapter | Dia |
| 4 | Implement `SpatialGridPickable` adapter | Dia |
| 5 | Implement `ShapePickable` adapter (Circle, AARect, ConvexPolygon) | Dia |
| 6 | Extract `Camera2DModule` from `VisualDebuggerModule` | CluicheGameBaseline |
| 7 | Extend `InputStreamModule` with mouse button state | CluicheGameBaseline |
| 8 | Create `PickingModule` (DiaMailbox integration, orchestration) | CluicheGameBaseline |
| 9 | Refactor `Geometry2DTestStageModule` — subscribe to pick events, register pickables, drain + react | CluicheTest |
| 10 | Refactor `HexGridDrawer` + `SpatialGridDrawer` — remove input/selection, accept `const` selection | Dia |
| 11 | Unit tests for all Dia types + service + adapters + DiaMailbox integration | GoogleTests |
| 12 | Integration test: click hex → DiaMailbox → selection → highlight | CluicheTest |

---

## Binding Decisions

**From SD-PICK-001 (events via DiaMailbox):** Compliant — `PickingModule` sends `PickEvent<PickHit2D>` via DiaMailbox; consumers drain.

**From SD-PICK-002 (layer mask on Pick call):** Compliant — `PickingService2D::Pick()` takes a `PickLayerMask` parameter.

**From SD-PICK-003 (priority descending):** Compliant — `PickResult` sorts hits by `PickHit2D::priority` descending; `Best()` returns highest.

**From SD-PICK-004 (no dimension types in DiaPicking):** Compliant — `Vector2D`, `HexCoord`, `AARect` only appear in `DiaGeometry2DPicking`, not `DiaPicking`.

**From SD-PICK-005 (one message per trigger, routed by PickRouter):** Compliant — `PickingModule` sends one `PickEvent` per trigger; `PickRouter` resolves to subscribers of that trigger type.

**From SD-PICK-006 (PickEvent templated on THit):** Compliant — 2D sends `PickEvent<PickHit2D>`; DiaMailbox registers it as a typed queue.

---

## Design Resolutions

1. **PickHit2D uses typed union** — Keep the `Kind` enum + named union for debuggability. Inspectable in VS debugger. Cost of adding a new Kind is low.

2. **Hover performance** — Linear scan at max 64 pickables is acceptable. Revisit if count exceeds 64.

3. **Module ordering via manifest dependencies** — `PickingModule` declares `"dependencies": ["InputStreamModule", "Camera2DModule"]` in the manifest. Topological update order within SimPU enforced by the framework.

4. **Transport via DiaMailbox, not custom latch** — Pick events are messages, not state snapshots. DiaMailbox provides routed delivery, subscriber lifecycle, and future extensibility (e.g. routing by layer or entity). Avoids inventing a parallel not-quite-mailbox.

5. **Camera2DModule extracts viewport ownership** — Camera2D + window size move from `VisualDebuggerModule` (debug-only) to a new non-debug `Camera2DModule`. Both `VisualDebuggerModule` and `PickingModule` consume it via `ModuleRef`. Picking works in Release.

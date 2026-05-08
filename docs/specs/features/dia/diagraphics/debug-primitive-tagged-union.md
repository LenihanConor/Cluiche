# Feature Spec: Debug Primitive Tagged Union

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diagraphics.md | **debug-primitive-tagged-union** |

**Status:** `Done`
**Plan:** @docs/specs/features/dia/diagraphics/debug-primitive-tagged-union.plan.md

---

## Problem Statement

`DebugFrameData` holds two separate `DynamicArrayC<T, 16>` buffers — one per concrete shape type — so adding any new debug primitive requires changes to four files (`DebugFrameData.h/.cpp`, `DebugFrameDataVisitor.h`, and every visitor implementation), and the per-type cap of 16 is insufficient for complex debug scenes.

---

## Solution Overview

Replace all per-type buffers with a single `DynamicArrayC<DebugPrimitive, kDebugPrimitiveCapacity>` where `DebugPrimitive` is a hand-rolled tagged union covering all supported 2D debug shape types. The visitor interface collapses to a single `Visit(const DebugPrimitive&)`. Adding a new shape in the future requires only: a new enum tag, a new union member, and one new switch case in the renderer — `DebugFrameData` itself never changes again.

---

## DebugPrimitive Shape Set

### Primitive Type Tag

```cpp
namespace Dia::Graphics {
    enum class DebugPrimitiveType : uint8_t {
        Circle2D,
        Line2D,
        Point2D,
        Rect2D,
        Arc2D,
        Ray2D,
        Triangle2D
    };
}
```

### Per-Shape Payloads

| Shape | Fields | Fill colour? |
|-------|--------|-------------|
| Circle2D | `Vector2D position`, `float radius`, `RGBA outlineColour`, `RGBA fillColour` | Yes — alpha=0 means no fill |
| Line2D | `Vector2D start`, `Vector2D end`, `RGBA colour` | No |
| Point2D | `Vector2D position`, `RGBA colour` | No |
| Rect2D | `Vector2D min`, `Vector2D max`, `RGBA outlineColour`, `RGBA fillColour` | Yes — alpha=0 means no fill |
| Arc2D | `Vector2D position`, `float radius`, `float startAngleDeg`, `float endAngleDeg`, `RGBA colour` | No |
| Ray2D | `Vector2D origin`, `Vector2D direction`, `float length`, `RGBA colour` | No |
| Triangle2D | `Vector2D p1`, `Vector2D p2`, `Vector2D p3`, `RGBA outlineColour`, `RGBA fillColour` | Yes — alpha=0 means no fill |

### DebugPrimitive Struct

```cpp
namespace Dia::Graphics {
    struct DebugPrimitive {
        DebugPrimitiveType type;
        union {
            DebugPrimitiveCircle2D    circle2D;
            DebugPrimitiveData_Line2D      line2D;
            DebugPrimitivePoint2D     point2D;
            DebugPrimitiveRect2D      rect2D;
            DebugPrimitiveArc2D       arc2D;
            DebugPrimitiveRay2D       ray2D;
            DebugPrimitiveTriangle2D  triangle2D;
        };
    };
}
```

All union members are plain-old-data structs. `DebugPrimitive` is trivially copyable — no virtuals, no pointers.

---

## DebugFrameData API

### Capacity

```cpp
static constexpr unsigned int kDebugPrimitiveCapacity = 1024;
```

### RequestDraw Overloads

Fillable shapes (Circle2D, Rect2D, Triangle2D) have two overloads — one with explicit fill colour, one defaulting fill to `RGBA(0,0,0,0)` (transparent = no fill):

```cpp
// Circle2D
void RequestDraw(const Maths::Vector2D& position, float radius,
                 RGBA outlineColour, RGBA fillColour = RGBA(0,0,0,0));

// Line2D
void RequestDraw(const Maths::Vector2D& start, const Maths::Vector2D& end,
                 RGBA colour);

// Point2D
void RequestDraw(const Maths::Vector2D& position, RGBA colour);

// Rect2D
void RequestDraw(const Maths::Vector2D& min, const Maths::Vector2D& max,
                 RGBA outlineColour, RGBA fillColour = RGBA(0,0,0,0));

// Arc2D
void RequestDraw(const Maths::Vector2D& position, float radius,
                 float startAngleDeg, float endAngleDeg, RGBA colour);

// Ray2D
void RequestDraw(const Maths::Vector2D& origin, const Maths::Vector2D& direction,
                 float length, RGBA colour);

// Triangle2D
void RequestDraw(const Maths::Vector2D& p1, const Maths::Vector2D& p2,
                 const Maths::Vector2D& p3,
                 RGBA outlineColour, RGBA fillColour = RGBA(0,0,0,0));
```

### AcceptVisitor

```cpp
void AcceptVisitor(const DebugFrameDataVisitor& visitor) const;
// Iterates mDebugPrimitiveBuffer in insertion order, calls visitor.Visit(primitive) for each
```

---

## DebugFrameDataVisitor API

```cpp
namespace Dia::Graphics {
    class DebugFrameDataVisitor {
    public:
        virtual ~DebugFrameDataVisitor() {}
        virtual void Visit(const DebugPrimitive& primitive) const = 0;
        virtual void Visit(const DebugFrameData& frameData) const = 0;
    };
}
```

The per-type `Visit(Circle2D)` / `Visit(Line2D)` overloads are removed. Implementations switch on `primitive.type` internally.

---

## DebugFrameRendererVisitor (DiaSFML)

`DebugFrameRendererVisitor::Visit(const DebugPrimitive&)` switches on tag and dispatches to private helpers:

```cpp
void DebugFrameRendererVisitor::Visit(const DebugPrimitive& p) const {
    switch (p.type) {
        case DebugPrimitiveType::Circle2D:   DrawCircle2D(p.circle2D);     break;
        case DebugPrimitiveType::Line2D:     DrawLine2D(p.line2D);         break;
        case DebugPrimitiveType::Point2D:    DrawPoint2D(p.point2D);       break;
        case DebugPrimitiveType::Rect2D:     DrawRect2D(p.rect2D);         break;
        case DebugPrimitiveType::Arc2D:      DrawArc2D(p.arc2D);           break;
        case DebugPrimitiveType::Ray2D:      DrawRay2D(p.ray2D);           break;
        case DebugPrimitiveType::Triangle2D: DrawTriangle2D(p.triangle2D); break;
    }
}
```

### Arc2D Tessellation (renderer-side)

Segment count scales with angular span — computed in the renderer, not stored in the primitive:

| Arc span | Segments |
|----------|---------|
| < 45° | 4 |
| < 90° | 8 |
| < 180° | 12 |
| ≥ 180° | 16 |

### Fill Colour Convention

For Circle2D, Rect2D, Triangle2D: if `fillColour.a == 0` the shape is drawn outline-only. Otherwise both fill and outline are drawn. SFML's `setFillColor()` / `setOutlineColor()` handle this natively.

---

## Files Affected

### DiaGraphics — new files

| File | Purpose |
|------|---------|
| `Dia/DiaGraphics/Frame/DebugPrimitive.h` | `DebugPrimitiveType` enum, all per-shape payload structs, `DebugPrimitive` union struct |

### DiaGraphics — modified files

| File | Change |
|------|--------|
| `Dia/DiaGraphics/Frame/DebugFrameData.h` | Replace two `DynamicArrayC<T,16>` fields with `DynamicArrayC<DebugPrimitive, kDebugPrimitiveCapacity>`; replace two `RequestDraw()` overloads with seven |
| `Dia/DiaGraphics/Frame/DebugFrameData.cpp` | Implement new `RequestDraw()` overloads; update `ClearDebugBuffer()`, `CopyDebugBuffer()`, `AcceptVisitor()` |
| `Dia/DiaGraphics/Frame/DebugFrameDataVisitor.h` | Remove per-type `Visit()` overloads; add single `Visit(const DebugPrimitive&)` |
| `Dia/DiaGraphics/DiaGraphics.vcxproj` | Add `DebugPrimitive.h` |
| `Dia/DiaGraphics/DiaGraphics.vcxproj.filters` | Add `DebugPrimitive.h` under Frame filter |

### DiaGraphics — deleted files

| File | Reason |
|------|--------|
| `Dia/DiaGraphics/Frame/DebugFrameDataBase.h` | Replaced by value-type union; virtual dispatch no longer needed |
| `Dia/DiaGraphics/Frame/DebugFrameDataCircle2D.h` | Folded into `DebugPrimitive` union |
| `Dia/DiaGraphics/Frame/DebugFrameDataCircle2D.cpp` | Deleted |
| `Dia/DiaGraphics/Frame/DebugFrameDataLine2D.h` | Folded into `DebugPrimitive` union |
| `Dia/DiaGraphics/Frame/DebugFrameDataLine2D.cpp` | Deleted |

### DiaSFML — modified files

| File | Change |
|------|--------|
| `Dia/DiaSFML/DebugFrameRendererVisitor.h` | Remove per-type `Visit()` declarations; add `Visit(const DebugPrimitive&)` |
| `Dia/DiaSFML/DebugFrameRendererVisitor.cpp` | Implement tag-switch dispatcher + per-shape private draw helpers |

### Tests — modified files

| File | Change |
|------|--------|
| `Cluiche/Tests/GoogleTests/Graphics/TestFrameData.cpp` | Update existing tests to use new `RequestDraw()` API; add new tests for all seven primitive types and visitor dispatch |

---

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| 1 | `DebugPrimitive.h` defines `DebugPrimitiveType` enum with 7 values and all per-shape payload structs | Code review |
| 2 | `DebugFrameData` holds exactly one `DynamicArrayC<DebugPrimitive, 1024>` — no per-type fields | Code review |
| 3 | `kDebugPrimitiveCapacity = 1024` defined as `constexpr` | Code review |
| 4 | `RequestDraw()` overloads exist for all 7 primitive types | Code review |
| 5 | Fillable shapes (Circle2D, Rect2D, Triangle2D) have a two-param overload with `fillColour` defaulting to `RGBA(0,0,0,0)` | Code review |
| 6 | `DebugFrameDataVisitor` has a single `Visit(const DebugPrimitive&)` — no per-type overloads | Code review |
| 7 | `DebugFrameRendererVisitor` switches on `DebugPrimitiveType` and handles all 7 cases | Code review |
| 8 | Arc2D tessellation is renderer-side, angle-driven (4/8/12/16 segments) | Code review |
| 9 | `DebugPrimitive` is trivially copyable — no virtual methods, no pointers | Static analysis / code review |
| 10 | Insertion order preserved — primitives rendered in push order | Unit test |
| 11 | Old concrete classes (`DebugFrameDataBase`, `DebugFrameDataCircle2D`, `DebugFrameDataLine2D`) deleted from codebase and `.vcxproj` | File review |
| 12 | All existing `TestFrameData.cpp` tests pass against the new API | Test run |
| 13 | New tests cover all 7 primitive types via visitor dispatch | Test run |
| 14 | Solution builds cleanly in Debug\|x64 and Release\|x64 with no warnings | Build verification |
| 15 | Adding a future 8th shape requires zero changes to `DebugFrameData.h/.cpp` | Code review |

---

## Tasks

| # | Task | Depends On | Notes |
|---|------|------------|-------|
| 1 | Create `DebugPrimitive.h` — enum, 7 payload structs, `DebugPrimitive` union | — | All plain-old-data; no includes beyond DiaMaths and RGBA |
| 2 | Update `DebugFrameData.h/.cpp` — replace fields, implement 7 `RequestDraw()` overloads, update Clear/Copy/AcceptVisitor | 1 | `kDebugPrimitiveCapacity = 1024` |
| 3 | Update `DebugFrameDataVisitor.h` — remove per-type overloads, add `Visit(const DebugPrimitive&)` | 1 | Cascades to all visitor implementations |
| 4 | Delete `DebugFrameDataBase.h`, `DebugFrameDataCircle2D.h/.cpp`, `DebugFrameDataLine2D.h/.cpp`; remove from `.vcxproj` | 2, 3 | |
| 5 | Update `DebugFrameRendererVisitor.h/.cpp` in DiaSFML — implement tag-switch, 7 draw helpers, arc tessellation logic | 3 | Fill colour: alpha=0 check before `setFillColor()` |
| 6 | Update `TestFrameData.cpp` — fix existing tests, add new tests for all 7 types and visitor dispatch | 2, 3 | |
| 7 | Build and run full test suite in Debug\|x64 and Release\|x64 | 1–6 | |

---

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for identifiers | Compliant — `DebugPrimitiveType` is an enum, not a string; no StringCRC needed here |
| PD-004 | Platform | No STL in public APIs | Compliant — `DebugPrimitive` is a hand-rolled union; `DynamicArrayC` used for storage; no `std::variant`, `std::vector`, or `std::any` |
| PD-005 | Platform | x64 only | Compliant — no architecture-specific code introduced |
| PD-006 | Platform | VS project files source of truth | Compliant — `.vcxproj` and `.vcxproj.filters` updated for new/deleted files |
| PD-007 | Platform | C++20 required | Compliant — no C++20-exclusive features required, but code compiles cleanly under `/std:c++20` |
| PD-008 | Platform | Directory.Build.props owns build paths | Compliant — no per-project output path overrides |
| AD-001 | Dia App | Module YAML documentation | Compliant — `dia.graphics.architecture.module.md` exists; no new module created by this feature |
| AD-002 | Dia App | No STL in public APIs | Compliant — same as PD-004 |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | Compliant — all new types in `Dia::Graphics::` |
| GD-001 | DiaGraphics | Tagged union, not `std::variant` | Compliant — this feature implements GD-001 |
| GD-002 | DiaGraphics | FrameData copy trivially correct | Compliant — `DebugPrimitive` is trivially copyable; `CopyDebugBuffer()` uses value assignment |
| GD-004 | DiaGraphics | Insertion order preserved | Compliant — single buffer guarantees push order |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | DebugPrimitive size | What is the worst-case sizeof(DebugPrimitive)? Triangle2D has 3×Vector2D + 2×RGBA = 3×8 + 2×4 = 32 bytes. At 1024 primitives that is 32 KB for the buffer. Is this acceptable? | Yes — 32 KB is negligible. `FrameData` is copied to the render stream once per tick; 32 KB is well within budget. |
| 2 | Visitor contract | Removing per-type Visit() overloads means the compiler no longer enforces that a new shape type is handled. Is this acceptable? | Yes — the switch in `DebugFrameRendererVisitor` should have a `DIA_ASSERT(false)` or `static_assert`-driven default to catch unhandled tags at runtime in debug builds. |
| 3 | RGBA alpha convention | Using alpha=0 for "no fill" is implicit. Could this confuse callers who pass an uninitialized RGBA? | `RGBA` default-constructs to white (255,255,255,255) based on existing usage. The no-fill default is explicit in the overload signature (`RGBA(0,0,0,0)`). Low risk. |
| 4 | Arc2D direction | Should arc angles be clockwise or counter-clockwise? SFML uses clockwise from the positive X axis. | Angles follow SFML convention (clockwise from positive X) since the renderer is the only consumer. Document in `DebugPrimitive.h`. |
| 5 | Existing callers | `SimProcessingUnit` and `DiaRig2DVisualDebugger` call `RequestDraw(DebugFrameDataCircle2D(...))` today. Do those call sites need updating? | Yes — Task 2 (updating `DebugFrameData`) changes the API. Existing callers switch to the new parameter-based overloads. Covered in Task 6 scope. |
| 6 | Ray2D direction normalisation | Should `direction` in Ray2D be required to be a unit vector, or is any non-zero vector accepted? | Callers are responsible for passing a unit vector. Add a `DIA_ASSERT` in the `RequestDraw(Ray2D)` impl that checks `direction.LengthSquared() > 0`. |

---

## Open Questions

None — all resolved above.

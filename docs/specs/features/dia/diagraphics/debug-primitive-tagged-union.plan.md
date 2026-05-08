# Plan: Debug Primitive Tagged Union

**Spec:** @docs/specs/features/dia/diagraphics/debug-primitive-tagged-union.md
**Status:** Done
**Started:** 2026-05-02
**Last Updated:** 2026-05-02

---

## Implementation Patterns

### DebugPrimitive.h — value union
All payload structs are plain-old-data (no ctors, no virtuals). The union tag is `DebugPrimitiveType : uint8_t`. The outer `DebugPrimitive` struct has no constructor — callers use the static factory helpers on `DebugFrameData`. Example shape struct:

```cpp
struct DebugPrimitiveCircle2D {
    Dia::Maths::Vector2D position;
    float                radius;
    RGBA                 outlineColour;
    RGBA                 fillColour;   // alpha==0 → no fill
};
```

Include guard only (`#pragma once`). No `DIA_TYPE_DECLARATION` needed — this is not a reflected type.

### DebugFrameData — RequestDraw factory pattern
Each overload fills a `DebugPrimitive`, sets the tag, and calls `mDebugPrimitiveBuffer.Add(p)`:

```cpp
void DebugFrameData::RequestDraw(const Maths::Vector2D& position, float radius,
                                  RGBA outlineColour, RGBA fillColour)
{
    DebugPrimitive p;
    p.type               = DebugPrimitiveType::Circle2D;
    p.circle2D.position  = position;
    p.circle2D.radius    = radius;
    p.circle2D.outlineColour = outlineColour;
    p.circle2D.fillColour    = fillColour;
    mDebugPrimitiveBuffer.Add(p);
}
```

Default-fill overloads are inline in the header:

```cpp
void RequestDraw(const Maths::Vector2D& position, float radius, RGBA outlineColour)
{
    RequestDraw(position, radius, outlineColour, RGBA(0, 0, 0, 0));
}
```

### DebugFrameDataVisitor — collapsed interface
```cpp
class DebugFrameDataVisitor {
public:
    virtual ~DebugFrameDataVisitor() {}
    virtual void Visit(const DebugPrimitive& primitive) const = 0;
    virtual void Visit(const DebugFrameData& frameData) const = 0;
};
```

`Visit(const DebugFrameData&)` is the frame-level entry point called by `DebugFrameRendererVisitor::Visit(DebugFrameData)` → calls `AcceptVisitor(*this)`.

### DebugFrameRendererVisitor — tag switch
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
        default: DIA_ASSERT(0, "Unhandled DebugPrimitiveType"); break;
    }
}
```

Fill colour convention in DrawCircle2D / DrawRect2D / DrawTriangle2D:
```cpp
sf::Color fill;
Convert(fill, p.circle2D.fillColour);
shape.setFillColor(fill);  // SFML: alpha=0 → transparent, handled natively
```

Arc2D tessellation helper — segment count from angular span:
```cpp
static int ArcSegmentCount(float startDeg, float endDeg) {
    float span = endDeg - startDeg;
    if (span < 0.0f) span += 360.0f;
    if (span < 45.0f)  return 4;
    if (span < 90.0f)  return 8;
    if (span < 180.0f) return 12;
    return 16;
}
```

### MockVisitors.h — updated recording visitor
```cpp
struct RecordingDebugVisitor : public DebugFrameDataVisitor {
    mutable int primitiveCount[7] = {};  // indexed by DebugPrimitiveType
    mutable int frameCount = 0;

    void Visit(const DebugPrimitive& p) const override {
        ++primitiveCount[static_cast<int>(p.type)];
    }
    void Visit(const DebugFrameData&) const override { ++frameCount; }
};
```

### Test pattern — per-primitive type test
```cpp
TEST(DiaGraphics_DebugPrimitive, RequestDrawCircle2D_VisitorReceivesCorrectType)
{
    DebugFrameData dfd;
    dfd.RequestDraw(Vector2D(1.0f, 2.0f), 3.0f, RGBA::White);

    RecordingDebugVisitor v;
    dfd.AcceptVisitor(v);
    EXPECT_EQ(v.primitiveCount[static_cast<int>(DebugPrimitiveType::Circle2D)], 1);
}
```

---

## Tasks

| # | Task | Status | Notes |
|---|------|--------|-------|
| 1 | Create `Dia/DiaGraphics/Frame/DebugPrimitive.h` — enum, 7 payload structs, `DebugPrimitive` union | Done | Header-only; no .cpp needed |
| 2 | Update `DiaGraphics.vcxproj` and `.vcxproj.filters` — add `DebugPrimitive.h`, remove old shape files | Done | Do alongside Task 1; removes Circle2D/Line2D/Base entries |
| 3 | Update `DebugFrameDataVisitor.h` — collapse to `Visit(const DebugPrimitive&)` + `Visit(const DebugFrameData&)` | Done | Must happen before Task 4 to avoid cascading compile errors |
| 4 | Update `DebugFrameData.h/.cpp` — replace fields with `DynamicArrayC<DebugPrimitive, 1024>`, implement 7 RequestDraw overloads, update Clear/Copy/AcceptVisitor | Done | Depends on Tasks 1, 3 |
| 5 | Delete `DebugFrameDataBase.h`, `DebugFrameDataCircle2D.h/.cpp`, `DebugFrameDataLine2D.h/.cpp` from disk | Done | Depends on Task 4 compiling; removes files from repo |
| 6 | Update `MockVisitors.h` in `DiaGraphics/Testing/` — replace per-type counts with primitive type array | Done | Depends on Tasks 1, 3 |
| 7 | Update `DebugFrameRendererVisitor.h/.cpp` in DiaSFML — implement tag-switch + 7 draw helpers + arc tessellation | Done | Depends on Tasks 1, 3; fill via alpha check |
| 8 | Update external callers — `DiaRig2DVisualDebugger/VisualDebugger.cpp` and `CluicheTest/SimProcessingUnit.cpp` | Done | Switch from old concrete-type API to new parameter overloads |
| 9 | Update `TestFrameData.cpp` — migrate existing tests to new API; add 7 new per-type dispatch tests + insertion-order test + fill-colour test + copy-preserves-primitives test | Done | Depends on Tasks 1, 3, 4, 6 |
| 10 | Build Debug\|x64 — fix any remaining compile errors | Done | Depends on Tasks 1–9 |
| 11 | Build Release\|x64 — verify clean | Done | Depends on Task 10 |
| 12 | Run `UnitTests.exe` — all tests green | Done | Depends on Task 11 |

---

## Test Coverage Plan

### Existing tests to migrate (in `TestFrameData.cpp`)

| Existing test | Migration action |
|---------------|-----------------|
| `DebugFrameData_DefaultConstruction_Empty` | Replace `circleCount`/`lineCount` checks with `primitiveCount` array |
| `DebugFrameData_RequestDrawCircle_VisitorReceivesIt` | Replace `RequestDraw(DebugFrameDataCircle2D(...))` with `RequestDraw(pos, radius, colour)` |
| `DebugFrameData_RequestDrawLine_VisitorReceivesIt` | Replace with `RequestDraw(start, end, colour)` |
| `DebugFrameData_ClearDebugBuffer_RemovesAll` | Update API calls; check total primitive count is 0 |
| `DebugFrameData_MultipleCirclesAndLines` | Update API calls; check per-type counts |
| `FrameData_Clear_ClearsBothEntityAndDebug` | Update RequestDraw call |
| `FrameData_CopyPreservesData` | Update RequestDraw call; verify copy count |
| `DebugCircle2D_Accessors_ReturnConstructedValues` | **Delete** — no standalone Circle2D class anymore |
| `DebugLine2D_Accessors_ReturnConstructedValues` | **Delete** — no standalone Line2D class anymore |

### New tests to add

| Test name | What it verifies |
|-----------|-----------------|
| `DebugPrimitive_RequestDrawCircle2D_CorrectTag` | Circle2D RequestDraw → visitor receives `DebugPrimitiveType::Circle2D` |
| `DebugPrimitive_RequestDrawLine2D_CorrectTag` | Line2D RequestDraw → correct tag |
| `DebugPrimitive_RequestDrawPoint2D_CorrectTag` | Point2D RequestDraw → correct tag |
| `DebugPrimitive_RequestDrawRect2D_CorrectTag` | Rect2D RequestDraw → correct tag |
| `DebugPrimitive_RequestDrawArc2D_CorrectTag` | Arc2D RequestDraw → correct tag |
| `DebugPrimitive_RequestDrawRay2D_CorrectTag` | Ray2D RequestDraw → correct tag |
| `DebugPrimitive_RequestDrawTriangle2D_CorrectTag` | Triangle2D RequestDraw → correct tag |
| `DebugPrimitive_InsertionOrderPreserved` | Push Circle, Line, Point in that order → visitor receives them in that order |
| `DebugPrimitive_FillColourDefault_IsTransparent` | Circle overload without fill → `fillColour.a == 0` |
| `DebugPrimitive_FillColourExplicit_StoredCorrectly` | Circle overload with explicit fill → correct `fillColour` stored |
| `DebugPrimitive_Circle2D_FieldsStoredCorrectly` | Push circle → visitor receives correct position/radius/colour |
| `DebugPrimitive_Line2D_FieldsStoredCorrectly` | Push line → visitor receives correct start/end/colour |
| `DebugPrimitive_Rect2D_FieldsStoredCorrectly` | Push rect → visitor receives correct min/max/colours |
| `DebugPrimitive_Triangle2D_FieldsStoredCorrectly` | Push triangle → visitor receives correct p1/p2/p3/colours |
| `DebugPrimitive_Arc2D_FieldsStoredCorrectly` | Push arc → visitor receives correct position/radius/angles/colour |
| `DebugPrimitive_Ray2D_FieldsStoredCorrectly` | Push ray → visitor receives correct origin/direction/length/colour |
| `DebugPrimitive_MixedTypes_AllCounted` | Push 2 circles + 1 line + 1 rect → counts correct per type |
| `DebugPrimitive_CopyPreservesAll` | Push 3 mixed, copy → destination visitor sees same 3 in same order |
| `DebugPrimitive_ClearThenAdd_CorrectCount` | Push 5, clear, push 1 → count is 1 |

---

## Session Notes

### 2026-05-02
- Research complete; Candidate 1 (tagged union) chosen over Candidate 7 (preserved visitor) to avoid dual-dispatch legacy
- Candidate 5 (decoupled debug queue) parked as future follow-on for editor/persistent debug
- System spec `diagraphics.md` created; feature spec written and Approved
- Plan written; implementation not yet started

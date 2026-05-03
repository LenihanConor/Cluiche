# Feature Spec: debug-budget

## Traceability

| Level | Spec |
|-------|------|
| Platform | @docs/specs/platform/Cluiche.md |
| Application | @docs/specs/applications/dia.md |
| System | @docs/specs/systems/dia/diavisualdebugger.md |
| Feature | this file |

---

## Summary

Adds `DroppedCount()` and `IsOverCapacity()` to `DebugFrameData` so that silent primitive dropping is visible and diagnosable. Also adds an `entityId` field to all `DebugPrimitive` variants as a scene-editor picking seam. The compile-time capacity constant `kDebugPrimitiveCapacity` remains the budget knob — increase it by editing the constant when more budget is needed.

**Problem solved:** When more than 1024 debug primitives are submitted in a single frame, `DynamicArrayC::Add()` silently no-ops on overflow. Developers lose debug output with no indication why. With multiple draw class stacks active simultaneously (physics shapes + bones + IK + geometry), 1024 is easily exhausted.

---

## Acceptance Criteria

1. `DebugFrameData` tracks how many `RequestDraw*` calls were dropped after the buffer was full
2. `DroppedCount() const` returns the number of dropped primitives since the last `ClearDebugBuffer()` call
3. `IsOverCapacity() const` returns `true` if any primitives were dropped this frame
4. `ClearDebugBuffer()` resets `DroppedCount()` to 0
5. `DroppedCount()` is logged via DiaLogger (warning channel) at the end of each frame when > 0 — responsibility of the caller (`DebugLayerManager::Draw()` in a later feature); this feature only provides the query
6. `DebugPrimitive` gains a `uint32_t entityId` field (default 0, meaning untagged); field present on the struct itself, not inside the union variants
7. All existing `RequestDraw*` call sites compile and behave identically — no caller changes required
8. `DebugFrameData` remains trivially assignable (hand-rolled copy assignment already exists; `DroppedCount` is a plain `uint32_t` — no change to copyability)
9. `kDebugPrimitiveCapacity` stays as the compile-time budget knob; no runtime constructor parameter

---

## Design

### DebugPrimitive — entityId field

`entityId` is added **outside the union**, on the `DebugPrimitive` struct itself. It is a plain `uint32_t` defaulting to 0. This avoids touching all 7 variant structs and keeps the union layout unchanged.

```cpp
struct DebugPrimitive
{
    DebugPrimitiveType type;
    uint32_t           entityId = 0;  // picking seam — 0 = untagged

    union { ... };  // unchanged

    DebugPrimitive() : type(DebugPrimitiveType::Circle2D), entityId(0), circle2D() {}
    // copy constructor and operator= must copy entityId
};
```

### DebugFrameData — DroppedCount tracking

`DynamicArrayC::Add()` is the only insertion path. It currently no-ops silently when full. `DebugFrameData` wraps each `RequestDraw*` call to check capacity before inserting:

```cpp
// DebugFrameData.h — private helper
bool CanAdd() const
{
    if (mDebugPrimitiveBuffer.Size() >= kDebugPrimitiveCapacity)
    {
        ++mDroppedCount;
        return false;
    }
    return true;
}

// DebugFrameData.h — new public queries
bool     IsOverCapacity() const { return mDroppedCount > 0; }
uint32_t DroppedCount()   const { return mDroppedCount; }

// DebugFrameData.h — private member added
uint32_t mDroppedCount = 0;

// ClearDebugBuffer() — reset both
void ClearDebugBuffer()
{
    mDebugPrimitiveBuffer.RemoveAll();
    mDroppedCount = 0;
}

// CopyDebugBuffer() — copy DroppedCount too
void CopyDebugBuffer(const DebugFrameData& rhs)
{
    mDebugPrimitiveBuffer = rhs.mDebugPrimitiveBuffer;
    mDroppedCount         = rhs.mDroppedCount;
}
```

Each `RequestDraw*` implementation becomes:

```cpp
void DebugFrameData::RequestDraw(const Maths::Vector2D& position, float radius,
    RGBA outlineColour, RGBA fillColour)
{
    if (!CanAdd()) return;
    DebugPrimitive p;
    p.type                   = DebugPrimitiveType::Circle2D;
    p.circle2D.position      = position;
    p.circle2D.radius        = radius;
    p.circle2D.outlineColour = outlineColour;
    p.circle2D.fillColour    = fillColour;
    mDebugPrimitiveBuffer.Add(p);
}
```

### DynamicArrayC capacity check

`DynamicArrayC` already has a `Size()` method and a compile-time `N` capacity. The guard compares `Size() >= kDebugPrimitiveCapacity`. `DynamicArrayC::Add()` itself is not changed.

### Construction sites audit

All `DebugFrameData` construction is through its default constructor (confirmed by audit — only one construction site: `FrameData::FrameData()` which default-constructs its base). No call site changes needed. `mDroppedCount` is value-initialised to 0 in the member declaration.

---

## Files Changed

| File | Change |
|------|--------|
| `Dia/DiaGraphics/Frame/DebugPrimitive.h` | Add `uint32_t entityId = 0` to `DebugPrimitive` struct; update default constructor and `operator=` to include `entityId` |
| `Dia/DiaGraphics/Frame/DebugFrameData.h` | Add `IsOverCapacity()`, `DroppedCount()` public methods; add `CanAdd()` private helper; add `mDroppedCount` member |
| `Dia/DiaGraphics/Frame/DebugFrameData.cpp` | Update all `RequestDraw*` implementations to call `CanAdd()`; update `ClearDebugBuffer()` and `CopyDebugBuffer()` to reset/copy `mDroppedCount` |

**No other files change.** `FrameData.h/.cpp`, `DebugFrameDataVisitor.h`, DiaSFML renderer, and all existing visual debugger files are untouched.

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Add `entityId` to `DebugPrimitive` struct and update constructor + `operator=` | `DebugPrimitive.h` |
| 2 | Add `mDroppedCount`, `IsOverCapacity()`, `DroppedCount()`, `CanAdd()` to `DebugFrameData` | `.h` changes |
| 3 | Update `ClearDebugBuffer()` and `CopyDebugBuffer()` to handle `mDroppedCount` | `.cpp` changes |
| 4 | Wrap all 7 `RequestDraw*` implementations with `CanAdd()` guard | `.cpp` changes |
| 5 | Build solution — verify zero new warnings | `msbuild Cluiche/Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |
| 6 | Write tests — see test plan below | `TestDebugBudget.cpp` in GoogleTests |
| 7 | Run tests | `Cluiche/bin/Debug/x64/UnitTests.exe` |

---

## Test Plan

**File:** `Cluiche/Tests/GoogleTests/DiaGraphics/TestDebugBudget.cpp`

| Suite | Test | What it verifies |
|-------|------|-----------------|
| DroppedCount | `DefaultZero` | Fresh `DebugFrameData` → `DroppedCount() == 0` |
| DroppedCount | `NotOverCapacityWhenEmpty` | Fresh → `IsOverCapacity() == false` |
| DroppedCount | `NoPrimitivesDroppedBelowCapacity` | Submit `kDebugPrimitiveCapacity` primitives → `DroppedCount() == 0` |
| DroppedCount | `DropsWhenFull` | Submit `kDebugPrimitiveCapacity + 5` → `DroppedCount() == 5` |
| DroppedCount | `IsOverCapacityWhenDropped` | Submit over capacity → `IsOverCapacity() == true` |
| DroppedCount | `ClearResetsDropCount` | Fill past capacity → `ClearDebugBuffer()` → `DroppedCount() == 0` |
| DroppedCount | `ClearResetsIsOverCapacity` | Fill past capacity → `ClearDebugBuffer()` → `IsOverCapacity() == false` |
| DroppedCount | `CopyPreservesDropCount` | Fill past capacity → `CopyDebugBuffer(src)` → dest `DroppedCount() == src.DroppedCount()` |
| AllDrawTypes | `RequestDraw_Circle_CountsDrops` | Overflow via circle draws → `DroppedCount() > 0` |
| AllDrawTypes | `RequestDraw_Line_CountsDrops` | Overflow via line draws → `DroppedCount() > 0` |
| AllDrawTypes | `RequestDraw_Ray_CountsDrops` | Overflow via ray draws → `DroppedCount() > 0` |
| AllDrawTypes | `RequestDraw_Rect_CountsDrops` | Overflow via rect draws → `DroppedCount() > 0` |
| AllDrawTypes | `RequestDraw_Arc_CountsDrops` | Overflow via arc draws → `DroppedCount() > 0` |
| AllDrawTypes | `RequestDraw_Triangle_CountsDrops` | Overflow via triangle draws → `DroppedCount() > 0` |
| AllDrawTypes | `RequestDraw_Point_CountsDrops` | Overflow via point draws → `DroppedCount() > 0` |
| EntityId | `DefaultEntityIdIsZero` | New `DebugPrimitive` → `entityId == 0` |
| EntityId | `EntityIdCopied` | Set `entityId = 42`, copy construct → copy `entityId == 42` |
| EntityId | `EntityIdAssigned` | Set `entityId = 7`, assign → assigned `entityId == 7` |
| EntityId | `EntityIdPreservedAcrossRequestDraw` | Submit primitive, retrieve via visitor → `entityId` survives round-trip (note: requires visitor to expose primitive; may need test-only accessor) |

---

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|-----------|
| PD-001 | StringCRC for identifiers | Compliant — no string IDs introduced in this feature |
| PD-002 | ProcessingUnit/Phase/Module architecture | Compliant — no application lifecycle changes; `DebugFrameData` is a data type, not a module |
| PD-003 | Component-based entities | Compliant — `entityId` is a forward seam for component IDs; value 0 is explicitly untagged |
| PD-004 | No STL in public APIs | Compliant — `DynamicArrayC` unchanged; `uint32_t` and `bool` are primitive types; no STL introduced |
| PD-005 | x64 only | Compliant — no platform-specific code |
| PD-006 | Visual Studio project files are source of truth | Compliant — no new projects; existing `DiaGraphics.vcxproj` already includes these files |
| PD-007 | C++20 required | Compliant — no language features below C++20 used |
| PD-008 | `Directory.Build.props` owns build output paths | Compliant — no project file changes |
| PD-009 | Generated output under `Cluiche/out/` | Compliant — no generated output |
| AD-001 | Module YAML frontmatter | Compliant — no new module; `dia.graphics.architecture.module.md` updated to reflect new public API |
| AD-002 | No STL in public APIs | Compliant — reinforces PD-004 |
| AD-003 | `Dia::<Module>::` namespace | Compliant — all changes in `Dia::Graphics::` namespace |
| SD-DBG-002 | `#ifdef DIA_DEBUG` guards | Compliant — `DroppedCount()` and `IsOverCapacity()` are debug-only queries; guarded in all callers. Note: `DebugFrameData` itself is only linked in Debug builds (per DiaVisualDebugger system spec); no guard needed inside the class itself |
| SD-DBG-007 | `entityId = 0` reserved on all `DebugPrimitive` variants | Compliant — this feature implements the seam exactly as specified |
| SD-DBG-009 | `DebugFrameData` must remain trivially copyable | Compliant — `mDroppedCount` is `uint32_t`; hand-rolled copy assignment updated to include it; no heap allocation, no pointer members added |
| SD-DBG-012 | `TextPrimitive` uses `char[64]` not `std::string` | N/A — TextPrimitive is a separate feature (debug-text-primitive) |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | CanAdd() | `DynamicArrayC::Add()` silently no-ops when full today. Should `CanAdd()` check `Size() >= kDebugPrimitiveCapacity` directly, or should `DynamicArrayC` expose an `IsFull()` method? | Check `Size() >= kDebugPrimitiveCapacity` directly in `DebugFrameData` — avoids changing `DynamicArrayC` public API for a single consumer. |
| 2 | EntityId round-trip test | Test `EntityIdPreservedAcrossRequestDraw` requires reading back a submitted primitive to verify `entityId`. `AcceptVisitor` is the only read path but takes a visitor with no return value. Is a test-only accessor acceptable? | Add a `GetDebugPrimitiveCount() const` and `GetDebugPrimitive(uint32_t index) const` pair to `DebugFrameData` for test access — these are also useful for `DebugLayerManager` overflow logging and are not test-only. |
| 3 | CopyDebugBuffer semantics | When copying a frame that was over capacity, should the copy's `DroppedCount()` reflect the source's dropped count (copy the drop state) or reset to 0 (clean copy)? | Copy the drop count — `CopyDebugBuffer` is used for cross-thread frame transport; the receiving thread should know the frame was truncated. |
| 4 | DebugFrameData in Release | The DiaVisualDebugger system says all debug draw code is `#ifdef DIA_DEBUG`. Does `DebugFrameData` itself need guards, or is it excluded at the vcxproj level? | Excluded at the vcxproj level — visual debugger projects don't link in Release. `DebugFrameData` is part of `DiaGraphics.vcxproj` which is always compiled, but `RequestDraw*` calls are only made from debug-only draw classes. No `#ifdef` needed inside `DebugFrameData` itself. |
| 5 | RGBA kDebugPrimitiveCapacity location | `kDebugPrimitiveCapacity` is defined in `DebugPrimitive.h` as a file-scope `static constexpr`. Should it move to `DebugFrameData.h` since it is `DebugFrameData`'s constraint, not `DebugPrimitive`'s? | Move it to `DebugFrameData.h` as `DebugFrameData::kCapacity` — it belongs to the buffer owner. Update the one reference in `TestRigidBody2DVisualDebugger.cpp` to `Dia::Graphics::DebugFrameData::kCapacity`. |

---

## Status

`Approved`

# Feature Spec: rigidbody2d-visual-debugger-stack

## Traceability

| Level | Spec |
|-------|------|
| Platform | @docs/specs/platform/Cluiche.md |
| Application | @docs/specs/applications/dia.md |
| System | @docs/specs/systems/dia/diavisualdebugger.md |
| Feature | this file |

---

## Summary

Decomposes the existing monolithic `DiaRigidBodyVisualDebugger` into a stack of focused `IVisualDebugger` draw classes, each responsible for one concern. The existing single-class API is retired and replaced by five classes registered independently with `DebugLayerManager`. All draw logic is migrated and the ad-hoc colour literals are replaced with `DebugColourPalette` constants. Velocity arrow parameters previously stored in `VisualDebuggerOptions` become constructor parameters on `VelocityArrowsDrawer`.

**Problem solved:** The existing `DiaRigidBodyVisualDebugger` is a monolithic on/off toggle — you cannot suppress AABB overlays while keeping shape outlines, and you cannot selectively enable contact normals. The stack model makes each concern independently toggleable via `DebugLayerManager`.

---

## Acceptance Criteria

1. Five focused draw classes replace `DiaRigidBodyVisualDebugger`: `PhysicsShapesDrawer`, `PhysicsAABBDrawer`, `VelocityArrowsDrawer`, `ContactNormalsDrawer`, `ConstraintLinesDrawer`
2. Each implements `IVisualDebugger` and lives in `DiaRigidBody2DVisualDebugger.vcxproj`
3. Each stores a `const PhysicsWorld&` reference and a `const Dia::Debug::DebugLayerManager&` reference (set at construction, not per-call)
4. `Draw(FrameData&)` uses `DebugColourPalette` constants — no ad-hoc colour literals
5. All sizes/lengths multiplied by `DebugLayerManager::GetDebugScale()` — pass manager ref at construction
6. Canonical layer names from `DebugLayerNames.h` used at `GetLayerName()` return
7. Old `DiaRigidBodyVisualDebugger` class and `VisualDebuggerOptions` struct are **removed**
8. `DiaVisualDebugger.lib` added as project reference to `DiaRigidBody2DVisualDebugger.vcxproj`
9. Build with no warnings; all tests pass

---

## Draw Classes

### 1. PhysicsShapesDrawer (`LayerNames::kPhysicsShapes`, priority 10)

Draws collision shapes for all bodies in the physics world, coloured by body state.

```cpp
class PhysicsShapesDrawer : public Dia::Debug::IVisualDebugger
{
public:
    PhysicsShapesDrawer(const Dia::RigidBody2D::PhysicsWorld& world,
                        const Dia::Debug::DebugLayerManager& manager);

    Dia::Core::StringCRC GetLayerName() const override { return Dia::Debug::LayerNames::kPhysicsShapes; }
    void Draw(Dia::Graphics::FrameData& frameData) override;

private:
    const Dia::RigidBody2D::PhysicsWorld& mWorld;
    const Dia::Debug::DebugLayerManager&  mManager;
};
```

Draw logic:
- Iterates `world.GetPointBodies()` and `world.GetRigidBodies()`
- Per body, reads `GetTransform()->GetWorldPosition()` and `GetTransform()->GetLocalRotation().AsRadians()`
- Circle bodies: `RequestDraw(pos, radius, colour)` — outline only, no fill
- Polygon bodies: iterate vertices, rotate to world space via `cos/sin`, emit one `RequestDraw(wv0, wv1, colour)` line per edge
- Unknown shape kind fallback: `RequestDrawPoint(pos, colour)`
- Colour by body state:
  - Sleeping (not `IsAwake()`): `DebugColourPalette::kDeepSleep`
  - `BodyType::kDynamic`: `DebugColourPalette::kActive`
  - `BodyType::kStatic`: `DebugColourPalette::kInactive`
  - `BodyType::kKinematic`: `DebugColourPalette::kGoal`

### 2. PhysicsAABBDrawer (`LayerNames::kPhysicsAABB`, priority 15)

Draws the axis-aligned bounding box of each body's shape (new — not present in old debugger).

Draw logic:
- Iterates `world.GetRigidBodies()`
- Per body: reads `GetAABB()` (returns `const AARect&`) — calls `RequestDrawRect(aabb.GetBottomLeft(), aabb.GetTopRight(), DebugColourPalette::kWarning)`
- Root bodies with no transform: skip
- Scale does not affect AABB (it is world-space already); no `debugScale` multiplication needed here

### 3. VelocityArrowsDrawer (`LayerNames::kPhysicsVelocity`, priority 20)

Draws a `Ray2D` on each dynamic, awake body showing its linear velocity direction and magnitude.

```cpp
class VelocityArrowsDrawer : public Dia::Debug::IVisualDebugger
{
public:
    VelocityArrowsDrawer(const Dia::RigidBody2D::PhysicsWorld& world,
                         const Dia::Debug::DebugLayerManager& manager,
                         float arrowScale   = 0.1f,
                         float arrowMaxLen  = 10.0f);

    Dia::Core::StringCRC GetLayerName() const override { return Dia::Debug::LayerNames::kPhysicsVelocity; }
    void Draw(Dia::Graphics::FrameData& frameData) override;

private:
    const Dia::RigidBody2D::PhysicsWorld& mWorld;
    const Dia::Debug::DebugLayerManager&  mManager;
    float mArrowScale;
    float mArrowMaxLen;
};
```

Draw logic:
- Iterates `world.GetPointBodies()` and `world.GetRigidBodies()`
- Skips sleeping bodies (`!IsAwake()`) and static bodies
- Computes speed; skips if `speed < 1e-4f`
- Arrow length = `min(speed * mArrowScale, mArrowMaxLen) * debugScale`
- `RequestDrawRay(pos, normalizedVel, len, DebugColourPalette::kWarning)` — yellow

### 4. ContactNormalsDrawer (`LayerNames::kPhysicsContacts`, priority 20)

Draws a ray at each contact point showing the collision normal from the last simulation step.

Draw logic:
- Iterates `world.GetLastContacts()`
- Per contact: `RequestDrawRay(contact.point, contact.normal, kContactNormalLength * scale, DebugColourPalette::kError)` — red
- `kContactNormalLength = 0.3f` (static constexpr inside .cpp)

### 5. ConstraintLinesDrawer (`LayerNames::kPhysicsConstraints`, priority 10)

Draws a line between the two world-space anchor points of each constraint.

Draw logic:
- Iterates `world.GetConstraints()`
- Per constraint: `RequestDraw(c->GetWorldAnchorA(), c->GetWorldAnchorB(), DebugColourPalette::kGoal)` — cyan

---

## Colour Mapping

| Old literal | New constant | Semantic |
|-------------|--------------|----------|
| `RGBA::White` (dynamic body) | `kActive` | Active/dynamic state |
| `RGBA(128,128,128,255)` (static body) | `kInactive` | Inactive/static state |
| `RGBA::Cyan` (kinematic body) | `kGoal` | Externally driven |
| `RGBA(0,0,80,255)` (sleeping body) | `kDeepSleep` | Sleeping/hibernated state |
| `RGBA(255,255,0,255)` (velocity) | `kWarning` | Yellow for velocity overlay |
| `RGBA(255,128,0,255)` (contact) | `kError` | Red/orange for collision events |
| `RGBA(0,200,255,255)` (constraint) | `kGoal` | Cyan — joint/target links |

Note: `kError` is red (255,0,0,255) not orange — contact normals shift from orange to red to align with the palette. This is an intentional improvement over the old literal.

---

## Registration Example (game code)

```cpp
static PhysicsShapesDrawer    shapesDrawer  (world, manager);
static PhysicsAABBDrawer      aabbDrawer    (world, manager);
static VelocityArrowsDrawer   velDrawer     (world, manager);
static ContactNormalsDrawer   contactDrawer (world, manager);
static ConstraintLinesDrawer  constraintDrawer(world, manager);

manager.Register(&shapesDrawer,     10);
manager.Register(&aabbDrawer,       15);
manager.Register(&velDrawer,        20);
manager.Register(&contactDrawer,    20);
manager.Register(&constraintDrawer, 10);

// Each frame, after PhysicsWorld::Step():
manager.Draw(frameData);
```

---

## Files Changed

| File | Change |
|------|--------|
| `Dia/DiaRigidBody2DVisualDebugger/DiaRigidBodyVisualDebugger.h` | **Removed** |
| `Dia/DiaRigidBody2DVisualDebugger/DiaRigidBodyVisualDebugger.cpp` | **Removed** |
| `Dia/DiaRigidBody2DVisualDebugger/PhysicsShapesDrawer.h` | New |
| `Dia/DiaRigidBody2DVisualDebugger/PhysicsShapesDrawer.cpp` | New |
| `Dia/DiaRigidBody2DVisualDebugger/PhysicsAABBDrawer.h` | New |
| `Dia/DiaRigidBody2DVisualDebugger/PhysicsAABBDrawer.cpp` | New |
| `Dia/DiaRigidBody2DVisualDebugger/VelocityArrowsDrawer.h` | New |
| `Dia/DiaRigidBody2DVisualDebugger/VelocityArrowsDrawer.cpp` | New |
| `Dia/DiaRigidBody2DVisualDebugger/ContactNormalsDrawer.h` | New |
| `Dia/DiaRigidBody2DVisualDebugger/ContactNormalsDrawer.cpp` | New |
| `Dia/DiaRigidBody2DVisualDebugger/ConstraintLinesDrawer.h` | New |
| `Dia/DiaRigidBody2DVisualDebugger/ConstraintLinesDrawer.cpp` | New |
| `Dia/DiaRigidBody2DVisualDebugger/DiaRigidBody2DVisualDebugger.vcxproj` | Remove old files, add 10 new files, add DiaVisualDebugger reference |
| `Dia/DiaRigidBody2DVisualDebugger/DiaRigidBody2DVisualDebugger.vcxproj.filters` | Update filters |

**Dependencies added to vcxproj:** `DiaVisualDebugger.lib` (for `IVisualDebugger`, `DebugColourPalette`, `DebugLayerNames`, `DebugLayerManager`)

**Prerequisites:** `debug-budget`, `debug-text-primitive`, `debug-layer-manager` all implemented.

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 0 | Audit `CluicheTest/` for `#include "DiaRigidBodyVisualDebugger.h"` — update call sites to register stack classes | Before removing old files |
| 1 | Write `PhysicsShapesDrawer.h/.cpp` | Migrates `DrawBodies` logic; uses `kActive`/`kInactive`/`kGoal`/`kDeepSleep` |
| 2 | Write `PhysicsAABBDrawer.h/.cpp` | New — queries `GetAABB()` per rigid body; uses `kWarning` |
| 3 | Write `VelocityArrowsDrawer.h/.cpp` | Migrates `DrawVelocityArrows`; constructor takes arrowScale + arrowMaxLen |
| 4 | Write `ContactNormalsDrawer.h/.cpp` | Migrates `DrawContactNormals`; uses `kError` |
| 5 | Write `ConstraintLinesDrawer.h/.cpp` | Migrates `DrawConstraints`; uses `kGoal` |
| 6 | Update `DiaRigidBody2DVisualDebugger.vcxproj` — remove old files, add 10 new, add DiaVisualDebugger reference | |
| 7 | Update `DiaRigidBody2DVisualDebugger.vcxproj.filters` | |
| 8 | Build solution — verify zero warnings | `msbuild Cluiche/Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |
| 9 | Write tests | `TestRigidBody2DVisualDebuggerStack.cpp` |
| 10 | Run tests | `Cluiche/bin/Debug/x64/UnitTests.exe` |

---

## Test Plan

**File:** `Cluiche/Tests/GoogleTests/RigidBody2D/TestRigidBody2DVisualDebuggerStack.cpp`

Test setup: a `PhysicsWorld` with 3 bodies (one dynamic, one static, one kinematic), two constraints, and seeded contact data. World transforms known.

| Suite | Test | What it verifies |
|-------|------|-----------------|
| PhysicsShapesDrawer | `Draw_DynamicCircle_DrawsCircle` | Dynamic circle body → one circle primitive |
| PhysicsShapesDrawer | `Draw_DynamicCircle_IsActive` | Dynamic circle colour == `kActive` |
| PhysicsShapesDrawer | `Draw_StaticBody_IsInactive` | Static body colour == `kInactive` |
| PhysicsShapesDrawer | `Draw_KinematicBody_IsGoal` | Kinematic body colour == `kGoal` |
| PhysicsShapesDrawer | `Draw_SleepingBody_IsDeepSleep` | Sleeping body colour == `kDeepSleep` |
| PhysicsShapesDrawer | `Draw_Disabled_NoPrimitives` | `SetEnabled(false)` → 0 primitives |
| PhysicsShapesDrawer | `LayerName_IsPhysicsShapes` | `GetLayerName() == LayerNames::kPhysicsShapes` |
| VelocityArrowsDrawer | `Draw_StaticBody_NoArrow` | Static bodies skipped |
| VelocityArrowsDrawer | `Draw_SleepingBody_NoArrow` | Sleeping bodies skipped |
| VelocityArrowsDrawer | `Draw_MovingBody_DrawsRay` | Awake dynamic body with velocity → 1 ray |
| VelocityArrowsDrawer | `Draw_Colour_IsWarning` | Velocity ray colour == `kWarning` |
| VelocityArrowsDrawer | `Draw_Scale_AffectsLength` | `SetDebugScale(2.0f)` → ray length doubles |
| VelocityArrowsDrawer | `LayerName_IsPhysicsVelocity` | `GetLayerName() == LayerNames::kPhysicsVelocity` |
| ContactNormalsDrawer | `Draw_TwoContacts_TwoRays` | 2 contacts → 2 ray primitives |
| ContactNormalsDrawer | `Draw_Colour_IsError` | Contact ray colour == `kError` |
| ContactNormalsDrawer | `Draw_Normal_MatchesContactNormal` | Ray direction matches `contact.normal` |
| ContactNormalsDrawer | `LayerName_IsPhysicsContacts` | `GetLayerName() == LayerNames::kPhysicsContacts` |
| ConstraintLinesDrawer | `Draw_TwoConstraints_TwoLines` | 2 constraints → 2 line primitives |
| ConstraintLinesDrawer | `Draw_Colour_IsGoal` | Constraint line colour == `kGoal` |
| ConstraintLinesDrawer | `Draw_Endpoints_MatchAnchors` | Line endpoints match `GetWorldAnchorA/B()` |
| ConstraintLinesDrawer | `LayerName_IsPhysicsConstraints` | `GetLayerName() == LayerNames::kPhysicsConstraints` |
| PhysicsAABBDrawer | `Draw_RigidBody_DrawsRect` | 1 rigid body → 1 rect primitive |
| PhysicsAABBDrawer | `Draw_Colour_IsWarning` | AABB rect colour == `kWarning` |
| PhysicsAABBDrawer | `LayerName_IsPhysicsAABB` | `GetLayerName() == LayerNames::kPhysicsAABB` |
| Migration | `AllFiveDrawers_Registered_DrawCorrectTotalPrimitives` | All 5 registered → total primitive count matches expected sum |

---

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|-----------|
| PD-001 | StringCRC for identifiers | Compliant — layer names use `LayerNames::kPhysics*` constants (StringCRC) |
| PD-002 | ProcessingUnit/Phase/Module | Compliant — draw classes are plain C++ objects; caller schedules Draw() |
| PD-003 | Component-based entities | Compliant — no entity ID concerns in physics drawing; picking seam is manager's concern |
| PD-004 | No STL in public APIs | Compliant — all public methods use `DynamicArrayC`, `StringCRC`, and primitives only |
| PD-005 | x64 only | Compliant |
| PD-006 | VS project files are source of truth | Compliant — `DiaRigidBody2DVisualDebugger.vcxproj` updated; old files removed, new files added |
| PD-007 | C++20 required | Compliant |
| PD-008 | `Directory.Build.props` owns build paths | Compliant — no per-project overrides |
| PD-009 | Generated output under `Cluiche/out/` | Compliant — no generated output |
| AD-001 | Module YAML frontmatter | Compliant — `dia.rigidbody2dvisualdebugger.architecture.module.md` updated to reflect new class list |
| AD-002 | No STL in public APIs | Compliant |
| AD-003 | `Dia::<Module>::` namespace | Compliant — all classes in `Dia::RigidBody2D::` namespace |
| SD-DBG-001 | Stack of focused draw classes | Compliant — this feature implements the stack model for RigidBody2D |
| SD-DBG-002 | `#ifdef DIA_DEBUG` | Compliant — `DiaRigidBody2DVisualDebugger.vcxproj` not linked in Release |
| SD-DBG-003 | Priority-ordered draw | Compliant — canonical priorities used: 10 for geometry, 15 for AABB, 20 for overlays |
| SD-DBG-005 | Global `debugScale` | Compliant — velocity arrow length multiplied by `mManager.GetDebugScale()` |
| SD-DBG-006 | Assert on name collision | Compliant — each drawer returns a distinct `LayerNames::kPhysics*` constant |
| SD-DBG-010 | `DebugColourPalette` colours binding | Compliant — all ad-hoc RGBA literals replaced with palette constants |
| SD-DBG-014 | Same-family classes share vcxproj | Compliant — all 5 physics draw classes in `DiaRigidBody2DVisualDebugger.vcxproj` |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | PhysicsAABBDrawer — GetAABB() | The existing `Body2DBase` class is read via `GetTransform()` and `GetShapeKind()`. Does it expose a `GetAABB()` method returning a world-space `AARect`? If not, the AABB must be computed inline (center ± halfExtents from shape + transform). | Must be verified at implementation time. If `GetAABB()` does not exist, compute AABB inline: for circles, `AARect(pos - radius, pos + radius)`; for polygons, compute min/max over rotated vertices. Task 2 should check this before writing `PhysicsAABBDrawer`. |
| 2 | ContactNormal colour change | The old `kContactColour` was orange `RGBA(255,128,0,255)`. The new mapping uses `kError` (red). Is this an acceptable intentional change? | Yes — orange maps to `kCapped` in the palette, not `kError`. Contact normals indicate collision events (errors in a sense), so `kError` (red) is the better semantic fit. Orange (`kCapped`) is reserved for cap/clamp indicators. |
| 3 | `VisualDebuggerOptions` removal | `VisualDebuggerOptions::velocityArrowScale` and `velocityArrowMaxLen` were configurable via `SetOptions()`. Moving them to constructor parameters means callers must reconstruct the drawer to change them. Is this acceptable? | Yes — debug draw configuration is set once at startup, not changed per-frame. Constructor parameters are appropriate. If per-frame adjustment is needed in future, a `SetArrowScale(float)` setter can be added then. |
| 4 | Old debugger audit | Task 0 requires auditing `CluicheTest/` for `DiaRigidBodyVisualDebugger` usage. Where is the most likely call site? | Most likely in `CluicheTest/Stages/DummyStage.cpp` or the main processing unit setup. Grep for `DiaRigidBodyVisualDebugger` before removing old files. |
| 5 | debugScale on AABB | `PhysicsAABBDrawer` draws world-space AABBs that don't need scale multiplication. Is this correct? | Yes — AABB coordinates are already in world space; multiplying by debugScale would distort them. Only sizes derived from simulation parameters (arrow lengths, circle radii for decorative markers) get the scale multiplier. |

---

## Status

`Approved`

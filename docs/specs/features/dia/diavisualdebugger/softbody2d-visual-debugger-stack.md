# Feature Spec: softbody2d-visual-debugger-stack

## Traceability

| Level | Spec |
|-------|------|
| Platform | @docs/specs/platform/Cluiche.md |
| Application | @docs/specs/applications/dia.md |
| System | @docs/specs/systems/dia/diavisualdebugger.md |
| Feature | this file |

---

## Summary

Creates a new `DiaSoftBody2DVisualDebugger` module as a stack of focused `IVisualDebugger` draw classes. There is no existing debugger to decompose — this is entirely new. Four draw classes cover particles, constraints, anchor links, and velocity arrows. Constraint lines are colour-coded by `DistanceConstraint::type` (rope, structural, shear, bend), giving developers immediate visual insight into cloth and rope simulation internals.

**Problem solved:** `DiaSoftBody2D` has no visual debugging at all. Developers cannot see particle positions, constraint connections, anchor attachment points, or velocity without adding bespoke draw code. The four-class stack provides full simulation visibility on independent, toggleable layers.

---

## Acceptance Criteria

1. Four focused draw classes: `SoftParticlesDrawer`, `SoftConstraintsDrawer`, `SoftAnchorLinksDrawer`, `SoftVelocityDrawer`
2. Each implements `IVisualDebugger` and lives in a new `DiaSoftBody2DVisualDebugger.vcxproj` static library
3. Each stores a `const SoftBodyWorld&` and `const Dia::Debug::DebugLayerManager&` reference (set at construction)
4. `Draw(FrameData&)` uses `DebugColourPalette` constants — no ad-hoc colour literals
5. `SoftConstraintsDrawer` colour-codes lines by `DistanceConstraint::type`: rope=`kActive`, structural=`kActive`, shear=`kGoal`, bend=`kInactive`; torn (inactive) constraints are **skipped**
6. `SoftParticlesDrawer` distinguishes pinned particles (`invMass == 0`) from dynamic particles
7. `SoftAnchorLinksDrawer` draws lines from rope endpoint particles to their rigid-body anchor world positions; skips `nullptr` anchors
8. `SoftVelocityDrawer` derives velocity from Verlet state (`position - prevPosition`) and draws as a line segment; skips particles where delta magnitude < 1e-4f
9. All sizes multiplied by `DebugLayerManager::GetDebugScale()`
10. Canonical layer names from `DebugLayerNames.h` used at `GetLayerName()` return
11. New `DiaSoftBody2DVisualDebugger.vcxproj` added to `Cluiche.sln`
12. Build with no warnings; all tests pass

---

## Draw Classes

### 1. SoftParticlesDrawer (`LayerNames::kSoftParticles`, priority 10)

Draws a circle at each particle position, coloured by pinning state.

Draw logic:
- Iterates `world.GetBodies()` → for each body, `GetParticleCount()` → `GetParticle(i)`
- Pinned particle (`invMass == 0`): `RequestDraw(pos, particle.radius * scale, DebugColourPalette::kPinned)` — magenta
- Dynamic particle (`invMass > 0`): `RequestDraw(pos, particle.radius * scale, DebugColourPalette::kActive)` — white

### 2. SoftConstraintsDrawer (`LayerNames::kSoftConstraints`, priority 10)

Draws a line between the two particles of each constraint, colour-coded by constraint type.

Draw logic:
- Iterates `world.GetBodies()` → for each body, `GetConstraintCount()` → `GetConstraint(i)`
- Skips constraints where `constraint.active == false` (torn)
- Retrieves `posA = GetParticle(constraint.indexA).position` and `posB = GetParticle(constraint.indexB).position`
- Colour by `constraint.type`:
  - `kRope`: `DebugColourPalette::kActive` — white rope chain
  - `kStructural`: `DebugColourPalette::kActive` — white cloth structure
  - `kShear`: `DebugColourPalette::kGoal` — cyan diagonal bracing
  - `kBend`: `DebugColourPalette::kInactive` — grey skip-one bend springs
- `RequestDraw(posA, posB, colour)`

### 3. SoftAnchorLinksDrawer (`LayerNames::kSoftAnchors`, priority 20)

Draws lines from rope endpoint particles to their rigid-body anchor world positions.

Draw logic:
- Iterates `world.GetBodies()` — only processes bodies where `GetBodyType() == BodyType::kRope` (downcast to `Rope*`)
- `GetStartAnchor()` is non-null: `RequestDraw(rope.GetParticle(0).position, startAnchor->GetTransform()->GetWorldPosition(), DebugColourPalette::kWarning)` — yellow
- `GetEndAnchor()` is non-null: `RequestDraw(rope.GetParticle(count-1).position, endAnchor->GetTransform()->GetWorldPosition(), DebugColourPalette::kWarning)` — yellow
- Skips ropes with zero particles

### 4. SoftVelocityDrawer (`LayerNames::kSoftVelocity`, priority 20)

Draws the Verlet velocity (position delta) of each dynamic particle as a line segment.

Draw logic:
- Iterates all particles across all bodies
- Skips pinned particles (`invMass == 0`)
- Computes `delta = particle.position - particle.prevPosition`
- Skips if `sqrt(delta.x*delta.x + delta.y*delta.y) < 1e-4f`
- `RequestDraw(particle.position, particle.position + delta, DebugColourPalette::kHealthy)` — green
- Note: draws raw one-step displacement (no timestep scaling); length reflects actual per-frame movement

---

## Constraint Type Colour Reference

| Type | Colour | Semantic |
|------|--------|---------|
| `kRope` | `kActive` (white) | Main chain links |
| `kStructural` | `kActive` (white) | Horizontal/vertical cloth grid |
| `kShear` | `kGoal` (cyan) | Diagonal stiffening |
| `kBend` | `kInactive` (grey) | Long-range bend resistance |
| Torn (`!active`) | **skipped** | Not drawn |

---

## New Module: DiaSoftBody2DVisualDebugger

This feature creates a new Visual Studio project. It follows `DiaRigidBody2DVisualDebugger.vcxproj` as the structural template.

**Project references required:**
- `DiaCore.vcxproj`
- `DiaMaths.vcxproj`
- `DiaSoftBody2D.vcxproj`
- `DiaGraphics.vcxproj`
- `DiaVisualDebugger.vcxproj`

---

## Registration Example (game code)

```cpp
static SoftParticlesDrawer   particlesDrawer  (softWorld, manager);
static SoftConstraintsDrawer constraintsDrawer(softWorld, manager);
static SoftAnchorLinksDrawer anchorsDrawer    (softWorld, manager);
static SoftVelocityDrawer    velocityDrawer   (softWorld, manager);

manager.Register(&particlesDrawer,   10);
manager.Register(&constraintsDrawer, 10);
manager.Register(&anchorsDrawer,     20);
manager.Register(&velocityDrawer,    20);

// Each frame, after SoftBodyWorld::Update():
manager.Draw(frameData);
```

---

## Files Changed

| File | Change |
|------|--------|
| `Dia/DiaSoftBody2DVisualDebugger/SoftParticlesDrawer.h` | New |
| `Dia/DiaSoftBody2DVisualDebugger/SoftParticlesDrawer.cpp` | New |
| `Dia/DiaSoftBody2DVisualDebugger/SoftConstraintsDrawer.h` | New |
| `Dia/DiaSoftBody2DVisualDebugger/SoftConstraintsDrawer.cpp` | New |
| `Dia/DiaSoftBody2DVisualDebugger/SoftAnchorLinksDrawer.h` | New |
| `Dia/DiaSoftBody2DVisualDebugger/SoftAnchorLinksDrawer.cpp` | New |
| `Dia/DiaSoftBody2DVisualDebugger/SoftVelocityDrawer.h` | New |
| `Dia/DiaSoftBody2DVisualDebugger/SoftVelocityDrawer.cpp` | New |
| `Dia/DiaSoftBody2DVisualDebugger/DiaSoftBody2DVisualDebugger.vcxproj` | New project |
| `Dia/DiaSoftBody2DVisualDebugger/DiaSoftBody2DVisualDebugger.vcxproj.filters` | New filters |
| `Cluiche/Cluiche.sln` | Add new project |

**Prerequisites:** `debug-budget`, `debug-text-primitive`, `debug-layer-manager` all implemented.

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Create `DiaSoftBody2DVisualDebugger/` directory and new `DiaSoftBody2DVisualDebugger.vcxproj` | Modelled on `DiaRigidBody2DVisualDebugger.vcxproj` |
| 2 | Write `SoftParticlesDrawer.h/.cpp` | Pinned=magenta, dynamic=white; radius scaled by debugScale |
| 3 | Write `SoftConstraintsDrawer.h/.cpp` | Colour by constraint type; skip torn constraints |
| 4 | Write `SoftAnchorLinksDrawer.h/.cpp` | Rope only; null-check anchors; yellow lines |
| 5 | Write `SoftVelocityDrawer.h/.cpp` | Verlet delta velocity; green lines; skip pinned + near-zero |
| 6 | Add project to `Cluiche.sln` | |
| 7 | Build solution — verify zero warnings | `msbuild Cluiche/Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |
| 8 | Write tests | `TestSoftBody2DVisualDebuggerStack.cpp` |
| 9 | Run tests | `Cluiche/bin/Debug/x64/UnitTests.exe` |

---

## Test Plan

**File:** `Cluiche/Tests/GoogleTests/SoftBody2D/TestSoftBody2DVisualDebuggerStack.cpp`

Test setup: a `SoftBodyWorld` containing a 3-particle rope (start anchor, middle free, end anchor) and a 4-particle cloth patch (2×2 grid, top row pinned).

| Suite | Test | What it verifies |
|-------|------|-----------------|
| SoftParticlesDrawer | `Draw_ThreeParticles_ThreeCircles` | 3 particles → 3 circle primitives |
| SoftParticlesDrawer | `Draw_PinnedParticle_IsMagenta` | `invMass==0` particle → `kPinned` colour |
| SoftParticlesDrawer | `Draw_DynamicParticle_IsWhite` | `invMass>0` particle → `kActive` colour |
| SoftParticlesDrawer | `Draw_Disabled_NoPrimitives` | `SetEnabled(false)` → 0 primitives |
| SoftParticlesDrawer | `LayerName_IsSoftParticles` | `GetLayerName() == LayerNames::kSoftParticles` |
| SoftConstraintsDrawer | `Draw_RopeConstraint_IsActive` | `kRope` constraint → `kActive` colour |
| SoftConstraintsDrawer | `Draw_StructuralConstraint_IsActive` | `kStructural` constraint → `kActive` colour |
| SoftConstraintsDrawer | `Draw_ShearConstraint_IsGoal` | `kShear` constraint → `kGoal` colour |
| SoftConstraintsDrawer | `Draw_BendConstraint_IsInactive` | `kBend` constraint → `kInactive` colour |
| SoftConstraintsDrawer | `Draw_TornConstraint_Skipped` | `active==false` → not drawn |
| SoftConstraintsDrawer | `LayerName_IsSoftConstraints` | `GetLayerName() == LayerNames::kSoftConstraints` |
| SoftAnchorLinksDrawer | `Draw_RopeWithTwoAnchors_TwoLines` | 2 non-null anchors → 2 line primitives |
| SoftAnchorLinksDrawer | `Draw_NullAnchor_Skipped` | Null anchor → no line |
| SoftAnchorLinksDrawer | `Draw_Colour_IsWarning` | Anchor lines are `kWarning` |
| SoftAnchorLinksDrawer | `LayerName_IsSoftAnchors` | `GetLayerName() == LayerNames::kSoftAnchors` |
| SoftVelocityDrawer | `Draw_MovingParticle_DrawsLine` | Particle with delta > epsilon → 1 line |
| SoftVelocityDrawer | `Draw_StillParticle_Skipped` | delta < 1e-4f → no primitive |
| SoftVelocityDrawer | `Draw_PinnedParticle_Skipped` | `invMass==0` → no primitive |
| SoftVelocityDrawer | `Draw_Colour_IsHealthy` | Velocity lines are `kHealthy` |
| SoftVelocityDrawer | `LayerName_IsSoftVelocity` | `GetLayerName() == LayerNames::kSoftVelocity` |

---

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|-----------|
| PD-001 | StringCRC for identifiers | Compliant — layer names use `LayerNames::kSoft*` constants (StringCRC) |
| PD-002 | ProcessingUnit/Phase/Module | Compliant — draw classes are plain C++ objects; caller schedules Draw() |
| PD-003 | Component-based entities | Compliant — no entity ID concerns; picking seam is manager's concern |
| PD-004 | No STL in public APIs | Compliant — all public methods use `DynamicArrayC` and primitives only |
| PD-005 | x64 only | Compliant |
| PD-006 | VS project files are source of truth | Compliant — new `DiaSoftBody2DVisualDebugger.vcxproj` created; added to solution |
| PD-007 | C++20 required | Compliant |
| PD-008 | `Directory.Build.props` owns build paths | Compliant — no per-project overrides |
| PD-009 | Generated output under `Cluiche/out/` | Compliant — no generated output |
| AD-001 | Module YAML frontmatter | Compliant — new `dia.softbody2dvisualdebugger.architecture.module.md` created |
| AD-002 | No STL in public APIs | Compliant |
| AD-003 | `Dia::<Module>::` namespace | Compliant — all classes in `Dia::SoftBody2D::` namespace |
| SD-DBG-001 | Stack of focused draw classes | Compliant — this feature implements the stack model for SoftBody2D |
| SD-DBG-002 | `#ifdef DIA_DEBUG` | Compliant — `DiaSoftBody2DVisualDebugger.vcxproj` not linked in Release |
| SD-DBG-003 | Priority-ordered draw | Compliant — geometry at 10, overlays at 20 |
| SD-DBG-005 | Global `debugScale` | Compliant — particle radius multiplied by `mManager.GetDebugScale()` |
| SD-DBG-006 | Assert on name collision | Compliant — each drawer returns a distinct `LayerNames::kSoft*` constant |
| SD-DBG-010 | `DebugColourPalette` colours binding | Compliant — all colours from palette |
| SD-DBG-014 | Same-family classes share vcxproj | Compliant — all 4 soft body draw classes in `DiaSoftBody2DVisualDebugger.vcxproj` |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | SoftBodyWorld::GetBodies() | The research agent confirms `GetBodies()` returns `const DynamicArray<SoftBody*>&`. `DynamicArray` uses heap storage (unlike `DynamicArrayC`). Does `SoftParticlesDrawer::Draw()` need to handle a null body pointer from this array? | No — `SoftBodyWorld` owns all bodies; null entries are not expected. A `DIA_ASSERT(body != nullptr)` at the top of the loop is sufficient. |
| 2 | Cloth particle iteration | `Cloth::GetParticle(int x, int y)` takes 2D grid indices, but the constraint `indexA`/`indexB` are flat indices. Is there a flat-index particle accessor on `Cloth`? | Must be verified at implementation time. If only `GetParticle(x, y)` exists, add a `GetParticle(int flatIndex)` accessor (or compute `x = index % resX, y = index / resX`). The spec treats particle access as flat-index for uniformity; Task 3 should check this. |
| 3 | Rope endpoint particle count | `GetEndAnchor()` link draws from `GetParticle(GetParticleCount() - 1)`. If a rope has 0 particles (pathological), this would be invalid. Should a count guard be added? | Yes — guard with `if (rope.GetParticleCount() > 0)` before accessing particles. Already implied in Task 4 notes. |
| 4 | BodyType for rope/cloth | `SoftAnchorLinksDrawer` must downcast `SoftBody*` to `Rope*` only for rope bodies. The research confirms `GetBodyType()` returns an enum distinguishing `kRope` from `kCloth`. Is there a safe cast helper, or must it be `static_cast` with a type guard? | Use `static_cast<const Rope*>(body)` guarded by `body->GetBodyType() == SoftBody::BodyType::kRope`. No dynamic_cast needed (no polymorphic overhead, type is verified). |
| 5 | New vcxproj GUID | The new `DiaSoftBody2DVisualDebugger.vcxproj` needs a unique GUID. | Generate a new GUID at project creation time. Use a UUID generator or VS "Add New Project" which assigns one automatically. Do not reuse any existing GUID. |

---

## Status

`Approved`

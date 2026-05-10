# System Spec: DiaVisualDebugger

## Parent Application
@docs/specs/applications/dia.md

---

## Purpose

DiaVisualDebugger is the visual debug rendering system for the Dia engine. It provides a central `DebugLayerManager` that orchestrates a stack of small, focused draw classes — each responsible for one concern (physics shapes, velocity arrows, bone lines, IK targets, etc.) — registered as named layers with a priority, enabled/disabled independently, and drawn in priority order each frame into `FrameData`.

The stack model replaces the `VisualDebuggerOptions` boolean-flag pattern. Enable/disable, draw order, world scale, colour palette, and editor bridge are all handled at registration time by the manager, not inside each draw class. Every draw class is a pure function: it reads `const` simulation state and writes `DebugPrimitive` objects into `FrameData`.

The manager exposes its layer state via DiaAPI commands so that both in-game keypresses (via DiaVisualDebuggerConsole) and the CluicheEditor panel send the same command regardless of surface.

**Architectural role:**

```
Simulation state (PhysicsWorld, Skeleton, IKSolver, ...)
    ↓  read-only
Draw classes (PhysicsShapesDrawer, BoneLinesDrawer, IKTargetDrawer, ...)
    ↓  IVisualDebugger::Draw(FrameData&)
DebugLayerManager  ←→  DiaAPI commands  ←→  DiaVisualDebuggerConsole / Editor panel
    ↓  sorted by priority
DebugFrameData (inside FrameData)
    ↓
DiaSFML renderer (DebugFrameDataVisitor)
```

**Only two concerns fell outside this model and are deferred:**
- **Rewind** → future `DiaReplay` / simulation capture system
- **Entity picking** → future scene editor; seams reserved in this system

---

## Responsibilities

- Define `IVisualDebugger` — the interface all draw classes implement
- Provide `DebugLayerManager` — registry, priority sort, global scale, DiaAPI command registration, `debug.layer.state` broadcast, overflow reporting
- Provide `DebugColourPalette` — canonical RGBA constants with defined semantic meanings
- Provide `DebugLayerNames` — canonical `StringCRC` constants for all Dia-owned layer names
- Decompose `DiaRig2DVisualDebugger` into a stack of focused draw classes
- Decompose `DiaRigidBody2DVisualDebugger` into a stack of focused draw classes
- Provide `DiaSoftBody2DVisualDebugger` as a stack of focused draw classes (new)
- Provide `DiaIK2DVisualDebugger` as a stack of focused draw classes (new)
- Provide `DiaGeometry2DVisualDebugger` as a stack of focused draw classes (new)
- Provide `DiaVisualDebuggerConsole` — ImGui-based in-game debug console (layer toggles, metrics, DiaAPI command input, log tail)
- Provide an Editor Debug Layer Panel plugin for CluicheEditor
- Extend `DiaGraphics` with a configurable `DebugFrameData` budget and `DroppedCount()`
- Extend `DiaGraphics` with a `TextPrimitive` world-space text variant in `DebugPrimitive`
- Provide `DiaAnimation2DVisualDebugger` stack of focused draw classes for animation state visibility
- Guard all debug draw code with `#ifdef DIA_DEBUG`; zero overhead in Release

## Non-Responsibilities

- Simulation state modification — all draw classes are strictly read-only
- Rendering — writes primitives to `FrameData`; DiaSFML owns rendering
- Entity picking — deferred to scene editor research; seams reserved
- Rewind / replay — deferred to `DiaReplay`; `DebugFrameData` stays trivially copyable as a seam
- Persistent draws across multiple frames — each `Draw()` call is stateless per frame
- Hot reload lifecycle — caller's responsibility to deregister/reregister layers around hot reload

---

## Public Interfaces

### IVisualDebugger

```cpp
// Dia/DiaVisualDebugger/IVisualDebugger.h
namespace Dia::Debug {

class IVisualDebugger {
public:
    virtual ~IVisualDebugger() = default;
    virtual Dia::Core::StringCRC GetLayerName() const = 0;
    virtual void Draw(Dia::Graphics::FrameData& frameData) = 0;
};

} // namespace Dia::Debug
```

### DebugLayerManager

```cpp
// Dia/DiaVisualDebugger/DebugLayerManager.h
namespace Dia::Debug {

class DebugLayerManager {
public:
    static const unsigned int kMaxLayers = 64;

    // Register a draw class. DIA_ASSERT if layerName already registered.
    // priority: lower = drawn first (under); higher = drawn last (on top).
    void Register(IVisualDebugger* debugger, int priority = 0);

    void UnregisterLayer(Dia::Core::StringCRC layerName);

    void EnableLayer(Dia::Core::StringCRC layerName);
    void DisableLayer(Dia::Core::StringCRC layerName);
    bool IsLayerEnabled(Dia::Core::StringCRC layerName) const;

    // Global scale applied to all size/length parameters at draw time.
    void  SetDebugScale(float scale);
    float GetDebugScale() const;

    // Picking seam — no-op until scene editor. Stored; passed to draw classes that check it.
    void     SetSelectedEntityId(uint32_t id);
    uint32_t GetSelectedEntityId() const;

    // Call once per frame. Draws all enabled layers in priority order.
    void Draw(Dia::Graphics::FrameData& frameData);

    // DiaAPI command registration — call once at application startup.
    // Registers: debug.layer.enable, debug.layer.disable, debug.layer.list,
    //            debug.scale, debug.pick (no-op stub)
    void RegisterDiaAPICommands();

    // Broadcast current layer state to DiaDebugServer subscribers.
    // Call after Draw() each frame.
    void BroadcastLayerState();

private:
    struct LayerEntry {
        IVisualDebugger* debugger = nullptr;
        int              priority = 0;
        bool             enabled  = true;
    };

    Dia::Core::Containers::DynamicArrayC<LayerEntry, kMaxLayers> mLayers;
    float    mDebugScale        = 1.0f;
    uint32_t mSelectedEntityId  = 0;  // 0 = none (picking seam)
    bool     mSortDirty         = false;

    void SortByPriority();
    int  FindLayerIndex(Dia::Core::StringCRC layerName) const;
};

} // namespace Dia::Debug
```

### DebugColourPalette

```cpp
// Dia/DiaVisualDebugger/DebugColourPalette.h
namespace Dia::Debug {

struct DebugColourPalette {
    static const Dia::Graphics::RGBA kActive;       // white  — dynamic/active
    static const Dia::Graphics::RGBA kInactive;     // grey   — static/sleeping/inactive
    static const Dia::Graphics::RGBA kHealthy;      // green  — converged/solved/ok
    static const Dia::Graphics::RGBA kWarning;      // yellow — best-effort/warning
    static const Dia::Graphics::RGBA kError;        // red    — failed/torn/error
    static const Dia::Graphics::RGBA kGoal;         // cyan   — target/goal
    static const Dia::Graphics::RGBA kPinned;       // magenta — pinned/constrained
    static const Dia::Graphics::RGBA kCapped;       // orange  — capped/limit-hit
    static const Dia::Graphics::RGBA kDeepSleep;    // dark blue — deep sleep
};

} // namespace Dia::Debug
```

### DebugLayerNames

```cpp
// Dia/DiaVisualDebugger/DebugLayerNames.h
namespace Dia::Debug {

// Canonical layer name constants — use these to avoid accidental CRC collisions.
namespace LayerNames {
    static const Dia::Core::StringCRC kRigBones         = "rig.bones";
    static const Dia::Core::StringCRC kRigJoints        = "rig.joints";
    static const Dia::Core::StringCRC kRigArrows        = "rig.arrows";
    static const Dia::Core::StringCRC kRigRestPose      = "rig.rest_pose";
    static const Dia::Core::StringCRC kRigLabels        = "rig.labels";

    static const Dia::Core::StringCRC kPhysicsShapes    = "physics.shapes";
    static const Dia::Core::StringCRC kPhysicsAABB      = "physics.aabb";
    static const Dia::Core::StringCRC kPhysicsVelocity  = "physics.velocity";
    static const Dia::Core::StringCRC kPhysicsContacts  = "physics.contacts";
    static const Dia::Core::StringCRC kPhysicsConstraints = "physics.constraints";

    static const Dia::Core::StringCRC kSoftParticles    = "soft.particles";
    static const Dia::Core::StringCRC kSoftConstraints  = "soft.constraints";
    static const Dia::Core::StringCRC kSoftAnchors      = "soft.anchors";
    static const Dia::Core::StringCRC kSoftVelocity     = "soft.velocity";

    static const Dia::Core::StringCRC kIKChains         = "ik.chains";
    static const Dia::Core::StringCRC kIKTargets        = "ik.targets";
    static const Dia::Core::StringCRC kIKPoleVectors    = "ik.pole_vectors";
    static const Dia::Core::StringCRC kIKLimits         = "ik.limits";
    static const Dia::Core::StringCRC kIKConvergence    = "ik.convergence";

    static const Dia::Core::StringCRC kGeoShapes        = "geometry.shapes";
    static const Dia::Core::StringCRC kGeoSpatialGrid   = "geometry.spatial_grid";
    static const Dia::Core::StringCRC kGeoQuadtree      = "geometry.quadtree";
    static const Dia::Core::StringCRC kGeoBVH           = "geometry.bvh";
    static const Dia::Core::StringCRC kGeoContacts      = "geometry.contacts";

    static const Dia::Core::StringCRC kAnimSpring       = "anim.spring";
    static const Dia::Core::StringCRC kAnimClipCursor   = "anim.clip_cursor";
    static const Dia::Core::StringCRC kAnimBlendWeights = "anim.blend_weights";
}

} // namespace Dia::Debug
```

### DiaGraphics extensions (features 1 and 2)

```cpp
// Extension to DebugFrameData — configurable capacity
namespace Dia::Graphics {
    class DebugFrameData {
    public:
        explicit DebugFrameData(uint32_t capacity = kDefaultCapacity);
        static const uint32_t kDefaultCapacity = 1024;

        bool     IsOverCapacity() const;
        uint32_t DroppedCount() const;
        // ... existing RequestDraw overloads unchanged
    };
}

// New DebugPrimitive variant — Text2D
// Added to the DebugPrimitiveType enum and DebugPrimitive union
struct DebugPrimitiveText2D {
    Dia::Maths::Vector2D position;
    char                 text[64];   // fixed-length, null-terminated; truncated silently
    float                fontSize;
    Dia::Graphics::RGBA  colour;
    uint32_t             entityId;   // 0 = untagged (picking seam)
};
// entityId field added to ALL DebugPrimitive variants as picking seam (0 = untagged)
```

---

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| debug-budget | `DroppedCount()` / `IsOverCapacity()` on `DebugFrameData` + `entityId` picking seam on `DebugPrimitive` — extends DiaGraphics | [debug-budget.md](../../features/dia/diavisualdebugger/debug-budget.md) | Done |
| debug-text-primitive | `Text2D` variant in `DebugPrimitive` + `RequestDrawText()` + DiaSFML visitor `DrawText2D()` — extends DiaGraphics + DiaSFML | [debug-text-primitive.md](../../features/dia/diavisualdebugger/debug-text-primitive.md) | Done |
| debug-layer-manager | `DebugLayerManager`, `IVisualDebugger`, `DebugColourPalette`, `DebugLayerNames` — new DiaVisualDebugger module | [debug-layer-manager.md](../../features/dia/diavisualdebugger/debug-layer-manager.md) | Approved |
| rig2d-visual-debugger-stack | Decompose `DiaRig2DVisualDebugger` into stack of focused draw classes | [rig2d-visual-debugger-stack.md](../../features/dia/diavisualdebugger/rig2d-visual-debugger-stack.md) | Approved |
| rigidbody2d-visual-debugger-stack | Decompose `DiaRigidBody2DVisualDebugger` into stack of focused draw classes | [rigidbody2d-visual-debugger-stack.md](../../features/dia/diavisualdebugger/rigidbody2d-visual-debugger-stack.md) | Approved |
| softbody2d-visual-debugger-stack | New `DiaSoftBody2DVisualDebugger` as stack of focused draw classes | [softbody2d-visual-debugger-stack.md](../../features/dia/diavisualdebugger/softbody2d-visual-debugger-stack.md) | Approved |
| ik2d-visual-debugger-stack | New `DiaIK2DVisualDebugger` as stack of focused draw classes | [ik2d-visual-debugger-stack.md](../../features/dia/diavisualdebugger/ik2d-visual-debugger-stack.md) | Approved |
| geometry2d-visual-debugger-stack | New `DiaGeometry2DVisualDebugger` as stack of focused draw classes | [geometry2d-visual-debugger-stack.md](../../features/dia/diavisualdebugger/geometry2d-visual-debugger-stack.md) | Approved |
| debug-console | `DiaVisualDebuggerConsole` — ImGui in-game debug console (layer toggles, metrics, DiaAPI input, log tail) | [debug-console.md](../../features/dia/diavisualdebugger/debug-console.md) | Approved |
| debug-editor-panel | CluicheEditor `DebugLayerPanel` plugin — checkbox tree, overflow badge, sub-layer toggles | [debug-editor-panel.md](../../features/dia/diavisualdebugger/debug-editor-panel.md) | Approved |
| animation2d-visual-debugger-stack | New `DiaAnimation2DVisualDebugger` stack — clip cursors, blend weights, spring state | [animation2d-visual-debugger-stack.md](../../features/dia/diavisualdebugger/animation2d-visual-debugger-stack.md) | Done |
| fixed-draw-layer | `FixedDrawRegistry`, `IObjectRenderer`, `IFixedPrimitiveBuffer` — render-thread-owned primitive buffers for fixed-topology objects; default renderers for `SpatialGrid`, `Quadtree`, `BVH`, `HexGrid` | [fixed-draw-layer.md](../../features/dia/diavisualdebugger/fixed-draw-layer.md) | Approved |

**Natural build order:** debug-budget → debug-text-primitive → debug-layer-manager → rig2d-visual-debugger-stack → rigidbody2d-visual-debugger-stack → softbody2d-visual-debugger-stack → ik2d-visual-debugger-stack → geometry2d-visual-debugger-stack → debug-console → debug-editor-panel → animation2d-visual-debugger-stack (when unblocked) → fixed-draw-layer (depends on debug-layer-manager)

---

## Dependencies on Other Systems

**Required:**
- **DiaGraphics** — `FrameData`, `DebugFrameData`, `DebugPrimitive`, `RGBA`; extended by features 1 and 2
- **DiaCore** — `StringCRC`, `DynamicArrayC`
- **DiaAPI** — command registration for layer toggle commands
- **DiaDebugServer** — `debug.layer.state` broadcast; `BroadcastLayerState()` calls into DiaDebugServer
- **DiaLogger** — `DroppedCount()` overflow logged via DiaLogger each frame
- **DiaRig2D** — `Skeleton`, `BoneTransform`, `Pose` (rig draw stack)
- **DiaRigidBody2D** — `PhysicsWorld`, `Body2DBase`, `IConstraint` (physics draw stack)
- **DiaSoftBody2D** — `SoftBodyWorld` (soft body draw stack)
- **DiaIK2D** — `IKSolver`, `IKChainDef` (IK draw stack); requires `GetWorldTransforms()` accessor added to `IKSolver`
- **DiaGeometry2D** — shape types, `ISpatialStructure`, `SpatialGrid`, `Quadtree`, `BVH` (geometry draw stack)
- **DiaSFML** — `imgui-sfml` bridge for DiaVisualDebuggerConsole; new visitor case for `TextPrimitive`
- **DiaEditor** — `IEditorPlugin` interface for editor panel
- **DiaDebugProtocol** — `debug.layer.state` message format

**External:**
- **ImGui** (MIT) — `External/imgui/` — new dependency for DiaVisualDebuggerConsole
- **imgui-sfml** — two `.cpp` files added to `DiaSFML`

**Explicitly excluded:**
- **DiaApplicationFlow** — no dependency; caller schedules `Draw()` in the appropriate phase
- **DiaStateMachine** — state graph visualisation belongs in the editor (VisJS), not a draw class

**Dependents:**
- Game code (CluicheTest and future games) — links draw class stacks it needs; links nothing it doesn't use
- CluicheEditor — links editor panel plugin

---

## Out of Scope

- Entity picking — deferred to scene editor; seams reserved (see Decisions SD-DBG-007, SD-DBG-008)
- Debug state rewind — deferred to `DiaReplay`; seam: `DebugFrameData` stays trivially copyable
- State machine graph visualisation — editor concern (VisJS graph), not a draw class
- Phase transition visualisation — DiaApplicationEditor already owns this
- Frame time profiler — DiaDebugServer broadcasts `core_metrics` with per-PU frame times; DiaVisualDebuggerConsole surfaces them
- Tuning profile (partial debug in Release) — out of scope; all debug code guarded by `#ifdef DIA_DEBUG`
- Hot reload lifecycle management — documented as caller responsibility

---

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-DBG-001 | Stack of focused draw classes, not VisualDebuggerOptions bool flags | Each draw concern is independently toggleable, independently testable, independently prioritised; the manager IS the options system | All draw classes | Accepted | Yes |
| SD-DBG-002 | `#ifdef DIA_DEBUG` guards all debug draw code; nothing links in Release | Zero overhead in shipped builds; Tuning profile deferred | All DiaVisualDebugger code | Accepted | Yes |
| SD-DBG-003 | Draw order by integer priority at registration; canonical tiers: 0=spatial structures, 10=simulation shapes, 20=secondary overlays, 30=goals/targets, 40=state indicators | Deterministic layering without per-type sort passes; game code may use any integer | DebugLayerManager | Accepted | Yes |
| SD-DBG-004 | First-come-first-served budget in draw-priority order; `DroppedCount()` logged via DiaLogger on overflow | Simpler than per-layer allocation; highest-priority layers drawn first so they are never starved | DebugLayerManager + DebugFrameData | Accepted | Yes |
| SD-DBG-005 | Global `debugScale` float on `DebugLayerManager`; draw classes read it before submitting primitives | Single knob for world-scale differences without touching each draw class | DebugLayerManager | Accepted | Yes |
| SD-DBG-006 | `DIA_ASSERT` on layer name collision at `Register()`; canonical names in `DebugLayerNames.h` | Prevents silent CRC collisions from ad-hoc string literals | DebugLayerManager | Accepted | Yes |
| SD-DBG-007 | `entityId = 0` reserved on all `DebugPrimitive` variants (0 = untagged) | Picking seam — slots in without breaking the DebugPrimitive binary layout | DiaGraphics DebugPrimitive | Accepted | Yes |
| SD-DBG-008 | `SetSelectedEntityId()` / `GetSelectedEntityId()` stubs on `DebugLayerManager`; `debug.pick` registered as no-op DiaAPI command | Picking seam — editor can call the command without a protocol change when scene editor ships | DebugLayerManager | Accepted | Yes |
| SD-DBG-009 | `DebugFrameData` must remain trivially copyable; no pointer or non-trivial members | Rewind seam — a future `DiaReplay` system needs to snapshot `DebugFrameData` per frame | DiaGraphics DebugFrameData | Accepted | Yes |
| SD-DBG-010 | `DebugColourPalette` canonical colours are binding; draw classes must not use ad-hoc colour literals | Consistent visual language across all draw classes; readable when multiple layers active simultaneously | All draw classes | Accepted | Yes |
| SD-DBG-011 | ImGui (MIT) + imgui-sfml approved as new external dependencies | DiaVisualDebuggerConsole requires immediate-mode UI; ImGui is the industry standard; imgui-sfml is a minimal bridge (two files) | DiaVisualDebuggerConsole, DiaSFML | Accepted | Yes |
| SD-DBG-012 | `TextPrimitive` uses a fixed-length `char[64]` buffer, not `std::string` | PD-004 compliance; `DebugPrimitive` must remain trivially copyable (SD-DBG-009) | DiaGraphics TextPrimitive | Accepted | Yes |
| SD-DBG-013 | `IKSolver::GetWorldTransforms()` accessor required; currently private | DiaIK2DVisualDebugger stack needs world-space joint positions | DiaIK2D | Accepted | Yes |
| SD-DBG-014 | Draw classes for the same module family share a vcxproj (e.g. all rig draw classes in `DiaRig2DVisualDebugger.vcxproj`) | Avoids project proliferation; each distinct module boundary still gets its own vcxproj | Project structure | Accepted | Yes |

---

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|-----------------------------|
| PD-001 | Platform | StringCRC for all identifiers | All layer names, palette keys, and layer name constants use `StringCRC`; no raw `const char*` comparisons |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | `DebugLayerManager::Draw()` is called from game code's sim/render phase; `RegisterDiaAPICommands()` called during module start |
| PD-003 | Platform | Component system for entities | `entityId` picking seam uses `uint32_t` placeholder today; will map to `IComponentObject` ID when scene editor ships |
| PD-004 | Platform | No STL in public APIs | `DebugLayerManager` uses `DynamicArrayC`; `IVisualDebugger` has no STL; `TextPrimitive` uses `char[64]` not `std::string` |
| PD-005 | Platform | x64 only | All new vcxprojs target x64 exclusively |
| PD-006 | Platform | Visual Studio project files are source of truth | New `DiaVisualDebugger.vcxproj`, `DiaVisualDebuggerConsole.vcxproj`, updated `DiaRig2DVisualDebugger.vcxproj`, `DiaRigidBody2DVisualDebugger.vcxproj`, new `DiaSoftBody2DVisualDebugger.vcxproj`, `DiaIK2DVisualDebugger.vcxproj`, `DiaGeometry2DVisualDebugger.vcxproj` — all manually maintained with `.vcxproj.filters` |
| PD-007 | Platform | C++20 required | All code compiled under `/std:c++20` |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir/toolchain | No per-project overrides |
| PD-009 | Platform | Generated output under `Cluiche/out/<AppName>/` | N/A — no generated file output |
| AD-001 | Dia App | Module YAML frontmatter documentation | `dia.debug.architecture.module.md`, `dia.debug.console.architecture.module.md`, `dia.rig2d.visualdebugger.architecture.module.md` etc. created for each new module |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004 |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | `Dia::Debug::` for DebugLayerManager/IVisualDebugger/palette; draw classes in their simulation module's namespace (e.g. `Dia::RigidBody2D::`) |
| AD-004 | Dia App | ProcessingUnit/Phase/Module for app structure | Reinforces PD-002 |
| AD-005 | Dia App | Component-based entities | Picking seam aligned to IComponent entity IDs when scene editor ships |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | IVisualDebugger | Should draw classes implement `IVisualDebugger` or does the manager accept raw lambdas/functors? | `IVisualDebugger` interface — each draw class implements it. World references captured in the draw class constructor. This is PD-004 compliant and avoids `std::function`. |
| 2 | Existing debugger specs | `diarigidbody2dvisualdebugger.md` has an `Approved` system spec with a single monolithic draw class. Does this spec supersede it? | Yes — DiaVisualDebugger supersedes `diarigidbody2dvisualdebugger.md`. The existing spec's Approved status and decisions are folded into this system. The old spec file should be marked `Superseded` once `rigidbody2d-visual-debugger-stack` is Approved. |
| 3 | DiaSoftBody2DVisualDebugger memory | Prior memory records DiaSoftBody2DVisualDebugger with a `VisualDebuggerOptions` struct and 32 tests across 9 suites. That design is superseded by the stack model. | Confirmed — the stack model supersedes the prior design. The memory entry for `project_softbody2d_visual_debugger.md` should be updated when the softbody2d-visual-debugger-stack feature spec is written. |
| 4 | Editor panel dependency | The `debug-editor-panel` feature depends on `DiaApplicationEditor` being built. Is that a hard prerequisite? | Soft prerequisite — the editor panel is a standalone `IEditorPlugin` and can be developed independently. It requires the DiaEditor plugin framework (which exists) and the DiaDebugServer subscription protocol (which exists). DiaApplicationEditor itself does not need to be complete. |
| 5 | ImGui render pass | ImGui requires its own render pass (NewFrame/Render calls) in the DiaSFML render loop. Does this conflict with the existing FrameData/visitor pattern? | ImGui renders independently via its SFML backend — it does not go through `DebugFrameData`. The DiaSFML render loop needs a `DiaVisualDebuggerConsole::Render()` call after the main frame draw. This is additive, not conflicting. |
| 6 | IKSolver accessor | `GetWorldTransforms()` is not currently on `IKSolver`. Should this be a change request on the DiaIK2D spec or tracked as a prerequisite task in the ik2d-visual-debugger-stack feature spec? | Change request on DiaIK2D spec — the accessor belongs to IKSolver's public API. The ik2d-visual-debugger-stack feature spec declares it as a prerequisite. |
| 7 | Budget default | The existing `DebugFrameData` hard-codes 1024. Changing to a constructor parameter is backwards-compatible (default = 1024), but all existing `DebugFrameData` construction sites must be audited. How many sites exist? | To be resolved in the `debug-budget` feature spec — audit all `DebugFrameData` and `FrameData` construction sites before changing the constructor. |
| 8 | DiaVisualDebuggerConsole toggle key | The console toggle keypress is configurable. Who owns that configuration — game code, a config file, or a hardcoded default? | Game code registers the keypress binding and calls `DiaVisualDebuggerConsole::Toggle()`. The console itself has no DiaInput dependency. Default suggestion: backtick key, documented in the feature spec. |

---

## Status

`Approved`

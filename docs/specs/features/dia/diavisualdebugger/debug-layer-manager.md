# Feature Spec: debug-layer-manager

## Traceability

| Level | Spec |
|-------|------|
| Platform | @docs/specs/platform/Cluiche.md |
| Application | @docs/specs/applications/dia.md |
| System | @docs/specs/systems/dia/diavisualdebugger.md |
| Feature | this file |

---

## Summary

Implements the `DiaVisualDebugger` module: `IVisualDebugger`, `DebugLayerManager`, `DebugColourPalette`, and `DebugLayerNames`. The manager is the central registry for all debug draw classes — it sorts layers by priority, calls `Draw()` on each enabled layer per frame, exposes a global debug scale, registers DiaAPI commands for layer toggling, and optionally broadcasts layer state to subscribed editors via `DiaDebugServer`. This is the foundational infrastructure all subsequent draw class stacks build on.

**Problem solved:** Each existing visual debugger has its own isolated `SetEnabled()` with nothing orchestrating them. There is no shared toggle surface, no draw ordering, no global scale, and no editor visibility into which layers are active.

---

## Acceptance Criteria

1. `IVisualDebugger` interface with `GetLayerName()`, `Draw(FrameData&)`, `IsEnabled()`, `SetEnabled(bool)` 
2. `DebugLayerManager` registers draw classes by name+priority; `DIA_ASSERT` on name collision
3. `DebugLayerManager::Draw(FrameData&)` calls all enabled layers in ascending priority order
4. `EnableLayer()` / `DisableLayer()` / `IsLayerEnabled()` toggle layers by `StringCRC` name
5. `SetDebugScale()` / `GetDebugScale()` expose a global float (default 1.0) readable by draw classes
6. `SetSelectedEntityId()` / `GetSelectedEntityId()` stubs (always 0 until scene editor)
7. `RegisterDiaAPICommands()` registers five commands: `debug.layer.enable`, `debug.layer.disable`, `debug.layer.list`, `debug.scale`, `debug.pick` (no-op)
8. `BroadcastLayerState()` serialises layer names + enabled flags to JSON and calls `DebugServerModule::NotifySubscribers()` — optional (no-op if DiaDebugServer is not present)
9. `DebugColourPalette` provides 9 RGBA constants with semantic meanings
10. `DebugLayerNames` provides canonical `StringCRC` constants for all Dia-owned layer names
11. New module lives in `Dia/DiaVisualDebugger/` with its own `DiaVisualDebugger.vcxproj` static library
12. All code guarded by `#ifdef DIA_DEBUG`

---

## Design

### IVisualDebugger

```cpp
// Dia/DiaVisualDebugger/IVisualDebugger.h
namespace Dia::Debug {

class IVisualDebugger
{
public:
    virtual ~IVisualDebugger() = default;
    virtual Dia::Core::StringCRC GetLayerName() const = 0;
    virtual void Draw(Dia::Graphics::FrameData& frameData) = 0;
    virtual void SetEnabled(bool enabled) { mEnabled = enabled; }
    virtual bool IsEnabled() const { return mEnabled; }
private:
    bool mEnabled = true;
};

} // namespace Dia::Debug
```

`SetEnabled` / `IsEnabled` have default implementations so simple draw classes need not override them. Classes with more complex enable semantics (e.g. suppress bones when rig layer is active) can override.

### DebugLayerManager

```cpp
// Dia/DiaVisualDebugger/DebugLayerManager.h
namespace Dia::Debug {

class DebugLayerManager
{
public:
    static const unsigned int kMaxLayers = 64;

    // Register a draw class. DIA_ASSERT if layer name already registered.
    // priority: lower drawn first (underneath), higher drawn last (on top).
    void Register(IVisualDebugger* debugger, int priority = 0);
    void Unregister(Dia::Core::StringCRC layerName);

    // Layer toggle
    void EnableLayer (Dia::Core::StringCRC layerName);
    void DisableLayer(Dia::Core::StringCRC layerName);
    bool IsLayerEnabled(Dia::Core::StringCRC layerName) const;

    // Global scale — draw classes read this before submitting size/length values
    void  SetDebugScale(float scale);
    float GetDebugScale() const;

    // Picking seam — no-op until scene editor
    void     SetSelectedEntityId(uint32_t id);
    uint32_t GetSelectedEntityId() const;

    // Call once per frame after simulation update, before rendering.
    // Draws all enabled layers in ascending priority order.
    void Draw(Dia::Graphics::FrameData& frameData);

    // DiaAPI command registration — call once during application startup.
    // Registers: debug.layer.enable, debug.layer.disable, debug.layer.list,
    //            debug.scale, debug.pick (no-op stub).
    void RegisterDiaAPICommands();

    // Broadcast current layer state to DiaDebugServer subscribers.
    // No-op if debugServer is nullptr.
    // Call after Draw() each frame (or when layer state changes).
    void BroadcastLayerState(Dia::DebugServer::DebugServerModule* debugServer);

    // Query — useful for draw classes that suppress overlap (e.g. IK suppressing
    // bone lines when rig layer is already active).
    int  GetLayerCount() const;
    bool HasLayer(Dia::Core::StringCRC layerName) const;

private:
    struct LayerEntry
    {
        IVisualDebugger* debugger = nullptr;
        int              priority = 0;
    };

    Dia::Core::Containers::DynamicArrayC<LayerEntry, kMaxLayers> mLayers;
    float    mDebugScale       = 1.0f;
    uint32_t mSelectedEntityId = 0;
    bool     mSortDirty        = false;

    void SortByPriority();
    int  FindLayerIndex(Dia::Core::StringCRC layerName) const;
};

} // namespace Dia::Debug
```

### Draw call

```cpp
void DebugLayerManager::Draw(Dia::Graphics::FrameData& frameData)
{
    if (mSortDirty)
        SortByPriority();

    for (unsigned int i = 0; i < mLayers.Size(); ++i)
    {
        IVisualDebugger* d = mLayers[i].debugger;
        if (d->IsEnabled())
            d->Draw(frameData);
    }
}
```

### Registration

```cpp
void DebugLayerManager::Register(IVisualDebugger* debugger, int priority)
{
    DIA_ASSERT(debugger != nullptr, "DebugLayerManager::Register — debugger must not be null");
    DIA_ASSERT(FindLayerIndex(debugger->GetLayerName()) < 0,
               "DebugLayerManager::Register — layer name already registered");

    LayerEntry entry;
    entry.debugger = debugger;
    entry.priority = priority;
    mLayers.Add(entry);
    mSortDirty = true;
}
```

### Sort

Uses an insertion sort (stable, in-place, appropriate for small N ≤ 64):

```cpp
void DebugLayerManager::SortByPriority()
{
    // Insertion sort — stable, O(N²) acceptable for kMaxLayers=64
    for (unsigned int i = 1; i < mLayers.Size(); ++i)
    {
        LayerEntry key = mLayers[i];
        int j = static_cast<int>(i) - 1;
        while (j >= 0 && mLayers[j].priority > key.priority)
        {
            mLayers[j + 1] = mLayers[j];
            --j;
        }
        mLayers[j + 1] = key;
    }
    mSortDirty = false;
}
```

### DiaAPI command registration

```cpp
void DebugLayerManager::RegisterDiaAPICommands()
{
    // debug.layer.enable <layer-name>
    Dia::API::RegisterCommand({
        Dia::Core::StringCRC("debug.layer.enable"),
        "Enable a named debug layer",
        Dia::Core::StringCRC("debug"),
        "DiaVisualDebugger", "1.0.0",
        "debug.layer.enable physics.shapes",
        [this](const Dia::API::CommandArgs& args) -> int {
            if (args.positionalArgs.Size() < 1) return 2; // bad args
            EnableLayer(Dia::Core::StringCRC(args.positionalArgs[0]));
            return 0;
        }
    });

    // debug.layer.disable <layer-name>
    Dia::API::RegisterCommand({
        Dia::Core::StringCRC("debug.layer.disable"),
        "Disable a named debug layer",
        Dia::Core::StringCRC("debug"),
        "DiaVisualDebugger", "1.0.0",
        "debug.layer.disable physics.shapes",
        [this](const Dia::API::CommandArgs& args) -> int {
            if (args.positionalArgs.Size() < 1) return 2;
            DisableLayer(Dia::Core::StringCRC(args.positionalArgs[0]));
            return 0;
        }
    });

    // debug.layer.list — returns nothing, used by editor to trigger BroadcastLayerState
    Dia::API::RegisterCommand({
        Dia::Core::StringCRC("debug.layer.list"),
        "List all registered debug layers and their enabled state",
        Dia::Core::StringCRC("debug"),
        "DiaVisualDebugger", "1.0.0",
        "debug.layer.list",
        [this](const Dia::API::CommandArgs&) -> int { return 0; }
    });

    // debug.scale <float>
    Dia::API::RegisterCommand({
        Dia::Core::StringCRC("debug.scale"),
        "Set global debug draw scale factor",
        Dia::Core::StringCRC("debug"),
        "DiaVisualDebugger", "1.0.0",
        "debug.scale 2.0",
        [this](const Dia::API::CommandArgs& args) -> int {
            if (args.positionalArgs.Size() < 1) return 2;
            SetDebugScale(static_cast<float>(std::atof(args.positionalArgs[0])));
            return 0;
        }
    });

    // debug.pick — no-op stub, picking seam for scene editor
    Dia::API::RegisterCommand({
        Dia::Core::StringCRC("debug.pick"),
        "Pick entity at screen coordinates (scene editor seam — not yet implemented)",
        Dia::Core::StringCRC("debug"),
        "DiaVisualDebugger", "1.0.0",
        "debug.pick 320 240",
        [](const Dia::API::CommandArgs&) -> int { return 0; }
    });
}
```

### BroadcastLayerState

```cpp
void DebugLayerManager::BroadcastLayerState(
    Dia::DebugServer::DebugServerModule* debugServer)
{
    if (debugServer == nullptr) return;

    Json::Value payload;
    Json::Value layers(Json::arrayValue);

    for (unsigned int i = 0; i < mLayers.Size(); ++i)
    {
        Json::Value layer;
        layer["name"]    = mLayers[i].debugger->GetLayerName().AsChar();
        layer["enabled"] = mLayers[i].debugger->IsEnabled();
        layer["priority"] = mLayers[i].priority;
        layers.append(layer);
    }

    payload["layers"]      = layers;
    payload["debug_scale"] = mDebugScale;

    debugServer->NotifySubscribers(
        Dia::Core::StringCRC("debug.layer.state"), payload);
}
```

### DebugColourPalette

```cpp
// Dia/DiaVisualDebugger/DebugColourPalette.h
namespace Dia::Debug {

struct DebugColourPalette
{
    static const Dia::Graphics::RGBA kActive;     // white     (255,255,255,255) — dynamic/active
    static const Dia::Graphics::RGBA kInactive;   // grey      (128,128,128,255) — static/sleeping/inactive
    static const Dia::Graphics::RGBA kHealthy;    // green     (0,220,0,255)     — converged/solved/ok
    static const Dia::Graphics::RGBA kWarning;    // yellow    (255,220,0,255)   — best-effort/warning
    static const Dia::Graphics::RGBA kError;      // red       (220,0,0,255)     — failed/torn/error
    static const Dia::Graphics::RGBA kGoal;       // cyan      (0,220,220,255)   — target/goal position
    static const Dia::Graphics::RGBA kPinned;     // magenta   (220,0,220,255)   — pinned/constrained
    static const Dia::Graphics::RGBA kCapped;     // orange    (255,140,0,255)   — capped/limit-hit
    static const Dia::Graphics::RGBA kDeepSleep;  // dark blue (0,0,80,255)      — deep sleep
};

} // namespace Dia::Debug
```

### DebugLayerNames

```cpp
// Dia/DiaVisualDebugger/DebugLayerNames.h
namespace Dia::Debug::LayerNames {

// Rig
static const Dia::Core::StringCRC kRigBones     = "rig.bones";
static const Dia::Core::StringCRC kRigJoints    = "rig.joints";
static const Dia::Core::StringCRC kRigArrows    = "rig.arrows";
static const Dia::Core::StringCRC kRigRestPose  = "rig.rest_pose";
static const Dia::Core::StringCRC kRigLabels    = "rig.labels";

// Physics
static const Dia::Core::StringCRC kPhysicsShapes      = "physics.shapes";
static const Dia::Core::StringCRC kPhysicsAABB        = "physics.aabb";
static const Dia::Core::StringCRC kPhysicsVelocity    = "physics.velocity";
static const Dia::Core::StringCRC kPhysicsContacts    = "physics.contacts";
static const Dia::Core::StringCRC kPhysicsConstraints = "physics.constraints";

// Soft body
static const Dia::Core::StringCRC kSoftParticles   = "soft.particles";
static const Dia::Core::StringCRC kSoftConstraints = "soft.constraints";
static const Dia::Core::StringCRC kSoftAnchors     = "soft.anchors";
static const Dia::Core::StringCRC kSoftVelocity    = "soft.velocity";

// IK
static const Dia::Core::StringCRC kIKChains      = "ik.chains";
static const Dia::Core::StringCRC kIKTargets     = "ik.targets";
static const Dia::Core::StringCRC kIKPoleVectors = "ik.pole_vectors";
static const Dia::Core::StringCRC kIKLimits      = "ik.limits";
static const Dia::Core::StringCRC kIKConvergence = "ik.convergence";

// Geometry
static const Dia::Core::StringCRC kGeoShapes      = "geometry.shapes";
static const Dia::Core::StringCRC kGeoSpatialGrid = "geometry.spatial_grid";
static const Dia::Core::StringCRC kGeoQuadtree    = "geometry.quadtree";
static const Dia::Core::StringCRC kGeoBVH         = "geometry.bvh";
static const Dia::Core::StringCRC kGeoContacts    = "geometry.contacts";

// Animation
static const Dia::Core::StringCRC kAnimSpring       = "anim.spring";
static const Dia::Core::StringCRC kAnimClipCursor   = "anim.clip_cursor";
static const Dia::Core::StringCRC kAnimBlendWeights = "anim.blend_weights";

} // namespace Dia::Debug::LayerNames
```

---

## Files Changed / Created

| File | Change |
|------|--------|
| `Dia/DiaVisualDebugger/IVisualDebugger.h` | New — interface |
| `Dia/DiaVisualDebugger/DebugLayerManager.h` | New — class declaration |
| `Dia/DiaVisualDebugger/DebugLayerManager.cpp` | New — implementation |
| `Dia/DiaVisualDebugger/DebugColourPalette.h` | New — RGBA constants declaration |
| `Dia/DiaVisualDebugger/DebugColourPalette.cpp` | New — RGBA constant definitions |
| `Dia/DiaVisualDebugger/DebugLayerNames.h` | New — StringCRC constants |
| `Dia/DiaVisualDebugger/DiaVisualDebugger.vcxproj` | New — static library project |
| `Dia/DiaVisualDebugger/DiaVisualDebugger.vcxproj.filters` | New — IDE filters |
| `Dia/DiaVisualDebugger/dia.debug.architecture.module.md` | New — YAML module doc |
| `Cluiche/Cluiche.sln` | Add DiaVisualDebugger project reference |

**Prerequisites:** `debug-budget` and `debug-text-primitive` must be implemented (both provide types used by draw classes that register with this manager).

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Create `Dia/DiaVisualDebugger/` directory and write `IVisualDebugger.h` | Header only |
| 2 | Write `DebugColourPalette.h` and `DebugColourPalette.cpp` | 9 RGBA constants |
| 3 | Write `DebugLayerNames.h` | `StringCRC` constants, header only |
| 4 | Write `DebugLayerManager.h` and `DebugLayerManager.cpp` | Full implementation |
| 5 | Create `DiaVisualDebugger.vcxproj` static library — dependencies: DiaCore, DiaGraphics, DiaAPI, DiaDebugServer (optional/conditional) | Follow `DiaRig2DVisualDebugger.vcxproj` as pattern |
| 6 | Create `DiaVisualDebugger.vcxproj.filters` | IDE organisation |
| 7 | Create `dia.debug.architecture.module.md` | YAML frontmatter |
| 8 | Add project to `Cluiche.sln` | Register in solution |
| 9 | Build solution — verify zero warnings | `msbuild Cluiche/Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |
| 10 | Write tests — see test plan | `TestDebugLayerManager.cpp` in GoogleTests |
| 11 | Run tests | `Cluiche/bin/Debug/x64/UnitTests.exe` |

---

## Test Plan

**File:** `Cluiche/Tests/GoogleTests/DiaVisualDebugger/TestDebugLayerManager.cpp`

A stub `IVisualDebugger` implementation is used throughout (`TestLayer` — records `Draw()` call count and accepts a configurable layer name).

| Suite | Test | What it verifies |
|-------|------|-----------------|
| Registration | `Register_SingleLayer` | Register one layer → `GetLayerCount() == 1` |
| Registration | `Register_MultipleLayersUniqueName` | Register 3 unique layers → `GetLayerCount() == 3` |
| Registration | `Register_DuplicateNameAsserts` | Register same name twice → `DIA_ASSERT` fires |
| Registration | `Register_NullptrAsserts` | `Register(nullptr)` → `DIA_ASSERT` fires |
| Registration | `HasLayer_ReturnsTrueAfterRegister` | Register → `HasLayer(name) == true` |
| Registration | `HasLayer_ReturnsFalseBeforeRegister` | Fresh manager → `HasLayer(name) == false` |
| Unregister | `Unregister_RemovesLayer` | Register then unregister → `GetLayerCount() == 0` |
| Unregister | `Unregister_UnknownNameNoOp` | Unregister non-existent name → no crash, count unchanged |
| Toggle | `EnableLayer_DefaultEnabled` | Register layer → `IsLayerEnabled(name) == true` |
| Toggle | `DisableLayer_LayerDisabled` | Disable → `IsLayerEnabled(name) == false` |
| Toggle | `EnableLayer_ReEnables` | Disable then enable → `IsLayerEnabled(name) == true` |
| Toggle | `Toggle_UnknownNameNoOp` | `EnableLayer` on unregistered name → no crash |
| Draw | `Draw_CallsEnabledLayer` | Register enabled layer → `Draw()` → layer `DrawCallCount == 1` |
| Draw | `Draw_SkipsDisabledLayer` | Register disabled layer → `Draw()` → layer `DrawCallCount == 0` |
| Draw | `Draw_MultipleLayersAllEnabled` | 3 enabled layers → `Draw()` → each called once |
| Draw | `Draw_PriorityOrder` | Register layers with priorities 20, 0, 10 → `Draw()` calls in order 0, 10, 20 |
| Draw | `Draw_PriorityTieBreakByRegistration` | Two layers same priority → called in registration order |
| Draw | `Draw_EmptyManagerNoOp` | No registered layers → `Draw()` → no crash |
| Scale | `DebugScale_DefaultIsOne` | Fresh manager → `GetDebugScale() == 1.0f` |
| Scale | `SetDebugScale_Persists` | `SetDebugScale(2.5f)` → `GetDebugScale() == 2.5f` |
| EntityId | `SelectedEntityId_DefaultZero` | Fresh manager → `GetSelectedEntityId() == 0` |
| EntityId | `SetSelectedEntityId_Persists` | `SetSelectedEntityId(42)` → `GetSelectedEntityId() == 42` |
| Palette | `DebugColourPalette_ActiveIsWhite` | `kActive == RGBA(255,255,255,255)` |
| Palette | `DebugColourPalette_AllNineDistinct` | All 9 constants have distinct RGBA values |

---

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|-----------|
| PD-001 | StringCRC for identifiers | Compliant — all layer names are `StringCRC`; `DebugLayerNames.h` provides constants; DiaAPI command names are `StringCRC` |
| PD-002 | ProcessingUnit/Phase/Module architecture | Compliant — `DebugLayerManager` is a plain C++ object; caller owns its lifecycle within a `Module::DoUpdate()` call; `RegisterDiaAPICommands()` called during `DoStart()` |
| PD-003 | Component-based entities | Compliant — `SetSelectedEntityId()` stub aligned to future IComponent IDs |
| PD-004 | No STL in public APIs | Compliant — `DynamicArrayC<LayerEntry, kMaxLayers>` for the registry; `StringCRC` for names; no STL containers in any public method signature. Note: `RegisterDiaAPICommands()` uses `std::atof` internally (not in public API) |
| PD-005 | x64 only | Compliant — `DiaVisualDebugger.vcxproj` targets x64 exclusively |
| PD-006 | Visual Studio project files are source of truth | Compliant — new `DiaVisualDebugger.vcxproj` + `.vcxproj.filters` created and maintained manually |
| PD-007 | C++20 required | Compliant — all code under `/std:c++20` |
| PD-008 | `Directory.Build.props` owns build paths | Compliant — no per-project overrides |
| PD-009 | Generated output under `Cluiche/out/` | Compliant — no generated output |
| AD-001 | Module YAML frontmatter | Compliant — `dia.debug.architecture.module.md` created |
| AD-002 | No STL in public APIs | Compliant — reinforces PD-004 |
| AD-003 | `Dia::<Module>::` namespace | Compliant — all in `Dia::Debug::` namespace |
| SD-DBG-001 | Stack of focused draw classes, not options flags | Compliant — `IVisualDebugger` is the one interface; each draw concern is a separate implementation |
| SD-DBG-002 | `#ifdef DIA_DEBUG` guards | Compliant — entire `DiaVisualDebugger.vcxproj` excluded in Release; `RegisterDiaAPICommands()` and `BroadcastLayerState()` are never called outside debug builds |
| SD-DBG-003 | Priority sort at registration | Compliant — insertion sort on `LayerEntry.priority`; canonical tiers documented in `DebugLayerNames.h` comments |
| SD-DBG-004 | First-come-first-served budget in priority order | Compliant — `Draw()` calls layers in priority order; `DebugFrameData` capacity and drop tracking is `debug-budget`'s concern |
| SD-DBG-005 | Global `debugScale` on manager | Compliant — `SetDebugScale()` / `GetDebugScale()` implemented |
| SD-DBG-006 | `DIA_ASSERT` on layer name collision | Compliant — assertion fires in `Register()` if name already exists |
| SD-DBG-008 | `debug.pick` no-op command + seam stubs | Compliant — `debug.pick` registered as no-op; `SetSelectedEntityId()` / `GetSelectedEntityId()` stubs provided |
| SD-DBG-010 | `DebugColourPalette` colours are binding | Compliant — `DebugColourPalette` defined here; all draw classes in subsequent features must use it |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | DiaDebugServer dependency | `BroadcastLayerState()` takes a raw `DebugServerModule*`. This creates an optional dependency on DiaDebugServer in DiaVisualDebugger.vcxproj. Should DiaDebugServer be a required or optional link? | Optional — `DiaVisualDebugger.vcxproj` does not link `DiaDebugServer.lib`. The `DebugServerModule*` parameter is forward-declared; caller passes nullptr if not present. The header includes a forward declaration only, not the full header. |
| 2 | Lambda captures in DiaAPI commands | The `debug.layer.enable/disable/scale` command lambdas capture `this` (the `DebugLayerManager` instance). If the manager is destroyed before the command is unregistered, the lambda is a dangling capture. Is this a problem? | `RegisterDiaAPICommands()` is called once at startup; the manager lives for the duration of the application. No unregistration mechanism exists in DiaAPI today. Document as a lifetime requirement: manager must outlive the application. |
| 3 | `std::atof` in command handler | `debug.scale` uses `std::atof` to parse the float argument. Is this acceptable inside DiaAPI? | Acceptable — `std::atof` is used internally in the command callback, not in a public API signature. It is not a container or allocator. No DiaCore alternative exists for string-to-float conversion. |
| 4 | `DebugColourPalette` as struct vs namespace | The spec defines `DebugColourPalette` as a struct with static members. Should it be a namespace instead? | Struct with static const members — consistent with existing engine patterns (e.g. `RGBA::White()`). Forward-declarable and groupable. A namespace of free `const RGBA` values is equally valid but slightly less discoverable. Struct chosen for consistency. |
| 5 | `BroadcastLayerState` call site | Who calls `BroadcastLayerState()`? It should be called each frame after `Draw()`. Is it game code's responsibility or should `Draw()` call it internally? | Game code calls it — `Draw()` takes only `FrameData&`; `BroadcastLayerState()` takes `DebugServerModule*`. Mixing them would create a DiaDebugServer dependency in `Draw()`. Caller calls both: `manager.Draw(frameData); manager.BroadcastLayerState(debugServer);` |
| 6 | `DebugLayerNames` as `static const StringCRC` | `StringCRC` constants are defined `static const` in a header. This creates a separate instance per translation unit. Should they be `inline constexpr` (C++17+) or `extern const` instead? | `inline constexpr` — C++20 is required (PD-007), so `inline constexpr Dia::Core::StringCRC` is valid and avoids the one-definition-rule issue with `static const` in headers. Update all constants to `inline constexpr`. |

---

## Status

`Approved`

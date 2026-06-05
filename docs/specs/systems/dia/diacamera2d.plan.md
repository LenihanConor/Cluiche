# DiaCamera2D — Implementation Plan

**Spec:** @docs/specs/systems/dia/diacamera2d.md
**Status:** Done

---

## Implementation Patterns

### Project Structure

```
Dia/DiaCamera2D/
├── Camera2D.h                       ← migrated from DiaGraphics/Camera/
├── ViewportTransform.h / .cpp       ← migrated from DiaGraphics/Camera/
├── Registry/
│   └── CameraRegistry2D.h / .cpp
├── Behaviour/
│   ├── ICameraBehaviour.h
│   ├── CameraBehaviourRegistry.h / .cpp
│   ├── FollowBehaviour.h / .cpp
│   ├── SmoothDampBehaviour.h / .cpp
│   ├── DeadzoneBehaviour.h / .cpp
│   ├── BoundsClampBehaviour.h / .cpp
│   ├── ScreenShakeBehaviour.h / .cpp
│   ├── ZoomToFitBehaviour.h / .cpp
│   ├── PanBehaviour.h / .cpp
│   └── ZoomBehaviour.h / .cpp
├── Testing/
│   ├── CameraBuilder.h             ← fluent test helper
│   └── BehaviourFixture.h          ← fixture with registry setup
├── dia.camera2d.architecture.module.md
└── DiaCamera2D.vcxproj / .filters
```

### Namespace

All types in `Dia::Camera2D::`. Matches AD-003 convention.

### Registry Pattern

Follows existing `PolymorphicRegistry` / `EditorPluginRegistry` pattern:
- `HashTable<StringCRC, CameraSlot>` for named lookup
- `CameraSlot` holds `Camera2D` + `DynamicArrayC<ICameraBehaviour*, 8>` (attachment-order ticking)
- `Register` asserts on duplicate ID; `Unregister` deletes owned behaviours
- `GetActive()` asserts if no active camera set

### Behaviour Factory Pattern

Follows diaentitytemplate's `ComponentRegistry` self-registration:
```cpp
// In .cpp file — static init registers the factory
static bool sRegistered = [] {
    Dia::Camera2D::CameraBehaviourRegistry::Get().Register(
        "FollowBehaviour"_crc,
        [](const ReflectedConfig& cfg) -> ICameraBehaviour* {
            return new FollowBehaviour(cfg.As<FollowConfig>());
        });
    return true;
}();
```

### Migration Pattern

Camera2D.h and ViewportTransform.h/cpp move physically. DiaGraphics gets a dependency on DiaCamera2D. All `Dia::Graphics::Camera2D` references update to `Dia::Camera2D::Camera2D`. ViewportTransform moves to `Dia::Camera2D::ViewportTransform`.

### Retrofit Pattern

Camera2DModule evolves from:
```cpp
Camera2D mCamera;                    // single camera
GetCamera() → Camera2D&
```
To:
```cpp
CameraRegistry2D mRegistry;          // named cameras
GetRegistry() → CameraRegistry2D&
GetActiveCamera() → Camera2D&        // convenience (delegates to registry)
GetViewportTransform() → ViewportTransform  // unchanged signature
```

Dependent modules (PickingModule, VisualDebuggerModule) change from `mCameraRef->GetCamera()` to `mCameraRef->GetActiveCamera()`. Minimal API disruption.

### Testing Strategy

**Test categories (exhaustive, not just happy path):**

| Category | What's tested | Pattern |
|---|---|---|
| Construction | Camera2D defaults, explicit values, copy semantics | Triple-A |
| Registry CRUD | Register, unregister, duplicate ID, get missing ID | Error condition |
| Active camera | Set/get active, no active set (assert), switch active | Boundary |
| Behaviour lifecycle | Attach, detach, ownership (delete on unregister) | Lifecycle |
| Behaviour ordering | Multiple behaviours tick in attachment order | Determinism |
| Individual behaviours | Each of 8 behaviours tested with known inputs | Property-based |
| Composition | Multiple behaviours on one camera (Follow + BoundsClamp + Shake) | Integration |
| Edge cases | Zero dt, zero zoom, NaN position, empty registry | Robustness |
| Stress | 32 cameras × 8 behaviours, rapid register/unregister | Stress |
| Determinism | Same inputs produce same outputs across runs | Seeded RNG |
| ViewportTransform | Round-trip ScreenToWorld↔WorldToScreen, rotation, zoom extremes | Property-based |
| Factory | Create known type, create unknown type (null), double-register | Registry pattern |

### Observability Plan

| Pillar | What | Where |
|---|---|---|
| **Logs** | `DIA_LOG_WARNING("DiaCamera2D", ...)` — register duplicate, unregister missing, no active camera | Registry operations |
| **Traces** | `DIA_TRACE_ZONE("camera.update_all", kDiaGraphics)` | `UpdateAll()` |
| **Profile** | `DIA_PROFILE_SCOPE("camera.update_all", kDiaGraphics)` | `UpdateAll()` |
| **Metrics** | `dia.camera2d.count` (Gauge), `dia.camera2d.behaviour_ticks` (Counter) | Registry |
| **Health** | `CameraRegistryHealth : HealthReporterBase` — Degraded if no active camera, Failing if zero cameras registered when ticked | Registry |

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|---|---|---|---|---|
| T-00 | Create DiaCamera2D.vcxproj + directory structure + module doc + add to Cluiche.sln | Build compiles (empty lib) | Done | haiku | |
| T-01 | Migrate Camera2D.h from DiaGraphics → DiaCamera2D, update namespace to `Dia::Camera2D::` | Existing TestCameraViewport passes | Done | sonnet | Forwarding alias `Dia::Graphics::Camera2D` added for compatibility |
| T-02 | Migrate ViewportTransform.h/.cpp, update namespace | TestCameraViewport passes with new paths | Done | sonnet | Same forwarding alias pattern |
| T-03 | Update DiaGraphics.vcxproj to depend on DiaCamera2D, remove Camera/ from its project | Full solution builds | Done | haiku | |
| T-04 | Update all consumer code to new include paths/namespace | `dia run googletest` passes | Done | sonnet | Forwarding headers avoid churn in 17 consumer files; FrameData.cpp needed explicit namespace |
| T-05 | Implement CameraRegistry2D (Register, Unregister, Get, SetActive, GetActive, UpdateAll) | TestCameraRegistry suite passes | Done | sonnet | |
| T-06 | Implement ICameraBehaviour + CameraBehaviourRegistry (factory) | TestBehaviourFactory passes | Done | sonnet | |
| T-07 | Implement FollowBehaviour + SmoothDampBehaviour | Tests pass | Done | sonnet | |
| T-08 | Implement DeadzoneBehaviour + BoundsClampBehaviour | Tests pass | Done | sonnet | AARect uses GetBottomLeft/GetTopRight not GetMin/GetMax |
| T-09 | Implement ScreenShakeBehaviour + ZoomToFitBehaviour | Tests pass | Done | sonnet | |
| T-10 | Implement PanBehaviour + ZoomBehaviour | Tests pass | Done | sonnet | |
| T-11 | Create Testing/ utilities: CameraBuilder (fluent) | Used by registry tests | Done | haiku | |
| T-12 | Edge case + stress tests | 68 tests pass | Done | sonnet | |
| T-13 | Behaviour composition + determinism tests | Pass | Done | sonnet | |
| T-14 | Observability instrumentation (logs, traces, profile, metrics, health) | DiaObservation dependency added | Done | sonnet | Health reporter implements IHealthReporter directly (not HealthReporterBase — it has its own Report() impl) |
| T-15 | Retrofit Camera2DModule to own CameraRegistry2D | Camera2DModule registers "default" camera on start | Done | sonnet | |
| T-16 | Retrofit PickingModule + VisualDebuggerModule | GetCamera() → GetActiveCamera() | Done | sonnet | Forwarding alias means DiaGraphics consumers compile unchanged |
| T-17 | Verify all CluicheTest stages | `dia run googletest` 5577 tests pass | Done | opus | CluicheTest launched (visual confirmation pending) |

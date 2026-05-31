# Implementation Plan: 2D Picking Service

**Spec:** @docs/specs/features/dia/diapicking/geometry2d-picking.md
**Status:** In Progress

---

## Implementation Patterns

### DiaPicking (Task 1)

**Project:** `Dia/DiaPicking/DiaPicking.vcxproj` — static library.

**Namespace:** `Dia::Picking`

**Files:**
- `PickResult.h` — template class, header-only (sorted hit container)
- `PickEvent.h` — template struct, header-only (trigger + hits)
- `PickTrigger.h` — enum
- `PickLayer.h` — enum + `PickLayerMask` typedef
- `PickAddress.h/.cpp` — helper to build DiaMailbox::Address
- `PickRouter.h/.cpp` — IMailboxRouter implementation (resolves by trigger type)

**Pattern for PickResult:**
```cpp
template<typename THit, unsigned int MaxHits = 16>
class PickResult {
    Dia::Core::Containers::DynamicArrayC<THit, MaxHits> mHits;
public:
    void Add(const THit& hit);  // insert-sorted by priority desc
    bool HasHit() const { return mHits.Size() > 0; }
    const THit& Best() const { return mHits[0]; }
    unsigned int Count() const { return mHits.Size(); }
    const THit& operator[](unsigned int i) const { return mHits[i]; }
};
```

**Pattern for PickEvent:**
```cpp
template<typename THit, unsigned int MaxHits = 16>
struct PickEvent {
    PickTrigger trigger;
    PickResult<THit, MaxHits> hits;
};
```

**Pattern for PickRouter:**
```cpp
class PickRouter : public Dia::Mailbox::IMailboxRouter {
    // For each PickTrigger, tracks which SubscriberIds are interested.
    // Resolve() matches address payload (which encodes trigger) → subscriber set.
    static constexpr unsigned int kMaxTriggers = 4;
    Dia::Core::Containers::DynamicArrayC<Dia::Mailbox::SubscriberId, 16> mSubscribers[kMaxTriggers];
};
```

**Dependencies:** DiaCore, DiaMailbox.

---

### DiaGeometry2DPicking (Tasks 2–5)

**Project:** `Dia/DiaGeometry2DPicking/DiaGeometry2DPicking.vcxproj` — static library.

**Namespace:** `Dia::Geometry2DPicking`

**Files:**
- `IPickable2D.h` — abstract interface
- `PickHit2D.h` — struct with Kind enum + union
- `PickingService2D.h/.cpp` — register/unregister + Pick/PickArea (stateless query, stateful registry)
- `Adapters/HexGridPickable.h/.inl` — template, wraps `HexGrid<T,Max>`
- `Adapters/SpatialGridPickable.h/.inl` — template, wraps `SpatialGrid<T,Max>`
- `Adapters/ShapePickable.h/.cpp` — wraps individual shapes (non-template, point-in-shape dispatch)

**Adapter pattern (HexGridPickable):**
```cpp
template<typename T, unsigned int MaxObjects>
class HexGridPickable : public IPickable2D {
    const Dia::Geometry2D::HexGrid<T, MaxObjects>& mGrid;
    Dia::Core::StringCRC mId;
    int mPriority;
    Dia::Picking::PickLayer mLayer;
public:
    bool TryPick(const Vector2D& worldPos, PickHit2D& out) const override {
        HexCoord hex = mGrid.WorldToHex(worldPos);
        if (!mGrid.IsValidHex(hex)) return false;
        out.pickableId = mId;
        out.kind = PickHit2D::Kind::kHexCell;
        out.priority = mPriority;
        out.worldPos = worldPos;
        out.hexCell = hex;
        return true;
    }
};
```

**PickingService2D pattern:**
```cpp
PickResult<PickHit2D> Pick(const Vector2D& worldPos, PickLayerMask mask) const {
    PickResult<PickHit2D> result;
    for (each registered pickable) {
        if (!(static_cast<unsigned int>(pickable->GetLayer()) & mask)) continue;
        PickHit2D hit;
        if (pickable->TryPick(worldPos, hit))
            result.Add(hit);
    }
    return result;
}
```

**ShapePickable:** Non-template. Stores a `StringCRC` shape type + pointer to const shape. Uses existing point-in-shape tests. One class with a `Kind` switch internally.

**Dependencies:** DiaPicking, DiaGeometry2D, DiaMaths, DiaCore.

---

### CameraModule Extraction (Task 6)

**Problem:** `VisualDebuggerModule` owns Camera2D + windowSize and is `#ifdef DIA_DEBUG`. Picking is a game mechanic that must work in Release.

**Solution:** New `CameraModule` in CluicheGameBaseline (SimPU, all stages, non-debug).

**File:** `Cluiche/CluicheGameBaseline/Modules/CameraModule.h/.cpp`

**Pattern:**
```cpp
class CameraModule : public Module {
    Dia::Graphics::Camera2D mCamera;
    Dia::Maths::Vector2D    mWindowSize{1400.0f, 1000.0f};
public:
    const Dia::Graphics::Camera2D& GetCamera() const { return mCamera; }
    const Dia::Maths::Vector2D& GetWindowSize() const { return mWindowSize; }
    Dia::Graphics::ViewportTransform GetViewportTransform() const;
    void SetCamera(const Dia::Graphics::Camera2D& cam);
    void SetWindowSize(const Dia::Maths::Vector2D& size);
};
```

**VisualDebuggerModule refactor:** Remove camera/windowSize members. Add `ModuleRef<CameraModule>`. Read camera from it in `DoUpdate()`.

**Manifest:** `CameraModule` added to SimPU with `stages: ["all"]`, no dependencies. `VisualDebuggerModule` adds `"CameraModule"` to its dependencies.

---

### InputStreamModule Extension (Task 7)

**File:** `Cluiche/CluicheGameBaseline/Modules/InputStreamModule.h/.cpp`

**Pattern:** Mirror the existing key tracking:
```cpp
static constexpr unsigned int kMaxMouseButtons = 8;
bool mCurrentMouse[kMaxMouseButtons];
bool mPreviousMouse[kMaxMouseButtons];
```

Process `kMouseButtonPressed`/`kMouseButtonReleased` in `DoUpdate()`. Expose `WasMouseButtonPressed(int)`, `IsMouseButtonDown(int)`, `WasMouseButtonReleased(int)`.

---

### PickingModule (Task 15)

**File:** `Cluiche/CluicheGameBaseline/Modules/PickingModule.h/.cpp`

**Pattern:**
```cpp
class PickingModule : public Module {
    Dia::Geometry2DPicking::PickingService2D mService;
    Dia::Mailbox::Mailbox mMailbox;
    Dia::Picking::PickRouter mPickRouter;

    ModuleRef<InputStreamModule> mInputRef{this};
    ModuleRef<CameraModule> mCameraRef{this};

    StartResult DoStart() override {
        mMailbox.RegisterType<Dia::Picking::PickEvent<PickHit2D>, 4>();
        mMailbox.RegisterRouter(&mPickRouter);
        return StartResult::kReady;
    }

    void DoUpdate(float dt) override {
        auto vt = mCameraRef->GetViewportTransform();
        auto worldPos = vt.ScreenToWorld(Vector2D(mInputRef->GetMouseX(), mInputRef->GetMouseY()));

        // Hover every frame
        PickEvent<PickHit2D> hoverEvt{PickTrigger::kHover, mService.Pick(worldPos)};
        mMailbox.Send(PickAddress::ForTrigger(PickTrigger::kHover), hoverEvt);

        // Click
        if (mInputRef->WasMouseButtonPressed(0)) {
            PickEvent<PickHit2D> evt{PickTrigger::kClick, mService.Pick(worldPos)};
            mMailbox.Send(PickAddress::ForTrigger(PickTrigger::kClick), evt);
        }

        // Right-click
        if (mInputRef->WasMouseButtonPressed(1)) {
            PickEvent<PickHit2D> evt{PickTrigger::kRightClick, mService.Pick(worldPos)};
            mMailbox.Send(PickAddress::ForTrigger(PickTrigger::kRightClick), evt);
        }
    }
};
```

**Public access:**
- `GetService()` — for register/unregister pickables
- `GetMailbox()` — for consumers to subscribe/drain
- `GetRouter()` — for trigger-specific subscription

---

### Drawer Refactor (Task 16)

**HexGridDrawer changes:**
- Remove `mHasSelection`, `mSelectedHex` members
- Remove click detection block from `DrawImGui()`
- Add `void SetSelection(const Dia::Geometry2D::HexCoord* selected)` — stores pointer, nullable
- `Draw()` highlights `mSelected` cell if non-null
- `DrawImGui()` shows inspector if `mSelected != nullptr` (read-only)
- Remove ImGui mouse debug text (picking owns that flow now)
- Keep coord labels checkbox

Same pattern for `SpatialGridDrawer`.

---

### Consumer Integration (Task 17)

**Geometry2DTestStageModule changes:**
- `OnStart()`: register `mHexGridPickable` + `mSpatialGridPickable` with `mPickingRef->GetService()`. Subscribe to `kClick` via `mPickingRef->GetRouter().SubscribeToTrigger(...)`.
- `OnUpdate()`: drain `PickEvent<PickHit2D>` from `mPickingRef->GetMailbox()`. On click hit, store `mSelectedHex`, call `mHexGridDrawer->SetSelection(&mSelectedHex)`.
- `OnStop()`: unregister pickables, unsubscribe.

---

### Manifest Entries (Task 15)

```json
{
    "instance_id": "CameraModule",
    "type_id": "CameraModule",
    "stages": ["all"],
    "dependencies": [],
    "channels": []
},
{
    "instance_id": "PickingModule",
    "type_id": "PickingModule",
    "stages": ["RigidBody2DTestStage", "Geometry2DTestStage"],
    "dependencies": ["InputStreamModule", "CameraModule"],
    "channels": []
}
```

Update `VisualDebuggerModule` to add `"CameraModule"` to its dependencies.

---

## Task Table

### Phase 1 — Foundation

| # | Task | Test | Status | Model | Notes |
|---|---|---|---|---|---|
| 1 | Create `DiaPicking` vcxproj — `PickResult<T>`, `PickEvent<T>`, `PickTrigger`, `PickLayer`, `PickAddress`, `PickRouter` | Build clean | Done | sonnet | DiaPicking.lib ✓ |
| 2 | Create `DiaGeometry2DPicking` vcxproj — `IPickable2D`, `PickHit2D`, `PickingService2D` | Build clean | Done | sonnet | DiaGeometry2DPicking.lib ✓ |
| 3 | Implement `HexGridPickable` adapter | Build clean | Done | sonnet | Template .h/.inl, wraps WorldToHex + IsValidHex |
| 4 | Implement `SpatialGridPickable` adapter | Build clean | Done | sonnet | Template .h/.inl, wraps cell math |
| 5 | Implement `ShapePickable` adapter | Build clean | Done | sonnet | Non-template, point-in-shape dispatch |
| 6 | Extract `CameraModule` from `VisualDebuggerModule` | Build clean | Done | sonnet | Non-debug, all stages; VDM reads from CameraModule via ModuleRef |
| 7 | Extend `InputStreamModule` — mouse button state | Build clean | Done | haiku | WasMouseButtonPressed/IsDown/Released |

### Phase 2 — Exhaustive Testing

| # | Task | Test | Status | Model | Notes |
|---|---|---|---|---|---|
| 8 | Unit tests: `PickResult` + `PickLayer` + `PickAddress` + `PickRouter` + `PickEvent` | AC1, AC9 pass | Done | sonnet | TestPickingCore.cpp — 64/64 pass |
| 9 | Unit tests: `HexGridPickable` — hit, miss outside, valid-cell round-trip, PickArea | AC3 pass | Done | sonnet | TestHexGridPickable.cpp |
| 10 | Unit tests: `SpatialGridPickable` — hit cell, miss outside, PickArea 2×2 | AC4 pass | Done | sonnet | TestSpatialGridPickable.cpp |
| 11 | Unit tests: `ShapePickable` — Circle/AARect/ConvexPolygon hit/miss + PickArea | AC5 pass | Done | sonnet | TestShapePickable.cpp |
| 12 | Unit tests: `PickingService2D::Pick` — priority ordering, layer mask, no hits | AC6, AC8 pass | Done | sonnet | TestPickingService2D.cpp |
| 13 | Unit tests: `PickingService2D::PickArea` — overlapping/non-overlapping | AC7 pass | Done | sonnet | TestPickingService2D.cpp |
| 14 | Unit tests: mouse button tracking logic (mirrors InputStreamModule) | AC10 pass | Done | sonnet | TestInputStreamMouseButtons.cpp |
| 15 | Unit tests: `ViewportTransform` screen↔world + camera offset | AC11 pass | Done | sonnet | TestCameraViewport.cpp |

### Phase 3 — Wiring

| # | Task | Test | Status | Model | Notes |
|---|---|---|---|---|---|
| 17 | Create `PickingModule` + manifest entries | Build clean | Done | sonnet | CluicheTest passes ✓ |
| 18 | Refactor `HexGridDrawer` + `SpatialGridDrawer` — `SetSelection()`, remove ImGui input path | Build clean | Done | sonnet | Drawers now read-only; selection owned by stage module |

### Phase 4 — Hex Grid Integration (Geometry2D test stage)

| # | Task | Test | Status | Model | Notes |
|---|---|---|---|---|---|
| 19 | Wire `Geometry2DTestStageModule` — register pickables, subscribe to kClick, drain + store selection, pass to drawers | AC13 | Done | sonnet | PickingModule::GetStatic() cross-PU pattern; HexGrid + SpatialGrid pickables registered |
| 20 | `dia run cluichetest` — PickingModule starts, CameraModule starts, logs confirm | AC12, AC14, AC15 | Done | sonnet | `dia cluichetest: PASSED`; PickingModule started log confirmed |

### Phase 5 — Observability

| # | Task | Test | Status | Model | Notes |
|---|---|---|---|---|---|
| 21 | Add `DIA_LOG_INFO("picking", ...)` in `PickingService2D` — register/unregister/hit count/best id | Log confirmed | Done | haiku | Also logs register/unregister |
| 22 | Add `DIA_LOG_INFO("picking", ...)` in `PickingModule` — hit/miss/right-click/start/stop | Log confirmed | Done | haiku | Miss explicit; confirmed in `dia run cluichetest` output |
| 23 | Add `DIA_LOG_INFO("picking", ...)` in `Geometry2DTestStageModule` — selection change only | Log on change | Done | haiku | Change-detection guard — no per-frame spam |
| 24 | Observation scan — future: trace zone on `PickingModule::DoUpdate`, hit/miss metrics | Notes | Done | sonnet | Non-blocking: `DIA_TRACE_ZONE`, `picking.hit_count`/`picking.miss_count` counters |

---

## Dependency Graph

```
Phase 1:  1 ──► 2 ──► 3 ┐
                    ├──► 4 ├──► Phase 2
                    └──► 5 ┘
          6 (independent — CameraModule)
          7 (independent — InputStreamModule)

Phase 2:  8–16 (parallel within phase)

Phase 3:  Phase 1 + Phase 2 ──► 17, 18 (parallel)

Phase 4:  17 + 18 ──► 19 ──► 20

Phase 5:  20 ──► 21, 22, 23 (parallel) ──► 24
```

**Parallel opportunities:**
- Phase 1: Tasks 1, 6, 7 are all independent; tasks 3, 4, 5 independent
- Phase 2: All test tasks independent
- Phase 3: Tasks 17 + 18 independent
- Phase 5: Tasks 21, 22, 23 independent

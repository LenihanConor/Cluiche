**Spec:** @docs/specs/applications/cluichetest/systems/teststages/gridvisibility-test-stage.md
**Status:** Done

## Implementation Patterns

### Module structure
- Inherits `TestStageModuleBase` — see `PathfindingTestStageModule.h` as reference
- `static const StringCRC kTypeId`, `static constexpr PUAffinity kAllowedPUs = kSim`
- `static constexpr const char* kDescription`, `static constexpr unsigned int kMinDisplayFrames`
- Overrides: `GetStageName()`, `GetBudgetFrames()` (returns 1200), `GetCheckpointNames()`, `OnStart()`, `OnUpdate()`, `OnStop()`
- `DIA_MODULE` + `DIA_DESCRIBE` macros at bottom of .cpp
- `ModuleRef<VisualDebuggerModule> mVisualDebuggerRef{this}` in `#ifdef DIA_DEBUG` block

### Grid constants
- `kGridW=24`, `kGridH=18`, `kCellSize=30.f`
- `kGridOrigin = Vector2D(-360.f, -270.f)` — top-left corner of grid in world space
- Cell centre: `kGridOrigin + Vector2D((col+0.5f)*kCellSize, (row+0.5f)*kCellSize)`
- Wall mask: `static constexpr bool kWalls[kGridH][kGridW]` — centre divider + L-walls per spec

### Visibility system
- `Dia::Pathfinding::SquarePathGrid mGrid` — owned; built once in `OnStart`
- `std::unique_ptr<Dia::GridVisibility::GridVisibilitySystem<Dia::Pathfinding::SquarePathGrid>> mVisSystem`
- Two groups: `kGroupRed = VisibilityGroupId(0)`, `kGroupBlue = VisibilityGroupId(1)`
- Sight radius: **6** vis-cells (ODQ-2 resolution)

### Entity + spatial (pattern from EntitySpatialTestStageModule)
- `Dia::Entity::Domain mDomain` — local
- `std::unique_ptr<Dia::EntitySpatial::EntitySpatialModule> mSpatialModule`
- Register `ComponentPool<SpatialComponent>` in `OnStart`
- `Entity mPatrolEntities[2]`, `Entity mEnemyEntities[5]`
- Per frame: update SpatialComponent positions via `mDomain`, call `mDomain.EndOfFrame()`, `mSpatialModule->Update()`, `mVisSystem->Update(kCellSize, *mSpatialModule, mDomain)`

### Shared state for drawer (pre-agreed interface)
```cpp
// Owned by module, read by drawer
Dia::Maths::Vector2D mEntityWorldPos[7];  // [0]=RedPatrol, [1]=BluePatrol, [2-6]=E0..E4
float mEnemyFlashTimers[5];               // seconds remaining, >0 = pulsing
float mStageTime;
```

### Patrol movement
- `static constexpr CellCoord kRedWaypoints[4] = {{1,1},{9,1},{9,16},{1,16}}`
- `static constexpr CellCoord kBlueWaypoints[4] = {{14,1},{22,1},{22,16},{14,16}}`
- `mPatrolT[2]` (0→1 interpolation param), `mPatrolWpIdx[2]` (0→3)
- Speed: 4 vis-cells/sec = `kPatrolSpeed = 120.f` world units/sec
- Loop: increment `mLoopCount[patrol]` when `mPatrolWpIdx` wraps back to 0

### Drawer constructor (pre-agreed, both Task 2 and Task 3 must match)
```cpp
GridVisibilityDrawer(
    const Dia::GridVisibility::GridVisibilitySystem<Dia::Pathfinding::SquarePathGrid>& visSystem,
    const Dia::Pathfinding::SquarePathGrid& grid,
    float cellSize,
    const Dia::Maths::Vector2D& gridOrigin,
    const Dia::Maths::Vector2D* entityPositions,  // [7]: [0]=Red, [1]=Blue, [2..6]=E0..E4
    const float* enemyFlashTimers,                 // [5]
    const float& stageTime,
    const Dia::Debug::DebugLayerManager& layerManager
);
```

### IVisibilityChangeObserver
- Module implements `IVisibilityChangeObserver` (or use a local lambda-based impl)
- `OnEntityBecameVisible`: if entity is in `mEnemyEntities[]`, set its flash timer to 0.6f
- `OnEntityBecameHidden`: no action needed

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Stage scaffold: `dia scaffold stage GridVisibilityTestStage --modules DiaGridVisibility DiaPathfinding DiaEntitySpatial`; add to registeredtypes.diaschema; verify skeleton compiles | `dia run googletest` builds | Done | haiku |  |
| 2 | Module implementation: OnStart (grid+wall mask, Domain, EntitySpatialModule, 7 entities, RegisterSightSource, IVisibilityChangeObserver, checkpoints, metrics), OnUpdate (patrol movement, Update(), observer decay, checkpoints), OnStop (cleanup) | `dia run googletest` builds clean | Done | sonnet | after Task 1; parallel with Task 3; Coord fix: mEntityWorldPos now world-space |
| 3 | GridVisibilityDrawer: new Drawers/GridVisibilityDrawer.h + .cpp — IVisualDebugger; cell pass (colour table), entity pass (patrol circles + enemy pulse), HUD text | Compiles under DIA_DEBUG | Done | sonnet | after Task 1; parallel with Task 2 |
| 4 | Wire drawer into OnUpdate (lazy-init in DIA_DEBUG block) + add all new files to CluicheTest.vcxproj + .vcxproj.filters | `dia run googletest` builds | Done | haiku | after Tasks 2+3; 8581 tests pass; drawer wired; vcxproj synced |
| 5 | Final verify: `dia run googletest` (all pass); `dia run cluichetest` (visual fog renders) | All tests pass; no regressions | Done | haiku | after Task 4; 8581 googletest pass; cluichetest visual requires manual review |

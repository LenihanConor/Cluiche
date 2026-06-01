# Plan: SoftBody2D Test Stage

**Spec:** @docs/specs/features/cluichetest/teststages/softbody2d-stage.md
**Status:** In Progress

---

## Implementation Patterns

### Stage module
- Inherits `TestStageModuleBase` (not raw `Module`) — provides `mAutomation`, frame counter, settled-state helpers.
- Lives on **SimPU** (`kAllowedPUs = PUAffinity::kSim`).
- `DoStart`: create `PhysicsWorld` + static anchor body → create `SoftBodyWorld(WorldDef{.rigidBodyWorld=mRBWorld})` → `AddRope` / `AddCloth` → `RegisterCheckpoints`.
- `DoUpdate`: call `mWorld->Update(dt)`, then settle checks using `DeriveVelocity(particle, kFixedDt)`.
- `DoStop`: `mWorld` destructor removes all bodies; delete worlds.

### Settle detection
```cpp
static constexpr float kVelocityEpsilon = 0.001f;
static constexpr float kFixedDt = 1.0f / 30.0f;
// iterate rope particles via mRope->GetParticle(i), cloth via mCloth->GetParticle(x,y)
DeriveVelocity(p, kFixedDt).LengthSquared() < kVelocityEpsilon * kVelocityEpsilon
```

### RopeDef / ClothDef
```cpp
RopeDef ropeDef;
ropeDef.id = StringCRC{"softbody_rope"};
ropeDef.startPoint = { 0.0f, 6.0f };
ropeDef.endPoint   = { 0.0f, 0.5f };
ropeDef.particleCount = 12;
ropeDef.startAnchor = mRBWorld->CreateStaticBody(...);  // actual RB API TBD

ClothDef clothDef;
clothDef.id = StringCRC{"softbody_cloth"};
clothDef.origin = { 2.0f, 4.0f };
clothDef.width  = 1.5f;  clothDef.height = 1.5f;
clothDef.resX   = 4;     clothDef.resY   = 4;
// After AddCloth: mCloth->PinParticle(0,0); mCloth->PinParticle(3,0);
```

### UIUltra panel
- Alpine.js, single file, no build step — matches UIUltralightTestStage pattern.
- Persistent: loaded once in DoStart, stays visible for the stage lifetime.
- Shows two checkpoint rows: `rope_settled` + `cloth_settled` with PASS/PENDING badges.
- C++ updates badge state via BoundMethods (`app.SetRopeSettled(true)`, `app.SetClothSettled(true)`).

### HUD bottom bar
- Provided by `visual-feedback` feature — `TestStageHUDModule` already wired.
- No extra work needed; HUD reads from `TestResultsRegistry` automatically.

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Scaffold stage via `dia scaffold stage SoftBody2D --modules SoftBody2DStageModule` | Dirs + stubs created | Done | haiku | `.diastage`, `.diaapp`, vcxproj stubs |
| 2 | Create `SoftBody2DStageModule.h/.cpp`: stubs + `kTypeId` + `DIA_MODULE` | Compiles clean | Done | sonnet | SimPU affinity; `ModuleRef<Physics2DModule>` |
| 3 | Implement `SetupRope`: `PhysicsWorld` static anchor + `SoftBodyWorld::AddRope(RopeDef{...})` | `mRope != nullptr`; build green | Done | sonnet | Static RB at (400,600); `RopeDef.startAnchor` |
| 4 | Implement `SetupCloth`: `AddCloth(ClothDef{...})` + `PinParticle(0,0)` + `PinParticle(3,0)` | `mCloth != nullptr` | Done | sonnet | 4x4 grid, top-left + top-right corners pinned |
| 5 | Implement `DoUpdate`: `mWorld->Update(dt)`, settle detection, `EmitMetrics()` on cloth settle | Both checkpoints pass in isolation test | Done | sonnet | `DeriveVelocity` + `SquareMagnitude()` |
| 6 | Implement `DoStop`: delete worlds + unregister drawers | No leaks; second run starts clean | Done | haiku | |
| 7 | Register checkpoints + metrics in `DoStart` + wire SoftBody2D drawers via VisualDebuggerModule | Checkpoints queryable; drawers show in console | Done | sonnet | `test.soft_body.rope_settled`, `test.soft_body.cloth_settled`; 4 drawers registered |
| 8 | Create `softbody2d_test.html` UIUltra panel — Alpine, persistent, rope + cloth badges | Open in browser; badges update | Todo | sonnet | Load in DoStart via UIModule |
| 9 | Wire UIUltra panel BoundMethods to module state | Live badge updates when checkpoints fire | Todo | sonnet | `SetRopeSettled` / `SetClothSettled` bound methods |
| 10 | Update vcxproj + filters | Clean VS build | Done | haiku | Added `DiaSoftBody2D` + `DiaSoftBody2DVisualDebugger` project references |
| 11 | Write pytest scenario `test_softbody2d_settle.py` + register in `default.json` | Scenario validates rope < cloth ordering | Done | sonnet | |
| 12 | Run `dia run cluichetest` — visual verify vs mockup; `dia orchestrate` scenario passes | Manual pass + orchestrator green | Todo | sonnet | |

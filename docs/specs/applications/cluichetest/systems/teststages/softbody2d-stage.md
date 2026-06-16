# Feature Spec: SoftBody2D Test Stage

## Parent System
@docs/specs/applications/cluichetest/systems/teststages/teststages.md

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-004, PD-006, PD-007 |
| Application | @docs/specs/applications/cluichetest/cluichetest.md | AD-001, AD-004, AD-005 |
| System | @docs/specs/applications/cluichetest/systems/teststages/teststages.md | SD-TS-001, SD-TS-002, SD-TS-003, SD-TS-004, SD-TS-005 |
| Sibling | @docs/specs/applications/cluichetest/systems/teststages/test-stage-infrastructure.md | Depends on Tasks 1-2 (manifest loader + transitions field) |

## Problem Statement

Unit tests for DiaSoftBody2D validate individual constraints and particle steps in isolation. They cannot exercise: multi-constraint systems (rope chains, cloth grids) settling under real SimPU timing, rigid body anchor coupling, or the interaction between soft body damping and the fixed-timestep accumulator. This stage validates rope and cloth settling with rigid body coupling under real conditions.

## Template Answers (T1–T10)

| # | Question | Answer |
|---|----------|--------|
| T1 | Engine feature exercised | DiaSoftBody2D: rope (chain constraint solver), cloth (grid constraint solver), rigid body anchor coupling |
| T2 | Scene setup in DoStart | Two sub-scenes: (A) Rope — 12 particles in a chain, top pinned to a static rigid body anchor. (B) Cloth — 4x4 particle grid, top-left and top-right corners pinned. Both under gravity. |
| T3 | Checkpoint(s) and success conditions | `soft_body.rope_settled` → true when all rope particles' velocity magnitude < epsilon. `soft_body.cloth_settled` → true when all cloth particles' velocity magnitude < epsilon. |
| T4 | Metrics emitted | `cluichetest.softbody.rope_settle_frame_count`, `cluichetest.softbody.cloth_settle_frame_count`, `cluichetest.softbody.constraint_iterations` (solver iterations per frame) |
| T5 | Processing Unit | SimPU (fixed-timestep for deterministic particle simulation) |
| T6 | Assets needed | None — particles and constraints created programmatically |
| T7 | Gap vs unit tests | Unit tests step individual constraints; this validates multi-constraint chains/grids converging under real timing with rigid body coupling |
| T8 | Determinism constraints | Fixed timestep (SimPU 30Hz), fixed gravity, fixed damping, no randomness, fixed solver iteration count |
| T9 | Expected frame budget | ~300 frames (10s at 30Hz) — cloth takes longer than rope due to more constraints |
| T10 | Dependencies on other modules | TimeServer, AutomationModule, DiaSoftBody2D (particles, constraints, solver), DiaRigidBody2D (static anchor body) |

## Acceptance Criteria

### Shared ACs (inherited from Infrastructure spec)

This stage satisfies AC-S1 through AC-S9 as defined in the [Infrastructure spec](test-stage-infrastructure.md#a--shared-acs-every-test-stage-must-satisfy-these).

### Stage-Specific ACs

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-SB1 | DoStart creates rope (12 particles, chain constraints, pinned to static RB) and cloth (4x4 grid, corner pins) | Code review + both checkpoints eventually pass |
| AC-SB2 | Rope's top particle is coupled to a static rigid body (not just a fixed position) | Rigid body exists; constraint references RB anchor point |
| AC-SB3 | `soft_body.rope_settled` returns true when all 12 rope particles have velocity < 0.001 | Settle detection via velocity threshold |
| AC-SB4 | `soft_body.cloth_settled` returns true when all 16 cloth particles have velocity < 0.001 | Settle detection via velocity threshold |
| AC-SB5 | Rope settles before cloth (fewer constraints, simpler topology) | `rope_settle_frame_count` < `cloth_settle_frame_count` |
| AC-SB6 | Stage completes within 400 frames (13.3s at 30Hz) | Orchestrator timeout |
| AC-SB7 | Repeated runs produce identical settle frame counts ±0 | Determinism (AC-S7) |
| AC-SB8 | All particles and constraints destroyed in DoStop | No leaks |
| AC-SB9 | ImGui drawer shows a toggle checkbox enabling/disabling the SoftBody2DTestStage | Visual — checkbox visible; no checkpoint details in ImGui |
| AC-SB10 | UIUltra panel is persistent (always visible while stage runs) and shows per-checkpoint pass/fail badges (rope_settled ✓, cloth_settled ✓) | Visual against mockup |
| AC-SB11 | HUD bottom bar shows: stage name \| checkpoint badges \| frame counter \| PASS/FAIL/TIMEOUT status \| ✕ exit button | Visual — matches visual-feedback spec AC-VF1–VF8, AC-VF14–VF15 |

**Visual mockup:** [@docs/specs/applications/cluichetest/systems/teststages/softbody2d-stage.mockup.html](softbody2d-stage.mockup.html) — open in a browser for visual acceptance gate reference.

## Design

### Scene Layout

```
Sub-scene A: Rope              Sub-scene B: Cloth

  [RB Anchor]                    [Pin]-------[Pin]
       |                           |  \  |  /  |
       o  (particle 1)            o----o----o----o
       |                           |  \  |  /  |
       o  (particle 2)            o----o----o----o
       |                           |  \  |  /  |
       o  ...                     o----o----o----o
       |                           |  \  |  /  |
       o  (particle 12)           o----o----o----o
                                  (4x4 grid, structural + shear constraints)
```

**Rope:**
- 12 particles, spacing 0.5 units vertically
- Distance constraints between consecutive particles
- Top particle attached to static rigid body at (0, 6)
- Gravity: (0, -9.81)
- Damping: 0.98

**Cloth:**
- 4x4 grid (16 particles), spacing 0.5 units
- Structural constraints (horizontal + vertical neighbors)
- Shear constraints (diagonal neighbors)
- Top-left (0,0) and top-right (1.5, 0) pinned (infinite mass)
- Gravity: (0, -9.81)
- Damping: 0.95 (slightly more damping for stability)

### Settle Detection

```cpp
static constexpr float kVelocityEpsilon = 0.001f;

bool IsRopeSettled() const
{
    for (int i = 0; i < mRope->GetParticleCount(); ++i)
    {
        const auto vel = Dia::SoftBody2D::DeriveVelocity(mRope->GetParticle(i), kFixedDt);
        if (vel.LengthSquared() > kVelocityEpsilon * kVelocityEpsilon)
            return false;
    }
    return true;
}
```

Same pattern for cloth (iterate all 16 particles).

### Module Structure

```cpp
// Cluiche/CluicheTest/Modules/TestStages/SoftBody2DStageModule.h
namespace CluicheTest {

class SoftBody2DStageModule : public Dia::ApplicationFlow::Module
{
public:
    static constexpr Dia::Core::StringCRC kTypeId{"SoftBody2DStageModule"};
    explicit SoftBody2DStageModule(const Dia::Core::StringCRC& instanceId);

protected:
    StartResult DoStart() override;
    void DoUpdate(float deltaTime) override;
    StopResult DoStop() override;

private:
    void SetupRope();
    void SetupCloth();
    void RegisterCheckpoints(Dia::Automation::AutomationService& automation);
    bool IsRopeSettled() const;
    bool IsClothSettled() const;
    void EmitMetrics();

    Dia::ApplicationFlow::ModuleRef<AutomationModule> mAutomation{this};

    // World (owns all bodies)
    Dia::SoftBody2D::SoftBodyWorld* mWorld = nullptr;
    Dia::RigidBody2D::PhysicsWorld* mRBWorld = nullptr; // for rope anchor coupling

    // Body handles (non-owning — world owns)
    Dia::SoftBody2D::Rope*  mRope  = nullptr;
    Dia::SoftBody2D::Cloth* mCloth = nullptr;

    // Tracking
    unsigned int mFrameCount = 0;
    unsigned int mRopeSettleFrame = 0;
    unsigned int mClothSettleFrame = 0;
    bool mRopeSettled = false;
    bool mClothSettled = false;

    static constexpr float kVelocityEpsilon = 0.001f;
    static constexpr float kFixedDt = 1.0f / 30.0f;
};

} // namespace CluicheTest
DIA_MODULE(SoftBody2DStageModule);
```

### Checkpoint Logic

```cpp
automation->RegisterCheckpoint(this, StringCRC("soft_body.rope_settled"),
    [this]() -> CheckpointResult {
        return { mRopeSettled, mRopeSettled ? "12 particles at rest" : "rope still moving", 0.0f };
    });

automation->RegisterCheckpoint(this, StringCRC("soft_body.cloth_settled"),
    [this]() -> CheckpointResult {
        return { mClothSettled, mClothSettled ? "16 particles at rest" : "cloth still moving", 0.0f };
    });
```

### Update Loop

```cpp
void SoftBody2DStageModule::DoUpdate(float deltaTime)
{
    ++mFrameCount;

    mWorld->Update(deltaTime);

    if (!mRopeSettled && IsRopeSettled())
    {
        mRopeSettled = true;
        mRopeSettleFrame = mFrameCount;
    }

    if (!mClothSettled && IsClothSettled())
    {
        mClothSettled = true;
        mClothSettleFrame = mFrameCount;
        EmitMetrics();  // Emit once both are settled (cloth settles last)
    }
}
```

### Manifest Entry

```json
{
    "name": "SoftBody2DStage",
    "manifest": "stages/SoftBody2DStage/misc/ApplicationFlow/softbody2d_stage.diaapp",
    "config": {
        "path_aliases": { "stage_root": "." }
    },
    "transitions": ["Boot"],
    "auto_advance": false
}
```

### Pytest Scenario

```python
# Tools/orchestrator/scenarios/cluichetest/softbody2d/test_softbody2d_settle.py

def test_softbody2d_rope_and_cloth_settle(dia_client):
    """Navigate to SoftBody2DStage, wait for both rope and cloth to settle."""
    dia_client.navigate_to("SoftBody2DStage")

    # Rope should settle first
    result = dia_client.poll_checkpoint("soft_body.rope_settled", timeout_s=8.0)
    assert result["passed"], f"Rope did not settle: {result['message']}"

    # Cloth takes longer
    result = dia_client.poll_checkpoint("soft_body.cloth_settled", timeout_s=14.0)
    assert result["passed"], f"Cloth did not settle: {result['message']}"

    metrics = dia_client.query_metrics("cluichetest.softbody.*")
    assert metrics["cluichetest.softbody.rope_settle_frame_count"] > 0
    assert metrics["cluichetest.softbody.cloth_settle_frame_count"] > metrics["cluichetest.softbody.rope_settle_frame_count"]
    assert metrics["cluichetest.softbody.constraint_iterations"] == 4

    dia_client.navigate_to("Boot")
```

## Files Touched

| File | Change |
|------|--------|
| `Cluiche/CluicheTest/Modules/TestStages/SoftBody2DStageModule.h` | New — module header |
| `Cluiche/CluicheTest/Modules/TestStages/SoftBody2DStageModule.cpp` | New — module implementation |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` | Add new source files |
| `Cluiche/CluicheTest/CluicheTest.vcxproj.filters` | Add filter entries |
| `Cluiche/Assets/Stages/SoftBody2DStage/misc/ApplicationFlow/softbody2d_stage.diaapp` | New — stage app manifest |
| `Cluiche/Assets/Stages/SoftBody2DStage/softbody2d_stage.diastage` | New — stage declaration |
| `Cluiche/Assets/CluicheTest/cluichetest.diagame` | Add import for SoftBody2D stage |
| `Tools/orchestrator/scenarios/cluichetest/softbody2d/test_softbody2d_settle.py` | New — pytest scenario |
| `Cluiche/Assets/Stages/SoftBody2DStage/Presentation/UI/softbody2d_test.html` | New — UIUltra panel (persistent, checkpoint pass/fail display) |
| `Tools/orchestrator/plans/cluichetest/default.json` | Add scenario to plan |

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Scaffold stage via `dia scaffold stage SoftBody2D` | Stage dirs + manifest stubs created | Todo | haiku | Creates .diastage, .diaapp, vcxproj stub |
| 2 | Create SoftBody2DStageModule (.h/.cpp): DoStart/DoUpdate/DoStop stubs, kTypeId, DIA_MODULE | Compiles, module registered | Todo | sonnet | SimPU; ModuleRef<AutomationModule> |
| 3 | Implement SetupRope: `SoftBodyWorld::AddRope(RopeDef{...})` — 12 particles, startAnchor = static RB at (0,6) | Rope created, anchor body coupled | Todo | sonnet | Use `RopeDef.startAnchor`; create `PhysicsWorld` + static body for anchor |
| 4 | Implement SetupCloth: `SoftBodyWorld::AddCloth(ClothDef{...})` — 4x4 grid, `pinTopRow=false`, `PinParticle(0,0)` + `PinParticle(3,0)` | Cloth created, corners pinned | Todo | sonnet | `ClothDef.resX=4, resY=4`; pin top-left + top-right |
| 5 | Implement DoUpdate: `mWorld->Update(dt)`, settle detection via `DeriveVelocity()` < epsilon | Both checkpoints eventually pass | Todo | sonnet | Use `DeriveVelocity(particle, dt)` helper; emit metrics once cloth settles |
| 6 | Implement DoStop: `RemoveBody()` + destroy world | No leaks | Todo | haiku | |
| 7 | Register checkpoints (`soft_body.rope_settled`, `soft_body.cloth_settled`) + metrics | Checkpoints queryable; metrics emitted | Todo | haiku | Registered in DoStart |
| 8 | Create `softbody2d_test.html` UIUltra panel — persistent, shows checkpoint pass/fail badges | Open in browser — matches mockup | Todo | sonnet | Alpine.js; rope_settled + cloth_settled badges |
| 9 | Wire UIUltra panel to stage module (load page via UIModule, update checkpoint state) | Panel shows live pass/fail as stage runs | Todo | sonnet | Follows UIUltralight stage pattern |
| 10 | Add vcxproj + filters entries | Clean build | Todo | haiku | |
| 11 | Write pytest scenario | Both checkpoints pass, rope_frame < cloth_frame confirmed | Todo | sonnet | |
| 12 | Add scenario to plan JSON | `--list` shows softbody2d | Todo | haiku | |
| 13 | Verify: `dia run cluichetest` — visual check against mockup; all checkpoints PASS; orchestrator green | Manual visual gate + no ERROR logs | Todo | sonnet | |

## Dependencies

- **Infrastructure spec Tasks 1-6** must be complete before Task 12
- **DiaSoftBody2D** must support: Particle, distance constraints, constraint groups, Solver with iteration count
- **DiaRigidBody2D** must support: static body creation (for rope anchor)
- **Rigid-soft coupling**: DiaSoftBody2D must support pinning a particle to a rigid body position

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | `kTypeId`, checkpoint names all StringCRC. |
| PD-004 | No STL in public APIs | Module interface uses Dia containers (DynamicArrayC for particles). No STL in public surface. |
| PD-006 | VS project files source of truth | Task 9 adds files to CluicheTest.vcxproj. |
| PD-007 | C++20 required | constexpr StringCRC, standard features. |
| AD-001 (CT) | Three PUs | Module lives on SimPU (physics/soft body simulation). |
| AD-004 (CT) | Test levels included | This IS a test level. |
| AD-005 (CT) | App is testbed not product | Stage exists purely for soft body validation. |
| SD-TS-001 | One manifest stage per feature | One stage: SoftBody2DStage. |
| SD-TS-002 | Checkpoints in DoStart, auto-clear on stop | RegisterCheckpoint in DoStart. Auto-clear on stop. |
| SD-TS-003 | Metrics for threshold assertions | `rope_settle_frame_count`, `cloth_settle_frame_count`, `constraint_iterations` emitted. Pytest asserts ordering + values. |
| SD-TS-004 | All stages return to Boot | `transitions: ["Boot"]` in manifest. |
| SD-TS-005 | Individual stages are feature specs | This spec IS the feature spec. |

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Solver Stability | Can a 4x4 cloth with 4 solver iterations converge, or does it need more? | 4 iterations is standard for game-quality Verlet integration with distance constraints. The cloth is small (16 particles, ~40 constraints). If it doesn't converge, increase iterations — the test will catch it by failing AC-SB6 (timeout). |
| 2 | Rigid-Soft Coupling | How does the rope anchor work — particle pinned to RB position, or constraint between particle and RB? | Particle's position is overwritten each frame to match the RB anchor point (infinite mass particle pattern). The RB is static, so its position never changes. This is simpler than a full constraint and sufficient for anchoring. |
| 3 | Velocity Epsilon | Is 0.001 appropriate for both rope and cloth? | Yes — both use the same units and similar damping. 0.001 m/s is effectively at rest. If cloth oscillates around this threshold, increase damping slightly. The test is about convergence, not threshold tuning. |
| 4 | Cloth Shear Constraints | Are shear constraints necessary for settling or just visual? | Necessary for stability. Without shear, the grid can collapse into a line (degenerate configuration). Shear constraints maintain the 2D shape and ensure a stable rest pose exists. |
| 5 | Metrics Timing | Metrics emit when cloth settles (the slower one). What about rope metrics? | `EmitMetrics()` is called once when cloth settles (the last to finish). It emits all 3 metrics at that point — both settle frame counts are already stored. No need for separate emission points. |
| 6 | Dependencies | Does DiaSoftBody2D exist today with the required API? | **Confirmed** — `SoftBodyWorld`, `Rope`/`Cloth`/`Particle`, `RopeDef`/`ClothDef`, `DeriveVelocity()` all present. `SoftBodyWorld::Update()` drives the solver. `RopeDef.startAnchor` wires rigid body coupling. `Cloth::PinParticle(x,y)` pins corner particles. No blockers. |

## Status

`Approved` — 2026-05-22

**Plan:** [softbody2d-stage.plan.md](softbody2d-stage.plan.md)

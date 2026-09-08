# Feature Spec: Mesh3DRenderSystemTestStage

## Parent System
@docs/specs/applications/cluichetest/systems/teststages/teststages.md

---

## Problem Statement

`Mesh3DTestStage` only proves the 3D render pipeline doesn't crash over 30 frames — its pass condition is purely temporal. No test covers `MeshRenderer` behaviour under draw-call variety: multiple commands per frame, or silent skipping of a draw command whose mesh ID is not in `MeshGpuCache`.

---

## Acceptance Criteria

### Shared ACs (inherited from test-stage-infrastructure.md)

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-S1 | Stage has a manifest entry with `transitions: ["Boot"]` | Manifest review |
| AC-S2 | Stage module registers at least one checkpoint in OnStart via AutomationService | Code review + checkpoint trigger returns success |
| AC-S3 | Stage module emits at least one metric to MetricRegistry | Metric query returns value |
| AC-S4 | Stage module logs at OnStart entry and OnStop entry (`DIA_LOG_INFO`) | Session log review |
| AC-S5 | Stage module returns kReady only after scene setup is complete | Code review |
| AC-S6 | Stage module cleans up all resources in OnStop | No leaks; registered meshes deregistered |
| AC-S7 | Navigating Boot → Stage → Boot → Stage produces identical checkpoint results | Orchestrator scenario runs twice |
| AC-S8 | Checkpoint names follow convention: `<feature>.<checkpoint_name>` | Code review |
| AC-S9 | Stage has a matching pytest scenario file | File exists |

### Stage-Specific ACs

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-R1 | Each frame submits exactly 5 draw commands: 3 valid meshes (unit cube × 3 transforms) + 1 known-ready glTF mesh + 1 unknown mesh ID (`"mesh3d.does_not_exist"`) | Code review of OnUpdate |
| AC-R2 | The unknown-mesh draw command is silently skipped by `MeshRenderer` — no crash, no GPU error, and subsequent commands in the same frame are unaffected | Stage completes 60 frames without crash; draw-call metric reflects only valid commands rendered |
| AC-R3 | `test.mesh3drendersystem.passed` checkpoint fires after 60 consecutive frames where all 4 valid draw commands are submitted and the stage has not crashed | Orchestrator polls checkpoint; passes within 65 frames |
| AC-R4 | `dia.render3d.mesh_draw_calls` metric is non-zero on each frame after the glTF asset is ready | Metric query returns value ≥ 3 (cube × 3) once asset loaded |
| AC-R5 | Stage is visually observable: 3 unit cubes at distinct positions and the avocado glTF visible when asset loads | Manual visual confirmation |

---

## Design

### Scene Layout

```
+----------------------------------------------+
|                                              |
|   [cube -3,0,0]  [cube 0,0,0]  [cube 3,0,0] |
|                                              |
|              [avocado 0,2,0]                 |
|   (appears after asset load; 15x scale)      |
|                                              |
|  HUD: Mesh3DRenderSystemTestStage | PENDING/PASS | 042 |
+----------------------------------------------+
```

Camera: eye at (0, 3, -8), target (0, 0, 0), up (0, 1, 0), FOV 60°.
Three unit cubes at (−3, 0, 0), (0, 0, 0), (3, 0, 0) — visually confirms multiple draw commands render to distinct positions. Avocado at (0, 2, 0) confirms glTF mesh submits correctly once loaded. Unknown mesh ID submitted every frame but never appears — absence confirms silent skip.

### Module Structure

```cpp
// CluicheTest/Modules/TestStages/Mesh3DRenderSystemTestStageModule.h
class Mesh3DRenderSystemTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;  // "Mesh3DRenderSystemTestStageModule"
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = kSim;
    explicit Mesh3DRenderSystemTestStageModule(const Dia::Core::StringCRC& instanceId);

protected:
    bool AreDependenciesReady() override;
    Dia::Core::StringCRC GetStageName() const override;   // "Mesh3DRenderSystemTestStage"
    unsigned int GetBudgetFrames() const override;         // 300
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    Dia::ApplicationFlow::StreamWriter<Dia::Graphics3D::FrameData3D>           mRenderOutput{this, "SimToRender3D"};
    Dia::ApplicationFlow::ServiceStreamReader<Dia::Mesh3D::Mesh3DAssetHandler> mMeshHandlerService{this, "KernelMeshHandler"};

    Dia::Graphics3D::FrameData3D mFrame;
    Dia::Mesh3D::Mesh3DAsset*    mUnitCubeAsset = nullptr;
};
```

Module lives on **SimPU**, matching `Mesh3DTestStageModule` (render stream writes from Sim; Canvas3D reads on render thread).

### Checkpoint Logic

```cpp
service->RegisterCheckpoint(this, Dia::Core::StringCRC("test.mesh3drendersystem.passed"),
    [this]() -> Dia::Automation::CheckpointResult {
        const bool passed = GetFrameCount() >= 60;
        return { passed, passed ? "rendered 60 frames" : "waiting", static_cast<float>(GetFrameCount()) };
    });
```

### OnUpdate Pattern

```cpp
void Mesh3DRenderSystemTestStageModule::OnUpdate(float)
{
    mFrame.Clear();
    // camera + lights (same as Mesh3DTestStage)

    // 3 valid unit cubes at distinct positions
    for (int i = -1; i <= 1; ++i)
    {
        Dia::Graphics3D::Mesh3DDrawCommand cmd;
        cmd.meshId     = Dia::Core::StringCRC("unit_cube");
        cmd.materialId = Dia::Core::StringCRC("default_3d");
        cmd.transform  = Dia::Maths::Matrix44::FromTranslation({ 3.0f * i, 0.0f, 0.0f });
        mFrame.RequestDrawMesh(cmd);
    }

    // 1 glTF mesh (skipped silently until asset is ready)
    Dia::Graphics3D::Mesh3DDrawCommand avocado;
    avocado.meshId     = Dia::Core::StringCRC("mesh3d.avocado");
    avocado.materialId = Dia::Core::StringCRC("default_3d");
    avocado.transform  = Dia::Maths::Matrix44::FromTranslation({ 0.0f, 2.0f, 0.0f })
                       * Dia::Maths::Matrix44::FromScale(15.0f);
    mFrame.RequestDrawMesh(avocado);

    // 1 unknown mesh ID — must be silently skipped, must not crash
    Dia::Graphics3D::Mesh3DDrawCommand ghost;
    ghost.meshId     = Dia::Core::StringCRC("mesh3d.does_not_exist");
    ghost.materialId = Dia::Core::StringCRC("default_3d");
    ghost.transform  = Dia::Maths::Matrix44::Identity();
    mFrame.RequestDrawMesh(ghost);

    mRenderOutput.Write(mFrame, Dia::Core::TimeAbsolute::Zero());

    if (GetFrameCount() == 60)
        ReportPassed();
}
```

---

## Files Touched

| File | Change |
|------|--------|
| `docs/specs/applications/cluichetest/systems/teststages/mesh3d-render-system-stage.md` | This spec |
| `Cluiche/CluicheTest/Modules/TestStages/Mesh3DRenderSystemTestStageModule.h` | Implement (replace scaffold stub) |
| `Cluiche/CluicheTest/Modules/TestStages/Mesh3DRenderSystemTestStageModule.cpp` | Implement (replace scaffold stub) |
| `Cluiche/Assets/CluicheTest/assets.catalogue.json` | Fix stage/manifest IDs (mesh3drendersystem → mesh3d_render_system) |
| `Tools/orchestrator/scenarios/cluichetest/mesh3d_render_system_stage/smoke.py` | New pytest scenario |

Note: scaffold files (`.diastage`, `.diaapp`, vcxproj entries, diagame import) already exist from commit `856e56ec`. Only the catalogue ID rename and the module implementation are outstanding.

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Fix catalogue IDs: rename `mesh3drendersystem_test_stage` → `mesh3d_render_system_test_stage` in `assets.catalogue.json` (stage id, manifest id, contains target) | Warning spam stops on launch | Todo | haiku | Fixes the `GetLoadProgress: unknown stage` warning |
| 2 | Implement `Mesh3DRenderSystemTestStageModule` — `AreDependenciesReady`, `OnStart`, `OnUpdate`, `OnConnectStreams`, `OnStop` per spec design | Stage loads, 3 cubes visible, checkpoint fires at frame 60 | Todo | sonnet | Replace scaffold TODOs; follow Mesh3DTestStageModule pattern exactly |
| 3 | Write pytest scenario `mesh3d_render_system_stage/smoke.py` | Orchestrator scenario passes | Todo | sonnet | Navigate → wait for checkpoint → assert draw_calls metric |
| 4 | `dia run cluichetest` — visual verify: 3 cubes + avocado appear, no crash over 60 frames | Manual visual gate | Todo | sonnet | |
| 5 | Commit + update spec status → Done | — | Todo | haiku | |

---

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | `kTypeId`, checkpoint name `"test.mesh3drendersystem.passed"`, stage name `"Mesh3DRenderSystemTestStage"`, mesh IDs all `StringCRC`. |
| PD-004 | No STL in public APIs | Module interface uses Dia types. |
| PD-006 | VS project files are source of truth | vcxproj entries already added in scaffold commit; no new files added. |
| SD-TS-001 | One manifest stage per feature | Exactly one stage: `Mesh3DRenderSystemTestStage`. |
| SD-TS-002 | Checkpoints in OnStart, auto-clear on stop | `RegisterCheckpoint` called in `OnStart`; auto-clear via module ownership. |
| SD-TS-003 | Metrics for threshold assertions | `dia.render3d.mesh_draw_calls` emitted by Canvas3D; pytest asserts non-zero. |
| SD-TS-004 | All stages return to Boot | `transitions: ["Boot"]` already in `.diastage`. |
| SD-TS-005 | Individual stages are feature specs | This spec IS the feature spec. |

---

## Open Design Questions

1. **Silent-skip coverage** — `MeshRenderer::DrawCommand` currently skips unknown mesh IDs silently. If that behaviour is ever made strict (log + skip vs. crash), this AC-R2 becomes a regression detector. Worth checking at implementation start whether `DrawCommand` logs a warning for unknown IDs — if it does, AC-R2 should assert the warning appears exactly once per frame (not zero, not more).

2. **Metric source** — AC-R4 uses `dia.render3d.mesh_draw_calls` which is owned by Canvas3D, not this module. If the metric only counts GPU submissions (not CPU draw commands), it will read 3 (cubes only) until the avocado loads, then 4 — not 5. The pytest threshold should assert ≥ 3, not == 5.

---

## Status

`Done`

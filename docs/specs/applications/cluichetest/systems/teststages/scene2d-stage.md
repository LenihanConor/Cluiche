# Feature Spec: Scene2DTestStage

**Parent:** @docs/specs/applications/cluichetest/systems/teststages/teststages.md

**Status:** `Approved`

---

## Problem Statement

No runtime validation path exists for DiaScene2D's load pipeline. Unit tests can verify struct serialization in isolation, but cannot confirm that `SceneLoader2D` correctly hydrates `CameraRegistry2D`, `LightRegistry2D`, and `diaentitytemplate::Domain` from a `.diascene` file under real PU timing. A test stage closes that gap.

---

## Template Answers

| # | Question | Answer |
|---|----------|--------|
| T1 | Engine feature under test | `DiaScene2D` — `SceneLoader2D`, `Scene2D` reflected struct, `LayerTable`, `.diascene` file format, camera/light registry hydration, entity spawning with instance_data |
| T2 | Scene layout | Load a single handcrafted `.diascene` file exercising all key paths: 3 layers, 1 active camera with behaviour + instance_data, 2 lights with layer masks, 4+ entities with instance_data overrides. Render the loaded state to confirm hydration visually. |
| T3 | Acceptance criteria | Shared ACs 1–9 + stage-specific ACs AC-SC1 through AC-SC5 (see below) |
| T4 | Metrics | `scene.entity_count` (entities spawned), `scene.layer_count` (layers resolved), `scene.load_time_ms` (time from Load call to completion) |
| T5 | PU assignment | `Scene2DTestStageModule` on **MainPU** (AutomationModule dependency) |
| T6 | Assets | `.diascene` test file at `Assets/Stages/Scene2DTestStage/test_scene.diascene`; `.diastage` + `.diaapp` via scaffold |
| T7 | Unit test gap | DiaScene2D unit tests cover struct serialization roundtrip and LayerTable logic; this stage covers the full load pipeline under real PU timing with real registries |
| T8 | Determinism | Fully deterministic — static scene, no simulation after load |
| T9 | Frame budget | One-shot load; negligible per-frame cost after hydration |
| T10 | Dependencies | DiaScene2D (scene2d-format + scene2d-loader features must be Done), DiaCamera2D, DiaLighting2D, diaentitytemplate |

---

## Acceptance Criteria

### Shared ACs (inherited from test-stage-infrastructure.md)

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-S1 | Stage has a manifest entry with `transitions: ["Boot"]` | Manifest review |
| AC-S2 | Stage module registers at least one checkpoint in DoStart via AutomationService | Code review + checkpoint trigger returns success |
| AC-S3 | Stage module emits at least one metric to MetricRegistry | Metric query from orchestrator returns value |
| AC-S4 | Stage module logs at DoStart entry, DoStop entry (`DIA_LOG_INFO`) | Session log review |
| AC-S5 | Stage module returns `StartResult::kReady` only after scene setup is complete | Code review |
| AC-S6 | Stage module cleans up all resources in DoStop, returns `StopResult::kDone` | No leaks; asset handles released; SceneLoader2D::Unload called |
| AC-S7 | Navigating Boot → Stage → Boot → Stage produces identical checkpoint results | Orchestrator scenario runs twice in sequence |
| AC-S8 | Checkpoint names follow convention: `<feature>.<checkpoint_name>` | Code review |
| AC-S9 | Stage has a matching pytest scenario file | File exists |

### Stage-Specific ACs

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-SC1 | `scene.loaded` checkpoint passes after `.diascene` file is parsed and `SceneLoader2D::Load` returns true | Orchestrator polls checkpoint → `passed: true` within 5 frames |
| AC-SC2 | `scene.cameras_hydrated` checkpoint passes when CameraRegistry2D contains exactly 1 camera with correct position and behaviour type matching the `.diascene` file | Checkpoint lambda queries registry and validates properties |
| AC-SC3 | `scene.lights_hydrated` checkpoint passes when LightRegistry2D contains the expected lights with correct positions and layer masks matching `affects_layers` in the `.diascene` file | Checkpoint lambda queries registry and validates bitmask |
| AC-SC4 | `scene.entities_spawned` checkpoint passes when Domain contains the expected entity count, each at the position specified by `instance_data` in the `.diascene` file | Checkpoint lambda queries Domain entity list + component data |
| AC-SC5 | `scene.layers_resolved` checkpoint passes when LayerTable has the expected layer count, correct sort order, and bitmask resolution matches `affects_layers` references | Checkpoint lambda queries LayerTable state |

---

## Design

### Scene File (`test_scene.diascene`)

```json
{
  "scene2d": {
    "world_bounds": { "min": [0, 0], "max": [1920, 1080] },
    "layers": [
      { "id": "background", "sort_order": -10, "parallax": [0.5, 0.0], "sort_policy": "insertion", "enabled": true },
      { "id": "midground", "sort_order": 0, "parallax": [1.0, 1.0], "sort_policy": "insertion", "enabled": true },
      { "id": "foreground", "sort_order": 10, "parallax": [1.0, 1.0], "sort_policy": "insertion", "enabled": true }
    ],
    "cameras": [
      { "id": "test_camera", "active": true, "blueprint": "camera_2d_static", "instance_data": { "Camera2D.position": [960, 540] } }
    ],
    "lights": [
      { "id": "light_a", "blueprint": "point_light_warm", "enabled": true, "instance_data": { "PointLight2D.position": [300, 200] }, "affects_layers": ["midground", "foreground"] },
      { "id": "light_b", "blueprint": "point_light_cool", "enabled": true, "instance_data": { "PointLight2D.position": [1600, 800] }, "affects_layers": ["foreground"] }
    ],
    "entities": [
      { "id": "entity_a", "blueprint": "test_entity", "instance_data": { "Transform2D.position": [200, 300] } },
      { "id": "entity_b", "blueprint": "test_entity", "instance_data": { "Transform2D.position": [600, 400] } },
      { "id": "entity_c", "name": "named_entity", "blueprint": "test_entity", "instance_data": { "Transform2D.position": [1000, 500] } },
      { "id": "entity_d", "blueprint": "test_entity", "enabled": false, "instance_data": { "Transform2D.position": [1400, 600] } }
    ]
  }
}
```

Tests: 3 layers, 1 camera (active, with instance_data), 2 lights (different layer mask configurations), 4 entities (one named, one disabled, varied positions).

### Module Structure

```cpp
// CluicheTest/Modules/TestStages/Scene2DTestStageModule.h
class Scene2DTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;  // "Scene2DTestStageModule"
    explicit Scene2DTestStageModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::Core::StringCRC GetStageName() const override;       // "Scene2DTestStage"
    unsigned int GetBudgetFrames() const override;             // 60
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;

    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;

private:
    void LoadScene();
    bool ValidateCameras() const;
    bool ValidateLights() const;
    bool ValidateEntities() const;
    bool ValidateLayers() const;

    Dia::Scene2D::SceneLoader2D mSceneLoader;
    Dia::Scene2D::LayerTable mLayerTable;

    // Registries owned by this module for the test
    Dia::Camera2D::CameraRegistry2D mCameraRegistry;
    Dia::Lighting2D::LightRegistry2D mLightRegistry;
    // Domain reference from ModuleRef or owned locally for test isolation

    bool mLoadSucceeded = false;
};
```

### Checkpoint Logic

```cpp
void Scene2DTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    LoadScene();

    service->RegisterCheckpoint(this, StringCRC("scene.loaded"),
        [this]() -> CheckpointResult {
            return { mLoadSucceeded, mLoadSucceeded ? "loaded" : "load_failed", 0.0f };
        });

    service->RegisterCheckpoint(this, StringCRC("scene.cameras_hydrated"),
        [this]() -> CheckpointResult {
            bool ok = ValidateCameras();
            return { ok, ok ? "cameras_valid" : "cameras_invalid", 0.0f };
        });

    service->RegisterCheckpoint(this, StringCRC("scene.lights_hydrated"),
        [this]() -> CheckpointResult {
            bool ok = ValidateLights();
            return { ok, ok ? "lights_valid" : "lights_invalid", 0.0f };
        });

    service->RegisterCheckpoint(this, StringCRC("scene.entities_spawned"),
        [this]() -> CheckpointResult {
            bool ok = ValidateEntities();
            return { ok, ok ? "entities_valid" : "entities_invalid", 0.0f };
        });

    service->RegisterCheckpoint(this, StringCRC("scene.layers_resolved"),
        [this]() -> CheckpointResult {
            bool ok = ValidateLayers();
            return { ok, ok ? "layers_valid" : "layers_invalid", 0.0f };
        });
}
```

### Pytest Scenario

```python
# Tools/orchestrator/scenarios/cluichetest/scene2d_stage/smoke.py
def test_scene2d_stage_loads(cluichetest):
    cluichetest.navigate_to("Scene2DTestStage")
    cluichetest.wait_for_checkpoint("scene.loaded", timeout_frames=10)
    cluichetest.wait_for_checkpoint("scene.cameras_hydrated", timeout_frames=5)
    cluichetest.wait_for_checkpoint("scene.lights_hydrated", timeout_frames=5)
    cluichetest.wait_for_checkpoint("scene.entities_spawned", timeout_frames=5)
    cluichetest.wait_for_checkpoint("scene.layers_resolved", timeout_frames=5)

    entity_count = cluichetest.get_metric("scene.entity_count")
    assert entity_count == 3  # 4 defined, 1 disabled

    layer_count = cluichetest.get_metric("scene.layer_count")
    assert layer_count == 3

    cluichetest.navigate_to("Boot")
```

---

## Files Touched

| File | Change |
|------|--------|
| `docs/specs/features/cluichetest/teststages/scene2d-stage.md` | This spec |
| **Scaffold (via `dia scaffold stage Scene2D --budget 60`)** | |
| `Cluiche/Assets/Stages/Scene2DTestStage/scene2d_test_stage.diastage` | New stage declaration |
| `Cluiche/Assets/Stages/Scene2DTestStage/Misc/ApplicationFlow/scene2d_test_stage.diaapp` | New app manifest |
| `Cluiche/Assets/Stages/Scene2DTestStage/test_scene.diascene` | Test scene file |
| `Cluiche/CluicheTest/Modules/TestStages/Scene2DTestStageModule.h/.cpp` | New module |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` + `.vcxproj.filters` | Add module source |
| `Cluiche/Assets/CluicheTest/Global/Misc/ApplicationFlow/cluiche_main.diaapp` | Stage + Boot transition + HUD |
| `Cluiche/Assets/CluicheTest/cluichetest.diagame` | Add import |
| `Cluiche/Assets/CluicheTest/assets.catalogue.json` | Add stage + manifest entries |
| **E2E** | |
| `Tools/orchestrator/scenarios/cluichetest/scene2d_stage/smoke.py` | E2E scenario |

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `dia scaffold stage Scene2D --budget 60` | `dia pipeline --target cluichetest` green; stage in Boot menu | Todo | haiku | Script handles scaffold files |
| 2 | Create `test_scene.diascene` test file | File parses via JsonArchive | Todo | haiku | Handcrafted; exercises all paths |
| 3 | Implement `Scene2DTestStageModule` — LoadScene, validation lambdas, checkpoint registration | Stage loads; all 5 checkpoints fire correctly | Todo | sonnet | Depends on DiaScene2D being available |
| 4 | Wire metrics: `scene.entity_count`, `scene.layer_count`, `scene.load_time_ms` | Metric queries return expected values | Todo | haiku | |
| 5 | Write pytest scenario `scene2d_stage/smoke.py` | Orchestrator scenario passes | Todo | sonnet | |
| 6 | `dia run cluichetest` — visual verify loaded scene | Manual visual gate | Todo | sonnet | |
| 7 | Commit + update spec status → Done | — | Todo | haiku | |

---

## Dependencies

```
Task 1 (scaffold)   → start first
Task 2 (scene file) → independent; parallel with Task 1
Task 3 (module)     → after Tasks 1 + 2; requires DiaScene2D implementation complete
Task 4 (metrics)    → after Task 3
Task 5 (pytest)     → after Task 3
Task 6 (verify)     → after Tasks 3 + 4 + 5
Task 7 (commit)     → after Task 6
```

**Blocking dependency:** DiaScene2D system (scene2d-format + scene2d-loader features) must be Done before Task 3 can start.

---

## Binding Decisions

| ID | Decision | Compliance |
|----|----------|------------|
| SD-TS-001 | One manifest stage per feature | Exactly one stage: `Scene2DTestStage` |
| SD-TS-002 | Checkpoints in DoStart, auto-clear on stop | All 5 checkpoints registered in `OnStart`; auto-clear via module ownership |
| SD-TS-003 | Metrics for threshold assertions | `scene.entity_count`, `scene.layer_count`, `scene.load_time_ms` emitted |
| SD-TS-004 | All stages return to Boot | `transitions: ["Boot"]` in `.diastage` |
| PD-001 | StringCRC for all IDs | Stage name, checkpoint names, metric names, entity/layer/camera IDs all StringCRC |
| PD-010 | `.diastage` declares stage metadata | `scene2d_test_stage.diastage` added; scene referenced from stage config |

---

## Open Design Questions

1. **Entity Domain ownership** — Should the test stage own its own `Dia::Entity::Domain` instance for isolation, or share the existing one from EntityModule (if present on MainPU)? Own instance is simpler for validation but may diverge from real usage.

2. **Disabled entity assertion** — The test scene includes one `enabled: false` entity. Should `scene.entities_spawned` assert that only 3 entities exist in Domain (disabled skipped), or that 4 exist but one is marked inactive? Depends on SceneLoader2D's behaviour with `enabled: false`.

---

## Status

`Approved` — 2026-06-01

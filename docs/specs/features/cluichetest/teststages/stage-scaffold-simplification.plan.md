# Stage Scaffold Simplification Plan

**Goal:** Reduce new CluicheTest stage creation from 8 manual touch points to ~4, eliminate silent failure modes.

**Spec:** N/A — cross-cutting DX/hardening pass. Updates existing systems, no new feature.

## Session Notes

Six agreed changes to reduce fragility of adding new test stages. Key constraints:
- Stage-scoped module lifecycle MUST be preserved (clean Start/Stop) — no "stages: all" for test modules
- FrameStream auto-flush must not spam logs — fire-once flag pattern
- Pipeline changes must not break DiaAssetCatalogueEditor or DiaPipelineEditor
- `"disabled": true` flag in assets.catalogue.json opts a stage out of auto-build

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Pipeline auto-deploy `.diastage` from `.diagame` imports | `dia pipeline --target cluichetest` deploys new stage without pipeline.toml edit | Done | sonnet | `_derive_diastage_deploy_rules()` in package_stage.py; explicit .diastage entries removed from pipeline.toml |
| 2 | TestStageHUDModule warn on unconfigured stage | Activate stage not in HUD list → one DIA_LOG_WARNING in output | Done | haiku | `mHUDWarnedStage` + `DIA_LOG_WARNING` in TestMainStateProducerModule::DoPopulateFrame |
| 3 | Boot transitions auto-derived from stages array | Add stage to stages[] → appears in Boot menu without editing Boot.transitions | Done | sonnet | Application::GetStageTransitions empty-transitions fallback; cluiche_main.diaapp Boot has `"transitions": []` |
| 4 | FrameStream auto-flush with fire-once warn | Remove VisualDebuggerModule from stage .diaapp → no freeze, one warn logged | Done | sonnet | `mWarnedNoData` flag in FrameStreamStore; `FrameStream_WarnNoData()` helper in FrameStreamDiagnostics.cpp (avoids macro-in-template issue) |
| 5 | Pipeline derives asset_stages from catalogue | Remove asset_stages from pipeline.toml cluichetest section → pipeline still builds stage assets | Done | sonnet | config_loader.py reads catalogue JSON, filters type==stage && !disabled; asset_stages removed from pipeline.toml |
| 6 | Fix pipeline up-to-date check for global manifest | Edit cluiche_main.diaapp → next `dia pipeline` re-deploys it without force-copy | Done | haiku | cluiche_main.diaapp added to pipeline.toml deploy files |
| 7 | Update /new-cluichetest-stage skill | Skill reflects reduced touch points | Done | haiku | Skill updated with Pending Simplifications section noting completed items |
| 8 | Add DIA_TRACE_ZONE to FrameStream auto-flush path | Profiler shows when auto-flush fires | Todo | haiku | Deferred — hot path, needs discussion before adding trace overhead |
| 9 | Clean up existing stages after all changes land | Existing stages compile and run cleanly with no redundant boilerplate | Done | haiku | All three stage .diaapps already clean — AssetRuntimeStage uses AssetRuntimeRendererModule (writes SimToRender directly), DummyStage uses DummyLevelModule, RigidBody2DStage has empty SimPU modules[]. Skill updated: VisualDebuggerModule removed from template, pipeline.toml steps removed, Boot transitions use `[]`, step numbering corrected |

## Dependency Order

```
#6 (independent bug fix)
#2 (independent, small)
#4 (engine change, enables #8)
#3 (manifest runtime change)
#1 + #5 (pipeline changes, can parallel)
#7 (skill update, after all above)
#8 (after #4)
#9 (after #1, #3, #4, #5 — final cleanup pass)
```

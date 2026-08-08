**Spec:** @docs/specs/applications/cluichetest/systems/teststages/arena-test-stage.md
**Status:** In Progress

## API Decisions (resolved before implementation)

- **TestStageModuleBase** — extends base, not Module directly; `OnStart/OnUpdate/OnStop` pattern
- **TriggerScriptModule** — embedded as value member; call `LoadFromJson`/`SetActionRegistry`/etc. in OnStart, `Tick(dt)` in OnUpdate (DoStart/DoUpdate/DoStop not accessible)
- **Blackboard → IConditionContext** — bridge via `ConditionRegistry`; global registry bridges global Blackboard; per-enemy registry bridges per-enemy Blackboard
- **ISpatialProvider does not exist** — use `EntitySpatialModule` + `Entity::Domain` value members; track player as entity with `SpatialComponent`; call `mEntitySpatialModule.Update()` each frame
- **TriggerActionRegistry** — takes `ITriggerActionHandler*` only; one `ArenaActionHandler` class wrapping a callback, 4 instances owned by module
- **delay_seconds on State triggers** — NOT supported in DiaTriggerScript StateParams; inter-wave delay implemented as 2.0s countdown in FireEvent handler; wave_cleared flags only set after countdown expires; State trigger then fires immediately when flag becomes 1
- **DiaTriggerScript + DiaObjective + DiaStateMachine** — not in CluicheTest.vcxproj; added in Task 1
- **Player start position** — (0, -3) rather than origin; moves north at 1 unit/s; enters power-up zone (-1,-1)→(1,1) around t=2s (satisfies AC-A3 without firing spatial trigger on frame 1)

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Scaffold: create asset dirs + stub .h/.cpp + .diastage + .diaapp; update cluiche_main.diaapp (Boot transitions + stage entry + VisualDebuggerModule/TestStageHUDModule/VisualDebuggerConsoleModule lists); update cluichetest.diagame + assets.catalogue.json; add ClCompile/ClInclude to vcxproj + filters; add ProjectRefs for DiaTriggerScript/DiaObjective/DiaStateMachine | Build succeeds (no linker errors from new files); ArenaTestStage appears in Boot menu | Pending | haiku | |
| 2 | Create `arena_trigger_script.json` (7 triggers, all 4 types, no delay_seconds on State triggers) + `arena_objectives.json` (3 Primary + 1 Optional) | JSON parses — `dia validate manifest` passes | Pending | sonnet | State triggers use wave_cleared==1 condition only; delay handled by module countdown |
| 3 | Write full `ArenaTestStageModule.h`: EnemyAgent struct (FSM, Blackboard, ConditionRegistry unique_ptr, UtilitySet, RuleSet, per-enemy members), ArenaActionHandler class, all private method declarations, all member fields matching spec design | Compiles with stub .cpp | Pending | sonnet | |
| 4 | Implement `DoStart` + `LoadTriggerScript` + `LoadObjectives` + `RegisterCheckpoints`: global Blackboard + ConditionRegistry bridge; Domain + EntitySpatialModule construction (player entity + SpatialComponent); TriggerScriptModule setup (LoadFromJson, 4 ArenaActionHandler instances, SetConditionContext, SetSpatialModule); ObjectiveSet load; 6 checkpoints registered | Module reaches kReady; TriggerScript validation passes | Pending | sonnet | Read EntitySpatialIndex.h for SquareDef; read ObjectiveSet.h for LoadFromJson signature |
| 5 | Implement `SpawnWave` + full `EnemyAgent` initialization: FlatStateMachine (4 states + transitions, read StateMachineDefinition.h for API), per-enemy Blackboard + ConditionRegistry, UtilitySet LoadFromJson (2-action JSON inline), RuleSet LoadFromJson (DespCharge rule inline), per-enemy DiaRules::RuleActionRegistry | Wave 1 spawns 5 fully-initialized agents | Pending | sonnet | |
| 6 | Implement `DoUpdate` full frame loop: Tick TriggerScript; Evaluate ObjectiveSet; per-enemy loop (blackboard update → FSM Update → UtilitySet::SelectWinner → RuleSet::Evaluate → move → combat → death → IncrementCount); player walk; wave countdown timers (mWave1ClearDelay etc.); frame count; victory detection | All 3 waves clear; arena.victory fires; AC-A10 determinism | Pending | sonnet | `UtilitySet::SelectWinner` for read-only score; `RuleActionRegistry` (DiaRules) dispatches DespCharge action |
| 7 | Register 5 metrics in `DoStart` via MetricRegistry; emit updated values in DoUpdate | Metric queries return correct final counts | Pending | haiku | total_kills, waves_completed, powerup_collected, desperate_charges_fired, total_frames |
| 8 | Implement `DrawDebugImGui`: ImGui::GetBackgroundDrawList() colored rects (player=green, alive=red, dead=grey, zone=cyan); sidebar window (objective states + last trigger + wave counter) | Visual renders during manual run | Pending | sonnet | |
| 9 | Implement `DoStop`: stop TriggerScriptModule (if needed), reset EntitySpatial domain, clear enemy array, reset all counters/flags | Clean stop; second run produces identical frame count | Pending | haiku | |
| 10 | Write `Tools/orchestrator/scenarios/cluichetest/arena/test_arena_e2e.py` + add scenario to `default.json` | `dia orchestrate --list` shows arena scenario | Pending | sonnet | Include determinism second-pass assertion per spec |
| 11 | E2E verification: `dia run cluichetest`; navigate to ArenaTestStage; confirm all 6 checkpoints pass within 900 frames | All checkpoints pass; metrics match expected values | Pending | sonnet | Final gate — builds, runs, all ACs confirmed |

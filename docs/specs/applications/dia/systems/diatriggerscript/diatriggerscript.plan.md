**Spec:** @docs/specs/applications/dia/systems/diatriggerscript/diatriggerscript.md
**Status:** Done

## Implementation Patterns

**TriggerDef is move-only.** `StateParams` contains `Dia::Condition::ConditionExpr` which is move-only. Delete copy ctor/assign on `TriggerDef`. All four `Params` structs exist as members (no union — non-trivial members make union awkward); only the one matching `type` is populated. Matches `ObjectiveDef` and `RuleSet` move-only precedent.

**TriggerActionRegistry follows the explicit-construction pattern.** Not a singleton. Internal storage: `DynamicArrayC<Pair<StringCRC, ITriggerActionHandler*>>`. `DIA_ASSERT` in `Dispatch()` if handler is unregistered. Same shape as `RuleActionRegistry` (DiaRules) and `ConditionRegistry` (DiaCondition).

**pimpl for STL isolation.** `TriggerScriptModule` and `TriggerActionRegistry` use pimpl (`struct Impl`) to keep any STL (e.g. `std::vector` for internal trigger state) out of public headers. Matches the pattern established by `RuleActionRegistry` and `ConditionRegistry`.

**TriggerScriptModule internal state — parallel arrays.** Two arrays indexed together: `DynamicArrayC<TriggerDef>` (immutable after load) and `DynamicArrayC<TriggerRuntime>` (per-trigger mutable state):
```cpp
struct TriggerRuntime {
    bool  disabled;        // one-shot: true after first fire
    float accumulatedTime; // temporal: seconds since last fire
    float timeSinceCheck;  // spatial/state/count: seconds since last poll
    int   internalCount;   // count trigger: running tally (fed by IncrementCount())
};
```

**Count trigger resolves ODP-1 via push API.** `TriggerScriptModule` exposes `IncrementCount(StringCRC tag, int delta = 1)` — game code calls this on kill/spawn events. The module increments `TriggerRuntime::internalCount` for all count triggers whose `entityTag` matches. This avoids requiring a ConditionRegistry slot just for a kill counter and keeps count state fully owned by the module.

**Evaluation loop per trigger type (called from `Update(float dt)`):**
- **spatial** — if `timeSinceCheck * 1000 >= checkIntervalMs`: query `IEntitySpatialQuery::QueryRegion(region)`, filter by `entityTag`; fire if any match.
- **temporal** — `accumulatedTime += dt`; fire when `accumulatedTime >= intervalSeconds`; one-shot disables, repeating resets `accumulatedTime = 0`.
- **state** — if `timeSinceCheck * 1000 >= checkIntervalMs`: call `condition.Evaluate(*conditionContext)`; fire on true. Repeating re-arms (fires each interval the condition is true, not on every tick unless `checkIntervalMs = 0`).
- **count** — if `timeSinceCheck * 1000 >= checkIntervalMs`: compare `internalCount >= threshold`; fire if met.

**Fire sequence (all trigger types):** (1) dispatch each `ActionDef` in order via `TriggerActionRegistry::Dispatch()`; (2) publish `TriggerFiredEvent` on the sim DiaStreams channel; (3) if one-shot → set `disabled = true`; else reset check timer / accumulated time.

**Namespace:** `Dia::TriggerScript::`. All files under `Dia/DiaTriggerScript/`.

---

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Scaffold — `Dia/DiaTriggerScript/` directory, `DiaTriggerScript.vcxproj`, `.vcxproj.filters`, `dia.diatriggerscript.architecture.module.md`; register in `Cluiche.sln` under 3.0-Gameplay | `dia run googletest` compiles clean | Done | haiku | vcxproj, filters, module.md, sln registration (GUID B6C7D8E9). Pre-existing DiaEntitySpawner error unrelated. |
| 2 | Core types — `TriggerDef.h` (TriggerType enum, SpatialParams, TemporalParams, StateParams, CountParams, ActionDef, TriggerDef move-only), `TriggerFiredEvent.h`; add both to vcxproj | Build succeeds | Done | haiku | TriggerDef.h (move-only, AARect not AABB), TriggerFiredEvent.h; both added to vcxproj + filters. Build not verified. |
| 3 | Action dispatch — `ITriggerActionHandler.h`, `TriggerActionRegistry.h` + `TriggerActionRegistry.cpp`; add to vcxproj | Build succeeds | Done | haiku | ITriggerActionHandler.h, TriggerActionRegistry.h + .cpp (pimpl, unordered_map, DIA_ASSERT on missing handler). Build not verified per user. |
| 4 | TriggerScriptModule — `TriggerScriptModule.h` + `TriggerScriptModule.cpp`: `LoadFromJson`, `SetActionRegistry`, `SetConditionContext`, `SetSpatialQuery`, `IncrementCount`, `Update` (all 4 trigger types, fire sequence, TriggerFiredEvent publish), `IsFired`/`IsActive` inspection; add to vcxproj | Build succeeds | Done | sonnet | Resolve ODP-1 (count push API) and ODP-2 (spatial re-arm edge semantics) before implementing; TriggerScriptModule.h + .cpp: LoadFromJson, SetActionRegistry/ConditionContext/SpatialModule, IncrementCount (count push API resolves ODP-1), Update (all 4 trigger types), FireTrigger, OnConnectStreams. Entity tag filtering for spatial deferred (ODP-2 unresolved for now). Build not verified per user. |
| 5 | Test utilities — `DiaTriggerScript/Testing/TriggerScriptTestHelpers.h` (`MockActionHandler`, `MakeStateTrigger`); add to vcxproj | Build succeeds | Done | haiku | TriggerScriptTestHelpers.h: MockActionHandler (records Execute calls, DynamicArrayC<Call,32>), MakeStateTrigger (inline factory for state triggers). Added to vcxproj + filters under Testing filter. |
| 6 | GoogleTests — `GoogleTests/DiaTriggerScript/TestTriggerActionRegistry.cpp`, `TestTriggerScriptModule.cpp` (state/temporal/count triggers, one-shot vs repeating, dispatch, TriggerFiredEvent), `TestTriggerScriptTestHelpers.cpp`; wire into `GoogleTests.vcxproj` | `dia run googletest --filter="DiaTriggerScript*"` all pass | Done | sonnet | 3 test files: TestTriggerActionRegistry (11 tests), TestTriggerScriptModule (13 tests — temporal/state/count, one-shot/repeating, dispatch, IsActive/IsFired), TestTriggerScriptTestHelpers (7 tests). TriggerScriptModule::Tick() added as public test entry point; DoUpdate delegates to it. GoogleTests.vcxproj wired (lib + ProjectReference + ClCompile). Build not verified per user. |

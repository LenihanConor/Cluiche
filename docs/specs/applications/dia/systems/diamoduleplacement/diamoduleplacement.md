# System Spec: DiaModulePlacement

**Parent:** @docs/specs/applications/dia/dia.md
**Status:** `In Progress`
**Plan:** @docs/specs/applications/dia/systems/diamoduleplacement/diamoduleplacement.plan.md
**Origin:** 2026-11-23 architecture cleanup. `dia check arch` reported `dia.messagebus -> dia.application`; tracing it showed the cause is a module wrapper inside a service package, and that 14 module classes in `Dia/` break PD-012.

## Summary

Move every concrete application-flow module class (`SimModule`, `RenderModule`, `MainModule`, `Module` subclasses) out of the Dia packages and into the application layer, so each Dia package is a plain library again. Wiring (processing-unit affinity, bus, save, debug domains) is where games differ, and PD-012 already says it always stays in the application "as with Dia". Today it does not.

## Problem

- **PD-012 is not true of Dia.** The platform spec says wiring always stays in the application; 14 module classes live in Dia packages (list below).
- **It causes real layering damage.** A package that holds a module must depend on `DiaApplicationFlow`, so it cannot sit below the application layer. `DiaMessageBus` is a service that `dia.input` (platform) uses; its 70-line `MessageBusModule` wrapper forces an arch violation, and relabelling the package up breaks `dia.input`.
- **Two copies of the same idea.** `CluicheGameBaseline` already holds `XHostModule` wrappers around Dia libraries (`MessageBusHostModule`, `AssetServiceModule`, `EntityModule`), so the Dia-side wrappers are a second, inconsistent pattern.

## Goals

1. No class in `Dia/` derives from `SimModule`, `RenderModule`, `MainModule` or `Module`, except the framework base classes in `DiaApplicationFlow`.
2. Each moved module lands in `CluicheGameBaseline` if two or more applications (or Baseline itself) use it, or in the one application that uses it.
3. A guard keeps it that way: `dia check arch` reports **placement** violations for application wiring in any `Dia/` or `Draoi/` package. **Built 2026-11-23, ahead of the moves**, so the 12 module classes now show as errors until they move. See Guard below.
4. `dia check arch` and `dia check deps` stay green or improve; package layers are re-reviewed afterwards because several packages can drop their `DiaApplicationFlow` dependency.

## Non-Goals

1. Redesigning what any module does. Moves are mechanical: same behaviour, same `kTypeId`, same `DIA_DESCRIBE` text.
2. Moving the framework base classes (`Module`, `SimModule`, `RenderModule`, `MainModule`) out of `DiaApplicationFlow`.
3. Changing the Draoi layer, which already has no modules.

## Binding Decisions

| Decision | Source | How it constrains this system |
|---|---|---|
| PD-012: wiring always stays in the application | `docs/specs/platform/Cluiche.md` | The reason for the whole system. |
| PD-011: layers point down only | `docs/specs/platform/Cluiche.md` | Baseline and apps may depend on Dia; Dia may not depend on them. A moved module may include its library, never the reverse. |
| Escalation rule | `CLAUDE.md` | No new Dia headers are added. Files only leave Dia. |
| **MP-001 (new):** keep `kTypeId` strings and `DIA_DESCRIBE` text unchanged | this spec | `.diaapp` manifests, the describe-mode export and command help key on them. A moved module must stay manifest-compatible with no manifest edits. |
| **MP-002 (new):** destination rule | decided with the user 2026-11-23 | Shared by 2+ apps or used by Baseline -> `CluicheGameBaseline`. Used by exactly one app -> that app. |
| **MP-003 (new):** framework base classes stay in `DiaApplicationFlow`; framework modules that are not base classes move or go | decided with the user 2026-11-23 | `ObservationModule` becomes the base `ObservationModuleBase` and stays; `MetricsCollectorModule` is deleted. |
| **MP-004 (new):** tests split by what they test | decided with the user 2026-11-23 | Library tests (Bus, Impl) stay with the Dia package. Tests that drive `DoStart`/`DoUpdate`/`DoStop` move beside the module in a Baseline test folder. |

## Features

| Feature | Spec | Status |
|---|---|---|
| One feature per moved module, ordered in the plan | the plan rows | Not started; no feature specs needed, each move is mechanical |

## Architecture

### Where each module goes (MP-002)

Consumers were counted from `#include` of the module header outside its own package **and from `type_id` entries in the `.diaapp` manifests** (2026-11-23). The include count alone missed modules that self-register with `DIA_MODULE(...)` in their own `.cpp` and are loaded purely by manifest.

| Module (package, layer today) | App consumers | GoogleTests | Destination |
|---|---|---|---|
| `MessageBusModule` (DiaMessageBus, services) | Baseline host + CluicheTest subclass | 3 | **Baseline.** Fold into the existing `MessageBusHostModule`. Clears the arch violation. |
| `DiaSimTimeModule` (DiaSimTime, gameplay) | manifests: `cow_main`, `arena_test_stage`; includes: CluicheTest, CoW | 4 | **Baseline** (shared) |
| `DiaMainTime`, `DiaRenderTime` (DiaSimTime) | no manifest, no app code; named in the SimTime spec and in `ProcessingUnit` comments as the planned render-time pusher | 1 each | **Baseline**, with SimTime (kept: planned framework pieces, not dead) |
| `CellLayersModule` (DiaCellLayers, visual) | manifests: `cell_layers_viewer_test_stage`, `s1world_generation_testbed_stage` (CluicheTest and CoW) | 3 | **Baseline** (shared) |
| `DeferredStepModule` (DiaDeferredStep, application) | manifests: `cow_main`, `deferred_step_test_stage` (CoW and CluicheTest) | 3 | **Baseline** (shared). Sequence with the `DiaParallelExecutor` backlog row. |
| `AIBudgetModule` (DiaAIBudget, gameplay) | Baseline | 2 | **Baseline** |
| `ValueSchemaModule` (DiaValueSchema, assets) | **live:** manifests `cluiche_main`, `cow_main`; self-registers in Dia | 7 | **Baseline** |
| `SensorModule` (DiaSensor, gameplay) | none: no manifest, no app code, only its own package test (the whole DiaSensor package has no app consumer) | 1 | **Baseline.** Decided 2026-11-23: keep, because sensing is generic (entity, spatial and blackboard only) and Draoi cannot hold modules. Nothing loads it yet. |
| `CellHistoryModule` (DiaCellHistory, visual) | manifest: `cell_layers_viewer_test_stage` (CluicheTest) | 1 | **CluicheTest** |
| `EntitySpawnerModule` (DiaEntitySpawner, gameplay) | CluicheTest only (already subclasses it) | 2 | **CluicheTest** |
| `TriggerScriptModule` (DiaTriggerScript, gameplay) | CluicheTest only | 1 | **CluicheTest** |
| `ObservationModule` (DiaApplicationFlow) | manifests `cluiche_main`, `cow_main`, `editor`; Baseline and CluicheEditor each subclass it | 0 | **Rename the base to `ObservationModuleBase` and keep it in `DiaApplicationFlow`** (it is a framework base class, like `SimModule`). The apps' concrete `ObservationModule` classes keep their name and type id. See Open Design Question 1. |
| `MetricsCollectorModule` (DiaApplicationFlow/Metrics) | **none: dead.** No `DIA_MODULE`, no manifest, no include. Only the editor's `known_types.json` dropdown lists it. | 0 | **Delete** (Open Design Question 3, answered) |

### The move, per module

1. The Dia package keeps its plain library (Impl, registries, types, `Testing/`). The package manifest drops `dia.application` if nothing else needs it.
2. The module class moves to the destination, keeping its name, namespace alias if one exists, `kTypeId` and `DIA_DESCRIBE` (MP-001). Where the destination already has a host subclass (`MessageBusHostModule`, the CluicheTest `Testable...` subclasses) the two fold into one.
3. Includes of the old header change in apps and tests. Project files (`.vcxproj` and filters) are updated with `dia docs vcxproj-add`.
4. Module-lifecycle tests move (MP-004); library tests stay.
5. `dia check deps` and `dia check arch` run, the package layer is re-reviewed, and the registry is regenerated.

### Guard (built 2026-11-23)

`dia check arch` gains a `placement` violation kind (`arch_placement.py`). In any `Dia/` or `Draoi/` package it flags, once per kind per file:

- a class deriving from `SimModule`, `RenderModule`, `MainModule` or `Module`;
- a `DIA_MODULE(...)` registration;
- processing-unit affinity (`PUAffinity`, `kAllowedPUs`);
- an include of `DiaApplicationFlow/ProcessingUnit.h`.

Comments are ignored. Exempt: the framework package `Dia/DiaApplicationFlow` (it defines these concepts) and `Testing/` helpers. `.inl` files are scanned. The Module Tower shows one card per offending package.

Deliberately not flagged yet, because they are judgment calls: stream writers (`EventStreamWriter`, `ServiceStreamStore`), `Application.h`, `IApplicationControl`, and `DIA_STREAM_TYPE` / `DIA_DESCRIBE` on plain data types.

At the time of writing, 10 packages trip it (SimTime, ValueSchema, DeferredStep, CellLayers, CellHistory, TriggerScript, Sensor, MessageBus, EntitySpawner, AIBudget), with 30 reports in total. The gate was already red from 4 layer violations; it stays red until the moves are done. When `ObservationModule` becomes `ObservationModuleBase` it stays exempt as part of the framework package.

## Open Design Questions

All three were answered 2026-11-23; kept here as the record.

1. ~~`ObservationModule` and `CluicheEditor`~~ **Answered: it is a base class, so rename it.** `Dia::ApplicationFlow::ObservationModule` is a 182-line reusable base (log sinks, observation session, per-frame `SessionManager::Tick`). Baseline and CluicheEditor each subclass it in about 30 lines, and each subclass is itself named `ObservationModule`, which is what made it look like a concrete module. Decision: rename the Dia class to **`ObservationModuleBase`** (files `Observation/ObservationModuleBase.h/.cpp`) and keep it in `DiaApplicationFlow` beside `Module`, `SimModule` and the other framework bases. The apps' concrete classes keep the name `ObservationModule` and the type id `"ObservationModule"` (MP-001), so manifests do not change. The editor needs no Baseline reference. This amends MP-003: the base is not moved.
2. ~~`SensorModule` and the DiaSensor package~~ **Answered: keep it and move it to Baseline** (see the table).
3. ~~`MetricsCollectorModule` registration~~ **Answered: dead, delete.** It is a `MainModule` skeleton that collected FPS, frame time and memory. It was never registered or loaded, and the observability research (`observ_telemetry/ideate.md`) proposed subsuming it with the metrics registry, which then shipped. Delete it with its `.vcxproj` entries and the `known_types.json` entry in the editor assets (two copies: source and built).

## Status

In Progress. Implemented 2026-11-23 (all moves, the deletion, the rename and the guard; `dia check arch` is at 0). Open: layer re-review, Release build, E2E for moved modules, stale docs.

Was Draft. Plan written 2026-11-23. Approval gate: the user approves this spec, then `/implement`.

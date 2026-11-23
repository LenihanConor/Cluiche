# System Spec: DiaModulePlacement

**Parent:** @docs/specs/applications/dia/dia.md
**Status:** `Draft`
**Plan:** @docs/specs/applications/dia/systems/diamoduleplacement/diamoduleplacement.plan.md
**Origin:** 2026-11-23 architecture cleanup. `dia check arch` reported `dia.messagebus -> dia.application`; tracing it showed the cause is a module wrapper inside a service package, and that 14 module classes in `Dia/` break PD-012.

## Summary

Move every application-flow module class (`SimModule`, `RenderModule`, `MainModule`, `Module` subclasses) out of the Dia packages and into the application layer, so each Dia package is a plain library again. Wiring (processing-unit affinity, bus, save, debug domains) is where games differ, and PD-012 already says it always stays in the application "as with Dia". Today it does not.

## Problem

- **PD-012 is not true of Dia.** The platform spec says wiring always stays in the application; 14 module classes live in Dia packages (list below).
- **It causes real layering damage.** A package that holds a module must depend on `DiaApplicationFlow`, so it cannot sit below the application layer. `DiaMessageBus` is a service that `dia.input` (platform) uses; its 70-line `MessageBusModule` wrapper forces an arch violation, and relabelling the package up breaks `dia.input`.
- **Two copies of the same idea.** `CluicheGameBaseline` already holds `XHostModule` wrappers around Dia libraries (`MessageBusHostModule`, `AssetServiceModule`, `EntityModule`), so the Dia-side wrappers are a second, inconsistent pattern.

## Goals

1. No class in `Dia/` derives from `SimModule`, `RenderModule`, `MainModule` or `Module`, except the framework base classes in `DiaApplicationFlow`.
2. Each moved module lands in `CluicheGameBaseline` if two or more applications (or Baseline itself) use it, or in the one application that uses it.
3. A guard keeps it that way: `dia docs precommit` (or `dia check arch`) fails on a new module subclass under `Dia/`.
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
| **MP-003 (new):** move everything, including the framework's own modules | decided with the user 2026-11-23 | `ObservationModule` and `MetricsCollectorModule` move too (see Open Design Question 1). |
| **MP-004 (new):** tests split by what they test | decided with the user 2026-11-23 | Library tests (Bus, Impl) stay with the Dia package. Tests that drive `DoStart`/`DoUpdate`/`DoStop` move beside the module in a Baseline test folder. |

## Features

| Feature | Spec | Status |
|---|---|---|
| One feature per moved module, ordered in the plan | the plan rows | Not started; no feature specs needed, each move is mechanical |

## Architecture

### Where each module goes (MP-002)

Consumers were counted from `#include` of the module header outside its own package (2026-11-23).

| Module (package, layer today) | App consumers | GoogleTests | Destination |
|---|---|---|---|
| `MessageBusModule` (DiaMessageBus, services) | Baseline host + CluicheTest subclass | 3 | **Baseline.** Fold into the existing `MessageBusHostModule`. Clears the arch violation. |
| `DiaSimTimeModule` (DiaSimTime, gameplay) | CluicheTest, CoW | 4 | **Baseline** (shared) |
| `DiaMainTime`, `DiaRenderTime` (DiaSimTime) | none | 1 each | **Baseline**, with SimTime |
| `CellLayersModule` (DiaCellLayers, visual) | CluicheTest, CoW | 3 | **Baseline** (shared) |
| `DeferredStepModule` (DiaDeferredStep, application) | CluicheTest, CoW | 3 | **Baseline** (shared). Sequence with the `DiaParallelExecutor` backlog row. |
| `AIBudgetModule` (DiaAIBudget, gameplay) | Baseline | 2 | **Baseline** |
| `ValueSchemaModule` (DiaValueSchema, assets) | none | 7 | **Baseline** (no app uses it; confirm it is not dead, Open Design Question 2) |
| `SensorModule` (DiaSensor, gameplay) | none | 1 | **Baseline** (same check) |
| `CellHistoryModule` (DiaCellHistory, visual) | CluicheTest only | 1 | **CluicheTest** |
| `EntitySpawnerModule` (DiaEntitySpawner, gameplay) | CluicheTest only (already subclasses it) | 2 | **CluicheTest** |
| `TriggerScriptModule` (DiaTriggerScript, gameplay) | CluicheTest only | 1 | **CluicheTest** |
| `ObservationModule` (DiaApplicationFlow) | Baseline subclass, CluicheEditor subclass | 0 | **Baseline** (Open Design Question 1: CluicheEditor) |
| `MetricsCollectorModule` (DiaApplicationFlow/Metrics) | none found | 0 | **Baseline**, after confirming how it is registered |

### The move, per module

1. The Dia package keeps its plain library (Impl, registries, types, `Testing/`). The package manifest drops `dia.application` if nothing else needs it.
2. The module class moves to the destination, keeping its name, namespace alias if one exists, `kTypeId` and `DIA_DESCRIBE` (MP-001). Where the destination already has a host subclass (`MessageBusHostModule`, the CluicheTest `Testable...` subclasses) the two fold into one.
3. Includes of the old header change in apps and tests. Project files (`.vcxproj` and filters) are updated with `dia docs vcxproj-add`.
4. Module-lifecycle tests move (MP-004); library tests stay.
5. `dia check deps` and `dia check arch` run, the package layer is re-reviewed, and the registry is regenerated.

### Guard

A forbidden-pattern check, in `dia docs precommit` (it already runs forbidden patterns) or as an arch rule: any `class X : public ...Module` under `Dia/` outside `DiaApplicationFlow`'s framework base files fails. The exact home is decided in plan task 13.

## Open Design Questions

1. **`ObservationModule` and `CluicheEditor`.** Today `CluicheEditor` subclasses `ObservationModule` from `DiaApplicationFlow` without referencing Baseline. Moving the class to Baseline needs either a new Baseline project reference from the editor, or a second copy owned by the editor. Choose before plan task 12.
2. **Are `ValueSchemaModule`, `SensorModule`, `DiaMainTime` and `DiaRenderTime` live?** No application includes them, only tests. If a `.diaapp` manifest lists them by type id they are live and move to Baseline; if nothing does, they are candidates for deletion rather than a move. Check manifests in plan task 0.
3. **Does `MetricsCollectorModule` self-register?** No app includes its header, so something in the framework instantiates it. If it is wired inside `DiaApplicationFlow`, moving it needs a registration hook, which would be a new Dia header. That would trigger the escalation rule, so stop and ask at plan task 12.

## Status

Draft. Plan written 2026-11-23. Approval gate: the user approves this spec, then `/implement`.

# System Spec: DiaBlackboard

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** ai

## Features

> Stub system spec: the body (purpose, responsibilities, decisions) is filled in by the Blackboard Scopes feature. Core (`Blackboard`, `IBlackboardObserver`, `BlackboardComponent`, `GlobalBlackboard`) is built; see [diablackboard.plan.md](diablackboard.plan.md).

| # | Feature | Description | Spec | Status |
|---|---------|-------------|------|--------|
| 1 | Core | Typed slot store, observers, `BlackboardComponent`, `GlobalBlackboard` | [diablackboard.plan.md](diablackboard.plan.md) | Done |
| 2 | Scopes and Change Notifications *(added 2026-11-18, world-sim review)* | Parent chain with explicit `Lookup`/`HasInChain`/`FindOwner` (existing `Get`/`TryGet` stay local), constructor slot cap, board name, `Set<T>` + defaulted `OnSlotChanged`, inspector chain view | [blackboard-scopes.md](blackboard-scopes.md) | Approved |

## Status

`In Progress` (Core Done; Feature 2 Approved 2026-11-18, not built)

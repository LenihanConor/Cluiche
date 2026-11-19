# System Spec: DiaBlackboard

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** ai

## Purpose

DiaBlackboard is the shared typed-slot store for gameplay and AI systems. Each board owns typed struct slots keyed by `StringCRC`, so systems register their own data without field collisions.

## Responsibilities

- Typed slot store with local `Register`/`Get`/`TryGet`/`Has`/`Unregister`, a board name and a constructor slot cap.
- Scopes: optional parent chain with explicit `Lookup`/`HasInChain`/`FindOwner` reads.
- `Set<T>` plus defaulted `IBlackboardObserver::OnSlotChanged` for change notification on the owning board.
- Inspector reporting of name, parent and inherited slots.

## Non-goals and Constraints

- Thread safety: boards are used on SimPU only.
- Parent changes are not propagated to child observers.
- A parent must outlive its children; chains are capped at 8 boards with no cycles.
- `GlobalBlackboard` stays as debt, unused by new code. CoW world/game boards belong to the World Step / CoW amendment, not here.

## Features

| # | Feature | Description | Spec | Status |
|---|---------|-------------|------|--------|
| 1 | Core | Typed slot store, observers, `BlackboardComponent`, `GlobalBlackboard` | [diablackboard.plan.md](diablackboard.plan.md) | Done |
| 2 | Scopes and Change Notifications *(added 2026-11-18, world-sim review)* | Parent chain with explicit `Lookup`/`HasInChain`/`FindOwner` (existing `Get`/`TryGet` stay local), constructor slot cap, board name, `Set<T>` + defaulted `OnSlotChanged`, inspector chain view | [blackboard-scopes.md](blackboard-scopes.md) | Done |

## Status

`Done` (Core and Scopes Done 2026-11-18)

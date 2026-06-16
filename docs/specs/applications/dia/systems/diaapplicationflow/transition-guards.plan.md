# Plan: Transition Guards

**Spec:** @docs/specs/applications/dia/systems/diaapplicationflow/transition-guards.md
**Status:** Done
**Started:** 2026-05-21

## Session Notes

Implementing transition guards for DiaApplicationFlow. The spec adds a veto-on-transition hook so modules can hold pending stage transitions (primary consumer: RemoteControlModule for E2E orchestration).

**Spec decisions summary (Platform → System):**
- PD-001: StringCRC for IDs. Guard owners tracked by `Module*` pointer; lifecycle events use StringCRC stage fields.
- PD-004: No STL containers in public APIs. Use `DynamicArrayC<GuardEntry, 8>` for guard registry.
- SD-005: Transitions queued, execute next frame. Guards run inside `ApplyPendingTransition` before consuming the pending transition.
- SD-017: Clean break. Zero guards = today's behaviour (AC8).
- Guards run on main thread only. No locks needed on `mGuards` itself.
- Fixed capacity kMaxGuards=8. Asserts in debug, returns false in release on overflow.
- `mPendingHeldByGuardEmitted` prevents per-frame event spam — re-armed on commit or new target.
- `GetTransitionInfo().heldByGuards` uses cached last result to avoid calling guards from inspectable getter.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | TDD-RED: write `TestTransitionGuards.cpp` covering AC1–AC12. Run `dia run googletest --filter="TransitionGuards*"` and quote failing output. | All AC1–AC12 fail to compile | Done | sonnet | TDD red gate confirmed |
| 2 | Add declarations to headers: `GuardResult`, `TransitionGuardFn`, `RegisterTransitionGuard`, `UnregisterTransitionGuards`, `GuardEntry`, `mGuards`, `mPendingHeldByGuardEmitted`, `CheckGuards()` to `Application.h`; `heldByGuards` to `TransitionInfo`; `kStageTransitionHeldByGuard` to `LifecycleEvent.h`. | Compiles | Done | haiku | |
| 3 | Implement `RegisterTransitionGuard`, `UnregisterTransitionGuards`, `CheckGuards` in `Application.cpp`; wire members. | AC1, AC2, AC9 pass | Done | sonnet | |
| 4 | Update `ApplyPendingTransition` to peek-then-guard-check-then-consume; emit `kStageTransitionHeldByGuard` once per held-pending; update `TransitionTo` to clear emit flag on new target. | AC3, AC4, AC5, AC6, AC11, AC12 pass | Done | sonnet | |
| 5 | Update `GetTransitionInfo` to populate `heldByGuards` (use cached result from last frame's guard check). | AC10 pass | Done | haiku | |
| 6 | Add `TestTransitionGuards.cpp` to `GoogleTests.vcxproj`. | Builds | Done | haiku | |
| 7 | Run `dia run googletest`; confirm AC1–AC12 pass and existing tests pass. Run `dia run cluichetest`. Quote output. | AC8, AC13 verification gate | Done | sonnet | 5174 tests pass; CluicheTest passes |
| 8 | Add SD-020 to `diaapplicationflow.md` Decisions table; add Transition Guards row to Features table. Update `dia.applicationflow.architecture.module.md`. Commit. | Doc + git | Done | haiku | |

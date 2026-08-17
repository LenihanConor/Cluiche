**Spec:** @docs/specs/applications/dia/systems/diagridvisibility/diagridvisibility.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Project scaffold: Dia/DiaGridVisibility/ directory, DiaGridVisibility.vcxproj + .vcxproj.filters, add to Cluiche.sln | `dia run googletest` builds with new project | Done | haiku | 8520 tests pass |
| 2 | Core type headers: VisibilityGroupId.h, VisibilityState.h, VisibilityLogChannel.h, IVisibilityChangeObserver.h | Headers compile in googletest | Done | sonnet | 8520 tests pass ‖ parallel with Task 9 |
| 3 | GridVisibilitySystem class: full header (GridVisibilitySystem.h), internal state types (per-group grid, dirty flags, sight-source registry, visible-entity list), constructor with chunk-grid consolidation pass, RegisterSightSource/UnregisterSightSource | Class compiles | Done | opus | GridVisibilitySystem.h: CVisibilityGraph concept, SightSourceEntry/GroupState, chunk-consolidation ctor, Register/UnregisterSightSource. Update/queries stubbed for Tasks 4-5. 8535 tests pass (15 new). |
| 4 | Update pass: recursive-octant shadowcasting per dirty source, merge to per-group grid, Visible→Revealed decay, visible-entity list rebuild via DiaEntitySpatial, IVisibilityChangeObserver callbacks fired | Unit tests pass | Pending | opus | ‖ parallel with Task 6 |
| 5 | Query methods + listener registration: CanSee, GetCellState, GetVisibleEntities, AddChangeListener, RemoveChangeListener | Tests pass | Pending | sonnet | after Task 4 |
| 6 | Test utilities: MockVisibilityGraph + assert helpers in DiaGridVisibility/Testing/VisibilityTestHelpers.h | Compile in googletest | Pending | sonnet | ‖ parallel with Task 4 |
| 7 | GoogleTest suite: DiaGridVisibility/ test directory + core test files (types, registration, queries, observer callbacks); add to GoogleTests.vcxproj + AdditionalDependencies | All tests pass | Pending | sonnet | after Tasks 4+5+6 |
| 8 | Exhaustive tests: shadowcasting boundary/golden/invariant/stress tests — wall blocking, chunk-size variants, multi-group isolation, large-grid stress, decay correctness, dirty-flag skip-when-stationary | All tests pass | Pending | sonnet | after Task 7 |
| 9 | Module doc: Docs/dia.gridvisibility.architecture.module.md | Registry entry correct | Done | haiku | ‖ parallel with Task 2 |

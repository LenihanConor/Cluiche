**Spec:** @docs/specs/applications/dia/systems/diaorder/diaorder.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create core library headers and sources: `IOrder.h`, `IOrderQueueObserver.h`, `OrderQueue.h`, `OrderQueue.inl`, `OrderQueue.cpp`, `OrderMessage.h`, `OrderQueueComponent.h`, `OrderQueueComponent.cpp` under `Dia/DiaOrder/` | Build compiles; grep confirms all public API symbols present | Done | sonnet | All 8 files created |
| 2 | Create `DiaOrder.vcxproj`, `DiaOrder.vcxproj.filters`, `Docs/dia.order.architecture.module.md` under `Dia/DiaOrder/` | `dia pipeline --target googletest` builds DiaOrder.lib | Done | haiku | 6 project refs (DiaCore, DiaObservation, DiaMailbox, DiaEntity, DiaBlackboard, DiaApplicationFlow) |
| 3 | Register `DiaOrder` in `Cluiche.sln` under `3.0-Gameplay` folder | Solution loads in VS; DiaOrder appears in solution explorer under Dia/3.0-Gameplay | Done | haiku | Added project entry, config platforms, and NestedProjects entry |
| 4 | Create test utilities `Dia/DiaOrder/Testing/OrderTestHelpers.h` (`MockOrder<TContext>`, `AssertOrderPending`, `AssertQueueEmpty`) | Included in test file; compiles | Done | sonnet | MockOrder + MockOrderQueueObserver + 2 assert helpers |
| 5 | Create `Cluiche/Tests/GoogleTests/DiaOrder/TestOrderQueue.cpp` and wire into GoogleTests.vcxproj + add `DiaOrder.lib` to linker deps and `ProjectReference` | `dia run googletest --filter="DiaOrder*"` all pass | Done | sonnet | 36/36 tests pass |
| 6 | [order-observer-bus-adapter](order-observer-bus-adapter.md) — bridging pattern only; blocked until a concrete `OrderQueue<TContext>` consumer exists | — | Blocked | — | Tracked in [diamessagebus.plan.md](../diamessagebus/diamessagebus.plan.md) Phase 8, task 15. Grep for `OrderQueue<` before starting |

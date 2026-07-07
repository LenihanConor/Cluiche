# System Spec: DiaOrder

## Parent Application
@docs/specs/applications/dia/dia.md

## Purpose

DiaOrder is the order queue and action system for the Dia engine. It provides a generic, reusable primitive for issuing discrete units of work to entities in a sequenced, cancellable manner.

An **order** is a self-contained action with a four-phase lifecycle (Start → Update → Finish | Cancel). An **order queue** holds an ordered sequence of orders for one entity, executing them one at a time. AI systems, ability systems, player input, and scripted sequences all express entity intent as orders — the queue serialises concurrent requests and provides a clean cancellation path.

The system is intentionally a primitive: it owns sequencing and lifecycle, not entity resolution or mailbox wiring. Cross-PU order posting is handled by `OrderMessage` routed via DiaMailbox; the owning Module drains that mailbox and calls `Enqueue()`.

**Dependency chain:**
`DiaOrder → DiaCore (containers, StringCRC, Observer)`
`DiaOrder → DiaMailbox (OrderMessage type only — no internal drain)`

## Responsibilities

- Provide `IOrder<TContext>` — interface with `Start`, `Update`, `Finish`, and `Cancel` callbacks; `Update` returns bool (true = done)
- Provide `OrderQueue<TContext>` — owns a FIFO sequence of `IOrder<TContext>` pointers; executes one at a time; ticks via `Update(TContext&, float dt)`
- Support `Enqueue(IOrder<TContext>*)` — append to back of queue
- Support `EnqueueFront(IOrder<TContext>*)` — insert at front (urgent orders: Stop, Dodge, Death reactions); cancels the current order first
- Support `Clear()` — cancel current order and drain the queue
- Support `Cancel()` — cancel current order only; queue continues with next
- Support `IsEmpty()`, `GetQueueDepth()`, `GetCurrentOrder()` for inspection
- Provide `IOrderQueueObserver<TContext>` — notified on `OnOrderStarted`, `OnOrderFinished`, `OnOrderCancelled`, `OnQueueEmpty`; uses `Dia::Core::Observer`
- Provide `OrderQueueComponent` — `IComponent` wrapper owning a `OrderQueue<EntityOrderContext>`; registered via `ComponentFactoryRegistry`
- Define `OrderMessage` — a `DiaMailbox`-routable message type carrying an order pointer and target entity handle; enables cross-PU order posting without direct queue reference
- Provide `OrderQueueModule` — a `DiaApplicationFlow` Module that owns `OrderQueueComponent` draining; drains `Mailbox<OrderMessage>` each tick and calls `Enqueue()` on the target queue
- Emit `DIA_LOG_INFO` on order start, finish, and cancel — keyed by order `StringCRC` id
- Provide test utilities under `DiaOrder/Testing/`: `AssertOrderPending`, `AssertQueueEmpty`, `MockOrder<TContext>` — shipped with the library, consumer opt-in via include
- Identify all order types via `StringCRC kOrderId` constant on each order class (PD-001)
- Use DiaCore containers exclusively in all public APIs (AD-002 / PD-004)
- Provide `dia.order.architecture.module.md` YAML module documentation
- Provide `DiaOrder.vcxproj` static library registered in `Cluiche.sln` under `domain/gameplay/core`

## Non-Responsibilities

- Entity ownership or lifecycle — orders operate on entities via TContext; they do not own them
- Parallel order execution — one order runs at a time per queue; parallelism is caller's concern (multiple queues per entity is valid)
- Order serialisation or save/load — out of scope for this module
- Priority-based preemption — `EnqueueFront` covers urgent cases; full priority levels deferred
- Pause/Resume of individual orders — `DiaStateMachine`'s PushdownAutomaton covers interrupt-and-resume at a higher level
- Thread safety within order callbacks — single-threaded per queue; caller synchronises across PUs via `OrderMessage` / DiaMailbox
- Visual debugger widget — deferred to a future `DiaOrderVisualDebugger` system
- AI decision-making — orders are *instructions*; which order to issue is decided by DiaBlackboard + decision systems

## Public Interfaces

### IOrder

```cpp
namespace Dia::Order {
    template<typename TContext>
    struct IOrder {
        virtual ~IOrder() = default;

        virtual Dia::Core::StringCRC GetOrderId() const = 0;

        virtual void   Start(TContext& ctx) = 0;
        virtual bool   Update(TContext& ctx, float dt) = 0; // true = finished
        virtual void   Finish(TContext& ctx) = 0;
        virtual void   Cancel(TContext& ctx) = 0;
    };
}
```

### OrderQueue

```cpp
namespace Dia::Order {
    template<typename TContext>
    class OrderQueue {
    public:
        // Enqueue
        void Enqueue(IOrder<TContext>* order);          // append to back
        void EnqueueFront(IOrder<TContext>* order);     // insert at front; cancels current

        // Queue control
        void Clear();                                   // cancel current + drain all
        void Cancel();                                  // cancel current only; continue queue

        // Tick — call once per frame from owning module
        void Update(TContext& ctx, float dt);

        // Inspection
        bool IsEmpty() const;
        int  GetQueueDepth() const;
        const IOrder<TContext>* GetCurrentOrder() const;

        // Observer
        void AddObserver(IOrderQueueObserver<TContext>& observer);
        void RemoveObserver(IOrderQueueObserver<TContext>& observer);
    };
}
```

### IOrderQueueObserver

```cpp
namespace Dia::Order {
    template<typename TContext>
    class IOrderQueueObserver {
    public:
        virtual ~IOrderQueueObserver() = default;

        virtual void OnOrderStarted(const IOrder<TContext>& order)   {}
        virtual void OnOrderFinished(const IOrder<TContext>& order)  {}
        virtual void OnOrderCancelled(const IOrder<TContext>& order) {}
        virtual void OnQueueEmpty()                                   {}
    };
}
```

### OrderMessage

```cpp
namespace Dia::Order {
    // DiaMailbox-routable message for cross-PU order posting.
    // The owning OrderQueueModule drains Mailbox<OrderMessage> each tick
    // and calls Enqueue() / EnqueueFront() on the target queue.
    struct OrderMessage {
        EntityHandle               target;
        IOrder<void*>*             order;      // type-erased; module casts to concrete TContext
        bool                       urgent;     // true = EnqueueFront, false = Enqueue
    };
}
```

### OrderQueueComponent

```cpp
namespace Dia::Order {
    // Default concrete context for entity-bound queues.
    struct EntityOrderContext {
        EntityHandle  entity;
        Blackboard&   board;     // entity's blackboard (DiaBlackboard)
    };

    class OrderQueueComponent : public Dia::Core::IComponent {
    public:
        static constexpr Dia::Core::StringCRC kUniqueId{"OrderQueueComponent"};

        OrderQueue<EntityOrderContext>& GetQueue();
        const OrderQueue<EntityOrderContext>& GetQueue() const;
    };
}
```

### Log Channel

```cpp
namespace Dia::Order {
    static constexpr Dia::Core::StringCRC kLogChannel{"Order"};
    // DIA_LOG_INFO on Start / Finish / Cancel with order StringCRC id.
}
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Order Interface | `IOrder<TContext>` — `Start` / `Update` / `Finish` / `Cancel` lifecycle, `GetOrderId()` StringCRC identity. | inline | Draft |
| Order Queue | `OrderQueue<TContext>` — FIFO execution, `Enqueue` / `EnqueueFront` / `Clear` / `Cancel`, single-order-at-a-time `Update` tick. | inline | Draft |
| Queue Observer | `IOrderQueueObserver<TContext>` — `OnOrderStarted`, `OnOrderFinished`, `OnOrderCancelled`, `OnQueueEmpty` via `Dia::Core::Observer`. | inline | Draft |
| OrderQueueComponent | `IComponent` wrapper owning a `OrderQueue<EntityOrderContext>`; registered via `ComponentFactoryRegistry`. | inline | Draft |
| OrderMessage | `DiaMailbox`-routable struct for cross-PU order posting. Owning module drains and enqueues. | inline | Draft |
| OrderQueueModule | `DiaApplicationFlow` Module that drains `Mailbox<OrderMessage>` and ticks all `OrderQueueComponent` instances each frame. | inline | Draft |
| Lifecycle Logging | `DIA_LOG_INFO` on order Start / Finish / Cancel with order id. | inline | Draft |
| Test Utilities | `DiaOrder/Testing/` — `AssertOrderPending`, `AssertQueueEmpty`, `MockOrder<TContext>`. Ships with library; consumer opt-in via include. | inline | Draft |

## Dependencies on Other Systems

**Required:**
- **DiaCore** — `StringCRC` (order identity), `DynamicArrayC` (queue storage, observer list), `IComponent` / `IComponentObject` / `ComponentFactoryRegistry` (entity integration), `Observer` / `ObserverSubject` (queue events), `DIA_ASSERT`, `DIA_LOG_*`, `EntityHandle`
- **DiaMailbox** — `OrderMessage` type registration; `Mailbox<OrderMessage>` used by `OrderQueueModule` for cross-PU posting
- **DiaApplicationFlow** — `OrderQueueModule` is a Module in the PU/Phase/Module hierarchy

**Depends on (at game layer — not compile-time):**
- **DiaBlackboard** — `EntityOrderContext` carries a `Blackboard&`; orders read/write entity blackboard slots

**Explicitly excluded:**
- **DiaStateMachine** — no dependency; AI systems that use both wire them at the game layer
- **DiaStreams** — lifecycle events go through Observer; no stream overhead for what are synchronous calls
- **DiaMaths / DiaGeometry2D** — orders are agnostic to spatial data; callers encode targets as handles or blackboard lookups

**Dependents (future):**
- `DiaOrder` consumers: ability system (C16), aggro/engagement (C37), rally/waypoint (C24), squad/formation (C23)
- AI decision systems (DiaBlackboard, DiaUtilityAI, DiaBehaviourTree) issue orders as their output

## Out of Scope

- Parallel order execution per queue
- Priority-based preemption (full priority levels)
- Pause/Resume of individual orders
- Order serialisation / save-load
- Thread-safe queue access — caller responsibility; cross-PU uses `OrderMessage` / DiaMailbox
- Visual debugger widget — deferred to `DiaOrderVisualDebugger`

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| OD-001 | Four-phase lifecycle: Start / Update / Finish / Cancel | Finish and Cancel are separate paths — completed orders trigger arrival callbacks; cancelled orders just stop. Conflating them forces callers to distinguish internally. Pause/Resume deferred — PushdownAutomaton covers interrupt-and-resume at higher level. | Order Interface | Accepted | Yes |
| OD-002 | Caller-defined `TContext` template, not hard-wired entity/blackboard | Zero coupling to entity system. Fully testable with a plain struct. `EntityOrderContext` is the default concrete context; callers providing other contexts (e.g. unit tests) pay no overhead. Same pattern as `FlatStateMachine<TContext>`. | Order Queue | Accepted | Yes |
| OD-003 | FIFO + `EnqueueFront` for urgent orders | Pure FIFO is too rigid (Stop, Dodge, Death reactions need immediate execution). Full priority levels add complexity without clear payoff at this stage. `EnqueueFront` cancels the current order first, then inserts — one extra method, zero priority ambiguity. | Order Queue | Accepted | Yes |
| OD-004 | `OrderMessage` type only — no built-in mailbox drain inside `OrderQueue` | `OrderQueue` stays a pure primitive (no DiaMailbox dependency). The owning `OrderQueueModule` drains `Mailbox<OrderMessage>` each tick and calls `Enqueue()`. Follows the Dia pattern: modules own mailbox wiring, primitives stay decoupled. Same-PU callers call `Enqueue()` directly. | OrderMessage, OrderQueueModule | Accepted | Yes |
| OD-005 | Observer for queue lifecycle events; no streams | Queue events are synchronous and same-thread. Observer is zero overhead when no listeners. Stream overhead not justified for what are infrequent lifecycle notifications (order start/finish/cancel). | Queue Observer | Accepted | Yes |
| OD-006 | `Update()` returns bool (true = done); queue auto-advances | Order signals completion by returning true from `Update`. Queue calls `Finish()`, then pops and starts the next order in the same tick. No explicit "done" state enum — keeps the interface minimal. | Order Interface, Order Queue | Accepted | Yes |
| OD-007 | `EnqueueFront` cancels current order before inserting | Urgent orders need a clean slate. Calling `Cancel()` on the displaced order gives it a chance to clean up. The displaced order is *not* re-queued — it is lost. Callers that need the old queue preserved should snapshot it first. | Order Queue | Accepted | Yes |
| OD-008 | Test utilities ship inside `DiaOrder/Testing/` | Platform-wide pattern (established by DiaStateMachine SD-017, DiaBlackboard BD-008). Helpers live in the library; consumers opt in via include. | Test Utilities | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | All order type IDs (`kOrderId`), `OrderQueueComponent::kUniqueId`, and log channel keys use `StringCRC`. |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | `OrderQueueModule` is a Module in the PU/Phase/Module hierarchy. `OrderQueue` itself has no dependency on DiaApplicationFlow. |
| PD-003 | Platform | Component-based entities | `OrderQueueComponent` implements `IComponent`; registered via `ComponentFactoryRegistry`. |
| PD-004 | Platform | No STL containers in public APIs | Queue storage and observer list use `DiaCore::DynamicArrayC`. Internal implementation may use STL. |
| PD-005 | Platform | x64 only | `DiaOrder.vcxproj` targets x64 exclusively. |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaOrder.vcxproj` and `.vcxproj.filters` created and maintained manually. |
| PD-007 | Platform | C++20 required | Compiled under `/std:c++20`. Template `OrderQueue<TContext>` and `IOrder<TContext>` can use concepts to constrain context types. |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaOrder.vcxproj` must NOT override `OutDir`, `IntDir`, `PlatformToolset`, `WindowsTargetPlatformVersion`, or `LanguageStandard`. |
| PD-009 | Platform | Generated output under Cluiche/out/ | Any log output goes under `Cluiche/out/<AppName>/`. |
| AD-001 | Dia App | Module system with YAML frontmatter | Create `dia.order.architecture.module.md` with public API, responsibilities, and dependency declarations. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::Order::` namespace. |
| AD-005 | Dia App | Component-based entities | Reinforces PD-003 for `OrderQueueComponent`. |

## Status

`Done`

**Plan:** @docs/specs/applications/dia/systems/diaorder/diaorder.plan.md

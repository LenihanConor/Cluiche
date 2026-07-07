---
schema: dia.module.v1
module_id: dia.order
name: DiaOrder
owner_team: TBD
layer: domain/gameplay/core
status: active
maturity: dev

path: Dia/DiaOrder
language: cpp
parent_module_id: dia.root

summary: >
  Order queue and action system — IOrder<TContext> lifecycle interface,
  OrderQueue<TContext> FIFO execution with observer notifications, and
  OrderQueueComponent for entity-bound queues.

intent: >
  Provides a generic, reusable primitive for issuing discrete units of work
  to entities in a sequenced, cancellable manner. AI systems, ability systems,
  player input, and scripted sequences express entity intent as orders;
  the queue serialises concurrent requests and provides a clean cancellation path.

responsibilities:
  - IOrder<TContext> lifecycle interface (Start/Update/Finish/Cancel)
  - OrderQueue<TContext> FIFO execution with Enqueue/EnqueueFront/Clear/Cancel
  - IOrderQueueObserver<TContext> lifecycle event notifications
  - OrderQueueComponent IComponent wrapper for entity attachment
  - OrderMessage struct for cross-PU order posting via DiaMailbox
  - DIA_LOG_INFO on order Start / Finish / Cancel
  - Test utilities under Testing/ subdirectory

non_responsibilities:
  - Entity ownership or lifecycle
  - Parallel order execution
  - Order serialisation or save/load
  - Priority-based preemption beyond EnqueueFront
  - Thread safety within order callbacks
  - Visual debugger widget

dependent_modules: []

public_api:
  headers:
    - Dia/DiaOrder/IOrder.h
    - Dia/DiaOrder/IOrderQueueObserver.h
    - Dia/DiaOrder/OrderQueue.h
    - Dia/DiaOrder/OrderMessage.h
    - Dia/DiaOrder/OrderQueueComponent.h
  namespaces:
    - Dia::Order
  entry_points:
    - IOrder
    - OrderQueue
    - IOrderQueueObserver
    - OrderMessage
    - OrderQueueComponent
    - EntityOrderContext

dependencies:
  required:
    - dia.core
    - dia.observation
    - dia.mailbox
    - dia.entity
    - dia.blackboard
    - dia.application
  forbidden:
    - dia.statemachine
    - dia.streams
    - dia.graphics
    - dia.maths
---

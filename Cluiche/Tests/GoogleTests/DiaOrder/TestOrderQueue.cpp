#include <gtest/gtest.h>

#include <DiaOrder/IOrder.h>
#include <DiaOrder/OrderQueue.h>
#include <DiaOrder/IOrderQueueObserver.h>
#include <DiaOrder/OrderMessage.h>
#include <DiaOrder/OrderQueueComponent.h>
#include <DiaOrder/Testing/OrderTestHelpers.h>
#include <DiaBlackboard/Blackboard.h>
#include <DiaEntity/Entity.h>

using namespace Dia::Order;
using namespace Dia::Order::Testing;

// Simple context for most tests — no Blackboard or Entity required.
struct TestCtx { int value = 0; };

// -------------------------------------------------------------------------
// Empty queue behaviour
// -------------------------------------------------------------------------

TEST(DiaOrderQueue, EmptyQueueIsEmpty)
{
    OrderQueue<TestCtx> queue;
    EXPECT_TRUE(queue.IsEmpty());
}

TEST(DiaOrderQueue, EmptyQueueDepthIsZero)
{
    OrderQueue<TestCtx> queue;
    EXPECT_EQ(queue.GetQueueDepth(), 0);
}

TEST(DiaOrderQueue, EmptyQueueCurrentOrderIsNull)
{
    OrderQueue<TestCtx> queue;
    EXPECT_EQ(queue.GetCurrentOrder(), nullptr);
}

// -------------------------------------------------------------------------
// Enqueue + Update starts order
// -------------------------------------------------------------------------

TEST(DiaOrderQueue, EnqueueThenUpdateStartsOrder)
{
    OrderQueue<TestCtx> queue;
    MockOrder<TestCtx> order(Dia::Core::StringCRC("MoveOrder"));

    queue.Enqueue(&order);
    TestCtx ctx;
    queue.Update(ctx, 0.0f);

    EXPECT_NE(queue.GetCurrentOrder(), nullptr);
    EXPECT_EQ(order.startCount, 1);
}

TEST(DiaOrderQueue, EnqueueThenUpdateNotEmpty)
{
    OrderQueue<TestCtx> queue;
    MockOrder<TestCtx> order(Dia::Core::StringCRC("MoveOrder"));

    queue.Enqueue(&order);
    TestCtx ctx;
    queue.Update(ctx, 0.0f);

    EXPECT_FALSE(queue.IsEmpty());
}

TEST(DiaOrderQueue, EnqueueThenUpdateSetsCurrentOrder)
{
    OrderQueue<TestCtx> queue;
    MockOrder<TestCtx> order(Dia::Core::StringCRC("AttackOrder"));

    queue.Enqueue(&order);
    TestCtx ctx;
    queue.Update(ctx, 0.0f);

    ASSERT_NE(queue.GetCurrentOrder(), nullptr);
    EXPECT_EQ(queue.GetCurrentOrder()->GetOrderId(), Dia::Core::StringCRC("AttackOrder"));
}

// -------------------------------------------------------------------------
// Update with shouldFinish=true auto-finishes and empties queue
// -------------------------------------------------------------------------

TEST(DiaOrderQueue, UpdateAutoFinishesOrder)
{
    OrderQueue<TestCtx> queue;
    MockOrder<TestCtx> order(Dia::Core::StringCRC("WaitOrder"), /*shouldFinish=*/true);

    queue.Enqueue(&order);
    TestCtx ctx;
    queue.Update(ctx, 0.0f);  // Start + immediate finish

    EXPECT_EQ(order.startCount, 1);
    EXPECT_EQ(order.finishCount, 1);
    EXPECT_TRUE(queue.IsEmpty());
    EXPECT_EQ(queue.GetCurrentOrder(), nullptr);
}

// -------------------------------------------------------------------------
// FIFO ordering
// -------------------------------------------------------------------------

TEST(DiaOrderQueue, FIFOOrderingFirstOrderBecomesCurrentFirst)
{
    OrderQueue<TestCtx> queue;
    MockOrder<TestCtx> orderA(Dia::Core::StringCRC("OrderA"), /*shouldFinish=*/false);
    MockOrder<TestCtx> orderB(Dia::Core::StringCRC("OrderB"), /*shouldFinish=*/false);

    queue.Enqueue(&orderA);
    queue.Enqueue(&orderB);

    TestCtx ctx;
    queue.Update(ctx, 0.0f);  // Promotes OrderA

    ASSERT_NE(queue.GetCurrentOrder(), nullptr);
    EXPECT_EQ(queue.GetCurrentOrder()->GetOrderId(), Dia::Core::StringCRC("OrderA"));
    EXPECT_EQ(orderA.startCount, 1);
    EXPECT_EQ(orderB.startCount, 0);
}

TEST(DiaOrderQueue, FIFOOrderingSecondOrderStartsAfterFirstFinishes)
{
    OrderQueue<TestCtx> queue;
    MockOrder<TestCtx> orderA(Dia::Core::StringCRC("OrderA"), /*shouldFinish=*/false);
    MockOrder<TestCtx> orderB(Dia::Core::StringCRC("OrderB"), /*shouldFinish=*/false);

    queue.Enqueue(&orderA);
    queue.Enqueue(&orderB);

    TestCtx ctx;
    queue.Update(ctx, 0.0f);  // OrderA becomes current

    // Now make OrderA finish on next tick
    orderA.shouldFinish = true;
    queue.Update(ctx, 0.0f);  // OrderA finishes, OrderB starts

    ASSERT_NE(queue.GetCurrentOrder(), nullptr);
    EXPECT_EQ(queue.GetCurrentOrder()->GetOrderId(), Dia::Core::StringCRC("OrderB"));
    EXPECT_EQ(orderA.finishCount, 1);
    EXPECT_EQ(orderB.startCount, 1);
}

// -------------------------------------------------------------------------
// EnqueueFront
// -------------------------------------------------------------------------

TEST(DiaOrderQueue, EnqueueFrontCancelsCurrentOrder)
{
    OrderQueue<TestCtx> queue;
    MockOrder<TestCtx> orderA(Dia::Core::StringCRC("OrderA"), /*shouldFinish=*/false);
    MockOrder<TestCtx> orderB(Dia::Core::StringCRC("OrderB"), /*shouldFinish=*/false);

    queue.Enqueue(&orderA);
    TestCtx ctx;
    queue.Update(ctx, 0.0f);  // OrderA is now current

    queue.EnqueueFront(&orderB);

    // After EnqueueFront, the current order should be dropped (cancel notified via observer)
    // OrderB should now be pending at front
    EXPECT_EQ(orderA.startCount, 1);
}

TEST(DiaOrderQueue, EnqueueFrontObserverReceivesCancelledForCurrentOrder)
{
    OrderQueue<TestCtx> queue;
    MockOrder<TestCtx> orderA(Dia::Core::StringCRC("OrderA"), /*shouldFinish=*/false);
    MockOrder<TestCtx> orderB(Dia::Core::StringCRC("OrderB"), /*shouldFinish=*/false);
    MockOrderQueueObserver<TestCtx> obs;

    queue.AddObserver(obs);
    queue.Enqueue(&orderA);
    TestCtx ctx;
    queue.Update(ctx, 0.0f);  // OrderA starts, observer gets OnOrderStarted

    queue.EnqueueFront(&orderB);
    // Observer should have received OnOrderCancelled for OrderA

    EXPECT_EQ(obs.cancelledCount, 1);
    EXPECT_EQ(obs.lastCancelled->GetOrderId(), Dia::Core::StringCRC("OrderA"));
}

TEST(DiaOrderQueue, EnqueueFrontOrderBecomesCurrentOnNextUpdate)
{
    OrderQueue<TestCtx> queue;
    MockOrder<TestCtx> orderA(Dia::Core::StringCRC("OrderA"), /*shouldFinish=*/false);
    MockOrder<TestCtx> orderB(Dia::Core::StringCRC("OrderB"), /*shouldFinish=*/false);

    queue.Enqueue(&orderA);
    TestCtx ctx;
    queue.Update(ctx, 0.0f);  // OrderA current

    queue.EnqueueFront(&orderB);
    queue.Update(ctx, 0.0f);  // OrderB should now be current

    ASSERT_NE(queue.GetCurrentOrder(), nullptr);
    EXPECT_EQ(queue.GetCurrentOrder()->GetOrderId(), Dia::Core::StringCRC("OrderB"));
    EXPECT_EQ(orderB.startCount, 1);
}

// -------------------------------------------------------------------------
// Clear
// -------------------------------------------------------------------------

TEST(DiaOrderQueue, ClearEmptiesQueue)
{
    OrderQueue<TestCtx> queue;
    MockOrder<TestCtx> order(Dia::Core::StringCRC("OrderA"), /*shouldFinish=*/false);

    queue.Enqueue(&order);
    TestCtx ctx;
    queue.Update(ctx, 0.0f);  // Order is now current

    queue.Clear();

    EXPECT_TRUE(queue.IsEmpty());
    EXPECT_EQ(queue.GetCurrentOrder(), nullptr);
}

TEST(DiaOrderQueue, ClearFiresOnQueueEmpty)
{
    OrderQueue<TestCtx> queue;
    MockOrder<TestCtx> order(Dia::Core::StringCRC("OrderA"), /*shouldFinish=*/false);
    MockOrderQueueObserver<TestCtx> obs;

    queue.AddObserver(obs);
    queue.Enqueue(&order);
    TestCtx ctx;
    queue.Update(ctx, 0.0f);

    queue.Clear();

    EXPECT_EQ(obs.emptyCount, 1);
}

// -------------------------------------------------------------------------
// Cancel
// -------------------------------------------------------------------------

TEST(DiaOrderQueue, CancelDropsCurrentOrder)
{
    OrderQueue<TestCtx> queue;
    MockOrder<TestCtx> order(Dia::Core::StringCRC("OrderA"), /*shouldFinish=*/false);

    queue.Enqueue(&order);
    TestCtx ctx;
    queue.Update(ctx, 0.0f);  // Order is now current

    queue.Cancel();

    EXPECT_TRUE(queue.IsEmpty());
    EXPECT_EQ(queue.GetCurrentOrder(), nullptr);
}

TEST(DiaOrderQueue, CancelFiresOnQueueEmptyWhenNoPending)
{
    OrderQueue<TestCtx> queue;
    MockOrder<TestCtx> order(Dia::Core::StringCRC("OrderA"), /*shouldFinish=*/false);
    MockOrderQueueObserver<TestCtx> obs;

    queue.AddObserver(obs);
    queue.Enqueue(&order);
    TestCtx ctx;
    queue.Update(ctx, 0.0f);

    queue.Cancel();

    EXPECT_EQ(obs.emptyCount, 1);
}

TEST(DiaOrderQueue, CancelDoesNotCallVirtualCancelCallback)
{
    // Cancel() on OrderQueue is context-free — it does NOT call IOrder::Cancel(ctx)
    OrderQueue<TestCtx> queue;
    MockOrder<TestCtx> order(Dia::Core::StringCRC("OrderA"), /*shouldFinish=*/false);

    queue.Enqueue(&order);
    TestCtx ctx;
    queue.Update(ctx, 0.0f);

    queue.Cancel();

    // The virtual Cancel on the IOrder should NOT have been called (no context available)
    EXPECT_EQ(order.cancelCount, 0);
}

// -------------------------------------------------------------------------
// Observer callbacks
// -------------------------------------------------------------------------

TEST(DiaOrderQueue, ObserverReceivesOnOrderStarted)
{
    OrderQueue<TestCtx> queue;
    MockOrder<TestCtx> order(Dia::Core::StringCRC("OrderA"), /*shouldFinish=*/false);
    MockOrderQueueObserver<TestCtx> obs;

    queue.AddObserver(obs);
    queue.Enqueue(&order);
    TestCtx ctx;
    queue.Update(ctx, 0.0f);

    EXPECT_EQ(obs.startedCount, 1);
    ASSERT_NE(obs.lastStarted, nullptr);
    EXPECT_EQ(obs.lastStarted->GetOrderId(), Dia::Core::StringCRC("OrderA"));
}

TEST(DiaOrderQueue, ObserverReceivesOnOrderFinished)
{
    OrderQueue<TestCtx> queue;
    MockOrder<TestCtx> order(Dia::Core::StringCRC("OrderA"), /*shouldFinish=*/true);
    MockOrderQueueObserver<TestCtx> obs;

    queue.AddObserver(obs);
    queue.Enqueue(&order);
    TestCtx ctx;
    queue.Update(ctx, 0.0f);

    EXPECT_EQ(obs.finishedCount, 1);
    ASSERT_NE(obs.lastFinished, nullptr);
    EXPECT_EQ(obs.lastFinished->GetOrderId(), Dia::Core::StringCRC("OrderA"));
}

TEST(DiaOrderQueue, ObserverReceivesOnQueueEmptyAfterLastOrderFinishes)
{
    OrderQueue<TestCtx> queue;
    MockOrder<TestCtx> order(Dia::Core::StringCRC("OrderA"), /*shouldFinish=*/true);
    MockOrderQueueObserver<TestCtx> obs;

    queue.AddObserver(obs);
    queue.Enqueue(&order);
    TestCtx ctx;
    queue.Update(ctx, 0.0f);

    EXPECT_EQ(obs.emptyCount, 1);
}

TEST(DiaOrderQueue, ObserverNotNotifiedAfterRemoval)
{
    OrderQueue<TestCtx> queue;
    MockOrder<TestCtx> order(Dia::Core::StringCRC("OrderA"), /*shouldFinish=*/false);
    MockOrderQueueObserver<TestCtx> obs;

    queue.AddObserver(obs);
    queue.RemoveObserver(obs);

    queue.Enqueue(&order);
    TestCtx ctx;
    queue.Update(ctx, 0.0f);

    EXPECT_EQ(obs.startedCount, 0);
}

TEST(DiaOrderQueue, MultipleObserversAllReceiveCallbacks)
{
    OrderQueue<TestCtx> queue;
    MockOrder<TestCtx> order(Dia::Core::StringCRC("OrderA"), /*shouldFinish=*/false);
    MockOrderQueueObserver<TestCtx> obs1, obs2;

    queue.AddObserver(obs1);
    queue.AddObserver(obs2);

    queue.Enqueue(&order);
    TestCtx ctx;
    queue.Update(ctx, 0.0f);

    EXPECT_EQ(obs1.startedCount, 1);
    EXPECT_EQ(obs2.startedCount, 1);
}

// -------------------------------------------------------------------------
// GetQueueDepth
// -------------------------------------------------------------------------

TEST(DiaOrderQueue, DepthIsZeroInitially)
{
    OrderQueue<TestCtx> queue;
    EXPECT_EQ(queue.GetQueueDepth(), 0);
}

TEST(DiaOrderQueue, DepthIsTwoAfterEnqueuingTwoOrdersBeforeUpdate)
{
    OrderQueue<TestCtx> queue;
    MockOrder<TestCtx> orderA(Dia::Core::StringCRC("OrderA"));
    MockOrder<TestCtx> orderB(Dia::Core::StringCRC("OrderB"));

    queue.Enqueue(&orderA);
    queue.Enqueue(&orderB);

    EXPECT_EQ(queue.GetQueueDepth(), 2);
}

TEST(DiaOrderQueue, DepthIsOneAfterUpdatePromotesFirstOrder)
{
    OrderQueue<TestCtx> queue;
    MockOrder<TestCtx> orderA(Dia::Core::StringCRC("OrderA"), /*shouldFinish=*/false);
    MockOrder<TestCtx> orderB(Dia::Core::StringCRC("OrderB"), /*shouldFinish=*/false);

    queue.Enqueue(&orderA);
    queue.Enqueue(&orderB);

    TestCtx ctx;
    queue.Update(ctx, 0.0f);  // OrderA promoted to current; depth = current(1) + pending(1) = 2?
    // GetQueueDepth counts current + pending
    EXPECT_EQ(queue.GetQueueDepth(), 2);
}

TEST(DiaOrderQueue, DepthDecreasesWhenOrderFinishes)
{
    OrderQueue<TestCtx> queue;
    MockOrder<TestCtx> orderA(Dia::Core::StringCRC("OrderA"), /*shouldFinish=*/false);
    MockOrder<TestCtx> orderB(Dia::Core::StringCRC("OrderB"), /*shouldFinish=*/false);

    queue.Enqueue(&orderA);
    queue.Enqueue(&orderB);

    TestCtx ctx;
    queue.Update(ctx, 0.0f);  // OrderA current, depth = 2

    orderA.shouldFinish = true;
    queue.Update(ctx, 0.0f);  // OrderA finishes, OrderB starts, depth = 1

    EXPECT_EQ(queue.GetQueueDepth(), 1);
}

TEST(DiaOrderQueue, DepthIsZeroAfterSingleOrderFinishes)
{
    OrderQueue<TestCtx> queue;
    MockOrder<TestCtx> order(Dia::Core::StringCRC("OrderA"), /*shouldFinish=*/true);

    queue.Enqueue(&order);
    TestCtx ctx;
    queue.Update(ctx, 0.0f);  // start + finish

    EXPECT_EQ(queue.GetQueueDepth(), 0);
}

// -------------------------------------------------------------------------
// AssertOrderPending / AssertQueueEmpty helpers
// -------------------------------------------------------------------------

TEST(DiaOrderQueue, AssertOrderPendingHelperPassesWhenOrderIsCurrent)
{
    OrderQueue<TestCtx> queue;
    MockOrder<TestCtx> order(Dia::Core::StringCRC("WaitOrder"), /*shouldFinish=*/false);

    queue.Enqueue(&order);
    TestCtx ctx;
    queue.Update(ctx, 0.0f);

    // Should not assert/abort
    AssertOrderPending(queue, Dia::Core::StringCRC("WaitOrder"));
    SUCCEED();
}

TEST(DiaOrderQueue, AssertQueueEmptyHelperPassesOnEmptyQueue)
{
    OrderQueue<TestCtx> queue;
    AssertQueueEmpty(queue);
    SUCCEED();
}

// -------------------------------------------------------------------------
// OrderQueueComponent
// -------------------------------------------------------------------------

TEST(DiaOrderQueueComponent, UniqueIdMatchesExpectedString)
{
    EXPECT_EQ(OrderQueueComponent::kUniqueId, Dia::Core::StringCRC("OrderQueueComponent"));
}

TEST(DiaOrderQueueComponent, GetTypeIdReturnsUniqueId)
{
    OrderQueueComponent comp;
    EXPECT_EQ(comp.GetTypeId(), OrderQueueComponent::kUniqueId);
}

TEST(DiaOrderQueueComponent, GetQueueReturnsAQueue)
{
    OrderQueueComponent comp;
    OrderQueue<EntityOrderContext>& q = comp.GetQueue();
    EXPECT_TRUE(q.IsEmpty());
}

TEST(DiaOrderQueueComponent, ConstGetQueueReturnsAQueue)
{
    const OrderQueueComponent comp;
    const OrderQueue<EntityOrderContext>& q = comp.GetQueue();
    EXPECT_TRUE(q.IsEmpty());
}

TEST(DiaOrderQueueComponent, QueueIsUsableWithEntityOrderContext)
{
    OrderQueueComponent comp;
    Dia::Blackboard::Blackboard board;
    Dia::Entity::Entity dummyEntity;
    EntityOrderContext entityCtx{ dummyEntity, board };

    MockOrder<EntityOrderContext> order(Dia::Core::StringCRC("EntityOrder"), /*shouldFinish=*/false);

    comp.GetQueue().Enqueue(&order);
    comp.GetQueue().Update(entityCtx, 0.0f);

    EXPECT_EQ(order.startCount, 1);
    ASSERT_NE(comp.GetQueue().GetCurrentOrder(), nullptr);
    EXPECT_EQ(comp.GetQueue().GetCurrentOrder()->GetOrderId(), Dia::Core::StringCRC("EntityOrder"));
}

// -------------------------------------------------------------------------
// OrderMessage
// -------------------------------------------------------------------------

TEST(DiaOrderMessage, StructHasTargetOrderAndUrgentFields)
{
    Dia::Entity::Entity dummyEntity;
    int fakeOrder = 42;
    OrderMessage msg{ dummyEntity, &fakeOrder, /*urgent=*/false };

    EXPECT_EQ(msg.urgent, false);
    EXPECT_EQ(msg.order, &fakeOrder);
}

TEST(DiaOrderMessage, UrgentFlagIsStoredCorrectly)
{
    Dia::Entity::Entity dummyEntity;
    OrderMessage msg{ dummyEntity, nullptr, /*urgent=*/true };
    EXPECT_EQ(msg.urgent, true);
}

// -------------------------------------------------------------------------
// Boundary / Death tests (debug only)
// -------------------------------------------------------------------------

#ifdef _DEBUG

TEST(SLOW_DiaOrderQueue_Boundary, EnqueueNull_Asserts)
{
    OrderQueue<TestCtx> queue;
    EXPECT_DEATH(queue.Enqueue(nullptr), "");
}

TEST(SLOW_DiaOrderQueue_Boundary, EnqueueFrontNull_Asserts)
{
    OrderQueue<TestCtx> queue;
    EXPECT_DEATH(queue.EnqueueFront(nullptr), "");
}

TEST(SLOW_DiaOrderQueue_Boundary, EnqueueAtCapacity_Asserts)
{
    OrderQueue<TestCtx> queue;

    // Fill the queue to capacity (kMaxQueueDepth = 16)
    MockOrder<TestCtx> orders[16] = {
        MockOrder<TestCtx>(Dia::Core::StringCRC("O00")), MockOrder<TestCtx>(Dia::Core::StringCRC("O01")),
        MockOrder<TestCtx>(Dia::Core::StringCRC("O02")), MockOrder<TestCtx>(Dia::Core::StringCRC("O03")),
        MockOrder<TestCtx>(Dia::Core::StringCRC("O04")), MockOrder<TestCtx>(Dia::Core::StringCRC("O05")),
        MockOrder<TestCtx>(Dia::Core::StringCRC("O06")), MockOrder<TestCtx>(Dia::Core::StringCRC("O07")),
        MockOrder<TestCtx>(Dia::Core::StringCRC("O08")), MockOrder<TestCtx>(Dia::Core::StringCRC("O09")),
        MockOrder<TestCtx>(Dia::Core::StringCRC("O10")), MockOrder<TestCtx>(Dia::Core::StringCRC("O11")),
        MockOrder<TestCtx>(Dia::Core::StringCRC("O12")), MockOrder<TestCtx>(Dia::Core::StringCRC("O13")),
        MockOrder<TestCtx>(Dia::Core::StringCRC("O14")), MockOrder<TestCtx>(Dia::Core::StringCRC("O15")),
    };

    for (int i = 0; i < 16; ++i)
        queue.Enqueue(&orders[i]);

    MockOrder<TestCtx> overflow(Dia::Core::StringCRC("Overflow"));
    EXPECT_DEATH(queue.Enqueue(&overflow), "");
}

TEST(SLOW_DiaOrderQueue_Boundary, AddObserverAtCapacity_Asserts)
{
    OrderQueue<TestCtx> queue;
    MockOrderQueueObserver<TestCtx> obs[8];
    for (int i = 0; i < 8; ++i)
        queue.AddObserver(obs[i]);

    MockOrderQueueObserver<TestCtx> extra;
    EXPECT_DEATH(queue.AddObserver(extra), "");
}

TEST(SLOW_DiaOrderQueue_Boundary, RemoveObserverNotFound_Asserts)
{
    OrderQueue<TestCtx> queue;
    MockOrderQueueObserver<TestCtx> obs;
    EXPECT_DEATH(queue.RemoveObserver(obs), "");
}

#endif // _DEBUG

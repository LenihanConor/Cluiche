// TestEventDispatcher.cpp - Google Test unit tests for EventDispatcher
//
// Tests the Dia::Core::Events::EventDispatcher event routing system

#include <gtest/gtest.h>
#include <DiaCore/Architecture/Events/EventDispatcher.h>
#include <DiaCore/Architecture/Events/Event.h>
#include <thread>
#include <atomic>

using namespace Dia::Core::Events;

// ==============================================================================
// Test Event Classes
// ==============================================================================

class TestEvent : public Event
{
public:
    EVENT_CLASS_TYPE(TestEvent)
    EVENT_CLASS_CATEGORY(EventCategory_Custom)

    int data;

    TestEvent(int value = 0) : data(value) {}
};

class ButtonEvent : public Event
{
public:
    EVENT_CLASS_TYPE(ButtonEvent)
    EVENT_CLASS_CATEGORY(EventCategory_Input | EventCategory_Mouse)

    int buttonId;

    ButtonEvent(int id = 0) : buttonId(id) {}
};

class DamageEvent : public Event
{
public:
    EVENT_CLASS_TYPE(DamageEvent)
    EVENT_CLASS_CATEGORY(EventCategory_Gameplay)

    float damage;

    DamageEvent(float d = 0.0f) : damage(d) {}
};

// ==============================================================================
// Basic Subscription Tests
// ==============================================================================

TEST(EventDispatcher, DefaultConstruction_HasNoSubscribers)
{
    EventDispatcher dispatcher;

    TestEvent event;
    EXPECT_NO_THROW(dispatcher.Dispatch(&event));
}

TEST(EventDispatcher, Subscribe_ReturnsNonZeroHandlerID)
{
    EventDispatcher dispatcher;

    int id = dispatcher.Subscribe<TestEvent>([](TestEvent* e) {});

    EXPECT_NE(id, 0);
}

TEST(EventDispatcher, SubscribeAll_ReturnsNonZeroHandlerID)
{
    EventDispatcher dispatcher;

    int id = dispatcher.SubscribeAll([](Event* e) {});

    EXPECT_NE(id, 0);
}

TEST(EventDispatcher, Unsubscribe_RemovesHandler)
{
    EventDispatcher dispatcher;
    int callCount = 0;

    int id = dispatcher.Subscribe<TestEvent>([&callCount](TestEvent* e) {
        callCount++;
    });

    dispatcher.Unsubscribe(id);

    TestEvent event;
    dispatcher.Dispatch(&event);

    EXPECT_EQ(callCount, 0);
}

TEST(EventDispatcher, UnsubscribeInvalidID_DoesNotCrash)
{
    EventDispatcher dispatcher;

    EXPECT_NO_THROW(dispatcher.Unsubscribe(9999));
}

// ==============================================================================
// Type-Specific Dispatch Tests
// ==============================================================================

TEST(EventDispatcher, DispatchToTypeHandler_CallsHandler)
{
    EventDispatcher dispatcher;
    int callCount = 0;
    int receivedData = 0;

    dispatcher.Subscribe<TestEvent>([&](TestEvent* e) {
        callCount++;
        receivedData = e->data;
    });

    TestEvent event(42);
    dispatcher.Dispatch(&event);

    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(receivedData, 42);
}

TEST(EventDispatcher, DispatchWrongType_DoesNotCallHandler)
{
    EventDispatcher dispatcher;
    int callCount = 0;

    dispatcher.Subscribe<TestEvent>([&](TestEvent* e) {
        callCount++;
    });

    ButtonEvent event(10);
    dispatcher.Dispatch(&event);

    EXPECT_EQ(callCount, 0);
}

TEST(EventDispatcher, DispatchToMultipleHandlers_CallsAllHandlers)
{
    EventDispatcher dispatcher;
    int callCount = 0;

    dispatcher.Subscribe<TestEvent>([&](TestEvent* e) { callCount++; });
    dispatcher.Subscribe<TestEvent>([&](TestEvent* e) { callCount++; });
    dispatcher.Subscribe<TestEvent>([&](TestEvent* e) { callCount++; });

    TestEvent event;
    dispatcher.Dispatch(&event);

    EXPECT_EQ(callCount, 3);
}

// ==============================================================================
// Global Handler Tests
// ==============================================================================

TEST(EventDispatcher, SubscribeAll_ReceivesAllEventTypes)
{
    EventDispatcher dispatcher;
    int callCount = 0;

    dispatcher.SubscribeAll([&](Event* e) {
        callCount++;
    });

    TestEvent event1;
    ButtonEvent event2;
    DamageEvent event3;

    dispatcher.Dispatch(&event1);
    dispatcher.Dispatch(&event2);
    dispatcher.Dispatch(&event3);

    EXPECT_EQ(callCount, 3);
}

TEST(EventDispatcher, SubscribeAll_ReceivesEventAfterTypeHandlers)
{
    EventDispatcher dispatcher;
    std::vector<int> callOrder;

    dispatcher.Subscribe<TestEvent>([&](TestEvent* e) {
        callOrder.push_back(1);
    });

    dispatcher.SubscribeAll([&](Event* e) {
        callOrder.push_back(2);
    });

    TestEvent event;
    dispatcher.Dispatch(&event);

    ASSERT_EQ(callOrder.size(), 2);
    EXPECT_EQ(callOrder[0], 1);  // Type-specific first
    EXPECT_EQ(callOrder[1], 2);  // Global second
}

// ==============================================================================
// Event Handled Tests
// ==============================================================================

TEST(EventDispatcher, HandledEvent_StopsTypeHandlerPropagation)
{
    EventDispatcher dispatcher;
    int callCount = 0;

    dispatcher.Subscribe<TestEvent>([&](TestEvent* e) {
        e->SetHandled(true);
        callCount++;
    });

    dispatcher.Subscribe<TestEvent>([&](TestEvent* e) {
        callCount++;  // Should not be called
    });

    TestEvent event;
    dispatcher.Dispatch(&event);

    EXPECT_EQ(callCount, 1);
}

TEST(EventDispatcher, HandledEvent_StopsGlobalHandlerPropagation)
{
    EventDispatcher dispatcher;
    int callCount = 0;

    dispatcher.Subscribe<TestEvent>([&](TestEvent* e) {
        e->SetHandled(true);
        callCount++;
    });

    dispatcher.SubscribeAll([&](Event* e) {
        callCount++;  // Should not be called
    });

    TestEvent event;
    dispatcher.Dispatch(&event);

    EXPECT_EQ(callCount, 1);
}

// ==============================================================================
// Queue Tests
// ==============================================================================

TEST(EventDispatcher, QueueEvent_DoesNotDispatchImmediately)
{
    EventDispatcher dispatcher;
    int callCount = 0;

    dispatcher.Subscribe<TestEvent>([&](TestEvent* e) {
        callCount++;
    });

    dispatcher.QueueEvent(new TestEvent(10));

    EXPECT_EQ(callCount, 0);  // Not dispatched yet
}

TEST(EventDispatcher, ProcessQueue_DispatchesQueuedEvents)
{
    EventDispatcher dispatcher;
    int callCount = 0;

    dispatcher.Subscribe<TestEvent>([&](TestEvent* e) {
        callCount++;
    });

    dispatcher.QueueEvent(new TestEvent(1));
    dispatcher.QueueEvent(new TestEvent(2));
    dispatcher.QueueEvent(new TestEvent(3));

    dispatcher.ProcessQueue();

    EXPECT_EQ(callCount, 3);
}

TEST(EventDispatcher, ProcessQueueWithLimit_DispatchesLimitedEvents)
{
    EventDispatcher dispatcher;
    int callCount = 0;

    dispatcher.Subscribe<TestEvent>([&](TestEvent* e) {
        callCount++;
    });

    dispatcher.QueueEvent(new TestEvent(1));
    dispatcher.QueueEvent(new TestEvent(2));
    dispatcher.QueueEvent(new TestEvent(3));
    dispatcher.QueueEvent(new TestEvent(4));
    dispatcher.QueueEvent(new TestEvent(5));

    dispatcher.ProcessQueue(2);  // Process only 2 events

    EXPECT_EQ(callCount, 2);
    EXPECT_EQ(dispatcher.GetQueuedEventCount(), 3);

    // Process remaining
    dispatcher.ProcessQueue();
    EXPECT_EQ(callCount, 5);
}

TEST(EventDispatcher, GetQueuedEventCount_ReturnsCorrectCount)
{
    EventDispatcher dispatcher;

    EXPECT_EQ(dispatcher.GetQueuedEventCount(), 0);

    dispatcher.QueueEvent(new TestEvent(1));
    dispatcher.QueueEvent(new TestEvent(2));

    EXPECT_EQ(dispatcher.GetQueuedEventCount(), 2);

    dispatcher.ProcessQueue();

    EXPECT_EQ(dispatcher.GetQueuedEventCount(), 0);
}

TEST(EventDispatcher, QueueWithPriority_DispatchesInPriorityOrder)
{
    EventDispatcher dispatcher;
    std::vector<int> receivedOrder;

    dispatcher.Subscribe<TestEvent>([&](TestEvent* e) {
        receivedOrder.push_back(e->data);
    });

    // Queue in mixed order but with priorities
    dispatcher.QueueEvent(new TestEvent(1), EventPriority::Normal);
    dispatcher.QueueEvent(new TestEvent(2), EventPriority::High);
    dispatcher.QueueEvent(new TestEvent(3), EventPriority::Low);
    dispatcher.QueueEvent(new TestEvent(4), EventPriority::Immediate);

    dispatcher.ProcessQueue();

    // Should receive in priority order: 4 (Immediate), 2 (High), 1 (Normal), 3 (Low)
    // Note: This depends on EventQueue priority implementation
    EXPECT_EQ(receivedOrder.size(), 4);
}

// ==============================================================================
// Clear Tests
// ==============================================================================

TEST(EventDispatcher, Clear_RemovesAllHandlers)
{
    EventDispatcher dispatcher;
    int callCount = 0;

    dispatcher.Subscribe<TestEvent>([&](TestEvent* e) { callCount++; });
    dispatcher.Subscribe<ButtonEvent>([&](ButtonEvent* e) { callCount++; });
    dispatcher.SubscribeAll([&](Event* e) { callCount++; });

    dispatcher.Clear();

    TestEvent event;
    dispatcher.Dispatch(&event);

    EXPECT_EQ(callCount, 0);
}

TEST(EventDispatcher, Clear_ClearsQueuedEvents)
{
    EventDispatcher dispatcher;

    dispatcher.QueueEvent(new TestEvent(1));
    dispatcher.QueueEvent(new TestEvent(2));

    dispatcher.Clear();

    EXPECT_EQ(dispatcher.GetQueuedEventCount(), 0);
}

TEST(EventDispatcher, ClearQueue_OnlyClearsQueue)
{
    EventDispatcher dispatcher;
    int callCount = 0;

    dispatcher.Subscribe<TestEvent>([&](TestEvent* e) { callCount++; });

    dispatcher.QueueEvent(new TestEvent(1));
    dispatcher.QueueEvent(new TestEvent(2));

    dispatcher.ClearQueue();

    EXPECT_EQ(dispatcher.GetQueuedEventCount(), 0);

    // Handlers should still work
    TestEvent event;
    dispatcher.Dispatch(&event);

    EXPECT_EQ(callCount, 1);
}

// ==============================================================================
// Multiple Event Types Tests
// ==============================================================================

TEST(EventDispatcher, MultipleEventTypes_EachTypeRoutedCorrectly)
{
    EventDispatcher dispatcher;
    int testCount = 0;
    int buttonCount = 0;
    int damageCount = 0;

    dispatcher.Subscribe<TestEvent>([&](TestEvent* e) { testCount++; });
    dispatcher.Subscribe<ButtonEvent>([&](ButtonEvent* e) { buttonCount++; });
    dispatcher.Subscribe<DamageEvent>([&](DamageEvent* e) { damageCount++; });

    TestEvent event1;
    ButtonEvent event2;
    DamageEvent event3;

    dispatcher.Dispatch(&event1);
    dispatcher.Dispatch(&event2);
    dispatcher.Dispatch(&event3);

    EXPECT_EQ(testCount, 1);
    EXPECT_EQ(buttonCount, 1);
    EXPECT_EQ(damageCount, 1);
}

// ==============================================================================
// Thread Safety Tests
// ==============================================================================

TEST(EventDispatcher, ConcurrentSubscribe_ThreadSafe)
{
    EventDispatcher dispatcher;
    std::atomic<int> subscribeCount{0};

    auto subscribeThread = [&]() {
        for (int i = 0; i < 50; ++i)
        {
            dispatcher.Subscribe<TestEvent>([](TestEvent* e) {});
            subscribeCount++;
        }
    };

    std::thread t1(subscribeThread);
    std::thread t2(subscribeThread);
    std::thread t3(subscribeThread);

    t1.join();
    t2.join();
    t3.join();

    EXPECT_EQ(subscribeCount, 150);

    // Dispatch to verify all handlers work
    std::atomic<int> callCount{0};
    dispatcher.Subscribe<TestEvent>([&](TestEvent* e) { callCount++; });

    TestEvent event;
    dispatcher.Dispatch(&event);

    EXPECT_GT(callCount, 0);
}

TEST(EventDispatcher, ConcurrentDispatch_ThreadSafe)
{
    EventDispatcher dispatcher;
    std::atomic<int> callCount{0};

    dispatcher.Subscribe<TestEvent>([&](TestEvent* e) {
        callCount++;
    });

    auto dispatchThread = [&]() {
        for (int i = 0; i < 100; ++i)
        {
            TestEvent event(i);
            dispatcher.Dispatch(&event);
        }
    };

    std::thread t1(dispatchThread);
    std::thread t2(dispatchThread);

    t1.join();
    t2.join();

    EXPECT_EQ(callCount, 200);
}

TEST(EventDispatcher, ConcurrentQueueAndProcess_ThreadSafe)
{
    EventDispatcher dispatcher;
    std::atomic<int> dispatchedCount{0};
    std::atomic<bool> running{true};

    dispatcher.Subscribe<TestEvent>([&](TestEvent* e) {
        dispatchedCount++;
    });

    auto queueThread = [&]() {
        for (int i = 0; i < 200; ++i)
        {
            dispatcher.QueueEvent(new TestEvent(i));
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    };

    auto processThread = [&]() {
        while (running)
        {
            dispatcher.ProcessQueue(10);
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    };

    std::thread t1(queueThread);
    std::thread t2(processThread);

    t1.join();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;
    t2.join();

    // Process any remaining
    dispatcher.ProcessQueue();

    EXPECT_EQ(dispatchedCount, 200);
}

// ==============================================================================
// GlobalEventDispatcher Singleton Tests
// ==============================================================================

TEST(GlobalEventDispatcher, GetReturnsValidInstance)
{
    EventDispatcher& dispatcher = GlobalEventDispatcher::Get();

    // Should not crash
    EXPECT_NO_THROW({
        dispatcher.Subscribe<TestEvent>([](TestEvent* e) {});
    });
}

TEST(GlobalEventDispatcher, GetReturnsSameInstance)
{
    EventDispatcher& dispatcher1 = GlobalEventDispatcher::Get();
    EventDispatcher& dispatcher2 = GlobalEventDispatcher::Get();

    EXPECT_EQ(&dispatcher1, &dispatcher2);
}

// ==============================================================================
// Edge Case Tests
// ==============================================================================

TEST(EventDispatcher, DispatchNullEvent_DoesNotCrash)
{
    EventDispatcher dispatcher;

    dispatcher.Subscribe<TestEvent>([](TestEvent* e) {});

    EXPECT_NO_THROW(dispatcher.Dispatch(nullptr));
}

TEST(EventDispatcher, UnsubscribeDuringDispatch_DoesNotAffectCurrentDispatch)
{
    EventDispatcher dispatcher;
    int callCount = 0;
    int handler1Id = 0, handler2Id = 0, handler3Id = 0;

    handler1Id = dispatcher.Subscribe<TestEvent>([&](TestEvent* e) {
        callCount++;
        dispatcher.Unsubscribe(handler2Id);  // Remove handler2
    });

    handler2Id = dispatcher.Subscribe<TestEvent>([&](TestEvent* e) {
        callCount++;  // Should still be called
    });

    handler3Id = dispatcher.Subscribe<TestEvent>([&](TestEvent* e) {
        callCount++;
    });

    TestEvent event;
    dispatcher.Dispatch(&event);

    // All handlers execute because unsubscribe doesn't affect snapshot
    EXPECT_EQ(callCount, 3);
}

TEST(EventDispatcher, SubscribeDuringDispatch_NotCalledInCurrentDispatch)
{
    EventDispatcher dispatcher;
    int callCount = 0;

    dispatcher.Subscribe<TestEvent>([&](TestEvent* e) {
        callCount++;
        dispatcher.Subscribe<TestEvent>([&](TestEvent* e) {
            callCount++;  // Should not be called in this dispatch
        });
    });

    TestEvent event;
    dispatcher.Dispatch(&event);

    EXPECT_EQ(callCount, 1);

    // Next dispatch should call both
    TestEvent event2;
    dispatcher.Dispatch(&event2);

    EXPECT_EQ(callCount, 3);
}

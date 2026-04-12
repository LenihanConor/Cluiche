// TestEventQueue.cpp - Google Test unit tests for EventQueue
//
// Tests the Dia::Core::Events::EventQueue priority queue for event storage

#include <gtest/gtest.h>
#include <DiaCore/Architecture/Events/EventQueue.h>
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

class InputEvent : public Event
{
public:
    EVENT_CLASS_TYPE(InputEvent)
    EVENT_CLASS_CATEGORY(EventCategory_Input)

    int keyCode;

    InputEvent(int key = 0) : keyCode(key) {}
};

class GameplayEvent : public Event
{
public:
    EVENT_CLASS_TYPE(GameplayEvent)
    EVENT_CLASS_CATEGORY(EventCategory_Gameplay)

    float score;

    GameplayEvent(float s = 0.0f) : score(s) {}
};

// ==============================================================================
// Basic Queue Tests
// ==============================================================================

TEST(EventQueue, DefaultConstruction_IsEmpty)
{
    EventQueue queue;

    EXPECT_TRUE(queue.IsEmpty());
    EXPECT_EQ(queue.Size(), 0);
}

TEST(EventQueue, PushEvent_IncreasesSize)
{
    EventQueue queue;

    queue.Push(new TestEvent(10));

    EXPECT_FALSE(queue.IsEmpty());
    EXPECT_EQ(queue.Size(), 1);
}

TEST(EventQueue, PushNullEvent_DoesNotIncreaseSize)
{
    EventQueue queue;

    queue.Push(nullptr);

    EXPECT_TRUE(queue.IsEmpty());
    EXPECT_EQ(queue.Size(), 0);
}

TEST(EventQueue, PushMultipleEvents_IncreasesSize)
{
    EventQueue queue;

    queue.Push(new TestEvent(1));
    queue.Push(new TestEvent(2));
    queue.Push(new TestEvent(3));

    EXPECT_EQ(queue.Size(), 3);
}

// ==============================================================================
// Pop Tests
// ==============================================================================

TEST(EventQueue, Pop_ReturnsAndRemovesEvent)
{
    EventQueue queue;
    queue.Push(new TestEvent(42));

    Event* event = queue.Pop();

    ASSERT_NE(event, nullptr);
    EXPECT_TRUE(queue.IsEmpty());
    EXPECT_EQ(queue.Size(), 0);

    TestEvent* testEvent = EventCast<TestEvent>(event);
    ASSERT_NE(testEvent, nullptr);
    EXPECT_EQ(testEvent->data, 42);

    delete event;
}

TEST(EventQueue, PopEmpty_ReturnsNull)
{
    EventQueue queue;

    Event* event = queue.Pop();

    EXPECT_EQ(event, nullptr);
}

TEST(EventQueue, PopFIFO_ReturnsEventsInOrder)
{
    EventQueue queue;

    queue.Push(new TestEvent(1));
    queue.Push(new TestEvent(2));
    queue.Push(new TestEvent(3));

    TestEvent* e1 = EventCast<TestEvent>(queue.Pop());
    TestEvent* e2 = EventCast<TestEvent>(queue.Pop());
    TestEvent* e3 = EventCast<TestEvent>(queue.Pop());

    ASSERT_NE(e1, nullptr);
    ASSERT_NE(e2, nullptr);
    ASSERT_NE(e3, nullptr);

    EXPECT_EQ(e1->data, 1);
    EXPECT_EQ(e2->data, 2);
    EXPECT_EQ(e3->data, 3);

    delete e1;
    delete e2;
    delete e3;
}

// ==============================================================================
// PopAll Tests
// ==============================================================================

TEST(EventQueue, PopAll_RetrievesAllEvents)
{
    EventQueue queue;

    queue.Push(new TestEvent(1));
    queue.Push(new TestEvent(2));
    queue.Push(new TestEvent(3));

    std::vector<Event*> events;
    queue.PopAll(events);

    EXPECT_EQ(events.size(), 3);
    EXPECT_TRUE(queue.IsEmpty());

    // Verify order
    TestEvent* e1 = EventCast<TestEvent>(events[0]);
    TestEvent* e2 = EventCast<TestEvent>(events[1]);
    TestEvent* e3 = EventCast<TestEvent>(events[2]);

    ASSERT_NE(e1, nullptr);
    ASSERT_NE(e2, nullptr);
    ASSERT_NE(e3, nullptr);

    EXPECT_EQ(e1->data, 1);
    EXPECT_EQ(e2->data, 2);
    EXPECT_EQ(e3->data, 3);

    // Cleanup
    for (Event* e : events) delete e;
}

TEST(EventQueue, PopAllEmpty_ReturnsEmptyVector)
{
    EventQueue queue;
    std::vector<Event*> events;

    queue.PopAll(events);

    EXPECT_TRUE(events.empty());
}

// ==============================================================================
// Peek Tests
// ==============================================================================

TEST(EventQueue, Peek_ReturnsEventWithoutRemoving)
{
    EventQueue queue;
    queue.Push(new TestEvent(42));

    Event* event = queue.Peek();

    ASSERT_NE(event, nullptr);
    EXPECT_FALSE(queue.IsEmpty());
    EXPECT_EQ(queue.Size(), 1);

    TestEvent* testEvent = EventCast<TestEvent>(event);
    ASSERT_NE(testEvent, nullptr);
    EXPECT_EQ(testEvent->data, 42);

    // Cleanup
    delete queue.Pop();
}

TEST(EventQueue, PeekEmpty_ReturnsNull)
{
    EventQueue queue;

    Event* event = queue.Peek();

    EXPECT_EQ(event, nullptr);
}

// ==============================================================================
// Clear Tests
// ==============================================================================

TEST(EventQueue, Clear_RemovesAndDeletesAllEvents)
{
    EventQueue queue;

    queue.Push(new TestEvent(1));
    queue.Push(new TestEvent(2));
    queue.Push(new TestEvent(3));

    queue.Clear();

    EXPECT_TRUE(queue.IsEmpty());
    EXPECT_EQ(queue.Size(), 0);
}

TEST(EventQueue, ClearEmpty_DoesNotCrash)
{
    EventQueue queue;

    EXPECT_NO_THROW(queue.Clear());
}

// ==============================================================================
// Priority Queue Tests
// ==============================================================================

TEST(EventQueue, PriorityMode_OrdersByPriority)
{
    EventQueue queue;
    queue.SetUsePriority(true);

    // Push in mixed order
    queue.Push(new TestEvent(1), EventPriority::Normal);
    queue.Push(new TestEvent(2), EventPriority::High);
    queue.Push(new TestEvent(3), EventPriority::Low);
    queue.Push(new TestEvent(4), EventPriority::Immediate);

    // Should pop in priority order: Immediate > High > Normal > Low
    TestEvent* e1 = EventCast<TestEvent>(queue.Pop());
    TestEvent* e2 = EventCast<TestEvent>(queue.Pop());
    TestEvent* e3 = EventCast<TestEvent>(queue.Pop());
    TestEvent* e4 = EventCast<TestEvent>(queue.Pop());

    ASSERT_NE(e1, nullptr);
    ASSERT_NE(e2, nullptr);
    ASSERT_NE(e3, nullptr);
    ASSERT_NE(e4, nullptr);

    EXPECT_EQ(e1->data, 4);  // Immediate
    EXPECT_EQ(e2->data, 2);  // High
    EXPECT_EQ(e3->data, 1);  // Normal
    EXPECT_EQ(e4->data, 3);  // Low

    delete e1;
    delete e2;
    delete e3;
    delete e4;
}

TEST(EventQueue, PriorityModeDisabled_OrdersByFIFO)
{
    EventQueue queue;
    queue.SetUsePriority(false);

    // Push in mixed priority order
    queue.Push(new TestEvent(1), EventPriority::Low);
    queue.Push(new TestEvent(2), EventPriority::Immediate);
    queue.Push(new TestEvent(3), EventPriority::High);

    // Should pop in FIFO order regardless of priority
    TestEvent* e1 = EventCast<TestEvent>(queue.Pop());
    TestEvent* e2 = EventCast<TestEvent>(queue.Pop());
    TestEvent* e3 = EventCast<TestEvent>(queue.Pop());

    ASSERT_NE(e1, nullptr);
    ASSERT_NE(e2, nullptr);
    ASSERT_NE(e3, nullptr);

    EXPECT_EQ(e1->data, 1);
    EXPECT_EQ(e2->data, 2);
    EXPECT_EQ(e3->data, 3);

    delete e1;
    delete e2;
    delete e3;
}

// ==============================================================================
// Filter Tests
// ==============================================================================

TEST(EventQueue, PopByType_FiltersCorrectEvents)
{
    EventQueue queue;

    queue.Push(new TestEvent(1));
    queue.Push(new InputEvent(10));
    queue.Push(new TestEvent(2));
    queue.Push(new GameplayEvent(100.0f));
    queue.Push(new TestEvent(3));

    std::vector<Event*> testEvents;
    queue.PopByType(TestEvent::GetStaticType(), testEvents);

    EXPECT_EQ(testEvents.size(), 3);
    EXPECT_EQ(queue.Size(), 2);  // InputEvent and GameplayEvent remain

    // Verify we got TestEvents
    for (Event* event : testEvents)
    {
        EXPECT_NE(EventCast<TestEvent>(event), nullptr);
        delete event;
    }

    // Cleanup remaining
    queue.Clear();
}

TEST(EventQueue, PopByCategory_FiltersCorrectEvents)
{
    EventQueue queue;

    queue.Push(new TestEvent(1));          // Custom
    queue.Push(new InputEvent(10));        // Input
    queue.Push(new TestEvent(2));          // Custom
    queue.Push(new GameplayEvent(100.0f)); // Gameplay

    std::vector<Event*> customEvents;
    queue.PopByCategory(EventCategory_Custom, customEvents);

    EXPECT_EQ(customEvents.size(), 2);
    EXPECT_EQ(queue.Size(), 2);  // InputEvent and GameplayEvent remain

    // Verify we got Custom events
    for (Event* event : customEvents)
    {
        EXPECT_TRUE(event->IsInCategory(EventCategory_Custom));
        delete event;
    }

    // Cleanup remaining
    queue.Clear();
}

// ==============================================================================
// Thread Safety Tests
// ==============================================================================

TEST(EventQueue, ConcurrentPush_AllEventsAdded)
{
    EventQueue queue;
    std::atomic<int> pushCount{0};

    auto pushEvents = [&]() {
        for (int i = 0; i < 100; ++i)
        {
            queue.Push(new TestEvent(i));
            pushCount++;
        }
    };

    std::thread t1(pushEvents);
    std::thread t2(pushEvents);
    std::thread t3(pushEvents);

    t1.join();
    t2.join();
    t3.join();

    EXPECT_EQ(queue.Size(), 300);
    EXPECT_EQ(pushCount, 300);

    queue.Clear();
}

TEST(EventQueue, ConcurrentPop_ThreadSafe)
{
    EventQueue queue;

    // Push 300 events
    for (int i = 0; i < 300; ++i)
    {
        queue.Push(new TestEvent(i));
    }

    std::atomic<int> popCount{0};
    std::atomic<int> nullCount{0};

    auto popEvents = [&]() {
        for (int i = 0; i < 100; ++i)
        {
            Event* event = queue.Pop();
            if (event)
            {
                popCount++;
                delete event;
            }
            else
            {
                nullCount++;
            }
        }
    };

    std::thread t1(popEvents);
    std::thread t2(popEvents);
    std::thread t3(popEvents);

    t1.join();
    t2.join();
    t3.join();

    // All events should be popped
    EXPECT_EQ(popCount, 300);
    EXPECT_TRUE(queue.IsEmpty());
}

TEST(EventQueue, ConcurrentPushPop_ThreadSafe)
{
    EventQueue queue;
    std::atomic<bool> running{true};
    std::atomic<int> pushCount{0};
    std::atomic<int> popCount{0};

    auto pushThread = [&]() {
        for (int i = 0; i < 500; ++i)
        {
            queue.Push(new TestEvent(i));
            pushCount++;
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    };

    auto popThread = [&]() {
        while (running)
        {
            Event* event = queue.Pop();
            if (event)
            {
                popCount++;
                delete event;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    };

    std::thread t1(pushThread);
    std::thread t2(popThread);
    std::thread t3(popThread);

    t1.join();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;
    t2.join();
    t3.join();

    // Pop any remaining events
    while (!queue.IsEmpty())
    {
        Event* event = queue.Pop();
        if (event)
        {
            popCount++;
            delete event;
        }
    }

    EXPECT_EQ(pushCount, 500);
    EXPECT_EQ(popCount, 500);
}

// ==============================================================================
// Memory Management Tests
// ==============================================================================

TEST(EventQueue, Destructor_DeletesAllQueuedEvents)
{
    // Create queue in scope
    {
        EventQueue queue;
        queue.Push(new TestEvent(1));
        queue.Push(new TestEvent(2));
        queue.Push(new TestEvent(3));
    }  // Queue destroyed, should delete events

    // No memory leaks (verified with sanitizers in CI)
    SUCCEED();
}

TEST(EventQueue, Clear_DeletesAllEvents)
{
    EventQueue queue;
    queue.Push(new TestEvent(1));
    queue.Push(new TestEvent(2));
    queue.Push(new TestEvent(3));

    queue.Clear();

    // No memory leaks (verified with sanitizers in CI)
    EXPECT_TRUE(queue.IsEmpty());
}

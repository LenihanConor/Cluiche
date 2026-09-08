// TestInputBusAdapter.cpp
//
// InputBusAdapter drains a live InputSourceManager (via its own scratch
// EventData, cleared each Flush()) and broadcasts each drained event as its
// corresponding generated Messages::* type. Covers per-kind payload
// correctness, same-tick delivery, and repeat-Flush behaviour.

#include <gtest/gtest.h>

#include <DiaInput/InputBusAdapter.h>
#include <DiaInput/InputSourceManager.h>
#include <DiaMessageBus/Bus.h>

#include "Fixtures/FakeInputSource.h"

#include <vector>

using namespace Dia::Input;

// ---------------------------------------------------------------------------
// KeyDownEvent / KeyUpEvent
// ---------------------------------------------------------------------------

TEST(InputBusAdapter, KeyPress_DeliversKeyDownEvent_SameTick)
{
    InputSourceManager mgr;
    TestFixtures::FakeInputSource src;
    mgr.AddInputSource(&src);

    Dia::MessageBus::Bus bus;
    bus.Initialize();
    InputBusAdapter adapter(mgr, bus);
    bus.RegisterFlushAdapter(&adapter);

    std::vector<Messages::KeyDownEvent> received;
    auto handle = bus.Subscribe<Messages::KeyDownEvent>(
        Dia::Core::StringCRC("TestInputBusAdapter"),
        [&received](const Messages::KeyDownEvent& e) { received.push_back(e); });

    src.QueueKeyPress(42);
    bus.Update(); // pre-Primary Flush drains the source, Primary pass delivers — same tick

    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0].code, 42);
    EXPECT_FALSE(received[0].alt);
    EXPECT_FALSE(received[0].control);
    EXPECT_FALSE(received[0].shift);
    EXPECT_FALSE(received[0].system);
}

TEST(InputBusAdapter, KeyRelease_DeliversKeyUpEvent)
{
    InputSourceManager mgr;
    TestFixtures::FakeInputSource src;
    mgr.AddInputSource(&src);

    Dia::MessageBus::Bus bus;
    bus.Initialize();
    InputBusAdapter adapter(mgr, bus);
    bus.RegisterFlushAdapter(&adapter);

    std::vector<Messages::KeyUpEvent> received;
    auto handle = bus.Subscribe<Messages::KeyUpEvent>(
        Dia::Core::StringCRC("TestInputBusAdapter"),
        [&received](const Messages::KeyUpEvent& e) { received.push_back(e); });

    src.QueueKeyRelease(7);
    bus.Update();

    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0].code, 7);
}

// ---------------------------------------------------------------------------
// MouseButtonEvent (pressed + released) / MouseMovedEvent
// ---------------------------------------------------------------------------

static Event MakeMouseButtonEvent(Event::EType type, int button, int x, int y)
{
    Event e;
    e.type          = type;
    e.mouseButton.button = button;
    e.mouseButton.x      = x;
    e.mouseButton.y      = y;
    return e;
}

TEST(InputBusAdapter, MouseButtonPressed_DeliversMouseButtonEvent_PressedTrue)
{
    InputSourceManager mgr;
    TestFixtures::FakeInputSource src;
    mgr.AddInputSource(&src);

    Dia::MessageBus::Bus bus;
    bus.Initialize();
    InputBusAdapter adapter(mgr, bus);
    bus.RegisterFlushAdapter(&adapter);

    std::vector<Messages::MouseButtonEvent> received;
    auto handle = bus.Subscribe<Messages::MouseButtonEvent>(
        Dia::Core::StringCRC("TestInputBusAdapter"),
        [&received](const Messages::MouseButtonEvent& e) { received.push_back(e); });

    src.QueueEvent(MakeMouseButtonEvent(Event::EType::kMouseButtonPressed, 0, 10, 20));
    bus.Update();

    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0].button, 0);
    EXPECT_EQ(received[0].x, 10);
    EXPECT_EQ(received[0].y, 20);
    EXPECT_TRUE(received[0].pressed);
}

TEST(InputBusAdapter, MouseButtonReleased_DeliversMouseButtonEvent_PressedFalse)
{
    InputSourceManager mgr;
    TestFixtures::FakeInputSource src;
    mgr.AddInputSource(&src);

    Dia::MessageBus::Bus bus;
    bus.Initialize();
    InputBusAdapter adapter(mgr, bus);
    bus.RegisterFlushAdapter(&adapter);

    std::vector<Messages::MouseButtonEvent> received;
    auto handle = bus.Subscribe<Messages::MouseButtonEvent>(
        Dia::Core::StringCRC("TestInputBusAdapter"),
        [&received](const Messages::MouseButtonEvent& e) { received.push_back(e); });

    src.QueueEvent(MakeMouseButtonEvent(Event::EType::kMouseButtonReleased, 1, 5, 6));
    bus.Update();

    ASSERT_EQ(received.size(), 1u);
    EXPECT_FALSE(received[0].pressed);
}

TEST(InputBusAdapter, MouseMoved_DeliversMouseMovedEvent)
{
    InputSourceManager mgr;
    TestFixtures::FakeInputSource src;
    mgr.AddInputSource(&src);

    Dia::MessageBus::Bus bus;
    bus.Initialize();
    InputBusAdapter adapter(mgr, bus);
    bus.RegisterFlushAdapter(&adapter);

    std::vector<Messages::MouseMovedEvent> received;
    auto handle = bus.Subscribe<Messages::MouseMovedEvent>(
        Dia::Core::StringCRC("TestInputBusAdapter"),
        [&received](const Messages::MouseMovedEvent& e) { received.push_back(e); });

    Event move;
    move.type          = Event::EType::kMouseMoved;
    move.mouseMove.x   = 100;
    move.mouseMove.y   = 200;
    src.QueueEvent(move);
    bus.Update();

    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0].x, 100);
    EXPECT_EQ(received[0].y, 200);
}

// ---------------------------------------------------------------------------
// Multiple event kinds drained and delivered within a single Flush/tick
// ---------------------------------------------------------------------------

TEST(InputBusAdapter, MultipleQueuedEvents_AllDeliveredSameTick)
{
    InputSourceManager mgr;
    TestFixtures::FakeInputSource src;
    mgr.AddInputSource(&src);

    Dia::MessageBus::Bus bus;
    bus.Initialize();
    InputBusAdapter adapter(mgr, bus);
    bus.RegisterFlushAdapter(&adapter);

    int keyDownCount = 0, mouseMoveCount = 0;
    auto h1 = bus.Subscribe<Messages::KeyDownEvent>(
        Dia::Core::StringCRC("k"), [&keyDownCount](const Messages::KeyDownEvent&) { ++keyDownCount; });
    auto h2 = bus.Subscribe<Messages::MouseMovedEvent>(
        Dia::Core::StringCRC("m"), [&mouseMoveCount](const Messages::MouseMovedEvent&) { ++mouseMoveCount; });

    src.QueueKeyPress(1);
    Event move; move.type = Event::EType::kMouseMoved; move.mouseMove.x = 1; move.mouseMove.y = 2;
    src.QueueEvent(move);

    bus.Update();

    EXPECT_EQ(keyDownCount, 1);
    EXPECT_EQ(mouseMoveCount, 1);
}

// ---------------------------------------------------------------------------
// Repeat Flush without new events delivers nothing new (scratch buffer is
// cleared each Flush(); InputSourceManager::Update is caller-driven and does
// not replay already-drained source events).
// ---------------------------------------------------------------------------

TEST(InputBusAdapter, SecondFlushWithNoNewEvents_DeliversNothing)
{
    InputSourceManager mgr;
    TestFixtures::FakeInputSource src;
    mgr.AddInputSource(&src);

    Dia::MessageBus::Bus bus;
    bus.Initialize();
    InputBusAdapter adapter(mgr, bus);
    bus.RegisterFlushAdapter(&adapter);

    int count = 0;
    auto handle = bus.Subscribe<Messages::KeyDownEvent>(
        Dia::Core::StringCRC("TestInputBusAdapter"),
        [&count](const Messages::KeyDownEvent&) { ++count; });

    src.QueueKeyPress(9);
    bus.Update();
    EXPECT_EQ(count, 1);

    bus.Update(); // no new queued events this tick
    EXPECT_EQ(count, 1);
}

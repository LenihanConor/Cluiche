#include <gtest/gtest.h>

#include <DiaApplication/ApplicationProcessingUnit.h>
#include <DiaApplication/ApplicationPhase.h>
#include <DiaApplication/ApplicationModule.h>
#include <DiaApplication/MessageBus.h>
#include <DiaInput/InputSourceManager.h>
#include <DiaInput/EventData.h>
#include <DiaInput/Event.h>

#include "Fixtures/FakeInputSource.h"
#include "Fixtures/ProcessingUnitFixture.h"

using namespace Dia::Application;
using namespace Dia::Core;
using namespace TestFixtures;

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

class InputTestPU : public ProcessingUnit
{
public:
    InputTestPU() : ProcessingUnit(StringCRC("InputTestPU"), -1.0f, 16, 16)
                  , mUpdateCount(0), mMaxUpdates(0) {}

    void SetMaxUpdates(int n) { mMaxUpdates = n; mUpdateCount = 0; }
    virtual bool FlaggedToStopUpdating() const override
    {
        if (mMaxUpdates <= 0) return true;
        return (mUpdateCount++ >= mMaxUpdates);
    }
private:
    mutable int mUpdateCount;
    int mMaxUpdates;
};

class InputTestPhase : public Phase
{
public:
    InputTestPhase(ProcessingUnit* pu, const StringCRC& id) : Phase(pu, id) {}
    virtual bool FlaggedToStopUpdating() const override { return true; }
};

// Module that owns an InputSourceManager and collects events each update
class InputCollectorModule : public Module
{
public:
    InputCollectorModule(ProcessingUnit* pu)
        : Module(pu, StringCRC("InputCollector"), RunningEnum::kUpdate)
    {}

    void AddSource(Dia::Input::IInputSource* source)
    {
        mManager.AddInputSource(source);
    }

    virtual void DoUpdate() override
    {
        mLastFrameEvents = Dia::Input::EventData();
        mManager.StartFrame();
        mManager.Update(mLastFrameEvents);
        mManager.EndFrame();
    }

    const Dia::Input::EventData& GetLastFrameEvents() const { return mLastFrameEvents; }

private:
    Dia::Input::InputSourceManager mManager;
    Dia::Input::EventData          mLastFrameEvents;
};

// Module that subscribes to a MessageBus message and counts receipts
static int sMessageReceivedCount = 0;
static Dia::Core::StringCRC sLastMessageSender;

class MessageSubscriberModule : public Module
{
public:
    MessageSubscriberModule(ProcessingUnit* pu)
        : Module(pu, StringCRC("MsgSubscriber"), RunningEnum::kUpdate)
    {}

    virtual StateObject::OpertionResponse DoStart(const IStartData*) override
    {
        GetAssociatedProcessingUnit()->GetMessageBus().Subscribe(
            StringCRC("InputEvent"),
            StringCRC("MsgSubscriber"),
            [](const Message& msg)
            {
                sMessageReceivedCount++;
                sLastMessageSender = msg.senderId;
            });
        return StateObject::OpertionResponse::kImmediate;
    }

    virtual void DoStop() override
    {
        GetAssociatedProcessingUnit()->GetMessageBus().Unsubscribe(
            StringCRC("InputEvent"),
            StringCRC("MsgSubscriber"));
    }
};

// Module that sends a message via the bus each update
class MessageSenderModule : public Module
{
public:
    MessageSenderModule(ProcessingUnit* pu)
        : Module(pu, StringCRC("MsgSender"), RunningEnum::kUpdate)
    {}

    virtual void DoUpdate() override
    {
        int payload = 42;
        GetAssociatedProcessingUnit()->GetMessageBus().SendImmediate(
            StringCRC("InputEvent"),
            StringCRC("MsgSender"),
            payload);
    }
};

//-----------------------------------------------------------------------------
// Tests: FakeInputSource
//-----------------------------------------------------------------------------

TEST(InputPipeline, FakeSourceDeliversQueuedEvents)
{
    FakeInputSource source;
    source.QueueKeyPress(65);  // 'A'
    source.QueueKeyPress(66);  // 'B'

    Dia::Input::EventData events;
    source.StartFrame();
    source.Poll(events);
    source.EndFrame();

    EXPECT_EQ(events.Size(), 2u);
    EXPECT_EQ(events[0].type, Dia::Input::Event::EType::kKeyPressed);
    EXPECT_EQ(events[0].key.code, 65);
    EXPECT_EQ(events[1].key.code, 66);
    EXPECT_EQ(source.PollCallCount(), 1);
    EXPECT_EQ(source.StartFrameCallCount(), 1);
    EXPECT_EQ(source.EndFrameCallCount(), 1);
}

TEST(InputPipeline, FakeSourceClearsAfterPoll)
{
    FakeInputSource source;
    source.QueueKeyPress(65);

    Dia::Input::EventData first;
    source.Poll(first);
    EXPECT_EQ(first.Size(), 1u);

    Dia::Input::EventData second;
    source.Poll(second);
    EXPECT_EQ(second.Size(), 0u);  // Events consumed on first poll
}

//-----------------------------------------------------------------------------
// Tests: InputSourceManager integration with FakeInputSource
//-----------------------------------------------------------------------------

TEST(InputPipeline, ManagerAggregatesMultipleSources)
{
    FakeInputSource sourceA;
    FakeInputSource sourceB;

    sourceA.QueueKeyPress(10);
    sourceB.QueueKeyPress(20);
    sourceB.QueueKeyPress(30);

    Dia::Input::InputSourceManager manager;
    manager.AddInputSource(&sourceA);
    manager.AddInputSource(&sourceB);

    manager.StartFrame();
    Dia::Input::EventData events;
    manager.Update(events);
    manager.EndFrame();

    EXPECT_EQ(events.Size(), 3u);
}

TEST(InputPipeline, ManagerCallsLifecycleMethods)
{
    FakeInputSource source;

    Dia::Input::InputSourceManager manager;
    manager.AddInputSource(&source);

    manager.StartFrame();
    Dia::Input::EventData events;
    manager.Update(events);
    manager.EndFrame();

    EXPECT_EQ(source.StartFrameCallCount(), 1);
    EXPECT_EQ(source.PollCallCount(), 1);
    EXPECT_EQ(source.EndFrameCallCount(), 1);
}

TEST(InputPipeline, RemovedSourceNoLongerPolled)
{
    FakeInputSource source;
    source.QueueKeyPress(42);

    Dia::Input::InputSourceManager manager;
    manager.AddInputSource(&source);
    manager.RemoveInputSource(&source);

    Dia::Input::EventData events;
    manager.Update(events);

    EXPECT_EQ(events.Size(), 0u);
    EXPECT_EQ(source.PollCallCount(), 0);
}

//-----------------------------------------------------------------------------
// Tests: InputCollectorModule inside a live ProcessingUnit
//-----------------------------------------------------------------------------

TEST(InputPipeline, ModuleCollectsEventsEachUpdate)
{
    InputTestPU pu;
    InputTestPhase phase(&pu, StringCRC("Phase"));
    FakeInputSource source;
    InputCollectorModule collector(&pu);
    collector.AddSource(&source);

    phase.AddModule(&collector);
    pu.SetInitialPhase(&phase);
    pu.Initialize();
    pu.Start();

    source.QueueKeyPress(65);
    pu.SetMaxUpdates(1);
    pu.Update();

    EXPECT_EQ(collector.GetLastFrameEvents().Size(), 1u);
    EXPECT_EQ(collector.GetLastFrameEvents()[0].key.code, 65);

    pu.Stop();
}

TEST(InputPipeline, NoEventsQueuedMeansEmptyFrame)
{
    InputTestPU pu;
    InputTestPhase phase(&pu, StringCRC("Phase"));
    FakeInputSource source;
    InputCollectorModule collector(&pu);
    collector.AddSource(&source);

    phase.AddModule(&collector);
    pu.SetInitialPhase(&phase);
    pu.Initialize();
    pu.Start();

    pu.SetMaxUpdates(1);
    pu.Update();

    EXPECT_EQ(collector.GetLastFrameEvents().Size(), 0u);

    pu.Stop();
}

//-----------------------------------------------------------------------------
// Tests: MessageBus routing from one module to another
//-----------------------------------------------------------------------------

TEST(InputPipeline, MessageBusDeliversBetweenModules)
{
    sMessageReceivedCount = 0;
    sLastMessageSender = StringCRC("");

    InputTestPU pu;
    InputTestPhase phase(&pu, StringCRC("Phase"));
    MessageSenderModule sender(&pu);
    MessageSubscriberModule subscriber(&pu);

    phase.AddModule(&subscriber);
    phase.AddModule(&sender);
    pu.SetInitialPhase(&phase);
    pu.Initialize();
    pu.Start();

    pu.SetMaxUpdates(1);
    pu.Update();

    EXPECT_EQ(sMessageReceivedCount, 1);
    EXPECT_EQ(sLastMessageSender, StringCRC("MsgSender"));

    pu.Stop();
}

TEST(InputPipeline, MessageBusDeliveriesAccumulateAcrossUpdates)
{
    sMessageReceivedCount = 0;

    InputTestPU pu;
    InputTestPhase phase(&pu, StringCRC("Phase"));
    MessageSenderModule sender(&pu);
    MessageSubscriberModule subscriber(&pu);

    phase.AddModule(&subscriber);
    phase.AddModule(&sender);
    pu.SetInitialPhase(&phase);
    pu.Initialize();
    pu.Start();

    pu.SetMaxUpdates(3);
    pu.Update();

    EXPECT_EQ(sMessageReceivedCount, 3);

    pu.Stop();
}

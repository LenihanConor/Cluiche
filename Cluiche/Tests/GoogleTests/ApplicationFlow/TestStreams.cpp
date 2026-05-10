////////////////////////////////////////////////////////////////////////////////
// Filename: TestStreams.cpp
// GoogleTest suite — DiaApplicationFlow v2 Stream stores and handles
//
// Three sections:
//   1. FrameStreamStore<T> unit tests (direct, no Application)
//   2. EventStreamStore<T> unit tests (direct, no Application)
//   3. Integration tests through Application using StreamWriter/Reader handles
//
// Module types prefixed "Str_" to avoid ODR collisions with other test files.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV2.h>
#include <DiaApplicationFlow/Streams/FrameStreamStore.h>
#include <DiaApplicationFlow/Streams/EventStreamStore.h>
#include <DiaApplicationFlow/Streams/StreamWriter.h>
#include <DiaApplicationFlow/Streams/StreamReader.h>
#include <DiaApplicationFlow/Streams/EventStreamWriter.h>
#include <DiaApplicationFlow/Streams/EventStreamReader.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Time/TimeAbsolute.h>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace Dia::Core::Containers;

// ---------------------------------------------------------------------------
// Section 1: FrameStreamStore<T> unit tests
// ---------------------------------------------------------------------------

TEST(FrameStream, FetchLatestReturnsNullBeforeWrite)
{
    FrameStreamStore<int> store(StringCRC("s_null"));
    const int* val = store.FetchLatest();
    EXPECT_EQ(val, nullptr)
        << "FetchLatest should return nullptr before any Write";
}

TEST(FrameStream, WriteAndFetchLatest)
{
    FrameStreamStore<int> store(StringCRC("s_write"));
    TimeAbsolute t = TimeAbsolute::CreateFromMilliseconds(0);
    store.Write(42, t);

    const int* val = store.FetchLatest();
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 42);
}

TEST(FrameStream, DoubleBufferWriteReadsLatest)
{
    FrameStreamStore<int> store(StringCRC("s_double"));
    TimeAbsolute t0 = TimeAbsolute::CreateFromMilliseconds(0);
    TimeAbsolute t1 = TimeAbsolute::CreateFromMilliseconds(16);

    store.Write(1, t0);
    store.Write(2, t1);

    const int* val = store.FetchLatest();
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 2)
        << "FetchLatest should return the most recently written value";
}

// ---------------------------------------------------------------------------
// Section 2: EventStreamStore<T> unit tests
// ---------------------------------------------------------------------------

TEST(EventStream, RegisterReaderReturnsValidIndex)
{
    EventStreamStore<int> estore(StringCRC("e_idx"));
    int idx = estore.RegisterReader();
    EXPECT_GE(idx, 0)
        << "RegisterReader should return a non-negative reader index";
}

TEST(EventStream, SendAndConsume)
{
    EventStreamStore<int> estore(StringCRC("e_basic"));
    int rIdx = estore.RegisterReader();

    estore.Send(10);
    estore.Send(20);

    DynamicArrayC<int, 32> out;
    estore.Consume(rIdx, out);

    ASSERT_EQ(out.Size(), 2u);
    EXPECT_EQ(out[0], 10);
    EXPECT_EQ(out[1], 20);
}

TEST(EventStream, ConsumeIsPerReader)
{
    EventStreamStore<int> estore(StringCRC("e_per_reader"));
    int rA = estore.RegisterReader();
    int rB = estore.RegisterReader();

    estore.Send(100);
    estore.Send(200);

    // Each reader should independently receive both events.
    DynamicArrayC<int, 32> outA;
    estore.Consume(rA, outA);
    ASSERT_EQ(outA.Size(), 2u) << "Reader A should get 2 events";
    EXPECT_EQ(outA[0], 100);
    EXPECT_EQ(outA[1], 200);

    DynamicArrayC<int, 32> outB;
    estore.Consume(rB, outB);
    ASSERT_EQ(outB.Size(), 2u) << "Reader B should get 2 events independently";
    EXPECT_EQ(outB[0], 100);
    EXPECT_EQ(outB[1], 200);
}

// When the ring buffer is full (capacity=2) and a third event is sent, the
// oldest event is dropped. Consume should return the 2 most recent events.
TEST(EventStream, OverflowDropsOldest)
{
    // Capacity of 2: only 2 events can be buffered per reader.
    EventStreamStore<int> estore(StringCRC("e_overflow"), /*capacity=*/2u);
    int rIdx = estore.RegisterReader();

    estore.Send(1);  // oldest — will be dropped
    estore.Send(2);
    estore.Send(3);  // causes overflow: 1 is dropped, 2 and 3 remain

    DynamicArrayC<int, 32> out;
    estore.Consume(rIdx, out);

    ASSERT_EQ(out.Size(), 2u)
        << "Buffer capacity 2 should hold at most 2 events after overflow";
    EXPECT_EQ(out[0], 2) << "Second event should be first after overflow drop";
    EXPECT_EQ(out[1], 3) << "Third event should be second";
}

// ---------------------------------------------------------------------------
// Section 3: Stream integration through Application
//
// Writer and reader modules connect their handles in OnConnectStreams and
// exchange data through an Application's stream registry.
//
// File-scope module pointers capture the instances created by the factory
// (TypeRegistry only accepts raw function pointers, not capturing lambdas).
// ---------------------------------------------------------------------------

// ---- FrameStream integration -----------------------------------------------

struct Str_FrameWriterModule : Module
{
    using Module::Module;
    static const StringCRC kTypeId;

    StreamWriter<int> mWriter{this, StringCRC("Str_FrameChannel")};
    int writeValue = 0;

    void OnConnectStreams(Application& app) override { mWriter.Connect(app); }
    StartResult DoStart() override { return StartResult::kReady; }
    void DoUpdate(float) override
    {
        TimeAbsolute t = TimeAbsolute::CreateFromMilliseconds(0);
        mWriter.Write(writeValue, t);
    }
    StopResult DoStop() override { return StopResult::kDone; }
};
const StringCRC Str_FrameWriterModule::kTypeId("Str_FrameWriterModule");

struct Str_FrameReaderModule : Module
{
    using Module::Module;
    static const StringCRC kTypeId;

    StreamReader<int> mReader{this, StringCRC("Str_FrameChannel")};
    int lastRead = -1;

    void OnConnectStreams(Application& app) override { mReader.Connect(app); }
    StartResult DoStart() override { return StartResult::kReady; }
    void DoUpdate(float) override
    {
        const int* v = mReader.FetchLatest();
        if (v) lastRead = *v;
    }
    StopResult DoStop() override { return StopResult::kDone; }
};
const StringCRC Str_FrameReaderModule::kTypeId("Str_FrameReaderModule");

static Str_FrameWriterModule* g_frameWriter = nullptr;
static Str_FrameReaderModule* g_frameReader = nullptr;

static Module* CreateFrameWriter(const StringCRC& id)
{
    auto* m = new Str_FrameWriterModule(id);
    g_frameWriter = m;
    return m;
}

static Module* CreateFrameReader(const StringCRC& id)
{
    auto* m = new Str_FrameReaderModule(id);
    g_frameReader = m;
    return m;
}

// Helper: pump Update() until all modules in puId are kActive.
static bool PumpUntilAllActive(Application& app, const StringCRC& puId, int limit = 100)
{
    for (int i = 0; i < limit; ++i)
    {
        DynamicArrayC<ModuleStateInfo, 64> infos;
        app.GetActiveModules(puId, infos);
        if (infos.Size() > 0)
        {
            bool allActive = true;
            for (unsigned int m = 0; m < infos.Size(); ++m)
            {
                if (infos[m].state != ModuleState::kActive)
                {
                    allActive = false;
                    break;
                }
            }
            if (allActive)
                return true;
        }
        app.Update(1.0f / 60.0f);
    }
    return false;
}

TEST(StreamIntegration, FrameStreamWriterReaderRoundTrip)
{
    g_frameWriter = nullptr;
    g_frameReader = nullptr;

    TypeRegistry reg;
    reg.Register(Str_FrameWriterModule::kTypeId, CreateFrameWriter);
    reg.Register(Str_FrameReaderModule::kTypeId, CreateFrameReader);

    ApplicationManifestV2 manifest;
    manifest.version = 2;

    StageDeclaration boot;
    boot.name = StringCRC("Boot");
    manifest.stages.Add(boot);
    manifest.initialStage = StringCRC("Boot");

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    // Writer declared before reader so stream store is created on first Connect.
    {
        ModuleDeclaration mod;
        mod.instanceId     = StringCRC("frameWriter");
        mod.typeId         = Str_FrameWriterModule::kTypeId;
        mod.startTimeoutMs = 10000.0f;
        mod.stopTimeoutMs  = 5000.0f;
        mod.stages.Add(StringCRC("Boot"));
        pu.modules.Add(mod);
    }
    {
        ModuleDeclaration mod;
        mod.instanceId     = StringCRC("frameReader");
        mod.typeId         = Str_FrameReaderModule::kTypeId;
        mod.startTimeoutMs = 10000.0f;
        mod.stopTimeoutMs  = 5000.0f;
        mod.stages.Add(StringCRC("Boot"));
        pu.modules.Add(mod);
    }
    manifest.processingUnits.Add(pu);

    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    ASSERT_TRUE(PumpUntilAllActive(app, StringCRC("MainPU"), 50));
    ASSERT_NE(g_frameWriter, nullptr);
    ASSERT_NE(g_frameReader, nullptr);

    EXPECT_TRUE(g_frameWriter->mWriter.IsConnected())
        << "Writer handle should be connected after OnConnectStreams";
    EXPECT_TRUE(g_frameReader->mReader.IsConnected())
        << "Reader handle should be connected after OnConnectStreams";

    // Set the value we want to observe, then pump one Update so DoUpdate runs.
    g_frameWriter->writeValue = 99;
    app.Update(1.0f / 60.0f);  // writer calls Write(99, ...), reader calls FetchLatest()
    app.Update(1.0f / 60.0f);  // second tick ensures reader observes the write

    EXPECT_EQ(g_frameReader->lastRead, 99)
        << "Reader should observe the value written by the writer";
}

// ---- EventStream integration -----------------------------------------------

struct Str_EventWriterModule : Module
{
    using Module::Module;
    static const StringCRC kTypeId;

    EventStreamWriter<int> mWriter{this, StringCRC("Str_EventChannel")};
    bool sendOnNextUpdate = false;
    int  sendValue        = 0;

    void OnConnectStreams(Application& app) override { mWriter.Connect(app); }
    StartResult DoStart() override { return StartResult::kReady; }
    void DoUpdate(float) override
    {
        if (sendOnNextUpdate)
        {
            mWriter.Send(sendValue);
            sendOnNextUpdate = false;
        }
    }
    StopResult DoStop() override { return StopResult::kDone; }
};
const StringCRC Str_EventWriterModule::kTypeId("Str_EventWriterModule");

struct Str_EventReaderModule : Module
{
    using Module::Module;
    static const StringCRC kTypeId;

    EventStreamReader<int> mReader{this, StringCRC("Str_EventChannel")};
    DynamicArrayC<int, 32> consumed;

    void OnConnectStreams(Application& app) override { mReader.Connect(app); }
    StartResult DoStart() override { return StartResult::kReady; }
    void DoUpdate(float) override
    {
        mReader.Consume(consumed);
    }
    StopResult DoStop() override { return StopResult::kDone; }
};
const StringCRC Str_EventReaderModule::kTypeId("Str_EventReaderModule");

static Str_EventWriterModule* g_eventWriter = nullptr;
static Str_EventReaderModule* g_eventReader = nullptr;

static Module* CreateEventWriter(const StringCRC& id)
{
    auto* m = new Str_EventWriterModule(id);
    g_eventWriter = m;
    return m;
}

static Module* CreateEventReader(const StringCRC& id)
{
    auto* m = new Str_EventReaderModule(id);
    g_eventReader = m;
    return m;
}

TEST(StreamIntegration, EventStreamWriterReaderRoundTrip)
{
    g_eventWriter = nullptr;
    g_eventReader = nullptr;

    TypeRegistry reg;
    reg.Register(Str_EventWriterModule::kTypeId, CreateEventWriter);
    reg.Register(Str_EventReaderModule::kTypeId, CreateEventReader);

    ApplicationManifestV2 manifest;
    manifest.version = 2;

    StageDeclaration boot;
    boot.name = StringCRC("Boot");
    manifest.stages.Add(boot);
    manifest.initialStage = StringCRC("Boot");

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    {
        ModuleDeclaration mod;
        mod.instanceId     = StringCRC("eventWriter");
        mod.typeId         = Str_EventWriterModule::kTypeId;
        mod.startTimeoutMs = 10000.0f;
        mod.stopTimeoutMs  = 5000.0f;
        mod.stages.Add(StringCRC("Boot"));
        pu.modules.Add(mod);
    }
    {
        ModuleDeclaration mod;
        mod.instanceId     = StringCRC("eventReader");
        mod.typeId         = Str_EventReaderModule::kTypeId;
        mod.startTimeoutMs = 10000.0f;
        mod.stopTimeoutMs  = 5000.0f;
        mod.stages.Add(StringCRC("Boot"));
        pu.modules.Add(mod);
    }
    manifest.processingUnits.Add(pu);

    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());

    ASSERT_TRUE(PumpUntilAllActive(app, StringCRC("MainPU"), 50));
    ASSERT_NE(g_eventWriter, nullptr);
    ASSERT_NE(g_eventReader, nullptr);

    EXPECT_TRUE(g_eventWriter->mWriter.IsConnected())
        << "EventStreamWriter handle should be connected after OnConnectStreams";
    EXPECT_TRUE(g_eventReader->mReader.IsConnected())
        << "EventStreamReader handle should be connected after OnConnectStreams";

    // Queue a send on the next DoUpdate, then pump two ticks:
    //   Tick 1: writer sends event 77; reader consumes it in the same tick.
    //   Tick 2: no new events; consumed array unchanged from tick 1.
    g_eventWriter->sendValue        = 77;
    g_eventWriter->sendOnNextUpdate = true;
    app.Update(1.0f / 60.0f);
    app.Update(1.0f / 60.0f);

    ASSERT_GE(g_eventReader->consumed.Size(), 1u)
        << "Reader should have consumed at least one event";
    EXPECT_EQ(g_eventReader->consumed[0], 77)
        << "Reader should have consumed the event value sent by the writer";
}

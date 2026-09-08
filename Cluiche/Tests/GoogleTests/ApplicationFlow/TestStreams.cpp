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
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaStreams/FrameStreamStore.h>
#include <DiaStreams/EventStreamStore.h>
#include <DiaStreams/StreamWriter.h>
#include <DiaStreams/StreamReader.h>
#include <DiaStreams/EventStreamWriter.h>
#include <DiaStreams/EventStreamReader.h>
#include <DiaStreams/Event.h>
#include <DiaStreams/SendResult.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaObservation/Log/Logger.h>
#include <DiaObservation/Log/ISink.h>
#include <DiaObservation/Log/LogEntry.h>
#include <DiaObservation/Log/LogLevel.h>

#include <string.h>

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

// ---- Ring buffer + FetchClosestTo tests ------------------------------------

TEST(FrameStream, FetchClosestToReturnsNullBeforeWrite)
{
    FrameStreamStore<int> store(StringCRC("s_closest_null"));
    const int* val = store.FetchClosestTo(TimeAbsolute::CreateFromMilliseconds(0));
    EXPECT_EQ(val, nullptr)
        << "FetchClosestTo should return nullptr before any Write";
}

TEST(FrameStream, FetchClosestToExactMatch)
{
    FrameStreamStore<int> store(StringCRC("s_closest_exact"));
    store.Write(10, TimeAbsolute::CreateFromMilliseconds(10));
    store.Write(20, TimeAbsolute::CreateFromMilliseconds(20));
    store.Write(30, TimeAbsolute::CreateFromMilliseconds(30));

    const int* val = store.FetchClosestTo(TimeAbsolute::CreateFromMilliseconds(20));
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 20)
        << "FetchClosestTo should return the sample with an exactly matching timestamp";
}

TEST(FrameStream, FetchClosestToNearestEarlier)
{
    // Samples at 10ms and 20ms; request 12ms -> closer to the 10ms sample.
    FrameStreamStore<int> store(StringCRC("s_closest_earlier"));
    store.Write(10, TimeAbsolute::CreateFromMilliseconds(10));
    store.Write(20, TimeAbsolute::CreateFromMilliseconds(20));

    const int* val = store.FetchClosestTo(TimeAbsolute::CreateFromMilliseconds(12));
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 10)
        << "Request 12ms falls between 10ms and 20ms, nearer the earlier (10ms) sample";
}

TEST(FrameStream, FetchClosestToNearestLater)
{
    // Samples at 10ms and 20ms; request 18ms -> closer to the 20ms sample.
    FrameStreamStore<int> store(StringCRC("s_closest_later"));
    store.Write(10, TimeAbsolute::CreateFromMilliseconds(10));
    store.Write(20, TimeAbsolute::CreateFromMilliseconds(20));

    const int* val = store.FetchClosestTo(TimeAbsolute::CreateFromMilliseconds(18));
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 20)
        << "Request 18ms falls between 10ms and 20ms, nearer the later (20ms) sample";
}

TEST(FrameStream, RingHoldsCapacitySamplesAndFetchLatestIsNewest)
{
    // Write more than the ring capacity (8). Values keyed to timestamps in ms.
    FrameStreamStore<int> store(StringCRC("s_ring_capacity"));
    const int kWrites = 12;  // > kRingCapacity (8)
    for (int i = 1; i <= kWrites; ++i)
        store.Write(i * 100, TimeAbsolute::CreateFromMilliseconds(i));

    const int* latest = store.FetchLatest();
    ASSERT_NE(latest, nullptr);
    EXPECT_EQ(*latest, kWrites * 100)
        << "FetchLatest should return the most recently written value after wrap-around";

    // The 5 oldest writes (ts 1..4) were overwritten; only ts 5..12 remain.
    // Requesting exactly ts=12 returns its value; requesting exactly ts=5 (oldest
    // retained) returns its value.
    const int* newest = store.FetchClosestTo(TimeAbsolute::CreateFromMilliseconds(12));
    ASSERT_NE(newest, nullptr);
    EXPECT_EQ(*newest, 1200);

    const int* oldestRetained = store.FetchClosestTo(TimeAbsolute::CreateFromMilliseconds(5));
    ASSERT_NE(oldestRetained, nullptr);
    EXPECT_EQ(*oldestRetained, 500)
        << "Oldest retained sample (ts=5) should still be retrievable exactly";
}

TEST(FrameStream, FetchClosestToOlderThanAllRetainedReturnsOldest)
{
    // Overwrite the ring so ts 5..12 remain. Requesting an ancient timestamp (ts=1,
    // which has been overwritten) should gracefully return the oldest RETAINED
    // sample (ts=5), not nullptr.
    FrameStreamStore<int> store(StringCRC("s_ring_graceful"));
    const int kWrites = 12;
    for (int i = 1; i <= kWrites; ++i)
        store.Write(i * 100, TimeAbsolute::CreateFromMilliseconds(i));

    const int* val = store.FetchClosestTo(TimeAbsolute::CreateFromMilliseconds(1));
    ASSERT_NE(val, nullptr)
        << "FetchClosestTo on a timestamp older than everything retained should "
           "degrade to the nearest (oldest retained) sample, not nullptr";
    EXPECT_EQ(*val, 500)
        << "Oldest retained sample is ts=5 (value 500) after 12 writes into an 8-slot ring";
}

// ---------------------------------------------------------------------------
// Section 2: EventStreamStore<T> unit tests
// ---------------------------------------------------------------------------

// Helper to make an Event<int> with a given payload value.
static Event<int> MakeEvent(int payload)
{
    Event<int> ev;
    ev.payload = payload;
    return ev;
}

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

    estore.Send(MakeEvent(10));
    estore.Send(MakeEvent(20));

    DynamicArrayC<Event<int>, 32> out;
    estore.Consume(rIdx, out);

    ASSERT_EQ(out.Size(), 2u);
    EXPECT_EQ(out[0].payload, 10);
    EXPECT_EQ(out[1].payload, 20);
}

TEST(EventStream, ConsumeIsPerReader)
{
    EventStreamStore<int> estore(StringCRC("e_per_reader"));
    int rA = estore.RegisterReader();
    int rB = estore.RegisterReader();

    estore.Send(MakeEvent(100));
    estore.Send(MakeEvent(200));

    DynamicArrayC<Event<int>, 32> outA;
    estore.Consume(rA, outA);
    ASSERT_EQ(outA.Size(), 2u) << "Reader A should get 2 events";
    EXPECT_EQ(outA[0].payload, 100);
    EXPECT_EQ(outA[1].payload, 200);

    DynamicArrayC<Event<int>, 32> outB;
    estore.Consume(rB, outB);
    ASSERT_EQ(outB.Size(), 2u) << "Reader B should get 2 events independently";
    EXPECT_EQ(outB[0].payload, 100);
    EXPECT_EQ(outB[1].payload, 200);
}

// When the ring buffer is full (capacity=2) and a third event is sent, the
// oldest event is dropped. Consume should return the 2 most recent events.
TEST(EventStream, OverflowDropsOldest)
{
    // Capacity of 2: only 2 events can be buffered per reader.
    EventStreamStore<int> estore(StringCRC("e_overflow"), Dia::Core::StringCRC::kZero, /*capacity=*/2u);
    int rIdx = estore.RegisterReader();

    SendResult r1 = estore.Send(MakeEvent(1));  // oldest — will be dropped
    SendResult r2 = estore.Send(MakeEvent(2));
    SendResult r3 = estore.Send(MakeEvent(3));  // causes overflow: 1 is dropped

    EXPECT_EQ(r1, SendResult::kDelivered);
    EXPECT_EQ(r2, SendResult::kDelivered);
    EXPECT_EQ(r3, SendResult::kDroppedOldest);

    DynamicArrayC<Event<int>, 32> out;
    estore.Consume(rIdx, out);

    ASSERT_EQ(out.Size(), 2u)
        << "Buffer capacity 2 should hold at most 2 events after overflow";
    EXPECT_EQ(out[0].payload, 2) << "Second event should be first after overflow drop";
    EXPECT_EQ(out[1].payload, 3) << "Third event should be second";
}

TEST(EventStream, DropNewestPolicy)
{
    EventStreamStore<int> estore(StringCRC("e_drop_newest"),
        Dia::Core::StringCRC::kZero, /*capacity=*/2u,
        /*maxReaders=*/EventStreamStore<int>::kDefaultMaxReaders,
        OverflowPolicy::kDropNewest);
    int rIdx = estore.RegisterReader();

    estore.Send(MakeEvent(1));
    estore.Send(MakeEvent(2));
    SendResult r3 = estore.Send(MakeEvent(3));  // should be dropped-newest

    EXPECT_EQ(r3, SendResult::kDroppedNewest);

    DynamicArrayC<Event<int>, 32> out;
    estore.Consume(rIdx, out);

    ASSERT_EQ(out.Size(), 2u);
    EXPECT_EQ(out[0].payload, 1);
    EXPECT_EQ(out[1].payload, 2);
}

TEST(EventStream, SequenceMonotonic)
{
    EventStreamStore<int> estore(StringCRC("e_sequence"));
    int rIdx = estore.RegisterReader();

    estore.Send(MakeEvent(1));
    estore.Send(MakeEvent(2));
    estore.Send(MakeEvent(3));

    DynamicArrayC<Event<int>, 32> out;
    estore.Consume(rIdx, out);

    ASSERT_EQ(out.Size(), 3u);
    EXPECT_LT(out[0].sequence, out[1].sequence)
        << "sequence should be monotonically increasing";
    EXPECT_LT(out[1].sequence, out[2].sequence);
}

// ---- Reader-slot capacity ceiling (Task 3.3) --------------------------------
//
// EventStreamStore's reader-slot storage used to be a compile-time fixed
// array sized to kDefaultMaxReaders (8), regardless of the runtime
// `maxReaders` constructor argument — so passing >8 silently capped at 8.
// These tests cover: (a) a 9th distinct reader succeeding when the store is
// constructed with maxReaders > 8, (b) a logged warning (not a silent -1)
// when a reader connects past the store's actual configured capacity, and
// (c) the existing default (8-reader) ceiling behaviour is unchanged.

class StreamLogSink : public Dia::Observation::Log::ISink
{
public:
    static const unsigned int kMaxEntries = 128;

    StreamLogSink()
        : mEntryCount(0)
    {
        SetLevelThreshold(Dia::Observation::Log::LogLevel::kDebug);
        SetChannelFilter(Dia::Core::StringCRC("stream"), true);
    }

    void OnLogEntry(const Dia::Observation::Log::LogEntry& entry) override
    {
        if (mEntryCount < kMaxEntries)
            mEntries[mEntryCount++] = entry;
    }

    const char* GetName() const override { return "StreamLogSink"; }

    void Clear() { mEntryCount = 0; }

    unsigned int CountByLevel(Dia::Observation::Log::LogLevel level) const
    {
        unsigned int count = 0;
        for (unsigned int i = 0; i < mEntryCount; ++i)
            if (mEntries[i].level == level) ++count;
        return count;
    }

    bool HasMessageContaining(const char* substring) const
    {
        for (unsigned int i = 0; i < mEntryCount; ++i)
            if (strstr(mEntries[i].message, substring) != nullptr)
                return true;
        return false;
    }

private:
    Dia::Observation::Log::LogEntry mEntries[kMaxEntries];
    unsigned int mEntryCount;
};

class EventStreamCapacityTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mSink.Clear();
        Dia::Observation::Log::Logger::Instance().RegisterThreadBuffer();
        Dia::Observation::Log::Logger::Instance().RegisterSink(&mSink);
    }

    void TearDown() override
    {
        Dia::Observation::Log::Logger::Instance().UnregisterSink(&mSink);
        Dia::Observation::Log::Logger::Instance().UnregisterThreadBuffer();
    }

    void FlushLogs()
    {
        Dia::Observation::Log::Logger::Instance().FlushSync();
    }

    StreamLogSink mSink;
};

TEST_F(EventStreamCapacityTest, NinthReaderSucceedsWhenMaxReadersRaised)
{
    // maxReaders=16 must actually back a 16-slot allocation, not silently
    // cap at the compile-time kDefaultMaxReaders (8).
    EventStreamStore<int> estore(StringCRC("e_cap_16"),
        Dia::Core::StringCRC::kZero,
        /*capacity=*/EventStreamStore<int>::kDefaultCapacity,
        /*maxReaders=*/16u);

    int indices[9];
    for (int i = 0; i < 9; ++i)
        indices[i] = estore.RegisterReader();

    for (int i = 0; i < 9; ++i)
        EXPECT_GE(indices[i], 0) << "reader " << i << " should register successfully (maxReaders=16)";

    // All 9 indices must be distinct.
    for (int i = 0; i < 9; ++i)
        for (int j = i + 1; j < 9; ++j)
            EXPECT_NE(indices[i], indices[j]) << "reader indices " << i << " and " << j << " collided";

    // Ninth reader must actually receive independently-consumable events —
    // proves it's backed by real storage, not a stub slot.
    estore.Send(MakeEvent(42));
    DynamicArrayC<Event<int>, 32> out;
    estore.Consume(indices[8], out);
    ASSERT_EQ(out.Size(), 1u);
    EXPECT_EQ(out[0].payload, 42);
}

TEST_F(EventStreamCapacityTest, ReaderPastConfiguredCapacityLogsWarningNotSilentFailure)
{
    EventStreamStore<int> estore(StringCRC("e_cap_default"));  // default maxReaders=8

    for (int i = 0; i < 8; ++i)
        ASSERT_GE(estore.RegisterReader(), 0);

    // 9th reader exceeds the default 8-reader ceiling — must fail...
    int overflowIdx = estore.RegisterReader();
    EXPECT_EQ(overflowIdx, -1);

    // ...but must log a warning rather than failing silently.
    FlushLogs();
    EXPECT_GE(mSink.CountByLevel(Dia::Observation::Log::LogLevel::kWarning), 1u);
    EXPECT_TRUE(mSink.HasMessageContaining("stream.reader.capacity_exceeded"));
    EXPECT_TRUE(mSink.HasMessageContaining("e_cap_default"));
}

TEST_F(EventStreamCapacityTest, DefaultEightReaderCeilingUnchanged)
{
    // Regression guard: default-capacity behaviour must be identical to
    // pre-fix behaviour — exactly 8 readers succeed, the 9th is rejected.
    EventStreamStore<int> estore(StringCRC("e_cap_regress"));

    int successCount = 0;
    for (int i = 0; i < 9; ++i)
    {
        if (estore.RegisterReader() >= 0)
            ++successCount;
    }

    EXPECT_EQ(successCount, 8)
        << "exactly kDefaultMaxReaders (8) readers should succeed for a default-constructed store";
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

struct Str_FrameWriterModule : SimModule
{
    using SimModule::SimModule;
    static const StringCRC kTypeId;

    StreamWriter<int> mWriter{this, StringCRC("Str_FrameChannel")};
    int writeValue = 0;

    void OnConnectStreams(Application& app) override { mWriter.Connect(app); }
    StartResult DoStart() override { return StartResult::kReady; }
    void DoUpdate(const Dia::SimTime::SimTimeContext&) override
    {
        TimeAbsolute t = TimeAbsolute::CreateFromMilliseconds(0);
        mWriter.Write(writeValue, t);
    }
    StopResult DoStop() override { return StopResult::kDone; }
};
const StringCRC Str_FrameWriterModule::kTypeId("Str_FrameWriterModule");

struct Str_FrameReaderModule : SimModule
{
    using SimModule::SimModule;
    static const StringCRC kTypeId;

    StreamReader<int> mReader{this, StringCRC("Str_FrameChannel")};
    int lastRead = -1;

    void OnConnectStreams(Application& app) override { mReader.Connect(app); }
    StartResult DoStart() override { return StartResult::kReady; }
    void DoUpdate(const Dia::SimTime::SimTimeContext&) override
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

    ApplicationManifestV3 manifest;
    manifest.version = 2;

    StageDeclaration boot;
    boot.name = StringCRC("Boot");
    manifest.stages.Add(boot);
    manifest.initialStage = StringCRC("Boot");

    // Declare the stream so manifest-gating passes.
    StreamDeclaration sd;
    sd.id   = StringCRC("Str_FrameChannel");
    sd.kind = StringCRC("FrameStream");
    manifest.streams.Add(sd);

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

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

    g_frameWriter->writeValue = 99;
    app.Update(1.0f / 60.0f);
    app.Update(1.0f / 60.0f);

    EXPECT_EQ(g_frameReader->lastRead, 99)
        << "Reader should observe the value written by the writer";
}

// ---- EventStream integration -----------------------------------------------

struct Str_EventWriterModule : SimModule
{
    using SimModule::SimModule;
    static const StringCRC kTypeId;

    EventStreamWriter<int> mWriter{this, StringCRC("Str_EventChannel")};
    bool sendOnNextUpdate = false;
    int  sendValue        = 0;

    void OnConnectStreams(Application& app) override { mWriter.Connect(app); }
    StartResult DoStart() override { return StartResult::kReady; }
    void DoUpdate(const Dia::SimTime::SimTimeContext&) override
    {
        if (sendOnNextUpdate)
        {
            (void)mWriter.Send(sendValue);
            sendOnNextUpdate = false;
        }
    }
    StopResult DoStop() override { return StopResult::kDone; }
};
const StringCRC Str_EventWriterModule::kTypeId("Str_EventWriterModule");

struct Str_EventReaderModule : SimModule
{
    using SimModule::SimModule;
    static const StringCRC kTypeId;

    EventStreamReader<int> mReader{this, StringCRC("Str_EventChannel")};
    DynamicArrayC<Event<int>, 32> consumed;

    void OnConnectStreams(Application& app) override { mReader.Connect(app); }
    StartResult DoStart() override { return StartResult::kReady; }
    void DoUpdate(const Dia::SimTime::SimTimeContext&) override
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

    ApplicationManifestV3 manifest;
    manifest.version = 2;

    StageDeclaration boot;
    boot.name = StringCRC("Boot");
    manifest.stages.Add(boot);
    manifest.initialStage = StringCRC("Boot");

    // Declare the stream so manifest-gating passes.
    StreamDeclaration sd;
    sd.id   = StringCRC("Str_EventChannel");
    sd.kind = StringCRC("EventStream");
    manifest.streams.Add(sd);

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

    g_eventWriter->sendValue        = 77;
    g_eventWriter->sendOnNextUpdate = true;
    app.Update(1.0f / 60.0f);
    app.Update(1.0f / 60.0f);

    ASSERT_GE(g_eventReader->consumed.Size(), 1u)
        << "Reader should have consumed at least one event";
    EXPECT_EQ(g_eventReader->consumed[0].payload, 77)
        << "Reader should have consumed the event value sent by the writer";
}

////////////////////////////////////////////////////////////////////////////////
// Filename: TestStreamEnvelope.cpp
// GoogleTest suite — Event<T> envelope correctness
//
// Covers:
//   - timestamp non-decreasing across consecutive Sends
//   - senderCrc round-trip (EventStreamWriter stamps owner's instanceId crc)
//   - sequence contiguous and monotonic across Sends
//   - Fan-out: all readers receive identical envelope for the same event
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV2.h>
#include <DiaApplicationFlow/Streams/EventStreamStore.h>
#include <DiaApplicationFlow/Streams/EventStreamWriter.h>
#include <DiaApplicationFlow/Streams/EventStreamReader.h>
#include <DiaApplicationFlow/Streams/Event.h>
#include <DiaApplicationFlow/Streams/SendResult.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Time/TimeAbsolute.h>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace Dia::Core::Containers;

// ---------------------------------------------------------------------------
// Section 1: Direct EventStreamStore<T> envelope tests (no Application)
// ---------------------------------------------------------------------------

static Event<int> MakeEnvEvent(int payload)
{
    Event<int> ev;
    ev.timestampUs = TimeAbsolute::GetSystemTime().AsLongLongInMicroseconds();
    ev.senderCrc   = StringCRC("sender").Value();
    ev.sequence    = 0;  // stamped inside Send
    ev.payload     = payload;
    return ev;
}

// Timestamp on each event is non-decreasing.
TEST(StreamEnvelope, TimestampNonDecreasing)
{
    EventStreamStore<int> store(StringCRC("env_ts"));
    int rIdx = store.RegisterReader();

    store.Send(MakeEnvEvent(1));
    store.Send(MakeEnvEvent(2));
    store.Send(MakeEnvEvent(3));

    DynamicArrayC<Event<int>, 32> out;
    store.Consume(rIdx, out);

    ASSERT_EQ(out.Size(), 3u);
    EXPECT_LE(out[0].timestampUs, out[1].timestampUs)
        << "Timestamps should be non-decreasing";
    EXPECT_LE(out[1].timestampUs, out[2].timestampUs);
}

// senderCrc is preserved through the send/consume round-trip.
TEST(StreamEnvelope, SenderCrcRoundTrip)
{
    EventStreamStore<int> store(StringCRC("env_sender"));
    int rIdx = store.RegisterReader();

    const unsigned int expectedCrc = StringCRC("myModule").Value();

    Event<int> ev;
    ev.timestampUs = 0;
    ev.senderCrc   = expectedCrc;
    ev.sequence    = 0;
    ev.payload     = 42;
    store.Send(ev);

    DynamicArrayC<Event<int>, 8> out;
    store.Consume(rIdx, out);

    ASSERT_EQ(out.Size(), 1u);
    EXPECT_EQ(out[0].senderCrc, expectedCrc)
        << "senderCrc should be preserved through Send/Consume";
}

// Sequence numbers are monotonically increasing across consecutive Sends.
TEST(StreamEnvelope, SequenceMonotonic)
{
    EventStreamStore<int> store(StringCRC("env_seq_mono"));
    int rIdx = store.RegisterReader();

    store.Send(MakeEnvEvent(10));
    store.Send(MakeEnvEvent(20));
    store.Send(MakeEnvEvent(30));

    DynamicArrayC<Event<int>, 32> out;
    store.Consume(rIdx, out);

    ASSERT_EQ(out.Size(), 3u);
    EXPECT_LT(out[0].sequence, out[1].sequence)
        << "Sequence numbers should be strictly increasing";
    EXPECT_LT(out[1].sequence, out[2].sequence);
}

// Sequence numbers start from 0 for a fresh store.
TEST(StreamEnvelope, SequenceStartsAtZero)
{
    EventStreamStore<int> store(StringCRC("env_seq_zero"));
    int rIdx = store.RegisterReader();

    store.Send(MakeEnvEvent(1));

    DynamicArrayC<Event<int>, 8> out;
    store.Consume(rIdx, out);

    ASSERT_EQ(out.Size(), 1u);
    EXPECT_EQ(out[0].sequence, 0u)
        << "First event sequence should be 0";
}

// Sequence numbers are contiguous (no gaps).
TEST(StreamEnvelope, SequenceContiguous)
{
    EventStreamStore<int> store(StringCRC("env_seq_cont"));
    int rIdx = store.RegisterReader();

    const unsigned int count = 5;
    for (unsigned int i = 0; i < count; ++i)
        store.Send(MakeEnvEvent(static_cast<int>(i)));

    DynamicArrayC<Event<int>, 32> out;
    store.Consume(rIdx, out);

    ASSERT_EQ(out.Size(), count);
    for (unsigned int i = 1; i < count; ++i)
    {
        EXPECT_EQ(out[i].sequence, out[i - 1].sequence + 1u)
            << "Sequence numbers should be contiguous (no gaps) at index " << i;
    }
}

// Fan-out: all readers receive an identical envelope for the same send.
TEST(StreamEnvelope, FanOutReadersSameEnvelope)
{
    EventStreamStore<int> store(StringCRC("env_fanout"));
    int rA = store.RegisterReader();
    int rB = store.RegisterReader();
    int rC = store.RegisterReader();

    Event<int> original;
    original.timestampUs = 99999;
    original.senderCrc   = StringCRC("writer").Value();
    original.sequence    = 0;  // will be overwritten to 0 by store
    original.payload     = 77;
    store.Send(original);

    DynamicArrayC<Event<int>, 4> outA, outB, outC;
    store.Consume(rA, outA);
    store.Consume(rB, outB);
    store.Consume(rC, outC);

    ASSERT_EQ(outA.Size(), 1u);
    ASSERT_EQ(outB.Size(), 1u);
    ASSERT_EQ(outC.Size(), 1u);

    EXPECT_EQ(outA[0].sequence,  outB[0].sequence)  << "sequence must match across readers";
    EXPECT_EQ(outB[0].sequence,  outC[0].sequence);
    EXPECT_EQ(outA[0].senderCrc, outB[0].senderCrc) << "senderCrc must match across readers";
    EXPECT_EQ(outA[0].payload,   outB[0].payload)   << "payload must match across readers";
    EXPECT_EQ(outA[0].payload,   outC[0].payload);
}

// ---------------------------------------------------------------------------
// Section 2: EventStreamWriter envelope stamping (through Application)
//
// Verifies that EventStreamWriter stamps the module's instanceId CRC as
// senderCrc when sending.
// ---------------------------------------------------------------------------

struct Env_WriterModule : Module
{
    using Module::Module;
    static const StringCRC kTypeId;

    EventStreamWriter<int> mWriter{this, StringCRC("Env_Channel")};

    void OnConnectStreams(Application& app) override { mWriter.Connect(app); }
    StartResult DoStart() override { return StartResult::kReady; }
    void DoUpdate(float) override {}
    StopResult DoStop() override { return StopResult::kDone; }
};
const StringCRC Env_WriterModule::kTypeId("Env_WriterModule");

struct Env_ReaderModule : Module
{
    using Module::Module;
    static const StringCRC kTypeId;

    EventStreamReader<int> mReader{this, StringCRC("Env_Channel")};
    DynamicArrayC<Event<int>, 32> consumed;

    void OnConnectStreams(Application& app) override { mReader.Connect(app); }
    StartResult DoStart() override { return StartResult::kReady; }
    void DoUpdate(float) override { mReader.Consume(consumed); }
    StopResult DoStop() override { return StopResult::kDone; }
};
const StringCRC Env_ReaderModule::kTypeId("Env_ReaderModule");

static Env_WriterModule* g_envWriter = nullptr;
static Env_ReaderModule* g_envReader = nullptr;

static Module* CreateEnvWriter(const StringCRC& id)
{
    g_envWriter = new Env_WriterModule(id);
    return g_envWriter;
}
static Module* CreateEnvReader(const StringCRC& id)
{
    g_envReader = new Env_ReaderModule(id);
    return g_envReader;
}

static bool EnvPumpUntilAllActive(Application& app, const StringCRC& puId, int limit = 100)
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
            if (allActive) return true;
        }
        app.Update(1.0f / 60.0f);
    }
    return false;
}

TEST(StreamEnvelope, WriterStampsOwnerInstanceIdAsSenderCrc)
{
    g_envWriter = nullptr;
    g_envReader = nullptr;

    TypeRegistry reg;
    reg.Register(Env_WriterModule::kTypeId, CreateEnvWriter);
    reg.Register(Env_ReaderModule::kTypeId, CreateEnvReader);

    ApplicationManifestV2 manifest;
    manifest.version = 2;

    StageDeclaration boot;
    boot.name = StringCRC("Boot");
    manifest.stages.Add(boot);
    manifest.initialStage = StringCRC("Boot");

    StreamDeclaration sd;
    sd.id   = StringCRC("Env_Channel");
    sd.kind = StringCRC("EventStream");
    manifest.streams.Add(sd);

    ProcessingUnitDeclaration pu;
    pu.instanceId      = StringCRC("MainPU");
    pu.frequencyHz     = 60.0f;
    pu.dedicatedThread = false;

    const StringCRC writerInstanceId("envWriterInst");

    {
        ModuleDeclaration mod;
        mod.instanceId     = writerInstanceId;
        mod.typeId         = Env_WriterModule::kTypeId;
        mod.startTimeoutMs = 10000.0f;
        mod.stopTimeoutMs  = 5000.0f;
        mod.stages.Add(StringCRC("Boot"));
        pu.modules.Add(mod);
    }
    {
        ModuleDeclaration mod;
        mod.instanceId     = StringCRC("envReaderInst");
        mod.typeId         = Env_ReaderModule::kTypeId;
        mod.startTimeoutMs = 10000.0f;
        mod.stopTimeoutMs  = 5000.0f;
        mod.stages.Add(StringCRC("Boot"));
        pu.modules.Add(mod);
    }
    manifest.processingUnits.Add(pu);

    Application app(manifest, reg);
    ASSERT_TRUE(app.Start());
    ASSERT_TRUE(EnvPumpUntilAllActive(app, StringCRC("MainPU"), 50));
    ASSERT_NE(g_envWriter, nullptr);
    ASSERT_NE(g_envReader, nullptr);

    // Send one event from the writer.
    (void)g_envWriter->mWriter.Send(100);
    app.Update(1.0f / 60.0f);
    app.Update(1.0f / 60.0f);

    ASSERT_GE(g_envReader->consumed.Size(), 1u);
    EXPECT_EQ(g_envReader->consumed[0].senderCrc, writerInstanceId.Value())
        << "EventStreamWriter should stamp senderCrc with owner module's instanceId CRC";
    EXPECT_EQ(g_envReader->consumed[0].payload, 100);
}

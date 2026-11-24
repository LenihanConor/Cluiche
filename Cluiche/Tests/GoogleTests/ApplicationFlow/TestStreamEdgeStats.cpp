////////////////////////////////////////////////////////////////////////////////
// Filename: TestStreamEdgeStats.cpp
// GoogleTest suite — Trace-Linked Streams Task 5: Edge statistics with MetricInfo
// (AC5: per-stream edge metrics)
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaStreams/FrameStreamStore.h>
#include <DiaStreams/EventStreamStore.h>
#include <DiaStreams/Event.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

using namespace Dia::ApplicationFlow;
using namespace Dia::Observation::Metric;
using namespace Dia::Core;
using namespace Dia::Core::Containers;

namespace {
    struct TestPayload
    {
        int value;
    };

    Event<TestPayload> MakeEvent(int value)
    {
        Event<TestPayload> evt;
        evt.payload.value = value;
        evt.timestampUs = 0;
        return evt;
    }
}

// AC5: FrameStreamStore registers metrics with MetricInfo
TEST(StreamEdgeStats, FrameStreamRegistersMetricsWithInfo)
{
    FrameStreamStore<TestPayload> store(StringCRC("edge_frame"));
    auto& registry = MetricRegistry::Instance();

    // Check that metrics were registered
    Counter* writes = registry.FindCounter(StringCRC("stream.frame.edge_frame.writes"));
    Counter* bytes = registry.FindCounter(StringCRC("stream.frame.edge_frame.bytes"));
    Gauge* dataAge = registry.FindGauge(StringCRC("stream.frame.edge_frame.data_age_ns"));
    Gauge* writeCost = registry.FindGauge(StringCRC("stream.frame.edge_frame.write_cost_ns"));
    Gauge* readCost = registry.FindGauge(StringCRC("stream.frame.edge_frame.read_cost_ns"));

    EXPECT_NE(writes, nullptr) << "Writes counter should be registered";
    EXPECT_NE(bytes, nullptr) << "Bytes counter should be registered";
    EXPECT_NE(dataAge, nullptr) << "Data age gauge should be registered";
    EXPECT_NE(writeCost, nullptr) << "Write cost gauge should be registered";
    EXPECT_NE(readCost, nullptr) << "Read cost gauge should be registered";
}

// AC5: FrameStreamStore updates write counters
TEST(StreamEdgeStats, FrameStreamUpdatesWriteCounters)
{
    FrameStreamStore<TestPayload> store(StringCRC("edge_frame_write"));
    auto& registry = MetricRegistry::Instance();

    Counter* writes = registry.FindCounter(StringCRC("stream.frame.edge_frame_write.writes"));
    Counter* bytes = registry.FindCounter(StringCRC("stream.frame.edge_frame_write.bytes"));
    ASSERT_NE(writes, nullptr);
    ASSERT_NE(bytes, nullptr);

    double writesBefore = writes->Value();
    double bytesBefore = bytes->Value();

    TestPayload payload = { 42 };
    store.Write(payload, TimeAbsolute::CreateFromMilliseconds(0));

    EXPECT_EQ(writes->Value(), writesBefore + 1) << "Writes counter should increment";
    EXPECT_EQ(bytes->Value(), bytesBefore + sizeof(TestPayload)) << "Bytes counter should add payload size";
}

// AC5: FrameStreamStore updates read metrics and data age
TEST(StreamEdgeStats, FrameStreamUpdatesReadMetricsAndDataAge)
{
    FrameStreamStore<TestPayload> store(StringCRC("edge_frame_read"));
    auto& registry = MetricRegistry::Instance();

    Gauge* dataAge = registry.FindGauge(StringCRC("stream.frame.edge_frame_read.data_age_ns"));
    Gauge* readCost = registry.FindGauge(StringCRC("stream.frame.edge_frame_read.read_cost_ns"));
    ASSERT_NE(dataAge, nullptr);
    ASSERT_NE(readCost, nullptr);

    TestPayload payload = { 100 };
    store.Write(payload, TimeAbsolute::CreateFromMilliseconds(0));

    // Small delay so data age is measurable
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    StreamLink link;
    const TestPayload* data = store.FetchLatestLinked(link);
    ASSERT_NE(data, nullptr);

    EXPECT_GT(dataAge->Value(), 0.0) << "Data age should be positive after write-then-read";
    EXPECT_GT(readCost->Value(), 0.0) << "Read cost should be positive";
}

// AC5: EventStreamStore registers metrics with MetricInfo
TEST(StreamEdgeStats, EventStreamRegistersMetricsWithInfo)
{
    EventStreamStore<TestPayload> store(StringCRC("edge_event"),
                                        StringCRC::kZero,
                                        /*capacity=*/256);
    auto& registry = MetricRegistry::Instance();

    // Check that edge metrics were registered
    Counter* writes = registry.FindCounter(StringCRC("stream.edge_event.edge.writes"));
    Counter* bytes = registry.FindCounter(StringCRC("stream.edge_event.edge.bytes"));
    Gauge* dataAge = registry.FindGauge(StringCRC("stream.edge_event.edge.data_age_ns"));
    Gauge* writeCost = registry.FindGauge(StringCRC("stream.edge_event.edge.write_cost_ns"));
    Gauge* readCost = registry.FindGauge(StringCRC("stream.edge_event.edge.read_cost_ns"));

    EXPECT_NE(writes, nullptr) << "Edge writes counter should be registered";
    EXPECT_NE(bytes, nullptr) << "Edge bytes counter should be registered";
    EXPECT_NE(dataAge, nullptr) << "Edge data age gauge should be registered";
    EXPECT_NE(writeCost, nullptr) << "Edge write cost gauge should be registered";
    EXPECT_NE(readCost, nullptr) << "Edge read cost gauge should be registered";
}

// AC5: EventStreamStore updates write counters
TEST(StreamEdgeStats, EventStreamUpdatesWriteCounters)
{
    EventStreamStore<TestPayload> store(StringCRC("edge_event_write"),
                                        StringCRC::kZero,
                                        /*capacity=*/256);
    int rIdx = store.RegisterReader();
    ASSERT_GE(rIdx, 0);

    auto& registry = MetricRegistry::Instance();
    Counter* writes = registry.FindCounter(StringCRC("stream.edge_event_write.edge.writes"));
    Counter* bytes = registry.FindCounter(StringCRC("stream.edge_event_write.edge.bytes"));
    ASSERT_NE(writes, nullptr);
    ASSERT_NE(bytes, nullptr);

    double writesBefore = writes->Value();
    double bytesBefore = bytes->Value();

    store.Send(MakeEvent(42));

    EXPECT_EQ(writes->Value(), writesBefore + 1) << "Writes counter should increment";
    EXPECT_EQ(bytes->Value(), bytesBefore + sizeof(TestPayload)) << "Bytes counter should add payload size";
}

// AC5: EventStreamStore updates read metrics and data age
TEST(StreamEdgeStats, EventStreamUpdatesReadMetricsAndDataAge)
{
    EventStreamStore<TestPayload> store(StringCRC("edge_event_read"),
                                        StringCRC::kZero,
                                        /*capacity=*/256);
    int rIdx = store.RegisterReader();
    ASSERT_GE(rIdx, 0);

    auto& registry = MetricRegistry::Instance();
    Gauge* dataAge = registry.FindGauge(StringCRC("stream.edge_event_read.edge.data_age_ns"));
    Gauge* readCost = registry.FindGauge(StringCRC("stream.edge_event_read.edge.read_cost_ns"));
    ASSERT_NE(dataAge, nullptr);
    ASSERT_NE(readCost, nullptr);

    store.Send(MakeEvent(100));

    // Small delay so data age is measurable
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    DynamicArrayC<Event<TestPayload>, 32> events;
    store.Consume(rIdx, events);
    ASSERT_EQ(events.Size(), 1u);

    EXPECT_GT(dataAge->Value(), 0.0) << "Data age should be positive after send-then-consume";
    EXPECT_GT(readCost->Value(), 0.0) << "Read cost should be positive";
}

// AC5: Write rate derived by consumer (counters only, no per-write rate computation)
TEST(StreamEdgeStats, WriteRateDerivedByConsumer)
{
    FrameStreamStore<TestPayload> store(StringCRC("edge_rate"));
    auto& registry = MetricRegistry::Instance();

    Counter* writes = registry.FindCounter(StringCRC("stream.frame.edge_rate.writes"));
    ASSERT_NE(writes, nullptr);

    // Write multiple times
    for (int i = 0; i < 5; ++i)
    {
        TestPayload payload = { i };
        store.Write(payload, TimeAbsolute::CreateFromMilliseconds(0));
    }

    EXPECT_EQ(writes->Value(), 5.0) << "Counter should accumulate, rate is derived by consumer";
}

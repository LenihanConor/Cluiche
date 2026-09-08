////////////////////////////////////////////////////////////////////////////////
// Filename: TestStreamTelemetry.cpp
// GoogleTest suite — EventStreamStore metric instrumentation (Task 32/33)
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaStreams/EventStreamStore.h>
#include <DiaStreams/Event.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Testing/MetricFixture.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::Core;
using namespace Dia::Observation::Metric;
using namespace Dia::ApplicationFlow;

static Event<int> MakeEvent(int payload)
{
    Event<int> ev;
    ev.payload = payload;
    return ev;
}

struct StreamTelemetryTest : ::testing::Test
{
    MetricFixture fixture;
};

TEST_F(StreamTelemetryTest, MetricsRegisteredOnConstruction)
{
    EventStreamStore<int> store(StringCRC("tel_reg"), StringCRC("int"), 8);

    Gauge*   sizeG = MetricRegistry::Instance().FindGauge(StringCRC("stream.tel_reg.current_size"));
    Counter* dropC = MetricRegistry::Instance().FindCounter(StringCRC("stream.tel_reg.drops_total"));
    EXPECT_NE(sizeG, nullptr) << "current_size gauge not registered";
    EXPECT_NE(dropC, nullptr) << "drops_total counter not registered";
}

TEST_F(StreamTelemetryTest, CurrentSizeGaugeTracksQueueFill)
{
    EventStreamStore<int> store(StringCRC("tel_fill"), StringCRC("int"), 8);
    int readerId = store.RegisterReader();
    ASSERT_GE(readerId, 0);

    store.Send(MakeEvent(1));
    store.Send(MakeEvent(2));
    store.Send(MakeEvent(3));
    store.Flush();

    Gauge* g = MetricRegistry::Instance().FindGauge(StringCRC("stream.tel_fill.current_size"));
    ASSERT_NE(g, nullptr);
    EXPECT_GE(g->Value(), 3.0) << "3 events queued, gauge should reflect";
}

TEST_F(StreamTelemetryTest, DropsCounterIncrementsOnOverflow)
{
    EventStreamStore<int> store(StringCRC("tel_drop"), StringCRC("int"), 2,
                                4, OverflowPolicy::kDropNewest);
    int readerId = store.RegisterReader();
    ASSERT_GE(readerId, 0);

    store.Send(MakeEvent(1));
    store.Send(MakeEvent(2));
    store.Send(MakeEvent(3)); // should drop
    store.Flush();

    Counter* c = MetricRegistry::Instance().FindCounter(StringCRC("stream.tel_drop.drops_total"));
    ASSERT_NE(c, nullptr);
    EXPECT_GE(c->Value(), 1u) << "One event dropped, counter should be >= 1";
}

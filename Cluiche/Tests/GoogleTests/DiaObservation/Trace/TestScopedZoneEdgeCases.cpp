#include <gtest/gtest.h>
#include <DiaObservation/Trace/Tracer.h>
#include <DiaObservation/Trace/ScopedZone.h>
#include <DiaObservation/Trace/ITraceSink.h>
#include <DiaCore/CRC/StringCRC.h>

#include <thread>
#include <chrono>
#include <atomic>
#include <cstring>

using namespace Dia::Observation::Trace;

namespace
{
    const char* kTestTracePath = "test_scoped_edge.jsonl";

    struct CountingSink : public ITraceSink
    {
        std::atomic<unsigned int> count{0};
        void OnSpan(const SpanRecord&) override
        {
            count.fetch_add(1, std::memory_order_relaxed);
        }
    };
}

struct ScopedZoneEdgeCaseTest : ::testing::Test
{
    void SetUp() override
    {
        Tracer::Instance().Stop();
    }

    void TearDown() override
    {
        Tracer::Instance().Stop();
        remove(kTestTracePath);
    }
};

TEST_F(ScopedZoneEdgeCaseTest, StopMidScope_DestructorNoOp)
{
    Tracer::Instance().Start(kTestTracePath, Category::kAll, "sess", 0);
    Tracer::Instance().RegisterThreadSpanBuffer();

    CountingSink sink;
    Tracer::Instance().RegisterTraceSink(&sink);

    {
        ScopedZone outerZone(Dia::Core::StringCRC("Outer"), Category::kAll);

        // Stop the tracer while the zone is still open
        // Note: UnregisterThreadSpanBuffer before stop to cleanly remove ring
        Tracer::Instance().UnregisterThreadSpanBuffer();
        Tracer::Instance().Stop();

        // Zone destructs here — should not crash or write to dead state
    }

    // No crash is the primary assertion
    EXPECT_EQ(sink.count.load(), 0u);

    Tracer::Instance().UnregisterTraceSink(&sink);
}

TEST_F(ScopedZoneEdgeCaseTest, ZoneCreatedBeforeStart_NoOp)
{
    // Tracer not started yet
    {
        ScopedZone zone(Dia::Core::StringCRC("BeforeStart"), Category::kAll);
        // Now start it mid-scope
        Tracer::Instance().Start(kTestTracePath, Category::kAll, "sess", 0);
    }

    // Zone was constructed before start, so spanId==0, destructor should no-op
    Tracer::Instance().Stop();
    SUCCEED();
}

TEST_F(ScopedZoneEdgeCaseTest, DeeplyNested_63Levels)
{
    Tracer::Instance().Start(kTestTracePath, Category::kAll, "sess", 0);
    // Unregister/re-register to ensure clean thread-local state
    Tracer::Instance().UnregisterThreadSpanBuffer();
    Tracer::Instance().RegisterThreadSpanBuffer();

    CountingSink sink;
    Tracer::Instance().RegisterTraceSink(&sink);

    // Open 63 nested zones (stays within the 64-capacity stack with margin)
    static const int kDepth = 63;
    SpanRecord records[63];
    for (int i = 0; i < kDepth; ++i)
    {
        std::memset(&records[i], 0, sizeof(SpanRecord));
        records[i].name          = Dia::Core::StringCRC("Deep");
        records[i].category      = Category::kAll;
        records[i].startSteadyNs = static_cast<uint64_t>(i);
        records[i].threadId      = 1;
        Tracer::Instance().OnSpanOpen(records[i]);
    }

    // Close them all
    for (int i = kDepth - 1; i >= 0; --i)
    {
        records[i].endSteadyNs = static_cast<uint64_t>(i + 100);
        Tracer::Instance().OnSpanClose(records[i]);
    }

    // Wait for drain
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    Tracer::Instance().UnregisterThreadSpanBuffer();
    Tracer::Instance().Stop();

    EXPECT_EQ(sink.count.load(), static_cast<unsigned int>(kDepth));
    Tracer::Instance().UnregisterTraceSink(&sink);
}

TEST_F(ScopedZoneEdgeCaseTest, EndTimestamp_GreaterThanStart)
{
    Tracer::Instance().Start(kTestTracePath, Category::kAll, "sess", 0);
    Tracer::Instance().RegisterThreadSpanBuffer();

    struct TimingSink : public ITraceSink
    {
        SpanRecord captured;
        std::atomic<bool> received{false};
        void OnSpan(const SpanRecord& s) override
        {
            captured = s;
            received.store(true);
        }
    };

    TimingSink sink;
    Tracer::Instance().RegisterTraceSink(&sink);

    {
        ScopedZone zone(Dia::Core::StringCRC("Timing"), Category::kAll);
        // Brief work to ensure measurable time
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    // Wait for drain
    for (int i = 0; i < 100 && !sink.received.load(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    ASSERT_TRUE(sink.received.load());
    EXPECT_GT(sink.captured.endSteadyNs, sink.captured.startSteadyNs);

    Tracer::Instance().UnregisterTraceSink(&sink);
    Tracer::Instance().UnregisterThreadSpanBuffer();
}

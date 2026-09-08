#include <gtest/gtest.h>
#include <DiaObservation/Trace/Tracer.h>
#include <DiaObservation/Trace/ScopedZone.h>
#include <DiaObservation/Trace/ITraceSink.h>
#include <DiaCore/CRC/StringCRC.h>

#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <mutex>
#include <cstring>

using namespace Dia::Observation::Trace;

namespace
{
    const char* kTestTracePath = "test_trace_output.jsonl";

    struct CaptureTraceSink : public ITraceSink
    {
        static const unsigned int kMaxSpans = 512;
        SpanRecord spans[kMaxSpans];
        std::atomic<unsigned int> count{0};
        std::mutex mtx;

        void OnSpan(const SpanRecord& span) override
        {
            std::lock_guard<std::mutex> lock(mtx);
            unsigned int c = count.load(std::memory_order_relaxed);
            if (c < kMaxSpans)
            {
                spans[c] = span;
                count.store(c + 1, std::memory_order_release);
            }
        }
    };

    void WaitForDrain(CaptureTraceSink* sink, unsigned int expected, int maxMs = 100)
    {
        for (int i = 0; i < maxMs && sink->count.load(std::memory_order_acquire) < expected; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

struct TracerTest : ::testing::Test
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

TEST_F(TracerTest, StartAndStop)
{
    EXPECT_FALSE(Tracer::Instance().IsStarted());
    EXPECT_TRUE(Tracer::Instance().Start(kTestTracePath, Category::kAll, "test-session", 0));
    EXPECT_TRUE(Tracer::Instance().IsStarted());
    Tracer::Instance().Stop();
    EXPECT_FALSE(Tracer::Instance().IsStarted());
}

TEST_F(TracerTest, DoubleStartReturnsFalse)
{
    EXPECT_TRUE(Tracer::Instance().Start(kTestTracePath, Category::kAll, "sess", 0));
    EXPECT_FALSE(Tracer::Instance().Start(kTestTracePath, Category::kAll, "sess2", 0));
}

TEST_F(TracerTest, SpanNoOp_WhenNotStarted)
{
    SpanRecord rec;
    std::memset(&rec, 0, sizeof(rec));
    rec.name = Dia::Core::StringCRC("TestSpan");
    Tracer::Instance().OnSpanOpen(rec);
    EXPECT_EQ(rec.spanId, 0u);
}

TEST_F(TracerTest, SpanNoOp_WhenThreadNotRegistered)
{
    Tracer::Instance().Start(kTestTracePath, Category::kAll, "sess", 0);

    SpanRecord rec;
    std::memset(&rec, 0, sizeof(rec));
    rec.name = Dia::Core::StringCRC("TestSpan");
    Tracer::Instance().OnSpanOpen(rec);
    EXPECT_EQ(rec.spanId, 0u);
}

TEST_F(TracerTest, RegisterThread_SpanGetsIds)
{
    Tracer::Instance().Start(kTestTracePath, Category::kAll, "sess", 0);
    Tracer::Instance().RegisterThreadSpanBuffer();

    SpanRecord rec;
    std::memset(&rec, 0, sizeof(rec));
    rec.name     = Dia::Core::StringCRC("TestSpan");
    rec.category = Category::kAll;
    Tracer::Instance().OnSpanOpen(rec);

    EXPECT_NE(rec.spanId, 0u);
    EXPECT_NE(rec.traceId, 0u);
    EXPECT_EQ(rec.parentSpanId, 0u);

    Tracer::Instance().OnSpanClose(rec);
    Tracer::Instance().UnregisterThreadSpanBuffer();
}

TEST_F(TracerTest, NestedSpans_FormParentChain)
{
    Tracer::Instance().Start(kTestTracePath, Category::kAll, "sess", 0);
    Tracer::Instance().RegisterThreadSpanBuffer();

    SpanRecord parent;
    std::memset(&parent, 0, sizeof(parent));
    parent.name     = Dia::Core::StringCRC("Parent");
    parent.category = Category::kAll;
    Tracer::Instance().OnSpanOpen(parent);

    SpanRecord child;
    std::memset(&child, 0, sizeof(child));
    child.name     = Dia::Core::StringCRC("Child");
    child.category = Category::kAll;
    Tracer::Instance().OnSpanOpen(child);

    EXPECT_EQ(child.traceId, parent.traceId);
    EXPECT_EQ(child.parentSpanId, parent.spanId);
    EXPECT_NE(child.spanId, parent.spanId);

    Tracer::Instance().OnSpanClose(child);
    Tracer::Instance().OnSpanClose(parent);
    Tracer::Instance().UnregisterThreadSpanBuffer();
}

TEST_F(TracerTest, DrainDispatchesToSink)
{
    Tracer::Instance().Start(kTestTracePath, Category::kAll, "sess", 0);
    Tracer::Instance().RegisterThreadSpanBuffer();

    CaptureTraceSink sink;
    Tracer::Instance().RegisterTraceSink(&sink);

    SpanRecord rec;
    std::memset(&rec, 0, sizeof(rec));
    rec.name          = Dia::Core::StringCRC("Drained");
    rec.category      = Category::kAll;
    rec.startSteadyNs = 100;
    Tracer::Instance().OnSpanOpen(rec);
    rec.endSteadyNs = 200;
    Tracer::Instance().OnSpanClose(rec);

    WaitForDrain(&sink, 1);

    EXPECT_GE(sink.count.load(), 1u);
    EXPECT_EQ(sink.spans[0].name.Value(), Dia::Core::StringCRC("Drained").Value());

    Tracer::Instance().UnregisterTraceSink(&sink);
    Tracer::Instance().UnregisterThreadSpanBuffer();
}

TEST_F(TracerTest, UnregisterSink_StopsReceiving)
{
    Tracer::Instance().Start(kTestTracePath, Category::kAll, "sess", 0);
    Tracer::Instance().RegisterThreadSpanBuffer();

    CaptureTraceSink sink;
    Tracer::Instance().RegisterTraceSink(&sink);
    Tracer::Instance().UnregisterTraceSink(&sink);

    SpanRecord rec;
    std::memset(&rec, 0, sizeof(rec));
    rec.name = Dia::Core::StringCRC("After");
    Tracer::Instance().OnSpanOpen(rec);
    rec.endSteadyNs = 100;
    Tracer::Instance().OnSpanClose(rec);

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(sink.count.load(), 0u);

    Tracer::Instance().UnregisterThreadSpanBuffer();
}

TEST_F(TracerTest, ScopedZone_AssignsSpanId)
{
    Tracer::Instance().Start(kTestTracePath, Category::kAll, "sess", 0);
    Tracer::Instance().RegisterThreadSpanBuffer();

    CaptureTraceSink sink;
    Tracer::Instance().RegisterTraceSink(&sink);

    {
        ScopedZone zone(Dia::Core::StringCRC("ScopedTest"), Category::kAll);
    }

    WaitForDrain(&sink, 1);
    EXPECT_GE(sink.count.load(), 1u);

    Tracer::Instance().UnregisterTraceSink(&sink);
    Tracer::Instance().UnregisterThreadSpanBuffer();
}

TEST_F(TracerTest, ScopedZone_NoOp_WhenNotStarted)
{
    CaptureTraceSink sink;
    {
        ScopedZone zone(Dia::Core::StringCRC("Noop"), Category::kAll);
    }
    EXPECT_EQ(sink.count.load(), 0u);
}

TEST_F(TracerTest, MultiThread_SpansCollected)
{
    Tracer::Instance().Start(kTestTracePath, Category::kAll, "sess", 0);

    CaptureTraceSink sink;
    Tracer::Instance().RegisterTraceSink(&sink);

    const int kThreads = 4;
    const int kSpansPerThread = 10;
    std::atomic<int> readyCount{0};
    std::atomic<bool> go{false};

    auto worker = [&]()
    {
        Tracer::Instance().RegisterThreadSpanBuffer();
        readyCount.fetch_add(1);
        while (!go.load())
            std::this_thread::yield();

        for (int i = 0; i < kSpansPerThread; ++i)
        {
            ScopedZone zone(Dia::Core::StringCRC("MT"), Category::kAll);
        }

        // Wait for drain to pick up all our spans before unregistering
        WaitForDrain(&sink, static_cast<unsigned int>(kThreads * kSpansPerThread), 200);

        Tracer::Instance().UnregisterThreadSpanBuffer();
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreads; ++i)
        threads.emplace_back(worker);

    while (readyCount.load() < kThreads)
        std::this_thread::yield();
    go.store(true);

    for (auto& t : threads)
        t.join();

    EXPECT_GE(sink.count.load(), static_cast<unsigned int>(kThreads * kSpansPerThread));

    Tracer::Instance().UnregisterTraceSink(&sink);
}

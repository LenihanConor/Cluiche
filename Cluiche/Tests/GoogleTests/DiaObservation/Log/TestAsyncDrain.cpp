#include <gtest/gtest.h>

#include <DiaObservation/Log/Logger.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Testing/MockSink.h>

#include <thread>
#include <chrono>

using namespace Dia::Observation::Log;

// Each test manages its own sink registration to avoid cross-test state.
// The drain thread is started once (on first RegisterSink) and runs for the
// lifetime of the process — tests must not call Stop() except when testing Stop().
// Tests that call Stop() are tagged accordingly and run last.

// Helper: register buffer + sink, run body, unregister.
// NOT used by Stop-tests (they manage their own lifecycle).
struct ScopedLoggerSetup
{
    MockSink& sink;

    explicit ScopedLoggerSetup(MockSink& s) : sink(s)
    {
        Logger::Instance().RegisterThreadBuffer();
        Logger::Instance().RegisterSink(&sink);
    }
    ~ScopedLoggerSetup()
    {
        Logger::Instance().UnregisterSink(&sink);
        Logger::Instance().UnregisterThreadBuffer();
    }
};

// AC17: sink is called from a thread that is NOT the test's main thread.
TEST(AsyncDrainTest, DrainHappensOffMainThread)
{
    MockSink sink;
    ScopedLoggerSetup setup(sink);

    const std::thread::id mainThread = std::this_thread::get_id();

    for (int i = 0; i < 100; ++i)
        DIA_LOG_INFO("Test", "entry %d", i);

    // Wait up to 50ms for the drain thread to process.
    for (int waited = 0; waited < 50 && sink.Count() == 0; ++waited)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    ASSERT_GT(sink.Count(), 0u) << "No entries received within 50ms";

    bool anyOffMain = false;
    for (unsigned int i = 0; i < sink.Count(); ++i)
    {
        if (sink.CallingThreadAt(i) != mainThread)
        {
            anyOffMain = true;
            break;
        }
    }
    EXPECT_TRUE(anyOffMain) << "All entries delivered on main thread — drain is not async";
}

// AC20: DispatchImmediate delivers on the calling thread, not the drain thread.
TEST(AsyncDrainTest, AssertBypassesDrain)
{
    MockSink sink;
    ScopedLoggerSetup setup(sink);

    const std::thread::id mainThread = std::this_thread::get_id();

    LogEntry e;
    e.level   = LogLevel::kError;
    e.channel = Dia::Core::StringCRC("Assert");
    snprintf(e.message, sizeof(e.message), "assert msg");

    Logger::Instance().DispatchImmediate(e);

    ASSERT_EQ(sink.Count(), 1u);
    EXPECT_EQ(sink.CallingThreadAt(0), mainThread)
        << "DispatchImmediate must deliver on the calling thread";
}

// AC22: entries pushed before sink registration arrive once drain runs.
TEST(AsyncDrainTest, EntriesArrivedBeforeSinkRegistrationAreNotLost)
{
    // Register a buffer, push 10 entries, THEN register the sink.
    Logger::Instance().RegisterThreadBuffer();

    const unsigned int kBurst = 10;
    for (unsigned int i = 0; i < kBurst; ++i)
        DIA_LOG_INFO("Test", "burst %u", i);

    MockSink sink;
    Logger::Instance().RegisterSink(&sink); // Drain thread already running; will pick these up.

    // Wait up to 100ms.
    for (int waited = 0; waited < 100 && sink.Count() < kBurst; ++waited)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    Logger::Instance().UnregisterSink(&sink);
    Logger::Instance().UnregisterThreadBuffer();

    EXPECT_EQ(sink.Count(), kBurst);
}

// AC18 + AC14: Stop() drains all pending entries and the thread joins.
// This test calls Stop() — the drain thread will not restart after this.
// It must run LAST in this file (GTest runs in declaration order within a suite).
TEST(AsyncDrainTest, StopDrainsAllPendingEntries)
{
    MockSink sink;
    Logger::Instance().RegisterThreadBuffer();
    Logger::Instance().RegisterSink(&sink);

    const unsigned int kCount = 200;
    for (unsigned int i = 0; i < kCount; ++i)
        DIA_LOG_INFO("Test", "entry %u", i);

    Logger::Instance().Stop(); // Blocks until drain thread joins and final flush runs.

    Logger::Instance().UnregisterSink(&sink);
    Logger::Instance().UnregisterThreadBuffer();

    EXPECT_EQ(sink.Count(), kCount);
}

// Idempotent Stop: a second call must be a no-op (no hang or crash).
TEST(AsyncDrainTest, StopIsIdempotent)
{
    // Drain thread is already stopped from previous test — this must not hang.
    EXPECT_NO_FATAL_FAILURE(Logger::Instance().Stop());
}

// TestJobSystem.cpp - Google Test unit tests for JobSystem
//
// Tests JobSystem from DiaCore Threading subsystem

#include <gtest/gtest.h>
#include <DiaCore/Threading/JobSystem.h>
#include <DiaCore/Threading/Thread.h>
#include <DiaCore/Threading/Atomic.h>
#include <DiaCore/Core/Assert.h>
#include <chrono>
#include <memory>

using namespace Dia::Core;

namespace
{
    // Recording assert handler for testing DIA_ASSERT behavior
    Atomic<int> gAssertCount{0};

    void RecordingAssertHandler(const char*, const char*, int, const char*, ...)
    {
        gAssertCount.Increment();
    }
}

// ==============================================================================
// Submit / Wait / IsComplete
// ==============================================================================

TEST(JobSystemInstance, Submit_ExecutesFunction)
{
    Atomic<int> counter{0};

    JobSystem js;
    js.Initialize(2);

    JobHandle h = js.Submit([&]() {
        counter.Increment();
    });

    js.Wait(h);

    EXPECT_EQ(counter.Load(), 1);

    js.Shutdown();
}

TEST(JobSystemInstance, WaitFromInsideRunningJob_DoesNotDeadlock)
{
    // With a 2-thread pool:
    //   - Thread A picks up and runs 'outer'
    //   - 'outer' calls Wait(inner), which blocks Thread A
    //   - Thread B picks up and runs 'inner'
    //   - 'inner' completes, Thread A unblocks, 'outer' completes
    //
    // With a 1-thread pool, this WOULD deadlock (known limitation):
    //   - Thread A runs 'outer'
    //   - 'outer' calls Wait(inner), blocking the only worker
    //   - No thread available to run 'inner'

    JobSystem js;
    js.Initialize(2);

    JobHandle inner = js.Submit([]() {
        ThisThread::SleepMs(20);
    });

    JobHandle outer = js.Submit([&]() {
        js.Wait(inner);
    });

    auto start = std::chrono::steady_clock::now();
    while (!js.IsComplete(outer))
    {
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(5))
        {
            FAIL() << "Deadlock detected: outer Wait did not complete within 5 seconds";
        }
        ThisThread::Yield();
    }

    js.Wait(outer);
    js.Shutdown();
}

TEST(JobSystemInstance, SubmitAfterShutdown_Asserts)
{
#ifdef DEBUG
    auto* prevAssertFunc = Dia::Core::g_pAssertFunc;
    Dia::Core::g_pAssertFunc = RecordingAssertHandler;
    gAssertCount.Store(0);

    JobSystem js;
    js.Initialize(2);
    js.Shutdown();

    JobHandle h = js.Submit([]() {});  // Should fire DIA_ASSERT then return invalid handle

    EXPECT_GT(gAssertCount.Load(), 0) << "Expected DIA_ASSERT to fire on Submit-after-Shutdown";
    EXPECT_FALSE(h.IsValid()) << "Submit after Shutdown must return an invalid handle";

    Dia::Core::g_pAssertFunc = prevAssertFunc;
#else
    SUCCEED() << "DIA_ASSERT compiles out in Release; behavior not testable here";
#endif
}

TEST(JobSystemInstance, DoubleInitialize_Asserts)
{
#ifdef DEBUG
    auto* prevAssertFunc = Dia::Core::g_pAssertFunc;
    Dia::Core::g_pAssertFunc = RecordingAssertHandler;
    gAssertCount.Store(0);

    JobSystem js;
    js.Initialize(2);
    js.Initialize(2);  // Should fire DIA_ASSERT and be a no-op

    EXPECT_GT(gAssertCount.Load(), 0) << "Expected DIA_ASSERT to fire on double Initialize";

    // Pool is still operational from the first Initialize
    Atomic<int> counter{0};
    JobHandle h = js.Submit([&]() { counter.Increment(); });
    js.Wait(h);
    EXPECT_EQ(counter.Load(), 1);

    Dia::Core::g_pAssertFunc = prevAssertFunc;
    js.Shutdown();
#else
    SUCCEED() << "DIA_ASSERT compiles out in Release; behavior not testable here";
#endif
}

TEST(JobSystemInstance, ReInitializeAfterShutdown_ProducesFreshPool)
{
    Atomic<int> counter{0};

    JobSystem js;

    js.Initialize(2);
    JobHandle h1 = js.Submit([&]() { counter.Increment(); });
    js.Wait(h1);
    js.Shutdown();

    js.Initialize(2);
    JobHandle h2 = js.Submit([&]() { counter.Increment(); });
    js.Wait(h2);
    js.Shutdown();

    EXPECT_EQ(counter.Load(), 2) << "Both jobs across two init cycles should have executed";
}

TEST(JobSystemInstance, HandleOutlivesWait_AndIsCompleteSafe)
{
    JobSystem js;
    js.Initialize(2);

    JobHandle h = js.Submit([]() {
        ThisThread::SleepMs(10);
    });

    js.Wait(h);

    // After Wait completes, the job is done but the handle is still valid
    // IsComplete should return true without UAF
    EXPECT_TRUE(js.IsComplete(h));

    js.Shutdown();
}

TEST(JobSystemInstance, HandleDestructionFreesJob_AfterCompletion)
{
    // Proves Job storage and its captures are freed when:
    //   1. The job has finished execution, AND
    //   2. The last JobHandle to it is destroyed

    JobSystem js;
    js.Initialize(2);

    auto sharedData = std::make_shared<int>(42);
    std::weak_ptr<int> weakData = sharedData;

    {
        JobHandle h = js.Submit([capturedData = sharedData]() {
            EXPECT_EQ(*capturedData, 42);
        });

        js.Wait(h);

        // Wait() only guarantees the job finished executing — give the worker
        // time to destroy the task lambda (which holds the captured shared_ptr).
        ThisThread::SleepMs(50);

        EXPECT_FALSE(weakData.expired()) << "sharedData should still be alive (held by test)";
    }

    sharedData.reset();

    EXPECT_TRUE(weakData.expired()) << "All shared_ptr references should be released";

    js.Shutdown();
}

TEST(JobSystemInstance, IsCompleteOnDefaultHandle_ReturnsTrue)
{
    JobSystem js;
    js.Initialize(2);

    JobHandle defaultHandle;

    EXPECT_TRUE(js.IsComplete(defaultHandle));

    js.Shutdown();
}

// ==============================================================================
// Shutdown drain semantics
// ==============================================================================

TEST(JobSystemInstance, Shutdown_DrainsPendingTasks)
{
    // Tasks submitted before Shutdown must run, even if Shutdown is called
    // before they get picked up by a worker.

    JobSystem js;
    js.Initialize(2);

    Atomic<int> ran{0};
    const int kNumJobs = 64;

    std::vector<JobHandle> handles;
    handles.reserve(kNumJobs);

    for (int i = 0; i < kNumJobs; ++i)
    {
        handles.push_back(js.Submit([&]() {
            ThisThread::SleepMs(2);
            ran.Increment();
        }));
    }

    // Shutdown immediately. The destructor must drain — every submitted task
    // is expected to run.
    js.Shutdown();

    EXPECT_EQ(ran.Load(), kNumJobs)
        << "Shutdown must drain pending tasks (ran=" << ran.Load() << ", expected=" << kNumJobs << ")";

    // Handles outlive the JobSystem; IsComplete on a default-constructed
    // JobSystem-less context is moot, so we just check the counter.
}

TEST(JobSystemInstance, ShutdownIsIdempotent)
{
    JobSystem js;
    js.Initialize(2);
    js.Shutdown();
    js.Shutdown();  // Must not crash
    SUCCEED();
}

TEST(JobSystemInstance, StressTest_ManySmallJobs)
{
    JobSystem js;
    js.Initialize(0);  // hardware concurrency

    Atomic<int> counter{0};
    const int kNumJobs = 1000;

    std::vector<JobHandle> handles;
    handles.reserve(kNumJobs);

    for (int i = 0; i < kNumJobs; ++i)
    {
        handles.push_back(js.Submit([&]() {
            counter.Increment();
        }));
    }

    for (auto& h : handles)
    {
        js.Wait(h);
    }

    EXPECT_EQ(counter.Load(), kNumJobs);

    js.Shutdown();
}

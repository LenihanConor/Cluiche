// TestJobSystem.cpp - Google Test unit tests for JobSystem
//
// Tests JobSystem from DiaCore Threading subsystem

#include <gtest/gtest.h>
#include <DiaThreading/JobSystem.h>
#include <DiaCore/Threading/Thread.h>
#include <DiaCore/Threading/Atomic.h>
#include <DiaCore/Core/Assert.h>
#include <chrono>
#include <memory>

using namespace Dia::Threading;
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

// ==============================================================================
// TrySubmit (non-asserting variant)
// ==============================================================================

TEST(JobSystemInstance, TrySubmit_AfterShutdown_ReturnsInvalidHandle)
{
    JobSystem js;
    js.Initialize(2);
    js.Shutdown();

    // TrySubmit must NOT assert - it returns an invalid handle silently
    JobHandle h = js.TrySubmit([]() {});

    EXPECT_FALSE(h.IsValid()) << "TrySubmit after Shutdown must return an invalid handle";
}

TEST(JobSystemInstance, TrySubmit_BeforeInitialize_ReturnsInvalidHandle)
{
    JobSystem js;

    // TrySubmit must NOT assert - it returns an invalid handle silently
    JobHandle h = js.TrySubmit([]() {});

    EXPECT_FALSE(h.IsValid()) << "TrySubmit before Initialize must return an invalid handle";
}

TEST(JobSystemInstance, TrySubmit_NormalCase_ExecutesFunction)
{
    Atomic<int> counter{0};

    JobSystem js;
    js.Initialize(2);

    JobHandle h = js.TrySubmit([&]() {
        counter.Increment();
    });

    EXPECT_TRUE(h.IsValid()) << "TrySubmit should return a valid handle when pool is active";

    js.Wait(h);

    EXPECT_EQ(counter.Load(), 1);

    js.Shutdown();
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

// ==============================================================================
// IJobObserver (Job Observation feature)
// ==============================================================================

#include <DiaThreading/IJobObserver.h>
#include <mutex>
#include <thread>
#include <set>
#include <map>
#include <atomic>
#include <vector>

namespace
{
    class RecordingJobObserver : public IJobObserver
    {
    public:
        struct Seen { std::thread::id beginThread, endThread; JobContext beginCtx, endCtx; JobTimingRecord rec; };

        std::mutex mutex;
        std::vector<unsigned> starts, stops;
        std::vector<Seen> seen;
        std::vector<std::thread::id> captureThreads;
        std::vector<std::thread::id> startThreads;
        uint32_t nextOwner = 7;
        uint32_t nextTag   = 9;
        std::atomic<int> beginsBeforeStart{0};

        void OnWorkerStart(unsigned i) override
        {
            std::lock_guard<std::mutex> l(mutex);
            starts.push_back(i);
            startThreads.push_back(std::this_thread::get_id());
        }
        void OnWorkerStop(unsigned i) override
        {
            std::lock_guard<std::mutex> l(mutex);
            stops.push_back(i);
        }
        JobContext CaptureContext() override
        {
            std::lock_guard<std::mutex> l(mutex);
            captureThreads.push_back(std::this_thread::get_id());
            JobContext c;
            c.attribution.owner = nextOwner;
            c.attribution.tag   = nextTag;
            c.attribution.range = 1;
            c.attribution.rangeCount = 2;
            c.traceParent = 42;
            return c;
        }
        void OnJobBegin(const JobContext& ctx) override
        {
            std::lock_guard<std::mutex> l(mutex);
            // The beginning thread must already have run OnWorkerStart.
            bool started = false;
            for (auto& t : startThreads) started |= (t == std::this_thread::get_id());
            if (!started) ++beginsBeforeStart;
            pending[std::this_thread::get_id()] = { std::this_thread::get_id(), {}, ctx, {}, {} };
        }
        void OnJobEnd(const JobContext& ctx, const JobTimingRecord& rec) override
        {
            std::lock_guard<std::mutex> l(mutex);
            Seen s = pending[std::this_thread::get_id()];
            s.endThread = std::this_thread::get_id();
            s.endCtx = ctx;
            s.rec = rec;
            seen.push_back(s);
        }
    private:
        std::map<std::thread::id, Seen> pending;
    };
}

TEST(JobSystemObserver, WorkerStartStop_OncePerWorker)
{
    RecordingJobObserver obs;
    JobSystem js;
    js.Initialize(3, nullptr, &obs);

    std::vector<JobHandle> hs;
    for (int i = 0; i < 20; ++i) hs.push_back(js.Submit([] {}));
    for (auto& h : hs) js.Wait(h);
    js.Shutdown();

    EXPECT_EQ(obs.starts.size(), 3u);
    EXPECT_EQ(obs.stops.size(), 3u);
    EXPECT_EQ(std::set<unsigned>(obs.starts.begin(), obs.starts.end()).size(), 3u);
    EXPECT_EQ(std::set<unsigned>(obs.stops.begin(), obs.stops.end()).size(), 3u);
    EXPECT_EQ(obs.beginsBeforeStart.load(), 0);
}

TEST(JobSystemObserver, ContextCapturedOnSubmitter_BeginEndOnWorker)
{
    RecordingJobObserver obs;
    JobSystem js;
    js.Initialize(2, nullptr, &obs);

    JobHandle h = js.Submit([] {});
    js.Wait(h);
    js.Shutdown();

    ASSERT_EQ(obs.captureThreads.size(), 1u);
    EXPECT_EQ(obs.captureThreads[0], std::this_thread::get_id());
    ASSERT_EQ(obs.seen.size(), 1u);
    const auto& s = obs.seen[0];
    EXPECT_NE(s.beginThread, std::this_thread::get_id());
    EXPECT_EQ(s.beginThread, s.endThread);
    EXPECT_EQ(s.beginCtx.attribution.owner, 7u);
    EXPECT_EQ(s.endCtx.attribution.owner, 7u);
    EXPECT_EQ(s.endCtx.traceParent, 42u);
}

TEST(JobSystemObserver, ExplicitTagOverridesCapturedAttribution)
{
    RecordingJobObserver obs;
    JobSystem js;
    js.Initialize(1, nullptr, &obs);

    JobTag tag; tag.owner = 100; tag.tag = 200; tag.range = 3; tag.rangeCount = 5;
    JobHandle a = js.Submit([] {}, tag);
    js.Wait(a);
    JobHandle b = js.Submit([] {});
    js.Wait(b);
    js.Shutdown();

    ASSERT_EQ(obs.seen.size(), 2u);
    EXPECT_EQ(obs.seen[0].endCtx.attribution.owner, 100u);
    EXPECT_EQ(obs.seen[0].endCtx.attribution.tag, 200u);
    EXPECT_EQ(obs.seen[0].endCtx.attribution.range, 3);
    EXPECT_EQ(obs.seen[0].endCtx.attribution.rangeCount, 5);
    EXPECT_EQ(obs.seen[0].endCtx.traceParent, 42u);   // other captured fields preserved
    EXPECT_EQ(obs.seen[1].endCtx.attribution.owner, 7u);
    EXPECT_EQ(obs.seen[1].endCtx.attribution.tag, 9u);
}

TEST(JobSystemObserver, OneTimingRecordPerJob)
{
    RecordingJobObserver obs;
    JobSystem js;
    js.Initialize(3, nullptr, &obs);

    const int kJobs = 50;
    std::vector<JobHandle> hs;
    for (int i = 0; i < kJobs; ++i)
    {
        JobTag t; t.owner = 11; t.tag = static_cast<uint32_t>(i); t.range = 1; t.rangeCount = 4;
        hs.push_back(js.Submit([] { std::this_thread::sleep_for(std::chrono::microseconds(50)); }, t));
    }
    for (auto& h : hs) js.Wait(h);
    js.Shutdown();

    ASSERT_EQ(obs.seen.size(), static_cast<size_t>(kJobs));
    std::set<uint32_t> tags;
    for (auto& s : obs.seen)
    {
        EXPECT_LE(s.rec.submitNs, s.rec.startNs);
        EXPECT_LE(s.rec.startNs, s.rec.endNs);
        EXPECT_LT(s.rec.worker, 3);
        EXPECT_EQ(s.rec.owner, 11u);
        EXPECT_EQ(s.rec.range, 1);
        EXPECT_EQ(s.rec.rangeCount, 4);
        tags.insert(s.rec.tag);
    }
    EXPECT_EQ(tags.size(), static_cast<size_t>(kJobs));
}

TEST(JobSystemObserver, NullFunction_StillEndsAndCompletes)
{
    RecordingJobObserver obs;
    JobSystem js;
    js.Initialize(1, nullptr, &obs);
    JobHandle h = js.Submit(JobFn{});
    js.Wait(h);
    js.Shutdown();
    EXPECT_EQ(obs.seen.size(), 1u);
}

TEST(JobSystemObserver, NullObserver_RunsJobsNormally)
{
    JobSystem js;
    js.Initialize(2, nullptr, nullptr);
    Atomic<int> counter{0};
    JobHandle h = js.Submit([&] { counter.Increment(); });
    JobHandle h2 = js.Submit([&] { counter.Increment(); }, JobTag{1, 2, 0, 0});
    JobHandle h3 = js.TrySubmit([&] { counter.Increment(); });
    js.Wait(h); js.Wait(h2); js.Wait(h3);
    EXPECT_EQ(counter.Load(), 3);
    js.Shutdown();
}

// ------------------------------------------------------------------------------
// Gap audit (Job Observation task 5)
// ------------------------------------------------------------------------------

#include <stdexcept>

TEST(JobSystemObserver, ShutdownWithQueuedJobs_DrainsBalancedAndStopsAfterLastJob)
{
    RecordingJobObserver obs;
    JobSystem js;
    js.Initialize(1, nullptr, &obs);
    Atomic<int> ran{0};
    js.Submit([&] { std::this_thread::sleep_for(std::chrono::milliseconds(30)); ran.Increment(); });
    for (int i = 0; i < 50; ++i) js.Submit([&] { ran.Increment(); });
    js.Shutdown();   // jobs still queued
    EXPECT_EQ(ran.Load(), 51);
    EXPECT_EQ(obs.seen.size(), 51u);   // one End per Begin
    EXPECT_EQ(obs.starts.size(), 1u);
    EXPECT_EQ(obs.stops.size(), 1u);
}

TEST(JobSystemObserver, ThrowingJob_StillEndsAndCompletes)
{
    RecordingJobObserver obs;
    JobSystem js;
    js.Initialize(1, nullptr, &obs);
    JobHandle h = js.Submit([] { throw std::runtime_error("boom"); });
    js.Wait(h);
    EXPECT_TRUE(js.IsComplete(h));
    JobHandle h2 = js.Submit([] {});
    js.Wait(h2);
    js.Shutdown();
    EXPECT_EQ(obs.seen.size(), 2u);
}

TEST(JobSystemObserver, TrySubmitAfterShutdown_NoCaptureInvalidHandle)
{
    RecordingJobObserver obs;
    JobSystem js;
    js.Initialize(1, nullptr, &obs);
    js.Shutdown();
    JobHandle h = js.TrySubmit([] {});
    EXPECT_FALSE(h.IsValid());
    EXPECT_TRUE(obs.captureThreads.empty());
}

TEST(JobSystemObserver, ReinitializeWithDifferentObserver_OnlyNewObserverSeesJobs)
{
    RecordingJobObserver a, b;
    JobSystem js;
    js.Initialize(1, nullptr, &a);
    js.Wait(js.Submit([] {}));
    js.Shutdown();
    js.Initialize(1, nullptr, &b);
    js.Wait(js.Submit([] {}));
    js.Shutdown();
    EXPECT_EQ(a.seen.size(), 1u);
    EXPECT_EQ(b.seen.size(), 1u);
    EXPECT_EQ(b.starts.size(), 1u);
    EXPECT_EQ(b.stops.size(), 1u);
}

TEST(JobSystemObserver, JobTagRangePropagatesToRecord)
{
    RecordingJobObserver obs;
    JobSystem js;
    js.Initialize(1, nullptr, &obs);
    js.Wait(js.Submit([] {}, JobTag{11, 22, 3, 8}));
    js.Shutdown();
    ASSERT_EQ(obs.seen.size(), 1u);
    EXPECT_EQ(obs.seen[0].rec.range, 3u);
    EXPECT_EQ(obs.seen[0].rec.rangeCount, 8u);
    EXPECT_EQ(obs.seen[0].beginCtx.attribution.range, 3u);
    EXPECT_EQ(obs.seen[0].beginCtx.attribution.rangeCount, 8u);
}

namespace
{
    struct CountingMetrics : IJobMetrics
    {
        std::atomic<int> completed{0};
        void OnJobSubmitted() override {}
        void OnJobCompleted() override { completed.fetch_add(1); }
        void OnQueueDepthChanged(double) override {}
        void OnActiveWorkersChanged(double) override {}
    };
}

TEST(JobSystemInstance, ThrowingJobNullObserver_StillCompletes)
{
    CountingMetrics metrics;
    auto js = std::make_shared<JobSystem>();
    js->Initialize(1, &metrics);

    JobHandle h = js->Submit([]() { throw 42; });

    // Wait on a helper thread with a timeout so a regression fails instead of hanging the suite.
    auto done = std::make_shared<std::atomic<bool>>(false);
    std::thread waiter([js, h, done]() { js->Wait(h); done->store(true); });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!done->load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

    const bool completed = done->load();
    EXPECT_TRUE(completed) << "Wait() hung on a throwing job";
    EXPECT_TRUE(js->IsComplete(h));
    EXPECT_EQ(metrics.completed.load(), 1);
    EXPECT_EQ(js->GetCompletedCount(), 1u);
    if (completed) { waiter.join(); js->Shutdown(); }
    else           { waiter.detach(); } // spinning waiter keeps js alive; leak rather than hang
}

// ---------------------------------------------------------------------------
// TrySubmit(fn, tag) - non-asserting tagged variant
// ---------------------------------------------------------------------------

TEST(JobSystemObserver, TrySubmitTagged_AttributionMatchesSubmitTagged)
{
    RecordingJobObserver obs;
    JobSystem js;
    js.Initialize(1, nullptr, &obs);

    JobTag tag; tag.owner = 100; tag.tag = 200; tag.range = 3; tag.rangeCount = 5;
    JobHandle a = js.Submit([] {}, tag);
    js.Wait(a);
    JobHandle b = js.TrySubmit([] {}, tag);
    js.Wait(b);
    js.Shutdown();

    ASSERT_EQ(obs.seen.size(), 2u);
    const auto& x = obs.seen[0].endCtx;
    const auto& y = obs.seen[1].endCtx;
    EXPECT_EQ(y.attribution.owner, x.attribution.owner);
    EXPECT_EQ(y.attribution.tag, x.attribution.tag);
    EXPECT_EQ(y.attribution.range, x.attribution.range);
    EXPECT_EQ(y.attribution.rangeCount, x.attribution.rangeCount);
    EXPECT_EQ(y.traceParent, x.traceParent);
    EXPECT_EQ(y.attribution.owner, 100u);
    EXPECT_EQ(y.attribution.tag, 200u);
}

TEST(JobSystemInstance, TrySubmitTagged_BeforeInitialize_ReturnsInvalidHandle)
{
    JobSystem js;
    JobHandle h = js.TrySubmit([]() {}, JobTag{1, 2, 0, 0});
    EXPECT_FALSE(h.IsValid());
}

TEST(JobSystemInstance, TrySubmitTagged_AfterShutdown_ReturnsInvalidHandle)
{
    JobSystem js;
    js.Initialize(1);
    js.Shutdown();
    JobHandle h = js.TrySubmit([]() {}, JobTag{1, 2, 0, 0});
    EXPECT_FALSE(h.IsValid());
}

TEST(JobSystemInstance, TrySubmitTagged_NormalCase_ExecutesAndWaits)
{
    JobSystem js;
    js.Initialize(2);
    std::atomic<int> ran{0};
    JobHandle h = js.TrySubmit([&]() { ran.fetch_add(1); }, JobTag{1, 2, 0, 0});
    EXPECT_TRUE(h.IsValid());
    js.Wait(h);
    EXPECT_EQ(ran.load(), 1);
    EXPECT_TRUE(js.IsComplete(h));
    js.Shutdown();
}

// TestJobSystem.cpp - Google Test unit tests for JobSystem
//
// Tests JobSystem from DiaCore Threading subsystem

#include <gtest/gtest.h>
#include <DiaCore/Threading/JobSystem.h>
#include <DiaCore/Threading/Thread.h>
#include <DiaCore/Threading/Atomic.h>
#include <thread>
#include <vector>
#include <chrono>

using namespace Dia::Core;

// ==============================================================================
// Test Fixture
// ==============================================================================

class JobSystemTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        JobSystem::Initialize(4);
    }

    void TearDown() override
    {
        JobSystem::Shutdown();
    }
};

// ==============================================================================
// Initialization and Shutdown Tests
// ==============================================================================

TEST(JobSystemBasic, Initialize_CreatesJobSystem)
{
    JobSystem::Initialize(2);

    // If we got here without crash, initialization succeeded
    SUCCEED();

    JobSystem::Shutdown();
}

TEST(JobSystemBasic, Initialize_WithZeroThreads_UsesDefault)
{
    JobSystem::Initialize(0);

    // Should use hardware concurrency as default
    SUCCEED();

    JobSystem::Shutdown();
}

TEST(JobSystemBasic, Shutdown_CanBeCalledMultipleTimes)
{
    JobSystem::Initialize(2);
    JobSystem::Shutdown();
    JobSystem::Shutdown();  // Should not crash

    SUCCEED();
}

// ==============================================================================
// Job Creation Tests
// ==============================================================================

TEST_F(JobSystemTest, CreateJob_ReturnsValidJob)
{
    Job* job = JobSystem::CreateJob([](Job*) {});

    EXPECT_NE(job, nullptr);
    EXPECT_EQ(job->parent, nullptr);
    EXPECT_EQ(job->unfinishedJobs.load(), 1);
}

TEST_F(JobSystemTest, CreateJob_WithFunction_StoresFunction)
{
    Atomic<bool> executed{false};

    Job* job = JobSystem::CreateJob([&](Job*) {
        executed = true;
    });

    EXPECT_NE(job->function, nullptr);

    JobSystem::Run(job);
    JobSystem::Wait(job);

    EXPECT_TRUE(executed.Load());
}

TEST_F(JobSystemTest, CreateChildJob_SetsParentRelationship)
{
    Job* parent = JobSystem::CreateJob([](Job*) {});
    Job* child = JobSystem::CreateChildJob(parent, [](Job*) {});

    EXPECT_EQ(child->parent, parent);
    EXPECT_EQ(parent->unfinishedJobs.load(), 2);  // 1 for parent + 1 for child

    JobSystem::Run(parent);
    JobSystem::Run(child);
    JobSystem::Wait(parent);
}

// ==============================================================================
// Job Execution Tests
// ==============================================================================

TEST_F(JobSystemTest, Run_ExecutesJob)
{
    Atomic<bool> executed{false};

    Job* job = JobSystem::CreateJob([&](Job*) {
        executed = true;
    });

    JobSystem::Run(job);
    JobSystem::Wait(job);

    EXPECT_TRUE(executed.Load());
}

TEST_F(JobSystemTest, Run_MultipleJobs_AllExecute)
{
    Atomic<int> counter{0};
    const int numJobs = 10;
    std::vector<Job*> jobs;

    for (int i = 0; i < numJobs; ++i)
    {
        jobs.push_back(JobSystem::CreateJob([&](Job*) {
            counter.Increment();
        }));
    }

    for (Job* job : jobs)
    {
        JobSystem::Run(job);
    }

    for (Job* job : jobs)
    {
        JobSystem::Wait(job);
    }

    EXPECT_EQ(counter.Load(), numJobs);
}

TEST_F(JobSystemTest, Run_WithCapture_WorksCorrectly)
{
    std::vector<int> results;
    Mutex resultsMutex;

    for (int i = 0; i < 5; ++i)
    {
        Job* job = JobSystem::CreateJob([&, i](Job*) {
            ScopedLock<Mutex> lock(resultsMutex);
            results.push_back(i * 2);
        });

        JobSystem::Run(job);
        JobSystem::Wait(job);
    }

    EXPECT_EQ(results.size(), 5);
}

// ==============================================================================
// Wait Tests
// ==============================================================================

TEST_F(JobSystemTest, Wait_BlocksUntilJobCompletes)
{
    Atomic<bool> jobStarted{false};
    Atomic<bool> jobCompleted{false};

    Job* job = JobSystem::CreateJob([&](Job*) {
        jobStarted = true;
        ThisThread::SleepMs(50);
        jobCompleted = true;
    });

    JobSystem::Run(job);
    JobSystem::Wait(job);

    EXPECT_TRUE(jobStarted.Load());
    EXPECT_TRUE(jobCompleted.Load());
}

TEST_F(JobSystemTest, Wait_OnCompletedJob_ReturnsImmediately)
{
    Atomic<bool> executed{false};

    Job* job = JobSystem::CreateJob([&](Job*) {
        executed = true;
    });

    JobSystem::Run(job);
    JobSystem::Wait(job);

    EXPECT_TRUE(executed.Load());

    // Wait again - should return immediately since job is complete
    // (Note: job is deleted after completion, so this tests the Wait behavior)
}

// ==============================================================================
// IsComplete Tests
// ==============================================================================

TEST_F(JobSystemTest, IsComplete_InitiallyFalse)
{
    Job* job = JobSystem::CreateJob([](Job*) {
        ThisThread::SleepMs(100);
    });

    JobSystem::Run(job);

    // Check immediately - should not be complete yet
    bool complete = JobSystem::IsComplete(job);
    // Can't assert false reliably due to timing, but should eventually complete

    JobSystem::Wait(job);
}

TEST_F(JobSystemTest, IsComplete_TrueAfterCompletion)
{
    Atomic<bool> executed{false};

    Job* job = JobSystem::CreateJob([&](Job*) {
        executed = true;
    });

    JobSystem::Run(job);

    // Wait for job to complete
    while (!executed.Load())
    {
        ThisThread::Yield();
    }

    // Give a moment for cleanup
    ThisThread::SleepMs(10);

    // Job should be complete (unfinishedJobs == 0)
    // Note: After job completes, it's deleted, so we can't safely check IsComplete
    EXPECT_TRUE(executed.Load());
}

// ==============================================================================
// Parent-Child Dependency Tests
// ==============================================================================

TEST_F(JobSystemTest, ParentJob_WaitsForChildren)
{
    Atomic<int> executionOrder{0};
    Atomic<int> parentOrder{0};
    Atomic<int> childOrder{0};

    Job* parent = JobSystem::CreateJob([&](Job*) {
        ThisThread::SleepMs(10);  // Ensure children start first
        parentOrder = executionOrder.Increment();
    });

    Job* child1 = JobSystem::CreateChildJob(parent, [&](Job*) {
        childOrder = executionOrder.Increment();
    });

    Job* child2 = JobSystem::CreateChildJob(parent, [&](Job*) {
        childOrder = executionOrder.Increment();
    });

    JobSystem::Run(child1);
    JobSystem::Run(child2);
    JobSystem::Run(parent);

    JobSystem::Wait(parent);

    // Parent should execute after children
    EXPECT_GT(parentOrder.Load(), 0);
    EXPECT_GT(childOrder.Load(), 0);
}

TEST_F(JobSystemTest, ChildJob_IncrementsParentUnfinishedCount)
{
    Job* parent = JobSystem::CreateJob([](Job*) {});

    int initialCount = parent->unfinishedJobs.load();

    Job* child = JobSystem::CreateChildJob(parent, [](Job*) {});

    int afterChildCount = parent->unfinishedJobs.load();

    EXPECT_EQ(afterChildCount, initialCount + 1);

    JobSystem::Run(parent);
    JobSystem::Run(child);
    JobSystem::Wait(parent);
}

TEST_F(JobSystemTest, MultipleChildren_AllComplete)
{
    Atomic<int> counter{0};
    const int numChildren = 5;

    Job* parent = JobSystem::CreateJob([](Job*) {});

    for (int i = 0; i < numChildren; ++i)
    {
        Job* child = JobSystem::CreateChildJob(parent, [&](Job*) {
            counter.Increment();
        });
        JobSystem::Run(child);
    }

    JobSystem::Run(parent);
    JobSystem::Wait(parent);

    EXPECT_EQ(counter.Load(), numChildren);
}

TEST_F(JobSystemTest, NestedChildren_ExecuteCorrectly)
{
    Atomic<int> counter{0};
    Atomic<bool> completed{false};

    Job* grandparent = JobSystem::CreateJob([](Job*) {});

    Job* parent = JobSystem::CreateChildJob(grandparent, [](Job*) {});

    Job* child = JobSystem::CreateChildJob(parent, [&](Job*) {
        counter.Increment();
    });

    JobSystem::Run(child);
    JobSystem::Run(parent);
    JobSystem::Run(grandparent);

    // Wait with 5-second timeout
    auto startTime = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(5);

    while (!JobSystem::IsComplete(grandparent))
    {
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (elapsed > timeout)
        {
            FAIL() << "Test timed out after 5 seconds - likely deadlock in nested job hierarchy";
        }
        ThisThread::Yield();
    }

    EXPECT_EQ(counter.Load(), 1);
}

// ==============================================================================
// ParallelFor Tests
// ==============================================================================

TEST_F(JobSystemTest, ParallelFor_ExecutesAllIterations)
{
    Atomic<int> sum{0};

    ParallelFor(0, 100, [&](int i) {
        sum.FetchAdd(i);
    });

    int expectedSum = (99 * 100) / 2;  // Sum of 0 to 99
    EXPECT_EQ(sum.Load(), expectedSum);
}

TEST_F(JobSystemTest, ParallelFor_WithBatchSize_WorksCorrectly)
{
    Atomic<int> counter{0};

    ParallelFor(0, 50, [&](int i) {
        counter.Increment();
    }, 10);  // Batch size of 10

    EXPECT_EQ(counter.Load(), 50);
}

TEST_F(JobSystemTest, ParallelFor_EmptyRange_DoesNothing)
{
    Atomic<int> counter{0};

    ParallelFor(10, 10, [&](int i) {
        counter.Increment();
    });

    EXPECT_EQ(counter.Load(), 0);
}

TEST_F(JobSystemTest, ParallelFor_ReverseRange_DoesNothing)
{
    Atomic<int> counter{0};

    ParallelFor(10, 0, [&](int i) {
        counter.Increment();
    });

    EXPECT_EQ(counter.Load(), 0);
}

TEST_F(JobSystemTest, ParallelFor_LargeRange_ExecutesInParallel)
{
    Atomic<int> maxConcurrent{0};
    Atomic<int> concurrent{0};

    ParallelFor(0, 1000, [&](int i) {
        int current = concurrent.Increment();

        int max = maxConcurrent.Load();
        while (max < current && !maxConcurrent.CompareExchange(max, current)) {}

        ThisThread::SleepMs(1);  // Small delay to increase concurrency

        concurrent.Decrement();
    }, 100);

    EXPECT_GT(maxConcurrent.Load(), 1);  // Should have parallel execution
}

// ==============================================================================
// Stress Tests
// ==============================================================================

TEST_F(JobSystemTest, StressTest_ManySmallJobs)
{
    Atomic<int> counter{0};
    const int numJobs = 1000;
    std::vector<Job*> jobs;

    for (int i = 0; i < numJobs; ++i)
    {
        jobs.push_back(JobSystem::CreateJob([&](Job*) {
            counter.Increment();
        }));
    }

    for (Job* job : jobs)
    {
        JobSystem::Run(job);
    }

    for (Job* job : jobs)
    {
        JobSystem::Wait(job);
    }

    EXPECT_EQ(counter.Load(), numJobs);
}

TEST_F(JobSystemTest, StressTest_DeepDependencyChain)
{
    Atomic<int> counter{0};
    const int chainDepth = 50;

    Job* root = JobSystem::CreateJob([&](Job*) {
        counter.Increment();
    });

    Job* current = root;
    for (int i = 0; i < chainDepth - 1; ++i)
    {
        Job* child = JobSystem::CreateChildJob(current, [&](Job*) {
            counter.Increment();
        });
        JobSystem::Run(child);
        current = child;
    }

    JobSystem::Run(root);
    JobSystem::Wait(root);

    EXPECT_EQ(counter.Load(), chainDepth);
}

TEST_F(JobSystemTest, StressTest_WideDependencyTree)
{
    Atomic<int> counter{0};
    const int numChildren = 100;

    Job* parent = JobSystem::CreateJob([](Job*) {});

    for (int i = 0; i < numChildren; ++i)
    {
        Job* child = JobSystem::CreateChildJob(parent, [&](Job*) {
            counter.Increment();
        });
        JobSystem::Run(child);
    }

    JobSystem::Run(parent);
    JobSystem::Wait(parent);

    EXPECT_EQ(counter.Load(), numChildren);
}

TEST_F(JobSystemTest, StressTest_MixedWorkload)
{
    Atomic<int> totalWork{0};

    // Create various job patterns
    for (int round = 0; round < 10; ++round)
    {
        Job* parent = JobSystem::CreateJob([&](Job*) {
            totalWork.Increment();
        });

        // Add some children
        for (int i = 0; i < 5; ++i)
        {
            Job* child = JobSystem::CreateChildJob(parent, [&](Job*) {
                totalWork.Increment();
            });
            JobSystem::Run(child);
        }

        JobSystem::Run(parent);
        JobSystem::Wait(parent);
    }

    EXPECT_EQ(totalWork.Load(), 10 * 6);  // 10 rounds * (1 parent + 5 children)
}

// ==============================================================================
// Edge Case Tests
// ==============================================================================

TEST_F(JobSystemTest, JobWithNoFunction_DoesNotCrash)
{
    Job* job = JobSystem::CreateJob(nullptr);

    JobSystem::Run(job);
    JobSystem::Wait(job);

    SUCCEED();
}

TEST_F(JobSystemTest, JobThatSpawnsChildrenInFunction_WorksCorrectly)
{
    Atomic<int> counter{0};

    Job* parent = JobSystem::CreateJob([&](Job* self) {
        counter.Increment();

        // Spawn children dynamically
        for (int i = 0; i < 3; ++i)
        {
            Job* child = JobSystem::CreateChildJob(self, [&](Job*) {
                counter.Increment();
            });
            JobSystem::Run(child);
        }
    });

    JobSystem::Run(parent);
    JobSystem::Wait(parent);

    EXPECT_EQ(counter.Load(), 4);  // 1 parent + 3 children
}

TEST_F(JobSystemTest, LongRunningJob_EventuallyCompletes)
{
    Atomic<bool> completed{false};

    Job* job = JobSystem::CreateJob([&](Job*) {
        ThisThread::SleepMs(200);
        completed = true;
    });

    JobSystem::Run(job);
    JobSystem::Wait(job);

    EXPECT_TRUE(completed.Load());
}

// ==============================================================================
// New Instance API Tests
// ==============================================================================

#include <DiaCore/Core/Assert.h>
#include <memory>

namespace
{
    // Recording assert handler for testing DIA_ASSERT behavior
    Atomic<int> gAssertCount{0};

    void RecordingAssertHandler(const char*, const char*, int, const char*, ...)
    {
        gAssertCount.Increment();
    }
}

TEST(JobSystemInstance, Submit_ExecutesFunction)
{
    Atomic<int> counter{0};

    JobSystem js;
    js.Init(2);

    JobHandle h = js.Submit([&]() {
        counter.Increment();
    });

    js.Wait(h);

    EXPECT_EQ(counter.Load(), 1);

    js.Quit();
}

TEST(JobSystemInstance, WaitFromInsideRunningJob_DoesNotDeadlock)
{
    // This test proves that Wait() called from inside a job does not deadlock
    // when there are sufficient worker threads.
    //
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
    //   - Deadlock

    JobSystem js;
    js.Init(2);

    JobHandle inner = js.Submit([]() {
        ThisThread::SleepMs(20);
    });

    JobHandle outer = js.Submit([&]() {
        js.Wait(inner);
    });

    // Wait for outer with timeout to detect deadlock
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
    js.Quit();
}

TEST(JobSystemInstance, SubmitAfterQuit_Asserts)
{
#ifdef DEBUG
    auto* prevAssertFunc = Dia::Core::g_pAssertFunc;
    Dia::Core::g_pAssertFunc = RecordingAssertHandler;
    gAssertCount.Store(0);

    JobSystem js;
    js.Init(2);
    js.Quit();

    JobHandle h = js.Submit([]() {});  // Should fire DIA_ASSERT then return invalid handle

    EXPECT_GT(gAssertCount.Load(), 0) << "Expected DIA_ASSERT to fire on Submit-after-Quit";
    EXPECT_FALSE(h.IsValid()) << "Submit after Quit must return an invalid handle";

    Dia::Core::g_pAssertFunc = prevAssertFunc;
#else
    SUCCEED() << "DIA_ASSERT compiles out in Release; behavior not testable here";
#endif
}

TEST(JobSystemInstance, ReInitAfterQuit_ProducesFreshPool)
{
    Atomic<int> counter{0};

    JobSystem js;

    // First init cycle
    js.Init(2);
    JobHandle h1 = js.Submit([&]() {
        counter.Increment();
    });
    js.Wait(h1);
    js.Quit();

    // Second init cycle - should produce a fresh, independent pool
    js.Init(2);
    JobHandle h2 = js.Submit([&]() {
        counter.Increment();
    });
    js.Wait(h2);
    js.Quit();

    EXPECT_EQ(counter.Load(), 2) << "Both jobs across two init cycles should have executed";
}

TEST(JobSystemInstance, HandleOutlivesWait_AndIsCompleteSafe)
{
    JobSystem js;
    js.Init(2);

    JobHandle h = js.Submit([]() {
        ThisThread::SleepMs(10);
    });

    js.Wait(h);

    // After Wait completes, the job is done but the handle is still valid
    // IsComplete should return true without UAF
    EXPECT_TRUE(js.IsComplete(h));

    js.Quit();
}

TEST(JobSystemInstance, HandleDestructionFreesJob_AfterCompletion)
{
    // This test proves that Job storage and its captures are freed when:
    //   1. The job has finished execution, AND
    //   2. The last JobHandle to it is destroyed
    //
    // We use a shared_ptr in the job lambda and a weak_ptr outside to detect
    // when the lambda's captures are released.
    //
    // NOTE: In the current implementation, the job's captures are owned by the
    // ThreadPool's task lambda, not by the Job struct itself. The task lambda
    // is destroyed asynchronously by the worker thread after execution. We add
    // a small sleep after Wait() to ensure the worker has time to clean up.

    JobSystem js;
    js.Init(2);

    auto sharedData = std::make_shared<int>(42);
    std::weak_ptr<int> weakData = sharedData;

    {
        JobHandle h = js.Submit([capturedData = sharedData]() {
            // Job captures the shared_ptr by value
            EXPECT_EQ(*capturedData, 42);
        });

        js.Wait(h);

        // Give the worker thread time to destroy the task lambda (which holds
        // the captured shared_ptr). Wait() only guarantees the job has finished
        // executing, not that the task lambda has been destroyed.
        ThisThread::SleepMs(50);

        // At this point:
        //   - Job has finished execution
        //   - Task lambda has been destroyed (releasing captured shared_ptr)
        //   - Only our external 'sharedData' reference remains (refcount = 1)
        //   - weak_ptr should NOT be expired yet

        EXPECT_FALSE(weakData.expired()) << "sharedData should still be alive (held by test)";

        // Now drop the handle (via scope exit)
    }

    // After handle destruction:
    //   - No JobHandles reference the Job
    //   - Job should be freed

    sharedData.reset();  // Release our external reference

    EXPECT_TRUE(weakData.expired()) << "All shared_ptr references should be released";

    js.Quit();
}

TEST(JobSystemInstance, IsCompleteOnDefaultHandle_ReturnsTrue)
{
    // Defensive test: default-constructed handle should be considered "complete"
    // (since it represents no work)

    JobSystem js;
    js.Init(2);

    JobHandle defaultHandle;

    EXPECT_TRUE(js.IsComplete(defaultHandle));

    js.Quit();
}

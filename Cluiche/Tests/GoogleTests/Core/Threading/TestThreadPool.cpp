// TestThreadPool.cpp - Google Test unit tests for ThreadPool
//
// Tests ThreadPool from DiaCore Threading subsystem

#include <gtest/gtest.h>
#include <DiaCore/Threading/ThreadPool.h>
#include <DiaCore/Threading/Thread.h>
#include <DiaCore/Threading/Atomic.h>
#include <thread>
#include <vector>
#include <chrono>

using namespace Dia::Core;

// ==============================================================================
// ThreadPool Construction Tests
// ==============================================================================

TEST(ThreadPool, DefaultConstruction_UsesHardwareConcurrency)
{
    ThreadPool pool;

    EXPECT_GT(pool.GetThreadCount(), 0);
    EXPECT_LE(pool.GetThreadCount(), std::thread::hardware_concurrency() + 4);  // Should be reasonable
}

TEST(ThreadPool, ConstructWithThreadCount_CreatesSpecifiedThreads)
{
    ThreadPool pool(4);

    EXPECT_EQ(pool.GetThreadCount(), 4);
}

TEST(ThreadPool, ConstructWithZeroThreads_UsesDefault)
{
    ThreadPool pool(0);

    EXPECT_GT(pool.GetThreadCount(), 0);
}

// ==============================================================================
// Task Enqueueing Tests
// ==============================================================================

TEST(ThreadPool, EnqueueSingleTask_Executes)
{
    ThreadPool pool(2);
    Atomic<bool> executed{false};

    pool.Enqueue([&]() {
        executed = true;
    });

    pool.WaitAll();

    EXPECT_TRUE(executed.Load());
}

TEST(ThreadPool, EnqueueMultipleTasks_AllExecute)
{
    ThreadPool pool(4);
    Atomic<int> counter{0};
    const int numTasks = 10;

    for (int i = 0; i < numTasks; ++i)
    {
        pool.Enqueue([&]() {
            counter.Increment();
        });
    }

    pool.WaitAll();

    EXPECT_EQ(counter.Load(), numTasks);
}

TEST(ThreadPool, EnqueueTask_WithCapture_WorksCorrectly)
{
    ThreadPool pool(2);
    std::vector<int> results;
    Mutex resultsMutex;

    for (int i = 0; i < 5; ++i)
    {
        pool.Enqueue([&, i]() {
            ScopedLock<Mutex> lock(resultsMutex);
            results.push_back(i * 2);
        });
    }

    pool.WaitAll();

    EXPECT_EQ(results.size(), 5);
    // Results may be in any order due to parallel execution
}

// ==============================================================================
// WaitAll Tests
// ==============================================================================

TEST(ThreadPool, WaitAll_BlocksUntilAllTasksComplete)
{
    ThreadPool pool(2);
    Atomic<int> tasksCompleted{0};
    const int numTasks = 5;

    for (int i = 0; i < numTasks; ++i)
    {
        pool.Enqueue([&]() {
            ThisThread::SleepMs(10);
            tasksCompleted.Increment();
        });
    }

    pool.WaitAll();

    EXPECT_EQ(tasksCompleted.Load(), numTasks);
}

TEST(ThreadPool, WaitAll_WhenNoTasks_ReturnsImmediately)
{
    ThreadPool pool(2);

    auto start = std::chrono::steady_clock::now();
    pool.WaitAll();
    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 10);  // Should be nearly instant
}

TEST(ThreadPool, WaitAll_CalledMultipleTimes_WorksCorrectly)
{
    ThreadPool pool(2);
    Atomic<int> counter{0};

    pool.Enqueue([&]() { counter.Increment(); });
    pool.WaitAll();
    EXPECT_EQ(counter.Load(), 1);

    pool.Enqueue([&]() { counter.Increment(); });
    pool.WaitAll();
    EXPECT_EQ(counter.Load(), 2);

    pool.Enqueue([&]() { counter.Increment(); });
    pool.WaitAll();
    EXPECT_EQ(counter.Load(), 3);
}

// ==============================================================================
// Pending Task Count Tests
// ==============================================================================

TEST(ThreadPool, GetPendingTaskCount_InitiallyZero)
{
    ThreadPool pool(2);

    EXPECT_EQ(pool.GetPendingTaskCount(), 0);
}

TEST(ThreadPool, GetPendingTaskCount_ReflectsQueuedTasks)
{
    ThreadPool pool(1);  // Single thread to keep tasks queued

    // Enqueue a long-running task to block the worker
    pool.Enqueue([]() {
        ThisThread::SleepMs(100);
    });

    // Enqueue more tasks while worker is busy
    for (int i = 0; i < 5; ++i)
    {
        pool.Enqueue([]() {});
    }

    // Should have some pending tasks
    size_t pending = pool.GetPendingTaskCount();
    EXPECT_GT(pending, 0);

    pool.WaitAll();
    EXPECT_EQ(pool.GetPendingTaskCount(), 0);
}

// ==============================================================================
// Shutdown Tests
// ==============================================================================

TEST(ThreadPool, Shutdown_WaitsForAllTasks)
{
    ThreadPool* pool = new ThreadPool(2);
    Atomic<int> counter{0};

    pool->Enqueue([&]() {
        ThisThread::SleepMs(50);
        counter.Increment();
    });

    pool->Enqueue([&]() {
        ThisThread::SleepMs(50);
        counter.Increment();
    });

    delete pool;  // Calls Shutdown() in destructor

    EXPECT_EQ(counter.Load(), 2);  // Both tasks should have completed
}

TEST(ThreadPool, Shutdown_CalledExplicitly_StopsAcceptingTasks)
{
    ThreadPool pool(2);
    Atomic<int> counter{0};

    pool.Enqueue([&]() { counter.Increment(); });
    pool.WaitAll();

    pool.Shutdown();

    // After shutdown, GetThreadCount should be 0
    EXPECT_EQ(pool.GetThreadCount(), 0);
}

TEST(ThreadPool, Shutdown_CalledMultipleTimes_Idempotent)
{
    ThreadPool pool(2);

    pool.Shutdown();
    pool.Shutdown();  // Should not crash

    SUCCEED();
}

// ==============================================================================
// Parallel Execution Tests
// ==============================================================================

TEST(ThreadPool, ParallelExecution_TasksRunConcurrently)
{
    ThreadPool pool(4);
    Atomic<int> concurrentTasks{0};
    Atomic<int> maxConcurrent{0};

    auto task = [&]() {
        int current = concurrentTasks.Increment();

        // Update max concurrent
        int max = maxConcurrent.Load();
        while (max < current && !maxConcurrent.CompareExchange(max, current)) {}

        ThisThread::SleepMs(20);  // Simulate work

        concurrentTasks.Decrement();
    };

    for (int i = 0; i < 10; ++i)
    {
        pool.Enqueue(task);
    }

    pool.WaitAll();

    EXPECT_GT(maxConcurrent.Load(), 1);  // Multiple tasks ran concurrently
}

TEST(ThreadPool, LoadBalancing_DistributesWorkAcrossThreads)
{
    const int numThreads = 4;
    ThreadPool pool(numThreads);

    std::vector<Atomic<int>> threadCounters(numThreads);
    for (auto& counter : threadCounters)
    {
        counter.Store(0);
    }

    Mutex threadIdMapMutex;
    std::map<std::thread::id, int> threadIdMap;
    Atomic<int> nextIndex{0};

    auto task = [&]() {
        std::thread::id tid = ThisThread::GetId();

        int index;
        {
            ScopedLock<Mutex> lock(threadIdMapMutex);
            if (threadIdMap.find(tid) == threadIdMap.end())
            {
                threadIdMap[tid] = nextIndex.Increment() - 1;
            }
            index = threadIdMap[tid];
        }

        if (index < numThreads)
        {
            threadCounters[index].Increment();
        }
    };

    const int numTasks = 100;
    for (int i = 0; i < numTasks; ++i)
    {
        pool.Enqueue(task);
    }

    pool.WaitAll();

    // Check that work was distributed
    int totalWork = 0;
    int threadsUsed = 0;
    for (int i = 0; i < numThreads; ++i)
    {
        int count = threadCounters[i].Load();
        totalWork += count;
        if (count > 0)
        {
            ++threadsUsed;
        }
    }

    EXPECT_EQ(totalWork, numTasks);
    EXPECT_GT(threadsUsed, 1);  // Multiple threads were used
}

// ==============================================================================
// Exception Safety Tests
// ==============================================================================

TEST(ThreadPool, TaskThrowsException_OtherTasksStillExecute)
{
    ThreadPool pool(2);
    Atomic<int> successCount{0};

    // Task that throws
    pool.Enqueue([&]() {
        throw std::runtime_error("Test exception");
    });

    // Tasks that succeed
    for (int i = 0; i < 5; ++i)
    {
        pool.Enqueue([&]() {
            ThisThread::SleepMs(10);
            successCount.Increment();
        });
    }

    pool.WaitAll();

    // Other tasks should have executed despite exception
    EXPECT_EQ(successCount.Load(), 5);
}

// ==============================================================================
// Stress Tests
// ==============================================================================

TEST(ThreadPool, StressTest_ManyShortTasks)
{
    ThreadPool pool(4);
    Atomic<int> counter{0};
    const int numTasks = 10000;

    for (int i = 0; i < numTasks; ++i)
    {
        pool.Enqueue([&]() {
            counter.Increment();
        });
    }

    pool.WaitAll();

    EXPECT_EQ(counter.Load(), numTasks);
}

TEST(ThreadPool, StressTest_FewLongTasks)
{
    ThreadPool pool(4);
    Atomic<int> counter{0};
    const int numTasks = 10;

    for (int i = 0; i < numTasks; ++i)
    {
        pool.Enqueue([&]() {
            ThisThread::SleepMs(50);
            counter.Increment();
        });
    }

    pool.WaitAll();

    EXPECT_EQ(counter.Load(), numTasks);
}

TEST(ThreadPool, StressTest_MixedTaskDurations)
{
    ThreadPool pool(8);
    Atomic<int> counter{0};
    const int numTasks = 100;

    for (int i = 0; i < numTasks; ++i)
    {
        pool.Enqueue([&, i]() {
            // Variable sleep duration
            int sleepMs = (i % 5) * 5;
            if (sleepMs > 0)
            {
                ThisThread::SleepMs(sleepMs);
            }
            counter.Increment();
        });
    }

    pool.WaitAll();

    EXPECT_EQ(counter.Load(), numTasks);
}

TEST(ThreadPool, StressTest_RepeatedEnqueueAndWait)
{
    ThreadPool pool(4);
    Atomic<int> totalTasks{0};

    for (int round = 0; round < 10; ++round)
    {
        for (int i = 0; i < 100; ++i)
        {
            pool.Enqueue([&]() {
                totalTasks.Increment();
            });
        }

        pool.WaitAll();
    }

    EXPECT_EQ(totalTasks.Load(), 1000);
}

// ==============================================================================
// Edge Case Tests
// ==============================================================================

TEST(ThreadPool, SingleThreadPool_ExecutesTasksSequentially)
{
    ThreadPool pool(1);
    std::vector<int> executionOrder;
    Mutex orderMutex;

    for (int i = 0; i < 5; ++i)
    {
        pool.Enqueue([&, i]() {
            ScopedLock<Mutex> lock(orderMutex);
            executionOrder.push_back(i);
        });
    }

    pool.WaitAll();

    EXPECT_EQ(executionOrder.size(), 5);
    // Single thread should execute in order
    for (size_t i = 0; i < executionOrder.size(); ++i)
    {
        EXPECT_EQ(executionOrder[i], static_cast<int>(i));
    }
}

TEST(ThreadPool, LargeThreadPool_HandlesCorrectly)
{
    ThreadPool pool(128);  // Many threads

    EXPECT_EQ(pool.GetThreadCount(), 128);

    Atomic<int> counter{0};
    for (int i = 0; i < 200; ++i)
    {
        pool.Enqueue([&]() {
            counter.Increment();
        });
    }

    pool.WaitAll();

    EXPECT_EQ(counter.Load(), 200);
}

TEST(ThreadPool, EmptyTaskQueue_WaitAllReturnsImmediately)
{
    ThreadPool pool(4);

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < 5; ++i)
    {
        pool.WaitAll();
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 10);  // Should be nearly instant
}

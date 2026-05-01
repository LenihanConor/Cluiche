// TestAtomic.cpp - Google Test unit tests for Threading Atomic classes
//
// Tests Atomic<T>, AtomicCounter, AtomicFlag, SpinLock from DiaCore Threading subsystem

#include <gtest/gtest.h>
#include <DiaCore/Threading/Atomic.h>
#include <DiaCore/Threading/Mutex.h>
#include <DiaCore/Threading/Thread.h>
#include <thread>
#include <vector>

using namespace Dia::Core;

// ==============================================================================
// Atomic<int> Tests
// ==============================================================================

TEST(Atomic, DefaultConstruction_InitializesToZero)
{
    Atomic<int> value;

    EXPECT_EQ(value.Load(), 0);
}

TEST(Atomic, ConstructWithValue_StoresValue)
{
    Atomic<int> value(42);

    EXPECT_EQ(value.Load(), 42);
}

TEST(Atomic, Store_UpdatesValue)
{
    Atomic<int> value;

    value.Store(100);

    EXPECT_EQ(value.Load(), 100);
}

TEST(Atomic, Exchange_ReturnsOldValue)
{
    Atomic<int> value(10);

    int oldValue = value.Exchange(20);

    EXPECT_EQ(oldValue, 10);
    EXPECT_EQ(value.Load(), 20);
}

TEST(Atomic, CompareExchange_WhenEqual_Exchanges)
{
    Atomic<int> value(10);
    int expected = 10;

    bool success = value.CompareExchange(expected, 20);

    EXPECT_TRUE(success);
    EXPECT_EQ(value.Load(), 20);
    EXPECT_EQ(expected, 10);  // Expected unchanged on success
}

TEST(Atomic, CompareExchange_WhenNotEqual_DoesNotExchange)
{
    Atomic<int> value(10);
    int expected = 15;

    bool success = value.CompareExchange(expected, 20);

    EXPECT_FALSE(success);
    EXPECT_EQ(value.Load(), 10);  // Value unchanged
    EXPECT_EQ(expected, 10);  // Expected updated to actual value
}

TEST(Atomic, CompareExchangeStrong_WhenEqual_Exchanges)
{
    Atomic<int> value(10);
    int expected = 10;

    bool success = value.CompareExchangeStrong(expected, 20);

    EXPECT_TRUE(success);
    EXPECT_EQ(value.Load(), 20);
}

TEST(Atomic, FetchAdd_ReturnsOldValue)
{
    Atomic<int> value(10);

    int oldValue = value.FetchAdd(5);

    EXPECT_EQ(oldValue, 10);
    EXPECT_EQ(value.Load(), 15);
}

TEST(Atomic, FetchSub_ReturnsOldValue)
{
    Atomic<int> value(10);

    int oldValue = value.FetchSub(3);

    EXPECT_EQ(oldValue, 10);
    EXPECT_EQ(value.Load(), 7);
}

TEST(Atomic, Increment_ReturnsNewValue)
{
    Atomic<int> value(10);

    int newValue = value.Increment();

    EXPECT_EQ(newValue, 11);
    EXPECT_EQ(value.Load(), 11);
}

TEST(Atomic, Decrement_ReturnsNewValue)
{
    Atomic<int> value(10);

    int newValue = value.Decrement();

    EXPECT_EQ(newValue, 9);
    EXPECT_EQ(value.Load(), 9);
}

TEST(Atomic, OperatorConversion_ReadsValue)
{
    Atomic<int> value(42);

    int result = value;

    EXPECT_EQ(result, 42);
}

TEST(Atomic, OperatorAssignment_WritesValue)
{
    Atomic<int> value;

    value = 100;

    EXPECT_EQ(value.Load(), 100);
}

TEST(Atomic, OperatorPrefixIncrement_ReturnsNewValue)
{
    Atomic<int> value(10);

    int result = ++value;

    EXPECT_EQ(result, 11);
    EXPECT_EQ(value.Load(), 11);
}

TEST(Atomic, OperatorPostfixIncrement_ReturnsOldValue)
{
    Atomic<int> value(10);

    int result = value++;

    EXPECT_EQ(result, 10);
    EXPECT_EQ(value.Load(), 11);
}

TEST(Atomic, OperatorPrefixDecrement_ReturnsNewValue)
{
    Atomic<int> value(10);

    int result = --value;

    EXPECT_EQ(result, 9);
    EXPECT_EQ(value.Load(), 9);
}

TEST(Atomic, OperatorPostfixDecrement_ReturnsOldValue)
{
    Atomic<int> value(10);

    int result = value--;

    EXPECT_EQ(result, 10);
    EXPECT_EQ(value.Load(), 9);
}

TEST(Atomic, OperatorPlusEquals_ReturnsNewValue)
{
    Atomic<int> value(10);

    int result = (value += 5);

    EXPECT_EQ(result, 15);
    EXPECT_EQ(value.Load(), 15);
}

TEST(Atomic, OperatorMinusEquals_ReturnsNewValue)
{
    Atomic<int> value(10);

    int result = (value -= 3);

    EXPECT_EQ(result, 7);
    EXPECT_EQ(value.Load(), 7);
}

// ==============================================================================
// Atomic Thread Safety Tests
// ==============================================================================

TEST(Atomic, ConcurrentIncrements_ThreadSafe)
{
    Atomic<int> counter(0);
    const int numThreads = 10;
    const int incrementsPerThread = 1000;

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back([&]() {
            for (int j = 0; j < incrementsPerThread; ++j)
            {
                counter.Increment();
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    EXPECT_EQ(counter.Load(), numThreads * incrementsPerThread);
}

TEST(Atomic, ConcurrentAdditions_ThreadSafe)
{
    Atomic<int> sum(0);
    const int numThreads = 8;
    const int additionsPerThread = 500;

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back([&]() {
            for (int j = 0; j < additionsPerThread; ++j)
            {
                sum.FetchAdd(2);
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    EXPECT_EQ(sum.Load(), numThreads * additionsPerThread * 2);
}

TEST(Atomic, CompareExchangeLoop_ImplementsAtomicMax)
{
    Atomic<int> maxValue(0);
    const int numThreads = 10;

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back([&, i]() {
            int newValue = (i + 1) * 10;  // 10, 20, 30, ..., 100

            // Atomic max using compare-exchange
            int current = maxValue.Load();
            while (newValue > current)
            {
                if (maxValue.CompareExchangeStrong(current, newValue))
                {
                    break;
                }
                // current is updated to actual value on failure, retry
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    EXPECT_EQ(maxValue.Load(), 100);  // Maximum thread ID * 10
}

// ==============================================================================
// AtomicCounter Tests
// ==============================================================================

TEST(AtomicCounter, DefaultConstruction_InitializesToZero)
{
    AtomicCounter counter;

    EXPECT_EQ(counter.Get(), 0);
}

TEST(AtomicCounter, ConstructWithValue_StoresValue)
{
    AtomicCounter counter(42);

    EXPECT_EQ(counter.Get(), 42);
}

TEST(AtomicCounter, Increment_ReturnsNewValue)
{
    AtomicCounter counter(10);

    int newValue = counter.Increment();

    EXPECT_EQ(newValue, 11);
    EXPECT_EQ(counter.Get(), 11);
}

TEST(AtomicCounter, Decrement_ReturnsNewValue)
{
    AtomicCounter counter(10);

    int newValue = counter.Decrement();

    EXPECT_EQ(newValue, 9);
    EXPECT_EQ(counter.Get(), 9);
}

TEST(AtomicCounter, Set_UpdatesValue)
{
    AtomicCounter counter;

    counter.Set(100);

    EXPECT_EQ(counter.Get(), 100);
}

TEST(AtomicCounter, ConcurrentIncrements_ThreadSafe)
{
    AtomicCounter counter(0);
    const int numThreads = 10;
    const int incrementsPerThread = 1000;

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back([&]() {
            for (int j = 0; j < incrementsPerThread; ++j)
            {
                counter.Increment();
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    EXPECT_EQ(counter.Get(), numThreads * incrementsPerThread);
}

TEST(AtomicCounter, MixedOperations_ThreadSafe)
{
    AtomicCounter counter(5000);
    const int numThreads = 4;
    const int opsPerThread = 500;

    std::vector<std::thread> threads;

    // Half threads increment
    for (int i = 0; i < numThreads / 2; ++i)
    {
        threads.emplace_back([&]() {
            for (int j = 0; j < opsPerThread; ++j)
            {
                counter.Increment();
            }
        });
    }

    // Half threads decrement
    for (int i = 0; i < numThreads / 2; ++i)
    {
        threads.emplace_back([&]() {
            for (int j = 0; j < opsPerThread; ++j)
            {
                counter.Decrement();
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    EXPECT_EQ(counter.Get(), 5000);  // Back to initial value
}

// ==============================================================================
// AtomicFlag Tests
// ==============================================================================

TEST(AtomicFlag, DefaultConstruction_ClearState)
{
    AtomicFlag flag;

    bool wasSet = flag.TestAndSet();

    EXPECT_FALSE(wasSet);  // Was not set initially
}

TEST(AtomicFlag, TestAndSet_ReturnsPreviousValue)
{
    AtomicFlag flag;

    bool first = flag.TestAndSet();
    bool second = flag.TestAndSet();

    EXPECT_FALSE(first);   // Was clear
    EXPECT_TRUE(second);   // Was set
}

TEST(AtomicFlag, Clear_ResetsFlag)
{
    AtomicFlag flag;
    flag.TestAndSet();

    flag.Clear();

    bool wasSet = flag.TestAndSet();
    EXPECT_FALSE(wasSet);  // Was cleared
}

TEST(AtomicFlag, ThreadSafety_OnlyOneThreadAcquires)
{
    AtomicFlag flag;
    std::atomic<int> acquireCount{0};
    const int numThreads = 10;

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back([&]() {
            if (!flag.TestAndSet())  // Try to acquire
            {
                ++acquireCount;
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    EXPECT_EQ(acquireCount.load(), 1);  // Only one thread acquired
}

// ==============================================================================
// SpinLock Tests
// ==============================================================================

TEST(SpinLock, LockUnlock_BasicUsage)
{
    SpinLock spinLock;

    spinLock.Lock();
    spinLock.Unlock();

    SUCCEED();
}

TEST(SpinLock, TryLock_WhenUnlocked_ReturnsTrue)
{
    SpinLock spinLock;

    bool acquired = spinLock.TryLock();

    EXPECT_TRUE(acquired);
    spinLock.Unlock();
}

TEST(SpinLock, TryLock_WhenLocked_ReturnsFalse)
{
    SpinLock spinLock;
    spinLock.Lock();

    std::thread t([&]() {
        bool acquired = spinLock.TryLock();
        EXPECT_FALSE(acquired);
    });

    t.join();
    spinLock.Unlock();
}

TEST(SpinLock, ProtectsSharedData)
{
    SpinLock spinLock;
    int counter = 0;
    const int numThreads = 5;
    const int incrementsPerThread = 1000;

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back([&]() {
            for (int j = 0; j < incrementsPerThread; ++j)
            {
                spinLock.Lock();
                ++counter;
                spinLock.Unlock();
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    EXPECT_EQ(counter, numThreads * incrementsPerThread);
}

TEST(SpinLock, WorksWithScopedLock)
{
    SpinLock spinLock;
    int counter = 0;

    {
        ScopedLock<SpinLock> lock(spinLock);
        ++counter;
    }

    EXPECT_EQ(counter, 1);

    // Should be unlocked now
    bool acquired = spinLock.TryLock();
    EXPECT_TRUE(acquired);
    spinLock.Unlock();
}

// ==============================================================================
// Memory Ordering Tests
// ==============================================================================

TEST(Atomic, LoadStore_WithRelaxedOrdering)
{
    Atomic<int> value(0);

    value.Store(42, MemoryOrder::Relaxed);
    int loaded = value.Load(MemoryOrder::Relaxed);

    EXPECT_EQ(loaded, 42);
}

TEST(Atomic, LoadStore_WithAcquireRelease)
{
    Atomic<int> value(0);

    value.Store(100, MemoryOrder::Release);
    int loaded = value.Load(MemoryOrder::Acquire);

    EXPECT_EQ(loaded, 100);
}

TEST(Atomic, LoadStore_WithSequentialConsistency)
{
    Atomic<int> value(0);

    value.Store(200, MemoryOrder::SeqCst);
    int loaded = value.Load(MemoryOrder::SeqCst);

    EXPECT_EQ(loaded, 200);
}

// ==============================================================================
// Stress Tests
// ==============================================================================

TEST(Atomic, StressTest_HighContentionCounter)
{
    Atomic<int> counter(0);
    const int numThreads = 20;
    const int opsPerThread = 5000;

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back([&]() {
            for (int j = 0; j < opsPerThread; ++j)
            {
                ++counter;
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    EXPECT_EQ(counter.Load(), numThreads * opsPerThread);
}

TEST(SpinLock, StressTest_ShortCriticalSections)
{
    SpinLock spinLock;
    std::atomic<long long> sum{0};
    const int numThreads = 10;
    const int opsPerThread = 10000;

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < opsPerThread; ++j)
            {
                spinLock.Lock();
                sum.fetch_add(1, std::memory_order_relaxed);
                spinLock.Unlock();
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    EXPECT_EQ(sum.load(), numThreads * opsPerThread);
}

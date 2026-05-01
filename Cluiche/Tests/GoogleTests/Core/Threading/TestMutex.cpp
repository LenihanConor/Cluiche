// TestMutex.cpp - Google Test unit tests for Threading Mutex classes
//
// Tests Mutex, RecursiveMutex, RWLock, ScopedLock, SpinLock from DiaCore Threading subsystem

#include <gtest/gtest.h>
#include <DiaCore/Threading/Mutex.h>
#include <DiaCore/Threading/Thread.h>
#include <thread>
#include <vector>
#include <atomic>

using namespace Dia::Core;

// ==============================================================================
// Mutex Tests
// ==============================================================================

TEST(Mutex, DefaultConstruction_CanLockUnlock)
{
    Mutex mutex;

    mutex.Lock();
    mutex.Unlock();

    // Test passes if no deadlock
    SUCCEED();
}

TEST(Mutex, TryLock_WhenUnlocked_ReturnsTrue)
{
    Mutex mutex;

    bool acquired = mutex.TryLock();

    EXPECT_TRUE(acquired);
    mutex.Unlock();
}

TEST(Mutex, TryLock_WhenLocked_ReturnsFalse)
{
    Mutex mutex;
    mutex.Lock();

    std::thread t([&]() {
        bool acquired = mutex.TryLock();
        EXPECT_FALSE(acquired);
    });

    t.join();
    mutex.Unlock();
}

TEST(Mutex, LockUnlock_MultipleIterations)
{
    Mutex mutex;

    for (int i = 0; i < 100; ++i)
    {
        mutex.Lock();
        mutex.Unlock();
    }

    SUCCEED();
}

TEST(Mutex, BasicThreadSafety_IncrementCounter)
{
    Mutex mutex;
    int counter = 0;
    const int iterations = 1000;

    std::thread t1([&]() {
        for (int i = 0; i < iterations; ++i)
        {
            mutex.Lock();
            ++counter;
            mutex.Unlock();
        }
    });

    std::thread t2([&]() {
        for (int i = 0; i < iterations; ++i)
        {
            mutex.Lock();
            ++counter;
            mutex.Unlock();
        }
    });

    t1.join();
    t2.join();

    EXPECT_EQ(counter, iterations * 2);
}

TEST(Mutex, GetNative_ReturnsValidReference)
{
    Mutex mutex;

    std::mutex& native = mutex.GetNative();

    // Can use native mutex
    native.lock();
    native.unlock();

    SUCCEED();
}

// ==============================================================================
// RecursiveMutex Tests
// ==============================================================================

TEST(RecursiveMutex, SameThreadCanLockMultipleTimes)
{
    RecursiveMutex mutex;

    mutex.Lock();
    mutex.Lock();  // Should not deadlock
    mutex.Lock();

    mutex.Unlock();
    mutex.Unlock();
    mutex.Unlock();

    SUCCEED();
}

TEST(RecursiveMutex, RecursiveFunction_NoDeadlock)
{
    RecursiveMutex mutex;
    int counter = 0;

    std::function<void(int)> recursiveFunc = [&](int depth) {
        if (depth <= 0) return;

        mutex.Lock();
        ++counter;
        recursiveFunc(depth - 1);
        mutex.Unlock();
    };

    recursiveFunc(10);

    EXPECT_EQ(counter, 10);
}

TEST(RecursiveMutex, TryLock_WhenLockedBySameThread_ReturnsTrue)
{
    RecursiveMutex mutex;

    mutex.Lock();
    bool acquired = mutex.TryLock();

    EXPECT_TRUE(acquired);

    mutex.Unlock();
    mutex.Unlock();
}

TEST(RecursiveMutex, TryLock_WhenLockedByOtherThread_ReturnsFalse)
{
    RecursiveMutex mutex;
    mutex.Lock();

    std::thread t([&]() {
        bool acquired = mutex.TryLock();
        EXPECT_FALSE(acquired);
    });

    t.join();
    mutex.Unlock();
}

// ==============================================================================
// ScopedLock Tests (RAII)
// ==============================================================================

TEST(ScopedLock, ConstructorLocksDestructorUnlocks)
{
    Mutex mutex;

    {
        ScopedLock<Mutex> lock(mutex);
        // Mutex is locked here

        // Try to lock from another thread - should fail
        std::atomic<bool> acquiredInThread{false};
        std::thread t([&]() {
            acquiredInThread = mutex.TryLock();
        });
        t.join();

        EXPECT_FALSE(acquiredInThread);
    }

    // After scope, mutex should be unlocked
    bool acquired = mutex.TryLock();
    EXPECT_TRUE(acquired);
    mutex.Unlock();
}

TEST(ScopedLock, WorksWithRecursiveMutex)
{
    RecursiveMutex mutex;

    {
        ScopedLock<RecursiveMutex> lock1(mutex);
        {
            ScopedLock<RecursiveMutex> lock2(mutex);  // Should not deadlock
            SUCCEED();
        }
    }
}

TEST(ScopedLock, ProtectsSharedData)
{
    Mutex mutex;
    int counter = 0;
    const int iterations = 1000;

    std::thread t1([&]() {
        for (int i = 0; i < iterations; ++i)
        {
            ScopedLock<Mutex> lock(mutex);
            ++counter;
        }
    });

    std::thread t2([&]() {
        for (int i = 0; i < iterations; ++i)
        {
            ScopedLock<Mutex> lock(mutex);
            ++counter;
        }
    });

    t1.join();
    t2.join();

    EXPECT_EQ(counter, iterations * 2);
}

// ==============================================================================
// RWLock Tests (Read-Write Lock)
// ==============================================================================

TEST(RWLock, SingleReaderLockUnlock)
{
    RWLock rwLock;

    rwLock.LockRead();
    rwLock.UnlockRead();

    SUCCEED();
}

TEST(RWLock, SingleWriterLockUnlock)
{
    RWLock rwLock;

    rwLock.LockWrite();
    rwLock.UnlockWrite();

    SUCCEED();
}

TEST(RWLock, MultipleReadersCanAcquireSimultaneously)
{
    RWLock rwLock;
    std::atomic<int> concurrentReaders{0};
    std::atomic<int> maxConcurrentReaders{0};

    auto readerFunc = [&]() {
        rwLock.LockRead();

        int current = ++concurrentReaders;
        ThisThread::SleepMs(10);  // Hold lock briefly

        // Track max concurrent readers
        int max = maxConcurrentReaders.load();
        while (max < current && !maxConcurrentReaders.compare_exchange_weak(max, current)) {}

        --concurrentReaders;
        rwLock.UnlockRead();
    };

    std::vector<std::thread> readers;
    for (int i = 0; i < 5; ++i)
    {
        readers.emplace_back(readerFunc);
    }

    for (auto& t : readers)
    {
        t.join();
    }

    EXPECT_GT(maxConcurrentReaders.load(), 1);  // Multiple readers acquired simultaneously
}

TEST(RWLock, WriterBlocksReaders)
{
    RWLock rwLock;
    std::atomic<bool> writerActive{false};
    std::atomic<bool> readerAcquired{false};

    rwLock.LockWrite();

    std::thread writer([&]() {
        writerActive = true;
        ThisThread::SleepMs(50);
        writerActive = false;
        rwLock.UnlockWrite();
    });

    ThisThread::SleepMs(10);  // Ensure writer acquires lock

    std::thread reader([&]() {
        rwLock.LockRead();
        readerAcquired = true;
        EXPECT_FALSE(writerActive);  // Writer should be done
        rwLock.UnlockRead();
    });

    writer.join();
    reader.join();

    EXPECT_TRUE(readerAcquired);
}

TEST(RWLock, WriterBlocksOtherWriters)
{
    RWLock rwLock;
    std::atomic<int> activeWriters{0};
    std::atomic<int> maxActiveWriters{0};

    auto writerFunc = [&]() {
        rwLock.LockWrite();

        int active = ++activeWriters;
        int max = maxActiveWriters.load();
        while (max < active && !maxActiveWriters.compare_exchange_weak(max, active)) {}

        ThisThread::SleepMs(10);

        --activeWriters;
        rwLock.UnlockWrite();
    };

    std::vector<std::thread> writers;
    for (int i = 0; i < 3; ++i)
    {
        writers.emplace_back(writerFunc);
    }

    for (auto& t : writers)
    {
        t.join();
    }

    EXPECT_EQ(maxActiveWriters.load(), 1);  // Only one writer at a time
}

TEST(RWLock, TryLockRead_WhenUnlocked_ReturnsTrue)
{
    RWLock rwLock;

    bool acquired = rwLock.TryLockRead();

    EXPECT_TRUE(acquired);
    rwLock.UnlockRead();
}

TEST(RWLock, TryLockRead_WhenWriteLocked_ReturnsFalse)
{
    RWLock rwLock;
    rwLock.LockWrite();

    std::thread t([&]() {
        bool acquired = rwLock.TryLockRead();
        EXPECT_FALSE(acquired);
    });

    t.join();
    rwLock.UnlockWrite();
}

TEST(RWLock, TryLockWrite_WhenUnlocked_ReturnsTrue)
{
    RWLock rwLock;

    bool acquired = rwLock.TryLockWrite();

    EXPECT_TRUE(acquired);
    rwLock.UnlockWrite();
}

TEST(RWLock, TryLockWrite_WhenReadLocked_ReturnsFalse)
{
    RWLock rwLock;
    rwLock.LockRead();

    std::thread t([&]() {
        bool acquired = rwLock.TryLockWrite();
        EXPECT_FALSE(acquired);
    });

    t.join();
    rwLock.UnlockRead();
}

// ==============================================================================
// ScopedReadLock Tests
// ==============================================================================

TEST(ScopedReadLock, AutomaticallyLocksAndUnlocks)
{
    RWLock rwLock;

    {
        ScopedReadLock lock(rwLock);
        // Read lock is held here

        std::atomic<bool> writerAcquired{false};
        std::thread t([&]() {
            writerAcquired = rwLock.TryLockWrite();
        });
        t.join();

        EXPECT_FALSE(writerAcquired);
    }

    // After scope, should be unlocked
    bool acquired = rwLock.TryLockWrite();
    EXPECT_TRUE(acquired);
    rwLock.UnlockWrite();
}

TEST(ScopedReadLock, MultipleLocksCanCoexist)
{
    RWLock rwLock;

    ScopedReadLock lock1(rwLock);
    ScopedReadLock lock2(rwLock);
    ScopedReadLock lock3(rwLock);

    // All three read locks can exist simultaneously
    SUCCEED();
}

// ==============================================================================
// ScopedWriteLock Tests
// ==============================================================================

TEST(ScopedWriteLock, AutomaticallyLocksAndUnlocks)
{
    RWLock rwLock;

    {
        ScopedWriteLock lock(rwLock);
        // Write lock is held here

        std::atomic<bool> readerAcquired{false};
        std::thread t([&]() {
            readerAcquired = rwLock.TryLockRead();
        });
        t.join();

        EXPECT_FALSE(readerAcquired);
    }

    // After scope, should be unlocked
    bool acquired = rwLock.TryLockRead();
    EXPECT_TRUE(acquired);
    rwLock.UnlockRead();
}

TEST(ScopedWriteLock, ProtectsSharedData)
{
    RWLock rwLock;
    int counter = 0;
    const int iterations = 500;

    std::thread writer([&]() {
        for (int i = 0; i < iterations; ++i)
        {
            ScopedWriteLock lock(rwLock);
            ++counter;
        }
    });

    std::thread reader([&]() {
        for (int i = 0; i < iterations; ++i)
        {
            ScopedReadLock lock(rwLock);
            int value = counter;  // Read counter
            EXPECT_GE(value, 0);
            EXPECT_LE(value, iterations);
        }
    });

    writer.join();
    reader.join();

    EXPECT_EQ(counter, iterations);
}

// ==============================================================================
// Stress Tests
// ==============================================================================

TEST(Mutex, StressTest_HighContention)
{
    Mutex mutex;
    std::atomic<int> counter{0};
    const int numThreads = 10;
    const int iterationsPerThread = 1000;

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back([&]() {
            for (int j = 0; j < iterationsPerThread; ++j)
            {
                ScopedLock<Mutex> lock(mutex);
                ++counter;
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    EXPECT_EQ(counter.load(), numThreads * iterationsPerThread);
}

TEST(RWLock, StressTest_ManyReadersOneWriter)
{
    RWLock rwLock;
    std::atomic<int> counter{0};
    const int numReaders = 8;
    const int numWriters = 2;
    const int iterations = 500;

    std::vector<std::thread> threads;

    // Spawn readers
    for (int i = 0; i < numReaders; ++i)
    {
        threads.emplace_back([&]() {
            for (int j = 0; j < iterations; ++j)
            {
                ScopedReadLock lock(rwLock);
                int value = counter.load();
                EXPECT_GE(value, 0);
            }
        });
    }

    // Spawn writers
    for (int i = 0; i < numWriters; ++i)
    {
        threads.emplace_back([&]() {
            for (int j = 0; j < iterations; ++j)
            {
                ScopedWriteLock lock(rwLock);
                counter.fetch_add(1);
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    EXPECT_EQ(counter.load(), numWriters * iterations);
}

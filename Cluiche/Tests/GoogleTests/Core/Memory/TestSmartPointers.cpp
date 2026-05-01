// TestSmartPointers.cpp - Google Test unit tests for Smart Pointers
//
// Tests UniquePtr, RefPtr, and WeakPtr from DiaCore Memory subsystem

#include <gtest/gtest.h>
#include <DiaCore/Memory/UniquePtr.h>
#include <DiaCore/Memory/RefPtr.h>
#include <DiaCore/Memory/WeakPtr.h>

using namespace Dia::Core;

// ==============================================================================
// Test Helper Classes
// ==============================================================================

class TrackedObject : public RefCountedWeakable
{
public:
    static int constructCount;
    static int destructCount;

    int value;

    TrackedObject(int v = 0) : value(v) { constructCount++; }
    ~TrackedObject() { destructCount++; }

    static void ResetCounts() { constructCount = 0; destructCount = 0; }
};

int TrackedObject::constructCount = 0;
int TrackedObject::destructCount = 0;

// ==============================================================================
// UniquePtr Tests
// ==============================================================================

TEST(UniquePtr, DefaultConstruction_IsNull)
{
    UniquePtr<int> ptr;

    EXPECT_FALSE(ptr);
    EXPECT_EQ(ptr.Get(), nullptr);
}

TEST(UniquePtr, ConstructWithPointer_OwnsPointer)
{
    TrackedObject::ResetCounts();

    UniquePtr<TrackedObject> ptr(new TrackedObject(42));

    EXPECT_TRUE(ptr);
    EXPECT_EQ(ptr->value, 42);
    EXPECT_EQ(TrackedObject::constructCount, 1);
    EXPECT_EQ(TrackedObject::destructCount, 0);
}

TEST(UniquePtr, Destructor_DeletesObject)
{
    TrackedObject::ResetCounts();

    {
        UniquePtr<TrackedObject> ptr(new TrackedObject());
    }

    EXPECT_EQ(TrackedObject::destructCount, 1);
}

TEST(UniquePtr, MoveConstructor_TransfersOwnership)
{
    UniquePtr<TrackedObject> ptr1(new TrackedObject(10));
    UniquePtr<TrackedObject> ptr2(std::move(ptr1));

    EXPECT_FALSE(ptr1);
    EXPECT_TRUE(ptr2);
    EXPECT_EQ(ptr2->value, 10);
}

TEST(UniquePtr, MoveAssignment_TransfersOwnership)
{
    TrackedObject::ResetCounts();

    UniquePtr<TrackedObject> ptr1(new TrackedObject(20));
    UniquePtr<TrackedObject> ptr2(new TrackedObject(30));

    ptr2 = std::move(ptr1);

    EXPECT_FALSE(ptr1);
    EXPECT_TRUE(ptr2);
    EXPECT_EQ(ptr2->value, 20);
    EXPECT_EQ(TrackedObject::destructCount, 1);  // ptr2's original object deleted
}

TEST(UniquePtr, Release_ReturnsPointerWithoutDeleting)
{
    TrackedObject::ResetCounts();

    UniquePtr<TrackedObject> ptr(new TrackedObject());
    TrackedObject* raw = ptr.Release();

    EXPECT_FALSE(ptr);
    EXPECT_NE(raw, nullptr);
    EXPECT_EQ(TrackedObject::destructCount, 0);

    delete raw;
}

TEST(UniquePtr, Reset_DeletesOldObject)
{
    TrackedObject::ResetCounts();

    UniquePtr<TrackedObject> ptr(new TrackedObject());
    ptr.Reset(new TrackedObject());

    EXPECT_EQ(TrackedObject::destructCount, 1);
}

TEST(UniquePtr, MakeUnique_CreatesObject)
{
    auto ptr = MakeUnique<TrackedObject>(100);

    EXPECT_TRUE(ptr);
    EXPECT_EQ(ptr->value, 100);
}

TEST(UniquePtr, ArraySpecialization_WorksWithArrays)
{
    UniquePtr<int[]> arr = MakeUniqueArray<int>(5);

    arr[0] = 10;
    arr[4] = 50;

    EXPECT_EQ(arr[0], 10);
    EXPECT_EQ(arr[4], 50);
}

// ==============================================================================
// RefPtr Tests
// ==============================================================================

TEST(RefPtr, DefaultConstruction_IsNull)
{
    RefPtr<TrackedObject> ptr;

    EXPECT_FALSE(ptr);
    EXPECT_EQ(ptr.Get(), nullptr);
}

TEST(RefPtr, ConstructWithPointer_IncrementsRefCount)
{
    TrackedObject* obj = new TrackedObject();

    EXPECT_EQ(obj->GetRefCount(), 0);

    RefPtr<TrackedObject> ptr(obj);

    EXPECT_EQ(obj->GetRefCount(), 1);
}

TEST(RefPtr, CopyConstructor_SharesOwnership)
{
    RefPtr<TrackedObject> ptr1(new TrackedObject(42));
    RefPtr<TrackedObject> ptr2(ptr1);

    EXPECT_EQ(ptr1.Get(), ptr2.Get());
    EXPECT_EQ(ptr1->GetRefCount(), 2);
}

TEST(RefPtr, CopyAssignment_SharesOwnership)
{
    TrackedObject::ResetCounts();

    RefPtr<TrackedObject> ptr1(new TrackedObject(10));
    RefPtr<TrackedObject> ptr2(new TrackedObject(20));

    ptr2 = ptr1;

    EXPECT_EQ(ptr1.Get(), ptr2.Get());
    EXPECT_EQ(ptr1->GetRefCount(), 2);
    EXPECT_EQ(TrackedObject::destructCount, 1);  // ptr2's original object deleted
}

TEST(RefPtr, Destructor_DecrementsRefCount)
{
    TrackedObject::ResetCounts();
    TrackedObject* obj = new TrackedObject();

    {
        RefPtr<TrackedObject> ptr1(obj);
        EXPECT_EQ(obj->GetRefCount(), 1);

        {
            RefPtr<TrackedObject> ptr2(ptr1);
            EXPECT_EQ(obj->GetRefCount(), 2);
        }

        EXPECT_EQ(obj->GetRefCount(), 1);
        EXPECT_EQ(TrackedObject::destructCount, 0);
    }

    EXPECT_EQ(TrackedObject::destructCount, 1);
}

TEST(RefPtr, LastReferenceDestruction_DeletesObject)
{
    TrackedObject::ResetCounts();

    {
        RefPtr<TrackedObject> ptr(new TrackedObject());
    }

    EXPECT_EQ(TrackedObject::destructCount, 1);
}

TEST(RefPtr, MultipleReferences_OnlyDeleteWhenLastGone)
{
    TrackedObject::ResetCounts();

    RefPtr<TrackedObject> ptr1(new TrackedObject());
    RefPtr<TrackedObject> ptr2 = ptr1;
    RefPtr<TrackedObject> ptr3 = ptr1;

    EXPECT_EQ(ptr1->GetRefCount(), 3);

    ptr1.Reset();
    EXPECT_EQ(TrackedObject::destructCount, 0);

    ptr2.Reset();
    EXPECT_EQ(TrackedObject::destructCount, 0);

    ptr3.Reset();
    EXPECT_EQ(TrackedObject::destructCount, 1);
}

TEST(RefPtr, Reset_DecrementsRefCount)
{
    RefPtr<TrackedObject> ptr(new TrackedObject());

    EXPECT_EQ(ptr->GetRefCount(), 1);

    ptr.Reset();

    EXPECT_FALSE(ptr);
}

TEST(RefPtr, MoveConstructor_TransfersWithoutIncrement)
{
    RefPtr<TrackedObject> ptr1(new TrackedObject());
    int refCount = ptr1->GetRefCount();

    RefPtr<TrackedObject> ptr2(std::move(ptr1));

    EXPECT_FALSE(ptr1);
    EXPECT_TRUE(ptr2);
    EXPECT_EQ(ptr2->GetRefCount(), refCount);  // Should not increment
}

// ==============================================================================
// WeakPtr Tests
// ==============================================================================

TEST(WeakPtr, DefaultConstruction_IsExpired)
{
    WeakPtr<TrackedObject> weak;

    EXPECT_TRUE(weak.IsExpired());
}

TEST(WeakPtr, ConstructFromRefPtr_DoesNotIncrementRefCount)
{
    RefPtr<TrackedObject> ref(new TrackedObject());
    int refCount = ref->GetRefCount();

    WeakPtr<TrackedObject> weak(ref);

    EXPECT_EQ(ref->GetRefCount(), refCount);  // Should not change
    EXPECT_FALSE(weak.IsExpired());
}

TEST(WeakPtr, Lock_UpgradesToRefPtr)
{
    RefPtr<TrackedObject> ref(new TrackedObject(42));
    WeakPtr<TrackedObject> weak(ref);

    RefPtr<TrackedObject> locked = weak.Lock();

    EXPECT_TRUE(locked);
    EXPECT_EQ(locked->value, 42);
    EXPECT_EQ(ref->GetRefCount(), 2);
}

TEST(WeakPtr, Lock_ReturnsNullWhenExpired)
{
    WeakPtr<TrackedObject> weak;

    {
        RefPtr<TrackedObject> ref(new TrackedObject());
        weak = ref;
    }  // ref destroyed

    RefPtr<TrackedObject> locked = weak.Lock();

    EXPECT_FALSE(locked);
    EXPECT_TRUE(weak.IsExpired());
}

TEST(WeakPtr, IsExpired_TrueAfterRefPtrDestroyed)
{
    WeakPtr<TrackedObject> weak;

    {
        RefPtr<TrackedObject> ref(new TrackedObject());
        weak = ref;
        EXPECT_FALSE(weak.IsExpired());
    }

    EXPECT_TRUE(weak.IsExpired());
}

TEST(WeakPtr, CopyConstructor_SharesWeakReference)
{
    RefPtr<TrackedObject> ref(new TrackedObject());
    WeakPtr<TrackedObject> weak1(ref);
    WeakPtr<TrackedObject> weak2(weak1);

    EXPECT_FALSE(weak1.IsExpired());
    EXPECT_FALSE(weak2.IsExpired());
    EXPECT_EQ(ref->GetRefCount(), 1);  // Weak references don't increment
}

TEST(WeakPtr, Reset_MakesExpired)
{
    RefPtr<TrackedObject> ref(new TrackedObject());
    WeakPtr<TrackedObject> weak(ref);

    weak.Reset();

    EXPECT_TRUE(weak.IsExpired());
}

// ==============================================================================
// Circular Reference Tests
// ==============================================================================

class Node : public RefCounted
{
public:
    RefPtr<Node> next;
    WeakPtr<Node> parent;
    int data;

    Node(int d) : data(d) {}
};

TEST(RefPtr, CircularReferenceWithWeakPtr_BreaksCycle)
{
    TrackedObject::ResetCounts();

    RefPtr<Node> parent(new Node(1));
    RefPtr<Node> child(new Node(2));

    parent->next = child;
    child->parent = parent;  // Weak reference breaks cycle

    EXPECT_EQ(parent->GetRefCount(), 1);  // Only parent RefPtr
    EXPECT_EQ(child->GetRefCount(), 2);   // parent->next + child RefPtr

    // When parent goes out of scope, both should be deleted
}

// ==============================================================================
// Thread Safety Tests (Basic)
// ==============================================================================

#include <thread>
#include <atomic>

TEST(RefPtr, ConcurrentCopyConstruction_ThreadSafe)
{
    RefPtr<TrackedObject> original(new TrackedObject(100));
    std::atomic<int> copyCount{0};

    auto copyThread = [&]() {
        for (int i = 0; i < 100; ++i)
        {
            RefPtr<TrackedObject> copy = original;
            copyCount++;
        }
    };

    std::thread t1(copyThread);
    std::thread t2(copyThread);
    std::thread t3(copyThread);

    t1.join();
    t2.join();
    t3.join();

    EXPECT_EQ(copyCount, 300);
    EXPECT_EQ(original->GetRefCount(), 1);  // All copies destroyed
}

TEST(RefPtr, ConcurrentRefCountIncrement_Atomic)
{
    TrackedObject::ResetCounts();

    RefPtr<TrackedObject> shared(new TrackedObject());
    std::vector<std::thread> threads;

    // Create 10 threads that each create 100 copies
    for (int i = 0; i < 10; ++i)
    {
        threads.emplace_back([&shared]() {
            std::vector<RefPtr<TrackedObject>> copies;
            for (int j = 0; j < 100; ++j)
            {
                copies.push_back(shared);
            }
            // All copies destroyed at end of scope
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    // After all threads finish, only original shared pointer remains
    EXPECT_EQ(shared->GetRefCount(), 1);
    EXPECT_EQ(TrackedObject::destructCount, 0);

    shared.Reset();
    EXPECT_EQ(TrackedObject::destructCount, 1);
}

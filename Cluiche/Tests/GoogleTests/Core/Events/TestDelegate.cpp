// TestDelegate.cpp - Google Test unit tests for Delegate
//
// Tests the Dia::Core::Events::Delegate multi-cast delegate system

#include <gtest/gtest.h>
#include <DiaCore/Architecture/Events/Delegate.h>
#include <atomic>
#include <thread>
#include <chrono>

using namespace Dia::Core::Events;

// ==============================================================================
// Helper Classes for Testing
// ==============================================================================

class TestObject
{
public:
    int callCount = 0;
    int lastValue = 0;

    void OnCallback(int value)
    {
        callCount++;
        lastValue = value;
    }

    void OnCallbackNoArgs()
    {
        callCount++;
    }
};

// ==============================================================================
// Basic Add/Remove Tests
// ==============================================================================

TEST(Delegate, DefaultConstruction_HasNoSubscribers)
{
    Delegate<> delegate;

    EXPECT_EQ(delegate.GetSubscriberCount(), 0);
    EXPECT_FALSE(delegate.HasSubscribers());
}

TEST(Delegate, AddLambda_ReturnsNonZeroID)
{
    Delegate<> delegate;

    int id = delegate.Add([]() {});

    EXPECT_NE(id, 0) << "Valid callback ID should be non-zero";
    EXPECT_EQ(delegate.GetSubscriberCount(), 1);
    EXPECT_TRUE(delegate.HasSubscribers());
}

TEST(Delegate, AddNullCallback_ReturnsZero)
{
    Delegate<> delegate;

    int id = delegate.Add(nullptr);

    EXPECT_EQ(id, 0) << "Null callback should return ID 0";
    EXPECT_EQ(delegate.GetSubscriberCount(), 0);
}

TEST(Delegate, AddMultipleCallbacks_IncreasesSubscriberCount)
{
    Delegate<> delegate;

    delegate.Add([]() {});
    delegate.Add([]() {});
    delegate.Add([]() {});

    EXPECT_EQ(delegate.GetSubscriberCount(), 3);
}

TEST(Delegate, AddCallback_ReturnsUniqueIDs)
{
    Delegate<> delegate;

    int id1 = delegate.Add([]() {});
    int id2 = delegate.Add([]() {});
    int id3 = delegate.Add([]() {});

    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_NE(id1, id3);
}

TEST(Delegate, RemoveValidID_ReturnsTrue)
{
    Delegate<> delegate;

    int id = delegate.Add([]() {});
    bool removed = delegate.Remove(id);

    EXPECT_TRUE(removed);
    EXPECT_EQ(delegate.GetSubscriberCount(), 0);
}

TEST(Delegate, RemoveInvalidID_ReturnsFalse)
{
    Delegate<> delegate;

    bool removed = delegate.Remove(9999);

    EXPECT_FALSE(removed);
}

TEST(Delegate, RemoveSameIDTwice_SecondReturnsFalse)
{
    Delegate<> delegate;

    int id = delegate.Add([]() {});
    delegate.Remove(id);
    bool removed = delegate.Remove(id);

    EXPECT_FALSE(removed);
}

TEST(Delegate, Clear_RemovesAllSubscribers)
{
    Delegate<> delegate;

    delegate.Add([]() {});
    delegate.Add([]() {});
    delegate.Add([]() {});

    delegate.Clear();

    EXPECT_EQ(delegate.GetSubscriberCount(), 0);
    EXPECT_FALSE(delegate.HasSubscribers());
}

// ==============================================================================
// Invocation Tests
// ==============================================================================

TEST(Delegate, InvokeNoArgs_CallsAllSubscribers)
{
    Delegate<> delegate;
    int callCount = 0;

    delegate.Add([&callCount]() { callCount++; });
    delegate.Add([&callCount]() { callCount++; });
    delegate.Add([&callCount]() { callCount++; });

    delegate.Invoke();

    EXPECT_EQ(callCount, 3);
}

TEST(Delegate, InvokeWithArgs_PassesParametersCorrectly)
{
    Delegate<int, float> delegate;
    int receivedInt = 0;
    float receivedFloat = 0.0f;

    delegate.Add([&](int i, float f) {
        receivedInt = i;
        receivedFloat = f;
    });

    delegate.Invoke(42, 3.14f);

    EXPECT_EQ(receivedInt, 42);
    EXPECT_FLOAT_EQ(receivedFloat, 3.14f);
}

TEST(Delegate, OperatorParentheses_InvokesDelegate)
{
    Delegate<int> delegate;
    int value = 0;

    delegate.Add([&value](int v) { value = v; });

    delegate(100);  // Use operator()

    EXPECT_EQ(value, 100);
}

TEST(Delegate, InvokeWithNoSubscribers_DoesNotCrash)
{
    Delegate<> delegate;

    // Should not crash
    EXPECT_NO_THROW(delegate.Invoke());
}

TEST(Delegate, InvokeMultipleTimes_CallsEachTime)
{
    Delegate<> delegate;
    int callCount = 0;

    delegate.Add([&callCount]() { callCount++; });

    delegate.Invoke();
    delegate.Invoke();
    delegate.Invoke();

    EXPECT_EQ(callCount, 3);
}

// ==============================================================================
// Member Function Binding Tests
// ==============================================================================

TEST(Delegate, MemberFunctionBinding_CallsMemberFunction)
{
    Delegate<int> delegate;
    TestObject obj;

    delegate.Add(BIND_MEMBER_FUNC(OnCallback, &obj));

    delegate.Invoke(42);

    EXPECT_EQ(obj.callCount, 1);
    EXPECT_EQ(obj.lastValue, 42);
}

TEST(Delegate, MemberFunctionBindingNoArgs_CallsMemberFunction)
{
    Delegate<> delegate;
    TestObject obj;

    delegate.Add(BIND_MEMBER_FUNC(OnCallbackNoArgs, &obj));

    delegate.Invoke();

    EXPECT_EQ(obj.callCount, 1);
}

TEST(Delegate, MultipleMemberFunctions_AllCalled)
{
    Delegate<int> delegate;
    TestObject obj1, obj2, obj3;

    delegate.Add(BIND_MEMBER_FUNC(OnCallback, &obj1));
    delegate.Add(BIND_MEMBER_FUNC(OnCallback, &obj2));
    delegate.Add(BIND_MEMBER_FUNC(OnCallback, &obj3));

    delegate.Invoke(100);

    EXPECT_EQ(obj1.callCount, 1);
    EXPECT_EQ(obj2.callCount, 1);
    EXPECT_EQ(obj3.callCount, 1);
}

// ==============================================================================
// Callback Order Tests
// ==============================================================================

TEST(Delegate, InvocationOrder_MatchesSubscriptionOrder)
{
    Delegate<> delegate;
    std::vector<int> callOrder;

    delegate.Add([&callOrder]() { callOrder.push_back(1); });
    delegate.Add([&callOrder]() { callOrder.push_back(2); });
    delegate.Add([&callOrder]() { callOrder.push_back(3); });

    delegate.Invoke();

    ASSERT_EQ(callOrder.size(), 3);
    EXPECT_EQ(callOrder[0], 1);
    EXPECT_EQ(callOrder[1], 2);
    EXPECT_EQ(callOrder[2], 3);
}

// ==============================================================================
// Removal During Invocation Tests
// ==============================================================================

TEST(Delegate, RemoveDuringInvocation_DoesNotAffectCurrentInvocation)
{
    Delegate<> delegate;
    int callCount = 0;
    int id1 = 0, id2 = 0, id3 = 0;

    // First callback removes second
    id1 = delegate.Add([&]() {
        callCount++;
        delegate.Remove(id2);  // Remove second callback
    });

    // Second callback (will be removed during invocation)
    id2 = delegate.Add([&]() {
        callCount++;
    });

    // Third callback
    id3 = delegate.Add([&]() {
        callCount++;
    });

    delegate.Invoke();

    // All three callbacks should execute because removal
    // doesn't affect the snapshot taken before invocation
    EXPECT_EQ(callCount, 3);

    // But second callback should be removed for next invocation
    EXPECT_EQ(delegate.GetSubscriberCount(), 2);
}

// ==============================================================================
// Thread Safety Tests (Basic)
// ==============================================================================

TEST(Delegate, ConcurrentAdd_AllCallbacksRegistered)
{
    Delegate<> delegate;
    std::atomic<int> callCount{0};

    // Add callbacks from multiple threads
    auto addCallbacks = [&]() {
        for (int i = 0; i < 10; ++i)
        {
            delegate.Add([&callCount]() { callCount++; });
        }
    };

    std::thread t1(addCallbacks);
    std::thread t2(addCallbacks);
    std::thread t3(addCallbacks);

    t1.join();
    t2.join();
    t3.join();

    EXPECT_EQ(delegate.GetSubscriberCount(), 30);

    // Invoke and verify all callbacks execute
    delegate.Invoke();
    EXPECT_EQ(callCount, 30);
}

TEST(Delegate, ConcurrentRemove_ThreadSafe)
{
    Delegate<> delegate;

    // Add 100 callbacks
    std::vector<int> ids;
    for (int i = 0; i < 100; ++i)
    {
        ids.push_back(delegate.Add([]() {}));
    }

    // Remove from multiple threads
    auto removeCallbacks = [&](int start, int count) {
        for (int i = start; i < start + count && i < ids.size(); ++i)
        {
            delegate.Remove(ids[i]);
        }
    };

    std::thread t1(removeCallbacks, 0, 33);
    std::thread t2(removeCallbacks, 33, 33);
    std::thread t3(removeCallbacks, 66, 34);

    t1.join();
    t2.join();
    t3.join();

    EXPECT_EQ(delegate.GetSubscriberCount(), 0);
}

// ==============================================================================
// ScopedDelegateConnection Tests
// ==============================================================================

TEST(ScopedDelegateConnection, DefaultConstruction_NotConnected)
{
    ScopedDelegateConnection<> connection;

    EXPECT_FALSE(connection.IsConnected());
}

TEST(ScopedDelegateConnection, ValidConnection_IsConnected)
{
    Delegate<> delegate;

    int id = delegate.Add([]() {});
    ScopedDelegateConnection<> connection(&delegate, id);

    EXPECT_TRUE(connection.IsConnected());
}

TEST(ScopedDelegateConnection, Destructor_AutomaticallyDisconnects)
{
    Delegate<> delegate;

    {
        int id = delegate.Add([]() {});
        ScopedDelegateConnection<> connection(&delegate, id);

        EXPECT_EQ(delegate.GetSubscriberCount(), 1);
    }  // connection destroyed here

    EXPECT_EQ(delegate.GetSubscriberCount(), 0);
}

TEST(ScopedDelegateConnection, ManualDisconnect_RemovesCallback)
{
    Delegate<> delegate;

    int id = delegate.Add([]() {});
    ScopedDelegateConnection<> connection(&delegate, id);

    connection.Disconnect();

    EXPECT_FALSE(connection.IsConnected());
    EXPECT_EQ(delegate.GetSubscriberCount(), 0);
}

TEST(ScopedDelegateConnection, MoveConstruction_TransfersOwnership)
{
    Delegate<> delegate;

    int id = delegate.Add([]() {});
    ScopedDelegateConnection<> connection1(&delegate, id);

    ScopedDelegateConnection<> connection2(std::move(connection1));

    EXPECT_FALSE(connection1.IsConnected());
    EXPECT_TRUE(connection2.IsConnected());
    EXPECT_EQ(delegate.GetSubscriberCount(), 1);
}

TEST(ScopedDelegateConnection, MoveAssignment_TransfersOwnership)
{
    Delegate<> delegate;

    int id = delegate.Add([]() {});
    ScopedDelegateConnection<> connection1(&delegate, id);
    ScopedDelegateConnection<> connection2;

    connection2 = std::move(connection1);

    EXPECT_FALSE(connection1.IsConnected());
    EXPECT_TRUE(connection2.IsConnected());
    EXPECT_EQ(delegate.GetSubscriberCount(), 1);
}

// ==============================================================================
// Edge Case Tests
// ==============================================================================

TEST(Delegate, AddCallbackDuringInvocation_NotCalledInCurrentInvocation)
{
    Delegate<> delegate;
    int callCount = 0;

    // First callback adds a new callback
    delegate.Add([&]() {
        callCount++;
        delegate.Add([&]() { callCount++; });
    });

    delegate.Invoke();

    // Only first callback should execute
    EXPECT_EQ(callCount, 1);

    // But new callback should be registered
    EXPECT_EQ(delegate.GetSubscriberCount(), 2);

    // Next invocation should call both
    delegate.Invoke();
    EXPECT_EQ(callCount, 3);
}

TEST(Delegate, ClearDuringInvocation_DoesNotAffectCurrentInvocation)
{
    Delegate<> delegate;
    int callCount = 0;

    delegate.Add([&]() {
        callCount++;
        delegate.Clear();
    });
    delegate.Add([&]() { callCount++; });
    delegate.Add([&]() { callCount++; });

    delegate.Invoke();

    // All callbacks execute because Clear happens during snapshot iteration
    EXPECT_EQ(callCount, 3);

    // But all callbacks should be cleared after
    EXPECT_EQ(delegate.GetSubscriberCount(), 0);
}

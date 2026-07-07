#include <gtest/gtest.h>

#include <DiaBlackboard/Blackboard.h>
#include <DiaBlackboard/BlackboardComponent.h>
#include <DiaBlackboard/GlobalBlackboard.h>
#include <DiaBlackboard/Testing/BlackboardTestHelpers.h>

using namespace Dia::Blackboard;
using namespace Dia::Blackboard::Testing;

struct HealthBoard
{
    float mCurrentHp = 100.0f;
    float mMaxHp     = 100.0f;
};

struct ThreatBoard
{
    int mThreatLevel = 0;
};

// -------------------------------------------------------------------------
// Register / Has / Unregister
// -------------------------------------------------------------------------

TEST(DiaBlackboard, RegisterAndHas)
{
    Blackboard board;
    EXPECT_FALSE(board.Has(Dia::Core::StringCRC("Health")));

    board.Register<HealthBoard>(Dia::Core::StringCRC("Health"));

    EXPECT_TRUE(board.Has(Dia::Core::StringCRC("Health")));
}

TEST(DiaBlackboard, UnregisterRemovesSlot)
{
    Blackboard board;
    board.Register<HealthBoard>(Dia::Core::StringCRC("Health"));
    board.Unregister(Dia::Core::StringCRC("Health"));
    EXPECT_FALSE(board.Has(Dia::Core::StringCRC("Health")));
}

TEST(DiaBlackboard, MultipleSlots)
{
    Blackboard board;
    board.Register<HealthBoard>(Dia::Core::StringCRC("Health"));
    board.Register<ThreatBoard>(Dia::Core::StringCRC("Threat"));

    EXPECT_TRUE(board.Has(Dia::Core::StringCRC("Health")));
    EXPECT_TRUE(board.Has(Dia::Core::StringCRC("Threat")));
}

// -------------------------------------------------------------------------
// Get (typed access)
// -------------------------------------------------------------------------

TEST(DiaBlackboard, GetReturnsWritableReference)
{
    Blackboard board;
    board.Register<HealthBoard>(Dia::Core::StringCRC("Health"));

    HealthBoard& h = board.Get<HealthBoard>(Dia::Core::StringCRC("Health"));
    h.mCurrentHp = 50.0f;

    const HealthBoard& h2 = board.Get<HealthBoard>(Dia::Core::StringCRC("Health"));
    EXPECT_FLOAT_EQ(h2.mCurrentHp, 50.0f);
}

TEST(DiaBlackboard, RegisterReturnsReference)
{
    Blackboard board;
    HealthBoard& h = board.Register<HealthBoard>(Dia::Core::StringCRC("Health"));
    h.mCurrentHp = 75.0f;

    EXPECT_FLOAT_EQ(board.Get<HealthBoard>(Dia::Core::StringCRC("Health")).mCurrentHp, 75.0f);
}

// -------------------------------------------------------------------------
// TryGet (nullable access)
// -------------------------------------------------------------------------

TEST(DiaBlackboard, TryGetReturnsNullWhenAbsent)
{
    Blackboard board;
    HealthBoard* h = board.TryGet<HealthBoard>(Dia::Core::StringCRC("Health"));
    EXPECT_EQ(h, nullptr);
}

TEST(DiaBlackboard, TryGetReturnsPointerWhenPresent)
{
    Blackboard board;
    board.Register<HealthBoard>(Dia::Core::StringCRC("Health"));
    HealthBoard* h = board.TryGet<HealthBoard>(Dia::Core::StringCRC("Health"));
    ASSERT_NE(h, nullptr);
    h->mCurrentHp = 42.0f;
    EXPECT_FLOAT_EQ(board.Get<HealthBoard>(Dia::Core::StringCRC("Health")).mCurrentHp, 42.0f);
}

TEST(DiaBlackboard, TryGetWrongTypeReturnsNull)
{
    Blackboard board;
    board.Register<HealthBoard>(Dia::Core::StringCRC("Health"));
    // Ask for ThreatBoard on a HealthBoard slot — should return null, not crash
    ThreatBoard* t = board.TryGet<ThreatBoard>(Dia::Core::StringCRC("Health"));
    EXPECT_EQ(t, nullptr);
}

// -------------------------------------------------------------------------
// Const access
// -------------------------------------------------------------------------

TEST(DiaBlackboard, ConstGet)
{
    Blackboard board;
    board.Register<HealthBoard>(Dia::Core::StringCRC("Health"));
    board.Get<HealthBoard>(Dia::Core::StringCRC("Health")).mCurrentHp = 30.0f;

    const Blackboard& cb = board;
    EXPECT_FLOAT_EQ(cb.Get<HealthBoard>(Dia::Core::StringCRC("Health")).mCurrentHp, 30.0f);
}

TEST(DiaBlackboard, ConstTryGet)
{
    Blackboard board;
    board.Register<HealthBoard>(Dia::Core::StringCRC("Health"));

    const Blackboard& cb = board;
    const HealthBoard* h = cb.TryGet<HealthBoard>(Dia::Core::StringCRC("Health"));
    ASSERT_NE(h, nullptr);
    EXPECT_FLOAT_EQ(h->mMaxHp, 100.0f);
}

// -------------------------------------------------------------------------
// Observer
// -------------------------------------------------------------------------

TEST(DiaBlackboard, ObserverNotifiedOnRegister)
{
    Blackboard board;
    MockBlackboardObserver obs;
    board.AddObserver(obs);

    board.Register<HealthBoard>(Dia::Core::StringCRC("Health"));

    EXPECT_EQ(obs.registerCount, 1);
    EXPECT_EQ(obs.lastRegistered, Dia::Core::StringCRC("Health"));
    EXPECT_EQ(obs.unregisterCount, 0);
}

TEST(DiaBlackboard, ObserverNotifiedOnUnregister)
{
    Blackboard board;
    MockBlackboardObserver obs;
    board.AddObserver(obs);

    board.Register<HealthBoard>(Dia::Core::StringCRC("Health"));
    board.Unregister(Dia::Core::StringCRC("Health"));

    EXPECT_EQ(obs.unregisterCount, 1);
    EXPECT_EQ(obs.lastUnregistered, Dia::Core::StringCRC("Health"));
}

TEST(DiaBlackboard, ObserverNotNotifiedAfterRemoval)
{
    Blackboard board;
    MockBlackboardObserver obs;
    board.AddObserver(obs);
    board.RemoveObserver(obs);

    board.Register<HealthBoard>(Dia::Core::StringCRC("Health"));

    EXPECT_EQ(obs.registerCount, 0);
}

TEST(DiaBlackboard, MultipleObservers)
{
    Blackboard board;
    MockBlackboardObserver obs1, obs2;
    board.AddObserver(obs1);
    board.AddObserver(obs2);

    board.Register<HealthBoard>(Dia::Core::StringCRC("Health"));

    EXPECT_EQ(obs1.registerCount, 1);
    EXPECT_EQ(obs2.registerCount, 1);
}

// -------------------------------------------------------------------------
// Test helpers
// -------------------------------------------------------------------------

TEST(DiaBlackboard, AssertHasSlotHelper)
{
    Blackboard board;
    board.Register<HealthBoard>(Dia::Core::StringCRC("Health"));
    AssertHasSlot(board, Dia::Core::StringCRC("Health"));
}

TEST(DiaBlackboard, AssertSlotAbsentHelper)
{
    Blackboard board;
    AssertSlotAbsent(board, Dia::Core::StringCRC("Health"));
}

// -------------------------------------------------------------------------
// BlackboardComponent
// -------------------------------------------------------------------------

TEST(DiaBlackboard, ComponentOwnsBoard)
{
    BlackboardComponent comp;
    Blackboard& board = comp.GetBlackboard();
    board.Register<HealthBoard>(Dia::Core::StringCRC("Health"));
    EXPECT_TRUE(comp.GetBlackboard().Has(Dia::Core::StringCRC("Health")));
}

TEST(DiaBlackboard, ComponentUniqueId)
{
    EXPECT_EQ(BlackboardComponent::kUniqueId, Dia::Core::StringCRC("BlackboardComponent"));
}

// -------------------------------------------------------------------------
// Destructor cleanup
// -------------------------------------------------------------------------

TEST(DiaBlackboard, DestructorFreesSlots)
{
    // Just ensure no crash / memory leak on destruction with live slots.
    // Valgrind/ASan would catch actual leaks.
    {
        Blackboard board;
        board.Register<HealthBoard>(Dia::Core::StringCRC("Health"));
        board.Register<ThreatBoard>(Dia::Core::StringCRC("Threat"));
        // board destroyed here
    }
    SUCCEED();
}

// -------------------------------------------------------------------------
// Re-registration after unregister
// -------------------------------------------------------------------------

TEST(DiaBlackboard, RegisterAfterUnregister)
{
    Blackboard board;
    board.Register<HealthBoard>(Dia::Core::StringCRC("Health"));
    board.Unregister(Dia::Core::StringCRC("Health"));

    HealthBoard& h = board.Register<HealthBoard>(Dia::Core::StringCRC("Health"));
    h.mCurrentHp = 55.0f;

    EXPECT_TRUE(board.Has(Dia::Core::StringCRC("Health")));
    EXPECT_FLOAT_EQ(board.Get<HealthBoard>(Dia::Core::StringCRC("Health")).mCurrentHp, 55.0f);
}

// -------------------------------------------------------------------------
// Observer receives correct keys across multiple slots
// -------------------------------------------------------------------------

TEST(DiaBlackboard, ObserverReceivesCorrectKeysForMultipleSlots)
{
    Blackboard board;
    MockBlackboardObserver obs;
    board.AddObserver(obs);

    board.Register<HealthBoard>(Dia::Core::StringCRC("Health"));
    board.Register<ThreatBoard>(Dia::Core::StringCRC("Threat"));

    EXPECT_EQ(obs.registerCount, 2);
    EXPECT_EQ(obs.lastRegistered, Dia::Core::StringCRC("Threat"));
}

// -------------------------------------------------------------------------
// Stress: fill to capacity and unregister all
// -------------------------------------------------------------------------

TEST(DiaBlackboard_Stress, FillToCapacityAndUnregisterAll)
{
    Blackboard board;
    const unsigned int kCapacity = 32;
    char keyBuf[16];

    for (unsigned int i = 0; i < kCapacity; ++i)
    {
        snprintf(keyBuf, sizeof(keyBuf), "Slot%u", i);
        board.Register<HealthBoard>(Dia::Core::StringCRC(keyBuf));
    }

    for (unsigned int i = 0; i < kCapacity; ++i)
    {
        snprintf(keyBuf, sizeof(keyBuf), "Slot%u", i);
        EXPECT_TRUE(board.Has(Dia::Core::StringCRC(keyBuf)));
    }

    for (unsigned int i = 0; i < kCapacity; ++i)
    {
        snprintf(keyBuf, sizeof(keyBuf), "Slot%u", i);
        board.Unregister(Dia::Core::StringCRC(keyBuf));
    }

    for (unsigned int i = 0; i < kCapacity; ++i)
    {
        snprintf(keyBuf, sizeof(keyBuf), "Slot%u", i);
        EXPECT_FALSE(board.Has(Dia::Core::StringCRC(keyBuf)));
    }
}

TEST(DiaBlackboard_Stress, RegisterUnregisterLoop)
{
    Blackboard board;
    for (int i = 0; i < 1000; ++i)
    {
        board.Register<HealthBoard>(Dia::Core::StringCRC("Health"));
        EXPECT_TRUE(board.Has(Dia::Core::StringCRC("Health")));
        board.Unregister(Dia::Core::StringCRC("Health"));
        EXPECT_FALSE(board.Has(Dia::Core::StringCRC("Health")));
    }
}

// -------------------------------------------------------------------------
// GlobalBlackboard
// -------------------------------------------------------------------------

TEST(DiaBlackboard, GlobalBlackboard_GetBoard)
{
    GlobalBlackboard instance;
    Blackboard& board = instance.GetBoard();
    board.Register<HealthBoard>(Dia::Core::StringCRC("GlobalHealth"));
    EXPECT_TRUE(board.Has(Dia::Core::StringCRC("GlobalHealth")));
    board.Unregister(Dia::Core::StringCRC("GlobalHealth"));
}

// -------------------------------------------------------------------------
// Boundary / Death tests (debug only)
// -------------------------------------------------------------------------

#ifdef _DEBUG

TEST(SLOW_DiaBlackboard_Boundary, Register_DuplicateKey_Asserts)
{
    Blackboard board;
    board.Register<HealthBoard>(Dia::Core::StringCRC("Health"));
    EXPECT_DEATH(board.Register<HealthBoard>(Dia::Core::StringCRC("Health")), "");
}

TEST(SLOW_DiaBlackboard_Boundary, Unregister_AbsentKey_Asserts)
{
    Blackboard board;
    EXPECT_DEATH(board.Unregister(Dia::Core::StringCRC("Health")), "");
}

TEST(SLOW_DiaBlackboard_Boundary, Get_AbsentKey_Asserts)
{
    Blackboard board;
    EXPECT_DEATH(board.Get<HealthBoard>(Dia::Core::StringCRC("Health")), "");
}

TEST(SLOW_DiaBlackboard_Boundary, Get_WrongType_Asserts)
{
    Blackboard board;
    board.Register<HealthBoard>(Dia::Core::StringCRC("Health"));
    EXPECT_DEATH(board.Get<ThreatBoard>(Dia::Core::StringCRC("Health")), "");
}

TEST(SLOW_DiaBlackboard_Boundary, Register_AtCapacity_Asserts)
{
    Blackboard board;
    char keyBuf[16];
    for (unsigned int i = 0; i < 32; ++i)
    {
        snprintf(keyBuf, sizeof(keyBuf), "Slot%u", i);
        board.Register<HealthBoard>(Dia::Core::StringCRC(keyBuf));
    }
    EXPECT_DEATH(board.Register<HealthBoard>(Dia::Core::StringCRC("Overflow")), "");
}

TEST(SLOW_DiaBlackboard_Boundary, AddObserver_AtCapacity_Asserts)
{
    Blackboard board;
    MockBlackboardObserver obs[8];
    for (int i = 0; i < 8; ++i)
        board.AddObserver(obs[i]);
    MockBlackboardObserver extra;
    EXPECT_DEATH(board.AddObserver(extra), "");
}

TEST(SLOW_DiaBlackboard_Boundary, RemoveObserver_NotRegistered_Asserts)
{
    Blackboard board;
    MockBlackboardObserver obs;
    EXPECT_DEATH(board.RemoveObserver(obs), "");
}

#endif // _DEBUG

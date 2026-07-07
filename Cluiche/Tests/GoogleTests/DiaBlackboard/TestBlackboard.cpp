#include <gtest/gtest.h>

#include <DiaBlackboard/Blackboard.h>
#include <DiaBlackboard/BlackboardComponent.h>
#include <DiaBlackboard/GlobalBlackboard.h>
#include <DiaBlackboard/Testing/BlackboardTestHelpers.h>
#include <DiaBlackboard/BlackboardRegistry.h>

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

// =========================================================================
// BlackboardRegistry tests
// =========================================================================

// -------------------------------------------------------------------------
// Register / Unregister / GetCount / GetAll
// -------------------------------------------------------------------------

// AC1: Register stores entry; GetCount increments; GetAll returns it
TEST(DiaBlackboardRegistry, Register_StoresEntry)
{
    BlackboardRegistry registry;
    Blackboard board;

    EXPECT_EQ(registry.GetCount(), 0);

    registry.Register(Dia::Core::StringCRC{"PlayerBoard"}, "Player", board);

    EXPECT_EQ(registry.GetCount(), 1);

    const auto& all = registry.GetAll();
    ASSERT_EQ(static_cast<int>(all.Size()), 1);
    EXPECT_EQ(all[0].id,    Dia::Core::StringCRC{"PlayerBoard"});
    EXPECT_STREQ(all[0].label, "Player");
    EXPECT_EQ(all[0].board, &board);
}

// AC2: Unregister removes entry by id; GetCount decrements
TEST(DiaBlackboardRegistry, Unregister_RemovesEntry)
{
    BlackboardRegistry registry;
    Blackboard board;

    registry.Register(Dia::Core::StringCRC{"PlayerBoard"}, "Player", board);
    EXPECT_EQ(registry.GetCount(), 1);

    registry.Unregister(Dia::Core::StringCRC{"PlayerBoard"});
    EXPECT_EQ(registry.GetCount(), 0);

    const auto& all = registry.GetAll();
    EXPECT_EQ(static_cast<int>(all.Size()), 0);
}

// AC3: Unregister with unknown id is a no-op — no crash, no assert
TEST(DiaBlackboardRegistry, Unregister_UnknownId_IsNoOp)
{
    BlackboardRegistry registry;

    // Should not crash or assert when the id was never registered
    registry.Unregister(Dia::Core::StringCRC{"NonExistent"});

    EXPECT_EQ(registry.GetCount(), 0);
}

// AC4: GetAll() return type is const DynamicArrayC<BlackboardEntry, 16>& — no STL
// (compile-only check via static_assert)
TEST(DiaBlackboardRegistry, GetAll_ReturnsCorrectType)
{
    using ExpectedType = const Dia::Core::Containers::DynamicArrayC<BlackboardEntry, 16>&;
    using ActualType   = decltype(std::declval<const BlackboardRegistry>().GetAll());
    static_assert(std::is_same<ActualType, ExpectedType>::value,
                  "GetAll() must return const DynamicArrayC<BlackboardEntry, 16>&");
    SUCCEED();
}

// -------------------------------------------------------------------------
// Serializer
// -------------------------------------------------------------------------

// AC5: RegisterSerializer stores a lambda; HasSerializer returns true for that type's typeTag
TEST(DiaBlackboardRegistry, RegisterSerializer_HasSerializer_ReturnsTrue)
{
    BlackboardRegistry registry;

    registry.RegisterSerializer<HealthBoard>([](const void* /*data*/, Json::Value& /*out*/) {});

    EXPECT_TRUE(registry.HasSerializer(Dia::Blackboard::Detail::TypeTag<HealthBoard>()));
}

// AC6: Serialize calls the registered lambda and populates the Json::Value
TEST(DiaBlackboardRegistry, Serialize_CallsLambda)
{
    BlackboardRegistry registry;

    registry.RegisterSerializer<HealthBoard>([](const void* data, Json::Value& out)
    {
        const HealthBoard* hb = static_cast<const HealthBoard*>(data);
        out["current"] = hb->mCurrentHp;
    });

    HealthBoard hb;
    hb.mCurrentHp = 75.0f;

    Json::Value result;
    registry.Serialize(Dia::Blackboard::Detail::TypeTag<HealthBoard>(), &hb, result);

    EXPECT_FLOAT_EQ(result["current"].asFloat(), 75.0f);
}

// AC7: HasSerializer returns false for a type with no registered serializer
TEST(DiaBlackboardRegistry, HasSerializer_UnregisteredType_ReturnsFalse)
{
    BlackboardRegistry registry;

    // ThreatBoard has never had a serializer registered
    EXPECT_FALSE(registry.HasSerializer(Dia::Blackboard::Detail::TypeTag<ThreatBoard>()));
}

// -------------------------------------------------------------------------
// Observer (IBlackboardObserver / MockBlackboardObserver)
// -------------------------------------------------------------------------

// AC10: MockBlackboardObserver::GetId() returns StringCRC{"MockObserver"}
TEST(DiaBlackboardRegistry, MockObserver_GetId_ReturnsMockObserver)
{
    MockBlackboardObserver obs;
    EXPECT_EQ(obs.GetId(), Dia::Core::StringCRC{"MockObserver"});
}

// -------------------------------------------------------------------------
// Non-owning pointer semantics (AC14)
// -------------------------------------------------------------------------

// AC14: BlackboardEntry::board is const Blackboard* (non-owning);
// registry does not call delete — creating and destroying the board
// must leave the registry intact (no crash, no double-free).
TEST(DiaBlackboardRegistry, Entry_BoardPointer_IsNonOwning)
{
    BlackboardRegistry registry;

    {
        Blackboard board;
        registry.Register(Dia::Core::StringCRC{"TempBoard"}, "Temp", board);
        EXPECT_EQ(registry.GetCount(), 1);
        // board destroyed here — registry must NOT call delete on board
    }

    // Registry still holds the (now dangling) entry; that is intentional
    // non-owning semantics.  The count should remain 1 — the registry
    // does not observe board lifetime.
    EXPECT_EQ(registry.GetCount(), 1);

    // Unregistering after the board is gone must not crash (no delete called)
    registry.Unregister(Dia::Core::StringCRC{"TempBoard"});
    EXPECT_EQ(registry.GetCount(), 0);
}

// -------------------------------------------------------------------------
// Overflow / Boundary / Death tests (debug only)
// -------------------------------------------------------------------------

#ifdef _DEBUG

// AC12: Max 16 board entries (kMaxEntries = 16); assert on overflow
TEST(SLOW_DiaBlackboardRegistry_Boundary, Register_AtCapacity_Asserts)
{
    BlackboardRegistry registry;
    Blackboard boards[16];
    char labelBuf[16];

    for (unsigned int i = 0; i < 16; ++i)
    {
        snprintf(labelBuf, sizeof(labelBuf), "Board%u", i);
        registry.Register(Dia::Core::StringCRC{labelBuf}, labelBuf, boards[i]);
    }

    EXPECT_EQ(registry.GetCount(), 16);

    // 17th registration must assert
    Blackboard extra;
    EXPECT_DEATH(registry.Register(Dia::Core::StringCRC{"Overflow"}, "Overflow", extra), "");
}

// AC13: Max 32 serializer entries (kMaxSerializers = 32); assert on overflow
TEST(SLOW_DiaBlackboardRegistry_Boundary, RegisterSerializer_AtCapacity_Asserts)
{
    // We need 33 distinct types to overflow the serializer table.
    // Use a helper struct template to mint unique types at compile time.
    struct S00 {}; struct S01 {}; struct S02 {}; struct S03 {};
    struct S04 {}; struct S05 {}; struct S06 {}; struct S07 {};
    struct S08 {}; struct S09 {}; struct S10 {}; struct S11 {};
    struct S12 {}; struct S13 {}; struct S14 {}; struct S15 {};
    struct S16 {}; struct S17 {}; struct S18 {}; struct S19 {};
    struct S20 {}; struct S21 {}; struct S22 {}; struct S23 {};
    struct S24 {}; struct S25 {}; struct S26 {}; struct S27 {};
    struct S28 {}; struct S29 {}; struct S30 {}; struct S31 {};
    struct S32 {};

    BlackboardRegistry registry;
    auto noopFn = [](const void*, Json::Value&) {};

    registry.RegisterSerializer<S00>(noopFn); registry.RegisterSerializer<S01>(noopFn);
    registry.RegisterSerializer<S02>(noopFn); registry.RegisterSerializer<S03>(noopFn);
    registry.RegisterSerializer<S04>(noopFn); registry.RegisterSerializer<S05>(noopFn);
    registry.RegisterSerializer<S06>(noopFn); registry.RegisterSerializer<S07>(noopFn);
    registry.RegisterSerializer<S08>(noopFn); registry.RegisterSerializer<S09>(noopFn);
    registry.RegisterSerializer<S10>(noopFn); registry.RegisterSerializer<S11>(noopFn);
    registry.RegisterSerializer<S12>(noopFn); registry.RegisterSerializer<S13>(noopFn);
    registry.RegisterSerializer<S14>(noopFn); registry.RegisterSerializer<S15>(noopFn);
    registry.RegisterSerializer<S16>(noopFn); registry.RegisterSerializer<S17>(noopFn);
    registry.RegisterSerializer<S18>(noopFn); registry.RegisterSerializer<S19>(noopFn);
    registry.RegisterSerializer<S20>(noopFn); registry.RegisterSerializer<S21>(noopFn);
    registry.RegisterSerializer<S22>(noopFn); registry.RegisterSerializer<S23>(noopFn);
    registry.RegisterSerializer<S24>(noopFn); registry.RegisterSerializer<S25>(noopFn);
    registry.RegisterSerializer<S26>(noopFn); registry.RegisterSerializer<S27>(noopFn);
    registry.RegisterSerializer<S28>(noopFn); registry.RegisterSerializer<S29>(noopFn);
    registry.RegisterSerializer<S30>(noopFn); registry.RegisterSerializer<S31>(noopFn);

    // 33rd serializer must assert
    EXPECT_DEATH(registry.RegisterSerializer<S32>(noopFn), "");
}

#endif // _DEBUG

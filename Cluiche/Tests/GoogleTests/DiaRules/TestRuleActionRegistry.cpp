#include <gtest/gtest.h>

#include <DiaRules/RuleActionRegistry.h>

#include <cstdio>

// ---------------------------------------------------------------------------
// DiaRules_ActionRegistry
//
// Covers RuleActionRegistry construction, registration, Has lookups, and
// Find dispatch.  Each test owns its own registry; no shared mutable state.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Empty registry
// ---------------------------------------------------------------------------

TEST(DiaRules_ActionRegistry, DefaultConstruct_Has_ReturnsFalse)
{
    Dia::Rules::RuleActionRegistry registry;

    EXPECT_FALSE(registry.Has(Dia::Core::StringCRC("fire")));
}

TEST(DiaRules_ActionRegistry, DefaultConstruct_Find_ReturnsNullptr)
{
    Dia::Rules::RuleActionRegistry registry;

    EXPECT_EQ(registry.Find(Dia::Core::StringCRC("fire")), nullptr);
}

// ---------------------------------------------------------------------------
// Register + Has
// ---------------------------------------------------------------------------

TEST(DiaRules_ActionRegistry, Register_Has_ReturnsTrue)
{
    Dia::Rules::RuleActionRegistry registry;

    auto handler = [](void*) {};
    registry.Register(Dia::Core::StringCRC("jump"), handler);

    EXPECT_TRUE(registry.Has(Dia::Core::StringCRC("jump")));
}

TEST(DiaRules_ActionRegistry, Register_DifferentKey_Has_ReturnsFalse)
{
    Dia::Rules::RuleActionRegistry registry;

    auto handler = [](void*) {};
    registry.Register(Dia::Core::StringCRC("jump"), handler);

    EXPECT_FALSE(registry.Has(Dia::Core::StringCRC("fire")));
}

// ---------------------------------------------------------------------------
// Register + Find
// ---------------------------------------------------------------------------

TEST(DiaRules_ActionRegistry, Register_Find_ReturnsRegisteredHandler)
{
    Dia::Rules::RuleActionRegistry registry;

    auto handler = [](void*) {};
    registry.Register(Dia::Core::StringCRC("attack"), handler);

    EXPECT_EQ(registry.Find(Dia::Core::StringCRC("attack")), handler);
}

TEST(DiaRules_ActionRegistry, Register_Find_UnknownKey_ReturnsNullptr)
{
    Dia::Rules::RuleActionRegistry registry;

    auto handler = [](void*) {};
    registry.Register(Dia::Core::StringCRC("attack"), handler);

    EXPECT_EQ(registry.Find(Dia::Core::StringCRC("dodge")), nullptr);
}

// ---------------------------------------------------------------------------
// Duplicate Register: last write wins
// ---------------------------------------------------------------------------

TEST(DiaRules_ActionRegistry, Register_Duplicate_OverwritesHandler)
{
    Dia::Rules::RuleActionRegistry registry;

    bool firstCalled  = false;
    bool secondCalled = false;

    auto first  = [](void* ctx) { *static_cast<bool*>(ctx) = true; };
    auto second = [](void* ctx) { *static_cast<bool*>(ctx) = true; };

    registry.Register(Dia::Core::StringCRC("run"), first);
    registry.Register(Dia::Core::StringCRC("run"), second);

    // The retrieved handler must be the second one (last wins).
    EXPECT_EQ(registry.Find(Dia::Core::StringCRC("run")), second);
    EXPECT_NE(registry.Find(Dia::Core::StringCRC("run")), first);
}

TEST(DiaRules_ActionRegistry, Register_Duplicate_HasStillReturnsTrue)
{
    Dia::Rules::RuleActionRegistry registry;

    auto first  = [](void*) {};
    auto second = [](void*) {};

    registry.Register(Dia::Core::StringCRC("run"), first);
    registry.Register(Dia::Core::StringCRC("run"), second);

    EXPECT_TRUE(registry.Has(Dia::Core::StringCRC("run")));
}

// ---------------------------------------------------------------------------
// Two distinct actions: both findable
// ---------------------------------------------------------------------------

TEST(DiaRules_ActionRegistry, RegisterTwo_BothFindable)
{
    Dia::Rules::RuleActionRegistry registry;

    auto handlerA = [](void*) {};
    auto handlerB = [](void*) {};

    registry.Register(Dia::Core::StringCRC("actionA"), handlerA);
    registry.Register(Dia::Core::StringCRC("actionB"), handlerB);

    EXPECT_EQ(registry.Find(Dia::Core::StringCRC("actionA")), handlerA);
    EXPECT_EQ(registry.Find(Dia::Core::StringCRC("actionB")), handlerB);
    EXPECT_TRUE(registry.Has(Dia::Core::StringCRC("actionA")));
    EXPECT_TRUE(registry.Has(Dia::Core::StringCRC("actionB")));
}

TEST(DiaRules_ActionRegistry, RegisterTwo_NoInterference)
{
    Dia::Rules::RuleActionRegistry registry;

    auto handlerA = [](void*) {};
    auto handlerB = [](void*) {};

    registry.Register(Dia::Core::StringCRC("alpha"), handlerA);
    registry.Register(Dia::Core::StringCRC("beta"),  handlerB);

    // alpha does not resolve to beta and vice versa
    EXPECT_NE(registry.Find(Dia::Core::StringCRC("alpha")), handlerB);
    EXPECT_NE(registry.Find(Dia::Core::StringCRC("beta")),  handlerA);
}

// ---------------------------------------------------------------------------
// Null handler: caller may clear a slot
// ---------------------------------------------------------------------------

TEST(DiaRules_ActionRegistry, RegisterNullHandler_Has_ReturnsTrue)
{
    Dia::Rules::RuleActionRegistry registry;

    registry.Register(Dia::Core::StringCRC("clear_me"), nullptr);

    EXPECT_TRUE(registry.Has(Dia::Core::StringCRC("clear_me")));
}

TEST(DiaRules_ActionRegistry, RegisterNullHandler_Find_ReturnsNullptr)
{
    Dia::Rules::RuleActionRegistry registry;

    registry.Register(Dia::Core::StringCRC("clear_me"), nullptr);

    EXPECT_EQ(registry.Find(Dia::Core::StringCRC("clear_me")), nullptr);
}

TEST(DiaRules_ActionRegistry, RegisterNullHandler_OverwritesNonNull)
{
    Dia::Rules::RuleActionRegistry registry;

    auto handler = [](void*) {};
    registry.Register(Dia::Core::StringCRC("slot"), handler);
    registry.Register(Dia::Core::StringCRC("slot"), nullptr);

    EXPECT_EQ(registry.Find(Dia::Core::StringCRC("slot")), nullptr);
    EXPECT_TRUE(registry.Has(Dia::Core::StringCRC("slot")));
}

// ---------------------------------------------------------------------------
// Unknown key
// ---------------------------------------------------------------------------

TEST(DiaRules_ActionRegistry, UnknownKey_Find_ReturnsNullptr)
{
    Dia::Rules::RuleActionRegistry registry;

    EXPECT_EQ(registry.Find(Dia::Core::StringCRC("no_such_action")), nullptr);
}

TEST(DiaRules_ActionRegistry, UnknownKey_Has_ReturnsFalse)
{
    Dia::Rules::RuleActionRegistry registry;

    EXPECT_FALSE(registry.Has(Dia::Core::StringCRC("no_such_action")));
}

// ---------------------------------------------------------------------------
// Two independent registries: same key does not cross-contaminate
// ---------------------------------------------------------------------------

TEST(DiaRules_ActionRegistry, TwoRegistries_SameKey_DoNotInterfere)
{
    Dia::Rules::RuleActionRegistry regA;
    Dia::Rules::RuleActionRegistry regB;

    auto handlerA = [](void*) {};
    auto handlerB = [](void*) {};

    regA.Register(Dia::Core::StringCRC("act"), handlerA);
    regB.Register(Dia::Core::StringCRC("act"), handlerB);

    EXPECT_EQ(regA.Find(Dia::Core::StringCRC("act")), handlerA);
    EXPECT_EQ(regB.Find(Dia::Core::StringCRC("act")), handlerB);
}

TEST(DiaRules_ActionRegistry, TwoRegistries_RegisterInOne_OtherHasNothing)
{
    Dia::Rules::RuleActionRegistry regA;
    Dia::Rules::RuleActionRegistry regB;

    auto handler = [](void*) {};
    regA.Register(Dia::Core::StringCRC("act"), handler);

    EXPECT_TRUE (regA.Has(Dia::Core::StringCRC("act")));
    EXPECT_FALSE(regB.Has(Dia::Core::StringCRC("act")));
}

// ---------------------------------------------------------------------------
// Register null, then re-register with a real handler
// ---------------------------------------------------------------------------

TEST(DiaRules_ActionRegistry, RegisterNullThenReRegister_FindReturnsNewHandler)
{
    Dia::Rules::RuleActionRegistry registry;

    auto handler1 = [](void*) {};
    auto handler2 = [](void*) {};

    registry.Register(Dia::Core::StringCRC("slot"), handler1);
    registry.Register(Dia::Core::StringCRC("slot"), nullptr);
    registry.Register(Dia::Core::StringCRC("slot"), handler2);

    EXPECT_TRUE(registry.Has(Dia::Core::StringCRC("slot")));
    EXPECT_EQ(registry.Find(Dia::Core::StringCRC("slot")), handler2);
}

// ---------------------------------------------------------------------------
// Large number of entries
// ---------------------------------------------------------------------------

TEST(DiaRules_ActionRegistry, LargeNumberOfEntries_AllFindable)
{
    Dia::Rules::RuleActionRegistry registry;

    // Register 50 distinct keys using sprintf-style names "action_00" .. "action_49"
    static const int kCount = 50;
    auto handler = [](void*) {};

    char buf[16];
    for (int i = 0; i < kCount; ++i)
    {
        std::snprintf(buf, sizeof(buf), "action_%02d", i);
        registry.Register(Dia::Core::StringCRC(buf), handler);
    }

    for (int i = 0; i < kCount; ++i)
    {
        std::snprintf(buf, sizeof(buf), "action_%02d", i);
        Dia::Core::StringCRC key(buf);
        EXPECT_TRUE(registry.Has(key))  << "Has failed for " << buf;
        EXPECT_EQ(registry.Find(key), handler) << "Find failed for " << buf;
    }
}

// ---------------------------------------------------------------------------
// Has vs Find with null handler: slot exists but is not callable
// ---------------------------------------------------------------------------

TEST(DiaRules_ActionRegistry, HasVsFindNullHandlerContract_SlotExistsButNotCallable)
{
    Dia::Rules::RuleActionRegistry registry;

    registry.Register(Dia::Core::StringCRC("empty_slot"), nullptr);

    // Has reports the slot exists
    EXPECT_TRUE(registry.Has(Dia::Core::StringCRC("empty_slot")));

    // Find returns nullptr — handler is not callable
    EXPECT_EQ(registry.Find(Dia::Core::StringCRC("empty_slot")), nullptr);
}

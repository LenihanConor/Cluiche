#include <gtest/gtest.h>

#include <DiaCondition/ConditionRegistry.h>

// ---------------------------------------------------------------------------
// DiaCondition_Registry
//
// Covers ConditionRegistry construction, registration, Has* lookups, and
// Get* accessor dispatch.  Each test owns its own registry + data; no shared
// mutable state.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Empty registry
// ---------------------------------------------------------------------------

TEST(DiaCondition_Registry, EmptyRegistry_HasFloat_ReturnsFalse)
{
    int data = 0;
    Dia::Condition::ConditionRegistry registry(&data);

    EXPECT_FALSE(registry.HasFloat(Dia::Core::StringCRC("slot"), Dia::Core::StringCRC("field")));
}

TEST(DiaCondition_Registry, EmptyRegistry_HasBool_ReturnsFalse)
{
    int data = 0;
    Dia::Condition::ConditionRegistry registry(&data);

    EXPECT_FALSE(registry.HasBool(Dia::Core::StringCRC("slot"), Dia::Core::StringCRC("field")));
}

// ---------------------------------------------------------------------------
// RegisterFloat — HasFloat
// ---------------------------------------------------------------------------

TEST(DiaCondition_Registry, RegisterFloat_HasFloat_ReturnsTrue)
{
    float value = 42.0f;
    Dia::Condition::ConditionRegistry registry(&value);

    registry.RegisterFloat(
        Dia::Core::StringCRC("entity"),
        Dia::Core::StringCRC("health"),
        [](void* d) { return *static_cast<float*>(d); });

    EXPECT_TRUE(registry.HasFloat(Dia::Core::StringCRC("entity"), Dia::Core::StringCRC("health")));
}

TEST(DiaCondition_Registry, RegisterFloat_DifferentSlotField_HasBool_ReturnsFalse)
{
    float value = 1.0f;
    Dia::Condition::ConditionRegistry registry(&value);

    registry.RegisterFloat(
        Dia::Core::StringCRC("entity"),
        Dia::Core::StringCRC("health"),
        [](void* d) { return *static_cast<float*>(d); });

    // HasBool for the same key should still be false
    EXPECT_FALSE(registry.HasBool(Dia::Core::StringCRC("entity"), Dia::Core::StringCRC("health")));
}

// ---------------------------------------------------------------------------
// RegisterBool — HasBool
// ---------------------------------------------------------------------------

TEST(DiaCondition_Registry, RegisterBool_HasBool_ReturnsTrue)
{
    bool value = true;
    Dia::Condition::ConditionRegistry registry(&value);

    registry.RegisterBool(
        Dia::Core::StringCRC("entity"),
        Dia::Core::StringCRC("visible"),
        [](void* d) { return *static_cast<bool*>(d); });

    EXPECT_TRUE(registry.HasBool(Dia::Core::StringCRC("entity"), Dia::Core::StringCRC("visible")));
}

TEST(DiaCondition_Registry, RegisterBool_DifferentSlotField_HasFloat_ReturnsFalse)
{
    bool value = false;
    Dia::Condition::ConditionRegistry registry(&value);

    registry.RegisterBool(
        Dia::Core::StringCRC("entity"),
        Dia::Core::StringCRC("visible"),
        [](void* d) { return *static_cast<bool*>(d); });

    EXPECT_FALSE(registry.HasFloat(Dia::Core::StringCRC("entity"), Dia::Core::StringCRC("visible")));
}

// ---------------------------------------------------------------------------
// GetFloat — calls accessor with the correct data pointer
// ---------------------------------------------------------------------------

TEST(DiaCondition_Registry, GetFloat_CallsAccessorWithCorrectDataPointer)
{
    float value = 75.5f;
    Dia::Condition::ConditionRegistry registry(&value);

    registry.RegisterFloat(
        Dia::Core::StringCRC("player"),
        Dia::Core::StringCRC("speed"),
        [](void* d) { return *static_cast<float*>(d); });

    EXPECT_FLOAT_EQ(registry.GetFloat(Dia::Core::StringCRC("player"), Dia::Core::StringCRC("speed")), 75.5f);
}

TEST(DiaCondition_Registry, GetFloat_ReflectsUpdatedDataValue)
{
    float value = 10.0f;
    Dia::Condition::ConditionRegistry registry(&value);

    registry.RegisterFloat(
        Dia::Core::StringCRC("player"),
        Dia::Core::StringCRC("ammo"),
        [](void* d) { return *static_cast<float*>(d); });

    EXPECT_FLOAT_EQ(registry.GetFloat(Dia::Core::StringCRC("player"), Dia::Core::StringCRC("ammo")), 10.0f);

    // Mutate the data — the accessor must observe the new value.
    value = 99.0f;
    EXPECT_FLOAT_EQ(registry.GetFloat(Dia::Core::StringCRC("player"), Dia::Core::StringCRC("ammo")), 99.0f);
}

// ---------------------------------------------------------------------------
// GetBool — calls accessor with the correct data pointer
// ---------------------------------------------------------------------------

TEST(DiaCondition_Registry, GetBool_CallsAccessorWithCorrectDataPointer_True)
{
    bool value = true;
    Dia::Condition::ConditionRegistry registry(&value);

    registry.RegisterBool(
        Dia::Core::StringCRC("enemy"),
        Dia::Core::StringCRC("alive"),
        [](void* d) { return *static_cast<bool*>(d); });

    EXPECT_TRUE(registry.GetBool(Dia::Core::StringCRC("enemy"), Dia::Core::StringCRC("alive")));
}

TEST(DiaCondition_Registry, GetBool_CallsAccessorWithCorrectDataPointer_False)
{
    bool value = false;
    Dia::Condition::ConditionRegistry registry(&value);

    registry.RegisterBool(
        Dia::Core::StringCRC("enemy"),
        Dia::Core::StringCRC("alive"),
        [](void* d) { return *static_cast<bool*>(d); });

    EXPECT_FALSE(registry.GetBool(Dia::Core::StringCRC("enemy"), Dia::Core::StringCRC("alive")));
}

TEST(DiaCondition_Registry, GetBool_ReflectsUpdatedDataValue)
{
    bool value = false;
    Dia::Condition::ConditionRegistry registry(&value);

    registry.RegisterBool(
        Dia::Core::StringCRC("enemy"),
        Dia::Core::StringCRC("is_alert"),
        [](void* d) { return *static_cast<bool*>(d); });

    EXPECT_FALSE(registry.GetBool(Dia::Core::StringCRC("enemy"), Dia::Core::StringCRC("is_alert")));

    value = true;
    EXPECT_TRUE(registry.GetBool(Dia::Core::StringCRC("enemy"), Dia::Core::StringCRC("is_alert")));
}

// ---------------------------------------------------------------------------
// Multiple float registrations — different slot/field combos return distinct values
// ---------------------------------------------------------------------------

struct MultiFloatData
{
    float health  = 80.0f;
    float speed   = 5.0f;
    float stamina = 50.0f;
};

TEST(DiaCondition_Registry, MultipleFloats_EachReturnsCorrectValue)
{
    MultiFloatData data;
    Dia::Condition::ConditionRegistry registry(&data);

    registry.RegisterFloat(
        Dia::Core::StringCRC("player"), Dia::Core::StringCRC("health"),
        [](void* d) { return static_cast<MultiFloatData*>(d)->health; });
    registry.RegisterFloat(
        Dia::Core::StringCRC("player"), Dia::Core::StringCRC("speed"),
        [](void* d) { return static_cast<MultiFloatData*>(d)->speed; });
    registry.RegisterFloat(
        Dia::Core::StringCRC("player"), Dia::Core::StringCRC("stamina"),
        [](void* d) { return static_cast<MultiFloatData*>(d)->stamina; });

    EXPECT_FLOAT_EQ(registry.GetFloat(Dia::Core::StringCRC("player"), Dia::Core::StringCRC("health")),  80.0f);
    EXPECT_FLOAT_EQ(registry.GetFloat(Dia::Core::StringCRC("player"), Dia::Core::StringCRC("speed")),    5.0f);
    EXPECT_FLOAT_EQ(registry.GetFloat(Dia::Core::StringCRC("player"), Dia::Core::StringCRC("stamina")), 50.0f);
}

TEST(DiaCondition_Registry, MultipleFloats_DifferentSlots_EachReturnsCorrectValue)
{
    float playerHealth = 100.0f;
    float enemyHealth  = 30.0f;

    Dia::Condition::ConditionRegistry reg1(&playerHealth);
    Dia::Condition::ConditionRegistry reg2(&enemyHealth);

    reg1.RegisterFloat(
        Dia::Core::StringCRC("player"), Dia::Core::StringCRC("health"),
        [](void* d) { return *static_cast<float*>(d); });
    reg2.RegisterFloat(
        Dia::Core::StringCRC("enemy"), Dia::Core::StringCRC("health"),
        [](void* d) { return *static_cast<float*>(d); });

    EXPECT_FLOAT_EQ(reg1.GetFloat(Dia::Core::StringCRC("player"), Dia::Core::StringCRC("health")), 100.0f);
    EXPECT_FLOAT_EQ(reg2.GetFloat(Dia::Core::StringCRC("enemy"),  Dia::Core::StringCRC("health")),  30.0f);
}

// ---------------------------------------------------------------------------
// Two independent registries with the same slot/field keys don't interfere
// ---------------------------------------------------------------------------

TEST(DiaCondition_Registry, TwoRegistries_SameKeys_DoNotInterfere_Float)
{
    float dataA = 11.0f;
    float dataB = 22.0f;

    Dia::Condition::ConditionRegistry regA(&dataA);
    Dia::Condition::ConditionRegistry regB(&dataB);

    auto accessor = [](void* d) { return *static_cast<float*>(d); };
    regA.RegisterFloat(Dia::Core::StringCRC("slot"), Dia::Core::StringCRC("field"), accessor);
    regB.RegisterFloat(Dia::Core::StringCRC("slot"), Dia::Core::StringCRC("field"), accessor);

    EXPECT_FLOAT_EQ(regA.GetFloat(Dia::Core::StringCRC("slot"), Dia::Core::StringCRC("field")), 11.0f);
    EXPECT_FLOAT_EQ(regB.GetFloat(Dia::Core::StringCRC("slot"), Dia::Core::StringCRC("field")), 22.0f);
}

TEST(DiaCondition_Registry, TwoRegistries_SameKeys_DoNotInterfere_Bool)
{
    bool dataA = true;
    bool dataB = false;

    Dia::Condition::ConditionRegistry regA(&dataA);
    Dia::Condition::ConditionRegistry regB(&dataB);

    auto accessor = [](void* d) { return *static_cast<bool*>(d); };
    regA.RegisterBool(Dia::Core::StringCRC("slot"), Dia::Core::StringCRC("flag"), accessor);
    regB.RegisterBool(Dia::Core::StringCRC("slot"), Dia::Core::StringCRC("flag"), accessor);

    EXPECT_TRUE (regA.GetBool(Dia::Core::StringCRC("slot"), Dia::Core::StringCRC("flag")));
    EXPECT_FALSE(regB.GetBool(Dia::Core::StringCRC("slot"), Dia::Core::StringCRC("flag")));
}

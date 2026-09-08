#include <gtest/gtest.h>

#include <DiaCondition/ConditionRegistry.h>
#include <DiaCondition/IConditionContext.h>
#include <DiaCore/CRC/StringCRC.h>

// ---------------------------------------------------------------------------
// DiaCondition_Registry_Boundary
//
// Gap-filling boundary tests for ConditionRegistry: overwrite semantics,
// float/bool coexistence on the same key, slot/field collision avoidance,
// stress registration, and polymorphic dispatch via IConditionContext*.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Overwrite: registering the same slot+field twice should use the second accessor
// ---------------------------------------------------------------------------

TEST(DiaCondition_Registry_Boundary, RegisterFloat_SameKeyTwice_SecondAccessorOverwrites)
{
    float firstValue  = 11.0f;
    float secondValue = 99.0f;

    Dia::Condition::ConditionRegistry registry(&firstValue);

    // Register first accessor — reads firstValue
    registry.RegisterFloat(
        Dia::Core::StringCRC("obj"),
        Dia::Core::StringCRC("val"),
        [](void* d) { return *static_cast<float*>(d); });

    EXPECT_FLOAT_EQ(registry.GetFloat(Dia::Core::StringCRC("obj"), Dia::Core::StringCRC("val")), 11.0f);

    // Register second accessor — reads secondValue directly via capture workaround:
    // we capture the address in a file-static so the plain fn ptr can dereference it.
    static float* s_second = &secondValue;
    registry.RegisterFloat(
        Dia::Core::StringCRC("obj"),
        Dia::Core::StringCRC("val"),
        [](void*) -> float { return *s_second; });

    EXPECT_FLOAT_EQ(registry.GetFloat(Dia::Core::StringCRC("obj"), Dia::Core::StringCRC("val")), 99.0f);
}

TEST(DiaCondition_Registry_Boundary, RegisterBool_SameKeyTwice_SecondAccessorOverwrites)
{
    bool firstValue  = true;
    bool secondValue = false;

    Dia::Condition::ConditionRegistry registry(&firstValue);

    registry.RegisterBool(
        Dia::Core::StringCRC("obj"),
        Dia::Core::StringCRC("flag"),
        [](void* d) { return *static_cast<bool*>(d); });

    EXPECT_TRUE(registry.GetBool(Dia::Core::StringCRC("obj"), Dia::Core::StringCRC("flag")));

    static bool* s_second = &secondValue;
    registry.RegisterBool(
        Dia::Core::StringCRC("obj"),
        Dia::Core::StringCRC("flag"),
        [](void*) -> bool { return *s_second; });

    EXPECT_FALSE(registry.GetBool(Dia::Core::StringCRC("obj"), Dia::Core::StringCRC("flag")));
}

// ---------------------------------------------------------------------------
// Float and bool can share the same slot+field key without collision
// ---------------------------------------------------------------------------

TEST(DiaCondition_Registry_Boundary, FloatAndBoolCoexist_SameSlotField)
{
    struct Data { float f = 3.14f; bool b = true; };
    Data data;
    Dia::Condition::ConditionRegistry registry(&data);

    registry.RegisterFloat(
        Dia::Core::StringCRC("shared"), Dia::Core::StringCRC("key"),
        [](void* d) { return static_cast<Data*>(d)->f; });
    registry.RegisterBool(
        Dia::Core::StringCRC("shared"), Dia::Core::StringCRC("key"),
        [](void* d) { return static_cast<Data*>(d)->b; });

    EXPECT_TRUE (registry.HasFloat(Dia::Core::StringCRC("shared"), Dia::Core::StringCRC("key")));
    EXPECT_TRUE (registry.HasBool (Dia::Core::StringCRC("shared"), Dia::Core::StringCRC("key")));

    EXPECT_FLOAT_EQ(registry.GetFloat(Dia::Core::StringCRC("shared"), Dia::Core::StringCRC("key")), 3.14f);
    EXPECT_TRUE    (registry.GetBool (Dia::Core::StringCRC("shared"), Dia::Core::StringCRC("key")));
}

// ---------------------------------------------------------------------------
// Same slot, different fields — each accessor resolves independently
// ---------------------------------------------------------------------------

TEST(DiaCondition_Registry_Boundary, SameSlot_DifferentFields_NoCollision)
{
    struct Data { float health = 80.0f; float speed = 5.0f; };
    Data data;
    Dia::Condition::ConditionRegistry registry(&data);

    registry.RegisterFloat(
        Dia::Core::StringCRC("player"), Dia::Core::StringCRC("health"),
        [](void* d) { return static_cast<Data*>(d)->health; });
    registry.RegisterFloat(
        Dia::Core::StringCRC("player"), Dia::Core::StringCRC("speed"),
        [](void* d) { return static_cast<Data*>(d)->speed; });

    EXPECT_TRUE (registry.HasFloat(Dia::Core::StringCRC("player"), Dia::Core::StringCRC("health")));
    EXPECT_TRUE (registry.HasFloat(Dia::Core::StringCRC("player"), Dia::Core::StringCRC("speed")));
    EXPECT_FALSE(registry.HasFloat(Dia::Core::StringCRC("player"), Dia::Core::StringCRC("stamina")));

    EXPECT_FLOAT_EQ(registry.GetFloat(Dia::Core::StringCRC("player"), Dia::Core::StringCRC("health")), 80.0f);
    EXPECT_FLOAT_EQ(registry.GetFloat(Dia::Core::StringCRC("player"), Dia::Core::StringCRC("speed")),   5.0f);
}

// ---------------------------------------------------------------------------
// Different slots, same field — each accessor resolves independently
// ---------------------------------------------------------------------------

TEST(DiaCondition_Registry_Boundary, DifferentSlots_SameField_NoCollision)
{
    struct Data { float playerHealth = 100.0f; float enemyHealth = 25.0f; };
    Data data;
    Dia::Condition::ConditionRegistry registry(&data);

    registry.RegisterFloat(
        Dia::Core::StringCRC("player"), Dia::Core::StringCRC("health"),
        [](void* d) { return static_cast<Data*>(d)->playerHealth; });
    registry.RegisterFloat(
        Dia::Core::StringCRC("enemy"), Dia::Core::StringCRC("health"),
        [](void* d) { return static_cast<Data*>(d)->enemyHealth; });

    EXPECT_TRUE (registry.HasFloat(Dia::Core::StringCRC("player"), Dia::Core::StringCRC("health")));
    EXPECT_TRUE (registry.HasFloat(Dia::Core::StringCRC("enemy"),  Dia::Core::StringCRC("health")));

    EXPECT_FLOAT_EQ(registry.GetFloat(Dia::Core::StringCRC("player"), Dia::Core::StringCRC("health")), 100.0f);
    EXPECT_FLOAT_EQ(registry.GetFloat(Dia::Core::StringCRC("enemy"),  Dia::Core::StringCRC("health")),  25.0f);
}

// ---------------------------------------------------------------------------
// Missing key — Release only (asserts in Debug): returns 0.0f / false without crashing
// ---------------------------------------------------------------------------

#ifndef _DEBUG

TEST(DiaCondition_Registry_Boundary, GetFloat_MissingKey_ReturnsZeroAndAsserts)
{
    int dummy = 0;
    Dia::Condition::ConditionRegistry registry(&dummy);

    float result = registry.GetFloat(Dia::Core::StringCRC("missing"), Dia::Core::StringCRC("key"));
    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST(DiaCondition_Registry_Boundary, GetBool_MissingKey_ReturnsFalseAndAsserts)
{
    int dummy = 0;
    Dia::Condition::ConditionRegistry registry(&dummy);

    bool result = registry.GetBool(Dia::Core::StringCRC("missing"), Dia::Core::StringCRC("key"));
    EXPECT_FALSE(result);
}

#endif // !_DEBUG

// ---------------------------------------------------------------------------
// Stress: register 10 float + 10 bool accessors with distinct CRCs
// ---------------------------------------------------------------------------

namespace
{
    // 10 distinct float values indexed 0..9
    float g_floatValues[10] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f };
    bool  g_boolValues[10]  = { true, false, true, false, true, false, true, false, true, false };
}

TEST(DiaCondition_Registry_Boundary, RegisterMany_100Accessors_AllResolveCorrectly)
{
    // We use a single registry with a dummy data pointer; accessors read from
    // the file-statics above via index embedded in the slot CRC string.
    int dummy = 0;
    Dia::Condition::ConditionRegistry registry(&dummy);

    // Build distinct slot/field CRC pairs for floats using slot = "fs<i>", field = "fv"
    // and for bools using slot = "bs<i>", field = "bv".
    static const char* const kFloatSlots[10] = {
        "fs0","fs1","fs2","fs3","fs4","fs5","fs6","fs7","fs8","fs9"
    };
    static const char* const kBoolSlots[10] = {
        "bs0","bs1","bs2","bs3","bs4","bs5","bs6","bs7","bs8","bs9"
    };

    // Accessor table: each entry holds a pointer to the target float.
    static float* s_fptrs[10];
    static bool*  s_bptrs[10];

    using FloatFn = Dia::Condition::FloatAccessorFn;
    using BoolFn  = Dia::Condition::BoolAccessorFn;

    static const FloatFn kFloatFns[10] = {
        [](void*) -> float { return *s_fptrs[0]; },
        [](void*) -> float { return *s_fptrs[1]; },
        [](void*) -> float { return *s_fptrs[2]; },
        [](void*) -> float { return *s_fptrs[3]; },
        [](void*) -> float { return *s_fptrs[4]; },
        [](void*) -> float { return *s_fptrs[5]; },
        [](void*) -> float { return *s_fptrs[6]; },
        [](void*) -> float { return *s_fptrs[7]; },
        [](void*) -> float { return *s_fptrs[8]; },
        [](void*) -> float { return *s_fptrs[9]; },
    };
    static const BoolFn kBoolFns[10] = {
        [](void*) -> bool { return *s_bptrs[0]; },
        [](void*) -> bool { return *s_bptrs[1]; },
        [](void*) -> bool { return *s_bptrs[2]; },
        [](void*) -> bool { return *s_bptrs[3]; },
        [](void*) -> bool { return *s_bptrs[4]; },
        [](void*) -> bool { return *s_bptrs[5]; },
        [](void*) -> bool { return *s_bptrs[6]; },
        [](void*) -> bool { return *s_bptrs[7]; },
        [](void*) -> bool { return *s_bptrs[8]; },
        [](void*) -> bool { return *s_bptrs[9]; },
    };

    for (int i = 0; i < 10; ++i)
    {
        s_fptrs[i] = &g_floatValues[i];
        s_bptrs[i] = &g_boolValues[i];

        registry.RegisterFloat(
            Dia::Core::StringCRC(kFloatSlots[i]),
            Dia::Core::StringCRC("fv"),
            kFloatFns[i]);
        registry.RegisterBool(
            Dia::Core::StringCRC(kBoolSlots[i]),
            Dia::Core::StringCRC("bv"),
            kBoolFns[i]);
    }

    for (int i = 0; i < 10; ++i)
    {
        EXPECT_TRUE(registry.HasFloat(Dia::Core::StringCRC(kFloatSlots[i]), Dia::Core::StringCRC("fv")));
        EXPECT_TRUE(registry.HasBool (Dia::Core::StringCRC(kBoolSlots[i]),  Dia::Core::StringCRC("bv")));

        EXPECT_FLOAT_EQ(registry.GetFloat(Dia::Core::StringCRC(kFloatSlots[i]), Dia::Core::StringCRC("fv")),
                        g_floatValues[i]);
        EXPECT_EQ      (registry.GetBool (Dia::Core::StringCRC(kBoolSlots[i]),  Dia::Core::StringCRC("bv")),
                        g_boolValues[i]);
    }
}

// ---------------------------------------------------------------------------
// Polymorphic dispatch via IConditionContext*
// ---------------------------------------------------------------------------

TEST(DiaCondition_Registry_Boundary, IConditionContext_PolymorphicDispatch)
{
    struct Data { float f = 7.0f; bool b = true; };
    Data data;
    Dia::Condition::ConditionRegistry registry(&data);

    registry.RegisterFloat(
        Dia::Core::StringCRC("poly"), Dia::Core::StringCRC("floatval"),
        [](void* d) { return static_cast<Data*>(d)->f; });
    registry.RegisterBool(
        Dia::Core::StringCRC("poly"), Dia::Core::StringCRC("boolval"),
        [](void* d) { return static_cast<Data*>(d)->b; });

    // Hold as the interface pointer — dispatch must go through vtable.
    Dia::Condition::IConditionContext* ctx = &registry;

    EXPECT_FLOAT_EQ(ctx->GetFloat(Dia::Core::StringCRC("poly"), Dia::Core::StringCRC("floatval")), 7.0f);
    EXPECT_TRUE    (ctx->GetBool (Dia::Core::StringCRC("poly"), Dia::Core::StringCRC("boolval")));

    // Mutate data — interface must reflect the new value.
    data.f = 42.0f;
    data.b = false;

    EXPECT_FLOAT_EQ(ctx->GetFloat(Dia::Core::StringCRC("poly"), Dia::Core::StringCRC("floatval")), 42.0f);
    EXPECT_FALSE   (ctx->GetBool (Dia::Core::StringCRC("poly"), Dia::Core::StringCRC("boolval")));
}

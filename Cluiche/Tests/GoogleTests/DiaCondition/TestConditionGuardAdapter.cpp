#include <gtest/gtest.h>

#include <DiaCondition/ConditionGuardAdapter.h>
#include <DiaCondition/ConditionExpr.h>
#include <DiaCondition/Testing/ConditionTestHelpers.h>
#include <DiaStateMachine/CallbackRegistry.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

// ---------------------------------------------------------------------------
// DiaCondition_GuardAdapter
//
// Tests for RegisterAsGuard.
//
// IMPORTANT: ConditionGuardAdapter uses a file-static slot table (max 16).
// Each test registers guards with unique names and each test's expr/ctx are
// kept alive on the stack for the duration of the test.  Guard slots are
// consumed across the entire test binary lifetime, so unique guard names are
// used per test to avoid slot index aliasing.
// ---------------------------------------------------------------------------

namespace
{
    // Helper: load a valid condition expression from a JSON string.
    Dia::Condition::ConditionExpr MakeFloatExpr(const char* json)
    {
        Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
        Json::Value root;
        Json::Reader reader;
        reader.parse(json, root);
        return Dia::Condition::ConditionExpr::LoadFromJson(root, errors);
    }
}

// ---------------------------------------------------------------------------
// RegisterAsGuard registers the guard name in CallbackRegistry
// ---------------------------------------------------------------------------

TEST(DiaCondition_GuardAdapter, RegisterAsGuard_HasGuard_ReturnsTrue)
{
    Dia::Condition::ConditionExpr expr = MakeFloatExpr(
        R"({"op":">=","slot":"hero","field":"hp","value":10})");
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("hp"), 50.0f);

    Dia::StateMachine::CallbackRegistry registry;

    Dia::Condition::RegisterAsGuard(
        Dia::Core::StringCRC("guard_has_guard_A"),
        expr, ctx, registry);

    EXPECT_TRUE(registry.HasGuard(Dia::Core::StringCRC("guard_has_guard_A")));
}

// ---------------------------------------------------------------------------
// Guard evaluates to true when condition is met
// ---------------------------------------------------------------------------

TEST(DiaCondition_GuardAdapter, RegisterAsGuard_ConditionTrue_GuardReturnsTrue)
{
    Dia::Condition::ConditionExpr expr = MakeFloatExpr(
        R"({"op":">=","slot":"hero","field":"hp","value":50})");
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("hp"), 80.0f);  // >= 50 → true

    Dia::StateMachine::CallbackRegistry registry;

    Dia::Condition::RegisterAsGuard(
        Dia::Core::StringCRC("guard_true_B"),
        expr, ctx, registry);

    auto guardFn = registry.FindGuard(Dia::Core::StringCRC("guard_true_B"));
    ASSERT_NE(guardFn, nullptr);
    EXPECT_TRUE(guardFn(nullptr));
}

// ---------------------------------------------------------------------------
// Guard evaluates to false when condition is not met
// ---------------------------------------------------------------------------

TEST(DiaCondition_GuardAdapter, RegisterAsGuard_ConditionFalse_GuardReturnsFalse)
{
    Dia::Condition::ConditionExpr expr = MakeFloatExpr(
        R"({"op":">=","slot":"hero","field":"hp","value":50})");
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("hp"), 20.0f);  // < 50 → false

    Dia::StateMachine::CallbackRegistry registry;

    Dia::Condition::RegisterAsGuard(
        Dia::Core::StringCRC("guard_false_C"),
        expr, ctx, registry);

    auto guardFn = registry.FindGuard(Dia::Core::StringCRC("guard_false_C"));
    ASSERT_NE(guardFn, nullptr);
    EXPECT_FALSE(guardFn(nullptr));
}

// ---------------------------------------------------------------------------
// Two guards with different names behave independently
// ---------------------------------------------------------------------------

TEST(DiaCondition_GuardAdapter, TwoGuards_IndependentResults)
{
    Dia::Condition::ConditionExpr exprTrue = MakeFloatExpr(
        R"({"op":">=","slot":"hero","field":"stamina","value":10})");
    Dia::Condition::ConditionExpr exprFalse = MakeFloatExpr(
        R"({"op":">=","slot":"enemy","field":"hp","value":100})");
    ASSERT_TRUE(exprTrue.IsValid());
    ASSERT_TRUE(exprFalse.IsValid());

    Dia::Condition::Testing::MockConditionContext ctxTrue;
    ctxTrue.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("stamina"), 50.0f); // 50 >= 10 → true

    Dia::Condition::Testing::MockConditionContext ctxFalse;
    ctxFalse.SetFloat(Dia::Core::StringCRC("enemy"), Dia::Core::StringCRC("hp"), 5.0f);   // 5 >= 100 → false

    Dia::StateMachine::CallbackRegistry registry;

    Dia::Condition::RegisterAsGuard(
        Dia::Core::StringCRC("guard_two_true_D"),
        exprTrue, ctxTrue, registry);

    Dia::Condition::RegisterAsGuard(
        Dia::Core::StringCRC("guard_two_false_E"),
        exprFalse, ctxFalse, registry);

    auto guardTrue  = registry.FindGuard(Dia::Core::StringCRC("guard_two_true_D"));
    auto guardFalse = registry.FindGuard(Dia::Core::StringCRC("guard_two_false_E"));

    ASSERT_NE(guardTrue,  nullptr);
    ASSERT_NE(guardFalse, nullptr);

    EXPECT_TRUE (guardTrue(nullptr));
    EXPECT_FALSE(guardFalse(nullptr));
}

// ---------------------------------------------------------------------------
// Unregistered guard name returns nullptr from FindGuard
// ---------------------------------------------------------------------------

TEST(DiaCondition_GuardAdapter, UnregisteredName_FindGuard_ReturnsNullptr)
{
    Dia::StateMachine::CallbackRegistry registry;
    EXPECT_EQ(registry.FindGuard(Dia::Core::StringCRC("guard_nonexistent_F")), nullptr);
}

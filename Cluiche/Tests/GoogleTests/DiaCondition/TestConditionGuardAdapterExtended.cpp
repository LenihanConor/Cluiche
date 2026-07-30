#include <gtest/gtest.h>

#include <DiaCondition/ConditionGuardAdapter.h>
#include <DiaCondition/ConditionExpr.h>
#include <DiaCondition/Testing/ConditionTestHelpers.h>
#include <DiaStateMachine/CallbackRegistry.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/CRC/StringCRC.h>

// ---------------------------------------------------------------------------
// DiaCondition_GuardAdapter_Extended
//
// Extended tests for RegisterAsGuard.
//
// IMPORTANT: ConditionGuardAdapter uses a file-static slot table (max 16).
// The existing 5 tests in TestConditionGuardAdapter.cpp consume guard slots
// named:  guard_has_guard_A, guard_true_B, guard_false_C,
//         guard_two_true_D, guard_two_false_E, guard_nonexistent_F
// (F does not consume a slot — it is never registered.)
// This file uses: ext_guard_live_G, ext_guard_iso_H, ext_guard_iso_I,
//                 ext_guard_composite_J, ext_guard_bool_K
// Total slots consumed: 10 of 16.
// ---------------------------------------------------------------------------

namespace
{
    Dia::Condition::ConditionExpr MakeExprFromJson(const char* json)
    {
        Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
        Json::Value root;
        Json::Reader reader;
        reader.parse(json, root);
        return Dia::Condition::ConditionExpr::LoadFromJson(root, errors);
    }
}

// ---------------------------------------------------------------------------
// Live-update: the guard holds a reference to ctx, not a copy.
// Changing ctx after registration is reflected in subsequent guard evaluations.
// ---------------------------------------------------------------------------

TEST(DiaCondition_GuardAdapter_Extended, RegisterAsGuard_ContextLiveUpdate_GuardReflectsNewValue)
{
    Dia::Condition::ConditionExpr expr = MakeExprFromJson(
        R"({"op":">=","slot":"hero","field":"hp","value":50})");
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("hp"), 80.0f);  // >= 50 → true

    Dia::StateMachine::CallbackRegistry registry;

    Dia::Condition::RegisterAsGuard(
        Dia::Core::StringCRC("ext_guard_live_G"),
        expr, ctx, registry);

    auto guardFn = registry.FindGuard(Dia::Core::StringCRC("ext_guard_live_G"));
    ASSERT_NE(guardFn, nullptr);

    // Guard is true while hp is 80.
    EXPECT_TRUE(guardFn(nullptr));

    // Update the context value AFTER registration.
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("hp"), 20.0f);  // < 50 → false

    // The guard must now reflect the new context value (pointer semantics).
    EXPECT_FALSE(guardFn(nullptr));
}

// ---------------------------------------------------------------------------
// Guard is registered in one CallbackRegistry — a different registry does not
// see it.
// ---------------------------------------------------------------------------

TEST(DiaCondition_GuardAdapter_Extended, RegisterAsGuard_GuardIsolatedToOneRegistry)
{
    Dia::Condition::ConditionExpr expr = MakeExprFromJson(
        R"({"op":">=","slot":"hero","field":"hp","value":10})");
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("hp"), 50.0f);

    Dia::StateMachine::CallbackRegistry registryA;
    Dia::StateMachine::CallbackRegistry registryB;

    Dia::Condition::RegisterAsGuard(
        Dia::Core::StringCRC("ext_guard_iso_H"),
        expr, ctx, registryA);

    // registryA has the guard; registryB must not.
    EXPECT_TRUE (registryA.HasGuard(Dia::Core::StringCRC("ext_guard_iso_H")));
    EXPECT_FALSE(registryB.HasGuard(Dia::Core::StringCRC("ext_guard_iso_H")));
}

// ---------------------------------------------------------------------------
// Composite expression guard: AND(health>=50, enemy_visible==true)
// ---------------------------------------------------------------------------

TEST(DiaCondition_GuardAdapter_Extended, RegisterAsGuard_CompositeExpr_GuardEvaluatesCorrectly)
{
    Dia::Condition::ConditionExpr expr = MakeExprFromJson(R"(
    {
        "op": "and",
        "conditions": [
            {"op":">=","slot":"hero","field":"health","value":50},
            {"op":"==","slot":"enemy","field":"visible","value":true}
        ]
    })");
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"),  Dia::Core::StringCRC("health"),  60.0f);
    ctx.SetBool (Dia::Core::StringCRC("enemy"), Dia::Core::StringCRC("visible"), true);

    Dia::StateMachine::CallbackRegistry registry;

    Dia::Condition::RegisterAsGuard(
        Dia::Core::StringCRC("ext_guard_composite_J"),
        expr, ctx, registry);

    auto guardFn = registry.FindGuard(Dia::Core::StringCRC("ext_guard_composite_J"));
    ASSERT_NE(guardFn, nullptr);

    // Both conditions met → true.
    EXPECT_TRUE(guardFn(nullptr));

    // Drop health below threshold → false.
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 20.0f);
    EXPECT_FALSE(guardFn(nullptr));

    // Restore health, hide enemy → false.
    ctx.SetFloat(Dia::Core::StringCRC("hero"),  Dia::Core::StringCRC("health"),  60.0f);
    ctx.SetBool (Dia::Core::StringCRC("enemy"), Dia::Core::StringCRC("visible"), false);
    EXPECT_FALSE(guardFn(nullptr));

    // Both restored → true.
    ctx.SetBool(Dia::Core::StringCRC("enemy"), Dia::Core::StringCRC("visible"), true);
    EXPECT_TRUE(guardFn(nullptr));
}

// ---------------------------------------------------------------------------
// Bool condition guard: toggle context value between calls
// ---------------------------------------------------------------------------

TEST(DiaCondition_GuardAdapter_Extended, RegisterAsGuard_BoolCondition_TrueAndFalseTransitions)
{
    Dia::Condition::ConditionExpr expr = MakeExprFromJson(
        R"({"op":"==","slot":"hero","field":"is_ready","value":true})");
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetBool(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("is_ready"), false);

    Dia::StateMachine::CallbackRegistry registry;

    Dia::Condition::RegisterAsGuard(
        Dia::Core::StringCRC("ext_guard_bool_K"),
        expr, ctx, registry);

    auto guardFn = registry.FindGuard(Dia::Core::StringCRC("ext_guard_bool_K"));
    ASSERT_NE(guardFn, nullptr);

    // Initially false.
    EXPECT_FALSE(guardFn(nullptr));

    // Toggle to true.
    ctx.SetBool(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("is_ready"), true);
    EXPECT_TRUE(guardFn(nullptr));

    // Toggle back to false.
    ctx.SetBool(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("is_ready"), false);
    EXPECT_FALSE(guardFn(nullptr));
}

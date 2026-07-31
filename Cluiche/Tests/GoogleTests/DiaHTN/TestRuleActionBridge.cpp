#include <gtest/gtest.h>
#include <DiaHTN/RuleActionBridge.h>
#include <DiaHTN/OperatorRegistry.h>
#include <DiaHTN/TaskResult.h>
#include <DiaRules/RuleActionRegistry.h>
#include <DiaCore/CRC/StringCRC.h>

// DiaHTN_RuleActionBridge
// Covers RegisterRuleActionAsOperator: registration, return value, context forwarding, params ignored.

TEST(DiaHTN_RuleActionBridge, Register_MakesOperatorHasable)
{
    Dia::HTN::OperatorRegistry opReg;

    bool actionCalled = false;
    auto action = [](void* ctx) { *static_cast<bool*>(ctx) = true; };
    Dia::Rules::RuleActionFn actionFn = action;

    Dia::HTN::RegisterRuleActionAsOperator(
        Dia::Core::StringCRC("fire"), actionFn, opReg);

    EXPECT_TRUE(opReg.Has(Dia::Core::StringCRC("fire")));
}

TEST(DiaHTN_RuleActionBridge, Register_FindReturnsNonNull)
{
    Dia::HTN::OperatorRegistry opReg;
    auto action = [](void*) {};
    Dia::HTN::RegisterRuleActionAsOperator(
        Dia::Core::StringCRC("shoot"), static_cast<Dia::Rules::RuleActionFn>(action), opReg);

    EXPECT_NE(opReg.Find(Dia::Core::StringCRC("shoot")), nullptr);
}

TEST(DiaHTN_RuleActionBridge, WrappedFn_AlwaysReturnsSucceeded)
{
    Dia::HTN::OperatorRegistry opReg;
    // Action that "fails" by setting a flag — return value should still be kSucceeded.
    auto action = [](void*) {};
    Dia::HTN::RegisterRuleActionAsOperator(
        Dia::Core::StringCRC("wrappedFnOp"), static_cast<Dia::Rules::RuleActionFn>(action), opReg);

    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8> params;
    const auto result = opReg.Find(Dia::Core::StringCRC("wrappedFnOp"))(nullptr, params);
    EXPECT_EQ(result, Dia::HTN::TaskResult::kSucceeded);
}

TEST(DiaHTN_RuleActionBridge, OperatorContext_ForwardedToRuleAction)
{
    Dia::HTN::OperatorRegistry opReg;
    bool called = false;
    auto action = [](void* ctx) { *static_cast<bool*>(ctx) = true; };

    Dia::HTN::RegisterRuleActionAsOperator(
        Dia::Core::StringCRC("notify"), static_cast<Dia::Rules::RuleActionFn>(action), opReg);

    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8> params;
    opReg.Find(Dia::Core::StringCRC("notify"))(&called, params);
    EXPECT_TRUE(called);
}

TEST(DiaHTN_RuleActionBridge, Params_AreIgnored_NocrashWithNonEmptyParams)
{
    Dia::HTN::OperatorRegistry opReg;
    auto action = [](void*) {};
    Dia::HTN::RegisterRuleActionAsOperator(
        Dia::Core::StringCRC("patrol"), static_cast<Dia::Rules::RuleActionFn>(action), opReg);

    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8> params;
    params.Add(Dia::Core::StringCRC("waypoint_a"));
    params.Add(Dia::Core::StringCRC("waypoint_b"));

    // Should not crash and should return kSucceeded
    const auto result = opReg.Find(Dia::Core::StringCRC("patrol"))(nullptr, params);
    EXPECT_EQ(result, Dia::HTN::TaskResult::kSucceeded);
}

TEST(DiaHTN_RuleActionBridge, MultipleRegistrations_EachGetOwnSlot)
{
    Dia::HTN::OperatorRegistry opReg;

    int counterA = 0, counterB = 0;
    auto actionA = [](void* ctx) { ++(*static_cast<int*>(ctx)); };
    auto actionB = [](void* ctx) { ++(*(static_cast<int*>(ctx) + 1)); };

    Dia::HTN::RegisterRuleActionAsOperator(
        Dia::Core::StringCRC("opA"), static_cast<Dia::Rules::RuleActionFn>(actionA), opReg);
    Dia::HTN::RegisterRuleActionAsOperator(
        Dia::Core::StringCRC("opB"), static_cast<Dia::Rules::RuleActionFn>(actionB), opReg);

    int counters[2] = {0, 0};
    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8> params;
    opReg.Find(Dia::Core::StringCRC("opA"))(counters, params);
    opReg.Find(Dia::Core::StringCRC("opB"))(counters, params);

    EXPECT_EQ(counters[0], 1);
    EXPECT_EQ(counters[1], 1);
}

TEST(DiaHTN_RuleActionBridge, ReRegisterSameId_UpdatesAction)
{
    // Use the same registry for both registrations.
    // The first call installs the trampoline; the second call updates the bridge entry.
    // Calling the trampoline after the second registration should dispatch to the new action.
    Dia::HTN::OperatorRegistry opReg;

    auto first  = [](void* ctx) { *static_cast<bool*>(ctx)        = true; };
    auto second = [](void* ctx) { *(static_cast<bool*>(ctx) + 1)  = true; };

    Dia::HTN::RegisterRuleActionAsOperator(
        Dia::Core::StringCRC("reRegisterOp"), static_cast<Dia::Rules::RuleActionFn>(first), opReg);
    // Second call: re-registration updates the bridge entry for the same id.
    Dia::HTN::RegisterRuleActionAsOperator(
        Dia::Core::StringCRC("reRegisterOp"), static_cast<Dia::Rules::RuleActionFn>(second), opReg);

    bool flags[2] = {false, false};
    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8> params;
    // Trampoline still found via the first registration — it points to the updated bridge entry.
    opReg.Find(Dia::Core::StringCRC("reRegisterOp"))(flags, params);

    // Second action should fire (flags[1]), first should not (flags[0]).
    EXPECT_FALSE(flags[0]);
    EXPECT_TRUE(flags[1]);
}

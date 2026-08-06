#include <gtest/gtest.h>

#include <DiaTriggerScript/Testing/TriggerScriptTestHelpers.h>
#include <DiaTriggerScript/TriggerActionRegistry.h>
#include <DiaCondition/Testing/ConditionTestHelpers.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

// ---------------------------------------------------------------------------
// DiaTriggerScript_TestHelpers — MockActionHandler
// ---------------------------------------------------------------------------

TEST(DiaTriggerScript_TestHelpers, MockActionHandler_DefaultConstruct_NoCalls)
{
    Dia::TriggerScript::Testing::MockActionHandler handler;
    EXPECT_EQ(handler.GetCalls().Size(), 0u);
}

TEST(DiaTriggerScript_TestHelpers, MockActionHandler_Execute_RecordsCall)
{
    Dia::TriggerScript::Testing::MockActionHandler handler;

    Json::Value params(Json::objectValue);
    Dia::TriggerScript::ActionContext ctx{ Dia::Core::StringCRC("trig-A"), params };
    handler.Execute(ctx);

    EXPECT_EQ(handler.GetCalls().Size(), 1u);
    EXPECT_EQ(handler.GetCalls()[0].triggerId, Dia::Core::StringCRC("trig-A"));
}

TEST(DiaTriggerScript_TestHelpers, MockActionHandler_Execute_MultipleCalls_AllRecorded)
{
    Dia::TriggerScript::Testing::MockActionHandler handler;

    Json::Value params(Json::objectValue);
    Dia::TriggerScript::ActionContext ctxA{ Dia::Core::StringCRC("A"), params };
    Dia::TriggerScript::ActionContext ctxB{ Dia::Core::StringCRC("B"), params };
    Dia::TriggerScript::ActionContext ctxC{ Dia::Core::StringCRC("C"), params };

    handler.Execute(ctxA);
    handler.Execute(ctxB);
    handler.Execute(ctxC);

    EXPECT_EQ(handler.GetCalls().Size(), 3u);
    EXPECT_EQ(handler.GetCalls()[0].triggerId, Dia::Core::StringCRC("A"));
    EXPECT_EQ(handler.GetCalls()[1].triggerId, Dia::Core::StringCRC("B"));
    EXPECT_EQ(handler.GetCalls()[2].triggerId, Dia::Core::StringCRC("C"));
}

TEST(DiaTriggerScript_TestHelpers, MockActionHandler_Clear_ResetsCallList)
{
    Dia::TriggerScript::Testing::MockActionHandler handler;

    Json::Value params(Json::objectValue);
    Dia::TriggerScript::ActionContext ctx{ Dia::Core::StringCRC("t"), params };
    handler.Execute(ctx);
    handler.Execute(ctx);

    handler.Clear();

    EXPECT_EQ(handler.GetCalls().Size(), 0u);
}

// ---------------------------------------------------------------------------
// DiaTriggerScript_TestHelpers — MakeStateTrigger
// ---------------------------------------------------------------------------

TEST(DiaTriggerScript_TestHelpers, MakeStateTrigger_HasCorrectId)
{
    Dia::Condition::Testing::MockConditionContext ctx;
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    auto expr = Dia::Condition::ConditionExpr::LoadFromJson(
        []{ Json::Value v; v["op"] = "=="; v["slot"] = "x"; v["field"] = "y"; v["value"] = true; return v; }(),
        errors);

    auto def = Dia::TriggerScript::Testing::MakeStateTrigger(
        Dia::Core::StringCRC("my-trigger"),
        std::move(expr),
        Dia::Core::StringCRC("FireEvent"));

    EXPECT_EQ(def.id,   Dia::Core::StringCRC("my-trigger"));
    EXPECT_EQ(def.type, Dia::TriggerScript::TriggerType::kState);
    EXPECT_TRUE(def.oneShot);
}

TEST(DiaTriggerScript_TestHelpers, MakeStateTrigger_Repeating_OneShotFalse)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    auto expr = Dia::Condition::ConditionExpr::LoadFromJson(
        []{ Json::Value v; v["op"] = "=="; v["slot"] = "x"; v["field"] = "y"; v["value"] = true; return v; }(),
        errors);

    auto def = Dia::TriggerScript::Testing::MakeStateTrigger(
        Dia::Core::StringCRC("rep"),
        std::move(expr),
        Dia::Core::StringCRC("FireEvent"),
        /*oneShot=*/false);

    EXPECT_FALSE(def.oneShot);
}

TEST(DiaTriggerScript_TestHelpers, MakeStateTrigger_HasOneAction)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    auto expr = Dia::Condition::ConditionExpr::LoadFromJson(
        []{ Json::Value v; v["op"] = "=="; v["slot"] = "x"; v["field"] = "y"; v["value"] = true; return v; }(),
        errors);

    auto def = Dia::TriggerScript::Testing::MakeStateTrigger(
        Dia::Core::StringCRC("t"),
        std::move(expr),
        Dia::Core::StringCRC("MyAction"));

    ASSERT_EQ(def.actions.Size(), 1u);
    EXPECT_EQ(def.actions[0].actionType, Dia::Core::StringCRC("MyAction"));
}

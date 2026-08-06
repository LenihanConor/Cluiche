#include <gtest/gtest.h>

#include <DiaTriggerScript/TriggerScriptModule.h>
#include <DiaTriggerScript/TriggerActionRegistry.h>
#include <DiaTriggerScript/Testing/TriggerScriptTestHelpers.h>
#include <DiaCondition/ConditionRegistry.h>
#include <DiaCondition/Testing/ConditionTestHelpers.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{
    Json::Value ParseJson(const char* src)
    {
        Json::Value root;
        Json::Reader reader;
        reader.parse(src, root);
        return root;
    }

    // Build an empty ConditionRegistry (no slots — only used for validation in tests
    // that don't exercise state triggers).
    Dia::Condition::ConditionRegistry MakeEmptyRegistry()
    {
        return Dia::Condition::ConditionRegistry(nullptr);
    }
}

// ---------------------------------------------------------------------------
// DiaTriggerScript_Module — LoadFromJson
// ---------------------------------------------------------------------------

TEST(DiaTriggerScript_Module, LoadFromJson_ValidTemporalTrigger_GetCountCorrect)
{
    Dia::TriggerScript::TriggerScriptModule module;
    Dia::Condition::ConditionRegistry validationReg = MakeEmptyRegistry();
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;

    Json::Value root = ParseJson(R"({
        "triggers": [
            {
                "id": "wave-timer",
                "type": "temporal",
                "one_shot": false,
                "interval_s": 30.0,
                "actions": []
            }
        ]
    })");

    bool ok = module.LoadFromJson(root, validationReg, errors);

    EXPECT_TRUE(ok);
    EXPECT_EQ(module.GetTriggerCount(), 1);
    EXPECT_EQ(errors.Size(), 0u);
}

TEST(DiaTriggerScript_Module, LoadFromJson_MissingTriggersKey_ReturnsFalse)
{
    Dia::TriggerScript::TriggerScriptModule module;
    Dia::Condition::ConditionRegistry validationReg = MakeEmptyRegistry();
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;

    Json::Value root = ParseJson(R"({})");
    bool ok = module.LoadFromJson(root, validationReg, errors);

    EXPECT_FALSE(ok);
    EXPECT_GT(errors.Size(), 0u);
}

TEST(DiaTriggerScript_Module, LoadFromJson_MultipleTriggers_AllLoaded)
{
    Dia::TriggerScript::TriggerScriptModule module;
    Dia::Condition::ConditionRegistry validationReg = MakeEmptyRegistry();
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;

    Json::Value root = ParseJson(R"({
        "triggers": [
            { "id": "t1", "type": "temporal", "interval_s": 5.0,  "actions": [] },
            { "id": "t2", "type": "temporal", "interval_s": 10.0, "actions": [] },
            { "id": "t3", "type": "count",    "entity_tag": "enemy", "threshold": 5, "actions": [] }
        ]
    })");

    bool ok = module.LoadFromJson(root, validationReg, errors);

    EXPECT_TRUE(ok);
    EXPECT_EQ(module.GetTriggerCount(), 3);
}

// ---------------------------------------------------------------------------
// DiaTriggerScript_Module — temporal trigger
// ---------------------------------------------------------------------------

TEST(DiaTriggerScript_Module, TemporalTrigger_NotFiredBeforeInterval)
{
    Dia::TriggerScript::TriggerScriptModule module;
    Dia::TriggerScript::TriggerActionRegistry registry;
    Dia::TriggerScript::Testing::MockActionHandler handler;
    registry.Register(Dia::Core::StringCRC("FireEvent"), &handler);

    Dia::Condition::ConditionRegistry validationReg = MakeEmptyRegistry();
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;

    Json::Value root = ParseJson(R"({
        "triggers": [{
            "id": "wave-timer",
            "type": "temporal",
            "one_shot": true,
            "interval_s": 5.0,
            "actions": [{ "type": "FireEvent", "params": {} }]
        }]
    })");

    module.LoadFromJson(root, validationReg, errors);
    module.SetActionRegistry(&registry);

    module.Tick(2.0f);  // 2s — below 5s threshold

    EXPECT_EQ(handler.GetCalls().Size(), 0u);
    EXPECT_TRUE(module.IsActive(Dia::Core::StringCRC("wave-timer")));
}

TEST(DiaTriggerScript_Module, TemporalTrigger_OneShot_FiresOnceAtInterval)
{
    Dia::TriggerScript::TriggerScriptModule module;
    Dia::TriggerScript::TriggerActionRegistry registry;
    Dia::TriggerScript::Testing::MockActionHandler handler;
    registry.Register(Dia::Core::StringCRC("FireEvent"), &handler);

    Dia::Condition::ConditionRegistry validationReg = MakeEmptyRegistry();
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;

    Json::Value root = ParseJson(R"({
        "triggers": [{
            "id": "wave-timer",
            "type": "temporal",
            "one_shot": true,
            "interval_s": 5.0,
            "actions": [{ "type": "FireEvent", "params": {} }]
        }]
    })");

    module.LoadFromJson(root, validationReg, errors);
    module.SetActionRegistry(&registry);

    module.Tick(5.0f);  // exactly at threshold

    EXPECT_EQ(handler.GetCalls().Size(), 1u);
    EXPECT_TRUE(module.IsFired(Dia::Core::StringCRC("wave-timer")));
}

TEST(DiaTriggerScript_Module, TemporalTrigger_OneShot_DoesNotFireAgain)
{
    Dia::TriggerScript::TriggerScriptModule module;
    Dia::TriggerScript::TriggerActionRegistry registry;
    Dia::TriggerScript::Testing::MockActionHandler handler;
    registry.Register(Dia::Core::StringCRC("FireEvent"), &handler);

    Dia::Condition::ConditionRegistry validationReg = MakeEmptyRegistry();
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;

    Json::Value root = ParseJson(R"({
        "triggers": [{
            "id": "once",
            "type": "temporal",
            "one_shot": true,
            "interval_s": 1.0,
            "actions": [{ "type": "FireEvent", "params": {} }]
        }]
    })");

    module.LoadFromJson(root, validationReg, errors);
    module.SetActionRegistry(&registry);

    module.Tick(1.0f);
    module.Tick(1.0f);
    module.Tick(1.0f);

    EXPECT_EQ(handler.GetCalls().Size(), 1u);
}

TEST(DiaTriggerScript_Module, TemporalTrigger_Repeating_FiresMultipleTimes)
{
    Dia::TriggerScript::TriggerScriptModule module;
    Dia::TriggerScript::TriggerActionRegistry registry;
    Dia::TriggerScript::Testing::MockActionHandler handler;
    registry.Register(Dia::Core::StringCRC("FireEvent"), &handler);

    Dia::Condition::ConditionRegistry validationReg = MakeEmptyRegistry();
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;

    Json::Value root = ParseJson(R"({
        "triggers": [{
            "id": "repeating",
            "type": "temporal",
            "one_shot": false,
            "interval_s": 1.0,
            "actions": [{ "type": "FireEvent", "params": {} }]
        }]
    })");

    module.LoadFromJson(root, validationReg, errors);
    module.SetActionRegistry(&registry);

    module.Tick(1.0f);
    module.Tick(1.0f);
    module.Tick(1.0f);

    EXPECT_EQ(handler.GetCalls().Size(), 3u);
    EXPECT_TRUE(module.IsActive(Dia::Core::StringCRC("repeating")));
}

// ---------------------------------------------------------------------------
// DiaTriggerScript_Module — state trigger
// ---------------------------------------------------------------------------

TEST(DiaTriggerScript_Module, StateTrigger_ConditionFalse_DoesNotFire)
{
    Dia::TriggerScript::TriggerScriptModule module;
    Dia::TriggerScript::TriggerActionRegistry registry;
    Dia::TriggerScript::Testing::MockActionHandler handler;
    registry.Register(Dia::Core::StringCRC("FireEvent"), &handler);

    // Condition: player.health < 20  (health = 50 -> false)
    float health = 50.0f;
    Dia::Condition::ConditionRegistry condReg(&health);
    condReg.RegisterFloat(
        Dia::Core::StringCRC("player"), Dia::Core::StringCRC("health"),
        [](void* d) { return *static_cast<float*>(d); });

    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Json::Value root = ParseJson(R"({
        "triggers": [{
            "id": "low-health",
            "type": "state",
            "one_shot": true,
            "check_interval_ms": 0,
            "condition": { "op": "<", "slot": "player", "field": "health", "value": 20.0 },
            "actions": [{ "type": "FireEvent", "params": {} }]
        }]
    })");

    module.LoadFromJson(root, condReg, errors);
    module.SetActionRegistry(&registry);
    module.SetConditionContext(&condReg);

    module.Tick(0.016f);

    EXPECT_EQ(handler.GetCalls().Size(), 0u);
}

TEST(DiaTriggerScript_Module, StateTrigger_ConditionTrue_Fires)
{
    Dia::TriggerScript::TriggerScriptModule module;
    Dia::TriggerScript::TriggerActionRegistry registry;
    Dia::TriggerScript::Testing::MockActionHandler handler;
    registry.Register(Dia::Core::StringCRC("FireEvent"), &handler);

    float health = 10.0f;
    Dia::Condition::ConditionRegistry condReg(&health);
    condReg.RegisterFloat(
        Dia::Core::StringCRC("player"), Dia::Core::StringCRC("health"),
        [](void* d) { return *static_cast<float*>(d); });

    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Json::Value root = ParseJson(R"({
        "triggers": [{
            "id": "low-health",
            "type": "state",
            "one_shot": true,
            "check_interval_ms": 0,
            "condition": { "op": "<", "slot": "player", "field": "health", "value": 20.0 },
            "actions": [{ "type": "FireEvent", "params": {} }]
        }]
    })");

    module.LoadFromJson(root, condReg, errors);
    module.SetActionRegistry(&registry);
    module.SetConditionContext(&condReg);

    module.Tick(0.016f);

    EXPECT_EQ(handler.GetCalls().Size(), 1u);
    EXPECT_TRUE(module.IsFired(Dia::Core::StringCRC("low-health")));
}

TEST(DiaTriggerScript_Module, StateTrigger_OneShot_DoesNotFireAgain)
{
    Dia::TriggerScript::TriggerScriptModule module;
    Dia::TriggerScript::TriggerActionRegistry registry;
    Dia::TriggerScript::Testing::MockActionHandler handler;
    registry.Register(Dia::Core::StringCRC("FireEvent"), &handler);

    float health = 10.0f;
    Dia::Condition::ConditionRegistry condReg(&health);
    condReg.RegisterFloat(
        Dia::Core::StringCRC("player"), Dia::Core::StringCRC("health"),
        [](void* d) { return *static_cast<float*>(d); });

    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Json::Value root = ParseJson(R"({
        "triggers": [{
            "id": "once",
            "type": "state",
            "one_shot": true,
            "check_interval_ms": 0,
            "condition": { "op": "<", "slot": "player", "field": "health", "value": 20.0 },
            "actions": [{ "type": "FireEvent", "params": {} }]
        }]
    })");

    module.LoadFromJson(root, condReg, errors);
    module.SetActionRegistry(&registry);
    module.SetConditionContext(&condReg);

    module.Tick(0.016f);
    module.Tick(0.016f);
    module.Tick(0.016f);

    EXPECT_EQ(handler.GetCalls().Size(), 1u);
}

// ---------------------------------------------------------------------------
// DiaTriggerScript_Module — count trigger
// ---------------------------------------------------------------------------

TEST(DiaTriggerScript_Module, CountTrigger_BelowThreshold_DoesNotFire)
{
    Dia::TriggerScript::TriggerScriptModule module;
    Dia::TriggerScript::TriggerActionRegistry registry;
    Dia::TriggerScript::Testing::MockActionHandler handler;
    registry.Register(Dia::Core::StringCRC("FireEvent"), &handler);

    Dia::Condition::ConditionRegistry validationReg = MakeEmptyRegistry();
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;

    Json::Value root = ParseJson(R"({
        "triggers": [{
            "id": "ten-kills",
            "type": "count",
            "one_shot": true,
            "check_interval_ms": 0,
            "entity_tag": "enemy",
            "threshold": 10,
            "actions": [{ "type": "FireEvent", "params": {} }]
        }]
    })");

    module.LoadFromJson(root, validationReg, errors);
    module.SetActionRegistry(&registry);

    module.IncrementCount(Dia::Core::StringCRC("enemy"), 5);
    module.Tick(0.016f);

    EXPECT_EQ(handler.GetCalls().Size(), 0u);
}

TEST(DiaTriggerScript_Module, CountTrigger_AtThreshold_Fires)
{
    Dia::TriggerScript::TriggerScriptModule module;
    Dia::TriggerScript::TriggerActionRegistry registry;
    Dia::TriggerScript::Testing::MockActionHandler handler;
    registry.Register(Dia::Core::StringCRC("FireEvent"), &handler);

    Dia::Condition::ConditionRegistry validationReg = MakeEmptyRegistry();
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;

    Json::Value root = ParseJson(R"({
        "triggers": [{
            "id": "ten-kills",
            "type": "count",
            "one_shot": true,
            "check_interval_ms": 0,
            "entity_tag": "enemy",
            "threshold": 10,
            "actions": [{ "type": "FireEvent", "params": {} }]
        }]
    })");

    module.LoadFromJson(root, validationReg, errors);
    module.SetActionRegistry(&registry);

    module.IncrementCount(Dia::Core::StringCRC("enemy"), 10);
    module.Tick(0.016f);

    EXPECT_EQ(handler.GetCalls().Size(), 1u);
    EXPECT_TRUE(module.IsFired(Dia::Core::StringCRC("ten-kills")));
}

TEST(DiaTriggerScript_Module, CountTrigger_WrongTag_DoesNotIncrement)
{
    Dia::TriggerScript::TriggerScriptModule module;
    Dia::TriggerScript::TriggerActionRegistry registry;
    Dia::TriggerScript::Testing::MockActionHandler handler;
    registry.Register(Dia::Core::StringCRC("FireEvent"), &handler);

    Dia::Condition::ConditionRegistry validationReg = MakeEmptyRegistry();
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;

    Json::Value root = ParseJson(R"({
        "triggers": [{
            "id": "enemy-kills",
            "type": "count",
            "one_shot": true,
            "check_interval_ms": 0,
            "entity_tag": "enemy",
            "threshold": 3,
            "actions": [{ "type": "FireEvent", "params": {} }]
        }]
    })");

    module.LoadFromJson(root, validationReg, errors);
    module.SetActionRegistry(&registry);

    // Increment with a different tag — should not count
    module.IncrementCount(Dia::Core::StringCRC("ally"), 10);
    module.Tick(0.016f);

    EXPECT_EQ(handler.GetCalls().Size(), 0u);
}

// ---------------------------------------------------------------------------
// DiaTriggerScript_Module — dispatch and test helpers
// ---------------------------------------------------------------------------

TEST(DiaTriggerScript_Module, MockActionHandler_RecordsTriggerId)
{
    Dia::TriggerScript::TriggerScriptModule module;
    Dia::TriggerScript::TriggerActionRegistry registry;
    Dia::TriggerScript::Testing::MockActionHandler handler;
    registry.Register(Dia::Core::StringCRC("FireEvent"), &handler);

    Dia::Condition::ConditionRegistry validationReg = MakeEmptyRegistry();
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;

    Json::Value root = ParseJson(R"({
        "triggers": [{
            "id": "castle-entered",
            "type": "temporal",
            "one_shot": true,
            "interval_s": 0.0,
            "actions": [{ "type": "FireEvent", "params": {} }]
        }]
    })");

    module.LoadFromJson(root, validationReg, errors);
    module.SetActionRegistry(&registry);
    module.Tick(0.016f);

    ASSERT_EQ(handler.GetCalls().Size(), 1u);
    EXPECT_EQ(handler.GetCalls()[0].triggerId, Dia::Core::StringCRC("castle-entered"));
}

TEST(DiaTriggerScript_Module, IsActive_BeforeFire_ReturnsTrue)
{
    Dia::TriggerScript::TriggerScriptModule module;
    Dia::TriggerScript::TriggerActionRegistry registry;
    Dia::TriggerScript::Testing::MockActionHandler handler;
    registry.Register(Dia::Core::StringCRC("FireEvent"), &handler);

    Dia::Condition::ConditionRegistry validationReg = MakeEmptyRegistry();
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;

    Json::Value root = ParseJson(R"({
        "triggers": [{
            "id": "t",
            "type": "temporal",
            "one_shot": true,
            "interval_s": 99.0,
            "actions": []
        }]
    })");

    module.LoadFromJson(root, validationReg, errors);
    module.SetActionRegistry(&registry);

    EXPECT_TRUE (module.IsActive(Dia::Core::StringCRC("t")));
    EXPECT_FALSE(module.IsFired (Dia::Core::StringCRC("t")));
}

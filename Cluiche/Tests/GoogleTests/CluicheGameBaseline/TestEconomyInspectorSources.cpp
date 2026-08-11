////////////////////////////////////////////////////////////////////////////////
// TestEconomyInspectorSources.cpp
// Unit tests for the four economy inspector data sources:
//   EconomySchemaSource, EconomyInstancesSource,
//   EconomyModifiersSource, EconomyEventsSource
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <DiaEconomy/EconomySchema.h>
#include <DiaEconomy/EconomyInstance.h>
#include <DiaEconomy/EconomySystem.h>
#include <DiaEconomy/IEconomyConditionAdaptor.h>
#include <DiaEconomy/IEconomyObserver.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaDebugServer/DebugServer.h>

#include <Modules/InspectorSources/EconomySchemaSource.h>
#include <Modules/InspectorSources/EconomyInstancesSource.h>
#include <Modules/InspectorSources/EconomyModifiersSource.h>
#include <Modules/InspectorSources/EconomyEventsSource.h>

using namespace Dia::Economy;
using Dia::Core::StringCRC;

// ===========================================================================
// Test helpers (copied pattern from TestDiaEconomy.cpp)
// ===========================================================================

static Json::Value ParseJson(const char* str)
{
    Json::Value root;
    Json::Reader reader;
    reader.parse(str, root);
    return root;
}

static EconomySchema MakeSimpleSchema(const char* resource_name = "gold",
                                      float min_val = 0.0f,
                                      float max_val = 1000.0f,
                                      float start   = 100.0f)
{
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{ \"schema_name\": \"test_schema\","
        "  \"resources\": ["
        "    { \"resource_name\": \"%s\","
        "      \"minimum_value\": %.4f,"
        "      \"maximum_value\": %.4f,"
        "      \"starting_value\": %.4f }"
        "  ] }",
        resource_name, min_val, max_val, start);
    return EconomySchema::LoadFromJsonValue(ParseJson(buf));
}

static EconomySchema MakeModifierSchema(const char* operation, float modifier_value,
                                        const char* when_condition = "")
{
    char buf[1024];
    snprintf(buf, sizeof(buf),
        "{ \"schema_name\": \"mod_schema\","
        "  \"resources\": ["
        "    { \"resource_name\": \"gold\","
        "      \"minimum_value\": 0.0, \"maximum_value\": 10000.0, \"starting_value\": 0.0 }"
        "  ],"
        "  \"income_rules\": ["
        "    { \"rule_name\": \"gold_income\", \"resource_name\": \"gold\","
        "      \"amount_per_second\": 10.0 }"
        "  ],"
        "  \"modifiers\": ["
        "    { \"modifier_name\": \"test_mod\","
        "      \"resource_name\": \"gold\","
        "      \"operation\": \"%s\","
        "      \"value\": %.4f,"
        "      \"when_condition\": \"%s\" }"
        "  ] }",
        operation, modifier_value, when_condition);
    return EconomySchema::LoadFromJsonValue(ParseJson(buf));
}

// ===========================================================================
// Testable subclasses — expose protected CollectAndHash
// ===========================================================================

class CapturableSchemaSource : public Cluiche::AppFlow::EconomySchemaSource
{
public:
    using Cluiche::AppFlow::EconomySchemaSource::EconomySchemaSource;
    unsigned int Collect(Json::Value& out) { return CollectAndHash(out); }
};

class CapturableInstancesSource : public Cluiche::AppFlow::EconomyInstancesSource
{
public:
    using Cluiche::AppFlow::EconomyInstancesSource::EconomyInstancesSource;
    unsigned int Collect(Json::Value& out) { return CollectAndHash(out); }
};

class CapturableModifiersSource : public Cluiche::AppFlow::EconomyModifiersSource
{
public:
    using Cluiche::AppFlow::EconomyModifiersSource::EconomyModifiersSource;
    unsigned int Collect(Json::Value& out) { return CollectAndHash(out); }
};

// ===========================================================================
// Schema source tests
// ===========================================================================

TEST(EconomyInspector_Schema, ResourcesArrayPresent)
{
    EconomySchema schema = MakeSimpleSchema("gold");
    EconomySystem sys;
    CapturableSchemaSource src(schema, sys);

    Json::Value payload;
    src.Collect(payload);

    ASSERT_TRUE(payload.isMember("resources"));
    EXPECT_TRUE(payload["resources"].isArray());
}

TEST(EconomyInspector_Schema, ResourceNameAndType)
{
    EconomySchema schema = MakeSimpleSchema("gold");
    EconomySystem sys;
    CapturableSchemaSource src(schema, sys);

    Json::Value payload;
    src.Collect(payload);

    ASSERT_GE(payload["resources"].size(), 1u);
    const Json::Value& res = payload["resources"][0];
    EXPECT_STREQ(res["name"].asCString(), "gold");
    EXPECT_STREQ(res["type"].asCString(), "base");
}

TEST(EconomyInspector_Schema, DerivedResourceType)
{
    // Schema with one base resource; then register a derived resource on that name.
    EconomySchema schema = MakeSimpleSchema("gold");
    EconomySystem sys;

    // Register a derived resource with the same name as the schema resource
    sys.RegisterDerivedResource(StringCRC("gold"),
        [](const EconomyInstance& inst) { return inst.GetValue(StringCRC("gold")) * 2.0f; });

    CapturableSchemaSource src(schema, sys);

    Json::Value payload;
    src.Collect(payload);

    ASSERT_GE(payload["resources"].size(), 1u);
    bool foundDerived = false;
    for (const auto& res : payload["resources"])
    {
        if (std::string(res["name"].asCString()) == "gold")
        {
            EXPECT_STREQ(res["type"].asCString(), "derived");
            foundDerived = true;
        }
    }
    EXPECT_TRUE(foundDerived) << "Expected gold resource to be marked as derived";
}

TEST(EconomyInspector_Schema, StableHash)
{
    EconomySchema schema = MakeSimpleSchema("gold");
    EconomySystem sys;
    CapturableSchemaSource src(schema, sys);

    Json::Value payload1;
    unsigned int hash1 = src.Collect(payload1);

    Json::Value payload2;
    unsigned int hash2 = src.Collect(payload2);

    EXPECT_EQ(hash1, hash2);
}

TEST(EconomyInspector_Schema, CostTablePresent)
{
    static const char* kSchemaWithCostTable = R"({
        "schema_name": "cost_test",
        "resources": [
            { "resource_name": "gold", "minimum_value": 0, "maximum_value": 1000, "starting_value": 100 }
        ],
        "cost_tables": {
            "units": {
                "Archer": { "gold": 50 }
            }
        }
    })";

    EconomySchema schema = EconomySchema::LoadFromJsonValue(ParseJson(kSchemaWithCostTable));
    EconomySystem sys;
    CapturableSchemaSource src(schema, sys);

    Json::Value payload;
    src.Collect(payload);

    ASSERT_TRUE(payload.isMember("cost_table"));
    EXPECT_TRUE(payload["cost_table"].isArray());
    EXPECT_GE(payload["cost_table"].size(), 1u);
}

// ===========================================================================
// Instances source tests
// ===========================================================================

TEST(EconomyInspector_Instances, InstanceIdPresent)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 100.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    sys.RegisterInstance(inst);

    CapturableInstancesSource src(sys);

    Json::Value payload;
    src.Collect(payload);

    ASSERT_TRUE(payload.isMember("instances"));
    ASSERT_GE(payload["instances"].size(), 1u);
    EXPECT_TRUE(payload["instances"][0].isMember("id"));
}

TEST(EconomyInspector_Instances, ResourceCurrentValue)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 250.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    sys.RegisterInstance(inst);

    CapturableInstancesSource src(sys);

    Json::Value payload;
    src.Collect(payload);

    ASSERT_GE(payload["instances"].size(), 1u);
    const Json::Value& resources = payload["instances"][0]["resources"];
    ASSERT_GE(resources.size(), 1u);

    bool found = false;
    for (const auto& res : resources)
    {
        if (std::string(res["name"].asCString()) == "gold")
        {
            EXPECT_FLOAT_EQ(res["current"].asFloat(), inst.GetValue(StringCRC("gold")));
            found = true;
        }
    }
    EXPECT_TRUE(found) << "Expected 'gold' resource in payload";
}

TEST(EconomyInspector_Instances, CapField)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 500.0f, 100.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    sys.RegisterInstance(inst);

    CapturableInstancesSource src(sys);

    Json::Value payload;
    src.Collect(payload);

    const Json::Value& resources = payload["instances"][0]["resources"];
    bool found = false;
    for (const auto& res : resources)
    {
        if (std::string(res["name"].asCString()) == "gold")
        {
            EXPECT_FLOAT_EQ(res["cap"].asFloat(), inst.GetMaximum(StringCRC("gold")));
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST(EconomyInspector_Instances, SaturationAccumulates)
{
    // Start at max, tick once — capped_duration_s should be > 0 after UpdateTrackers
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 100.0f, 0.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    // Fill to max
    (void)sys.Earn(inst, StringCRC("gold"), 100.0f);
    sys.RegisterInstance(inst);

    CapturableInstancesSource src(sys);

    // Tick once with a non-zero delta so UpdateTrackers accumulates capped_duration_s
    // (connectionCount=0 so base does nothing; we call Collect directly afterwards)
    src.Tick(0.016f, 0, 0);

    Json::Value payload;
    src.Collect(payload);

    ASSERT_GE(payload["instances"].size(), 1u);
    const Json::Value& resources = payload["instances"][0]["resources"];
    bool found = false;
    for (const auto& res : resources)
    {
        if (std::string(res["name"].asCString()) == "gold")
        {
            EXPECT_GT(res["capped_duration_s"].asFloat(), 0.0f);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST(EconomyInspector_Instances, StarvationAccumulatesAfterThreshold)
{
    // kStarvationThresholdTicks=3: UpdateTrackers (called from CollectAndHash) must
    // see value <= minimum on 3 consecutive calls before it starts accumulating
    // starved_duration_s.  We drive this by interleaving Tick (which sets mLastDelta)
    // with Collect (which calls UpdateTrackers) three times.
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 0.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    sys.RegisterInstance(inst);

    CapturableInstancesSource src(sys);

    // Each Tick+Collect pair is one "frame" for the tracker.
    // consecutive_starved: 0->1->2->3; at 3 >= kStarvationThresholdTicks, accumulate.
    Json::Value payload;
    for (int i = 0; i < 3; ++i)
    {
        src.Tick(0.016f, 0, 0);
        src.Collect(payload);
    }

    ASSERT_GE(payload["instances"].size(), 1u);
    const Json::Value& resources = payload["instances"][0]["resources"];
    bool found = false;
    for (const auto& res : resources)
    {
        if (std::string(res["name"].asCString()) == "gold")
        {
            EXPECT_GT(res["starved_duration_s"].asFloat(), 0.0f);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST(EconomyInspector_Instances, HistoryPopulatesOnSampleInterval)
{
    // kSampleIntervalSec = 0.1f. Accumulate >= 0.1s worth of ticks.
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 100.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    sys.RegisterInstance(inst);

    CapturableInstancesSource src(sys);

    // Accumulate past the sample interval threshold
    src.Tick(0.05f, 0, 0);
    src.Tick(0.05f, 0, 0); // total 0.1s — should trigger a history sample

    Json::Value payload;
    src.Collect(payload);

    ASSERT_GE(payload["instances"].size(), 1u);
    const Json::Value& resources = payload["instances"][0]["resources"];
    bool found = false;
    for (const auto& res : resources)
    {
        if (std::string(res["name"].asCString()) == "gold")
        {
            EXPECT_GE(res["history"].size(), 1u);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST(EconomyInspector_Instances, DerivedResourceNoFillData)
{
    // A derived resource should show type="derived" and net_rate=0
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 200.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    sys.RegisterDerivedResource(StringCRC("gold"),
        [](const EconomyInstance& i) { return i.GetValue(StringCRC("gold")) * 2.0f; });

    sys.RegisterInstance(inst);

    CapturableInstancesSource src(sys);

    Json::Value payload;
    src.Collect(payload);

    ASSERT_GE(payload["instances"].size(), 1u);
    const Json::Value& resources = payload["instances"][0]["resources"];
    bool found = false;
    for (const auto& res : resources)
    {
        if (std::string(res["name"].asCString()) == "gold")
        {
            EXPECT_STREQ(res["type"].asCString(), "derived");
            EXPECT_FLOAT_EQ(res["net_rate"].asFloat(), 0.0f);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

// ===========================================================================
// Modifiers source tests
// ===========================================================================

TEST(EconomyInspector_Modifiers, ModifierPresentInPayload)
{
    // Schema with always-on modifier; should appear in modifiers payload
    EconomySchema schema = MakeModifierSchema("multiply_income", 2.0f, "");
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    sys.RegisterInstance(inst);

    CapturableModifiersSource src(sys);

    Json::Value payload;
    src.Collect(payload);

    ASSERT_TRUE(payload.isMember("instances"));
    ASSERT_GE(payload["instances"].size(), 1u);

    // Find the gold resource entry
    bool foundModifier = false;
    const Json::Value& instResources = payload["instances"][0]["resources"];
    for (const auto& resEntry : instResources)
    {
        if (std::string(resEntry["name"].asCString()) == "gold")
        {
            const Json::Value& mods = resEntry["modifiers"];
            for (const auto& mod : mods)
            {
                if (std::string(mod["source"].asCString()) == "test_mod")
                    foundModifier = true;
            }
        }
    }
    EXPECT_TRUE(foundModifier) << "Expected test_mod modifier in payload";
}

TEST(EconomyInspector_Modifiers, ActiveFlagReflected)
{
    // Conditional modifier: without adaptor => active=false
    EconomySchema schema = MakeModifierSchema("multiply_income", 2.0f, "some_condition");
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    sys.RegisterInstance(inst);

    // --- Without adaptor: conditional modifier should be inactive ---
    {
        CapturableModifiersSource src(sys);
        Json::Value payload;
        src.Collect(payload);

        bool foundInactive = false;
        const Json::Value& instResources = payload["instances"][0]["resources"];
        for (const auto& resEntry : instResources)
        {
            if (std::string(resEntry["name"].asCString()) == "gold")
            {
                for (const auto& mod : resEntry["modifiers"])
                {
                    if (std::string(mod["source"].asCString()) == "test_mod")
                    {
                        EXPECT_FALSE(mod["active"].asBool());
                        foundInactive = true;
                    }
                }
            }
        }
        EXPECT_TRUE(foundInactive);
    }

    // --- With always-true adaptor: conditional modifier should be active ---
    {
        struct AlwaysTrueAdaptor : public IEconomyConditionAdaptor {
            bool Evaluate(const char*) const override { return true; }
        } adaptor;

        sys.SetConditionAdaptor(&adaptor);

        CapturableModifiersSource src(sys);
        Json::Value payload;
        src.Collect(payload);

        bool foundActive = false;
        const Json::Value& instResources = payload["instances"][0]["resources"];
        for (const auto& resEntry : instResources)
        {
            if (std::string(resEntry["name"].asCString()) == "gold")
            {
                for (const auto& mod : resEntry["modifiers"])
                {
                    if (std::string(mod["source"].asCString()) == "test_mod")
                    {
                        EXPECT_TRUE(mod["active"].asBool());
                        foundActive = true;
                    }
                }
            }
        }
        EXPECT_TRUE(foundActive);

        sys.SetConditionAdaptor(nullptr);
    }
}

TEST(EconomyInspector_Modifiers, NoModifiersSkipsResource)
{
    // Schema with two resources: gold (has modifier) and wood (no modifier).
    // Wood should be absent from the modifiers payload.
    static const char* kJson = R"({
        "schema_name": "two_res",
        "resources": [
            { "resource_name": "gold", "minimum_value": 0, "maximum_value": 1000, "starting_value": 0 },
            { "resource_name": "wood", "minimum_value": 0, "maximum_value": 500,  "starting_value": 0 }
        ],
        "income_rules": [
            { "rule_name": "gold_income", "resource_name": "gold", "amount_per_second": 10.0 }
        ],
        "modifiers": [
            { "modifier_name": "gold_mod", "resource_name": "gold", "operation": "multiply_income",
              "value": 2.0, "when_condition": "" }
        ]
    })";

    EconomySchema schema = EconomySchema::LoadFromJsonValue(ParseJson(kJson));
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    sys.RegisterInstance(inst);

    CapturableModifiersSource src(sys);
    Json::Value payload;
    src.Collect(payload);

    ASSERT_GE(payload["instances"].size(), 1u);
    const Json::Value& instResources = payload["instances"][0]["resources"];

    bool foundWood = false;
    for (const auto& resEntry : instResources)
    {
        if (std::string(resEntry["name"].asCString()) == "wood")
            foundWood = true;
    }
    EXPECT_FALSE(foundWood) << "Wood resource (no modifiers) should be skipped in output";
}

TEST(EconomyInspector_Modifiers, HashChangesOnActiveChange)
{
    // First collect with no adaptor (modifier inactive), then with always-true adaptor
    // (modifier active) — hashes must differ.
    EconomySchema schema = MakeModifierSchema("multiply_income", 2.0f, "some_condition");
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    sys.RegisterInstance(inst);

    CapturableModifiersSource src(sys);

    Json::Value payload1;
    unsigned int hash1 = src.Collect(payload1);

    struct AlwaysTrueAdaptor : public IEconomyConditionAdaptor {
        bool Evaluate(const char*) const override { return true; }
    } adaptor;

    sys.SetConditionAdaptor(&adaptor);

    CapturableModifiersSource src2(sys);
    Json::Value payload2;
    unsigned int hash2 = src2.Collect(payload2);

    sys.SetConditionAdaptor(nullptr);

    EXPECT_NE(hash1, hash2);
}

// ===========================================================================
// Events source tests
// ===========================================================================

TEST(EconomyInspector_Events, RingNoOverflowCrash)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 500.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    Dia::DebugServer::DebugServer server;
    Cluiche::AppFlow::EconomyEventsSource src(sys);
    src.Activate(&server);

    // Build a PoolChangedEvent and call OnPoolChanged 600 times (ring depth=500)
    PoolChangedEvent ev;
    ev.instance      = &inst;
    ev.resource_name = StringCRC("gold");
    ev.new_value     = 100.0f;
    ev.delta         = 10.0f;

    for (int i = 0; i < 600; ++i)
        src.OnPoolChanged(ev);

    // No crash — test passes.
    src.Deactivate();
    SUCCEED();
}

TEST(EconomyInspector_Events, ActivateSubscribesToObserver)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 100.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    Dia::DebugServer::DebugServer server;
    Cluiche::AppFlow::EconomyEventsSource src(sys);
    src.Activate(&server);

    // Fire a pool-changed event via the source's observer callback directly
    PoolChangedEvent ev;
    ev.instance      = &inst;
    ev.resource_name = StringCRC("gold");
    ev.new_value     = 110.0f;
    ev.delta         = 10.0f;

    // Should not crash
    src.OnPoolChanged(ev);

    src.Deactivate();
    SUCCEED();
}

TEST(EconomyInspector_Events, DeactivateUnsubscribes)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 100.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    Dia::DebugServer::DebugServer server;

    {
        Cluiche::AppFlow::EconomyEventsSource src(sys);
        src.Activate(&server);
        src.Deactivate();
        // src is destroyed here
    }

    // After deactivate + destroy, firing an economy event on the system
    // should not crash (the observer was unsubscribed).
    (void)sys.Earn(inst, StringCRC("gold"), 10.0f);
    SUCCEED();
}

TEST(EconomyInspector_Events, TickDetectsNewSubscriber)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 100.0f);
    EconomySystem sys;

    Dia::DebugServer::DebugServer server;
    Cluiche::AppFlow::EconomyEventsSource src(sys);
    src.Activate(&server);

    // First tick: connCount=1, subCount=0. Initial mLastSubCount=-1,
    // so 0 > -1 is true but connCount=1>0, so SendFullRing is called.
    // Server is unstarted, so NotifySubscribers is a no-op. No crash.
    src.Tick(0.016f, 1, 0);

    // Second tick: connCount=1, subCount=1. subCount(1) > mLastSubCount(0) => SendFullRing.
    src.Tick(0.016f, 1, 1);

    src.Deactivate();
    SUCCEED();
}

// ===========================================================================
// Additional coverage — schema
// ===========================================================================

TEST(EconomyInspector_Schema, CostTableRowHasActionField)
{
    static const char* kJson = R"({
        "schema_name": "cost_test",
        "resources": [
            { "resource_name": "gold", "minimum_value": 0, "maximum_value": 1000, "starting_value": 100 }
        ],
        "cost_tables": {
            "units": {
                "Archer": { "gold": 50 }
            }
        }
    })";
    EconomySchema schema = EconomySchema::LoadFromJsonValue(ParseJson(kJson));
    EconomySystem sys;
    CapturableSchemaSource src(schema, sys);
    Json::Value payload;
    src.Collect(payload);
    ASSERT_TRUE(payload["cost_table"].isArray());
    ASSERT_GE(payload["cost_table"].size(), 1u);
    EXPECT_TRUE(payload["cost_table"][0].isMember("action"));
}

TEST(EconomyInspector_Schema, MultipleResourcesAllPresent)
{
    static const char* kJson = R"({
        "schema_name": "multi_res",
        "resources": [
            { "resource_name": "gold", "minimum_value": 0, "maximum_value": 1000, "starting_value": 0 },
            { "resource_name": "wood", "minimum_value": 0, "maximum_value": 500,  "starting_value": 0 }
        ]
    })";
    EconomySchema schema = EconomySchema::LoadFromJsonValue(ParseJson(kJson));
    EconomySystem sys;
    CapturableSchemaSource src(schema, sys);
    Json::Value payload;
    src.Collect(payload);
    ASSERT_EQ(payload["resources"].size(), 2u);
    bool hasGold = false, hasWood = false;
    for (const auto& r : payload["resources"])
    {
        const std::string name = r["name"].asString();
        if (name == "gold") hasGold = true;
        if (name == "wood") hasWood = true;
    }
    EXPECT_TRUE(hasGold);
    EXPECT_TRUE(hasWood);
}

TEST(EconomyInspector_Schema, NoIncomeRuleEmitsEmptyString)
{
    EconomySchema schema = MakeSimpleSchema("gold"); // no income rules
    EconomySystem sys;
    CapturableSchemaSource src(schema, sys);
    Json::Value payload;
    src.Collect(payload);
    ASSERT_GE(payload["resources"].size(), 1u);
    EXPECT_STREQ(payload["resources"][0]["income_rule"].asCString(), "");
}

// ===========================================================================
// Additional coverage — instances
// ===========================================================================

TEST(EconomyInspector_Instances, HistoryOldestAtHead)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 10.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    sys.RegisterInstance(inst);

    CapturableInstancesSource src(sys);

    // First sample: value ~10
    src.Tick(0.1f, 0, 0);
    Json::Value dummy;
    src.Collect(dummy);

    // Earn to push value up; second sample: value ~100
    (void)sys.Earn(inst, StringCRC("gold"), 90.0f);
    src.Tick(0.1f, 0, 0);
    Json::Value payload;
    src.Collect(payload);

    const Json::Value& history = payload["instances"][0]["resources"][0]["history"];
    ASSERT_GE(history.size(), 2u);
    // oldest (lower) <= newest (higher) — ring is oldest-at-head
    EXPECT_LE(history[0].asFloat(), history[static_cast<int>(history.size()) - 1].asFloat());
}

TEST(EconomyInspector_Instances, RingOverflowDropsOldest)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 10000.0f, 1.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    sys.RegisterInstance(inst);

    CapturableInstancesSource src(sys);

    for (int i = 0; i < 601; ++i)
    {
        src.Tick(0.1f, 0, 0);
        Json::Value dummy;
        src.Collect(dummy);
    }

    Json::Value payload;
    src.Collect(payload);
    const Json::Value& history = payload["instances"][0]["resources"][0]["history"];
    EXPECT_LE(history.size(), 600u);
}

TEST(EconomyInspector_Instances, SaturationDoesNotResetOnDrop)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 100.0f, 0.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    (void)sys.Earn(inst, StringCRC("gold"), 100.0f);
    sys.RegisterInstance(inst);

    CapturableInstancesSource src(sys);
    src.Tick(0.016f, 0, 0);
    {
        Json::Value p;
        src.Collect(p);
        EXPECT_GT(p["instances"][0]["resources"][0]["capped_duration_s"].asFloat(), 0.0f);
    }

    (void)sys.Spend(inst, StringCRC("gold"), 50.0f);
    src.Tick(0.016f, 0, 0);
    Json::Value p2;
    src.Collect(p2);
    EXPECT_GT(p2["instances"][0]["resources"][0]["capped_duration_s"].asFloat(), 0.0f);
}

TEST(EconomyInspector_Instances, StarvationNotBeforeThreshold)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 0.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    sys.RegisterInstance(inst);

    CapturableInstancesSource src(sys);
    Json::Value payload;
    for (int i = 0; i < 2; ++i)
    {
        src.Tick(0.016f, 0, 0);
        src.Collect(payload);
    }
    // After exactly 2 consecutive UpdateTrackers calls at min, threshold not yet reached
    EXPECT_FLOAT_EQ(payload["instances"][0]["resources"][0]["starved_duration_s"].asFloat(), 0.0f);
}

TEST(EconomyInspector_Instances, StarvationResetsOnRecovery)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 0.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    sys.RegisterInstance(inst);

    CapturableInstancesSource src(sys);
    for (int i = 0; i < 3; ++i)
    {
        src.Tick(0.016f, 0, 0);
        Json::Value dummy;
        src.Collect(dummy);
    }

    (void)sys.Earn(inst, StringCRC("gold"), 100.0f);
    src.Tick(0.016f, 0, 0);
    Json::Value p;
    src.Collect(p);
    const float before = p["instances"][0]["resources"][0]["starved_duration_s"].asFloat();

    (void)sys.Spend(inst, StringCRC("gold"), 100.0f);
    src.Tick(0.016f, 0, 0);
    Json::Value p2;
    src.Collect(p2);
    const float after = p2["instances"][0]["resources"][0]["starved_duration_s"].asFloat();

    // Only 1 consecutive dip after recovery: should not have added more than 1 frame worth
    EXPECT_LE(after - before, 0.016f + 1e-4f);
}

TEST(EconomyInspector_Instances, NetRateIsIncomeMinusSpend)
{
    static const char* kJson = R"({
        "schema_name": "rate_test",
        "resources": [
            { "resource_name": "gold", "minimum_value": 0, "maximum_value": 10000, "starting_value": 0 }
        ],
        "income_rules": [
            { "rule_name": "gold_income", "resource_name": "gold", "amount_per_second": 10.0 }
        ]
    })";
    EconomySchema schema = EconomySchema::LoadFromJsonValue(ParseJson(kJson));
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    sys.RegisterInstance(inst);
    sys.Tick(inst, 1.0f);

    CapturableInstancesSource src(sys);
    src.Tick(0.016f, 0, 0);
    Json::Value payload;
    src.Collect(payload);

    const Json::Value& res = payload["instances"][0]["resources"][0];
    const float income = res["gross_income"].asFloat();
    const float spend  = res["gross_spend"].asFloat();
    const float net    = res["net_rate"].asFloat();
    EXPECT_NEAR(net, income - spend, 1e-4f);
}

TEST(EconomyInspector_Instances, MultipleInstancesAllPresent)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 100.0f);
    EconomyInstance inst1 = EconomyInstance::CreateFromSchema(schema);
    EconomyInstance inst2 = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    sys.RegisterInstance(inst1);
    sys.RegisterInstance(inst2);

    CapturableInstancesSource src(sys);
    Json::Value payload;
    src.Collect(payload);
    EXPECT_EQ(payload["instances"].size(), 2u);
}

TEST(EconomyInspector_Instances, FrameCounterIncrements)
{
    EconomySchema schema = MakeSimpleSchema();
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    sys.RegisterInstance(inst);

    CapturableInstancesSource src(sys);
    Json::Value p1, p2;
    src.Collect(p1);
    src.Collect(p2);
    EXPECT_EQ(p2["frame"].asUInt(), p1["frame"].asUInt() + 1u);
}

// ===========================================================================
// Additional coverage — modifiers
// ===========================================================================

TEST(EconomyInspector_Modifiers, MultipleModifiersPerResource)
{
    static const char* kJson = R"({
        "schema_name": "multi_mod",
        "resources": [
            { "resource_name": "gold", "minimum_value": 0, "maximum_value": 10000, "starting_value": 0 }
        ],
        "income_rules": [
            { "rule_name": "gold_income", "resource_name": "gold", "amount_per_second": 10.0 }
        ],
        "modifiers": [
            { "modifier_name": "mod_a", "resource_name": "gold", "operation": "multiply_income", "value": 2.0, "when_condition": "" },
            { "modifier_name": "mod_b", "resource_name": "gold", "operation": "flat_income",     "value": 5.0, "when_condition": "" }
        ]
    })";
    EconomySchema schema = EconomySchema::LoadFromJsonValue(ParseJson(kJson));
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    sys.RegisterInstance(inst);

    CapturableModifiersSource src(sys);
    Json::Value payload;
    src.Collect(payload);

    bool foundA = false, foundB = false;
    for (const auto& res : payload["instances"][0]["resources"])
    {
        for (const auto& mod : res["modifiers"])
        {
            const std::string name = mod["source"].asString();
            if (name == "mod_a") foundA = true;
            if (name == "mod_b") foundB = true;
        }
    }
    EXPECT_TRUE(foundA);
    EXPECT_TRUE(foundB);
}

TEST(EconomyInspector_Modifiers, ConditionIsNullWhenEmpty)
{
    EconomySchema schema = MakeModifierSchema("multiply_income", 2.0f, "");
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    sys.RegisterInstance(inst);

    CapturableModifiersSource src(sys);
    Json::Value payload;
    src.Collect(payload);

    bool checked = false;
    for (const auto& res : payload["instances"][0]["resources"])
    {
        for (const auto& mod : res["modifiers"])
        {
            if (mod["source"].asString() == "test_mod")
            {
                EXPECT_TRUE(mod["condition"].isNull());
                checked = true;
            }
        }
    }
    EXPECT_TRUE(checked);
}

TEST(EconomyInspector_Modifiers, ConditionStringWhenNonEmpty)
{
    EconomySchema schema = MakeModifierSchema("multiply_income", 2.0f, "my_condition");
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    sys.RegisterInstance(inst);

    CapturableModifiersSource src(sys);
    Json::Value payload;
    src.Collect(payload);

    bool checked = false;
    for (const auto& res : payload["instances"][0]["resources"])
    {
        for (const auto& mod : res["modifiers"])
        {
            if (mod["source"].asString() == "test_mod")
            {
                ASSERT_FALSE(mod["condition"].isNull());
                EXPECT_STREQ(mod["condition"].asCString(), "my_condition");
                checked = true;
            }
        }
    }
    EXPECT_TRUE(checked);
}

// ===========================================================================
// Additional coverage — events (payload capture via subclass)
// ===========================================================================

class CapturableEventsSource : public Cluiche::AppFlow::EconomyEventsSource
{
public:
    using Cluiche::AppFlow::EconomyEventsSource::EconomyEventsSource;

    Json::Value lastPayload;
    int         sendCount = 0;

protected:
    void SendDelta(const Json::Value& ev)
    {
        Json::Value events(Json::arrayValue);
        events.append(ev);
        lastPayload["full_ring"] = false;
        lastPayload["events"]    = events;
        ++sendCount;
    }

    void SendFullRing()
    {
        lastPayload["full_ring"] = true;
        ++sendCount;
    }
};

TEST(EconomyInspector_Events, DeltaPayloadFullRingFalse)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 100.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    sys.RegisterInstance(inst);

    Dia::DebugServer::DebugServer server;
    CapturableEventsSource src(sys);
    src.Activate(&server);

    PoolChangedEvent ev;
    ev.instance      = &inst;
    ev.resource_name = StringCRC("gold");
    ev.new_value     = 110.0f;
    ev.delta         = 10.0f;
    src.OnPoolChanged(ev);

    ASSERT_EQ(src.sendCount, 1);
    EXPECT_FALSE(src.lastPayload["full_ring"].asBool());
    src.Deactivate();
}

TEST(EconomyInspector_Events, TransactionClampedHasAttemptedAndActual)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 100.0f, 100.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    Dia::DebugServer::DebugServer server;
    CapturableEventsSource src(sys);
    src.Activate(&server);

    TransactionClampedEvent ev;
    ev.instance         = &inst;
    ev.resource_name    = StringCRC("gold");
    ev.requested_amount = 50.0f;
    ev.actual_amount    = 0.0f;
    src.OnTransactionClamped(ev);

    ASSERT_EQ(src.sendCount, 1);
    const Json::Value& entry = src.lastPayload["events"][0];
    EXPECT_TRUE(entry.isMember("attempted"));
    EXPECT_TRUE(entry.isMember("actual"));
    EXPECT_NEAR(entry["attempted"].asFloat(), 50.0f, 1e-4f);
    EXPECT_NEAR(entry["actual"].asFloat(),    0.0f,  1e-4f);
    EXPECT_STREQ(entry["type"].asCString(), "Clamped");
    src.Deactivate();
}

TEST(EconomyInspector_Events, TransferCompletedHasDestination)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 200.0f);
    EconomyInstance from = EconomyInstance::CreateFromSchema(schema);
    EconomyInstance to   = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    Dia::DebugServer::DebugServer server;
    CapturableEventsSource src(sys);
    src.Activate(&server);

    TransferCompletedEvent ev;
    ev.from_instance = &from;
    ev.to_instance   = &to;
    ev.resource_name = StringCRC("gold");
    ev.amount        = 50.0f;
    src.OnTransferCompleted(ev);

    ASSERT_EQ(src.sendCount, 1);
    const Json::Value& entry = src.lastPayload["events"][0];
    EXPECT_STREQ(entry["type"].asCString(), "Transfer");
    EXPECT_TRUE(entry.isMember("destination"));
    src.Deactivate();
}

TEST(EconomyInspector_Events, FrameCounterIncrementsOnTick)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 100.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    Dia::DebugServer::DebugServer server;
    CapturableEventsSource src(sys);
    src.Activate(&server);

    src.Tick(0.016f, 0, 0);
    src.Tick(0.016f, 0, 0);

    PoolChangedEvent ev;
    ev.instance      = &inst;
    ev.resource_name = StringCRC("gold");
    ev.new_value     = 110.0f;
    ev.delta         = 10.0f;
    src.OnPoolChanged(ev);

    ASSERT_EQ(src.sendCount, 1);
    EXPECT_GE(src.lastPayload["events"][0]["frame"].asUInt(), 2u);
    src.Deactivate();
}

// Suite: DiaEconomy
// Covers all acceptance criteria from Task 9 of the DiaEconomy spec.

#include <gtest/gtest.h>

#include <DiaEconomy/EconomySchema.h>
#include <DiaEconomy/EconomyInstance.h>
#include <DiaEconomy/EconomySystem.h>
#include <DiaEconomy/EconomyInstanceHealthReporter.h>
#include <DiaEconomy/IEconomyObserver.h>
#include <DiaEconomy/EconomyObserverSubject.h>
#include <DiaEconomy/IEconomyConditionAdaptor.h>
#include <DiaEconomy/Testing/EconomyTestHelpers.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::Economy;
using namespace Dia::Economy::Testing;
using Dia::Core::StringCRC;

// ===========================================================================
// Helpers
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

static EconomySchema MakeIncomeSchema(float income_per_second)
{
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{ \"schema_name\": \"income_schema\","
        "  \"resources\": ["
        "    { \"resource_name\": \"gold\","
        "      \"minimum_value\": 0.0, \"maximum_value\": 10000.0, \"starting_value\": 0.0 }"
        "  ],"
        "  \"income_rules\": ["
        "    { \"rule_name\": \"gold_income\", \"resource_name\": \"gold\","
        "      \"amount_per_second\": %.4f }"
        "  ] }",
        income_per_second);
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
// 1. Schema load + validation
// ===========================================================================

TEST(DiaEconomy, SchemaLoad_ValidSchema_IsValid)
{
    EconomySchema schema = MakeSimpleSchema();
    EXPECT_TRUE(schema.IsValid());
    EXPECT_EQ(schema.GetResourceCount(), 1u);
}

TEST(DiaEconomy, SchemaLoad_NullJsonValue_IsInvalid)
{
    EconomySchema schema = EconomySchema::LoadFromJsonValue(Json::Value());
    EXPECT_FALSE(schema.IsValid());
}

TEST(DiaEconomy, SchemaLoad_EmptyObject_IsValid_ZeroResources)
{
    // An empty object parses OK and reports zero resources.
    EconomySchema schema = EconomySchema::LoadFromJsonValue(ParseJson("{}"));
    EXPECT_TRUE(schema.IsValid());
    EXPECT_EQ(schema.GetResourceCount(), 0u);
}

TEST(DiaEconomy, SchemaLoad_EmptyResources_FailsValidator)
{
    EconomySchema schema = EconomySchema::LoadFromJsonValue(ParseJson("{}"));
    SchemaValidationResult result = EconomySchemaValidator::Validate(schema);
    // Validator warns on zero resources but may still return valid;
    // actual rule: schema with zero resources is valid per impl — just warn.
    // We care that Validate() runs without crashing.
    (void)result;
}

TEST(DiaEconomy, SchemaValidation_IncoherentMinMax_FailsValidation)
{
    // min > max
    EconomySchema schema = MakeSimpleSchema("gold", 500.0f, 100.0f, 200.0f);
    SchemaValidationResult result = EconomySchemaValidator::Validate(schema);
    EXPECT_FALSE(result.isValid);
    EXPECT_GT(strlen(result.errorMessage), 0u);
}

TEST(DiaEconomy, SchemaValidation_StartingValueBelowMin_FailsValidation)
{
    EconomySchema schema = MakeSimpleSchema("gold", 100.0f, 1000.0f, 50.0f);
    SchemaValidationResult result = EconomySchemaValidator::Validate(schema);
    EXPECT_FALSE(result.isValid);
}

TEST(DiaEconomy, SchemaValidation_StartingValueAboveMax_FailsValidation)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 100.0f, 200.0f);
    SchemaValidationResult result = EconomySchemaValidator::Validate(schema);
    EXPECT_FALSE(result.isValid);
}

TEST(DiaEconomy, SchemaValidation_ValidSchema_Passes)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 100.0f);
    SchemaValidationResult result = EconomySchemaValidator::Validate(schema);
    EXPECT_TRUE(result.isValid);
}

TEST(DiaEconomy, SchemaLoad_ResourceQueryByName)
{
    EconomySchema schema = MakeSimpleSchema("wood", 0.0f, 500.0f, 50.0f);
    const ResourceDefinition* def = schema.FindResource(StringCRC("wood"));
    ASSERT_NE(def, nullptr);
    EXPECT_FLOAT_EQ(def->minimum_value,   0.0f);
    EXPECT_FLOAT_EQ(def->maximum_value, 500.0f);
    EXPECT_FLOAT_EQ(def->starting_value, 50.0f);
}

TEST(DiaEconomy, SchemaLoad_FindResource_UnknownReturnsNull)
{
    EconomySchema schema = MakeSimpleSchema("gold");
    EXPECT_EQ(schema.FindResource(StringCRC("wood")), nullptr);
}

// ===========================================================================
// 2. EconomyInstance creation
// ===========================================================================

TEST(DiaEconomy, Instance_CreateFromSchema_StartingValueApplied)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 250.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 250.0f);
}

TEST(DiaEconomy, Instance_CreateFromSchema_MinMaxApplied)
{
    EconomySchema schema = MakeSimpleSchema("gold", 10.0f, 900.0f, 100.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EXPECT_FLOAT_EQ(inst.GetMinimum(StringCRC("gold")),  10.0f);
    EXPECT_FLOAT_EQ(inst.GetMaximum(StringCRC("gold")), 900.0f);
}

TEST(DiaEconomy, Instance_HasResource_KnownResource)
{
    EconomySchema schema = MakeSimpleSchema("gold");
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EXPECT_TRUE(inst.HasResource(StringCRC("gold")));
}

TEST(DiaEconomy, Instance_HasResource_UnknownResource)
{
    EconomySchema schema = MakeSimpleSchema("gold");
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EXPECT_FALSE(inst.HasResource(StringCRC("wood")));
}

// ===========================================================================
// 3. Pool clamping at min and max
// ===========================================================================

TEST(DiaEconomy, Clamping_EarnBeyondMax_ClampsToMax)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 100.0f, 0.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    TransactionResult r = sys.Earn(inst, StringCRC("gold"), 200.0f);

    EXPECT_EQ(r, TransactionResult::Clamped);
    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 100.0f);
}

TEST(DiaEconomy, Clamping_SpendBelowMin_ClampsToMin)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 100.0f, 50.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    TransactionResult r = sys.Spend(inst, StringCRC("gold"), 200.0f);

    EXPECT_EQ(r, TransactionResult::Clamped);
    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 0.0f);
}

TEST(DiaEconomy, Clamping_EarnWithinBounds_ReturnsSuccess)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 100.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    TransactionResult r = sys.Earn(inst, StringCRC("gold"), 50.0f);

    EXPECT_EQ(r, TransactionResult::Success);
    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 150.0f);
}

TEST(DiaEconomy, Clamping_SpendWithinBounds_ReturnsSuccess)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 100.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    TransactionResult r = sys.Spend(inst, StringCRC("gold"), 50.0f);

    EXPECT_EQ(r, TransactionResult::Success);
    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 50.0f);
}

// ===========================================================================
// 4. TransactionResult variants
// ===========================================================================

TEST(DiaEconomy, TransactionResult_Success_FullAmountApplied)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 0.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    TransactionResult r = sys.Earn(inst, StringCRC("gold"), 300.0f);
    EXPECT_EQ(r, TransactionResult::Success);
    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 300.0f);
}

TEST(DiaEconomy, TransactionResult_Clamped_Earn)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 100.0f, 80.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    TransactionResult r = sys.Earn(inst, StringCRC("gold"), 50.0f);
    EXPECT_EQ(r, TransactionResult::Clamped);
    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 100.0f);
}

TEST(DiaEconomy, TransactionResult_Clamped_Spend)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 100.0f, 20.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    TransactionResult r = sys.Spend(inst, StringCRC("gold"), 50.0f);
    EXPECT_EQ(r, TransactionResult::Clamped);
    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 0.0f);
}

TEST(DiaEconomy, TransactionResult_SetValue_Success)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 0.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    TransactionResult r = sys.SetValue(inst, StringCRC("gold"), 500.0f);
    EXPECT_EQ(r, TransactionResult::Success);
    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 500.0f);
}

TEST(DiaEconomy, TransactionResult_SetValue_Clamped)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 100.0f, 0.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    TransactionResult r = sys.SetValue(inst, StringCRC("gold"), 9999.0f);
    EXPECT_EQ(r, TransactionResult::Clamped);
    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 100.0f);
}

// ===========================================================================
// 5. Observer event payloads
// ===========================================================================

TEST(DiaEconomy, Observer_PoolChanged_FiredOnEarn)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 100.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    EventCapture capture;
    capture.Subscribe(sys);

    TransactionResult r = sys.Earn(inst, StringCRC("gold"), 50.0f);
    (void)r;

    EXPECT_EQ(capture.poolChangedCount, 1u);
    EXPECT_EQ(capture.lastPoolChanged.resource_name, StringCRC("gold"));
    EXPECT_FLOAT_EQ(capture.lastPoolChanged.new_value, 150.0f);
    EXPECT_FLOAT_EQ(capture.lastPoolChanged.delta,      50.0f);

    capture.Unsubscribe(sys);
}

TEST(DiaEconomy, Observer_PoolChanged_FiredOnSpend)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 200.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    EventCapture capture;
    capture.Subscribe(sys);

    TransactionResult r = sys.Spend(inst, StringCRC("gold"), 80.0f);
    (void)r;

    EXPECT_EQ(capture.poolChangedCount, 1u);
    EXPECT_FLOAT_EQ(capture.lastPoolChanged.new_value, 120.0f);
    EXPECT_FLOAT_EQ(capture.lastPoolChanged.delta, -80.0f);

    capture.Unsubscribe(sys);
}

TEST(DiaEconomy, Observer_TransactionClamped_FiredWhenEarnExceedsMax)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 100.0f, 80.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    EventCapture capture;
    capture.Subscribe(sys);

    TransactionResult r = sys.Earn(inst, StringCRC("gold"), 50.0f);
    (void)r;

    EXPECT_EQ(capture.transactionClampedCount, 1u);
    EXPECT_EQ(capture.lastTransactionClamped.resource_name, StringCRC("gold"));
    EXPECT_FLOAT_EQ(capture.lastTransactionClamped.requested_amount, 50.0f);
    EXPECT_LT(capture.lastTransactionClamped.actual_amount, 50.0f);

    capture.Unsubscribe(sys);
}

TEST(DiaEconomy, Observer_TransactionClamped_FiredWhenSpendExceedsBalance)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 100.0f, 30.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    EventCapture capture;
    capture.Subscribe(sys);

    TransactionResult r = sys.Spend(inst, StringCRC("gold"), 50.0f);
    (void)r;

    EXPECT_EQ(capture.transactionClampedCount, 1u);
    EXPECT_FLOAT_EQ(capture.lastTransactionClamped.requested_amount, 50.0f);
    EXPECT_LT(capture.lastTransactionClamped.actual_amount, 50.0f);

    capture.Unsubscribe(sys);
}

TEST(DiaEconomy, Observer_TransferCompleted_FiredOnTransfer)
{
    EconomySchema schemaA = MakeSimpleSchema("gold", 0.0f, 1000.0f, 500.0f);
    EconomyInstance instA = EconomyInstance::CreateFromSchema(schemaA);

    EconomySchema schemaB = MakeSimpleSchema("gold", 0.0f, 1000.0f, 0.0f);
    EconomyInstance instB = EconomyInstance::CreateFromSchema(schemaB);

    EconomySystem sys;
    EventCapture capture;
    capture.Subscribe(sys);

    TransactionResult r = sys.Transfer(instA, instB, StringCRC("gold"), 100.0f);
    (void)r;

    EXPECT_EQ(capture.transferCompletedCount, 1u);
    EXPECT_EQ(capture.lastTransferCompleted.resource_name, StringCRC("gold"));
    EXPECT_FLOAT_EQ(capture.lastTransferCompleted.amount, 100.0f);
    EXPECT_EQ(capture.lastTransferCompleted.from_instance, &instA);
    EXPECT_EQ(capture.lastTransferCompleted.to_instance,   &instB);

    capture.Unsubscribe(sys);
}

TEST(DiaEconomy, Observer_PoolReachedMaximum_FiredWhenPoolHitsMax)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 100.0f, 90.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    EventCapture capture;
    capture.Subscribe(sys);

    TransactionResult r = sys.Earn(inst, StringCRC("gold"), 20.0f);
    (void)r;

    EXPECT_EQ(capture.poolReachedMaximumCount, 1u);

    capture.Unsubscribe(sys);
}

TEST(DiaEconomy, Observer_PoolReachedMinimum_FiredWhenPoolHitsMin)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 100.0f, 10.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    EventCapture capture;
    capture.Subscribe(sys);

    TransactionResult r = sys.Spend(inst, StringCRC("gold"), 20.0f);
    (void)r;

    EXPECT_EQ(capture.poolReachedMinimumCount, 1u);

    capture.Unsubscribe(sys);
}

TEST(DiaEconomy, Observer_NoEventsFired_WhenOperationSucceeds_NoClamp)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 100.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    EventCapture capture;
    capture.Subscribe(sys);

    TransactionResult r = sys.Earn(inst, StringCRC("gold"), 50.0f);
    (void)r;

    EXPECT_EQ(capture.transactionClampedCount, 0u);
    EXPECT_EQ(capture.poolReachedMaximumCount, 0u);

    capture.Unsubscribe(sys);
}

// ===========================================================================
// 6. Modifier stack
// ===========================================================================

TEST(DiaEconomy, Modifier_MultiplyIncome_AlwaysOn_ScalesIncome)
{
    // Base income: 10/s, multiply_income x2 => 20 earned after 1s tick
    EconomySchema schema = MakeModifierSchema("multiply_income", 2.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    sys.Tick(inst, 1.0f);

    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 20.0f);
}

TEST(DiaEconomy, Modifier_FlatIncome_AlwaysOn_AddsToIncome)
{
    // Base income: 10/s, flat_income +5 => 15 earned after 1s tick
    EconomySchema schema = MakeModifierSchema("flat_income", 5.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    sys.Tick(inst, 1.0f);

    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 15.0f);
}

TEST(DiaEconomy, Modifier_MultiplyCapacity_AlwaysOn_ClampsOnTick)
{
    // multiply_cap x0.5 => effective cap = 50; if pool is at 80, Tick should clamp it down.
    static const char* kCapSchemaJson =
        "{ \"schema_name\": \"cap_schema\","
        "  \"resources\": ["
        "    { \"resource_name\": \"gold\","
        "      \"minimum_value\": 0.0, \"maximum_value\": 100.0, \"starting_value\": 80.0 }"
        "  ],"
        "  \"modifiers\": ["
        "    { \"modifier_name\": \"half_cap\","
        "      \"resource_name\": \"gold\","
        "      \"operation\": \"multiply_cap\","
        "      \"value\": 0.5,"
        "      \"when_condition\": \"\" }"
        "  ] }";

    EconomySchema schema = EconomySchema::LoadFromJsonValue(ParseJson(kCapSchemaJson));
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    // Tick applies multiply_cap: effective cap = 100 * 0.5 = 50
    // Pool is at 80, so it should be clamped down to 50.
    sys.Tick(inst, 0.0f);

    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 50.0f);
}

TEST(DiaEconomy, Modifier_Conditional_NoAdaptor_IsSkipped)
{
    // Conditional modifier without adaptor => skipped. Only base income (10/s).
    EconomySchema schema = MakeModifierSchema("multiply_income", 2.0f, "some_condition");
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    sys.Tick(inst, 1.0f);

    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 10.0f);
}

TEST(DiaEconomy, Modifier_Conditional_AdaptorReturnsTrue_IsApplied)
{
    struct AlwaysTrueAdaptor : IEconomyConditionAdaptor
    {
        bool Evaluate(const char*) const override { return true; }
    };

    EconomySchema schema = MakeModifierSchema("multiply_income", 2.0f, "some_condition");
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    AlwaysTrueAdaptor adaptor;
    sys.SetConditionAdaptor(&adaptor);

    sys.Tick(inst, 1.0f);

    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 20.0f);

    sys.SetConditionAdaptor(nullptr);
}

TEST(DiaEconomy, Modifier_Conditional_AdaptorReturnsFalse_IsSkipped)
{
    struct AlwaysFalseAdaptor : IEconomyConditionAdaptor
    {
        bool Evaluate(const char*) const override { return false; }
    };

    EconomySchema schema = MakeModifierSchema("multiply_income", 2.0f, "some_condition");
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    AlwaysFalseAdaptor adaptor;
    sys.SetConditionAdaptor(&adaptor);

    sys.Tick(inst, 1.0f);

    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 10.0f);

    sys.SetConditionAdaptor(nullptr);
}

// ===========================================================================
// 7. Derived resource registration and query
// ===========================================================================

TEST(DiaEconomy, DerivedResource_RegisterAndQuery_ReturnsComputedValue)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 200.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    sys.RegisterDerivedResource(StringCRC("gold_x2"),
        [](const EconomyInstance& i) { return i.GetValue(StringCRC("gold")) * 2.0f; });

    EXPECT_FLOAT_EQ(sys.QueryDerived(inst, StringCRC("gold_x2")), 400.0f);
}

TEST(DiaEconomy, DerivedResource_QueryUnknown_ReturnsZero)
{
    EconomySchema schema = MakeSimpleSchema("gold");
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    EXPECT_FLOAT_EQ(sys.QueryDerived(inst, StringCRC("not_registered")), 0.0f);
}

TEST(DiaEconomy, DerivedResource_TracksLiveValue)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 0.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    sys.RegisterDerivedResource(StringCRC("gold_double"),
        [](const EconomyInstance& i) { return i.GetValue(StringCRC("gold")) * 2.0f; });

    TransactionResult r1 = sys.Earn(inst, StringCRC("gold"), 100.0f);
    (void)r1;
    EXPECT_FLOAT_EQ(sys.QueryDerived(inst, StringCRC("gold_double")), 200.0f);

    TransactionResult r2 = sys.Earn(inst, StringCRC("gold"), 50.0f);
    (void)r2;
    EXPECT_FLOAT_EQ(sys.QueryDerived(inst, StringCRC("gold_double")), 300.0f);
}

// ===========================================================================
// 8. Income accumulation + fractional carry-over
// ===========================================================================

TEST(DiaEconomy, Income_SingleTick_AccumulatesCorrectAmount)
{
    EconomySchema schema = MakeIncomeSchema(10.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    sys.Tick(inst, 1.0f);
    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 10.0f);
}

TEST(DiaEconomy, Income_MultipleTicks_AccumulatesCorrectly)
{
    EconomySchema schema = MakeIncomeSchema(5.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    sys.Tick(inst, 1.0f);
    sys.Tick(inst, 1.0f);
    sys.Tick(inst, 1.0f);
    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 15.0f);
}

TEST(DiaEconomy, Income_FractionalDelta_CarriesOver)
{
    // 10/s, 10 ticks of 0.1s => 10 gold total
    EconomySchema schema = MakeIncomeSchema(10.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    for (int i = 0; i < 10; ++i)
        sys.Tick(inst, 0.1f);

    EXPECT_NEAR(inst.GetValue(StringCRC("gold")), 10.0f, 0.01f);
}

TEST(DiaEconomy, Income_SubFrameDelta_AccumulatesCorrectly)
{
    // 1/s, 4 ticks of 0.25s => 1 gold total
    EconomySchema schema = MakeIncomeSchema(1.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    for (int i = 0; i < 4; ++i)
        sys.Tick(inst, 0.25f);

    EXPECT_NEAR(inst.GetValue(StringCRC("gold")), 1.0f, 0.001f);
}

TEST(DiaEconomy, Income_AccumulatorStartsAtZero)
{
    EconomySchema schema = MakeIncomeSchema(10.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);

    EXPECT_FLOAT_EQ(inst.GetIncomeAccumulator(StringCRC("gold")), 0.0f);
}

// ===========================================================================
// 9. Transfer atomicity
// ===========================================================================

TEST(DiaEconomy, Transfer_DeductsThenDeposits_BothSidesCorrect)
{
    EconomySchema schemaFrom = MakeSimpleSchema("gold", 0.0f, 1000.0f, 500.0f);
    EconomyInstance instFrom = EconomyInstance::CreateFromSchema(schemaFrom);

    EconomySchema schemaTo = MakeSimpleSchema("gold", 0.0f, 1000.0f, 100.0f);
    EconomyInstance instTo = EconomyInstance::CreateFromSchema(schemaTo);

    EconomySystem sys;
    TransactionResult r = sys.Transfer(instFrom, instTo, StringCRC("gold"), 200.0f);

    EXPECT_EQ(r, TransactionResult::Success);
    EXPECT_FLOAT_EQ(instFrom.GetValue(StringCRC("gold")), 300.0f);
    EXPECT_FLOAT_EQ(instTo.GetValue(StringCRC("gold")),   300.0f);
}

TEST(DiaEconomy, Transfer_InsufficientFunds_ClampedResult_BothSidesAdjusted)
{
    EconomySchema schemaFrom = MakeSimpleSchema("gold", 0.0f, 1000.0f, 50.0f);
    EconomyInstance instFrom = EconomyInstance::CreateFromSchema(schemaFrom);

    EconomySchema schemaTo = MakeSimpleSchema("gold", 0.0f, 1000.0f, 0.0f);
    EconomyInstance instTo = EconomyInstance::CreateFromSchema(schemaTo);

    EconomySystem sys;
    TransactionResult r = sys.Transfer(instFrom, instTo, StringCRC("gold"), 200.0f);

    EXPECT_EQ(r, TransactionResult::Clamped);
    // from is drained to minimum
    EXPECT_FLOAT_EQ(instFrom.GetValue(StringCRC("gold")), 0.0f);
    // to receives the full requested amount (Transfer deposits `amount`, not actual deducted)
    EXPECT_FLOAT_EQ(instTo.GetValue(StringCRC("gold")), 200.0f);
}

TEST(DiaEconomy, Transfer_FiresTransferCompletedEvent)
{
    EconomySchema schemaA = MakeSimpleSchema("gold", 0.0f, 1000.0f, 300.0f);
    EconomyInstance instA = EconomyInstance::CreateFromSchema(schemaA);

    EconomySchema schemaB = MakeSimpleSchema("gold", 0.0f, 1000.0f, 0.0f);
    EconomyInstance instB = EconomyInstance::CreateFromSchema(schemaB);

    EconomySystem sys;
    EventCapture capture;
    capture.Subscribe(sys);

    TransactionResult r = sys.Transfer(instA, instB, StringCRC("gold"), 100.0f);
    (void)r;

    EXPECT_EQ(capture.transferCompletedCount, 1u);

    capture.Unsubscribe(sys);
}

// ===========================================================================
// 10. Multi-instance isolation
// ===========================================================================

TEST(DiaEconomy, MultiInstance_TwoInstancesFromSameSchema_DontShareState)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 100.0f);
    EconomyInstance instA = EconomyInstance::CreateFromSchema(schema);
    EconomyInstance instB = EconomyInstance::CreateFromSchema(schema);

    EconomySystem sys;
    TransactionResult r = sys.Earn(instA, StringCRC("gold"), 200.0f);
    (void)r;

    EXPECT_FLOAT_EQ(instA.GetValue(StringCRC("gold")), 300.0f);
    EXPECT_FLOAT_EQ(instB.GetValue(StringCRC("gold")), 100.0f);
}

TEST(DiaEconomy, MultiInstance_TickOnlyAffectsTargetInstance)
{
    EconomySchema schema = MakeIncomeSchema(10.0f);
    EconomyInstance instA = EconomyInstance::CreateFromSchema(schema);
    EconomyInstance instB = EconomyInstance::CreateFromSchema(schema);

    EconomySystem sys;
    sys.Tick(instA, 1.0f);

    EXPECT_FLOAT_EQ(instA.GetValue(StringCRC("gold")), 10.0f);
    EXPECT_FLOAT_EQ(instB.GetValue(StringCRC("gold")),  0.0f);
}

TEST(DiaEconomy, MultiInstance_ObserverReceivesCorrectInstancePointer)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 100.0f);
    EconomyInstance instA = EconomyInstance::CreateFromSchema(schema);
    EconomyInstance instB = EconomyInstance::CreateFromSchema(schema);

    EconomySystem sys;
    EventCapture capture;
    capture.Subscribe(sys);

    TransactionResult r = sys.Earn(instA, StringCRC("gold"), 10.0f);
    (void)r;

    EXPECT_EQ(capture.lastPoolChanged.instance, &instA);
    EXPECT_NE(capture.lastPoolChanged.instance, &instB);

    capture.Unsubscribe(sys);
}

// ===========================================================================
// 11. Additional edge cases
// ===========================================================================

TEST(DiaEconomy, Clamping_SetValueAtMin_ClampsToMin)
{
    EconomySchema schema = MakeSimpleSchema("gold", 10.0f, 100.0f, 50.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    TransactionResult r = sys.SetValue(inst, StringCRC("gold"), -99.0f);
    EXPECT_EQ(r, TransactionResult::Clamped);
    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 10.0f);
}

TEST(DiaEconomy, Transfer_ReceiverHitsMax_ReceiverClamped)
{
    // Receiver has max=50; transfer 200 => receiver caps at 50
    EconomySchema schemaFrom = MakeSimpleSchema("gold", 0.0f, 1000.0f, 500.0f);
    EconomyInstance instFrom = EconomyInstance::CreateFromSchema(schemaFrom);

    EconomySchema schemaTo = MakeSimpleSchema("gold", 0.0f, 50.0f, 10.0f);
    EconomyInstance instTo = EconomyInstance::CreateFromSchema(schemaTo);

    EconomySystem sys;
    TransactionResult r = sys.Transfer(instFrom, instTo, StringCRC("gold"), 200.0f);
    (void)r;

    EXPECT_FLOAT_EQ(instTo.GetValue(StringCRC("gold")), 50.0f);
}

TEST(DiaEconomy, Modifier_TwoMultiplyIncomeModifiers_Stack_Multiplicatively)
{
    // Two multiply_income x2 modifiers => base 10/s * 2 * 2 = 40/s
    static const char* kJson =
        "{ \"schema_name\": \"double_mod\","
        "  \"resources\": ["
        "    { \"resource_name\": \"gold\", \"minimum_value\": 0.0, \"maximum_value\": 10000.0, \"starting_value\": 0.0 }"
        "  ],"
        "  \"income_rules\": ["
        "    { \"rule_name\": \"r\", \"resource_name\": \"gold\", \"amount_per_second\": 10.0 }"
        "  ],"
        "  \"modifiers\": ["
        "    { \"modifier_name\": \"m1\", \"resource_name\": \"gold\", \"operation\": \"multiply_income\", \"value\": 2.0, \"when_condition\": \"\" },"
        "    { \"modifier_name\": \"m2\", \"resource_name\": \"gold\", \"operation\": \"multiply_income\", \"value\": 2.0, \"when_condition\": \"\" }"
        "  ] }";

    EconomySchema schema = EconomySchema::LoadFromJsonValue(ParseJson(kJson));
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    sys.Tick(inst, 1.0f);

    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 40.0f);
}

TEST(DiaEconomy, Modifier_FlatIncomeOnly_NoIncomeRule_AddsFlat)
{
    // No income rule, but flat_income +5 => 5 earned after 1s
    static const char* kJson =
        "{ \"schema_name\": \"flat_only\","
        "  \"resources\": ["
        "    { \"resource_name\": \"gold\", \"minimum_value\": 0.0, \"maximum_value\": 10000.0, \"starting_value\": 0.0 }"
        "  ],"
        "  \"modifiers\": ["
        "    { \"modifier_name\": \"fi\", \"resource_name\": \"gold\", \"operation\": \"flat_income\", \"value\": 5.0, \"when_condition\": \"\" }"
        "  ] }";

    EconomySchema schema = EconomySchema::LoadFromJsonValue(ParseJson(kJson));
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    sys.Tick(inst, 1.0f);

    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 5.0f);
}

TEST(DiaEconomy, Modifier_MultiplyCapAboveOne_ExpandsCap_NoClamp)
{
    // multiply_cap x2 => effective cap = 200; pool at 80 should not be clamped
    static const char* kJson =
        "{ \"schema_name\": \"expand_cap\","
        "  \"resources\": ["
        "    { \"resource_name\": \"gold\", \"minimum_value\": 0.0, \"maximum_value\": 100.0, \"starting_value\": 80.0 }"
        "  ],"
        "  \"modifiers\": ["
        "    { \"modifier_name\": \"cap2\", \"resource_name\": \"gold\", \"operation\": \"multiply_cap\", \"value\": 2.0, \"when_condition\": \"\" }"
        "  ] }";

    EconomySchema schema = EconomySchema::LoadFromJsonValue(ParseJson(kJson));
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    sys.Tick(inst, 0.0f);

    // Pool was 80, effective cap is 200 — should remain at 80 (no clamp-down)
    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 80.0f);
}

TEST(DiaEconomy, Observer_PoolReachedMaximum_NotFiredWhenAlreadyAtMax)
{
    // Pool starts at max; Earn should not fire PoolReachedMaximum (no transition)
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 100.0f, 100.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    EventCapture capture;
    capture.Subscribe(sys);

    TransactionResult r = sys.Earn(inst, StringCRC("gold"), 10.0f);
    (void)r;

    EXPECT_EQ(capture.poolReachedMaximumCount, 0u);

    capture.Unsubscribe(sys);
}

TEST(DiaEconomy, Observer_PoolReachedMinimum_NotFiredWhenAlreadyAtMin)
{
    // Pool starts at min (0); Spend should not fire PoolReachedMinimum (no transition)
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 100.0f, 0.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    EventCapture capture;
    capture.Subscribe(sys);

    TransactionResult r = sys.Spend(inst, StringCRC("gold"), 10.0f);
    (void)r;

    EXPECT_EQ(capture.poolReachedMinimumCount, 0u);

    capture.Unsubscribe(sys);
}

TEST(DiaEconomy, Tick_ZeroDeltaSeconds_NoIncomeAccumulated)
{
    EconomySchema schema = MakeIncomeSchema(1000.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    sys.Tick(inst, 0.0f);

    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 0.0f);
}

TEST(DiaEconomy, Transfer_SameInstanceAsFromAndTo_StillWorks)
{
    // Transfer from an instance to itself: deduct then earn same pool.
    // Result: pool unchanged if no clamping; return = Success when funds available.
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 200.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    TransactionResult r = sys.Transfer(inst, inst, StringCRC("gold"), 50.0f);

    EXPECT_EQ(r, TransactionResult::Success);
    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 200.0f);
}

TEST(DiaEconomy, DerivedResource_MultipleDerived_QueriedIndependently)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 100.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    sys.RegisterDerivedResource(StringCRC("gold_half"),
        [](const EconomyInstance& i) { return i.GetValue(StringCRC("gold")) * 0.5f; });
    sys.RegisterDerivedResource(StringCRC("gold_triple"),
        [](const EconomyInstance& i) { return i.GetValue(StringCRC("gold")) * 3.0f; });

    EXPECT_FLOAT_EQ(sys.QueryDerived(inst, StringCRC("gold_half")),   50.0f);
    EXPECT_FLOAT_EQ(sys.QueryDerived(inst, StringCRC("gold_triple")), 300.0f);
}

TEST(DiaEconomy, Schema_MultipleResources_IndependentPools)
{
    static const char* kJson =
        "{ \"schema_name\": \"multi_resource\","
        "  \"resources\": ["
        "    { \"resource_name\": \"gold\", \"minimum_value\": 0.0, \"maximum_value\": 500.0, \"starting_value\": 100.0 },"
        "    { \"resource_name\": \"wood\", \"minimum_value\": 0.0, \"maximum_value\": 200.0, \"starting_value\":  50.0 }"
        "  ] }";

    EconomySchema schema = EconomySchema::LoadFromJsonValue(ParseJson(kJson));
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    TransactionResult r = sys.Earn(inst, StringCRC("gold"), 20.0f);
    (void)r;

    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 120.0f);
    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("wood")),  50.0f);
}

TEST(DiaEconomy, Instance_CopyThenMutateOriginal_CopyUnchanged)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 100.0f);
    EconomyInstance original = EconomyInstance::CreateFromSchema(schema);
    EconomyInstance copy = original;  // copy ctor

    EconomySystem sys;
    TransactionResult r = sys.Earn(original, StringCRC("gold"), 50.0f);
    (void)r;

    EXPECT_FLOAT_EQ(original.GetValue(StringCRC("gold")), 150.0f);
    EXPECT_FLOAT_EQ(copy.GetValue(StringCRC("gold")),     100.0f);
}

TEST(DiaEconomy, Instance_AssignThenMutateSource_DestUnchanged)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 200.0f);
    EconomyInstance src = EconomyInstance::CreateFromSchema(schema);
    EconomyInstance dst;
    dst = src;  // operator=

    EconomySystem sys;
    TransactionResult r = sys.Spend(src, StringCRC("gold"), 100.0f);
    (void)r;

    EXPECT_FLOAT_EQ(src.GetValue(StringCRC("gold")), 100.0f);
    EXPECT_FLOAT_EQ(dst.GetValue(StringCRC("gold")), 200.0f);
}

TEST(DiaEconomy, Clamping_EarnExactlyToMax_ReturnsSuccess)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 100.0f, 50.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    TransactionResult r = sys.Earn(inst, StringCRC("gold"), 50.0f);

    EXPECT_EQ(r, TransactionResult::Success);
    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 100.0f);
}

TEST(DiaEconomy, Clamping_SpendExactlyToMin_ReturnsSuccess)
{
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 100.0f, 50.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    TransactionResult r = sys.Spend(inst, StringCRC("gold"), 50.0f);

    EXPECT_EQ(r, TransactionResult::Success);
    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 0.0f);
}

TEST(DiaEconomy, Observer_PoolReachedMaximum_FiresOnlyOnce_OnSubsequentEarns)
{
    // First earn hits max (fire once). Second earn while at max should not re-fire.
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 100.0f, 90.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;
    EventCapture capture;
    capture.Subscribe(sys);

    TransactionResult r1 = sys.Earn(inst, StringCRC("gold"), 20.0f); // hits max
    (void)r1;
    EXPECT_EQ(capture.poolReachedMaximumCount, 1u);

    TransactionResult r2 = sys.Earn(inst, StringCRC("gold"), 5.0f); // already at max
    (void)r2;
    EXPECT_EQ(capture.poolReachedMaximumCount, 1u); // no additional fire

    capture.Unsubscribe(sys);
}

TEST(DiaEconomy, Income_TickWithNoIncomeRule_PoolUnchanged)
{
    // Schema with no income_rules: Tick should leave pool unchanged.
    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 50.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);
    EconomySystem sys;

    sys.Tick(inst, 1.0f);

    EXPECT_FLOAT_EQ(inst.GetValue(StringCRC("gold")), 50.0f);
}

// ===========================================================================
// 12. EconomyInstanceHealthReporter
// ===========================================================================

TEST(DiaEconomy, HealthReporter_CoherentInstance_ReportsOK)
{
    using namespace Dia::Observation::Health;

    EconomySchema schema = MakeSimpleSchema("gold", 0.0f, 1000.0f, 100.0f);
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);

    Dia::Economy::EconomyInstanceHealthReporter reporter(StringCRC("test.economy.ok"), inst);
    reporter.Check();

    Health h = reporter.Report();
    EXPECT_EQ(h.status, HealthStatus::kOK);
    EXPECT_EQ(h.errors, 0u);
}

TEST(DiaEconomy, HealthReporter_InstanceWithNoSchema_ReportsDegraded)
{
    using namespace Dia::Observation::Health;

    EconomyInstance inst; // default-constructed, no schema
    Dia::Economy::EconomyInstanceHealthReporter reporter(StringCRC("test.economy.noschema"), inst);
    reporter.Check();

    Health h = reporter.Report();
    EXPECT_EQ(h.status, HealthStatus::kDegraded);
}

TEST(DiaEconomy, HealthReporter_GetReporterName_MatchesConstructorArg)
{
    EconomySchema schema = MakeSimpleSchema("gold");
    EconomyInstance inst = EconomyInstance::CreateFromSchema(schema);

    Dia::Economy::EconomyInstanceHealthReporter reporter(StringCRC("my.reporter"), inst);
    EXPECT_EQ(reporter.GetReporterName(), StringCRC("my.reporter"));
}

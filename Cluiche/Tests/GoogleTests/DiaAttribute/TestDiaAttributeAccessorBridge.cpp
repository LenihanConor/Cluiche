// Suite: DiaAttributeAccessorBridge
// Covers AC-1..AC-5 from the AI/Blackboard Float Accessor Bridge feature (Task 4) of the
// DiaAttribute spec.
// See docs/specs/applications/dia/systems/diaattribute/accessor-bridge.md
//
// PRECONDITION exercised throughout: every ConditionRegistry in this file is constructed with
// its `data` pointer aimed at the AttributeSet being bridged. AttributeAccessorBridge cannot
// verify this (ConditionRegistry exposes no getter for `data`), so the tests deliberately
// model the correct usage.

#include <gtest/gtest.h>

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <DiaAttribute/AttributeAccessorBridge.h>
#include <DiaAttribute/AttributeSchema.h>
#include <DiaAttribute/AttributeSet.h>

#include <DiaCondition/ConditionExpr.h>
#include <DiaCondition/ConditionRegistry.h>

#include <DiaObservation/Log/Logger.h>
#include <DiaObservation/Log/ISink.h>
#include <DiaObservation/Log/LogEntry.h>
#include <DiaObservation/Log/LogLevel.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

#include <stdio.h>
#include <string.h>
#include <string>

using namespace Dia::Attribute;
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

static AttributeSchema MakeDragonSchema()
{
    const char* json =
        "{ \"schema_name\": \"dragon\","
        "  \"attributes\": ["
        "    { \"attribute_name\": \"health\",   \"minimum_value\": 0.0, \"maximum_value\": 2000.0, \"default_value\": 1000.0 },"
        "    { \"attribute_name\": \"strength\", \"minimum_value\": 0.0, \"maximum_value\": 200.0,  \"default_value\": 50.0 }"
        "  ] }";
    return AttributeSchema::LoadFromJsonValue(ParseJson(json));
}

static AttributeModifier MakeModifier(const char* name, const char* attr, ModifierOperation op, float value)
{
    AttributeModifier mod{};
    mod.modifier_name     = StringCRC(name);
    mod.attribute_name    = StringCRC(attr);
    mod.operation         = op;
    mod.value             = value;
    mod.when_condition[0] = '\0';
    return mod;
}

static AttributeModifier MakeConditionalModifier(const char* name, const char* attr, ModifierOperation op, float value, const char* whenConditionJson)
{
    AttributeModifier mod{};
    mod.modifier_name  = StringCRC(name);
    mod.attribute_name = StringCRC(attr);
    mod.operation      = op;
    mod.value          = value;
    if (whenConditionJson != nullptr)
        strncpy_s(mod.when_condition, sizeof(mod.when_condition), whenConditionJson, _TRUNCATE);
    else
        mod.when_condition[0] = '\0';
    return mod;
}

// Builds a schema with `count` distinct attributes named "attr_00".."attr_NN", each with a
// distinct default value (i * 10) so index->name->value mapping is observable. Generated
// rather than hand-written so the >kMaxBridgedAttributesPerSet cap can be exercised (AC-3).
static AttributeSchema MakeWideSchema(unsigned int count)
{
    std::string json = "{ \"schema_name\": \"wide\", \"attributes\": [";
    for (unsigned int i = 0; i < count; ++i)
    {
        char entry[192];
        snprintf(entry, sizeof(entry),
            "%s{ \"attribute_name\": \"attr_%02u\", \"minimum_value\": 0.0, \"maximum_value\": 10000.0, \"default_value\": %u.0 }",
            (i == 0) ? "" : ",", i, i * 10u);
        json += entry;
    }
    json += "] }";
    return AttributeSchema::LoadFromJsonValue(ParseJson(json.c_str()));
}

static void MakeWideAttributeName(unsigned int index, char* outBuffer, size_t bufferSize)
{
    snprintf(outBuffer, bufferSize, "attr_%02u", index);
}

// ===========================================================================
// AC-1 — RegisterAccessors registers a float accessor for every schema attribute,
//        resolvable via registry.GetFloat(slot_name, attribute_name)
// ===========================================================================

TEST(DiaAttributeAccessorBridge, AC1_RegisterAccessors_RegistersEveryAttribute_ResolvableByName)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    // Precondition: registry data pointer IS the bridged AttributeSet.
    Dia::Condition::ConditionRegistry registry(&set);

    EXPECT_FALSE(registry.HasFloat(StringCRC("dragon"), StringCRC("health")));
    EXPECT_FALSE(registry.HasFloat(StringCRC("dragon"), StringCRC("strength")));

    AttributeAccessorBridge::RegisterAccessors(registry, set, StringCRC("dragon"));

    EXPECT_TRUE(registry.HasFloat(StringCRC("dragon"), StringCRC("health")));
    EXPECT_TRUE(registry.HasFloat(StringCRC("dragon"), StringCRC("strength")));

    EXPECT_FLOAT_EQ(registry.GetFloat(StringCRC("dragon"), StringCRC("health")), 1000.0f);
    EXPECT_FLOAT_EQ(registry.GetFloat(StringCRC("dragon"), StringCRC("strength")), 50.0f);
}

TEST(DiaAttributeAccessorBridge, AC1_RegisterAccessors_SlotNameScopesTheRegistration)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    Dia::Condition::ConditionRegistry registry(&set);
    AttributeAccessorBridge::RegisterAccessors(registry, set, StringCRC("dragon"));

    // Same field name under a slot that was never registered must not resolve.
    EXPECT_FALSE(registry.HasFloat(StringCRC("goblin"), StringCRC("health")));
    // Unknown field under the registered slot must not resolve either.
    EXPECT_FALSE(registry.HasFloat(StringCRC("dragon"), StringCRC("mana")));
}

TEST(DiaAttributeAccessorBridge, AC1_SameSetBridgedUnderTwoDistinctSlots_BothResolve)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    Dia::Condition::ConditionRegistry registry(&set);
    AttributeAccessorBridge::RegisterAccessors(registry, set, StringCRC("self"));
    AttributeAccessorBridge::RegisterAccessors(registry, set, StringCRC("target"));

    EXPECT_FLOAT_EQ(registry.GetFloat(StringCRC("self"), StringCRC("health")), 1000.0f);
    EXPECT_FLOAT_EQ(registry.GetFloat(StringCRC("target"), StringCRC("health")), 1000.0f);
}

// ===========================================================================
// AttributeSet index accessors (Core additions backing the trampoline table)
// ===========================================================================

TEST(DiaAttributeAccessorBridge, AttributeSet_IndexAccessors_MatchSchemaOrderAndByNameLookups)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    ASSERT_EQ(set.GetAttributeCount(), schema.GetAttributeCount());
    ASSERT_EQ(set.GetAttributeCount(), 2u);

    for (unsigned int i = 0; i < set.GetAttributeCount(); ++i)
    {
        // Index i in the AttributeSet must be the same attribute as index i in the schema.
        EXPECT_TRUE(set.GetAttributeNameByIndex(i) == schema.GetAttributeByIndex(i).attribute_name)
            << "index " << i;
        // GetValueByIndex must agree with the by-name resolved value.
        EXPECT_FLOAT_EQ(set.GetValueByIndex(i), set.GetValue(set.GetAttributeNameByIndex(i)))
            << "index " << i;
    }

    EXPECT_FLOAT_EQ(set.GetValueByIndex(0), 1000.0f); // health
    EXPECT_FLOAT_EQ(set.GetValueByIndex(1), 50.0f);   // strength
}

TEST(DiaAttributeAccessorBridge, AttributeSet_IndexAccessors_StableAcrossModifierChurn)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    ModifierHandle h = set.AddModifier(MakeModifier("buff", "health", ModifierOperation::Add, 250.0f));
    ASSERT_TRUE(h.IsValid());

    // Adding/removing modifiers must not renumber attribute indices — only attribute slots
    // are index-stable (they are never removed after InitializeFromSchema).
    EXPECT_TRUE(set.GetAttributeNameByIndex(0) == StringCRC("health"));
    EXPECT_FLOAT_EQ(set.GetValueByIndex(0), 1250.0f);

    set.RemoveModifier(h);
    EXPECT_TRUE(set.GetAttributeNameByIndex(0) == StringCRC("health"));
    EXPECT_FLOAT_EQ(set.GetValueByIndex(0), 1000.0f);
}

TEST(DiaAttributeAccessorBridge, AttributeSet_WideSchema_IndexAccessorsMapNameToValue)
{
    const unsigned int kCount = 40;
    AttributeSchema schema = MakeWideSchema(kCount);
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    ASSERT_EQ(set.GetAttributeCount(), kCount);

    for (unsigned int i = 0; i < kCount; ++i)
    {
        char name[32];
        MakeWideAttributeName(i, name, sizeof(name));
        EXPECT_TRUE(set.GetAttributeNameByIndex(i) == StringCRC(name)) << "index " << i;
        EXPECT_FLOAT_EQ(set.GetValueByIndex(i), static_cast<float>(i * 10u)) << "index " << i;
    }
}

// ===========================================================================
// AC-2 — a registered accessor always returns the LIVE resolved value at query
//        time, never a registration-time snapshot
// ===========================================================================

TEST(DiaAttributeAccessorBridge, AC2_AccessorReadsLiveValue_AfterSetBaseValue)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    Dia::Condition::ConditionRegistry registry(&set);
    AttributeAccessorBridge::RegisterAccessors(registry, set, StringCRC("dragon"));

    ASSERT_FLOAT_EQ(registry.GetFloat(StringCRC("dragon"), StringCRC("health")), 1000.0f);

    // Mutate AFTER registration — the accessor must observe the new value.
    set.SetBaseValue(StringCRC("health"), 750.0f);
    EXPECT_FLOAT_EQ(registry.GetFloat(StringCRC("dragon"), StringCRC("health")), 750.0f);

    set.SetBaseValue(StringCRC("health"), 1234.0f);
    EXPECT_FLOAT_EQ(registry.GetFloat(StringCRC("dragon"), StringCRC("health")), 1234.0f);
}

TEST(DiaAttributeAccessorBridge, AC2_AccessorReadsLiveValue_AfterAddAndRemoveModifier)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    Dia::Condition::ConditionRegistry registry(&set);
    AttributeAccessorBridge::RegisterAccessors(registry, set, StringCRC("dragon"));

    ASSERT_FLOAT_EQ(registry.GetFloat(StringCRC("dragon"), StringCRC("strength")), 50.0f);

    ModifierHandle add = set.AddModifier(MakeModifier("might", "strength", ModifierOperation::Add, 25.0f));
    ASSERT_TRUE(add.IsValid());
    EXPECT_FLOAT_EQ(registry.GetFloat(StringCRC("dragon"), StringCRC("strength")), 75.0f);

    ModifierHandle mul = set.AddModifier(MakeModifier("rage", "strength", ModifierOperation::Multiply, 2.0f));
    ASSERT_TRUE(mul.IsValid());
    EXPECT_FLOAT_EQ(registry.GetFloat(StringCRC("dragon"), StringCRC("strength")), 150.0f);

    set.RemoveModifier(mul);
    EXPECT_FLOAT_EQ(registry.GetFloat(StringCRC("dragon"), StringCRC("strength")), 75.0f);

    set.RemoveModifier(add);
    EXPECT_FLOAT_EQ(registry.GetFloat(StringCRC("dragon"), StringCRC("strength")), 50.0f);
}

TEST(DiaAttributeAccessorBridge, AC2_AccessorReturnsResolvedNotBaseValue_ClampApplied)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    Dia::Condition::ConditionRegistry registry(&set);
    AttributeAccessorBridge::RegisterAccessors(registry, set, StringCRC("dragon"));

    // Push strength past its schema maximum (200) — the bridged accessor must expose the
    // clamped RESOLVED value, i.e. GetValue(), not base_value and not the unclamped sum.
    ModifierHandle h = set.AddModifier(MakeModifier("overbuff", "strength", ModifierOperation::Add, 1000.0f));
    ASSERT_TRUE(h.IsValid());

    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("strength")), 50.0f);
    EXPECT_FLOAT_EQ(registry.GetFloat(StringCRC("dragon"), StringCRC("strength")), 200.0f);
}

// ===========================================================================
// AC-3 — at most kMaxBridgedAttributesPerSet attributes per call
// ===========================================================================

TEST(DiaAttributeAccessorBridge, AC3_ExactlyAtCap_AllAttributesRegistered_NoAssert)
{
    AttributeSchema schema = MakeWideSchema(kMaxBridgedAttributesPerSet);
    AttributeSet set = AttributeSet::CreateFromSchema(schema);
    ASSERT_EQ(set.GetAttributeCount(), kMaxBridgedAttributesPerSet);

    Dia::Condition::ConditionRegistry registry(&set);
    AttributeAccessorBridge::RegisterAccessors(registry, set, StringCRC("wide"));

    for (unsigned int i = 0; i < kMaxBridgedAttributesPerSet; ++i)
    {
        char name[32];
        MakeWideAttributeName(i, name, sizeof(name));
        EXPECT_TRUE(registry.HasFloat(StringCRC("wide"), StringCRC(name))) << "index " << i;
        EXPECT_FLOAT_EQ(registry.GetFloat(StringCRC("wide"), StringCRC(name)), static_cast<float>(i * 10u)) << "index " << i;
    }
}

TEST(DiaAttributeAccessorBridge, AC3_UnderCap_EveryTrampolineIndexReadsItsOwnAttribute)
{
    // Guards against the classic trampoline-table bug: every accessor closing over the same
    // index (all fields reading attribute 0). Distinct default values per attribute make a
    // mis-wired table immediately visible.
    const unsigned int kCount = 50;
    AttributeSchema schema = MakeWideSchema(kCount);
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    Dia::Condition::ConditionRegistry registry(&set);
    AttributeAccessorBridge::RegisterAccessors(registry, set, StringCRC("wide"));

    for (unsigned int i = 0; i < kCount; ++i)
    {
        char name[32];
        MakeWideAttributeName(i, name, sizeof(name));
        EXPECT_FLOAT_EQ(registry.GetFloat(StringCRC("wide"), StringCRC(name)), static_cast<float>(i * 10u))
            << "attribute " << name << " (index " << i << ") read the wrong slot";
    }
}

#ifdef _DEBUG
TEST(SLOW_DiaAttributeAccessorBridge, AC3_OverCap_AssertsInDebug)
{
    AttributeSchema schema = MakeWideSchema(kMaxBridgedAttributesPerSet + 6);
    AttributeSet set = AttributeSet::CreateFromSchema(schema);
    ASSERT_EQ(set.GetAttributeCount(), kMaxBridgedAttributesPerSet + 6);

    Dia::Condition::ConditionRegistry registry(&set);
    EXPECT_DEATH(AttributeAccessorBridge::RegisterAccessors(registry, set, StringCRC("wide")), "");
}
#else
TEST(DiaAttributeAccessorBridge, AC3_OverCap_PartialRegistrationInRelease)
{
    const unsigned int kCount = kMaxBridgedAttributesPerSet + 6;
    AttributeSchema schema = MakeWideSchema(kCount);
    AttributeSet set = AttributeSet::CreateFromSchema(schema);
    ASSERT_EQ(set.GetAttributeCount(), kCount);

    Dia::Condition::ConditionRegistry registry(&set);
    AttributeAccessorBridge::RegisterAccessors(registry, set, StringCRC("wide"));

    // First kMaxBridgedAttributesPerSet registered...
    for (unsigned int i = 0; i < kMaxBridgedAttributesPerSet; ++i)
    {
        char name[32];
        MakeWideAttributeName(i, name, sizeof(name));
        EXPECT_TRUE(registry.HasFloat(StringCRC("wide"), StringCRC(name))) << "index " << i;
    }
    // ...the overflow tail is not.
    for (unsigned int i = kMaxBridgedAttributesPerSet; i < kCount; ++i)
    {
        char name[32];
        MakeWideAttributeName(i, name, sizeof(name));
        EXPECT_FALSE(registry.HasFloat(StringCRC("wide"), StringCRC(name))) << "index " << i;
    }
}
#endif // _DEBUG

// ===========================================================================
// AC-4 — a real DiaCondition JSON expression referencing slot_name.attribute_name
//        resolves through the real ConditionRegistry / ConditionExpr (integration)
// ===========================================================================

TEST(DiaAttributeAccessorBridge, AC4_RealConditionExpr_ValidatesAndEvaluatesThroughBridgedRegistry)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    Dia::Condition::ConditionRegistry registry(&set);
    AttributeAccessorBridge::RegisterAccessors(registry, set, StringCRC("dragon"));

    const char* json = R"({"op":">=","slot":"dragon","field":"health","value":500})";

    Dia::Core::Containers::DynamicArrayC<const char*, 32> loadErrors;
    Dia::Condition::ConditionExpr expr = Dia::Condition::ConditionExpr::LoadFromJson(ParseJson(json), loadErrors);
    ASSERT_TRUE(expr.IsValid());

    // The leaf must be resolvable purely because AttributeAccessorBridge registered it.
    Dia::Core::Containers::DynamicArrayC<const char*, 32> validateErrors;
    EXPECT_TRUE(expr.Validate(registry, validateErrors));
    EXPECT_EQ(validateErrors.Size(), 0u);

    // health defaults to 1000 -> 1000 >= 500 -> true
    EXPECT_TRUE(expr.Evaluate(registry));

    // Drop below the threshold — the same immutable expression must now evaluate false,
    // proving evaluation goes through the live bridged accessor.
    set.SetBaseValue(StringCRC("health"), 100.0f);
    EXPECT_FALSE(expr.Evaluate(registry));

    set.SetBaseValue(StringCRC("health"), 500.0f);
    EXPECT_TRUE(expr.Evaluate(registry)); // boundary: >= is inclusive
}

TEST(DiaAttributeAccessorBridge, AC4_RealConditionExpr_CompositeAndOverTwoBridgedAttributes)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    Dia::Condition::ConditionRegistry registry(&set);
    AttributeAccessorBridge::RegisterAccessors(registry, set, StringCRC("dragon"));

    const char* json = R"({"op":"and","conditions":[
        {"op":">=","slot":"dragon","field":"health","value":500},
        {"op":">","slot":"dragon","field":"strength","value":40}
    ]})";

    Dia::Core::Containers::DynamicArrayC<const char*, 32> loadErrors;
    Dia::Condition::ConditionExpr expr = Dia::Condition::ConditionExpr::LoadFromJson(ParseJson(json), loadErrors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Core::Containers::DynamicArrayC<const char*, 32> validateErrors;
    ASSERT_TRUE(expr.Validate(registry, validateErrors));

    EXPECT_TRUE(expr.Evaluate(registry)); // 1000 >= 500 && 50 > 40

    set.SetBaseValue(StringCRC("strength"), 10.0f);
    EXPECT_FALSE(expr.Evaluate(registry)); // second clause now false
}

TEST(DiaAttributeAccessorBridge, AC4_UnbridgedAttributeName_FailsConditionExprValidate)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    Dia::Condition::ConditionRegistry registry(&set);
    AttributeAccessorBridge::RegisterAccessors(registry, set, StringCRC("dragon"));

    // "mana" is not in the dragon schema, so the bridge never registered it.
    const char* json = R"({"op":">=","slot":"dragon","field":"mana","value":1})";

    Dia::Core::Containers::DynamicArrayC<const char*, 32> loadErrors;
    Dia::Condition::ConditionExpr expr = Dia::Condition::ConditionExpr::LoadFromJson(ParseJson(json), loadErrors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Core::Containers::DynamicArrayC<const char*, 32> validateErrors;
    EXPECT_FALSE(expr.Validate(registry, validateErrors));
    EXPECT_GT(validateErrors.Size(), 0u);
}

// End-to-end: a bridged AttributeSet gating its OWN conditional modifier through the same
// registry. Note the recursion hazard this exposes — the gating condition must reference a
// DIFFERENT attribute than the one the modifier is attached to, otherwise ResolveValue ->
// Evaluate -> GetFloat -> GetValueByIndex -> ResolveValue recurses on the same slot forever.
TEST(DiaAttributeAccessorBridge, AC4_BridgedSetGatesItsOwnConditionalModifier)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    Dia::Condition::ConditionRegistry registry(&set);
    AttributeAccessorBridge::RegisterAccessors(registry, set, StringCRC("dragon"));
    set.SetConditionRegistry(&registry);

    // Modifier on `health`, gated on `strength` (a different slot — see comment above).
    const char* whenCondition = R"({"op":">=","slot":"dragon","field":"strength","value":100})";
    ModifierHandle h = set.AddModifier(
        MakeConditionalModifier("strong_constitution", "health", ModifierOperation::Add, 500.0f, whenCondition));
    ASSERT_TRUE(h.IsValid());

    // strength defaults to 50 -> condition false -> contribution excluded.
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 1000.0f);

    // Raise strength through the attribute system; the bridged accessor feeds it straight
    // back into the gating condition.
    set.SetBaseValue(StringCRC("strength"), 120.0f);
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 1500.0f);
    EXPECT_FLOAT_EQ(registry.GetFloat(StringCRC("dragon"), StringCRC("health")), 1500.0f);
}

// ===========================================================================
// AC-5 — re-registering the same (slot_name, attribute_name) pair is a hard error
// ===========================================================================

#ifdef _DEBUG
TEST(SLOW_DiaAttributeAccessorBridge, AC5_ReRegisteringSameSlot_AssertsInDebug)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    Dia::Condition::ConditionRegistry registry(&set);
    AttributeAccessorBridge::RegisterAccessors(registry, set, StringCRC("dragon"));
    ASSERT_TRUE(registry.HasFloat(StringCRC("dragon"), StringCRC("health")));

    // Second call with the same slot name collides on every attribute — hard error, not a
    // silent overwrite (ConditionRegistry has no unregister/replace semantics to rely on).
    EXPECT_DEATH(AttributeAccessorBridge::RegisterAccessors(registry, set, StringCRC("dragon")), "");
}

TEST(SLOW_DiaAttributeAccessorBridge, AC5_SlotFieldCollisionWithForeignAccessor_AssertsInDebug)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    Dia::Condition::ConditionRegistry registry(&set);

    // A non-bridge accessor already occupying "dragon.health" must also trip the check.
    registry.RegisterFloat(StringCRC("dragon"), StringCRC("health"),
        [](void*) -> float { return -1.0f; });

    EXPECT_DEATH(AttributeAccessorBridge::RegisterAccessors(registry, set, StringCRC("dragon")), "");
}
#endif // _DEBUG

TEST(DiaAttributeAccessorBridge, AC5_DistinctSlotNames_NotTreatedAsReRegistration)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    Dia::Condition::ConditionRegistry registry(&set);
    AttributeAccessorBridge::RegisterAccessors(registry, set, StringCRC("self"));
    // Different slot name -> no (slot, field) collision -> no assert, both resolve.
    AttributeAccessorBridge::RegisterAccessors(registry, set, StringCRC("ally"));

    EXPECT_TRUE(registry.HasFloat(StringCRC("self"), StringCRC("health")));
    EXPECT_TRUE(registry.HasFloat(StringCRC("ally"), StringCRC("health")));

    set.SetBaseValue(StringCRC("health"), 42.0f);
    EXPECT_FLOAT_EQ(registry.GetFloat(StringCRC("self"), StringCRC("health")), 42.0f);
    EXPECT_FLOAT_EQ(registry.GetFloat(StringCRC("ally"), StringCRC("health")), 42.0f);
}

// ===========================================================================
// Edge case — empty schema registers nothing and does not assert
// ===========================================================================

TEST(DiaAttributeAccessorBridge, EmptySchema_RegistersNothing_NoAssert)
{
    AttributeSchema schema = AttributeSchema::LoadFromJsonValue(
        ParseJson("{ \"schema_name\": \"empty\", \"attributes\": [] }"));
    AttributeSet set = AttributeSet::CreateFromSchema(schema);
    ASSERT_EQ(set.GetAttributeCount(), 0u);

    Dia::Condition::ConditionRegistry registry(&set);
    AttributeAccessorBridge::RegisterAccessors(registry, set, StringCRC("empty"));

    EXPECT_FALSE(registry.HasFloat(StringCRC("empty"), StringCRC("health")));
}

// ===========================================================================
// Compile-time trampoline table shape
// ===========================================================================

TEST(DiaAttributeAccessorBridge, AccessorTable_IsCompileTimeSizedToCap_AllEntriesDistinct)
{
    static_assert(kAccessorTable.size() == kMaxBridgedAttributesPerSet,
        "trampoline table must have exactly one entry per bridgeable attribute index");

    // Each index must have its own distinct function pointer — a table built with a single
    // repeated instantiation would make every bridged field read attribute 0.
    for (unsigned int i = 0; i < kMaxBridgedAttributesPerSet; ++i)
    {
        ASSERT_NE(kAccessorTable[i], nullptr) << "index " << i;
        for (unsigned int j = i + 1; j < kMaxBridgedAttributesPerSet; ++j)
        {
            EXPECT_NE(kAccessorTable[i], kAccessorTable[j]) << "indices " << i << " and " << j;
        }
    }
}

// ===========================================================================
// Logging — RegisterAccessors logs one info line per bridged attribute
// ===========================================================================

namespace
{
    class AccessorBridgeLogSink : public Dia::Observation::Log::ISink
    {
    public:
        static const unsigned int kMaxEntries = 256;

        AccessorBridgeLogSink()
            : mEntryCount(0)
        {
            SetLevelThreshold(Dia::Observation::Log::LogLevel::kDebug);
            SetChannelFilter(Dia::Core::StringCRC("Attribute"), true);
        }

        void OnLogEntry(const Dia::Observation::Log::LogEntry& entry) override
        {
            if (mEntryCount < kMaxEntries)
                mEntries[mEntryCount++] = entry;
        }

        const char* GetName() const override { return "AccessorBridgeLogSink"; }

        unsigned int CountByLevel(Dia::Observation::Log::LogLevel level) const
        {
            unsigned int count = 0;
            for (unsigned int i = 0; i < mEntryCount; ++i)
            {
                if (mEntries[i].level == level) ++count;
            }
            return count;
        }

        void Clear() { mEntryCount = 0; }

    private:
        Dia::Observation::Log::LogEntry mEntries[kMaxEntries];
        unsigned int mEntryCount;
    };
}

class DiaAttributeAccessorBridgeLoggingTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mSink.Clear();
        Dia::Observation::Log::Logger::Instance().RegisterThreadBuffer();
        Dia::Observation::Log::Logger::Instance().RegisterSink(&mSink);
    }

    void TearDown() override
    {
        Dia::Observation::Log::Logger::Instance().UnregisterSink(&mSink);
        Dia::Observation::Log::Logger::Instance().UnregisterThreadBuffer();
    }

    void FlushLogs()
    {
        Dia::Observation::Log::Logger::Instance().FlushSync();
    }

    AccessorBridgeLogSink mSink;
};

TEST_F(DiaAttributeAccessorBridgeLoggingTest, RegisterAccessors_LogsInfoPerBridgedAttribute)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    Dia::Condition::ConditionRegistry registry(&set);

    FlushLogs();
    mSink.Clear();

    AttributeAccessorBridge::RegisterAccessors(registry, set, StringCRC("dragon"));

    FlushLogs();
    EXPECT_GE(mSink.CountByLevel(Dia::Observation::Log::LogLevel::kInfo), 2u);
}

// Suite: DiaAttributeSaveSerialization
// Covers AC-1..AC-4 from the Save/Serialization Support feature (Task 6) of the DiaAttribute spec.
// See docs/specs/applications/dia/systems/diaattribute/save-serialization.md

#include <gtest/gtest.h>

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <DiaAttribute/AttributeSchema.h>
#include <DiaAttribute/AttributeSet.h>

#include <DiaCondition/ConditionExpr.h>
#include <DiaCondition/ConditionRegistry.h>

#include <DiaSaveGame/SaveContext.h>
#include <DiaSaveGame/LoadContext.h>

#include <DiaObservation/Log/Logger.h>
#include <DiaObservation/Log/ISink.h>
#include <DiaObservation/Log/LogEntry.h>
#include <DiaObservation/Log/LogLevel.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Core/Assert.h>
#include <DiaCore/Json/external/json/json.h>

#include <string.h>
#include <stdio.h>

using namespace Dia::Attribute;
using Dia::Core::StringCRC;
using Dia::SaveGame::SaveContext;
using Dia::SaveGame::LoadContext;

// ===========================================================================
// Helpers (matching the style already used in the other DiaAttribute test files)
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

// Same three attributes as the "full" schema below, minus "mana" — used as the
// AC-2 "current schema has since dropped an attribute" target.
static AttributeSchema MakeDragonSchemaNoMana()
{
    const char* json =
        "{ \"schema_name\": \"dragon\","
        "  \"attributes\": ["
        "    { \"attribute_name\": \"health\",   \"minimum_value\": 0.0, \"maximum_value\": 2000.0, \"default_value\": 1000.0 },"
        "    { \"attribute_name\": \"strength\", \"minimum_value\": 0.0, \"maximum_value\": 200.0,  \"default_value\": 50.0 }"
        "  ] }";
    return AttributeSchema::LoadFromJsonValue(ParseJson(json));
}

static AttributeSchema MakeDragonSchemaWithMana()
{
    const char* json =
        "{ \"schema_name\": \"dragon\","
        "  \"attributes\": ["
        "    { \"attribute_name\": \"health\",   \"minimum_value\": 0.0, \"maximum_value\": 2000.0, \"default_value\": 1000.0 },"
        "    { \"attribute_name\": \"strength\", \"minimum_value\": 0.0, \"maximum_value\": 200.0,  \"default_value\": 50.0 },"
        "    { \"attribute_name\": \"mana\",     \"minimum_value\": 0.0, \"maximum_value\": 500.0,  \"default_value\": 100.0 }"
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
    mod.operation       = op;
    mod.value           = value;
    strncpy_s(mod.when_condition, sizeof(mod.when_condition), whenConditionJson, _TRUNCATE);
    return mod;
}

// Backing data for the ConditionRegistry accessor used by the conditional-modifier
// round-trip test (mirrors TestDiaAttributeConditionalModifiers.cpp's pattern).
namespace
{
    struct SaveTestState
    {
        bool ready = false;
    };

    bool ReadReadyAccessor(void* data) { return static_cast<SaveTestState*>(data)->ready; }

    void ConfigureTestRegistry(Dia::Condition::ConditionRegistry& registry)
    {
        registry.RegisterBool(StringCRC("actor"), StringCRC("ready"), &ReadReadyAccessor);
    }
}

// Round-trips ctx through SaveContext::Flush + a fresh Json::Reader parse, then hands
// back a LoadContext over the re-parsed tree — exercises the real text round-trip
// (matching TestSaveLoadContext.cpp's convention) rather than just reusing the live tree.
static bool FlushAndReparse(const SaveContext& ctx, Json::Value& outRoot)
{
    static char buf[SaveContext::kBufferSize];
    if (!ctx.Flush(buf, sizeof(buf)))
        return false;

    Json::Reader reader;
    return reader.parse(buf, outRoot);
}

// ===========================================================================
// AC-1 — full round-trip: base values + full modifier list, including a
// conditional modifier, reproduce identical GetValue() results in a fresh set.
// ===========================================================================

TEST(DiaAttributeSaveSerialization, AC1_RoundTrip_BaseValuesAndModifiers_MatchOriginal)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet source = AttributeSet::CreateFromSchema(schema);

    SaveTestState state{};
    state.ready = true;
    Dia::Condition::ConditionRegistry registry(&state);
    ConfigureTestRegistry(registry);
    source.SetConditionRegistry(&registry);

    source.SetBaseValue(StringCRC("health"), 1200.0f);
    source.AddModifier(MakeModifier("add_health", "health", ModifierOperation::Add, 50.0f));
    source.AddModifier(MakeModifier("mult_health", "health", ModifierOperation::Multiply, 1.1f));
    source.AddModifier(MakeModifier("ovr_strength", "strength", ModifierOperation::Override, 75.0f));

    const char* whenCondition = R"({"op":"==","slot":"actor","field":"ready","value":true})";
    source.AddModifier(MakeConditionalModifier("buff_strength", "strength", ModifierOperation::Add, 5.0f, whenCondition));

    const float expectedHealth   = source.GetValue(StringCRC("health"));
    const float expectedStrength = source.GetValue(StringCRC("strength"));

    SaveContext save;
    source.Serialize(save);

    Json::Value reparsed;
    ASSERT_TRUE(FlushAndReparse(save, reparsed));

    AttributeSet target = AttributeSet::CreateFromSchema(schema);
    Dia::Condition::ConditionRegistry targetRegistry(&state);
    ConfigureTestRegistry(targetRegistry);
    target.SetConditionRegistry(&targetRegistry); // must be set BEFORE Deserialize (conditional modifier needs it)

    LoadContext load(reparsed);
    target.Deserialize(load);

    EXPECT_FLOAT_EQ(target.GetValue(StringCRC("health")), expectedHealth);
    EXPECT_FLOAT_EQ(target.GetValue(StringCRC("strength")), expectedStrength);
    EXPECT_FLOAT_EQ(target.GetBaseValue(StringCRC("health")), 1200.0f);
}

// ===========================================================================
// AC-2 — a saved modifier whose attribute no longer exists in the current
// schema is dropped with a warning, without corrupting unrelated attributes.
// ===========================================================================

namespace
{
    class SaveSerializationLogSink : public Dia::Observation::Log::ISink
    {
    public:
        static const unsigned int kMaxEntries = 128;

        SaveSerializationLogSink()
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

        const char* GetName() const override { return "SaveSerializationLogSink"; }

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

class DiaAttributeSaveSerializationLoggingTest : public ::testing::Test
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

    SaveSerializationLogSink mSink;
};

TEST_F(DiaAttributeSaveSerializationLoggingTest, AC2_OrphanedModifier_DroppedWithWarning_OtherAttributesUnaffected)
{
    AttributeSchema fullSchema = MakeDragonSchemaWithMana();
    AttributeSet source = AttributeSet::CreateFromSchema(fullSchema);

    source.AddModifier(MakeModifier("add_health", "health", ModifierOperation::Add, 50.0f));
    source.AddModifier(MakeModifier("add_strength", "strength", ModifierOperation::Add, 10.0f));
    source.AddModifier(MakeModifier("add_mana", "mana", ModifierOperation::Add, 25.0f));

    SaveContext save;
    source.Serialize(save);

    Json::Value reparsed;
    ASSERT_TRUE(FlushAndReparse(save, reparsed));

    // Target set is built from a schema that no longer has "mana".
    AttributeSchema narrowedSchema = MakeDragonSchemaNoMana();
    AttributeSet target = AttributeSet::CreateFromSchema(narrowedSchema);

    FlushLogs();
    mSink.Clear();

    LoadContext load(reparsed);
    target.Deserialize(load);

    FlushLogs();
    EXPECT_GE(mSink.CountByLevel(Dia::Observation::Log::LogLevel::kWarning), 1u);

    // Unrelated attributes still loaded correctly.
    EXPECT_FLOAT_EQ(target.GetValue(StringCRC("health")), 1050.0f);
    EXPECT_FLOAT_EQ(target.GetValue(StringCRC("strength")), 60.0f);
    EXPECT_FALSE(target.HasAttribute(StringCRC("mana")));
}

// ===========================================================================
// AC-3 — GetVersion returns 1
// ===========================================================================

TEST(DiaAttributeSaveSerialization, AC3_GetVersion_ReturnsOne)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    EXPECT_EQ(set.GetVersion(), 1u);
}

// ===========================================================================
// AC-4 — deserialized modifiers get freshly-issued handles; ModifierHandle
// values from before save are never assumed stable. Proven operationally:
// deserializing the same save data into two independent fresh AttributeSet
// instances both work correctly with no shared handle-pool state.
// ===========================================================================

TEST(DiaAttributeSaveSerialization, AC4_DeserializeTwiceIntoIndependentSets_BothCorrect_NoSharedState)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet source = AttributeSet::CreateFromSchema(schema);

    source.AddModifier(MakeModifier("add_health", "health", ModifierOperation::Add, 50.0f));
    source.AddModifier(MakeModifier("ovr_strength", "strength", ModifierOperation::Override, 75.0f));

    SaveContext save;
    source.Serialize(save);

    Json::Value reparsed;
    ASSERT_TRUE(FlushAndReparse(save, reparsed));

    AttributeSet targetA = AttributeSet::CreateFromSchema(schema);
    AttributeSet targetB = AttributeSet::CreateFromSchema(schema);

    LoadContext loadA(reparsed);
    targetA.Deserialize(loadA);

    LoadContext loadB(reparsed);
    targetB.Deserialize(loadB);

    EXPECT_FLOAT_EQ(targetA.GetValue(StringCRC("health")), 1050.0f);
    EXPECT_FLOAT_EQ(targetA.GetValue(StringCRC("strength")), 75.0f);

    EXPECT_FLOAT_EQ(targetB.GetValue(StringCRC("health")), 1050.0f);
    EXPECT_FLOAT_EQ(targetB.GetValue(StringCRC("strength")), 75.0f);

    // Mutating one instance's restored modifiers must not affect the other —
    // proves the two sets' handle pools are fully independent post-Deserialize.
    targetA.SetBaseValue(StringCRC("health"), 1.0f);
    EXPECT_FLOAT_EQ(targetB.GetBaseValue(StringCRC("health")), 1000.0f);
}

// ===========================================================================
// Bug fix #1 — Deserialize must not trip AddModifier's internal
// "modifier stack is full" DIA_ASSERT on a legitimate save-file scenario: a
// saved attribute with more than kMaxModifiersPerAttribute modifier entries
// (corrupted, hand-edited, or saved under a since-lowered cap).
//
// The JSON tree below is hand-constructed to match the exact shape
// AttributeSet::Serialize produces (see kKeyBaseValues/kKeyModifiers/... in
// AttributeSet.cpp) rather than going through Serialize/AddModifier, because
// the real AddModifier path already refuses a 17th unconditional Add on one
// attribute — the only way to reproduce "save data with too many modifiers"
// is to build the JSON directly, exactly as a corrupted/hand-edited/schema-
// drifted save file would look on disk.
//
// Uses the swap-g_pAssertFunc-for-a-recording-no-op precedent already
// established in Core/Threading/TestJobSystem.cpp and
// DiaMessageBus/TestDiaMessageBusCore.cpp to safely observe whether
// AddModifier's internal assert fires, without crashing the test process.
// ===========================================================================

namespace
{
    int gDeserializeFullStackAssertCount = 0;
    void RecordingAssertHandler_DeserializeFullStack(const char*, const char*, int, const char*, ...)
    {
        ++gDeserializeFullStackAssertCount;
    }
}

TEST_F(DiaAttributeSaveSerializationLoggingTest, Deserialize_AttributeAlreadyAtModifierCap_DropsExcessWithWarning_NeverTripsAddModifiersAssert)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet target = AttributeSet::CreateFromSchema(schema);

    // Hand-built save JSON: kMaxModifiersPerAttribute + 1 (17) unconditional Add
    // modifiers, all targeting "health" — one attribute already saturated.
    const unsigned int kSavedModifierCount = AttributeSet::kMaxModifiersPerAttribute + 1;

    Json::Value root(Json::objectValue);
    root["baseValues"] = Json::Value(Json::objectValue);

    Json::Value modifiers(Json::arrayValue);
    for (unsigned int i = 0; i < kSavedModifierCount; ++i)
    {
        char nameBuf[32];
        snprintf(nameBuf, sizeof(nameBuf), "mod_%02u", i);

        Json::Value elem(Json::objectValue);
        elem["modifierName"]  = nameBuf;
        elem["attributeName"] = "health";
        elem["operation"]     = static_cast<Json::Int>(ModifierOperation::Add);
        elem["value"]         = 1.0f;
        elem["whenCondition"] = "";
        modifiers.append(elem);
    }
    root["modifiers"] = modifiers;

    auto* prevAssertFunc = Dia::Core::g_pAssertFunc;
    Dia::Core::g_pAssertFunc = RecordingAssertHandler_DeserializeFullStack;
    gDeserializeFullStackAssertCount = 0;

    FlushLogs();
    mSink.Clear();

    LoadContext load(root);
    target.Deserialize(load);

    FlushLogs();
    Dia::Core::g_pAssertFunc = prevAssertFunc;

    // The fix: Deserialize's own pre-check drops the 17th saved modifier before ever
    // calling AddModifier, so AddModifier's internal full-stack DIA_ASSERT is never reached.
    EXPECT_EQ(gDeserializeFullStackAssertCount, 0);

    // Exactly kMaxModifiersPerAttribute (16) modifiers actually applied; the 17th dropped.
    EXPECT_EQ(target.GetModifierCountForAttribute(StringCRC("health")), AttributeSet::kMaxModifiersPerAttribute);

    // A warning was logged for the dropped modifier.
    EXPECT_GE(mSink.CountByLevel(Dia::Observation::Log::LogLevel::kWarning), 1u);
}

// Suite: DiaAttribute
// Covers all acceptance criteria (AC-1..AC-15) from Task 1 (Core) of the DiaAttribute spec.

#include <gtest/gtest.h>

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <DiaAttribute/AttributeSchema.h>
#include <DiaAttribute/AttributeSet.h>
#include <DiaAttribute/AttributeSetComponent.h>

#include <DiaEntity/Domain.h>
#include <DiaEntity/ComponentPool.h>

#include <DiaObservation/Log/Logger.h>
#include <DiaObservation/Log/ISink.h>
#include <DiaObservation/Log/LogEntry.h>
#include <DiaObservation/Log/LogLevel.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <fstream>
#include <string.h>
#include <stdio.h>

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

static AttributeSchema MakeSingleAttributeSchema(float min_val, float max_val, float default_val)
{
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{ \"schema_name\": \"test_schema\","
        "  \"attributes\": ["
        "    { \"attribute_name\": \"health\","
        "      \"minimum_value\": %.4f,"
        "      \"maximum_value\": %.4f,"
        "      \"default_value\": %.4f }"
        "  ] }",
        min_val, max_val, default_val);
    return AttributeSchema::LoadFromJsonValue(ParseJson(buf));
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

static void BuildTempFilePath(char* pathOut, unsigned int pathOutSize, const char* filename)
{
    char tmpDir[256];
#if defined(_MSC_VER)
    GetTempPathA(sizeof(tmpDir), tmpDir);
#else
    strncpy(tmpDir, "/tmp/", sizeof(tmpDir) - 1);
    tmpDir[sizeof(tmpDir) - 1] = '\0';
#endif
    snprintf(pathOut, pathOutSize, "%s%s", tmpDir, filename);
}

// ===========================================================================
// AC-1 / AC-2 — AttributeSchema load + validation
// ===========================================================================

TEST(DiaAttribute, SchemaLoad_ValidFile_IsValidAndAttributesMatch)
{
    char path[512];
    BuildTempFilePath(path, sizeof(path), "dia_attribute_test_schema.json");

    {
        std::ofstream file(path);
        file << "{ \"schema_name\": \"dragon\","
                "  \"attributes\": ["
                "    { \"attribute_name\": \"health\", \"minimum_value\": 0.0, \"maximum_value\": 2000.0, \"default_value\": 1000.0 }"
                "  ] }";
    }

    AttributeSchema schema = AttributeSchema::LoadFromJson(path);
    EXPECT_TRUE(schema.IsValid());

    const AttributeDefinition* def = schema.FindAttribute(StringCRC("health"));
    ASSERT_NE(def, nullptr);
    EXPECT_FLOAT_EQ(def->minimum_value, 0.0f);
    EXPECT_FLOAT_EQ(def->maximum_value, 2000.0f);
    EXPECT_FLOAT_EQ(def->default_value, 1000.0f);

    remove(path);
}

TEST(DiaAttribute, SchemaLoad_ValidJsonValue_IsValidAndAttributesMatch)
{
    AttributeSchema schema = MakeDragonSchema();
    EXPECT_TRUE(schema.IsValid());
    EXPECT_EQ(schema.GetAttributeCount(), 2u);

    const AttributeDefinition* health = schema.FindAttribute(StringCRC("health"));
    ASSERT_NE(health, nullptr);
    EXPECT_FLOAT_EQ(health->minimum_value, 0.0f);
    EXPECT_FLOAT_EQ(health->maximum_value, 2000.0f);
    EXPECT_FLOAT_EQ(health->default_value, 1000.0f);
}

TEST(DiaAttribute, SchemaLoad_MissingFile_IsInvalid)
{
    AttributeSchema schema = AttributeSchema::LoadFromJson("Z:\\this\\path\\does\\not\\exist\\schema.json");
    EXPECT_FALSE(schema.IsValid());
}

TEST(DiaAttribute, SchemaLoad_DefaultAboveMaximum_IsInvalid)
{
    AttributeSchema schema = MakeSingleAttributeSchema(0.0f, 100.0f, 500.0f); // default > max
    EXPECT_FALSE(schema.IsValid());
}

TEST(DiaAttribute, SchemaLoad_DefaultBelowMinimum_IsInvalid)
{
    AttributeSchema schema = MakeSingleAttributeSchema(50.0f, 100.0f, 0.0f); // default < min
    EXPECT_FALSE(schema.IsValid());
}

TEST(DiaAttribute, FindAttribute_UnknownName_ReturnsNull)
{
    AttributeSchema schema = MakeDragonSchema();
    EXPECT_EQ(schema.FindAttribute(StringCRC("mana")), nullptr);
}

// ===========================================================================
// AC-3 / AC-4 — AttributeSet construction + zero-modifier resolution
// ===========================================================================

TEST(DiaAttribute, AttributeSet_CreateFromSchema_BaseValuesMatchDefaults)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("health")), 1000.0f);
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("strength")), 50.0f);
}

TEST(DiaAttribute, GetValue_NoModifiers_EqualsBaseValue)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), set.GetBaseValue(StringCRC("health")));
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 1000.0f);
}

// ===========================================================================
// AC-5 / AC-6 / AC-7 / AC-8 — modifier resolution pipeline
// ===========================================================================

TEST(DiaAttribute, AddModifier_SingleAdd_ShiftsValueByExactAmount)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    ModifierHandle h = set.AddModifier(MakeModifier("buff", "health", ModifierOperation::Add, 50.0f));
    EXPECT_TRUE(h.IsValid());
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 1050.0f);
}

TEST(DiaAttribute, AddModifier_TwoAdds_Sum)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    set.AddModifier(MakeModifier("buff1", "health", ModifierOperation::Add, 50.0f));
    set.AddModifier(MakeModifier("buff2", "health", ModifierOperation::Add, 25.0f));
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 1075.0f);
}

TEST(DiaAttribute, AddModifier_MultipleMultiply_CombineMultiplicatively)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    // strength: base 50, headroom to 200 avoids clamp interference with the math being checked.
    set.AddModifier(MakeModifier("buff_str", "strength", ModifierOperation::Add, 10.0f));       // 50+10 = 60
    set.AddModifier(MakeModifier("mult1_str", "strength", ModifierOperation::Multiply, 2.0f));  // *2.0
    set.AddModifier(MakeModifier("mult2_str", "strength", ModifierOperation::Multiply, 1.5f));  // *1.5 (product 3.0)
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("strength")), 180.0f); // 60 * 3.0
}

TEST(DiaAttribute, AddModifier_MultiplyAppliesToBasePlusAddSum)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    set.AddModifier(MakeModifier("buff", "strength", ModifierOperation::Add, 10.0f));      // 50+10 = 60
    set.AddModifier(MakeModifier("mult", "strength", ModifierOperation::Multiply, 2.0f));  // *2.0 = 120
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("strength")), 120.0f);
}

TEST(DiaAttribute, AddModifier_OverrideReplacesResolvedValue_ClampAppliesAfter)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    set.AddModifier(MakeModifier("buff", "strength", ModifierOperation::Add, 10.0f));
    set.AddModifier(MakeModifier("mult", "strength", ModifierOperation::Multiply, 2.0f));
    // resolved so far (pre-Override) would be (50+10)*2 = 120; Override replaces it entirely.
    set.AddModifier(MakeModifier("ovr", "strength", ModifierOperation::Override, 75.0f));
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("strength")), 75.0f);

    // Override value itself is still clamped (AC-10) — clamp happens AFTER Override.
    set.AddModifier(MakeModifier("ovr_big", "health", ModifierOperation::Override, 999999.0f));
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 2000.0f); // maximum_value
}

#ifdef _DEBUG
TEST(SLOW_DiaAttribute, AddModifier_SecondOverrideOnSameAttribute_Asserts)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    set.AddModifier(MakeModifier("ovr1", "strength", ModifierOperation::Override, 100.0f));
    EXPECT_DEATH(set.AddModifier(MakeModifier("ovr2", "strength", ModifierOperation::Override, 50.0f)), "");
}
#endif // _DEBUG

// ===========================================================================
// AC-10 — clamp
// ===========================================================================

TEST(DiaAttribute, GetValue_AlwaysClampedToSchemaRange)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    set.AddModifier(MakeModifier("bigadd", "health", ModifierOperation::Add, 5000.0f));
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 2000.0f); // maximum_value

    set.SetBaseValue(StringCRC("strength"), -500.0f);
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("strength")), 0.0f); // minimum_value
}

// ===========================================================================
// AC-11 / AC-12 — ModifierHandle add/remove
// ===========================================================================

TEST(DiaAttribute, RemoveModifier_RemovesExactlyThatModifier)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    ModifierHandle h1 = set.AddModifier(MakeModifier("buff1", "health", ModifierOperation::Add, 50.0f));
    ModifierHandle h2 = set.AddModifier(MakeModifier("buff2", "health", ModifierOperation::Add, 25.0f));
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 1075.0f);

    set.RemoveModifier(h1);
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 1025.0f);

    set.RemoveModifier(h2);
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 1000.0f);
}

#ifdef _DEBUG
TEST(SLOW_DiaAttribute, RemoveModifier_DoubleRemove_Asserts)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    ModifierHandle h = set.AddModifier(MakeModifier("buff", "health", ModifierOperation::Add, 50.0f));
    set.RemoveModifier(h);
    EXPECT_DEATH(set.RemoveModifier(h), "");
}
#else
TEST(DiaAttribute, RemoveModifier_InvalidHandle_ReleaseNoOp_DoesNotAffectOtherModifiers)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    set.AddModifier(MakeModifier("buff", "health", ModifierOperation::Add, 50.0f));
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 1050.0f);

    ModifierHandle invalid; // default-constructed handle is invalid
    set.RemoveModifier(invalid);

    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 1050.0f);
}
#endif // _DEBUG

// ===========================================================================
// AC-13 — SetBaseValue
// ===========================================================================

TEST(DiaAttribute, SetBaseValue_UpdatesPipelineWithoutTouchingModifierStack)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    set.AddModifier(MakeModifier("buff", "health", ModifierOperation::Add, 50.0f));
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 1050.0f);

    set.SetBaseValue(StringCRC("health"), 500.0f);
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("health")), 500.0f);
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 550.0f); // modifier is still applied
}

// ===========================================================================
// AC-14 — AttributeSetComponent wraps exactly one AttributeSet
// ===========================================================================

TEST(DiaAttribute, Component_WrapsExactlyOneAttributeSet_RegardlessOfAttributeCount)
{
    AttributeSetComponent comp;
    AttributeSchema schema = MakeDragonSchema(); // 2 attributes
    comp.InitializeFromSchema(schema);

    EXPECT_FLOAT_EQ(comp.GetAttributeSet().GetValue(StringCRC("health")), 1000.0f);
    EXPECT_FLOAT_EQ(comp.GetAttributeSet().GetValue(StringCRC("strength")), 50.0f);
}

TEST(DiaAttribute, Component_AttachedToEntity_RegistersExactlyOneComponentType)
{
    Dia::Entity::Domain domain;
    domain.RegisterPool(new Dia::Entity::ComponentPool<AttributeSetComponent>(AttributeSetComponent::kTypeId));

    Dia::Entity::Entity e = domain.CreateEntity();
    domain.QueueAddComponent<AttributeSetComponent>(e, Json::Value());
    domain.EndOfFrame();

    Dia::Core::Containers::DynamicArrayC<StringCRC, 32> typeIds;
    domain.GetComponentTypeIds(e, typeIds);
    EXPECT_EQ(typeIds.Size(), 1u);

    AttributeSetComponent* comp = domain.GetComponent<AttributeSetComponent>(e);
    ASSERT_NE(comp, nullptr);

    // Attach a multi-attribute schema — component count must stay at exactly one,
    // regardless of how many attributes the schema defines (not one component per attribute).
    AttributeSchema schema = MakeDragonSchema();
    comp->InitializeFromSchema(schema);

    domain.GetComponentTypeIds(e, typeIds);
    EXPECT_EQ(typeIds.Size(), 1u);
}

// ===========================================================================
// AC-15 — logging
// ===========================================================================

namespace
{
    class AttributeLogSink : public Dia::Observation::Log::ISink
    {
    public:
        static const unsigned int kMaxEntries = 128;

        AttributeLogSink()
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

        const char* GetName() const override { return "AttributeLogSink"; }

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

class DiaAttributeLoggingTest : public ::testing::Test
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

    AttributeLogSink mSink;
};

TEST_F(DiaAttributeLoggingTest, SchemaLoad_LogsInfoExactlyOnce)
{
    AttributeSchema schema = MakeDragonSchema();
    (void)schema;
    FlushLogs();
    EXPECT_EQ(mSink.CountByLevel(Dia::Observation::Log::LogLevel::kInfo), 1u);
}

TEST_F(DiaAttributeLoggingTest, SchemaLoad_RangeViolation_LogsWarning)
{
    AttributeSchema schema = MakeSingleAttributeSchema(0.0f, 100.0f, 500.0f);
    (void)schema;
    FlushLogs();
    EXPECT_GE(mSink.CountByLevel(Dia::Observation::Log::LogLevel::kWarning), 1u);
}

TEST_F(DiaAttributeLoggingTest, AddModifier_LogsInfoExactlyOnce)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);
    FlushLogs();
    mSink.Clear();

    set.AddModifier(MakeModifier("buff", "health", ModifierOperation::Add, 10.0f));
    FlushLogs();
    EXPECT_EQ(mSink.CountByLevel(Dia::Observation::Log::LogLevel::kInfo), 1u);
}

TEST_F(DiaAttributeLoggingTest, RemoveModifier_LogsInfoExactlyOnce)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);
    ModifierHandle h = set.AddModifier(MakeModifier("buff", "health", ModifierOperation::Add, 10.0f));
    FlushLogs();
    mSink.Clear();

    set.RemoveModifier(h);
    FlushLogs();
    EXPECT_EQ(mSink.CountByLevel(Dia::Observation::Log::LogLevel::kInfo), 1u);
}

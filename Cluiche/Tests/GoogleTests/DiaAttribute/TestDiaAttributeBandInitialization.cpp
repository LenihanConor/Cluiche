// Suite: DiaAttribute
// Covers Feature 7 (Band-Based Initialization) acceptance criteria AC-1..AC-10
// from the band-based-initialization spec.

#include <gtest/gtest.h>

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <DiaAttribute/AttributeSchema.h>
#include <DiaAttribute/AttributeSet.h>
#include <DiaAttribute/Testing/AttributeTestHelpers.h>

#include <DiaObservation/Log/Logger.h>
#include <DiaObservation/Log/ISink.h>
#include <DiaObservation/Log/LogEntry.h>
#include <DiaObservation/Log/LogLevel.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <string>
#include <vector>

using namespace Dia::Attribute;
using namespace Dia::Attribute::Testing;
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

// ===========================================================================
// Tests
// ===========================================================================

// AC-1: Schema with bands field loads correctly; band names and ranges accessible via API
TEST(DiaAttributeBandInitialization, SchemaLoadsBandsCorrectly)
{
    const char* json =
        "{ \"schema_name\": \"dragon\","
        "  \"band_schema_version\": 1,"
        "  \"attributes\": ["
        "    { \"attribute_name\": \"strength\","
        "      \"minimum_value\": 0.0,"
        "      \"maximum_value\": 200.0,"
        "      \"default_value\": 50.0,"
        "      \"bands\": {"
        "        \"LOW\":    [10.0, 40.0],"
        "        \"MEDIUM\": [40.0, 110.0],"
        "        \"HIGH\":   [110.0, 160.0]"
        "      }"
        "    }"
        "  ] }";

    AttributeSchema schema = AttributeSchema::LoadFromJsonValue(ParseJson(json));

    ASSERT_TRUE(schema.IsValid());
    EXPECT_EQ(schema.GetBandSchemaVersion(), 1u);
    EXPECT_TRUE(schema.HasBands(StringCRC("strength")));
}

// AC-2: InitializeFromBands() resolves a single band to the midpoint of its range, stores as base value
TEST(DiaAttributeBandInitialization, SingleBandResolvesToMidpoint)
{
    AttributeSchema schema = MakeTestSchemaWithBands("strength", 0.0f, 200.0f,
        {{"MEDIUM", 10.0f, 20.0f}});

    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    Dia::Core::Containers::DynamicArrayC<BandValue, AttributeSet::kMaxBandValuesPerCall> bands;
    BandValue bv;
    bv.attribute_name = StringCRC("strength");
    bv.band_name = StringCRC("MEDIUM");
    bands.Add(bv);

    set.InitializeFromBands(schema, bands);

    // Midpoint of [10.0, 20.0] is 15.0
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("strength")), 15.0f);
}

// AC-3: InitializeFromBands() resolves multiple attributes from bands in one call
TEST(DiaAttributeBandInitialization, MultipleAttributesFromBands)
{
    const char* json =
        "{ \"schema_name\": \"multi\","
        "  \"attributes\": ["
        "    { \"attribute_name\": \"strength\","
        "      \"minimum_value\": 0.0, \"maximum_value\": 100.0, \"default_value\": 50.0,"
        "      \"bands\": { \"LOW\": [0.0, 30.0], \"HIGH\": [70.0, 100.0] } },"
        "    { \"attribute_name\": \"agility\","
        "      \"minimum_value\": 0.0, \"maximum_value\": 100.0, \"default_value\": 50.0,"
        "      \"bands\": { \"MEDIUM\": [40.0, 60.0] } }"
        "  ] }";

    AttributeSchema schema = AttributeSchema::LoadFromJsonValue(ParseJson(json));
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    Dia::Core::Containers::DynamicArrayC<BandValue, AttributeSet::kMaxBandValuesPerCall> bands;
    BandValue bv1;
    bv1.attribute_name = StringCRC("strength");
    bv1.band_name = StringCRC("HIGH");
    bands.Add(bv1);

    BandValue bv2;
    bv2.attribute_name = StringCRC("agility");
    bv2.band_name = StringCRC("MEDIUM");
    bands.Add(bv2);

    set.InitializeFromBands(schema, bands);

    // strength HIGH: [70, 100] -> 85.0
    // agility MEDIUM: [40, 60] -> 50.0
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("strength")), 85.0f);
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("agility")), 50.0f);
}

// AC-4: Missing band name (not in schema) falls back to default_value
TEST(DiaAttributeBandInitialization, MissingBandNameFallsBackToDefault)
{
    AttributeSchema schema = MakeTestSchemaWithBands("strength", 0.0f, 200.0f,
        {{"LOW", 0.0f, 50.0f}});

    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    Dia::Core::Containers::DynamicArrayC<BandValue, AttributeSet::kMaxBandValuesPerCall> bands;
    BandValue bv;
    bv.attribute_name = StringCRC("strength");
    bv.band_name = StringCRC("NONEXISTENT");  // Band doesn't exist in schema
    bands.Add(bv);

    set.InitializeFromBands(schema, bands);

    // Should fall back to default_value (100.0 = midpoint of [0, 200])
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("strength")), 100.0f);
}

// AC-5: Attribute with no bands field falls back to default_value
TEST(DiaAttributeBandInitialization, AttributeWithoutBandsFallsBackToDefault)
{
    const char* json =
        "{ \"schema_name\": \"test\","
        "  \"attributes\": ["
        "    { \"attribute_name\": \"health\","
        "      \"minimum_value\": 0.0,"
        "      \"maximum_value\": 100.0,"
        "      \"default_value\": 75.0 }"
        "  ] }";  // Note: no bands field

    AttributeSchema schema = AttributeSchema::LoadFromJsonValue(ParseJson(json));
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    Dia::Core::Containers::DynamicArrayC<BandValue, AttributeSet::kMaxBandValuesPerCall> bands;
    BandValue bv;
    bv.attribute_name = StringCRC("health");
    bv.band_name = StringCRC("LOW");  // Try to use band on attribute that has no bands
    bands.Add(bv);

    set.InitializeFromBands(schema, bands);

    // Should fall back to default_value
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("health")), 75.0f);
}

// AC-6: Schema validation rejects band range where min > max
TEST(DiaAttributeBandInitialization, ValidationRejectsInvertedRange)
{
    const char* json =
        "{ \"schema_name\": \"test\","
        "  \"attributes\": ["
        "    { \"attribute_name\": \"strength\","
        "      \"minimum_value\": 0.0,"
        "      \"maximum_value\": 100.0,"
        "      \"default_value\": 50.0,"
        "      \"bands\": {"
        "        \"VALID\": [10.0, 30.0],"
        "        \"INVALID\": [60.0, 40.0]"  // min > max - should be rejected
        "      }"
        "    }"
        "  ] }";

    AttributeSchema schema = AttributeSchema::LoadFromJsonValue(ParseJson(json));

    // Schema should still be valid (bands are optional)
    EXPECT_TRUE(schema.IsValid());

    // The invalid band should NOT have been added (only VALID band exists)
    // We can verify by trying to use the INVALID band - it should fall back to default
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    Dia::Core::Containers::DynamicArrayC<BandValue, AttributeSet::kMaxBandValuesPerCall> bands;
    BandValue bv;
    bv.attribute_name = StringCRC("strength");
    bv.band_name = StringCRC("INVALID");
    bands.Add(bv);

    set.InitializeFromBands(schema, bands);

    // Should fall back to default (50.0) not resolve to anything from [60, 40]
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("strength")), 50.0f);
}

// AC-7: Schema validation rejects band range outside attribute's [minimum_value, maximum_value]
TEST(DiaAttributeBandInitialization, ValidationRejectsOutOfBoundsRange)
{
    const char* json =
        "{ \"schema_name\": \"test\","
        "  \"attributes\": ["
        "    { \"attribute_name\": \"strength\","
        "      \"minimum_value\": 0.0,"
        "      \"maximum_value\": 100.0,"
        "      \"default_value\": 50.0,"
        "      \"bands\": {"
        "        \"VALID\": [20.0, 40.0],"
        "        \"OUTOFBOUNDS\": [90.0, 150.0]"  // max exceeds maximum_value
        "      }"
        "    }"
        "  ] }";

    AttributeSchema schema = AttributeSchema::LoadFromJsonValue(ParseJson(json));

    EXPECT_TRUE(schema.IsValid());

    // The out-of-bounds band should NOT have been added
    // Verify by trying to use it - should fall back to default
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    Dia::Core::Containers::DynamicArrayC<BandValue, AttributeSet::kMaxBandValuesPerCall> bands;
    BandValue bv;
    bv.attribute_name = StringCRC("strength");
    bv.band_name = StringCRC("OUTOFBOUNDS");
    bands.Add(bv);

    set.InitializeFromBands(schema, bands);

    // Should fall back to default (50.0), not resolve to 120.0 from [90, 150]
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("strength")), 50.0f);
}

// AC-8: band_schema_version field in schema is accessible via GetBandSchemaVersion()
TEST(DiaAttributeBandInitialization, BandSchemaVersionAccessible)
{
    const char* json =
        "{ \"schema_name\": \"versioned\","
        "  \"band_schema_version\": 42,"
        "  \"attributes\": ["
        "    { \"attribute_name\": \"health\","
        "      \"minimum_value\": 0.0,"
        "      \"maximum_value\": 100.0,"
        "      \"default_value\": 50.0 }"
        "  ] }";

    AttributeSchema schema = AttributeSchema::LoadFromJsonValue(ParseJson(json));

    EXPECT_EQ(schema.GetBandSchemaVersion(), 42u);
}

// AC-9: Modifiers work normally on band-initialized attributes (base value + modifier stack)
TEST(DiaAttributeBandInitialization, ModifiersWorkOnBandInitializedAttributes)
{
    AttributeSchema schema = MakeTestSchemaWithBands("strength", 0.0f, 200.0f,
        {{"MEDIUM", 10.0f, 20.0f}});

    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    Dia::Core::Containers::DynamicArrayC<BandValue, AttributeSet::kMaxBandValuesPerCall> bands;
    BandValue bv;
    bv.attribute_name = StringCRC("strength");
    bv.band_name = StringCRC("MEDIUM");
    bands.Add(bv);

    set.InitializeFromBands(schema, bands);

    // Base value is 15.0 (midpoint of [10, 20])
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("strength")), 15.0f);

    // Add a modifier
    AttributeModifier mod{};
    mod.modifier_name = StringCRC("buff");
    mod.attribute_name = StringCRC("strength");
    mod.operation = ModifierOperation::Add;
    mod.value = 10.0f;
    mod.when_condition[0] = '\0';

    set.AddModifier(mod);

    // Resolved value should be 15.0 + 10.0 = 25.0
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("strength")), 25.0f);
}

// AC-10: Empty InitializeFromBands() call leaves all attributes at default_value
TEST(DiaAttributeBandInitialization, EmptyBandMapNoOp)
{
    AttributeSchema schema = MakeTestSchemaWithBands("strength", 0.0f, 200.0f,
        {{"LOW", 0.0f, 50.0f}});

    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    // Default value is 100.0 (midpoint of [0, 200])
    float before = set.GetBaseValue(StringCRC("strength"));

    Dia::Core::Containers::DynamicArrayC<BandValue, AttributeSet::kMaxBandValuesPerCall> empty_bands;
    set.InitializeFromBands(schema, empty_bands);

    // Should remain unchanged
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("strength")), before);
}

// Additional test: Attribute not in band map uses default_value (AC-9 variant)
TEST(DiaAttributeBandInitialization, AttributeNotInBandMapUnchanged)
{
    const char* json =
        "{ \"schema_name\": \"multi\","
        "  \"attributes\": ["
        "    { \"attribute_name\": \"strength\","
        "      \"minimum_value\": 0.0, \"maximum_value\": 100.0, \"default_value\": 50.0,"
        "      \"bands\": { \"LOW\": [0.0, 30.0] } },"
        "    { \"attribute_name\": \"agility\","
        "      \"minimum_value\": 0.0, \"maximum_value\": 100.0, \"default_value\": 60.0,"
        "      \"bands\": { \"MEDIUM\": [40.0, 60.0] } }"
        "  ] }";

    AttributeSchema schema = AttributeSchema::LoadFromJsonValue(ParseJson(json));
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    // Only initialize strength, leave agility at default
    Dia::Core::Containers::DynamicArrayC<BandValue, AttributeSet::kMaxBandValuesPerCall> bands;
    BandValue bv;
    bv.attribute_name = StringCRC("strength");
    bv.band_name = StringCRC("LOW");
    bands.Add(bv);

    set.InitializeFromBands(schema, bands);

    // strength LOW: [0, 30] -> 15.0
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("strength")), 15.0f);
    // agility unchanged at default
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("agility")), 60.0f);
}

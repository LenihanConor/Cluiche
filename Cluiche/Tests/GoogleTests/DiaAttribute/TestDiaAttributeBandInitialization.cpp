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

// ===========================================================================
// Edge Cases
// ===========================================================================

// Edge: Band exactly at attribute boundaries
TEST(DiaAttributeBandInitialization, BandAtExactBoundaries)
{
    AttributeSchema schema = MakeTestSchemaWithBands("health", 0.0f, 100.0f,
        {{"MIN", 0.0f, 0.0f}, {"MAX", 100.0f, 100.0f}});

    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    // Test MIN band (degenerate range [0, 0])
    Dia::Core::Containers::DynamicArrayC<BandValue, AttributeSet::kMaxBandValuesPerCall> bands;
    BandValue bv;
    bv.attribute_name = StringCRC("health");
    bv.band_name = StringCRC("MIN");
    bands.Add(bv);

    set.InitializeFromBands(schema, bands);
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("health")), 0.0f);

    // Test MAX band (degenerate range [100, 100])
    bands[0].band_name = StringCRC("MAX");
    set.InitializeFromBands(schema, bands);
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("health")), 100.0f);
}

// Edge: Overlapping band ranges (should be allowed per spec)
TEST(DiaAttributeBandInitialization, OverlappingBandsAllowed)
{
    const char* json =
        "{ \"schema_name\": \"test\","
        "  \"attributes\": ["
        "    { \"attribute_name\": \"power\","
        "      \"minimum_value\": 0.0, \"maximum_value\": 100.0, \"default_value\": 50.0,"
        "      \"bands\": {"
        "        \"LOW\":    [0.0, 40.0],"
        "        \"MEDIUM\": [30.0, 70.0],"
        "        \"HIGH\":   [60.0, 100.0]"
        "      }"
        "    }"
        "  ] }";

    AttributeSchema schema = AttributeSchema::LoadFromJsonValue(ParseJson(json));
    EXPECT_TRUE(schema.IsValid());

    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    // All three overlapping bands should resolve independently
    Dia::Core::Containers::DynamicArrayC<BandValue, AttributeSet::kMaxBandValuesPerCall> bands;
    BandValue bv;
    bv.attribute_name = StringCRC("power");

    // LOW: [0, 40] -> 20.0
    bv.band_name = StringCRC("LOW");
    bands.Add(bv);
    set.InitializeFromBands(schema, bands);
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("power")), 20.0f);

    // MEDIUM: [30, 70] -> 50.0
    bands[0].band_name = StringCRC("MEDIUM");
    set.InitializeFromBands(schema, bands);
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("power")), 50.0f);

    // HIGH: [60, 100] -> 80.0
    bands[0].band_name = StringCRC("HIGH");
    set.InitializeFromBands(schema, bands);
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("power")), 80.0f);
}

// Edge: Large number of bands (stress test)
TEST(DiaAttributeBandInitialization, ManyBandsHandledCorrectly)
{
    // Create schema with 20 bands
    const char* json =
        "{ \"schema_name\": \"test\","
        "  \"band_schema_version\": 1,"
        "  \"attributes\": ["
        "    { \"attribute_name\": \"stat\","
        "      \"minimum_value\": 0.0, \"maximum_value\": 1000.0, \"default_value\": 500.0,"
        "      \"bands\": {"
        "        \"B0\":  [0.0, 50.0], \"B1\":  [50.0, 100.0], \"B2\":  [100.0, 150.0],"
        "        \"B3\":  [150.0, 200.0], \"B4\":  [200.0, 250.0], \"B5\":  [250.0, 300.0],"
        "        \"B6\":  [300.0, 350.0], \"B7\":  [350.0, 400.0], \"B8\":  [400.0, 450.0],"
        "        \"B9\":  [450.0, 500.0], \"B10\": [500.0, 550.0], \"B11\": [550.0, 600.0],"
        "        \"B12\": [600.0, 650.0], \"B13\": [650.0, 700.0], \"B14\": [700.0, 750.0],"
        "        \"B15\": [750.0, 800.0], \"B16\": [800.0, 850.0], \"B17\": [850.0, 900.0],"
        "        \"B18\": [900.0, 950.0], \"B19\": [950.0, 1000.0]"
        "      }"
        "    }"
        "  ] }";

    AttributeSchema schema = AttributeSchema::LoadFromJsonValue(ParseJson(json));
    EXPECT_TRUE(schema.IsValid());

    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    // Test first and last bands
    Dia::Core::Containers::DynamicArrayC<BandValue, AttributeSet::kMaxBandValuesPerCall> bands;
    BandValue bv;
    bv.attribute_name = StringCRC("stat");

    bv.band_name = StringCRC("B0");
    bands.Add(bv);
    set.InitializeFromBands(schema, bands);
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("stat")), 25.0f);  // [0, 50] -> 25

    bands[0].band_name = StringCRC("B19");
    set.InitializeFromBands(schema, bands);
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("stat")), 975.0f);  // [950, 1000] -> 975
}

// Edge: Multiple schemas with different band_schema_version
TEST(DiaAttributeBandInitialization, DifferentSchemaVersionsCoexist)
{
    const char* json_v1 =
        "{ \"schema_name\": \"dragon\","
        "  \"band_schema_version\": 1,"
        "  \"attributes\": ["
        "    { \"attribute_name\": \"strength\","
        "      \"minimum_value\": 0.0, \"maximum_value\": 100.0, \"default_value\": 50.0,"
        "      \"bands\": { \"MEDIUM\": [40.0, 60.0] }"
        "    }"
        "  ] }";

    const char* json_v2 =
        "{ \"schema_name\": \"dragon\","
        "  \"band_schema_version\": 2,"
        "  \"attributes\": ["
        "    { \"attribute_name\": \"strength\","
        "      \"minimum_value\": 0.0, \"maximum_value\": 100.0, \"default_value\": 50.0,"
        "      \"bands\": { \"MEDIUM\": [30.0, 70.0] }"
        "    }"
        "  ] }";

    AttributeSchema schema_v1 = AttributeSchema::LoadFromJsonValue(ParseJson(json_v1));
    AttributeSchema schema_v2 = AttributeSchema::LoadFromJsonValue(ParseJson(json_v2));

    EXPECT_EQ(schema_v1.GetBandSchemaVersion(), 1u);
    EXPECT_EQ(schema_v2.GetBandSchemaVersion(), 2u);

    // Create AttributeSets from different schema versions
    AttributeSet set_v1 = AttributeSet::CreateFromSchema(schema_v1);
    AttributeSet set_v2 = AttributeSet::CreateFromSchema(schema_v2);

    Dia::Core::Containers::DynamicArrayC<BandValue, AttributeSet::kMaxBandValuesPerCall> bands;
    BandValue bv;
    bv.attribute_name = StringCRC("strength");
    bv.band_name = StringCRC("MEDIUM");
    bands.Add(bv);

    // V1 MEDIUM: [40, 60] -> 50.0
    set_v1.InitializeFromBands(schema_v1, bands);
    EXPECT_FLOAT_EQ(set_v1.GetBaseValue(StringCRC("strength")), 50.0f);

    // V2 MEDIUM: [30, 70] -> 50.0
    set_v2.InitializeFromBands(schema_v2, bands);
    EXPECT_FLOAT_EQ(set_v2.GetBaseValue(StringCRC("strength")), 50.0f);

    // Same band name, different ranges both work correctly
}

// Edge: Band with very small range (precision test)
TEST(DiaAttributeBandInitialization, SmallRangePrecision)
{
    AttributeSchema schema = MakeTestSchemaWithBands("precision", 0.0f, 1.0f,
        {{"NARROW", 0.499f, 0.501f}});

    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    Dia::Core::Containers::DynamicArrayC<BandValue, AttributeSet::kMaxBandValuesPerCall> bands;
    BandValue bv;
    bv.attribute_name = StringCRC("precision");
    bv.band_name = StringCRC("NARROW");
    bands.Add(bv);

    set.InitializeFromBands(schema, bands);

    // Midpoint of [0.499, 0.501] = 0.5
    EXPECT_NEAR(set.GetBaseValue(StringCRC("precision")), 0.5f, 0.001f);
}

// Edge: Band name with special characters (StringCRC handling)
TEST(DiaAttributeBandInitialization, BandNamesWithSpecialCharacters)
{
    const char* json =
        "{ \"schema_name\": \"test\","
        "  \"attributes\": ["
        "    { \"attribute_name\": \"stat\","
        "      \"minimum_value\": 0.0, \"maximum_value\": 100.0, \"default_value\": 50.0,"
        "      \"bands\": {"
        "        \"low-tier\":    [0.0, 30.0],"
        "        \"mid_tier\":    [30.0, 70.0],"
        "        \"high.tier\":   [70.0, 100.0],"
        "        \"tier::ultra\": [90.0, 100.0]"
        "      }"
        "    }"
        "  ] }";

    AttributeSchema schema = AttributeSchema::LoadFromJsonValue(ParseJson(json));
    EXPECT_TRUE(schema.IsValid());

    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    Dia::Core::Containers::DynamicArrayC<BandValue, AttributeSet::kMaxBandValuesPerCall> bands;
    BandValue bv;
    bv.attribute_name = StringCRC("stat");

    // Test hyphen
    bv.band_name = StringCRC("low-tier");
    bands.Add(bv);
    set.InitializeFromBands(schema, bands);
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("stat")), 15.0f);

    // Test underscore
    bands[0].band_name = StringCRC("mid_tier");
    set.InitializeFromBands(schema, bands);
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("stat")), 50.0f);

    // Test dot
    bands[0].band_name = StringCRC("high.tier");
    set.InitializeFromBands(schema, bands);
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("stat")), 85.0f);

    // Test colon
    bands[0].band_name = StringCRC("tier::ultra");
    set.InitializeFromBands(schema, bands);
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("stat")), 95.0f);
}

// Edge: Zero-width range (min == max)
TEST(DiaAttributeBandInitialization, ZeroWidthRangeIsValid)
{
    AttributeSchema schema = MakeTestSchemaWithBands("health", 0.0f, 100.0f,
        {{"EXACT", 75.0f, 75.0f}});

    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    Dia::Core::Containers::DynamicArrayC<BandValue, AttributeSet::kMaxBandValuesPerCall> bands;
    BandValue bv;
    bv.attribute_name = StringCRC("health");
    bv.band_name = StringCRC("EXACT");
    bands.Add(bv);

    set.InitializeFromBands(schema, bands);

    // Midpoint of [75, 75] = 75
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("health")), 75.0f);
}

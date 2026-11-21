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

// ===========================================================================
// Tests
// ===========================================================================

// AC-1: Schema with bands field loads correctly; band names and ranges accessible via API
TEST(DiaAttributeBandInitialization, SchemaLoadsBandsCorrectly)
{
    const std::string text = MakeSidecarText({ SidecarFloat("strength", 0, 200, 50, R"(,"bands":{"LOW":[10.0,40.0],"MEDIUM":[40.0,110.0],"HIGH":[110.0,160.0]})") }, nullptr, "dragon");

    AttributeSchema schema = LoadSchemaOrDie(text.c_str());

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
    const std::string text = MakeSidecarText({ SidecarFloat("strength", 0, 100, 50, R"(,"bands":{"LOW":[0.0,30.0],"HIGH":[70.0,100.0]})"), SidecarFloat("agility", 0, 100, 50, R"(,"bands":{"MEDIUM":[40.0,60.0]})") }, nullptr, "multi");

    AttributeSchema schema = LoadSchemaOrDie(text.c_str());
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
    const std::string text = MakeSidecarText({ SidecarFloat("health", 0, 100, 75) }, nullptr, "test");

    AttributeSchema schema = LoadSchemaOrDie(text.c_str());
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

// AC-6: Schema validation rejects band range where min > max.
// Behaviour change with sidecars: the whole schema is rejected (error names the attribute) instead of the bad band being skipped.
TEST(DiaAttributeBandInitialization, ValidationRejectsInvertedRange)
{
    const std::string text = MakeSidecarText({ SidecarFloat("strength", 0, 100, 50,
        R"(,"bands":{"VALID":[10.0,30.0],"INVALID":[60.0,40.0]})") }, nullptr, "test");  // INVALID: min > max

    AttributeSchema schema;
    SchemaError err;
    EXPECT_FALSE(TryLoadSchema(text.c_str(), schema, err));
    EXPECT_FALSE(schema.IsValid());
    EXPECT_EQ(schema.FindAttribute(StringCRC("strength")), nullptr);
}

// AC-7: Schema validation rejects band range outside attribute's [minimum_value, maximum_value].
// Behaviour change with sidecars: the whole schema is rejected instead of the bad band being skipped.
TEST(DiaAttributeBandInitialization, ValidationRejectsOutOfBoundsRange)
{
    const std::string text = MakeSidecarText({ SidecarFloat("strength", 0, 100, 50,
        R"(,"bands":{"VALID":[20.0,40.0],"OUTOFBOUNDS":[90.0,150.0]})") }, nullptr, "test");  // max exceeds maximum_value

    AttributeSchema schema;
    SchemaError err;
    EXPECT_FALSE(TryLoadSchema(text.c_str(), schema, err));
    EXPECT_FALSE(schema.IsValid());
    EXPECT_STREQ(err.key.AsCStr(), "strength");
}

// AC-8: bandSchemaVersion field in schema is accessible via GetBandSchemaVersion()
TEST(DiaAttributeBandInitialization, BandSchemaVersionAccessible)
{
    const std::string text = MakeSidecarText({ SidecarFloat("health", 0.0, 100.0, 50.0, "") }, nullptr, "versioned", 42);
    AttributeSchema schema = LoadSchemaOrDie(text.c_str());

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
    const std::string text = MakeSidecarText({ SidecarFloat("strength", 0, 100, 50, R"(,"bands":{"LOW":[0.0,30.0]})"), SidecarFloat("agility", 0, 100, 60, R"(,"bands":{"MEDIUM":[40.0,60.0]})") }, nullptr, "multi");

    AttributeSchema schema = LoadSchemaOrDie(text.c_str());
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
    const std::string text = MakeSidecarText({ SidecarFloat("power", 0, 100, 50, R"(,"bands":{"LOW":[0.0,40.0],"MEDIUM":[30.0,70.0],"HIGH":[60.0,100.0]})") }, nullptr, "test");

    AttributeSchema schema = LoadSchemaOrDie(text.c_str());
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
    const std::string text = MakeSidecarText({ SidecarFloat("stat", 0, 1000, 500, R"(,"bands":{"B0":[0.0,50.0],"B1":[50.0,100.0],"B2":[100.0,150.0],"B3":[150.0,200.0],"B4":[200.0,250.0],"B5":[250.0,300.0],"B6":[300.0,350.0],"B7":[350.0,400.0],"B8":[400.0,450.0],"B9":[450.0,500.0],"B10":[500.0,550.0],"B11":[550.0,600.0],"B12":[600.0,650.0],"B13":[650.0,700.0],"B14":[700.0,750.0],"B15":[750.0,800.0],"B16":[800.0,850.0],"B17":[850.0,900.0],"B18":[900.0,950.0],"B19":[950.0,1000.0]})") }, nullptr, "test");

    AttributeSchema schema = LoadSchemaOrDie(text.c_str());
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

// Edge: Multiple schemas with different bandSchemaVersion
TEST(DiaAttributeBandInitialization, DifferentSchemaVersionsCoexist)
{
    const std::string text_v1 = MakeSidecarText(
        { SidecarFloat("strength", 0.0, 100.0, 50.0, ",\"bands\":{\"MEDIUM\":[40.0,60.0]}") }, nullptr, "dragon", 1);
    const std::string text_v2 = MakeSidecarText(
        { SidecarFloat("strength", 0.0, 100.0, 50.0, ",\"bands\":{\"MEDIUM\":[30.0,70.0]}") }, nullptr, "dragon", 2);

    AttributeSchema schema_v1 = LoadSchemaOrDie(text_v1.c_str());
    AttributeSchema schema_v2 = LoadSchemaOrDie(text_v2.c_str());

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
    const std::string text = MakeSidecarText({ SidecarFloat("stat", 0, 100, 50, R"(,"bands":{"low-tier":[0.0,30.0],"mid_tier":[30.0,70.0],"high.tier":[70.0,100.0],"tier::ultra":[90.0,100.0]})") }, nullptr, "test");

    AttributeSchema schema = LoadSchemaOrDie(text.c_str());
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

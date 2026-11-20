// Suite: DiaAttributeParamBinding
// ParamBinding<T> (parameter-files Tasks 23, 23a; AC 14, 15, 20, 21).

#include <gtest/gtest.h>

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <cstring>
#include <string>

#include <DiaAttribute/ParamBinding.h>
#include <DiaAttribute/Testing/AttributeTestHelpers.h>

#include "DiaPathfinding/GridSearchAllocCounter.h"   // the test binary's single counting operator new

using namespace Dia::Attribute;
using namespace Dia::Attribute::Testing;
using Dia::Core::StringCRC;

namespace {

    enum class Mode : uint8_t { Calm, Windy, Stormy };
    enum Wide : int32_t { WideA, WideB, WideC };

    struct Rules
    {
        float   level   = -1.0f;
        int32_t octaves = -1;
        bool    enabled = false;
        Mode    mode    = Mode::Calm;
        Wide    wide    = WideA;
        float   extra   = -1.0f;
    };

    struct MapRules
    {
        float   wet[3]   = { -1, -1, -1 };
        int32_t count[3] = { -1, -1, -1 };
        bool    flag[3]  = { true, true, true };
        Mode    mode[3]  = { Mode::Calm, Mode::Calm, Mode::Calm };
        float   scalar   = -1.0f;
    };

    struct EmptyRules {};

    AttributeModifier Mod(const char* name, const char* attr, ModifierOperation op, float v)
    {
        AttributeModifier m{};
        m.modifier_name = StringCRC(name);
        m.attribute_name = StringCRC(attr);
        m.operation = op;
        m.value = v;
        m.when_condition[0] = '\0';
        return m;
    }

    std::string RulesSidecar()
    {
        return MakeSidecarText({
            SidecarFloat("sea.level", 0, 10, 2.5),
            SidecarInt("sea.octaves", 0, 10, 4),
            SidecarBool("sea.enabled", true),
            SidecarEnum("sea.mode", { "Calm", "Windy", "Stormy" }, "Windy"),
            SidecarEnum("sea.wide", { "A", "B", "C" }, "C") });
    }

    void BindAllRules(ParamBinding<Rules>& b)
    {
        b.Bind(StringCRC("sea.level"), &Rules::level);
        b.Bind(StringCRC("sea.octaves"), &Rules::octaves);
        b.Bind(StringCRC("sea.enabled"), &Rules::enabled);
        b.BindEnum(StringCRC("sea.mode"), &Rules::mode);
        b.BindEnum(StringCRC("sea.wide"), &Rules::wide);
    }

    // Biome map columns: one float map, one int map, one bool map (all keyed by the fixture key source).
    std::string MapSidecar()
    {
        const char* ints = "{\"key\":\"biome.count\",\"kind\":\"parameter\",\"label\":\"l\",\"description\":\"d\",\"type\":\"map\","
            "\"keyRef\":\"cow.biomes/biomes[].id\",\"valueType\":\"int\",\"min\":0,\"max\":9,"
            "\"default\":{\"TemperateForest\":1,\"Desert\":2,\"Tundra\":3}}";
        const char* bools = "{\"key\":\"biome.flag\",\"kind\":\"parameter\",\"label\":\"l\",\"description\":\"d\",\"type\":\"map\","
            "\"keyRef\":\"cow.biomes/biomes[].id\",\"valueType\":\"bool\","
            "\"default\":{\"TemperateForest\":1,\"Desert\":0,\"Tundra\":1}}";
        return MakeSidecarText({
            SidecarMap("biome.wet", 0, 1, "{\"TemperateForest\":0.25,\"Desert\":0.5,\"Tundra\":0.75}"),
            ints, bools,
            SidecarFloat("biome.scalar", 0, 5, 3) });
    }

    struct Loaded
    {
        FixtureMapKeySource keys;
        AttributeSchema schema;
        AttributeSet    set;
        explicit Loaded(const std::string& text)
        {
            schema = LoadSchemaOrDie(text.c_str(), &keys);
            set.InitializeFromSchema(schema);
        }
    };
}

// ---------------------------------------------------------------------------------------------------------------
// Snapshot (AC 14)
// ---------------------------------------------------------------------------------------------------------------

TEST(DiaAttributeParamBinding, SnapshotCopiesDefaultsForEachKind)
{
    Loaded l(RulesSidecar());
    ParamBinding<Rules> b;
    BindAllRules(b);
    BindingReport report;
    ASSERT_TRUE(b.Resolve(l.schema, report));

    Rules r;
    b.Snapshot(l.set, r);
    EXPECT_FLOAT_EQ(r.level, 2.5f);
    EXPECT_EQ(r.octaves, 4);
    EXPECT_TRUE(r.enabled);
    EXPECT_EQ(r.mode, Mode::Windy);   // ordinal 1
    EXPECT_EQ(r.wide, WideC);         // ordinal 2 into an int-sized enum
}

TEST(DiaAttributeParamBinding, SnapshotCopiesResolvedValuesWithModifiers)
{
    Loaded l(RulesSidecar());
    ParamBinding<Rules> b;
    BindAllRules(b);
    BindingReport report;
    ASSERT_TRUE(b.Resolve(l.schema, report));

    EXPECT_TRUE(l.set.AddModifier(Mod("f", "sea.level", ModifierOperation::Add, 1.5f)).IsValid());
    EXPECT_TRUE(l.set.AddModifier(Mod("i", "sea.octaves", ModifierOperation::Multiply, 2.0f)).IsValid());
    EXPECT_TRUE(l.set.AddModifier(Mod("b", "sea.enabled", ModifierOperation::Override, 0.0f)).IsValid());
    EXPECT_TRUE(l.set.AddModifier(Mod("e", "sea.mode", ModifierOperation::Override, 2.0f)).IsValid());

    Rules r;
    b.Snapshot(l.set, r);
    EXPECT_FLOAT_EQ(r.level, 4.0f);          // base 2.5 + 1.5
    EXPECT_EQ(r.octaves, 8);                 // 4 * 2
    EXPECT_FALSE(r.enabled);                 // override to 0
    EXPECT_EQ(r.mode, Mode::Stormy);         // override ordinal 2
    // Base value untouched: the snapshot read the resolved value, not the base.
    EXPECT_FLOAT_EQ(l.set.GetBaseValue(StringCRC("sea.level")), 2.5f);
}

TEST(DiaAttributeParamBinding, SnapshotBoolIsExactlyZeroOrOne)
{
    Loaded l(RulesSidecar());
    ParamBinding<Rules> b;
    BindAllRules(b);
    BindingReport report;
    ASSERT_TRUE(b.Resolve(l.schema, report));

    Rules r;
    r.enabled = false;
    b.Snapshot(l.set, r);
    unsigned char raw = 0;
    std::memcpy(&raw, &r.enabled, 1);
    EXPECT_EQ(raw, 1);
}

TEST(DiaAttributeParamBinding, SnapshotAllocatesNothing)
{
    Loaded l(RulesSidecar());
    ParamBinding<Rules> b;
    BindAllRules(b);
    BindingReport report;
    ASSERT_TRUE(b.Resolve(l.schema, report));
    EXPECT_TRUE(l.set.AddModifier(Mod("f", "sea.level", ModifierOperation::Add, 1.0f)).IsValid());

    Rules r;
    {
        GridSearchTest::AllocCounter counter;
        // Positive control: the hook really counts, so a zero below is meaningful.
        int* probe = new int(1);
        EXPECT_GE(counter.Count(), 1u);
        delete probe;

        GridSearchTest::AllocCounter scoped;
        for (int i = 0; i < 100; ++i) b.Snapshot(l.set, r);
        EXPECT_EQ(scoped.Count(), 0u);
    }
    EXPECT_FLOAT_EQ(r.level, 3.5f);
}

TEST(DiaAttributeParamBinding, SnapshotReadsByIndexNotByName)
{
    // Resolve against schema A, then Snapshot a set built from schema B: same shape and order, but every
    // attribute has a DIFFERENT name. A name (hash) lookup would find nothing; reading by index still works.
    Loaded a(MakeSidecarText({ SidecarFloat("sea.level", 0, 10, 2.5), SidecarInt("sea.octaves", 0, 10, 4) }));
    Loaded other(MakeSidecarText({ SidecarFloat("zzz.one", 0, 10, 7.5), SidecarInt("zzz.two", 0, 10, 9) }));

    ParamBinding<Rules> b;
    b.Bind(StringCRC("sea.level"), &Rules::level);
    b.Bind(StringCRC("sea.octaves"), &Rules::octaves);
    BindingReport report;
    ASSERT_TRUE(b.Resolve(a.schema, report));

    Rules r;
    b.Snapshot(other.set, r);
    EXPECT_FLOAT_EQ(r.level, 7.5f);
    EXPECT_EQ(r.octaves, 9);
}

TEST(DiaAttributeParamBinding, SnapshotBeforeResolveDoesNothing)
{
    Loaded l(RulesSidecar());
    ParamBinding<Rules> b;
    BindAllRules(b);
    Rules r;
    b.Snapshot(l.set, r);
    EXPECT_FLOAT_EQ(r.level, -1.0f);
    EXPECT_EQ(r.octaves, -1);
}

// ---------------------------------------------------------------------------------------------------------------
// Resolve coverage (AC 15)
// ---------------------------------------------------------------------------------------------------------------

TEST(DiaAttributeParamBinding, ResolveReportsEveryUnboundParameter)
{
    Loaded l(RulesSidecar());
    ParamBinding<Rules> b;
    b.Bind(StringCRC("sea.level"), &Rules::level);
    b.Bind(StringCRC("sea.octaves"), &Rules::octaves);
    BindingReport report;
    EXPECT_FALSE(b.Resolve(l.schema, report));
    ASSERT_EQ(report.unboundParameters.Size(), 3u);
    EXPECT_STREQ(report.unboundParameters[0].AsCStr(), "sea.enabled");
    EXPECT_STREQ(report.unboundParameters[1].AsCStr(), "sea.mode");
    EXPECT_STREQ(report.unboundParameters[2].AsCStr(), "sea.wide");
    EXPECT_EQ(report.bindingsWithoutEntry.Size(), 0u);
}

TEST(DiaAttributeParamBinding, ResolveReportsEveryBindingWithNoEntry)
{
    Loaded l(RulesSidecar());
    ParamBinding<Rules> b;
    BindAllRules(b);
    b.Bind(StringCRC("sea.ghost1"), &Rules::extra);
    b.Bind(StringCRC("sea.ghost2"), &Rules::extra);
    BindingReport report;
    EXPECT_FALSE(b.Resolve(l.schema, report));
    ASSERT_EQ(report.bindingsWithoutEntry.Size(), 2u);
    EXPECT_STREQ(report.bindingsWithoutEntry[0].AsCStr(), "sea.ghost1");
    EXPECT_STREQ(report.bindingsWithoutEntry[1].AsCStr(), "sea.ghost2");
    EXPECT_EQ(report.unboundParameters.Size(), 0u);
}

TEST(DiaAttributeParamBinding, ResolveReportsBothDirectionsAtOnce)
{
    Loaded l(RulesSidecar());
    ParamBinding<Rules> b;
    b.Bind(StringCRC("sea.level"), &Rules::level);
    b.Bind(StringCRC("nope"), &Rules::extra);
    BindingReport report;
    EXPECT_FALSE(b.Resolve(l.schema, report));
    EXPECT_EQ(report.unboundParameters.Size(), 4u);
    EXPECT_EQ(report.bindingsWithoutEntry.Size(), 1u);
}

TEST(DiaAttributeParamBinding, ResolveCleanWhenFullyCovered)
{
    Loaded l(RulesSidecar());
    ParamBinding<Rules> b;
    BindAllRules(b);
    BindingReport report;
    EXPECT_TRUE(b.Resolve(l.schema, report));
    EXPECT_TRUE(report.IsClean());
    EXPECT_EQ(b.BindingCount(), 5u);
}

// ---------------------------------------------------------------------------------------------------------------
// Maps (AC 20, 21)
// ---------------------------------------------------------------------------------------------------------------

namespace {
    void BindAllMaps(ParamBinding<MapRules>& b)
    {
        b.BindMap(StringCRC("biome.wet"), &MapRules::wet);
        b.BindMap(StringCRC("biome.count"), &MapRules::count);
        b.BindMap(StringCRC("biome.flag"), &MapRules::flag);
        b.Bind(StringCRC("biome.scalar"), &MapRules::scalar);
    }
}

TEST(DiaAttributeParamBinding, MapSnapshotsIntoArraysInKeySourceOrder)
{
    Loaded l(MapSidecar());
    ParamBinding<MapRules> b;
    BindAllMaps(b);
    BindingReport report;
    ASSERT_TRUE(b.Resolve(l.schema, report)) << (report.unboundParameters.Size() ? report.unboundParameters[0].AsCStr() : "");

    MapRules r;
    b.Snapshot(l.set, r);
    // Key-source order: TemperateForest, Desert, Tundra.
    EXPECT_FLOAT_EQ(r.wet[0], 0.25f);
    EXPECT_FLOAT_EQ(r.wet[1], 0.5f);
    EXPECT_FLOAT_EQ(r.wet[2], 0.75f);
    EXPECT_EQ(r.count[0], 1);
    EXPECT_EQ(r.count[1], 2);
    EXPECT_EQ(r.count[2], 3);
    EXPECT_TRUE(r.flag[0]);
    EXPECT_FALSE(r.flag[1]);
    EXPECT_TRUE(r.flag[2]);
    EXPECT_FLOAT_EQ(r.scalar, 3.0f);
}

TEST(DiaAttributeParamBinding, MapSnapshotIncludesModifiersAndAllocatesNothing)
{
    Loaded l(MapSidecar());
    ParamBinding<MapRules> b;
    BindAllMaps(b);
    BindingReport report;
    ASSERT_TRUE(b.Resolve(l.schema, report));
    EXPECT_TRUE(l.set.AddModifier(Mod("m", "biome.wet.Desert", ModifierOperation::Add, 0.25f)).IsValid());

    MapRules r;
    {
        GridSearchTest::AllocCounter counter;
        b.Snapshot(l.set, r);
        EXPECT_EQ(counter.Count(), 0u);
    }
    EXPECT_FLOAT_EQ(r.wet[0], 0.25f);
    EXPECT_FLOAT_EQ(r.wet[1], 0.75f);
    EXPECT_FLOAT_EQ(r.wet[2], 0.75f);
}

TEST(DiaAttributeParamBinding, MapEnumArrayStoresOrdinals)
{
    // The enum overload over an enum-valued map: ordinals land in the enum array.
    const char* modes = "{\"key\":\"biome.mode\",\"kind\":\"parameter\",\"label\":\"l\",\"description\":\"d\",\"type\":\"map\","
        "\"keyRef\":\"cow.biomes/biomes[].id\",\"valueType\":\"enum\","
        "\"values\":[{\"id\":\"Calm\"},{\"id\":\"Windy\"},{\"id\":\"Stormy\"}],"
        "\"default\":\"Windy\"}";
    Loaded l(MakeSidecarText({ modes }));
    ParamBinding<MapRules> b;
    b.BindMap(StringCRC("biome.mode"), &MapRules::mode);
    BindingReport report;
    ASSERT_TRUE(b.Resolve(l.schema, report));
    MapRules r;
    b.Snapshot(l.set, r);
    EXPECT_EQ(r.mode[0], Mode::Windy);   // scalar enum default on a map: every column is ordinal 1
    EXPECT_EQ(r.mode[1], Mode::Windy);
    EXPECT_EQ(r.mode[2], Mode::Windy);
    EXPECT_TRUE(l.set.AddModifier(Mod("m", "biome.mode.Desert", ModifierOperation::Override, 2.0f)).IsValid());
    b.Snapshot(l.set, r);
    EXPECT_EQ(r.mode[1], Mode::Stormy);  // 2
}

TEST(DiaAttributeParamBinding, MapEnumArrayOnIntMapIsAKindMismatch)
{
    Loaded l(MapSidecar());
    ParamBinding<MapRules> b;
    b.BindMap(StringCRC("biome.count"), &MapRules::mode);   // enum array on an int map
    BindingReport report;
    EXPECT_FALSE(b.Resolve(l.schema, report));
    ASSERT_EQ(report.kindMismatches.Size(), 1u);
    EXPECT_STREQ(report.kindMismatches[0].AsCStr(), "biome.count");
}

TEST(DiaAttributeParamBinding, MapFirstElementFormCountMismatch)
{
    Loaded l(MapSidecar());
    ParamBinding<MapRules> b;
    b.BindMap(StringCRC("biome.wet"), &MapRules::scalar, 1);   // 1 bound, 3 in the key source
    BindingReport report;
    EXPECT_FALSE(b.Resolve(l.schema, report));
    ASSERT_EQ(report.countMismatches.Size(), 1u);
    EXPECT_STREQ(report.countMismatches[0].AsCStr(), "biome.wet");
}

TEST(DiaAttributeParamBinding, MapFirstElementFormSnapshots)
{
    struct Flat { float a = -1, b = -1, c = -1; };
    Loaded l(MakeSidecarText({ SidecarMap("biome.wet", 0, 1, "{\"TemperateForest\":0.25,\"Desert\":0.5,\"Tundra\":0.75}") }));
    ParamBinding<Flat> bind;
    bind.BindMap(StringCRC("biome.wet"), &Flat::a, 3);
    BindingReport report;
    ASSERT_TRUE(bind.Resolve(l.schema, report));
    Flat r;
    bind.Snapshot(l.set, r);
    EXPECT_FLOAT_EQ(r.a, 0.25f);
    EXPECT_FLOAT_EQ(r.b, 0.5f);
    EXPECT_FLOAT_EQ(r.c, 0.75f);
}

TEST(DiaAttributeParamBinding, UnboundMapColumnReportsEveryKey)
{
    Loaded l(MapSidecar());
    ParamBinding<MapRules> b;
    b.BindMap(StringCRC("biome.wet"), &MapRules::wet);
    b.BindMap(StringCRC("biome.flag"), &MapRules::flag);
    b.Bind(StringCRC("biome.scalar"), &MapRules::scalar);
    BindingReport report;
    EXPECT_FALSE(b.Resolve(l.schema, report));
    ASSERT_EQ(report.unboundParameters.Size(), 3u);
    EXPECT_STREQ(report.unboundParameters[0].AsCStr(), "biome.count.TemperateForest");
    EXPECT_STREQ(report.unboundParameters[1].AsCStr(), "biome.count.Desert");
    EXPECT_STREQ(report.unboundParameters[2].AsCStr(), "biome.count.Tundra");
}

TEST(DiaAttributeParamBinding, MapCountMismatchIsReportedAndSkipped)
{
    struct TwoWide { float wet[2] = { -1, -1 }; };
    Loaded l(MakeSidecarText({ SidecarMap("biome.wet", 0, 1, "{\"TemperateForest\":0.25,\"Desert\":0.5,\"Tundra\":0.75}") }));
    ParamBinding<TwoWide> b;
    b.BindMap(StringCRC("biome.wet"), &TwoWide::wet);   // 2 bound, 3 in the key source
    BindingReport report;
    EXPECT_FALSE(b.Resolve(l.schema, report));
    ASSERT_EQ(report.countMismatches.Size(), 1u);
    EXPECT_STREQ(report.countMismatches[0].AsCStr(), "biome.wet");
    EXPECT_EQ(report.unboundParameters.Size(), 0u);   // the columns are covered; only the count is wrong

    TwoWide r;
    b.Snapshot(l.set, r);                              // skipped: nothing written, nothing overrun
    EXPECT_FLOAT_EQ(r.wet[0], -1.0f);
    EXPECT_FLOAT_EQ(r.wet[1], -1.0f);
}

TEST(DiaAttributeParamBinding, MapBindingWithNoEntryIsReported)
{
    Loaded l(RulesSidecar());
    ParamBinding<MapRules> b;
    b.BindMap(StringCRC("biome.none"), &MapRules::wet);
    BindingReport report;
    EXPECT_FALSE(b.Resolve(l.schema, report));
    ASSERT_EQ(report.bindingsWithoutEntry.Size(), 1u);
    EXPECT_STREQ(report.bindingsWithoutEntry[0].AsCStr(), "biome.none");
}

// ---------------------------------------------------------------------------------------------------------------
// Edges
// ---------------------------------------------------------------------------------------------------------------

TEST(DiaAttributeParamBinding, EmptyStructAndSchemaAreClean)
{
    Loaded l(MakeSidecarText({}));
    ParamBinding<EmptyRules> b;
    BindingReport report;
    EXPECT_TRUE(b.Resolve(l.schema, report));
    EmptyRules r;
    b.Snapshot(l.set, r);
    EXPECT_EQ(b.BindingCount(), 0u);
}

TEST(DiaAttributeParamBinding, EmptyBindingReportsAllParametersUnbound)
{
    Loaded l(RulesSidecar());
    ParamBinding<Rules> b;
    BindingReport report;
    EXPECT_FALSE(b.Resolve(l.schema, report));
    EXPECT_EQ(report.unboundParameters.Size(), 5u);
}

TEST(DiaAttributeParamBinding, SnapshotAfterReloadWithReorderedSchemaNeedsReResolve)
{
    Loaded a(MakeSidecarText({ SidecarFloat("p.x", 0, 10, 1), SidecarFloat("p.y", 0, 10, 2) }));
    Loaded c(MakeSidecarText({ SidecarFloat("p.y", 0, 100, 20), SidecarFloat("p.x", 0, 100, 10) }));   // same keys, swapped order

    struct Pair { float x = 0; float y = 0; };
    ParamBinding<Pair> b;
    b.Bind(StringCRC("p.x"), &Pair::x);
    b.Bind(StringCRC("p.y"), &Pair::y);
    BindingReport report;
    ASSERT_TRUE(b.Resolve(a.schema, report));
    Pair r;
    b.Snapshot(a.set, r);
    EXPECT_FLOAT_EQ(r.x, 1.0f);
    EXPECT_FLOAT_EQ(r.y, 2.0f);

    ASSERT_TRUE(b.Resolve(c.schema, report));
    b.Snapshot(c.set, r);
    EXPECT_FLOAT_EQ(r.x, 10.0f);
    EXPECT_FLOAT_EQ(r.y, 20.0f);
}

TEST(DiaAttributeParamBinding, MaxBindingsFitsAndCoversTerrainRulesRowCount)
{
    static_assert(ParamBinding<Rules>::kMaxBindings >= 99, "TerrainRules has 99 rows today");
    ParamBinding<Rules> b;
    for (uint32_t i = 0; i < ParamBinding<Rules>::kMaxBindings; ++i)
    {
        const std::string key = "k" + std::to_string(i);
        b.Bind(StringCRC(key.c_str()), &Rules::extra);
    }
    EXPECT_EQ(b.BindingCount(), ParamBinding<Rules>::kMaxBindings);
}

#ifdef DEBUG
TEST(DiaAttributeParamBindingDeathTest, BindingTheSameKeyTwiceAsserts)
{
    EXPECT_DEATH({
        ParamBinding<Rules> b;
        b.Bind(StringCRC("sea.level"), &Rules::level);
        b.Bind(StringCRC("sea.level"), &Rules::extra);
    }, "");
}

TEST(DiaAttributeParamBindingDeathTest, ExceedingMaxBindingsAsserts)
{
    EXPECT_DEATH({
        ParamBinding<Rules> b;
        for (uint32_t i = 0; i <= ParamBinding<Rules>::kMaxBindings; ++i)
        {
            const std::string key = "k" + std::to_string(i);
            b.Bind(StringCRC(key.c_str()), &Rules::extra);
        }
    }, "");
}
#endif

// ---------------------------------------------------------------------------------------------------------------
// Kind check (close-out gap 1): a binding whose member kind does not match the attribute kind is reported in
// BindingReport::kindMismatches and skipped by Snapshot. BindIntFromFloat is the explicit opt-in for an int member
// on a Float attribute. (These replace the plan task 28 PinnedGap_* tests: intended behaviour change.)
// ---------------------------------------------------------------------------------------------------------------

TEST(DiaAttributeParamBinding, KindMismatch_ScalarMemberKindDifferingFromAttributeKindIsReported)
{
    Loaded l(RulesSidecar());   // sea.level float 2.5, sea.octaves int 4, sea.enabled bool true, sea.mode enum Windy(1)
    ParamBinding<Rules> b;
    b.Bind(StringCRC("sea.level"), &Rules::octaves);     // int32 member on a float attribute
    b.Bind(StringCRC("sea.octaves"), &Rules::level);     // float member on an int attribute
    b.Bind(StringCRC("sea.enabled"), &Rules::extra);     // float member on a bool attribute
    b.Bind(StringCRC("sea.wide"), &Rules::enabled);      // bool member on an enum attribute
    b.BindEnum(StringCRC("sea.mode"), &Rules::wide);     // enum member on an enum attribute: fine

    BindingReport report;
    EXPECT_FALSE(b.Resolve(l.schema, report));
    EXPECT_FALSE(report.IsClean());
    ASSERT_EQ(report.kindMismatches.Size(), 4u);
    EXPECT_STREQ(report.kindMismatches[0].AsCStr(), "sea.level");
    EXPECT_STREQ(report.kindMismatches[1].AsCStr(), "sea.octaves");
    EXPECT_STREQ(report.kindMismatches[2].AsCStr(), "sea.enabled");
    EXPECT_STREQ(report.kindMismatches[3].AsCStr(), "sea.wide");
    EXPECT_EQ(report.bindingsWithoutEntry.Size(), 0u);
    EXPECT_EQ(report.droppedKindMismatches, 0u);

    Rules r;
    b.Snapshot(l.set, r);                  // mismatched bindings are skipped; the matching one still copies
    EXPECT_EQ(r.octaves, -1);
    EXPECT_FLOAT_EQ(r.level, -1.0f);
    EXPECT_FLOAT_EQ(r.extra, -1.0f);
    EXPECT_FALSE(r.enabled);
    EXPECT_EQ(r.wide, WideB);              // sea.mode ordinal 1
}

TEST(DiaAttributeParamBinding, KindMismatch_BindEnumOnNonEnumAttributeIsReported)
{
    Loaded l(RulesSidecar());
    ParamBinding<Rules> b;
    b.BindEnum(StringCRC("sea.level"), &Rules::mode);
    BindingReport report;
    EXPECT_FALSE(b.Resolve(l.schema, report));
    ASSERT_EQ(report.kindMismatches.Size(), 1u);
    EXPECT_STREQ(report.kindMismatches[0].AsCStr(), "sea.level");
}

TEST(DiaAttributeParamBinding, KindMismatch_MapColumnKindDifferingFromArrayTypeIsReported)
{
    Loaded l(MapSidecar());
    ParamBinding<MapRules> b;
    b.BindMap(StringCRC("biome.wet"), &MapRules::count);     // int array on a float map
    b.BindMap(StringCRC("biome.count"), &MapRules::wet);     // float array on an int map
    b.BindMap(StringCRC("biome.flag"), &MapRules::flag);
    b.Bind(StringCRC("biome.scalar"), &MapRules::scalar);
    BindingReport report;
    EXPECT_FALSE(b.Resolve(l.schema, report));
    ASSERT_EQ(report.kindMismatches.Size(), 2u);
    EXPECT_STREQ(report.kindMismatches[0].AsCStr(), "biome.wet");
    EXPECT_STREQ(report.kindMismatches[1].AsCStr(), "biome.count");
    EXPECT_EQ(report.unboundParameters.Size(), 0u);          // mismatched maps still cover their columns
    MapRules r;
    b.Snapshot(l.set, r);
    EXPECT_EQ(r.count[1], -1);                               // skipped
    EXPECT_FLOAT_EQ(r.wet[2], -1.0f);
    EXPECT_FALSE(r.flag[1]);                                 // matching map copied (default {1,0,1})
}

TEST(DiaAttributeParamBinding, KindMismatch_MatchingBindingsStayClean)
{
    Loaded l(RulesSidecar());
    ParamBinding<Rules> b;
    BindAllRules(b);
    BindingReport report;
    EXPECT_TRUE(b.Resolve(l.schema, report));
    EXPECT_EQ(report.kindMismatches.Size(), 0u);
    EXPECT_TRUE(report.IsClean());
}

TEST(DiaAttributeParamBinding, BindIntFromFloat_OnFloatAttributeIsCleanAndRoundsHalfAwayFromZero)
{
    Loaded l(MakeSidecarText({ SidecarFloat("a", -10, 10, 2.5), SidecarFloat("b", -10, 10, -2.5),
                               SidecarFloat("c", -10, 10, 2.4), SidecarFloat("d", -10, 10, -2.4) }));
    struct R { int32_t a = 0, b = 0, c = 0, d = 0; };
    ParamBinding<R> bind;
    bind.BindIntFromFloat(StringCRC("a"), &R::a);
    bind.BindIntFromFloat(StringCRC("b"), &R::b);
    bind.BindIntFromFloat(StringCRC("c"), &R::c);
    bind.BindIntFromFloat(StringCRC("d"), &R::d);
    BindingReport report;
    EXPECT_TRUE(bind.Resolve(l.schema, report));
    EXPECT_TRUE(report.IsClean());
    R r;
    bind.Snapshot(l.set, r);
    EXPECT_EQ(r.a, 3);
    EXPECT_EQ(r.b, -3);
    EXPECT_EQ(r.c, 2);
    EXPECT_EQ(r.d, -2);
}

TEST(DiaAttributeParamBinding, BindIntFromFloat_OnIntAttributeIsAKindMismatch)
{
    Loaded l(RulesSidecar());
    ParamBinding<Rules> b;
    b.BindIntFromFloat(StringCRC("sea.octaves"), &Rules::octaves);
    BindingReport report;
    EXPECT_FALSE(b.Resolve(l.schema, report));
    ASSERT_EQ(report.kindMismatches.Size(), 1u);
    EXPECT_STREQ(report.kindMismatches[0].AsCStr(), "sea.octaves");
}

TEST(DiaAttributeParamBinding, BindIntFromFloat_SaturatesToInt32Range)
{
    Loaded l(MakeSidecarText({ SidecarFloat("hi", -1e12, 1e12, 1e11), SidecarFloat("lo", -1e12, 1e12, -1e11),
                               SidecarFloat("edge", -1e12, 1e12, 2147483648.0) }));
    struct R { int32_t hi = 0, lo = 0, edge = 0; };
    ParamBinding<R> bind;
    bind.BindIntFromFloat(StringCRC("hi"), &R::hi);
    bind.BindIntFromFloat(StringCRC("lo"), &R::lo);
    bind.BindIntFromFloat(StringCRC("edge"), &R::edge);
    BindingReport report;
    ASSERT_TRUE(bind.Resolve(l.schema, report));
    R r;
    bind.Snapshot(l.set, r);
    EXPECT_EQ(r.hi, INT32_MAX);
    EXPECT_EQ(r.lo, INT32_MIN);
    EXPECT_EQ(r.edge, INT32_MAX);
}

TEST(DiaAttributeParamBinding, BindIntFromFloat_SnapshotAllocatesNothing)
{
    Loaded l(MakeSidecarText({ SidecarFloat("a", 0, 10, 2.5) }));
    struct R { int32_t a = 0; };
    ParamBinding<R> bind;
    bind.BindIntFromFloat(StringCRC("a"), &R::a);
    BindingReport report;
    ASSERT_TRUE(bind.Resolve(l.schema, report));
    R r;
    GridSearchTest::AllocCounter scoped;
    for (int i = 0; i < 100; ++i) bind.Snapshot(l.set, r);
    EXPECT_EQ(scoped.Count(), 0u);
    EXPECT_EQ(r.a, 3);
}

TEST(DiaAttributeParamBinding, ScalarBindToMapKeyAndMapBindToScalarKeyAreReportedWithoutEntry)
{
    Loaded l(MapSidecar());
    ParamBinding<MapRules> b;
    b.Bind(StringCRC("biome.wet"), &MapRules::scalar);                       // scalar bind on a map column name
    b.BindMap(StringCRC("biome.scalar"), &MapRules::wet);                    // map bind on a scalar key
    BindingReport report;
    EXPECT_FALSE(b.Resolve(l.schema, report));
    EXPECT_EQ(report.bindingsWithoutEntry.Size(), 2u);
    EXPECT_GE(report.unboundParameters.Size(), 1u);
}

// Suite: DiaAttributeSchemaFromValueSchema
// Tests for AttributeSchema::FromValueSchema (parameter-files plan tasks 8-9): kinds, map expansion, constraints.

#include <gtest/gtest.h>

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <DiaAttribute/AttributeSchema.h>
#include <DiaAttribute/IMapKeySource.h>
#include <DiaAttribute/Testing/AttributeTestHelpers.h>

#include <cmath>
#include <cstring>
#include <string>

using namespace Dia::Attribute;
using namespace Dia::Attribute::Testing;
using Dia::Core::StringCRC;
using Dia::ValueSchema::ConstraintOp;
using Dia::ValueSchema::LoadResult;
using Dia::ValueSchema::ValueSchemaLoader;

namespace
{
    LoadResult Parse(const std::string& text)
    {
        LoadResult r = ValueSchemaLoader::LoadFromText(text.c_str(), "test.schema.json");
        EXPECT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
        return r;
    }

    struct Built
    {
        bool            ok = false;
        AttributeSchema schema;
        SchemaError     err;
    };

    Built Build(const LoadResult& r, const IMapKeySource* src = nullptr)
    {
        Built b;
        b.ok = AttributeSchema::FromValueSchema(r, src, b.schema, b.err);
        return b;
    }

    Built BuildText(const std::string& text, const IMapKeySource* src = nullptr)
    {
        return Build(Parse(text), src);
    }

    bool Contains(const Dia::Core::Containers::String256& s, const char* needle)
    {
        return std::strstr(s.AsCStr(), needle) != nullptr;
    }

    const char* const kAffinityObject = R"({"TemperateForest":0.5,"Desert":0.2,"Tundra":1})";

    // A key source with a configurable key list.
    class ListKeySource : public IMapKeySource
    {
    public:
        ListKeySource(const char* const* names, StringCRC* ids, uint32_t count) : mNames(names), mIds(ids), mCount(count) {}
        bool GetKeys(const char*, MapKeys& out) const override { out.ids = mIds; out.names = mNames; out.count = mCount; return true; }
    private:
        const char* const* mNames;
        StringCRC*         mIds;
        uint32_t           mCount;
    };
}

// ---------------------------------------------------------------------------
// AC 5: building from a sidecar
// ---------------------------------------------------------------------------
TEST(DiaAttributeSchemaFromValueSchema, OneDefinitionPerParameterInFileOrder)
{
    const Built b = BuildText(MakeSidecarText({
        SidecarFloat("zeta.rate", 0, 10, 2.5),
        SidecarMetric("some.metric"),
        SidecarInt("alpha.count", 1, 9, 4),
        SidecarBool("mid.flag", true),
        SidecarEnum("beta.mode", { "Low", "Mid", "High" }, "Mid") }));
    ASSERT_TRUE(b.ok) << b.err.key.AsCStr() << ": " << b.err.reason.AsCStr();
    ASSERT_EQ(4u, b.schema.GetAttributeCount());
    EXPECT_TRUE(b.schema.IsValid());
    EXPECT_EQ(StringCRC("test.params"), b.schema.GetSchemaName());

    EXPECT_EQ(StringCRC("zeta.rate"),   b.schema.GetAttributeByIndex(0).attribute_name);
    EXPECT_EQ(StringCRC("alpha.count"), b.schema.GetAttributeByIndex(1).attribute_name);
    EXPECT_EQ(StringCRC("mid.flag"),    b.schema.GetAttributeByIndex(2).attribute_name);
    EXPECT_EQ(StringCRC("beta.mode"),   b.schema.GetAttributeByIndex(3).attribute_name);

    const AttributeDefinition& f = b.schema.GetAttributeByIndex(0);
    EXPECT_EQ(AttributeKind::Float, f.kind);
    EXPECT_FLOAT_EQ(0.0f, f.minimum_value);
    EXPECT_FLOAT_EQ(10.0f, f.maximum_value);
    EXPECT_FLOAT_EQ(2.5f, f.default_value);
    EXPECT_STREQ("zeta.rate", f.fullKey.AsCStr());

    const AttributeDefinition& i = b.schema.GetAttributeByIndex(1);
    EXPECT_EQ(AttributeKind::Int, i.kind);
    EXPECT_FLOAT_EQ(4.0f, i.default_value);

    const AttributeDefinition& bo = b.schema.GetAttributeByIndex(2);
    EXPECT_EQ(AttributeKind::Bool, bo.kind);
    EXPECT_FLOAT_EQ(0.0f, bo.minimum_value);
    EXPECT_FLOAT_EQ(1.0f, bo.maximum_value);
    EXPECT_FLOAT_EQ(1.0f, bo.default_value);

    EXPECT_EQ(AttributeKind::Enum, b.schema.GetAttributeByIndex(3).kind);
    EXPECT_NE(nullptr, b.schema.FindAttribute(StringCRC("alpha.count")));
    EXPECT_EQ(nullptr, b.schema.FindAttribute(StringCRC("some.metric")));
}

TEST(DiaAttributeSchemaFromValueSchema, EnumDefaultIsOrdinalAndIdsKept)
{
    const Built b = BuildText(MakeSidecarText({ SidecarEnum("mode", { "Low", "Mid", "High" }, "High") }));
    ASSERT_TRUE(b.ok);
    const AttributeDefinition& d = b.schema.GetAttributeByIndex(0);
    EXPECT_FLOAT_EQ(2.0f, d.default_value);
    EXPECT_FLOAT_EQ(0.0f, d.minimum_value);
    EXPECT_FLOAT_EQ(2.0f, d.maximum_value);
    ASSERT_EQ(3u, d.enumIdCount);
    EXPECT_STREQ("Low", b.schema.GetEnumId(0, 0).AsCStr());
    EXPECT_STREQ("Mid", b.schema.GetEnumId(0, 1).AsCStr());
    EXPECT_STREQ("High", b.schema.GetEnumId(0, 2).AsCStr());
}

TEST(DiaAttributeSchemaFromValueSchema, TwoEnumsKeepSeparateIdRanges)
{
    const Built b = BuildText(MakeSidecarText({ SidecarEnum("e1", { "A", "B" }, "A"), SidecarEnum("e2", { "X", "Y", "Z" }, "Z") }));
    ASSERT_TRUE(b.ok);
    EXPECT_EQ(2u, b.schema.GetAttributeByIndex(0).enumIdCount);
    EXPECT_EQ(3u, b.schema.GetAttributeByIndex(1).enumIdCount);
    EXPECT_STREQ("B", b.schema.GetEnumId(0, 1).AsCStr());
    EXPECT_STREQ("X", b.schema.GetEnumId(1, 0).AsCStr());
    EXPECT_FLOAT_EQ(2.0f, b.schema.GetAttributeByIndex(1).default_value);
}

TEST(DiaAttributeSchemaFromValueSchema, MissingDefaultFailsNamingKey)
{
    LoadResult r = Parse(MakeSidecarText({ SidecarFloat("ok.one", 0, 1, 0.5), SidecarFloat("no.default", 0, 1, 0.5) }));
    r.mEntries[1].mHasDefault = false;
    const Built b = Build(r);
    EXPECT_FALSE(b.ok);
    EXPECT_STREQ("no.default", b.err.key.AsCStr());
    EXPECT_GT(std::strlen(b.err.reason.AsCStr()), 0u);
}

TEST(DiaAttributeSchemaFromValueSchema, FailedBuildLeavesOutUnchanged)
{
    LoadResult r = Parse(MakeSidecarText({ SidecarFloat("a", 0, 1, 0.5) }));
    AttributeSchema out;
    SchemaError err;
    ASSERT_TRUE(AttributeSchema::FromValueSchema(r, nullptr, out, err));
    r.mEntries[0].mHasDefault = false;
    EXPECT_FALSE(AttributeSchema::FromValueSchema(r, nullptr, out, err));
    EXPECT_EQ(1u, out.GetAttributeCount());
}

TEST(DiaAttributeSchemaFromValueSchema, DefaultOutOfRangeFails)
{
    LoadResult r = Parse(MakeSidecarText({ SidecarFloat("a.high", 0, 1, 0.5), SidecarFloat("a.low", 0, 1, 0.5) }));
    r.mEntries[0].mDefaultNumber = 1.5;
    Built b = Build(r);
    EXPECT_FALSE(b.ok);
    EXPECT_STREQ("a.high", b.err.key.AsCStr());

    r.mEntries[0].mDefaultNumber = 0.5;
    r.mEntries[1].mDefaultNumber = -0.1;
    b = Build(r);
    EXPECT_FALSE(b.ok);
    EXPECT_STREQ("a.low", b.err.key.AsCStr());
}

TEST(DiaAttributeSchemaFromValueSchema, DefaultWrongKindFails)
{
    {   // int with a fractional default
        LoadResult r = Parse(MakeSidecarText({ SidecarInt("i", 0, 10, 3) }));
        r.mEntries[0].mDefaultNumber = 2.5;
        const Built b = Build(r);
        EXPECT_FALSE(b.ok);
        EXPECT_STREQ("i", b.err.key.AsCStr());
    }
    {   // unknown enum id
        LoadResult r = Parse(MakeSidecarText({ SidecarEnum("e", { "A", "B" }, "A") }));
        Dia::ValueSchema::SetText(r.mEntries[0].mDefaultString, "Nope");
        const Built b = Build(r);
        EXPECT_FALSE(b.ok);
        EXPECT_STREQ("e", b.err.key.AsCStr());
    }
    {   // bool not 0/1
        LoadResult r = Parse(MakeSidecarText({ SidecarBool("flag", false) }));
        r.mEntries[0].mDefaultNumber = 2.0;
        const Built b = Build(r);
        EXPECT_FALSE(b.ok);
        EXPECT_STREQ("flag", b.err.key.AsCStr());
    }
}

TEST(DiaAttributeSchemaFromValueSchema, OtherKindsAreIgnored)
{
    const Built b = BuildText(MakeSidecarText({ SidecarMetric("m.one"), SidecarMetric("m.two") }));
    ASSERT_TRUE(b.ok);
    EXPECT_EQ(0u, b.schema.GetAttributeCount());
}

TEST(DiaAttributeSchemaFromValueSchema, BandSchemaVersionIsSidecarVersion)
{
    const Built b = BuildText(MakeSidecarText({ SidecarFloat("a", 0, 1, 0.5) }));
    ASSERT_TRUE(b.ok);
    EXPECT_EQ(1u, b.schema.GetBandSchemaVersion());
}

TEST(DiaAttributeSchemaFromValueSchema, BandsAreCopiedAndResolve)
{
    const Built b = BuildText(MakeSidecarText({
        SidecarFloat("p", 0, 1, 0.5, R"(,"bands":{"LOW":[0.0,0.3],"HIGH":[0.7,1.0]})") }));
    ASSERT_TRUE(b.ok) << b.err.reason.AsCStr();
    EXPECT_TRUE(b.schema.HasBands(StringCRC("p")));
    EXPECT_FLOAT_EQ(0.15f, b.schema.ResolveBand(StringCRC("p"), StringCRC("LOW")));
    EXPECT_FLOAT_EQ(0.85f, b.schema.ResolveBand(StringCRC("p"), StringCRC("HIGH")));
    const BandRange* hi = b.schema.FindAttribute(StringCRC("p"))->bands.TryGetItemConst(StringCRC("HIGH"));
    ASSERT_NE(nullptr, hi);
    EXPECT_FLOAT_EQ(0.7f, hi->min);
    EXPECT_FLOAT_EQ(1.0f, hi->max);
}

TEST(DiaAttributeSchemaFromValueSchema, NoBandsMeansHasBandsFalse)
{
    const Built b = BuildText(MakeSidecarText({ SidecarFloat("p", 0, 1, 0.5) }));
    ASSERT_TRUE(b.ok);
    EXPECT_FALSE(b.schema.HasBands(StringCRC("p")));
}

TEST(DiaAttributeSchemaFromValueSchema, BandOutsideRangeFails)
{
    LoadResult r = Parse(MakeSidecarText({ SidecarFloat("p", 0, 1, 0.5, R"(,"bands":{"LOW":[0.0,0.3]})") }));
    r.mEntries[0].mBands[0].mHi = 5.0;
    Built b = Build(r);
    EXPECT_FALSE(b.ok);
    EXPECT_STREQ("p", b.err.key.AsCStr());

    r.mEntries[0].mBands[0].mHi = 0.3;
    r.mEntries[0].mBands[0].mLo = -0.5;
    b = Build(r);
    EXPECT_FALSE(b.ok);
    EXPECT_STREQ("p", b.err.key.AsCStr());
}

TEST(DiaAttributeSchemaFromValueSchema, BandLoGreaterThanHiFails)
{
    LoadResult r = Parse(MakeSidecarText({ SidecarFloat("p", 0, 1, 0.5, R"(,"bands":{"LOW":[0.0,0.3]})") }));
    r.mEntries[0].mBands[0].mLo = 0.4;
    const Built b = Build(r);
    EXPECT_FALSE(b.ok);
    EXPECT_STREQ("p", b.err.key.AsCStr());
}

// Edge cases
TEST(DiaAttributeSchemaFromValueSchema, EmptySidecarBuildsEmptyValidSchema)
{
    const Built b = BuildText(MakeSidecarText({}));
    ASSERT_TRUE(b.ok);
    EXPECT_EQ(0u, b.schema.GetAttributeCount());
    EXPECT_TRUE(b.schema.IsValid());
    EXPECT_EQ(0u, b.schema.GetConstraintCount());
}

TEST(DiaAttributeSchemaFromValueSchema, IntBoundsAreAcceptedAtTheEdges)
{
    const Built b = BuildText(MakeSidecarText({ SidecarInt("lo", 0, 10, 0), SidecarInt("hi", 0, 10, 10), SidecarInt("neg", -5, 5, -5) }));
    ASSERT_TRUE(b.ok);
    EXPECT_FLOAT_EQ(0.0f, b.schema.GetAttributeByIndex(0).default_value);
    EXPECT_FLOAT_EQ(10.0f, b.schema.GetAttributeByIndex(1).default_value);
    EXPECT_FLOAT_EQ(-5.0f, b.schema.GetAttributeByIndex(2).default_value);
}

TEST(DiaAttributeSchemaFromValueSchema, EnumWithOneValue)
{
    const Built b = BuildText(MakeSidecarText({ SidecarEnum("e", { "Only" }, "Only") }));
    ASSERT_TRUE(b.ok);
    const AttributeDefinition& d = b.schema.GetAttributeByIndex(0);
    EXPECT_FLOAT_EQ(0.0f, d.default_value);
    EXPECT_FLOAT_EQ(0.0f, d.minimum_value);
    EXPECT_FLOAT_EQ(0.0f, d.maximum_value);
    EXPECT_EQ(1u, d.enumIdCount);
    EXPECT_STREQ("Only", b.schema.GetEnumId(0, 0).AsCStr());
}

TEST(DiaAttributeSchemaFromValueSchema, BoolDefaultTrueIsOne)
{
    const Built b = BuildText(MakeSidecarText({ SidecarBool("f", true), SidecarBool("g", false) }));
    ASSERT_TRUE(b.ok);
    EXPECT_FLOAT_EQ(1.0f, b.schema.GetAttributeByIndex(0).default_value);
    EXPECT_FLOAT_EQ(0.0f, b.schema.GetAttributeByIndex(1).default_value);
}

TEST(DiaAttributeSchemaFromValueSchema, MinEqualsMaxIsAccepted)
{
    const Built b = BuildText(MakeSidecarText({ SidecarFloat("fixed", 3, 3, 3) }));
    ASSERT_TRUE(b.ok);
    EXPECT_FLOAT_EQ(3.0f, b.schema.GetAttributeByIndex(0).default_value);
}

TEST(DiaAttributeSchemaFromValueSchema, NegativeDefaults)
{
    const Built b = BuildText(MakeSidecarText({ SidecarFloat("n", -10, -1, -2.5) }));
    ASSERT_TRUE(b.ok);
    EXPECT_FLOAT_EQ(-2.5f, b.schema.GetAttributeByIndex(0).default_value);
    EXPECT_FLOAT_EQ(-10.0f, b.schema.GetAttributeByIndex(0).minimum_value);
}

TEST(DiaAttributeSchemaFromValueSchema, NegativeZeroDefaultIsPreserved)
{
    const Built b = BuildText(MakeSidecarText({ SidecarFloat("z", -1, 1, -0.0) }));
    ASSERT_TRUE(b.ok);
    const float v = b.schema.GetAttributeByIndex(0).default_value;
    EXPECT_EQ(0.0f, v);
    EXPECT_TRUE(std::signbit(v));
}

TEST(DiaAttributeSchemaFromValueSchema, CopiedSchemaKeepsEnumIdsBandsAndConstraints)
{
    const Built b = BuildText(MakeSidecarText({
        SidecarFloat("x", 0, 1, 0.2, R"(,"bands":{"LOW":[0.0,0.3]})"), SidecarFloat("y", 0, 1, 0.8),
        SidecarEnum("e", { "A", "B" }, "B") }, R"([{"a":"x","op":"<","b":"y"}])"));
    ASSERT_TRUE(b.ok);
    const AttributeSchema copy(b.schema);
    AttributeSchema assigned;
    assigned = b.schema;
    const AttributeSchema* const both[2] = { &copy, &assigned };
    for (const AttributeSchema* s : both)
    {
        ASSERT_EQ(3u, s->GetAttributeCount());
        EXPECT_TRUE(s->HasBands(StringCRC("x")));
        EXPECT_EQ(2u, s->GetAttributeByIndex(2).enumIdCount);
        EXPECT_STREQ("B", s->GetEnumId(2, 1).AsCStr());
        EXPECT_EQ(1u, s->GetConstraintCount());
        EXPECT_EQ(1u, s->GetBandSchemaVersion());
    }
}

// ---------------------------------------------------------------------------
// AC 6: map entries
// ---------------------------------------------------------------------------
TEST(DiaAttributeSchemaFromValueSchema, MapExpandsInKeySourceOrderWithFullKeys)
{
    FixtureMapKeySource src;
    const Built b = BuildText(MakeSidecarText({
        SidecarFloat("before", 0, 1, 0.1), SidecarMap("affinity", 0, 1, kAffinityObject), SidecarFloat("after", 0, 1, 0.9) }), &src);
    ASSERT_TRUE(b.ok) << b.err.key.AsCStr() << ": " << b.err.reason.AsCStr();
    ASSERT_EQ(5u, b.schema.GetAttributeCount());
    EXPECT_STREQ("before", b.schema.GetAttributeByIndex(0).fullKey.AsCStr());
    EXPECT_STREQ("affinity.TemperateForest", b.schema.GetAttributeByIndex(1).fullKey.AsCStr());
    EXPECT_STREQ("affinity.Desert", b.schema.GetAttributeByIndex(2).fullKey.AsCStr());
    EXPECT_STREQ("affinity.Tundra", b.schema.GetAttributeByIndex(3).fullKey.AsCStr());
    EXPECT_STREQ("after", b.schema.GetAttributeByIndex(4).fullKey.AsCStr());
    EXPECT_EQ(StringCRC("affinity.Desert"), b.schema.GetAttributeByIndex(2).attribute_name);
    EXPECT_FLOAT_EQ(0.5f, b.schema.GetAttributeByIndex(1).default_value);
    EXPECT_FLOAT_EQ(0.2f, b.schema.GetAttributeByIndex(2).default_value);
    EXPECT_FLOAT_EQ(1.0f, b.schema.GetAttributeByIndex(3).default_value);
    EXPECT_EQ(AttributeKind::Float, b.schema.GetAttributeByIndex(3).kind);
    EXPECT_FLOAT_EQ(1.0f, b.schema.GetAttributeByIndex(3).maximum_value);
}

TEST(DiaAttributeSchemaFromValueSchema, MapObjectKeyOrderDoesNotChangeSchemaOrder)
{
    FixtureMapKeySource src;
    const Built b = BuildText(MakeSidecarText({ SidecarMap("m", 0, 1, R"({"Tundra":0.3,"TemperateForest":0.1,"Desert":0.2})") }), &src);
    ASSERT_TRUE(b.ok);
    EXPECT_STREQ("m.TemperateForest", b.schema.GetAttributeByIndex(0).fullKey.AsCStr());
    EXPECT_FLOAT_EQ(0.1f, b.schema.GetAttributeByIndex(0).default_value);
    EXPECT_FLOAT_EQ(0.3f, b.schema.GetAttributeByIndex(2).default_value);
}

TEST(DiaAttributeSchemaFromValueSchema, MapScalarDefaultAppliesToEveryKey)
{
    FixtureMapKeySource src;
    const Built b = BuildText(MakeSidecarText({ SidecarMap("m", 0, 1, "0.25") }), &src);
    ASSERT_TRUE(b.ok) << b.err.reason.AsCStr();
    ASSERT_EQ(3u, b.schema.GetAttributeCount());
    for (unsigned i = 0; i < 3; ++i) EXPECT_FLOAT_EQ(0.25f, b.schema.GetAttributeByIndex(i).default_value);
}

TEST(DiaAttributeSchemaFromValueSchema, MapMissingKeyInObjectDefaultFails)
{
    FixtureMapKeySource src;
    const Built b = BuildText(MakeSidecarText({ SidecarMap("affinity", 0, 1, R"({"Desert":0.2,"Tundra":1})") }), &src);
    EXPECT_FALSE(b.ok);
    EXPECT_STREQ("affinity.TemperateForest", b.err.key.AsCStr());
}

TEST(DiaAttributeSchemaFromValueSchema, MapExtraKeyNotInKeySourceFails)
{
    FixtureMapKeySource src;
    const Built b = BuildText(MakeSidecarText({
        SidecarMap("affinity", 0, 1, R"({"TemperateForest":0.5,"Desert":0.2,"Tundra":1,"Swamp":0.4})") }), &src);
    EXPECT_FALSE(b.ok);
    EXPECT_STREQ("affinity.Swamp", b.err.key.AsCStr());
}

TEST(DiaAttributeSchemaFromValueSchema, MapWithNoKeySourceFails)
{
    const Built b = BuildText(MakeSidecarText({ SidecarMap("affinity", 0, 1, kAffinityObject) }), nullptr);
    EXPECT_FALSE(b.ok);
    EXPECT_STREQ("affinity", b.err.key.AsCStr());
}

TEST(DiaAttributeSchemaFromValueSchema, MapWithUnknownKeyRefFails)
{
    FixtureMapKeySource src;
    LoadResult r = Parse(MakeSidecarText({ SidecarMap("affinity", 0, 1, kAffinityObject) }));
    Dia::ValueSchema::SetText(r.mEntries[0].mMapKeyRef, "other.ref");
    const Built b = Build(r, &src);
    EXPECT_FALSE(b.ok);
    EXPECT_STREQ("affinity", b.err.key.AsCStr());
}

TEST(DiaAttributeSchemaFromValueSchema, MapWithoutAnyDefaultFails)
{
    FixtureMapKeySource src;
    const Built b = BuildText(MakeSidecarText({ SidecarMap("affinity", 0, 1, nullptr) }), &src);
    EXPECT_FALSE(b.ok);
    EXPECT_STREQ("affinity", b.err.key.AsCStr());
}

TEST(DiaAttributeSchemaFromValueSchema, MapDefaultValueOutOfRangeNamesTheColumn)
{
    FixtureMapKeySource src;
    LoadResult r = Parse(MakeSidecarText({ SidecarMap("affinity", 0, 1, kAffinityObject) }));
    for (unsigned i = 0; i < r.mEntries[0].mMapDefaults.Size(); ++i)
    {
        if (std::strcmp(r.mEntries[0].mMapDefaults[i].mMapKey.AsCStr(), "Desert") == 0) r.mEntries[0].mMapDefaults[i].mNumber = 7.0;
    }
    const Built b = Build(r, &src);
    EXPECT_FALSE(b.ok);
    EXPECT_STREQ("affinity.Desert", b.err.key.AsCStr());
}

// ---- enum-valued map with an object default (close-out gap 5): mapKey -> enum id -> ordinal --------------------------
TEST(DiaAttributeSchemaFromValueSchema, EnumMapObjectDefaultExpandsInKeySourceOrderWithOrdinals)
{
    FixtureMapKeySource src;
    // Object order differs from the key-source order (TemperateForest, Desert, Tundra): the key source decides.
    const Built b = BuildText(MakeSidecarText({
        SidecarFloat("a", 0, 1, 0.5),
        SidecarEnumMap("mode", { "Calm", "Windy", "Stormy" }, R"({"Tundra":"Calm","TemperateForest":"Stormy","Desert":"Windy"})") }), &src);
    ASSERT_TRUE(b.ok) << b.err.key.AsCStr() << ": " << b.err.reason.AsCStr();
    ASSERT_EQ(4u, b.schema.GetAttributeCount());
    const char* keys[3] = { "mode.TemperateForest", "mode.Desert", "mode.Tundra" };
    const float ordinals[3] = { 2.0f, 1.0f, 0.0f };
    for (unsigned i = 0; i < 3; ++i)
    {
        const AttributeDefinition& d = b.schema.GetAttributeByIndex(1 + i);
        EXPECT_STREQ(keys[i], d.fullKey.AsCStr());
        EXPECT_EQ(AttributeKind::Enum, d.kind);
        EXPECT_FLOAT_EQ(ordinals[i], d.default_value) << keys[i];
        EXPECT_FLOAT_EQ(2.0f, d.maximum_value);
        EXPECT_STREQ("Windy", b.schema.GetEnumId(1 + i, 1).AsCStr());
    }
}

TEST(DiaAttributeSchemaFromValueSchema, EnumMapScalarDefaultStillFillsEveryColumn)
{
    FixtureMapKeySource src;
    const Built b = BuildText(MakeSidecarText({ SidecarEnumMap("mode", { "Calm", "Windy", "Stormy" }, "\"Windy\"") }), &src);
    ASSERT_TRUE(b.ok) << b.err.reason.AsCStr();
    for (unsigned i = 0; i < 3; ++i) EXPECT_FLOAT_EQ(1.0f, b.schema.GetAttributeByIndex(i).default_value);
}

TEST(DiaAttributeSchemaFromValueSchema, EnumMapObjectDefaultMissingAKeyNamesTheColumn)
{
    FixtureMapKeySource src;
    const Built b = BuildText(MakeSidecarText({ SidecarEnumMap("mode", { "Calm", "Windy" }, R"({"TemperateForest":"Calm","Tundra":"Windy"})") }), &src);
    EXPECT_FALSE(b.ok);
    EXPECT_STREQ("mode.Desert", b.err.key.AsCStr());
    EXPECT_TRUE(Contains(b.err.reason, "missing"));
}

TEST(DiaAttributeSchemaFromValueSchema, EnumMapObjectDefaultWithAnIdNotInTheEnumNamesTheColumn)
{
    // The loader rejects an unknown id before this point; a hand-edited LoadResult must still fail naming the column.
    FixtureMapKeySource src;
    LoadResult r = Parse(MakeSidecarText({ SidecarEnumMap("mode", { "Calm", "Windy" }, R"({"TemperateForest":"Calm","Desert":"Windy","Tundra":"Calm"})") }));
    for (unsigned i = 0; i < r.mEntries[0].mMapDefaults.Size(); ++i)
        if (std::strcmp(r.mEntries[0].mMapDefaults[i].mMapKey.AsCStr(), "Desert") == 0)
            Dia::ValueSchema::SetText(r.mEntries[0].mMapDefaults[i].mString, "Gale");
    const Built b = Build(r, &src);
    EXPECT_FALSE(b.ok);
    EXPECT_STREQ("mode.Desert", b.err.key.AsCStr());
    EXPECT_TRUE(Contains(b.err.reason, "Gale"));
}

TEST(DiaAttributeSchemaFromValueSchema, MapWithZeroKeysExpandsToNothing)
{
    ListKeySource src(nullptr, nullptr, 0);
    const Built b = BuildText(MakeSidecarText({ SidecarFloat("a", 0, 1, 0.5), SidecarMap("m", 0, 1, "{}") }), &src);
    ASSERT_TRUE(b.ok) << b.err.key.AsCStr() << ": " << b.err.reason.AsCStr();
    EXPECT_EQ(1u, b.schema.GetAttributeCount());
}

TEST(DiaAttributeSchemaFromValueSchema, MapKeysComeFromTheSourceNotTheDefaultObject)
{
    const char* names[2] = { "Beta", "Alpha" };
    StringCRC ids[2] = { StringCRC("Beta"), StringCRC("Alpha") };
    ListKeySource src(names, ids, 2);
    const Built b = BuildText(MakeSidecarText({ SidecarMap("m", 0, 1, R"({"Alpha":0.1,"Beta":0.2})") }), &src);
    ASSERT_TRUE(b.ok);
    EXPECT_STREQ("m.Beta", b.schema.GetAttributeByIndex(0).fullKey.AsCStr());
    EXPECT_FLOAT_EQ(0.2f, b.schema.GetAttributeByIndex(0).default_value);
    EXPECT_STREQ("m.Alpha", b.schema.GetAttributeByIndex(1).fullKey.AsCStr());
}

// ---------------------------------------------------------------------------
// AC 6: constraints
// ---------------------------------------------------------------------------
TEST(DiaAttributeSchemaFromValueSchema, ConstraintsOnDefaultsAreResolvedAndPass)
{
    const Built b = BuildText(MakeSidecarText({ SidecarFloat("x", 0, 1, 0.2), SidecarFloat("y", 0, 1, 0.8) },
        R"([{"a":"x","op":"<=","b":"y"},{"a":"y","op":">","b":"x"}])"));
    ASSERT_TRUE(b.ok) << b.err.reason.AsCStr();
    ASSERT_EQ(2u, b.schema.GetConstraintCount());
    EXPECT_EQ(0u, b.schema.GetConstraintByIndex(0).a);
    EXPECT_EQ(1u, b.schema.GetConstraintByIndex(0).b);
    EXPECT_EQ(ConstraintOp::Le, b.schema.GetConstraintByIndex(0).op);
    EXPECT_EQ(1u, b.schema.GetConstraintByIndex(1).a);
    EXPECT_EQ(ConstraintOp::Gt, b.schema.GetConstraintByIndex(1).op);
}

TEST(DiaAttributeSchemaFromValueSchema, ViolatingDefaultConstraintNamesBothKeys)
{
    const Built b = BuildText(MakeSidecarText({ SidecarFloat("snow.melt", 0, 1, 0.9), SidecarFloat("snow.threshold", 0, 1, 0.2) },
        R"([{"a":"snow.melt","op":"<=","b":"snow.threshold"}])"));
    EXPECT_FALSE(b.ok);
    EXPECT_STREQ("snow.melt", b.err.key.AsCStr());
    EXPECT_TRUE(Contains(b.err.reason, "snow.melt"));
    EXPECT_TRUE(Contains(b.err.reason, "snow.threshold"));
}

TEST(DiaAttributeSchemaFromValueSchema, ConstraintAcrossMapColumn)
{
    FixtureMapKeySource src;
    const Built ok = BuildText(MakeSidecarText({ SidecarFloat("lo", 0, 1, 0.1), SidecarMap("affinity", 0, 1, kAffinityObject) },
        R"([{"a":"lo","op":"<=","b":"affinity.Desert"}])"), &src);
    ASSERT_TRUE(ok.ok) << ok.err.key.AsCStr() << ": " << ok.err.reason.AsCStr();
    ASSERT_EQ(1u, ok.schema.GetConstraintCount());
    EXPECT_EQ(0u, ok.schema.GetConstraintByIndex(0).a);
    EXPECT_EQ(2u, ok.schema.GetConstraintByIndex(0).b);   // lo, TemperateForest, Desert

    const Built bad = BuildText(MakeSidecarText({ SidecarFloat("lo", 0, 1, 0.9), SidecarMap("affinity", 0, 1, kAffinityObject) },
        R"([{"a":"lo","op":"<=","b":"affinity.Desert"}])"), &src);
    EXPECT_FALSE(bad.ok);
    EXPECT_STREQ("lo", bad.err.key.AsCStr());
    EXPECT_TRUE(Contains(bad.err.reason, "affinity.Desert"));
}

TEST(DiaAttributeSchemaFromValueSchema, ConstraintMapColumnThatDoesNotExistFails)
{
    FixtureMapKeySource src;
    const Built b = BuildText(MakeSidecarText({ SidecarFloat("lo", 0, 1, 0.1), SidecarMap("affinity", 0, 1, kAffinityObject) },
        R"([{"a":"lo","op":"<=","b":"affinity.Swamp"}])"), &src);
    EXPECT_FALSE(b.ok);
    EXPECT_STREQ("affinity.Swamp", b.err.key.AsCStr());
}

TEST(DiaAttributeSchemaFromValueSchema, ConstraintWithUnknownKeyFails)
{
    LoadResult r = Parse(MakeSidecarText({ SidecarFloat("x", 0, 1, 0.2), SidecarFloat("y", 0, 1, 0.8) }, R"([{"a":"x","op":"<","b":"y"}])"));
    Dia::ValueSchema::SetText(r.mConstraints[0].mB, "nope");
    const Built b = Build(r);
    EXPECT_FALSE(b.ok);
    EXPECT_STREQ("nope", b.err.key.AsCStr());

    Dia::ValueSchema::SetText(r.mConstraints[0].mB, "y");
    Dia::ValueSchema::SetText(r.mConstraints[0].mA, "missing");
    const Built b2 = Build(r);
    EXPECT_FALSE(b2.ok);
    EXPECT_STREQ("missing", b2.err.key.AsCStr());
}

TEST(DiaAttributeSchemaFromValueSchema, ConstraintWithNonNumericKeyFails)
{
    LoadResult r = Parse(MakeSidecarText({ SidecarFloat("x", 0, 1, 0.2), SidecarFloat("y", 0, 1, 0.8),
        SidecarBool("flag", true), SidecarEnum("mode", { "A", "B" }, "A") }, R"([{"a":"x","op":"<","b":"y"}])"));
    Dia::ValueSchema::SetText(r.mConstraints[0].mB, "flag");
    Built b = Build(r);
    EXPECT_FALSE(b.ok);
    EXPECT_STREQ("flag", b.err.key.AsCStr());

    Dia::ValueSchema::SetText(r.mConstraints[0].mB, "mode");
    b = Build(r);
    EXPECT_FALSE(b.ok);
    EXPECT_STREQ("mode", b.err.key.AsCStr());
}

TEST(DiaAttributeSchemaFromValueSchema, EveryOperatorOnDefaults)
{
    struct Case { const char* op; float x; float y; bool holds; };
    const Case cases[] = {
        { "<", 1, 2, true }, { "<", 2, 2, false },
        { "<=", 2, 2, true }, { "<=", 3, 2, false },
        { ">", 3, 2, true }, { ">", 2, 2, false },
        { ">=", 2, 2, true }, { ">=", 1, 2, false },
        { "==", 2, 2, true }, { "==", 1, 2, false },
        { "!=", 1, 2, true }, { "!=", 2, 2, false },
    };
    for (const Case& c : cases)
    {
        const std::string constraint = std::string(R"([{"a":"x","op":")") + c.op + R"(","b":"y"}])";
        LoadResult r = Parse(MakeSidecarText({ SidecarFloat("x", 0, 10, 1), SidecarFloat("y", 0, 10, 2) }, constraint.c_str()));
        r.mEntries[0].mDefaultNumber = c.x;
        r.mEntries[1].mDefaultNumber = c.y;
        const Built b = Build(r);
        EXPECT_EQ(c.holds, b.ok) << "op " << c.op << " x=" << c.x << " y=" << c.y;
        if (!c.holds) { EXPECT_STREQ("x", b.err.key.AsCStr()); }
    }
}

TEST(DiaAttributeSchemaFromValueSchema, CheckConstraintsOnExplicitValuesNamesBothKeys)
{
    const Built b = BuildText(MakeSidecarText({ SidecarFloat("x", 0, 1, 0.1), SidecarFloat("y", 0, 1, 0.8) }, R"([{"a":"x","op":"<","b":"y"}])"));
    ASSERT_TRUE(b.ok);
    SchemaError err;
    const float good[2] = { 0.1f, 0.2f };
    EXPECT_TRUE(b.schema.CheckConstraints(good, 2, err));
    const float bad[2] = { 0.5f, 0.2f };
    EXPECT_FALSE(b.schema.CheckConstraints(bad, 2, err));
    EXPECT_STREQ("x", err.key.AsCStr());
    EXPECT_TRUE(Contains(err.reason, "x"));
    EXPECT_TRUE(Contains(err.reason, "y"));
    EXPECT_FALSE(b.schema.CheckConstraints(good, 1, err));   // wrong count never passes
}

TEST(DiaAttributeSchemaFromValueSchema, EqualityComparisonIsExact)
{
    const Built b = BuildText(MakeSidecarText({ SidecarFloat("x", 0, 1, 0.5), SidecarFloat("y", 0, 1, 0.5) },
        R"([{"a":"x","op":"==","b":"y"}])"));
    ASSERT_TRUE(b.ok);
    SchemaError err;
    const float nearly[2] = { 0.5f, std::nextafter(0.5f, 1.0f) };
    EXPECT_FALSE(b.schema.CheckConstraints(nearly, 2, err));
    const float same[2] = { 0.5f, 0.5f };
    EXPECT_TRUE(b.schema.CheckConstraints(same, 2, err));

    const Built ne = BuildText(MakeSidecarText({ SidecarFloat("x", 0, 1, 0.5), SidecarFloat("y", 0, 1, 0.6) },
        R"([{"a":"x","op":"!=","b":"y"}])"));
    ASSERT_TRUE(ne.ok);
    EXPECT_TRUE(ne.schema.CheckConstraints(nearly, 2, err));
    EXPECT_FALSE(ne.schema.CheckConstraints(same, 2, err));
}

TEST(DiaAttributeSchemaFromValueSchema, SchemaWithoutConstraintsAlwaysPasses)
{
    const Built b = BuildText(MakeSidecarText({ SidecarFloat("x", 0, 1, 0.5) }));
    ASSERT_TRUE(b.ok);
    SchemaError err;
    const float v[1] = { 0.9f };
    EXPECT_TRUE(b.schema.CheckConstraints(v, 1, err));
}

TEST(DiaAttributeSchemaFromValueSchema, LoadSchemaOrDieBuildsAndThrowsOnFailure)
{
    EXPECT_EQ(1u, LoadSchemaOrDie(MakeSidecarText({ SidecarFloat("x", 0, 1, 0.5) }).c_str()).GetAttributeCount());
    EXPECT_THROW(LoadSchemaOrDie(MakeSidecarText({ SidecarMap("m", 0, 1, "0.5") }).c_str()), std::runtime_error);
}

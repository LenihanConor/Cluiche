#include <gtest/gtest.h>
#include <DiaValueSchema/ValueSchemaLoader.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/CategoricalColours.h>
#include <DiaValueSchema/Testing/SchemaFixtureBuilder.h>
#include <string>

using namespace Dia::ValueSchema;
using namespace Dia::ValueSchema::Testing;
using Dia::Core::StringCRC;

TEST(DiaValueSchemaLoader, ParameterWithEveryKey)
{
	const char* text = R"({ "format": "dia.value_schema", "version": 1, "for": "cow.ecology_params",
	  "entries": [ { "key": "grazing.rate", "kind": "parameter", "label": "Grazing rate",
	  "description": "How fast", "unit": "x per season", "type": "float", "min": 0, "max": 2, "default": 1.0,
	  "affects": ["producers_total", "b"], "related": ["regrowth.forest"], "hint": "A hint", "source": "GDD 4.2",
	  "ui": {"group":"Herbivory","step":0.05,"decimals":2}, "history": {"track": true} } ] })";
	LoadResult r = ValueSchemaLoader::LoadFromText(text, "eco.diavalues");
	ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
	EXPECT_STREQ(r.mFor.AsCStr(), "cow.ecology_params");
	ASSERT_EQ(r.mEntries.Size(), 1u);
	const ValueInfo& v = r.mEntries[0];
	EXPECT_STREQ(v.mFor.AsCStr(), "cow.ecology_params");
	EXPECT_STREQ(v.mKey.AsCStr(), "grazing.rate");
	EXPECT_EQ(v.mKind, ValueKind::Parameter);
	EXPECT_EQ(v.mType, ValueType::Float);
	EXPECT_STREQ(v.mLabel.AsCStr(), "Grazing rate");
	EXPECT_STREQ(v.mDescription.AsCStr(), "How fast");
	EXPECT_STREQ(v.mUnit.AsCStr(), "x per season");
	EXPECT_TRUE(v.mHasMin);
	EXPECT_TRUE(v.mHasMax);
	EXPECT_DOUBLE_EQ(v.mMin, 0.0);
	EXPECT_DOUBLE_EQ(v.mMax, 2.0);
	EXPECT_TRUE(v.mHasDefault);
	EXPECT_DOUBLE_EQ(v.mDefaultNumber, 1.0);
	ASSERT_EQ(v.mAffects.Size(), 2u);
	EXPECT_STREQ(v.mAffects[0].AsCStr(), "producers_total");
	EXPECT_STREQ(v.mAffects[1].AsCStr(), "b");
	ASSERT_EQ(v.mRelated.Size(), 1u);
	EXPECT_STREQ(v.mRelated[0].AsCStr(), "regrowth.forest");
	EXPECT_STREQ(v.mHint.AsCStr(), "A hint");
	EXPECT_STREQ(v.mSource.AsCStr(), "GDD 4.2");
	EXPECT_STREQ(v.mSourceFile.AsCStr(), "eco.diavalues");
	EXPECT_STREQ(v.mUi["group"].asCString(), "Herbivory");
	EXPECT_DOUBLE_EQ(v.mUi["step"].asDouble(), 0.05);
	EXPECT_EQ(v.mUi["decimals"].asInt(), 2);
	EXPECT_TRUE(v.mHistory["track"].asBool());
}

TEST(DiaValueSchemaLoader, KindsAndTypesParse)
{
	SchemaFixtureBuilder b;
	b.Entry("a", "field", "int").Entry("b", "metric", "bool").Entry("c", "arg", "string")
	 .Entry("d", "command", "ref").Ref("x/y").Entry("e", "event", "ref[]").Ref("x/y")
	 .Entry("f", "component_field", "map").Set("keyRef", Json::Value("x/y")).Set("valueType", Json::Value("float"))
	 .Entry("g", "field", "enum").EnumValueEntry("v", "V");
	LoadResult r = ValueSchemaLoader::LoadFromText(b.Build().c_str(), "k.diavalues");
	ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
	ASSERT_EQ(r.mEntries.Size(), 7u);
	EXPECT_EQ(r.mEntries[0].mKind, ValueKind::Field);
	EXPECT_EQ(r.mEntries[0].mType, ValueType::Int);
	EXPECT_EQ(r.mEntries[1].mKind, ValueKind::Metric);
	EXPECT_EQ(r.mEntries[1].mType, ValueType::Bool);
	EXPECT_EQ(r.mEntries[2].mKind, ValueKind::Arg);
	EXPECT_EQ(r.mEntries[2].mType, ValueType::String);
	EXPECT_EQ(r.mEntries[3].mKind, ValueKind::Command);
	EXPECT_EQ(r.mEntries[3].mType, ValueType::Ref);
	EXPECT_EQ(r.mEntries[4].mKind, ValueKind::Event);
	EXPECT_EQ(r.mEntries[4].mType, ValueType::RefList);
	EXPECT_EQ(r.mEntries[5].mKind, ValueKind::ComponentField);
	EXPECT_EQ(r.mEntries[5].mType, ValueType::Map);
	EXPECT_EQ(r.mEntries[6].mType, ValueType::Enum);
}

TEST(DiaValueSchemaLoader, DefaultsBoolAndString)
{
	SchemaFixtureBuilder b;
	b.Entry("flag", "parameter", "bool").Default(Json::Value(true))
	 .Entry("name", "parameter", "string").Default(Json::Value("hello"))
	 .Entry("none", "parameter", "float");
	LoadResult r = ValueSchemaLoader::LoadFromText(b.Build().c_str(), "d.diavalues");
	ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
	EXPECT_TRUE(r.mEntries[0].mHasDefault);
	EXPECT_DOUBLE_EQ(r.mEntries[0].mDefaultNumber, 1.0);
	EXPECT_TRUE(r.mEntries[1].mHasDefault);
	EXPECT_STREQ(r.mEntries[1].mDefaultString.AsCStr(), "hello");
	EXPECT_FALSE(r.mEntries[2].mHasDefault);
	EXPECT_FALSE(r.mEntries[2].mHasMin);
	EXPECT_FALSE(r.mEntries[2].mHasMax);
}

TEST(DiaValueSchemaLoader, EnumColourGivenVersusAssigned)
{
	SchemaFixtureBuilder b;
	b.Entry("category", "field", "enum")
	 .EnumValueEntry("forest", "Forest", "Trees", "#112233")
	 .EnumValueEntry("grass", "Grass")
	 .EnumValueEntry("rock", "Rock", "Stone");
	LoadResult r = ValueSchemaLoader::LoadFromText(b.Build().c_str(), "e.diavalues");
	ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
	const ValueInfo& v = r.mEntries[0];
	ASSERT_EQ(v.mEnumValues.Size(), 3u);
	EXPECT_STREQ(v.mEnumValues[0].mId.AsCStr(), "forest");
	EXPECT_STREQ(v.mEnumValues[0].mLabel.AsCStr(), "Forest");
	EXPECT_STREQ(v.mEnumValues[0].mDescription.AsCStr(), "Trees");
	EXPECT_TRUE(v.mEnumValues[0].mHasColour);
	EXPECT_STREQ(v.mEnumValues[0].mColour.AsCStr(), "#112233");
	EXPECT_FALSE(v.mEnumValues[1].mHasColour);
	EXPECT_STREQ(v.mEnumValues[1].mColour.AsCStr(), GetCategoricalColour(1));
	EXPECT_FALSE(v.mEnumValues[2].mHasColour);
	EXPECT_STREQ(v.mEnumValues[2].mColour.AsCStr(), GetCategoricalColour(2));
	EXPECT_STRNE(GetCategoricalColour(1), GetCategoricalColour(2));
}

TEST(DiaValueSchemaLoader, EnumAssignedColoursWrapAfterTwelve)
{
	SchemaFixtureBuilder b;
	b.Entry("c", "field", "enum");
	for (int i = 0; i < 14; ++i)
	{
		std::string id = "v" + std::to_string(i);
		b.EnumValueEntry(id.c_str(), id.c_str());
	}
	LoadResult r = ValueSchemaLoader::LoadFromText(b.Build().c_str(), "w.diavalues");
	ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
	const ValueInfo& v = r.mEntries[0];
	ASSERT_EQ(v.mEnumValues.Size(), 14u);
	EXPECT_STREQ(v.mEnumValues[12].mColour.AsCStr(), v.mEnumValues[0].mColour.AsCStr());
	EXPECT_STREQ(v.mEnumValues[13].mColour.AsCStr(), v.mEnumValues[1].mColour.AsCStr());
	for (unsigned int a = 0; a < 12; ++a)
	{
		for (unsigned int c = a + 1; c < 12; ++c)
		{
			EXPECT_STRNE(GetCategoricalColour(a), GetCategoricalColour(c));
		}
	}
}

TEST(DiaValueSchemaLoader, SplitRecordPathCases)
{
	ValueList<Dia::Core::Containers::String64> s;
	s.Reserve(8);
	ASSERT_TRUE(SplitRecordPath("species[].growth_rate", s));
	ASSERT_EQ(s.Size(), 3u);
	EXPECT_STREQ(s[0].AsCStr(), "species");
	EXPECT_STREQ(s[1].AsCStr(), "[]");
	EXPECT_STREQ(s[2].AsCStr(), "growth_rate");

	ValueList<Dia::Core::Containers::String64> n;
	n.Reserve(8);
	ASSERT_TRUE(SplitRecordPath("a.b[].c", n));
	ASSERT_EQ(n.Size(), 4u);
	EXPECT_STREQ(n[0].AsCStr(), "a");
	EXPECT_STREQ(n[1].AsCStr(), "b");
	EXPECT_STREQ(n[2].AsCStr(), "[]");
	EXPECT_STREQ(n[3].AsCStr(), "c");

	ValueList<Dia::Core::Containers::String64> p;
	p.Reserve(8);
	ASSERT_TRUE(SplitRecordPath("plain", p));
	ASSERT_EQ(p.Size(), 1u);
	EXPECT_STREQ(p[0].AsCStr(), "plain");
}

TEST(DiaValueSchemaLoader, LoaderFillsPathSegmentsAndKeepsKeyAsWritten)
{
	SchemaFixtureBuilder b;
	b.Entry("species[].growth_rate").Entry("plain");
	LoadResult r = ValueSchemaLoader::LoadFromText(b.Build().c_str(), "p.diavalues");
	ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
	EXPECT_STREQ(r.mEntries[0].mKey.AsCStr(), "species[].growth_rate");
	ASSERT_EQ(r.mEntries[0].mPathSegments.Size(), 3u);
	EXPECT_STREQ(r.mEntries[0].mPathSegments[1].AsCStr(), "[]");
	ASSERT_EQ(r.mEntries[1].mPathSegments.Size(), 1u);
	EXPECT_STREQ(r.mEntries[1].mPathSegments[0].AsCStr(), "plain");
}

TEST(DiaValueSchemaLoader, UniqueRequiredRefRoundTrip)
{
	SchemaFixtureBuilder b;
	b.Entry("id", "field", "string").Unique().Required()
	 .Entry("biome", "field", "ref").Ref("cow.biomes/biomes[].id")
	 .Entry("plain", "field", "string");
	LoadResult r = ValueSchemaLoader::LoadFromText(b.Build().c_str(), "u.diavalues");
	ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
	EXPECT_TRUE(r.mEntries[0].mUnique);
	EXPECT_TRUE(r.mEntries[0].mRequired);
	EXPECT_STREQ(r.mEntries[1].mRef.AsCStr(), "cow.biomes/biomes[].id");
	EXPECT_FALSE(r.mEntries[1].mUnique);
	EXPECT_FALSE(r.mEntries[2].mRequired);
}

TEST(DiaValueSchemaLoader, EntriesInFileOrder)
{
	SchemaFixtureBuilder b;
	b.Entry("zeta").Entry("alpha").Entry("mid");
	LoadResult r = ValueSchemaLoader::LoadFromText(b.Build().c_str(), "o.diavalues");
	ASSERT_TRUE(r.mOk);
	ASSERT_EQ(r.mEntries.Size(), 3u);
	EXPECT_STREQ(r.mEntries[0].mKey.AsCStr(), "zeta");
	EXPECT_STREQ(r.mEntries[1].mKey.AsCStr(), "alpha");
	EXPECT_STREQ(r.mEntries[2].mKey.AsCStr(), "mid");
}

TEST(DiaValueSchemaLoader, UnparsableJsonFails)
{
	LoadResult r = ValueSchemaLoader::LoadFromText("{ not json", "bad.diavalues");
	EXPECT_FALSE(r.mOk);
	EXPECT_STREQ(r.mFailure.mFile.AsCStr(), "bad.diavalues");
	EXPECT_GT(r.mFailure.mReason.Length(), 0u);
	EXPECT_EQ(r.mEntries.Size(), 0u);
}

TEST(DiaValueSchemaLoader, MissingEntriesFails)
{
	SchemaFixtureBuilder b;
	LoadResult r = ValueSchemaLoader::LoadFromText(b.BuildWithoutEntries().c_str(), "noent.diavalues");
	EXPECT_FALSE(r.mOk);
	EXPECT_STREQ(r.mFailure.mFile.AsCStr(), "noent.diavalues");
	EXPECT_GT(r.mFailure.mReason.Length(), 0u);
}

TEST(DiaValueSchemaLoader, OverlongFieldFailsNamingEntry)
{
	SchemaFixtureBuilder b;
	std::string unit(ValueInfo::kMaxUnitLength + 1, 'u');
	b.Entry("ok").Entry("toolong").Unit(unit.c_str());
	LoadResult r = ValueSchemaLoader::LoadFromText(b.Build().c_str(), "long.diavalues");
	EXPECT_FALSE(r.mOk);
	EXPECT_STREQ(r.mFailure.mEntry.AsCStr(), "toolong");
	EXPECT_EQ(r.mEntries.Size(), 0u);
}

TEST(DiaValueSchemaLoader, LoadedEntriesRegisterAndFindEndToEnd)
{
	SchemaFixtureBuilder b;
	b.For("cow.ecology_params").Entry("grazing.rate").Min(0).Max(2).Entry("regrowth.forest");
	LoadResult r = ValueSchemaLoader::LoadFromText(b.Build().c_str(), "e2e.diavalues");
	ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
	ValueSchemaRegistry reg;
	for (unsigned int i = 0; i < r.mEntries.Size(); ++i)
	{
		EXPECT_EQ(reg.Register(r.mEntries[i]), RegisterResult::Ok);
	}
	const ValueInfo* v = reg.Find(StringCRC("cow.ecology_params"), StringCRC("grazing.rate"));
	ASSERT_NE(v, nullptr);
	EXPECT_TRUE(v->mHasMax);
	EXPECT_STREQ(v->mFullKey.AsCStr(), "cow.ecology_params/grazing.rate");
	EXPECT_NE(reg.Find(StringCRC("cow.ecology_params/regrowth.forest")), nullptr);
}

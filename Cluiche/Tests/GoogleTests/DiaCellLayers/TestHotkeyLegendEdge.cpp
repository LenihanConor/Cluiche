#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayerLegend.h>
#include <DiaCellLayers/HotkeyTable.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>

#include <limits>
#include <stdio.h>
#include <string.h>
#include <string>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;

namespace
{
	const float kNaN = std::numeric_limits<float>::quiet_NaN();
	const float kInf = std::numeric_limits<float>::infinity();

	struct HotkeyEdgeTest : ::testing::Test
	{
		UploaderSchemaFixture fx;
		Dia::Graphics::CellGridDesc gridA, gridB;
		void SetUp() override { FakePuId() = 1; }

		void AddKey(const char* name, const Json::Value& hotkey)
		{
			auto e = UploaderSchemaFixture::FloatEntry(name, 0, 1);
			e.mUi["hotkey"] = hotkey;
			fx.Add(e);
		}
		static std::string Key(const char* name) { return std::string("cells/") + name; }
	};

	struct LegendEdgeTest : ::testing::Test
	{
		UploaderSchemaFixture fx;
		TestGrid grid{ 2, 2 };
		StubProvider provider;
		CellLayerLegend legend;

		Dia::ValueSchema::LegendData BuildFloat(double mn, double mx, bool seen, float smin, float smax)
		{
			fx.Add(UploaderSchemaFixture::FloatEntry("v", mn, mx));
			const auto& svc = fx.Publish();
			resolver.reset(new Dia::ValueSchema::RampResolver(svc));
			return legend.Build(provider.MakeDesc("v", &grid.desc, CellValueKind::Float, "cells/v"), &svc, *resolver, seen, smin, smax);
		}
		std::unique_ptr<Dia::ValueSchema::RampResolver> resolver;
	};
}

TEST_F(HotkeyEdgeTest, MultiCharValueUsesFirstCharacter)
{
	AddKey("a", "tm");
	const auto& svc = fx.Publish();
	CellLayerRegistry reg(&svc, &FakePuIdFn);
	reg.Register(MakeLayer("a", &gridA, Key("a").c_str()));
	HotkeyTable table(reg, &svc);
	EXPECT_EQ(table.Rebuild(&gridA), 1u);
	EXPECT_EQ(table.HotkeyOf(StringCRC("a")), 't');
	EXPECT_EQ(table.Lookup('m'), nullptr);
}

TEST_F(HotkeyEdgeTest, EmptyAndNonStringValuesBindNothingWithoutClash)
{
	AddKey("e", "");
	AddKey("n", 5);
	AddKey("b", true);
	AddKey("o", Json::Value(Json::objectValue));
	const auto& svc = fx.Publish();
	CellLayerRegistry reg(&svc, &FakePuIdFn);
	for (const char* k : { "e", "n", "b", "o" }) { reg.Register(MakeLayer(k, &gridA, Key(k).c_str())); }
	HotkeyTable table(reg, &svc);
	EXPECT_EQ(table.Rebuild(&gridA), 0u);
	EXPECT_EQ(table.ClashCount(), 0u);
	EXPECT_EQ(table.Lookup('\0'), nullptr);
	EXPECT_EQ(table.HotkeyOf(StringCRC("e")), 0);
}

TEST_F(HotkeyEdgeTest, NonAsciiUtf8LeadByteBindsAsSingleChar)
{
	AddKey("a", "\xC3\xA9");	// e-acute in UTF-8
	AddKey("b", "\xC3\xA8");	// shares the lead byte: clashes
	const auto& svc = fx.Publish();
	CellLayerRegistry reg(&svc, &FakePuIdFn);
	reg.Register(MakeLayer("a", &gridA, Key("a").c_str()));
	reg.Register(MakeLayer("b", &gridA, Key("b").c_str()));
	HotkeyTable table(reg, &svc);
	EXPECT_EQ(table.Rebuild(&gridA), 1u);
	EXPECT_EQ(table.ClashCount(), 1u);
	EXPECT_EQ(table.Lookup('\xC3')->id, StringCRC("a"));
}

TEST_F(HotkeyEdgeTest, KeysAreCaseSensitive)
{
	AddKey("a", "t");
	AddKey("b", "T");
	const auto& svc = fx.Publish();
	CellLayerRegistry reg(&svc, &FakePuIdFn);
	reg.Register(MakeLayer("a", &gridA, Key("a").c_str()));
	reg.Register(MakeLayer("b", &gridA, Key("b").c_str()));
	HotkeyTable table(reg, &svc);
	EXPECT_EQ(table.Rebuild(&gridA), 2u);
	EXPECT_EQ(table.Lookup('t')->id, StringCRC("a"));
	EXPECT_EQ(table.Lookup('T')->id, StringCRC("b"));
}

TEST_F(HotkeyEdgeTest, SameKeyOnDifferentGridsNeverClashes)
{
	AddKey("a", "t");
	AddKey("b", "t");
	const auto& svc = fx.Publish();
	CellLayerRegistry reg(&svc, &FakePuIdFn);
	reg.Register(MakeLayer("a", &gridA, Key("a").c_str()));
	reg.Register(MakeLayer("b", &gridB, Key("b").c_str()));
	HotkeyTable table(reg, &svc);
	EXPECT_EQ(table.Rebuild(&gridA), 1u);
	EXPECT_EQ(table.Lookup('t')->id, StringCRC("a"));
	EXPECT_EQ(table.Rebuild(&gridB), 1u);
	EXPECT_EQ(table.Lookup('t')->id, StringCRC("b"));
	EXPECT_EQ(table.ClashCount(), 0u);
	EXPECT_EQ(table.HotkeyOf(StringCRC("a")), 0);
}

TEST_F(HotkeyEdgeTest, RebuildTwiceIsIdempotentAndResetsClashCount)
{
	AddKey("a", "t");
	AddKey("b", "t");
	AddKey("c", "m");
	const auto& svc = fx.Publish();
	CellLayerRegistry reg(&svc, &FakePuIdFn);
	for (const char* k : { "a", "b", "c" }) { reg.Register(MakeLayer(k, &gridA, Key(k).c_str())); }
	HotkeyTable table(reg, &svc);
	EXPECT_EQ(table.Rebuild(&gridA), 2u);
	EXPECT_EQ(table.Rebuild(&gridA), 2u);
	EXPECT_EQ(table.BindingCount(), 2u);
	EXPECT_EQ(table.ClashCount(), 1u);
}

TEST_F(HotkeyEdgeTest, RebuildAfterUnregisterDropsAndFreesKeys)
{
	AddKey("a", "t");
	AddKey("b", "t");
	const auto& svc = fx.Publish();
	CellLayerRegistry reg(&svc, &FakePuIdFn);
	reg.Register(MakeLayer("a", &gridA, Key("a").c_str()));
	reg.Register(MakeLayer("b", &gridA, Key("b").c_str()));
	HotkeyTable table(reg, &svc);
	table.Rebuild(&gridA);
	ASSERT_EQ(table.Lookup('t')->id, StringCRC("a"));
	reg.Unregister(StringCRC("a"));
	table.Rebuild(&gridA);
	EXPECT_EQ(table.Lookup('t')->id, StringCRC("b"));
	EXPECT_EQ(table.ClashCount(), 0u);
}

TEST_F(HotkeyEdgeTest, EmptyGridAndNullGridRebuildToZero)
{
	AddKey("a", "t");
	const auto& svc = fx.Publish();
	CellLayerRegistry reg(&svc, &FakePuIdFn);
	reg.Register(MakeLayer("a", &gridA, Key("a").c_str()));
	HotkeyTable table(reg, &svc);
	EXPECT_EQ(table.Rebuild(&gridA), 1u);
	EXPECT_EQ(table.Rebuild(&gridB), 0u);
	EXPECT_EQ(table.Lookup('t'), nullptr);
	EXPECT_EQ(table.Rebuild(nullptr), 0u);
}

TEST_F(HotkeyEdgeTest, LayerWithUnknownSchemaKeyIsSkippedSilently)
{
	AddKey("a", "t");
	const auto& svc = fx.Publish();
	CellLayerRegistry reg(&svc, &FakePuIdFn);
	reg.Register(MakeLayer("ghost", &gridA, "cells/ghost"));
	reg.Register(MakeLayer("a", &gridA, Key("a").c_str()));
	HotkeyTable table(reg, &svc);
	EXPECT_EQ(table.Rebuild(&gridA), 1u);
	EXPECT_EQ(table.ClashCount(), 0u);
}

TEST_F(HotkeyEdgeTest, AllPrintableAsciiKeysBindOnOneGrid)
{
	std::vector<std::string> names;
	for (int c = 33; c < 127; ++c) { char n[8]; snprintf(n, sizeof(n), "k%d", c); names.push_back(n); char k[2] = { static_cast<char>(c), 0 }; AddKey(n, k); }
	const auto& svc = fx.Publish();
	CellLayerRegistry reg(&svc, &FakePuIdFn);
	for (const auto& n : names) { ASSERT_EQ(reg.Register(MakeLayer(n.c_str(), &gridA, Key(n.c_str()).c_str())), CellRegisterResult::Ok); }
	HotkeyTable table(reg, &svc);
	EXPECT_EQ(table.Rebuild(&gridA), 94u);
	for (int c = 33; c < 127; ++c)
	{
		char n[8]; snprintf(n, sizeof(n), "k%d", c);
		ASSERT_NE(table.Lookup(static_cast<char>(c)), nullptr);
		EXPECT_EQ(table.Lookup(static_cast<char>(c))->id, StringCRC(n));
		EXPECT_EQ(table.HotkeyOf(StringCRC(n)), static_cast<char>(c));
	}
}

TEST(HotkeyGroupNumbering, TwentyLayersInterleavedGroupsNumberIndependently)
{
	FakePuId() = 1;
	Dia::Graphics::CellGridDesc grid;
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	for (int i = 0; i < 20; ++i)
	{
		char id[16];
		snprintf(id, sizeof(id), "l%d", i);
		reg.Register(MakeLayer(id, &grid, nullptr, CellValueKind::Float, (i % 3 == 0) ? "A" : (i % 3 == 1) ? "B" : "C"));
	}
	// Group A holds l0,l3,...,l18 (7 layers), B l1,l4,...,l19 (7), C l2,l5,...,l17 (6).
	EXPECT_EQ(HotkeyTable::GroupNumber(reg, &grid, StringCRC("l0")), 1u);
	EXPECT_EQ(HotkeyTable::GroupNumber(reg, &grid, StringCRC("l18")), 7u);
	EXPECT_EQ(HotkeyTable::GroupNumber(reg, &grid, StringCRC("l1")), 1u);
	EXPECT_EQ(HotkeyTable::GroupNumber(reg, &grid, StringCRC("l19")), 7u);
	EXPECT_EQ(HotkeyTable::GroupNumber(reg, &grid, StringCRC("l2")), 1u);
	EXPECT_EQ(HotkeyTable::GroupNumber(reg, &grid, StringCRC("l17")), 6u);
}

TEST(HotkeyGroupNumbering, NinthIsNumberedTenthIsNot)
{
	FakePuId() = 1;
	Dia::Graphics::CellGridDesc grid;
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	for (int i = 0; i < 20; ++i) { char id[16]; snprintf(id, sizeof(id), "g%d", i); reg.Register(MakeLayer(id, &grid, nullptr, CellValueKind::Float, "G")); }
	EXPECT_EQ(HotkeyTable::GroupNumber(reg, &grid, StringCRC("g8")), 9u);
	for (int i = 9; i < 20; ++i) { char id[16]; snprintf(id, sizeof(id), "g%d", i); EXPECT_EQ(HotkeyTable::GroupNumber(reg, &grid, StringCRC(id)), 0u) << id; }
}

TEST(HotkeyGroupNumbering, GroupsOnDifferentGridsAreIndependentAndWrongGridIsZero)
{
	FakePuId() = 1;
	Dia::Graphics::CellGridDesc a, b;
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	reg.Register(MakeLayer("a1", &a, nullptr, CellValueKind::Float, "G"));
	reg.Register(MakeLayer("b1", &b, nullptr, CellValueKind::Float, "G"));
	reg.Register(MakeLayer("a2", &a, nullptr, CellValueKind::Float, "G"));
	EXPECT_EQ(HotkeyTable::GroupNumber(reg, &a, StringCRC("a2")), 2u);
	EXPECT_EQ(HotkeyTable::GroupNumber(reg, &b, StringCRC("b1")), 1u);
	EXPECT_EQ(HotkeyTable::GroupNumber(reg, &b, StringCRC("a1")), 0u);
}

TEST(HotkeyGroupNumbering, UnregisterRenumbersFollowingLayers)
{
	FakePuId() = 1;
	Dia::Graphics::CellGridDesc g;
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	reg.Register(MakeLayer("a", &g));
	reg.Register(MakeLayer("b", &g));
	reg.Register(MakeLayer("c", &g));
	reg.Unregister(StringCRC("a"));
	EXPECT_EQ(HotkeyTable::GroupNumber(reg, &g, StringCRC("b")), 1u);
	EXPECT_EQ(HotkeyTable::GroupNumber(reg, &g, StringCRC("c")), 2u);
}

TEST_F(LegendEdgeTest, NegativeRangeKeepsOrderedEndpointTicks)
{
	const auto l = BuildFloat(-10.0, -5.0, false, 0, 0);
	EXPECT_EQ(l.mMin, -10.0f);
	EXPECT_EQ(l.mMax, -5.0f);
	ASSERT_GE(l.mTickCount, 2u);
	EXPECT_EQ(l.mTicks[0], -10.0f);
	EXPECT_EQ(l.mTicks[l.mTickCount - 1], -5.0f);
	for (unsigned i = 1; i < l.mTickCount; ++i) { EXPECT_GT(l.mTicks[i], l.mTicks[i - 1]); }
}

TEST_F(LegendEdgeTest, TinyRangeProducesStrictlyIncreasingTicks)
{
	const auto l = BuildFloat(1.0e-7, 2.0e-7, false, 0, 0);
	ASSERT_GE(l.mTickCount, 1u);
	EXPECT_LE(l.mTickCount, 5u);
	for (unsigned i = 1; i < l.mTickCount; ++i) { EXPECT_GT(l.mTicks[i], l.mTicks[i - 1]); }
}

TEST_F(LegendEdgeTest, EqualMinMaxGivesSingleTick)
{
	const auto l = BuildFloat(5.0, 5.0, false, 0, 0);
	EXPECT_EQ(l.mTickCount, 1u);
	EXPECT_EQ(l.mTicks[0], 5.0f);
}

TEST_F(LegendEdgeTest, ObservedNaNOrInfMeansNotSeen)
{
	EXPECT_FALSE(BuildFloat(0, 1, true, kNaN, 1.0f).mHasSeen);
}

TEST_F(LegendEdgeTest, ObservedInfinityMeansNotSeen)
{
	EXPECT_FALSE(BuildFloat(0, 1, true, 0.0f, kInf).mHasSeen);
}

TEST_F(LegendEdgeTest, ObservedInvertedIsSwapped)
{
	const auto l = BuildFloat(0, 10, true, 9.0f, -2.0f);
	ASSERT_TRUE(l.mHasSeen);
	EXPECT_EQ(l.mSeenMin, -2.0f);
	EXPECT_EQ(l.mSeenMax, 9.0f);
}

TEST_F(LegendEdgeTest, ObservedAbsentLeavesHasSeenFalseEvenWithValues)
{
	EXPECT_FALSE(BuildFloat(0, 1, false, 3.0f, 4.0f).mHasSeen);
}

TEST_F(LegendEdgeTest, EnumWithEmptyLabelsFallsBackToIdsInOrder)
{
	fx.Add(UploaderSchemaFixture::EnumEntry("biome", 4));
	const auto& svc = fx.Publish();
	Dia::ValueSchema::RampResolver r(svc);
	const auto l = legend.Build(provider.MakeDesc("biome", &grid.desc, CellValueKind::Category, "cells/biome"), &svc, r, false, 0, 0);
	ASSERT_TRUE(l.mIsCategory);
	ASSERT_EQ(l.mSwatches.Size(), 4u);
	for (unsigned i = 0; i < 4; ++i)
	{
		EXPECT_STREQ(l.mSwatches[i].mId.AsCStr(), (std::string("v") + std::to_string(i)).c_str());
		EXPECT_STREQ(l.mSwatches[i].mLabel.AsCStr(), l.mSwatches[i].mId.AsCStr());
	}
	EXPECT_EQ(l.mTickCount, 0u);
}

TEST_F(LegendEdgeTest, EnumWithZeroValuesHasNoSwatches)
{
	fx.Add(UploaderSchemaFixture::EnumEntry("none", 0));
	const auto& svc = fx.Publish();
	Dia::ValueSchema::RampResolver r(svc);
	const auto l = legend.Build(provider.MakeDesc("none", &grid.desc, CellValueKind::Category, "cells/none"), &svc, r, false, 0, 0);
	EXPECT_EQ(l.mSwatches.Size(), 0u);
}

TEST_F(LegendEdgeTest, PresentedWithNullPresentationHasEmptyPrefixAndMatchesBuild)
{
	fx.Add(UploaderSchemaFixture::FloatEntry("v", 0, 10));
	const auto& svc = fx.Publish();
	Dia::ValueSchema::RampResolver r(svc);
	const auto d = provider.MakeDesc("v", &grid.desc, CellValueKind::Float, "cells/v");
	const auto plain = legend.Build(d, &svc, r, true, 1, 2);
	const auto pres = legend.BuildPresented(d, &svc, r, true, 1, 2, nullptr);
	EXPECT_EQ(pres.unitPrefix[0], '\0');
	EXPECT_EQ(pres.legend.mMin, plain.mMin);
	EXPECT_EQ(pres.legend.mTickCount, plain.mTickCount);
}

TEST_F(LegendEdgeTest, PresentationWithUnknownRampKeepsSchemaRampButReplacesRange)
{
	fx.Add(UploaderSchemaFixture::FloatEntry("v", 0, 10, "test.heat"));
	const auto& svc = fx.Publish();
	Dia::ValueSchema::RampResolver r(svc);
	ViewPresentation p;
	p.rampId = StringCRC("no.such.ramp");
	p.rangeMin = -4.0f;
	p.rangeMax = 4.0f;
	const auto out = legend.BuildPresented(provider.MakeDesc("v", &grid.desc, CellValueKind::Float, "cells/v"), &svc, r, false, 0, 0, &p);
	EXPECT_EQ(out.legend.mRamp.mRamp.mId, StringCRC("test.heat"));
	EXPECT_EQ(out.legend.mMin, -4.0f);
	EXPECT_EQ(out.legend.mMax, 4.0f);
	EXPECT_EQ(out.legend.mTicks[0], -4.0f);
	EXPECT_EQ(out.legend.mTicks[out.legend.mTickCount - 1], 4.0f);
}

TEST_F(LegendEdgeTest, PresentationWithKnownRampReplacesIt)
{
	fx.Add(UploaderSchemaFixture::FloatEntry("v", 0, 10, "sequential.grey"));
	const auto& svc = fx.Publish();
	Dia::ValueSchema::RampResolver r(svc);
	ViewPresentation p;
	p.rampId = StringCRC("test.heat");
	const auto out = legend.BuildPresented(provider.MakeDesc("v", &grid.desc, CellValueKind::Float, "cells/v"), &svc, r, false, 0, 0, &p);
	EXPECT_EQ(out.legend.mRamp.mRamp.mId, StringCRC("test.heat"));
}

TEST_F(LegendEdgeTest, PresentationOnCategoryOnlyCopiesPrefix)
{
	fx.Add(UploaderSchemaFixture::EnumEntry("biome", 2));
	const auto& svc = fx.Publish();
	Dia::ValueSchema::RampResolver r(svc);
	ViewPresentation p;
	strncpy(p.unitPrefix, "d", sizeof(p.unitPrefix) - 1);
	p.rangeMin = -9.0f;
	const auto out = legend.BuildPresented(provider.MakeDesc("biome", &grid.desc, CellValueKind::Category, "cells/biome"), &svc, r, false, 0, 0, &p);
	EXPECT_TRUE(out.legend.mIsCategory);
	EXPECT_EQ(out.legend.mSwatches.Size(), 2u);
	EXPECT_STREQ(out.unitPrefix, "d");
	EXPECT_EQ(out.legend.mTickCount, 0u);
}

TEST_F(LegendEdgeTest, UnterminatedPrefixIsTerminated)
{
	fx.Add(UploaderSchemaFixture::FloatEntry("v", 0, 10));
	const auto& svc = fx.Publish();
	Dia::ValueSchema::RampResolver r(svc);
	ViewPresentation p;
	memset(p.unitPrefix, 'x', sizeof(p.unitPrefix));
	const auto out = legend.BuildPresented(provider.MakeDesc("v", &grid.desc, CellValueKind::Float, "cells/v"), &svc, r, false, 0, 0, &p);
	EXPECT_EQ(out.unitPrefix[sizeof(out.unitPrefix) - 1], '\0');
	EXPECT_EQ(strlen(out.unitPrefix), sizeof(out.unitPrefix) - 1);
}

TEST_F(LegendEdgeTest, PresentationWithNullSchemaYieldsEmptyLegendButPrefix)
{
	fx.Add(UploaderSchemaFixture::FloatEntry("v", 0, 10));
	const auto& svc = fx.Publish();
	Dia::ValueSchema::RampResolver r(svc);
	ViewPresentation p;
	strncpy(p.unitPrefix, "d", sizeof(p.unitPrefix) - 1);
	const auto out = legend.BuildPresented(provider.MakeDesc("v", &grid.desc, CellValueKind::Float, "cells/v"), nullptr, r, false, 0, 0, &p);
	EXPECT_EQ(out.legend.mTickCount, 0u);
	EXPECT_STREQ(out.unitPrefix, "d");
}

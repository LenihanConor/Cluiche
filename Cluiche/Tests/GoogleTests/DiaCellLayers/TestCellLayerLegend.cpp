#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayerLegend.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;

namespace
{
	struct CellLayerLegendTest : ::testing::Test
	{
		UploaderSchemaFixture fx;
		TestGrid grid{ 2, 2 };
		StubProvider provider;
		CellLayerLegend legend;
	};
}

TEST_F(CellLayerLegendTest, FloatLayerHasTicksUnitRampAndObserved)
{
	auto e = UploaderSchemaFixture::FloatEntry("temp", 0.0, 1.0, "test.heat");
	e.mUnit = Dia::Core::Containers::String32("%s", "C");
	fx.Add(e);
	const auto& svc = fx.Publish();
	Dia::ValueSchema::RampResolver resolver(svc);
	const CellLayerDesc d = provider.MakeDesc("temp", &grid.desc, CellValueKind::Float, "cells/temp");

	const auto l = legend.Build(d, &svc, resolver, true, -2.0f, 9.0f);
	EXPECT_FALSE(l.mIsCategory);
	EXPECT_EQ(l.mMin, 0.0f);
	EXPECT_EQ(l.mMax, 1.0f);
	ASSERT_GE(l.mTickCount, 2u);
	EXPECT_LE(l.mTickCount, 5u);
	EXPECT_EQ(l.mTicks[0], 0.0f);
	EXPECT_EQ(l.mTicks[l.mTickCount - 1], 1.0f);
	EXPECT_STREQ(l.mUnit.AsCStr(), "C");
	EXPECT_EQ(l.mRamp.mRamp.mId, StringCRC("test.heat"));
	EXPECT_TRUE(l.mHasSeen);
	EXPECT_EQ(l.mSeenMin, -2.0f);
	EXPECT_EQ(l.mSeenMax, 9.0f);
}

TEST_F(CellLayerLegendTest, NoObservedRangeLeavesHasSeenFalse)
{
	fx.Add(UploaderSchemaFixture::FloatEntry("temp", 0.0, 10.0));
	const auto& svc = fx.Publish();
	Dia::ValueSchema::RampResolver resolver(svc);
	const CellLayerDesc d = provider.MakeDesc("temp", &grid.desc, CellValueKind::Float, "cells/temp");
	const auto l = legend.Build(d, &svc, resolver, false, 0.0f, 0.0f);
	EXPECT_FALSE(l.mHasSeen);
	EXPECT_EQ(l.mMax, 10.0f);
}

TEST_F(CellLayerLegendTest, CategoryLayerHasOneSwatchPerValueInOrder)
{
	fx.Add(UploaderSchemaFixture::EnumEntry("biome", 3));
	const auto& svc = fx.Publish();
	Dia::ValueSchema::RampResolver resolver(svc);
	const CellLayerDesc d = provider.MakeDesc("biome", &grid.desc, CellValueKind::Category, "cells/biome");
	const auto l = legend.Build(d, &svc, resolver, true, 0.0f, 2.0f);
	EXPECT_TRUE(l.mIsCategory);
	EXPECT_EQ(l.mTickCount, 0u);
	ASSERT_EQ(l.mSwatches.Size(), 3u);
	EXPECT_STREQ(l.mSwatches[0].mId.AsCStr(), "v0");
	EXPECT_STREQ(l.mSwatches[1].mId.AsCStr(), "v1");
	EXPECT_STREQ(l.mSwatches[2].mId.AsCStr(), "v2");
	EXPECT_STREQ(l.mSwatches[1].mLabel.AsCStr(), "v1");	// empty label falls back to id
	EXPECT_NE(l.mSwatches[0].mRgba8, 0u);
}

TEST_F(CellLayerLegendTest, UnknownSchemaKeyGivesEmptyLegendAndWarnsOncePerKey)
{
	fx.Add(UploaderSchemaFixture::FloatEntry("temp", 0.0, 1.0));
	const auto& svc = fx.Publish();
	Dia::ValueSchema::RampResolver resolver(svc);
	const CellLayerDesc d = provider.MakeDesc("ghost", &grid.desc);
	for (int i = 0; i < 3; ++i)
	{
		const auto l = legend.Build(d, &svc, resolver, true, 0.0f, 1.0f);
		EXPECT_EQ(l.mTickCount, 0u);
		EXPECT_EQ(l.mSwatches.Size(), 0u);
		EXPECT_FALSE(l.mIsCategory);
		EXPECT_FALSE(l.mHasSeen);
	}
	EXPECT_EQ(legend.UnresolvedWarningCount(), 1u);
	legend.Build(provider.MakeDesc("ghost2", &grid.desc), &svc, resolver, false, 0.0f, 0.0f);
	EXPECT_EQ(legend.UnresolvedWarningCount(), 2u);
}

TEST_F(CellLayerLegendTest, NullSchemaGivesEmptyLegend)
{
	fx.Add(UploaderSchemaFixture::FloatEntry("temp", 0.0, 1.0));
	const auto& svc = fx.Publish();
	Dia::ValueSchema::RampResolver resolver(svc);
	const auto l = legend.Build(provider.MakeDesc("temp", &grid.desc, CellValueKind::Float, "cells/temp"), nullptr, resolver, true, 0.0f, 1.0f);
	EXPECT_EQ(l.mTickCount, 0u);
	EXPECT_EQ(l.mSwatches.Size(), 0u);
	EXPECT_FALSE(l.mHasSeen);
	EXPECT_EQ(legend.UnresolvedWarningCount(), 0u);
}

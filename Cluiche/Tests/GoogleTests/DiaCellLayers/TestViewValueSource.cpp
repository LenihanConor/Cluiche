#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayerLegend.h>
#include <DiaCellLayers/LayerUploader.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/ViewValueSourceTestSupport.h>

#include <string.h>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
using Dia::Graphics::CellFieldRegistry;

namespace
{
	struct ViewValueSourceTest : ::testing::Test
	{
		TestGrid grid{ 4, 3 };
		CellFieldRegistry cellField;
		RecordingSink sink;
		UploaderSchemaFixture fx;
		StubProvider temp;
		StubViewValueSource source;

		void SetUp() override
		{
			FakePuId() = 1;
			temp.values = { 5, -2, 7, 3, 1, 0, 4, 9, 2, 2, 2, 2 };
			source.values = std::vector<float>(12, 42.0f);
		}
	};
}

TEST_F(ViewValueSourceTest, NoSourceUsesLayerReadAndIsLive)
{
	CellLayerRegistry registry(nullptr, &FakePuIdFn);
	ASSERT_EQ(registry.Register(temp.MakeDesc("temp", &grid.desc)), CellRegisterResult::Ok);
	EXPECT_TRUE(registry.IsLive());
	LayerUploader up(cellField, nullptr);
	EXPECT_EQ(up.Update(registry.Find(StringCRC("temp")), registry), UploadResult::Ok);
	EXPECT_EQ(temp.readCalls, 1u);
	cellField.Drain(sink);
	ASSERT_EQ(sink.updates.size(), 1u);
	EXPECT_EQ(sink.updates[0].values[0], 5.0f);
}

TEST_F(ViewValueSourceTest, LiveSourceUsesLayerReadNotGetValues)
{
	CellLayerRegistry registry(nullptr, &FakePuIdFn);
	ASSERT_EQ(registry.Register(temp.MakeDesc("temp", &grid.desc)), CellRegisterResult::Ok);
	source.live = true;
	ASSERT_TRUE(registry.SetViewValueSource(&source));
	EXPECT_TRUE(registry.IsLive());
	LayerUploader up(cellField, nullptr);
	EXPECT_EQ(up.Update(registry.Find(StringCRC("temp")), registry), UploadResult::Ok);
	EXPECT_EQ(temp.readCalls, 1u);
	EXPECT_EQ(source.getValuesCalls, 0u);
}

TEST_F(ViewValueSourceTest, NotLiveUsesGetValues)
{
	CellLayerRegistry registry(nullptr, &FakePuIdFn);
	ASSERT_EQ(registry.Register(temp.MakeDesc("temp", &grid.desc)), CellRegisterResult::Ok);
	ASSERT_TRUE(registry.SetViewValueSource(&source));
	EXPECT_FALSE(registry.IsLive());
	LayerUploader up(cellField, nullptr);
	EXPECT_EQ(up.Update(registry.Find(StringCRC("temp")), registry), UploadResult::Ok);
	EXPECT_EQ(temp.readCalls, 0u);
	EXPECT_EQ(source.getValuesCalls, 1u);
	cellField.Drain(sink);
	ASSERT_EQ(sink.updates.size(), 1u);
	EXPECT_EQ(sink.updates[0].values[3], 42.0f);
	registry.ClearViewValueSource();
	EXPECT_TRUE(registry.IsLive());
}

TEST_F(ViewValueSourceTest, NotLiveNoDataNeverFallsBackToLive)
{
	CellLayerRegistry registry(nullptr, &FakePuIdFn);
	ASSERT_EQ(registry.Register(temp.MakeDesc("temp", &grid.desc)), CellRegisterResult::Ok);
	ASSERT_TRUE(registry.SetViewValueSource(&source));
	source.hasData = false;
	LayerUploader up(cellField, nullptr);
	EXPECT_EQ(up.Update(registry.Find(StringCRC("temp")), registry), UploadResult::NoData);
	EXPECT_EQ(temp.readCalls, 0u);
	EXPECT_EQ(up.Uploads(), 0u);
	EXPECT_FALSE(up.DrawRequest().layer.IsValid());
	uint32_t n = 0;
	EXPECT_EQ(up.BaseValues(StringCRC("temp"), n), nullptr);
}

TEST_F(ViewValueSourceTest, EpochChangeForcesReuploadWithSameVersion)
{
	CellLayerRegistry registry(nullptr, &FakePuIdFn);
	ASSERT_EQ(registry.Register(temp.MakeDesc("temp", &grid.desc)), CellRegisterResult::Ok);
	ASSERT_TRUE(registry.SetViewValueSource(&source));
	LayerUploader up(cellField, nullptr);
	const CellLayerDesc* base = registry.Find(StringCRC("temp"));
	up.Update(base, registry);
	up.Update(base, registry);
	EXPECT_EQ(source.getValuesCalls, 1u);	// version+epoch gated
	source.epoch = 2;
	source.values = std::vector<float>(12, 7.0f);
	EXPECT_EQ(up.Update(base, registry), UploadResult::Ok);
	EXPECT_EQ(source.getValuesCalls, 2u);
	EXPECT_EQ(up.Uploads(), 2u);
	cellField.Drain(sink);
	ASSERT_GE(sink.updates.size(), 1u);	// Drain coalesces per layer: the latest values win
	EXPECT_EQ(sink.updates.back().values[0], 7.0f);
}

TEST_F(ViewValueSourceTest, LiveToNotLiveWithSameEpochStillRereads)
{
	CellLayerRegistry registry(nullptr, &FakePuIdFn);
	ASSERT_EQ(registry.Register(temp.MakeDesc("temp", &grid.desc)), CellRegisterResult::Ok);
	source.live = true;
	ASSERT_TRUE(registry.SetViewValueSource(&source));
	LayerUploader up(cellField, nullptr);
	const CellLayerDesc* base = registry.Find(StringCRC("temp"));
	up.Update(base, registry);
	source.live = false;
	up.Update(base, registry);
	EXPECT_EQ(source.getValuesCalls, 1u);
	EXPECT_EQ(up.Uploads(), 2u);
}

TEST_F(ViewValueSourceTest, SecondInstallRejected)
{
	CellLayerRegistry registry(nullptr, &FakePuIdFn);
	StubViewValueSource other;
	ASSERT_TRUE(registry.SetViewValueSource(&source));
	EXPECT_FALSE(registry.SetViewValueSource(&other));
	EXPECT_EQ(registry.ViewValueSource(), &source);
}

TEST_F(ViewValueSourceTest, IsLiveFollowsSource)
{
	CellLayerRegistry registry(nullptr, &FakePuIdFn);
	ASSERT_TRUE(registry.SetViewValueSource(&source));
	source.live = false;
	EXPECT_FALSE(registry.IsLive());
	source.live = true;
	EXPECT_TRUE(registry.IsLive());
}

TEST_F(ViewValueSourceTest, PresentationOverridesRampAndRange)
{
	fx.Add(UploaderSchemaFixture::FloatEntry("temp", 0.0, 10.0, "sequential.grey"));
	const auto& svc = fx.Publish();
	CellLayerRegistry registry(&svc, &FakePuIdFn);
	ASSERT_EQ(registry.Register(temp.MakeDesc("temp", &grid.desc, CellValueKind::Float, "cells/temp")), CellRegisterResult::Ok);
	ASSERT_TRUE(registry.SetViewValueSource(&source));
	source.hasPresentation = true;
	source.presentation.rampId = StringCRC("test.heat");
	source.presentation.rangeMin = -3.0f;
	source.presentation.rangeMax = 3.0f;
	LayerUploader up(cellField, &svc);
	EXPECT_EQ(up.Update(registry.Find(StringCRC("temp")), registry), UploadResult::Ok);
	EXPECT_EQ(up.DrawRequest().colourMap, StringCRC("test.heat").Value());
	EXPECT_EQ(up.DrawRequest().rampMin, -3.0f);
	EXPECT_EQ(up.DrawRequest().rampMax, 3.0f);
}

TEST_F(ViewValueSourceTest, WithoutPresentationSchemaColouringKept)
{
	fx.Add(UploaderSchemaFixture::FloatEntry("temp", 0.0, 10.0, "test.heat"));
	const auto& svc = fx.Publish();
	CellLayerRegistry registry(&svc, &FakePuIdFn);
	ASSERT_EQ(registry.Register(temp.MakeDesc("temp", &grid.desc, CellValueKind::Float, "cells/temp")), CellRegisterResult::Ok);
	ASSERT_TRUE(registry.SetViewValueSource(&source));
	LayerUploader up(cellField, &svc);
	up.Update(registry.Find(StringCRC("temp")), registry);
	EXPECT_EQ(up.DrawRequest().colourMap, StringCRC("test.heat").Value());
	EXPECT_EQ(up.DrawRequest().rampMax, 10.0f);
}

TEST_F(ViewValueSourceTest, LegendCarriesPrefixRampAndRange)
{
	fx.Add(UploaderSchemaFixture::FloatEntry("temp", 0.0, 10.0, "sequential.grey"));
	const auto& svc = fx.Publish();
	Dia::ValueSchema::RampResolver resolver(svc);
	CellLayerLegend legend;
	const CellLayerDesc d = temp.MakeDesc("temp", &grid.desc, CellValueKind::Float, "cells/temp");
	ViewPresentation p;
	p.rampId = StringCRC("test.heat");
	p.rangeMin = -4.0f;
	p.rangeMax = 4.0f;
	strcpy(p.unitPrefix, "D");
	const PresentedLegend l = legend.BuildPresented(d, &svc, resolver, true, 0.0f, 1.0f, &p);
	EXPECT_STREQ(l.unitPrefix, "D");
	EXPECT_EQ(l.legend.mMin, -4.0f);
	EXPECT_EQ(l.legend.mMax, 4.0f);
	EXPECT_EQ(l.legend.mRamp.mRamp.mId, StringCRC("test.heat"));
	EXPECT_EQ(l.legend.mTicks[0], -4.0f);

	const PresentedLegend none = legend.BuildPresented(d, &svc, resolver, true, 0.0f, 1.0f, nullptr);
	EXPECT_STREQ(none.unitPrefix, "");
	EXPECT_EQ(none.legend.mMax, 10.0f);
}

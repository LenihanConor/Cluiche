#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayersModule.h>
#include <DiaCellLayers/LayerUploader.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/ViewValueSourceTestSupport.h>

#ifdef DIA_DEBUG
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#endif

#include <limits>
#include <random>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
using Dia::Graphics::CellFieldRegistry;

namespace
{
	struct ViewSourceEdgeTest : ::testing::Test
	{
		TestGrid grid{ 4, 3 };
		CellFieldRegistry cellField;
		RecordingSink sink;
		UploaderSchemaFixture fx;
		StubProvider temp, rain;
		StubViewValueSource src;
		CellLayerRegistry registry{ nullptr, &FakePuIdFn };

		void SetUp() override
		{
			FakePuId() = 1;
			temp.values = { 5, -2, 7, 3, 1, 0, 4, 9, 2, 2, 2, 2 };
			rain.values = std::vector<float>(12, 1.0f);
			src.values = std::vector<float>(12, 42.0f);
		}
		void Use(const Dia::ValueSchema::ValueSchemaService* svc) { registry.~CellLayerRegistry(); new (&registry) CellLayerRegistry(svc, &FakePuIdFn); }
		const CellLayerDesc* L(const char* id) { return registry.Find(StringCRC(id)); }
		void Reg(const CellLayerDesc& d) { ASSERT_EQ(registry.Register(d), CellRegisterResult::Ok); }
	};

	struct ExposedModule : CellLayersModule
	{
		explicit ExposedModule(const char* id) : CellLayersModule(StringCRC(id)) { SetCurrentPuIdFn(&FakePuIdFn); }
		void Tick() { DoUpdate(0.016f); }
	};
}

TEST_F(ViewSourceEdgeTest, LiveNotLiveTransitionsReadOncePerTransition)
{
	Reg(temp.MakeDesc("temp", &grid.desc));
	registry.SetViewValueSource(&src);
	src.live = true;
	LayerUploader up(cellField, nullptr);
	up.Update(L("temp"), registry);
	EXPECT_EQ(temp.readCalls, 1u);
	src.live = false;
	up.Update(L("temp"), registry);
	up.Update(L("temp"), registry);
	EXPECT_EQ(src.getValuesCalls, 1u);
	src.live = true;
	up.Update(L("temp"), registry);
	up.Update(L("temp"), registry);
	EXPECT_EQ(temp.readCalls, 2u);
	EXPECT_EQ(up.Reads(), 3u);
	EXPECT_EQ(up.Uploads(), 3u);
}

TEST_F(ViewSourceEdgeTest, EachEpochChangeReuploadsOnceIncludingWrap)
{
	Reg(temp.MakeDesc("temp", &grid.desc));
	registry.SetViewValueSource(&src);
	LayerUploader up(cellField, nullptr);
	const uint64_t epochs[] = { 1, 2, std::numeric_limits<uint64_t>::max(), 0, 1 };
	uint64_t expected = 0;
	for (uint64_t e : epochs)
	{
		src.epoch = e;
		up.Update(L("temp"), registry);
		up.Update(L("temp"), registry);
		++expected;
		ASSERT_EQ(up.Reads(), expected) << e;
		ASSERT_EQ(up.Uploads(), expected) << e;
	}
	EXPECT_EQ(src.getValuesCalls, 5u);
	EXPECT_EQ(temp.readCalls, 0u);
}

TEST_F(ViewSourceEdgeTest, LiveSourceEpochChangeRereadsLayerOnce)
{
	Reg(temp.MakeDesc("temp", &grid.desc));
	src.live = true;
	registry.SetViewValueSource(&src);
	LayerUploader up(cellField, nullptr);
	up.Update(L("temp"), registry);
	src.epoch = 7;
	up.Update(L("temp"), registry);
	up.Update(L("temp"), registry);
	EXPECT_EQ(temp.readCalls, 2u);
	EXPECT_EQ(src.getValuesCalls, 0u);
}

TEST_F(ViewSourceEdgeTest, InstallingLiveSourceAfterUploadCostsAtMostOneReread)
{
	Reg(temp.MakeDesc("temp", &grid.desc));
	LayerUploader up(cellField, nullptr);
	up.Update(L("temp"), registry);
	src.live = true;
	registry.SetViewValueSource(&src);
	for (int i = 0; i < 10; ++i) { up.Update(L("temp"), registry); }
	EXPECT_LE(temp.readCalls, 2u);
}

TEST_F(ViewSourceEdgeTest, ClearingNotLiveSourceReturnsToLiveValues)
{
	Reg(temp.MakeDesc("temp", &grid.desc));
	registry.SetViewValueSource(&src);
	LayerUploader up(cellField, nullptr);
	up.Update(L("temp"), registry);
	uint32_t n = 0;
	EXPECT_EQ(up.BaseValues(StringCRC("temp"), n)[0], 42.0f);
	registry.ClearViewValueSource();
	EXPECT_EQ(up.Update(L("temp"), registry), UploadResult::Ok);
	EXPECT_EQ(up.BaseValues(StringCRC("temp"), n)[0], 5.0f);
	EXPECT_EQ(temp.readCalls, 1u);
}

TEST_F(ViewSourceEdgeTest, GetValuesFalseGivesNoDataClearsEverythingThenRecovers)
{
	Reg(temp.MakeDesc("temp", &grid.desc));
	registry.SetViewValueSource(&src);
	LayerUploader up(cellField, nullptr);
	ASSERT_EQ(up.Update(L("temp"), registry), UploadResult::Ok);
	src.hasData = false;
	src.epoch = 2;
	EXPECT_EQ(up.Update(L("temp"), registry), UploadResult::NoData);
	EXPECT_FALSE(up.DrawRequest().layer.IsValid());
	uint32_t n = 0;
	EXPECT_EQ(up.BaseValues(StringCRC("temp"), n), nullptr);
	cellField.Drain(sink);
	EXPECT_EQ(cellField.GetStats().layersLive, 0u);
	EXPECT_EQ(temp.readCalls, 0u);
	EXPECT_EQ(up.ReadFailures(), 0u);
	src.hasData = true;
	EXPECT_EQ(up.Update(L("temp"), registry), UploadResult::Ok);
	EXPECT_TRUE(up.DrawRequest().layer.IsValid());
	EXPECT_NE(up.BaseValues(StringCRC("temp"), n), nullptr);
}

TEST_F(ViewSourceEdgeTest, NoDataRepeatedlyDoesNotReadLiveOrLeakLayers)
{
	Reg(temp.MakeDesc("temp", &grid.desc));
	src.hasData = false;
	registry.SetViewValueSource(&src);
	LayerUploader up(cellField, nullptr);
	for (int i = 0; i < 50; ++i) { ASSERT_EQ(up.Update(L("temp"), registry), UploadResult::NoData); cellField.Drain(sink); }
	EXPECT_EQ(temp.readCalls, 0u);
	EXPECT_EQ(cellField.GetStats().layersLive, 0u);
}

TEST_F(ViewSourceEdgeTest, NoDataThenLiveReadsLayer)
{
	Reg(temp.MakeDesc("temp", &grid.desc));
	src.hasData = false;
	registry.SetViewValueSource(&src);
	LayerUploader up(cellField, nullptr);
	EXPECT_EQ(up.Update(L("temp"), registry), UploadResult::NoData);
	src.live = true;
	EXPECT_EQ(up.Update(L("temp"), registry), UploadResult::Ok);
	EXPECT_EQ(temp.readCalls, 1u);
}

TEST_F(ViewSourceEdgeTest, PresentationWithUnknownRampKeepsSchemaRampButUsesRange)
{
	fx.Add(UploaderSchemaFixture::FloatEntry("temp", 0, 10, "test.heat"));
	const auto& svc = fx.Publish();
	Use(&svc);
	Reg(temp.MakeDesc("temp", &grid.desc, CellValueKind::Float, "cells/temp"));
	src.hasPresentation = true;
	src.presentation.rampId = StringCRC("no.such.ramp");
	src.presentation.rangeMin = -3.0f;
	src.presentation.rangeMax = 3.0f;
	registry.SetViewValueSource(&src);
	LayerUploader up(cellField, &svc);
	ASSERT_EQ(up.Update(L("temp"), registry), UploadResult::Ok);
	EXPECT_EQ(up.DrawRequest().colourMap, StringCRC("test.heat").Value());
	EXPECT_EQ(up.DrawRequest().rampMin, -3.0f);
	EXPECT_EQ(up.DrawRequest().rampMax, 3.0f);
}

TEST_F(ViewSourceEdgeTest, PresentationWithKnownRampReplacesColourMap)
{
	fx.Add(UploaderSchemaFixture::FloatEntry("temp", 0, 10, "sequential.grey"));
	const auto& svc = fx.Publish();
	Use(&svc);
	Reg(temp.MakeDesc("temp", &grid.desc, CellValueKind::Float, "cells/temp"));
	src.hasPresentation = true;
	src.presentation.rampId = StringCRC("test.heat");
	registry.SetViewValueSource(&src);
	LayerUploader up(cellField, &svc);
	ASSERT_EQ(up.Update(L("temp"), registry), UploadResult::Ok);
	EXPECT_EQ(up.DrawRequest().colourMap, StringCRC("test.heat").Value());
}

TEST_F(ViewSourceEdgeTest, PresentationIgnoredForCategoryLayers)
{
	fx.Add(UploaderSchemaFixture::EnumEntry("biome", 3));
	const auto& svc = fx.Publish();
	Use(&svc);
	Reg(rain.MakeDesc("biome", &grid.desc, CellValueKind::Category, "cells/biome"));
	src.hasPresentation = true;
	src.presentation.rampId = StringCRC("test.heat");
	registry.SetViewValueSource(&src);
	LayerUploader up(cellField, &svc);
	ASSERT_EQ(up.Update(L("biome"), registry), UploadResult::Ok);
	EXPECT_EQ(up.DrawRequest().colourMap, StringCRC("cells/biome").Value());
}

TEST_F(ViewSourceEdgeTest, SourceWithoutPresentationOnUnknownSchemaStillUsesGrey)
{
	Reg(temp.MakeDesc("temp", &grid.desc));
	registry.SetViewValueSource(&src);
	LayerUploader up(cellField, nullptr);
	ASSERT_EQ(up.Update(L("temp"), registry), UploadResult::Ok);
	EXPECT_EQ(up.DrawRequest().colourMap, StringCRC("sequential.grey").Value());
	EXPECT_EQ(up.ObservedMin(), 42.0f);
	EXPECT_EQ(up.ObservedMax(), 42.0f);
}

TEST_F(ViewSourceEdgeTest, RandomSourceStateWalkKeepsReadsEqualToUploads)
{
	Reg(temp.MakeDesc("temp", &grid.desc));
	Reg(rain.MakeDesc("rain", &grid.desc));
	registry.SetViewValueSource(&src);
	LayerUploader up(cellField, nullptr);
	std::mt19937 rng(31337);
	for (int i = 0; i < 800; ++i)
	{
		switch (rng() % 5)
		{
		case 0: src.live = !src.live; break;
		case 1: src.epoch = rng() % 4; break;
		case 2: src.hasData = (rng() % 4) != 0; src.epoch++; break;	// contract: availability change bumps the epoch
		case 3: temp.versionValue = rng() % 3; break;
		default: break;
		}
		const UploadResult r = up.Update(L(rng() % 2 ? "temp" : "rain"), registry);
		const bool expectNoData = !src.live && !src.hasData;
		ASSERT_EQ(r, expectNoData ? UploadResult::NoData : UploadResult::Ok) << i;
		ASSERT_EQ(up.ReadFailures(), 0u);
		cellField.Drain(sink);
		ASSERT_LE(cellField.GetStats().layersLive, 1u);
	}
	EXPECT_EQ(up.Reads(), up.Uploads());
}

#ifdef DIA_DEBUG
TEST_F(ViewSourceEdgeTest, ViewerNoDataFlagAppearsAndClearsInJSON)
{
	Reg(temp.MakeDesc("temp", &grid.desc));
	registry.SetViewValueSource(&src);
	LayerUploader up(cellField, nullptr);
	CellLayerViewerDomain domain(registry, up, nullptr);
	FakeDebugDraw draw;
	src.hasData = false;
	domain.Update(draw);
	Json::Value j;
	domain.GetJSONState(j);
	EXPECT_TRUE(j["noData"].asBool());
	EXPECT_FALSE(j["legend"]["available"].asBool());
	src.hasData = true;
	domain.Update(draw);
	Json::Value j2;
	domain.GetJSONState(j2);
	EXPECT_FALSE(j2.isMember("noData"));
}
#endif

TEST(CellLayersModuleEdge, GetRegistryNullBeforeStart)
{
	ExposedModule m("m0");
	EXPECT_EQ(m.GetRegistry(), nullptr);
	const ExposedModule& cm = m;
	EXPECT_EQ(cm.GetRegistry(), nullptr);
}

TEST(CellLayersModuleEdge, DoUpdateIsANoOp)
{
	ExposedModule m("m1");
	m.Tick();
	EXPECT_EQ(m.GetRegistry(), nullptr);
}

TEST(CellLayersModuleEdge, SchemaAndTypeConstantsAreStable)
{
	EXPECT_STREQ(CellLayersModule::kServiceStreamId, "CellLayerRegistryService");
	EXPECT_EQ(CellLayersModule::kTypeId, StringCRC("CellLayersModule"));
}

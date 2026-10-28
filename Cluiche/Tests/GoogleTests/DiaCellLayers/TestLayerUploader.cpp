#include <gtest/gtest.h>
#include <DiaCellLayers/LayerUploader.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
using Dia::Graphics::CellFieldRegistry;

namespace
{
	struct LayerUploaderTest : ::testing::Test
	{
		TestGrid grid{ 4, 3 };
		CellFieldRegistry cellField;
		RecordingSink sink;
		UploaderSchemaFixture fx;
		StubProvider temp, rain, height;

		void SetUp() override
		{
			FakePuId() = 1;
			temp.values = { 5, -2, 7, 3, 1, 0, 4, 9, 2, 2, 2, 2 };
			rain.values = std::vector<float>(12, 1.0f);
			height.values = std::vector<float>(12, 2.0f);
		}
	};

	// Registers layers into a registry for each test.
	struct Rig
	{
		CellLayerRegistry registry;
		explicit Rig(const Dia::ValueSchema::ValueSchemaService* s) : registry(s, &FakePuIdFn) {}
	};

	void Add(Rig& rig, const CellLayerDesc& d) { ASSERT_EQ(rig.registry.Register(d), CellRegisterResult::Ok); }
}

TEST_F(LayerUploaderTest, UnchangedBaseAndVersionDoesNoReadOrUpload)
{
	Rig rig(nullptr);
	Add(rig, temp.MakeDesc("temp", &grid.desc));
	LayerUploader up(cellField, nullptr);
	const CellLayerDesc* base = rig.registry.Find(StringCRC("temp"));
	EXPECT_EQ(up.Update(base, rig.registry), UploadResult::Ok);
	for (int i = 0; i < 50; ++i) { EXPECT_EQ(up.Update(base, rig.registry), UploadResult::Ok); }
	EXPECT_EQ(temp.readCalls, 1u);
	EXPECT_EQ(up.Reads(), 1u);
	EXPECT_EQ(up.Uploads(), 1u);
	EXPECT_EQ(up.SkippedFrames(), 50u);
	cellField.Drain(sink);
	EXPECT_EQ(sink.updates.size(), 1u);
}

TEST_F(LayerUploaderTest, VersionChangeCausesOneReadAndOneUpload)
{
	Rig rig(nullptr);
	Add(rig, temp.MakeDesc("temp", &grid.desc));
	LayerUploader up(cellField, nullptr);
	const CellLayerDesc* base = rig.registry.Find(StringCRC("temp"));
	up.Update(base, rig.registry);
	temp.versionValue = 2;
	up.Update(base, rig.registry);
	up.Update(base, rig.registry);
	EXPECT_EQ(up.Reads(), 2u);
	EXPECT_EQ(up.Uploads(), 2u);
	EXPECT_EQ(temp.readCalls, 2u);
}

TEST_F(LayerUploaderTest, BaseSwitchIsOneReadAndOneUploadAndDestroysPrevious)
{
	Rig rig(nullptr);
	Add(rig, temp.MakeDesc("temp", &grid.desc));
	Add(rig, rain.MakeDesc("rain", &grid.desc));
	LayerUploader up(cellField, nullptr);
	up.Update(rig.registry.Find(StringCRC("temp")), rig.registry);
	const uint64_t reads0 = up.Reads(), uploads0 = up.Uploads();
	up.Update(rig.registry.Find(StringCRC("rain")), rig.registry);
	EXPECT_EQ(up.Reads() - reads0, 1u);
	EXPECT_EQ(up.Uploads() - uploads0, 1u);
	EXPECT_EQ(rain.readCalls, 1u);
	cellField.Drain(sink);
	EXPECT_EQ(sink.creates, 2u);
	EXPECT_EQ(sink.destroys, 1u);
	EXPECT_EQ(cellField.GetStats().layersLive, 1u);
}

TEST_F(LayerUploaderTest, ObservedMinMax)
{
	Rig rig(nullptr);
	Add(rig, temp.MakeDesc("temp", &grid.desc));
	LayerUploader up(cellField, nullptr);
	up.Update(rig.registry.Find(StringCRC("temp")), rig.registry);
	EXPECT_FLOAT_EQ(up.ObservedMin(), -2.0f);
	EXPECT_FLOAT_EQ(up.ObservedMax(), 9.0f);
	// Without schema range the draw request uses the observed range.
	EXPECT_FLOAT_EQ(up.DrawRequest().rampMin, -2.0f);
	EXPECT_FLOAT_EQ(up.DrawRequest().rampMax, 9.0f);
}

TEST_F(LayerUploaderTest, UploadedValuesMatchLayerRead)
{
	Rig rig(nullptr);
	Add(rig, temp.MakeDesc("temp", &grid.desc));
	LayerUploader up(cellField, nullptr);
	up.Update(rig.registry.Find(StringCRC("temp")), rig.registry);
	cellField.Drain(sink);
	ASSERT_EQ(sink.updates.size(), 1u);
	EXPECT_EQ(sink.updates[0].values, temp.values);
}

TEST_F(LayerUploaderTest, RampRegisteredOnceAcrossFramesAndSwitches)
{
	fx.Add(UploaderSchemaFixture::FloatEntry("temp", -10, 40, "test.heat"));
	fx.Add(UploaderSchemaFixture::FloatEntry("rain", 0, 5, "test.heat"));
	const auto& svc = fx.Publish();
	Rig rig(&svc);
	Add(rig, temp.MakeDesc("temp", &grid.desc, CellValueKind::Float, "cells/temp"));
	Add(rig, rain.MakeDesc("rain", &grid.desc, CellValueKind::Float, "cells/rain"));
	LayerUploader up(cellField, &svc);
	for (int i = 0; i < 3; ++i)
	{
		up.Update(rig.registry.Find(StringCRC("temp")), rig.registry);
		up.Update(rig.registry.Find(StringCRC("rain")), rig.registry);
	}
	EXPECT_EQ(up.RegisteredColourMapCount(), 1u);
	Dia::Graphics::CellFieldRampData ramp;
	ASSERT_TRUE(cellField.GetRamp(StringCRC("test.heat").Value(), ramp));
	EXPECT_EQ(ramp.revision, 1u);
	EXPECT_EQ(ramp.count, 2u);
	EXPECT_EQ(ramp.stops[1].rgba8, 0xFF0000FFu);
	EXPECT_EQ(up.DrawRequest().colourMap, StringCRC("test.heat").Value());
}

TEST_F(LayerUploaderTest, CategoryLayerRegistersPaletteFromEnum)
{
	fx.Add(UploaderSchemaFixture::EnumEntry("biome", 3));
	const auto& svc = fx.Publish();
	Rig rig(&svc);
	rain.values = { 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2 };
	Add(rig, rain.MakeDesc("biome", &grid.desc, CellValueKind::Category, "cells/biome"));
	LayerUploader up(cellField, &svc);
	up.Update(rig.registry.Find(StringCRC("biome")), rig.registry);
	up.Update(rig.registry.Find(StringCRC("biome")), rig.registry);
	EXPECT_EQ(up.RegisteredColourMapCount(), 1u);
	Dia::Graphics::CellFieldPaletteData pal;
	ASSERT_TRUE(cellField.GetPalette(up.DrawRequest().colourMap, pal));
	EXPECT_EQ(pal.count, 3u);
	EXPECT_EQ(pal.revision, 1u);
	EXPECT_EQ(up.DrawRequest().colourMap, StringCRC("cells/biome").Value());
}

TEST_F(LayerUploaderTest, UnknownRampFallsBackToGrey)
{
	fx.Add(UploaderSchemaFixture::FloatEntry("temp", 0, 1, "no.such.ramp"));
	const auto& svc = fx.Publish();
	Rig rig(&svc);
	Add(rig, temp.MakeDesc("temp", &grid.desc, CellValueKind::Float, "cells/temp"));
	LayerUploader up(cellField, &svc);
	EXPECT_EQ(up.Update(rig.registry.Find(StringCRC("temp")), rig.registry), UploadResult::Ok);
	EXPECT_EQ(up.DrawRequest().colourMap, StringCRC("sequential.grey").Value());
	Dia::Graphics::CellFieldRampData ramp;
	EXPECT_TRUE(cellField.GetRamp(StringCRC("sequential.grey").Value(), ramp));
}

TEST_F(LayerUploaderTest, DrawRequestFieldsFromSchema)
{
	auto e = UploaderSchemaFixture::FloatEntry("temp", -10, 40, "test.heat");
	e.mUi["inset"] = 0.1;
	e.mUi["backdrop"] = "#102030";
	fx.Add(e);
	const auto& svc = fx.Publish();
	Rig rig(&svc);
	Add(rig, temp.MakeDesc("temp", &grid.desc, CellValueKind::Float, "cells/temp"));
	LayerUploader up(cellField, &svc);
	up.Update(rig.registry.Find(StringCRC("temp")), rig.registry);
	const auto& r = up.DrawRequest();
	EXPECT_TRUE(r.layer.IsValid());
	EXPECT_FLOAT_EQ(r.rampMin, -10.0f);
	EXPECT_FLOAT_EQ(r.rampMax, 40.0f);
	EXPECT_FLOAT_EQ(r.inset, 0.1f);
	EXPECT_EQ(r.backdropColour, 0x102030FFu);
	EXPECT_FALSE(r.elevationLayer.IsValid());
	EXPECT_FLOAT_EQ(r.hillshadeStrength, 0.0f);
}

TEST_F(LayerUploaderTest, ElevationLayerUploadedOnlyWhenSchemaNamesIt)
{
	auto e = UploaderSchemaFixture::FloatEntry("temp", -10, 40, "test.heat");
	e.mUi["elevation_layer"] = "height";
	fx.Add(e);
	fx.Add(UploaderSchemaFixture::FloatEntry("rain", 0, 5));
	const auto& svc = fx.Publish();
	Rig rig(&svc);
	Add(rig, temp.MakeDesc("temp", &grid.desc, CellValueKind::Float, "cells/temp"));
	Add(rig, rain.MakeDesc("rain", &grid.desc, CellValueKind::Float, "cells/rain"));
	Add(rig, height.MakeDesc("height", &grid.desc));
	LayerUploader up(cellField, &svc);

	up.Update(rig.registry.Find(StringCRC("temp")), rig.registry);
	EXPECT_TRUE(up.DrawRequest().elevationLayer.IsValid());
	EXPECT_NE(up.DrawRequest().elevationLayer.index, up.DrawRequest().layer.index);
	EXPECT_FLOAT_EQ(up.DrawRequest().hillshadeStrength, 1.0f);
	EXPECT_EQ(height.readCalls, 1u);
	up.Update(rig.registry.Find(StringCRC("temp")), rig.registry);
	EXPECT_EQ(height.readCalls, 1u);

	// Switching to a base without elevation drops hillshade and the elevation CellField layer.
	up.Update(rig.registry.Find(StringCRC("rain")), rig.registry);
	EXPECT_FALSE(up.DrawRequest().elevationLayer.IsValid());
	EXPECT_FLOAT_EQ(up.DrawRequest().hillshadeStrength, 0.0f);
	cellField.Drain(sink);
	EXPECT_EQ(cellField.GetStats().layersLive, 1u);
}

TEST_F(LayerUploaderTest, LargeGridSwitchCostsOneReadAndOneUpload)
{
	TestGrid large(104, 64);
	StubProvider a, b;
	a.values = std::vector<float>(large.CellCount(), 1.0f);
	b.values = std::vector<float>(large.CellCount(), 3.0f);
	Rig rig(nullptr);
	Add(rig, a.MakeDesc("a", &large.desc));
	Add(rig, b.MakeDesc("b", &large.desc));
	LayerUploader up(cellField, nullptr);
	ASSERT_EQ(up.Update(rig.registry.Find(StringCRC("a")), rig.registry), UploadResult::Ok);
	const uint64_t r0 = up.Reads(), u0 = up.Uploads();
	ASSERT_EQ(up.Update(rig.registry.Find(StringCRC("b")), rig.registry), UploadResult::Ok);
	EXPECT_EQ(up.Reads() - r0, 1u);
	EXPECT_EQ(up.Uploads() - u0, 1u);
	cellField.Drain(sink);
	ASSERT_EQ(sink.updates.size(), 2u);
	EXPECT_EQ(sink.updates[1].values.size(), 104u * 64u);
}

TEST_F(LayerUploaderTest, ReadFailureWhenNoReadDelegate)
{
	Rig rig(nullptr);
	CellLayerDesc d = temp.MakeDesc("temp", &grid.desc);
	d.read = fastdelegate::FastDelegate2<float*, uint32_t>();
	Add(rig, d);
	LayerUploader up(cellField, nullptr);
	EXPECT_EQ(up.Update(rig.registry.Find(StringCRC("temp")), rig.registry), UploadResult::ReadFailed);
	EXPECT_EQ(up.ReadFailures(), 1u);
	EXPECT_EQ(up.Uploads(), 0u);
	cellField.Drain(sink);
	EXPECT_TRUE(sink.updates.empty());
}

TEST_F(LayerUploaderTest, ReadFailureWhenGridInvalid)
{
	Dia::Graphics::CellGridDesc bad;	// zero size
	Rig rig(nullptr);
	Add(rig, temp.MakeDesc("temp", &bad));
	LayerUploader up(cellField, nullptr);
	EXPECT_EQ(up.Update(rig.registry.Find(StringCRC("temp")), rig.registry), UploadResult::ReadFailed);
	EXPECT_EQ(up.ReadFailures(), 1u);
	EXPECT_EQ(up.Uploads(), 0u);
	EXPECT_EQ(temp.readCalls, 0u);
}

TEST_F(LayerUploaderTest, NullBaseReturnsNoBaseAndReleasesLayers)
{
	Rig rig(nullptr);
	Add(rig, temp.MakeDesc("temp", &grid.desc));
	LayerUploader up(cellField, nullptr);
	up.Update(rig.registry.Find(StringCRC("temp")), rig.registry);
	EXPECT_EQ(up.Update(nullptr, rig.registry), UploadResult::NoBase);
	cellField.Drain(sink);
	EXPECT_EQ(cellField.GetStats().layersLive, 0u);
}

TEST_F(LayerUploaderTest, LayerCapReturnsCreatePendingThenRetriesWithoutReading)
{
	Rig rig(nullptr);
	Add(rig, temp.MakeDesc("temp", &grid.desc));
	Dia::Core::CellFieldLayerHandle fillers[CellFieldRegistry::kMaxLayers];
	for (uint32_t i = 0; i < CellFieldRegistry::kMaxLayers; ++i)
	{
		ASSERT_TRUE(cellField.CreateLayer(grid.desc, Dia::Graphics::CellFieldValueKind::Float, fillers[i]).ok);
	}
	LayerUploader up(cellField, nullptr);
	const CellLayerDesc* base = rig.registry.Find(StringCRC("temp"));
	EXPECT_EQ(up.Update(base, rig.registry), UploadResult::CreatePending);
	EXPECT_EQ(temp.readCalls, 0u);
	cellField.Destroy(fillers[0]);
	cellField.Drain(sink);
	EXPECT_EQ(up.Update(base, rig.registry), UploadResult::Ok);
	EXPECT_EQ(temp.readCalls, 1u);
	EXPECT_EQ(up.Uploads(), 1u);
}

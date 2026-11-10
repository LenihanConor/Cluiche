#include <gtest/gtest.h>
#include <DiaCellLayers/LayerUploader.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/ViewValueSourceTestSupport.h>

#include <limits>
#include <random>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
using Dia::Graphics::CellFieldRegistry;

namespace
{
	const float kNaN = std::numeric_limits<float>::quiet_NaN();
	const float kInf = std::numeric_limits<float>::infinity();

	struct UploaderEdgeTest : ::testing::Test
	{
		TestGrid grid{ 4, 3 };
		TestGrid otherGrid{ 3, 3 };
		CellFieldRegistry cellField;
		RecordingSink sink;
		UploaderSchemaFixture fx;
		StubProvider temp, rain, height;
		CellLayerRegistry registry{ nullptr, &FakePuIdFn };

		void SetUp() override
		{
			FakePuId() = 1;
			temp.values = { 5, -2, 7, 3, 1, 0, 4, 9, 2, 2, 2, 2 };
			rain.values = std::vector<float>(12, 1.0f);
			height.values = std::vector<float>(12, 2.0f);
		}
		void Reg(const CellLayerDesc& d) { ASSERT_EQ(registry.Register(d), CellRegisterResult::Ok); }
		const CellLayerDesc* L(const char* id) { return registry.Find(StringCRC(id)); }
		void UseSchema(const Dia::ValueSchema::ValueSchemaService& svc) { registry.~CellLayerRegistry(); new (&registry) CellLayerRegistry(&svc, &FakePuIdFn); }
	};
}

TEST_F(UploaderEdgeTest, MissingVersionDelegateIsReadFailure)
{
	CellLayerDesc d = temp.MakeDesc("temp", &grid.desc);
	d.version = fastdelegate::FastDelegate0<uint64_t>();
	Reg(d);
	LayerUploader up(cellField, nullptr);
	EXPECT_EQ(up.Update(L("temp"), registry), UploadResult::ReadFailed);
	EXPECT_EQ(up.ReadFailures(), 1u);
	EXPECT_EQ(up.Reads(), 0u);
}

TEST_F(UploaderEdgeTest, NullGridIsReadFailure)
{
	// The registry rejects a null grid, but the uploader still defends itself when handed such a descriptor directly.
	const CellLayerDesc d = temp.MakeDesc("temp", nullptr);
	EXPECT_EQ(registry.Register(d), CellRegisterResult::NullGrid);
	LayerUploader up(cellField, nullptr);
	EXPECT_EQ(up.Update(&d, registry), UploadResult::ReadFailed);
	EXPECT_EQ(temp.readCalls, 0u);
}

TEST_F(UploaderEdgeTest, ReadFailureClearsBaseValuesAndRecoversOnceFixed)
{
	Reg(temp.MakeDesc("temp", &grid.desc));
	LayerUploader up(cellField, nullptr);
	ASSERT_EQ(up.Update(L("temp"), registry), UploadResult::Ok);
	uint32_t n = 0;
	ASSERT_NE(up.BaseValues(StringCRC("temp"), n), nullptr);
	CellLayerDesc broken = *L("temp");
	broken.read = fastdelegate::FastDelegate2<float*, uint32_t>();
	EXPECT_EQ(up.Update(&broken, registry), UploadResult::ReadFailed);
	EXPECT_EQ(up.BaseValues(StringCRC("temp"), n), nullptr);
	EXPECT_EQ(n, 0u);
	EXPECT_EQ(up.Update(L("temp"), registry), UploadResult::Ok);
}

// BUG (production): the early "base not readable" return in Update() clears mBaseCopyCount but keeps the held slot, so a
// recovery with the same id and unchanged version() is version-gated and BaseValues() stays null (hover shows nothing)
// until version() changes. Repro: Update(good); Update(copy of good with empty read) -> ReadFailed; Update(good) -> Ok.
TEST_F(UploaderEdgeTest, BaseValuesAvailableAgainAfterReadFailureRecoveryWithSameVersion)
{
	Reg(temp.MakeDesc("temp", &grid.desc));
	LayerUploader up(cellField, nullptr);
	ASSERT_EQ(up.Update(L("temp"), registry), UploadResult::Ok);
	CellLayerDesc broken = *L("temp");
	broken.read = fastdelegate::FastDelegate2<float*, uint32_t>();
	ASSERT_EQ(up.Update(&broken, registry), UploadResult::ReadFailed);
	ASSERT_EQ(up.Update(L("temp"), registry), UploadResult::Ok);
	uint32_t n = 0;
	EXPECT_NE(up.BaseValues(StringCRC("temp"), n), nullptr);
}

TEST_F(UploaderEdgeTest, CellFieldRejectingGridIsReadFailureNotPending)
{
	TestGrid bad(4, 3);
	bad.desc.centresXY = nullptr;
	Reg(temp.MakeDesc("temp", &bad.desc));
	LayerUploader up(cellField, nullptr);
	EXPECT_EQ(up.Update(L("temp"), registry), UploadResult::ReadFailed);
	EXPECT_EQ(up.ReadFailures(), 1u);
	EXPECT_EQ(temp.readCalls, 0u);
}

TEST_F(UploaderEdgeTest, CreatePendingRepeatsUntilSlotFreedAndNeverReads)
{
	Reg(temp.MakeDesc("temp", &grid.desc));
	Dia::Core::CellFieldLayerHandle fillers[CellFieldRegistry::kMaxLayers];
	for (auto& f : fillers) { ASSERT_TRUE(cellField.CreateLayer(grid.desc, Dia::Graphics::CellFieldValueKind::Float, f).ok); }
	LayerUploader up(cellField, nullptr);
	for (int i = 0; i < 5; ++i) { EXPECT_EQ(up.Update(L("temp"), registry), UploadResult::CreatePending); }
	EXPECT_EQ(up.Reads(), 0u);
	EXPECT_EQ(up.ReadFailures(), 0u);
	cellField.Destroy(fillers[3]);
	EXPECT_EQ(up.Update(L("temp"), registry), UploadResult::CreatePending);	// slot reusable only after Drain
	cellField.Drain(sink);
	EXPECT_EQ(up.Update(L("temp"), registry), UploadResult::Ok);
	EXPECT_EQ(up.Reads(), 1u);
}

TEST_F(UploaderEdgeTest, ElevationPendingAtCapDrawsFlatThenRecovers)
{
	fx.Add([] { auto e = UploaderSchemaFixture::FloatEntry("temp", 0, 10); e.mUi["elevation_layer"] = "height"; return e; }());
	const auto& svc = fx.Publish();
	UseSchema(svc);
	Reg(temp.MakeDesc("temp", &grid.desc, CellValueKind::Float, "cells/temp"));
	Reg(height.MakeDesc("height", &grid.desc));
	Dia::Core::CellFieldLayerHandle fillers[CellFieldRegistry::kMaxLayers - 1];
	for (auto& f : fillers) { ASSERT_TRUE(cellField.CreateLayer(grid.desc, Dia::Graphics::CellFieldValueKind::Float, f).ok); }
	LayerUploader up(cellField, &svc);
	EXPECT_EQ(up.Update(L("temp"), registry), UploadResult::CreatePending);
	EXPECT_TRUE(up.DrawRequest().layer.IsValid());
	EXPECT_FALSE(up.DrawRequest().elevationLayer.IsValid());
	EXPECT_EQ(height.readCalls, 0u);
	cellField.Destroy(fillers[0]);
	cellField.Drain(sink);
	EXPECT_EQ(up.Update(L("temp"), registry), UploadResult::Ok);
	EXPECT_TRUE(up.DrawRequest().elevationLayer.IsValid());
	EXPECT_EQ(temp.readCalls, 1u);
	EXPECT_EQ(height.readCalls, 1u);
}

TEST_F(UploaderEdgeTest, BaseSwitchBackAndForthCostsExactlyOneReadAndUploadEach)
{
	Reg(temp.MakeDesc("temp", &grid.desc));
	Reg(rain.MakeDesc("rain", &grid.desc));
	LayerUploader up(cellField, nullptr);
	up.Update(L("temp"), registry);
	for (int i = 1; i <= 100; ++i)
	{
		const uint64_t r0 = up.Reads(), u0 = up.Uploads();
		ASSERT_EQ(up.Update(L(i & 1 ? "rain" : "temp"), registry), UploadResult::Ok);
		ASSERT_EQ(up.Reads() - r0, 1u) << i;
		ASSERT_EQ(up.Uploads() - u0, 1u) << i;
		ASSERT_EQ(up.Update(L(i & 1 ? "rain" : "temp"), registry), UploadResult::Ok);
		ASSERT_EQ(up.Reads() - r0, 1u) << i;
		cellField.Drain(sink);
		ASSERT_EQ(cellField.GetStats().layersLive, 1u) << i;
	}
	EXPECT_EQ(temp.readCalls + rain.readCalls, 101u);
}

TEST_F(UploaderEdgeTest, VersionWrapsThroughMaxToZeroRereads)
{
	Reg(temp.MakeDesc("temp", &grid.desc));
	LayerUploader up(cellField, nullptr);
	temp.versionValue = 0;
	up.Update(L("temp"), registry);
	up.Update(L("temp"), registry);
	EXPECT_EQ(up.Reads(), 1u);	// newly created slot reads even at version 0, then gates
	temp.versionValue = std::numeric_limits<uint64_t>::max();
	up.Update(L("temp"), registry);
	up.Update(L("temp"), registry);
	EXPECT_EQ(up.Reads(), 2u);
	temp.versionValue = 0;
	up.Update(L("temp"), registry);
	EXPECT_EQ(up.Reads(), 3u);
}

TEST_F(UploaderEdgeTest, SameLayerIdMovedToOtherGridIsRecreated)
{
	TestGrid big(5, 5);
	StubProvider other;
	other.values = std::vector<float>(25, 8.0f);
	Reg(temp.MakeDesc("temp", &grid.desc));
	LayerUploader up(cellField, nullptr);
	up.Update(L("temp"), registry);
	registry.Unregister(StringCRC("temp"));
	Reg(other.MakeDesc("temp", &big.desc));
	EXPECT_EQ(up.Update(L("temp"), registry), UploadResult::Ok);
	uint32_t n = 0;
	const float* v = up.BaseValues(StringCRC("temp"), n);
	ASSERT_NE(v, nullptr);
	EXPECT_EQ(n, 25u);
	EXPECT_EQ(v[24], 8.0f);
	EXPECT_EQ(up.Reads(), 2u);
}

TEST_F(UploaderEdgeTest, ElevationLayerMissingSelfOrOnOtherGridIsIgnored)
{
	fx.Add([] { auto e = UploaderSchemaFixture::FloatEntry("a", 0, 10); e.mUi["elevation_layer"] = "nope"; return e; }());
	fx.Add([] { auto e = UploaderSchemaFixture::FloatEntry("b", 0, 10); e.mUi["elevation_layer"] = "b"; return e; }());
	fx.Add([] { auto e = UploaderSchemaFixture::FloatEntry("c", 0, 10); e.mUi["elevation_layer"] = "far"; return e; }());
	fx.Add([] { auto e = UploaderSchemaFixture::FloatEntry("d", 0, 10); e.mUi["elevation_layer"] = 7; return e; }());
	const auto& svc = fx.Publish();
	UseSchema(svc);
	StubProvider far;
	far.values = std::vector<float>(9, 1.0f);
	Reg(temp.MakeDesc("a", &grid.desc, CellValueKind::Float, "cells/a"));
	Reg(temp.MakeDesc("b", &grid.desc, CellValueKind::Float, "cells/b"));
	Reg(temp.MakeDesc("c", &grid.desc, CellValueKind::Float, "cells/c"));
	Reg(temp.MakeDesc("d", &grid.desc, CellValueKind::Float, "cells/d"));
	Reg(far.MakeDesc("far", &otherGrid.desc));
	LayerUploader up(cellField, &svc);
	for (const char* id : { "a", "b", "c", "d" })
	{
		ASSERT_EQ(up.Update(L(id), registry), UploadResult::Ok) << id;
		EXPECT_FALSE(up.DrawRequest().elevationLayer.IsValid()) << id;
		EXPECT_FLOAT_EQ(up.DrawRequest().hillshadeStrength, 0.0f) << id;
	}
	EXPECT_EQ(far.readCalls, 0u);
}

TEST_F(UploaderEdgeTest, ElevationReadFailureReportsFailureButBaseStillDrawn)
{
	fx.Add([] { auto e = UploaderSchemaFixture::FloatEntry("temp", 0, 10); e.mUi["elevation_layer"] = "height"; return e; }());
	const auto& svc = fx.Publish();
	UseSchema(svc);
	CellLayerDesc h = height.MakeDesc("height", &grid.desc);
	h.read = fastdelegate::FastDelegate2<float*, uint32_t>();
	Reg(temp.MakeDesc("temp", &grid.desc, CellValueKind::Float, "cells/temp"));
	Reg(h);
	LayerUploader up(cellField, &svc);
	EXPECT_EQ(up.Update(L("temp"), registry), UploadResult::ReadFailed);
	EXPECT_TRUE(up.DrawRequest().layer.IsValid());
	EXPECT_FALSE(up.DrawRequest().elevationLayer.IsValid());
	uint32_t n = 0;
	EXPECT_NE(up.BaseValues(StringCRC("temp"), n), nullptr);
}

TEST_F(UploaderEdgeTest, ElevationDoesNotChangeObservedRange)
{
	fx.Add([] { auto e = UploaderSchemaFixture::FloatEntry("temp", 0, 10); e.mUi["elevation_layer"] = "height"; return e; }());
	const auto& svc = fx.Publish();
	UseSchema(svc);
	height.values = std::vector<float>(12, 1000.0f);
	Reg(temp.MakeDesc("temp", &grid.desc, CellValueKind::Float, "cells/temp"));
	Reg(height.MakeDesc("height", &grid.desc));
	LayerUploader up(cellField, &svc);
	up.Update(L("temp"), registry);
	EXPECT_EQ(up.ObservedMin(), -2.0f);
	EXPECT_EQ(up.ObservedMax(), 9.0f);
}

TEST_F(UploaderEdgeTest, DestructorReleasesCellFieldLayers)
{
	Reg(temp.MakeDesc("temp", &grid.desc));
	{
		LayerUploader up(cellField, nullptr);
		up.Update(L("temp"), registry);
		cellField.Drain(sink);
		ASSERT_EQ(cellField.GetStats().layersLive, 1u);
	}
	cellField.Drain(sink);
	EXPECT_EQ(cellField.GetStats().layersLive, 0u);
}

TEST_F(UploaderEdgeTest, NaNAndInfValuesAreExcludedFromObservedButUploadedVerbatim)
{
	temp.values = { kNaN, 4, kInf, -kInf, 2, 9, kNaN, 1, 1, 1, 1, 1 };
	Reg(temp.MakeDesc("temp", &grid.desc));
	LayerUploader up(cellField, nullptr);
	ASSERT_EQ(up.Update(L("temp"), registry), UploadResult::Ok);
	EXPECT_EQ(up.ObservedMin(), 1.0f);
	EXPECT_EQ(up.ObservedMax(), 9.0f);
	cellField.Drain(sink);
	ASSERT_EQ(sink.updates.size(), 1u);
	EXPECT_TRUE(std::isnan(sink.updates[0].values[0]));
	EXPECT_TRUE(std::isinf(sink.updates[0].values[2]));
}

TEST_F(UploaderEdgeTest, AllNonFiniteValuesGiveZeroObservedRange)
{
	temp.values = std::vector<float>(12, kNaN);
	Reg(temp.MakeDesc("temp", &grid.desc));
	LayerUploader up(cellField, nullptr);
	ASSERT_EQ(up.Update(L("temp"), registry), UploadResult::Ok);
	EXPECT_EQ(up.ObservedMin(), 0.0f);
	EXPECT_EQ(up.ObservedMax(), 0.0f);
	EXPECT_EQ(up.DrawRequest().rampMin, 0.0f);
	EXPECT_EQ(up.DrawRequest().rampMax, 0.0f);
}

TEST_F(UploaderEdgeTest, NegativeOnlyValuesObservedRange)
{
	temp.values = std::vector<float>(12, -3.5f);
	temp.values[4] = -9.0f;
	Reg(temp.MakeDesc("temp", &grid.desc));
	LayerUploader up(cellField, nullptr);
	up.Update(L("temp"), registry);
	EXPECT_EQ(up.ObservedMin(), -9.0f);
	EXPECT_EQ(up.ObservedMax(), -3.5f);
}

TEST_F(UploaderEdgeTest, SchemaRangeWinsOverObservedAndObservedUsedWithoutIt)
{
	fx.Add(UploaderSchemaFixture::FloatEntry("temp", -50, 50));
	fx.Add(Dia::ValueSchema::Testing::MakeValueInfo("cells", "free"));
	const auto& svc = fx.Publish();
	UseSchema(svc);
	Reg(temp.MakeDesc("temp", &grid.desc, CellValueKind::Float, "cells/temp"));
	Reg(rain.MakeDesc("free", &grid.desc, CellValueKind::Float, "cells/free"));
	rain.values[3] = 6.0f;
	LayerUploader up(cellField, &svc);
	up.Update(L("temp"), registry);
	EXPECT_EQ(up.DrawRequest().rampMin, -50.0f);
	EXPECT_EQ(up.DrawRequest().rampMax, 50.0f);
	up.Update(L("free"), registry);
	EXPECT_EQ(up.DrawRequest().rampMin, 1.0f);
	EXPECT_EQ(up.DrawRequest().rampMax, 6.0f);
}

TEST_F(UploaderEdgeTest, InsetIsClampedAndBadBackdropIgnored)
{
	auto big = UploaderSchemaFixture::FloatEntry("big", 0, 1); big.mUi["inset"] = 5.0; big.mUi["backdrop"] = "not-a-colour";
	auto neg = UploaderSchemaFixture::FloatEntry("neg", 0, 1); neg.mUi["inset"] = -2.0;
	fx.Add(big);
	fx.Add(neg);
	const auto& svc = fx.Publish();
	UseSchema(svc);
	Reg(temp.MakeDesc("big", &grid.desc, CellValueKind::Float, "cells/big"));
	Reg(temp.MakeDesc("neg", &grid.desc, CellValueKind::Float, "cells/neg"));
	LayerUploader up(cellField, &svc);
	up.Update(L("big"), registry);
	EXPECT_FLOAT_EQ(up.DrawRequest().inset, 0.499f);
	EXPECT_EQ(up.DrawRequest().backdropColour, 0u);
	up.Update(L("neg"), registry);
	EXPECT_FLOAT_EQ(up.DrawRequest().inset, 0.0f);
}

TEST_F(UploaderEdgeTest, NoBaseResetsDrawRequestAndBaseValues)
{
	Reg(temp.MakeDesc("temp", &grid.desc));
	LayerUploader up(cellField, nullptr);
	up.Update(L("temp"), registry);
	ASSERT_TRUE(up.DrawRequest().layer.IsValid());
	EXPECT_EQ(up.Update(nullptr, registry), UploadResult::NoBase);
	EXPECT_FALSE(up.DrawRequest().layer.IsValid());
	uint32_t n = 7;
	EXPECT_EQ(up.BaseValues(StringCRC("temp"), n), nullptr);
	EXPECT_EQ(n, 0u);
	EXPECT_EQ(up.Update(L("temp"), registry), UploadResult::Ok);
	EXPECT_EQ(up.Reads(), 2u);
}

TEST_F(UploaderEdgeTest, ManySwitchesRegisterEachColourMapOnce)
{
	auto heat = UploaderSchemaFixture::FloatEntry("t1", 0, 1, "test.heat");
	auto grey = UploaderSchemaFixture::FloatEntry("t2", 0, 1, "sequential.grey");
	auto heat2 = UploaderSchemaFixture::FloatEntry("t3", 0, 1, "test.heat");
	fx.Add(heat); fx.Add(grey); fx.Add(heat2);
	fx.Add(UploaderSchemaFixture::EnumEntry("b1", 3));
	fx.Add(UploaderSchemaFixture::EnumEntry("b2", 2));
	const auto& svc = fx.Publish();
	UseSchema(svc);
	StubProvider cat;
	cat.values = std::vector<float>(12, 1.0f);
	for (const char* id : { "t1", "t2", "t3" }) { Reg(temp.MakeDesc(id, &grid.desc, CellValueKind::Float, (std::string("cells/") + id).c_str())); }
	for (const char* id : { "b1", "b2" }) { Reg(cat.MakeDesc(id, &grid.desc, CellValueKind::Category, (std::string("cells/") + id).c_str())); }
	LayerUploader up(cellField, &svc);
	const char* ids[] = { "t1", "t2", "t3", "b1", "b2" };
	for (int i = 0; i < 100; ++i) { ASSERT_EQ(up.Update(L(ids[i % 5]), registry), UploadResult::Ok); cellField.Drain(sink); }
	EXPECT_EQ(up.RegisteredColourMapCount(), 4u);	// two ramps + two palettes
	Dia::Graphics::CellFieldRampData ramp;
	ASSERT_TRUE(cellField.GetRamp(StringCRC("test.heat").Value(), ramp));
	EXPECT_EQ(ramp.revision, 1u);
}

TEST_F(UploaderEdgeTest, ColourMapsBeyondTrackingCapacityDoNotCrash)
{
	LayerUploader up(cellField, nullptr);
	Reg(temp.MakeDesc("temp", &grid.desc));
	for (int i = 0; i < 300; ++i) { temp.versionValue = i + 1; ASSERT_EQ(up.Update(L("temp"), registry), UploadResult::Ok); }
	EXPECT_EQ(up.RegisteredColourMapCount(), 1u);
}

TEST_F(UploaderEdgeTest, Large104x64ThreeLayersSwitchCountersAndSteadyState)
{
	TestGrid large(104, 64);
	StubProvider a, b, c;
	for (StubProvider* p : { &a, &b, &c }) { p->values = std::vector<float>(large.CellCount(), 2.0f); }
	Reg(a.MakeDesc("a", &large.desc));
	Reg(b.MakeDesc("b", &large.desc));
	Reg(c.MakeDesc("c", &large.desc));
	LayerUploader up(cellField, nullptr);
	up.Update(L("a"), registry);
	for (int i = 0; i < 200; ++i) { up.Update(L("a"), registry); }
	EXPECT_EQ(up.Reads(), 1u);
	EXPECT_EQ(up.Uploads(), 1u);
	EXPECT_EQ(up.SkippedFrames(), 200u);
	const char* seq[] = { "b", "c", "a", "c", "b" };
	for (const char* s : seq)
	{
		const uint64_t r0 = up.Reads(), u0 = up.Uploads();
		up.Update(L(s), registry);
		up.Update(L(s), registry);
		EXPECT_EQ(up.Reads() - r0, 1u) << s;
		EXPECT_EQ(up.Uploads() - u0, 1u) << s;
		cellField.Drain(sink);
	}
	EXPECT_EQ(sink.updates.back().values.size(), 104u * 64u);
}

TEST_F(UploaderEdgeTest, RandomBaseSwitchingKeepsCountersConsistent)
{
	StubProvider p[4];
	const char* ids[] = { "p0", "p1", "p2", "p3" };
	for (int i = 0; i < 4; ++i) { p[i].values = std::vector<float>(12, static_cast<float>(i)); Reg(p[i].MakeDesc(ids[i], &grid.desc)); }
	LayerUploader up(cellField, nullptr);
	std::mt19937 rng(99);
	int current = -1;
	uint64_t expectedReads = 0;
	for (int iter = 0; iter < 600; ++iter)
	{
		const uint32_t action = rng() % 10;
		if (action < 5 || current < 0)
		{
			const int next = static_cast<int>(rng() % 4);
			if (next != current) { ++expectedReads; }
			current = next;
		}
		else if (action < 8)
		{
			p[current].versionValue++;
			++expectedReads;
		}
		ASSERT_EQ(up.Update(L(ids[current]), registry), UploadResult::Ok);
		ASSERT_EQ(up.Reads(), expectedReads) << iter;
		ASSERT_EQ(up.Uploads(), up.Reads()) << iter;
		uint32_t n = 0;
		const float* v = up.BaseValues(StringCRC(ids[current]), n);
		ASSERT_NE(v, nullptr);
		ASSERT_EQ(v[0], static_cast<float>(current));
		cellField.Drain(sink);
		ASSERT_EQ(cellField.GetStats().layersLive, 1u);
	}
}

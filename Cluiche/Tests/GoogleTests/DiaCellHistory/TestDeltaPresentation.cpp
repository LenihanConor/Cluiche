#include "HistoryTestRig.h"
#include "../DiaValueSchema/RampTestSupport.h"
#include <DiaCellHistory/DeltaScale.h>
#include <DiaCellHistory/DeltaValueSource.h>
#include <DiaCellHistory/HistoryValueSource.h>
#include <DiaCellHistory/HistoryViewCommands.h>
#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/LayerUploader.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/FakeMirrorRegistrar.h>
#include <DiaAPI/DiaAPI.h>

#include <cstring>
#include <memory>

#ifdef DIA_DEBUG

using namespace Dia::CellHistory;
using namespace Dia::CellHistory::Testing;
using namespace HistoryRig;
using Dia::CellLayers::ViewPresentation;
using Dia::Core::StringCRC;

// Delta presentation (Feature 4 task 4.5): HistoryValueSource::GetPresentation in delta mode supplies the layer's delta ramp,
// a symmetric [-s, +s] range and the "\xCE\x94" (UTF-8 for the Greek capital delta) unit prefix.

namespace
{
	const char* kDelta = "\xCE\x94";

	// A float entry with an optional ui.delta_ramp.
	std::string UiFloatEntry(const char* key, const char* deltaRamp)
	{
		std::string s = std::string("{\"key\":\"") + key + "\",\"kind\":\"parameter\",\"label\":\"L\",\"description\":\"D\",\"type\":\"float\",\"min\":0,\"max\":100";
		if (deltaRamp) { s += std::string(",\"ui\":{\"delta_ramp\":\"") + deltaRamp + "\"}"; }
		return s + "}";
	}

	// Rig plus a service that knows the shipped ramps, over the same published entries.
	struct PresRig : Rig
	{
		VsTest::RampFixture ramps{ true, false };
		std::unique_ptr<Dia::ValueSchema::ValueSchemaService> rampService;

		void Build(bool withUi)
		{
			grid.SetTopology("hex.flat.oddq");
			grid.AddFloatLayer("temp", 40.0f);
			grid.AddFloatLayer("wet", 10.0f);
			grid.AddCategoryLayer("biome", 1);
			ASSERT_TRUE(schema.Load({ UiFloatEntry("temp", nullptr), UiFloatEntry("wet", withUi ? "sequential.heat" : nullptr), EnumEntry("biome", 3) }));
			cfg.mTrackCount = 0;
			cfg.mTrack[cfg.mTrackCount++] = Dia::Core::Containers::String64("%s", "temp");
			cfg.mTrack[cfg.mTrackCount++] = Dia::Core::Containers::String64("%s", "wet");
			Start();
			grid.Set("temp", 3, 10.0f); grid.Set("wet", 3, 10.0f); grid.BumpVersion("temp"); grid.BumpVersion("wet"); Take();
			grid.Set("temp", 3, 70.3f); grid.Set("wet", 3, 70.3f); grid.BumpVersion("temp"); grid.BumpVersion("wet"); Take();
			for (uint32_t i = 0; i < schema.registry.Count(); ++i) { ramps.mSchema.Register(*schema.registry.At(i)); }
			ramps.Finish();
			rampService = std::make_unique<Dia::ValueSchema::ValueSchemaService>(schema.registry, &ramps.mRamps);
		}
		const Dia::CellLayers::CellLayerDesc& Desc(const char* id)
		{
			const Dia::CellLayers::CellLayerDesc* d = registry.Find(StringCRC(id));
			EXPECT_NE(d, nullptr);
			return *d;
		}
	};

	Dia::CellLayers::CellPickerView ViewFn0() { Dia::CellLayers::CellPickerView v; v.viewportWidth = 1280; v.viewportHeight = 720; return v; }

	Side S(uint32_t i) { Side s; s.kind = SideKind::Sample; s.sample = i; return s; }
}

TEST(DeltaPresentation, NotInDeltaIsFalseLiveAndScrub)
{
	PresRig r; r.Build(false);
	HistoryViewState v(*r.rec);
	HistoryValueSource s(r.registry, *r.rec, v);
	s.SetSchema(r.rampService.get());
	ViewPresentation p;
	EXPECT_FALSE(s.GetPresentation(r.Desc("temp"), p));
	ViewResult res;
	ASSERT_TRUE(v.ViewSample(1, res));
	EXPECT_FALSE(s.GetPresentation(r.Desc("temp"), p));
}

TEST(DeltaPresentation, AutoScaleUsesDefaultDeltaRampNiceRangeAndDeltaPrefix)
{
	PresRig r; r.Build(false);
	HistoryViewState v(*r.rec);
	HistoryValueSource s(r.registry, *r.rec, v);
	s.SetSchema(r.rampService.get());
	ViewResult res;
	ASSERT_TRUE(v.EnterDelta(StringCRC("temp"), S(0), S(1), res));
	float out[12];
	ASSERT_TRUE(s.GetValues(r.Desc("temp"), out, 12));	// observes ~60.3 -> scale 100
	ViewPresentation p;
	ASSERT_TRUE(s.GetPresentation(r.Desc("temp"), p));
	EXPECT_EQ(p.rampId, StringCRC("diverging.blue_red"));
	EXPECT_EQ(p.rangeMin, -DeltaScale::NiceCeil(61.0f));
	EXPECT_EQ(p.rangeMax, DeltaScale::NiceCeil(61.0f));
	EXPECT_EQ(p.rangeMax, 100.0f);
	EXPECT_EQ(std::memcmp(p.unitPrefix, "\xCE\x94\0\0\0\0\0\0", 8), 0);
	EXPECT_STREQ(p.unitPrefix, kDelta);
	EXPECT_EQ(std::strlen(p.unitPrefix), 2u);
	EXPECT_TRUE(s.IsAutoScale());
}

TEST(DeltaPresentation, FixedScaleGivesPlusMinusN)
{
	PresRig r; r.Build(false);
	HistoryViewState v(*r.rec);
	HistoryValueSource s(r.registry, *r.rec, v);
	s.SetSchema(r.rampService.get());
	ViewResult res;
	ASSERT_TRUE(v.EnterDelta(StringCRC("temp"), S(0), S(1), res, false, 7.5f));
	ViewPresentation p;
	ASSERT_TRUE(s.GetPresentation(r.Desc("temp"), p));
	EXPECT_EQ(p.rangeMin, -7.5f);
	EXPECT_EQ(p.rangeMax, 7.5f);
	EXPECT_FALSE(s.IsAutoScale());
	ASSERT_TRUE(v.SetDeltaScaleAuto(res));
	EXPECT_TRUE(s.IsAutoScale());
}

TEST(DeltaPresentation, LayerDeltaRampIsUsed)
{
	PresRig r; r.Build(true);
	HistoryViewState v(*r.rec);
	HistoryValueSource s(r.registry, *r.rec, v);
	s.SetSchema(r.rampService.get());
	ViewResult res;
	ASSERT_TRUE(v.EnterDelta(StringCRC("wet"), S(0), S(1), res, false, 5.0f));
	ViewPresentation p;
	ASSERT_TRUE(s.GetPresentation(r.Desc("wet"), p));
	EXPECT_EQ(p.rampId, StringCRC("sequential.heat"));
}

TEST(DeltaPresentation, CategoryLayerIsFalse)
{
	PresRig r; r.Build(false);
	HistoryViewState v(*r.rec);
	HistoryValueSource s(r.registry, *r.rec, v);
	s.SetSchema(r.rampService.get());
	ViewResult res;
	ASSERT_TRUE(v.EnterDelta(StringCRC("temp"), S(0), S(1), res));
	ViewPresentation p;
	EXPECT_FALSE(s.GetPresentation(r.Desc("biome"), p));
}

TEST(DeltaPresentation, NoSchemaOrUnknownSchemaKeyIsFalse)
{
	PresRig r; r.Build(false);
	HistoryViewState v(*r.rec);
	HistoryValueSource s(r.registry, *r.rec, v);
	ViewResult res;
	ASSERT_TRUE(v.EnterDelta(StringCRC("temp"), S(0), S(1), res));
	ViewPresentation p;
	EXPECT_FALSE(s.GetPresentation(r.Desc("temp"), p)) << "no schema set: schema colouring is kept";
	s.SetSchema(r.rampService.get());
	EXPECT_TRUE(s.GetPresentation(r.Desc("temp"), p));
	Dia::CellLayers::CellLayerDesc nokey = r.Desc("temp");
	nokey.schemaKey = StringCRC("cells/unknown");
	EXPECT_FALSE(s.GetPresentation(nokey, p)) << "layer without ValueInfo";
	s.SetSchema(nullptr);
	EXPECT_FALSE(s.GetPresentation(r.Desc("temp"), p));
}

TEST(DeltaPresentation, ViewerShowsDeltaRampRangeAndPrefixThenRestoresSchemaColouring)
{
	Dia::API::Initialize();
	{
		PresRig r; r.Build(false);
		Dia::Graphics::CellFieldRegistry cellField;
		Dia::CellLayers::Testing::FakeDebugDraw draw;
		Dia::CellLayers::LayerUploader uploader(cellField, r.rampService.get());
		Dia::CellLayers::CellLayerViewerDomain domain(r.registry, uploader, r.rampService.get(), Dia::CellLayers::CellLayerViewerDomain::ViewFn(&ViewFn0));
		HistoryViewState v(*r.rec);
		HistoryValueSource s(r.registry, *r.rec, v);
		s.SetSchema(r.rampService.get());
		ASSERT_TRUE(s.Install());
		ASSERT_TRUE(domain.SetBase(StringCRC("temp")));
		auto frame = [&] { s.Tick(); domain.Update(draw); };
		frame();

		Json::Value live;
		domain.GetJSONState(live);
		const double liveMax = live["legend"]["max"].asDouble();
		EXPECT_FALSE(live["legend"].isMember("unitPrefix") && live["legend"]["unitPrefix"].asString().size() > 0);

		ViewResult res;
		ASSERT_TRUE(v.EnterDelta(StringCRC("temp"), S(0), S(1), res));
		frame();
		Json::Value st;
		domain.GetJSONState(st);
		EXPECT_EQ(st["legend"]["min"].asDouble(), -100.0);
		EXPECT_EQ(st["legend"]["max"].asDouble(), 100.0);
		EXPECT_EQ(st["legend"]["unitPrefix"].asString(), std::string(kDelta));
		EXPECT_EQ(st["legend"]["ramp"].asString(), "diverging.blue_red");

		ASSERT_TRUE(v.ExitDelta());
		frame();
		Json::Value back;
		domain.GetJSONState(back);
		EXPECT_EQ(back["legend"]["max"].asDouble(), liveMax);
		EXPECT_EQ(back["legend"]["ramp"].asString(), "sequential.grey");
		EXPECT_TRUE(!back["legend"].isMember("unitPrefix") || back["legend"]["unitPrefix"].asString().empty());
		s.Uninstall();
	}
	Dia::API::Shutdown();
}

#endif // DIA_DEBUG

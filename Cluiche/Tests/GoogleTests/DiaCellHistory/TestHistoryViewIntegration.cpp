#include "HistoryTestRig.h"
#include <DiaCellHistory/HistoryValueSource.h>
#include <DiaCellHistory/HistoryViewCommands.h>
#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCellHistory/HistoryViewStatus.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/LayerReader.h>
#include <DiaCellLayers/LayerUploader.h>
#include <DiaCellLayers/MirrorQueries.h>
#include <DiaCellLayers/SelectionFingerprint.h>
#include <DiaCellLayers/Testing/CellPickerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/FakeMirrorRegistrar.h>
#include <DiaCellLayers/Testing/ViewerCommands.h>
#include <DiaAPI/DiaAPI.h>
#include <DiaPathfinding/HexTopology.h>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#ifdef DIA_DEBUG

// Integration of the history view with the REAL DiaCellLayers consumers (uploader, reader, viewer hover, query, inspector
// stats, cells.layer.values). Rig: 4x3 grid (12 cells), layers temp (float, tracked), biome (category, tracked), rain
// (float, NOT tracked). Sample i (0..4): temp[c] = 10*i + c, biome[c] = (i + c) % 3, rain = 5. Live values after recording:
// temp[c] = 70 + c, biome = 2, rain = 9.

using namespace Dia::CellHistory;
using namespace Dia::CellHistory::Testing;
using namespace HistoryRig;
using namespace Dia::CellLayers;
using Dia::CellLayers::Testing::FakeDebugDraw;
using Dia::CellLayers::Testing::FakeMirrorRegistrar;
using Dia::CellLayers::Testing::ViewerCommands;
using Dia::Core::StringCRC;

namespace
{
	CellPickerView TestView() { CellPickerView v; v.viewportWidth = 1280; v.viewportHeight = 720; return v; }
	CellPickerView ViewFn0() { return TestView(); }

	bool DecodeFloats(const Json::Value& r, std::vector<float>& out)
	{
		out.clear();
		if (!r.isObject() || !r["data"].isString()) { return false; }
		const std::string text = r["data"].asString();
		auto val = [](char c) -> int
		{
			if (c >= 'A' && c <= 'Z') { return c - 'A'; }
			if (c >= 'a' && c <= 'z') { return c - 'a' + 26; }
			if (c >= '0' && c <= '9') { return c - '0' + 52; }
			if (c == '+') { return 62; }
			if (c == '/') { return 63; }
			return 0;
		};
		std::vector<uint8_t> bytes;
		for (size_t i = 0; i + 3 < text.size(); i += 4)
		{
			uint32_t v = 0;
			int pad = 0;
			for (int k = 0; k < 4; ++k)
			{
				const char c = text[i + k];
				if (c == '=') { ++pad; v <<= 6; }
				else { v = (v << 6) | static_cast<uint32_t>(val(c)); }
			}
			bytes.push_back(static_cast<uint8_t>(v >> 16));
			if (pad < 2) { bytes.push_back(static_cast<uint8_t>(v >> 8)); }
			if (pad < 1) { bytes.push_back(static_cast<uint8_t>(v)); }
		}
		out.resize(bytes.size() / 4);
		for (size_t i = 0; i < out.size(); ++i) { std::memcpy(&out[i], &bytes[i * 4], 4); }
		return true;
	}

	Json::Value LayerArgs(const char* layer) { Json::Value a(Json::objectValue); a["layer"] = layer; return a; }
	Json::Value ExprArgs(const char* e) { Json::Value a(Json::objectValue); a["expr"] = e; return a; }
	Json::Value StatsArgs(const char* layer)
	{
		Json::Value a(Json::objectValue);
		Json::Value l(Json::arrayValue);
		l.append(layer);
		a["layers"] = l;
		return a;
	}

	struct HistoryViewIntegration : ::testing::Test
	{
		void SetUp() override { Dia::API::Initialize(); }
		void TearDown() override { Dia::API::Shutdown(); }
	};

	struct ViewRig : Rig
	{
		static constexpr uint32_t kSamples = 5;
		Dia::Graphics::CellFieldRegistry cellField;
		FakeDebugDraw draw;
		std::unique_ptr<LayerUploader> uploader;
		std::unique_ptr<CellLayerViewerDomain> domain;
		std::unique_ptr<ViewerCommands> viewerCommands;
		std::unique_ptr<MirrorQueries> queries;
		std::unique_ptr<LayerReader> reader;
		std::unique_ptr<HistoryViewState> view;
		std::unique_ptr<HistoryValueSource> source;
		std::unique_ptr<HistoryViewCommands> commands;
		FakeMirrorRegistrar registrar;

		static float TempAt(uint32_t sample, uint32_t cell) { return 10.0f * sample + cell; }
		static float BiomeAt(uint32_t sample, uint32_t cell) { return float((sample + cell) % 3); }

		// smallWindow: track only biome (the base) with a per-cell window of 3 samples; sampleCount samples recorded.
		void Setup(bool smallWindow, uint32_t sampleCount = kSamples)
		{
			grid.SetTopology("hex.flat.oddq");
			grid.AddFloatLayer("temp", 0.0f);
			grid.AddCategoryLayer("biome", 0);
			grid.AddFloatLayer("rain", 5.0f);
			ASSERT_TRUE(schema.Load({ FloatEntry("temp", 0, 100), EnumEntry("biome", 3), FloatEntry("rain", 0, 100) }));
			cfg.mTrackCount = 0;
			cfg.mTrack[cfg.mTrackCount++] = Dia::Core::Containers::String64("%s", "biome");
			RecorderBudgets b;
			if (smallWindow)
			{
				b.mAggregateBytes = 1 << 20;
				b.mCellBytes = BudgetBytesForWindow(3, BytesPerSample(0, 1, 12));
				b.mEventBytes = 4096;
			}
			else
			{
				cfg.mTrack[cfg.mTrackCount++] = Dia::Core::Containers::String64("%s", "temp");
			}
			Start(smallWindow ? &b : nullptr, false);
			for (uint32_t i = 0; i < sampleCount; ++i) { Write(i); Take(); }
			// Live now differs from every recorded sample.
			for (uint32_t c = 0; c < 12; ++c) { grid.Set("temp", c, 70.0f + c); grid.Set("biome", c, 2.0f); }
			grid.SetAll("rain", 9.0f);
			grid.BumpVersion("temp"); grid.BumpVersion("biome"); grid.BumpVersion("rain");

			uploader = std::make_unique<LayerUploader>(cellField, schema.service.get());
			domain = std::make_unique<CellLayerViewerDomain>(registry, *uploader, schema.service.get(), CellLayerViewerDomain::ViewFn(&ViewFn0));
			viewerCommands = std::make_unique<ViewerCommands>(*domain);
			queries = std::make_unique<MirrorQueries>(registry, *domain, schema.service.get());
			reader = std::make_unique<LayerReader>(registry);
			view = std::make_unique<HistoryViewState>(*rec);
			source = std::make_unique<HistoryValueSource>(registry, *rec, *view);
			ASSERT_TRUE(source->Install());
			commands = std::make_unique<HistoryViewCommands>(*rec, *view, &cfg);
			ASSERT_TRUE(commands->Register(registrar));
			ASSERT_TRUE(domain->SetBase(StringCRC(smallWindow ? "biome" : "temp")));
			Frame();
		}

		void Write(uint32_t sample)
		{
			for (uint32_t c = 0; c < 12; ++c) { grid.Set("temp", c, TempAt(sample, c)); grid.Set("biome", c, BiomeAt(sample, c)); }
			grid.BumpVersion("temp"); grid.BumpVersion("biome");
		}

		void Frame() { source->Tick(); domain->Update(draw); }
		void ViewSample(uint32_t n)
		{
			Json::Value a(Json::objectValue); a["sample"] = n;
			const Json::Value r = registrar.Execute("cells.history.view", a);
			ASSERT_TRUE(r["ok"].asBool()) << r.toStyledString();
			Frame();
		}
		void GoLive() { registrar.Execute("cells.history.live", Json::Value(Json::objectValue)); Frame(); }

		std::vector<float> Recorded(uint32_t sample, const char* layer)
		{
			std::vector<float> v(12);
			EXPECT_TRUE(rec->CellValues(sample, StringCRC(layer), v.data(), 12));
			return v;
		}
		std::vector<float> UploaderBase(const char* layer)
		{
			uint32_t n = 0;
			const float* p = uploader->BaseValues(StringCRC(layer), n);
			return p ? std::vector<float>(p, p + n) : std::vector<float>();
		}
		std::vector<float> ReaderValues(const char* layer)
		{
			const LayerValues lv = reader->Read(StringCRC(layer));
			return lv.hasData && lv.values ? std::vector<float>(lv.values, lv.values + lv.count) : std::vector<float>();
		}
		std::vector<float> MirrorValues(const char* layer)
		{
			std::vector<float> out;
			DecodeFloats(queries->LayerValues(LayerArgs(layer)), out);
			return out;
		}
		uint32_t QueryCount(const char* expr)
		{
			const Json::Value r = ViewerCommands::Exec("cells.query.check", ExprArgs(expr));
			return r["success"].asBool() ? r["data"]["count"].asUInt() : 9999u;
		}
		void SelectCells(std::initializer_list<uint32_t> cells)
		{
			CellSelection* sel = domain->ActiveSelection();
			ASSERT_NE(sel, nullptr);
			sel->BeginEdit();
			for (uint32_t c : cells) { sel->Set(c, true); }
			sel->EndEdit();
		}
		double SelectedMean(const char* layer)
		{
			const Json::Value r = queries->SelectionStats(StatsArgs(layer));
			if (!r.isObject() || !r["layers"].isArray() || r["layers"].size() != 1u) { ADD_FAILURE() << r.toStyledString(); return -1.0; }
			return r["layers"][0u]["mean"].asDouble();
		}
		SelectionFingerprint Fingerprint() { return SelectionFingerprintFn::Compute(*domain->ActiveSelectionView()); }
	};
}

TEST_F(HistoryViewIntegration, UploaderReaderQueryStatsAndMirrorShowTheViewedSample)
{
	ViewRig r; r.Setup(false);
	r.SelectCells({ 0, 1, 2 });
	for (uint32_t n : { 0u, 2u, 4u })
	{
		r.ViewSample(n);
		const std::vector<float> temp = r.Recorded(n, "temp");
		EXPECT_EQ(r.UploaderBase("temp"), temp) << n;
		EXPECT_EQ(r.ReaderValues("temp"), temp) << n;
		EXPECT_EQ(r.ReaderValues("biome"), r.Recorded(n, "biome")) << n;
		EXPECT_EQ(r.MirrorValues("temp"), temp) << n;
		EXPECT_NEAR(temp[5], ViewRig::TempAt(n, 5), 0.5f) << "recorded sample tracks the fixture";
		EXPECT_EQ(r.QueryCount("temp > 44.5"), n == 4 ? 7u : 0u) << n;	// sample 4 holds 40..51
		EXPECT_NEAR(r.SelectedMean("temp"), 10.0 * n + 1.0, 0.5) << n;
	}
}

TEST_F(HistoryViewIntegration, GoLiveRestoresLiveValuesEverywhere)
{
	ViewRig r; r.Setup(false);
	r.SelectCells({ 0, 1, 2 });
	r.ViewSample(1);
	r.GoLive();
	std::vector<float> live(12);
	for (uint32_t c = 0; c < 12; ++c) { live[c] = 70.0f + c; }
	EXPECT_EQ(r.UploaderBase("temp"), live);
	EXPECT_EQ(r.ReaderValues("temp"), live);
	EXPECT_EQ(r.MirrorValues("temp"), live);
	EXPECT_EQ(r.QueryCount("temp > 44.5"), 12u);
	EXPECT_NEAR(r.SelectedMean("temp"), 71.0, 1e-4);
}

TEST_F(HistoryViewIntegration, ScrubbingAfterGoLiveSwitchesBackToTheRecordedValues)
{
	ViewRig r; r.Setup(false);
	r.SelectCells({ 0, 1, 2 });
	r.ViewSample(3);
	r.GoLive();
	r.ViewSample(3);
	EXPECT_EQ(r.UploaderBase("temp"), r.Recorded(3, "temp"));
	EXPECT_NEAR(r.SelectedMean("temp"), 31.0, 0.5);
}

TEST_F(HistoryViewIntegration, HoverShowsTheViewedValueAndStatusCarriesTheSampleBadge)
{
	ViewRig r; r.Setup(false);
	// Hover cell (2,1) = index 6.
	const Dia::Maths::Vector2D c = Dia::Pathfinding::HexTopology::CellCenter(Dia::Pathfinding::CellCoord{ 2, 1 }, 1.0f);
	float sx, sy;
	Dia::CellLayers::Testing::WorldToScreen(TestView(), c.X(), c.Y(), sx, sy);
	r.draw.mouse = Dia::Maths::Vector2D(sx, sy);

	r.ViewSample(3);
	const CellLayerViewerDomain::HoverInfo h = r.domain->Hover();
	ASSERT_TRUE(h.valid);
	EXPECT_EQ(h.q, 2); EXPECT_EQ(h.r, 1);
	EXPECT_EQ(h.value, r.Recorded(3, "temp")[6]);
	EXPECT_NEAR(h.value, 36.0f, 0.5f);

	// HoverInfo has no sample field; the badge data comes from HistoryViewStatus beside it.
	const HistoryViewStatus st = DescribeHistoryView(*r.rec, *r.view);
	EXPECT_EQ(st.mMode, ViewMode::Sample);
	EXPECT_TRUE(st.mHasSample);
	EXPECT_EQ(st.mSample, 3u);
	EXPECT_STREQ(st.mLabel, r.rec->Header(3).mLabel.AsCStr());
	EXPECT_FALSE(st.mEvicted);
	Json::Value state;
	r.domain->GetJSONState(state);
	EXPECT_TRUE(state["hover"]["valid"].asBool());
	EXPECT_NEAR(state["hover"]["value"].asDouble(), 36.0, 0.5);

	r.GoLive();
	EXPECT_EQ(r.domain->Hover().value, 76.0f);
	EXPECT_EQ(DescribeHistoryView(*r.rec, *r.view).mMode, ViewMode::Live);
}

TEST_F(HistoryViewIntegration, RegistryIsLiveFollowsTheView)
{
	ViewRig r; r.Setup(false);
	EXPECT_TRUE(r.registry.IsLive());
	r.ViewSample(4);	// the latest sample is scrub mode, not live
	EXPECT_FALSE(r.registry.IsLive());
	r.GoLive();
	EXPECT_TRUE(r.registry.IsLive());
	r.ViewSample(0);
	r.source->Uninstall();
	EXPECT_TRUE(r.registry.IsLive());	// no source installed
}

TEST_F(HistoryViewIntegration, UntrackedLayerShowsNoDataNeverLiveValues)
{
	ViewRig r; r.Setup(false);
	r.ViewSample(2);
	EXPECT_EQ(DescribeLayerView(*r.rec, *r.view, StringCRC("rain")), LayerViewState::Untracked);
	EXPECT_STREQ(LayerViewStateName(DescribeLayerView(*r.rec, *r.view, StringCRC("rain"))), "untracked");
	EXPECT_EQ(DescribeLayerView(*r.rec, *r.view, StringCRC("temp")), LayerViewState::HasData);

	EXPECT_TRUE(r.ReaderValues("rain").empty());
	const Json::Value mirror = r.queries->LayerValues(LayerArgs("rain"));
	ASSERT_TRUE(mirror.isMember("error"));
	EXPECT_NE(mirror["error"].asString().find("no data"), std::string::npos);
	const Json::Value q = ViewerCommands::Exec("cells.query.check", ExprArgs("rain > 0"));
	EXPECT_FALSE(q["success"].asBool()) << "a query on an untracked layer must not use live values";

	// Viewer: make the untracked layer the base. The map is cleared and flagged noData; hover is invalid.
	const uint32_t readsBefore = r.grid.ReadCount("rain");
	ASSERT_TRUE(r.domain->SetBase(StringCRC("rain")));
	r.Frame();
	Json::Value state;
	r.domain->GetJSONState(state);
	EXPECT_TRUE(state["noData"].asBool());
	EXPECT_TRUE(r.UploaderBase("rain").empty());
	EXPECT_FALSE(r.domain->Hover().valid);
	EXPECT_EQ(r.grid.ReadCount("rain"), readsBefore) << "the live read delegate must not run while scrubbed";

	// Back to live: the layer has its live values again and the flag clears.
	r.GoLive();
	EXPECT_EQ(DescribeLayerView(*r.rec, *r.view, StringCRC("rain")), LayerViewState::Live);
	Json::Value liveState;
	r.domain->GetJSONState(liveState);
	EXPECT_FALSE(liveState.isMember("noData"));
	EXPECT_EQ(r.UploaderBase("rain"), std::vector<float>(12, 9.0f));
}

TEST_F(HistoryViewIntegration, EvictedSampleShowsNoDataAndIsDistinctFromUntracked)
{
	ViewRig r; r.Setup(true, 8);	// window of 3: per-cell samples 5..7 retained
	ASSERT_EQ(r.rec->FirstRetained(), 5u);
	r.ViewSample(1);
	EXPECT_TRUE(r.view->IsEvicted());
	EXPECT_EQ(DescribeLayerView(*r.rec, *r.view, StringCRC("biome")), LayerViewState::Evicted);
	EXPECT_STREQ(LayerViewStateName(DescribeLayerView(*r.rec, *r.view, StringCRC("biome"))), "evicted");
	EXPECT_NE(DescribeLayerView(*r.rec, *r.view, StringCRC("biome")), DescribeLayerView(*r.rec, *r.view, StringCRC("rain")));
	EXPECT_TRUE(DescribeHistoryView(*r.rec, *r.view).mEvicted);
	Json::Value state;
	r.domain->GetJSONState(state);
	EXPECT_TRUE(state["noData"].asBool());
	EXPECT_TRUE(r.UploaderBase("biome").empty());
	EXPECT_TRUE(r.ReaderValues("biome").empty());
	ASSERT_TRUE(r.queries->LayerValues(LayerArgs("biome")).isMember("error"));
}

TEST_F(HistoryViewIntegration, WindowPassingTheViewedSampleKeepsTheViewAndSwitchesToEvicted)
{
	ViewRig r; r.Setup(true, 8);
	r.ViewSample(5);	// the oldest retained sample
	EXPECT_FALSE(r.view->IsEvicted());
	EXPECT_EQ(r.UploaderBase("biome"), r.Recorded(5, "biome"));
	EXPECT_EQ(DescribeLayerView(*r.rec, *r.view, StringCRC("biome")), LayerViewState::HasData);
	const uint64_t epoch = r.view->Epoch();

	r.Write(8); r.Take();	// window now 6..8: sample 5 falls out
	EXPECT_EQ(r.rec->FirstRetained(), 6u);
	r.Frame();
	EXPECT_EQ(r.view->Mode(), ViewMode::Sample);
	EXPECT_EQ(r.view->Sample(), 5u) << "the view stays on the sample";
	EXPECT_NE(r.view->Epoch(), epoch);
	EXPECT_EQ(DescribeLayerView(*r.rec, *r.view, StringCRC("biome")), LayerViewState::Evicted);
	EXPECT_TRUE(DescribeHistoryView(*r.rec, *r.view).mEvicted);
	EXPECT_TRUE(r.UploaderBase("biome").empty()) << "the map is cleared on the epoch bump, no stale values";
	Json::Value state;
	r.domain->GetJSONState(state);
	EXPECT_TRUE(state["noData"].asBool());
}

TEST_F(HistoryViewIntegration, EachViewChangeCostsExactlyOneReadAndOneUpload)
{
	ViewRig r; r.Setup(false);
	r.ViewSample(2);
	r.Frame();
	r.Frame();	// settle

	uint64_t reads = r.uploader->Reads(), uploads = r.uploader->Uploads();
	r.ViewSample(3);
	EXPECT_EQ(r.uploader->Reads() - reads, 1u);
	EXPECT_EQ(r.uploader->Uploads() - uploads, 1u);

	// Unchanged (version, epoch): no read, no upload.
	reads = r.uploader->Reads(); uploads = r.uploader->Uploads();
	r.Frame(); r.Frame(); r.Frame();
	EXPECT_EQ(r.uploader->Reads(), reads);
	EXPECT_EQ(r.uploader->Uploads(), uploads);

	// A step is one more.
	r.ViewSample(1);
	EXPECT_EQ(r.uploader->Reads() - reads, 1u);
	EXPECT_EQ(r.uploader->Uploads() - uploads, 1u);

	// Back to live: one read of the layer's own delegate.
	reads = r.uploader->Reads(); uploads = r.uploader->Uploads();
	const uint32_t liveReads = r.grid.ReadCount("temp");
	r.GoLive();
	EXPECT_EQ(r.uploader->Reads() - reads, 1u);
	EXPECT_EQ(r.uploader->Uploads() - uploads, 1u);
	EXPECT_EQ(r.grid.ReadCount("temp") - liveReads, 1u);
}

TEST_F(HistoryViewIntegration, ScrubbedViewNeverCallsTheLiveReadDelegate)
{
	ViewRig r; r.Setup(false);
	r.ViewSample(2);
	const uint32_t tempReads = r.grid.ReadCount("temp"), biomeReads = r.grid.ReadCount("biome");
	r.ReaderValues("temp"); r.ReaderValues("biome"); r.MirrorValues("temp");
	r.QueryCount("temp > 1");
	r.ViewSample(3);
	r.ViewSample(0);
	EXPECT_EQ(r.grid.ReadCount("temp"), tempReads);
	EXPECT_EQ(r.grid.ReadCount("biome"), biomeReads);
}

TEST_F(HistoryViewIntegration, ReaderReadsOncePerViewChange)
{
	ViewRig r; r.Setup(false);
	r.ViewSample(2);
	r.ReaderValues("temp");
	const uint32_t reads = r.reader->Reads();
	r.ReaderValues("temp"); r.ReaderValues("temp");
	EXPECT_EQ(r.reader->Reads(), reads) << "unchanged (version, epoch) is a cache hit";
	r.ViewSample(3);
	r.ReaderValues("temp"); r.ReaderValues("temp");
	EXPECT_EQ(r.reader->Reads(), reads + 1u);
	EXPECT_EQ(r.ReaderValues("temp"), r.Recorded(3, "temp"));
}

TEST_F(HistoryViewIntegration, SelectionIsUnchangedByScrubbing)
{
	ViewRig r; r.Setup(false);
	r.SelectCells({ 1, 5, 7 });
	const SelectionFingerprint before = r.Fingerprint();
	r.ViewSample(2);
	r.ViewSample(4);
	r.ViewSample(0);
	EXPECT_TRUE(before == r.Fingerprint());
	r.GoLive();
	EXPECT_TRUE(before == r.Fingerprint());
	CellSelection* sel = r.domain->ActiveSelection();
	ASSERT_NE(sel, nullptr);
	EXPECT_EQ(sel->Count(), 3u);
	EXPECT_TRUE(sel->Contains(1)); EXPECT_TRUE(sel->Contains(5)); EXPECT_TRUE(sel->Contains(7));
}

TEST(HistoryViewStatus, LiveReportsTheLatestSampleAndNoneWhenEmpty)
{
	Rig r;
	r.grid.AddFloatLayer("temp", 1.0f);
	ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100) }));
	r.Start(nullptr, false);
	HistoryViewState v(*r.rec);
	HistoryViewStatus s = DescribeHistoryView(*r.rec, v);
	EXPECT_EQ(s.mMode, ViewMode::Live);
	EXPECT_FALSE(s.mHasSample);
	r.Take(); r.Take();
	s = DescribeHistoryView(*r.rec, v);
	EXPECT_TRUE(s.mHasSample);
	EXPECT_EQ(s.mSample, 1u);
	EXPECT_FALSE(s.mEvicted);
	EXPECT_EQ(DescribeLayerView(*r.rec, v, Dia::Core::StringCRC("temp")), LayerViewState::Live);
}

#endif // DIA_DEBUG

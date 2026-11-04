#include "HistoryTestRig.h"
#include <DiaCellHistory/CellHistoryDomain.h>
#include <DiaCellHistory/TimelineCardState.h>
#include <DiaCellHistory/ViewHistorySourceAdapter.h>
#include <DiaVisualDebugger/Console/PanelJson.h>
#include <DiaVisualDebugger/Domain/DiaDebugDomainRegistry.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace Dia::CellHistory;
using namespace Dia::CellHistory::Testing;
using namespace HistoryRig;
using Dia::Core::StringCRC;

namespace
{
	// 6 samples of "temp" (float) and "biome" (category). temp is tracked unless asked otherwise. Events on samples 2 and 4.
	struct CardRig : Rig
	{
		std::unique_ptr<HistoryViewState> view;
		TimelineCardState card;

		void Setup(bool trackTemp = true)
		{
			grid.AddFloatLayer("temp", 40.0f);
			grid.AddCategoryLayer("biome", 1);
			ASSERT_TRUE(schema.Load({ FloatEntry("temp", 0, 100), EnumEntry("biome", 3) }));
			cfg.mBands[0] = Dia::Core::Containers::String32("%s", "Week");
			cfg.mBands[1] = Dia::Core::Containers::String32("%s", "Year");
			cfg.mBandCount = 2;
			Start(nullptr, false);
			for (int i = 0; i < 6; ++i)
			{
				if (i == 2 || i == 4) { HistoryEvent e; e.type = StringCRC("test.a"); rec->Emit(e); }
				Take();
				if (i == 0 && trackTemp) { ASSERT_TRUE(rec->TrackLayer(StringCRC("temp"))); }	// the layout exists after the first sample
			}
			view = std::make_unique<HistoryViewState>(*rec);
		}
		bool Update(const char* base) { return card.Update(*rec, *view, StringCRC(base), schema.service.get(), &cfg); }
	};
}

TEST(TimelineCard, HistoryFieldNullWithoutSource)
{
	EXPECT_TRUE(TimelineCardState::HistoryField(nullptr, nullptr).isNull());
}

TEST(TimelineCard, HistoryFieldLiveHasNullSampleAndLabel)
{
	CardRig r; r.Setup();
	const Json::Value h = TimelineCardState::HistoryField(r.rec.get(), r.view.get());
	ASSERT_TRUE(h.isObject());
	EXPECT_EQ(h["mode"].asString(), "live");
	EXPECT_TRUE(h["sample"].isNull());
	EXPECT_TRUE(h["label"].isNull());
}

TEST(TimelineCard, HistoryFieldSampleModeCarriesIndexAndLabel)
{
	CardRig r; r.Setup();
	ViewResult vr;
	ASSERT_TRUE(r.view->ViewSample(3, vr));
	const Json::Value h = TimelineCardState::HistoryField(r.rec.get(), r.view.get());
	EXPECT_EQ(h["mode"].asString(), "sample");
	EXPECT_EQ(h["sample"].asUInt(), 3u);
	ASSERT_TRUE(h["label"].isString());
	EXPECT_EQ(h["label"].asString(), std::string(r.rec->Header(3).mLabel.AsCStr()));
}

TEST(TimelineCard, ViewHistorySourceAdapterLiveThenScrubbed)
{
	CardRig r; r.Setup();
	ViewHistorySourceAdapter adapter(*r.rec, *r.view);
	const uint64_t liveStamp = adapter.HistoryStamp();
	Json::Value h = adapter.HistoryField();
	EXPECT_EQ(h["mode"].asString(), "live");
	EXPECT_TRUE(h["sample"].isNull());
	EXPECT_TRUE(h["label"].isNull());

	ViewResult vr;
	ASSERT_TRUE(r.view->ViewSample(3, vr));
	const uint64_t scrubStamp = adapter.HistoryStamp();
	EXPECT_NE(scrubStamp, liveStamp);
	h = adapter.HistoryField();
	EXPECT_EQ(h["mode"].asString(), "sample");
	EXPECT_EQ(h["sample"].asUInt(), 3u);
	EXPECT_EQ(h["label"].asString(), std::string(r.rec->Header(3).mLabel.AsCStr()));

	ASSERT_TRUE(r.view->ViewSample(4, vr));
	EXPECT_NE(adapter.HistoryStamp(), scrubStamp);
	r.view->GoLive();
	EXPECT_NE(adapter.HistoryStamp(), scrubStamp);
	EXPECT_TRUE(adapter.HistoryField()["sample"].isNull());
}

TEST(TimelineCard, ViewHistorySourceAdapterStampIgnoresNewSamples)
{
	CardRig r; r.Setup();
	ViewHistorySourceAdapter adapter(*r.rec, *r.view);
	const uint64_t before = adapter.HistoryStamp();
	r.Take();
	EXPECT_EQ(adapter.HistoryStamp(), before);
}

TEST(TimelineCard, LiveCardHasHiddenBannerAndTimeline)
{
	CardRig r; r.Setup();
	EXPECT_TRUE(r.card.Card().isNull());
	ASSERT_TRUE(r.Update("temp"));
	const Json::Value& c = r.card.Card();
	EXPECT_TRUE(c["available"].asBool());
	EXPECT_EQ(c["baseLayer"].asString(), "temp");
	EXPECT_FALSE(c["banner"]["visible"].asBool());
	EXPECT_EQ(c["banner"]["state"].asString(), "live");
	EXPECT_TRUE(c["banner"]["sample"].isNull());
	EXPECT_EQ(c["timeline"]["range"]["latest"].asUInt(), 5u);
	EXPECT_EQ(c["timeline"]["position"]["mode"].asString(), "live");
	EXPECT_TRUE(c["history"]["sample"].isNull());
}

TEST(TimelineCard, ScrubbedTrackedLayerShowsBannerWithData)
{
	CardRig r; r.Setup();
	ViewResult vr;
	ASSERT_TRUE(r.view->ViewSample(4, vr));
	ASSERT_TRUE(r.Update("temp"));
	const Json::Value& b = r.card.Card()["banner"];
	EXPECT_TRUE(b["visible"].asBool());
	EXPECT_EQ(b["state"].asString(), "data");
	EXPECT_EQ(b["sample"].asUInt(), 4u);
	EXPECT_EQ(b["label"].asString(), std::string(r.rec->Header(4).mLabel.AsCStr()));
	EXPECT_EQ(r.card.Card()["timeline"]["position"]["sample"].asUInt(), 4u);
}

TEST(TimelineCard, ScrubbedUntrackedLayerShowsUntrackedState)
{
	CardRig r; r.Setup();
	ViewResult vr;
	ASSERT_TRUE(r.view->ViewSample(4, vr));
	ASSERT_TRUE(r.Update("biome"));
	EXPECT_TRUE(r.card.Card()["banner"]["visible"].asBool());
	EXPECT_EQ(r.card.Card()["banner"]["state"].asString(), "untracked");
	EXPECT_EQ(r.card.Card()["banner"]["layer"].asString(), "biome");
}

TEST(TimelineCard, TrackingTheLayerFlipsStateWithoutNewSample)
{
	CardRig r; r.Setup(false);
	ViewResult vr;
	ASSERT_TRUE(r.view->ViewSample(4, vr));
	ASSERT_TRUE(r.Update("temp"));
	EXPECT_EQ(r.card.Card()["banner"]["state"].asString(), "untracked");
	ASSERT_TRUE(r.rec->TrackLayer(StringCRC("temp")));
	EXPECT_TRUE(r.Update("temp")) << "layer state change must rebuild the card";
	EXPECT_EQ(r.card.Card()["banner"]["state"].asString(), "data");
}

TEST(TimelineCard, EvictedStateWhenViewedSampleBeforeWindow)
{
	Rig r;
	r.grid.AddCategoryLayer("biome", 1);
	ASSERT_TRUE(r.schema.Load({ EnumEntry("biome", 3) }));
	r.cfg.mTrackCount = 1;
	r.cfg.mTrack[0] = Dia::Core::Containers::String64("%s", "biome");
	RecorderBudgets b;
	b.mAggregateBytes = 1 << 20;
	b.mCellBytes = BudgetBytesForWindow(3, BytesPerSample(0, 1, 12));
	b.mEventBytes = 4096;
	r.Start(&b);
	for (int i = 0; i < 8; ++i) { r.Take(); }
	ASSERT_EQ(r.rec->FirstRetained(), 5u);

	HistoryViewState view(*r.rec);
	ViewResult vr;
	ASSERT_TRUE(view.ViewSample(2, vr));
	TimelineCardState card;
	ASSERT_TRUE(card.Update(*r.rec, view, StringCRC("biome"), r.schema.service.get(), &r.cfg));
	const Json::Value& banner = card.Card()["banner"];
	EXPECT_TRUE(banner["visible"].asBool());
	EXPECT_EQ(banner["state"].asString(), "evicted");
	EXPECT_EQ(banner["firstRetained"].asUInt(), 5u);
	EXPECT_TRUE(card.Card()["timeline"]["preWindowDim"].isObject());
}

TEST(TimelineCard, RebuildsOnlyOnNewSampleOrViewChange)
{
	CardRig r; r.Setup();
	ASSERT_TRUE(r.Update("temp"));
	const uint64_t builds = r.card.CardBuildCount();
	EXPECT_FALSE(r.Update("temp"));
	EXPECT_FALSE(r.Update("temp"));
	EXPECT_EQ(r.card.CardBuildCount(), builds);
	EXPECT_EQ(r.card.Timeline().BuildCount(), 1u);

	r.Take();
	EXPECT_TRUE(r.Update("temp")) << "new sample";
	EXPECT_EQ(r.card.Card()["timeline"]["range"]["latest"].asUInt(), 6u);

	ViewResult vr;
	ASSERT_TRUE(r.view->ViewSample(1, vr));
	EXPECT_TRUE(r.Update("temp")) << "view change";
	EXPECT_FALSE(r.Update("temp"));

	EXPECT_TRUE(r.Update("biome")) << "base layer change";
	EXPECT_EQ(r.card.Card()["baseLayer"].asString(), "biome");
}

#ifdef DIA_DEBUG

namespace
{
	Json::Value Obj(const char* k, int v) { Json::Value a(Json::objectValue); a[k] = v; return a; }
	Json::Value Str(const char* k, const char* v) { Json::Value a(Json::objectValue); a[k] = v; return a; }

	struct DomainRig : CardRig
	{
		std::unique_ptr<HistoryViewCommands> viewCommands;
		std::unique_ptr<CellHistoryCommands> historyCommands;
		std::unique_ptr<CellHistoryDomain> domain;
		StringCRC base = StringCRC("temp");

		StringCRC Base() const { return base; }
		void Wire()
		{
			viewCommands = std::make_unique<HistoryViewCommands>(*rec, *view, &cfg);
			historyCommands = std::make_unique<CellHistoryCommands>(*rec);
			domain = std::make_unique<CellHistoryDomain>(*rec, *view, *viewCommands, *historyCommands,
				CellHistoryDomain::BaseLayerFn(this, &DomainRig::Base), schema.service.get(), &cfg);
		}
		void Build(bool trackTemp = true) { Setup(trackTemp); Wire(); domain->Update(); }
		Json::Value State() { Json::Value s(Json::objectValue); domain->GetJSONState(s); return s; }
		void Cmd(const char* c, const Json::Value& args = Json::Value(Json::objectValue)) { domain->OnCommand(StringCRC(c), args); }
	};

	std::filesystem::path FindGoldenDir()
	{
		namespace fs = std::filesystem;
		std::error_code ec;
		fs::path dir = fs::absolute(fs::path(__FILE__), ec).parent_path();
		for (int i = 0; i < 12 && !dir.empty(); ++i)
		{
			const fs::path candidate = dir / "Contracts" / "debugpanel";
			if (fs::is_directory(candidate, ec)) { return candidate; }
			if (dir == dir.parent_path()) { break; }
			dir = dir.parent_path();
		}
		return fs::path();
	}

	std::string DomainJson(CellHistoryDomain& domain)
	{
		Dia::VisualDebugger::DiaDebugDomainRegistry registry;
		registry.Register(domain);
		Json::Value root(Json::objectValue);
		Dia::VisualDebugger::Console::BuildDomainStatesRoot(registry, root);
		Json::FastWriter writer;
		writer.omitEndingLineFeed();
		const std::string out = writer.write(root);
		registry.Clear();
		return out;
	}

	void CheckGolden(const char* fileName, const std::string& produced)
	{
		const std::filesystem::path dir = FindGoldenDir();
		ASSERT_FALSE(dir.empty()) << "Golden folder Contracts/debugpanel not found above " << __FILE__;
		const std::filesystem::path file = dir / fileName;
		const char* update = std::getenv("DIA_UPDATE_GOLDEN");
		if (update != nullptr && std::strcmp(update, "1") == 0)
		{
			std::ofstream f(file, std::ios::binary | std::ios::trunc);
			ASSERT_TRUE(f.good());
			f << produced;
			return;
		}
		std::ifstream f(file, std::ios::binary);
		ASSERT_TRUE(f.good()) << "golden " << fileName << " missing; regenerate with DIA_UPDATE_GOLDEN=1";
		std::ostringstream ss; ss << f.rdbuf();
		EXPECT_EQ(ss.str(), produced) << "golden " << fileName << " differs; regenerate with DIA_UPDATE_GOLDEN=1 and review";
	}
}

TEST(CellHistoryDomain, IdentityAndPanelOnly)
{
	DomainRig r; r.Build();
	EXPECT_STREQ(r.domain->GetDisplayName(), "Cell History");
	EXPECT_EQ(r.domain->GetDomainId(), StringCRC("cellhistory"));
	EXPECT_FALSE(r.domain->HasWorldDrawers());
	EXPECT_LE(strlen(r.domain->GetDescription()), 80u);
}

TEST(CellHistoryDomain, StateIsTheCard)
{
	DomainRig r; r.Build();
	const Json::Value s = r.State();
	EXPECT_EQ(s, r.domain->Card().Card());
	EXPECT_TRUE(s.isMember("timeline"));
	EXPECT_TRUE(s.isMember("banner"));
}

TEST(CellHistoryDomain, StateBeforeFirstUpdateIsUnavailable)
{
	DomainRig r; r.Setup(); r.Wire();
	EXPECT_FALSE(r.State()["available"].asBool());
}

TEST(CellHistoryDomain, PanelViewStepEventLiveRunTheSameCommands)
{
	DomainRig r; r.Build();
	r.Cmd("view", Obj("sample", 3));
	EXPECT_TRUE(r.domain->LastResult()["ok"].asBool());
	EXPECT_EQ(r.State()["banner"]["sample"].asUInt(), 3u);
	EXPECT_TRUE(r.State()["banner"]["visible"].asBool());

	r.Cmd("step", Obj("n", -1));
	EXPECT_EQ(r.State()["banner"]["sample"].asUInt(), 2u);

	r.Cmd("event", Str("direction", "next"));
	EXPECT_EQ(r.State()["banner"]["sample"].asUInt(), 4u);

	r.Cmd("live");
	EXPECT_FALSE(r.State()["banner"]["visible"].asBool());
	EXPECT_TRUE(r.State()["history"]["sample"].isNull());
}

TEST(CellHistoryDomain, FailedCommandLeavesStateAndRecordsError)
{
	DomainRig r; r.Build();
	r.Cmd("view", Obj("sample", 999));
	EXPECT_TRUE(r.domain->LastResult().isMember("error"));
	EXPECT_FALSE(r.State()["banner"]["visible"].asBool());
}

TEST(CellHistoryDomain, TrackCommandDefaultsToBaseAndFixesUntrackedState)
{
	DomainRig r; r.Build(false);
	r.Cmd("view", Obj("sample", 3));
	EXPECT_EQ(r.State()["banner"]["state"].asString(), "untracked");
	r.Cmd("track");	// no layer arg: the base layer
	EXPECT_TRUE(r.domain->LastResult()["ok"].asBool());
	EXPECT_TRUE(r.rec->IsTracked(StringCRC("temp")));
	EXPECT_EQ(r.State()["banner"]["state"].asString(), "data");
}

TEST(CellHistoryDomain, TrackCommandWithExplicitLayer)
{
	DomainRig r; r.Build(false);
	r.Cmd("track", Str("layer", "biome"));
	EXPECT_TRUE(r.rec->IsTracked(StringCRC("biome")));
	EXPECT_FALSE(r.rec->IsTracked(StringCRC("temp")));
}

TEST(CellHistoryDomain, UnknownCommandIsIgnored)
{
	DomainRig r; r.Build();
	const Json::Value before = r.State();
	r.Cmd("bogus");
	EXPECT_EQ(r.State(), before);
}

TEST(CellHistoryDomain, GoldenLive)
{
	DomainRig r; r.Build();
	CheckGolden("domain_states_cellhistory_live.json", DomainJson(*r.domain));
}

TEST(CellHistoryDomain, GoldenScrubbed)
{
	DomainRig r; r.Build();
	r.Cmd("view", Obj("sample", 3));
	CheckGolden("domain_states_cellhistory_scrubbed.json", DomainJson(*r.domain));
}

TEST(CellHistoryDomain, GoldenUntracked)
{
	DomainRig r; r.Build(false);
	r.Cmd("view", Obj("sample", 3));
	CheckGolden("domain_states_cellhistory_untracked.json", DomainJson(*r.domain));
}

#endif

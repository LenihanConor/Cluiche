#include "HistoryTestRig.h"
#include <DiaCellHistory/HistoryViewCommands.h>
#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCellLayers/Testing/FakeMirrorRegistrar.h>

#include <json/json.h>

#ifdef DIA_DEBUG

using namespace Dia::CellHistory;
using namespace Dia::CellHistory::Testing;
using namespace HistoryRig;
using Dia::CellLayers::Testing::FakeMirrorRegistrar;
using Dia::Core::StringCRC;

namespace
{
	// Samples 0..count-1. Events: sample 2 "test.a", sample 5 "test.b", sample 7 "test.a" (when count > 7).
	struct ViewRig : Rig
	{
		FakeMirrorRegistrar registrar;
		std::unique_ptr<HistoryViewState> view;
		std::unique_ptr<HistoryViewCommands> commands;

		void Setup(int count)
		{
			grid.AddFloatLayer("temp", 40.0f);
			ASSERT_TRUE(schema.Load({ FloatEntry("temp", 0, 100) }));
			cfg.mBands[0] = Dia::Core::Containers::String32("%s", "Week");
			cfg.mBands[1] = Dia::Core::Containers::String32("%s", "Year");
			cfg.mBandCount = 2;
			Start(nullptr, false);
			for (int i = 0; i < count; ++i)
			{
				const char* type = (i == 2 || i == 7) ? "test.a" : (i == 5 ? "test.b" : nullptr);
				if (type != nullptr) { HistoryEvent e; e.type = StringCRC(type); ASSERT_EQ(rec->Emit(e), EmitResult::Ok); }
				Take();
			}
			view = std::make_unique<HistoryViewState>(*rec);
			commands = std::make_unique<HistoryViewCommands>(*rec, *view, &cfg);
			ASSERT_TRUE(commands->Register(registrar));
		}

		Json::Value Run(const char* name, const Json::Value& args = Json::Value(Json::objectValue)) { return registrar.Execute(name, args); }
		Json::Value View(const Json::Value& sample) { Json::Value a(Json::objectValue); a["sample"] = sample; return Run("cells.history.view", a); }
		Json::Value Step(const Json::Value& n, const char* band = nullptr)
		{
			Json::Value a(Json::objectValue); a["n"] = n;
			if (band != nullptr) { a["band"] = band; }
			return Run("cells.history.step", a);
		}
		Json::Value Event(const char* dir, const char* type = nullptr)
		{
			Json::Value a(Json::objectValue); a["direction"] = dir;
			if (type != nullptr) { a["type"] = type; }
			return Run("cells.history.event", a);
		}
	};

	bool IsError(const Json::Value& v) { return v.isObject() && v.isMember("error") && v["error"].isString() && !v["error"].asString().empty(); }

	struct Snap
	{
		ViewMode mode; uint32_t sample; uint64_t epoch;
		explicit Snap(const HistoryViewState& v) : mode(v.Mode()), sample(v.Sample()), epoch(v.Epoch()) {}
		bool Same(const HistoryViewState& v) const { return mode == v.Mode() && sample == v.Sample() && epoch == v.Epoch(); }
	};

	void ExpectOkAt(ViewRig& r, const Json::Value& out, uint32_t sample)
	{
		ASSERT_FALSE(IsError(out)) << out.toStyledString();
		EXPECT_TRUE(out["ok"].asBool());
		EXPECT_EQ(out["mode"].asString(), "sample");
		EXPECT_EQ(out["sample"].asUInt(), sample);
		EXPECT_EQ(out["label"].asString(), std::string(r.rec->Header(sample).mLabel.AsCStr()));
		EXPECT_EQ(r.view->Mode(), ViewMode::Sample);
		EXPECT_EQ(r.view->Sample(), sample);
	}

	void ExpectErrorUnchanged(ViewRig& r, const Json::Value& out, const Snap& before)
	{
		EXPECT_TRUE(IsError(out)) << out.toStyledString();
		EXPECT_TRUE(before.Same(*r.view));
	}
}

TEST(HistoryViewCommands, RegistersFiveCommandsWithHelp)
{
	ViewRig r; r.Setup(3);
	EXPECT_EQ(r.registrar.Count(), 5u);
	for (const char* name : { "cells.history.view", "cells.history.live", "cells.history.step", "cells.history.event", "cells.history.delta" })
	{
		ASSERT_TRUE(r.registrar.Has(name)) << name;
		EXPECT_TRUE(r.registrar.IsCommand(name)) << name;
		const Dia::DebugServer::SimHelpStored* help = r.registrar.Help(name);
		ASSERT_NE(help, nullptr) << name;
		EXPECT_GT(strlen(help->description), 0u) << name;
		EXPECT_STREQ(help->category, "cells.history") << name;
		EXPECT_GT(strlen(help->owner), 0u) << name;
	}
	EXPECT_TRUE(r.registrar.Help("cells.history.live")->noArgs);
	EXPECT_EQ(r.registrar.Help("cells.history.view")->paramCount, 1u);
	EXPECT_TRUE(r.registrar.Help("cells.history.view")->params[0].required);
	EXPECT_EQ(r.registrar.Help("cells.history.step")->paramCount, 2u);
	EXPECT_TRUE(r.registrar.Help("cells.history.step")->params[0].required);
	EXPECT_FALSE(r.registrar.Help("cells.history.step")->params[1].required);
	EXPECT_EQ(r.registrar.Help("cells.history.event")->paramCount, 2u);
	EXPECT_FALSE(r.registrar.Help("cells.history.event")->params[1].required);
}

TEST(HistoryViewCommands, UnregisterRemovesAllAndRegisterIsIdempotent)
{
	ViewRig r; r.Setup(3);
	EXPECT_TRUE(r.commands->Register(r.registrar));
	EXPECT_EQ(r.registrar.Count(), 5u);
	r.commands->Unregister(r.registrar);
	EXPECT_EQ(r.registrar.Count(), 0u);
	EXPECT_TRUE(r.commands->Register(r.registrar));
	EXPECT_EQ(r.registrar.Count(), 5u);
}

TEST(HistoryViewCommands, RegisterRollsBackWhenTheRegistrarRejectsAName)
{
	ViewRig r; r.Setup(3);
	r.commands->Unregister(r.registrar);
	FakeMirrorRegistrar taken;
	Dia::DebugServer::SimHelpInfo help;
	help.description = "taken";
	ASSERT_TRUE(taken.RegisterCommand("cells.history.step", help, [](const Json::Value&) { return Json::Value(); }));
	EXPECT_FALSE(r.commands->Register(taken));
	EXPECT_FALSE(taken.Has("cells.history.view"));
	EXPECT_FALSE(taken.Has("cells.history.live"));
	EXPECT_EQ(taken.Count(), 1u);
}

TEST(HistoryViewCommands, ViewEntersScrubModeAtTheSample)
{
	ViewRig r; r.Setup(6);
	ExpectOkAt(r, r.View(3), 3);
	ExpectOkAt(r, r.View(0), 0);
	ExpectOkAt(r, r.View(5), 5);		// the latest sample is scrub mode, not live
	EXPECT_FALSE(r.view->IsLive());
}

TEST(HistoryViewCommands, ViewAcceptsDecimalStrings)
{
	ViewRig r; r.Setup(6);
	ExpectOkAt(r, r.View("4"), 4);
}

TEST(HistoryViewCommands, ViewStaysOnTheSampleAsNewOnesArrive)
{
	ViewRig r; r.Setup(4);
	ExpectOkAt(r, r.View(1), 1);
	r.Take(); r.Take();
	r.view->Update();
	EXPECT_EQ(r.view->Mode(), ViewMode::Sample);
	EXPECT_EQ(r.view->Sample(), 1u);
}

TEST(HistoryViewCommands, ViewBadArgumentsAreErrorsAndChangeNothing)
{
	ViewRig r; r.Setup(6);
	Snap live(*r.view);
	ExpectErrorUnchanged(r, r.View(6), live);					// out of range, from live
	ExpectErrorUnchanged(r, r.View(1000), live);
	ExpectErrorUnchanged(r, r.View(-1), live);
	ExpectErrorUnchanged(r, r.View("abc"), live);				// non-numeric
	ExpectErrorUnchanged(r, r.View("3x"), live);
	ExpectErrorUnchanged(r, r.View(""), live);
	ExpectErrorUnchanged(r, r.View(2.5), live);
	ExpectErrorUnchanged(r, r.View(Json::Value(true)), live);
	ExpectErrorUnchanged(r, r.View(Json::Value(Json::nullValue)), live);
	ExpectErrorUnchanged(r, r.Run("cells.history.view"), live);	// missing arg
	ExpectErrorUnchanged(r, r.Run("cells.history.view", Json::Value(Json::nullValue)), live);

	ExpectOkAt(r, r.View(2), 2);
	Snap scrub(*r.view);
	ExpectErrorUnchanged(r, r.View(6), scrub);					// from scrub
	ExpectErrorUnchanged(r, r.View("abc"), scrub);
	ExpectErrorUnchanged(r, r.Run("cells.history.view"), scrub);
}

TEST(HistoryViewCommands, ViewWithNoSamplesIsAnError)
{
	ViewRig r; r.Setup(0);
	Snap live(*r.view);
	ExpectErrorUnchanged(r, r.View(0), live);
}

TEST(HistoryViewCommands, LiveReturnsToLiveFromScrub)
{
	ViewRig r; r.Setup(6);
	ExpectOkAt(r, r.View(2), 2);
	const Json::Value out = r.Run("cells.history.live");
	ASSERT_FALSE(IsError(out)) << out.toStyledString();
	EXPECT_TRUE(out["ok"].asBool());
	EXPECT_EQ(out["mode"].asString(), "live");
	EXPECT_EQ(out["sample"].asInt(), 5);
	EXPECT_EQ(out["label"].asString(), std::string(r.rec->Header(5).mLabel.AsCStr()));
	EXPECT_TRUE(r.view->IsLive());
}

TEST(HistoryViewCommands, LiveFromLiveIsOkAndChangesNothing)
{
	ViewRig r; r.Setup(3);
	const Snap before(*r.view);
	const Json::Value out = r.Run("cells.history.live");
	EXPECT_TRUE(out["ok"].asBool());
	EXPECT_EQ(out["mode"].asString(), "live");
	EXPECT_TRUE(before.Same(*r.view));
}

TEST(HistoryViewCommands, LiveWithNoSamplesReportsMinusOne)
{
	ViewRig r; r.Setup(0);
	const Json::Value out = r.Run("cells.history.live");
	EXPECT_TRUE(out["ok"].asBool());
	EXPECT_EQ(out["sample"].asInt(), -1);
	EXPECT_EQ(out["label"].asString(), "");
}

TEST(HistoryViewCommands, StepMovesBothDirections)
{
	ViewRig r; r.Setup(10);
	ExpectOkAt(r, r.View(5), 5);
	ExpectOkAt(r, r.Step(2), 7);
	ExpectOkAt(r, r.Step(-3), 4);
	ExpectOkAt(r, r.Step(1), 5);
	ExpectOkAt(r, r.Step(-1), 4);
	ExpectOkAt(r, r.Step("-2"), 2);
	ExpectOkAt(r, r.Step(0), 2);
}

TEST(HistoryViewCommands, StepClampsAtBothEnds)
{
	ViewRig r; r.Setup(10);
	ExpectOkAt(r, r.View(2), 2);
	ExpectOkAt(r, r.Step(-50), 0);
	ExpectOkAt(r, r.Step(-1), 0);
	ExpectOkAt(r, r.Step(50), 9);
	ExpectOkAt(r, r.Step(1), 9);
	ExpectOkAt(r, r.Step(Json::Value(Json::Int64(1) << 40)), 9);
	ExpectOkAt(r, r.Step(Json::Value(-(Json::Int64(1) << 40))), 0);
}

TEST(HistoryViewCommands, StepFromLiveStartsAtTheLatestAndEntersScrub)
{
	ViewRig r; r.Setup(10);
	ExpectOkAt(r, r.Step(-3), 6);
	r.view->GoLive();
	ExpectOkAt(r, r.Step(0), 9);
	r.view->GoLive();
	ExpectOkAt(r, r.Step(4), 9);		// clamped, still enters scrub mode
	EXPECT_FALSE(r.view->IsLive());
}

TEST(HistoryViewCommands, StepByBandFindsTheNearestBoundary)
{
	ViewRig r; r.Setup(120);			// Year-band samples: 0, 52, 104
	ExpectOkAt(r, r.View(110), 110);
	ExpectOkAt(r, r.Step(-1, "Year"), 104);		// previous Year start
	ExpectOkAt(r, r.Step(-1, "Year"), 52);
	ExpectOkAt(r, r.Step(-1, "Year"), 0);
	ExpectOkAt(r, r.Step(-1, "Year"), 0);		// none earlier: clamps to 0
	ExpectOkAt(r, r.Step(1, "Year"), 52);
	ExpectOkAt(r, r.Step(7, "Year"), 104);		// magnitude of n is ignored with a band
	ExpectOkAt(r, r.Step(1, "Year"), 119);		// none later: clamps to the latest
	ExpectOkAt(r, r.Step(-1, "1"), 104);		// band by index
	ExpectOkAt(r, r.Step(-1, "Week"), 103);		// Week (0) is at least band 0: every sample qualifies
	ExpectOkAt(r, r.Step(0, "Year"), 103);		// n == 0 stays put
}

TEST(HistoryViewCommands, StepByBandFromLive)
{
	ViewRig r; r.Setup(120);
	ExpectOkAt(r, r.Step(-1, "Year"), 104);
	r.view->GoLive();
	ExpectOkAt(r, r.Step(1, "Year"), 119);
}

TEST(HistoryViewCommands, StepBadArgumentsAreErrorsAndChangeNothing)
{
	ViewRig r; r.Setup(10);
	Snap live(*r.view);
	ExpectErrorUnchanged(r, r.Step("abc"), live);
	ExpectErrorUnchanged(r, r.Step(1.5), live);
	ExpectErrorUnchanged(r, r.Step(Json::Value(true)), live);
	ExpectErrorUnchanged(r, r.Step(1, "Decade"), live);			// unknown band name
	ExpectErrorUnchanged(r, r.Step(1, "5"), live);				// band index beyond the configured bands
	ExpectErrorUnchanged(r, r.Step(1, ""), live);
	ExpectErrorUnchanged(r, r.Run("cells.history.step"), live);	// missing n
	{
		Json::Value b(Json::objectValue); b["n"] = 1; b["band"] = Json::Value(Json::arrayValue);
		ExpectErrorUnchanged(r, r.Run("cells.history.step", b), live);
	}

	ExpectOkAt(r, r.View(4), 4);
	Snap scrub(*r.view);
	ExpectErrorUnchanged(r, r.Step("abc"), scrub);
	ExpectErrorUnchanged(r, r.Step(1, "Decade"), scrub);
	ExpectErrorUnchanged(r, r.Run("cells.history.step"), scrub);
}

TEST(HistoryViewCommands, StepWithNoSamplesIsAnError)
{
	ViewRig r; r.Setup(0);
	Snap live(*r.view);
	ExpectErrorUnchanged(r, r.Step(1), live);
}

TEST(HistoryViewCommands, EventPrevAndNextFromScrub)
{
	ViewRig r; r.Setup(10);			// events at 2 (a), 5 (b), 7 (a)
	ExpectOkAt(r, r.View(6), 6);
	ExpectOkAt(r, r.Event("prev"), 5);
	ExpectOkAt(r, r.Event("prev"), 2);
	ExpectOkAt(r, r.Event("next"), 5);
	ExpectOkAt(r, r.Event("next"), 7);
	ExpectOkAt(r, r.Event("prev"), 5);
	ExpectOkAt(r, r.View(5), 5);
	ExpectOkAt(r, r.Event("next"), 7);	// strictly after the current sample
}

TEST(HistoryViewCommands, EventFromLiveStartsAtTheLatest)
{
	ViewRig r; r.Setup(10);
	ExpectOkAt(r, r.Event("prev"), 7);
	r.view->GoLive();
	Snap live(*r.view);
	ExpectErrorUnchanged(r, r.Event("next"), live);	// nothing after the latest
}

TEST(HistoryViewCommands, EventTypeFilter)
{
	ViewRig r; r.Setup(10);
	ExpectOkAt(r, r.View(9), 9);
	ExpectOkAt(r, r.Event("prev", "test.b"), 5);
	ExpectOkAt(r, r.Event("prev", "test.a"), 2);
	ExpectOkAt(r, r.Event("next", "test.a"), 7);
	ExpectOkAt(r, r.View(0), 0);
	ExpectOkAt(r, r.Event("next", "test.b"), 5);
}

TEST(HistoryViewCommands, EventNotFoundIsAnErrorAndChangesNothing)
{
	ViewRig r; r.Setup(10);
	ExpectOkAt(r, r.View(2), 2);
	Snap at2(*r.view);
	ExpectErrorUnchanged(r, r.Event("prev"), at2);					// no earlier event
	ExpectErrorUnchanged(r, r.Event("prev", "test.a"), at2);
	ExpectOkAt(r, r.View(7), 7);
	Snap at7(*r.view);
	ExpectErrorUnchanged(r, r.Event("next"), at7);					// no later event
	ExpectErrorUnchanged(r, r.Event("next", "test.b"), at7);		// none of this type later
	ExpectErrorUnchanged(r, r.Event("prev", "test.unknown"), at7);	// type with no events
	ExpectErrorUnchanged(r, r.Event("next", "test.unknown"), at7);
}

TEST(HistoryViewCommands, EventBadArgumentsAreErrorsAndChangeNothing)
{
	ViewRig r; r.Setup(10);
	Snap live(*r.view);
	ExpectErrorUnchanged(r, r.Event("sideways"), live);
	ExpectErrorUnchanged(r, r.Event(""), live);
	ExpectErrorUnchanged(r, r.Event("PREV"), live);
	ExpectErrorUnchanged(r, r.Event("prev", ""), live);
	ExpectErrorUnchanged(r, r.Run("cells.history.event"), live);	// missing direction
	{
		Json::Value a(Json::objectValue); a["direction"] = 3;
		ExpectErrorUnchanged(r, r.Run("cells.history.event", a), live);
		Json::Value b(Json::objectValue); b["direction"] = "prev"; b["type"] = 5;
		ExpectErrorUnchanged(r, r.Run("cells.history.event", b), live);
	}
	ExpectOkAt(r, r.View(6), 6);
	Snap scrub(*r.view);
	ExpectErrorUnchanged(r, r.Event("sideways"), scrub);
	ExpectErrorUnchanged(r, r.Run("cells.history.event"), scrub);
}

TEST(HistoryViewCommands, ErrorsDoNotBumpTheEpoch)
{
	ViewRig r; r.Setup(10);
	ExpectOkAt(r, r.View(3), 3);
	const uint64_t e = r.view->Epoch();
	r.View(99); r.Step("x"); r.Event("up"); r.Event("prev", "nope");
	EXPECT_EQ(r.view->Epoch(), e);
}

TEST(HistoryViewCommands, SampleBeforeTheWindowStartIsAccepted)
{
	ViewRig r;
	r.grid.AddCategoryLayer("biome", 1);
	ASSERT_TRUE(r.schema.Load({ EnumEntry("biome", 3) }));
	r.cfg.mTrackCount = 1;
	r.cfg.mTrack[0] = Dia::Core::Containers::String64("%s", "biome");
	RecorderBudgets b;
	b.mAggregateBytes = 1 << 20;
	b.mCellBytes = BudgetBytesForWindow(3, BytesPerSample(0, 1, 12));
	b.mEventBytes = 4096;
	r.Start(&b, false);
	for (int i = 0; i < 8; ++i) { r.Take(); }
	ASSERT_EQ(r.rec->FirstRetained(), 5u);
	r.view = std::make_unique<HistoryViewState>(*r.rec);
	r.commands = std::make_unique<HistoryViewCommands>(*r.rec, *r.view);
	ASSERT_TRUE(r.commands->Register(r.registrar));
	ExpectOkAt(r, r.View(1), 1);
	EXPECT_TRUE(r.view->IsEvicted());
}

TEST(HistoryViewCommands, BandByNameNeedsAConfig)
{
	ViewRig r; r.Setup(60);
	r.commands = std::make_unique<HistoryViewCommands>(*r.rec, *r.view);	// no config
	Snap live(*r.view);
	Json::Value a(Json::objectValue); a["n"] = -1; a["band"] = "Year";
	EXPECT_TRUE(IsError(r.commands->Step(a)));
	EXPECT_TRUE(live.Same(*r.view));
	a["band"] = "1";									// an index still works without a config
	EXPECT_FALSE(IsError(r.commands->Step(a)));
	EXPECT_EQ(r.view->Sample(), 52u);
}


TEST(HistoryViewCommands, StepAndEventAreRefusedInDeltaMode)
{
	ViewRig r; r.Setup(10);
	ViewResult vr;
	Side now; now.kind = SideKind::Now;
	Side smp; smp.kind = SideKind::Sample; smp.sample = 1;
	ASSERT_TRUE(r.view->EnterDelta(StringCRC("temp"), now, smp, vr));
	Snap before(*r.view);
	const Json::Value step = r.Step(-1);
	ASSERT_TRUE(IsError(step));
	EXPECT_EQ(step["error"].asString(), "leave delta mode first");
	const Json::Value ev = r.Event("prev");
	ASSERT_TRUE(IsError(ev));
	EXPECT_EQ(ev["error"].asString(), "leave delta mode first");
	EXPECT_TRUE(before.Same(*r.view));
	EXPECT_TRUE(r.view->InDelta());
}

TEST(HistoryViewCommands, ViewAndLiveLeaveDeltaMode)
{
	ViewRig r; r.Setup(10);
	ViewResult vr;
	Side now; now.kind = SideKind::Now;
	Side smp; smp.kind = SideKind::Sample; smp.sample = 1;
	ASSERT_TRUE(r.view->EnterDelta(StringCRC("temp"), now, smp, vr));
	ExpectOkAt(r, r.View(4), 4);
	EXPECT_FALSE(r.view->InDelta());
	ASSERT_TRUE(r.view->EnterDelta(StringCRC("temp"), now, smp, vr));
	const Json::Value live = r.Run("cells.history.live");
	EXPECT_EQ(live["mode"].asString(), "live");
	EXPECT_FALSE(r.view->InDelta());
}

#endif // DIA_DEBUG

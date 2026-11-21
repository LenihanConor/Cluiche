#include "HistoryTestRig.h"
#include <DiaCellHistory/CellHistoryCommands.h>
#include <DiaCellHistory/CellHistoryDomain.h>
#include <DiaCellHistory/HistoryViewCommands.h>
#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCellLayers/Testing/FakeMirrorRegistrar.h>

#include <json/json.h>

#include <cmath>
#include <limits>

#ifdef DIA_DEBUG

using namespace Dia::CellHistory;
using namespace HistoryRig;
using Dia::CellLayers::Testing::FakeMirrorRegistrar;
using Dia::Core::StringCRC;

namespace
{
	const char* const kDelta = "cells.history.delta";

	// 5 samples (0..4) of temp/biome (tracked); rain is an untracked float layer.
	struct DeltaRig : Rig
	{
		FakeMirrorRegistrar registrar;
		std::unique_ptr<HistoryViewState> view;
		std::unique_ptr<HistoryViewCommands> commands;

		void Setup(int count = 5, bool withRegistry = true)
		{
			grid.AddFloatLayer("temp", 40.0f);
			grid.AddCategoryLayer("biome", 1);
			grid.AddFloatLayer("rain", 5.0f);
			ASSERT_TRUE(schema.Load({ FloatEntry("temp", 0, 100), EnumEntry("biome", 3), FloatEntry("rain", 0, 10) }));
			cfg.mTrackCount = 2;
			cfg.mTrack[0] = Dia::Core::Containers::String64("%s", "temp");
			cfg.mTrack[1] = Dia::Core::Containers::String64("%s", "biome");
			Start(nullptr, false);
			for (int i = 0; i < count; ++i)
			{
				for (uint32_t c = 0; c < 12; ++c) { grid.Set("temp", c, float(10 * i + c)); }
				grid.BumpVersion("temp");
				Take();
			}
			view = std::make_unique<HistoryViewState>(*rec);
			commands = std::make_unique<HistoryViewCommands>(*rec, *view, &cfg);
			if (withRegistry) { commands->SetLayerRegistry(&registry); }
			ASSERT_TRUE(commands->Register(registrar));
		}

		static Json::Value Args(const char* layer, const char* a, const char* b)
		{
			Json::Value v(Json::objectValue); v["layer"] = layer; v["a"] = a; v["b"] = b; return v;
		}
		Json::Value Run(const Json::Value& args) { return registrar.Execute(kDelta, args); }
		Json::Value Enter(const char* layer, const char* a, const char* b) { return Run(Args(layer, a, b)); }
		Json::Value Key(const char* k, const Json::Value& v) { Json::Value a(Json::objectValue); a[k] = v; return Run(a); }
	};

	Json::Value One(const char* k, const Json::Value& v) { Json::Value a(Json::objectValue); a[k] = v; return a; }

	bool IsError(const Json::Value& v) { return v.isObject() && v.isMember("error") && v["error"].isString() && !v["error"].asString().empty(); }

	struct Snap
	{
		ViewMode mode; uint32_t sample; uint64_t epoch; bool groupB;
		explicit Snap(const HistoryViewState& v) : mode(v.Mode()), sample(v.Sample()), epoch(v.Epoch()), groupB(v.Delta().groupSideB) {}
		bool Same(const HistoryViewState& v) const { return mode == v.Mode() && sample == v.Sample() && epoch == v.Epoch() && groupB == v.Delta().groupSideB; }
	};

	void ExpectRejected(DeltaRig& r, const Json::Value& args, const char* fragment = nullptr)
	{
		const Snap before(*r.view);
		const Json::Value out = r.Run(args);
		EXPECT_TRUE(IsError(out)) << out.toStyledString();
		if (fragment != nullptr && IsError(out)) { EXPECT_NE(out["error"].asString().find(fragment), std::string::npos) << out["error"].asString(); }
		EXPECT_TRUE(before.Same(*r.view)) << args.toStyledString();
	}
}

TEST(HistoryDeltaCommands, RegistersOneMoreCommandWithHelp)
{
	DeltaRig r; r.Setup();
	EXPECT_EQ(r.registrar.Count(), 5u);
	ASSERT_TRUE(r.registrar.Has(kDelta));
	EXPECT_TRUE(r.registrar.IsCommand(kDelta));
	const Dia::DebugServer::SimHelpStored* help = r.registrar.Help(kDelta);
	ASSERT_NE(help, nullptr);
	EXPECT_GT(strlen(help->description), 0u);
	EXPECT_STREQ(help->category, "cells.history");
	EXPECT_STREQ(help->owner, "DiaCellHistory");
	EXPECT_GE(help->paramCount, 4u);
	for (uint32_t i = 0; i < help->paramCount; ++i) { EXPECT_FALSE(help->params[i].required) << help->params[i].name; }
}

TEST(HistoryDeltaCommands, UnregisterRemovesTheNameAndRegisterIsIdempotent)
{
	DeltaRig r; r.Setup();
	EXPECT_TRUE(r.commands->Register(r.registrar));
	EXPECT_EQ(r.registrar.Count(), 5u);
	r.commands->Unregister(r.registrar);
	EXPECT_FALSE(r.registrar.Has(kDelta));
	EXPECT_EQ(r.registrar.Count(), 0u);
	EXPECT_TRUE(r.commands->Register(r.registrar));
	EXPECT_TRUE(r.registrar.Has(kDelta));
}

TEST(HistoryDeltaCommands, EnterNowVersusSample)
{
	DeltaRig r; r.Setup();
	const uint64_t e0 = r.view->Epoch();
	const Json::Value out = r.Enter("temp", "now", "sample:2");
	ASSERT_FALSE(IsError(out)) << out.toStyledString();
	EXPECT_TRUE(out["ok"].asBool());
	EXPECT_EQ(out["mode"].asString(), "delta");
	EXPECT_EQ(out["a"].asString(), "now");
	EXPECT_EQ(out["b"].asString(), "sample:2");
	EXPECT_TRUE(out["scale"].isNull());
	EXPECT_EQ(out["layer"].asString(), "temp");
	EXPECT_EQ(out["groupside"].asString(), "B");
	EXPECT_TRUE(r.view->InDelta());
	EXPECT_GT(r.view->Epoch(), e0);
	EXPECT_EQ(r.view->Delta().layer, StringCRC("temp"));
	EXPECT_EQ(r.view->Delta().a.kind, SideKind::Now);
	EXPECT_EQ(r.view->Delta().b.sample, 2u);
}

TEST(HistoryDeltaCommands, EnterSampleVersusSampleAndSampleVersusNow)
{
	DeltaRig r; r.Setup();
	Json::Value out = r.Enter("temp", "sample:4", "sample:1");
	ASSERT_FALSE(IsError(out)) << out.toStyledString();
	EXPECT_EQ(out["a"].asString(), "sample:4");
	EXPECT_EQ(out["b"].asString(), "sample:1");
	out = r.Enter("temp", "sample:0", "now");
	ASSERT_FALSE(IsError(out)) << out.toStyledString();
	EXPECT_EQ(out["a"].asString(), "sample:0");
	EXPECT_EQ(out["b"].asString(), "now");
	EXPECT_TRUE(r.view->InDelta());
}

TEST(HistoryDeltaCommands, EnterWithScaleFixesItInOneCall)
{
	DeltaRig r; r.Setup();
	Json::Value a = DeltaRig::Args("temp", "now", "sample:1");
	a["scale"] = 12.5;
	const Json::Value out = r.Run(a);
	ASSERT_FALSE(IsError(out)) << out.toStyledString();
	EXPECT_DOUBLE_EQ(out["scale"].asDouble(), 12.5);
	EXPECT_FALSE(r.view->Delta().autoScale);
	EXPECT_FLOAT_EQ(r.view->Delta().fixedScale, 12.5f);

	a["scale"] = "auto";
	const Json::Value back = r.Run(a);
	ASSERT_FALSE(IsError(back)) << back.toStyledString();
	EXPECT_TRUE(back["scale"].isNull());
	EXPECT_TRUE(r.view->Delta().autoScale);
}

TEST(HistoryDeltaCommands, EnterAcceptsNumericStrings)
{
	DeltaRig r; r.Setup();
	Json::Value a = DeltaRig::Args("temp", "now", "sample:1");
	a["scale"] = "3.5";
	const Json::Value out = r.Run(a);
	ASSERT_FALSE(IsError(out)) << out.toStyledString();
	EXPECT_DOUBLE_EQ(out["scale"].asDouble(), 3.5);
}

TEST(HistoryDeltaCommands, ScaleFixedThenAuto)
{
	DeltaRig r; r.Setup();
	ASSERT_FALSE(IsError(r.Enter("temp", "now", "sample:1")));
	const uint64_t e0 = r.view->Epoch();
	Json::Value out = r.Key("scale", 7.0);
	ASSERT_FALSE(IsError(out)) << out.toStyledString();
	EXPECT_EQ(out["mode"].asString(), "delta");
	EXPECT_DOUBLE_EQ(out["scale"].asDouble(), 7.0);
	EXPECT_EQ(out["a"].asString(), "now");
	EXPECT_EQ(out["b"].asString(), "sample:1");
	EXPECT_GT(r.view->Epoch(), e0);
	EXPECT_FLOAT_EQ(r.view->Delta().fixedScale, 7.0f);

	out = r.Key("scale", "auto");
	ASSERT_FALSE(IsError(out)) << out.toStyledString();
	EXPECT_TRUE(out["scale"].isNull());
	EXPECT_TRUE(r.view->Delta().autoScale);

	out = r.Key("scale", "2");
	ASSERT_FALSE(IsError(out)) << out.toStyledString();
	EXPECT_DOUBLE_EQ(out["scale"].asDouble(), 2.0);
}

TEST(HistoryDeltaCommands, GroupSideSwitchesAndResetsOnReentry)
{
	DeltaRig r; r.Setup();
	ASSERT_FALSE(IsError(r.Enter("temp", "now", "sample:1")));
	EXPECT_TRUE(r.view->Delta().groupSideB);
	uint64_t e = r.view->Epoch();
	Json::Value out = r.Key("groupside", "A");
	ASSERT_FALSE(IsError(out)) << out.toStyledString();
	EXPECT_EQ(out["groupside"].asString(), "A");
	EXPECT_EQ(out["mode"].asString(), "delta");
	EXPECT_FALSE(r.view->Delta().groupSideB);
	EXPECT_GT(r.view->Epoch(), e);

	e = r.view->Epoch();
	out = r.Key("groupside", "A");		// already A: no change
	ASSERT_FALSE(IsError(out));
	EXPECT_EQ(r.view->Epoch(), e);

	out = r.Key("groupside", "B");
	EXPECT_EQ(out["groupside"].asString(), "B");
	EXPECT_TRUE(r.view->Delta().groupSideB);
	EXPECT_GT(r.view->Epoch(), e);

	ASSERT_FALSE(IsError(r.Key("groupside", "A")));
	ASSERT_FALSE(IsError(r.Enter("temp", "now", "sample:2")));
	EXPECT_TRUE(r.view->Delta().groupSideB);	// reset to B on each new entry
}

TEST(HistoryDeltaCommands, OffReturnsToLiveWhenLiveWasHeld)
{
	DeltaRig r; r.Setup();
	ASSERT_FALSE(IsError(r.Enter("temp", "now", "sample:1")));
	const Json::Value out = r.Run(One("off", true));
	ASSERT_FALSE(IsError(out)) << out.toStyledString();
	EXPECT_TRUE(out["ok"].asBool());
	EXPECT_EQ(out["mode"].asString(), "live");
	EXPECT_EQ(out["sample"].asInt(), 4);
	EXPECT_TRUE(r.view->IsLive());
}

TEST(HistoryDeltaCommands, OffReturnsToTheHeldSample)
{
	DeltaRig r; r.Setup();
	ViewResult vr;
	ASSERT_TRUE(r.view->ViewSample(3, vr));
	ASSERT_FALSE(IsError(r.Enter("temp", "now", "sample:1")));
	const Json::Value out = r.Run(One("off", true));
	ASSERT_FALSE(IsError(out)) << out.toStyledString();
	EXPECT_EQ(out["mode"].asString(), "sample");
	EXPECT_EQ(out["sample"].asInt(), 3);
	EXPECT_EQ(r.view->Mode(), ViewMode::Sample);
}

TEST(HistoryDeltaCommands, ViewAndLiveLeaveDeltaWithCorrectResults)
{
	DeltaRig r; r.Setup();
	ASSERT_FALSE(IsError(r.Enter("temp", "now", "sample:1")));
	Json::Value out = r.registrar.Execute("cells.history.view", One("sample", 2));
	ASSERT_FALSE(IsError(out)) << out.toStyledString();
	EXPECT_EQ(out["mode"].asString(), "sample");
	EXPECT_EQ(out["sample"].asInt(), 2);
	EXPECT_FALSE(r.view->InDelta());

	ASSERT_FALSE(IsError(r.Enter("temp", "now", "sample:1")));
	out = r.registrar.Execute("cells.history.live", Json::Value(Json::objectValue));
	EXPECT_EQ(out["mode"].asString(), "live");
	EXPECT_EQ(out["sample"].asInt(), 4);
	EXPECT_FALSE(r.view->InDelta());
}

TEST(HistoryDeltaCommands, UnknownLayerRejected)
{
	DeltaRig r; r.Setup();
	ExpectRejected(r, DeltaRig::Args("nope", "now", "sample:1"), "layer");
	ExpectRejected(r, DeltaRig::Args("", "now", "sample:1"));
	ASSERT_FALSE(IsError(r.Enter("temp", "now", "sample:1")));		// also while already in delta
	ExpectRejected(r, DeltaRig::Args("nope", "now", "sample:2"), "layer");
	EXPECT_EQ(r.view->Delta().b.sample, 1u);
}

TEST(HistoryDeltaCommands, CategoryLayerRejected)
{
	DeltaRig r; r.Setup();
	ExpectRejected(r, DeltaRig::Args("biome", "now", "sample:1"), "category layers have no delta in v1");
}

TEST(HistoryDeltaCommands, BadSideTextRejected)
{
	DeltaRig r; r.Setup();
	ExpectRejected(r, DeltaRig::Args("temp", "yesterday", "sample:1"));
	ExpectRejected(r, DeltaRig::Args("temp", "now", "sample:-1"));
	ExpectRejected(r, DeltaRig::Args("temp", "now", "sample:x"));
	ExpectRejected(r, DeltaRig::Args("temp", "now", "snapshot:"));
	ExpectRejected(r, DeltaRig::Args("temp", "", "now"));
}

TEST(HistoryDeltaCommands, SampleOutOfRangeRejected)
{
	DeltaRig r; r.Setup();
	ExpectRejected(r, DeltaRig::Args("temp", "now", "sample:5"), "out of range");
	ExpectRejected(r, DeltaRig::Args("temp", "sample:99", "now"), "out of range");
	ExpectRejected(r, DeltaRig::Args("temp", "sample:99", "sample:100"), "out of range");
}

TEST(HistoryDeltaCommands, SnapshotSidesNotAvailableYet)
{
	DeltaRig r; r.Setup();
	ExpectRejected(r, DeltaRig::Args("temp", "now", "snapshot:before"), "snapshot sides are not available yet");
	ExpectRejected(r, DeltaRig::Args("temp", "snapshot:a", "sample:1"), "snapshot sides are not available yet");
}

TEST(HistoryDeltaCommands, IdenticalSidesRejected)
{
	DeltaRig r; r.Setup();
	ExpectRejected(r, DeltaRig::Args("temp", "now", "now"), "identical");
	ExpectRejected(r, DeltaRig::Args("temp", "sample:2", "sample:2"), "identical");
}

TEST(HistoryDeltaCommands, NoDataOnBothSidesRejectedButOneSideIsAllowed)
{
	DeltaRig r; r.Setup();
	// rain is not tracked: no per-cell history, so both sample sides have no data.
	ExpectRejected(r, DeltaRig::Args("rain", "sample:1", "sample:2"), "no data");
	// 'now' always has data, so one side missing is allowed (the Warning is logged at read time).
	const Json::Value out = r.Enter("rain", "now", "sample:2");
	ASSERT_FALSE(IsError(out)) << out.toStyledString();
	EXPECT_TRUE(r.view->InDelta());
}

TEST(HistoryDeltaCommands, BadScaleRejectedAndChangesNothing)
{
	DeltaRig r; r.Setup();
	const Json::Value bads[] = { Json::Value(0.0), Json::Value(-1.0), Json::Value("wide"), Json::Value("0"), Json::Value(true),
		Json::Value(Json::nullValue), Json::Value(std::numeric_limits<double>::infinity()), Json::Value("nan"), Json::Value("") };
	for (const Json::Value& bad : bads)
	{
		Json::Value a = DeltaRig::Args("temp", "now", "sample:1");
		a["scale"] = bad;
		ExpectRejected(r, a, "scale");
	}

	ASSERT_FALSE(IsError(r.Enter("temp", "now", "sample:1")));
	ASSERT_FALSE(IsError(r.Key("scale", 4.0)));
	for (const Json::Value& bad : bads)
	{
		ExpectRejected(r, One("scale", bad), "scale");
	}
	EXPECT_FLOAT_EQ(r.view->Delta().fixedScale, 4.0f);
}

TEST(HistoryDeltaCommands, MissingOrIllTypedEnterArgsRejected)
{
	DeltaRig r; r.Setup();
	for (const char* missing : { "a", "b" })
	{
		Json::Value a = DeltaRig::Args("temp", "now", "sample:1");
		a.removeMember(missing);
		ExpectRejected(r, a);
	}
	{
		Json::Value a = DeltaRig::Args("temp", "now", "sample:1");
		a.removeMember("layer");
		ExpectRejected(r, a);
	}
	for (const char* key : { "layer", "a", "b" })
	{
		Json::Value a = DeltaRig::Args("temp", "now", "sample:1");
		a[key] = 7;
		ExpectRejected(r, a);
		a[key] = Json::Value(Json::nullValue);
		ExpectRejected(r, a);
	}
	ExpectRejected(r, Json::Value(Json::nullValue));
	ExpectRejected(r, Json::Value("temp"));
	ExpectRejected(r, Json::Value(Json::objectValue), "give either");
}

TEST(HistoryDeltaCommands, MixedFormsRejected)
{
	DeltaRig r; r.Setup();
	ASSERT_FALSE(IsError(r.Enter("temp", "now", "sample:1")));
	{ Json::Value a = DeltaRig::Args("temp", "now", "sample:2"); a["groupside"] = "A"; ExpectRejected(r, a, "give either layer/a/b, scale, groupside or off"); }
	{ Json::Value a = DeltaRig::Args("temp", "now", "sample:2"); a["off"] = true; ExpectRejected(r, a, "give either"); }
	{ Json::Value a(Json::objectValue); a["scale"] = 3; a["groupside"] = "A"; ExpectRejected(r, a, "give either"); }
	{ Json::Value a(Json::objectValue); a["scale"] = 3; a["off"] = true; ExpectRejected(r, a, "give either"); }
	{ Json::Value a(Json::objectValue); a["groupside"] = "A"; a["off"] = true; ExpectRejected(r, a, "give either"); }
	{ Json::Value a(Json::objectValue); a["layer"] = "temp"; a["scale"] = 3; ExpectRejected(r, a); }
	EXPECT_TRUE(r.view->InDelta());
}

TEST(HistoryDeltaCommands, GroupSideWrongValuesRejected)
{
	DeltaRig r; r.Setup();
	ASSERT_FALSE(IsError(r.Enter("temp", "now", "sample:1")));
	const Json::Value bads[] = { Json::Value("C"), Json::Value("a"), Json::Value(""), Json::Value(1), Json::Value(true), Json::Value(Json::nullValue) };
	for (const Json::Value& bad : bads)
	{
		ExpectRejected(r, One("groupside", bad), "groupside");
	}
}

TEST(HistoryDeltaCommands, OffValueMustBeTrue)
{
	DeltaRig r; r.Setup();
	ASSERT_FALSE(IsError(r.Enter("temp", "now", "sample:1")));
	const Json::Value bads[] = { Json::Value(false), Json::Value("yes"), Json::Value(1), Json::Value(Json::nullValue) };
	for (const Json::Value& bad : bads)
	{
		ExpectRejected(r, One("off", bad), "off");
	}
	EXPECT_TRUE(r.view->InDelta());
}

TEST(HistoryDeltaCommands, ScaleGroupsideAndOffNeedDeltaMode)
{
	DeltaRig r; r.Setup();
	ExpectRejected(r, One("scale", 3), "not in delta");
	ExpectRejected(r, One("scale", "auto"), "not in delta");
	ExpectRejected(r, One("groupside", "A"), "not in delta");
	ExpectRejected(r, One("off", true), "not in delta");
	ViewResult vr;
	ASSERT_TRUE(r.view->ViewSample(2, vr));
	ExpectRejected(r, One("off", true), "not in delta");
	EXPECT_EQ(r.view->Mode(), ViewMode::Sample);
}

TEST(HistoryDeltaCommands, WithoutALayerRegistryEnterReportsIt)
{
	DeltaRig r; r.Setup(5, false);
	ExpectRejected(r, DeltaRig::Args("temp", "now", "sample:1"), "layer registry not available");
}

TEST(HistoryDeltaCommands, StepStillRefusedAfterEntering)
{
	DeltaRig r; r.Setup();
	ASSERT_FALSE(IsError(r.Enter("temp", "now", "sample:1")));
	EXPECT_TRUE(IsError(r.registrar.Execute("cells.history.step", One("n", 1))));
	EXPECT_TRUE(r.view->InDelta());
}

TEST(HistoryDeltaCommands, ReenteringTheSameSpecDoesNotBumpTheEpoch)
{
	DeltaRig r; r.Setup();
	ASSERT_FALSE(IsError(r.Enter("temp", "now", "sample:1")));
	const uint64_t e = r.view->Epoch();
	ASSERT_FALSE(IsError(r.Enter("temp", "now", "sample:1")));
	EXPECT_EQ(r.view->Epoch(), e);
}

TEST(HistoryDeltaCommands, StateSetDeltaGroupSide)
{
	DeltaRig r; r.Setup();
	ViewResult vr;
	EXPECT_FALSE(r.view->SetDeltaGroupSide(false, vr));	// not in delta
	EXPECT_FALSE(vr.mOk);
	EXPECT_STREQ(vr.mError, "not in delta mode");
	Side now; now.kind = SideKind::Now;
	Side s1; s1.kind = SideKind::Sample; s1.sample = 1;
	ASSERT_TRUE(r.view->EnterDelta(StringCRC("temp"), now, s1, vr));
	EXPECT_TRUE(r.view->Delta().groupSideB);
	const uint64_t e = r.view->Epoch();
	EXPECT_TRUE(r.view->SetDeltaGroupSide(true, vr));	// no-op
	EXPECT_EQ(r.view->Epoch(), e);
	EXPECT_TRUE(r.view->SetDeltaGroupSide(false, vr));
	EXPECT_FALSE(r.view->Delta().groupSideB);
	EXPECT_EQ(r.view->Epoch(), e + 1);
}

namespace
{
	struct BaseProvider { StringCRC Get() const { return StringCRC("temp"); } };
}

TEST(HistoryDeltaCommands, PanelDomainDrivesDelta)
{
	DeltaRig r; r.Setup();
	CellHistoryCommands historyCommands(*r.rec);
	BaseProvider base;
	CellHistoryDomain domain(*r.rec, *r.view, *r.commands, historyCommands,
		CellHistoryDomain::BaseLayerFn(&base, &BaseProvider::Get), r.schema.service.get(), &r.cfg);
	domain.OnCommand(StringCRC("delta"), DeltaRig::Args("temp", "now", "sample:1"));
	EXPECT_TRUE(r.view->InDelta());
	domain.OnCommand(StringCRC("delta"), One("scale", 5));
	EXPECT_FLOAT_EQ(r.view->Delta().fixedScale, 5.0f);
	domain.OnCommand(StringCRC("delta"), One("off", true));
	EXPECT_FALSE(r.view->InDelta());
}

#endif // DIA_DEBUG

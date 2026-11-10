#include "SnapshotSideRig.h"

#include <DiaCellHistory/CellHistoryCommands.h>
#include <DiaCellHistory/DeltaCardState.h>
#include <DiaCellHistory/DeltaCellInfo.h>
#include <DiaCellHistory/GroupDeltaTable.h>
#include <DiaCellHistory/HistoryValueSource.h>
#include <DiaCellHistory/HistoryViewCommands.h>
#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCellHistory/SnapshotCommands.h>
#include <DiaCellHistory/TimelineCardState.h>
#include <DiaCellHistory/TimelineModel.h>
#include <DiaCellLayers/Testing/FakeMirrorRegistrar.h>

#include <json/json.h>

#include <cmath>
#include <fstream>

using namespace Dia::CellHistory;
using namespace SnapRig;
#ifdef DIA_DEBUG
using Dia::CellLayers::Testing::FakeMirrorRegistrar;
#endif
using Dia::Core::StringCRC;

namespace
{
	Side SnapSide(const char* name) { Side s; s.kind = SideKind::Snapshot; s.snapshot = Dia::Core::Containers::String64("%s", name); return s; }
	Side SampleSide(uint32_t i) { Side s; s.kind = SideKind::Sample; s.sample = i; return s; }
	Side NowSide() { return Side(); }

	// Quantisation of a 0..100 layer: 100 / 65,535 per recorded side.
	constexpr float kTol = 0.004f;

	// 5 samples (0..4); snapshot "pre" = temp 20 + c (taken after sample 2), "post" = 40 + c (after sample 4); live temp 70 + c.
	struct DS
	{
		Fix f;
		void Init()
		{
			f.Setup(3);
			ASSERT_TRUE(f.Snap("pre"));
			f.Sample(3); f.Sample(4);
			ASSERT_TRUE(f.Snap("post"));
			f.SetLiveTemp(70.0f);
		}
		const Dia::CellLayers::CellLayerDesc& Temp() const { return f.Desc("temp"); }
	};

	bool IsError(const Json::Value& v) { return v.isObject() && v.isMember("error") && v["error"].isString() && !v["error"].asString().empty(); }
}

// ---- SideReader ----------------------------------------------------------------------------------------------

TEST(SideReaderSnapshot, OkValuesEqualTheDequantisedFile)
{
	DS d; d.Init();
	SideReader sr(*d.f.r.rec);
	sr.SetSnapshotCache(d.f.cache.get());
	float out[12] = {};
	ASSERT_EQ(sr.Read(SnapSide("pre"), d.Temp(), out, 12), SideReadStatus::Ok);
	for (uint32_t c = 0; c < 12; ++c) { EXPECT_NEAR(out[c], 20.0f + float(c), kTol) << c; }

	char err[128] = {};
	const SnapshotCellData* data = d.f.cache->Get("pre", err, sizeof(err));
	ASSERT_NE(data, nullptr);
	float expect[12] = {};
	ASSERT_TRUE(SnapshotCellFile::Dequantise(*data, StringCRC("temp"), expect, 12));
	for (uint32_t c = 0; c < 12; ++c) { EXPECT_EQ(out[c], expect[c]) << c; }
}

TEST(SideReaderSnapshot, CategoryIdsComeBackAsFloats)
{
	DS d; d.Init();
	SideReader sr(*d.f.r.rec);
	sr.SetSnapshotCache(d.f.cache.get());
	float out[12] = {};
	ASSERT_EQ(sr.Read(SnapSide("pre"), d.f.Desc("biome"), out, 12), SideReadStatus::Ok);
	EXPECT_EQ(out[0], 1.0f);
	EXPECT_EQ(out[6], 2.0f);
}

TEST(SideReaderSnapshot, NeverFallsBackToLiveValues)
{
	DS d; d.Init();
	SideReader sr(*d.f.r.rec);
	sr.SetSnapshotCache(d.f.cache.get());
	float out[12] = {};
	d.f.SetLiveTemp(99.0f);
	ASSERT_EQ(sr.Read(SnapSide("pre"), d.Temp(), out, 12), SideReadStatus::Ok);
	EXPECT_NEAR(out[0], 20.0f, kTol);
	EXPECT_EQ(sr.Read(SnapSide("pre"), d.f.Desc("rain"), out, 12), SideReadStatus::NoData);	// rain (aggregate false) is live-readable but not in the file
}

TEST(SideReaderSnapshot, NoCacheIsUnavailable)
{
	DS d; d.Init();
	SideReader sr(*d.f.r.rec);
	float out[12] = {};
	EXPECT_EQ(sr.Read(SnapSide("pre"), d.Temp(), out, 12), SideReadStatus::Unavailable);
	sr.SetSnapshotCache(d.f.cache.get());
	sr.SetSnapshotCache(nullptr);
	EXPECT_EQ(sr.Read(SnapSide("pre"), d.Temp(), out, 12), SideReadStatus::Unavailable);
}

TEST(SideReaderSnapshot, LayerAbsentFromTheFileIsNoData)
{
	DS d; d.Init();
	SideReader sr(*d.f.r.rec);
	sr.SetSnapshotCache(d.f.cache.get());
	float out[12] = {};
	EXPECT_EQ(sr.Read(SnapSide("pre"), d.f.Desc("rain"), out, 12), SideReadStatus::NoData);
}

TEST(SideReaderSnapshot, WrongGridIsNoData)
{
	DS d; d.Init();
	SideReader sr(*d.f.r.rec);
	sr.SetSnapshotCache(d.f.cache.get());
	Dia::CellLayers::CellLayerDesc layer = d.Temp();
	Dia::Graphics::CellGridDesc other = *layer.grid;
	other.id = StringCRC("another.grid");
	layer.grid = &other;
	float out[12] = {};
	EXPECT_EQ(sr.Read(SnapSide("pre"), layer, out, 12), SideReadStatus::NoData);
	layer.grid = nullptr;
	EXPECT_EQ(sr.Read(SnapSide("pre"), layer, out, 12), SideReadStatus::NoData);
}

TEST(SideReaderSnapshot, CellCountMismatchIsNoData)
{
	DS d; d.Init();
	SideReader sr(*d.f.r.rec);
	sr.SetSnapshotCache(d.f.cache.get());
	float out[16] = {};
	EXPECT_EQ(sr.Read(SnapSide("pre"), d.Temp(), out, 11), SideReadStatus::NoData);
	EXPECT_EQ(sr.Read(SnapSide("pre"), d.Temp(), out, 13), SideReadStatus::NoData);
	EXPECT_EQ(sr.Read(SnapSide("pre"), d.Temp(), nullptr, 12), SideReadStatus::NoData);
}

TEST(SideReaderSnapshot, UnloadableSnapshotIsNoDataNotUnavailable)
{
	DS d; d.Init();
	SideReader sr(*d.f.r.rec);
	sr.SetSnapshotCache(d.f.cache.get());
	float out[12] = {};
	EXPECT_EQ(sr.Read(SnapSide("ghost"), d.Temp(), out, 12), SideReadStatus::NoData);
}

// ---- values through HistoryValueSource -----------------------------------------------------------------------

namespace
{
	struct ValueRig : DS
	{
		std::unique_ptr<HistoryViewState> view;
		std::unique_ptr<HistoryValueSource> source;
		float out[12] = {};
		void Init2()
		{
			Init();
			view = std::make_unique<HistoryViewState>(*f.r.rec);
			source = std::make_unique<HistoryValueSource>(f.r.registry, *f.r.rec, *view);
			source->SetSnapshotCache(f.cache.get());
		}
		bool Enter(const Side& a, const Side& b)
		{
			ViewResult vr;
			return view->EnterDelta(StringCRC("temp"), a, b, vr);
		}
	};
}

TEST(DeltaSnapshotValues, SnapshotVersusNow)
{
	ValueRig v; v.Init2();
	ASSERT_TRUE(v.Enter(SnapSide("pre"), NowSide()));
	ASSERT_TRUE(v.source->GetValues(v.Temp(), v.out, 12));
	for (uint32_t c = 0; c < 12; ++c) { EXPECT_NEAR(v.out[c], 50.0f, kTol) << c; }
}

TEST(DeltaSnapshotValues, SnapshotVersusSample)
{
	ValueRig v; v.Init2();
	ASSERT_TRUE(v.Enter(SnapSide("pre"), SampleSide(3)));
	ASSERT_TRUE(v.source->GetValues(v.Temp(), v.out, 12));
	for (uint32_t c = 0; c < 12; ++c) { EXPECT_NEAR(v.out[c], 10.0f, 2 * kTol) << c; }
}

TEST(DeltaSnapshotValues, SampleVersusSnapshotIsBMinusA)
{
	ValueRig v; v.Init2();
	ASSERT_TRUE(v.Enter(SampleSide(3), SnapSide("pre")));
	ASSERT_TRUE(v.source->GetValues(v.Temp(), v.out, 12));
	for (uint32_t c = 0; c < 12; ++c) { EXPECT_NEAR(v.out[c], -10.0f, 2 * kTol) << c; }
}

TEST(DeltaSnapshotValues, SnapshotVersusSnapshot)
{
	ValueRig v; v.Init2();
	ASSERT_TRUE(v.Enter(SnapSide("pre"), SnapSide("post")));
	ASSERT_TRUE(v.source->GetValues(v.Temp(), v.out, 12));
	for (uint32_t c = 0; c < 12; ++c) { EXPECT_NEAR(v.out[c], 20.0f, 2 * kTol) << c; }
}

TEST(DeltaSnapshotValues, LayerAbsentFromSnapshotHasNoDelta)
{
	ValueRig v; v.Init2();
	ViewResult vr;
	ASSERT_TRUE(v.view->EnterDelta(StringCRC("rain"), SnapSide("pre"), NowSide(), vr));
	EXPECT_FALSE(v.source->GetValues(v.f.Desc("rain"), v.out, 12));
}

TEST(DeltaSnapshotValues, WithoutCacheTheSnapshotSideHasNoDelta)
{
	ValueRig v; v.Init2();
	v.source->SetSnapshotCache(nullptr);
	ASSERT_TRUE(v.Enter(SnapSide("pre"), NowSide()));
	EXPECT_FALSE(v.source->GetValues(v.Temp(), v.out, 12));
}

// ---- hover / cell info / group table / delta card --------------------------------------------------------------

TEST(DeltaSnapshotCellInfo, ReaderUsesTheSnapshotSide)
{
	DS d; d.Init();
	DeltaCellReader reader(*d.f.r.rec);
	reader.SetSnapshotCache(d.f.cache.get());
	DeltaSpec spec; spec.layer = StringCRC("temp"); spec.a = SnapSide("pre"); spec.b = NowSide();
	DeltaCellInfo info;
	ASSERT_TRUE(reader.Get(d.Temp(), spec, 1, 3, info));
	EXPECT_TRUE(info.aHasData);
	EXPECT_TRUE(info.bHasData);
	EXPECT_NEAR(info.a, 23.0f, kTol);
	EXPECT_NEAR(info.b, 73.0f, 1e-3f);
	EXPECT_NEAR(info.delta, 50.0f, kTol);
}

TEST(DeltaSnapshotCellInfo, MissingSnapshotHasNoDataAndNaNDelta)
{
	DS d; d.Init();
	DeltaCellReader reader(*d.f.r.rec);
	reader.SetSnapshotCache(d.f.cache.get());
	DeltaSpec spec; spec.layer = StringCRC("temp"); spec.a = SnapSide("ghost"); spec.b = NowSide();
	DeltaCellInfo info;
	ASSERT_TRUE(reader.Get(d.Temp(), spec, 1, 3, info));
	EXPECT_FALSE(info.aHasData);
	EXPECT_TRUE(info.bHasData);
	EXPECT_TRUE(std::isnan(info.delta));
}

TEST(DeltaSnapshotGroups, GroupSideASnapshotDefinesMembership)
{
	DS d; d.Init();
	GroupDeltaBuilder b(*d.f.r.rec);
	b.SetSnapshotCache(d.f.cache.get());
	DeltaSpec spec; spec.layer = StringCRC("temp"); spec.a = SnapSide("pre"); spec.b = SnapSide("post"); spec.groupSideB = false;
	const Dia::CellLayers::CellLayerDesc groups[1] = { d.f.Desc("biome") };
	auto t = std::make_unique<GroupDeltaTable>();
	b.Build(d.Temp(), spec, groups, 1, nullptr, *t);
	EXPECT_FALSE(t->baseNoData);
	ASSERT_EQ(t->rowCount, 2u);	// biome 1 on cells 0..5 and biome 2 on cells 6..11 in snapshot "pre"
	for (uint32_t i = 0; i < t->rowCount; ++i)
	{
		EXPECT_EQ(t->rows[i].count, 6u);
		EXPECT_NEAR(t->rows[i].change, 20.0, 2 * kTol);
	}
}

TEST(DeltaSnapshotGroups, WithoutCacheBaseIsNoData)
{
	DS d; d.Init();
	GroupDeltaBuilder b(*d.f.r.rec);
	DeltaSpec spec; spec.layer = StringCRC("temp"); spec.a = SnapSide("pre"); spec.b = SnapSide("post");
	const Dia::CellLayers::CellLayerDesc groups[1] = { d.f.Desc("biome") };
	auto t = std::make_unique<GroupDeltaTable>();
	b.Build(d.Temp(), spec, groups, 1, nullptr, *t);
	EXPECT_TRUE(t->baseNoData);
}

TEST(DeltaSnapshotCard, GroupsAndHoverUseTheSnapshotSide)
{
	DS d; d.Init();
	HistoryViewState view(*d.f.r.rec);
	DeltaCardState card(*d.f.r.rec, view);
	card.SetSnapshotCache(d.f.cache.get());
	ViewResult vr;
	ASSERT_TRUE(view.EnterDelta(StringCRC("temp"), SnapSide("pre"), SnapSide("post"), vr));
	card.Update(&d.f.r.registry, d.f.r.schema.service.get(), 1, 0);	// cell 1
	const Json::Value& c = card.Card();
	ASSERT_TRUE(c.isObject());
	EXPECT_EQ(c["a"].asString(), "snapshot:pre");
	EXPECT_EQ(c["b"].asString(), "snapshot:post");
	ASSERT_TRUE(c.isMember("groups"));
	EXPECT_FALSE(c["groups"]["baseNoData"].asBool());
	ASSERT_TRUE(c["hover"].isObject());
	EXPECT_TRUE(c["hover"]["aHasData"].asBool());
	EXPECT_NEAR(c["hover"]["a"].asDouble(), 21.0, kTol);
	EXPECT_NEAR(c["hover"]["delta"].asDouble(), 20.0, 2 * kTol);
}

// ---- timeline markers ------------------------------------------------------------------------------------------

TEST(DeltaSnapshotTimeline, MarkerIsTheSnapshotHeaderSample)
{
	DS d; d.Init();
	HistoryViewState view(*d.f.r.rec);
	ViewResult vr;
	ASSERT_TRUE(view.EnterDelta(StringCRC("temp"), SnapSide("pre"), SnapSide("post"), vr));
	TimelineModel m;
	m.SetSnapshotCache(d.f.cache.get());
	m.Build(*d.f.r.rec, view, StringCRC("temp"), d.f.r.schema.service.get(), &d.f.r.cfg);
	EXPECT_TRUE(m.DeltaAHasSample());
	EXPECT_TRUE(m.DeltaBHasSample());
	EXPECT_EQ(m.DeltaASample(), 2u);
	EXPECT_EQ(m.DeltaBSample(), 4u);
	const Json::Value j = m.ToJson();
	EXPECT_EQ(j["deltaMarkers"]["a"].asUInt(), 2u);
	EXPECT_EQ(j["deltaMarkers"]["b"].asUInt(), 4u);
}

TEST(DeltaSnapshotTimeline, NowStaysNullAndUnavailableSnapshotIsNull)
{
	DS d; d.Init();
	HistoryViewState view(*d.f.r.rec);
	ViewResult vr;
	ASSERT_TRUE(view.EnterDelta(StringCRC("temp"), SnapSide("ghost"), NowSide(), vr));
	TimelineModel m;
	m.SetSnapshotCache(d.f.cache.get());
	m.Build(*d.f.r.rec, view, StringCRC("temp"), d.f.r.schema.service.get(), &d.f.r.cfg);
	EXPECT_FALSE(m.DeltaAHasSample());
	EXPECT_FALSE(m.DeltaBHasSample());
	const Json::Value j = m.ToJson();
	EXPECT_TRUE(j["deltaMarkers"]["a"].isNull());
	EXPECT_TRUE(j["deltaMarkers"]["b"].isNull());
}

TEST(DeltaSnapshotTimeline, NoCacheMeansNoSnapshotMarker)
{
	DS d; d.Init();
	HistoryViewState view(*d.f.r.rec);
	ViewResult vr;
	ASSERT_TRUE(view.EnterDelta(StringCRC("temp"), SnapSide("pre"), SampleSide(1), vr));
	TimelineModel m;
	m.Build(*d.f.r.rec, view, StringCRC("temp"), d.f.r.schema.service.get(), &d.f.r.cfg);
	EXPECT_FALSE(m.DeltaAHasSample());
	EXPECT_TRUE(m.DeltaBHasSample());
}

TEST(DeltaSnapshotTimeline, PanelStateKeepsTheSnapshotSideLabel)
{
	DS d; d.Init();
	HistoryViewState view(*d.f.r.rec);
	ViewResult vr;
	ASSERT_TRUE(view.EnterDelta(StringCRC("temp"), SnapSide("pre"), NowSide(), vr));
	const Json::Value h = TimelineCardState::HistoryField(d.f.r.rec.get(), &view);
	EXPECT_EQ(h["a"].asString(), "snapshot:pre");
	EXPECT_EQ(h["b"].asString(), "now");
}

#ifdef DIA_DEBUG

// ---- the cells.history.delta enter path ------------------------------------------------------------------------

namespace
{
	const char* const kDelta = "cells.history.delta";

	struct CmdRig : DS
	{
		FakeMirrorRegistrar registrar;
		std::unique_ptr<HistoryViewState> view;
		std::unique_ptr<HistoryViewCommands> commands;

		void Init2(bool withCache = true)
		{
			Init();
			view = std::make_unique<HistoryViewState>(*f.r.rec);
			commands = std::make_unique<HistoryViewCommands>(*f.r.rec, *view, &f.r.cfg);
			commands->SetLayerRegistry(&f.r.registry);
			if (withCache) { commands->SetSnapshotCache(f.cache.get()); }
			ASSERT_TRUE(commands->Register(registrar));
		}
		static Json::Value Args(const char* layer, const char* a, const char* b)
		{
			Json::Value v(Json::objectValue); v["layer"] = layer; v["a"] = a; v["b"] = b; return v;
		}
		Json::Value Enter(const char* layer, const char* a, const char* b) { return registrar.Execute(kDelta, Args(layer, a, b)); }
		// Rejected, with `fragment` in the message, and the view is exactly as before.
		void ExpectRejected(const char* layer, const char* a, const char* b, const char* fragment)
		{
			const ViewMode mode = view->Mode();
			const uint64_t epoch = view->Epoch();
			const Json::Value out = Enter(layer, a, b);
			EXPECT_TRUE(IsError(out)) << out.toStyledString();
			if (IsError(out)) { EXPECT_NE(out["error"].asString().find(fragment), std::string::npos) << out["error"].asString(); }
			EXPECT_EQ(view->Mode(), mode);
			EXPECT_EQ(view->Epoch(), epoch);
			EXPECT_FALSE(view->InDelta());
		}
	};
}

TEST(DeltaSnapshotCommand, EntersWithSnapshotSidesOnEitherOrBoth)
{
	CmdRig r; r.Init2();
	Json::Value out = r.Enter("temp", "snapshot:pre", "now");
	ASSERT_FALSE(IsError(out)) << out.toStyledString();
	EXPECT_EQ(out["mode"].asString(), "delta");
	EXPECT_EQ(out["a"].asString(), "snapshot:pre");
	EXPECT_EQ(out["b"].asString(), "now");
	out = r.Enter("temp", "sample:1", "snapshot:post");
	EXPECT_FALSE(IsError(out)) << out.toStyledString();
	out = r.Enter("temp", "snapshot:pre", "snapshot:post");
	EXPECT_FALSE(IsError(out)) << out.toStyledString();
	EXPECT_EQ(r.view->Delta().b.snapshot.AsCStr(), std::string("post"));
}

TEST(DeltaSnapshotCommand, UnknownSnapshotIsRejected)
{
	CmdRig r; r.Init2();
	r.ExpectRejected("temp", "snapshot:ghost", "now", "unknown snapshot 'ghost'");
	r.ExpectRejected("temp", "now", "snapshot:ghost", "unknown snapshot 'ghost'");
}

TEST(DeltaSnapshotCommand, InvalidNameIsRejected)
{
	CmdRig r; r.Init2();
	r.ExpectRejected("temp", "snapshot:a.b", "now", "invalid snapshot name");
	r.ExpectRejected("temp", "now", "snapshot:0123456789012345678901234567890123", "invalid snapshot name");	// 34 chars: parses, too long for a snapshot
}

TEST(DeltaSnapshotCommand, SnapshotForAnotherGridIsRejected)
{
	CmdRig r; r.Init2();
	SnapshotCellData alien;
	std::snprintf(alien.mHeader.mName, sizeof(alien.mHeader.mName), "alien");
	alien.mHeader.mRunId = r.f.r.rec->RunId();
	alien.mGrid = SnapshotGridInfo();
	std::snprintf(alien.mGrid.mId, sizeof(alien.mGrid.mId), "%s", "some.other.grid");
	alien.mGrid.mWidth = 2; alien.mGrid.mHeight = 2;
	ASSERT_NE(alien.AddLayer("temp", SnapshotLayerKind::Float, 0.0f, 100.0f, 0), nullptr);
	char err[160] = {};
	ASSERT_TRUE(SnapshotCellFile::WriteAtomic(alien, r.f.Path("alien").c_str(), err, sizeof(err))) << err;
	r.ExpectRejected("temp", "snapshot:alien", "now", "snapshot 'alien' is for a different grid (2 x 2)");
}

TEST(DeltaSnapshotCommand, CorruptSnapshotFileIsRejectedWithTheReason)
{
	CmdRig r; r.Init2();
	{ std::ofstream o(r.f.Path("pre"), std::ios::binary | std::ios::trunc); o << "{ truncated"; }
	r.ExpectRejected("temp", "snapshot:pre", "now", "snapshot 'pre'");
	EXPECT_GE(r.f.cache->LoadFailures(), 1u);
}

TEST(DeltaSnapshotCommand, IdenticalSnapshotSidesAreRejected)
{
	CmdRig r; r.Init2();
	r.ExpectRejected("temp", "snapshot:pre", "snapshot:pre", "identical");
}

TEST(DeltaSnapshotCommand, LayerAbsentFromBothSnapshotsIsRejected)
{
	CmdRig r; r.Init2();
	r.ExpectRejected("rain", "snapshot:pre", "snapshot:post", "no data on either side");
}

TEST(DeltaSnapshotCommand, LayerAbsentFromOneSnapshotSideStillEntersWhenTheOtherSideHasData)
{
	CmdRig r; r.Init2();
	const Json::Value out = r.Enter("rain", "snapshot:pre", "now");
	EXPECT_FALSE(IsError(out)) << out.toStyledString();
}

TEST(DeltaSnapshotCommand, WithoutCacheTheOldMessageStays)
{
	CmdRig r; r.Init2(false);
	r.ExpectRejected("temp", "snapshot:pre", "now", "snapshot sides are not available yet");
	r.ExpectRejected("temp", "sample:1", "snapshot:pre", "snapshot sides are not available yet");
}

TEST(DeltaSnapshotCommand, SampleAndNowSidesStillWorkWithoutCache)
{
	CmdRig r; r.Init2(false);
	EXPECT_FALSE(IsError(r.Enter("temp", "sample:1", "now")));
}

// ---- coherence: overwrite and delete ----------------------------------------------------------------------------

namespace
{
	struct CoherenceRig : CmdRig
	{
		std::unique_ptr<SnapshotCommands> snapCommands;
		std::unique_ptr<HistoryValueSource> source;
		float out[12] = {};

		void Init3()
		{
			Init2();
			f.mgr->SetSnapshotCache(f.cache.get());
			f.mgr->SetViewState(view.get());
			f.cache->SetViewState(view.get());
			snapCommands = std::make_unique<SnapshotCommands>(*f.r.rec, *f.mgr, f.store);
			snapCommands->SetSnapshotCache(f.cache.get());
			ASSERT_TRUE(snapCommands->Register(registrar));
			source = std::make_unique<HistoryValueSource>(f.r.registry, *f.r.rec, *view);
			source->SetSnapshotCache(f.cache.get());
		}
		Json::Value Delete(const char* name)
		{
			Json::Value a(Json::objectValue); a["name"] = name;
			return registrar.Execute("cells.snapshot.delete", a);
		}
	};
}

TEST(DeltaSnapshotCoherence, OverwriteInvalidatesAndBumpsTheEpochWhenTheDeltaUsesIt)
{
	CoherenceRig r; r.Init3();
	ASSERT_FALSE(IsError(r.Enter("temp", "snapshot:pre", "now")));
	ASSERT_TRUE(r.source->GetValues(r.Temp(), r.out, 12));
	EXPECT_NEAR(r.out[0], 50.0f, kTol);
	EXPECT_TRUE(r.f.cache->Peek("pre"));
	const uint64_t epoch = r.view->Epoch();

	r.f.SetLiveTemp(30.0f);
	ASSERT_TRUE(r.f.Snap("pre", true));	// overwrite: pre now holds 30 + c
	EXPECT_EQ(r.view->Epoch(), epoch + 1);
	EXPECT_FALSE(r.f.cache->Peek("pre"));
	r.f.SetLiveTemp(70.0f);
	ASSERT_TRUE(r.source->GetValues(r.Temp(), r.out, 12));
	EXPECT_NEAR(r.out[0], 40.0f, kTol);	// 70 - 30, not the stale 70 - 20
}

TEST(DeltaSnapshotCoherence, OverwriteOfAnUnrelatedSnapshotLeavesTheEpoch)
{
	CoherenceRig r; r.Init3();
	ASSERT_FALSE(IsError(r.Enter("temp", "snapshot:post", "now")));
	ASSERT_TRUE(r.source->GetValues(r.Temp(), r.out, 12));
	const uint64_t epoch = r.view->Epoch();
	ASSERT_TRUE(r.f.Snap("pre", true));
	EXPECT_EQ(r.view->Epoch(), epoch);
	EXPECT_TRUE(r.f.cache->Peek("post"));
}

TEST(DeltaSnapshotCoherence, FailedTakeKeepsTheCacheEntryAndEpoch)
{
	CoherenceRig r; r.Init3();
	ASSERT_FALSE(IsError(r.Enter("temp", "snapshot:pre", "now")));
	ASSERT_TRUE(r.source->GetValues(r.Temp(), r.out, 12));
	const uint64_t epoch = r.view->Epoch();
	r.f.store.FailNext("disk full");
	TakeResult res;
	ASSERT_TRUE(r.f.mgr->Take("pre", true, res)) << res.error;
	r.f.store.RunFrameBoundary();
	EXPECT_EQ(r.view->Epoch(), epoch);
	EXPECT_TRUE(r.f.cache->Peek("pre"));
}

TEST(DeltaSnapshotCoherence, DeleteInvalidatesAndTheNextReadSeesNoData)
{
	CoherenceRig r; r.Init3();
	ASSERT_FALSE(IsError(r.Enter("temp", "snapshot:pre", "now")));
	ASSERT_TRUE(r.source->GetValues(r.Temp(), r.out, 12));
	const uint64_t epoch = r.view->Epoch();
	const Json::Value del = r.Delete("pre");
	ASSERT_FALSE(IsError(del)) << del.toStyledString();
	EXPECT_EQ(r.view->Epoch(), epoch + 1);
	EXPECT_FALSE(r.f.cache->Peek("pre"));
	EXPECT_FALSE(r.source->GetValues(r.Temp(), r.out, 12));	// never a stale read, never live values
	EXPECT_TRUE(r.view->InDelta());
}

TEST(DeltaSnapshotCoherence, DeleteOfAnUnrelatedSnapshotLeavesTheEpoch)
{
	CoherenceRig r; r.Init3();
	ASSERT_FALSE(IsError(r.Enter("temp", "snapshot:pre", "now")));
	ASSERT_TRUE(r.source->GetValues(r.Temp(), r.out, 12));
	const uint64_t epoch = r.view->Epoch();
	ASSERT_FALSE(IsError(r.Delete("post")));
	EXPECT_EQ(r.view->Epoch(), epoch);
	EXPECT_TRUE(r.f.cache->Peek("pre"));
}

TEST(DeltaSnapshotCoherence, DeleteOfAnUnknownSnapshotChangesNothing)
{
	CoherenceRig r; r.Init3();
	ASSERT_FALSE(IsError(r.Enter("temp", "snapshot:pre", "now")));
	const uint64_t epoch = r.view->Epoch();
	EXPECT_TRUE(IsError(r.Delete("ghost")));
	EXPECT_EQ(r.view->Epoch(), epoch);
}

#endif // DIA_DEBUG

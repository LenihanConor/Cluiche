// Closing gap-audit edge tests for DiaCellHistory Features 3 (snapshots), 4 (compare) and 5 (export). Only gaps not already
// covered by the per-feature test files live here.

#include "SnapshotSideRig.h"

#include <DiaCellHistory/DeltaValueSource.h>
#include <DiaCellHistory/HistoryExporter.h>
#include <DiaCellHistory/HistoryQueries.h>
#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCellHistory/SnapshotNaming.h>
#include <DiaCellHistory/Testing/ExportDecoder.h>
#include <DiaCellLayers/Testing/FakeMirrorRegistrar.h>

#include <json/json.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace Dia::CellHistory;
using namespace Dia::CellHistory::Testing;
using namespace HistoryRig;
using Dia::CellLayers::Testing::FakeMirrorRegistrar;
using Dia::Core::StringCRC;

namespace
{
	namespace fs = std::filesystem;

	Side SampleS(uint32_t i) { Side s; s.kind = SideKind::Sample; s.sample = i; return s; }
	Side NowS() { return Side(); }
	Side SnapS(const char* n) { Side s; s.kind = SideKind::Snapshot; s.snapshot = Dia::Core::Containers::String64("%s", n); return s; }
	bool IsError(const Json::Value& v) { return v.isObject() && v.isMember("error") && v["error"].isString() && !v["error"].asString().empty(); }

	// Strict UTF-8 check (no overlongs, surrogates or truncated sequences).
	bool ValidUtf8(const std::string& s)
	{
		size_t i = 0;
		while (i < s.size())
		{
			const unsigned char c = static_cast<unsigned char>(s[i]);
			size_t n = 0;
			if (c < 0x80) { n = 1; }
			else if (c >= 0xC2 && c <= 0xDF) { n = 2; }
			else if (c >= 0xE0 && c <= 0xEF) { n = 3; }
			else if (c >= 0xF0 && c <= 0xF4) { n = 4; }
			else { return false; }
			if (i + n > s.size()) { return false; }
			for (size_t k = 1; k < n; ++k) { if ((static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80) { return false; } }
			i += n;
		}
		return true;
	}
	std::string ReadAll(const std::string& path)
	{
		std::ifstream f(path, std::ios::binary);
		std::stringstream ss; ss << f.rdbuf();
		return ss.str();
	}
}

// ==== Feature 4: compare ====================================================================================================

TEST(HistoryClosingEdgesDelta, ExitDeltaAfterTheHeldSampleWasTruncatedGoesLiveWithoutAnUpdate)
{
	SnapRig::Fix f; f.Setup(6);
	HistoryViewState v(*f.r.rec);
	ViewResult res;
	ASSERT_TRUE(v.ViewSample(5, res));
	ASSERT_TRUE(v.EnterDelta(StringCRC("temp"), SampleS(1), NowS(), res));
	f.r.rec->TruncateAfter(2);			// held sample 5 no longer exists; delta side 1 does
	const uint64_t e = v.Epoch();
	EXPECT_TRUE(v.ExitDelta());
	EXPECT_TRUE(v.IsLive());
	EXPECT_FALSE(v.InDelta());
	EXPECT_EQ(v.Epoch(), e + 1);
}

TEST(HistoryClosingEdgesDelta, ASampleSideEvictedMidViewGivesNoDataKeepsDeltaModeAndTheEpoch)
{
	Rig r;
	r.grid.AddFloatLayer("temp", 40.0f);
	ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100, "{\"track\":true}") }));
	RecorderBudgets b;
	b.mAggregateBytes = 1 << 20;
	b.mCellBytes = BudgetBytesForWindow(3, BytesPerSample(1, 0, 12));
	b.mEventBytes = 4096;
	r.Start(&b, false);
	for (int i = 0; i < 3; ++i) { r.grid.SetAll("temp", float(i)); r.grid.BumpVersion("temp"); r.Take(); }
	HistoryViewState v(*r.rec); DeltaValueSource d(*r.rec, v);
	ViewResult res;
	ASSERT_TRUE(v.EnterDelta(StringCRC("temp"), SampleS(0), SampleS(2), res));
	const Dia::CellLayers::CellLayerDesc* desc = r.registry.Find(StringCRC("temp"));
	ASSERT_NE(desc, nullptr);
	float out[12];
	ASSERT_TRUE(d.GetValues(*desc, out, 12));
	for (int i = 3; i < 9; ++i) { r.grid.SetAll("temp", float(i)); r.grid.BumpVersion("temp"); r.Take(); }
	ASSERT_GT(r.rec->FirstRetained(), 0u);
	const uint64_t e = v.Epoch();
	EXPECT_FALSE(v.Update());				// eviction of a side is not a state change
	EXPECT_TRUE(v.InDelta());
	EXPECT_FALSE(d.GetValues(*desc, out, 12));	// no data, no crash, no live fallback
	EXPECT_EQ(v.Epoch(), e);
}

TEST(HistoryClosingEdgesDelta, ALayerTrackedMidRunHasDataOnlyFromItsFirstRecordedSample)
{
	Rig r;
	r.grid.AddFloatLayer("temp", 40.0f);
	ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100) }));
	r.Start(nullptr, false);
	for (int i = 0; i < 3; ++i) { r.grid.SetAll("temp", float(i)); r.grid.BumpVersion("temp"); r.Take(); }
	const StringCRC temp("temp");
	CellHistoryRecorder::TrackResult tr;
	ASSERT_TRUE(r.rec->Track(&temp, 1, tr)) << tr.mError;
	for (int i = 3; i < 6; ++i) { r.grid.SetAll("temp", float(i * 10)); r.grid.BumpVersion("temp"); r.Take(); }
	HistoryViewState v(*r.rec); DeltaValueSource d(*r.rec, v);
	const Dia::CellLayers::CellLayerDesc* desc = r.registry.Find(temp);
	ASSERT_NE(desc, nullptr);
	ViewResult res;
	float out[12];
	ASSERT_TRUE(v.EnterDelta(temp, SampleS(1), SampleS(4), res));
	EXPECT_FALSE(d.GetValues(*desc, out, 12));			// sample 1 predates tracking: no data
	ASSERT_TRUE(v.EnterDelta(temp, SampleS(1), NowS(), res));
	EXPECT_FALSE(d.GetValues(*desc, out, 12));			// and never filled from live values
	ASSERT_TRUE(v.EnterDelta(temp, SampleS(3), SampleS(5), res));
	ASSERT_TRUE(d.GetValues(*desc, out, 12));
	EXPECT_NEAR(out[0], 20.0f, 0.01f);
}

TEST(HistoryClosingEdgesDelta, ASnapshotFileDeletedBehindTheCacheIsServedFromTheCacheUntilInvalidated)
{
	SnapRig::Fix f; f.Setup(3);
	ASSERT_TRUE(f.Snap("pre"));
	f.SetLiveTemp(70.0f);
	HistoryViewState v(*f.r.rec); DeltaValueSource d(*f.r.rec, v);
	d.SetSnapshotCache(f.cache.get());
	f.cache->SetViewState(&v);
	ViewResult res;
	ASSERT_TRUE(v.EnterDelta(StringCRC("temp"), SnapS("pre"), NowS(), res));
	float out[12];
	ASSERT_TRUE(d.GetValues(f.Desc("temp"), out, 12));
	std::error_code ec;
	fs::remove(f.Path("pre"), ec);
	ASSERT_FALSE(fs::exists(f.Path("pre")));
	EXPECT_TRUE(d.GetValues(f.Desc("temp"), out, 12));	// defined: the loaded entry keeps serving
	const uint64_t e = v.Epoch();
	f.cache->Invalidate("pre");							// what delete does
	EXPECT_EQ(v.Epoch(), e + 1);
	EXPECT_FALSE(d.GetValues(f.Desc("temp"), out, 12));	// reload fails: no data, no crash
	EXPECT_TRUE(v.InDelta());
}

TEST(HistoryClosingEdgesDelta, ASnapshotFileGoneBeforeTheFirstReadIsNoData)
{
	SnapRig::Fix f; f.Setup(3);
	ASSERT_TRUE(f.Snap("pre"));
	HistoryViewState v(*f.r.rec); DeltaValueSource d(*f.r.rec, v);
	d.SetSnapshotCache(f.cache.get());
	ViewResult res;
	ASSERT_TRUE(v.EnterDelta(StringCRC("temp"), SnapS("pre"), NowS(), res));
	std::error_code ec;
	fs::remove(f.Path("pre"), ec);
	float out[12];
	EXPECT_FALSE(d.GetValues(f.Desc("temp"), out, 12));
}

// ==== Feature 3: snapshots ==================================================================================================

TEST(HistoryClosingEdgesSnapshot, RestoreWithTheCellFileCorruptedAfterTheCheckCompletesFromTheCachedHeader)
{
	// Documented behaviour: the header is read once when the restore is requested; the callback never re-reads the file, so a
	// file corrupted in between cannot leave a half-applied restore. The restore completes and truncates by the cached header.
	SnapRig::Fix f; f.Setup(3);
	ASSERT_TRUE(f.Snap("a"));			// taken at sample 2
	f.Sample(3); f.Sample(4);
	RestoreResult rr;
	ASSERT_TRUE(f.mgr->Restore("a", rr)) << rr.error;
	ASSERT_TRUE(rr.pending);
	std::ofstream(f.Path("a"), std::ios::binary | std::ios::trunc) << "{ not json";
	f.store.RunFrameBoundary();
	EXPECT_TRUE(f.mgr->LastRestore().mDone);
	EXPECT_TRUE(f.mgr->LastRestore().mOk);
	EXPECT_EQ(f.mgr->LastRestore().mDropped, 2u);
	EXPECT_EQ(f.r.rec->SampleCount(), 3u);			// all-or-nothing: either untouched or truncated exactly at sample 2
	EXPECT_FALSE(f.mgr->OperationInFlight());
	// And the next restore of the same (now corrupt) snapshot is refused up front.
	RestoreResult again;
	EXPECT_FALSE(f.mgr->Restore("a", again));
	EXPECT_EQ(f.r.rec->SampleCount(), 3u);
}

TEST(HistoryClosingEdgesSnapshot, TakeWithNoRecordableLayersIsRefusedAndWritesNothing)
{
	Rig r;
	r.grid.AddFloatLayer("temp", 1.0f);
	ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 1, "{\"aggregate\":false}") }));
	r.Start(nullptr, false);
	r.Take();
	FakeSnapshotStore store;
	SnapshotManager mgr(*r.rec, r.registry, r.schema.service.get(), store);
	TakeResult tr;
	EXPECT_FALSE(mgr.Take("x", false, tr));
	EXPECT_FALSE(tr.pending);
	EXPECT_NE(std::string(tr.error).size(), 0u);
	EXPECT_EQ(store.PendingCount(), 0u);
	EXPECT_FALSE(mgr.OperationInFlight());
	EXPECT_EQ(CountSnapshots(store), 0u);
}

TEST(HistoryClosingEdgesSnapshot, AThirtyTwoCharacterNameWithDashAndUnderscoreTakesAndRestores)
{
	SnapRig::Fix f; f.Setup(3);
	const std::string name = "a-b_c-d_e-f_g-h_i-j_k-l_m-n_o-p_";	// 32 characters
	ASSERT_EQ(name.size(), 32u);
	ASSERT_TRUE(f.Snap(name.c_str()));
	EXPECT_TRUE(fs::exists(f.Path(name.c_str())));
	f.Sample(3);
	RestoreResult rr;
	ASSERT_TRUE(f.mgr->Restore(name.c_str(), rr)) << rr.error;
	f.store.RunFrameBoundary();
	EXPECT_TRUE(f.mgr->LastRestore().mOk);
	EXPECT_EQ(f.r.rec->SampleCount(), 3u);
	TakeResult tr;
	EXPECT_FALSE(f.mgr->Take((name + "x").c_str(), false, tr));	// 33 characters
}

TEST(HistoryClosingEdgesSnapshot, TwoManagersOnTheSameFolderCountEachOthersCellFilesForTheCap)
{
	SnapRig::Fix f; f.Setup(3);
	SnapshotManager other(*f.r.rec, f.r.registry, f.r.schema.service.get(), f.store);
	for (uint32_t i = 0; i < 16; ++i)
	{
		const std::string n = "s" + std::to_string(i);
		TakeResult tr;
		SnapshotManager& m = (i % 2 == 0) ? *f.mgr : other;
		ASSERT_TRUE(m.Take(n.c_str(), false, tr)) << tr.error;
		f.store.RunFrameBoundary();
	}
	TakeResult tr;
	EXPECT_FALSE(f.mgr->Take("s16", false, tr));
	EXPECT_FALSE(other.Take("s17", false, tr));
	// Each manager lists only its own session's records.
	SnapshotSummary a[16], b[16];
	EXPECT_EQ(f.mgr->ListSummaries(a, 16), 8u);
	EXPECT_EQ(other.ListSummaries(b, 16), 8u);
}

// ==== Feature 5: export / queries ===========================================================================================

namespace
{
	const char* const kEventEntry = "{\"key\":\"cow.drought\",\"kind\":\"event\",\"type\":\"string\",\"label\":\"Drought\",\"description\":\"d\","
		"\"payload\":[{\"name\":\"weeks\",\"type\":\"int\",\"description\":\"d\"}]}";

	struct ExFix
	{
		Rig r;
		TempFolder tmp;
		FakeMirrorRegistrar registrar;
		std::unique_ptr<HistoryQueries> q;
		ExportOptions opt;
		ExportResult res;
		DecodedExport dec;

		void Setup()
		{
			r.grid.AddFloatLayer("temp", 40.0f);
			r.grid.AddFloatLayer("rain", 5.0f);
			r.grid.AddCategoryLayer("biome", 1);
			ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100, "{\"track\":true}"), FloatEntry("rain", 0, 10, "{\"aggregate\":false}"),
				EnumEntry("biome", 3), kEventEntry }));
			r.Start(nullptr, false);
			q = std::make_unique<HistoryQueries>(*r.rec);
			ASSERT_TRUE(q->Register(registrar));
		}
		void TakeLabelled(const char* label)
		{
			r.grid.SetAll("temp", 20.0f); r.grid.BumpVersion("temp");
			SampleInfo i; i.tick = 1; i.label = label; i.band = 0;
			r.rec->Sample(i);
		}
		bool Export(bool cells = false)
		{
			opt.cells = cells;
			opt.overwrite = true;
			const std::string p = tmp.File("e.cell_history.json");
			return HistoryExporter::WriteFile(*r.rec, opt, p.c_str(), res) && dec.Load(p.c_str());
		}
		std::string FileText() const { return ReadAll(tmp.File("e.cell_history.json")); }
		Json::Value Call(const char* n, const Json::Value& a = Json::Value(Json::objectValue)) { return registrar.Execute(n, a); }
	};
}

TEST(HistoryClosingEdgesExport, ALayerWithAggregateFalseIsAbsentFromTheFileAndTheFileStaysValid)
{
	ExFix f; f.Setup();
	for (int i = 0; i < 3; ++i) { f.TakeLabelled("w"); }
	ASSERT_TRUE(f.Export(true)) << f.res.error;
	EXPECT_FALSE(f.dec.root["aggregates"].isMember("rain"));
	EXPECT_TRUE(f.dec.root["aggregates"].isMember("temp"));
	EXPECT_FALSE(f.dec.root["cells"].isMember("rain"));
	const Json::Value r = f.Call("cells.history.cells", [] { Json::Value a(Json::objectValue); a["layer"] = "rain"; return a; }());
	EXPECT_TRUE(IsError(r));
}

TEST(HistoryClosingEdgesExport, ALabelWithQuotesBackslashesControlCharactersAndUnicodeIsValidJsonAndRoundTrips)
{
	ExFix f; f.Setup();
	const std::string label = "q\"b\\s\n\t\x01 caf\xC3\xA9 \xE2\x82\xAC";		// 17 bytes: well under the 31-char cap
	f.TakeLabelled(label.c_str());
	ASSERT_TRUE(f.Export()) << f.res.error << f.dec.error;
	EXPECT_EQ(f.dec.root["samples"][0]["label"].asString(), label);
	EXPECT_TRUE(ValidUtf8(f.FileText()));
}

// Regression (closing audit, fixed 2026-11-04 with Utf8SafeLength): the 31-byte sample label was cut byte-wise and could end in half a UTF-8 character.
// byte-wise, so a multi-byte character straddling the cap leaves a lone lead byte and the exported file is not valid UTF-8.
TEST(HistoryClosingEdgesExport, ALabelTruncatedMidUtf8SequenceStillExportsValidUtf8)
{
	ExFix f; f.Setup();
	const std::string label = std::string(30, 'x') + "\xC3\xA9";		// the 31-byte cap cuts the 2-byte character in half
	f.TakeLabelled(label.c_str());
	ASSERT_TRUE(f.Export()) << f.res.error << f.dec.error;
	EXPECT_TRUE(ValidUtf8(f.FileText())) << "label: " << f.dec.root["samples"][0]["label"].asString();
}

// Regression (closing audit, fixed 2026-11-04): the 64-byte event text was cut byte-wise, same effect.
TEST(HistoryClosingEdgesExport, EventTextTruncatedMidUtf8SequenceStillExportsValidUtf8)
{
	ExFix f; f.Setup();
	f.TakeLabelled("a");
	const std::string text = std::string(63, 'x') + "\xC3\xA9";		// the 64-byte cap cuts the 2-byte character in half
	HistoryEvent e;
	e.type = StringCRC("cow.drought");
	e.text = text.c_str();
	f.r.rec->Emit(e);
	f.TakeLabelled("b");
	ASSERT_TRUE(f.Export()) << f.res.error << f.dec.error;
	EXPECT_TRUE(ValidUtf8(f.FileText()));
}

TEST(HistoryClosingEdgesExport, APayloadWithEscapesAndUnicodeRoundTripsThroughTheFileAndTheQuery)
{
	ExFix f; f.Setup();
	f.TakeLabelled("a");
	const char* payload = "{\"weeks\":3,\"note\":\"caf\xC3\xA9 \\\"q\\\" \\\\ \\n \\u20ac\"}";
	HistoryEvent e;
	e.type = StringCRC("cow.drought");
	e.payloadJson = payload;
	f.r.rec->Emit(e);
	f.TakeLabelled("b");
	ASSERT_TRUE(f.Export()) << f.res.error << f.dec.error;
	Json::Value expected;
	ASSERT_TRUE(Json::Reader().parse(payload, expected, false));
	ASSERT_EQ(f.dec.root["events"].size(), 1u);
	EXPECT_EQ(f.dec.root["events"][0]["payload"], expected);
	const Json::Value q = f.Call("cells.history.events");
	ASSERT_FALSE(IsError(q)) << q.toStyledString();
	ASSERT_EQ(q["events"].size(), 1u);
	EXPECT_EQ(q["events"][0]["payload"], expected);
}

TEST(HistoryClosingEdgesExport, AOneKilobytePayloadIsRecordedExportedAndQueried)
{
	ExFix f; f.Setup();
	f.TakeLabelled("a");
	std::string payload = "{\"weeks\":1,\"pad\":\"";
	const std::string tail = "\"}";
	payload.append(1024 - payload.size() - tail.size(), 'p');
	payload += tail;
	ASSERT_EQ(payload.size(), 1024u);
	HistoryEvent e;
	e.type = StringCRC("cow.drought");
	e.payloadJson = payload.c_str();
	EXPECT_EQ(f.r.rec->Emit(e), EmitResult::Ok);
	f.TakeLabelled("b");
	ASSERT_TRUE(f.Export()) << f.res.error << f.dec.error;
	ASSERT_EQ(f.dec.root["events"].size(), 1u);
	EXPECT_EQ(f.dec.root["events"][0]["payload"]["pad"].asString().size(), 1024u - 18u - 2u);
	const Json::Value q = f.Call("cells.history.events");
	ASSERT_FALSE(IsError(q)) << q.toStyledString();
	EXPECT_EQ(q["events"].size(), 1u);
}

TEST(HistoryClosingEdgesExport, QueriesOnAnEmptyRecorderAnswerWithoutCrashing)
{
	ExFix f; f.Setup();
	for (const char* name : { "cells.history.meta", "cells.history.aggregates", "cells.history.events" })
	{
		const Json::Value r = f.Call(name);
		ASSERT_TRUE(r.isObject()) << name;
		if (!IsError(r) && r.isMember("rows")) { EXPECT_EQ(r["rows"].size(), 0u) << name; }
	}
	Json::Value a(Json::objectValue); a["layer"] = "temp";
	const Json::Value c = f.Call("cells.history.cells", a);
	EXPECT_TRUE(c.isObject());
	// The events query on an empty log has no events and no continuation.
	const Json::Value ev = f.Call("cells.history.events");
	if (!IsError(ev))
	{
		EXPECT_EQ(ev["events"].size(), 0u);
		EXPECT_TRUE(ev["next"].isNull());
	}
}

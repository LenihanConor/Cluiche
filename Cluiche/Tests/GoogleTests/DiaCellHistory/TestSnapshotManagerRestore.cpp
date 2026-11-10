#include "HistoryTestRig.h"
#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCellHistory/SnapshotManager.h>
#include <DiaCellHistory/Testing/FakeSnapshotStore.h>
#include <DiaCellLayers/Testing/LogCapture.h>

#include <cstring>
#include <filesystem>
#include <fstream>

using namespace Dia::CellHistory;
using namespace Dia::CellHistory::Testing;
using namespace HistoryRig;
using Dia::CellLayers::Testing::LogCapture;
using Dia::Core::StringCRC;

namespace
{
	namespace fs = std::filesystem;

	int64_t FakeNow(void*) { return 1792510000; }
	uint64_t gSeed = 777;
	uint64_t FakeSeed(void*) { return gSeed; }
	const StringCRC kTemp("temp");
	const StringCRC kWorld("world");

	struct Fix
	{
		Rig r;
		FakeSnapshotStore store;
		std::unique_ptr<SnapshotManager> mgr;
		std::unique_ptr<HistoryViewState> view;
		TakeResult takeRes;
		RestoreResult res;

		// windowSamples > 0 sizes the per-cell window of the tracked temp layer; 0 = big budgets.
		void Setup(uint32_t windowSamples = 0)
		{
			r.grid.AddFloatLayer("temp", 40.0f);
			r.grid.AddCategoryLayer("biome", 1);
			ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100, "{\"track\":true}"), EnumEntry("biome", 3) }));
			if (windowSamples > 0)
			{
				RecorderBudgets b;
				b.mAggregateBytes = 1 << 20;
				b.mCellBytes = BudgetBytesForWindow(windowSamples, BytesPerSample(1, 0, 12));
				b.mEventBytes = 1 << 20;
				r.Start(&b);
			}
			else { r.Start(); }
			mgr = std::make_unique<SnapshotManager>(*r.rec, r.registry, r.schema.service.get(), store);
			mgr->SetTimeProvider(&FakeNow, nullptr);
			mgr->SetResetSeedProvider(&FakeSeed, nullptr);
			view = std::make_unique<HistoryViewState>(*r.rec);
			mgr->SetViewState(view.get());
			store.SetWriteStateFiles(true);
		}
		// n samples, each preceded by one emitted event, with a distinct temp value.
		void Run(uint32_t n)
		{
			HistoryEvent e; e.type = StringCRC("test.simple");
			for (uint32_t i = 0; i < n; ++i)
			{
				r.grid.SetAll("temp", float(r.rec->SampleCount())); r.grid.BumpVersion("temp");
				r.rec->Emit(e);
				r.Take();
			}
		}
		std::string CellPath(const char* name)
		{
			char p[300];
			EXPECT_TRUE(SnapshotCellFile::Path(store.Folder(), name, p, sizeof(p)));
			return p;
		}
		void Snap(const char* name)
		{
			ASSERT_TRUE(mgr->Take(name, false, takeRes)) << takeRes.error;
			store.RunFrameBoundary();
			ASSERT_FALSE(mgr->OperationInFlight());
		}
		// Rewrites the cell file after `mutate`.
		template <typename F> void Edit(const char* name, F mutate)
		{
			SnapshotCellData d; char err[256];
			ASSERT_TRUE(SnapshotCellFile::Read(CellPath(name).c_str(), d, err, sizeof(err))) << err;
			mutate(d);
			ASSERT_TRUE(SnapshotCellFile::WriteAtomic(d, CellPath(name).c_str(), err, sizeof(err))) << err;
		}
		size_t RestoreCalls() const
		{
			size_t n = 0;
			for (const auto& c : store.Calls()) { if (c.kind == FakeSnapshotStore::CallKind::Restore) { ++n; } }
			return n;
		}
		size_t EventCount() { return r.rec->Events(0, 100000).Count(); }

		struct State { uint32_t samples; uint64_t runId; uint64_t epoch; ViewMode mode; size_t events; size_t restoreCalls; };
		State Capture() { return { r.rec->SampleCount(), r.rec->RunId(), view->Epoch(), view->Mode(), EventCount(), RestoreCalls() }; }
		void ExpectUnchanged(const State& s)
		{
			EXPECT_EQ(r.rec->SampleCount(), s.samples);
			EXPECT_EQ(r.rec->RunId(), s.runId);
			EXPECT_EQ(view->Epoch(), s.epoch);
			EXPECT_EQ(view->Mode(), s.mode);
			EXPECT_EQ(EventCount(), s.events);
			EXPECT_EQ(RestoreCalls(), s.restoreCalls);
			EXPECT_EQ(store.PendingCount(), 0u);
			EXPECT_FALSE(mgr->OperationInFlight());
		}
		// A refused restore: false, error text, nothing changed, store not called, gate free.
		void ExpectRefused(const char* name, const char* errorFragment)
		{
			const State before = Capture();
			EXPECT_FALSE(mgr->Restore(name, res));
			EXPECT_FALSE(res.ok);
			EXPECT_FALSE(res.pending);
			EXPECT_NE(std::string(res.error).find(errorFragment), std::string::npos) << res.error;
			ExpectUnchanged(before);
		}
	};
}

// ---- refusals -----------------------------------------------------------------------------------------------

TEST(SnapshotManagerRestore, RefusesAnInvalidName)
{
	Fix f; f.Setup(); f.Run(3);
	f.ExpectRefused("../evil", "name");
	f.ExpectRefused("", "name");
	f.ExpectRefused(nullptr, "name");
}

TEST(SnapshotManagerRestore, RefusesWhileATakeIsInFlight)
{
	Fix f; f.Setup(); f.Run(3);
	f.Snap("a");
	ASSERT_TRUE(f.mgr->Take("b", false, f.takeRes));
	EXPECT_FALSE(f.mgr->Restore("a", f.res));
	EXPECT_NE(std::string(f.res.error).find("snapshot operation in progress"), std::string::npos) << f.res.error;
	EXPECT_TRUE(f.mgr->OperationInFlight());		// the take still owns the gate
	EXPECT_EQ(f.RestoreCalls(), 0u);
	f.store.RunFrameBoundary();
	EXPECT_FALSE(f.mgr->OperationInFlight());
}

TEST(SnapshotManagerRestore, ASecondRestoreOrTakeWhileARestoreIsInFlightIsRefused)
{
	Fix f; f.Setup(); f.Run(3);
	f.Snap("a");
	ASSERT_TRUE(f.mgr->Restore("a", f.res)) << f.res.error;
	EXPECT_TRUE(f.res.ok);
	EXPECT_TRUE(f.res.pending);
	EXPECT_TRUE(f.mgr->OperationInFlight());
	EXPECT_FALSE(f.mgr->Restore("a", f.res));
	EXPECT_NE(std::string(f.res.error).find("in progress"), std::string::npos);
	EXPECT_FALSE(f.mgr->Take("c", false, f.takeRes));
	EXPECT_NE(std::string(f.takeRes.error).find("in progress"), std::string::npos);
	EXPECT_EQ(f.store.PendingCount(), 1u);
	f.store.RunFrameBoundary();
	EXPECT_FALSE(f.mgr->OperationInFlight());
	EXPECT_EQ(f.RestoreCalls(), 1u);
}

TEST(SnapshotManagerRestore, RefusesWhenTheStateIsMissingTheCellFileIsMissingOrBoth)
{
	Fix f; f.Setup(); f.Run(3);
	f.ExpectRefused("nothing", "not found");

	f.Snap("a");
	f.Snap("b");
	ASSERT_TRUE(f.store.Delete("a"));								// cell file left, state gone
	f.ExpectRefused("a", "state");
	std::error_code ec;
	fs::remove(f.CellPath("b"), ec);								// state left, cell file gone
	f.ExpectRefused("b", "cell file");
	f.store.AddExisting("c");										// state only, never had a cell file
	f.ExpectRefused("c", "cell file");
}

TEST(SnapshotManagerRestore, RefusesACorruptCellFile)
{
	Fix f; f.Setup(); f.Run(3);
	f.Snap("a");
	std::ofstream(f.CellPath("a"), std::ios::binary | std::ios::trunc) << "{ not json";
	f.ExpectRefused("a", "");
	EXPECT_NE(std::string(f.res.error).size(), 0u);
}

TEST(SnapshotManagerRestore, RefusesAGridMismatchBeforeTheStoreIsCalled)
{
	Fix f; f.Setup(); f.Run(3);
	f.Snap("w"); f.Snap("h"); f.Snap("i");
	f.Edit("w", [](SnapshotCellData& d) { d.mGrid.mWidth += 1; });
	f.Edit("h", [](SnapshotCellData& d) { d.mGrid.mHeight += 2; });
	f.Edit("i", [](SnapshotCellData& d) { std::snprintf(d.mGrid.mId, sizeof(d.mGrid.mId), "other.grid"); });
	f.ExpectRefused("w", "different grid (5 x 3)");
	EXPECT_NE(std::string(f.res.error).find("load that world first"), std::string::npos);
	f.ExpectRefused("h", "different grid (4 x 5)");
	f.ExpectRefused("i", "different grid (4 x 3)");
}

// ---- same run -----------------------------------------------------------------------------------------------

TEST(SnapshotManagerRestore, SameRunTruncatesEveryTierAndWarnsWithTheDroppedCount)
{
	LogCapture log("HIS");
	Fix f; f.Setup();
	f.Run(6);							// samples 0..5, one event each
	f.Snap("pre");
	f.Run(5);							// samples 6..10
	HistoryEvent e; e.type = StringCRC("test.simple");
	f.r.rec->Emit(e);					// emitted, not attached
	ASSERT_EQ(f.r.rec->SampleCount(), 11u);
	ASSERT_EQ(f.EventCount(), 11u);
	AggregateStats st;
	float v[12];
	ASSERT_TRUE(f.r.rec->Aggregate(10, kTemp, kWorld, st));
	ASSERT_TRUE(f.r.rec->CellValues(10, kTemp, v, 12));

	ASSERT_TRUE(f.mgr->Restore("pre", f.res)) << f.res.error;
	EXPECT_EQ(f.r.rec->SampleCount(), 11u);			// nothing happens before the store callback
	log.Clear();
	f.store.RunFrameBoundary();

	EXPECT_EQ(f.r.rec->SampleCount(), 6u);
	for (uint32_t s = 6; s <= 10; ++s)
	{
		EXPECT_FALSE(f.r.rec->Aggregate(s, kTemp, kWorld, st)) << s;
		EXPECT_FALSE(f.r.rec->CellValues(s, kTemp, v, 12)) << s;
		EXPECT_FALSE(f.r.rec->HasHeader(s)) << s;
	}
	EXPECT_TRUE(f.r.rec->Aggregate(5, kTemp, kWorld, st));
	EXPECT_NEAR(st.mMean, 5.0f, 0.001f);
	EXPECT_TRUE(f.r.rec->CellValues(5, kTemp, v, 12));
	EXPECT_TRUE(f.r.rec->HasHeader(5));
	EXPECT_EQ(f.r.rec->Ring()->LatestSample(), 5u);
	EXPECT_EQ(f.EventCount(), 6u);
	EXPECT_EQ(f.r.rec->Events(6, 100).Count(), 0u);
	EXPECT_EQ(f.r.rec->EventsLog().PendingCount(), 0u);
	EXPECT_EQ(log.Warnings("dropped 5 samples after sample 5"), 1u);
	EXPECT_EQ(log.Warnings("restored 'pre'"), 1u);
	EXPECT_FALSE(f.mgr->OperationInFlight());
	EXPECT_TRUE(f.mgr->LastRestore().mOk);
	EXPECT_FALSE(f.mgr->LastRestore().mReset);
	EXPECT_EQ(f.mgr->LastRestore().mDropped, 5u);
}

TEST(SnapshotManagerRestore, TheNextSampleContinuesFromTheTruncatedHistoryAndEventsDoNotLeak)
{
	Fix f; f.Setup();
	f.Run(4);
	f.Snap("pre");
	f.Run(3);
	HistoryEvent e; e.type = StringCRC("test.simple");
	f.r.rec->Emit(e);
	ASSERT_TRUE(f.mgr->Restore("pre", f.res));
	f.store.RunFrameBoundary();
	f.r.grid.SetAll("temp", 88.0f); f.r.grid.BumpVersion("temp");
	const SampleResult next = f.r.Take();
	EXPECT_EQ(next.mSample, 4u);
	AggregateStats st;
	ASSERT_TRUE(f.r.rec->Aggregate(4, kTemp, kWorld, st));
	EXPECT_NEAR(st.mMean, 88.0f, 0.001f);
	EXPECT_EQ(f.r.rec->Events(4, 4).Count(), 0u);		// the unattached event was dropped, not attached to sample 4
}

TEST(SnapshotManagerRestore, RestoreToTheLatestSampleDropsNothingAndLogsNoTruncationWarning)
{
	LogCapture log("HIS");
	Fix f; f.Setup();
	f.Run(4);
	f.Snap("now");
	HistoryEvent e; e.type = StringCRC("test.simple");
	f.r.rec->Emit(e);					// pending: still dropped
	ASSERT_TRUE(f.mgr->Restore("now", f.res));
	f.store.RunFrameBoundary();
	EXPECT_EQ(f.r.rec->SampleCount(), 4u);
	EXPECT_EQ(f.r.rec->EventsLog().PendingCount(), 0u);
	EXPECT_EQ(f.mgr->LastRestore().mDropped, 0u);
	EXPECT_FALSE(f.mgr->LastRestore().mReset);
	EXPECT_EQ(log.Warnings("dropped"), 0u);
	EXPECT_EQ(log.Warnings("history reset"), 0u);
	EXPECT_EQ(f.r.Take().mSample, 4u);
}

TEST(SnapshotManagerRestore, ASnapshotOlderThanTheCellWindowLeavesTheRingEmptyWithAGapAndKeepsAggregatesAndEvents)
{
	Fix f; f.Setup(3);					// per-cell window of 3 samples
	f.Run(3);
	f.Snap("old");						// sample 2
	f.Run(8);							// samples 3..10; the ring now holds 8..10
	ASSERT_EQ(f.r.rec->FirstRetained(), 8u);
	ASSERT_TRUE(f.mgr->Restore("old", f.res));
	f.store.RunFrameBoundary();

	EXPECT_EQ(f.r.rec->SampleCount(), 3u);
	EXPECT_TRUE(f.r.rec->Ring()->Empty());
	ASSERT_GE(f.r.rec->Ring()->GapCount(), 1u);
	EXPECT_EQ(f.r.rec->Ring()->Gap(f.r.rec->Ring()->GapCount() - 1).mLastDropped, 2u);
	AggregateStats st;
	for (uint32_t s = 0; s <= 2; ++s) { EXPECT_TRUE(f.r.rec->Aggregate(s, kTemp, kWorld, st)) << s; }
	EXPECT_FALSE(f.r.rec->Aggregate(3, kTemp, kWorld, st));
	EXPECT_EQ(f.EventCount(), 3u);
	float v[12];
	EXPECT_FALSE(f.r.rec->CellValues(2, kTemp, v, 12));
	EXPECT_EQ(f.r.Take().mSample, 3u);
	EXPECT_TRUE(f.r.rec->CellValues(3, kTemp, v, 12));
}

TEST(SnapshotManagerRestore, TakeThenRunThenRestoreRoundTripEndsAtTheSnapshotSample)
{
	Fix f; f.Setup();
	f.Run(6);
	f.Snap("rt");											// sample 5
	AggregateStats before;
	ASSERT_TRUE(f.r.rec->Aggregate(5, kTemp, kWorld, before));
	f.Run(5);
	ASSERT_EQ(f.r.rec->SampleCount(), 11u);
	ASSERT_TRUE(f.mgr->Restore("rt", f.res));
	f.store.RunFrameBoundary();
	EXPECT_EQ(f.r.rec->SampleCount(), 6u);
	AggregateStats after;
	ASSERT_TRUE(f.r.rec->Aggregate(5, kTemp, kWorld, after));
	EXPECT_FLOAT_EQ(after.mMean, before.mMean);
	EXPECT_FALSE(f.mgr->OperationInFlight());
	ASSERT_EQ(f.store.Calls().back().kind, FakeSnapshotStore::CallKind::Restore);
	EXPECT_STREQ(f.store.Calls().back().name.c_str(), "rt");
}

// ---- foreign run --------------------------------------------------------------------------------------------

TEST(SnapshotManagerRestore, ForeignRunClearsHistoryCreatesANewRunIdAndRestartsAtZero)
{
	LogCapture log("HIS");
	Fix f; f.Setup();
	f.Run(6);
	f.Snap("far");
	f.Edit("far", [](SnapshotCellData& d) { d.mHeader.mRunId ^= 0xABCDEFull; });
	f.Run(3);
	HistoryEvent e; e.type = StringCRC("test.simple");
	f.r.rec->Emit(e);
	const uint64_t oldId = f.r.rec->RunId();
	ASSERT_TRUE(f.mgr->Restore("far", f.res)) << f.res.error;
	log.Clear();
	f.store.RunFrameBoundary();

	EXPECT_NE(f.r.rec->RunId(), oldId);
	EXPECT_EQ(f.r.rec->SampleCount(), 0u);
	AggregateStats st;
	float v[12];
	EXPECT_FALSE(f.r.rec->Aggregate(0, kTemp, kWorld, st));
	EXPECT_FALSE(f.r.rec->CellValues(0, kTemp, v, 12));
	EXPECT_FALSE(f.r.rec->HasHeader(0));
	EXPECT_EQ(f.EventCount(), 0u);
	EXPECT_EQ(f.r.rec->EventsLog().PendingCount(), 0u);
	EXPECT_EQ(log.Warnings("restored 'far': history reset (snapshot from another run)"), 1u);
	EXPECT_TRUE(f.mgr->LastRestore().mReset);

	const uint64_t newId = f.r.rec->RunId();
	f.r.grid.SetAll("temp", 12.0f); f.r.grid.BumpVersion("temp");
	const SampleResult first = f.r.Take();
	EXPECT_EQ(first.mSample, 0u);
	EXPECT_EQ(f.r.rec->RunId(), newId);						// stable until the next reset
	ASSERT_TRUE(f.r.rec->Aggregate(0, kTemp, kWorld, st));
	EXPECT_NEAR(st.mMean, 12.0f, 0.001f);
	EXPECT_EQ(f.r.rec->Events(0, 0).Count(), 0u);
}

TEST(SnapshotManagerRestore, ASampleBeyondTheHistoryWithTheSameRunIdIsTreatedAsForeign)
{
	Fix f; f.Setup();
	f.Run(6);
	f.Snap("late");											// sample 5, this run
	f.r.rec->TruncateAfter(2);								// history now holds 0..2
	const uint64_t oldId = f.r.rec->RunId();
	ASSERT_TRUE(f.mgr->Restore("late", f.res));
	f.store.RunFrameBoundary();
	EXPECT_TRUE(f.mgr->LastRestore().mReset);
	EXPECT_EQ(f.r.rec->SampleCount(), 0u);
	EXPECT_NE(f.r.rec->RunId(), oldId);
}

TEST(SnapshotManagerRestore, TwoForeignRestoresNeverShareARunIdEvenWithTheSameSeedOrDefaultProvider)
{
	Fix f; f.Setup();
	f.Run(3);
	f.Snap("far");
	f.Edit("far", [](SnapshotCellData& d) { d.mHeader.mRunId ^= 1; });
	gSeed = 5;
	ASSERT_TRUE(f.mgr->Restore("far", f.res)); f.store.RunFrameBoundary();
	const uint64_t a = f.r.rec->RunId();
	ASSERT_TRUE(f.mgr->Restore("far", f.res)); f.store.RunFrameBoundary();
	const uint64_t b = f.r.rec->RunId();
	EXPECT_NE(a, b);

	f.mgr->SetResetSeedProvider(nullptr, nullptr);			// default: time mixed with a counter
	ASSERT_TRUE(f.mgr->Restore("far", f.res)); f.store.RunFrameBoundary();
	const uint64_t c = f.r.rec->RunId();
	ASSERT_TRUE(f.mgr->Restore("far", f.res)); f.store.RunFrameBoundary();
	const uint64_t d = f.r.rec->RunId();
	EXPECT_NE(c, d);
	EXPECT_NE(c, b);
	gSeed = 777;
}

// ---- failure, viewer, selection -----------------------------------------------------------------------------

TEST(SnapshotManagerRestore, AStoreFailureChangesNothingAndReportsTheMessage)
{
	LogCapture log("HIS");
	Fix f; f.Setup();
	f.Run(6);
	f.Snap("a");
	f.Run(3);
	ViewResult vr;
	ASSERT_TRUE(f.view->ViewSample(2, vr));
	const auto before = f.Capture();
	AggregateStats st;
	ASSERT_TRUE(f.r.rec->Aggregate(8, kTemp, kWorld, st));

	f.store.FailNext("disk on fire");
	ASSERT_TRUE(f.mgr->Restore("a", f.res));
	log.Clear();
	f.store.RunFrameBoundary();

	EXPECT_EQ(f.r.rec->SampleCount(), before.samples);
	EXPECT_EQ(f.r.rec->RunId(), before.runId);
	EXPECT_EQ(f.view->Epoch(), before.epoch);
	EXPECT_EQ(f.view->Mode(), ViewMode::Sample);
	EXPECT_EQ(f.view->Sample(), 2u);
	EXPECT_EQ(f.EventCount(), before.events);
	EXPECT_TRUE(f.r.rec->Aggregate(8, kTemp, kWorld, st));
	EXPECT_FALSE(f.mgr->OperationInFlight());
	EXPECT_FALSE(f.mgr->LastRestore().mOk);
	EXPECT_NE(std::string(f.mgr->LastRestore().mMessage).find("disk on fire"), std::string::npos);
	EXPECT_GE(log.Errors("disk on fire"), 1u);

	// the gate is free again: a retry works
	ASSERT_TRUE(f.mgr->Restore("a", f.res));
	f.store.RunFrameBoundary();
	EXPECT_EQ(f.r.rec->SampleCount(), 6u);
	EXPECT_TRUE(f.mgr->LastRestore().mOk);
}

TEST(SnapshotManagerRestore, TheViewerGoesLiveFromScrubAndDeltaAndTheEpochBumps)
{
	Fix f; f.Setup();
	f.Run(6);
	f.Snap("a");
	f.Run(3);
	ViewResult vr;
	ASSERT_TRUE(f.view->ViewSample(7, vr));
	uint64_t e = f.view->Epoch();
	ASSERT_TRUE(f.mgr->Restore("a", f.res));
	f.store.RunFrameBoundary();
	EXPECT_TRUE(f.view->IsLive());
	EXPECT_GT(f.view->Epoch(), e);

	Side a; a.kind = SideKind::Sample; a.sample = 1;
	Side b; b.kind = SideKind::Sample; b.sample = 3;
	ASSERT_TRUE(f.view->EnterDelta(kTemp, a, b, vr));
	f.Run(2);
	e = f.view->Epoch();
	ASSERT_TRUE(f.mgr->Restore("a", f.res));
	f.store.RunFrameBoundary();
	EXPECT_TRUE(f.view->IsLive());
	EXPECT_FALSE(f.view->InDelta());
	EXPECT_GT(f.view->Epoch(), e);
}

TEST(SnapshotManagerRestore, TheEpochBumpsEvenWhenTheViewerWasAlreadyLive)
{
	Fix f; f.Setup();
	f.Run(6);
	f.Snap("a");
	f.Run(2);
	const uint64_t e = f.view->Epoch();
	ASSERT_TRUE(f.mgr->Restore("a", f.res));
	f.store.RunFrameBoundary();
	EXPECT_TRUE(f.view->IsLive());
	EXPECT_EQ(f.view->Epoch(), e + 1);
	// foreign branch too
	f.Edit("a", [](SnapshotCellData& d) { d.mHeader.mRunId ^= 4; });
	ASSERT_TRUE(f.mgr->Restore("a", f.res));
	f.store.RunFrameBoundary();
	EXPECT_EQ(f.view->Epoch(), e + 2);
}

TEST(SnapshotManagerRestore, WorksWithoutAViewState)
{
	Fix f; f.Setup();
	f.mgr->SetViewState(nullptr);
	f.Run(4);
	f.Snap("a");
	f.Run(2);
	ASSERT_TRUE(f.mgr->Restore("a", f.res));
	f.store.RunFrameBoundary();
	EXPECT_EQ(f.r.rec->SampleCount(), 4u);
}

TEST(SnapshotManagerRestore, TheSelectionIsKeptOnBothBranches)
{
	Fix f; f.Setup();
	f.Run(4);
	f.Snap("a");
	Dia::CellLayers::CellSelection* sel = f.r.registry.SelectionFor(f.r.rec->ActiveGrid());
	ASSERT_NE(sel, nullptr);
	ASSERT_TRUE(sel->Set(3, true));
	ASSERT_TRUE(f.mgr->Restore("a", f.res));
	f.store.RunFrameBoundary();
	EXPECT_EQ(sel->Count(), 1u);
	f.Edit("a", [](SnapshotCellData& d) { d.mHeader.mRunId ^= 4; });
	ASSERT_TRUE(f.mgr->Restore("a", f.res));
	f.store.RunFrameBoundary();
	EXPECT_EQ(sel->Count(), 1u);
	EXPECT_TRUE(sel->Contains(3));
}

TEST(SnapshotManagerRestore, ARestoreDoesNotTouchTheSnapshotRecords)
{
	Fix f; f.Setup();
	f.Run(4);
	f.Snap("a");
	uint32_t used = 0;
	for (uint32_t i = 0; i < f.mgr->Count(); ++i) { if (f.mgr->Records()[i].mUsed) { ++used; } }
	ASSERT_TRUE(f.mgr->Restore("a", f.res));
	f.store.RunFrameBoundary();
	uint32_t usedAfter = 0;
	for (uint32_t i = 0; i < f.mgr->Count(); ++i)
	{
		const SnapshotRecord& rec = f.mgr->Records()[i];
		if (rec.mUsed) { ++usedAfter; EXPECT_EQ(rec.mStatus, SnapshotStatus::Ready); }
	}
	EXPECT_EQ(used, usedAfter);
}

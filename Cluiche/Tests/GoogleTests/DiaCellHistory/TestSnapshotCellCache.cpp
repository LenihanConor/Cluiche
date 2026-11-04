#include "SnapshotSideRig.h"

#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCellHistory/SnapshotCellFile.h>

#include <cstdio>
#include <fstream>

using namespace Dia::CellHistory;
using namespace SnapRig;
using Dia::Core::StringCRC;

TEST(SnapshotCellCache, LoadsOnFirstGet)
{
	Fix f; f.Setup();
	ASSERT_TRUE(f.Snap("a"));
	char err[128] = {};
	const SnapshotCellData* d = f.cache->Get("a", err, sizeof(err));
	ASSERT_NE(d, nullptr) << err;
	EXPECT_STREQ(d->mHeader.mName, "a");
	EXPECT_EQ(d->mHeader.mSample, 4u);
	EXPECT_EQ(f.cache->Misses(), 1u);
	EXPECT_EQ(f.cache->Loads(), 1u);
	EXPECT_EQ(f.cache->Hits(), 0u);
	EXPECT_TRUE(f.cache->Peek("a"));
	EXPECT_EQ(f.cache->Count(), 1u);
}

TEST(SnapshotCellCache, SecondGetIsAHitAndDoesNotReload)
{
	Fix f; f.Setup();
	ASSERT_TRUE(f.Snap("a"));
	char err[128] = {};
	const SnapshotCellData* d1 = f.cache->Get("a", err, sizeof(err));
	const SnapshotCellData* d2 = f.cache->Get("a", err, sizeof(err));
	ASSERT_NE(d1, nullptr);
	EXPECT_EQ(d1, d2);
	EXPECT_EQ(f.cache->Hits(), 1u);
	EXPECT_EQ(f.cache->Misses(), 1u);
	EXPECT_EQ(f.cache->Loads(), 1u);
}

TEST(SnapshotCellCache, ThirdSnapshotEvictsTheLeastRecentlyUsed)
{
	Fix f; f.Setup();
	ASSERT_TRUE(f.Snap("a")); ASSERT_TRUE(f.Snap("b")); ASSERT_TRUE(f.Snap("c"));
	char err[128] = {};
	ASSERT_NE(f.cache->Get("a", err, sizeof(err)), nullptr);
	ASSERT_NE(f.cache->Get("b", err, sizeof(err)), nullptr);
	ASSERT_NE(f.cache->Get("c", err, sizeof(err)), nullptr);
	EXPECT_FALSE(f.cache->Peek("a"));
	EXPECT_TRUE(f.cache->Peek("b"));
	EXPECT_TRUE(f.cache->Peek("c"));
	EXPECT_EQ(f.cache->Count(), 2u);
	EXPECT_EQ(SnapshotCellCache::kCapacity, 2u);
}

TEST(SnapshotCellCache, HitBumpsRecencySoTheOtherIsEvicted)
{
	Fix f; f.Setup();
	ASSERT_TRUE(f.Snap("a")); ASSERT_TRUE(f.Snap("b")); ASSERT_TRUE(f.Snap("c"));
	char err[128] = {};
	f.cache->Get("a", err, sizeof(err));
	f.cache->Get("b", err, sizeof(err));
	f.cache->Get("a", err, sizeof(err));	// a is now the most recent
	f.cache->Get("c", err, sizeof(err));	// evicts b
	EXPECT_TRUE(f.cache->Peek("a"));
	EXPECT_FALSE(f.cache->Peek("b"));
	EXPECT_TRUE(f.cache->Peek("c"));
}

TEST(SnapshotCellCache, PeekDoesNotBumpRecencyOrCount)
{
	Fix f; f.Setup();
	ASSERT_TRUE(f.Snap("a")); ASSERT_TRUE(f.Snap("b")); ASSERT_TRUE(f.Snap("c"));
	char err[128] = {};
	f.cache->Get("a", err, sizeof(err));
	f.cache->Get("b", err, sizeof(err));
	EXPECT_TRUE(f.cache->Peek("a"));
	f.cache->Get("c", err, sizeof(err));	// a is still the oldest
	EXPECT_FALSE(f.cache->Peek("a"));
	EXPECT_EQ(f.cache->Hits(), 0u);
}

TEST(SnapshotCellCache, MissingFileFailsWithReasonAndNoEntry)
{
	Fix f; f.Setup();
	char err[128] = {};
	EXPECT_EQ(f.cache->Get("ghost", err, sizeof(err)), nullptr);
	EXPECT_NE(err[0], '\0');
	EXPECT_FALSE(f.cache->Peek("ghost"));
	EXPECT_EQ(f.cache->LoadFailures(), 1u);
	EXPECT_EQ(f.cache->Loads(), 0u);
	EXPECT_EQ(f.cache->Count(), 0u);
	EXPECT_FALSE(f.cache->Exists("ghost"));
}

TEST(SnapshotCellCache, CorruptFileFailsWithReasonAndNoEntry)
{
	Fix f; f.Setup();
	ASSERT_TRUE(f.Snap("a"));
	{ std::ofstream o(f.Path("a"), std::ios::binary | std::ios::trunc); o << "{ not json"; }
	char err[128] = {};
	EXPECT_EQ(f.cache->Get("a", err, sizeof(err)), nullptr);
	EXPECT_NE(err[0], '\0');
	EXPECT_FALSE(f.cache->Peek("a"));
	EXPECT_EQ(f.cache->LoadFailures(), 1u);
	EXPECT_TRUE(f.cache->Exists("a"));
}

TEST(SnapshotCellCache, FailedLoadDoesNotEvictAGoodEntry)
{
	Fix f; f.Setup();
	ASSERT_TRUE(f.Snap("a")); ASSERT_TRUE(f.Snap("b"));
	char err[128] = {};
	f.cache->Get("a", err, sizeof(err));
	f.cache->Get("b", err, sizeof(err));
	EXPECT_EQ(f.cache->Get("ghost", err, sizeof(err)), nullptr);
	EXPECT_TRUE(f.cache->Peek("a"));
	EXPECT_TRUE(f.cache->Peek("b"));
}

TEST(SnapshotCellCache, InvalidNameIsRefused)
{
	Fix f; f.Setup();
	char err[128] = {};
	EXPECT_EQ(f.cache->Get("../x", err, sizeof(err)), nullptr);
	EXPECT_NE(err[0], '\0');
	EXPECT_EQ(f.cache->Get(nullptr, err, sizeof(err)), nullptr);
	EXPECT_EQ(f.cache->Get("", err, sizeof(err)), nullptr);
	EXPECT_FALSE(f.cache->Exists("../x"));
	EXPECT_EQ(f.cache->Loads(), 0u);
}

TEST(SnapshotCellCache, InvalidateDropsOnlyThatEntryAndReloadsFresh)
{
	Fix f; f.Setup();
	ASSERT_TRUE(f.Snap("a")); ASSERT_TRUE(f.Snap("b"));
	char err[128] = {};
	f.cache->Get("a", err, sizeof(err));
	f.cache->Get("b", err, sizeof(err));
	f.cache->Invalidate("a");
	EXPECT_FALSE(f.cache->Peek("a"));
	EXPECT_TRUE(f.cache->Peek("b"));
	EXPECT_EQ(f.cache->Count(), 1u);
	f.cache->Invalidate("not-there");	// harmless
	f.cache->Invalidate(nullptr);
	EXPECT_EQ(f.cache->Count(), 1u);
	ASSERT_NE(f.cache->Get("a", err, sizeof(err)), nullptr);
	EXPECT_EQ(f.cache->Loads(), 3u);
}

TEST(SnapshotCellCache, ClearDropsEverything)
{
	Fix f; f.Setup();
	ASSERT_TRUE(f.Snap("a")); ASSERT_TRUE(f.Snap("b"));
	char err[128] = {};
	f.cache->Get("a", err, sizeof(err));
	f.cache->Get("b", err, sizeof(err));
	f.cache->Clear();
	EXPECT_EQ(f.cache->Count(), 0u);
	EXPECT_FALSE(f.cache->Peek("a"));
	EXPECT_FALSE(f.cache->Peek("b"));
}

TEST(SnapshotCellCache, GridMatchesComparesIdWidthAndHeight)
{
	Fix f; f.Setup();
	ASSERT_TRUE(f.Snap("a"));
	char err[128] = {};
	const SnapshotCellData* d = f.cache->Get("a", err, sizeof(err));
	ASSERT_NE(d, nullptr);
	const Dia::Graphics::CellGridDesc* grid = f.Desc("temp").grid;
	ASSERT_NE(grid, nullptr);
	EXPECT_TRUE(SnapshotCellCache::GridMatches(*d, *grid));
	Dia::Graphics::CellGridDesc wider = *grid;
	wider.width = static_cast<uint16_t>(wider.width + 1);
	EXPECT_FALSE(SnapshotCellCache::GridMatches(*d, wider));
	Dia::Graphics::CellGridDesc taller = *grid;
	taller.height = static_cast<uint16_t>(taller.height + 1);
	EXPECT_FALSE(SnapshotCellCache::GridMatches(*d, taller));
	Dia::Graphics::CellGridDesc renamed = *grid;
	renamed.id = StringCRC("another.grid");
	EXPECT_FALSE(SnapshotCellCache::GridMatches(*d, renamed));
}

TEST(SnapshotCellCache, InvalidateBumpsViewEpochOnlyWhenDeltaUsesTheSnapshot)
{
	Fix f; f.Setup();
	ASSERT_TRUE(f.Snap("a")); ASSERT_TRUE(f.Snap("b"));
	HistoryViewState view(*f.r.rec);
	f.cache->SetViewState(&view);
	// not in delta: no bump
	uint64_t e = view.Epoch();
	f.cache->Invalidate("a");
	EXPECT_EQ(view.Epoch(), e);

	Side a; a.kind = SideKind::Snapshot; a.snapshot = Dia::Core::Containers::String64("%s", "a");
	Side s2; s2.kind = SideKind::Sample; s2.sample = 1;
	ViewResult vr;
	ASSERT_TRUE(view.EnterDelta(StringCRC("temp"), a, s2, vr));
	e = view.Epoch();
	f.cache->Invalidate("b");	// unrelated snapshot
	EXPECT_EQ(view.Epoch(), e);
	f.cache->Invalidate("a");	// used by the delta
	EXPECT_EQ(view.Epoch(), e + 1);
	f.cache->SetViewState(nullptr);
	f.cache->Invalidate("a");
	EXPECT_EQ(view.Epoch(), e + 1);
}

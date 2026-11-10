#include <gtest/gtest.h>
#include <Modules/TestStages/CellLayersSnapshotStore.h>
#include <DiaCellHistory/Testing/FakeSnapshotStore.h>

using namespace CluicheTest;

namespace
{
	struct Result { int calls = 0; bool ok = false; std::string message; };
	void OnDone(bool ok, const char* message, void* user)
	{
		Result* r = static_cast<Result*>(user);
		++r->calls;
		r->ok = ok;
		r->message = message != nullptr ? message : "";
	}

	struct Fixture
	{
		Dia::CellHistory::Testing::TempFolder temp;
		CellLayersStageState live;		// the "stage"
		CellLayersSnapshotStore store;
		Fixture() : store(temp.Path(), [this]() { return live; }, [this](const CellLayersStageState& s) { live = s; }) {}
	};
}

TEST(CellLayersSnapshotStoreTest, TakeWritesStateFileAtNextBoundaryOnly)
{
	Fixture f;
	f.live = { 7, 3, 90 };
	Result r;
	f.store.RequestTake("pre", &OnDone, &r);
	EXPECT_EQ(r.calls, 0);
	EXPECT_FALSE(f.store.Exists("pre"));
	f.store.RunFrameBoundary();
	EXPECT_EQ(r.calls, 1);
	EXPECT_TRUE(r.ok) << r.message;
	EXPECT_TRUE(f.store.Exists("pre"));
	EXPECT_TRUE(std::filesystem::exists(f.temp.File("pre.state")));
}

TEST(CellLayersSnapshotStoreTest, RestoreLoadsBackTheSameValues)
{
	Fixture f;
	f.live = { 1234567890123ull, 17, 450 };
	Result t;
	f.store.RequestTake("a", &OnDone, &t);
	f.store.RunFrameBoundary();
	f.live = { 99, 1, 2 };
	Result r;
	f.store.RequestRestore("a", &OnDone, &r);
	EXPECT_EQ(f.live.tempVersion, 99u);	// not before the boundary
	f.store.RunFrameBoundary();
	EXPECT_TRUE(r.ok) << r.message;
	EXPECT_EQ(f.live.tempVersion, 1234567890123ull);
	EXPECT_EQ(f.live.animateCounter, 17u);
	EXPECT_EQ(f.live.historyFrame, 450u);
}

TEST(CellLayersSnapshotStoreTest, RestoreMissingFileFailsWithMessageAndLeavesState)
{
	Fixture f;
	f.live = { 5, 5, 5 };
	Result r;
	f.store.RequestRestore("nope", &OnDone, &r);
	f.store.RunFrameBoundary();
	EXPECT_EQ(r.calls, 1);
	EXPECT_FALSE(r.ok);
	EXPECT_NE(r.message.find("missing"), std::string::npos) << r.message;
	EXPECT_EQ(f.live.tempVersion, 5u);
}

TEST(CellLayersSnapshotStoreTest, RestoreCorruptFileFails)
{
	Fixture f;
	{ std::ofstream(f.temp.File("bad.state"), std::ios::binary) << "garbage"; }
	f.live = { 5, 5, 5 };
	Result r;
	f.store.RequestRestore("bad", &OnDone, &r);
	f.store.RunFrameBoundary();
	EXPECT_FALSE(r.ok);
	EXPECT_NE(r.message.find("corrupt"), std::string::npos) << r.message;
	EXPECT_EQ(f.live.tempVersion, 5u);
}

TEST(CellLayersSnapshotStoreTest, SecondRequestWhilePendingIsRefusedAndFirstStillRuns)
{
	Fixture f;
	f.live = { 8, 0, 0 };
	Result a, b;
	f.store.RequestTake("one", &OnDone, &a);
	f.store.RequestTake("two", &OnDone, &b);
	EXPECT_TRUE(f.store.HasPending());
	f.store.RunFrameBoundary();
	EXPECT_TRUE(a.ok) << a.message;
	EXPECT_EQ(b.calls, 1);
	EXPECT_FALSE(b.ok);
	EXPECT_NE(b.message.find("busy"), std::string::npos) << b.message;
	EXPECT_TRUE(f.store.Exists("one"));
	EXPECT_FALSE(f.store.Exists("two"));
	EXPECT_FALSE(f.store.HasPending());
}

TEST(CellLayersSnapshotStoreTest, DeleteRemovesFileAndReportsMissing)
{
	Fixture f;
	Result r;
	f.store.RequestTake("d", &OnDone, &r);
	f.store.RunFrameBoundary();
	EXPECT_TRUE(f.store.Delete("d"));
	EXPECT_FALSE(f.store.Exists("d"));
	EXPECT_FALSE(f.store.Delete("d"));
}

TEST(CellLayersSnapshotStoreTest, AtomicWriteLeavesNoTmpAndOverwriteReplaces)
{
	Fixture f;
	f.live = { 1, 0, 0 };
	Result r;
	f.store.RequestTake("o", &OnDone, &r);
	f.store.RunFrameBoundary();
	f.live = { 2, 0, 0 };
	f.store.RequestTake("o", &OnDone, &r);
	f.store.RunFrameBoundary();
	for (const auto& e : std::filesystem::directory_iterator(f.temp.Path()))
		EXPECT_EQ(e.path().extension().string(), ".state") << e.path();
	f.live = { 3, 0, 0 };
	f.store.RequestRestore("o", &OnDone, &r);
	f.store.RunFrameBoundary();
	EXPECT_EQ(f.live.tempVersion, 2u);
}

TEST(CellLayersSnapshotStoreTest, InvalidNamesAreRefusedAndNeverTouchTheFilesystem)
{
	Fixture f;
	Result r;
	f.store.RequestTake("../evil", &OnDone, &r);
	f.store.RunFrameBoundary();
	EXPECT_FALSE(r.ok);
	EXPECT_FALSE(f.store.Exists("../evil"));
	EXPECT_FALSE(f.store.Delete(""));
	EXPECT_FALSE(f.store.Exists(nullptr));
}

TEST(CellLayersSnapshotStoreTest, TakeCreatesMissingFolderOnDemand)
{
	Dia::CellHistory::Testing::TempFolder temp;
	const std::string sub = temp.Path() + "/deep/er";
	CellLayersStageState live{ 4, 0, 0 };
	CellLayersSnapshotStore store(sub, [&]() { return live; }, [&](const CellLayersStageState& s) { live = s; });
	Result r;
	store.RequestTake("x", &OnDone, &r);
	store.RunFrameBoundary();
	EXPECT_TRUE(r.ok) << r.message;
	EXPECT_TRUE(store.Exists("x"));
}

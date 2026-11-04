#include <gtest/gtest.h>
#include <DiaCellHistory/Testing/FakeSnapshotStore.h>
#include <filesystem>

using namespace Dia::CellHistory;
using namespace Dia::CellHistory::Testing;

namespace
{
	struct Seen { int calls = 0; bool ok = false; std::string msg; };
	void Cb(bool ok, const char* message, void* user)
	{
		Seen* s = static_cast<Seen*>(user);
		++s->calls; s->ok = ok; s->msg = message ? message : "";
	}
}

TEST(FakeSnapshotStore, RequestsAreQueuedUntilFrameBoundary)
{
	FakeSnapshotStore store;
	Seen seen;
	store.RequestTake("a", &Cb, &seen);
	EXPECT_EQ(seen.calls, 0);
	EXPECT_EQ(store.PendingCount(), 1u);
	EXPECT_FALSE(store.Exists("a"));
	store.RunFrameBoundary();
	EXPECT_EQ(seen.calls, 1);
	EXPECT_TRUE(seen.ok);
	EXPECT_TRUE(store.Exists("a"));
	EXPECT_EQ(store.PendingCount(), 0u);
}

TEST(FakeSnapshotStore, RecordsCallsInOrder)
{
	FakeSnapshotStore store;
	Seen s;
	store.RequestTake("a", &Cb, &s);
	store.RequestRestore("a", &Cb, &s);
	store.RunFrameBoundary();
	ASSERT_EQ(store.Calls().size(), 2u);
	EXPECT_EQ(store.Calls()[0].kind, FakeSnapshotStore::CallKind::Take);
	EXPECT_EQ(store.Calls()[0].name, "a");
	EXPECT_EQ(store.Calls()[1].kind, FakeSnapshotStore::CallKind::Restore);
	EXPECT_EQ(s.calls, 2);
}

TEST(FakeSnapshotStore, FailNextScriptsOneFailureWithMessage)
{
	FakeSnapshotStore store;
	Seen s;
	store.FailNext("disk full");
	store.RequestTake("a", &Cb, &s);
	store.RunFrameBoundary();
	EXPECT_FALSE(s.ok);
	EXPECT_EQ(s.msg, "disk full");
	EXPECT_FALSE(store.Exists("a"));
	store.RequestTake("a", &Cb, &s);
	store.RunFrameBoundary();
	EXPECT_TRUE(s.ok);
	EXPECT_TRUE(store.Exists("a"));
}

TEST(FakeSnapshotStore, WritesRealStateFileAndDeleteRemovesIt)
{
	FakeSnapshotStore store;
	store.SetWriteStateFiles(true);
	Seen s;
	store.RequestTake("a", &Cb, &s);
	store.RunFrameBoundary();
	const std::string file = store.StatePath("a");
	EXPECT_TRUE(std::filesystem::exists(file));
	EXPECT_TRUE(store.Delete("a"));
	EXPECT_FALSE(std::filesystem::exists(file));
	EXPECT_FALSE(store.Exists("a"));
	EXPECT_FALSE(store.Delete("a"));
}

TEST(FakeSnapshotStore, PartialStateLeftOnFailureWhenRequested)
{
	FakeSnapshotStore store;
	store.SetWriteStateFiles(true);
	store.SetLeavePartialOnFail(true);
	Seen s;
	store.FailNext("boom");
	store.RequestTake("a", &Cb, &s);
	store.RunFrameBoundary();
	EXPECT_FALSE(s.ok);
	EXPECT_TRUE(std::filesystem::exists(store.StatePath("a")));
}

TEST(FakeSnapshotStore, FolderExistsAndIsRemovedOnDestruction)
{
	std::string folder;
	{
		FakeSnapshotStore store;
		folder = store.Folder();
		EXPECT_TRUE(std::filesystem::is_directory(folder));
	}
	EXPECT_FALSE(std::filesystem::exists(folder));
}

TEST(FakeSnapshotStore, ClearResetsEverything)
{
	FakeSnapshotStore store;
	store.SetWriteStateFiles(true);
	Seen s;
	store.RequestTake("a", &Cb, &s);
	store.RunFrameBoundary();
	store.FailNext("x");
	store.RequestTake("b", &Cb, &s);
	store.Clear();
	EXPECT_FALSE(store.Exists("a"));
	EXPECT_TRUE(store.Calls().empty());
	EXPECT_EQ(store.PendingCount(), 0u);
	EXPECT_FALSE(std::filesystem::exists(store.StatePath("a")));
	store.RequestTake("c", &Cb, &s);
	store.RunFrameBoundary();
	EXPECT_TRUE(s.ok);	// scripted failure was cleared
}

TEST(FakeSnapshotStore, SeededNamesExist)
{
	FakeSnapshotStore store;
	store.AddExisting("old");
	EXPECT_TRUE(store.Exists("old"));
}

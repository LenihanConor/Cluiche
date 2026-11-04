#include <gtest/gtest.h>
#include <DiaCellHistory/SnapshotNaming.h>
#include <DiaCellHistory/SnapshotCellFile.h>
#include <DiaCellHistory/Testing/FakeSnapshotStore.h>
#include <cstring>
#include <fstream>
#include <string>

using namespace Dia::CellHistory;
using namespace Dia::CellHistory::Testing;

namespace
{
	void Touch(const std::string& path) { std::ofstream(path) << "{}"; }
	void MakeCellFile(const FakeSnapshotStore& store, const char* name)
	{
		char path[260];
		ASSERT_TRUE(SnapshotCellFile::Path(store.Folder(), name, path, sizeof(path)));
		Touch(path);
	}
}

TEST(SnapshotNaming, AcceptsAllowedCharacters)
{
	EXPECT_TRUE(IsValidSnapshotName("a"));
	EXPECT_TRUE(IsValidSnapshotName("pre-drought_01"));
	EXPECT_TRUE(IsValidSnapshotName("ABCxyz019_-"));
	EXPECT_TRUE(IsValidSnapshotName(std::string(32, 'x').c_str()));
}

TEST(SnapshotNaming, RejectsBadNames)
{
	EXPECT_FALSE(IsValidSnapshotName(nullptr));
	EXPECT_FALSE(IsValidSnapshotName(""));
	EXPECT_FALSE(IsValidSnapshotName("a b"));
	EXPECT_FALSE(IsValidSnapshotName("a.b"));
	EXPECT_FALSE(IsValidSnapshotName("a/b"));
	EXPECT_FALSE(IsValidSnapshotName("a\\b"));
	EXPECT_FALSE(IsValidSnapshotName("a:b"));
	EXPECT_FALSE(IsValidSnapshotName(".."));
	EXPECT_FALSE(IsValidSnapshotName("caf\xC3\xA9"));
	EXPECT_FALSE(IsValidSnapshotName(std::string(33, 'x').c_str()));
}

TEST(SnapshotNaming, CellPathRule)
{
	char path[260];
	ASSERT_TRUE(SnapshotCellFile::Path("out/saves", "pre-drought", path, sizeof(path)));
	EXPECT_STREQ(path, "out/saves/snapshot_pre-drought.cells.json");
	char tiny[8];
	EXPECT_FALSE(SnapshotCellFile::Path("out/saves", "x", tiny, sizeof(tiny)));
	EXPECT_FALSE(SnapshotCellFile::Path("out/saves", "bad name", path, sizeof(path)));
}

TEST(SnapshotNaming, CountsCellFilesInFolderIncludingEarlierSessions)
{
	FakeSnapshotStore store;
	EXPECT_EQ(CountSnapshots(store), 0u);
	MakeCellFile(store, "one");
	MakeCellFile(store, "two");
	Touch(std::string(store.Folder()) + "/unrelated.txt");
	Touch(std::string(store.Folder()) + "/three.state");
	EXPECT_EQ(CountSnapshots(store), 2u);
}

TEST(SnapshotGate, SecondBeginFailsUntilEnd)
{
	SnapshotGate gate;
	char err[96];
	EXPECT_FALSE(gate.InFlight());
	EXPECT_TRUE(gate.TryBegin(err, sizeof(err)));
	EXPECT_TRUE(gate.InFlight());
	EXPECT_FALSE(gate.TryBegin(err, sizeof(err)));
	EXPECT_STREQ(err, "snapshot operation in progress");
	gate.End();
	EXPECT_FALSE(gate.InFlight());
	EXPECT_TRUE(gate.TryBegin(err, sizeof(err)));
}

TEST(SnapshotCheckTake, InvalidNameMessage)
{
	FakeSnapshotStore store;
	SnapshotGate gate;
	char err[160];
	EXPECT_FALSE(CheckTake("bad name", false, store, gate, err, sizeof(err)));
	EXPECT_NE(std::strstr(err, "invalid snapshot name"), nullptr);
	EXPECT_FALSE(gate.InFlight());
}

TEST(SnapshotCheckTake, OrderingNameBeforeInFlightBeforeExistsBeforeCap)
{
	FakeSnapshotStore store;
	SnapshotGate gate;
	char err[160];
	store.AddExisting("dup");
	for (int i = 0; i < 16; ++i) { MakeCellFile(store, ("s" + std::to_string(i)).c_str()); }
	ASSERT_TRUE(gate.TryBegin(err, sizeof(err)));
	// Everything wrong at once: the name is reported first.
	EXPECT_FALSE(CheckTake("bad name", false, store, gate, err, sizeof(err)));
	EXPECT_NE(std::strstr(err, "invalid snapshot name"), nullptr);
	// Valid name, in flight, also exists and at cap: in flight first.
	EXPECT_FALSE(CheckTake("dup", false, store, gate, err, sizeof(err)));
	EXPECT_STREQ(err, "snapshot operation in progress");
	gate.End();
	// Exists and cap: exists first.
	EXPECT_FALSE(CheckTake("dup", false, store, gate, err, sizeof(err)));
	EXPECT_STREQ(err, "snapshot 'dup' already exists (use overwrite)");
	// New name at cap.
	EXPECT_FALSE(CheckTake("fresh", false, store, gate, err, sizeof(err)));
	EXPECT_STREQ(err, "snapshot limit of 16 reached; delete a snapshot first");
}

TEST(SnapshotCheckTake, ExistingCellFileCountsAsExisting)
{
	FakeSnapshotStore store;
	SnapshotGate gate;
	char err[160];
	MakeCellFile(store, "half");
	EXPECT_FALSE(CheckTake("half", false, store, gate, err, sizeof(err)));
	EXPECT_NE(std::strstr(err, "already exists"), nullptr);
	EXPECT_TRUE(CheckTake("half", true, store, gate, err, sizeof(err)));
}

TEST(SnapshotCheckTake, OverwriteAtCapSucceedsAndSeventeenthRefused)
{
	FakeSnapshotStore store;
	SnapshotGate gate;
	char err[160];
	for (int i = 0; i < 15; ++i) { MakeCellFile(store, ("s" + std::to_string(i)).c_str()); }
	EXPECT_TRUE(CheckTake("s15", false, store, gate, err, sizeof(err)));	// 16th allowed
	MakeCellFile(store, "s15");
	EXPECT_EQ(CountSnapshots(store), 16u);
	EXPECT_TRUE(CheckTake("s3", true, store, gate, err, sizeof(err)));		// overwrite at the cap
	EXPECT_FALSE(CheckTake("s16", false, store, gate, err, sizeof(err)));	// 17th
	EXPECT_STREQ(err, "snapshot limit of 16 reached; delete a snapshot first");
	EXPECT_FALSE(CheckTake("s16", true, store, gate, err, sizeof(err)));	// overwrite of a new name still counts
	EXPECT_EQ(CountSnapshots(store), 16u);
	EXPECT_FALSE(gate.InFlight());
	EXPECT_TRUE(store.Calls().empty());
}

#include "HistoryTestRig.h"
#include <DiaCellHistory/SnapshotManager.h>
#include <DiaCellHistory/Testing/FakeSnapshotStore.h>

#include <cstring>
#include <filesystem>

using namespace Dia::CellHistory;
using namespace Dia::CellHistory::Testing;
using namespace HistoryRig;
using Dia::Core::StringCRC;

namespace
{
	namespace fs = std::filesystem;

	int64_t gNow = 1792510000;
	int64_t FakeNow(void*) { return gNow; }

	struct Fix
	{
		Rig r;
		FakeSnapshotStore store;
		std::unique_ptr<SnapshotManager> mgr;
		TakeResult res;

		void Setup(bool takeSamples = true)
		{
			r.grid.AddFloatLayer("temp", 40.0f);
			r.grid.AddCategoryLayer("biome", 1);
			ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100), EnumEntry("biome", 3) }));
			r.Start();
			if (takeSamples) { r.Take(); r.Take(); }
			mgr = std::make_unique<SnapshotManager>(*r.rec, r.registry, r.schema.service.get(), store);
			mgr->SetTimeProvider(&FakeNow, nullptr);
			mgr->SetWorld("coast_range");
			store.SetWriteStateFiles(true);
		}
		std::string CellPath(const char* name)
		{
			char p[300];
			EXPECT_TRUE(SnapshotCellFile::Path(store.Folder(), name, p, sizeof(p)));
			return p;
		}
		const SnapshotRecord* Find(const char* name) const
		{
			for (uint32_t i = 0; i < mgr->Count(); ++i)
			{
				const SnapshotRecord& rec = mgr->Records()[i];
				if (rec.mUsed && std::strcmp(rec.mName, name) == 0) { return &rec; }
			}
			return nullptr;
		}
		bool HasTmp() const
		{
			for (const auto& e : fs::directory_iterator(store.Folder())) { if (e.path().extension() == ".tmp") { return true; } }
			return false;
		}
		size_t CellFileCount() const
		{
			size_t n = 0;
			for (const auto& e : fs::directory_iterator(store.Folder())) { if (e.path().string().find(".cells.json") != std::string::npos) { ++n; } }
			return n;
		}
		bool TakeOk(const char* name, bool overwrite = false)
		{
			bool ok = mgr->Take(name, overwrite, res);
			return ok;
		}
	};
}

TEST(SnapshotManagerTake, ReturnsPendingAndWritesNothingBeforeTheBoundary)
{
	Fix f; f.Setup();
	ASSERT_TRUE(f.TakeOk("snap1")) << f.res.error;
	EXPECT_TRUE(f.res.ok);
	EXPECT_TRUE(f.res.pending);
	EXPECT_TRUE(f.mgr->OperationInFlight());
	EXPECT_FALSE(fs::exists(f.CellPath("snap1")));
	EXPECT_FALSE(fs::exists(f.store.StatePath("snap1")));
	ASSERT_NE(f.Find("snap1"), nullptr);
	EXPECT_EQ(f.Find("snap1")->mStatus, SnapshotStatus::Pending);
}

TEST(SnapshotManagerTake, SuccessWritesBothHalvesAndFillsTheRecord)
{
	Fix f; f.Setup();
	gNow = 1792510123;
	ASSERT_TRUE(f.TakeOk("snap1"));
	f.store.RunFrameBoundary();
	EXPECT_FALSE(f.mgr->OperationInFlight());
	EXPECT_TRUE(fs::exists(f.store.StatePath("snap1")));
	EXPECT_TRUE(fs::exists(f.CellPath("snap1")));
	EXPECT_FALSE(f.HasTmp());
	const SnapshotRecord* rec = f.Find("snap1");
	ASSERT_NE(rec, nullptr);
	EXPECT_EQ(rec->mStatus, SnapshotStatus::Ready);
	EXPECT_GT(rec->mBytes, 0u);
	EXPECT_EQ(rec->mSample, f.r.rec->SampleCount() - 1);
	EXPECT_EQ(rec->mRunId, f.r.rec->RunId());
	EXPECT_EQ(rec->mCreatedUnix, 1792510123);
	EXPECT_STREQ(rec->mWorld, "coast_range");
	EXPECT_STREQ(rec->mLabel, f.r.rec->Header(f.r.rec->SampleCount() - 1).mLabel.AsCStr());

	SnapshotCellData d; char err[256];
	ASSERT_TRUE(SnapshotCellFile::Read(f.CellPath("snap1").c_str(), d, err, sizeof(err))) << err;
	EXPECT_STREQ(d.mHeader.mName, "snap1");
	EXPECT_EQ(d.mHeader.mSample, rec->mSample);
	EXPECT_EQ(d.mHeader.mRunId, f.r.rec->RunId());
	EXPECT_EQ(d.mHeader.mCreatedUnix, 1792510123);
	EXPECT_STREQ(d.mHeader.mWorld, "coast_range");
	EXPECT_STREQ(d.mHeader.mLabel, rec->mLabel);
	EXPECT_EQ(d.LayerCount(), 2u);
}

TEST(SnapshotManagerTake, CapturesLiveValuesAtTakeTimeEvenIfTheyChangeBeforeTheCallback)
{
	Fix f; f.Setup();
	for (uint32_t i = 0; i < 12; ++i) { f.r.grid.Set("temp", i, 10.0f + float(i)); }
	// live differs from what the recorder last sampled
	ASSERT_TRUE(f.TakeOk("snap1"));
	for (uint32_t i = 0; i < 12; ++i) { f.r.grid.Set("temp", i, 99.0f); }	// after the take, before the boundary
	f.store.RunFrameBoundary();

	SnapshotCellData d; char err[256];
	ASSERT_TRUE(SnapshotCellFile::Read(f.CellPath("snap1").c_str(), d, err, sizeof(err))) << err;
	float out[12];
	ASSERT_TRUE(SnapshotCellFile::Dequantise(d, StringCRC("temp"), out, 12));
	for (uint32_t i = 0; i < 12; ++i) { EXPECT_NEAR(out[i], 10.0f + float(i), 100.0f / 65535.0f) << i; }
}

TEST(SnapshotManagerTake, StoreFailureSurfacesTheMessageAndLeavesNoCellFile)
{
	Fix f; f.Setup();
	f.store.FailNext("disk full");
	ASSERT_TRUE(f.TakeOk("snap1"));
	f.store.RunFrameBoundary();
	const SnapshotRecord* rec = f.Find("snap1");
	ASSERT_NE(rec, nullptr);
	EXPECT_EQ(rec->mStatus, SnapshotStatus::Failed);
	EXPECT_NE(std::string(rec->mReason).find("disk full"), std::string::npos);
	EXPECT_FALSE(fs::exists(f.CellPath("snap1")));
	EXPECT_FALSE(f.HasTmp());
	EXPECT_FALSE(f.mgr->OperationInFlight());
	EXPECT_FALSE(f.store.Exists("snap1"));
}

TEST(SnapshotManagerTake, StoreFailureWithPartialStateIsDeleted)
{
	Fix f; f.Setup();
	f.store.SetLeavePartialOnFail(true);
	f.store.FailNext("half way");
	ASSERT_TRUE(f.TakeOk("snap1"));
	f.store.RunFrameBoundary();
	EXPECT_FALSE(f.store.Exists("snap1"));
	EXPECT_FALSE(fs::exists(f.store.StatePath("snap1")));
	EXPECT_EQ(f.Find("snap1")->mStatus, SnapshotStatus::Failed);
	EXPECT_EQ(f.CellFileCount(), 0u);
}

TEST(SnapshotManagerTake, CellFileWriteFailureDeletesTheStoreHalfAndMarksFailed)
{
	Fix f; f.Setup();
	ASSERT_TRUE(f.TakeOk("snap1"));
	std::error_code ec;
	fs::remove_all(f.store.Folder(), ec);		// the folder vanishes: the cell file cannot be written
	f.store.RunFrameBoundary();
	const SnapshotRecord* rec = f.Find("snap1");
	ASSERT_NE(rec, nullptr);
	EXPECT_EQ(rec->mStatus, SnapshotStatus::Failed);
	EXPECT_NE(rec->mReason[0], '\0');
	EXPECT_FALSE(f.store.Exists("snap1"));		// Delete was called
	EXPECT_FALSE(f.mgr->OperationInFlight());
	fs::create_directories(f.store.Folder());
	EXPECT_FALSE(f.HasTmp());
	EXPECT_EQ(f.CellFileCount(), 0u);
}

TEST(SnapshotManagerTake, RefusalsChangeNothingAndDoNotBeginTheGate)
{
	Fix f; f.Setup();
	TakeResult r;
	EXPECT_FALSE(f.mgr->Take("bad name!", false, r));
	EXPECT_FALSE(r.ok); EXPECT_FALSE(r.pending); EXPECT_NE(r.error[0], '\0');
	EXPECT_FALSE(f.mgr->OperationInFlight());
	EXPECT_EQ(f.store.PendingCount(), 0u);

	f.store.AddExisting("dup");
	EXPECT_FALSE(f.mgr->Take("dup", false, r));
	EXPECT_FALSE(f.mgr->OperationInFlight());
	EXPECT_EQ(f.Find("dup"), nullptr);
	EXPECT_EQ(f.store.PendingCount(), 0u);
}

TEST(SnapshotManagerTake, SeventeenthTakeIsRefused)
{
	Fix f; f.Setup();
	for (uint32_t i = 0; i < kMaxSnapshots; ++i)
	{
		const std::string n = "s" + std::to_string(i);
		ASSERT_TRUE(f.TakeOk(n.c_str())) << f.res.error;
		f.store.RunFrameBoundary();
	}
	EXPECT_EQ(f.CellFileCount(), size_t(kMaxSnapshots));
	TakeResult r;
	EXPECT_FALSE(f.mgr->Take("s16", false, r));
	EXPECT_FALSE(f.mgr->OperationInFlight());
	EXPECT_EQ(f.CellFileCount(), size_t(kMaxSnapshots));
	EXPECT_EQ(f.Find("s16"), nullptr);
	EXPECT_EQ(f.store.PendingCount(), 0u);
}

TEST(SnapshotManagerTake, SecondTakeWhileOneIsInFlightIsRefused)
{
	Fix f; f.Setup();
	ASSERT_TRUE(f.TakeOk("a"));
	TakeResult r;
	EXPECT_FALSE(f.mgr->Take("b", false, r));
	EXPECT_NE(std::string(r.error).find("in progress"), std::string::npos);
	EXPECT_EQ(f.store.PendingCount(), 1u);
	EXPECT_EQ(f.Find("b"), nullptr);
	EXPECT_TRUE(f.mgr->OperationInFlight());
	f.store.RunFrameBoundary();
	EXPECT_EQ(f.Find("a")->mStatus, SnapshotStatus::Ready);
}

TEST(SnapshotManagerTake, NoSamplesRecordedIsRefused)
{
	Fix f; f.Setup(false);
	TakeResult r;
	EXPECT_FALSE(f.mgr->Take("a", false, r));
	EXPECT_STREQ(r.error, "no samples recorded yet");
	EXPECT_FALSE(f.mgr->OperationInFlight());
	EXPECT_EQ(f.store.PendingCount(), 0u);
}

TEST(SnapshotManagerTake, OverwriteSuccessReplacesTheCellFile)
{
	Fix f; f.Setup();
	f.r.grid.SetAll("temp", 20.0f);
	ASSERT_TRUE(f.TakeOk("a")); f.store.RunFrameBoundary();
	f.r.grid.SetAll("temp", 80.0f);
	ASSERT_TRUE(f.TakeOk("a", true)) << f.res.error; f.store.RunFrameBoundary();
	SnapshotCellData d; char err[256];
	ASSERT_TRUE(SnapshotCellFile::Read(f.CellPath("a").c_str(), d, err, sizeof(err))) << err;
	float out[12];
	ASSERT_TRUE(SnapshotCellFile::Dequantise(d, StringCRC("temp"), out, 12));
	EXPECT_NEAR(out[0], 80.0f, 100.0f / 65535.0f);
	EXPECT_EQ(f.Find("a")->mStatus, SnapshotStatus::Ready);
}

TEST(SnapshotManagerTake, OverwriteWithStoreFailureKeepsTheOldCellFile)
{
	Fix f; f.Setup();
	f.r.grid.SetAll("temp", 20.0f);
	ASSERT_TRUE(f.TakeOk("a")); f.store.RunFrameBoundary();
	f.r.grid.SetAll("temp", 80.0f);
	f.store.FailNext("nope");
	ASSERT_TRUE(f.TakeOk("a", true)); f.store.RunFrameBoundary();
	EXPECT_EQ(f.Find("a")->mStatus, SnapshotStatus::Failed);
	SnapshotCellData d; char err[256];
	ASSERT_TRUE(SnapshotCellFile::Read(f.CellPath("a").c_str(), d, err, sizeof(err))) << err;
	float out[12];
	ASSERT_TRUE(SnapshotCellFile::Dequantise(d, StringCRC("temp"), out, 12));
	EXPECT_NEAR(out[0], 20.0f, 100.0f / 65535.0f);
	EXPECT_FALSE(f.HasTmp());
}

TEST(SnapshotManagerTake, GateIsReleasedAfterEachFailureSoALaterTakeWorks)
{
	Fix f; f.Setup();
	f.store.FailNext("boom");
	ASSERT_TRUE(f.TakeOk("a")); f.store.RunFrameBoundary();
	ASSERT_FALSE(f.mgr->OperationInFlight());
	ASSERT_TRUE(f.TakeOk("a")) << f.res.error;		// same name again: the failed record is replaced
	f.store.RunFrameBoundary();
	EXPECT_EQ(f.Find("a")->mStatus, SnapshotStatus::Ready);
	EXPECT_TRUE(f.Find("a")->mReason[0] == '\0');

	ASSERT_TRUE(f.TakeOk("b"));
	std::error_code ec; fs::remove_all(f.store.Folder(), ec);
	f.store.RunFrameBoundary();
	fs::create_directories(f.store.Folder());
	EXPECT_EQ(f.Find("b")->mStatus, SnapshotStatus::Failed);
	EXPECT_TRUE(f.TakeOk("c")) << f.res.error;
	f.store.RunFrameBoundary();
	EXPECT_EQ(f.Find("c")->mStatus, SnapshotStatus::Ready);
}

TEST(SnapshotManagerTake, TwoTakesOfDifferentNamesAfterBoundaries)
{
	Fix f; f.Setup();
	ASSERT_TRUE(f.TakeOk("a")); f.store.RunFrameBoundary();
	ASSERT_TRUE(f.TakeOk("b")); f.store.RunFrameBoundary();
	EXPECT_EQ(f.Find("a")->mStatus, SnapshotStatus::Ready);
	EXPECT_EQ(f.Find("b")->mStatus, SnapshotStatus::Ready);
	EXPECT_TRUE(fs::exists(f.CellPath("a")));
	EXPECT_TRUE(fs::exists(f.CellPath("b")));
}

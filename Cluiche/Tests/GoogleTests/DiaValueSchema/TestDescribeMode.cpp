#include <gtest/gtest.h>
#include <DiaValueSchema/DescribeMode.h>
#include <DiaValueSchema/ValueSchemaExporter.h>
#include <DiaValueSchema/Testing/ValueInfoBuilder.h>
#include "RampTestSupport.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// DiaValueSchema Feature 6 task 4: describe mode (AC 1). The runner writes the export once the barrier says every PU has
// finished Init, exactly once, and reports the exit code to request. No window, renderer or debug server is involved.

using namespace Dia::ValueSchema;
using Dia::ValueSchema::Testing::MakeValueInfo;

namespace
{
	struct FakeBarrier : IDescribeBarrier
	{
		BarrierState mState = BarrierState::kWaiting;
		unsigned int mPolls = 0;
		BarrierState Poll() override { ++mPolls; return mState; }
	};

	struct DescribeFixture : ::testing::Test
	{
		DescribeFixture() : mRamps(true, false) {}

		void SetUp() override
		{
			ValueInfo side = MakeValueInfo("cow.params", "rate", ValueKind::Parameter, "Global\\X/x.schema.json");
			ASSERT_EQ(RegisterResult::Ok, mRamps.mSchema.Register(side));
			mRamps.Finish();
			mPath = (std::filesystem::temp_directory_path() / "vs_describe_mode_test.json").string();
			std::remove(mPath.c_str());
			DescribeSession::Reset();
		}
		void TearDown() override
		{
			std::remove(mPath.c_str());
			DescribeSession::Reset();
		}

		DescribeRequest MakeRequest(const char* path) const
		{
			DescribeRequest r;
			r.mActive = true;
			SetText(r.mPath, path);
			SetText(r.mApp, "cow");
			SetText(r.mConfig, "Debug");
			return r;
		}
		DescribeStatus Tick(DescribeModeRunner& runner, IDescribeBarrier& barrier)
		{
			return runner.Tick(barrier, mRamps.Svc(), nullptr, nullptr);
		}
		bool FileExists() const { return std::filesystem::exists(mPath); }
		Json::Value ReadFile() const
		{
			std::ifstream in(mPath, std::ios::binary);
			std::stringstream ss; ss << in.rdbuf();
			Json::Value v; Json::Reader().parse(ss.str(), v);
			return v;
		}

		VsTest::RampFixture	mRamps;
		std::string			mPath;
	};
}

TEST_F(DescribeFixture, BarrierNotReadyWritesNothing)
{
	DescribeModeRunner runner(MakeRequest(mPath.c_str()));
	FakeBarrier barrier;
	EXPECT_EQ(DescribeStatus::kWaiting, Tick(runner, barrier));
	EXPECT_EQ(DescribeStatus::kWaiting, Tick(runner, barrier));
	EXPECT_FALSE(FileExists());
	EXPECT_FALSE(runner.IsFinished());
	EXPECT_EQ(2u, barrier.mPolls);
}

TEST_F(DescribeFixture, ReadyWritesValidExportAndExitsZero)
{
	DescribeModeRunner runner(MakeRequest(mPath.c_str()));
	FakeBarrier barrier;
	barrier.mState = BarrierState::kReady;
	EXPECT_EQ(DescribeStatus::kDone, Tick(runner, barrier));
	EXPECT_TRUE(runner.IsFinished());
	EXPECT_EQ(0, runner.GetExitCode());
	ASSERT_TRUE(FileExists());
	const Json::Value doc = ReadFile();
	EXPECT_EQ("dia.value_schema_export", doc["format"].asString());
	EXPECT_EQ("cow", doc["header"]["app"].asString());
	EXPECT_EQ("Debug", doc["header"]["config"].asString());
	EXPECT_EQ(1u, doc["entries"].size());
}

TEST_F(DescribeFixture, WaitsThenWritesWhenBarrierBecomesReady)
{
	DescribeModeRunner runner(MakeRequest(mPath.c_str()));
	FakeBarrier barrier;
	EXPECT_EQ(DescribeStatus::kWaiting, Tick(runner, barrier));
	EXPECT_FALSE(FileExists());
	barrier.mState = BarrierState::kReady;
	EXPECT_EQ(DescribeStatus::kDone, Tick(runner, barrier));
	EXPECT_TRUE(FileExists());
}

TEST_F(DescribeFixture, UnwritablePathFailsWithNonZeroExitAndMessage)
{
	DescribeModeRunner runner(MakeRequest("Z:\\no_such_dir_vs\\deeper\\export.json"));
	FakeBarrier barrier;
	barrier.mState = BarrierState::kReady;
	EXPECT_EQ(DescribeStatus::kFailed, Tick(runner, barrier));
	EXPECT_TRUE(runner.IsFinished());
	EXPECT_NE(0, runner.GetExitCode());
	EXPECT_EQ(DescribeModeRunner::kExitWriteFailed, runner.GetExitCode());
	EXPECT_NE('\0', runner.GetMessage()[0]);
}

TEST_F(DescribeFixture, RunsOnlyOnce)
{
	DescribeModeRunner runner(MakeRequest(mPath.c_str()));
	FakeBarrier barrier;
	barrier.mState = BarrierState::kReady;
	ASSERT_EQ(DescribeStatus::kDone, Tick(runner, barrier));
	const unsigned int pollsAfterFirst = barrier.mPolls;
	std::remove(mPath.c_str());
	EXPECT_EQ(DescribeStatus::kDone, Tick(runner, barrier));
	EXPECT_FALSE(FileExists());					// not rewritten
	EXPECT_EQ(pollsAfterFirst, barrier.mPolls);	// barrier not polled again
	EXPECT_EQ(0, runner.GetExitCode());
}

TEST_F(DescribeFixture, FailureIsFinalAndNotRetried)
{
	DescribeModeRunner runner(MakeRequest("Z:\\no_such_dir_vs\\export.json"));
	FakeBarrier barrier;
	barrier.mState = BarrierState::kReady;
	ASSERT_EQ(DescribeStatus::kFailed, Tick(runner, barrier));
	EXPECT_EQ(DescribeStatus::kFailed, Tick(runner, barrier));
	EXPECT_EQ(1u, barrier.mPolls);
}

TEST_F(DescribeFixture, AbortedBarrierFailsWithoutWriting)
{
	DescribeModeRunner runner(MakeRequest(mPath.c_str()));
	FakeBarrier barrier;
	barrier.mState = BarrierState::kAborted;
	EXPECT_EQ(DescribeStatus::kFailed, Tick(runner, barrier));
	EXPECT_FALSE(FileExists());
	EXPECT_EQ(DescribeModeRunner::kExitAborted, runner.GetExitCode());
	EXPECT_NE('\0', runner.GetMessage()[0]);
}

TEST_F(DescribeFixture, ExplicitFailCarriesMessageAndExitCode)
{
	DescribeModeRunner runner(MakeRequest(mPath.c_str()));
	runner.Fail("timed out waiting for Init");
	EXPECT_TRUE(runner.IsFinished());
	EXPECT_EQ(DescribeModeRunner::kExitTimedOut, runner.GetExitCode());
	EXPECT_STREQ("timed out waiting for Init", runner.GetMessage());
	FakeBarrier barrier;
	barrier.mState = BarrierState::kReady;
	EXPECT_EQ(DescribeStatus::kFailed, Tick(runner, barrier));
	EXPECT_FALSE(FileExists());
}

TEST_F(DescribeFixture, InactiveRequestNeverWrites)
{
	DescribeRequest r = MakeRequest(mPath.c_str());
	r.mActive = false;
	DescribeModeRunner runner(r);
	FakeBarrier barrier;
	barrier.mState = BarrierState::kReady;
	EXPECT_EQ(DescribeStatus::kWaiting, Tick(runner, barrier));
	EXPECT_FALSE(FileExists());
}

// --- Snapshot provider (Sim-Hosted Command Help task 4b) ----------------------

namespace
{
	struct FakeProvider : IDescribeSnapshotProvider
	{
		const ValueSchemaService* mService = nullptr;
		unsigned int mNotReadyCalls = 0;	// Acquire returns false this many times first
		unsigned int mCalls = 0;
		bool Acquire(DescribeView& out) override
		{
			++mCalls;
			if (mNotReadyCalls > 0) { --mNotReadyCalls; return false; }
			out.mService = mService;
			return true;
		}
	};
}

TEST_F(DescribeFixture, ProviderIsNotAskedWhileTheBarrierIsWaiting)
{
	DescribeModeRunner runner(MakeRequest(mPath.c_str()));
	FakeBarrier barrier;
	FakeProvider provider;
	provider.mService = &mRamps.Svc();
	EXPECT_EQ(DescribeStatus::kWaiting, runner.Tick(barrier, provider));
	EXPECT_EQ(0u, provider.mCalls);
	EXPECT_FALSE(FileExists());
}

TEST_F(DescribeFixture, ProviderNotReadyKeepsWaitingThenWritesTheProvidedSnapshot)
{
	DescribeModeRunner runner(MakeRequest(mPath.c_str()));
	FakeBarrier barrier;
	barrier.mState = BarrierState::kReady;
	FakeProvider provider;
	provider.mService = &mRamps.Svc();
	provider.mNotReadyCalls = 2;
	EXPECT_EQ(DescribeStatus::kWaiting, runner.Tick(barrier, provider));
	EXPECT_EQ(DescribeStatus::kWaiting, runner.Tick(barrier, provider));
	EXPECT_FALSE(FileExists());
	EXPECT_EQ(DescribeStatus::kDone, runner.Tick(barrier, provider));
	EXPECT_TRUE(FileExists());
	EXPECT_EQ(3u, provider.mCalls);
	EXPECT_EQ(DescribeModeRunner::kExitOk, runner.GetExitCode());
}

TEST_F(DescribeFixture, ProviderWithoutAServiceFailsWithoutWriting)
{
	DescribeModeRunner runner(MakeRequest(mPath.c_str()));
	FakeBarrier barrier;
	barrier.mState = BarrierState::kReady;
	FakeProvider provider;	// mService stays null
	EXPECT_EQ(DescribeStatus::kFailed, runner.Tick(barrier, provider));
	EXPECT_NE(0, runner.GetExitCode());
	EXPECT_FALSE(FileExists());
}

TEST_F(DescribeFixture, ProviderIsNotAskedAfterTheRunnerFinished)
{
	DescribeModeRunner runner(MakeRequest(mPath.c_str()));
	FakeBarrier barrier;
	barrier.mState = BarrierState::kReady;
	FakeProvider provider;
	provider.mService = &mRamps.Svc();
	ASSERT_EQ(DescribeStatus::kDone, runner.Tick(barrier, provider));
	EXPECT_EQ(DescribeStatus::kDone, runner.Tick(barrier, provider));
	EXPECT_EQ(1u, provider.mCalls);
}

// --- Argument parsing -------------------------------------------------------

TEST(DescribeModeArgs, AbsentFlagLeavesRequestInactive)
{
	const char* argv[] = { "app.exe", "--automation" };
	DescribeRequest r;
	EXPECT_TRUE(ParseDescribeArgs(2, argv, r));
	EXPECT_FALSE(r.mActive);
}

TEST(DescribeModeArgs, PathIsParsed)
{
	const char* argv[] = { "app.exe", "--automation", "--describe-schema", "C:\\out\\x.json" };
	DescribeRequest r;
	ASSERT_TRUE(ParseDescribeArgs(4, argv, r));
	EXPECT_TRUE(r.mActive);
	EXPECT_STREQ("C:\\out\\x.json", r.mPath.AsCStr());
	EXPECT_STREQ("", r.mStage.AsCStr());
}

TEST(DescribeModeArgs, StageIsParsed)
{
	const char* argv[] = { "app.exe", "--describe-schema", "x.json", "--stage", "Weather" };
	DescribeRequest r;
	ASSERT_TRUE(ParseDescribeArgs(5, argv, r));
	EXPECT_STREQ("Weather", r.mStage.AsCStr());
}

TEST(DescribeModeArgs, MissingPathValueIsAnError)
{
	const char* argv[] = { "app.exe", "--describe-schema" };
	DescribeRequest r;
	EXPECT_FALSE(ParseDescribeArgs(2, argv, r));
	const char* argv2[] = { "app.exe", "--describe-schema", "--stage", "S" };
	EXPECT_FALSE(ParseDescribeArgs(4, argv2, r));
}

TEST(DescribeModeArgs, StageWithoutDescribeIsIgnored)
{
	const char* argv[] = { "app.exe", "--stage", "Weather" };
	DescribeRequest r;
	EXPECT_TRUE(ParseDescribeArgs(3, argv, r));
	EXPECT_FALSE(r.mActive);
}

TEST(DescribeModeArgs, OverlongPathIsAnError)
{
	const std::string longPath(2000, 'a');
	const char* argv[] = { "app.exe", "--describe-schema", longPath.c_str() };
	DescribeRequest r;
	EXPECT_FALSE(ParseDescribeArgs(3, argv, r));
}

// --- Process-level handoff --------------------------------------------------

TEST(DescribeSessionTest, InactiveByDefaultAndExitCodeIsZero)
{
	DescribeSession::Reset();
	EXPECT_FALSE(DescribeSession::IsActive());
	EXPECT_EQ(0, DescribeSession::FinalExitCode());
}

TEST(DescribeSessionTest, ActiveButNeverCompletedIsAFailure)
{
	DescribeSession::Reset();
	DescribeRequest r; r.mActive = true; SetText(r.mPath, "x.json");
	DescribeSession::Begin(r);
	EXPECT_TRUE(DescribeSession::IsActive());
	EXPECT_FALSE(DescribeSession::IsComplete());
	EXPECT_EQ(DescribeModeRunner::kExitNotRun, DescribeSession::FinalExitCode());
	DescribeSession::Reset();
}

TEST(DescribeSessionTest, CompleteRecordsExitCode)
{
	DescribeSession::Reset();
	DescribeRequest r; r.mActive = true; SetText(r.mPath, "x.json");
	DescribeSession::Begin(r);
	DescribeSession::Complete(0);
	EXPECT_TRUE(DescribeSession::IsComplete());
	EXPECT_EQ(0, DescribeSession::FinalExitCode());
	DescribeSession::Reset();
	DescribeSession::Begin(r);
	DescribeSession::Complete(2);
	EXPECT_EQ(2, DescribeSession::FinalExitCode());
	DescribeSession::Reset();
}

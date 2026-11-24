#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <DiaParallelExecutor/SerialExecutor.h>
#include <DiaDeferredStep/Testing/ManualStepLauncher.h>
#include <DiaDeferredStep/Testing/RecordingStep.h>
#include <DiaDeferredStep/Testing/ScriptedKeyDriver.h>

using namespace Dia::DeferredStep;
using namespace Dia::DeferredStep::Testing;

namespace
{
	LaunchInfo MakeInfo(const char* channel, StepKey key)
	{
		LaunchInfo info;
		info.channelId = Dia::Core::StringCRC(channel);
		info.kickKey = key;
		return info;
	}

	struct FakeScheduler
	{
		void Tick(StepKey key) { keys.push_back(key); }
		std::vector<StepKey> keys;
	};
}

// ---- ManualStepLauncher ----

TEST(DeferredStepHelperTestLauncher, LaunchReturnsDistinctValidTokensStartingAtOne)
{
	ManualStepLauncher launcher;
	RecordingStep a, b;
	LaunchToken t1 = launcher.Launch(a, MakeInfo("A", 1));
	LaunchToken t2 = launcher.Launch(b, MakeInfo("B", 2));
	EXPECT_TRUE(t1.IsValid());
	EXPECT_TRUE(t2.IsValid());
	EXPECT_EQ(t1.value, 1u);
	EXPECT_EQ(t2.value, 2u);
	EXPECT_EQ(launcher.GetLaunchCount(), 2u);
	EXPECT_EQ(launcher.GetLastLaunchInfo().kickKey, 2u);
	EXPECT_EQ(launcher.GetStepForToken(t1), &a);
	EXPECT_EQ(launcher.GetStepForToken(t2), &b);
	EXPECT_EQ(a.GetRunCount(), 0u);
}

TEST(DeferredStepHelperTestLauncher, IsCompleteFalseUntilCompleteNext)
{
	ManualStepLauncher launcher;
	RecordingStep a;
	LaunchToken t = launcher.Launch(a, MakeInfo("A", 1));
	EXPECT_FALSE(launcher.IsComplete(t));
	launcher.CompleteNext();
	EXPECT_TRUE(launcher.IsComplete(t));
	EXPECT_EQ(a.GetRunCount(), 1u);
}

TEST(DeferredStepHelperTestLauncher, CompleteNextIsOldestFirst)
{
	ManualStepLauncher launcher;
	RecordingStep a, b, c;
	LaunchToken ta = launcher.Launch(a, MakeInfo("A", 1));
	LaunchToken tb = launcher.Launch(b, MakeInfo("B", 1));
	LaunchToken tc = launcher.Launch(c, MakeInfo("C", 1));
	launcher.CompleteNext();
	EXPECT_TRUE(launcher.IsComplete(ta));
	EXPECT_FALSE(launcher.IsComplete(tb));
	EXPECT_FALSE(launcher.IsComplete(tc));
	launcher.CompleteNext();
	EXPECT_TRUE(launcher.IsComplete(tb));
	EXPECT_FALSE(launcher.IsComplete(tc));
}

TEST(DeferredStepHelperTestLauncher, CompleteAllCompletesEverything)
{
	ManualStepLauncher launcher;
	RecordingStep a, b;
	LaunchToken ta = launcher.Launch(a, MakeInfo("A", 1));
	LaunchToken tb = launcher.Launch(b, MakeInfo("B", 1));
	launcher.CompleteAll();
	EXPECT_TRUE(launcher.IsComplete(ta));
	EXPECT_TRUE(launcher.IsComplete(tb));
	EXPECT_EQ(a.GetRunCount(), 1u);
	EXPECT_EQ(b.GetRunCount(), 1u);
	launcher.CompleteAll();
	EXPECT_EQ(a.GetRunCount(), 1u);
}

TEST(DeferredStepHelperTestLauncher, WaitRunsInlineExactlyOnce)
{
	ManualStepLauncher launcher;
	RecordingStep a;
	LaunchToken t = launcher.Launch(a, MakeInfo("A", 1));
	launcher.Wait(t);
	EXPECT_EQ(a.GetRunCount(), 1u);
	EXPECT_TRUE(launcher.IsComplete(t));
	EXPECT_EQ(a.GetRunThread(), std::this_thread::get_id());
	launcher.Wait(t);
	EXPECT_EQ(a.GetRunCount(), 1u);
	launcher.CompleteAll();
	EXPECT_EQ(a.GetRunCount(), 1u);
}

TEST(DeferredStepHelperTestLauncher, WaitAfterCompleteDoesNotRerun)
{
	ManualStepLauncher launcher;
	RecordingStep a;
	LaunchToken t = launcher.Launch(a, MakeInfo("A", 1));
	launcher.CompleteNext();
	launcher.Wait(t);
	EXPECT_EQ(a.GetRunCount(), 1u);
}

TEST(DeferredStepHelperTestLauncher, CallLogRecordsOrder)
{
	ManualStepLauncher launcher;
	RecordingStep a;
	LaunchToken t = launcher.Launch(a, MakeInfo("A", 1));
	launcher.IsComplete(t);
	launcher.Wait(t);
	launcher.Release(t);
	ASSERT_EQ(launcher.GetCallCount(), 4u);
	EXPECT_EQ(launcher.GetCall(0).kind, ManualStepLauncher::Call::kLaunch);
	EXPECT_EQ(launcher.GetCall(1).kind, ManualStepLauncher::Call::kIsComplete);
	EXPECT_EQ(launcher.GetCall(2).kind, ManualStepLauncher::Call::kWait);
	EXPECT_EQ(launcher.GetCall(3).kind, ManualStepLauncher::Call::kRelease);
	for (uint32_t i = 0; i < 4; ++i)
	{
		EXPECT_EQ(launcher.GetCall(i).token.value, t.value);
	}
	EXPECT_TRUE(launcher.IsReleased(t));
	EXPECT_EQ(launcher.GetReleaseCount(), 1u);
	launcher.ClearCalls();
	EXPECT_EQ(launcher.GetCallCount(), 0u);
}

TEST(DeferredStepHelperTestLauncher, ReleaseOfUnknownTokenIsDetected)
{
	ManualStepLauncher launcher;
	launcher.SetAssertOnBadRelease(false);
	LaunchToken bogus;
	bogus.value = 99;
	launcher.Release(bogus);
	EXPECT_EQ(launcher.GetBadReleaseCount(), 1u);
	EXPECT_EQ(launcher.GetReleaseCount(), 0u);
}

TEST(DeferredStepHelperTestLauncher, DoubleReleaseIsDetected)
{
	ManualStepLauncher launcher;
	launcher.SetAssertOnBadRelease(false);
	RecordingStep a;
	LaunchToken t = launcher.Launch(a, MakeInfo("A", 1));
	launcher.Release(t);
	EXPECT_EQ(launcher.GetBadReleaseCount(), 0u);
	launcher.Release(t);
	EXPECT_EQ(launcher.GetBadReleaseCount(), 1u);
	EXPECT_EQ(launcher.GetReleaseCount(), 1u);
}

// ---- RecordingStep ----

TEST(DeferredStepHelperTestRecordingStep, CountsAndThreadIds)
{
	RecordingStep step;
	Dia::ParallelExecutor::SerialExecutor executor;
	EXPECT_EQ(step.GetCaptureCount(), 0u);
	step.Capture();
	step.Run(executor);
	step.Apply();
	step.Apply();
	EXPECT_EQ(step.GetCaptureCount(), 1u);
	EXPECT_EQ(step.GetRunCount(), 1u);
	EXPECT_EQ(step.GetApplyCount(), 2u);
	EXPECT_EQ(step.GetCaptureThread(), std::this_thread::get_id());
	EXPECT_EQ(step.GetRunThread(), std::this_thread::get_id());
	EXPECT_EQ(step.GetApplyThread(), std::this_thread::get_id());
	EXPECT_EQ(step.GetLastExecutor(), &executor);
	EXPECT_EQ(step.GetPendingVersion(), 1u);
}

TEST(DeferredStepHelperTestRecordingStep, RunOnOtherThreadIsDistinguishable)
{
	RecordingStep step;
	Dia::ParallelExecutor::SerialExecutor executor;
	step.Capture();
	std::thread worker([&] { step.Run(executor); });
	worker.join();
	EXPECT_EQ(step.GetCaptureThread(), std::this_thread::get_id());
	EXPECT_NE(step.GetRunThread(), std::this_thread::get_id());
}

TEST(DeferredStepHelperTestRecordingStep, OnRunHookAndOrderSink)
{
	std::vector<int> order;
	RecordingStep a(1, &order), b(2, &order);
	int hookCalls = 0;
	a.SetOnRun([&] { ++hookCalls; });
	Dia::ParallelExecutor::SerialExecutor executor;
	a.Run(executor);
	EXPECT_EQ(hookCalls, 1);
	b.Apply();
	a.Apply();
	ASSERT_EQ(order.size(), 2u);
	EXPECT_EQ(order[0], 2);
	EXPECT_EQ(order[1], 1);
}

// ---- ScriptedKeyDriver ----

TEST(DeferredStepHelperTestKeyDriver, ReplaysKeysIncludingJumpAndRepeat)
{
	FakeScheduler scheduler;
	ScriptedKeyDriver::Run(scheduler, { 1, 2, 2, 10, 11 });
	ASSERT_EQ(scheduler.keys.size(), 5u);
	EXPECT_EQ(scheduler.keys[0], 1u);
	EXPECT_EQ(scheduler.keys[1], 2u);
	EXPECT_EQ(scheduler.keys[2], 2u);
	EXPECT_EQ(scheduler.keys[3], 10u);
	EXPECT_EQ(scheduler.keys[4], 11u);
}

TEST(DeferredStepHelperTestKeyDriver, ArrayOverloadAndEmpty)
{
	FakeScheduler scheduler;
	const StepKey keys[] = { 5, 3 };
	ScriptedKeyDriver::Run(scheduler, keys, 2);
	ASSERT_EQ(scheduler.keys.size(), 2u);
	EXPECT_EQ(scheduler.keys[1], 3u);
	ScriptedKeyDriver::Run(scheduler, keys, 0);
	EXPECT_EQ(scheduler.keys.size(), 2u);
}

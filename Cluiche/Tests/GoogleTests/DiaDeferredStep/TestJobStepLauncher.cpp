#include <gtest/gtest.h>
#include <DiaCore/Core/Assert.h>
#include <DiaDeferredStep/CapConfig.h>
#include <DiaDeferredStep/CapGate.h>
#include <DiaDeferredStep/IDeferredStep.h>
#include <DiaDeferredStep/JobStepLauncher.h>
#include <DiaDeferredStep/SerialExecutor.h>
#include <DiaDeferredStep/Testing/LogCapture.h>
#include <DiaThreading/JobSystem.h>
#include <atomic>
#include <functional>
#include <memory>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace Dia::DeferredStep;

namespace
{
	using Dia::Threading::JobSystem;

#ifdef DEBUG
	int gLauncherAssertCount = 0;
	void LauncherCountingAssertHandler(const char*, const char*, int, const char*, ...)
	{
		++gLauncherAssertCount;
	}

	struct AssertScope
	{
		AssertScope()
		{
			mPrev = Dia::Core::g_pAssertFunc;
			Dia::Core::g_pAssertFunc = LauncherCountingAssertHandler;
			gLauncherAssertCount = 0;
		}
		~AssertScope() { Dia::Core::g_pAssertFunc = mPrev; }
		Dia::Core::DIA_ASSERT_FUNC mPrev;
	};
#endif

	struct Latch
	{
		std::atomic<bool> open{ false };
		void Open() { open.store(true); }
		void Wait() const { while (!open.load()) { std::this_thread::yield(); } }
	};

	CapConfig CapOf(uint32_t m, uint32_t rangesPerWorker = 4)
	{
		CapConfig c;
		c.workerCap = m;
		c.rangesPerWorker = rangesPerWorker;
		return c;
	}

	// Step whose Run executes a caller-supplied body against the launcher's executor.
	class FnStep final : public IDeferredStep
	{
	public:
		explicit FnStep(std::function<void(IParallelExecutor&)> body = nullptr) : mBody(std::move(body)) {}

		void Capture() override {}
		void Run(IParallelExecutor& executor) override
		{
			mRunThread = std::this_thread::get_id();
			mRunCount.fetch_add(1);
			if (mBody) { mBody(executor); }
		}
		void Apply() override {}
		uint32_t GetPendingVersion() const override { return 1; }
		void SerializePending(Dia::SaveGame::SaveContext&) const override {}
		void DeserializePending(Dia::SaveGame::LoadContext&) override {}

		uint32_t GetRunCount() const { return mRunCount.load(); }
		std::thread::id GetRunThread() const { return mRunThread; }

	private:
		std::function<void(IParallelExecutor&)> mBody;
		std::atomic<uint32_t> mRunCount{ 0 };
		std::thread::id mRunThread;
	};

	// Fills out[i] = i * 3 + 1 over [0,count) through the executor.
	std::function<void(IParallelExecutor&)> FillBody(std::vector<uint32_t>& out)
	{
		return [&out](IParallelExecutor& exec)
		{
			exec.ParallelFor(static_cast<uint32_t>(out.size()), 8, [&out](uint32_t b, uint32_t e)
			{
				for (uint32_t i = b; i < e; ++i) { out[i] = i * 3 + 1; }
			});
		};
	}
}

TEST(DeferredStepLauncherTest, RoundTripLaunchWaitRelease)
{
	JobSystem jobs;
	jobs.Initialize(4);
	{
		JobStepLauncher launcher(jobs, CapOf(2));
		std::vector<uint32_t> out(500, 0);
		FnStep step(FillBody(out));
		const LaunchToken token = launcher.Launch(step, LaunchInfo());
		ASSERT_TRUE(token.IsValid());
		launcher.Wait(token);
		EXPECT_TRUE(launcher.IsComplete(token));
		EXPECT_EQ(step.GetRunCount(), 1u);
		for (uint32_t i = 0; i < out.size(); ++i) { ASSERT_EQ(out[i], i * 3 + 1) << i; }
		launcher.Release(token);
		EXPECT_EQ(launcher.GetCapGate().GetStats().slotsHeld, 0u);
	}
	jobs.Shutdown();
}

TEST(DeferredStepLauncherTest, ResultMatchesSerialExecutor)
{
	JobSystem jobs;
	jobs.Initialize(4);
	std::vector<uint32_t> capped(777, 0), serial(777, 0);
	{
		JobStepLauncher launcher(jobs, CapOf(3));
		FnStep step(FillBody(capped));
		const LaunchToken token = launcher.Launch(step, LaunchInfo());
		launcher.Wait(token);
		launcher.Release(token);
	}
	{
		FnStep step(FillBody(serial));
		SerialExecutor exec;
		step.Run(exec);
	}
	EXPECT_EQ(capped, serial);
	jobs.Shutdown();
}

TEST(DeferredStepLauncherTest, EightLaunchesGetDistinctValidTokens)
{
	JobSystem jobs;
	jobs.Initialize(4);
	{
		JobStepLauncher launcher(jobs, CapOf(2));
		std::vector<std::unique_ptr<FnStep>> steps;
		std::vector<LaunchToken> tokens;
		std::set<uint32_t> values;
		for (int i = 0; i < 8; ++i)
		{
			steps.push_back(std::make_unique<FnStep>([](IParallelExecutor& e) { e.ParallelFor(32, 4, [](uint32_t, uint32_t) {}); }));
			tokens.push_back(launcher.Launch(*steps.back(), LaunchInfo()));
			ASSERT_TRUE(tokens.back().IsValid());
			values.insert(tokens.back().value);
		}
		EXPECT_EQ(values.size(), 8u);
		for (const LaunchToken& t : tokens) { launcher.Wait(t); }
		for (const LaunchToken& t : tokens) { launcher.Release(t); }
		for (auto& s : steps) { EXPECT_EQ(s->GetRunCount(), 1u); }
		EXPECT_EQ(launcher.GetCapGate().GetStats().slotsHeld, 0u);
	}
	jobs.Shutdown();
}

// AC 14: a full gate must not make Launch block.
TEST(DeferredStepLauncherTest, LaunchWithFullGateReturnsImmediatelyAndQueues)
{
	JobSystem jobs;
	jobs.Initialize(4);
	{
		JobStepLauncher launcher(jobs, CapOf(1));
		Latch started, release;
		FnStep first([&](IParallelExecutor&) { started.Open(); release.Wait(); });
		FnStep second;

		const LaunchToken t1 = launcher.Launch(first, LaunchInfo());
		started.Wait();
		const LaunchToken t2 = launcher.Launch(second, LaunchInfo());   // gate full: must return, not block
		ASSERT_TRUE(t1.IsValid());
		ASSERT_TRUE(t2.IsValid());
		EXPECT_NE(t1.value, t2.value);
		EXPECT_FALSE(launcher.IsComplete(t2));
		EXPECT_EQ(second.GetRunCount(), 0u);
		EXPECT_GE(launcher.GetCapGate().GetStats().queuedAtLeastOnce, 1u);

		release.Open();
		launcher.Wait(t1);
		launcher.Wait(t2);
		EXPECT_TRUE(launcher.IsComplete(t1));
		EXPECT_TRUE(launcher.IsComplete(t2));
		EXPECT_EQ(second.GetRunCount(), 1u);
		launcher.Release(t1);
		launcher.Release(t2);
		EXPECT_EQ(launcher.GetCapGate().GetStats().slotsHeld, 0u);
	}
	jobs.Shutdown();
}

// AC 16 / 17: pool never initialised -> synchronous run, already complete, one warning.
TEST(DeferredStepLauncherTest, UninitialisedJobSystemRunsInlineAndWarns)
{
	JobSystem jobs;   // never Initialize()d
	Testing::LogCapture log;
	JobStepLauncher launcher(jobs, CapOf(1));
	std::vector<uint32_t> out(100, 0);
	FnStep step(FillBody(out));
	const LaunchToken token = launcher.Launch(step, LaunchInfo());
	ASSERT_TRUE(token.IsValid());
	EXPECT_EQ(step.GetRunCount(), 1u);
	EXPECT_EQ(step.GetRunThread(), std::this_thread::get_id());
	EXPECT_TRUE(launcher.IsComplete(token));
	for (uint32_t i = 0; i < out.size(); ++i) { ASSERT_EQ(out[i], i * 3 + 1) << i; }
	EXPECT_GE(log.Warnings(), 1u);
	launcher.Wait(token);   // immediate
	launcher.Release(token);
}

TEST(DeferredStepLauncherTest, ForcedSubmitFailureRunsInlineAndReadsComplete)
{
	JobSystem jobs;
	jobs.Initialize(4);
	{
		Testing::LogCapture log;
		JobStepLauncher launcher(jobs, CapOf(2));
		launcher.GetCapGate().SetSubmitFailureHookForTest([] { return true; });
		FnStep step;
		const LaunchToken token = launcher.Launch(step, LaunchInfo());
		ASSERT_TRUE(token.IsValid());
		EXPECT_EQ(step.GetRunCount(), 1u);
		EXPECT_EQ(step.GetRunThread(), std::this_thread::get_id());
		EXPECT_TRUE(launcher.IsComplete(token));
		EXPECT_GE(log.Warnings(), 1u);
		EXPECT_EQ(launcher.GetCapGate().GetStats().slotsHeld, 0u);
		launcher.Release(token);
	}
	jobs.Shutdown();
}

// A queued coordinator whose later (pump-time) submit fails must not stay incomplete forever.
TEST(DeferredStepLauncherTest, QueuedCoordinatorWithPumpTimeFailureCompletesInline)
{
	JobSystem jobs;
	jobs.Initialize(4);
	{
		JobStepLauncher launcher(jobs, CapOf(1));
		Latch started, release;
		FnStep first([&](IParallelExecutor&) { started.Open(); release.Wait(); });
		FnStep second;
		const LaunchToken t1 = launcher.Launch(first, LaunchInfo());
		started.Wait();
		const LaunchToken t2 = launcher.Launch(second, LaunchInfo());   // queued
		launcher.GetCapGate().SetSubmitFailureHookForTest([] { return true; });
		release.Open();   // first finishes, pumps second, submit "fails"

		launcher.Wait(t1);
		launcher.Wait(t2);   // must run second inline
		EXPECT_EQ(second.GetRunCount(), 1u);
		EXPECT_TRUE(launcher.IsComplete(t2));
		launcher.Release(t1);
		launcher.Release(t2);
		EXPECT_EQ(launcher.GetCapGate().GetStats().slotsHeld, 0u);
	}
	jobs.Shutdown();
}

TEST(DeferredStepLauncherTest, IsCompleteOnPumpTimeFailureEventuallyTurnsTrue)
{
	JobSystem jobs;
	jobs.Initialize(4);
	{
		JobStepLauncher launcher(jobs, CapOf(1));
		Latch started, release;
		FnStep first([&](IParallelExecutor&) { started.Open(); release.Wait(); });
		FnStep second;
		const LaunchToken t1 = launcher.Launch(first, LaunchInfo());
		started.Wait();
		const LaunchToken t2 = launcher.Launch(second, LaunchInfo());
		launcher.GetCapGate().SetSubmitFailureHookForTest([] { return true; });
		release.Open();
		while (!launcher.IsComplete(t2)) { std::this_thread::yield(); }   // polling must converge
		EXPECT_EQ(second.GetRunCount(), 1u);
		launcher.Wait(t1);
		launcher.Release(t1);
		launcher.Release(t2);
	}
	jobs.Shutdown();
}

// AC 22: Wait on an already-complete token does not touch the pool.
TEST(DeferredStepLauncherTest, WaitOnCompleteTokenDoesNotTouchJobSystem)
{
	JobSystem jobs;
	jobs.Initialize(2);
	JobStepLauncher launcher(jobs, CapOf(1));
	FnStep step;
	const LaunchToken token = launcher.Launch(step, LaunchInfo());
	while (!launcher.IsComplete(token)) { std::this_thread::yield(); }
	jobs.Shutdown();
	launcher.Wait(token);   // would hang or fail if it needed the pool
	EXPECT_TRUE(launcher.IsComplete(token));
	launcher.Release(token);
}

// AC 23: Release frees the slot and bumps the generation.
TEST(DeferredStepLauncherTest, ReleaseBumpsGenerationSoSlotReuseGivesDifferentToken)
{
	JobSystem jobs;
	jobs.Initialize(2);
	{
		JobStepLauncher launcher(jobs, CapOf(1));
		FnStep a, b;
		const LaunchToken t1 = launcher.Launch(a, LaunchInfo());
		launcher.Wait(t1);
		launcher.Release(t1);
		const LaunchToken t2 = launcher.Launch(b, LaunchInfo());
		ASSERT_TRUE(t2.IsValid());
		EXPECT_NE(t1.value, t2.value);
		EXPECT_EQ(t1.value & 0xFFu, t2.value & 0xFFu);   // same slot index
		launcher.Wait(t2);
		launcher.Release(t2);
	}
	jobs.Shutdown();
}

#ifdef DEBUG
// AC 21: zero / stale / released tokens assert and read as complete; Release of them is a no-op.
TEST(DeferredStepLauncherTest, ZeroStaleAndReleasedTokensAssertAndReadComplete)
{
	JobSystem jobs;
	jobs.Initialize(2);
	{
		JobStepLauncher launcher(jobs, CapOf(1));
		FnStep step;
		const LaunchToken good = launcher.Launch(step, LaunchInfo());
		launcher.Wait(good);
		launcher.Release(good);

		AssertScope scope;
		EXPECT_TRUE(launcher.IsComplete(LaunchToken()));
		EXPECT_EQ(gLauncherAssertCount, 1);
		EXPECT_TRUE(launcher.IsComplete(good));   // stale: slot freed, generation bumped
		EXPECT_EQ(gLauncherAssertCount, 2);
		launcher.Wait(good);
		EXPECT_EQ(gLauncherAssertCount, 3);
		launcher.Release(good);                   // double release
		EXPECT_EQ(gLauncherAssertCount, 4);
		launcher.Release(LaunchToken());
		EXPECT_EQ(gLauncherAssertCount, 5);
		LaunchToken bogus; bogus.value = 0xFFFFu;   // index out of range
		EXPECT_TRUE(launcher.IsComplete(bogus));
		EXPECT_EQ(gLauncherAssertCount, 6);

		// The launcher is still healthy afterwards.
		FnStep again;
		const LaunchToken t = launcher.Launch(again, LaunchInfo());
		launcher.Wait(t);
		EXPECT_EQ(again.GetRunCount(), 1u);
		launcher.Release(t);
	}
	jobs.Shutdown();
}

// Table exhaustion: Debug asserts; the step still runs synchronously and the token reads complete.
TEST(DeferredStepLauncherTest, TableExhaustionAssertsRunsInlineAndTokenIsInert)
{
	JobSystem jobs;
	jobs.Initialize(2);
	{
		JobStepLauncher launcher(jobs, CapOf(1));
		Latch release;
		std::atomic<int> started{ 0 };
		std::vector<std::unique_ptr<FnStep>> steps;
		std::vector<LaunchToken> tokens;
		for (uint32_t i = 0; i < JobStepLauncher::kMaxInFlight; ++i)
		{
			steps.push_back(std::make_unique<FnStep>([&](IParallelExecutor&) { started.fetch_add(1); release.Wait(); }));
			tokens.push_back(launcher.Launch(*steps.back(), LaunchInfo()));
		}

		AssertScope scope;
		FnStep overflow;
		const LaunchToken o = launcher.Launch(overflow, LaunchInfo());
		EXPECT_GE(gLauncherAssertCount, 1);
		EXPECT_TRUE(o.IsValid());
		EXPECT_EQ(overflow.GetRunCount(), 1u);
		EXPECT_EQ(overflow.GetRunThread(), std::this_thread::get_id());
		const int before = gLauncherAssertCount;
		EXPECT_TRUE(launcher.IsComplete(o));
		launcher.Wait(o);
		launcher.Release(o);
		EXPECT_EQ(gLauncherAssertCount, before);   // overflow token is valid and inert

		release.Open();
		for (const LaunchToken& t : tokens) { launcher.Wait(t); }
		for (const LaunchToken& t : tokens) { launcher.Release(t); }
	}
	jobs.Shutdown();
}
#endif

// AC 24: destructor with nothing in flight is fine and does not block.
TEST(DeferredStepLauncherTest, DestructorWithNothingInFlightIsClean)
{
	JobSystem jobs;
	jobs.Initialize(2);
	{
		JobStepLauncher launcher(jobs, CapOf(1));
		FnStep step;
		const LaunchToken t = launcher.Launch(step, LaunchInfo());
		launcher.Wait(t);
		launcher.Release(t);
	}
	{
		JobStepLauncher idle(jobs, CapOf(1));
	}
	jobs.Shutdown();
	SUCCEED();
}

TEST(DeferredStepLauncherTest, ThrowingStepOnPoolStillCompletes)
{
	JobSystem jobs;
	jobs.Initialize(2);
	{
		// Error log is emitted on a pool thread with no log buffer in tests, so it is not asserted here.
		JobStepLauncher launcher(jobs, CapOf(1));
		FnStep step([](IParallelExecutor&) { throw std::runtime_error("boom"); });
		const LaunchToken t = launcher.Launch(step, LaunchInfo());
		ASSERT_TRUE(t.IsValid());
		launcher.Wait(t);
		EXPECT_TRUE(launcher.IsComplete(t));
		launcher.Release(t);
		EXPECT_EQ(launcher.GetCapGate().GetStats().slotsHeld, 0u);

		// Launcher still works.
		FnStep ok;
		const LaunchToken t2 = launcher.Launch(ok, LaunchInfo());
		launcher.Wait(t2);
		EXPECT_EQ(ok.GetRunCount(), 1u);
		launcher.Release(t2);
	}
	jobs.Shutdown();
}

TEST(DeferredStepLauncherTest, ThrowingStepOnInlineFallbackStillReturnsCompleteToken)
{
	JobSystem jobs;   // uninitialised -> inline
	Testing::LogCapture log;
	JobStepLauncher launcher(jobs, CapOf(1));
	FnStep step([](IParallelExecutor&) { throw std::runtime_error("boom"); });
	LaunchToken t;
	EXPECT_NO_THROW(t = launcher.Launch(step, LaunchInfo()));
	ASSERT_TRUE(t.IsValid());
	EXPECT_TRUE(launcher.IsComplete(t));
	EXPECT_GE(log.Count(Testing::LogCapture::Level::kError), 1u);
	launcher.Release(t);
}

TEST(DeferredStepLauncherTest, ThreeChannelsAtCapTwoAllCompleteAndGateDrains)
{
	JobSystem jobs;
	jobs.Initialize(4);
	{
		JobStepLauncher launcher(jobs, CapOf(2));
		std::vector<uint32_t> o0(400, 0), o1(400, 0), o2(400, 0);
		FnStep s0(FillBody(o0)), s1(FillBody(o1)), s2(FillBody(o2));
		const LaunchToken t0 = launcher.Launch(s0, LaunchInfo());
		const LaunchToken t1 = launcher.Launch(s1, LaunchInfo());
		const LaunchToken t2 = launcher.Launch(s2, LaunchInfo());
		launcher.Wait(t2);
		launcher.Wait(t0);
		launcher.Wait(t1);
		launcher.Release(t0);
		launcher.Release(t1);
		launcher.Release(t2);
		for (uint32_t i = 0; i < 400; ++i)
		{
			ASSERT_EQ(o0[i], i * 3 + 1);
			ASSERT_EQ(o1[i], i * 3 + 1);
			ASSERT_EQ(o2[i], i * 3 + 1);
		}
		const CapStats s = launcher.GetCapGate().GetStats();
		EXPECT_EQ(s.slotsHeld, 0u);
		EXPECT_LE(s.peakSlotsHeld, 2u);
	}
	jobs.Shutdown();
}

TEST(DeferredStepLauncherTest, RepeatedLaunchReleaseCyclesReuseSlots)
{
	JobSystem jobs;
	jobs.Initialize(4);
	{
		JobStepLauncher launcher(jobs, CapOf(2));
		for (int i = 0; i < 300; ++i)   // wraps the 8-bit generation
		{
			FnStep step([](IParallelExecutor& e) { e.ParallelFor(20, 2, [](uint32_t, uint32_t) {}); });
			const LaunchToken t = launcher.Launch(step, LaunchInfo());
			ASSERT_TRUE(t.IsValid());
			launcher.Wait(t);
			ASSERT_EQ(step.GetRunCount(), 1u);
			launcher.Release(t);
		}
		EXPECT_EQ(launcher.GetCapGate().GetStats().slotsHeld, 0u);
	}
	jobs.Shutdown();
}

#include <gtest/gtest.h>
#include <DiaDeferredStep/CapConfig.h>
#include <DiaDeferredStep/DeferredStepTypes.h>
#include <DiaDeferredStep/IDeferredStep.h>
#include <DiaDeferredStep/JobStepLauncher.h>
#include <DiaThreading/IJobObserver.h>
#include <DiaThreading/JobSystem.h>
#include <functional>
#include <mutex>
#include <set>
#include <utility>
#include <vector>

using namespace Dia::DeferredStep;
using Dia::Threading::IJobObserver;
using Dia::Threading::JobContext;
using Dia::Threading::JobSystem;
using Dia::Threading::JobTimingRecord;

namespace
{
	struct Seen { uint32_t owner; uint32_t tag; uint16_t range; uint16_t rangeCount; };

	class CapturingObserver final : public IJobObserver
	{
	public:
		void OnWorkerStart(unsigned) override {}
		void OnWorkerStop(unsigned) override {}
		JobContext CaptureContext() override { return JobContext(); }
		void OnJobBegin(const JobContext&) override {}
		void OnJobEnd(const JobContext&, const JobTimingRecord& rec) override
		{
			std::lock_guard<std::mutex> l(mMutex);
			mSeen.push_back({ rec.owner, rec.tag, rec.range, rec.rangeCount });
		}

		std::vector<Seen> Take()
		{
			std::lock_guard<std::mutex> l(mMutex);
			return mSeen;
		}

	private:
		std::mutex        mMutex;
		std::vector<Seen> mSeen;
	};

	class FnStep final : public IDeferredStep
	{
	public:
		explicit FnStep(std::function<void(IParallelExecutor&)> body) : mBody(std::move(body)) {}
		void Capture() override {}
		void Run(IParallelExecutor& executor) override { mBody(executor); }
		void Apply() override {}
		uint32_t GetPendingVersion() const override { return 1; }
		void SerializePending(Dia::SaveGame::SaveContext&) const override {}
		void DeserializePending(Dia::SaveGame::LoadContext&) override {}
	private:
		std::function<void(IParallelExecutor&)> mBody;
	};

	CapConfig CapOf(uint32_t m, uint32_t rangesPerWorker = 4)
	{
		CapConfig c;
		c.workerCap = m;
		c.rangesPerWorker = rangesPerWorker;
		return c;
	}

	LaunchInfo Info(const char* channel, StepKey key)
	{
		LaunchInfo i;
		i.channelId = Dia::Core::StringCRC(channel);
		i.kickKey = key;
		return i;
	}

	// Runs one step (ParallelFor over 1000 items, min range 1) and returns the range count it used.
	uint32_t RunStep(JobStepLauncher& launcher, const LaunchInfo& info)
	{
		uint32_t rangeCount = 0;
		FnStep step([&rangeCount](IParallelExecutor& ex)
		{
			ex.ParallelFor(1000, 1, [](uint32_t, uint32_t) {});
			rangeCount = ex.GetLastRangeCount();
		});
		LaunchToken t = launcher.Launch(step, info);
		launcher.Wait(t);
		launcher.Release(t);
		return rangeCount;
	}

	std::vector<Seen> Filter(const std::vector<Seen>& all, uint32_t owner)
	{
		std::vector<Seen> out;
		for (const Seen& s : all) { if (s.owner == owner) { out.push_back(s); } }
		return out;
	}
}

TEST(DeferredStepAttribution, CoordinatorAndRangesAreTagged)
{
	CapturingObserver obs;
	JobSystem js;
	js.Initialize(4, nullptr, &obs);
	const LaunchInfo info = Info("AttribChan", 77);
	uint32_t rangeCount = 0;
	{
		JobStepLauncher launcher(js, CapOf(4));
		rangeCount = RunStep(launcher, info);
	}
	js.Shutdown();

	ASSERT_GT(rangeCount, 1u);
	const std::vector<Seen> recs = Filter(obs.Take(), info.channelId.Value());

	size_t coordinators = 0;
	std::set<uint16_t> ranges;
	for (const Seen& s : recs)
	{
		EXPECT_EQ(s.tag, 77u);
		if (s.range == kCoordinatorRange)
		{
			++coordinators;
			EXPECT_EQ(s.rangeCount, 0u);
		}
		else
		{
			EXPECT_EQ(s.rangeCount, rangeCount);
			EXPECT_GE(s.range, 1u);
			EXPECT_LT(s.range, rangeCount);
			EXPECT_TRUE(ranges.insert(s.range).second) << "duplicate range " << s.range;
		}
	}
	EXPECT_EQ(coordinators, 1u);
	// Range 0 runs on the coordinator and is never submitted; the rest are submitted or stolen.
	EXPECT_EQ(ranges.count(0), 0u);
	EXPECT_LE(ranges.size(), static_cast<size_t>(rangeCount - 1));
}

TEST(DeferredStepAttribution, StolenRangesProduceNoRecord)
{
	CapturingObserver obs;
	JobSystem js;
	js.Initialize(4, nullptr, &obs);
	const LaunchInfo info = Info("StealChan", 5);
	uint32_t rangeCount = 0;
	{
		// M = 1: the coordinator holds the only slot, so every range queues and is stolen.
		JobStepLauncher launcher(js, CapOf(1));
		rangeCount = RunStep(launcher, info);
		EXPECT_EQ(launcher.GetCapGate().GetStats().stolen, static_cast<uint64_t>(rangeCount - 1));
	}
	js.Shutdown();

	ASSERT_GT(rangeCount, 1u);
	const std::vector<Seen> recs = Filter(obs.Take(), info.channelId.Value());
	ASSERT_EQ(recs.size(), 1u);
	EXPECT_EQ(recs[0].range, kCoordinatorRange);
	EXPECT_EQ(recs[0].rangeCount, 0u);
}

TEST(DeferredStepAttribution, FailedTrySubmitRangesProduceNoRecord)
{
	CapturingObserver obs;
	JobSystem js;
	js.Initialize(4, nullptr, &obs);
	const LaunchInfo info = Info("FailChan", 9);
	uint32_t rangeCount = 0;
	{
		JobStepLauncher launcher(js, CapOf(4));
		int calls = 0;
		launcher.GetCapGate().SetSubmitFailureHookForTest([&calls] { return ++calls > 1; });   // coordinator ok, ranges fail
		rangeCount = RunStep(launcher, info);
		EXPECT_EQ(launcher.GetCapGate().GetStats().trySubmitFailed, static_cast<uint64_t>(rangeCount - 1));
	}
	js.Shutdown();

	ASSERT_GT(rangeCount, 1u);
	const std::vector<Seen> recs = Filter(obs.Take(), info.channelId.Value());
	ASSERT_EQ(recs.size(), 1u);
	EXPECT_EQ(recs[0].range, kCoordinatorRange);
}

TEST(DeferredStepAttribution, KickKeyTruncatesToLow32Bits)
{
	CapturingObserver obs;
	JobSystem js;
	js.Initialize(2, nullptr, &obs);
	const LaunchInfo info = Info("BigKeyChan", (static_cast<StepKey>(3) << 32) | 0xABCDu);
	{
		JobStepLauncher launcher(js, CapOf(2));
		RunStep(launcher, info);
	}
	js.Shutdown();

	const std::vector<Seen> recs = Filter(obs.Take(), info.channelId.Value());
	ASSERT_FALSE(recs.empty());
	for (const Seen& s : recs) { EXPECT_EQ(s.tag, 0xABCDu); }
}

TEST(DeferredStepAttribution, ConcurrentChannelsHaveDistinctOwnersAndUniqueRanges)
{
	CapturingObserver obs;
	JobSystem js;
	js.Initialize(4, nullptr, &obs);
	const LaunchInfo a = Info("ChanA", 1);
	const LaunchInfo b = Info("ChanB", 2);
	ASSERT_NE(a.channelId.Value(), b.channelId.Value());
	uint32_t rcA = 0;
	uint32_t rcB = 0;
	{
		JobStepLauncher launcher(js, CapOf(4));
		FnStep stepA([&rcA](IParallelExecutor& ex) { ex.ParallelFor(1000, 1, [](uint32_t, uint32_t) {}); rcA = ex.GetLastRangeCount(); });
		FnStep stepB([&rcB](IParallelExecutor& ex) { ex.ParallelFor(1000, 1, [](uint32_t, uint32_t) {}); rcB = ex.GetLastRangeCount(); });
		LaunchToken ta = launcher.Launch(stepA, a);
		LaunchToken tb = launcher.Launch(stepB, b);
		launcher.Wait(ta);
		launcher.Wait(tb);
		launcher.Release(ta);
		launcher.Release(tb);
	}
	js.Shutdown();

	const std::vector<Seen> all = obs.Take();
	for (const LaunchInfo* info : { &a, &b })
	{
		const uint32_t rc = (info == &a) ? rcA : rcB;
		const std::vector<Seen> recs = Filter(all, info->channelId.Value());
		std::set<uint16_t> ranges;
		size_t coordinators = 0;
		for (const Seen& s : recs)
		{
			EXPECT_EQ(s.tag, static_cast<uint32_t>(info->kickKey));
			if (s.range == kCoordinatorRange) { ++coordinators; continue; }
			EXPECT_EQ(s.rangeCount, rc);
			EXPECT_TRUE(ranges.insert(s.range).second);
		}
		EXPECT_EQ(coordinators, 1u);
	}
}

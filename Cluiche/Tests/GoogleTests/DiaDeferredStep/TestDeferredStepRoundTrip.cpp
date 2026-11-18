// Save-and-Load equivalence (DFS-005, AC 20-23): a save taken mid-step produces a load that is byte-identical
// to never having saved, and the step is never re-run on load.

#include <gtest/gtest.h>
#include <atomic>
#include <cstdio>
#include <direct.h>
#include <string>
#include <thread>
#include <vector>
#include <DiaCore/Json/external/json/json.h>
#include <DiaDeferredStep/DeferredStepSaveParticipant.h>
#include <DiaDeferredStep/DeferredStepScheduler.h>
#include <DiaDeferredStep/IDeferredStep.h>
#include <DiaDeferredStep/IStepLauncher.h>
#include <DiaDeferredStep/SerialExecutor.h>
#include <DiaDeferredStep/Testing/ManualStepLauncher.h>
#include <DiaDeferredStep/Testing/PendingStateStep.h>
#include <DiaDeferredStep/Testing/ScriptedKeyDriver.h>
#include <DiaSaveGame/ISaveable.h>
#include <DiaSaveGame/LoadContext.h>
#include <DiaSaveGame/SaveConfig.h>
#include <DiaSaveGame/SaveContext.h>
#include <DiaSaveGame/SaveFormat.h>
#include <DiaSaveGame/SaveManager.h>
#include <DiaSaveGame/SaveRegistry.h>

using namespace Dia::DeferredStep;
using namespace Dia::SaveGame;
using Dia::Core::StringCRC;
using Dia::DeferredStep::Testing::PendingStateStep;

namespace
{
	struct KickEvent
	{
		int channel;
		StepKey kickKey;
		StepKey applyKey;
	};

	// The consumer owns its committed state and saves it itself; the participant only stores in-flight results.
	class ConsumerSaveable final : public ISaveable
	{
	public:
		explicit ConsumerSaveable(std::vector<PendingStateStep>& steps) : mSteps(steps) {}

		void Serialize(SaveContext& ctx) const override
		{
			for (uint32_t i = 0; i < mSteps.size(); ++i)
			{
				char name[16];
				snprintf(name, sizeof(name), "step_%u", i);
				ctx.BeginObject(StringCRC(name));
				PendingStateStep::WriteState(ctx, mSteps[i].GetCommitted());
				ctx.EndObject();
			}
		}

		void Deserialize(LoadContext& ctx) override
		{
			for (uint32_t i = 0; i < mSteps.size(); ++i)
			{
				char name[16];
				snprintf(name, sizeof(name), "step_%u", i);
				if (ctx.BeginObject(StringCRC(name)))
				{
					PendingStateStep::State state;
					if (PendingStateStep::ReadState(ctx, state))
					{
						mSteps[i].SetCommitted(state);
					}
					ctx.EndObject();
				}
			}
		}

		uint32_t GetVersion() const override { return 1; }

	private:
		std::vector<PendingStateStep>& mSteps;
	};

	// A complete little world: scheduler, kNumChannels PendingStateSteps, the participant and a real save pipeline.
	class RoundTripWorld
	{
	public:
		static constexpr int kNumChannels = 4;

		RoundTripWorld(IStepLauncher& launcher, const std::vector<KickEvent>& script)
			: mSteps(kNumChannels)
			, mScheduler(launcher)
			, mScript(script)
			, mConfig{ "temp/deferredstep_save_tests/", "slot_{id}.sav", 0, SaveFormat::Json }
			, mConsumer(mSteps)
			, mParticipant(mScheduler)
		{
			_mkdir("temp");
			_mkdir("temp/deferredstep_save_tests");
			for (int i = 0; i < kNumChannels; ++i)
			{
				char name[32];
				snprintf(name, sizeof(name), "RoundTripChannel%d", i);
				mChannels[i] = StringCRC(name);
				mSteps[i].Configure(i, &mKey, &mLog);
				mScheduler.Register(mChannels[i], mSteps[i]);
			}
			mParticipant.Attach(mRegistry, mManager);
			mRegistry.Register(StringCRC("Consumer"), &mConsumer);
			mManager.Init(mConfig, mRegistry);
		}

		~RoundTripWorld()
		{
			mParticipant.Detach();
			mRegistry.Unregister(StringCRC("Consumer"));
			for (int i = 0; i < kNumChannels; ++i)
			{
				mScheduler.Unregister(mChannels[i]);
			}
		}

		// What a sim step does at a key: apply what is due, then kick what the script says to kick.
		void Tick(StepKey key)
		{
			mKey = key;
			mScheduler.Tick(key);
			for (const KickEvent& e : mScript)
			{
				if (e.kickKey == key)
				{
					mSteps[e.channel].SetSeed(static_cast<uint32_t>(e.channel * 1000 + e.kickKey));
					if (mScheduler.Kick(mChannels[e.channel], e.kickKey, e.applyKey) != KickResult::kLaunched)
					{
						++mKickFailures;
					}
				}
			}
		}

		void RunKeys(StepKey from, StepKey to)
		{
			std::vector<StepKey> keys;
			for (StepKey k = from; k <= to; ++k)
			{
				keys.push_back(k);
			}
			Testing::ScriptedKeyDriver::Run(*this, keys.data(), keys.size());
		}

		std::vector<PendingStateStep> mSteps;
		StepKey mKey = 0;
		PendingStateStep::ApplyLog mLog;
		uint32_t mKickFailures = 0;
		StringCRC mChannels[kNumChannels];
		DeferredStepScheduler mScheduler;
		std::vector<KickEvent> mScript;
		SaveConfig mConfig;
		SaveRegistry mRegistry;
		SaveManager mManager;
		ConsumerSaveable mConsumer;
		DeferredStepSaveParticipant mParticipant;
	};

	// Genuinely concurrent launcher: Run executes on its own thread, and the step's mid-run hook can block it
	// on gate. Wait opens the gate and joins, so a Save drain is what lets the step finish.
	class GatedThreadLauncher final : public IStepLauncher
	{
	public:
		~GatedThreadLauncher() override
		{
			gate = true;
			for (Entry& e : mEntries)
			{
				if (e.thread.joinable())
				{
					e.thread.join();
				}
			}
		}

		LaunchToken Launch(IDeferredStep& step, const LaunchInfo&) override
		{
			Entry& e = mEntries[mCount++];
			e.done = false;
			IDeferredStep* stepPtr = &step;
			Entry* entry = &e;
			e.thread = std::thread([stepPtr, entry]()
			{
				SerialExecutor executor;
				stepPtr->Run(executor);
				entry->done = true;
			});
			LaunchToken token;
			token.value = mCount;
			return token;
		}

		bool IsComplete(LaunchToken token) const override { return mEntries[token.value - 1].done; }

		void Wait(LaunchToken token) override
		{
			gate = true;
			Entry& e = mEntries[token.value - 1];
			if (e.thread.joinable())
			{
				e.thread.join();
			}
		}

		void Release(LaunchToken) override {}

		std::atomic<bool> gate{ false };

	private:
		struct Entry
		{
			std::thread thread;
			std::atomic<bool> done{ false };
		};

		Entry mEntries[8];
		uint32_t mCount = 0;
	};
}

class DeferredStepRoundTrip : public ::testing::Test
{
protected:
	// Slot names are unique per test: gtest shards run as parallel processes sharing the temp directory.
	static std::string SlotName()
	{
		return std::string("roundtrip_") + ::testing::UnitTest::GetInstance()->current_test_info()->name();
	}

	DeferredStepRoundTrip() : mSlotName(SlotName()), mSlot(mSlotName.c_str()) {}

	~DeferredStepRoundTrip() override { remove(SlotPath().c_str()); }

	std::string SlotPath() const { return "temp/deferredstep_save_tests/slot_" + mSlotName + ".sav"; }

	int PendingCountInFile() const
	{
		FILE* f = fopen(SlotPath().c_str(), "rb");
		if (f == nullptr)
		{
			return -1;
		}
		std::string text;
		char chunk[4096];
		size_t n;
		while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0)
		{
			text.append(chunk, n);
		}
		fclose(f);
		Json::Value root;
		Json::Reader reader;
		if (!reader.parse(text, root) || !root.isMember("DeferredStep"))
		{
			return -1;
		}
		return root["DeferredStep"]["pending_count"].asInt();
	}

	static void ExpectSameCommitted(const RoundTripWorld& expected, const RoundTripWorld& actual)
	{
		for (int i = 0; i < RoundTripWorld::kNumChannels; ++i)
		{
			EXPECT_EQ(expected.mSteps[i].GetCommittedBytes(), actual.mSteps[i].GetCommittedBytes()) << "channel " << i;
			EXPECT_EQ(expected.mSteps[i].GetCommitted(), actual.mSteps[i].GetCommitted()) << "channel " << i;
		}
	}

	static const std::vector<KickEvent>& SingleKick()
	{
		static const std::vector<KickEvent> script = { { 0, 10, 17 } };
		return script;
	}

	Testing::ManualStepLauncher mRefLauncher;
	Testing::ManualStepLauncher mSavedLauncher;
	Testing::ManualStepLauncher mLoadedLauncher;
	std::string mSlotName;
	StringCRC mSlot;
};

// AC 20: same committed bytes, Apply order and applied keys with and without a save/load at key 12.
TEST_F(DeferredStepRoundTrip, SaveAtKey12MatchesNoSaveRun)
{
	RoundTripWorld reference(mRefLauncher, SingleKick());
	reference.RunKeys(1, 20);
	ASSERT_EQ(reference.mKickFailures, 0u);
	ASSERT_EQ(reference.mLog.size(), 1u);
	ASSERT_EQ(reference.mLog[0].second, 17);
	ASSERT_NE(reference.mSteps[0].GetCommitted(), PendingStateStep::State(PendingStateStep::kSize, 0));

	RoundTripWorld saved(mSavedLauncher, SingleKick());
	saved.RunKeys(1, 12);
	ASSERT_EQ(saved.mScheduler.GetState(saved.mChannels[0]), StepState::kRunning);
	ASSERT_TRUE(saved.mManager.Save(mSlot).Ok());

	RoundTripWorld loaded(mLoadedLauncher, SingleKick());
	ASSERT_TRUE(loaded.mManager.Load(mSlot).Ok());
	loaded.RunKeys(13, 20);
	EXPECT_EQ(loaded.mKickFailures, 0u);

	ExpectSameCommitted(reference, loaded);
	PendingStateStep::ApplyLog combined = saved.mLog;
	combined.insert(combined.end(), loaded.mLog.begin(), loaded.mLog.end());
	EXPECT_EQ(combined, reference.mLog);
	EXPECT_EQ(loaded.mSteps[0].GetAppliedKeys(), reference.mSteps[0].GetAppliedKeys());
	ASSERT_EQ(loaded.mSteps[0].GetAppliedKeys().size(), 1u);
	EXPECT_EQ(loaded.mSteps[0].GetAppliedKeys()[0], 17);
	EXPECT_EQ(saved.mSteps[0].GetApplyCount() + loaded.mSteps[0].GetApplyCount(), reference.mSteps[0].GetApplyCount());
}

// AC 21: Run exactly once across the cycle; the step is not re-run, nor re-captured, on load.
TEST_F(DeferredStepRoundTrip, RunCalledExactlyOnceAcrossSaveLoadCycle)
{
	RoundTripWorld reference(mRefLauncher, SingleKick());
	reference.RunKeys(1, 20);

	RoundTripWorld saved(mSavedLauncher, SingleKick());
	saved.RunKeys(1, 12);
	ASSERT_EQ(saved.mSteps[0].GetRunCount(), 0u);   // Manual launcher: not run until the save's drain waits on it.
	ASSERT_TRUE(saved.mManager.Save(mSlot).Ok());

	RoundTripWorld loaded(mLoadedLauncher, SingleKick());
	ASSERT_TRUE(loaded.mManager.Load(mSlot).Ok());
	loaded.RunKeys(13, 20);

	EXPECT_EQ(saved.mSteps[0].GetRunCount(), 1u);
	EXPECT_EQ(loaded.mSteps[0].GetRunCount(), 0u);
	EXPECT_EQ(saved.mSteps[0].GetRunCount() + loaded.mSteps[0].GetRunCount(), reference.mSteps[0].GetRunCount());
	EXPECT_EQ(saved.mSteps[0].GetCaptureCount(), 1u);
	EXPECT_EQ(loaded.mSteps[0].GetCaptureCount(), 0u);
	EXPECT_EQ(loaded.mSteps[0].GetDeserializeCount(), 1u);
	EXPECT_EQ(loaded.mSteps[0].GetApplyCount(), 1u);
	EXPECT_EQ(saved.mSteps[0].GetApplyCount(), 0u);
	EXPECT_EQ(mLoadedLauncher.GetLaunchCount(), 0u);
	EXPECT_EQ(loaded.mScheduler.GetStats().stalls, 0u);
}

// AC 22: a save after the apply key has nothing pending and changes nothing.
TEST_F(DeferredStepRoundTrip, SaveAfterApplyKeyStoresNothingPending)
{
	RoundTripWorld reference(mRefLauncher, SingleKick());
	reference.RunKeys(1, 20);

	RoundTripWorld saved(mSavedLauncher, SingleKick());
	saved.RunKeys(1, 18);
	ASSERT_EQ(saved.mSteps[0].GetApplyCount(), 1u);
	ASSERT_TRUE(saved.mManager.Save(mSlot).Ok());
	EXPECT_EQ(PendingCountInFile(), 0);
	EXPECT_EQ(saved.mScheduler.GetStats().drains, 0u);

	RoundTripWorld loaded(mLoadedLauncher, SingleKick());
	ASSERT_TRUE(loaded.mManager.Load(mSlot).Ok());
	EXPECT_EQ(loaded.mScheduler.GetPendingCount(), 0u);
	loaded.RunKeys(19, 20);

	// Nothing was restored, so nothing applied or ran in the loaded world; committed state came from the consumer.
	EXPECT_EQ(loaded.mSteps[0].GetApplyCount(), 0u);
	EXPECT_EQ(loaded.mSteps[0].GetRunCount(), 0u);
	EXPECT_EQ(loaded.mScheduler.GetStats().applies, 0u);
	EXPECT_TRUE(loaded.mLog.empty());
	ExpectSameCommitted(reference, loaded);

	// And the saving world, carried on, is identical to the no-save run.
	saved.RunKeys(19, 20);
	ExpectSameCommitted(reference, saved);
	EXPECT_EQ(saved.mLog, reference.mLog);
	EXPECT_EQ(saved.mSteps[0].GetRunCount(), reference.mSteps[0].GetRunCount());
}

// AC 23: the step has not completed at save time; the drain completes it and it is stored as pending.
TEST_F(DeferredStepRoundTrip, SaveWhileStepNotYetCompleteStillRoundTrips)
{
	RoundTripWorld reference(mRefLauncher, SingleKick());
	reference.RunKeys(1, 20);

	RoundTripWorld saved(mSavedLauncher, SingleKick());
	saved.RunKeys(1, 12);
	ASSERT_EQ(saved.mSteps[0].GetRunCount(), 0u);
	ASSERT_EQ(saved.mScheduler.GetState(saved.mChannels[0]), StepState::kRunning);
	ASSERT_TRUE(saved.mManager.Save(mSlot).Ok());
	EXPECT_EQ(saved.mSteps[0].GetRunCount(), 1u);
	EXPECT_EQ(saved.mScheduler.GetState(saved.mChannels[0]), StepState::kComplete);
	EXPECT_EQ(PendingCountInFile(), 1);

	RoundTripWorld loaded(mLoadedLauncher, SingleKick());
	ASSERT_TRUE(loaded.mManager.Load(mSlot).Ok());
	EXPECT_EQ(loaded.mScheduler.GetState(loaded.mChannels[0]), StepState::kComplete);
	StepKey applyKey = 0;
	ASSERT_TRUE(loaded.mScheduler.GetApplyKey(loaded.mChannels[0], applyKey));
	EXPECT_EQ(applyKey, 17);
	loaded.RunKeys(13, 20);

	ExpectSameCommitted(reference, loaded);
	ASSERT_EQ(loaded.mLog.size(), 1u);
	EXPECT_EQ(loaded.mLog[0].second, 17);
	EXPECT_EQ(loaded.mSteps[0].GetRunCount(), 0u);
}

// AC 23 with real concurrency: Run is executing on another thread (blocked mid-chain) when the save is taken.
TEST_F(DeferredStepRoundTrip, SaveWhileRunGenuinelyInFlightOnAnotherThread)
{
	RoundTripWorld reference(mRefLauncher, SingleKick());
	reference.RunKeys(1, 20);

	GatedThreadLauncher threadLauncher;
	std::atomic<bool> midRunReached{ false };
	{
		RoundTripWorld saved(threadLauncher, SingleKick());
		saved.mSteps[0].SetMidRunHook([&]()
		{
			midRunReached = true;
			while (!threadLauncher.gate)
			{
				std::this_thread::yield();
			}
		});
		saved.RunKeys(1, 12);
		while (!midRunReached)
		{
			std::this_thread::yield();
		}
		const std::vector<int32_t> workingMidRun = saved.mSteps[0].GetWorking();
		ASSERT_EQ(saved.mScheduler.GetState(saved.mChannels[0]), StepState::kRunning);
		ASSERT_FALSE(threadLauncher.IsComplete(LaunchToken{ 1 }));

		ASSERT_TRUE(saved.mManager.Save(mSlot).Ok());
		EXPECT_EQ(saved.mSteps[0].GetRunCount(), 1u);
		EXPECT_EQ(saved.mScheduler.GetState(saved.mChannels[0]), StepState::kComplete);
		EXPECT_EQ(PendingCountInFile(), 1);
		// The stored result is the finished one, not the half-transformed working copy.
		EXPECT_NE(saved.mSteps[0].GetWorking(), workingMidRun);
	}

	RoundTripWorld loaded(mLoadedLauncher, SingleKick());
	ASSERT_TRUE(loaded.mManager.Load(mSlot).Ok());
	loaded.RunKeys(13, 20);

	ExpectSameCommitted(reference, loaded);
	ASSERT_EQ(loaded.mLog.size(), 1u);
	EXPECT_EQ(loaded.mLog[0].second, 17);
	EXPECT_EQ(loaded.mSteps[0].GetRunCount(), 0u);
}

// Several channels: one applied before the save, two pending (shared apply key), one idle until after the load.
TEST_F(DeferredStepRoundTrip, MultiChannelRoundTripKeepsApplyOrderAndKeys)
{
	// ch0 applies at 8 (before the save); ch1 and ch3 are pending at 12 and share apply key 15 (kick order 1, 3);
	// ch1 is kicked again at 16 after the load; ch2 is idle until it is first kicked at 14, after the load.
	const std::vector<KickEvent> script = { { 0, 3, 8 }, { 1, 5, 15 }, { 3, 9, 15 }, { 2, 14, 20 }, { 1, 16, 19 } };

	RoundTripWorld reference(mRefLauncher, script);
	reference.RunKeys(1, 20);
	ASSERT_EQ(reference.mKickFailures, 0u);
	const PendingStateStep::ApplyLog expectedOrder = { { 0, 8 }, { 1, 15 }, { 3, 15 }, { 1, 19 }, { 2, 20 } };
	ASSERT_EQ(reference.mLog, expectedOrder);

	RoundTripWorld saved(mSavedLauncher, script);
	saved.RunKeys(1, 12);
	ASSERT_EQ(saved.mLog.size(), 1u);
	ASSERT_EQ(saved.mScheduler.GetState(saved.mChannels[2]), StepState::kIdle);
	ASSERT_TRUE(saved.mManager.Save(mSlot).Ok());
	EXPECT_EQ(PendingCountInFile(), 2);

	RoundTripWorld loaded(mLoadedLauncher, script);
	ASSERT_TRUE(loaded.mManager.Load(mSlot).Ok());
	EXPECT_EQ(loaded.mScheduler.GetPendingCount(), 2u);
	loaded.RunKeys(13, 20);
	EXPECT_EQ(loaded.mKickFailures, 0u);

	ExpectSameCommitted(reference, loaded);
	PendingStateStep::ApplyLog combined = saved.mLog;
	combined.insert(combined.end(), loaded.mLog.begin(), loaded.mLog.end());
	EXPECT_EQ(combined, reference.mLog);
	EXPECT_EQ(combined, expectedOrder);

	// Run once per kick across the cycle: ch0, ch1 (first), ch3 on the saving side; ch2, ch1 (second) on the loaded side.
	uint32_t runsSaved = 0, runsLoaded = 0, runsReference = 0;
	for (int i = 0; i < RoundTripWorld::kNumChannels; ++i)
	{
		runsSaved += saved.mSteps[i].GetRunCount();
		runsLoaded += loaded.mSteps[i].GetRunCount();
		runsReference += reference.mSteps[i].GetRunCount();
	}
	EXPECT_EQ(runsReference, 5u);
	EXPECT_EQ(runsSaved + runsLoaded, runsReference);
	EXPECT_EQ(loaded.mSteps[1].GetRunCount(), 1u);   // only the post-load re-kick
	EXPECT_EQ(loaded.mSteps[3].GetRunCount(), 0u);   // restored, never re-run
}

// The save itself is invisible: the world that saved, continued to key 20, equals the no-save run.
TEST_F(DeferredStepRoundTrip, SaveDoesNotChangeTheSavingWorld)
{
	const std::vector<KickEvent> script = { { 0, 3, 8 }, { 1, 5, 15 }, { 3, 9, 15 }, { 2, 14, 20 }, { 1, 16, 19 } };

	RoundTripWorld reference(mRefLauncher, script);
	reference.RunKeys(1, 20);

	RoundTripWorld saved(mSavedLauncher, script);
	saved.RunKeys(1, 12);
	ASSERT_TRUE(saved.mManager.Save(mSlot).Ok());
	saved.RunKeys(13, 20);

	EXPECT_EQ(saved.mKickFailures, 0u);
	ExpectSameCommitted(reference, saved);
	EXPECT_EQ(saved.mLog, reference.mLog);
	for (int i = 0; i < RoundTripWorld::kNumChannels; ++i)
	{
		EXPECT_EQ(saved.mSteps[i].GetRunCount(), reference.mSteps[i].GetRunCount()) << "channel " << i;
		EXPECT_EQ(saved.mSteps[i].GetApplyCount(), reference.mSteps[i].GetApplyCount()) << "channel " << i;
		EXPECT_EQ(saved.mSteps[i].GetAppliedKeys(), reference.mSteps[i].GetAppliedKeys()) << "channel " << i;
	}
}

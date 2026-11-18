// Save-and-Load exhaustive pass (DFS-005 task 7): edge cases, observation assertions, rejection reasons, blob sizing.

#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <direct.h>
#include <memory>
#include <string>
#include <vector>
#include <DiaCore/Json/external/json/json.h>
#include <DiaDeferredStep/DeferredStepSaveParticipant.h>
#include <DiaDeferredStep/DeferredStepScheduler.h>
#include <DiaDeferredStep/IDeferredStep.h>
#include <DiaDeferredStep/Testing/LogCapture.h>
#include <DiaDeferredStep/Testing/ManualStepLauncher.h>
#include <DiaDeferredStep/Testing/RecordingStep.h>
#include <DiaSaveGame/LoadContext.h>
#include <DiaSaveGame/SaveConfig.h>
#include <DiaSaveGame/SaveContext.h>
#include <DiaSaveGame/SaveFormat.h>
#include <DiaSaveGame/SaveManager.h>
#include <DiaSaveGame/SaveRegistry.h>

using namespace Dia::DeferredStep;
using namespace Dia::SaveGame;
using Dia::Core::StringCRC;

namespace
{
	typedef Dia::DeferredStep::Testing::LogCapture LogCapture;
	typedef LogCapture::Level LogLevel;

	// Step with a configurable pending payload. Optionally writes nothing in SerializePending.
	class EdgeStep final : public IDeferredStep
	{
	public:
		void Configure(int id, int32_t payload, std::vector<int>* applyLog)
		{
			mId = id;
			mPayload = payload;
			mApplyLog = applyLog;
		}

		void Capture() override {}
		void Run(IParallelExecutor&) override { ++mRunCount; }
		void Apply() override
		{
			++mApplyCount;
			if (mApplyLog != nullptr) { mApplyLog->push_back(mId); }
		}
		uint32_t GetPendingVersion() const override { return 1; }
		void SerializePending(Dia::SaveGame::SaveContext& ctx) const override
		{
			if (mWritesPayload) { ctx.Write(StringCRC("payload"), mPayload); }
		}
		void DeserializePending(Dia::SaveGame::LoadContext& ctx) override
		{
			++mDeserializeCount;
			mPayloadReadOk = ctx.Read(StringCRC("payload"), mPayloadRead);
		}

		bool mWritesPayload = true;
		uint32_t mRunCount = 0;
		uint32_t mApplyCount = 0;
		uint32_t mDeserializeCount = 0;
		bool mPayloadReadOk = false;
		int32_t mPayloadRead = -1;

	private:
		int mId = 0;
		int32_t mPayload = 0;
		std::vector<int>* mApplyLog = nullptr;
	};

	std::string ReadFile(const std::string& path)
	{
		std::string text;
		FILE* f = fopen(path.c_str(), "rb");
		if (f == nullptr) { return text; }
		char chunk[4096];
		size_t n;
		while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) { text.append(chunk, n); }
		fclose(f);
		return text;
	}

	// N payload channels (<= kMaxChannels) with a real save pipeline. channelOrder controls registration order.
	class EdgeWorld
	{
	public:
		static constexpr int kMax = 8;

		EdgeWorld(IStepLauncher& launcher, int numChannels, bool reverseRegistration, std::vector<int>* applyLog)
			: mNum(numChannels)
			, mScheduler(launcher)
			, mConfig{ "temp/deferredstep_save_tests/", "slot_{id}.sav", 0, SaveFormat::Json }
			, mParticipant(mScheduler)
		{
			_mkdir("temp");
			_mkdir("temp/deferredstep_save_tests");
			for (int i = 0; i < kMax; ++i)
			{
				char name[32];
				snprintf(name, sizeof(name), "EdgeChannel%d", i);
				mChannels[i] = StringCRC(name);
				mSteps[i].Configure(i, 1000 + i, applyLog);
			}
			for (int n = 0; n < numChannels; ++n)
			{
				const int i = reverseRegistration ? (numChannels - 1 - n) : n;
				mScheduler.Register(mChannels[i], mSteps[i]);
			}
			mParticipant.Attach(mRegistry, mManager);
			mManager.Init(mConfig, mRegistry);
		}

		~EdgeWorld()
		{
			mParticipant.Detach();
			for (int i = 0; i < mNum; ++i) { mScheduler.Unregister(mChannels[i]); }
		}

		Dia::DeferredStep::KickResult Kick(int channel, StepKey kickKey, StepKey applyKey)
		{
			return mScheduler.Kick(mChannels[channel], kickKey, applyKey);
		}

		int mNum;
		DeferredStepScheduler mScheduler;
		SaveConfig mConfig;
		SaveRegistry mRegistry;
		SaveManager mManager;
		StringCRC mChannels[kMax];
		EdgeStep mSteps[kMax];
		DeferredStepSaveParticipant mParticipant;
	};
}

class DeferredStepSaveEdge : public ::testing::Test
{
protected:
	std::string SlotName(const char* prefix) const
	{
		return std::string("edge_") + prefix + "_" + ::testing::UnitTest::GetInstance()->current_test_info()->name();
	}

	// Registers a slot for cleanup and returns its CRC id.
	StringCRC Slot(const std::string& name)
	{
		mSlotNames.push_back(name);
		return StringCRC(name.c_str());
	}

	~DeferredStepSaveEdge() override
	{
		for (const std::string& n : mSlotNames) { remove(SlotPath(n).c_str()); }
	}

	static std::string SlotPath(const std::string& name) { return "temp/deferredstep_save_tests/slot_" + name + ".sav"; }

	std::vector<std::string> mSlotNames;
	Testing::ManualStepLauncher mSavedLauncher;
	Testing::ManualStepLauncher mLoadedLauncher;
	std::vector<int> mSavedApplyLog;
	std::vector<int> mLoadedApplyLog;
};

// (a) kMaxChannels pending, kicked in a scrambled order: all round-trip, order preserved.
TEST_F(DeferredStepSaveEdge, EightPendingChannelsRoundTripInKickOrder)
{
	const std::string name = SlotName("a");
	const StringCRC slot = Slot(name);
	const int kickOrder[8] = { 3, 0, 7, 1, 6, 2, 5, 4 };

	EdgeWorld saved(mSavedLauncher, 8, false, &mSavedApplyLog);
	for (int i = 0; i < 8; ++i)
	{
		ASSERT_EQ(saved.Kick(kickOrder[i], 1 + i, 20 + i), KickResult::kLaunched);
	}
	ASSERT_TRUE(saved.mManager.Save(slot).Ok());
	EXPECT_EQ(saved.mScheduler.GetStats().drainedSteps, 8u);

	EdgeWorld loaded(mLoadedLauncher, 8, false, &mLoadedApplyLog);
	ASSERT_TRUE(loaded.mManager.Load(slot).Ok());
	ASSERT_EQ(loaded.mScheduler.GetPendingCount(), 8u);
	for (uint32_t i = 0; i < 8; ++i)
	{
		PendingStepInfo info;
		ASSERT_TRUE(loaded.mScheduler.GetPendingAt(i, info));
		EXPECT_EQ(info.channelId, loaded.mChannels[kickOrder[i]]) << "index " << i;
		EXPECT_EQ(info.kickKey, static_cast<StepKey>(1 + i));
		EXPECT_EQ(info.applyKey, static_cast<StepKey>(20 + i));
		EXPECT_EQ(loaded.mSteps[kickOrder[i]].mPayloadRead, 1000 + kickOrder[i]);
	}
	loaded.mScheduler.Tick(100);
	ASSERT_EQ(mLoadedApplyLog.size(), 8u);
	for (int i = 0; i < 8; ++i) { EXPECT_EQ(mLoadedApplyLog[i], kickOrder[i]); }
	EXPECT_EQ(loaded.mScheduler.GetStats().stalls, 0u);
}

// (b) Channels registered in a different order on load: restored by id; apply order follows the written order.
TEST_F(DeferredStepSaveEdge, DifferentRegistrationOrderOnLoadRestoresByIdAndKeepsWrittenOrder)
{
	const std::string name = SlotName("b");
	const StringCRC slot = Slot(name);

	EdgeWorld saved(mSavedLauncher, 4, false, &mSavedApplyLog);
	// Same apply key so apply order is decided purely by restore (written) order.
	ASSERT_EQ(saved.Kick(2, 1, 10), KickResult::kLaunched);
	ASSERT_EQ(saved.Kick(0, 2, 10), KickResult::kLaunched);
	ASSERT_EQ(saved.Kick(3, 3, 10), KickResult::kLaunched);
	ASSERT_TRUE(saved.mManager.Save(slot).Ok());

	EdgeWorld loaded(mLoadedLauncher, 4, true, &mLoadedApplyLog);
	ASSERT_TRUE(loaded.mManager.Load(slot).Ok());
	EXPECT_EQ(loaded.mSteps[2].mPayloadRead, 1002);
	EXPECT_EQ(loaded.mSteps[0].mPayloadRead, 1000);
	EXPECT_EQ(loaded.mSteps[3].mPayloadRead, 1003);
	EXPECT_EQ(loaded.mSteps[1].mDeserializeCount, 0u);
	EXPECT_EQ(loaded.mScheduler.GetState(loaded.mChannels[1]), StepState::kIdle);

	loaded.mScheduler.Tick(10);
	ASSERT_EQ(mLoadedApplyLog.size(), 3u);
	EXPECT_EQ(mLoadedApplyLog[0], 2);
	EXPECT_EQ(mLoadedApplyLog[1], 0);
	EXPECT_EQ(mLoadedApplyLog[2], 3);
}

// (c) A step whose SerializePending writes nothing: channel object has only the 4 header fields.
TEST_F(DeferredStepSaveEdge, StepWritingNothingRoundTrips)
{
	const std::string name = SlotName("c");
	const StringCRC slot = Slot(name);

	EdgeWorld saved(mSavedLauncher, 2, false, &mSavedApplyLog);
	saved.mSteps[0].mWritesPayload = false;
	ASSERT_EQ(saved.Kick(0, 5, 12), KickResult::kLaunched);
	ASSERT_TRUE(saved.mManager.Save(slot).Ok());

	Json::Value root;
	Json::Reader reader;
	ASSERT_TRUE(reader.parse(ReadFile(SlotPath(name)), root));
	const Json::Value& channel = root["DeferredStep"]["channel_0"];
	ASSERT_TRUE(channel.isObject());
	EXPECT_EQ(channel.size(), 4u);
	EXPECT_TRUE(channel.isMember("channel_id"));
	EXPECT_TRUE(channel.isMember("kick_key"));
	EXPECT_TRUE(channel.isMember("apply_key"));
	EXPECT_TRUE(channel.isMember("step_version"));

	EdgeWorld loaded(mLoadedLauncher, 2, false, &mLoadedApplyLog);
	loaded.mSteps[0].mWritesPayload = false;
	ASSERT_TRUE(loaded.mManager.Load(slot).Ok());
	EXPECT_EQ(loaded.mSteps[0].mDeserializeCount, 1u);
	EXPECT_FALSE(loaded.mSteps[0].mPayloadReadOk);
	EXPECT_EQ(loaded.mScheduler.GetState(loaded.mChannels[0]), StepState::kComplete);
	loaded.mScheduler.Tick(12);
	EXPECT_EQ(loaded.mSteps[0].mApplyCount, 1u);
}

// (d) pending_count vs the objects actually present.
class DeferredStepSaveEdgeCount : public ::testing::Test
{
protected:
	DeferredStepSaveEdgeCount() : mWorld(mLauncher, 3, false, &mApplyLog) {}

	Json::Value Channel(int index, int payloadKey) const
	{
		Json::Value c(Json::objectValue);
		c["channel_id"] = static_cast<Json::Int64>(mWorld.mChannels[index].Value());
		c["kick_key"] = 1 + index;
		c["apply_key"] = 10;
		c["step_version"] = 1;
		c["payload"] = payloadKey;
		return c;
	}

	static Json::Value Section(int count)
	{
		Json::Value s(Json::objectValue);
		s["version"] = 1;
		s["pending_count"] = count;
		return s;
	}

	Testing::ManualStepLauncher mLauncher;
	std::vector<int> mApplyLog;
	EdgeWorld mWorld;
};

TEST_F(DeferredStepSaveEdgeCount, CountLargerThanObjectsFailsValidateAndDeserializeIsTolerant)
{
	Json::Value s = Section(2);
	s["channel_0"] = Channel(0, 50);

	{
		LoadContext ctx(s);
		LogCapture log;
		EXPECT_FALSE(mWorld.mParticipant.Validate(ctx));
		EXPECT_EQ(log.Count(LogLevel::kError, "channel_1"), 1u);
	}
	EXPECT_EQ(mWorld.mScheduler.GetPendingCount(), 0u);

	LoadContext ctx(s);
	LogCapture log;
	mWorld.mParticipant.Deserialize(ctx);
	EXPECT_GE(log.Warnings("channel_1"), 1u);
	EXPECT_EQ(mWorld.mScheduler.GetPendingCount(), 1u);
	EXPECT_EQ(mWorld.mSteps[0].mPayloadRead, 50);
	int32_t version = 0;
	EXPECT_TRUE(ctx.Read(StringCRC("version"), version));   // context balanced
}

TEST_F(DeferredStepSaveEdgeCount, CountSmallerThanObjectsHonorsCountOnly)
{
	Json::Value s = Section(1);
	s["channel_0"] = Channel(0, 50);
	s["channel_1"] = Channel(1, 60);

	{
		LoadContext ctx(s);
		EXPECT_TRUE(mWorld.mParticipant.Validate(ctx));
	}
	LoadContext ctx(s);
	mWorld.mParticipant.Deserialize(ctx);
	EXPECT_EQ(mWorld.mScheduler.GetPendingCount(), 1u);
	EXPECT_EQ(mWorld.mScheduler.GetState(mWorld.mChannels[0]), StepState::kComplete);
	EXPECT_EQ(mWorld.mScheduler.GetState(mWorld.mChannels[1]), StepState::kIdle);
	EXPECT_EQ(mWorld.mSteps[1].mDeserializeCount, 0u);
}

TEST_F(DeferredStepSaveEdgeCount, NegativeCountFailsValidateAndRestoresNothing)
{
	Json::Value s = Section(-1);
	s["channel_0"] = Channel(0, 50);

	{
		LoadContext ctx(s);
		LogCapture log;
		EXPECT_FALSE(mWorld.mParticipant.Validate(ctx));
		EXPECT_EQ(log.Count(LogLevel::kError), 1u);
	}
	LoadContext ctx(s);
	LogCapture log;
	mWorld.mParticipant.Deserialize(ctx);
	EXPECT_GE(log.Warnings(), 1u);
	EXPECT_EQ(mWorld.mScheduler.GetPendingCount(), 0u);
	EXPECT_EQ(mWorld.mSteps[0].mDeserializeCount, 0u);
}

// (e) The game drains explicitly before saving; the save's own drain is then a no-op.
TEST_F(DeferredStepSaveEdge, ExplicitDrainAllBeforeSaveRoundTrips)
{
	const std::string name = SlotName("e");
	const StringCRC slot = Slot(name);

	EdgeWorld saved(mSavedLauncher, 3, false, &mSavedApplyLog);
	ASSERT_EQ(saved.Kick(1, 1, 10), KickResult::kLaunched);
	saved.mScheduler.DrainAll();
	ASSERT_EQ(saved.mScheduler.GetStats().drains, 1u);
	ASSERT_EQ(saved.mScheduler.GetState(saved.mChannels[1]), StepState::kComplete);

	{
		LogCapture log;
		ASSERT_TRUE(saved.mManager.Save(slot).Ok());
		EXPECT_EQ(log.Infos("Save drain: 0 steps drained, 1 pending"), 1u);
	}
	EXPECT_EQ(saved.mScheduler.GetStats().drains, 1u);
	EXPECT_EQ(saved.mScheduler.GetStats().drainedSteps, 1u);

	EdgeWorld loaded(mLoadedLauncher, 3, false, &mLoadedApplyLog);
	ASSERT_TRUE(loaded.mManager.Load(slot).Ok());
	EXPECT_EQ(loaded.mScheduler.GetPendingCount(), 1u);
	EXPECT_EQ(loaded.mSteps[1].mPayloadRead, 1001);
	loaded.mScheduler.Tick(10);
	EXPECT_EQ(loaded.mSteps[1].mApplyCount, 1u);
}

// (f) Two saves in a row produce identical files; the step still applies exactly once.
TEST_F(DeferredStepSaveEdge, SavingTwiceProducesIdenticalFilesAndStepAppliesOnce)
{
	const std::string nameA = SlotName("f1");
	const std::string nameB = SlotName("f2");
	const StringCRC slotA = Slot(nameA);
	const StringCRC slotB = Slot(nameB);

	EdgeWorld saved(mSavedLauncher, 2, false, &mSavedApplyLog);
	ASSERT_EQ(saved.Kick(0, 1, 10), KickResult::kLaunched);
	ASSERT_TRUE(saved.mManager.Save(slotA).Ok());
	ASSERT_TRUE(saved.mManager.Save(slotB).Ok());

	const std::string textA = ReadFile(SlotPath(nameA));
	const std::string textB = ReadFile(SlotPath(nameB));
	ASSERT_FALSE(textA.empty());
	EXPECT_EQ(textA, textB);
	EXPECT_EQ(saved.mScheduler.GetStats().drains, 1u);   // second save found nothing in flight
	EXPECT_EQ(saved.mSteps[0].mRunCount, 1u);

	saved.mScheduler.Tick(10);
	saved.mScheduler.Tick(11);
	EXPECT_EQ(saved.mSteps[0].mApplyCount, 1u);
	EXPECT_EQ(saved.mScheduler.GetStats().applies, 1u);
}

// (g) The loading world has already ticked beyond the stored apply key: it applies at the next Tick, no assert, no stall.
TEST_F(DeferredStepSaveEdge, LoadIntoWorldThatTickedPastApplyKeyAppliesAtNextTick)
{
	const std::string name = SlotName("g");
	const StringCRC slot = Slot(name);

	EdgeWorld saved(mSavedLauncher, 2, false, &mSavedApplyLog);
	ASSERT_EQ(saved.Kick(0, 1, 10), KickResult::kLaunched);
	ASSERT_TRUE(saved.mManager.Save(slot).Ok());

	EdgeWorld loaded(mLoadedLauncher, 2, false, &mLoadedApplyLog);
	loaded.mScheduler.Tick(500);
	ASSERT_TRUE(loaded.mManager.Load(slot).Ok());
	ASSERT_EQ(loaded.mScheduler.GetState(loaded.mChannels[0]), StepState::kComplete);
	EXPECT_EQ(loaded.mSteps[0].mApplyCount, 0u);   // Load itself never applies

	loaded.mScheduler.Tick(500);   // same key is not "backwards"
	EXPECT_EQ(loaded.mSteps[0].mApplyCount, 1u);
	loaded.mScheduler.Tick(501);
	EXPECT_EQ(loaded.mSteps[0].mApplyCount, 1u);
	EXPECT_EQ(loaded.mScheduler.GetStats().stalls, 0u);
	EXPECT_EQ(loaded.mScheduler.GetStats().applies, 1u);
}

TEST_F(DeferredStepSaveEdge, LoadIntoWorldThatTickedPastApplyKeyAppliesAtFollowingTick)
{
	const std::string name = SlotName("g2");
	const StringCRC slot = Slot(name);

	EdgeWorld saved(mSavedLauncher, 2, false, &mSavedApplyLog);
	ASSERT_EQ(saved.Kick(0, 1, 10), KickResult::kLaunched);
	ASSERT_TRUE(saved.mManager.Save(slot).Ok());

	EdgeWorld loaded(mLoadedLauncher, 2, false, &mLoadedApplyLog);
	loaded.mScheduler.Tick(500);
	ASSERT_TRUE(loaded.mManager.Load(slot).Ok());
	loaded.mScheduler.Tick(501);
	EXPECT_EQ(loaded.mSteps[0].mApplyCount, 1u);
	EXPECT_EQ(loaded.mScheduler.GetStats().stalls, 0u);
}

// (h) Kick on a restored, unapplied channel is rejected.
TEST_F(DeferredStepSaveEdge, KickRightAfterLoadIsAlreadyInFlight)
{
	const std::string name = SlotName("h");
	const StringCRC slot = Slot(name);

	EdgeWorld saved(mSavedLauncher, 2, false, &mSavedApplyLog);
	ASSERT_EQ(saved.Kick(0, 1, 10), KickResult::kLaunched);
	ASSERT_TRUE(saved.mManager.Save(slot).Ok());

	EdgeWorld loaded(mLoadedLauncher, 2, false, &mLoadedApplyLog);
	ASSERT_TRUE(loaded.mManager.Load(slot).Ok());
	EXPECT_EQ(loaded.Kick(0, 5, 20), KickResult::kAlreadyInFlight);
	EXPECT_EQ(loaded.mScheduler.GetState(loaded.mChannels[0]), StepState::kComplete);
}

// ---------------------------------------------------------------------------
// Gap-fill: observation (AC 27) and scheduler stats after save.
// ---------------------------------------------------------------------------

TEST_F(DeferredStepSaveEdge, SaveDrainLogsInfoWithDrainedAndPendingCounts)
{
	const std::string name = SlotName("log");
	const StringCRC slot = Slot(name);

	EdgeWorld saved(mSavedLauncher, 4, false, &mSavedApplyLog);
	ASSERT_EQ(saved.Kick(0, 1, 10), KickResult::kLaunched);
	ASSERT_EQ(saved.Kick(2, 2, 11), KickResult::kLaunched);

	LogCapture log;
	ASSERT_TRUE(saved.mManager.Save(slot).Ok());
	EXPECT_EQ(log.Infos("Save drain: 2 steps drained, 2 pending"), 1u);
	EXPECT_EQ(log.Warnings(), 0u);
	EXPECT_EQ(log.Count(LogLevel::kError), 0u);

	EXPECT_EQ(saved.mScheduler.GetStats().drains, 1u);
	EXPECT_EQ(saved.mScheduler.GetStats().drainedSteps, 2u);
	EXPECT_EQ(saved.mScheduler.GetStats().applies, 0u);
}

TEST_F(DeferredStepSaveEdge, SaveWithNothingInFlightLogsZeroDrained)
{
	const std::string name = SlotName("log0");
	const StringCRC slot = Slot(name);

	EdgeWorld saved(mSavedLauncher, 2, false, &mSavedApplyLog);
	LogCapture log;
	ASSERT_TRUE(saved.mManager.Save(slot).Ok());
	EXPECT_EQ(log.Infos("Save drain: 0 steps drained, 0 pending"), 1u);
	EXPECT_EQ(saved.mScheduler.GetStats().drains, 0u);
	EXPECT_EQ(saved.mScheduler.GetStats().drainedSteps, 0u);
}

TEST_F(DeferredStepSaveEdge, DrainAndDiscardAllWithNothingToDiscardIsQuiet)
{
	EdgeWorld world(mSavedLauncher, 3, false, &mSavedApplyLog);
	LogCapture log;
	world.mScheduler.DrainAndDiscardAll();
	EXPECT_EQ(log.Total(), 0u);
}

// RestorePending rejection reasons, each with its Warning.
class DeferredStepSaveEdgeRestore : public ::testing::Test
{
protected:
	DeferredStepSaveEdgeRestore() : mScheduler(mLauncher), mChannel("EdgeRestoreChannel")
	{
		mScheduler.Register(mChannel, mStep);
	}
	~DeferredStepSaveEdgeRestore() override { mScheduler.Unregister(mChannel); }

	Testing::ManualStepLauncher mLauncher;
	DeferredStepScheduler mScheduler;
	Testing::RecordingStep mStep;
	StringCRC mChannel;
};

TEST_F(DeferredStepSaveEdgeRestore, UnknownChannelRejectedWithWarning)
{
	LogCapture log;
	EXPECT_FALSE(mScheduler.RestorePending(StringCRC("EdgeRestoreUnknown"), 1, 2));
	EXPECT_EQ(log.Warnings("not registered"), 1u);
	EXPECT_EQ(mScheduler.GetPendingCount(), 0u);
}

TEST_F(DeferredStepSaveEdgeRestore, RunningChannelRejectedWithWarning)
{
	ASSERT_EQ(mScheduler.Kick(mChannel, 1, 10), KickResult::kLaunched);
	LogCapture log;
	EXPECT_FALSE(mScheduler.RestorePending(mChannel, 2, 11));
	EXPECT_EQ(log.Warnings("not idle"), 1u);
	EXPECT_EQ(mScheduler.GetState(mChannel), StepState::kRunning);
}

TEST_F(DeferredStepSaveEdgeRestore, CompleteChannelRejectedWithWarning)
{
	ASSERT_EQ(mScheduler.Kick(mChannel, 1, 10), KickResult::kLaunched);
	mScheduler.DrainAll();
	LogCapture log;
	EXPECT_FALSE(mScheduler.RestorePending(mChannel, 2, 11));
	EXPECT_EQ(log.Warnings("not idle"), 1u);
	EXPECT_EQ(mScheduler.GetState(mChannel), StepState::kComplete);
}

TEST_F(DeferredStepSaveEdgeRestore, RestoredCompleteChannelRejectsSecondRestore)
{
	ASSERT_TRUE(mScheduler.RestorePending(mChannel, 1, 10));
	LogCapture log;
	EXPECT_FALSE(mScheduler.RestorePending(mChannel, 2, 11));
	EXPECT_EQ(log.Warnings("not idle"), 1u);
	StepKey key = 0;
	ASSERT_TRUE(mScheduler.GetApplyKey(mChannel, key));
	EXPECT_EQ(key, 10u);
}

// ---------------------------------------------------------------------------
// Blob sizing for system ODQ 1: a pending result shaped like A4 Large weather (104 x 64 cells x 4 float layers).
// Measurement, not a performance gate: it prints the saved JSON size and only asserts a generous sanity bound.
// ---------------------------------------------------------------------------

namespace
{
	class WeatherShapedStep final : public IDeferredStep
	{
	public:
		enum class Encoding { NumberedKeys, HexString };

		static constexpr uint32_t kCells = 104u * 64u;
		static constexpr uint32_t kLayers = 4u;

		explicit WeatherShapedStep(Encoding encoding) : mEncoding(encoding)
		{
			mValues.resize(kCells * kLayers);
			uint32_t x = 12345u;
			for (float& v : mValues)
			{
				x = x * 1664525u + 1013904223u;
				v = static_cast<float>(x >> 8) / 65536.0f;   // full-precision floats: deliberately worst case for text size
			}
		}

		void Capture() override {}
		void Run(IParallelExecutor&) override {}
		void Apply() override {}
		uint32_t GetPendingVersion() const override { return 1; }
		void DeserializePending(Dia::SaveGame::LoadContext&) override {}

		void SerializePending(Dia::SaveGame::SaveContext& ctx) const override
		{
			if (mEncoding == Encoding::NumberedKeys)
			{
				char name[24];
				for (uint32_t i = 0; i < mValues.size(); ++i)
				{
					snprintf(name, sizeof(name), "v%u", i);
					ctx.Write(StringCRC(name), mValues[i]);
				}
			}
			else
			{
				std::string hex;
				hex.reserve(mValues.size() * 8);
				for (float v : mValues)
				{
					uint32_t bits;
					memcpy(&bits, &v, sizeof(bits));
					char chunk[9];
					snprintf(chunk, sizeof(chunk), "%08x", bits);
					hex += chunk;
				}
				ctx.Write(StringCRC("layers_hex"), hex.c_str());
			}
		}

	private:
		Encoding mEncoding;
		std::vector<float> mValues;
	};

	size_t MeasureSavedBytes(WeatherShapedStep::Encoding encoding)
	{
		Testing::ManualStepLauncher launcher;
		DeferredStepScheduler scheduler(launcher);
		WeatherShapedStep step(encoding);
		const StringCRC channel("EdgeWeatherBlob");
		scheduler.Register(channel, step);
		scheduler.Kick(channel, 1, 10);
		scheduler.DrainAll();

		DeferredStepSaveParticipant participant(scheduler);
		SaveContext ctx;
		participant.Serialize(ctx);
		const unsigned int bufferSize = 32u * 1024u * 1024u;
		std::unique_ptr<char[]> buffer(new char[bufferSize]);
		size_t bytes = 0;
		if (ctx.Flush(buffer.get(), bufferSize))
		{
			bytes = strlen(buffer.get());
		}
		scheduler.Unregister(channel);
		return bytes;
	}
}

TEST(DeferredStepSaveBlobSize, A4LargeWeatherShapedPendingResult)
{
	const size_t numbered = MeasureSavedBytes(WeatherShapedStep::Encoding::NumberedKeys);
	const size_t hex = MeasureSavedBytes(WeatherShapedStep::Encoding::HexString);
	printf("[ODQ1] A4 Large weather pending blob (6656 cells x 4 float layers): numbered keys = %zu bytes, hex string = %zu bytes\n",
		numbered, hex);
	RecordProperty("NumberedKeysBytes", static_cast<int>(numbered));
	RecordProperty("HexStringBytes", static_cast<int>(hex));
	// Sanity bound only (not a perf gate).
	EXPECT_GT(numbered, 0u);
	EXPECT_GT(hex, 0u);
	EXPECT_LT(hex, 4u * 1024u * 1024u);
	EXPECT_LT(numbered, 4u * 1024u * 1024u);
}

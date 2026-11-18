// Save-and-Load participant skeleton (DFS-005): lifetime, observer dispatch, version.

#include <gtest/gtest.h>
#include <cstdio>
#include <direct.h>
#include <memory>
#include <string>
#include <vector>
#include <DiaCore/Json/external/json/json.h>
#include <DiaDeferredStep/IDeferredStep.h>
#include <DiaSaveGame/ISaveable.h>
#include <DiaSaveGame/SaveContext.h>
#include <DiaDeferredStep/DeferredStepSaveParticipant.h>
#include <DiaDeferredStep/DeferredStepScheduler.h>
#include <DiaDeferredStep/Testing/ManualStepLauncher.h>
#include <DiaDeferredStep/Testing/RecordingStep.h>
#include <DiaSaveGame/SaveConfig.h>
#include <DiaSaveGame/SaveFormat.h>
#include <DiaSaveGame/SaveManager.h>
#include <DiaSaveGame/SaveRegistry.h>

using namespace Dia::DeferredStep;
using namespace Dia::SaveGame;
using Dia::Core::StringCRC;

class DeferredStepSaveLifetime : public ::testing::Test
{
protected:
	DeferredStepSaveLifetime()
		: mScheduler(mLauncher)
		, mStep(1, &mOrder)
		, mChannel("SaveLifetimeChannel")
		, mConfig{ "temp/deferredstep_save_tests/", "slot_{id}.sav", 0, SaveFormat::Json }
	{
		_mkdir("temp");
		_mkdir("temp/deferredstep_save_tests");
		mScheduler.Register(mChannel, mStep);
		mManager.Init(mConfig, mRegistry);
	}

	~DeferredStepSaveLifetime() override
	{
		mParticipant.reset();
		mScheduler.Unregister(mChannel);
		remove("temp/deferredstep_save_tests/slot_lifetime.sav");
	}

	void KickOne()
	{
		ASSERT_EQ(mScheduler.Kick(mChannel, 1, 10), KickResult::kLaunched);
		ASSERT_EQ(mScheduler.GetState(mChannel), StepState::kRunning);
	}

	Testing::ManualStepLauncher mLauncher;
	DeferredStepScheduler mScheduler;
	std::vector<int> mOrder;
	Testing::RecordingStep mStep;
	StringCRC mChannel;
	SaveConfig mConfig;
	SaveRegistry mRegistry;
	SaveManager mManager;
	std::unique_ptr<DeferredStepSaveParticipant> mParticipant{ new DeferredStepSaveParticipant(mScheduler) };
};

TEST_F(DeferredStepSaveLifetime, VersionAndIdAreFixed)
{
	EXPECT_EQ(mParticipant->GetVersion(), 1u);
	EXPECT_EQ(DeferredStepSaveParticipant::kFormatVersion, 1u);
	EXPECT_EQ(DeferredStepSaveParticipant::kParticipantId, StringCRC("DeferredStep"));
}

TEST_F(DeferredStepSaveLifetime, AttachRegistersUnderParticipantId)
{
	ASSERT_EQ(mRegistry.GetParticipantCount(), 0u);
	mParticipant->Attach(mRegistry, mManager);
	ASSERT_EQ(mRegistry.GetParticipantCount(), 1u);
	EXPECT_EQ(mRegistry.GetIdAt(0), StringCRC("DeferredStep"));
	EXPECT_EQ(mRegistry.GetParticipantAt(0), static_cast<ISaveable*>(mParticipant.get()));
}

TEST_F(DeferredStepSaveLifetime, SaveStartedDrainsWithoutApplying)
{
	mParticipant->Attach(mRegistry, mManager);
	KickOne();
	ASSERT_TRUE(mManager.Save(StringCRC("lifetime")).Ok());
	EXPECT_EQ(mScheduler.GetState(mChannel), StepState::kComplete);
	EXPECT_EQ(mStep.GetApplyCount(), 0u);
}

TEST_F(DeferredStepSaveLifetime, LoadStartedNotificationDiscards)
{
	mParticipant->Attach(mRegistry, mManager);
	KickOne();
	mManager.OnLoadStarted().NotifyObservers(static_cast<int>(SaveEvent::LoadStarted));
	EXPECT_EQ(mScheduler.GetState(mChannel), StepState::kIdle);
	EXPECT_EQ(mStep.GetApplyCount(), 0u);
}

TEST_F(DeferredStepSaveLifetime, SaveStartedNotificationDrains)
{
	mParticipant->Attach(mRegistry, mManager);
	KickOne();
	mManager.OnSaveStarted().NotifyObservers(static_cast<int>(SaveEvent::SaveStarted));
	EXPECT_EQ(mScheduler.GetState(mChannel), StepState::kComplete);
	EXPECT_EQ(mStep.GetApplyCount(), 0u);
}

TEST_F(DeferredStepSaveLifetime, OtherSaveEventsAreIgnored)
{
	mParticipant->Attach(mRegistry, mManager);
	KickOne();
	// Even if the other subjects were routed to the participant, the codes themselves are ignored.
	mParticipant->ObserverNotification(nullptr, static_cast<int>(SaveEvent::SaveCompleted));
	mParticipant->ObserverNotification(nullptr, static_cast<int>(SaveEvent::LoadCompleted));
	mParticipant->ObserverNotification(nullptr, static_cast<int>(SaveEvent::MigrationApplied));
	mManager.OnSaveCompleted().NotifyObservers(static_cast<int>(SaveEvent::SaveCompleted));
	mManager.OnLoadCompleted().NotifyObservers(static_cast<int>(SaveEvent::LoadCompleted));
	mManager.OnMigrationApplied().NotifyObservers(static_cast<int>(SaveEvent::MigrationApplied));
	EXPECT_EQ(mScheduler.GetState(mChannel), StepState::kRunning);
	EXPECT_EQ(mStep.GetApplyCount(), 0u);
	EXPECT_EQ(mScheduler.GetStats().drains, 0u);
}

TEST_F(DeferredStepSaveLifetime, DetachUnregistersAndStopsReacting)
{
	mParticipant->Attach(mRegistry, mManager);
	mParticipant->Detach();
	EXPECT_EQ(mRegistry.GetParticipantCount(), 0u);
	KickOne();
	ASSERT_TRUE(mManager.Save(StringCRC("lifetime")).Ok());
	EXPECT_EQ(mScheduler.GetState(mChannel), StepState::kRunning);
	mManager.OnLoadStarted().NotifyObservers(static_cast<int>(SaveEvent::LoadStarted));
	EXPECT_EQ(mScheduler.GetState(mChannel), StepState::kRunning);
}

TEST_F(DeferredStepSaveLifetime, DetachAllowsReattach)
{
	mParticipant->Attach(mRegistry, mManager);
	mParticipant->Detach();
	mParticipant->Attach(mRegistry, mManager);
	EXPECT_EQ(mRegistry.GetParticipantCount(), 1u);
}

TEST_F(DeferredStepSaveLifetime, DestructorDetaches)
{
	mParticipant->Attach(mRegistry, mManager);
	mParticipant.reset();
	EXPECT_EQ(mRegistry.GetParticipantCount(), 0u);
	KickOne();
	ASSERT_TRUE(mManager.Save(StringCRC("lifetime")).Ok());
	EXPECT_EQ(mScheduler.GetState(mChannel), StepState::kRunning);
	mManager.OnLoadStarted().NotifyObservers(static_cast<int>(SaveEvent::LoadStarted));
	EXPECT_EQ(mScheduler.GetState(mChannel), StepState::kRunning);
}

TEST_F(DeferredStepSaveLifetime, DestructorWithoutAttachIsSafe)
{
	mParticipant.reset();
	EXPECT_EQ(mRegistry.GetParticipantCount(), 0u);
}

#ifdef _DEBUG
TEST_F(DeferredStepSaveLifetime, AttachTwiceAsserts)
{
	mParticipant->Attach(mRegistry, mManager);
	EXPECT_DEATH(mParticipant->Attach(mRegistry, mManager), "");
}

TEST_F(DeferredStepSaveLifetime, DetachWithoutAttachAsserts)
{
	EXPECT_DEATH(mParticipant->Detach(), "");
}
#else
TEST_F(DeferredStepSaveLifetime, AttachTwiceIsNoOpInRelease)
{
	mParticipant->Attach(mRegistry, mManager);
	mParticipant->Attach(mRegistry, mManager);
	EXPECT_EQ(mRegistry.GetParticipantCount(), 1u);
}

TEST_F(DeferredStepSaveLifetime, DetachWithoutAttachIsNoOpInRelease)
{
	mParticipant->Detach();
	EXPECT_EQ(mRegistry.GetParticipantCount(), 0u);
}
#endif

// ---------------------------------------------------------------------------
// Serialize (Save and Load task 3)
// ---------------------------------------------------------------------------

namespace
{
	// Step whose SerializePending writes the same key ("payload") in every instance, with a distinct value.
	class PayloadStep final : public IDeferredStep
	{
	public:
		explicit PayloadStep(int32_t payload, uint32_t pendingVersion = 3) : mPayload(payload), mPendingVersion(pendingVersion) {}

		void Capture() override {}
		void Run(IParallelExecutor&) override { ++mRunCount; }
		void Apply() override { ++mApplyCount; }
		uint32_t GetPendingVersion() const override { return mPendingVersion; }
		void SerializePending(Dia::SaveGame::SaveContext& ctx) const override
		{
			++mSerializeCount;
			ctx.Write(StringCRC("payload"), mPayload);
		}
		void DeserializePending(Dia::SaveGame::LoadContext&) override {}

		uint32_t mRunCount = 0;
		uint32_t mApplyCount = 0;
		mutable uint32_t mSerializeCount = 0;

	private:
		int32_t mPayload;
		uint32_t mPendingVersion;
	};

	// Registered after the DeferredStep participant; records what it sees when its own Serialize runs.
	class ProbeSaveable final : public ISaveable
	{
	public:
		ProbeSaveable(const DeferredStepScheduler& scheduler, const Testing::RecordingStep& step, StringCRC channel)
			: mScheduler(scheduler), mStep(step), mChannel(channel) {}

		void Serialize(Dia::SaveGame::SaveContext& ctx) const override
		{
			++mSerializeCalls;
			mRunCountSeen = mStep.GetRunCount();
			mApplyCountSeen = mStep.GetApplyCount();
			mStateSeen = mScheduler.GetState(mChannel);
			ctx.Write(StringCRC("probe"), static_cast<int32_t>(1));
		}
		void Deserialize(Dia::SaveGame::LoadContext&) override {}
		uint32_t GetVersion() const override { return 1; }

		mutable uint32_t mSerializeCalls = 0;
		mutable uint32_t mRunCountSeen = 0;
		mutable uint32_t mApplyCountSeen = 0;
		mutable StepState mStateSeen = StepState::kIdle;

	private:
		const DeferredStepScheduler& mScheduler;
		const Testing::RecordingStep& mStep;
		StringCRC mChannel;
	};
}

class DeferredStepSaveWrite : public ::testing::Test
{
protected:
	DeferredStepSaveWrite()
		: mScheduler(mLauncher)
		, mConfig{ "temp/deferredstep_save_tests/", "slot_{id}.sav", 0, SaveFormat::Json }
		, mChannelA("SaveWriteA")
		, mChannelB("SaveWriteB")
		, mChannelC("SaveWriteC")
		, mStepA(100)
		, mStepB(200, 7)
		, mStepC(300)
	{
		_mkdir("temp");
		_mkdir("temp/deferredstep_save_tests");
		mScheduler.Register(mChannelA, mStepA);
		mScheduler.Register(mChannelB, mStepB);
		mScheduler.Register(mChannelC, mStepC);
		mParticipant.Attach(mRegistry, mManager);
		mManager.Init(mConfig, mRegistry);
	}

	~DeferredStepSaveWrite() override
	{
		mParticipant.Detach();
		mScheduler.Unregister(mChannelA);
		mScheduler.Unregister(mChannelB);
		mScheduler.Unregister(mChannelC);
		remove("temp/deferredstep_save_tests/slot_write.sav");
	}

	// Serializes the participant straight into a root SaveContext and parses the result.
	bool SerializeToJson(Json::Value& out, std::string* text = nullptr)
	{
		SaveContext ctx;
		mParticipant.Serialize(ctx);
		std::unique_ptr<char[]> buffer(new char[1024 * 64]);
		if (!ctx.Flush(buffer.get(), 1024 * 64))
		{
			return false;
		}
		if (text != nullptr)
		{
			*text = buffer.get();
		}
		Json::Reader reader;
		return reader.parse(buffer.get(), out);
	}

	void KickDrained(StringCRC channel, StepKey kickKey, StepKey applyKey)
	{
		ASSERT_EQ(mScheduler.Kick(channel, kickKey, applyKey), KickResult::kLaunched);
	}

	static int64_t CrcOf(const StringCRC& id) { return static_cast<int64_t>(id.Value()); }

	Testing::ManualStepLauncher mLauncher;
	DeferredStepScheduler mScheduler;
	SaveConfig mConfig;
	SaveRegistry mRegistry;
	SaveManager mManager;
	DeferredStepSaveParticipant mParticipant{ mScheduler };
	StringCRC mChannelA, mChannelB, mChannelC;
	PayloadStep mStepA, mStepB, mStepC;
};

TEST_F(DeferredStepSaveWrite, NothingInFlightWritesZeroPendingAndNoChannels)
{
	Json::Value root;
	ASSERT_TRUE(SerializeToJson(root));
	EXPECT_EQ(root["version"].asInt(), 1);
	ASSERT_TRUE(root.isMember("pending_count"));
	EXPECT_EQ(root["pending_count"].asInt(), 0);
	EXPECT_FALSE(root.isMember("channel_0"));
	EXPECT_EQ(mStepA.mSerializeCount, 0u);
	EXPECT_EQ(mStepB.mSerializeCount, 0u);
}

TEST_F(DeferredStepSaveWrite, SaveWithNothingInFlightDoesNotDrain)
{
	const uint64_t drainsBefore = mScheduler.GetStats().drains;
	ASSERT_TRUE(mManager.Save(StringCRC("write")).Ok());
	EXPECT_EQ(mScheduler.GetStats().drains, drainsBefore);
	EXPECT_EQ(mScheduler.GetStats().drainedSteps, 0u);
}

TEST_F(DeferredStepSaveWrite, SaveDrainsBeforeAnyParticipantSerializes)
{
	Testing::RecordingStep probeStep;
	const StringCRC probeChannel("SaveWriteProbe");
	ASSERT_TRUE(mScheduler.Register(probeChannel, probeStep));
	ProbeSaveable probe(mScheduler, probeStep, probeChannel);
	mRegistry.Register(StringCRC("ZProbe"), &probe);

	ASSERT_EQ(mScheduler.Kick(probeChannel, 1, 10), KickResult::kLaunched);
	// Manual launcher: Run has not executed yet; Save's drain (Wait) is what runs it.
	ASSERT_EQ(probeStep.GetRunCount(), 0u);
	ASSERT_TRUE(mManager.Save(StringCRC("write")).Ok());

	EXPECT_EQ(probe.mSerializeCalls, 1u);
	EXPECT_EQ(probe.mRunCountSeen, 1u);
	EXPECT_EQ(probe.mStateSeen, StepState::kComplete);
	EXPECT_EQ(probe.mApplyCountSeen, 0u);

	mRegistry.Unregister(StringCRC("ZProbe"));
	mScheduler.Unregister(probeChannel);
}

TEST_F(DeferredStepSaveWrite, SaveDoesNotApplyAndKeepsApplyKey)
{
	KickDrained(mChannelA, 12, 17);
	ASSERT_TRUE(mManager.Save(StringCRC("write")).Ok());
	EXPECT_EQ(mStepA.mApplyCount, 0u);
	EXPECT_EQ(mScheduler.GetState(mChannelA), StepState::kComplete);
	StepKey applyKey = 0;
	ASSERT_TRUE(mScheduler.GetApplyKey(mChannelA, applyKey));
	EXPECT_EQ(applyKey, 17);
	EXPECT_EQ(mScheduler.GetStats().applies, 0u);
}

TEST_F(DeferredStepSaveWrite, SavedSlotFileContainsDeferredStepSection)
{
	KickDrained(mChannelA, 12, 17);
	ASSERT_TRUE(mManager.Save(StringCRC("write")).Ok());

	FILE* f = fopen("temp/deferredstep_save_tests/slot_write.sav", "rb");
	ASSERT_NE(f, nullptr);
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
	ASSERT_TRUE(reader.parse(text, root));
	ASSERT_TRUE(root.isMember("DeferredStep"));
	const Json::Value& section = root["DeferredStep"];
	EXPECT_EQ(section["version"].asInt(), 1);
	EXPECT_EQ(section["pending_count"].asInt(), 1);
	EXPECT_EQ(section["channel_0"]["channel_id"].asInt64(), CrcOf(mChannelA));
	EXPECT_EQ(section["channel_0"]["payload"].asInt(), 100);
}

TEST_F(DeferredStepSaveWrite, WritesChannelFieldsForOnePendingStep)
{
	KickDrained(mChannelB, 412, 419);
	mScheduler.DrainAll();
	Json::Value root;
	ASSERT_TRUE(SerializeToJson(root));
	EXPECT_EQ(root["version"].asInt(), 1);
	EXPECT_EQ(root["pending_count"].asInt(), 1);
	ASSERT_TRUE(root.isMember("channel_0"));
	EXPECT_FALSE(root.isMember("channel_1"));
	const Json::Value& ch = root["channel_0"];
	EXPECT_EQ(ch["channel_id"].asInt64(), CrcOf(mChannelB));
	EXPECT_EQ(ch["kick_key"].asInt64(), 412);
	EXPECT_EQ(ch["apply_key"].asInt64(), 419);
	EXPECT_EQ(ch["step_version"].asInt(), 7);
	EXPECT_EQ(ch["payload"].asInt(), 200);
}

TEST_F(DeferredStepSaveWrite, ChannelsAreWrittenInKickOrderWithNestedPayloads)
{
	// Registered A, B, C; kicked C, A, B.
	KickDrained(mChannelC, 1, 10);
	KickDrained(mChannelA, 2, 11);
	KickDrained(mChannelB, 3, 12);
	mScheduler.DrainAll();
	Json::Value root;
	ASSERT_TRUE(SerializeToJson(root));
	EXPECT_EQ(root["pending_count"].asInt(), 3);

	EXPECT_EQ(root["channel_0"]["channel_id"].asInt64(), CrcOf(mChannelC));
	EXPECT_EQ(root["channel_0"]["payload"].asInt(), 300);
	EXPECT_EQ(root["channel_0"]["kick_key"].asInt64(), 1);
	EXPECT_EQ(root["channel_0"]["apply_key"].asInt64(), 10);
	EXPECT_EQ(root["channel_0"]["step_version"].asInt(), 3);

	EXPECT_EQ(root["channel_1"]["channel_id"].asInt64(), CrcOf(mChannelA));
	EXPECT_EQ(root["channel_1"]["payload"].asInt(), 100);
	EXPECT_EQ(root["channel_1"]["kick_key"].asInt64(), 2);

	EXPECT_EQ(root["channel_2"]["channel_id"].asInt64(), CrcOf(mChannelB));
	EXPECT_EQ(root["channel_2"]["payload"].asInt(), 200);
	EXPECT_EQ(root["channel_2"]["apply_key"].asInt64(), 12);
	EXPECT_EQ(root["channel_2"]["step_version"].asInt(), 7);

	// Step keys must be nested, never at the section root.
	EXPECT_FALSE(root.isMember("payload"));
}

TEST_F(DeferredStepSaveWrite, SerializePendingCalledExactlyOncePerPendingChannel)
{
	KickDrained(mChannelA, 1, 10);
	KickDrained(mChannelC, 2, 10);
	mScheduler.DrainAll();
	Json::Value root;
	ASSERT_TRUE(SerializeToJson(root));
	EXPECT_EQ(mStepA.mSerializeCount, 1u);
	EXPECT_EQ(mStepB.mSerializeCount, 0u);
	EXPECT_EQ(mStepC.mSerializeCount, 1u);
}

TEST_F(DeferredStepSaveWrite, IdleAndAppliedChannelsWriteNothing)
{
	// A: kicked and applied; B: never kicked; C: pending.
	KickDrained(mChannelA, 1, 5);
	mLauncher.CompleteAll();
	mScheduler.Tick(5);
	ASSERT_EQ(mStepA.mApplyCount, 1u);
	KickDrained(mChannelC, 6, 20);
	mScheduler.DrainAll();

	Json::Value root;
	ASSERT_TRUE(SerializeToJson(root));
	EXPECT_EQ(root["pending_count"].asInt(), 1);
	EXPECT_EQ(root["channel_0"]["channel_id"].asInt64(), CrcOf(mChannelC));
	EXPECT_FALSE(root.isMember("channel_1"));
	EXPECT_EQ(mStepA.mSerializeCount, 0u);
	EXPECT_EQ(mStepB.mSerializeCount, 0u);
}

TEST_F(DeferredStepSaveWrite, SerializeIsIdempotentAndDoesNotMutateScheduler)
{
	KickDrained(mChannelB, 3, 9);
	KickDrained(mChannelA, 4, 9);
	mScheduler.DrainAll();
	const SchedulerStats before = mScheduler.GetStats();

	Json::Value rootA, rootB;
	std::string textA, textB;
	ASSERT_TRUE(SerializeToJson(rootA, &textA));
	ASSERT_TRUE(SerializeToJson(rootB, &textB));
	EXPECT_EQ(textA, textB);

	const SchedulerStats after = mScheduler.GetStats();
	EXPECT_EQ(after.drains, before.drains);
	EXPECT_EQ(after.drainedSteps, before.drainedSteps);
	EXPECT_EQ(after.applies, before.applies);
	EXPECT_EQ(after.kicks, before.kicks);
	EXPECT_EQ(after.inFlight, before.inFlight);
	EXPECT_EQ(mScheduler.GetPendingCount(), 2u);
	EXPECT_EQ(mScheduler.GetState(mChannelA), StepState::kComplete);
	EXPECT_EQ(mScheduler.GetState(mChannelB), StepState::kComplete);
	EXPECT_EQ(mStepA.mApplyCount, 0u);
	EXPECT_EQ(mStepB.mApplyCount, 0u);
}

TEST_F(DeferredStepSaveWrite, SerializeDoesNotDrainRunningStep)
{
	// Serialize alone (no SaveStarted) must not wait: a running channel is not pending, so it writes nothing.
	KickDrained(mChannelA, 1, 10);
	Json::Value root;
	ASSERT_TRUE(SerializeToJson(root));
	EXPECT_EQ(root["pending_count"].asInt(), 0);
	EXPECT_EQ(mScheduler.GetState(mChannelA), StepState::kRunning);
	EXPECT_EQ(mScheduler.GetStats().drains, 0u);
}

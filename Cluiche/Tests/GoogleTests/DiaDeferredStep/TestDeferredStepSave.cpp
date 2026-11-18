// Save-and-Load participant skeleton (DFS-005): lifetime, observer dispatch, version.

#include <gtest/gtest.h>
#include <cstdio>
#include <direct.h>
#include <vector>
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

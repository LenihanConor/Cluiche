// DeferredStepModule: registration metadata (AC 1), JobSystemService reader connection (AC 2),
// start/stop lifecycle and scheduler ownership (AC 3-6, 16, 34).
// AC 7 (code-review check): DeferredStepModule.cpp/.h add no singleton and no file static beyond kTypeId.
#include "DeferredStepModuleHarness.h"
#include <gtest/gtest.h>
#include <DiaDeferredStep/DeferredStepModule.h>
#include <DiaDeferredStep/DeferredStepScheduler.h>
#include <DiaDeferredStep/IStepKeySource.h>
#include <DiaDeferredStep/JobStepLauncher.h>
#include <DiaDeferredStep/Testing/LogCapture.h>
#include <DiaDeferredStep/Testing/RecordingStep.h>
#include <DiaSaveGame/ISaveable.h>
#include <DiaSaveGame/SaveConfig.h>
#include <DiaSaveGame/SaveFormat.h>
#include <DiaSaveGame/SaveManager.h>
#include <DiaSaveGame/SaveRegistry.h>
#include <atomic>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaApplicationFlow/SimModule.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <vector>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaStreams/ServiceStreamWriter.h>
#include <DiaThreading/JobSystem.h>
#include <DiaCore/CRC/StringCRC.h>
#include <chrono>
#include <cstring>
#include <thread>
#include <memory>
#include <string>

using namespace Dia::ApplicationFlow;
using Dia::Core::StringCRC;
using Dia::DeferredStep::DeferredStepModule;


TEST(DeferredStepModule, RegisteredWithGlobalTypeRegistry)
{
	const TypeRegistry& reg = TypeRegistry::Global();
	EXPECT_TRUE(reg.Contains(StringCRC("DeferredStepModule")));
	EXPECT_EQ(DeferredStepModule::kTypeId, StringCRC("DeferredStepModule"));
}

TEST(DeferredStepModule, AllowedOnlyOnSimPU)
{
	EXPECT_EQ(DeferredStepModule::kAllowedPUs, PUAffinity::kSim);
	EXPECT_EQ(TypeRegistry::Global().GetAllowedPUs(DeferredStepModule::kTypeId), PUAffinity::kSim);
}

TEST(DeferredStepModule, HasDescription)
{
	const char* desc = TypeRegistry::Global().GetDescription(DeferredStepModule::kTypeId);
	ASSERT_NE(desc, nullptr);
	EXPECT_GT(std::strlen(desc), 0u);
	EXPECT_NE(DeferredStepModule::kDescription, nullptr);
}

TEST(DeferredStepModule, FactoryCreatesModuleWithTypeId)
{
	std::unique_ptr<Module> m(TypeRegistry::Global().Create(DeferredStepModule::kTypeId, StringCRC("inst")));
	ASSERT_NE(m, nullptr);
	
}

TEST(DeferredStepModule, ConnectsJobSystemServiceReader)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	EXPECT_TRUE(g_probe->ReaderAvailable());
}

namespace
{
	const StringCRC kChannelA("DSM_ChannelA");
}

TEST(DeferredStepModule, StartWithJobSystemExposesUsableScheduler)
{
	Dia::DeferredStep::Testing::LogCapture logs;
	Harness h;
	ASSERT_TRUE(h.Start());
	ASSERT_TRUE(g_probe->ReaderAvailable());

	Dia::DeferredStep::DeferredStepScheduler& sched = g_probe->GetScheduler();
	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(sched.Register(kChannelA, step));
	EXPECT_EQ(sched.Kick(kChannelA, 1, 2), Dia::DeferredStep::KickResult::kLaunched);
	sched.Tick(2);
	EXPECT_EQ(step.GetCaptureCount(), 1u);
	EXPECT_EQ(step.GetRunCount(), 1u);
	EXPECT_EQ(step.GetApplyCount(), 1u);
	sched.Unregister(kChannelA);

	EXPECT_EQ(logs.Warnings("JobSystemService"), 0u);
	EXPECT_GE(logs.Infos("cap"), 1u);   // start Info carries the resolved cap
}

TEST(DeferredStepModule, StartWithoutJobSystemWarnsOnceAndRunsInline)
{
	Dia::DeferredStep::Testing::LogCapture logs;
	Harness h(false);
	ASSERT_TRUE(h.Start());
	EXPECT_FALSE(g_probe->ReaderAvailable());
	EXPECT_EQ(logs.Warnings("JobSystemService"), 1u);

	Dia::DeferredStep::DeferredStepScheduler& sched = g_probe->GetScheduler();
	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(sched.Register(kChannelA, step));
	EXPECT_EQ(sched.Kick(kChannelA, 1, 2), Dia::DeferredStep::KickResult::kLaunched);
	EXPECT_EQ(step.GetRunCount(), 1u);                       // ran synchronously inside Kick
	EXPECT_EQ(step.GetRunThread(), std::this_thread::get_id());
	sched.Tick(2);
	EXPECT_EQ(step.GetApplyCount(), 1u);
	sched.Unregister(kChannelA);
	EXPECT_EQ(logs.Warnings() - logs.Warnings("key source"), 1u);                          // nothing further per step; the idle-key-source warning is not part of this check
}

TEST(DeferredStepModule, StopDrainsInFlightAndSubmitsNothingAfterwards)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	Dia::DeferredStep::DeferredStepScheduler& sched = g_probe->GetScheduler();

	std::atomic<bool> release{ false };
	Dia::DeferredStep::Testing::RecordingStep step;
	step.SetOnRun([&release]() { while (!release.load()) { std::this_thread::yield(); } });
	ASSERT_TRUE(sched.Register(kChannelA, step));
	EXPECT_EQ(sched.Kick(kChannelA, 1, 100), Dia::DeferredStep::KickResult::kLaunched);
	release.store(true);
	sched.Unregister(kChannelA);   // consumers unregister (drain + discard) before the module stops
	EXPECT_EQ(step.GetApplyCount(), 0u);

	EXPECT_EQ(g_probe->CallStop(), StopResult::kDone);
	const uint64_t submittedAtStop = g_provider->mJobSystem.GetSubmittedCount();
	for (int i = 0; i < 5; ++i) h.mApp->Update(kDt);
	EXPECT_EQ(g_provider->mJobSystem.GetSubmittedCount(), submittedAtStop);
	EXPECT_EQ(step.GetRunCount(), 1u);
	EXPECT_EQ(step.GetApplyCount(), 0u);
	EXPECT_EQ(g_probe->CallStart(), StartResult::kReady);   // leave started for harness teardown
}

TEST(DeferredStepModule, StopLogsLifetimeCounts)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	Dia::DeferredStep::Testing::LogCapture logs;
	g_probe->CallStop();
	EXPECT_GE(logs.Infos("kicks"), 1u);
	g_probe->CallStart();
}

TEST(DeferredStepModule, DoubleStartStopCycleWorks)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	for (int cycle = 0; cycle < 3; ++cycle)
	{
		EXPECT_EQ(g_probe->CallStop(), StopResult::kDone);
		EXPECT_EQ(g_probe->CallStart(), StartResult::kReady);
		Dia::DeferredStep::DeferredStepScheduler& sched = g_probe->GetScheduler();
		EXPECT_EQ(sched.GetChannelCount(), 0u);
		Dia::DeferredStep::Testing::RecordingStep step;
		ASSERT_TRUE(sched.Register(kChannelA, step));
		EXPECT_EQ(sched.Kick(kChannelA, 1, 1), Dia::DeferredStep::KickResult::kLaunched);
		sched.Tick(1);
		EXPECT_EQ(step.GetApplyCount(), 1u);
		sched.Unregister(kChannelA);
	}
}

#ifdef _DEBUG
TEST(DeferredStepModule, StopWithChannelStillRegisteredAsserts)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	Dia::DeferredStep::Testing::RecordingStep step;
	EXPECT_DEATH(
		{
			g_probe->GetScheduler().Register(kChannelA, step);
			g_probe->CallStop();
		}, "");
}
#endif

namespace
{
	struct ConfigCase
	{
		const char* name;
		const char* json;
		unsigned    workers;
		uint32_t    expectedCap;
		uint32_t    expectedRanges;
		unsigned    expectedWarnings;
	};
}

TEST(DeferredStepModule, ConfigTable)
{
	const ConfigCase cases[] =
	{
		{ "absent",              "",                                                  8, 2, 4, 0 },
		{ "auto",                "{\"worker_cap\":\"auto\"}",                          8, 2, 4, 0 },
		{ "three on eight",      "{\"worker_cap\":3}",                                8, 3, 4, 0 },
		{ "clamped",             "{\"worker_cap\":64}",                               8, 8, 4, 1 },
		{ "zero",                "{\"worker_cap\":0}",                                8, 2, 4, 1 },
		{ "negative",            "{\"worker_cap\":-2}",                               8, 2, 4, 1 },
		{ "banana",              "{\"worker_cap\":\"banana\"}",                        8, 2, 4, 1 },
		{ "float",               "{\"worker_cap\":2.5}",                              8, 2, 4, 1 },
		{ "bool",                "{\"worker_cap\":true}",                             8, 2, 4, 1 },
		{ "object",              "{\"worker_cap\":{}}",                               8, 2, 4, 1 },
		{ "array",               "{\"worker_cap\":[1]}",                              8, 2, 4, 1 },
		{ "ranges absent",       "{\"worker_cap\":3}",                                8, 3, 4, 0 },
		{ "ranges zero",         "{\"ranges_per_worker\":0}",                         8, 2, 1, 1 },
		{ "ranges negative",     "{\"ranges_per_worker\":-5}",                        8, 2, 1, 1 },
		{ "ranges eight",        "{\"ranges_per_worker\":8}",                         8, 2, 8, 0 },
		{ "both bad",            "{\"worker_cap\":\"banana\",\"ranges_per_worker\":0}", 8, 2, 1, 2 },
		{ "malformed json",      "{not json",                                         8, 2, 4, 1 },
	};

	for (const ConfigCase& c : cases)
	{
		SCOPED_TRACE(c.name);
		Dia::DeferredStep::Testing::LogCapture logs;
		Harness h(true, c.json, c.workers);
		ASSERT_TRUE(h.Start());
		EXPECT_EQ(g_probe->ResolvedCap(), c.expectedCap);
		EXPECT_EQ(g_probe->ParsedConfig().rangesPerWorker, c.expectedRanges);
		EXPECT_EQ(logs.Warnings() - logs.Warnings("key source"), c.expectedWarnings);
		EXPECT_EQ(logs.Count(Dia::DeferredStep::Testing::LogCapture::Level::kError), 0u);
	}
}

// ---- Key source ticking (AC 12-16) ----
namespace
{
	using Dia::DeferredStep::StepKey;

	// Returns a settable key and counts how often the module read it (one read per sim step).
	struct CountingKeySource : Dia::DeferredStep::IStepKeySource
	{
		StepKey mKey = 0;
		mutable unsigned mReads = 0;
		StepKey GetCurrentKey() const override { ++mReads; return mKey; }
	};

	// Returns the next scripted key on every read (models catch-up sim steps within one real frame).
	struct ScriptedKeySource : Dia::DeferredStep::IStepKeySource
	{
		std::vector<StepKey> mKeys;
		mutable size_t mNext = 0;
		StepKey GetCurrentKey() const override { return mKeys[mNext < mKeys.size() ? mNext++ : mKeys.size() - 1]; }
	};
}

TEST(DeferredStepModule, UpdateWithKeySourceTicksOncePerStepWithSourceKey)
{
	CountingKeySource source;
	Harness h;
	ASSERT_TRUE(h.Start());
	g_probe->SetKeySource(source);
	Dia::DeferredStep::DeferredStepScheduler& sched = g_probe->GetScheduler();
	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(sched.Register(kChannelA, step));
	EXPECT_EQ(sched.Kick(kChannelA, 0, 3), Dia::DeferredStep::KickResult::kLaunched);

	source.mReads = 0;
	for (StepKey k = 1; k <= 2; ++k) { source.mKey = k; g_probe->CallUpdate(); }
	EXPECT_EQ(step.GetApplyCount(), 0u);          // key 3 not reached yet
	source.mKey = 3;
	g_probe->CallUpdate();
	EXPECT_EQ(step.GetApplyCount(), 1u);          // applies exactly when the source reaches applyKey
	EXPECT_EQ(source.mReads, 3u);                 // exactly one read (one Tick) per update
	sched.Unregister(kChannelA);
}

TEST(DeferredStepModule, UpdateWithoutKeySourceNeverTicksAndWarnsOnce)
{
	Dia::DeferredStep::Testing::LogCapture logs;
	Harness h;
	ASSERT_TRUE(h.Start());
	Dia::DeferredStep::DeferredStepScheduler& sched = g_probe->GetScheduler();
	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(sched.Register(kChannelA, step));
	EXPECT_EQ(sched.Kick(kChannelA, 0, 1), Dia::DeferredStep::KickResult::kLaunched);

	for (int i = 0; i < 100; ++i) g_probe->CallUpdate();
	EXPECT_EQ(step.GetApplyCount(), 0u);
	EXPECT_EQ(logs.Warnings("key source"), 1u);
	sched.Unregister(kChannelA);
}

TEST(DeferredStepModule, KeySourceSetAfterUpdatesWarnsOnceThenTicks)
{
	Dia::DeferredStep::Testing::LogCapture logs;
	CountingKeySource source;
	Harness h;
	ASSERT_TRUE(h.Start());
	Dia::DeferredStep::DeferredStepScheduler& sched = g_probe->GetScheduler();
	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(sched.Register(kChannelA, step));
	EXPECT_EQ(sched.Kick(kChannelA, 0, 1), Dia::DeferredStep::KickResult::kLaunched);

	for (int i = 0; i < 5; ++i) g_probe->CallUpdate();
	EXPECT_EQ(step.GetApplyCount(), 0u);
	EXPECT_EQ(logs.Warnings("key source"), 1u);

	source.mKey = 1;
	g_probe->SetKeySource(source);
	g_probe->CallUpdate();
	EXPECT_EQ(step.GetApplyCount(), 1u);
	for (int i = 0; i < 5; ++i) g_probe->CallUpdate();
	EXPECT_EQ(logs.Warnings("key source"), 1u);
	sched.Unregister(kChannelA);
}

TEST(DeferredStepModule, UnchangedKeyAppliesDueStepOnce)
{
	CountingKeySource source;
	Harness h;
	ASSERT_TRUE(h.Start());
	g_probe->SetKeySource(source);
	Dia::DeferredStep::DeferredStepScheduler& sched = g_probe->GetScheduler();
	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(sched.Register(kChannelA, step));
	EXPECT_EQ(sched.Kick(kChannelA, 0, 2), Dia::DeferredStep::KickResult::kLaunched);

	source.mKey = 2;
	source.mReads = 0;
	for (int i = 0; i < 50; ++i) g_probe->CallUpdate();
	EXPECT_EQ(source.mReads, 50u);
	EXPECT_EQ(step.GetApplyCount(), 1u);
	sched.Unregister(kChannelA);
}

// AC 15: several sim steps in one real frame. The accumulator loop is not reproducible from a test PU without
// the DiaSimTime clock module, so the catch-up frame is modelled by calling DoUpdate back to back with a key
// that advances between the calls (what the clock module does during catch-up).
TEST(DeferredStepModule, CatchUpStepsEachTickAndApplyInKeyOrder)
{
	ScriptedKeySource source;
	source.mKeys = { 1, 2, 3, 4 };
	Harness h;
	ASSERT_TRUE(h.Start());
	g_probe->SetKeySource(source);
	Dia::DeferredStep::DeferredStepScheduler& sched = g_probe->GetScheduler();

	const StringCRC chA("DSM_CatchA"), chB("DSM_CatchB"), chC("DSM_CatchC");
	std::vector<int> order;
	Dia::DeferredStep::Testing::RecordingStep stepA(1, &order), stepB(2, &order), stepC(3, &order);
	ASSERT_TRUE(sched.Register(chA, stepA));
	ASSERT_TRUE(sched.Register(chB, stepB));
	ASSERT_TRUE(sched.Register(chC, stepC));
	// Kicked out of order on purpose: apply order must follow keys, not kick order.
	EXPECT_EQ(sched.Kick(chB, 0, 2), Dia::DeferredStep::KickResult::kLaunched);
	EXPECT_EQ(sched.Kick(chC, 0, 3), Dia::DeferredStep::KickResult::kLaunched);
	EXPECT_EQ(sched.Kick(chA, 0, 1), Dia::DeferredStep::KickResult::kLaunched);

	for (int i = 0; i < 4; ++i) g_probe->CallUpdate();   // one "frame" of 4 catch-up sim steps
	ASSERT_EQ(order.size(), 3u);
	EXPECT_EQ(order[0], 1);
	EXPECT_EQ(order[1], 2);
	EXPECT_EQ(order[2], 3);
	sched.Unregister(chA);
	sched.Unregister(chB);
	sched.Unregister(chC);
}

TEST(DeferredStepModule, SiblingModuleUsesSchedulerViaModuleRef)
{
	g_siblingResult = DSM_SiblingResult();
	g_withSibling = true;
	Harness h;
	g_withSibling = false;
	ASSERT_TRUE(h.Start());
	EXPECT_TRUE(g_siblingResult.done);
	EXPECT_TRUE(g_siblingResult.registered);
	EXPECT_EQ(g_siblingResult.kick, Dia::DeferredStep::KickResult::kLaunched);
	EXPECT_EQ(g_siblingResult.applyCount, 1u);
	EXPECT_EQ(g_probe->GetScheduler().GetChannelCount(), 0u);
}

// ---- Save services injection and lazy attach (AC 17, 18, 5) ----
namespace
{
	using Dia::SaveGame::SaveRegistry;
	using Dia::SaveGame::SaveManager;
	using Dia::SaveGame::SaveConfig;
	using Dia::SaveGame::SaveFormat;

	// Registry and manager must outlive the Harness: its teardown runs DoStop, which detaches from them.
	struct SaveServices
	{
		SaveRegistry mRegistry;
		SaveManager  mManager;
		SaveConfig   mConfig{ "temp/deferredstep_module_tests/", "slot_{id}.sav", 0, SaveFormat::Json };
		SaveServices() { mManager.Init(mConfig, mRegistry); }
	};

	struct NullSaveable : Dia::SaveGame::ISaveable
	{
		void Serialize(Dia::SaveGame::SaveContext&) const override {}
		void Deserialize(Dia::SaveGame::LoadContext&) override {}
		uint32_t GetVersion() const override { return 1; }
	};

	bool HasSection(const SaveRegistry& reg)
	{
		for (uint32_t i = 0; i < reg.GetParticipantCount(); ++i)
		{
			if (reg.GetIdAt(i) == StringCRC("DeferredStep")) { return true; }
		}
		return false;
	}

	// Restarts the (already running) probe with save services injected, so the first-update latch is fresh.
	void RestartWithServices(SaveServices& s)
	{
		g_probe->CallStop();
		g_probe->SetSaveServices(s.mRegistry, s.mManager);
		g_probe->CallStart();
	}
}

TEST(DeferredStepModule, SaveServicesAttachOnFirstUpdateOnly)
{
	SaveServices s;
	Harness h;
	ASSERT_TRUE(h.Start());
	RestartWithServices(s);

	EXPECT_FALSE(HasSection(s.mRegistry));          // DoStart does not attach
	g_probe->CallUpdate();
	EXPECT_TRUE(HasSection(s.mRegistry));
	EXPECT_EQ(s.mRegistry.GetParticipantCount(), 1u);
	g_probe->CallUpdate();                          // second update must not attach again (would assert in Debug)
	g_probe->CallUpdate();
	EXPECT_EQ(s.mRegistry.GetParticipantCount(), 1u);
}

TEST(DeferredStepModule, SaveServicesDetachOnStopAndReattachOnRestart)
{
	SaveServices s;
	Harness h;
	ASSERT_TRUE(h.Start());
	RestartWithServices(s);
	g_probe->CallUpdate();
	ASSERT_TRUE(HasSection(s.mRegistry));

	EXPECT_EQ(g_probe->CallStop(), StopResult::kDone);
	EXPECT_FALSE(HasSection(s.mRegistry));

	EXPECT_EQ(g_probe->CallStart(), StartResult::kReady);
	EXPECT_FALSE(HasSection(s.mRegistry));
	g_probe->CallUpdate();
	EXPECT_TRUE(HasSection(s.mRegistry));
	EXPECT_EQ(s.mRegistry.GetParticipantCount(), 1u);
}

TEST(DeferredStepModule, SaveParticipantRegistersAfterOtherModulesParticipants)
{
	SaveServices s;
	NullSaveable other;
	Harness h;
	ASSERT_TRUE(h.Start());
	RestartWithServices(s);

	s.mRegistry.Register(StringCRC("DSM_OtherParticipant"), &other);   // another module's DoStart-time registration
	g_probe->CallUpdate();
	ASSERT_EQ(s.mRegistry.GetParticipantCount(), 2u);
	EXPECT_EQ(s.mRegistry.GetIdAt(0), StringCRC("DSM_OtherParticipant"));
	EXPECT_EQ(s.mRegistry.GetIdAt(1), StringCRC("DeferredStep"));
	g_probe->CallStop();
	s.mRegistry.Unregister(StringCRC("DSM_OtherParticipant"));
	g_probe->CallStart();
}

TEST(DeferredStepModule, NoSaveServicesLogsOneInfoAndDoesNotParticipate)
{
	Dia::DeferredStep::Testing::LogCapture logs;
	SaveServices s;   // exists but is never injected
	Harness h;
	ASSERT_TRUE(h.Start());
	for (int i = 0; i < 10; ++i) g_probe->CallUpdate();

	EXPECT_EQ(logs.Infos("save"), 1u);
	EXPECT_FALSE(HasSection(s.mRegistry));
	EXPECT_EQ(g_probe->CallStop(), StopResult::kDone);   // nothing attached, nothing to detach
	EXPECT_EQ(g_probe->CallStart(), StartResult::kReady);
}

// ---- Exhaustive pass: config branches not covered by ConfigTable ----
TEST(DeferredStepModule, ConfigTableRemainingBranches)
{
	const ConfigCase cases[] =
	{
		{ "empty object",           "{}",                                  8, 2, 4, 0 },
		{ "unknown keys ignored",   "{\"foo\":1,\"bar\":\"x\"}",           8, 2, 4, 0 },
		{ "cap one",                "{\"worker_cap\":1}",                  8, 1, 4, 0 },
		{ "cap equals workers",     "{\"worker_cap\":8}",                  8, 8, 4, 0 },
		{ "cap one above workers",  "{\"worker_cap\":9}",                  8, 8, 4, 1 },
		{ "auto on one worker",     "{\"worker_cap\":\"auto\"}",           1, 1, 4, 0 },
		{ "auto on three workers",  "{\"worker_cap\":\"auto\"}",           3, 1, 4, 0 },
		{ "auto on sixteen",        "{}",                                  16, 4, 4, 0 },
		{ "ranges one",             "{\"ranges_per_worker\":1}",           8, 2, 1, 0 },
		{ "ranges string",          "{\"ranges_per_worker\":\"banana\"}",  8, 2, 4, 1 },
		{ "ranges float",           "{\"ranges_per_worker\":2.5}",         8, 2, 4, 1 },
		{ "ranges bool",            "{\"ranges_per_worker\":true}",        8, 2, 4, 1 },
		{ "ranges object",          "{\"ranges_per_worker\":{}}",          8, 2, 4, 1 },
		{ "root is array",          "[1,2]",                               8, 2, 4, 1 },
		{ "root is string",         "\"hello\"",                           8, 2, 4, 1 },
		{ "root is number",         "7",                                   8, 2, 4, 1 },
		{ "bad cap, good ranges",   "{\"worker_cap\":\"x\",\"ranges_per_worker\":2}", 8, 2, 2, 1 },
	};

	for (const ConfigCase& c : cases)
	{
		SCOPED_TRACE(c.name);
		Dia::DeferredStep::Testing::LogCapture logs;
		Harness h(true, c.json, c.workers);
		ASSERT_TRUE(h.Start());
		EXPECT_EQ(g_probe->ResolvedCap(), c.expectedCap);
		EXPECT_EQ(g_probe->ParsedConfig().rangesPerWorker, c.expectedRanges);
		EXPECT_EQ(logs.Warnings() - logs.Warnings("key source"), c.expectedWarnings);
		EXPECT_EQ(logs.Count(Dia::DeferredStep::Testing::LogCapture::Level::kError), 0u);
	}
}

TEST(DeferredStepModule, WorkerCapAboveWorkerCountOnSingleWorkerPoolClampsToOneAndStepsComplete)
{
	Dia::DeferredStep::Testing::LogCapture logs;
	CountingKeySource source;
	Harness h(true, "{\"worker_cap\":4}", 1);
	ASSERT_TRUE(h.Start());
	EXPECT_EQ(g_probe->ResolvedCap(), 1u);
	EXPECT_EQ(logs.Warnings("exceeds worker count"), 1u);
	EXPECT_EQ(logs.Warnings() - logs.Warnings("key source"), 1u);

	g_probe->SetKeySource(source);
	Dia::DeferredStep::DeferredStepScheduler& sched = g_probe->GetScheduler();
	const StringCRC chA("DSM_OneWorkerA"), chB("DSM_OneWorkerB");
	Dia::DeferredStep::Testing::RecordingStep stepA, stepB;
	ASSERT_TRUE(sched.Register(chA, stepA));
	ASSERT_TRUE(sched.Register(chB, stepB));
	EXPECT_EQ(sched.Kick(chA, 1, 3), Dia::DeferredStep::KickResult::kLaunched);
	EXPECT_EQ(sched.Kick(chB, 1, 3), Dia::DeferredStep::KickResult::kLaunched);   // second one queues behind the cap of 1
	for (StepKey k = 1; k <= 3; ++k) { source.mKey = k; g_probe->CallUpdate(); }
	EXPECT_EQ(stepA.GetRunCount(), 1u);
	EXPECT_EQ(stepB.GetRunCount(), 1u);
	EXPECT_EQ(stepA.GetApplyCount(), 1u);
	EXPECT_EQ(stepB.GetApplyCount(), 1u);
	sched.Unregister(chA);
	sched.Unregister(chB);
	EXPECT_EQ(logs.Warnings("exceeds worker count"), 1u);   // not repeated per kick
}

// ---- AC 34: start Info values, nothing per frame, stop Info values ----
TEST(DeferredStepModule, StartInfoCarriesResolvedCapRangesAndWorkerCount)
{
	Dia::DeferredStep::Testing::LogCapture logs;
	Harness h(true, "{\"worker_cap\":3,\"ranges_per_worker\":6}", 8);
	ASSERT_TRUE(h.Start());
	EXPECT_EQ(logs.Infos("cap=3, rangesPerWorker=6, workers=8"), 1u);
}

TEST(DeferredStepModule, NothingLoggedPerFrameInSteadyState)
{
	CountingKeySource source;
	Dia::DeferredStep::Testing::LogCapture logs;
	Harness h;
	ASSERT_TRUE(h.Start());
	g_probe->SetKeySource(source);
	source.mKey = 1;
	g_probe->CallUpdate();     // first update logs the one-time "no save services" Info
	logs.Clear();
	for (int i = 0; i < 100; ++i) { ++source.mKey; g_probe->CallUpdate(); }
	EXPECT_EQ(logs.Total(), 0u);
}

TEST(DeferredStepModule, StopInfoReportsLifetimeCountValues)
{
	CountingKeySource source;
	Harness h;
	ASSERT_TRUE(h.Start());
	g_probe->SetKeySource(source);
	Dia::DeferredStep::DeferredStepScheduler& sched = g_probe->GetScheduler();
	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(sched.Register(kChannelA, step));
	EXPECT_EQ(sched.Kick(kChannelA, 1, 2), Dia::DeferredStep::KickResult::kLaunched);
	source.mKey = 2;
	g_probe->CallUpdate();
	sched.Unregister(kChannelA);

	Dia::DeferredStep::Testing::LogCapture logs;
	g_probe->CallStop();
	EXPECT_EQ(logs.Infos("kicks=1, applies=1"), 1u);
	g_probe->CallStart();
}

// ---- Lifecycle paths ----
TEST(DeferredStepModule, InlineModeKeySourceTicksAndRestartsCleanly)
{
	CountingKeySource source;
	Harness h(false);
	ASSERT_TRUE(h.Start());
	g_probe->SetKeySource(source);
	for (int cycle = 0; cycle < 2; ++cycle)
	{
		Dia::DeferredStep::DeferredStepScheduler& sched = g_probe->GetScheduler();
		Dia::DeferredStep::Testing::RecordingStep step;
		ASSERT_TRUE(sched.Register(kChannelA, step));
		EXPECT_EQ(sched.Kick(kChannelA, 1, 2), Dia::DeferredStep::KickResult::kLaunched);
		source.mKey = 2;
		g_probe->CallUpdate();
		EXPECT_EQ(step.GetApplyCount(), 1u);
		EXPECT_EQ(step.GetRunThread(), std::this_thread::get_id());
		sched.Unregister(kChannelA);
		EXPECT_EQ(g_probe->CallStop(), StopResult::kDone);
		EXPECT_EQ(g_probe->CallStart(), StartResult::kReady);
	}
}

TEST(DeferredStepModule, UpdateAfterStopIsANoOpAndDoesNotReadKeySource)
{
	CountingKeySource source;
	Harness h;
	ASSERT_TRUE(h.Start());
	g_probe->SetKeySource(source);
	g_probe->CallUpdate();
	EXPECT_EQ(source.mReads, 1u);
	g_probe->CallStop();
	for (int i = 0; i < 5; ++i) { g_probe->CallUpdate(); }
	EXPECT_EQ(source.mReads, 1u);
	g_probe->CallStart();
}

TEST(DeferredStepModule, UnregisterWaitsForRunningStepThenStopIsCleanAndNothingApplies)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	Dia::DeferredStep::DeferredStepScheduler& sched = g_probe->GetScheduler();
	std::atomic<bool> release{ false };
	Dia::DeferredStep::Testing::RecordingStep step;
	step.SetOnRun([&release]() { while (!release.load()) { std::this_thread::yield(); } });
	ASSERT_TRUE(sched.Register(kChannelA, step));
	EXPECT_EQ(sched.Kick(kChannelA, 1, 100), Dia::DeferredStep::KickResult::kLaunched);

	std::thread releaser([&release]() { std::this_thread::sleep_for(std::chrono::milliseconds(30)); release.store(true); });
	sched.Unregister(kChannelA);    // drain path: blocks until the worker finishes, never applies
	releaser.join();
	EXPECT_EQ(step.GetRunCount(), 1u);
	EXPECT_EQ(step.GetApplyCount(), 0u);
	EXPECT_EQ(sched.GetChannelCount(), 0u);

	EXPECT_EQ(g_probe->CallStop(), StopResult::kDone);
	EXPECT_EQ(g_probe->CallStart(), StartResult::kReady);
}

TEST(DeferredStepModule, RestartWithChannelsBetweenCyclesStartsFromFreshKeyState)
{
	Dia::DeferredStep::Testing::LogCapture logs;
	CountingKeySource source;
	Harness h;
	ASSERT_TRUE(h.Start());
	g_probe->SetKeySource(source);

	// Keys go DOWN across cycles (1000, 600, 200): a retained key floor or last-tick key would reject the kick or
	// treat the tick as backwards (assert in Debug). Cycle 1 leaves its step unapplied and unregisters it (discard).
	const StepKey bases[] = { 1000, 600, 200 };
	for (int cycle = 0; cycle < 3; ++cycle)
	{
		Dia::DeferredStep::DeferredStepScheduler& sched = g_probe->GetScheduler();
		EXPECT_EQ(sched.GetChannelCount(), 0u);
		Dia::DeferredStep::Testing::RecordingStep step;
		ASSERT_TRUE(sched.Register(kChannelA, step));
		EXPECT_EQ(sched.Kick(kChannelA, bases[cycle], bases[cycle] + 2), Dia::DeferredStep::KickResult::kLaunched);
		if (cycle != 1)
		{
			source.mKey = bases[cycle] + 2;
			g_probe->CallUpdate();
			EXPECT_EQ(step.GetApplyCount(), 1u);
		}
		sched.Unregister(kChannelA);
		if (cycle == 1) { EXPECT_EQ(step.GetApplyCount(), 0u); }
		EXPECT_EQ(g_probe->CallStop(), StopResult::kDone);
		EXPECT_EQ(g_probe->CallStart(), StartResult::kReady);
	}
	EXPECT_EQ(logs.Warnings("backwards"), 0u);
	EXPECT_EQ(logs.Warnings("Kick rejected"), 0u);
}

TEST(DeferredStepModule, KeySourceSurvivesRestartAndNoSourceWarningIsOncePerModuleLifetime)
{
	Dia::DeferredStep::Testing::LogCapture logs;
	CountingKeySource source;
	Harness h;
	ASSERT_TRUE(h.Start());
	for (int i = 0; i < 3; ++i) { g_probe->CallUpdate(); }
	EXPECT_EQ(logs.Warnings("key source"), 1u);
	g_probe->CallStop();
	g_probe->CallStart();
	for (int i = 0; i < 3; ++i) { g_probe->CallUpdate(); }
	EXPECT_EQ(logs.Warnings("key source"), 1u);        // pinned: the one-shot latch is not reset by a restart

	g_probe->SetKeySource(source);
	g_probe->CallStop();
	g_probe->CallStart();
	source.mReads = 0;
	g_probe->CallUpdate();
	EXPECT_EQ(source.mReads, 1u);                       // the injected source is kept across a restart
}

TEST(DeferredStepModule, KeySourceReplacedMidRunReadsOnlyTheNewSourceAndKeepsKeyOrder)
{
	CountingKeySource a, b;
	Harness h;
	ASSERT_TRUE(h.Start());
	g_probe->SetKeySource(a);
	Dia::DeferredStep::DeferredStepScheduler& sched = g_probe->GetScheduler();
	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(sched.Register(kChannelA, step));
	EXPECT_EQ(sched.Kick(kChannelA, 0, 8), Dia::DeferredStep::KickResult::kLaunched);

	a.mKey = 3;
	g_probe->CallUpdate();
	EXPECT_EQ(a.mReads, 1u);
	EXPECT_EQ(step.GetApplyCount(), 0u);

	b.mKey = 10;                         // replacement is ahead of the old source: applies the due step at once
	g_probe->SetKeySource(b);
	g_probe->CallUpdate();
	EXPECT_EQ(a.mReads, 1u);             // old source never read again
	EXPECT_EQ(b.mReads, 1u);
	EXPECT_EQ(step.GetApplyCount(), 1u);

	a.mKey = 99;                         // changing the dropped source has no effect
	g_probe->CallUpdate();
	EXPECT_EQ(a.mReads, 1u);
	EXPECT_EQ(step.GetApplyCount(), 1u);
	sched.Unregister(kChannelA);
}

#ifdef _DEBUG
// The scheduler asserts (after logging its Warning) on a key lower than the previous Tick; a replacement source
// that reports a lower key is the same condition.
TEST(DeferredStepModule, BackwardsKeyAssertsInDebug)
{
	CountingKeySource source;
	Harness h;
	ASSERT_TRUE(h.Start());
	g_probe->SetKeySource(source);
	source.mKey = 5;
	g_probe->CallUpdate();
	source.mKey = 3;
	EXPECT_DEATH(g_probe->CallUpdate(), "");
}

TEST(DeferredStepModule, GetSchedulerOutsideStartStopAsserts)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	g_probe->CallStop();
	EXPECT_DEATH(g_probe->GetScheduler(), "");
	g_probe->CallStart();
}
#else
// Release: a backwards key logs a Warning on every occurrence, applies nothing and does not disturb later ticks.
TEST(DeferredStepModule, BackwardsKeyIsIgnoredWithWarningAndNeverDoubleApplies)
{
	Dia::DeferredStep::Testing::LogCapture logs;
	CountingKeySource source;
	Harness h;
	ASSERT_TRUE(h.Start());
	g_probe->SetKeySource(source);
	Dia::DeferredStep::DeferredStepScheduler& sched = g_probe->GetScheduler();
	Dia::DeferredStep::Testing::RecordingStep step;
	ASSERT_TRUE(sched.Register(kChannelA, step));
	EXPECT_EQ(sched.Kick(kChannelA, 0, 6), Dia::DeferredStep::KickResult::kLaunched);

	source.mKey = 5; g_probe->CallUpdate();
	source.mKey = 3; g_probe->CallUpdate();
	EXPECT_EQ(step.GetApplyCount(), 0u);
	EXPECT_EQ(logs.Warnings("backwards"), 1u);
	source.mKey = 6; g_probe->CallUpdate();
	EXPECT_EQ(step.GetApplyCount(), 1u);
	source.mKey = 2; g_probe->CallUpdate();
	source.mKey = 6; g_probe->CallUpdate();
	EXPECT_EQ(step.GetApplyCount(), 1u);
	EXPECT_EQ(logs.Warnings("backwards"), 2u);
	sched.Unregister(kChannelA);
}
#endif

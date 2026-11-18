// DeferredStepModule: registration metadata (AC 1), JobSystemService reader connection (AC 2),
// start/stop lifecycle and scheduler ownership (AC 3-6, 16, 34).
// AC 7 (code-review check): DeferredStepModule.cpp/.h add no singleton and no file static beyond kTypeId.
#include <gtest/gtest.h>
#include <DiaDeferredStep/DeferredStepModule.h>
#include <DiaDeferredStep/DeferredStepScheduler.h>
#include <DiaDeferredStep/IStepKeySource.h>
#include <DiaDeferredStep/JobStepLauncher.h>
#include <DiaDeferredStep/Testing/LogCapture.h>
#include <DiaDeferredStep/Testing/RecordingStep.h>
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
#include <cstring>
#include <memory>
#include <string>

using namespace Dia::ApplicationFlow;
using Dia::Core::StringCRC;
using Dia::DeferredStep::DeferredStepModule;

namespace
{
	constexpr float kDt = 1.0f / 60.0f;

	// Exposes the (protected) reader so the test can observe the connection.
	struct DSM_Probe : DeferredStepModule
	{
		explicit DSM_Probe(const StringCRC& id) : DeferredStepModule(id) {}
		bool ReaderAvailable() const { return mJobSystemReader.IsAvailable(); }
		// Direct lifecycle access for restart / drain tests (bypasses the framework state machine).
		StartResult CallStart() { return DoStart(); }
		StopResult  CallStop()  { return DoStop(); }
		// One sim step, driven directly (the harness's own updates run before the test gets control).
		void CallUpdate() { Dia::SimTime::SimTimeContext ctx{ Dia::Core::TimeAbsolute::Zero(), Dia::Core::TimeRelative::Zero(), 0, 1.0f, false }; DoUpdate(ctx); }
		// Config parsed by OnConfigure and the cap the launcher resolved in DoStart.
		const Dia::DeferredStep::CapConfig& ParsedConfig() const { return mConfig; }
		uint32_t ResolvedCap() { return static_cast<Dia::DeferredStep::JobStepLauncher*>(mLauncher.Get())->GetCapGate().GetWorkerCap(); }
	};

	// Stands in for JobSystemModule on MainPU: publishes a JobSystem on "JobSystemService".
	unsigned g_providerWorkers = 4;
	std::string g_consumerConfig;

	struct DSM_ProviderModule : Module
	{
		static const StringCRC kTypeId;
		explicit DSM_ProviderModule(const StringCRC& id) : Module(id) {}
		Dia::Threading::JobSystem mJobSystem;
		ServiceStreamWriter<Dia::Threading::JobSystem> mWriter{ this, "JobSystemService" };
		void OnConnectStreams(Application& app) override { mWriter.Connect(app); }
		StartResult DoStart() override { mJobSystem.Initialize(g_providerWorkers); mWriter.Register(mJobSystem); return StartResult::kReady; }
		void DoUpdate(float) override {}
		StopResult DoStop() override { mWriter.Deregister(); mJobSystem.Shutdown(); return StopResult::kDone; }
	};
	const StringCRC DSM_ProviderModule::kTypeId("DSM_ProviderModule");

	// Sibling SimPU module that reaches the scheduler through ModuleRef<DeferredStepModule> (AC 16).
	struct DSM_SiblingResult
	{
		bool registered = false;
		Dia::DeferredStep::KickResult kick = Dia::DeferredStep::KickResult::kLaunched;
		uint32_t applyCount = 0;
		bool done = false;
	};
	DSM_SiblingResult g_siblingResult;
	bool g_withSibling = false;

	struct DSM_SiblingModule : SimModule
	{
		static const StringCRC kTypeId;
		explicit DSM_SiblingModule(const StringCRC& id) : SimModule(id) {}
		ModuleRef<DeferredStepModule> mDeferred{ this, StringCRC("deferredStep") };
		StartResult DoStart() override { return StartResult::kReady; }
		void DoUpdate(const Dia::SimTime::SimTimeContext&) override
		{
			if (g_siblingResult.done) return;
			DeferredStepModule* ds = mDeferred.Get();
			if (ds == nullptr) return;
			Dia::DeferredStep::DeferredStepScheduler& sched = ds->GetScheduler();
			Dia::DeferredStep::Testing::RecordingStep step;
			const StringCRC channel("DSM_SiblingChannel");
			g_siblingResult.registered = sched.Register(channel, step);
			g_siblingResult.kick = sched.Kick(channel, 1, 2);
			sched.Tick(2);
			g_siblingResult.applyCount = step.GetApplyCount();
			sched.Unregister(channel);
			g_siblingResult.done = true;
		}
		StopResult DoStop() override { return StopResult::kDone; }
	};
	const StringCRC DSM_SiblingModule::kTypeId("DSM_SiblingModule");
	Module* CreateSibling(const StringCRC& id) { return new DSM_SiblingModule(id); }

	DSM_Probe* g_probe = nullptr;
	DSM_ProviderModule* g_provider = nullptr;
	Module* CreateProbe(const StringCRC& id) { auto* m = new DSM_Probe(id); g_probe = m; return m; }
	Module* CreateProvider(const StringCRC& id) { auto* m = new DSM_ProviderModule(id); g_provider = m; return m; }

	ModuleDeclaration MakeModule(const StringCRC& typeId, const char* instanceId, const char* role)
	{
		ModuleDeclaration mod;
		mod.instanceId = StringCRC(instanceId);
		mod.typeId = typeId;
		mod.startTimeoutMs = 10000.0f;
		mod.stopTimeoutMs = 5000.0f;
		mod.stages.Add(StringCRC("Boot"));
		mod.stages.Add(StringCRC("Run"));
		ChannelBinding ch;
		ch.id = StringCRC("JobSystemService");
		ch.role = StringCRC(role);
		mod.channels.Add(ch);
			if (std::strcmp(role, "consumes") == 0) { mod.configJson = g_consumerConfig.c_str(); }
		return mod;
	}

	ApplicationManifestV3 BuildManifest(bool withProvider)
	{
		ApplicationManifestV3 m;
		m.version = 3;
		StageDeclaration boot;
		boot.name = StringCRC("Boot");
		m.stages.Add(boot);
		StageDeclaration run;
		run.name = StringCRC("Run");
		m.stages.Add(run);
		m.initialStage = StringCRC("Boot");

		StreamDeclaration svc;
		svc.id = StringCRC("JobSystemService");
		svc.kind = StringCRC("ServiceStream");
		svc.payloadType = StringCRC("JobSystem");
		m.streams.Add(svc);

		ProcessingUnitDeclaration mainPu;
		mainPu.instanceId = StringCRC("MainPU");
		mainPu.frequencyHz = 60.0f;
		mainPu.dedicatedThread = false;
		ModuleDeclaration provider = MakeModule(DSM_ProviderModule::kTypeId, "provider", "provides");
		if (!withProvider)
		{
			// Declared (so the manifest validates) but only active in Run, which the harness never enters:
			// the consumer starts in Boot and finds no JobSystemService.
			provider.stages.RemoveAll();
			provider.stages.Add(StringCRC("Run"));
		}
		mainPu.modules.Add(provider);
		m.processingUnits.Add(mainPu);

		ProcessingUnitDeclaration simPu;
		simPu.instanceId = StringCRC("SimPU");
		simPu.frequencyHz = 60.0f;
		simPu.dedicatedThread = false;
		ModuleDeclaration consumer = MakeModule(DeferredStepModule::kTypeId, "deferredStep", "consumes");
		if (withProvider)
		{
			// Cross-PU dependencies are not allowed; order via stages instead: provider is up in Boot, the consumer
			// only starts once the harness transitions to Run.
			consumer.stages.RemoveAll();
			consumer.stages.Add(StringCRC("Run"));
		}
		else
		{
			consumer.stages.RemoveAll();
			consumer.stages.Add(StringCRC("Boot"));
		}
		simPu.modules.Add(consumer);
		if (g_withSibling)
		{
			ModuleDeclaration sib;
			sib.instanceId = StringCRC("sibling");
			sib.typeId = DSM_SiblingModule::kTypeId;
			sib.startTimeoutMs = 10000.0f;
			sib.stopTimeoutMs = 5000.0f;
			sib.stages.Add(StringCRC("Run"));
			simPu.modules.Add(sib);
		}
		m.processingUnits.Add(simPu);
		return m;
	}

	struct Harness
	{
		TypeRegistry					mReg;
		ApplicationManifestV3			mManifest;
		std::unique_ptr<Application>	mApp;
		bool							mWithProvider = true;

		explicit Harness(bool withProvider = true, const char* configJson = "", unsigned workers = 4)
		{
			g_providerWorkers = workers;
			g_consumerConfig = configJson;
			g_probe = nullptr;
			g_provider = nullptr;
			mWithProvider = withProvider;
			mReg.Register(DeferredStepModule::kTypeId, CreateProbe);
			mReg.Register(DSM_ProviderModule::kTypeId, CreateProvider);
			mReg.Register(DSM_SiblingModule::kTypeId, CreateSibling);
			mManifest = BuildManifest(withProvider);
			mApp.reset(new Application(mManifest, mReg));
		}
		bool Start()
		{
			if (!mApp->Start()) return false;
			for (int i = 0; i < 10; ++i) mApp->Update(kDt);
			if (mWithProvider)
			{
				mApp->TransitionTo(StringCRC("Run"));
				for (int i = 0; i < 20; ++i) mApp->Update(kDt);
			}
			return g_probe != nullptr;
		}
		~Harness()
		{
			if (!mApp) return;
			mApp->RequestShutdown();
			for (int guard = 0; guard < 1000 && mApp->Update(kDt); ++guard) {}
			mApp.reset();
		}
	};
}

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

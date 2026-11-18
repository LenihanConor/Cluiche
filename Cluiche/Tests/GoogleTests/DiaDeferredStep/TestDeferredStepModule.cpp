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
	EXPECT_EQ(logs.Warnings(), 1u);                          // nothing further per step (no stall: inline is already complete)
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
		EXPECT_EQ(logs.Warnings(), c.expectedWarnings);
		EXPECT_EQ(logs.Count(Dia::DeferredStep::Testing::LogCapture::Level::kError), 0u);
	}
}

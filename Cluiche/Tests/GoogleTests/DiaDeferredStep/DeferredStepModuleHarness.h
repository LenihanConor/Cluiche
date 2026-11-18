#pragma once

// Shared harness for the DeferredStepModule test files (Application + provider + DSM_Probe).
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
		Dia::DeferredStep::DeferredStepHealth& HealthReporter() { return mHealth; }
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

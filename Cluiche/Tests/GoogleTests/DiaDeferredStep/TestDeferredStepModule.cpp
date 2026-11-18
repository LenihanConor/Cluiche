// DeferredStepModule skeleton: registration metadata (AC 1) and JobSystemService reader connection (AC 2).
#include <gtest/gtest.h>
#include <DiaDeferredStep/DeferredStepModule.h>
#include <DiaDeferredStep/IStepKeySource.h>
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
	};

	// Stands in for JobSystemModule on MainPU: publishes a JobSystem on "JobSystemService".
	struct DSM_ProviderModule : Module
	{
		static const StringCRC kTypeId;
		explicit DSM_ProviderModule(const StringCRC& id) : Module(id) {}
		Dia::Threading::JobSystem mJobSystem;
		ServiceStreamWriter<Dia::Threading::JobSystem> mWriter{ this, "JobSystemService" };
		void OnConnectStreams(Application& app) override { mWriter.Connect(app); }
		StartResult DoStart() override { mWriter.Register(mJobSystem); return StartResult::kReady; }
		void DoUpdate(float) override {}
		StopResult DoStop() override { mWriter.Deregister(); return StopResult::kDone; }
	};
	const StringCRC DSM_ProviderModule::kTypeId("DSM_ProviderModule");

	DSM_Probe* g_probe = nullptr;
	Module* CreateProbe(const StringCRC& id) { auto* m = new DSM_Probe(id); g_probe = m; return m; }
	Module* CreateProvider(const StringCRC& id) { return new DSM_ProviderModule(id); }

	ModuleDeclaration MakeModule(const StringCRC& typeId, const char* instanceId, const char* role)
	{
		ModuleDeclaration mod;
		mod.instanceId = StringCRC(instanceId);
		mod.typeId = typeId;
		mod.startTimeoutMs = 10000.0f;
		mod.stopTimeoutMs = 5000.0f;
		mod.stages.Add(StringCRC("Boot"));
		ChannelBinding ch;
		ch.id = StringCRC("JobSystemService");
		ch.role = StringCRC(role);
		mod.channels.Add(ch);
		return mod;
	}

	ApplicationManifestV3 BuildManifest()
	{
		ApplicationManifestV3 m;
		m.version = 3;
		StageDeclaration boot;
		boot.name = StringCRC("Boot");
		m.stages.Add(boot);
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
		mainPu.modules.Add(MakeModule(DSM_ProviderModule::kTypeId, "provider", "provides"));
		m.processingUnits.Add(mainPu);

		ProcessingUnitDeclaration simPu;
		simPu.instanceId = StringCRC("SimPU");
		simPu.frequencyHz = 60.0f;
		simPu.dedicatedThread = false;
		simPu.modules.Add(MakeModule(DeferredStepModule::kTypeId, "deferredStep", "consumes"));
		m.processingUnits.Add(simPu);
		return m;
	}

	struct Harness
	{
		TypeRegistry					mReg;
		ApplicationManifestV3			mManifest;
		std::unique_ptr<Application>	mApp;

		Harness()
		{
			g_probe = nullptr;
			mReg.Register(DeferredStepModule::kTypeId, CreateProbe);
			mReg.Register(DSM_ProviderModule::kTypeId, CreateProvider);
			mManifest = BuildManifest();
			mApp.reset(new Application(mManifest, mReg));
		}
		bool Start()
		{
			if (!mApp->Start()) return false;
			for (int i = 0; i < 10; ++i) mApp->Update(kDt);
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

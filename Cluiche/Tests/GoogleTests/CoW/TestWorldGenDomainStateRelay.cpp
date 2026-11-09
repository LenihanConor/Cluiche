// WorldGenDebugDomain attached through DomainStateRelayModule: Main-side adapter shows the card state and forwards commands.
#include <gtest/gtest.h>
#include <Modules/WorldGenerationModule.h>   // first: RandomStream.h must precede windows.h (min/max macros)

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <Modules/DomainStateBridgeModule.h>
#include <Modules/DomainStateRelayModule.h>
#include <Modules/WorldGenDebugDomain.h>

#include <memory>

using namespace Dia::ApplicationFlow;
using Cluiche::AppFlow::DomainStateBridgeModule;
using Cluiche::AppFlow::DomainStateRelayModule;
using Dia::Core::StringCRC;
using Dia::VisualDebugger::IDebugDomain;

namespace
{
	DomainStateRelayModule* gRelay = nullptr;
	DomainStateBridgeModule* gBridge = nullptr;
	Module* MakeRelay(const StringCRC& id) { gRelay = new DomainStateRelayModule(id); return gRelay; }
	Module* MakeBridge(const StringCRC& id) { gBridge = new DomainStateBridgeModule(id); return gBridge; }

	ModuleDeclaration Mod(const StringCRC& typeId, const char* instance)
	{
		ModuleDeclaration m;
		m.instanceId = StringCRC(instance);
		m.typeId = typeId;
		m.startTimeoutMs = 10000.0f;
		m.stopTimeoutMs = 5000.0f;
		m.stages.Add(StringCRC("Boot"));
		return m;
	}

	void AddStream(ApplicationManifestV3& m, const char* id, const char* kind, const char* from, const char* to, unsigned capacity = 0)
	{
		StreamDeclaration s;
		s.id = StringCRC(id);
		s.kind = StringCRC(kind);
		s.fromPU = StringCRC(from);
		s.toPU = StringCRC(to);
		s.capacity = capacity;
		m.streams.Add(s);
	}

	void Bind(ModuleDeclaration& m, const char* id, const char* role)
	{
		ChannelBinding b;
		b.id = StringCRC(id);
		b.role = StringCRC(role);
		m.channels.Add(b);
	}

	struct Harness
	{
		static constexpr float kDt = 0.0334f;	// just over one 30 Hz sim step: one sim tick per Update
		TypeRegistry reg;
		ApplicationManifestV3 manifest;
		std::unique_ptr<Application> app;

		Harness()
		{
			gRelay = nullptr; gBridge = nullptr;
			reg.Register(DomainStateRelayModule::kTypeId, MakeRelay);
			reg.Register(DomainStateBridgeModule::kTypeId, MakeBridge);

			manifest.version = 3;
			StageDeclaration boot;
			boot.name = StringCRC("Boot");
			manifest.stages.Add(boot);
			manifest.initialStage = StringCRC("Boot");

			ModuleDeclaration bridge = Mod(DomainStateBridgeModule::kTypeId, "bridge");
			ModuleDeclaration relay = Mod(DomainStateRelayModule::kTypeId, "relay");
			for (uint32_t i = 0; i < DomainStateBridgeModule::kSlotCount; ++i)
			{
				AddStream(manifest, DomainStateBridgeModule::kSnapshotStreamIds[i], "FrameStream", "SimPU", "MainPU");
				AddStream(manifest, DomainStateBridgeModule::kDemandStreamIds[i], "EventStream", "MainPU", "SimPU", 8);
				AddStream(manifest, DomainStateBridgeModule::kChannelStreamIds[i], "ServiceStream", "MainPU", "SimPU");
				Bind(bridge, DomainStateBridgeModule::kSnapshotStreamIds[i], "reads");
				Bind(bridge, DomainStateBridgeModule::kDemandStreamIds[i], "writes");
				Bind(bridge, DomainStateBridgeModule::kChannelStreamIds[i], "provides");
				Bind(relay, DomainStateBridgeModule::kSnapshotStreamIds[i], "writes");
				Bind(relay, DomainStateBridgeModule::kDemandStreamIds[i], "reads");
				Bind(relay, DomainStateBridgeModule::kChannelStreamIds[i], "consumes");
			}

			ProcessingUnitDeclaration mainPu;
			mainPu.instanceId = StringCRC("MainPU");
			mainPu.frequencyHz = 30.0f;
			mainPu.dedicatedThread = false;
			mainPu.modules.Add(bridge);
			manifest.processingUnits.Add(mainPu);

			ProcessingUnitDeclaration simPu;
			simPu.instanceId = StringCRC("SimPU");
			simPu.frequencyHz = 30.0f;
			simPu.dedicatedThread = false;
			simPu.modules.Add(relay);
			manifest.processingUnits.Add(simPu);

			app.reset(new Application(manifest, reg));
		}

		bool Start()
		{
			if (!app->Start()) return false;
			for (int i = 0; i < 20; ++i) app->Update(kDt);
			return gRelay && gBridge;
		}
		void Pump(int n) { for (int i = 0; i < n; ++i) app->Update(kDt); }
		void Shutdown()
		{
			if (!app) return;
			app->RequestShutdown();
			for (int guard = 0; guard < 1000 && app->Update(kDt); ++guard) {}
			app.reset();
		}
		~Harness() { Shutdown(); }
	};

	bool FetchState(Harness& h, IDebugDomain& adapter, Json::Value& out)
	{
		for (int i = 0; i < 60; ++i)
		{
			Json::Value s;
			adapter.GetJSONState(s);
			if (s.isMember("controls")) { out = s; return true; }
			h.Pump(1);
		}
		return false;
	}
}

TEST(WorldGenDomainStateRelay, AdapterShowsCardStateAndForwardsCommands)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	CoW::WorldGenerationModule module{ StringCRC("WorldGenerationModule") };
	Dia::Maths::RandomStream rng{ 1 };
	module.GetSession().Tick(rng);
	CoW::WorldGenDebugDomain domain{ module };

	IDebugDomain* adapter = gRelay->Attach(domain);
	ASSERT_NE(adapter, nullptr);
	EXPECT_EQ(adapter->GetDomainId(), StringCRC("WorldGen"));

	Json::Value state;
	ASSERT_TRUE(FetchState(h, *adapter, state));
	EXPECT_GE(state["controls"].size(), 19u);
	EXPECT_GE(state["stats"]["revision"].asUInt(), 1u);

	Json::Value args(Json::objectValue);
	args["key"] = "landFraction";
	args["value"] = 0.5;
	adapter->OnCommand(StringCRC("setScale"), args);
	h.Pump(5);
	CoW::WorldGeneration::WorldGenSession ref;
	Json::Value refArgs(Json::objectValue);
	refArgs["landFraction"] = 0.5;
	ref.Set(refArgs);
	EXPECT_EQ(module.GetSession().Params()["params"].toStyledString(), ref.Params()["params"].toStyledString());
	EXPECT_NEAR(module.GetSession().Params()["params"]["landFraction"].asDouble(), 0.5, 1e-6);
	EXPECT_TRUE(module.GetSession().IsDirty());

	gRelay->Detach(domain);
	EXPECT_FALSE(gRelay->IsAttached(domain));
}

#endif // DIA_DEBUG

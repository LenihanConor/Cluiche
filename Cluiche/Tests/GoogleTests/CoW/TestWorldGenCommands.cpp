// cow.worldgen.* terrain-slice commands, hosted by CoW::WorldGenerationModule on the SimPU.
// Real round trip: Main-side shim with a real DebugServer -> SimQueryRequest EventStream -> SimQueryHostModule (SimPU)
// -> handler -> SimQueryReply EventStream -> shim. Harness adapted from TestCellLayersIntegrationStreams.cpp.
#include <Shared/CoWNodeKinds.h>
#include <gtest/gtest.h>
#include <Modules/WorldGen/WorldGenerationModule.h>   // first: RandomStream.h must precede windows.h (min/max macros)

#ifdef DIA_DEBUG

#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaDebugServer/DebugServer.h>
#include <DiaDebugServer/ISimRequestSink.h>
#include <DiaDebugServer/SimHostedTypes.h>
#include <DiaDebugServer/SimQueryHost.h>
#include <DiaDebugServer/Testing/SimHelpTesting.h>
#include <DiaDebugProtocol/DiaDebugProtocol.h>
#include <DiaProtobuf/ProtoStructConverter.h>

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/MainModule.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaStreams/EventStreamReader.h>
#include <DiaStreams/EventStreamWriter.h>
#include <Modules/MessageBusHostModule.h>
#include <Modules/Plumbing/RandomStreamModule.h>
#include <Modules/SimQueryHostModule.h>
#include <DraoiHexWorldTerrain/Testing/TestPlacers.h>
#include <DraoiHexWorldTerrain/Testing/TestGuarantees.h>
#include <Shared/WorldGeneration/CoWTesting/CoWTerrainRulesTestHelpers.h>
#include <Types/SimRegistrationMessage.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>
#include <DraoiHexWorld/Testing/BiomeTestRegistry.h>

namespace Dia
{
	namespace DebugServer
	{
		// Friend of DebugServer. Must stay token-identical to the definition in TestSimHostedDispatch.cpp (ODR).
		struct DebugServerTestAccess
		{
			static void HandleCommand(DebugServer& s, int connId, const dia::debug::DebugMessage& m) { s.HandleCommand(connId, m); }
			static void HandleConnection(DebugServer& s, int connId, bool connected) { s.HandleConnection(connId, connected); }
			static void Sweep(DebugServer& s) { s.SweepSimTimeouts(); }
		};
	}
}

using namespace Dia::ApplicationFlow;
using Cluiche::AppFlow::SimQueryHostModule;
using Dia::Core::StringCRC;
using Dia::DebugServer::DebugServer;
using Dia::DebugServer::DebugServerTestAccess;
using Dia::DebugServer::SimReplyEvent;
using Dia::DebugServer::SimRequestEvent;

namespace
{
	struct Sent { int connId; dia::debug::DebugMessage msg; };
	void CaptureSend(void* ctx, int connId, const dia::debug::DebugMessage& msg) { static_cast<std::vector<Sent>*>(ctx)->push_back(Sent{ connId, msg }); }
	uint64_t gNowMs = 10000;
	uint64_t FakeNow() { return gNowMs; }

	class WgMainShim : public MainModule, private Dia::DebugServer::ISimRequestSink
	{
	public:
		static const StringCRC kTypeId;
		explicit WgMainShim(const StringCRC& id) : MainModule(id)
		{
			server.SetNowMsFunction(&FakeNow);
			server.SetSendHook(&CaptureSend, &sent);
			server.SetSimRequestSink(this);
		}

		DebugServer server;
		std::vector<Sent> sent;
		std::vector<std::string> registered;
		int malformed = 0;

		void Post(int conn, const char* name, const Json::Value& args)
		{
			dia::debug::DebugMessage m;
			m.set_type(dia::debug::MESSAGE_TYPE_COMMAND_REQUEST);
			auto* c = m.mutable_command_request();
			c->set_command(name);
			Dia::Proto::JsonValueToProtoStruct(args, c->mutable_payload());
			DebugServerTestAccess::HandleCommand(server, conn, m);
		}

		void OnConnectStreams(Application& app) override { mReq.Connect(app); mReps.Connect(app); }

	protected:
		StartResult DoStart() override { return StartResult::kReady; }
		StopResult DoStop() override { return StopResult::kDone; }
		void DoUpdate(const Dia::SimTime::MainTimeContext&) override { Drain(); }

	private:
		bool PostRequest(const SimRequestEvent& r) override { return IsSuccess(mReq.Send(r)); }

		void Drain()
		{
			Dia::Core::Containers::DynamicArrayC<Event<SimReplyEvent>, 2> batch;
			for (unsigned b = 0; b < 16 && mReps.HasPending(); ++b)
			{
				batch.RemoveAll();
				mReps.Consume(batch);
				for (unsigned i = 0; i < batch.Size(); ++i)
				{
					const SimReplyEvent& reply = batch[i].payload;
					const Cluiche::AppFlow::SimRegistration reg = Cluiche::AppFlow::ParseSimRegistrationReply(reply);
					switch (reg.op)
					{
					case Cluiche::AppFlow::SimRegistrationOp::NotRegistration: server.CompleteSimReply(reply); break;
					case Cluiche::AppFlow::SimRegistrationOp::Register:
						if (server.RegisterSimHosted(StringCRC(reg.name), reg.kind, Dia::DebugServer::Testing::DefaultSimHelp())) registered.push_back(reg.name);
						break;
					case Cluiche::AppFlow::SimRegistrationOp::Unregister: server.UnregisterSimHosted(StringCRC(reg.name)); break;
					case Cluiche::AppFlow::SimRegistrationOp::Malformed: ++malformed; break;
					}
				}
			}
		}

		EventStreamWriter<SimRequestEvent> mReq{ this, "SimQueryRequest" };
		EventStreamReader<SimReplyEvent> mReps{ this, "SimQueryReply" };
	};
	const StringCRC WgMainShim::kTypeId("WgMainShim");

	// A later SimPU module that registers a site kind in DoStart (as a game stage would). It holds generation while it
	// "loads" for a few frames, so the registration lands after the WorldGenerationModule's own DoStart finished.
	class WgRegistrar : public Dia::ApplicationFlow::SimModule
	{
	public:
		static const StringCRC kTypeId;
		explicit WgRegistrar(const StringCRC& id) : SimModule(id) {}
		CoW::WorldGeneration::Testing::TestPlacer placer{ "Test.Registered", {}, 7 };
		bool registered = false;
		bool unregistered = false;
		int loadingFrames = 3;
	protected:
		StartResult DoStart() override
		{
			CoW::WorldGenerationModule* wg = mWg.Get();
			if (!wg) return StartResult::kLoading;
			if (!mHeld) { wg->HoldGeneration(); mHeld = true; }
			if (loadingFrames-- > 0) return StartResult::kLoading;
			mWgPtr = wg;
			registered = wg->GetSiteRegistry().Register(&placer);
			wg->ReleaseGeneration();
			return StartResult::kReady;
		}
		void DoUpdate(const Dia::SimTime::SimTimeContext&) override {}
		StopResult DoStop() override
		{
			// ModuleRef::Get() is null while the target is also stopping; use the pointer kept from DoStart.
			if (mWgPtr) unregistered = mWgPtr->GetSiteRegistry().Unregister(StringCRC("Test.Registered"));
			return StopResult::kDone;
		}
	private:
		ModuleRef<CoW::WorldGenerationModule> mWg{ this };
		CoW::WorldGenerationModule* mWgPtr = nullptr;
		bool mHeld = false;
	};
	const StringCRC WgRegistrar::kTypeId("WgRegistrar");

	// Occupies one cow.worldgen name before WorldGenerationModule starts, so exactly one of its registrations is
	// rejected as a duplicate (a partial registration).
	class WgSquatter : public Dia::ApplicationFlow::SimModule
	{
	public:
		static const StringCRC kTypeId;
		static constexpr const char* kName = "cow.worldgen.cell";
		explicit WgSquatter(const StringCRC& id) : SimModule(id) {}
		bool squatted = false;
	protected:
		StartResult DoStart() override
		{
			SimQueryHostModule* host = mHost.Get();
			if (!host) return StartResult::kLoading;
			mHostPtr = host;
			squatted = host->RegisterQuery(kName, Dia::DebugServer::SimHelpInfo{ "squatter" },
				[](const Json::Value&) { Json::Value v(Json::objectValue); v["success"] = true; return v; });
			return StartResult::kReady;
		}
		void DoUpdate(const Dia::SimTime::SimTimeContext&) override {}
		StopResult DoStop() override
		{
			if (mHostPtr && squatted) mHostPtr->UnregisterQuery(kName);
			return StopResult::kDone;
		}
	private:
		ModuleRef<SimQueryHostModule> mHost{ this };
		SimQueryHostModule* mHostPtr = nullptr;
	};
	const StringCRC WgSquatter::kTypeId("WgSquatter");

	WgMainShim* gShim = nullptr;
	WgRegistrar* gRegistrar = nullptr;
	SimQueryHostModule* gHost = nullptr;
	CoW::WorldGenerationModule* gWg = nullptr;
	Cluiche::AppFlow::MessageBusHostModule* gBus = nullptr;
	CoW::RandomStreamModule* gRng = nullptr;

	Module* MakeShim(const StringCRC& id) { gShim = new WgMainShim(id); return gShim; }
	Module* MakeHost(const StringCRC& id) { gHost = new SimQueryHostModule(id); return gHost; }
	Module* MakeWg(const StringCRC& id) { gWg = new CoW::WorldGenerationModule(id); gWg->SetRulesSidecarText(CoW::WorldGeneration::TestSidecarText()); gWg->SetBiomesPath(Draoi::HexWorld::Testing::ShippedBiomesPath()); return gWg; }
	Module* MakeBus(const StringCRC& id) { gBus = new Cluiche::AppFlow::MessageBusHostModule(id); return gBus; }
	Module* MakeSquatter(const StringCRC& id) { return new WgSquatter(id); }
	Module* MakeRegistrar(const StringCRC& id) { gRegistrar = new WgRegistrar(id); return gRegistrar; }
	Module* MakeRng(const StringCRC& id) { gRng = new CoW::RandomStreamModule(id); return gRng; }

	ModuleDeclaration Mod(const StringCRC& typeId, const char* instance,
		std::initializer_list<std::pair<const char*, const char*>> channels, std::initializer_list<const char*> deps = {})
	{
		ModuleDeclaration m;
		m.instanceId = StringCRC(instance);
		m.typeId = typeId;
		m.startTimeoutMs = 10000.0f;
		m.stopTimeoutMs = 5000.0f;
		m.stages.Add(StringCRC("Boot"));
		for (const auto& c : channels)
		{
			ChannelBinding b;
			b.id = StringCRC(c.first);
			b.role = StringCRC(c.second);
			m.channels.Add(b);
		}
		for (const char* d : deps) m.dependencies.Add(StringCRC(d));
		return m;
	}

	void AddStream(ApplicationManifestV3& m, const char* id, const char* from, const char* to, unsigned capacity)
	{
		StreamDeclaration s;
		s.id = StringCRC(id);
		s.kind = StringCRC("EventStream");
		s.fromPU = StringCRC(from);
		s.toPU = StringCRC(to);
		s.capacity = capacity;
		m.streams.Add(s);
	}

	std::string FindShippedPresetDir()
	{
		std::error_code ec;
		std::filesystem::path dir = std::filesystem::absolute(std::filesystem::path(__FILE__), ec).parent_path();
		while (!dir.empty())
		{
			const std::filesystem::path candidate = dir / "Cluiche/Assets/CoW/Global/WorldGen/Presets";
			if (std::filesystem::is_directory(candidate, ec)) return candidate.generic_string();
			const std::filesystem::path parent = dir.parent_path();
			if (parent == dir) break;
			dir = parent;
		}
		return std::string();
	}

	Json::Value Obj() { return Json::Value(Json::objectValue); }

	struct Harness
	{
		static constexpr float kDt = 0.0334f;
		TypeRegistry reg;
		ApplicationManifestV3 manifest;
		std::unique_ptr<Application> app;
		std::string config;
		int nextConn = 1;

		// withHost=false leaves SimQueryHostModule out of the SimPU (the module must still start).
		explicit Harness(const std::string& cfg = std::string(), bool withHost = true, bool withRegistrar = false, bool withSquatter = false) : config(cfg)
		{
			gRegistrar = nullptr;
			gShim = nullptr; gHost = nullptr; gWg = nullptr; gBus = nullptr; gRng = nullptr;
			gNowMs = 10000;
			reg.Register(WgMainShim::kTypeId, MakeShim);
			reg.Register(SimQueryHostModule::kTypeId, MakeHost);
			reg.Register(CoW::WorldGenerationModule::kTypeId, MakeWg);
			reg.Register(StringCRC("MessageBusHostModule"), MakeBus);
			reg.Register(StringCRC("RandomStreamModule"), MakeRng);
			reg.Register(WgRegistrar::kTypeId, MakeRegistrar);
			reg.Register(WgSquatter::kTypeId, MakeSquatter);

			manifest.version = 3;
			StageDeclaration boot;
			boot.name = StringCRC("Boot");
			manifest.stages.Add(boot);
			manifest.initialStage = StringCRC("Boot");
			AddStream(manifest, "SimQueryRequest", "MainPU", "SimPU", 16);
			AddStream(manifest, "SimQueryReply", "SimPU", "MainPU", 48);

			ProcessingUnitDeclaration mainPu;
			mainPu.instanceId = StringCRC("MainPU");
			mainPu.frequencyHz = 30.0f;
			mainPu.dedicatedThread = false;
			mainPu.modules.Add(Mod(WgMainShim::kTypeId, "shim", { { "SimQueryRequest", "writes" }, { "SimQueryReply", "reads" } }));
			manifest.processingUnits.Add(mainPu);

			ProcessingUnitDeclaration simPu;
			simPu.instanceId = StringCRC("SimPU");
			simPu.frequencyHz = 30.0f;
			simPu.dedicatedThread = false;
			if (withHost)
				simPu.modules.Add(Mod(SimQueryHostModule::kTypeId, "SimQueryHostModule", { { "SimQueryRequest", "reads" }, { "SimQueryReply", "writes" } }));
			simPu.modules.Add(Mod(StringCRC("MessageBusHostModule"), "MessageBusHostModule", {}));
			simPu.modules.Add(Mod(StringCRC("RandomStreamModule"), "RandomStreamModule", {}));
			if (withHost && withSquatter)
				simPu.modules.Add(Mod(WgSquatter::kTypeId, "WgSquatter", {}, { "SimQueryHostModule" }));
			ModuleDeclaration wg = withHost
				? (withSquatter
					? Mod(CoW::WorldGenerationModule::kTypeId, "WorldGenerationModule", {}, { "MessageBusHostModule", "RandomStreamModule", "SimQueryHostModule", "WgSquatter" })
					: Mod(CoW::WorldGenerationModule::kTypeId, "WorldGenerationModule", {}, { "MessageBusHostModule", "RandomStreamModule", "SimQueryHostModule" }))
				: Mod(CoW::WorldGenerationModule::kTypeId, "WorldGenerationModule", {}, { "MessageBusHostModule", "RandomStreamModule" });
			if (!config.empty()) wg.configJson = config.c_str();
			simPu.modules.Add(wg);
			if (withRegistrar)
				simPu.modules.Add(Mod(WgRegistrar::kTypeId, "WgRegistrar", {}, { "WorldGenerationModule" }));
			manifest.processingUnits.Add(simPu);

			app.reset(new Application(manifest, reg));
		}

		bool Start()
		{
			if (!app->Start()) return false;
			for (int i = 0; i < 30; ++i)
			{
				app->Update(kDt);
				if (gWg && gWg->GetState() == ModuleState::kActive) break;
			}
			for (int i = 0; i < 4; ++i) app->Update(kDt);
			return gShim && gWg && gWg->GetState() == ModuleState::kActive;
		}
		void Pump(int n) { for (int i = 0; i < n; ++i) app->Update(kDt); }
		template <typename Pred>
		bool PumpUntil(Pred p, int limit = 200) { for (int i = 0; i < limit; ++i) { if (p()) return true; app->Update(kDt); } return p(); }
		void Shutdown()
		{
			if (!app) return;
			app->RequestShutdown();
			for (int guard = 0; guard < 1000 && app->Update(kDt); ++guard) {}
			app.reset();
		}
		~Harness() { Shutdown(); }

		// Posts a request and pumps until its reply arrives. Returns the reply payload; transport must succeed.
		Json::Value Call(const char* name, const Json::Value& args = Json::Value(Json::objectValue))
		{
			const size_t before = gShim->sent.size();
			gShim->Post(nextConn++, name, args);
			const bool got = PumpUntil([&] { return gShim->sent.size() > before; });
			EXPECT_TRUE(got) << name << " never answered";
			if (!got) return Json::Value();
			const auto& resp = gShim->sent[before].msg.command_response();
			EXPECT_TRUE(resp.success()) << name << ": " << resp.message();
			return Dia::Proto::ProtoStructToJsonValue(resp.payload());
		}
		uint32_t Rev() { return Call("cow.worldgen.status")["revision"].asUInt(); }
	};

	bool Failed(const Json::Value& r) { return r.isObject() && r.isMember("success") && !r["success"].asBool() && !r["error"].asString().empty(); }
}

TEST(WorldGenCommands, EveryCommandIsRegisteredWithHelpAndAnnouncedToMain)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	EXPECT_TRUE(gWg->CommandsRegistered());
	EXPECT_EQ(gShim->malformed, 0);
	for (const char* n : CoW::WorldGenerationModule::kCommandNames)
	{
		SCOPED_TRACE(n);
		EXPECT_TRUE(gHost->GetHost().Has(StringCRC(n)));
		const Dia::DebugServer::SimHelpStored* help = gHost->GetHost().GetHelp(StringCRC(n));
		ASSERT_NE(help, nullptr);
		EXPECT_GT(std::string(help->description).size(), 0u);
		EXPECT_STREQ(help->category, "worldgen");
		EXPECT_STREQ(help->owner, "WorldGenerationModule");
		bool announced = false;
		for (const std::string& r : gShim->registered) announced = announced || r == n;
		EXPECT_TRUE(announced) << "not announced to the Main DebugServer";
	}
	EXPECT_EQ(gShim->registered.size(), static_cast<size_t>(CoW::WorldGenerationModule::kCommandCount));
}

TEST(WorldGenCommands, QueriesRoundTripWithExpectedShapes)
{
	const std::string presets = FindShippedPresetDir();
	ASSERT_FALSE(presets.empty());
	Harness h("{\"presets_dir\":\"" + presets + "\"}");
	ASSERT_TRUE(h.Start());
	h.Pump(3);

	const Json::Value params = h.Call("cow.worldgen.status");   // shape sliders now live on status (kit task 32a)
	EXPECT_TRUE(params["success"].asBool());
	EXPECT_EQ(params["params"].size(), 20u);

	const Json::Value status = h.Call("cow.worldgen.status");
	EXPECT_TRUE(status["success"].asBool());
	EXPECT_EQ(status["state"].asString(), "Complete");
	EXPECT_EQ(status["revision"].asUInt(), 1u);
	EXPECT_EQ(status["dimensions"]["width"].asInt(), 66);

	const Json::Value list = h.Call("cow.worldgen.presets");
	EXPECT_TRUE(list["success"].asBool());
	ASSERT_TRUE(list["presets"].isArray());
	std::string ids;
	for (const Json::Value& p : list["presets"]) ids += p["id"].asString() + ",";
	for (const char* id : { "continental", "archipelago", "highlands" })
		EXPECT_NE(ids.find(id), std::string::npos) << id << " missing from " << ids;

	const Json::Value layers = h.Call("cow.worldgen.layers");
	EXPECT_TRUE(layers["success"].asBool());
	EXPECT_TRUE(layers["stages"].isArray());

	// Per-layer values are paged so every reply fits the 16 KB sim reply event (Call() fails on "reply too large").
	size_t gathered = 0;
	size_t offset = 0;
	for (int page = 0; page < 20; ++page)
	{
		Json::Value pa(Json::objectValue); pa["layer"] = "elevation"; pa["offset"] = static_cast<Json::UInt>(offset);
		const Json::Value pr = h.Call("cow.worldgen.layers", pa);
		ASSERT_TRUE(pr["success"].asBool()) << pr.toStyledString();
		gathered += pr["values"].size();
		if (pr["nextOffset"].isNull()) break;
		offset = pr["nextOffset"].asUInt();
	}
	EXPECT_EQ(gathered, 2772u);

	Json::Value xy(Json::objectValue); xy["x"] = 3; xy["y"] = 4;
	const Json::Value cell = h.Call("cow.worldgen.cell", xy);
	EXPECT_TRUE(cell["success"].asBool()) << cell.toStyledString();
	Json::Value idx(Json::objectValue); idx["index"] = 4 * 66 + 3;
	const Json::Value cell2 = h.Call("cow.worldgen.cell", idx);
	EXPECT_TRUE(cell2["success"].asBool());
}

TEST(WorldGenCommands, SetChangesTheWorldAndParamsReflectIt)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	const uint32_t before = h.Rev();

	Json::Value a(Json::objectValue); a["rivers"] = 0.9;
	const Json::Value r = h.Call("cow.worldgen.set", a);
	EXPECT_TRUE(r["success"].asBool()) << r.toStyledString();
	h.Pump(3);
	EXPECT_EQ(h.Rev(), before + 1);
	EXPECT_NEAR(h.Call("cow.worldgen.status")["params"]["rivers"].asDouble(), 0.9, 1e-6);
}

TEST(WorldGenCommands, StatusCarriesTheShapeFieldsTheOldParamsQueryReturned)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	const Json::Value st = h.Call("cow.worldgen.status");
	EXPECT_EQ(st["params"].size(), 20u);
	EXPECT_TRUE(st["ranges"].isMember("rivers"));
	EXPECT_TRUE(st["effective"].isMember("unresolvedPairs"));
	EXPECT_TRUE(st["preset"].isString());
	EXPECT_TRUE(st["modified"].isBool());
	EXPECT_TRUE(st.isMember("maxRetryAttempts"));
	// The existing status fields are kept.
	EXPECT_EQ(st["state"].asString(), "Complete");
	EXPECT_TRUE(st["dimensions"].isObject());
}

TEST(WorldGenCommands, ParamsIsTheParameterFileCommandWithGetAndList)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	h.Pump(3);

	Json::Value la(Json::objectValue); la["action"] = "list";
	const Json::Value list = h.Call("cow.worldgen.params", la);
	ASSERT_TRUE(list["success"].asBool()) << list.toStyledString();
	ASSERT_TRUE(list["params"].isArray());
	ASSERT_GT(list["params"].size(), 0u);
	const Json::Value& first = list["params"][0u];
	EXPECT_TRUE(first.isMember("key"));
	EXPECT_TRUE(first.isMember("value"));
	EXPECT_TRUE(first.isMember("default"));
	EXPECT_EQ(first["source"].asString(), "default");
	EXPECT_FALSE(list.isMember("ranges")) << "the old params reply shape is gone";

	Json::Value ga(Json::objectValue); ga["action"] = "get"; ga["key"] = first["key"].asString();
	const Json::Value got = h.Call("cow.worldgen.params", ga);
	EXPECT_TRUE(got["success"].asBool()) << got.toStyledString();
	EXPECT_EQ(got["key"].asString(), first["key"].asString());

	Json::Value bad(Json::objectValue); bad["action"] = "get"; bad["key"] = "no.such.key";
	const Json::Value err = h.Call("cow.worldgen.params", bad);
	EXPECT_FALSE(err["success"].asBool());
}

TEST(WorldGenCommands, ParamsSetIsQueuedAndAppliedAtTheNextRegenerate)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	const uint32_t rev = h.Rev();

	Json::Value la(Json::objectValue); la["action"] = "list";
	const Json::Value list = h.Call("cow.worldgen.params", la);
	ASSERT_GT(list["params"].size(), 0u);
	const Json::Value& first = list["params"][0u];
	const std::string key = first["key"].asString();

	Json::Value sa(Json::objectValue);
	sa["action"] = "set"; sa["key"] = key; sa["value"] = first["default"];
	const Json::Value sr = h.Call("cow.worldgen.params", sa);
	ASSERT_TRUE(sr["success"].asBool()) << sr.toStyledString();
	EXPECT_TRUE(sr["queued"].asBool());
	EXPECT_EQ(sr["effectiveDay"].asUInt(), rev + 1) << "effectiveDay is the next world revision";

	Json::Value ga(Json::objectValue); ga["action"] = "get"; ga["key"] = key;
	h.Pump(2);
	EXPECT_EQ(h.Call("cow.worldgen.params", ga)["source"].asString(), "default") << "not applied before a generate";

	ASSERT_TRUE(h.Call("cow.worldgen.regenerate")["success"].asBool());
	h.Pump(3);
	EXPECT_EQ(h.Rev(), rev + 1);
	EXPECT_EQ(h.Call("cow.worldgen.params", ga)["source"].asString(), "set");
}

TEST(WorldGenCommands, MoistureOffsetSetAppliesAndOutOfRangeIsClamped)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	const uint32_t before = h.Rev();

	Json::Value a(Json::objectValue); a["moistureOffset"] = 0.2;
	const Json::Value r = h.Call("cow.worldgen.set", a);
	EXPECT_TRUE(r["success"].asBool()) << r.toStyledString();
	EXPECT_EQ(r["clamped"].size(), 0u);
	h.Pump(3);
	EXPECT_EQ(h.Rev(), before + 1);
	const Json::Value p = h.Call("cow.worldgen.status");
	EXPECT_NEAR(p["params"]["moistureOffset"].asDouble(), 0.2, 1e-6);
	EXPECT_NEAR(p["ranges"]["moistureOffset"]["max"].asDouble(), 0.3, 1e-6);
	EXPECT_TRUE(p["ranges"].isMember("temperatureOffset"));
	EXPECT_TRUE(p["effective"].isMember("unresolvedPairs"));

	Json::Value b(Json::objectValue); b["moistureOffset"] = 0.9;
	const Json::Value rb = h.Call("cow.worldgen.set", b);
	ASSERT_EQ(rb["clamped"].size(), 1u) << rb.toStyledString();
	EXPECT_EQ(rb["clamped"][0].asString(), "moistureOffset");
	h.Pump(3);
	EXPECT_NEAR(h.Call("cow.worldgen.status")["params"]["moistureOffset"].asDouble(), 0.3, 1e-6);
}

TEST(WorldGenCommands, BiomeLayersTimingsCountsAndCellRoundTrip)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	h.Pump(3);

	const Json::Value summary = h.Call("cow.worldgen.layers");
	ASSERT_EQ(summary["stages"].size(), 15u);
	std::string names;
	for (const Json::Value& st : summary["stages"])
		for (const Json::Value& l : st["layers"]) names += l["name"].asString() + ",";
	for (const char* n : { "temperature", "waterDistance", "airMoisture", "rainOut", "moisture", "biomeScore", "biomePin", "biomeIndex", "fixupChanges" })
		EXPECT_NE(names.find(std::string(n) + ","), std::string::npos) << n;
	for (int s = 12; s <= 14; ++s)
		EXPECT_TRUE(summary["stages"][s].isMember("ms")) << s;
	int sum = 0;
	ASSERT_TRUE(summary["biomeCounts"].isArray());
	for (const Json::Value& e : summary["biomeCounts"]) sum += e["count"].asInt();
	EXPECT_EQ(sum, 66 * 42);

	// "moisture" values equal the generator's layer at every page window.
	const Draoi::HexWorld::TerrainLayers& layers = gWg->GetSession().Generator().GetLayers();
	const float* ref = layers.GetFloat(layers.FindLayer("moisture"));
	ASSERT_NE(ref, nullptr);
	size_t offset = 0;
	size_t gathered = 0;
	for (int page = 0; page < 40; ++page)
	{
		Json::Value pa(Json::objectValue); pa["layer"] = "moisture"; pa["offset"] = static_cast<Json::UInt>(offset);
		const Json::Value pr = h.Call("cow.worldgen.layers", pa);
		ASSERT_TRUE(pr["success"].asBool()) << pr.toStyledString();
		for (Json::ArrayIndex i = 0; i < pr["values"].size(); ++i)
			EXPECT_EQ(static_cast<float>(pr["values"][i].asDouble()), ref[offset + i]) << offset + i;
		gathered += pr["values"].size();
		if (pr["nextOffset"].isNull()) break;
		offset = pr["nextOffset"].asUInt();
	}
	EXPECT_EQ(gathered, static_cast<size_t>(66 * 42));

	Json::Value idx(Json::objectValue); idx["index"] = 20 * 66 + 30;
	const Json::Value cell = h.Call("cow.worldgen.cell", idx);
	ASSERT_TRUE(cell["success"].asBool()) << cell.toStyledString();
	for (const char* k : { "temperature", "moisture", "waterDistance", "biome", "biomeRunnerUp", "biomePin", "fixupChanges" })
		EXPECT_TRUE(cell.isMember(k)) << k;
}

TEST(WorldGenCommands, TwoSetsBackToBackCoalesceIntoOneRevision)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	const uint32_t before = h.Rev();

	Json::Value a(Json::objectValue); a["rivers"] = 0.8;
	Json::Value b(Json::objectValue); b["rivers"] = 0.3;
	const size_t sentBefore = gShim->sent.size();
	gShim->Post(90, "cow.worldgen.set", a);
	gShim->Post(91, "cow.worldgen.set", b);
	ASSERT_TRUE(h.PumpUntil([&] { return gShim->sent.size() >= sentBefore + 2; }));
	h.Pump(5);
	EXPECT_EQ(h.Rev(), before + 1);
}

TEST(WorldGenCommands, SetSizeStandardResizesAfterPumping)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	Json::Value a(Json::objectValue); a["size"] = "Standard";
	EXPECT_TRUE(h.Call("cow.worldgen.set_size", a)["success"].asBool());
	h.Pump(3);
	const Json::Value st = h.Call("cow.worldgen.status");
	EXPECT_EQ(st["dimensions"]["width"].asInt(), 80);
	EXPECT_EQ(st["dimensions"]["height"].asInt(), 52);
}

TEST(WorldGenCommands, RegenerateReportsPendingThenBumpsRevision)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	const uint32_t before = h.Rev();
	Json::Value a(Json::objectValue); a["seed"] = 5;
	const Json::Value r = h.Call("cow.worldgen.regenerate", a);
	EXPECT_TRUE(r["success"].asBool());
	EXPECT_TRUE(r["pending"].asBool());
	EXPECT_EQ(r["revision"].asUInt(), before);
	EXPECT_TRUE(r.isMember("ms"));
	h.Pump(3);
	EXPECT_EQ(h.Rev(), before + 1);
	EXPECT_EQ(h.Call("cow.worldgen.status")["seed"].asUInt(), 5u);
}

TEST(WorldGenCommands, PresetLoadAndSaveRoundTrip)
{
	const std::string presets = FindShippedPresetDir();
	ASSERT_FALSE(presets.empty());
	const std::filesystem::path out = std::filesystem::temp_directory_path() / "WorldGenCommandsOut";
	std::error_code ec;
	std::filesystem::remove_all(out, ec);
	Harness h("{\"presets_dir\":\"" + presets + "\",\"out_presets_dir\":\"" + out.generic_string() + "\"}");
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	const uint32_t before = h.Rev();

	Json::Value load(Json::objectValue); load["id"] = "archipelago";
	const Json::Value lr = h.Call("cow.worldgen.preset.load", load);
	EXPECT_TRUE(lr["success"].asBool()) << lr.toStyledString();
	h.Pump(3);
	EXPECT_EQ(h.Rev(), before + 1);
	EXPECT_EQ(h.Call("cow.worldgen.status")["preset"].asString(), "archipelago");

	Json::Value save(Json::objectValue);
	save["id"] = "wg_cmd_test"; save["displayName"] = "WG Cmd Test"; save["description"] = "temp";
	const Json::Value sr = h.Call("cow.worldgen.preset.save", save);
	EXPECT_TRUE(sr["success"].asBool()) << sr.toStyledString();
	const Json::Value list = h.Call("cow.worldgen.presets");
	std::string ids;
	for (const Json::Value& p : list["presets"]) ids += p["id"].asString() + ",";
	EXPECT_NE(ids.find("wg_cmd_test"), std::string::npos) << ids;
	std::filesystem::remove_all(out, ec);
}

TEST(WorldGenCommands, BadArgumentsFailWithAnErrorAndDoNotChangeTheRevision)
{
	const std::string presets = FindShippedPresetDir();
	Harness h("{\"presets_dir\":\"" + presets + "\"}");
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	const uint32_t before = h.Rev();

	Json::Value badSize(Json::objectValue); badSize["size"] = "Gigantic";
	EXPECT_TRUE(Failed(h.Call("cow.worldgen.set_size", badSize)));
	EXPECT_TRUE(Failed(h.Call("cow.worldgen.set_size", Obj())));
	EXPECT_TRUE(Failed(h.Call("cow.worldgen.preset.load", Obj())));
	Json::Value unknown(Json::objectValue); unknown["id"] = "no_such_preset";
	EXPECT_TRUE(Failed(h.Call("cow.worldgen.preset.load", unknown)));
	EXPECT_TRUE(Failed(h.Call("cow.worldgen.preset.save", Obj())));
	Json::Value outOfRange(Json::objectValue); outOfRange["x"] = 100000; outOfRange["y"] = 0;
	EXPECT_TRUE(Failed(h.Call("cow.worldgen.cell", outOfRange)));
	Json::Value badLayer(Json::objectValue); badLayer["layer"] = "no_such_layer";
	EXPECT_TRUE(Failed(h.Call("cow.worldgen.layers", badLayer)));

	h.Pump(5);
	EXPECT_EQ(h.Rev(), before);
}

TEST(WorldGenCommands, UnknownSetKeyLandsInRejected)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	Json::Value a(Json::objectValue); a["bogus"] = 1;
	const Json::Value r = h.Call("cow.worldgen.set", a);
	ASSERT_TRUE(r["rejected"].isArray());
	bool found = false;
	for (const Json::Value& v : r["rejected"]) found = found || v.asString() == "bogus";
	EXPECT_TRUE(found) << r.toStyledString();
}

TEST(WorldGenCommands, AutoGenerateFalseStartsOnlyAfterAnAction)
{
	Harness h("{\"auto_generate\":false}");
	ASSERT_TRUE(h.Start());
	h.Pump(5);
	EXPECT_EQ(h.Rev(), 0u);
	Json::Value a(Json::objectValue); a["rivers"] = 0.7;
	EXPECT_TRUE(h.Call("cow.worldgen.set", a)["success"].asBool());
	h.Pump(3);
	EXPECT_EQ(h.Rev(), 1u);
}

TEST(WorldGenCommands, CommandsAreUnregisteredOnStop)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	ASSERT_TRUE(gWg->CommandsRegistered());
	SimQueryHostModule* host = gHost;
	CoW::WorldGenerationModule* wg = gWg;
	h.app->RequestShutdown();
	for (int guard = 0; guard < 1000 && h.app->Update(Harness::kDt); ++guard) {}
	EXPECT_FALSE(wg->CommandsRegistered());
	for (const char* n : CoW::WorldGenerationModule::kCommandNames)
		EXPECT_FALSE(host->GetHost().Has(StringCRC(n))) << n;
	h.app.reset();
}

// Consumer Amendments ACs 10-11: a partial registration (one name already taken) leaves none of ours behind, and
// re-entering the stage registers cleanly with no duplicate rejections.
TEST(WorldGenCommands, PartialRegistrationRollsBackEveryNameItRegistered)
{
	SimQueryHostModule* host = nullptr;
	{
		Harness h(std::string(), true, false, true);
		ASSERT_TRUE(h.Start());
		h.Pump(3);
		host = gHost;
		EXPECT_FALSE(gWg->CommandsRegistered());
		for (const char* n : CoW::WorldGenerationModule::kCommandNames)
		{
			SCOPED_TRACE(n);
			// The squatter's name stays (it is not ours); every other name must be gone.
			EXPECT_EQ(host->GetHost().Has(StringCRC(n)), std::string(n) == WgSquatter::kName);
		}
	}
}

TEST(WorldGenCommands, StageReEntryRegistersCleanlyWithNoDuplicateRejections)
{
	for (int visit = 0; visit < 3; ++visit)
	{
		Harness h;
		Dia::CellLayers::Testing::LogCapture log("WGEN");
		ASSERT_TRUE(h.Start()) << "visit " << visit;
		h.Pump(3);
		EXPECT_TRUE(gWg->CommandsRegistered()) << "visit " << visit;
		EXPECT_EQ(0u, log.Errors("registration rejected")) << "visit " << visit;
	}
}

// Consumer Amendments ACs 13-14: the module owns the hex grid, exposes it, and rebuilds it in place on a size change.
TEST(WorldGenCommands, ModuleExposesTheHexGridAndRebuildsItInPlaceOnASizeChange)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	h.Pump(5);
	const Dia::Graphics::CellGridDesc* desc = nullptr;
	{
		const Draoi::HexWorld::WorldHexGrid* g = gWg->GetHexGrid();
		ASSERT_NE(g, nullptr);
		const Draoi::HexWorld::WorldStructure* w = gWg->GetCurrentWorld();
		ASSERT_NE(w, nullptr);
		EXPECT_EQ(g->Dimensions().width, w->dimensions.width);
		EXPECT_EQ(g->Dimensions().height, w->dimensions.height);
		desc = &g->Desc();
	}
	Json::Value a = Obj();
	a["size"] = "Standard";
	h.Call("cow.worldgen.set_size", a);
	ASSERT_TRUE(h.PumpUntil([&] { return gWg->GetCurrentWorld() && gWg->GetCurrentWorld()->dimensions.width != 66; }, 400));
	h.Pump(2);
	const Draoi::HexWorld::WorldHexGrid* g = gWg->GetHexGrid();
	ASSERT_NE(g, nullptr);
	EXPECT_EQ(g->Dimensions().width, gWg->GetCurrentWorld()->dimensions.width);
	EXPECT_EQ(g->Dimensions().height, gWg->GetCurrentWorld()->dimensions.height);
	EXPECT_EQ(&g->Desc(), desc) << "same CellGridDesc address after the rebuild";
}

TEST(WorldGenCommands, ModuleWithoutSimQueryHostStartsAndRegistersNothing)
{
	Harness h(std::string(), false);
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	EXPECT_FALSE(gWg->CommandsRegistered());
	EXPECT_EQ(gWg->GetWorldRevision(), 1u);
}

// ---- Regions and sites (AC13): real stream round trip -------------------------------------------------------------

TEST(WorldGenCommands, RegionsQueryRoundTripReturnsEveryRecord)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	ASSERT_TRUE(h.Call("cow.worldgen.regenerate")["success"].asBool());
	h.Pump(3);

	const Json::Value r = h.Call("cow.worldgen.regions");   // Call() fails on a reply over 16 KB
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
	EXPECT_TRUE(r["fresh"].asBool());
	const Draoi::HexWorld::WorldStructure* w = gWg->GetCurrentWorld();
	ASSERT_NE(w, nullptr);
	ASSERT_EQ(r["regions"].size(), w->regions.regions.Size());
	ASSERT_GT(r["regions"].size(), 0u);
	for (unsigned i = 0; i < r["regions"].size(); ++i)
	{
		const Json::Value& j = r["regions"][i];
		EXPECT_EQ(j["id"].asUInt(), static_cast<unsigned>(w->regions.regions[i].id));
		EXPECT_TRUE(j["neighbours"].isArray());
		EXPECT_EQ(j["neighbours"].size(), w->regions.regions[i].neighbours.Size());
	}
	EXPECT_TRUE(r.isMember("limitedBy"));
	EXPECT_TRUE(r.isMember("absorbedCells"));
	EXPECT_TRUE(r.isMember("mainlandRegionCount"));
}

TEST(WorldGenCommands, SitesQueryRoundTripPagesAllSitesAndFiltersByKind)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	ASSERT_TRUE(h.Call("cow.worldgen.regenerate")["success"].asBool());
	h.Pump(3);
	const Draoi::HexWorld::WorldStructure* w = gWg->GetCurrentWorld();
	ASSERT_NE(w, nullptr);
	const int total = w->sites.Count();
	ASSERT_GT(total, 100);

	size_t gathered = 0;
	for (int page = 0; page < 20 && gathered < static_cast<size_t>(total); ++page)
	{
		Json::Value a = Obj(); a["offset"] = static_cast<Json::UInt>(gathered);
		const Json::Value r = h.Call("cow.worldgen.sites", a);   // default page must fit the 16 KB reply
		ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
		EXPECT_EQ(r["total"].asInt(), total);
		ASSERT_GT(r["sites"].size(), 0u);
		gathered += r["sites"].size();
	}
	EXPECT_EQ(gathered, static_cast<size_t>(total));

	Json::Value big = Obj(); big["limit"] = 100000;
	EXPECT_TRUE(h.Call("cow.worldgen.sites", big)["success"].asBool());   // clamped, still under the reply cap

	Json::Value f = Obj(); f["kind"] = "CoW.Node.Corridor";
	const Json::Value fr = h.Call("cow.worldgen.sites", f);
	EXPECT_EQ(fr["total"].asInt(), w->sites.Count(CoW::WorldGeneration::BuiltinNodeKinds::Corridor()));
	for (const Json::Value& s : fr["sites"]) EXPECT_EQ(s["kind"].asString(), "CoW.Node.Corridor");

	Json::Value bad = Obj(); bad["kind"] = "No.Such.Kind";
	EXPECT_TRUE(Failed(h.Call("cow.worldgen.sites", bad)));
}

TEST(WorldGenCommands, ReportCommandRoundTripsTheRetryReport)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	h.Pump(3);

	// Before any Regenerate (boot generation is terrain only) there is no report.
	const Json::Value none = h.Call("cow.worldgen.report");
	ASSERT_TRUE(none["success"].asBool()) << none.toStyledString();

	Json::Value a = Obj(); a["maxRetryAttempts"] = 6;
	const Json::Value regen = h.Call("cow.worldgen.regenerate", a);
	ASSERT_TRUE(regen["success"].asBool()) << regen.toStyledString();
	EXPECT_EQ(regen["maxRetryAttempts"].asUInt(), 6u);
	h.Pump(3);

	const Json::Value r = h.Call("cow.worldgen.report");
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
	EXPECT_TRUE(r["hasReport"].asBool());
	EXPECT_EQ(r["outcome"].asString(), "Success");
	EXPECT_EQ(r["maxAttempts"].asUInt(), 6u);
	EXPECT_GE(r["attemptCount"].asUInt(), 1u);
	EXPECT_EQ(r["worldRevision"].asUInt(), gWg->GetWorldRevision());
	EXPECT_FALSE(r["stale"].asBool());
	ASSERT_GE(r["attempts"].size(), 1u);
	EXPECT_TRUE(r["attempts"][r["attempts"].size() - 1]["passed"].asBool());

	const Json::Value st = h.Call("cow.worldgen.status");
	EXPECT_EQ(st["generation"]["outcome"].asString(), "Success");

	Json::Value bad = Obj(); bad["offset"] = -3;
	EXPECT_TRUE(Failed(h.Call("cow.worldgen.report", bad)));
}

TEST(WorldGenCommands, ParamsEffectiveAndStatusReportRegionLimitsAndFreshness)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	ASSERT_TRUE(h.Call("cow.worldgen.regenerate")["success"].asBool());
	h.Pump(3);

	const Json::Value eff = h.Call("cow.worldgen.status")["effective"];
	ASSERT_TRUE(eff["corridorLimited"].isArray());
	EXPECT_EQ(eff["corridorLimited"].size(), 2u);
	EXPECT_TRUE(eff["nodeLimited"].isBool());
	EXPECT_TRUE(eff["regionLimitedBy"].isString());

	Json::Value st = h.Call("cow.worldgen.status");
	EXPECT_TRUE(st["regionsFresh"].asBool());
	ASSERT_TRUE(st["layout"].isObject());
	EXPECT_TRUE(st["layout"]["ok"].asBool()) << st["layout"].toStyledString();

	Json::Value a = Obj(); a["rivers"] = 0.8;
	ASSERT_TRUE(h.Call("cow.worldgen.set", a)["success"].asBool());
	h.Pump(3);
	st = h.Call("cow.worldgen.status");
	EXPECT_FALSE(st["regionsFresh"].asBool());
	EXPECT_EQ(h.Call("cow.worldgen.regions")["regions"].size(), 0u);
	EXPECT_EQ(h.Call("cow.worldgen.sites")["total"].asInt(), 0);
}

TEST(WorldGenCommands, CellQueryReportsRegionZoneAndSitesAndEveryLandCellHasARegion)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	ASSERT_TRUE(h.Call("cow.worldgen.regenerate")["success"].asBool());
	h.Pump(3);
	const Draoi::HexWorld::WorldStructure* w = gWg->GetCurrentWorld();
	ASSERT_NE(w, nullptr);

	for (int i = 0; i < w->CellCount(); ++i)
	{
		const Draoi::HexWorld::Cell& c = w->Cells()[i];
		const bool water = c.terrain == Draoi::HexWorld::TerrainType::DeepWater ||
		                   c.terrain == Draoi::HexWorld::TerrainType::ShallowWater ||
		                   c.terrain == Draoi::HexWorld::TerrainType::Lake;
		if (!water) ASSERT_NE(c.region, Draoi::HexWorld::kNoRegion) << "land cell " << i;
	}

	const Draoi::HexWorld::Site& site = w->sites.All().front();
	Json::Value a = Obj(); a["index"] = site.cell;
	const Json::Value r = h.Call("cow.worldgen.cell", a);
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
	EXPECT_EQ(r["region"].asUInt(), static_cast<unsigned>(w->Cells()[site.cell].region));
	EXPECT_EQ(r["zone"].asString(), Draoi::HexWorld::StratumName(::Draoi::HexWorld::Testing::ShippedStratumTable(), w->Cells()[site.cell].stratum));
	ASSERT_TRUE(r["sites"].isArray());
	EXPECT_GE(r["sites"].size(), 1u);
}

TEST(WorldGenCommands, ModuleOwnsRegistryAndDefersGenerationUntilDependantRegisters)
{
	// regenerate_on_start: the Regenerate is pending from the module's start, i.e. requested before the registrar ran.
	Harness h("{\"regenerate_on_start\":true}", true, true);
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	ASSERT_NE(gRegistrar, nullptr);
	EXPECT_TRUE(gRegistrar->registered);
	// The very first generation already includes the dependant's kind: generation waited for the hold to be released.
	EXPECT_EQ(gWg->GetSession().World().sites.Count(StringCRC("Test.Registered")), 1);

	Json::Value a = Obj(); a["kind"] = "Test.Registered";
	const Json::Value r = h.Call("cow.worldgen.sites", a);
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
	EXPECT_EQ(r["total"].asInt(), 1);
	EXPECT_EQ(r["sites"][0u]["cell"].asInt(), 7);

	WgRegistrar* reg = gRegistrar;
	CoW::WorldGenerationModule* wg = gWg;
	h.app->RequestShutdown();
	for (int guard = 0; guard < 1000 && h.app->Update(Harness::kDt); ++guard) {}
	EXPECT_TRUE(reg->unregistered);
	EXPECT_EQ(wg->GetSiteRegistry().Count(), 0);
	h.app.reset();
}

// ---- JSON export / import (Feature 4): real stream round trip ----------------------------------------------------------

namespace
{
	// A temporary worlds directory, configured through the module's out_worlds_dir key.
	struct WorldsDir
	{
		std::filesystem::path path;
		WorldsDir()
		{
			static int counter = 0;
			path = std::filesystem::temp_directory_path() / ("cow_cmd_worlds_" + std::to_string(++counter) + "_" +
				std::to_string(reinterpret_cast<uintptr_t>(this) & 0xFFFFFF));
			std::error_code ec;
			std::filesystem::remove_all(path, ec);
		}
		~WorldsDir() { std::error_code ec; std::filesystem::remove_all(path, ec); }
		std::string Config() const { return "{\"out_worlds_dir\":\"" + path.generic_string() + "\"}"; }
	};

	bool SameCellBytes(const Draoi::HexWorld::WorldStructure& a, const Draoi::HexWorld::WorldStructure& b)
	{
		return a.CellCount() == b.CellCount() &&
		       std::memcmp(a.Cells(), b.Cells(), sizeof(Draoi::HexWorld::Cell) * static_cast<size_t>(a.CellCount())) == 0;
	}
}

TEST(WorldGenCommands, ExportThenImportRestoresTheWorldAndOpensLocked)
{
	WorldsDir dir;
	Harness h(dir.Config());
	ASSERT_TRUE(h.Start());
	h.Pump(3);

	Json::Value regen = Obj(); regen["seed"] = 4242;
	ASSERT_TRUE(h.Call("cow.worldgen.regenerate", regen)["success"].asBool());
	h.Pump(4);
	const Draoi::HexWorld::WorldStructure original = gWg->GetSession().World();
	ASSERT_GT(original.regions.regions.Size(), 0u);

	Json::Value ex = Obj(); ex["name"] = "roundtrip";
	const Json::Value exported = h.Call("cow.worldgen.export", ex);
	ASSERT_TRUE(exported["success"].asBool()) << exported.toStyledString();
	EXPECT_EQ(exported["name"].asString(), "roundtrip");
	EXPECT_TRUE(std::filesystem::exists(dir.path / "roundtrip.wld.json"));
	EXPECT_GT(exported["bytes"].asUInt64(), 1000u);

	// Move to a different world, then bring the exported one back.
	Json::Value other = Obj(); other["seed"] = 9;
	ASSERT_TRUE(h.Call("cow.worldgen.regenerate", other)["success"].asBool());
	h.Pump(4);
	ASSERT_FALSE(SameCellBytes(gWg->GetSession().World(), original));

	Json::Value im = Obj(); im["name"] = "roundtrip";
	const Json::Value imported = h.Call("cow.worldgen.import", im);
	ASSERT_TRUE(imported["success"].asBool()) << imported.toStyledString();
	EXPECT_TRUE(imported["locked"].asBool());
	EXPECT_EQ(imported["warnings"].size(), 0u);
	EXPECT_TRUE(SameCellBytes(gWg->GetSession().World(), original));
	EXPECT_TRUE(gWg->GetSession().World().regions == original.regions);
	EXPECT_TRUE(gWg->GetSession().World().sites == original.sites);

	// The query side agrees.
	const Json::Value status = h.Call("cow.worldgen.status");
	EXPECT_TRUE(status["locked"].asBool());
	EXPECT_EQ(status["source"].asString(), "imported");
	EXPECT_EQ(status["seed"].asUInt64(), 4242u);
	h.Pump(4);
	EXPECT_EQ(h.Call("cow.worldgen.status")["revision"].asUInt(), status["revision"].asUInt()) << "an imported world is not regenerated away";
	EXPECT_TRUE(h.Call("cow.worldgen.status")["locked"].asBool());
}

TEST(WorldGenCommands, LockedImportRefusesActionsUntilUnlockedThroughTheCommands)
{
	WorldsDir dir;
	Harness h(dir.Config());
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	Json::Value regen = Obj(); regen["seed"] = 77;
	h.Call("cow.worldgen.regenerate", regen);
	h.Pump(4);
	Json::Value ex = Obj(); ex["name"] = "w";
	ASSERT_TRUE(h.Call("cow.worldgen.export", ex)["success"].asBool());
	Json::Value im = Obj(); im["name"] = "w";
	ASSERT_TRUE(h.Call("cow.worldgen.import", im)["success"].asBool());
	const uint32_t rev = h.Rev();

	Json::Value slider = Obj(); slider["rivers"] = 0.9;
	const Json::Value setRefused = h.Call("cow.worldgen.set", slider);
	EXPECT_FALSE(setRefused["success"].asBool());
	EXPECT_EQ(setRefused["error"].asString(), "locked");
	EXPECT_TRUE(setRefused["needsConfirm"].asBool());
	const Json::Value regenRefused = h.Call("cow.worldgen.regenerate", Obj());
	EXPECT_FALSE(regenRefused["success"].asBool());
	EXPECT_EQ(regenRefused["error"].asString(), "locked");
	Json::Value size = Obj(); size["size"] = "Large";
	EXPECT_EQ(h.Call("cow.worldgen.set_size", size)["error"].asString(), "locked");
	h.Pump(4);
	EXPECT_EQ(h.Rev(), rev);

	slider["unlock"] = true;
	const Json::Value unlocked = h.Call("cow.worldgen.set", slider);
	EXPECT_TRUE(unlocked["success"].asBool()) << unlocked.toStyledString();
	h.Pump(4);
	EXPECT_GT(h.Rev(), rev);
	const Json::Value status = h.Call("cow.worldgen.status");
	EXPECT_FALSE(status["locked"].asBool());
	EXPECT_EQ(status["source"].asString(), "generated");
}

TEST(WorldGenCommands, ImportPublishesWorldGenerationCompleteAndUpdatesTheRevision)
{
	WorldsDir dir;
	Harness h(dir.Config());
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	std::vector<CoW::WorldGenerationComplete> msgs;
	gBus->GetBus().RegisterType<CoW::WorldGenerationComplete, 4>();
	auto sub = gBus->GetBus().Subscribe<CoW::WorldGenerationComplete>(StringCRC("TestImportSink"),
		[&msgs](const CoW::WorldGenerationComplete& m) { msgs.push_back(m); });

	Json::Value regen = Obj(); regen["seed"] = 5;
	h.Call("cow.worldgen.regenerate", regen);
	h.Pump(4);
	Json::Value ex = Obj(); ex["name"] = "w";
	h.Call("cow.worldgen.export", ex);
	msgs.clear();
	Json::Value im = Obj(); im["name"] = "w";
	ASSERT_TRUE(h.Call("cow.worldgen.import", im)["success"].asBool());
	h.Pump(3);
	ASSERT_EQ(msgs.size(), 1u);
	EXPECT_EQ(msgs[0].seed, 5u);
	EXPECT_EQ(msgs[0].retryAttempts, 0u);
	EXPECT_EQ(msgs[0].worldRevision, gWg->GetSession().Revision());
}

TEST(WorldGenCommands, ExportAndImportErrorsAreStructuredAndChangeNothing)
{
	WorldsDir dir;
	Harness h(dir.Config());
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	const uint32_t rev = h.Rev();

	Json::Value bad = Obj(); bad["name"] = "../escape";
	for (const char* cmd : { "cow.worldgen.export", "cow.worldgen.import" })
	{
		SCOPED_TRACE(cmd);
		const Json::Value r = h.Call(cmd, bad);
		EXPECT_TRUE(Failed(r)) << r.toStyledString();
		EXPECT_EQ(r["status"].asString(), "InvalidName");
	}
	Json::Value missing = Obj(); missing["name"] = "ghost";
	const Json::Value gone = h.Call("cow.worldgen.import", missing);
	EXPECT_TRUE(Failed(gone));
	EXPECT_EQ(gone["status"].asString(), "NotFound");
	EXPECT_TRUE(Failed(h.Call("cow.worldgen.import", Obj())));   // name is required

	// A corrupt file: structured error naming the problem, session untouched.
	std::filesystem::create_directories(dir.path);
	{ std::ofstream f(dir.path / "broken.wld.json"); f << "{ \"format\": \"cow.world\", \"version\": 3 }"; }
	Json::Value broken = Obj(); broken["name"] = "broken";
	const Json::Value r = h.Call("cow.worldgen.import", broken);
	EXPECT_TRUE(Failed(r));
	EXPECT_EQ(r["field"].asString(), "version");
	EXPECT_NE(r["error"].asString().find("Unsupported cow.world version: 3"), std::string::npos);
	EXPECT_EQ(h.Rev(), rev);
	EXPECT_FALSE(h.Call("cow.worldgen.status")["locked"].asBool());
}

TEST(WorldGenCommands, ImportReplyStaysUnderTheReplyCapWithManyWarnings)
{
	WorldsDir dir;
	Harness h(dir.Config());
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	Json::Value regen = Obj(); regen["seed"] = 3;
	h.Call("cow.worldgen.regenerate", regen);
	h.Pump(4);
	Json::Value ex = Obj(); ex["name"] = "w";
	ASSERT_TRUE(h.Call("cow.worldgen.export", ex)["success"].asBool());

	// 600 unknown top-level fields => hundreds of warnings in the importer; the reply must still fit.
	std::string text;
	{ std::ifstream f(dir.path / "w.wld.json", std::ios::binary); text.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()); }
	std::string junk;
	for (int i = 0; i < 600; ++i) junk += "  \"junk_with_a_rather_long_field_name_" + std::to_string(i) + "\": " + std::to_string(i) + ",\n";
	text.insert(text.find("\"version\""), junk);
	{ std::ofstream f(dir.path / "noisy.wld.json", std::ios::binary); f << text; }

	Json::Value im = Obj(); im["name"] = "noisy";
	const Json::Value r = h.Call("cow.worldgen.import", im);
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString().substr(0, 400);
	EXPECT_LE(r["warnings"].size(), 41u);
	EXPECT_NE(r["warnings"][r["warnings"].size() - 1].asString().find("more"), std::string::npos);
}

TEST(WorldGenCommands, ExportOverwriteRulesApplyThroughTheCommand)
{
	WorldsDir dir;
	Harness h(dir.Config());
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	Json::Value ex = Obj(); ex["name"] = "once";
	ASSERT_TRUE(h.Call("cow.worldgen.export", ex)["success"].asBool());
	const Json::Value again = h.Call("cow.worldgen.export", ex);
	EXPECT_TRUE(Failed(again));
	EXPECT_EQ(again["status"].asString(), "AlreadyExists");
	ex["overwrite"] = true;
	EXPECT_TRUE(h.Call("cow.worldgen.export", ex)["success"].asBool());
	// Default name is the seed.
	const Json::Value byDefault = h.Call("cow.worldgen.export", Obj());
	ASSERT_TRUE(byDefault["success"].asBool()) << byDefault.toStyledString();
	EXPECT_EQ(byDefault["name"].asString(), std::to_string(gWg->GetSession().Seed()));
}

TEST(WorldGenCommands, WorldsDirectoryConfigKeysAreTypeChecked)
{
	Harness h("{\"out_worlds_dir\":5,\"worlds_dir\":[]}");
	ASSERT_TRUE(h.Start());   // wrong types are ignored with a warning; the defaults stay
	h.Pump(3);
	EXPECT_EQ(gWg->GetWorldRevision(), 1u);
}

// ---- Timeout and failure handling (Feature 5): cow.worldgen.cancel ------------------------------------------------------------


TEST(WorldGenCommandsCancel, CancelWithNothingRunningSucceedsAndSaysSo)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	const uint32_t revision = h.Rev();
	const Json::Value r = h.Call("cow.worldgen.cancel");
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
	EXPECT_FALSE(r["cancelled"].asBool());
	EXPECT_EQ(h.Rev(), revision);
}

TEST(WorldGenCommandsCancel, CancelMidRunThroughTheStreamKeepsThePreviousWorldAndReportsIt)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	const uint32_t revision = h.Rev();
	CoW::WorldGeneration::Testing::FixedGuarantee never("Test.Never", false);
	ASSERT_TRUE(gWg->GetSiteRegistry().RegisterGuarantee(&never));

	Json::Value a = Obj(); a["maxRetryAttempts"] = 64;
	ASSERT_TRUE(h.Call("cow.worldgen.regenerate", a)["success"].asBool());
	h.Pump(2);

	// A run is stepping: world-changing actions are refused with "busy", queries answer.
	Json::Value s = Obj(); s["rivers"] = 0.9;
	const Json::Value busy = h.Call("cow.worldgen.set", s);
	EXPECT_FALSE(busy["success"].asBool());
	EXPECT_EQ(busy["error"].asString(), "busy");
	EXPECT_TRUE(busy["generating"].asBool());
	const Json::Value mid = h.Call("cow.worldgen.status");
	EXPECT_TRUE(mid["generating"].asBool());
	EXPECT_EQ(mid["state"].asString(), "Generating");
	EXPECT_EQ(mid["revision"].asUInt(), revision);

	const Json::Value c = h.Call("cow.worldgen.cancel");
	ASSERT_TRUE(c["success"].asBool()) << c.toStyledString();
	EXPECT_TRUE(c["cancelled"].asBool());
	EXPECT_EQ(c["outcome"].asString(), "Cancelled");
	EXPECT_LT(c["attempts"].asUInt(), 64u);
	EXPECT_EQ(c["message"].asString(), "CANCEL: World generation cancelled by user");

	h.Pump(2);
	EXPECT_EQ(h.Rev(), revision);
	const Json::Value rep = h.Call("cow.worldgen.report");
	EXPECT_EQ(rep["outcome"].asString(), "Cancelled");
	EXPECT_EQ(rep["message"].asString(), "CANCEL: World generation cancelled by user");
	EXPECT_EQ(rep["worldRevision"].asUInt(), 0u);

	// Free again.
	EXPECT_TRUE(h.Call("cow.worldgen.set", s)["success"].asBool());
}

// ---- Panel gaps (worldgen-panel-rest plan task 1): validate, worlds, status.unavailableLayers --------------------------------

TEST(WorldGenCommandsPanelGaps, ValidateAndWorldsAreRegisteredAndAnswerThroughTheHost)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	EXPECT_TRUE(gHost->GetHost().Has(StringCRC("cow.worldgen.validate")));
	EXPECT_TRUE(gHost->GetHost().Has(StringCRC("cow.worldgen.worlds")));
	const uint32_t revision = h.Rev();
	const Json::Value v = h.Call("cow.worldgen.validate");
	ASSERT_TRUE(v["success"].asBool()) << v.toStyledString();
	EXPECT_EQ(v["revision"].asUInt(), revision);
	EXPECT_EQ(h.Rev(), revision);
	const Json::Value w = h.Call("cow.worldgen.worlds");
	ASSERT_TRUE(w["success"].asBool()) << w.toStyledString();
	EXPECT_TRUE(w["worlds"].isArray());
	EXPECT_TRUE(h.Call("cow.worldgen.status")["unavailableLayers"].isArray());
}

TEST(WorldGenCommandsCancel, RegenerateTakesATimeoutAndTheCommandIsListed)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	Json::Value a = Obj(); a["timeoutSeconds"] = 12.5;
	const Json::Value r = h.Call("cow.worldgen.regenerate", a);
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
	EXPECT_FLOAT_EQ(r["timeoutSeconds"].asFloat(), 12.5f);
	Json::Value bad = Obj(); bad["timeoutSeconds"] = -1;
	EXPECT_TRUE(Failed(h.Call("cow.worldgen.regenerate", bad)));

	bool listed = false;
	for (const char* n : CoW::WorldGenerationModule::kCommandNames) listed = listed || std::string(n) == "cow.worldgen.cancel";
	EXPECT_TRUE(listed);
	EXPECT_TRUE(gHost->GetHost().Has(StringCRC("cow.worldgen.cancel")));
}

// ---- World hand-off (SD-S1-010): the stage names its world in the module config ----------------------------------------

namespace
{
	using HealthStatus = Dia::Observation::Health::HealthStatus;

	bool HandoffFailing() { return gWg->GetHealthReporter().Report().status == HealthStatus::kFailing; }

	// The world a pure session builds for the preset form: preset, seed and size, then one Regenerate.
	Draoi::HexWorld::WorldGenSession PresetWorld(const std::string& presets, const char* id, uint64_t seed, const char* size = nullptr)
	{
		CoW::WorldGeneration::TestSession s;
		s.SetPresetDirectories(presets, std::string());
		if (size != nullptr) { Json::Value a = Obj(); a["size"] = size; s.SetSize(a); }
		Json::Value sd = Obj(); sd["seed"] = static_cast<Json::UInt64>(seed);
		s.Set(sd);
		Json::Value p = Obj(); p["id"] = id;
		EXPECT_TRUE(s.PresetLoad(p)["success"].asBool());
		s.Regenerate(Obj());
		Dia::Maths::RandomStream rng{ 1 };
		for (int i = 0; i < 10 && s.IsGenerating(); ++i) s.Tick(rng);
		for (int i = 0; i < 3 && s.Revision() == 0; ++i) s.Tick(rng);
		return s;
	}

	std::string HandoffConfig(const std::string& extra, const std::string& presets = std::string())
	{
		std::string c = "{" + extra;
		if (!presets.empty()) c += (extra.empty() ? "" : ",") + std::string("\"presets_dir\":\"") + presets + "\"";
		return c + "}";
	}
}

TEST(WorldHandoff, PresetFormGeneratesTheDeterministicWorldWithSeedAndSize)
{
	const std::string presets = FindShippedPresetDir();
	ASSERT_FALSE(presets.empty());
	Harness h(HandoffConfig("\"preset\":\"archipelago\",\"seed\":31", presets));
	ASSERT_TRUE(h.Start());
	h.Pump(6);

	ASSERT_NE(gWg->GetCurrentWorld(), nullptr);
	EXPECT_GT(gWg->GetWorldRevision(), 0u);
	EXPECT_TRUE(gWg->IsReady(1));
	EXPECT_FALSE(gWg->IsReady(gWg->GetWorldRevision() + 1));
	EXPECT_TRUE(gWg->GetHandoffError().empty());
	EXPECT_FALSE(HandoffFailing());
	EXPECT_EQ(gWg->GetSession().Seed(), 31u);
	EXPECT_EQ(gWg->GetSession().PresetId(), "archipelago");
	EXPECT_GT(gWg->GetCurrentWorld()->regions.regions.Size(), 0u) << "the preset form places regions and sites (a Regenerate)";

	const Draoi::HexWorld::WorldGenSession expected = PresetWorld(presets, "archipelago", 31);
	ASSERT_GT(expected.Revision(), 0u);
	EXPECT_TRUE(SameCellBytes(*gWg->GetCurrentWorld(), expected.World()));
	EXPECT_TRUE(gWg->GetCurrentWorld()->regions == expected.World().regions);
	EXPECT_TRUE(gWg->GetCurrentWorld()->sites == expected.World().sites);
}

TEST(WorldHandoff, PresetFormHonoursTheSizeKey)
{
	const std::string presets = FindShippedPresetDir();
	ASSERT_FALSE(presets.empty());
	Harness h(HandoffConfig("\"preset\":\"continental\",\"seed\":5,\"size\":\"Standard\"", presets));
	ASSERT_TRUE(h.Start());
	h.Pump(8);
	ASSERT_NE(gWg->GetCurrentWorld(), nullptr);
	const Draoi::HexWorld::WorldGenSession expected = PresetWorld(presets, "continental", 5, "Standard");
	EXPECT_EQ(gWg->GetCurrentWorld()->dimensions.width, expected.World().dimensions.width);
	EXPECT_EQ(gWg->GetCurrentWorld()->dimensions.height, expected.World().dimensions.height);
	EXPECT_NE(gWg->GetCurrentWorld()->dimensions.width, 66) << "Standard is larger than Small";
	EXPECT_TRUE(SameCellBytes(*gWg->GetCurrentWorld(), expected.World()));
}

TEST(WorldHandoff, NoHandoffKeysKeepsTodaysBehaviour)
{
	Harness h("{\"seed\":12}");
	ASSERT_TRUE(h.Start());
	h.Pump(6);
	ASSERT_NE(gWg->GetCurrentWorld(), nullptr);
	EXPECT_TRUE(gWg->GetHandoffError().empty());
	EXPECT_FALSE(HandoffFailing());
	EXPECT_EQ(gWg->GetSession().Seed(), 12u);
	EXPECT_EQ(gWg->GetCurrentWorld()->regions.regions.Size(), 0u) << "the plain auto-generated world is a live-preview world, no regions";
}

namespace
{
	// Starts a module with a bad hand-off config and checks the safe state.
	void ExpectHandoffFails(const std::string& config, const char* errorContains)
	{
		SCOPED_TRACE(config);
		Harness h(config);
		ASSERT_TRUE(h.Start()) << "a bad hand-off must not stop the module from starting";
		h.Pump(8);
		EXPECT_EQ(gWg->GetCurrentWorld(), nullptr);
		EXPECT_EQ(gWg->GetWorldRevision(), 0u);
		EXPECT_FALSE(gWg->IsReady(1));
		EXPECT_TRUE(HandoffFailing());
		EXPECT_GE(gWg->GetHealthReporter().Report().errors, 1u);
		EXPECT_NE(gWg->GetHandoffError().find(errorContains), std::string::npos) << gWg->GetHandoffError();
		EXPECT_EQ(h.Call("cow.worldgen.status")["revision"].asUInt(), 0u);
	}
}

TEST(WorldHandoff, UnknownPresetFailsClearly)
{
	const std::string presets = FindShippedPresetDir();
	ASSERT_FALSE(presets.empty());
	ExpectHandoffFails(HandoffConfig("\"preset\":\"no_such_preset\"", presets), "no_such_preset");
}

TEST(WorldHandoff, InvalidPresetIdFailsClearly)
{
	const std::string presets = FindShippedPresetDir();
	ExpectHandoffFails(HandoffConfig("\"preset\":\"../x\"", presets), "preset");
}

TEST(WorldHandoff, MoreThanOneSourceFails)
{
	const std::string presets = FindShippedPresetDir();
	ExpectHandoffFails(HandoffConfig("\"preset\":\"continental\",\"world_file\":\"w\"", presets), "more than one");
	ExpectHandoffFails(HandoffConfig("\"preset\":\"continental\",\"world_bundle\":\"b\"", presets), "more than one");
	ExpectHandoffFails(HandoffConfig("\"world_file\":\"w\",\"world_bundle\":\"b\""), "more than one");
}

TEST(WorldHandoff, WorldBundleIsRejectedBecauseWorldAssetsAreNotBuilt)
{
	ExpectHandoffFails("{\"world_bundle\":\"my_bundle\"}", "world_bundle");
}

TEST(WorldHandoff, KeysOfTheWrongTypeFail)
{
	ExpectHandoffFails("{\"preset\":5}", "preset");
	ExpectHandoffFails("{\"world_file\":true}", "world_file");
	ExpectHandoffFails("{\"world_bundle\":[1]}", "world_bundle");
	ExpectHandoffFails("{\"preset\":\"\"}", "preset");
}

TEST(WorldHandoff, AManualActionStillWorksAfterAFailedHandoff)
{
	Harness h("{\"world_bundle\":\"b\"}");
	ASSERT_TRUE(h.Start());
	h.Pump(6);
	ASSERT_EQ(gWg->GetWorldRevision(), 0u);
	ASSERT_TRUE(HandoffFailing());

	Json::Value regen = Obj(); regen["seed"] = 3;
	ASSERT_TRUE(h.Call("cow.worldgen.regenerate", regen)["success"].asBool());
	h.Pump(6);
	EXPECT_GT(gWg->GetWorldRevision(), 0u);
	EXPECT_NE(gWg->GetCurrentWorld(), nullptr);
}

namespace
{
	// Writes `<name>.wld.json` into `dir` by exporting a world a pure session generated (seed, preset-less defaults).
	// Returns that world so a test can compare the module's world with it.
	Draoi::HexWorld::WorldStructure WriteWorldFile(const std::filesystem::path& dir, const char* name, uint64_t seed)
	{
		CoW::WorldGeneration::TestSession s;
		s.SetWorldDirectories(std::string(), dir.generic_string());
		Json::Value sd = Obj(); sd["seed"] = static_cast<Json::UInt64>(seed);
		s.Set(sd);
		s.Regenerate(Obj());
		Dia::Maths::RandomStream rng{ 1 };
		for (int i = 0; i < 10 && (s.IsGenerating() || s.Revision() == 0); ++i) s.Tick(rng);
		EXPECT_GT(s.World().regions.regions.Size(), 0u);
		Json::Value ex = Obj(); ex["name"] = name;
		const Json::Value r = s.ExportWorld(ex);
		EXPECT_TRUE(r["success"].asBool()) << r.toStyledString();
		return s.World();
	}

	std::string FileConfig(const WorldsDir& dir, const std::string& extra)
	{
		return "{\"out_worlds_dir\":\"" + dir.path.generic_string() + "\"," + extra + "}";
	}

	void WriteText(const std::filesystem::path& file, const std::string& text)
	{
		std::filesystem::create_directories(file.parent_path());
		std::ofstream(file, std::ios::binary) << text;
	}
}

TEST(WorldHandoff, WorldFileFormLoadsTheExportedWorldLockedAndUnchanged)
{
	WorldsDir dir;
	const Draoi::HexWorld::WorldStructure expected = WriteWorldFile(dir.path, "hand", 4711);
	Harness h(FileConfig(dir, "\"world_file\":\"hand\""));
	ASSERT_TRUE(h.Start());
	h.Pump(6);

	ASSERT_NE(gWg->GetCurrentWorld(), nullptr);
	EXPECT_GT(gWg->GetWorldRevision(), 0u);
	EXPECT_TRUE(gWg->IsReady(1));
	EXPECT_TRUE(gWg->GetHandoffError().empty()) << gWg->GetHandoffError();
	EXPECT_FALSE(HandoffFailing());
	EXPECT_TRUE(SameCellBytes(*gWg->GetCurrentWorld(), expected));
	EXPECT_TRUE(gWg->GetCurrentWorld()->regions == expected.regions);
	EXPECT_TRUE(gWg->GetCurrentWorld()->sites == expected.sites);
	EXPECT_EQ(gWg->GetSession().Seed(), 4711u) << "the seed comes from the file's provenance";
	EXPECT_TRUE(gWg->GetSession().IsLocked());

	const uint32_t rev = gWg->GetWorldRevision();
	h.Pump(10);
	EXPECT_EQ(gWg->GetWorldRevision(), rev) << "the named world is not regenerated away";
	const Json::Value status = h.Call("cow.worldgen.status");
	EXPECT_TRUE(status["locked"].asBool());
	EXPECT_EQ(status["source"].asString(), "imported");
}

TEST(WorldHandoff, WorldFileIsFoundInTheDeployedWorldsDirectoryToo)
{
	WorldsDir dir;
	const Draoi::HexWorld::WorldStructure expected = WriteWorldFile(dir.path, "shipped", 21);
	WorldsDir emptyOut;
	Harness h("{\"worlds_dir\":\"" + dir.path.generic_string() + "\",\"out_worlds_dir\":\"" + emptyOut.path.generic_string() + "\",\"world_file\":\"shipped\"}");
	ASSERT_TRUE(h.Start());
	h.Pump(6);
	ASSERT_NE(gWg->GetCurrentWorld(), nullptr);
	EXPECT_TRUE(SameCellBytes(*gWg->GetCurrentWorld(), expected));
}

TEST(WorldHandoff, WorldFileIgnoresSeedAndSizeKeys)
{
	WorldsDir dir;
	const Draoi::HexWorld::WorldStructure expected = WriteWorldFile(dir.path, "hand", 55);
	Harness h(FileConfig(dir, "\"world_file\":\"hand\",\"seed\":999,\"size\":\"Large\""));
	ASSERT_TRUE(h.Start());
	h.Pump(6);
	ASSERT_NE(gWg->GetCurrentWorld(), nullptr);
	EXPECT_EQ(gWg->GetSession().Seed(), 55u);
	EXPECT_EQ(gWg->GetCurrentWorld()->dimensions.width, expected.dimensions.width);
	EXPECT_TRUE(SameCellBytes(*gWg->GetCurrentWorld(), expected));
}

TEST(WorldHandoff, WorldFileLoadsEvenWhenAutoGenerateIsOff)
{
	WorldsDir dir;
	const Draoi::HexWorld::WorldStructure expected = WriteWorldFile(dir.path, "hand", 8);
	Harness h(FileConfig(dir, "\"world_file\":\"hand\",\"auto_generate\":false"));
	ASSERT_TRUE(h.Start());
	h.Pump(6);
	ASSERT_NE(gWg->GetCurrentWorld(), nullptr) << "naming a world is an explicit request";
	EXPECT_TRUE(SameCellBytes(*gWg->GetCurrentWorld(), expected));
}

TEST(WorldHandoff, MissingWorldFileFails)
{
	WorldsDir dir;
	ExpectHandoffFails(FileConfig(dir, "\"world_file\":\"ghost\""), "ghost");
}

TEST(WorldHandoff, CorruptWorldFileFailsWithNoHalfWorld)
{
	WorldsDir dir;
	WriteText(dir.path / "bad.wld.json", "{ this is not json");
	ExpectHandoffFails(FileConfig(dir, "\"world_file\":\"bad\""), "bad");
}

TEST(WorldHandoff, TruncatedWorldFileFailsWithNoHalfWorld)
{
	WorldsDir dir;
	WriteWorldFile(dir.path, "cut", 3);
	std::string text;
	{
		std::ifstream in(dir.path / "cut.wld.json", std::ios::binary);
		text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	}
	ASSERT_GT(text.size(), 2000u);
	WriteText(dir.path / "cut.wld.json", text.substr(0, text.size() / 2));
	ExpectHandoffFails(FileConfig(dir, "\"world_file\":\"cut\""), "cut");
}

TEST(WorldHandoff, InvalidWorldNameFails)
{
	WorldsDir dir;
	ExpectHandoffFails(FileConfig(dir, "\"world_file\":\"../escape\""), "escape");
}

TEST(WorldHandoff, AManualRegenerateAfterAFailedWorldFileWorks)
{
	WorldsDir dir;
	Harness h(FileConfig(dir, "\"world_file\":\"ghost\""));
	ASSERT_TRUE(h.Start());
	h.Pump(6);
	ASSERT_EQ(gWg->GetWorldRevision(), 0u);
	Json::Value regen = Obj(); regen["seed"] = 6;
	ASSERT_TRUE(h.Call("cow.worldgen.regenerate", regen)["success"].asBool());
	h.Pump(6);
	EXPECT_GT(gWg->GetWorldRevision(), 0u);
}

TEST(WorldHandoff, WorldFileWinsOverARegenerateOnStartRequest)
{
	WorldsDir dir;
	const Draoi::HexWorld::WorldStructure expected = WriteWorldFile(dir.path, "hand", 90);
	Harness h(FileConfig(dir, "\"world_file\":\"hand\",\"regenerate_on_start\":true"));
	ASSERT_TRUE(h.Start());
	h.Pump(10);
	ASSERT_NE(gWg->GetCurrentWorld(), nullptr);
	EXPECT_TRUE(SameCellBytes(*gWg->GetCurrentWorld(), expected)) << "the pending Regenerate must not replace the named, locked world";
	EXPECT_TRUE(gWg->GetSession().IsLocked());
	EXPECT_EQ(gWg->GetSession().Seed(), 90u);
}

TEST(WorldHandoff, NullAndEmptyValuedKeysFail)
{
	ExpectHandoffFails("{\"preset\":null}", "preset");
	ExpectHandoffFails("{\"world_file\":null}", "world_file");
	ExpectHandoffFails("{\"world_file\":\"\"}", "world_file");
	ExpectHandoffFails("{\"world_bundle\":null}", "world_bundle");
}

TEST(WorldHandoff, WorldFileLoadsAfterADependantHasRegisteredItsKinds)
{
	WorldsDir dir;
	const Draoi::HexWorld::WorldStructure expected = WriteWorldFile(dir.path, "hand", 14);
	Harness h(FileConfig(dir, "\"world_file\":\"hand\""), true, true);
	ASSERT_TRUE(h.Start());
	h.Pump(6);
	ASSERT_NE(gRegistrar, nullptr);
	EXPECT_TRUE(gRegistrar->registered);
	ASSERT_NE(gWg->GetCurrentWorld(), nullptr);
	EXPECT_TRUE(SameCellBytes(*gWg->GetCurrentWorld(), expected));
	EXPECT_TRUE(gWg->GetHandoffError().empty());
}

TEST(WorldHandoff, MalformedConfigJsonIsNotAHandoffFailure)
{
	Harness h("{ not json");
	ASSERT_TRUE(h.Start());
	h.Pump(6);
	EXPECT_TRUE(gWg->GetHandoffError().empty());
	EXPECT_FALSE(HandoffFailing());
	EXPECT_NE(gWg->GetCurrentWorld(), nullptr) << "defaults apply";
}

#else
TEST(WorldGenCommands, RequiresDebugBuild) { GTEST_SKIP() << "sim-hosted commands exist only in DIA_DEBUG builds"; }
#endif

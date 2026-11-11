// cow.worldgen.* terrain-slice commands, hosted by CoW::WorldGenerationModule on the SimPU.
// Real round trip: Main-side shim with a real DebugServer -> SimQueryRequest EventStream -> SimQueryHostModule (SimPU)
// -> handler -> SimQueryReply EventStream -> shim. Harness adapted from TestCellLayersIntegrationStreams.cpp.
#include <gtest/gtest.h>
#include <Modules/WorldGenerationModule.h>   // first: RandomStream.h must precede windows.h (min/max macros)

#ifdef DIA_DEBUG

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
#include <Modules/RandomStreamModule.h>
#include <Modules/SimQueryHostModule.h>
#include <Shared/WorldGeneration/Testing/TestPlacers.h>
#include <Types/SimRegistrationMessage.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

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

	WgMainShim* gShim = nullptr;
	WgRegistrar* gRegistrar = nullptr;
	SimQueryHostModule* gHost = nullptr;
	CoW::WorldGenerationModule* gWg = nullptr;
	Cluiche::AppFlow::MessageBusHostModule* gBus = nullptr;
	CoW::RandomStreamModule* gRng = nullptr;

	Module* MakeShim(const StringCRC& id) { gShim = new WgMainShim(id); return gShim; }
	Module* MakeHost(const StringCRC& id) { gHost = new SimQueryHostModule(id); return gHost; }
	Module* MakeWg(const StringCRC& id) { gWg = new CoW::WorldGenerationModule(id); return gWg; }
	Module* MakeBus(const StringCRC& id) { gBus = new Cluiche::AppFlow::MessageBusHostModule(id); return gBus; }
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
		explicit Harness(const std::string& cfg = std::string(), bool withHost = true, bool withRegistrar = false) : config(cfg)
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
			ModuleDeclaration wg = withHost
				? Mod(CoW::WorldGenerationModule::kTypeId, "WorldGenerationModule", {}, { "MessageBusHostModule", "RandomStreamModule", "SimQueryHostModule" })
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

	const Json::Value params = h.Call("cow.worldgen.params");
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
	EXPECT_NEAR(h.Call("cow.worldgen.params")["params"]["rivers"].asDouble(), 0.9, 1e-6);
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
	const Json::Value p = h.Call("cow.worldgen.params");
	EXPECT_NEAR(p["params"]["moistureOffset"].asDouble(), 0.2, 1e-6);
	EXPECT_NEAR(p["ranges"]["moistureOffset"]["max"].asDouble(), 0.3, 1e-6);
	EXPECT_TRUE(p["ranges"].isMember("temperatureOffset"));
	EXPECT_TRUE(p["effective"].isMember("unresolvedPairs"));

	Json::Value b(Json::objectValue); b["moistureOffset"] = 0.9;
	const Json::Value rb = h.Call("cow.worldgen.set", b);
	ASSERT_EQ(rb["clamped"].size(), 1u) << rb.toStyledString();
	EXPECT_EQ(rb["clamped"][0].asString(), "moistureOffset");
	h.Pump(3);
	EXPECT_NEAR(h.Call("cow.worldgen.params")["params"]["moistureOffset"].asDouble(), 0.3, 1e-6);
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
	const CoW::WorldGeneration::TerrainLayers& layers = gWg->GetSession().Generator().GetLayers();
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
	EXPECT_EQ(h.Call("cow.worldgen.params")["preset"].asString(), "archipelago");

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
	const CoW::WorldGeneration::WorldStructure* w = gWg->GetCurrentWorld();
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
	const CoW::WorldGeneration::WorldStructure* w = gWg->GetCurrentWorld();
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

TEST(WorldGenCommands, ParamsEffectiveAndStatusReportRegionLimitsAndFreshness)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	h.Pump(3);
	ASSERT_TRUE(h.Call("cow.worldgen.regenerate")["success"].asBool());
	h.Pump(3);

	const Json::Value eff = h.Call("cow.worldgen.params")["effective"];
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
	const CoW::WorldGeneration::WorldStructure* w = gWg->GetCurrentWorld();
	ASSERT_NE(w, nullptr);

	for (int i = 0; i < w->CellCount(); ++i)
	{
		const CoW::WorldGeneration::Cell& c = w->Cells()[i];
		const bool water = c.terrain == CoW::WorldGeneration::TerrainType::DeepWater ||
		                   c.terrain == CoW::WorldGeneration::TerrainType::ShallowWater ||
		                   c.terrain == CoW::WorldGeneration::TerrainType::Lake;
		if (!water) ASSERT_NE(c.region, CoW::WorldGeneration::kNoRegion) << "land cell " << i;
	}

	const CoW::WorldGeneration::Site& site = w->sites.All().front();
	Json::Value a = Obj(); a["index"] = site.cell;
	const Json::Value r = h.Call("cow.worldgen.cell", a);
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
	EXPECT_EQ(r["region"].asUInt(), static_cast<unsigned>(w->Cells()[site.cell].region));
	EXPECT_EQ(r["zone"].asString(), CoW::WorldGeneration::DepthZoneName(w->Cells()[site.cell].depthZone));
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

#else
TEST(WorldGenCommands, RequiresDebugBuild) { GTEST_SKIP() << "sim-hosted commands exist only in DIA_DEBUG builds"; }
#endif

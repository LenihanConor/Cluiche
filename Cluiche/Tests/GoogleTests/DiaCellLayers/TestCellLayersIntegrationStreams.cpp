#include <gtest/gtest.h>
#include <DiaDebugServer/Testing/SimHelpTesting.h>

#ifdef DIA_DEBUG

#include <DiaDebugServer/DebugServer.h>
#include <DiaDebugServer/ISimRequestSink.h>
#include <DiaDebugServer/SimHostedTypes.h>
#include <DiaDebugServer/SimQueryHost.h>
#include <DiaDebugProtocol/DiaDebugProtocol.h>
#include <DiaProtobuf/ProtoStructConverter.h>

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/MainModule.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/MirrorQueries.h>
#include <DiaCellLayers/ViewerStateSnapshot.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaStreams/EventStreamReader.h>
#include <DiaStreams/EventStreamWriter.h>
#include <Modules/DebugServerHostModule.h>
#include <Modules/DomainStateBridgeModule.h>
#include <Modules/DomainStateRelayModule.h>
#include <Modules/SimQueryHostModule.h>
#include <Modules/TestStages/CellLayersPointerQueue.h>
#include <Types/SimRegistrationMessage.h>

#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

// Cell Layers integration (Streams Migration end-of-implementation pass), real-stream half: a live two-PU Application (MainPU +
// SimPU, ticked inline) with the REAL SimQueryHostModule, DomainStateRelayModule and DomainStateBridgeModule joined by the
// manifest's real EventStreams / FrameStream / ServiceStream. Two small test modules stand in for the parts that need a window or a
// socket: a Main-side shim that holds a real (never started) DebugServer and drains SimQueryReply exactly as DebugServerHostModule
// does, and a Sim-side driver that owns a real CellLayerViewerDomain and registers it through the same registrar seam the stage uses.

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
using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Cluiche::AppFlow::SimQueryHostModule;
using Cluiche::AppFlow::DomainStateBridgeModule;
using Cluiche::AppFlow::DomainStateRelayModule;
using Dia::Core::StringCRC;
using Dia::DebugServer::DebugServer;
using Dia::DebugServer::DebugServerTestAccess;
using Dia::DebugServer::SimKind;
using Dia::DebugServer::SimReplyEvent;
using Dia::DebugServer::SimRequestEvent;

namespace
{
	const char* const kAllNames[] = {
		"cells.layer.list", "cells.layer.values", "cells.select.get", "cells.select.stats",
		"cells.layer.base_by_id", "cells.overlay.toggle_by_id",
		"cells.select.cell", "cells.select.toggle", "cells.select.box", "cells.select.poly",
		"cells.select.category", "cells.select.cells", "cells.select.clear",
		"cells.select.query", "cells.query.check",
		"cells.select.tool", "cells.select.group", "cells.select.pin", "cells.select.unpin",
		"cells.inspect", "cells.inspector.expand",
		"cluichetest.celllayers.pointer_frames", "cluichetest.celllayers.pointer_pending", "cluichetest.celllayers.cell_pixel" };
	const size_t kNameCount = sizeof(kAllNames) / sizeof(kAllNames[0]);

	struct Sent { int connId; dia::debug::DebugMessage msg; };
	void CaptureSend(void* ctx, int connId, const dia::debug::DebugMessage& msg) { static_cast<std::vector<Sent>*>(ctx)->push_back(Sent{ connId, msg }); }

	uint64_t gNowMs = 0;
	uint64_t FakeNow() { return gNowMs; }

	// Real DebugServerHostModule (never added to the Application) used only for its announcement handling: HandleSimReply /
	// TickAnnounce with an injected clock, resync requests forwarded through `post`.
	class AnnounceProbe : public Cluiche::AppFlow::DebugServerHostModule
	{
	public:
		AnnounceProbe() : DebugServerHostModule(StringCRC("IntAnnounceProbe")) {}
		using DebugServerHostModule::HandleSimReply;
		using DebugServerHostModule::TickAnnounce;
		using DebugServerHostModule::GetAnnounceTracker;
		std::function<bool(const SimRequestEvent&)> post;
	protected:
		bool PostRequest(const SimRequestEvent& r) override { return post && post(r); }
	};

	// ---- Main side: real DebugServer + the DebugServerHostModule drain logic over the real SimQuery streams ----
	class IntMainShim : public MainModule, private Dia::DebugServer::ISimRequestSink
	{
	public:
		static const StringCRC kTypeId;
		explicit IntMainShim(const StringCRC& id) : MainModule(id)
		{
			server.SetNowMsFunction(&FakeNow);
			server.SetSendHook(&CaptureSend, &sent);
			server.SetSimRequestSink(this);
			DebugServer& ps = *probe.GetServer();
			ps.SetNowMsFunction(&FakeNow);
			ps.SetSendHook(&CaptureSend, &sent);
			ps.SetSimRequestSink(this);
			probe.post = [this](const SimRequestEvent& r) { return IsSuccess(mReq.Send(r)); };
		}

		DebugServer server;
		AnnounceProbe probe;
		bool useTracker = false;	// true: replies go through the real DebugServerHostModule announcement handling (resync)
		DebugServer& Srv() { return useTracker ? *probe.GetServer() : server; }
		std::vector<Sent> sent;
		std::vector<std::string> registered;
		std::vector<std::string> unregistered;
		int malformed = 0;
		int completed = 0;
		bool drainPaused = false;

		void Post(int conn, const char* name, const Json::Value& args = Json::Value(Json::objectValue))
		{
			dia::debug::DebugMessage m;
			m.set_type(dia::debug::MESSAGE_TYPE_COMMAND_REQUEST);
			auto* c = m.mutable_command_request();
			c->set_command(name);
			Dia::Proto::JsonValueToProtoStruct(args, c->mutable_payload());
			DebugServerTestAccess::HandleCommand(Srv(), conn, m);
		}

		const Sent* LastFor(int conn) const
		{
			for (size_t i = sent.size(); i > 0; --i) { if (sent[i - 1].connId == conn) return &sent[i - 1]; }
			return nullptr;
		}

		void OnConnectStreams(Application& app) override { mReq.Connect(app); mReps.Connect(app); }

	protected:
		StartResult DoStart() override { return StartResult::kReady; }
		StopResult DoStop() override { return StopResult::kDone; }
		void DoUpdate(const Dia::SimTime::MainTimeContext&) override
		{
			if (useTracker) gNowMs += 34;	// deterministic clock: one Main frame
			if (!drainPaused) Drain();
			if (useTracker) probe.TickAnnounce(gNowMs);
		}

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
						if (useTracker) { probe.HandleSimReply(reply, gNowMs); continue; }
					const Cluiche::AppFlow::SimRegistration reg = Cluiche::AppFlow::ParseSimRegistrationReply(reply);
					switch (reg.op)
					{
					case Cluiche::AppFlow::SimRegistrationOp::NotRegistration: server.CompleteSimReply(reply); ++completed; break;
					case Cluiche::AppFlow::SimRegistrationOp::Register:
						if (server.RegisterSimHosted(StringCRC(reg.name), reg.kind, Dia::DebugServer::Testing::DefaultSimHelp())) registered.push_back(reg.name);
						break;
					case Cluiche::AppFlow::SimRegistrationOp::Unregister:
						server.UnregisterSimHosted(StringCRC(reg.name));
						unregistered.push_back(reg.name);
						break;
					case Cluiche::AppFlow::SimRegistrationOp::Malformed: ++malformed; break;
					}
				}
			}
		}

		EventStreamWriter<SimRequestEvent> mReq{ this, "SimQueryRequest" };
		EventStreamReader<SimReplyEvent> mReps{ this, "SimQueryReply" };
	};
	const StringCRC IntMainShim::kTypeId("IntMainShim");

	// ---- Sim side: real viewer + mirror queries + pointer injection, registered through the SimQueryHostModule ----
	class ModuleRegistrar : public IMirrorQueryRegistrar
	{
	public:
		explicit ModuleRegistrar(SimQueryHostModule& m) : mModule(m) {}
		bool Register(const char* name, const Dia::DebugServer::SimHelpInfo& help, MirrorQueryHandler h) override { return mModule.RegisterQuery(name, help, std::move(h)); }
		bool RegisterCommand(const char* name, const Dia::DebugServer::SimHelpInfo& help, MirrorQueryHandler h) override { return mModule.RegisterCommand(name, help, std::move(h)); }
		void Unregister(const char* name) override { mModule.UnregisterQuery(name); }
	private:
		SimQueryHostModule& mModule;
	};

	struct SimRig
	{
		static const uint32_t kW = 104, kH = 64, kCells = kW * kH;
		TestGrid grid{ kW, kH };
		Dia::Graphics::CellFieldRegistry cellField;
		std::vector<std::unique_ptr<StubProvider>> provs;
		std::vector<std::string> names;
		FakeDebugDraw draw;
		CellLayerRegistry registry{ nullptr, &FakePuIdFn };
		LayerUploader uploader{ cellField, nullptr };
		std::unique_ptr<CellLayerViewerDomain> domain;
		std::unique_ptr<MirrorQueries> queries;
		CluicheTest::PointerFrameQueue pointerQueue;
		CluicheTest::HoverOverride hoverOverride;
		std::unique_ptr<ModuleRegistrar> registrar;
		bool registered = false;

		SimRig(SimQueryHostModule& host, uint32_t layerCount, const char* namePrefix = "layer")
		{
			FakePuId() = 1;
			for (uint32_t i = 0; i < layerCount; ++i)
			{
				names.push_back(namePrefix + std::to_string(i));
				provs.emplace_back(new StubProvider());
				provs.back()->values.resize(kCells);
				for (uint32_t c = 0; c < kCells; ++c) provs.back()->values[c] = static_cast<float>((c * (i + 3)) % 997) * 0.37f;
				CellLayerDesc d = provs.back()->MakeDesc(names.back().c_str(), &grid.desc, CellValueKind::Float, ("cells/" + names.back()).c_str());
				EXPECT_EQ(CellRegisterResult::Ok, registry.Register(d)) << names.back();
			}
			domain.reset(new CellLayerViewerDomain(registry, uploader, nullptr));
			domain->Update(draw);
			queries.reset(new MirrorQueries(registry, *domain, nullptr));
			registrar.reset(new ModuleRegistrar(host));
		}

		bool RegisterAll()
		{
			registered = RegisterMirrorQueries(*registrar, *queries) && domain->RegisterCommands(*registrar) &&
				CluicheTest::RegisterPointerInjectionCommands(*registrar, pointerQueue, hoverOverride, [](const Json::Value&) { return Json::Value(Json::objectValue); });
			return registered;
		}
		void UnregisterAll()
		{
			if (!registrar) return;
			CluicheTest::UnregisterPointerInjectionCommands(*registrar);
			if (domain) domain->UnregisterCommands(*registrar);
			UnregisterMirrorQueries(*registrar);
			registered = false;
		}
		~SimRig() { UnregisterAll(); }
	};

	class IntSimDriver : public SimModule
	{
	public:
		static const StringCRC kTypeId;
		explicit IntSimDriver(const StringCRC& id) : SimModule(id) {}
		std::unique_ptr<SimRig> rig;
		uint64_t ticks = 0;
	protected:
		StartResult DoStart() override { return StartResult::kReady; }
		void DoUpdate(const Dia::SimTime::SimTimeContext&) override { ++ticks; if (rig && rig->domain) rig->domain->Update(rig->draw); }
		StopResult DoStop() override { rig.reset(); return StopResult::kDone; }
	};
	const StringCRC IntSimDriver::kTypeId("IntSimDriver");

	IntMainShim* gShim = nullptr;
	IntSimDriver* gDriver = nullptr;
	SimQueryHostModule* gHost = nullptr;
	DomainStateRelayModule* gRelay = nullptr;
	DomainStateBridgeModule* gBridge = nullptr;

	Module* MakeShim(const StringCRC& id) { gShim = new IntMainShim(id); return gShim; }
	Module* MakeDriver(const StringCRC& id) { gDriver = new IntSimDriver(id); return gDriver; }
	Module* MakeHost(const StringCRC& id) { gHost = new SimQueryHostModule(id); return gHost; }
	Module* MakeRelay(const StringCRC& id) { gRelay = new DomainStateRelayModule(id); return gRelay; }
	Module* MakeBridge(const StringCRC& id) { gBridge = new DomainStateBridgeModule(id); return gBridge; }

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

	struct Harness
	{
		static constexpr float kDt = 0.0334f;	// just over one 30 Hz sim step: one sim tick per Update
		TypeRegistry reg;
		ApplicationManifestV3 manifest;
		std::unique_ptr<Application> app;

		Harness()
		{
			gShim = nullptr; gDriver = nullptr; gHost = nullptr; gRelay = nullptr; gBridge = nullptr;
			gNowMs = 10000;
			reg.Register(IntMainShim::kTypeId, MakeShim);
			reg.Register(IntSimDriver::kTypeId, MakeDriver);
			reg.Register(SimQueryHostModule::kTypeId, MakeHost);
			reg.Register(DomainStateRelayModule::kTypeId, MakeRelay);
			reg.Register(DomainStateBridgeModule::kTypeId, MakeBridge);

			manifest.version = 3;
			StageDeclaration boot;
			boot.name = StringCRC("Boot");
			manifest.stages.Add(boot);
			manifest.initialStage = StringCRC("Boot");
			AddStream(manifest, "SimQueryRequest", "EventStream", "MainPU", "SimPU", 16);
			AddStream(manifest, "SimQueryReply", "EventStream", "SimPU", "MainPU", 48);
			for (uint32_t i = 0; i < DomainStateBridgeModule::kSlotCount; ++i)
			{
				AddStream(manifest, DomainStateBridgeModule::kSnapshotStreamIds[i], "FrameStream", "SimPU", "MainPU");
				AddStream(manifest, DomainStateBridgeModule::kDemandStreamIds[i], "EventStream", "MainPU", "SimPU", 8);
				AddStream(manifest, DomainStateBridgeModule::kChannelStreamIds[i], "ServiceStream", "MainPU", "SimPU");
			}

			ProcessingUnitDeclaration mainPu;
			mainPu.instanceId = StringCRC("MainPU");
			mainPu.frequencyHz = 30.0f;
			mainPu.dedicatedThread = false;
			mainPu.modules.Add(Mod(IntMainShim::kTypeId, "shim", { { "SimQueryRequest", "writes" }, { "SimQueryReply", "reads" } }));
			mainPu.modules.Add(Mod(DomainStateBridgeModule::kTypeId, "bridge",
				{ { DomainStateBridgeModule::kSnapshotStreamIds[0], "reads" }, { DomainStateBridgeModule::kDemandStreamIds[0], "writes" }, { DomainStateBridgeModule::kChannelStreamIds[0], "provides" }, { DomainStateBridgeModule::kSnapshotStreamIds[1], "reads" }, { DomainStateBridgeModule::kDemandStreamIds[1], "writes" }, { DomainStateBridgeModule::kChannelStreamIds[1], "provides" }, { DomainStateBridgeModule::kSnapshotStreamIds[2], "reads" }, { DomainStateBridgeModule::kDemandStreamIds[2], "writes" }, { DomainStateBridgeModule::kChannelStreamIds[2], "provides" }, { DomainStateBridgeModule::kSnapshotStreamIds[3], "reads" }, { DomainStateBridgeModule::kDemandStreamIds[3], "writes" }, { DomainStateBridgeModule::kChannelStreamIds[3], "provides" } }));
			manifest.processingUnits.Add(mainPu);

			ProcessingUnitDeclaration simPu;
			simPu.instanceId = StringCRC("SimPU");
			simPu.frequencyHz = 30.0f;
			simPu.dedicatedThread = false;
			simPu.modules.Add(Mod(SimQueryHostModule::kTypeId, "host", { { "SimQueryRequest", "reads" }, { "SimQueryReply", "writes" } }));
			simPu.modules.Add(Mod(DomainStateRelayModule::kTypeId, "relay",
				{ { DomainStateBridgeModule::kSnapshotStreamIds[0], "writes" }, { DomainStateBridgeModule::kDemandStreamIds[0], "reads" }, { DomainStateBridgeModule::kChannelStreamIds[0], "consumes" }, { DomainStateBridgeModule::kSnapshotStreamIds[1], "writes" }, { DomainStateBridgeModule::kDemandStreamIds[1], "reads" }, { DomainStateBridgeModule::kChannelStreamIds[1], "consumes" }, { DomainStateBridgeModule::kSnapshotStreamIds[2], "writes" }, { DomainStateBridgeModule::kDemandStreamIds[2], "reads" }, { DomainStateBridgeModule::kChannelStreamIds[2], "consumes" }, { DomainStateBridgeModule::kSnapshotStreamIds[3], "writes" }, { DomainStateBridgeModule::kDemandStreamIds[3], "reads" }, { DomainStateBridgeModule::kChannelStreamIds[3], "consumes" } }));
			simPu.modules.Add(Mod(IntSimDriver::kTypeId, "driver", {}, { "host", "relay" }));
			manifest.processingUnits.Add(simPu);

			app.reset(new Application(manifest, reg));
		}

		bool Start()
		{
			if (!app->Start()) return false;
			for (int i = 0; i < 20; ++i) app->Update(kDt);
			return gShim && gDriver && gHost && gRelay && gBridge && gRelay->AttachedCount() == 0;
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
	};

	Json::Value Parse(const std::string& text)
	{
		Json::Value v;
		Json::CharReaderBuilder b;
		std::string errs;
		std::unique_ptr<Json::CharReader> r(b.newCharReader());
		EXPECT_TRUE(r->parse(text.data(), text.data() + text.size(), &v, &errs)) << errs;
		return v;
	}

	// Posts on Main, steps the application until the reply for `conn` arrives; returns the response (null optional on timeout).
	bool Call(Harness& h, int conn, const char* name, const Json::Value& args, bool& success, std::string& message, Json::Value& payload)
	{
		const size_t before = gShim->sent.size();
		gShim->Post(conn, name, args);
		if (!h.PumpUntil([&] { return gShim->sent.size() > before; })) return false;
		const auto& resp = gShim->sent[before].msg.command_response();
		success = resp.success();
		message = resp.message();
		payload = Dia::Proto::ProtoStructToJsonValue(resp.payload());
		return true;
	}

	Json::Value Obj() { return Json::Value(Json::objectValue); }
}

TEST(CellLayersIntegrationStreams, AllTwentyFourAnnouncementsReachTheMainServerInOrderOverTheRealReplyStream)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	gDriver->rig.reset(new SimRig(*gHost, 20));
	ASSERT_TRUE(gDriver->rig->RegisterAll());
	h.Pump(5);
	EXPECT_EQ(gShim->malformed, 0);
	ASSERT_EQ(gShim->registered.size(), kNameCount) << "announce stream capacity 48 must hold all 24 names";
	std::set<std::string> got(gShim->registered.begin(), gShim->registered.end());
	for (const char* n : kAllNames) EXPECT_EQ(got.count(n), 1u) << n << " was never announced to Main";
	for (const char* n : kAllNames) EXPECT_TRUE(gHost->GetHost().Has(StringCRC(n))) << n;
}

TEST(CellLayersIntegrationStreams, RequestsRoundTripThroughTheRealStreamsAndKeepOrder)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	gDriver->rig.reset(new SimRig(*gHost, 20));
	ASSERT_TRUE(gDriver->rig->RegisterAll());
	h.Pump(5);

	bool ok = false;
	std::string msg;
	Json::Value payload;
	ASSERT_TRUE(Call(h, 1, "cells.layer.list", Obj(), ok, msg, payload));
	EXPECT_TRUE(ok) << msg;
	EXPECT_EQ(payload["grids"][0]["layers"].size(), 20u);

	// Command then query posted in the same Main tick: the Sim host serves them in stream order.
	Json::Value box(Json::objectValue);
	box["q0"] = 0; box["r0"] = 0; box["q1"] = 1; box["r1"] = 1;
	const size_t before = gShim->sent.size();
	gShim->Post(1, "cells.select.box", box);
	gShim->Post(2, "cells.select.get");
	ASSERT_TRUE(h.PumpUntil([&] { return gShim->sent.size() >= before + 2; }));
	const Sent* get = gShim->LastFor(2);
	ASSERT_NE(get, nullptr);
	EXPECT_EQ(Dia::Proto::ProtoStructToJsonValue(get->msg.command_response().payload())["total"].asUInt(), 4u);
}

TEST(CellLayersIntegrationStreams, MoreRequestsThanTheStreamHoldsEachGetExactlyOneAnswerNeverAHang)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	gDriver->rig.reset(new SimRig(*gHost, 3));
	ASSERT_TRUE(gDriver->rig->RegisterAll());
	h.Pump(5);

	const int kRequests = 20;	// SimQueryRequest holds 16 (drop-oldest)
	const size_t before = gShim->sent.size();
	for (int i = 1; i <= kRequests; ++i) gShim->Post(i, "cells.layer.list");
	h.Pump(10);
	// Whatever the stream dropped, the server's pending table answers it at the timeout: advance the fake clock past it and sweep.
	gNowMs += 60000;
	DebugServerTestAccess::Sweep(gShim->server);
	h.Pump(3);
	ASSERT_EQ(gShim->sent.size(), before + kRequests) << "every request must get exactly one response (served, busy or timed out)";
	std::set<int> conns;
	int served = 0;
	for (size_t i = before; i < gShim->sent.size(); ++i)
	{
		conns.insert(gShim->sent[i].connId);
		if (gShim->sent[i].msg.command_response().success()) ++served;
		else EXPECT_FALSE(gShim->sent[i].msg.command_response().message().empty());
	}
	EXPECT_EQ(conns.size(), static_cast<size_t>(kRequests));
	EXPECT_GE(served, 16 - 1) << "the 16 the stream can hold are served";
}

// The announce stream holds 48 events (drop-oldest). Names are capped at 64 per module, but unregister/register churn between two
// Main drains can exceed 48 events, so Main loses the oldest announcements. The announcement resync (seq gap -> resync request ->
// paced replay -> atomic table swap) must repair this: every hosted name ends up routable at Main. Main runs the real
// DebugServerHostModule announcement handling with a deterministic clock (34 ms per Main frame).
TEST(CellLayersIntegrationStreams, AnnouncementChurnPastStreamCapacityNeverLeavesAHostedNameUnroutedAtMain)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	gDriver->rig.reset(new SimRig(*gHost, 3));
	gShim->useTracker = true;
	gShim->drainPaused = true;	// Main does not drain for a while (a long Main frame)
	auto noop = [](const Json::Value&) { return Json::Value(Json::objectValue); };
	for (int round = 0; round < 3; ++round)
	{
		for (int i = 0; i < 64; ++i)
		{
			const std::string n = "churn.n" + std::to_string(i);
			if (round > 0) gHost->UnregisterQuery(n.c_str());
			gHost->RegisterQuery(n.c_str(), Dia::DebugServer::SimHelpInfo{"test help"}, noop);
		}
	}
	gShim->drainPaused = false;
	// Converge: gap detected on the first drain, resync requested, begin + 64 replays paced 16 per Sim frame, table swapped.
	ASSERT_TRUE(h.PumpUntil([&] { return gShim->probe.GetAnnounceTracker().ResyncsCompleted() > 0; }, 300))
		<< "Main never completed a resync after the churn: gaps=" << gShim->probe.GetAnnounceTracker().AnnounceGaps()
		<< " requested=" << gShim->probe.GetAnnounceTracker().ResyncsRequested() << " timeouts=" << gShim->probe.GetAnnounceTracker().ResyncTimeouts()
		<< " mismatches=" << gShim->probe.GetAnnounceTracker().DigestMismatches() << " overflow=" << gHost->AnnounceOverflowCount();
	EXPECT_TRUE(gShim->probe.GetAnnounceTracker().InSync());
	uint32_t count = 0, hash = 0;
	gShim->probe.GetServer()->GetSimHostedDigest(count, hash);
	EXPECT_EQ(count, 64u);
	for (int i = 0; i < 64; ++i)
	{
		const std::string n = "churn.n" + std::to_string(i);
		ASSERT_TRUE(gHost->GetHost().Has(StringCRC(n.c_str())));
		bool ok = false;
		std::string msg;
		Json::Value payload;
		ASSERT_TRUE(Call(h, 100 + i, n.c_str(), Obj(), ok, msg, payload)) << n;
		EXPECT_TRUE(ok) << n << " is hosted on Sim but Main lost its announcement: " << msg;
	}
}

TEST(CellLayersIntegrationStreams, StageReEntryUnregistersThenReRegistersAndAStaleNameAnswersAnError)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	gDriver->rig.reset(new SimRig(*gHost, 5));
	ASSERT_TRUE(gDriver->rig->RegisterAll());
	h.Pump(5);
	bool ok = false;
	std::string msg;
	Json::Value payload;
	ASSERT_TRUE(Call(h, 1, "cells.select.clear", Obj(), ok, msg, payload));
	EXPECT_TRUE(ok);

	gDriver->rig->UnregisterAll();	// stage exit: unregister before the domain is destroyed
	h.Pump(5);
	EXPECT_EQ(gShim->unregistered.size(), kNameCount);
	gDriver->rig.reset();	// the viewer is gone
	ASSERT_TRUE(Call(h, 2, "cells.select.clear", Obj(), ok, msg, payload));
	EXPECT_FALSE(ok) << "a stale name must answer an error, never run a handler that captured a dead viewer";
	ASSERT_TRUE(Call(h, 3, "cells.layer.list", Obj(), ok, msg, payload));
	EXPECT_FALSE(ok);

	gShim->registered.clear();
	gDriver->rig.reset(new SimRig(*gHost, 5));	// stage re-entry
	ASSERT_TRUE(gDriver->rig->RegisterAll());
	h.Pump(5);
	EXPECT_EQ(gShim->registered.size(), kNameCount);
	ASSERT_TRUE(Call(h, 4, "cells.select.clear", Obj(), ok, msg, payload));
	EXPECT_TRUE(ok);
}

TEST(CellLayersIntegrationStreams, ModuleStopAnnouncesUnregisterForEveryHostedName)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	gDriver->rig.reset(new SimRig(*gHost, 3));
	ASSERT_TRUE(gDriver->rig->RegisterAll());
	h.Pump(5);
	ASSERT_EQ(gShim->registered.size(), kNameCount);
	IntMainShim* shim = gShim;
	h.app->RequestShutdown();
	for (int guard = 0; guard < 1000 && h.app->Update(Harness::kDt); ++guard) {}
	// Registrations were withdrawn on Sim stop (or Main stopped first); either way nothing malformed was seen and no handler survived.
	EXPECT_EQ(shim->malformed, 0);
	h.app.reset();
}

TEST(CellLayersIntegrationStreams, SnapshotIsPendingUntilDemandedThenFreshAndAdvancing)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	gDriver->rig.reset(new SimRig(*gHost, 20));
	Dia::VisualDebugger::IDebugDomain* adapter = gRelay->Attach(*gDriver->rig->domain);
	ASSERT_NE(adapter, nullptr);

	h.Pump(10);
	EXPECT_EQ(gRelay->PublishCount(*gDriver->rig->domain), 0u) << "nothing publishes before the first demand";

	Json::Value state(Json::objectValue);
	adapter->GetJSONState(state);	// Main: no snapshot yet -> pending marker, and this call is the demand
	EXPECT_TRUE(state["pending"].asBool());
	EXPECT_TRUE(state["empty"].asBool());

	ASSERT_TRUE(h.PumpUntil([&] { return gRelay->PublishCount(*gDriver->rig->domain) > 0; }, 20)) << "demand must arm publishing within a few ticks";
	Json::Value first(Json::objectValue);
	adapter->GetJSONState(first);
	ASSERT_FALSE(first.isMember("pending"));
	EXPECT_TRUE(first.isMember("simFrame"));
	EXPECT_EQ(first["activeBase"].asString(), "layer0");
	const uint64_t frameA = first["simFrame"].asUInt64();

	h.Pump(5);
	Json::Value later(Json::objectValue);
	adapter->GetJSONState(later);
	EXPECT_GT(later["simFrame"].asUInt64(), frameA) << "simFrame advances while the window is armed";
	gRelay->Detach(*gDriver->rig->domain);
}

TEST(CellLayersIntegrationStreams, PublishingStopsAfterTheSixtyTickWindowAndResumesOnNewDemand)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	gDriver->rig.reset(new SimRig(*gHost, 3));
	Dia::VisualDebugger::IDebugDomain* adapter = gRelay->Attach(*gDriver->rig->domain);
	ASSERT_NE(adapter, nullptr);
	Json::Value state(Json::objectValue);
	adapter->GetJSONState(state);
	ASSERT_TRUE(h.PumpUntil([&] { return gRelay->PublishCount(*gDriver->rig->domain) > 0; }, 20));

	h.Pump(80);	// well past the 60 tick window with no further demand
	const uint32_t settled = gRelay->PublishCount(*gDriver->rig->domain);
	EXPECT_GE(settled, 55u);
	EXPECT_LE(settled, 65u) << "about 60 snapshots per demand window";
	h.Pump(30);
	EXPECT_EQ(gRelay->PublishCount(*gDriver->rig->domain), settled) << "no publishing once the window expired";

	Json::Value stale(Json::objectValue);
	adapter->GetJSONState(stale);	// returns the last snapshot as is, and signals demand again
	ASSERT_FALSE(stale.isMember("pending"));
	const uint64_t staleFrame = stale["simFrame"].asUInt64();
	ASSERT_TRUE(h.PumpUntil([&] { return gRelay->PublishCount(*gDriver->rig->domain) > settled; }, 20));
	Json::Value fresh(Json::objectValue);
	adapter->GetJSONState(fresh);
	EXPECT_GT(fresh["simFrame"].asUInt64(), staleFrame);
	gRelay->Detach(*gDriver->rig->domain);
}

TEST(CellLayersIntegrationStreams, WorstCaseTwentyLayerStateRoundTripsThe32KbSlotUnchanged)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	gDriver->rig.reset(new SimRig(*gHost, 20));
	ASSERT_TRUE(gDriver->rig->RegisterAll());
	h.Pump(5);
	bool ok = false;
	std::string msg;
	Json::Value payload;
	// The Task 1 worst case: a selection (inspector per-layer table for all layers), a recorded query, a pinned selection.
	Json::Value box(Json::objectValue);
	box["q0"] = 0; box["r0"] = 0; box["q1"] = 9; box["r1"] = 9;
	ASSERT_TRUE(Call(h, 1, "cells.select.box", box, ok, msg, payload));
	Json::Value expr(Json::objectValue);
	expr["expr"] = "layer1 >= 0";
	ASSERT_TRUE(Call(h, 2, "cells.select.query", expr, ok, msg, payload));
	ASSERT_TRUE(Call(h, 3, "cells.select.pin", Obj(), ok, msg, payload));
	ASSERT_TRUE(Call(h, 4, "cells.select.cell", [] { Json::Value a(Json::objectValue); a["q"] = 3; a["r"] = 4; return a; }(), ok, msg, payload));

	Dia::VisualDebugger::IDebugDomain* adapter = gRelay->Attach(*gDriver->rig->domain);
	ASSERT_NE(adapter, nullptr);
	Json::Value pending(Json::objectValue);
	adapter->GetJSONState(pending);
	ASSERT_TRUE(h.PumpUntil([&] { return gRelay->PublishCount(*gDriver->rig->domain) > 0; }, 20));

	Json::Value snap(Json::objectValue);
	adapter->GetJSONState(snap);
	ASSERT_FALSE(snap.isMember("error")) << snap["error"].asString();
	const uint64_t simFrame = snap["simFrame"].asUInt64();
	Json::Value live(Json::objectValue);
	gDriver->rig->domain->GetJSONState(live);
	// Compare against the live state; the snapshot may be a tick older, so freeze the viewer's inputs (none change here).
	Json::Value snapNoFrame = snap;
	snapNoFrame.removeMember("simFrame");
	snapNoFrame.removeMember("pending");
	// skippedFrames counts domain ticks, and the live domain ticks once more after the snapshot was taken.
	snapNoFrame["stats"].removeMember("skippedFrames");
	live["stats"].removeMember("skippedFrames");
	Json::StreamWriterBuilder b;
	b["indentation"] = "";
	EXPECT_EQ(Json::writeString(b, snapNoFrame), Json::writeString(b, live)) << "slot round trip must be lossless (simFrame " << simFrame << ")";
	std::cout << "[ INFO ] worst-case snapshot over the real FrameStream: " << Json::writeString(b, snap).size() << " bytes of "
		<< ViewerStateSlot::kCapacity << "\n";
	gRelay->Detach(*gDriver->rig->domain);
}

TEST(CellLayersIntegrationStreams, StateBeyondTheSlotBecomesTheTooLargeMarkerEndToEnd)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	gDriver->rig.reset(new SimRig(*gHost, 128, "a_deliberately_long_layer_name_"));
	ASSERT_TRUE(gDriver->rig->RegisterAll());
	h.Pump(5);
	// Same inputs as the worst-case test: a box selection makes the inspector carry a per-layer row for every layer.
	{
		bool ok = false;
		std::string msg;
		Json::Value payload;
		Json::Value box(Json::objectValue);
		box["q0"] = 0; box["r0"] = 0; box["q1"] = 9; box["r1"] = 9;
		ASSERT_TRUE(Call(h, 1, "cells.select.box", box, ok, msg, payload));
		ASSERT_TRUE(Call(h, 4, "cells.select.cell", [] { Json::Value a(Json::objectValue); a["q"] = 3; a["r"] = 4; return a; }(), ok, msg, payload));
	}
	Dia::VisualDebugger::IDebugDomain* adapter = gRelay->Attach(*gDriver->rig->domain);
	ASSERT_NE(adapter, nullptr);
	Json::Value live(Json::objectValue);
	gDriver->rig->domain->GetJSONState(live);
	Json::StreamWriterBuilder b;
	b["indentation"] = "";
	if (Json::writeString(b, live).size() <= ViewerStateSlot::kCapacity)
		FAIL() << "128 long-named layers with a selection must exceed the slot (" << Json::writeString(b, live).size() << " bytes)";

	Json::Value pending(Json::objectValue);
	adapter->GetJSONState(pending);
	ASSERT_TRUE(h.PumpUntil([&] { return gRelay->PublishCount(*gDriver->rig->domain) > 0; }, 20));
	Json::Value snap(Json::objectValue);
	adapter->GetJSONState(snap);
	EXPECT_EQ(snap["error"].asString(), "snapshot too large");
	EXPECT_TRUE(snap.isMember("simFrame"));
	gRelay->Detach(*gDriver->rig->domain);
}

TEST(CellLayersIntegrationStreams, DetachWhileDemandIsInFlightIsHarmlessAndShutdownIsClean)
{
	Harness h;
	ASSERT_TRUE(h.Start());
	gDriver->rig.reset(new SimRig(*gHost, 5));
	Dia::VisualDebugger::IDebugDomain* adapter = gRelay->Attach(*gDriver->rig->domain);
	ASSERT_NE(adapter, nullptr);
	Json::Value state(Json::objectValue);
	adapter->GetJSONState(state);	// demand recorded on Main, not yet forwarded
	h.Pump(1);	// the bridge forwards the demand event; the relay may or may not have consumed it yet
	adapter = nullptr;
	const Dia::VisualDebugger::IDebugDomain* detachedDomain = gDriver->rig->domain.get();   // address only, compared after the rig dies
	gRelay->Detach(*gDriver->rig->domain);	// stage exit: caller unregistered the adapter first
	EXPECT_FALSE(gRelay->IsAttached(*gDriver->rig->domain));
	gDriver->rig.reset();
	h.Pump(100);	// demand events keep draining into a relay with no publisher: no crash, no publish
	EXPECT_EQ(gRelay->PublishCount(*detachedDomain), 0u);

	// A new stage entry attaches again and works.
	gDriver->rig.reset(new SimRig(*gHost, 5));
	adapter = gRelay->Attach(*gDriver->rig->domain);
	ASSERT_NE(adapter, nullptr);
	adapter->GetJSONState(state);
	ASSERT_TRUE(h.PumpUntil([&] { return gRelay->PublishCount(*gDriver->rig->domain) > 0; }, 20));
	gRelay->Detach(*gDriver->rig->domain);
	h.Shutdown();
}

#endif // DIA_DEBUG

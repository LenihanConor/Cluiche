#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaDebugServer/DebugServer.h>
#include <DiaDebugServer/ISimRequestSink.h>
#include <DiaDebugServer/SimHostedTypes.h>
#include <DiaDebugServer/SimQueryHost.h>
#include <DiaDebugProtocol/DiaDebugProtocol.h>
#include <DiaProtobuf/ProtoStructConverter.h>

#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/MirrorQueries.h>
#include <DiaCellLayers/ViewerStateSnapshot.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/CellPickerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaPathfinding/HexTopology.h>
#include <Modules/TestStages/CellLayersPointerQueue.h>
#include <Types/SimRegistrationMessage.h>

#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

// Cell Layers integration (Streams Migration end-of-implementation pass): the REAL SimQueryHost, REAL CellLayerViewerDomain and
// MirrorQueries, and the REAL Main-side DebugServer dispatch, joined by the same registrar seam the stage uses. The two EventStreams
// are replaced by an in-memory queue pair (the stream-backed path is covered in TestCellLayersIntegrationStreams.cpp). The sim
// side (host.Update) runs on a second thread, exactly as the Sim PU does.

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

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
using Dia::DebugServer::DebugServer;
using Dia::DebugServer::DebugServerTestAccess;
using Dia::DebugServer::SimKind;
using Dia::DebugServer::SimQueryHost;
using Dia::DebugServer::SimReplyEvent;
using Dia::DebugServer::SimRequestEvent;
using Dia::Graphics::CellFieldRegistry;
using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::HexTopology;

namespace
{
	const char* const kMirrorNames[] = { "cells.layer.list", "cells.layer.values", "cells.select.get", "cells.select.stats" };
	const char* const kViewerNames[] = {
		"cells.layer.base_by_id", "cells.overlay.toggle_by_id",
		"cells.select.cell", "cells.select.toggle", "cells.select.box", "cells.select.poly",
		"cells.select.category", "cells.select.cells", "cells.select.clear",
		"cells.select.query", "cells.query.check",
		"cells.select.tool", "cells.select.group", "cells.select.pin", "cells.select.unpin",
		"cells.inspect", "cells.inspector.expand" };
	const char* const kPointerNames[] = {
		"cluichetest.celllayers.pointer_frames", "cluichetest.celllayers.pointer_pending", "cluichetest.celllayers.cell_pixel" };

	struct WireSent
	{
		int connId;
		dia::debug::DebugMessage msg;
	};
	void CaptureSend(void* ctx, int connId, const dia::debug::DebugMessage& msg)
	{
		static_cast<std::vector<WireSent>*>(ctx)->push_back(WireSent{ connId, msg });
	}

	// Request sink (Main) + request source + reply sink (Sim) over two in-memory queues.
	class Pipe
		: public Dia::DebugServer::ISimRequestSink
		, public Dia::DebugServer::ISimRequestSource
		, public Dia::DebugServer::ISimReplySink
	{
	public:
		std::deque<SimRequestEvent> requests;
		std::deque<SimReplyEvent> replies;
		bool PostRequest(const SimRequestEvent& r) override { requests.push_back(r); return true; }
		bool TryPop(SimRequestEvent& out) override
		{
			if (requests.empty()) return false;
			out = requests.front();
			requests.pop_front();
			return true;
		}
		bool PostReply(const SimReplyEvent& r) override { replies.push_back(r); return true; }
	};

	// The registrar the stage uses (SimQueryHostModule): registers the handler on the real SimQueryHost and announces the name to the
	// Main-side server through the real registration message (Build -> Parse -> RegisterSimHosted/UnregisterSimHosted). Every handler
	// run is recorded (thread, count) by wrapping it.
	class HostRegistrar : public IMirrorQueryRegistrar
	{
	public:
		HostRegistrar(SimQueryHost& host, DebugServer& server) : mHost(host), mServer(server) {}

		bool Register(const char* name, MirrorQueryHandler h) override { return Add(name, SimKind::Query, std::move(h)); }
		bool RegisterCommand(const char* name, MirrorQueryHandler h) override { return Add(name, SimKind::Command, std::move(h)); }
		void Unregister(const char* name) override
		{
			if (!mHost.Has(StringCRC(name))) return;
			Announce(Cluiche::AppFlow::SimRegistrationOp::Unregister, name, SimKind::Query);
			mHost.UnregisterQuery(StringCRC(name));
		}

		std::set<std::thread::id> threads;
		std::map<std::string, int> calls;

	private:
		bool Add(const char* name, SimKind kind, MirrorQueryHandler h)
		{
			auto wrapped = [this, n = std::string(name), h = std::move(h)](const Json::Value& a) -> Json::Value
			{
				threads.insert(std::this_thread::get_id());
				++calls[n];
				return h(a);
			};
			const bool ok = (kind == SimKind::Command) ? mHost.RegisterCommand(StringCRC(name), wrapped) : mHost.RegisterQuery(StringCRC(name), wrapped);
			if (ok) Announce(Cluiche::AppFlow::SimRegistrationOp::Register, name, kind);
			return ok;
		}
		void Announce(Cluiche::AppFlow::SimRegistrationOp op, const char* name, SimKind kind)
		{
			SimReplyEvent msg;
			std::memset(&msg, 0, sizeof(msg));
			ASSERT_TRUE(Cluiche::AppFlow::BuildSimRegistrationReply(op, name, kind, msg)) << name;
			const Cluiche::AppFlow::SimRegistration reg = Cluiche::AppFlow::ParseSimRegistrationReply(msg);
			if (reg.op == Cluiche::AppFlow::SimRegistrationOp::Register)
				EXPECT_TRUE(mServer.RegisterSimHosted(StringCRC(reg.name), reg.kind)) << name;
			else if (reg.op == Cluiche::AppFlow::SimRegistrationOp::Unregister)
				mServer.UnregisterSimHosted(StringCRC(reg.name));
			else
				ADD_FAILURE() << "registration message did not round-trip: " << name;
		}
		SimQueryHost& mHost;
		DebugServer& mServer;
	};

	struct Reply
	{
		bool success = false;
		std::string message;
		Json::Value payload;
		size_t jsonBytes = 0;	// size of the SimReplyEvent json on the stream (0 for server-side errors)
	};

	std::string Compact(const Json::Value& v)
	{
		Json::StreamWriterBuilder b;
		b["indentation"] = "";
		return Json::writeString(b, v);
	}

	std::vector<uint8_t> DecodeBase64(const std::string& text)
	{
		std::vector<uint8_t> out;
		auto val = [](char c) -> int
		{
			if (c >= 'A' && c <= 'Z') return c - 'A';
			if (c >= 'a' && c <= 'z') return c - 'a' + 26;
			if (c >= '0' && c <= '9') return c - '0' + 52;
			if (c == '+') return 62;
			if (c == '/') return 63;
			return -1;
		};
		for (size_t i = 0; i + 3 < text.size(); i += 4)
		{
			int pad = 0;
			uint32_t v = 0;
			for (int k = 0; k < 4; ++k)
			{
				const char c = text[i + k];
				int d = 0;
				if (c == '=') ++pad; else d = val(c);
				v = (v << 6) | static_cast<uint32_t>(d < 0 ? 0 : d);
			}
			out.push_back(static_cast<uint8_t>(v >> 16));
			if (pad < 2) out.push_back(static_cast<uint8_t>(v >> 8));
			if (pad < 1) out.push_back(static_cast<uint8_t>(v));
		}
		return out;
	}

	// Real Large grid (104 x 64) with 20 float layers; the real viewer, mirror queries and sim host behind a real DebugServer.
	struct WireRig
	{
		static const uint32_t kLayers = 20;
		static const uint32_t kW = 104;
		static const uint32_t kH = 64;
		static const uint32_t kCells = kW * kH;

		TestGrid grid{ kW, kH };
		CellFieldRegistry cellField;
		std::vector<std::unique_ptr<StubProvider>> provs;
		std::vector<std::string> names;
		FakeDebugDraw draw;
		CellLayerRegistry registry{ nullptr, &FakePuIdFn };
		LayerUploader uploader{ cellField, nullptr };
		std::unique_ptr<CellLayerViewerDomain> domain;
		std::unique_ptr<MirrorQueries> queries;

		DebugServer server;
		Pipe pipe;
		SimQueryHost host;
		std::vector<WireSent> sent;
		std::unique_ptr<HostRegistrar> registrar;
		CluicheTest::PointerFrameQueue pointerQueue;

		explicit WireRig(bool registerAll = true)
		{
			FakePuId() = 1;
			for (uint32_t i = 0; i < kLayers; ++i)
			{
				names.push_back("layer" + std::to_string(i));
				provs.emplace_back(new StubProvider());
				provs.back()->values.resize(kCells);
				for (uint32_t c = 0; c < kCells; ++c)
					provs.back()->values[c] = static_cast<float>((c * (i + 3)) % 997) * 0.37f - 5.0f;
				CellLayerDesc d = provs.back()->MakeDesc(names.back().c_str(), &grid.desc, CellValueKind::Float, ("cells/" + names.back()).c_str());
				EXPECT_EQ(CellRegisterResult::Ok, registry.Register(d));
			}
			domain.reset(new CellLayerViewerDomain(registry, uploader, nullptr));
			domain->Update(draw);
			queries.reset(new MirrorQueries(registry, *domain, nullptr));

			server.SetSendHook(&CaptureSend, &sent);
			server.SetSimRequestSink(&pipe);
			host.SetRequestSource(&pipe);
			host.SetReplySink(&pipe);
			registrar.reset(new HostRegistrar(host, server));
			if (registerAll) RegisterAll();
		}

		~WireRig()
		{
			if (domain) UnregisterAll();
			host.SetRequestSource(nullptr);
			host.SetReplySink(nullptr);
		}

		Json::Value CellPixel(const Json::Value& params)
		{
			Json::Value out(Json::objectValue);
			out["x"] = params["q"];
			out["y"] = params["r"];
			return out;
		}

		void RegisterAll()
		{
			ASSERT_TRUE(RegisterMirrorQueries(*registrar, *queries));
			ASSERT_TRUE(domain->RegisterCommands(*registrar));
			ASSERT_TRUE(CluicheTest::RegisterPointerInjectionCommands(*registrar, pointerQueue,
				[this](const Json::Value& p) { return CellPixel(p); }));
		}
		void UnregisterAll()
		{
			CluicheTest::UnregisterPointerInjectionCommands(*registrar);
			domain->UnregisterCommands(*registrar);
			UnregisterMirrorQueries(*registrar);
		}

		// Main: queue a request (HandleCommand). Does not run the sim.
		void Post(int conn, const char* name, const Json::Value& args = Json::Value(Json::objectValue))
		{
			dia::debug::DebugMessage m;
			m.set_type(dia::debug::MESSAGE_TYPE_COMMAND_REQUEST);
			auto* c = m.mutable_command_request();
			c->set_command(name);
			Dia::Proto::JsonValueToProtoStruct(args, c->mutable_payload());
			DebugServerTestAccess::HandleCommand(server, conn, m);
		}

		// Sim: one host.Update on a second thread, then Main drains the reply queue into the server. Returns the max reply json size.
		size_t StepSimAndDrain()
		{
			std::thread sim([this] { host.Update(); });
			sim.join();
			size_t maxBytes = 0;
			while (!pipe.replies.empty())
			{
				const SimReplyEvent& r = pipe.replies.front();
				const size_t n = std::strlen(r.json);
				if (n > maxBytes) maxBytes = n;
				EXPECT_LT(n, static_cast<size_t>(Dia::DebugServer::kSimPayloadBytes));
				lastReplyBytes = n;
				server.CompleteSimReply(r);
				pipe.replies.pop_front();
			}
			return maxBytes;
		}

		Reply Call(const char* name, const Json::Value& args = Json::Value(Json::objectValue))
		{
			const size_t before = sent.size();
			Post(1, name, args);
			lastReplyBytes = 0;
			StepSimAndDrain();
			Reply r;
			if (sent.size() != before + 1)
			{
				ADD_FAILURE() << name << ": expected exactly one response, got " << (sent.size() - before);
				return r;
			}
			const auto& resp = sent.back().msg.command_response();
			r.success = resp.success();
			r.message = resp.message();
			r.payload = Dia::Proto::ProtoStructToJsonValue(resp.payload());
			r.jsonBytes = lastReplyBytes;
			return r;
		}

		size_t lastReplyBytes = 0;
	};

	Json::Value Box(int q0, int r0, int q1, int r1)
	{
		Json::Value a(Json::objectValue);
		a["q0"] = q0; a["r0"] = r0; a["q1"] = q1; a["r1"] = r1;
		return a;
	}
}

TEST(CellLayersIntegrationWire, AllTwentyFourNamesAreRoutedAndRunOnTheSimThreadOnly)
{
	WireRig rig;
	std::vector<std::string> all;
	for (const char* n : kMirrorNames) all.push_back(n);
	for (const char* n : kViewerNames) all.push_back(n);
	for (const char* n : kPointerNames) all.push_back(n);
	ASSERT_EQ(all.size(), 24u);

	for (const std::string& n : all)
	{
		const Reply r = rig.Call(n.c_str());
		EXPECT_TRUE(r.success) << n << " was not routed: " << r.message;
		EXPECT_NE(r.message, "unknown name") << n;
		EXPECT_EQ(rig.registrar->calls[n], 1) << n << " handler must run exactly once";
		EXPECT_TRUE(r.payload.isObject()) << n;
	}
	ASSERT_FALSE(rig.registrar->threads.empty());
	EXPECT_EQ(rig.registrar->threads.count(std::this_thread::get_id()), 0u) << "no handler may run on the Main (calling) thread";
}

TEST(CellLayersIntegrationWire, MirrorQueriesOnLargeTwentyLayerGridStayUnderTheReplyCap)
{
	WireRig rig;
	const Reply list = rig.Call("cells.layer.list");
	ASSERT_TRUE(list.success);
	EXPECT_LT(list.jsonBytes, static_cast<size_t>(Dia::DebugServer::kSimPayloadBytes));
	ASSERT_TRUE(list.payload.isMember("grids"));
	EXPECT_EQ(list.payload["grids"][0]["layers"].size(), WireRig::kLayers);
	EXPECT_EQ(list.payload["grids"][0]["width"].asUInt(), WireRig::kW);
	EXPECT_EQ(list.payload["grids"][0]["height"].asUInt(), WireRig::kH);
	EXPECT_TRUE(list.payload.isMember("layersVersion"));

	// A selection of the whole grid, then the stats for every layer.
	ASSERT_TRUE(rig.Call("cells.select.box", Box(0, 0, 200, 200)).success);
	Json::Value all(Json::objectValue);
	all["layers"] = "all";
	const Reply stats = rig.Call("cells.select.stats", all);
	ASSERT_TRUE(stats.success) << stats.message;
	EXPECT_FALSE(stats.payload.isMember("error")) << Compact(stats.payload["error"]);
	EXPECT_EQ(stats.payload["count"].asUInt(), WireRig::kCells);
	EXPECT_LT(stats.jsonBytes, static_cast<size_t>(Dia::DebugServer::kSimPayloadBytes));
	std::cout << "[ INFO ] cells.layer.list " << list.jsonBytes << " bytes, cells.select.stats(all, 20 layers) " << stats.jsonBytes << " bytes\n";
}

TEST(CellLayersIntegrationWire, LayerValuesPagedOverSevenCallsReassemblesBitExact)
{
	WireRig rig;
	const uint32_t layer = 7;
	std::vector<float> got;
	uint32_t calls = 0;
	for (uint32_t offset = 0; offset < WireRig::kCells; offset += MirrorQueries::kMaxValuesChunk)
	{
		Json::Value a(Json::objectValue);
		a["layer"] = rig.names[layer];
		a["offset"] = offset;
		a["count"] = MirrorQueries::kMaxValuesChunk;
		const Reply r = rig.Call("cells.layer.values", a);
		ASSERT_TRUE(r.success) << r.message;
		ASSERT_FALSE(r.payload.isMember("error")) << Compact(r.payload);
		EXPECT_LT(r.jsonBytes, static_cast<size_t>(Dia::DebugServer::kSimPayloadBytes));
		EXPECT_EQ(r.payload["total"].asUInt(), WireRig::kCells);
		const std::vector<uint8_t> bytes = DecodeBase64(r.payload["data"].asString());
		ASSERT_EQ(bytes.size(), static_cast<size_t>(r.payload["count"].asUInt()) * 4u);
		const size_t first = got.size();
		got.resize(first + bytes.size() / 4);
		std::memcpy(&got[first], bytes.data(), bytes.size());
		++calls;
	}
	EXPECT_EQ(calls, 7u);
	ASSERT_EQ(got.size(), WireRig::kCells);
	EXPECT_EQ(0, std::memcmp(got.data(), rig.provs[layer]->values.data(), got.size() * sizeof(float))) << "paged values must be bit-exact";
}

TEST(CellLayersIntegrationWire, SelectionGetPagesTheWholeGridAscendingWithAStableHash)
{
	WireRig rig;
	ASSERT_TRUE(rig.Call("cells.select.box", Box(0, 0, 200, 200)).success);
	std::vector<uint32_t> indices;
	std::string hash;
	uint32_t pages = 0;
	for (uint32_t offset = 0; offset < WireRig::kCells; offset += MirrorQueries::kMaxSelectionChunk)
	{
		Json::Value a(Json::objectValue);
		a["offset"] = offset;
		a["count"] = MirrorQueries::kMaxSelectionChunk;
		const Reply r = rig.Call("cells.select.get", a);
		ASSERT_TRUE(r.success) << r.message;
		ASSERT_FALSE(r.payload.isMember("error")) << Compact(r.payload);
		EXPECT_LT(r.jsonBytes, static_cast<size_t>(Dia::DebugServer::kSimPayloadBytes));
		EXPECT_EQ(r.payload["total"].asUInt(), WireRig::kCells);
		if (hash.empty()) hash = r.payload["hash"].asString();
		EXPECT_EQ(r.payload["hash"].asString(), hash) << "page " << pages;
		for (const Json::Value& v : r.payload["indices"]) indices.push_back(v.asUInt());
		++pages;
	}
	EXPECT_EQ(pages, 7u);
	ASSERT_EQ(indices.size(), WireRig::kCells);
	for (uint32_t i = 0; i < indices.size(); ++i) { ASSERT_EQ(indices[i], i); }
}

TEST(CellLayersIntegrationWire, CommandThenQueryQueuedTogetherAreServedInOrderInOneSimUpdate)
{
	WireRig rig;
	const size_t before = rig.sent.size();
	rig.Post(1, "cells.select.get");	// total 0 at this point
	rig.Post(2, "cells.select.box", Box(0, 0, 1, 1));
	rig.Post(3, "cells.select.get");	// must see the box
	rig.Post(4, "cells.select.clear");
	rig.Post(5, "cells.select.get");	// must see the clear
	ASSERT_EQ(rig.pipe.requests.size(), 5u);
	rig.StepSimAndDrain();	// one host.Update on the sim thread serves all five in FIFO order
	ASSERT_EQ(rig.sent.size(), before + 5);
	auto payload = [&](size_t i) { return Dia::Proto::ProtoStructToJsonValue(rig.sent[before + i].msg.command_response().payload()); };
	EXPECT_EQ(payload(0)["total"].asUInt(), 0u);
	EXPECT_TRUE(payload(1)["ok"].asBool());
	EXPECT_EQ(payload(2)["total"].asUInt(), 4u);
	EXPECT_EQ(payload(2)["indices"].size(), 4u);
	EXPECT_EQ(payload(4)["total"].asUInt(), 0u);
	for (size_t i = 0; i < 5; ++i) { EXPECT_EQ(rig.sent[before + i].connId, static_cast<int>(i) + 1) << "reply routed to its own connection"; }
}

TEST(CellLayersIntegrationWire, ErrorPayloadsKeepTheirShapeAcrossTheWire)
{
	WireRig rig;
	Json::Value badLayer(Json::objectValue);
	badLayer["layer"] = "no.such.layer";
	Reply r = rig.Call("cells.layer.values", badLayer);
	EXPECT_TRUE(r.success) << "a handler-level error is a normal reply carrying {error}";
	ASSERT_TRUE(r.payload["error"].isString());

	Json::Value past(Json::objectValue);
	past["offset"] = WireRig::kCells + 5;
	r = rig.Call("cells.select.get", past);
	ASSERT_TRUE(r.payload["error"].isString());

	Json::Value bad(Json::objectValue);
	bad["layers"] = 5;
	r = rig.Call("cells.select.stats", bad);
	ASSERT_TRUE(r.payload["error"].isString());

	Json::Value neg(Json::objectValue);
	neg["layer"] = rig.names[0];
	neg["offset"] = -1;
	r = rig.Call("cells.layer.values", neg);
	ASSERT_TRUE(r.payload["error"].isString());
}

TEST(CellLayersIntegrationWire, SelectCellsArgsPastTheRequestCapAreRejectedAndTheSelectionIsUntouched)
{
	WireRig rig;
	auto pairs = [](uint32_t n)
	{
		Json::Value args(Json::objectValue);
		Json::Value arr(Json::arrayValue);
		for (uint32_t i = 0; i < n; ++i)
		{
			Json::Value p(Json::arrayValue);
			p.append(static_cast<int>(i % WireRig::kW));
			p.append(static_cast<int>(i / WireRig::kW));
			arr.append(p);
		}
		args["cells"] = arr;
		return args;
	};
	const Reply ok = rig.Call("cells.select.cells", pairs(500));	// about 4.5 KB of args
	EXPECT_TRUE(ok.success) << ok.message;
	EXPECT_EQ(rig.Call("cells.select.get").payload["total"].asUInt(), 500u);

	const Json::Value huge = pairs(WireRig::kCells);	// about 55 KB: past the 16 KB request cap
	ASSERT_GT(Compact(huge).size(), static_cast<size_t>(Dia::DebugServer::kSimPayloadBytes));
	const Reply rejected = rig.Call("cells.select.cells", huge);
	EXPECT_FALSE(rejected.success) << "an oversize request must fail loudly, never be truncated silently";
	EXPECT_FALSE(rejected.message.empty());
	EXPECT_EQ(rig.Call("cells.select.get").payload["total"].asUInt(), 500u) << "the rejected call must not change the selection";
}

TEST(CellLayersIntegrationWire, DuplicateNameIsRejectedRolledBackAndUnroutedAtTheServer)
{
	WireRig rig(/*registerAll*/ false);
	// A foreign handler already owns one viewer command name.
	ASSERT_TRUE(rig.registrar->RegisterCommand("cells.select.clear", [](const Json::Value&) { Json::Value v(Json::objectValue); v["foreign"] = true; return v; }));
	EXPECT_FALSE(rig.domain->RegisterCommands(*rig.registrar));
	for (const char* n : kViewerNames)
	{
		if (std::string(n) == "cells.select.clear") continue;
		EXPECT_FALSE(rig.host.Has(StringCRC(n))) << n << " must be rolled back on the host";
		const Reply r = rig.Call(n);
		EXPECT_FALSE(r.success) << n << " must not be routed at the server after the rollback";
	}
	// The first handler stays.
	const Reply r = rig.Call("cells.select.clear");
	EXPECT_TRUE(r.success);
	EXPECT_TRUE(r.payload["foreign"].asBool());

	// cells.layer.list: a second registration of the mirror queries is rejected too and leaves the first working.
	ASSERT_TRUE(RegisterMirrorQueries(*rig.registrar, *rig.queries));
	EXPECT_FALSE(RegisterMirrorQueries(*rig.registrar, *rig.queries));
	EXPECT_TRUE(rig.Call("cells.layer.list").success);
	rig.registrar->Unregister("cells.select.clear");
}

TEST(CellLayersIntegrationWire, TeardownLeavesNoDanglingHandlerAndInFlightRequestsGetUnknownName)
{
	std::unique_ptr<WireRig> rig(new WireRig());
	ASSERT_TRUE(rig->Call("cells.select.get").success);

	// A request is queued, then the stage tears down (unregister before the domain dies), then the sim steps.
	rig->Post(1, "cells.select.box", Box(0, 0, 1, 1));
	rig->Post(2, "cells.select.get");
	const int boxCallsBefore = rig->registrar->calls["cells.select.box"];
	rig->UnregisterAll();
	rig->StepSimAndDrain();
	ASSERT_GE(rig->sent.size(), 3u);
	for (size_t i = rig->sent.size() - 2; i < rig->sent.size(); ++i)
	{
		const auto& resp = rig->sent[i].msg.command_response();
		EXPECT_FALSE(resp.success());
		EXPECT_EQ(resp.message(), "unknown name") << "in-flight request for a torn-down name";
	}
	EXPECT_EQ(rig->registrar->calls["cells.select.box"], boxCallsBefore);

	// The viewer and queries die; stale names answer with an error, never run a handler that captured a dead this.
	rig->queries.reset();
	rig->domain.reset();
	const int total = [&] { int t = 0; for (auto& kv : rig->registrar->calls) t += kv.second; return t; }();
	for (const char* n : kViewerNames)
		EXPECT_FALSE(rig->Call(n).success) << n;
	for (const char* n : kMirrorNames)
		EXPECT_FALSE(rig->Call(n).success) << n;
	int after = 0;
	for (auto& kv : rig->registrar->calls) after += kv.second;
	EXPECT_EQ(total, after) << "no handler may run after teardown";
}

TEST(CellLayersIntegrationWire, ReEntryAfterTeardownRegistersTheSameNamesAgain)
{
	WireRig rig;
	ASSERT_TRUE(rig.Call("cells.layer.list").success);
	rig.UnregisterAll();
	EXPECT_FALSE(rig.Call("cells.layer.list").success);
	rig.RegisterAll();
	EXPECT_TRUE(rig.Call("cells.layer.list").success);
	EXPECT_TRUE(rig.Call("cells.select.clear").success);
	EXPECT_TRUE(rig.Call("cluichetest.celllayers.pointer_pending").success);
}

TEST(CellLayersIntegrationWire, PointerFramesOverTheWireSelectACellVisibleToQueryAndSnapshot)
{
	WireRig rig;
	CellPickerView view;
	view.viewportWidth = 1280; view.viewportHeight = 720; view.cameraX = 50; view.cameraY = 40; view.zoom = 8.0f;
	const int q = 12, r = 8;
	const Dia::Maths::Vector2D c = HexTopology::CellCenter(CellCoord{ q, r }, 1.0f);
	float sx = 0, sy = 0;
	WorldToScreen(view, c.X(), c.Y(), sx, sy);

	Json::Value frames(Json::objectValue);
	Json::Value arr(Json::arrayValue);
	for (int i = 0; i < 3; ++i)
	{
		Json::Value f(Json::objectValue);
		f["x"] = sx; f["y"] = sy;
		f["left"] = (i == 1);	// move, press, release
		arr.append(f);
	}
	frames["frames"] = arr;
	const Reply queued = rig.Call("cluichetest.celllayers.pointer_frames", frames);
	ASSERT_TRUE(queued.success);
	EXPECT_EQ(queued.payload["queued"].asUInt(), 3u);
	EXPECT_EQ(rig.Call("cluichetest.celllayers.pointer_pending").payload["pending"].asUInt(), 3u);

	// The stage's Sim update: one queued frame per tick into the viewer (here driven on the thread that owns the sim side).
	std::thread sim([&] {
		CluicheTest::PointerFrameQueue::Frame f;
		while (rig.pointerQueue.TryPop(f))
		{
			rig.domain->SubmitPointerFrame(f, view);
			rig.domain->Update(rig.draw);
		}
	});
	sim.join();
	EXPECT_EQ(rig.Call("cluichetest.celllayers.pointer_pending").payload["pending"].asUInt(), 0u);

	const Reply got = rig.Call("cells.select.get");
	ASSERT_TRUE(got.success);
	EXPECT_EQ(got.payload["total"].asUInt(), 1u);
	ASSERT_EQ(got.payload["indices"].size(), 1u);
	EXPECT_EQ(got.payload["indices"][0].asUInt(), static_cast<uint32_t>(r * WireRig::kW + q));

	// The same selection is visible in the snapshot the panel reads.
	class SnapSink : public IViewerStateSink
	{
	public:
		void Write(const char* json, uint32_t length) override { text.assign(json, length); }
		std::string text;
	} sink;
	ViewerStateSnapshotPublisher pub(*rig.domain, sink);
	pub.Arm(1);
	pub.Update(2);
	Json::Value state;
	Json::CharReaderBuilder b;
	std::string errs;
	std::unique_ptr<Json::CharReader> reader(b.newCharReader());
	ASSERT_TRUE(reader->parse(sink.text.data(), sink.text.data() + sink.text.size(), &state, &errs)) << errs;
	EXPECT_EQ(state["selection"]["count"].asUInt(), 1u);
}

TEST(CellLayersIntegrationWire, WorstCaseStateSnapshotFitsTheSlotRoundTrip)
{
	WireRig rig;
	// The Task 1 worst case: Large grid, 20 layers, one selected cell (inspector table), a recorded query.
	ASSERT_TRUE(rig.Call("cells.select.cell", [] { Json::Value a(Json::objectValue); a["q"] = 3; a["r"] = 4; return a; }()).success);
	rig.domain->Update(rig.draw);
	class SnapSink : public IViewerStateSink
	{
	public:
		void Write(const char* json, uint32_t length) override { text.assign(json, length); }
		std::string text;
	} sink;
	ViewerStateSnapshotPublisher pub(*rig.domain, sink);
	pub.Arm(1);
	pub.Update(2);
	ASSERT_FALSE(sink.text.empty());
	EXPECT_LE(sink.text.size(), ViewerStateSlot::kCapacity);
	EXPECT_EQ(sink.text.find("snapshot too large"), std::string::npos);
	std::cout << "[ INFO ] Large 20-layer one-cell snapshot: " << sink.text.size() << " bytes of " << ViewerStateSlot::kCapacity << "\n";
}

#endif // DIA_DEBUG

#include <gtest/gtest.h>
#include <DiaDebugServer/Testing/SimHelpTesting.h>

#include <DiaDebugServer/DebugServer.h>
#include <DiaDebugServer/ISimRequestSink.h>
#include <DiaDebugServer/SimHostedTypes.h>
#include <DiaDebugServer/SimPendingTable.h>
#include <DiaDebugProtocol/DiaDebugProtocol.h>
#include <DiaProtobuf/ProtoStructConverter.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstring>
#include <string>
#include <vector>

// Sim-hosted queries observation (Task 5): dia.debugserver.sim.* counters and Info/Warning logs.

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

namespace
{
	using Dia::Core::StringCRC;
	using Dia::DebugServer::DebugServer;
	using Dia::DebugServer::DebugServerTestAccess;
	using Dia::DebugServer::SimKind;
	using Dia::DebugServer::SimPendingTable;
	using Dia::DebugServer::SimReplyEvent;
	using Dia::DebugServer::SimReplyStatus;
	using Dia::DebugServer::SimRequestEvent;
	using Dia::CellLayers::Testing::LogCapture;

	uint64_t gObsNowMs = 0;
	uint64_t ObsFakeNow() { return gObsNowMs; }
	void ObsDiscardSend(void*, int, const dia::debug::DebugMessage&) {}

	class ObsFakeSink : public Dia::DebugServer::ISimRequestSink
	{
	public:
		bool accept = true;
		std::vector<SimRequestEvent> posted;
		bool PostRequest(const SimRequestEvent& r) override
		{
			if (!accept) return false;
			posted.push_back(r);
			return true;
		}
	};

	dia::debug::DebugMessage ObsMakeCommand(const char* name, const Json::Value* args = nullptr)
	{
		dia::debug::DebugMessage m;
		m.set_type(dia::debug::MESSAGE_TYPE_COMMAND_REQUEST);
		auto* c = m.mutable_command_request();
		c->set_command(name);
		if (args)
			Dia::Proto::JsonValueToProtoStruct(*args, c->mutable_payload());
		return m;
	}

	SimReplyEvent ObsMakeReply(uint32_t id, SimReplyStatus st, const char* json)
	{
		SimReplyEvent r;
		std::memset(&r, 0, sizeof(r));
		r.requestId = id;
		r.status = static_cast<uint32_t>(st);
		std::strncpy(r.json, json, sizeof(r.json) - 1);
		return r;
	}

	double Counter(const char* name)
	{
		auto* c = Dia::Observation::Metric::MetricRegistry::Instance().FindCounter(StringCRC(name));
		return c ? static_cast<double>(c->Value()) : 0.0;
	}

	const char* const kNames[] = {
		"dia.debugserver.sim.requests", "dia.debugserver.sim.replies", "dia.debugserver.sim.timeouts",
		"dia.debugserver.sim.busy", "dia.debugserver.sim.too_large", "dia.debugserver.sim.handler_failed" };
}

class SimHostedObservationTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		gObsNowMs = 10000;
		mServer.SetNowMsFunction(&ObsFakeNow);
		mServer.SetSendHook(&ObsDiscardSend, nullptr);
		mServer.SetSimRequestSink(&mSink);
		mServer.RegisterSimHosted(StringCRC("obs.q"), SimKind::Query, Dia::DebugServer::Testing::DefaultSimHelp());
		for (int i = 0; i < 6; ++i) mBase[i] = Counter(kNames[i]);
	}

	void Send(int conn, const Json::Value* args = nullptr)
	{
		DebugServerTestAccess::HandleCommand(mServer, conn, ObsMakeCommand("obs.q", args));
	}

	// Expect the six counters' deltas since SetUp: requests, replies, timeouts, busy, too_large, handler_failed.
	void ExpectDeltas(double req, double rep, double to, double busy, double big, double fail)
	{
		const double want[6] = { req, rep, to, busy, big, fail };
		for (int i = 0; i < 6; ++i)
			EXPECT_EQ(want[i], Counter(kNames[i]) - mBase[i]) << kNames[i];
	}

	DebugServer mServer;
	ObsFakeSink mSink;
	double mBase[6];
};

TEST_F(SimHostedObservationTest, PostedRequestCountsRequestsOnly)
{
	Send(1);
	ExpectDeltas(1, 0, 0, 0, 0, 0);
}

TEST_F(SimHostedObservationTest, OkReplyCountsReplies)
{
	Send(1);
	mServer.CompleteSimReply(ObsMakeReply(mSink.posted[0].requestId, SimReplyStatus::Ok, "{\"a\":1}"));
	ExpectDeltas(1, 1, 0, 0, 0, 0);
}

TEST_F(SimHostedObservationTest, TimeoutCountsTimeoutsOnce)
{
	Send(1);
	gObsNowMs += SimPendingTable::kTimeoutMs + 3000;
	DebugServerTestAccess::Sweep(mServer);
	DebugServerTestAccess::Sweep(mServer);
	ExpectDeltas(1, 0, 1, 0, 0, 0);
}

TEST_F(SimHostedObservationTest, NoSinkCountsBusy)
{
	mServer.SetSimRequestSink(nullptr);
	Send(1);
	ExpectDeltas(0, 0, 0, 1, 0, 0);
}

TEST_F(SimHostedObservationTest, PostRequestRefusedCountsBusy)
{
	mSink.accept = false;
	Send(1);
	ExpectDeltas(0, 0, 0, 1, 0, 0);
}

TEST_F(SimHostedObservationTest, TableFullCountsBusy)
{
	for (uint32_t i = 0; i < SimPendingTable::kCapacity; ++i) Send(1);
	ExpectDeltas(SimPendingTable::kCapacity, 0, 0, 0, 0, 0);
	Send(1);
	ExpectDeltas(SimPendingTable::kCapacity, 0, 0, 1, 0, 0);
}

TEST_F(SimHostedObservationTest, OversizedArgsCountTooLarge)
{
	Json::Value args;
	args["blob"] = std::string(Dia::DebugServer::kSimPayloadBytes + 16, 'x');
	Send(1, &args);
	ExpectDeltas(0, 0, 0, 0, 1, 0);
}

TEST_F(SimHostedObservationTest, TooLargeReplyCountsTooLarge)
{
	Send(1);
	mServer.CompleteSimReply(ObsMakeReply(mSink.posted[0].requestId, SimReplyStatus::TooLarge, ""));
	ExpectDeltas(1, 0, 0, 0, 1, 0);
}

TEST_F(SimHostedObservationTest, HandlerFailedStatusCountsHandlerFailed)
{
	Send(1);
	mServer.CompleteSimReply(ObsMakeReply(mSink.posted[0].requestId, SimReplyStatus::HandlerFailed, ""));
	ExpectDeltas(1, 0, 0, 0, 0, 1);
}

TEST_F(SimHostedObservationTest, UnparseableJsonCountsHandlerFailed)
{
	Send(1);
	mServer.CompleteSimReply(ObsMakeReply(mSink.posted[0].requestId, SimReplyStatus::Ok, "{not json"));
	ExpectDeltas(1, 0, 0, 0, 0, 1);
}

TEST_F(SimHostedObservationTest, LateReplyCountsNothing)
{
	mServer.CompleteSimReply(ObsMakeReply(9999, SimReplyStatus::Ok, "{}"));
	ExpectDeltas(0, 0, 0, 0, 0, 0);
}

TEST(SimHostedObservationLogs, SinkAttachAndDetachLogInfoOnce)
{
	DebugServer server;
	ObsFakeSink sink;
	LogCapture log("DebugServer");
	server.SetSimRequestSink(&sink);
	EXPECT_EQ(1u, log.Count(LogCapture::Level::kInfo, "sink attached"));
	EXPECT_EQ(0u, log.Count(LogCapture::Level::kInfo, "sink detached"));
	server.SetSimRequestSink(nullptr);
	EXPECT_EQ(1u, log.Count(LogCapture::Level::kInfo, "sink detached"));
	EXPECT_EQ(1u, log.Count(LogCapture::Level::kInfo, "sink attached"));
}

TEST(SimHostedObservationLogs, RegisterAndUnregisterLogInfo)
{
	DebugServer server;
	LogCapture log("DebugServer");
	ASSERT_TRUE(server.RegisterSimHosted(StringCRC("obs.named"), SimKind::Query, Dia::DebugServer::Testing::DefaultSimHelp()));
	EXPECT_EQ(1u, log.Count(LogCapture::Level::kInfo, "obs.named"));
	server.UnregisterSimHosted(StringCRC("obs.named"));
	EXPECT_EQ(2u, log.Count(LogCapture::Level::kInfo, "obs.named"));
	EXPECT_EQ(1u, log.Count(LogCapture::Level::kInfo, "unregistered"));
}

TEST(SimHostedObservationLogs, TimeoutLogsWarning)
{
	DebugServer server;
	ObsFakeSink sink;
	gObsNowMs = 10000;
	server.SetNowMsFunction(&ObsFakeNow);
	server.SetSendHook(&ObsDiscardSend, nullptr);
	server.SetSimRequestSink(&sink);
	server.RegisterSimHosted(StringCRC("obs.q"), SimKind::Query, Dia::DebugServer::Testing::DefaultSimHelp());
	DebugServerTestAccess::HandleCommand(server, 1, ObsMakeCommand("obs.q"));
	LogCapture log("DebugServer");
	gObsNowMs += SimPendingTable::kTimeoutMs + 1000;
	DebugServerTestAccess::Sweep(server);
	EXPECT_EQ(1u, log.Warnings("timed out"));
}

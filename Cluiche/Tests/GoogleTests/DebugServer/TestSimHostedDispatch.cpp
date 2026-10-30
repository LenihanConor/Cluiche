#include <gtest/gtest.h>

#include <DiaDebugServer/DebugServer.h>
#include <DiaDebugServer/ISimRequestSink.h>
#include <DiaDebugServer/SimHostedTypes.h>
#include <DiaDebugProtocol/DiaDebugProtocol.h>
#include <DiaProtobuf/ProtoStructConverter.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstring>
#include <string>
#include <vector>

namespace Dia
{
	namespace DebugServer
	{
		// Friend of DebugServer: drives the private message path without a socket.
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
	using Dia::DebugServer::SimReplyEvent;
	using Dia::DebugServer::SimReplyStatus;
	using Dia::DebugServer::SimRequestEvent;

	uint64_t gNowMs = 0;
	uint64_t FakeNow() { return gNowMs; }

	struct Sent
	{
		int connId;
		dia::debug::DebugMessage msg;
	};

	void CaptureSend(void* ctx, int connId, const dia::debug::DebugMessage& msg)
	{
		static_cast<std::vector<Sent>*>(ctx)->push_back(Sent{ connId, msg });
	}

	class FakeSink : public Dia::DebugServer::ISimRequestSink
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

	dia::debug::DebugMessage MakeCommand(const char* name, const Json::Value* args = nullptr)
	{
		dia::debug::DebugMessage m;
		m.set_type(dia::debug::MESSAGE_TYPE_COMMAND_REQUEST);
		auto* c = m.mutable_command_request();
		c->set_command(name);
		if (args)
			Dia::Proto::JsonValueToProtoStruct(*args, c->mutable_payload());
		return m;
	}

	SimReplyEvent MakeReply(uint32_t id, SimReplyStatus st, const char* json)
	{
		SimReplyEvent r;
		std::memset(&r, 0, sizeof(r));
		r.requestId = id;
		r.status = static_cast<uint32_t>(st);
		std::strncpy(r.json, json, sizeof(r.json) - 1);
		return r;
	}
}

class SimHostedDispatchTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		gNowMs = 10000;
		mServer.SetNowMsFunction(&FakeNow);
		mServer.SetSendHook(&CaptureSend, &mSent);
		mServer.SetSimRequestSink(&mSink);
	}

	void Send(int conn, const char* name, const Json::Value* args = nullptr)
	{
		DebugServerTestAccess::HandleCommand(mServer, conn, MakeCommand(name, args));
	}

	Json::Value PayloadOf(size_t i) const
	{
		return Dia::Proto::ProtoStructToJsonValue(mSent[i].msg.command_response().payload());
	}

	DebugServer mServer;
	FakeSink mSink;
	std::vector<Sent> mSent;
};

TEST_F(SimHostedDispatchTest, DeferralSendsNoReplyUntilCompleted)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	Send(3, "sim.q");
	EXPECT_TRUE(mSent.empty());
	ASSERT_EQ(mSink.posted.size(), 1u);

	mServer.CompleteSimReply(MakeReply(mSink.posted[0].requestId, SimReplyStatus::Ok, "{\"a\":1}"));
	ASSERT_EQ(mSent.size(), 1u);
	EXPECT_EQ(mSent[0].connId, 3);
}

TEST_F(SimHostedDispatchTest, PostRequestReceivesIdNameKindAndArgs)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.cmd"), SimKind::Command));
	Json::Value args;
	args["x"] = 5;
	Send(1, "sim.cmd", &args);
	ASSERT_EQ(mSink.posted.size(), 1u);
	EXPECT_NE(mSink.posted[0].requestId, 0u);
	EXPECT_EQ(mSink.posted[0].nameCrc, StringCRC("sim.cmd").Value());
	EXPECT_EQ(mSink.posted[0].kind, static_cast<uint32_t>(SimKind::Command));
	EXPECT_STREQ(mSink.posted[0].json, "{\"x\":5}");
}

TEST_F(SimHostedDispatchTest, NoArgsPostsEmptyObject)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	Send(1, "sim.q");
	ASSERT_EQ(mSink.posted.size(), 1u);
	EXPECT_STREQ(mSink.posted[0].json, "{}");
}

TEST_F(SimHostedDispatchTest, ReplyWireMatchesMainHostedQuery)
{
	mServer.GetQueryRegistry().Register(StringCRC("main.q"), [](const Json::Value&) {
		Json::Value v;
		v["a"] = 1;
		v["b"] = "hi";
		return v;
	});
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));

	Send(4, "main.q");
	Send(4, "sim.q");
	mServer.CompleteSimReply(MakeReply(mSink.posted[0].requestId, SimReplyStatus::Ok, "{\"a\":1,\"b\":\"hi\"}"));

	ASSERT_EQ(mSent.size(), 2u);
	const auto& mainResp = mSent[0].msg.command_response();
	const auto& simResp = mSent[1].msg.command_response();
	EXPECT_EQ(mSent[1].msg.type(), mSent[0].msg.type());
	EXPECT_EQ(simResp.command(), "sim.q");
	EXPECT_EQ(simResp.success(), mainResp.success());
	EXPECT_TRUE(simResp.success());
	EXPECT_EQ(simResp.message(), mainResp.message());
	EXPECT_EQ(PayloadOf(1), PayloadOf(0));
}

TEST_F(SimHostedDispatchTest, TimeoutRepliesSimTimeout)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	Send(2, "sim.q");
	gNowMs += 1999;
	DebugServerTestAccess::Sweep(mServer);
	EXPECT_TRUE(mSent.empty());

	gNowMs += 1;
	DebugServerTestAccess::Sweep(mServer);
	ASSERT_EQ(mSent.size(), 1u);
	EXPECT_EQ(mSent[0].connId, 2);
	const auto& r = mSent[0].msg.command_response();
	EXPECT_EQ(r.command(), "sim.q");
	EXPECT_FALSE(r.success());
	EXPECT_EQ(r.message(), "sim timeout");
	EXPECT_EQ(PayloadOf(0)["error"].asString(), "sim timeout");
	EXPECT_FALSE(PayloadOf(0)["success"].asBool());
}

TEST_F(SimHostedDispatchTest, LateReplyAfterTimeoutIsDropped)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	Send(2, "sim.q");
	const uint32_t id = mSink.posted[0].requestId;
	gNowMs += 2000;
	DebugServerTestAccess::Sweep(mServer);
	ASSERT_EQ(mSent.size(), 1u);

	mServer.CompleteSimReply(MakeReply(id, SimReplyStatus::Ok, "{}"));
	mServer.CompleteSimReply(MakeReply(id, SimReplyStatus::Ok, "{}"));
	EXPECT_EQ(mSent.size(), 1u);
}

TEST_F(SimHostedDispatchTest, BusyWhenTableFull)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	for (uint32_t i = 0; i < Dia::DebugServer::SimPendingTable::kCapacity; ++i)
		Send(1, "sim.q");
	EXPECT_TRUE(mSent.empty());

	Send(9, "sim.q");
	ASSERT_EQ(mSent.size(), 1u);
	EXPECT_EQ(mSent[0].connId, 9);
	EXPECT_FALSE(mSent[0].msg.command_response().success());
	EXPECT_EQ(mSent[0].msg.command_response().message(), "sim busy");
	EXPECT_EQ(PayloadOf(0)["error"].asString(), "sim busy");
	EXPECT_EQ(mSink.posted.size(), static_cast<size_t>(Dia::DebugServer::SimPendingTable::kCapacity));
}

TEST_F(SimHostedDispatchTest, BusyWhenPostRequestFailsAndEntryNotLeaked)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	mSink.accept = false;
	for (uint32_t i = 0; i < Dia::DebugServer::SimPendingTable::kCapacity + 5; ++i)
		Send(1, "sim.q");
	ASSERT_EQ(mSent.size(), static_cast<size_t>(Dia::DebugServer::SimPendingTable::kCapacity + 5));
	for (const auto& s : mSent)
		EXPECT_EQ(s.msg.command_response().message(), "sim busy");

	// Table must not have leaked: once the sink accepts again, requests are deferred.
	mSink.accept = true;
	mSent.clear();
	Send(1, "sim.q");
	EXPECT_TRUE(mSent.empty());
	EXPECT_EQ(mSink.posted.size(), 1u);
}

TEST_F(SimHostedDispatchTest, NoSinkIsBusy)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	mServer.SetSimRequestSink(nullptr);
	Send(1, "sim.q");
	ASSERT_EQ(mSent.size(), 1u);
	EXPECT_EQ(mSent[0].msg.command_response().message(), "sim busy");
}

TEST_F(SimHostedDispatchTest, ArgsTooLargeRejectedImmediately)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	Json::Value args;
	args["big"] = std::string(Dia::DebugServer::kSimPayloadBytes, 'x');
	Send(1, "sim.q", &args);
	ASSERT_EQ(mSent.size(), 1u);
	EXPECT_FALSE(mSent[0].msg.command_response().success());
	EXPECT_EQ(mSent[0].msg.command_response().message(), "reply too large");
	EXPECT_TRUE(mSink.posted.empty());
}

TEST_F(SimHostedDispatchTest, TooLargeReplyStatusMapsToError)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	Send(1, "sim.q");
	mServer.CompleteSimReply(MakeReply(mSink.posted[0].requestId, SimReplyStatus::TooLarge, ""));
	ASSERT_EQ(mSent.size(), 1u);
	EXPECT_FALSE(mSent[0].msg.command_response().success());
	EXPECT_EQ(mSent[0].msg.command_response().message(), "reply too large");
	EXPECT_EQ(PayloadOf(0)["error"].asString(), "reply too large");
}

TEST_F(SimHostedDispatchTest, NonOkStatusesMapToErrors)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	Send(1, "sim.q");
	Send(1, "sim.q");
	mServer.CompleteSimReply(MakeReply(mSink.posted[0].requestId, SimReplyStatus::HandlerFailed, ""));
	mServer.CompleteSimReply(MakeReply(mSink.posted[1].requestId, SimReplyStatus::UnknownName, ""));
	ASSERT_EQ(mSent.size(), 2u);
	EXPECT_FALSE(mSent[0].msg.command_response().success());
	EXPECT_EQ(mSent[0].msg.command_response().message(), "handler failed");
	EXPECT_FALSE(mSent[1].msg.command_response().success());
	EXPECT_EQ(mSent[1].msg.command_response().message(), "unknown name");
}

TEST_F(SimHostedDispatchTest, OkWithUnparseableJsonIsHandlerFailed)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	Send(1, "sim.q");
	mServer.CompleteSimReply(MakeReply(mSink.posted[0].requestId, SimReplyStatus::Ok, "{not json"));
	ASSERT_EQ(mSent.size(), 1u);
	EXPECT_FALSE(mSent[0].msg.command_response().success());
	EXPECT_EQ(mSent[0].msg.command_response().message(), "handler failed");
}

TEST_F(SimHostedDispatchTest, ConnectionCloseDropsEntry)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	Send(5, "sim.q");
	Send(6, "sim.q");
	DebugServerTestAccess::HandleConnection(mServer, 5, false);

	mServer.CompleteSimReply(MakeReply(mSink.posted[0].requestId, SimReplyStatus::Ok, "{}"));
	EXPECT_TRUE(mSent.empty());

	mServer.CompleteSimReply(MakeReply(mSink.posted[1].requestId, SimReplyStatus::Ok, "{}"));
	ASSERT_EQ(mSent.size(), 1u);
	EXPECT_EQ(mSent[0].connId, 6);

	// Dropped entry must not time out into a send either.
	mSent.clear();
	gNowMs += 5000;
	DebugServerTestAccess::Sweep(mServer);
	EXPECT_TRUE(mSent.empty());
}

TEST_F(SimHostedDispatchTest, DuplicateSimRegistrationRejected)
{
	EXPECT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	EXPECT_FALSE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Command));
}

TEST_F(SimHostedDispatchTest, NameAlreadyInQueryRegistryRejected)
{
	mServer.GetQueryRegistry().Register(StringCRC("both"), [](const Json::Value&) { return Json::Value(); });
	EXPECT_FALSE(mServer.RegisterSimHosted(StringCRC("both"), SimKind::Query));
	Send(1, "both");
	EXPECT_TRUE(mSink.posted.empty());
}

TEST_F(SimHostedDispatchTest, UnregisterRestoresFallThrough)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	mServer.UnregisterSimHosted(StringCRC("sim.q"));
	EXPECT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	mServer.UnregisterSimHosted(StringCRC("sim.q"));
	Send(1, "sim.q");
	EXPECT_TRUE(mSink.posted.empty());
	EXPECT_EQ(mSent.size(), 1u);	// DiaAPI fallback answered immediately
}

TEST_F(SimHostedDispatchTest, UnregisteredNameFallsThroughToQueryRegistryThenDiaApi)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	mServer.GetQueryRegistry().Register(StringCRC("main.q"), [](const Json::Value&) {
		Json::Value v;
		v["ok"] = true;
		return v;
	});

	Send(1, "main.q");
	ASSERT_EQ(mSent.size(), 1u);
	EXPECT_TRUE(mSent[0].msg.command_response().success());
	EXPECT_TRUE(PayloadOf(0)["ok"].asBool());

	Send(1, "no.such.command");
	ASSERT_EQ(mSent.size(), 2u);
	EXPECT_EQ(mSent[1].msg.command_response().command(), "no.such.command");
	EXPECT_TRUE(mSink.posted.empty());
}

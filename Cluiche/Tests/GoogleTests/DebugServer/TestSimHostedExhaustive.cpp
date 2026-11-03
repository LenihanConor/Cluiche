#include <gtest/gtest.h>

#include <DiaDebugServer/DebugServer.h>
#include <DiaDebugServer/ISimRequestSink.h>
#include <DiaDebugServer/QueryRegistry.h>
#include <DiaDebugServer/SimHostedTypes.h>
#include <DiaDebugServer/SimPendingTable.h>
#include <DiaDebugServer/SimQueryHost.h>
#include <DiaDebugProtocol/DiaDebugProtocol.h>
#include <DiaProtobuf/ProtoStructConverter.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstring>
#include <deque>
#include <functional>
#include <set>
#include <string>
#include <thread>
#include <vector>

// Sim-hosted queries: exhaustive audit tests (Task 6).
// Covers: QueryRegistry veto (AC 10), Main->Sim->Main loopback with thread identity (AC 1),
// registration protocol edge cases, saturation / churn through DebugServer, payload boundaries,
// degenerate replies and reentrancy.

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
	using Dia::DebugServer::QueryRegistry;
	using Dia::DebugServer::SimKind;
	using Dia::DebugServer::SimPendingTable;
	using Dia::DebugServer::SimQueryHost;
	using Dia::DebugServer::SimReplyEvent;
	using Dia::DebugServer::SimReplyStatus;
	using Dia::DebugServer::SimRequestEvent;
	using Dia::CellLayers::Testing::LogCapture;

	uint64_t gExNowMs = 0;
	uint64_t ExFakeNow() { return gExNowMs; }

	struct ExSent
	{
		int connId;
		dia::debug::DebugMessage msg;
	};

	void ExCaptureSend(void* ctx, int connId, const dia::debug::DebugMessage& msg)
	{
		static_cast<std::vector<ExSent>*>(ctx)->push_back(ExSent{ connId, msg });
	}

	class ExFakeSink : public Dia::DebugServer::ISimRequestSink
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

	// Request source + request sink + reply sink in one queue pair: wires DebugServer to a SimQueryHost
	// with no EventStreams.
	class Loopback
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

	dia::debug::DebugMessage ExMakeCommand(const char* name, const Json::Value* args = nullptr)
	{
		dia::debug::DebugMessage m;
		m.set_type(dia::debug::MESSAGE_TYPE_COMMAND_REQUEST);
		auto* c = m.mutable_command_request();
		c->set_command(name);
		if (args)
			Dia::Proto::JsonValueToProtoStruct(*args, c->mutable_payload());
		return m;
	}

	SimReplyEvent ExMakeReply(uint32_t id, SimReplyStatus st, const char* json)
	{
		SimReplyEvent r;
		std::memset(&r, 0, sizeof(r));
		r.requestId = id;
		r.status = static_cast<uint32_t>(st);
		std::strncpy(r.json, json, sizeof(r.json) - 1);
		return r;
	}

	std::string ExCompact(const Json::Value& v)
	{
		Json::StreamWriterBuilder b;
		b["indentation"] = "";
		return Json::writeString(b, v);
	}

	// {"s":"xxx"} serialises to n + 8 characters for an n-character string.
	Json::Value ArgsOfCompactSize(size_t total)
	{
		Json::Value v;
		v["s"] = std::string(total - 8, 'x');
		return v;
	}

	bool ExVetoBadName(void* ctx, const StringCRC& name)
	{
		++*static_cast<int*>(ctx);
		return name == StringCRC("bad");
	}

	Json::Value ExOk(const Json::Value&)
	{
		Json::Value v;
		v["ok"] = true;
		return v;
	}
}

class SimHostedExhaustiveTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		gExNowMs = 10000;
		mServer.SetNowMsFunction(&ExFakeNow);
		mServer.SetSendHook(&ExCaptureSend, &mSent);
		mServer.SetSimRequestSink(&mSink);
	}

	void Send(int conn, const char* name, const Json::Value* args = nullptr)
	{
		DebugServerTestAccess::HandleCommand(mServer, conn, ExMakeCommand(name, args));
	}

	Json::Value PayloadOf(size_t i) const
	{
		return Dia::Proto::ProtoStructToJsonValue(mMsg(i).command_response().payload());
	}

	const dia::debug::DebugMessage& mMsg(size_t i) const { return mSent[i].msg; }

	DebugServer mServer;
	ExFakeSink mSink;
	std::vector<ExSent> mSent;
};

//------------------------------------------------------------------------------
// AC 10: QueryRegistry side of the duplicate-name rejection
//------------------------------------------------------------------------------

TEST_F(SimHostedExhaustiveTest, QueryRegistryRefusesNameAlreadySimHosted)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("v.q"), SimKind::Query));
	LogCapture log("DebugServer");

	mServer.GetQueryRegistry().Register(StringCRC("v.q"), &ExOk);

	EXPECT_FALSE(mServer.GetQueryRegistry().Has(StringCRC("v.q")));
	EXPECT_EQ(1u, log.Errors("sim-hosted"));

	// The sim-hosted entry still owns the name.
	Send(1, "v.q");
	EXPECT_TRUE(mSent.empty());
	EXPECT_EQ(1u, mSink.posted.size());
}

TEST_F(SimHostedExhaustiveTest, QueryRegistryAcceptsSimNameAfterUnregisterSimHosted)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("v.q"), SimKind::Query));
	mServer.UnregisterSimHosted(StringCRC("v.q"));

	mServer.GetQueryRegistry().Register(StringCRC("v.q"), &ExOk);
	EXPECT_TRUE(mServer.GetQueryRegistry().Has(StringCRC("v.q")));

	Send(1, "v.q");
	ASSERT_EQ(1u, mSent.size());
	EXPECT_TRUE(PayloadOf(0)["ok"].asBool());
	EXPECT_TRUE(mSink.posted.empty());

	// And now the sim registration is the one refused.
	EXPECT_FALSE(mServer.RegisterSimHosted(StringCRC("v.q"), SimKind::Query));
}

TEST_F(SimHostedExhaustiveTest, QueryRegistryUnrelatedNamesStillRegisterWithVetoInstalled)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.only"), SimKind::Query));
	mServer.GetQueryRegistry().Register(StringCRC("main.only"), &ExOk);
	EXPECT_TRUE(mServer.GetQueryRegistry().Has(StringCRC("main.only")));
	EXPECT_FALSE(mServer.GetQueryRegistry().Has(StringCRC("sim.only")));
}

TEST(QueryRegistryVeto, DefaultHasNoVeto)
{
	QueryRegistry reg;
	reg.Register(StringCRC("bad"), &ExOk);
	EXPECT_TRUE(reg.Has(StringCRC("bad")));
}

TEST(QueryRegistryVeto, CustomVetoIsConsultedAndRefusedNameIsNotAdded)
{
	QueryRegistry reg;
	int calls = 0;
	reg.SetRegisterVeto(&ExVetoBadName, &calls);

	reg.Register(StringCRC("bad"), &ExOk);
	reg.Register(StringCRC("good"), &ExOk);

	EXPECT_EQ(2, calls);
	EXPECT_FALSE(reg.Has(StringCRC("bad")));
	EXPECT_TRUE(reg.Has(StringCRC("good")));
	const Json::Value r = reg.Execute(StringCRC("bad"), Json::Value());
	EXPECT_EQ("unknown query", r["error"].asString());
}

TEST(QueryRegistryVeto, ClearingTheVetoAllowsTheName)
{
	QueryRegistry reg;
	int calls = 0;
	reg.SetRegisterVeto(&ExVetoBadName, &calls);
	reg.SetRegisterVeto(nullptr, nullptr);
	reg.Register(StringCRC("bad"), &ExOk);
	EXPECT_TRUE(reg.Has(StringCRC("bad")));
	EXPECT_EQ(0, calls);
}

//------------------------------------------------------------------------------
// AC 1: Main -> Sim -> Main loopback; handler never runs inline on Main
//------------------------------------------------------------------------------

TEST_F(SimHostedExhaustiveTest, HandlerNeverRunsOnMainThreadDuringHandleCommand)
{
	Loopback lb;
	SimQueryHost host;
	host.SetRequestSource(&lb);
	host.SetReplySink(&lb);

	int calls = 0;
	std::thread::id handlerThread;
	ASSERT_TRUE(host.RegisterQuery(StringCRC("lb.q"), Dia::DebugServer::SimHelpInfo{"test help"}, [&](const Json::Value&) {
		++calls;
		handlerThread = std::this_thread::get_id();
		Json::Value v;
		v["from"] = "sim";
		return v;
	}));
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("lb.q"), SimKind::Query));
	mServer.SetSimRequestSink(&lb);

	const std::thread::id mainThread = std::this_thread::get_id();
	Send(7, "lb.q");

	// Main side only queued the request: no handler call, no reply yet.
	EXPECT_EQ(0, calls);
	EXPECT_TRUE(mSent.empty());
	ASSERT_EQ(1u, lb.requests.size());

	// "SimPU" thread drains the queue.
	std::thread::id updateThread;
	std::thread sim([&] {
		updateThread = std::this_thread::get_id();
		host.Update();
	});
	sim.join();

	EXPECT_EQ(1, calls);
	EXPECT_EQ(updateThread, handlerThread);
	EXPECT_NE(mainThread, handlerThread);
	EXPECT_TRUE(mSent.empty());	// still nothing until Main drains the reply

	// Main drains the reply.
	ASSERT_EQ(1u, lb.replies.size());
	mServer.CompleteSimReply(lb.replies.front());
	ASSERT_EQ(1u, mSent.size());
	EXPECT_EQ(7, mSent[0].connId);
	EXPECT_EQ("lb.q", mSent[0].msg.command_response().command());
	EXPECT_TRUE(mSent[0].msg.command_response().success());
	EXPECT_EQ("sim", PayloadOf(0)["from"].asString());
}

TEST_F(SimHostedExhaustiveTest, LoopbackFullRoundTripWithArgsKindMismatchAndHandlerThrow)
{
	Loopback lb;
	SimQueryHost host;
	host.SetRequestSource(&lb);
	host.SetReplySink(&lb);
	ASSERT_TRUE(host.RegisterQuery(StringCRC("lb.echo"), Dia::DebugServer::SimHelpInfo{"test help"}, [](const Json::Value& a) {
		Json::Value v;
		v["got"] = a["x"];
		return v;
	}));
	ASSERT_TRUE(host.RegisterCommand(StringCRC("lb.throw"), Dia::DebugServer::SimHelpInfo{"test help"}, [](const Json::Value&) -> Json::Value { throw 1; }));
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("lb.echo"), SimKind::Query));
	// Server believes lb.throw is a Query; host has it as Command -> unknown name.
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("lb.throw"), SimKind::Query));
	mServer.SetSimRequestSink(&lb);

	Json::Value args;
	args["x"] = 41;
	Send(1, "lb.echo", &args);
	Send(2, "lb.throw");
	host.Update();
	ASSERT_EQ(2u, lb.replies.size());
	while (!lb.replies.empty())
	{
		mServer.CompleteSimReply(lb.replies.front());
		lb.replies.pop_front();
	}

	ASSERT_EQ(2u, mSent.size());
	EXPECT_EQ(1, mSent[0].connId);
	EXPECT_TRUE(mSent[0].msg.command_response().success());
	EXPECT_EQ(41, PayloadOf(0)["got"].asInt());
	EXPECT_EQ(2, mSent[1].connId);
	EXPECT_FALSE(mSent[1].msg.command_response().success());
	EXPECT_EQ("unknown name", mSent[1].msg.command_response().message());
}

//------------------------------------------------------------------------------
// Registration protocol edge cases (requestId 0 handled by DrainSimReplies -> these calls)
//------------------------------------------------------------------------------

TEST_F(SimHostedExhaustiveTest, UnregisterUnknownNameIsHarmlessAndSilent)
{
	LogCapture log("DebugServer");
	mServer.UnregisterSimHosted(StringCRC("never.registered"));
	EXPECT_EQ(0u, log.Errors());
	EXPECT_EQ(0u, log.Count(LogCapture::Level::kInfo, "unregistered"));
	EXPECT_TRUE(mServer.RegisterSimHosted(StringCRC("never.registered"), SimKind::Query));
}

TEST_F(SimHostedExhaustiveTest, UnregisterTwiceSecondIsNoOp)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	mServer.UnregisterSimHosted(StringCRC("sim.q"));
	LogCapture log("DebugServer");
	mServer.UnregisterSimHosted(StringCRC("sim.q"));
	EXPECT_EQ(0u, log.Count(LogCapture::Level::kInfo, "unregistered"));
}

TEST_F(SimHostedExhaustiveTest, DuplicateRegisterLogsErrorAndKeepsOriginalKind)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	LogCapture log("DebugServer");
	EXPECT_FALSE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Command));
	EXPECT_EQ(1u, log.Errors("already sim-hosted"));

	Send(1, "sim.q");
	ASSERT_EQ(1u, mSink.posted.size());
	EXPECT_EQ(static_cast<uint32_t>(SimKind::Query), mSink.posted[0].kind);
}

TEST_F(SimHostedExhaustiveTest, RegisterAfterSinkDetachedStillRegistersAndRequestsAreBusyUntilReattached)
{
	mServer.SetSimRequestSink(nullptr);
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));

	Send(1, "sim.q");
	ASSERT_EQ(1u, mSent.size());
	EXPECT_EQ("sim busy", mSent[0].msg.command_response().message());

	mServer.SetSimRequestSink(&mSink);
	mSent.clear();
	Send(1, "sim.q");
	EXPECT_TRUE(mSent.empty());
	EXPECT_EQ(1u, mSink.posted.size());
}

TEST_F(SimHostedExhaustiveTest, SimRegistersNameThatExistsInQueryRegistryErrorsAndLaterRequestsGoToQueryRegistry)
{
	mServer.GetQueryRegistry().Register(StringCRC("both"), &ExOk);
	LogCapture log("DebugServer");

	EXPECT_FALSE(mServer.RegisterSimHosted(StringCRC("both"), SimKind::Query));
	EXPECT_EQ(1u, log.Errors("QueryRegistry"));

	Send(2, "both");
	ASSERT_EQ(1u, mSent.size());
	EXPECT_EQ(2, mSent[0].connId);
	EXPECT_TRUE(PayloadOf(0)["ok"].asBool());
	EXPECT_TRUE(mSink.posted.empty());

	// A later unregister from the sim side must not disturb the QueryRegistry entry.
	mServer.UnregisterSimHosted(StringCRC("both"));
	Send(2, "both");
	EXPECT_EQ(2u, mSent.size());
}

TEST_F(SimHostedExhaustiveTest, SimNameCapacityIs32ThenRegistrationFailsWithErrorAndSlotIsReusable)
{
	for (int i = 0; i < 32; ++i)
	{
		const std::string n = "cap.n" + std::to_string(i);
		ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC(n.c_str()), SimKind::Query)) << n;
	}
	LogCapture log("DebugServer");
	EXPECT_FALSE(mServer.RegisterSimHosted(StringCRC("cap.extra"), SimKind::Query));
	EXPECT_EQ(1u, log.Errors("capacity"));

	mServer.UnregisterSimHosted(StringCRC("cap.n0"));
	EXPECT_TRUE(mServer.RegisterSimHosted(StringCRC("cap.extra"), SimKind::Query));
}

TEST_F(SimHostedExhaustiveTest, TwoNamesRouteWithTheirOwnKindAndCommandName)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("a.q"), SimKind::Query));
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("b.c"), SimKind::Command));
	Send(10, "a.q");
	Send(11, "b.c");
	ASSERT_EQ(2u, mSink.posted.size());
	EXPECT_EQ(StringCRC("a.q").Value(), mSink.posted[0].nameCrc);
	EXPECT_EQ(static_cast<uint32_t>(SimKind::Query), mSink.posted[0].kind);
	EXPECT_EQ(StringCRC("b.c").Value(), mSink.posted[1].nameCrc);
	EXPECT_EQ(static_cast<uint32_t>(SimKind::Command), mSink.posted[1].kind);

	// Complete out of order.
	mServer.CompleteSimReply(ExMakeReply(mSink.posted[1].requestId, SimReplyStatus::Ok, "{}"));
	mServer.CompleteSimReply(ExMakeReply(mSink.posted[0].requestId, SimReplyStatus::Ok, "{}"));
	ASSERT_EQ(2u, mSent.size());
	EXPECT_EQ(11, mSent[0].connId);
	EXPECT_EQ("b.c", mSent[0].msg.command_response().command());
	EXPECT_EQ(10, mSent[1].connId);
	EXPECT_EQ("a.q", mSent[1].msg.command_response().command());
}

// A name unregistered while a request is outstanding: the client's response should still carry the
// command name it asked for (clients correlate responses by command).
TEST_F(SimHostedExhaustiveTest, ReplyAfterUnregisterKeepsCommandName)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	Send(3, "sim.q");
	mServer.UnregisterSimHosted(StringCRC("sim.q"));
	mServer.CompleteSimReply(ExMakeReply(mSink.posted[0].requestId, SimReplyStatus::Ok, "{\"a\":1}"));
	ASSERT_EQ(1u, mSent.size());
	EXPECT_EQ("sim.q", mSent[0].msg.command_response().command());
}

TEST_F(SimHostedExhaustiveTest, TimeoutAfterUnregisterKeepsCommandName)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	Send(3, "sim.q");
	mServer.UnregisterSimHosted(StringCRC("sim.q"));
	gExNowMs += SimPendingTable::kTimeoutMs;
	DebugServerTestAccess::Sweep(mServer);
	ASSERT_EQ(1u, mSent.size());
	EXPECT_EQ("sim.q", mSent[0].msg.command_response().command());
}

TEST_F(SimHostedExhaustiveTest, EmptyCommandNameIsInvalidCommandErrorNotDeferred)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	Send(1, "");
	ASSERT_EQ(1u, mSent.size());
	EXPECT_EQ(dia::debug::MESSAGE_TYPE_ERROR, mSent[0].msg.type());
	EXPECT_TRUE(mSink.posted.empty());
}

//------------------------------------------------------------------------------
// Saturation, churn, wrap behaviour through DebugServer
//------------------------------------------------------------------------------

TEST_F(SimHostedExhaustiveTest, SixtyFifthConcurrentRequestIsBusyWhileEarlierOnesStillComplete)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	const int kCap = static_cast<int>(SimPendingTable::kCapacity);
	for (int i = 0; i < kCap; ++i)
		Send(i, "sim.q");
	ASSERT_EQ(static_cast<size_t>(kCap), mSink.posted.size());
	EXPECT_TRUE(mSent.empty());

	Send(500, "sim.q");
	ASSERT_EQ(1u, mSent.size());
	EXPECT_EQ(500, mSent[0].connId);
	EXPECT_EQ("sim busy", mSent[0].msg.command_response().message());
	EXPECT_EQ(static_cast<size_t>(kCap), mSink.posted.size());

	// Complete in reverse order, each answering its own connection with its own payload.
	for (int i = kCap - 1; i >= 0; --i)
	{
		const std::string json = "{\"i\":" + std::to_string(i) + "}";
		mServer.CompleteSimReply(ExMakeReply(mSink.posted[i].requestId, SimReplyStatus::Ok, json.c_str()));
		const ExSent& s = mSent.back();
		EXPECT_EQ(i, s.connId);
		EXPECT_TRUE(s.msg.command_response().success());
		EXPECT_EQ(i, Dia::Proto::ProtoStructToJsonValue(s.msg.command_response().payload())["i"].asInt());
	}
	EXPECT_EQ(static_cast<size_t>(kCap) + 1, mSent.size());

	// Freed slots are usable again.
	mSent.clear();
	Send(501, "sim.q");
	EXPECT_TRUE(mSent.empty());
	EXPECT_EQ(static_cast<size_t>(kCap) + 1, mSink.posted.size());
}

TEST_F(SimHostedExhaustiveTest, ChurnPastSeveralTableCapacitiesKeepsIdsUniqueAndRoutingCorrect)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	const int kTotal = 400;
	const size_t kWindow = 60;
	std::set<uint32_t> live;
	size_t nextComplete = 0;

	for (int n = 0; n < kTotal; ++n)
	{
		Send(1000 + n, "sim.q");
		ASSERT_EQ(static_cast<size_t>(n) + 1, mSink.posted.size()) << "request " << n << " was not accepted";
		const uint32_t id = mSink.posted[n].requestId;
		EXPECT_NE(0u, id);
		EXPECT_TRUE(live.insert(id).second) << "id " << id << " issued while still live";

		if (live.size() >= kWindow)
		{
			const uint32_t done = mSink.posted[nextComplete].requestId;
			mServer.CompleteSimReply(ExMakeReply(done, SimReplyStatus::Ok, "{}"));
			live.erase(done);
			++nextComplete;
		}
	}
	while (nextComplete < mSink.posted.size())
	{
		mServer.CompleteSimReply(ExMakeReply(mSink.posted[nextComplete].requestId, SimReplyStatus::Ok, "{}"));
		++nextComplete;
	}

	ASSERT_EQ(static_cast<size_t>(kTotal), mSent.size());
	for (int n = 0; n < kTotal; ++n)
		EXPECT_EQ(1000 + n, mSent[n].connId);
}

TEST_F(SimHostedExhaustiveTest, FullTableAllTimingOutInOneSweepAnswersEveryConnectionThenAcceptsNew)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	const int kCap = static_cast<int>(SimPendingTable::kCapacity);
	for (int i = 0; i < kCap; ++i)
		Send(i, "sim.q");

	gExNowMs += SimPendingTable::kTimeoutMs;
	DebugServerTestAccess::Sweep(mServer);

	ASSERT_EQ(static_cast<size_t>(kCap), mSent.size());
	std::set<int> conns;
	for (const auto& s : mSent)
	{
		conns.insert(s.connId);
		EXPECT_EQ("sim timeout", s.msg.command_response().message());
		EXPECT_EQ("sim.q", s.msg.command_response().command());
	}
	EXPECT_EQ(static_cast<size_t>(kCap), conns.size());

	mSent.clear();
	Send(900, "sim.q");
	EXPECT_TRUE(mSent.empty());
	EXPECT_EQ(static_cast<size_t>(kCap) + 1, mSink.posted.size());
}

TEST_F(SimHostedExhaustiveTest, DefaultClockDoesNotTimeOutImmediately)
{
	mServer.SetNowMsFunction(nullptr);
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	Send(1, "sim.q");
	DebugServerTestAccess::Sweep(mServer);
	EXPECT_TRUE(mSent.empty());
}

//------------------------------------------------------------------------------
// Payload boundaries (AC 7)
//------------------------------------------------------------------------------

TEST_F(SimHostedExhaustiveTest, ArgsOneUnderCapAcceptedAndTerminated)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	const Json::Value args = ArgsOfCompactSize(Dia::DebugServer::kSimPayloadBytes - 1);
	ASSERT_EQ(Dia::DebugServer::kSimPayloadBytes - 1, ExCompact(args).size());
	Send(1, "sim.q", &args);
	EXPECT_TRUE(mSent.empty());
	ASSERT_EQ(1u, mSink.posted.size());
	EXPECT_EQ(Dia::DebugServer::kSimPayloadBytes - 1, std::strlen(mSink.posted[0].json));
	EXPECT_EQ('\0', mSink.posted[0].json[Dia::DebugServer::kSimPayloadBytes - 1]);
}

TEST_F(SimHostedExhaustiveTest, ArgsExactlyAtCapRejectedAsTooLarge)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	const Json::Value args = ArgsOfCompactSize(Dia::DebugServer::kSimPayloadBytes);
	ASSERT_EQ(Dia::DebugServer::kSimPayloadBytes, ExCompact(args).size());
	Send(1, "sim.q", &args);
	ASSERT_EQ(1u, mSent.size());
	EXPECT_EQ("reply too large", mSent[0].msg.command_response().message());
	EXPECT_TRUE(mSink.posted.empty());
}

//------------------------------------------------------------------------------
// Degenerate / hostile replies delivered to CompleteSimReply
//------------------------------------------------------------------------------

TEST_F(SimHostedExhaustiveTest, LateReplyIsLoggedExactlyOnceAcrossManyDrops)
{
	LogCapture log("DebugServer");
	for (uint32_t id = 900; id < 905; ++id)
		mServer.CompleteSimReply(ExMakeReply(id, SimReplyStatus::Ok, "{}"));
	EXPECT_EQ(1u, log.Warnings("late"));
	EXPECT_TRUE(mSent.empty());
}

TEST_F(SimHostedExhaustiveTest, ReplyWithRequestIdZeroIsDroppedNotCompleted)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	Send(1, "sim.q");
	mServer.CompleteSimReply(ExMakeReply(0, SimReplyStatus::Ok, "{}"));
	EXPECT_TRUE(mSent.empty());

	// The real request is still pending and completes normally.
	mServer.CompleteSimReply(ExMakeReply(mSink.posted[0].requestId, SimReplyStatus::Ok, "{}"));
	EXPECT_EQ(1u, mSent.size());
}

TEST_F(SimHostedExhaustiveTest, OkReplyWithoutNulTerminatorIsHandlerFailedWithoutOverrun)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	Send(1, "sim.q");
	SimReplyEvent r;
	std::memset(&r, 0, sizeof(r));
	r.requestId = mSink.posted[0].requestId;
	r.status = static_cast<uint32_t>(SimReplyStatus::Ok);
	std::memset(r.json, 'x', sizeof(r.json));
	mServer.CompleteSimReply(r);
	ASSERT_EQ(1u, mSent.size());
	EXPECT_FALSE(mSent[0].msg.command_response().success());
	EXPECT_EQ("handler failed", mSent[0].msg.command_response().message());
}

TEST_F(SimHostedExhaustiveTest, UnknownStatusValueIsHandlerFailed)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	Send(1, "sim.q");
	SimReplyEvent r = ExMakeReply(mSink.posted[0].requestId, SimReplyStatus::Ok, "{}");
	r.status = 99;
	mServer.CompleteSimReply(r);
	ASSERT_EQ(1u, mSent.size());
	EXPECT_FALSE(mSent[0].msg.command_response().success());
	EXPECT_EQ("handler failed", mSent[0].msg.command_response().message());
}

TEST_F(SimHostedExhaustiveTest, OkReplyWithNonObjectJsonAnswersOnceAndDoesNotCrash)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	Send(1, "sim.q");
	Send(1, "sim.q");
	mServer.CompleteSimReply(ExMakeReply(mSink.posted[0].requestId, SimReplyStatus::Ok, "[1,2,3]"));
	mServer.CompleteSimReply(ExMakeReply(mSink.posted[1].requestId, SimReplyStatus::Ok, "5"));
	EXPECT_EQ(2u, mSent.size());
}

TEST_F(SimHostedExhaustiveTest, EmptyJsonOkReplyIsHandlerFailed)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	Send(1, "sim.q");
	mServer.CompleteSimReply(ExMakeReply(mSink.posted[0].requestId, SimReplyStatus::Ok, ""));
	ASSERT_EQ(1u, mSent.size());
	EXPECT_FALSE(mSent[0].msg.command_response().success());
	EXPECT_EQ("handler failed", mSent[0].msg.command_response().message());
}

//------------------------------------------------------------------------------
// Connection lifecycle
//------------------------------------------------------------------------------

TEST_F(SimHostedExhaustiveTest, CloseUnknownAndRepeatedConnectionsAreHarmless)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	Send(5, "sim.q");
	DebugServerTestAccess::HandleConnection(mServer, 77, false);
	DebugServerTestAccess::HandleConnection(mServer, 5, false);
	DebugServerTestAccess::HandleConnection(mServer, 5, false);
	mServer.CompleteSimReply(ExMakeReply(mSink.posted[0].requestId, SimReplyStatus::Ok, "{}"));
	EXPECT_TRUE(mSent.empty());
}

TEST_F(SimHostedExhaustiveTest, ReusedConnectionIdDoesNotReceiveTheOldConnectionsReply)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	Send(5, "sim.q");
	DebugServerTestAccess::HandleConnection(mServer, 5, false);
	Send(5, "sim.q");	// new connection that got the same id

	mServer.CompleteSimReply(ExMakeReply(mSink.posted[0].requestId, SimReplyStatus::Ok, "{}"));	// old: dropped
	EXPECT_TRUE(mSent.empty());
	mServer.CompleteSimReply(ExMakeReply(mSink.posted[1].requestId, SimReplyStatus::Ok, "{}"));
	EXPECT_EQ(1u, mSent.size());
}

TEST_F(SimHostedExhaustiveTest, ClosingAConnectionFreesItsSlotsForNewRequests)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	const int kCap = static_cast<int>(SimPendingTable::kCapacity);
	for (int i = 0; i < kCap; ++i)
		Send(5, "sim.q");
	DebugServerTestAccess::HandleConnection(mServer, 5, false);
	Send(6, "sim.q");
	EXPECT_TRUE(mSent.empty());
	EXPECT_EQ(static_cast<size_t>(kCap) + 1, mSink.posted.size());
}

//------------------------------------------------------------------------------
// Reentrancy through the send hook
//------------------------------------------------------------------------------

namespace
{
	struct ReentrantCtx
	{
		std::vector<ExSent>* sent = nullptr;
		std::function<void()> onFirst;
		bool fired = false;
	};

	void ExReentrantSend(void* ctx, int connId, const dia::debug::DebugMessage& msg)
	{
		ReentrantCtx* c = static_cast<ReentrantCtx*>(ctx);
		c->sent->push_back(ExSent{ connId, msg });
		if (!c->fired)
		{
			c->fired = true;
			c->onFirst();
		}
	}
}

TEST_F(SimHostedExhaustiveTest, CompleteFromInsideSendHookDeliversBothReplies)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	Send(1, "sim.q");
	Send(2, "sim.q");

	ReentrantCtx ctx;
	ctx.sent = &mSent;
	const uint32_t second = mSink.posted[1].requestId;
	ctx.onFirst = [&] { mServer.CompleteSimReply(ExMakeReply(second, SimReplyStatus::Ok, "{}")); };
	mServer.SetSendHook(&ExReentrantSend, &ctx);

	mServer.CompleteSimReply(ExMakeReply(mSink.posted[0].requestId, SimReplyStatus::Ok, "{}"));

	ASSERT_EQ(2u, mSent.size());
	EXPECT_EQ(1, mSent[0].connId);
	EXPECT_EQ(2, mSent[1].connId);
}

TEST_F(SimHostedExhaustiveTest, NewRequestFromInsideSendHookIsDeferredNormally)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	Send(1, "sim.q");

	ReentrantCtx ctx;
	ctx.sent = &mSent;
	ctx.onFirst = [&] { Send(3, "sim.q"); };
	mServer.SetSendHook(&ExReentrantSend, &ctx);

	mServer.CompleteSimReply(ExMakeReply(mSink.posted[0].requestId, SimReplyStatus::Ok, "{}"));
	ASSERT_EQ(1u, mSent.size());
	ASSERT_EQ(2u, mSink.posted.size());

	mServer.CompleteSimReply(ExMakeReply(mSink.posted[1].requestId, SimReplyStatus::Ok, "{}"));
	ASSERT_EQ(2u, mSent.size());
	EXPECT_EQ(3, mSent[1].connId);
}

TEST_F(SimHostedExhaustiveTest, SweepWithManyExpiredAnswersEachExactlyOnceEvenIfHookCompletesOthers)
{
	ASSERT_TRUE(mServer.RegisterSimHosted(StringCRC("sim.q"), SimKind::Query));
	Send(1, "sim.q");
	Send(2, "sim.q");
	gExNowMs += SimPendingTable::kTimeoutMs;

	ReentrantCtx ctx;
	ctx.sent = &mSent;
	// Late "real" replies arriving while the sweep is sending must be dropped, not double-answered.
	const uint32_t a = mSink.posted[0].requestId;
	const uint32_t b = mSink.posted[1].requestId;
	ctx.onFirst = [&] {
		mServer.CompleteSimReply(ExMakeReply(a, SimReplyStatus::Ok, "{}"));
		mServer.CompleteSimReply(ExMakeReply(b, SimReplyStatus::Ok, "{}"));
	};
	mServer.SetSendHook(&ExReentrantSend, &ctx);

	DebugServerTestAccess::Sweep(mServer);
	EXPECT_EQ(2u, mSent.size());
}

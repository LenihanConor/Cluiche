#include <gtest/gtest.h>

#include <DiaDebugServer/SimQueryHost.h>

#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace Dia::DebugServer;
using Dia::Core::StringCRC;

namespace
{
	class FakeSource : public ISimRequestSource
	{
	public:
		std::deque<SimRequestEvent> queue;
		bool TryPop(SimRequestEvent& out) override
		{
			if (queue.empty())
				return false;
			out = queue.front();
			queue.pop_front();
			return true;
		}
		void Push(uint32_t id, const char* name, SimKind kind, const char* json)
		{
			SimRequestEvent e;
			std::memset(&e, 0, sizeof(e));
			e.requestId = id;
			e.nameCrc = StringCRC(name).Value();
			e.kind = static_cast<uint32_t>(kind);
			std::strncpy(e.json, json, kSimPayloadBytes - 1);
			queue.push_back(e);
		}
	};

	class FakeSink : public ISimReplySink
	{
	public:
		std::vector<SimReplyEvent> replies;
		bool full = false;
		bool PostReply(const SimReplyEvent& r) override
		{
			if (full)
				return false;
			replies.push_back(r);
			return true;
		}
	};

	struct Fixture
	{
		SimQueryHost host;
		FakeSource source;
		FakeSink sink;
		Fixture()
		{
			host.SetRequestSource(&source);
			host.SetReplySink(&sink);
		}
	};

	Json::Value OkHandler(const Json::Value&)
	{
		Json::Value v;
		v["ok"] = 1;
		return v;
	}

	std::string Compact(const Json::Value& v)
	{
		Json::StreamWriterBuilder b;
		b["indentation"] = "";
		return Json::writeString(b, v);
	}

	Json::Value SizedReply(size_t totalLen)
	{
		Json::Value v;
		v["s"] = "";
		const size_t base = Compact(v).size();
		v["s"] = std::string(totalLen - base, 'x');
		return v;
	}

	Json::Value Parse(const char* s)
	{
		Json::Value v;
		Json::CharReaderBuilder b;
		std::string errs;
		std::unique_ptr<Json::CharReader> r(b.newCharReader());
		r->parse(s, s + std::strlen(s), &v, &errs);
		return v;
	}

	uint32_t St(SimReplyStatus s) { return static_cast<uint32_t>(s); }
}

TEST(SimQueryHost, QueryRoundTripReturnsOkWithSameRequestId)
{
	Fixture f;
	ASSERT_TRUE(f.host.RegisterQuery("q", Dia::DebugServer::SimHelpInfo{"test help"}, OkHandler));
	f.source.Push(42, "q", SimKind::Query, "{}");
	f.host.Update();
	ASSERT_EQ(f.sink.replies.size(), 1u);
	EXPECT_EQ(f.sink.replies[0].requestId, 42u);
	EXPECT_EQ(f.sink.replies[0].status, St(SimReplyStatus::Ok));
	EXPECT_EQ(Parse(f.sink.replies[0].json)["ok"].asInt(), 1);
}

TEST(SimQueryHost, ArgsAreParsedAndPassedToHandler)
{
	Fixture f;
	f.host.RegisterQuery("q", Dia::DebugServer::SimHelpInfo{"test help"}, [](const Json::Value& a) {
		Json::Value v;
		v["echo"] = a["n"].asInt() + 1;
		return v;
	});
	f.source.Push(1, "q", SimKind::Query, "{\"n\":9}");
	f.host.Update();
	ASSERT_EQ(f.sink.replies.size(), 1u);
	EXPECT_EQ(Parse(f.sink.replies[0].json)["echo"].asInt(), 10);
}

TEST(SimQueryHost, HandlerRunsOnThreadCallingUpdate)
{
	Fixture f;
	std::thread::id seen;
	f.host.RegisterQuery("q", Dia::DebugServer::SimHelpInfo{"test help"}, [&seen](const Json::Value&) {
		seen = std::this_thread::get_id();
		return Json::Value(Json::objectValue);
	});
	f.source.Push(1, "q", SimKind::Query, "{}");
	std::thread::id worker;
	std::thread t([&] {
		worker = std::this_thread::get_id();
		f.host.Update();
	});
	t.join();
	EXPECT_EQ(seen, worker);
	EXPECT_NE(seen, std::this_thread::get_id());
}

TEST(SimQueryHost, ThrowingHandlerYieldsHandlerFailedAndNextRequestServed)
{
	Fixture f;
	f.host.RegisterQuery("bad", Dia::DebugServer::SimHelpInfo{"test help"}, [](const Json::Value&) -> Json::Value { throw 1; });
	f.host.RegisterQuery("good", Dia::DebugServer::SimHelpInfo{"test help"}, OkHandler);
	f.source.Push(1, "bad", SimKind::Query, "{}");
	f.source.Push(2, "good", SimKind::Query, "{}");
	f.host.Update();
	ASSERT_EQ(f.sink.replies.size(), 2u);
	EXPECT_EQ(f.sink.replies[0].status, St(SimReplyStatus::HandlerFailed));
	const Json::Value v = Parse(f.sink.replies[0].json);
	EXPECT_FALSE(v["success"].asBool());
	EXPECT_EQ(v["error"].asString(), "handler failed");
	EXPECT_EQ(f.sink.replies[1].status, St(SimReplyStatus::Ok));
}

TEST(SimQueryHost, NonObjectResultYieldsHandlerFailed)
{
	Fixture f;
	f.host.RegisterQuery("arr", Dia::DebugServer::SimHelpInfo{"test help"}, [](const Json::Value&) { return Json::Value(Json::arrayValue); });
	f.host.RegisterQuery("num", Dia::DebugServer::SimHelpInfo{"test help"}, [](const Json::Value&) { return Json::Value(5); });
	f.source.Push(1, "arr", SimKind::Query, "{}");
	f.source.Push(2, "num", SimKind::Query, "{}");
	f.host.Update();
	ASSERT_EQ(f.sink.replies.size(), 2u);
	EXPECT_EQ(f.sink.replies[0].status, St(SimReplyStatus::HandlerFailed));
	EXPECT_EQ(f.sink.replies[1].status, St(SimReplyStatus::HandlerFailed));
}

TEST(SimQueryHost, OversizeResultYieldsTooLargeWithErrorJson)
{
	Fixture f;
	f.host.RegisterQuery("big", Dia::DebugServer::SimHelpInfo{"test help"}, [](const Json::Value&) { return SizedReply(kSimPayloadBytes); });
	f.source.Push(1, "big", SimKind::Query, "{}");
	f.host.Update();
	ASSERT_EQ(f.sink.replies.size(), 1u);
	EXPECT_EQ(f.sink.replies[0].status, St(SimReplyStatus::TooLarge));
	const Json::Value v = Parse(f.sink.replies[0].json);
	EXPECT_FALSE(v["success"].asBool());
	EXPECT_EQ(v["error"].asString(), "reply too large");
	EXPECT_LT(std::strlen(f.sink.replies[0].json), static_cast<size_t>(kSimPayloadBytes));
}

TEST(SimQueryHost, ResultOneUnderCapIsOkAndCapItselfFails)
{
	Fixture f;
	f.host.RegisterQuery("under", Dia::DebugServer::SimHelpInfo{"test help"}, [](const Json::Value&) { return SizedReply(kSimPayloadBytes - 1); });
	f.host.RegisterQuery("at", Dia::DebugServer::SimHelpInfo{"test help"}, [](const Json::Value&) { return SizedReply(kSimPayloadBytes); });
	f.source.Push(1, "under", SimKind::Query, "{}");
	f.source.Push(2, "at", SimKind::Query, "{}");
	f.host.Update();
	ASSERT_EQ(f.sink.replies.size(), 2u);
	EXPECT_EQ(f.sink.replies[0].status, St(SimReplyStatus::Ok));
	EXPECT_EQ(std::strlen(f.sink.replies[0].json), static_cast<size_t>(kSimPayloadBytes - 1));
	EXPECT_EQ(f.sink.replies[0].json[kSimPayloadBytes - 1], '\0');
	EXPECT_EQ(f.sink.replies[1].status, St(SimReplyStatus::TooLarge));
}

TEST(SimQueryHost, UnknownNameYieldsUnknownName)
{
	Fixture f;
	f.source.Push(5, "nope", SimKind::Query, "{}");
	f.host.Update();
	ASSERT_EQ(f.sink.replies.size(), 1u);
	EXPECT_EQ(f.sink.replies[0].requestId, 5u);
	EXPECT_EQ(f.sink.replies[0].status, St(SimReplyStatus::UnknownName));
	EXPECT_EQ(Parse(f.sink.replies[0].json)["error"].asString(), "unknown name");
}

TEST(SimQueryHost, KindMismatchYieldsUnknownName)
{
	Fixture f;
	f.host.RegisterQuery("q", Dia::DebugServer::SimHelpInfo{"test help"}, OkHandler);
	f.host.RegisterCommand("c", Dia::DebugServer::SimHelpInfo{"test help"}, OkHandler);
	f.source.Push(1, "q", SimKind::Command, "{}");
	f.source.Push(2, "c", SimKind::Query, "{}");
	f.host.Update();
	ASSERT_EQ(f.sink.replies.size(), 2u);
	EXPECT_EQ(f.sink.replies[0].status, St(SimReplyStatus::UnknownName));
	EXPECT_EQ(f.sink.replies[1].status, St(SimReplyStatus::UnknownName));
}

TEST(SimQueryHost, UnterminatedOrGarbageRequestJsonYieldsHandlerFailed)
{
	Fixture f;
	bool called = false;
	f.host.RegisterQuery("q", Dia::DebugServer::SimHelpInfo{"test help"}, [&called](const Json::Value&) {
		called = true;
		return Json::Value(Json::objectValue);
	});
	f.source.Push(1, "q", SimKind::Query, "{{not json");
	f.source.Push(2, "q", SimKind::Query, "{}");
	std::memset(f.source.queue.back().json, 'a', kSimPayloadBytes); // no NUL at all
	f.host.Update();
	ASSERT_EQ(f.sink.replies.size(), 2u);
	EXPECT_EQ(f.sink.replies[0].status, St(SimReplyStatus::HandlerFailed));
	EXPECT_EQ(f.sink.replies[1].status, St(SimReplyStatus::HandlerFailed));
	EXPECT_FALSE(called);
}

TEST(SimQueryHost, DuplicateRegistrationReturnsFalse)
{
	Fixture f;
	EXPECT_TRUE(f.host.RegisterQuery("q", Dia::DebugServer::SimHelpInfo{"test help"}, OkHandler));
	EXPECT_FALSE(f.host.RegisterQuery("q", Dia::DebugServer::SimHelpInfo{"test help"}, OkHandler));
	EXPECT_FALSE(f.host.RegisterCommand("q", Dia::DebugServer::SimHelpInfo{"test help"}, OkHandler));
}

TEST(SimQueryHost, CapacityIs64ThenRegistrationFails)
{
	Fixture f;
	for (int i = 0; i < 64; ++i)
		EXPECT_TRUE(f.host.RegisterQuery(StringCRC(("q" + std::to_string(i)).c_str()), Dia::DebugServer::SimHelpInfo{"test help"}, OkHandler));
	EXPECT_FALSE(f.host.RegisterQuery("overflow", Dia::DebugServer::SimHelpInfo{"test help"}, OkHandler));
	EXPECT_FALSE(f.host.Has("overflow"));
}

TEST(SimQueryHost, UnregisterThenUnknown)
{
	Fixture f;
	f.host.RegisterQuery("q", Dia::DebugServer::SimHelpInfo{"test help"}, OkHandler);
	EXPECT_TRUE(f.host.Has("q"));
	EXPECT_TRUE(f.host.UnregisterQuery("q"));
	EXPECT_FALSE(f.host.Has("q"));
	EXPECT_FALSE(f.host.UnregisterQuery("q"));
	f.source.Push(1, "q", SimKind::Query, "{}");
	f.host.Update();
	ASSERT_EQ(f.sink.replies.size(), 1u);
	EXPECT_EQ(f.sink.replies[0].status, St(SimReplyStatus::UnknownName));
}

TEST(SimQueryHost, NullHandlerRejected)
{
	Fixture f;
	EXPECT_FALSE(f.host.RegisterQuery("q", Dia::DebugServer::SimHelpInfo{"test help"}, SimQueryHost::QueryHandler()));
	EXPECT_FALSE(f.host.Has("q"));
}

TEST(SimQueryHost, EmptyQueueUpdateIsNoOp)
{
	Fixture f;
	f.host.RegisterQuery("q", Dia::DebugServer::SimHelpInfo{"test help"}, OkHandler);
	f.host.Update();
	EXPECT_TRUE(f.sink.replies.empty());
}

TEST(SimQueryHost, FullSinkDropsReplyAndLaterRepliesStillPost)
{
	Fixture f;
	f.host.RegisterQuery("q", Dia::DebugServer::SimHelpInfo{"test help"}, OkHandler);
	f.sink.full = true;
	f.source.Push(1, "q", SimKind::Query, "{}");
	f.source.Push(2, "q", SimKind::Query, "{}");
	f.host.Update();
	EXPECT_TRUE(f.sink.replies.empty());
	f.sink.full = false;
	f.source.Push(3, "q", SimKind::Query, "{}");
	f.host.Update();
	ASSERT_EQ(f.sink.replies.size(), 1u);
	EXPECT_EQ(f.sink.replies[0].requestId, 3u);
}

TEST(SimQueryHost, ManyRequestsDrainedInOneUpdateInOrder)
{
	Fixture f;
	f.host.RegisterQuery("q", Dia::DebugServer::SimHelpInfo{"test help"}, OkHandler);
	for (uint32_t i = 1; i <= 200; ++i)
		f.source.Push(i, "q", SimKind::Query, "{}");
	f.host.Update();
	ASSERT_EQ(f.sink.replies.size(), 200u);
	for (uint32_t i = 0; i < 200; ++i)
		EXPECT_EQ(f.sink.replies[i].requestId, i + 1);
}

TEST(SimQueryHost, CommandKindWorksTheSame)
{
	Fixture f;
	f.host.RegisterCommand("c", Dia::DebugServer::SimHelpInfo{"test help"}, OkHandler);
	f.source.Push(7, "c", SimKind::Command, "{}");
	f.host.Update();
	ASSERT_EQ(f.sink.replies.size(), 1u);
	EXPECT_EQ(f.sink.replies[0].requestId, 7u);
	EXPECT_EQ(f.sink.replies[0].status, St(SimReplyStatus::Ok));
}

TEST(SimQueryHost, ResyncRequestCallsHandlerOnceAndSendsNoReply)
{
	Fixture f;
	int calls = 0;
	f.host.SetResyncHandler([&calls]() { ++calls; });
	f.source.Push(0, "", SimKind::Resync, "{}");
	f.host.Update();
	EXPECT_EQ(calls, 1);
	EXPECT_TRUE(f.sink.replies.empty());
}

TEST(SimQueryHost, ResyncRequestWithNoHandlerIsSilentlyDropped)
{
	Fixture f;
	f.source.Push(0, "", SimKind::Resync, "{}");
	f.host.Update();
	EXPECT_TRUE(f.sink.replies.empty());
}

TEST(SimQueryHost, ResyncRequestNeverLooksUpNamedHandler)
{
	Fixture f;
	int handlerCalls = 0;
	int resyncCalls = 0;
	// A query and a command whose name CRC matches the resync request's nameCrc.
	ASSERT_TRUE(f.host.RegisterQuery("same", Dia::DebugServer::SimHelpInfo{"test help"}, [&handlerCalls](const Json::Value&) { ++handlerCalls; return Json::Value(Json::objectValue); }));
	ASSERT_TRUE(f.host.RegisterCommand("same2", Dia::DebugServer::SimHelpInfo{"test help"}, [&handlerCalls](const Json::Value&) { ++handlerCalls; return Json::Value(Json::objectValue); }));
	f.host.SetResyncHandler([&resyncCalls]() { ++resyncCalls; });
	f.source.Push(0, "same", SimKind::Resync, "{}");
	f.source.Push(0, "same2", SimKind::Resync, "{}");
	f.host.Update();
	EXPECT_EQ(handlerCalls, 0);
	EXPECT_EQ(resyncCalls, 2);
	EXPECT_TRUE(f.sink.replies.empty());
}

TEST(SimQueryHost, ResyncRequestDoesNotDisturbOrdinaryRequestsAroundIt)
{
	Fixture f;
	ASSERT_TRUE(f.host.RegisterQuery("q", Dia::DebugServer::SimHelpInfo{"test help"}, OkHandler));
	f.host.SetResyncHandler([]() {});
	f.source.Push(1, "q", SimKind::Query, "{}");
	f.source.Push(0, "", SimKind::Resync, "{}");
	f.source.Push(2, "q", SimKind::Query, "{}");
	f.host.Update();
	ASSERT_EQ(f.sink.replies.size(), 2u);
	EXPECT_EQ(f.sink.replies[0].requestId, 1u);
	EXPECT_EQ(f.sink.replies[1].requestId, 2u);
}

#include <gtest/gtest.h>

#include <DiaDebugServer/DebugServer.h>
#include <DiaDebugServer/QueryRegistry.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Metric/MetricRegistry.h>

#include <thread>

// Sim-Registered Queries Task 2: QueryRegistry refuses Register/Unregister from a thread that is not the owner.

using Dia::Core::StringCRC;
using Dia::DebugServer::QueryRegistry;
using Dia::CellLayers::Testing::LogCapture;

namespace
{
	Json::Value Ok(const Json::Value&) { return Json::Value(); }

	uint64_t WrongThreadCount()
	{
		auto* c = Dia::Observation::Metric::MetricRegistry::Instance().FindCounter(
			StringCRC("dia.debugserver.query.wrong_thread_register"));
		return c ? c->Value() : 0;
	}

	// Runs fn on a fresh thread (with its own log buffer, flushed before exit) and joins.
	template <typename F> void OnOtherThread(F fn)
	{
		std::thread t([&fn] {
			auto& logger = Dia::Observation::Log::Logger::Instance();
			logger.RegisterThreadBuffer();
			fn();
			logger.FlushSync();
			logger.UnregisterThreadBuffer();
		});
		t.join();
	}
}

TEST(QueryRegistryOwnerThread, NoOwnerAcceptsAnyThread)
{
	QueryRegistry reg;
	bool ok = false;
	OnOtherThread([&] { reg.Register(StringCRC("a"), &Ok); ok = reg.Has(StringCRC("a")); });
	EXPECT_TRUE(ok);
	EXPECT_TRUE(reg.Unregister(StringCRC("a")));
}

TEST(QueryRegistryOwnerThread, OwnerThreadCanRegisterAndUnregister)
{
	QueryRegistry reg;
	reg.SetOwnerThread(QueryRegistry::CurrentThreadId());
	EXPECT_TRUE(reg.Register(StringCRC("a"), &Ok));
	EXPECT_TRUE(reg.Has(StringCRC("a")));
	EXPECT_TRUE(reg.Unregister(StringCRC("a")));
	EXPECT_FALSE(reg.Has(StringCRC("a")));
}

TEST(QueryRegistryOwnerThread, RegisterFromOtherThreadIsRefusedLoggedAndCounted)
{
	QueryRegistry reg;
	reg.SetOwnerThread(QueryRegistry::CurrentThreadId());
	LogCapture log("DebugServer");
	const uint64_t before = WrongThreadCount();

	bool result = true;
	OnOtherThread([&] { result = reg.Register(StringCRC("wrong.thread.q"), &Ok); });

	EXPECT_FALSE(result);
	EXPECT_FALSE(reg.Has(StringCRC("wrong.thread.q")));
	EXPECT_EQ(1u, log.Errors("wrong.thread.q"));
	EXPECT_EQ(before + 1, WrongThreadCount());
}

TEST(QueryRegistryOwnerThread, UnregisterFromOtherThreadIsRefusedAndLeavesTableUnchanged)
{
	QueryRegistry reg;
	reg.SetOwnerThread(QueryRegistry::CurrentThreadId());
	ASSERT_TRUE(reg.Register(StringCRC("keep.me"), &Ok));
	LogCapture log("DebugServer");
	const uint64_t before = WrongThreadCount();

	bool result = true;
	OnOtherThread([&] { result = reg.Unregister(StringCRC("keep.me")); });

	EXPECT_FALSE(result);
	EXPECT_TRUE(reg.Has(StringCRC("keep.me")));
	EXPECT_EQ(1u, log.Errors("keep.me"));
	EXPECT_EQ(before + 1, WrongThreadCount());
}

TEST(QueryRegistryOwnerThread, UnregisterOfUnknownNameOnOwnerReturnsFalseWithoutError)
{
	QueryRegistry reg;
	reg.SetOwnerThread(QueryRegistry::CurrentThreadId());
	LogCapture log("DebugServer");
	EXPECT_FALSE(reg.Unregister(StringCRC("never")));
	EXPECT_EQ(0u, log.Errors());
}

TEST(QueryRegistryOwnerThread, ClearOwnerThreadRestoresAnyThread)
{
	QueryRegistry reg;
	reg.SetOwnerThread(QueryRegistry::CurrentThreadId());
	reg.ClearOwnerThread();
	bool ok = false;
	OnOtherThread([&] { ok = reg.Register(StringCRC("a"), &Ok); });
	EXPECT_TRUE(ok);
}

TEST(QueryRegistryOwnerThread, RefusedRegisterDoesNotConsumeCapacityOrBlockOwner)
{
	QueryRegistry reg;
	reg.SetOwnerThread(QueryRegistry::CurrentThreadId());
	OnOtherThread([&] { reg.Register(StringCRC("dup"), &Ok); });
	EXPECT_TRUE(reg.Register(StringCRC("dup"), &Ok));   // would assert "already registered" if the refused one had been added
}

TEST(QueryRegistryOwnerThread, DebugServerStartSetsOwnerAndStopClears)
{
	Dia::DebugServer::DebugServer server;
	server.EnableAutoStart(false);
	server.Start();

	bool refused = true;
	OnOtherThread([&] { refused = !server.GetQueryRegistry().Register(StringCRC("srv.q"), &Ok); });
	EXPECT_TRUE(refused);
	EXPECT_FALSE(server.GetQueryRegistry().Has(StringCRC("srv.q")));

	server.Stop();
	bool accepted = false;
	OnOtherThread([&] { accepted = server.GetQueryRegistry().Register(StringCRC("srv.q"), &Ok); });
	EXPECT_TRUE(accepted);
}

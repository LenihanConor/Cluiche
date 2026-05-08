#include <gtest/gtest.h>

#include "DiaDebugServer/CommandDispatcher.h"
#include "DiaDebugServer/QueryRegistry.h"

using namespace Dia::DebugServer;

class CommandDispatcherTest : public ::testing::Test
{
protected:
	CommandDispatcher dispatcher;
};

TEST_F(CommandDispatcherTest, ExecuteDiaAPICommand_UnknownCommandFails)
{
	Json::Value response = dispatcher.ExecuteDiaAPICommand(
		Dia::Core::StringCRC("nonexistent_api_command"), Json::Value::null);

	EXPECT_FALSE(response["success"].asBool());
	EXPECT_STREQ(response["message"].asCString(), "Unknown command");
}

TEST_F(CommandDispatcherTest, ExecuteDiaAPICommand_BoolAndStringArgs)
{
	Json::Value args;
	args["verbose"] = true;
	args["path"] = "/some/path";

	Json::Value response = dispatcher.ExecuteDiaAPICommand(
		Dia::Core::StringCRC("unknown_with_args"), args);

	EXPECT_FALSE(response["success"].asBool());
}

// --- QueryRegistry Tests ---

class QueryRegistryTest : public ::testing::Test
{
protected:
	QueryRegistry registry;
};

TEST_F(QueryRegistryTest, InitiallyEmpty)
{
	EXPECT_FALSE(registry.Has(Dia::Core::StringCRC("anything")));
}

TEST_F(QueryRegistryTest, Register_MakesItDiscoverable)
{
	Dia::Core::StringCRC name("test_query");
	registry.Register(name, [](const Json::Value&) -> Json::Value {
		return Json::Value("ok");
	});

	EXPECT_TRUE(registry.Has(name));
}

TEST_F(QueryRegistryTest, Unregister_RemovesHandler)
{
	Dia::Core::StringCRC name("removable");
	registry.Register(name, [](const Json::Value&) -> Json::Value {
		return Json::Value();
	});

	EXPECT_TRUE(registry.Has(name));
	registry.Unregister(name);
	EXPECT_FALSE(registry.Has(name));
}

TEST_F(QueryRegistryTest, Execute_ReturnsHandlerResult)
{
	Dia::Core::StringCRC name("get_data");
	registry.Register(name, [](const Json::Value&) -> Json::Value {
		Json::Value result;
		result["value"] = 42;
		return result;
	});

	Json::Value result = registry.Execute(name, Json::Value::null);
	EXPECT_EQ(result["value"].asInt(), 42);
}

TEST_F(QueryRegistryTest, Execute_PassesArgsToHandler)
{
	Dia::Core::StringCRC name("echo");
	registry.Register(name, [](const Json::Value& args) -> Json::Value {
		Json::Value result;
		result["echoed"] = args.get("input", "").asString();
		return result;
	});

	Json::Value args;
	args["input"] = "hello";
	Json::Value result = registry.Execute(name, args);
	EXPECT_STREQ(result["echoed"].asCString(), "hello");
}

TEST_F(QueryRegistryTest, Execute_UnknownReturnsError)
{
	Json::Value result = registry.Execute(Dia::Core::StringCRC("unknown"), Json::Value::null);
	EXPECT_FALSE(result.get("success", true).asBool());
	EXPECT_STREQ(result["error"].asCString(), "unknown query");
}

TEST_F(QueryRegistryTest, RegisterMultipleHandlers)
{
	registry.Register(Dia::Core::StringCRC("a"), [](const Json::Value&) -> Json::Value {
		Json::Value r; r["id"] = "a"; return r;
	});
	registry.Register(Dia::Core::StringCRC("b"), [](const Json::Value&) -> Json::Value {
		Json::Value r; r["id"] = "b"; return r;
	});
	registry.Register(Dia::Core::StringCRC("c"), [](const Json::Value&) -> Json::Value {
		Json::Value r; r["id"] = "c"; return r;
	});

	EXPECT_TRUE(registry.Has(Dia::Core::StringCRC("a")));
	EXPECT_TRUE(registry.Has(Dia::Core::StringCRC("b")));
	EXPECT_TRUE(registry.Has(Dia::Core::StringCRC("c")));

	EXPECT_STREQ(registry.Execute(Dia::Core::StringCRC("a"), Json::Value::null)["id"].asCString(), "a");
	EXPECT_STREQ(registry.Execute(Dia::Core::StringCRC("b"), Json::Value::null)["id"].asCString(), "b");
}

TEST_F(QueryRegistryTest, Unregister_NonexistentIsNoop)
{
	registry.Unregister(Dia::Core::StringCRC("does_not_exist"));
}

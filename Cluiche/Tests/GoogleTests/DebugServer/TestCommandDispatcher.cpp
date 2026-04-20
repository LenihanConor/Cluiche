#include <gtest/gtest.h>

#include "DiaDebugServer/CommandDispatcher.h"

using namespace Dia::DebugServer;

class CommandDispatcherTest : public ::testing::Test
{
protected:
	CommandDispatcher dispatcher;
};

TEST_F(CommandDispatcherTest, InitiallyNoProtocolCommands)
{
	EXPECT_FALSE(dispatcher.IsProtocolCommand(Dia::Core::StringCRC("get_state")));
}

TEST_F(CommandDispatcherTest, RegisterProtocolCommand_MakesItDiscoverable)
{
	Dia::Core::StringCRC cmdName("get_state");
	dispatcher.RegisterProtocolCommand(cmdName, [](const Json::Value&, Json::Value& out) {
		out["success"] = true;
	});

	EXPECT_TRUE(dispatcher.IsProtocolCommand(cmdName));
}

TEST_F(CommandDispatcherTest, UnregisterProtocolCommand_RemovesIt)
{
	Dia::Core::StringCRC cmdName("get_state");
	dispatcher.RegisterProtocolCommand(cmdName, [](const Json::Value&, Json::Value& out) {
		out["success"] = true;
	});

	EXPECT_TRUE(dispatcher.IsProtocolCommand(cmdName));

	dispatcher.UnregisterProtocolCommand(cmdName);
	EXPECT_FALSE(dispatcher.IsProtocolCommand(cmdName));
}

TEST_F(CommandDispatcherTest, UnregisterProtocolCommand_NonexistentIsNoop)
{
	dispatcher.UnregisterProtocolCommand(Dia::Core::StringCRC("nonexistent"));
}

TEST_F(CommandDispatcherTest, ExecuteProtocolCommand_CallsHandler)
{
	Dia::Core::StringCRC cmdName("test_cmd");
	bool handlerCalled = false;

	dispatcher.RegisterProtocolCommand(cmdName, [&handlerCalled](const Json::Value& payload, Json::Value& out) {
		handlerCalled = true;
		out["success"] = true;
		out["echo"] = payload.get("input", "none").asString();
	});

	Json::Value payload;
	payload["input"] = "hello";

	Json::Value response = dispatcher.ExecuteProtocolCommand(cmdName, payload, nullptr);

	EXPECT_TRUE(handlerCalled);
	EXPECT_TRUE(response["success"].asBool());
	EXPECT_STREQ(response["echo"].asCString(), "hello");
}

TEST_F(CommandDispatcherTest, ExecuteProtocolCommand_UnknownReturnsError)
{
	Json::Value response = dispatcher.ExecuteProtocolCommand(
		Dia::Core::StringCRC("unknown_cmd"), Json::Value::null, nullptr);

	EXPECT_FALSE(response["success"].asBool());
	EXPECT_STREQ(response["message"].asCString(), "Unknown protocol command");
}

TEST_F(CommandDispatcherTest, ExecuteDiaAPICommand_UnknownCommandFails)
{
	Json::Value response = dispatcher.ExecuteDiaAPICommand(
		Dia::Core::StringCRC("nonexistent_api_command"), Json::Value::null);

	EXPECT_FALSE(response["success"].asBool());
	EXPECT_STREQ(response["message"].asCString(), "Unknown command");
}

TEST_F(CommandDispatcherTest, ExecuteDiaAPICommand_ResponseContainsCommandName)
{
	Dia::Core::StringCRC cmdName("some_command");
	Json::Value response = dispatcher.ExecuteDiaAPICommand(cmdName, Json::Value::null);

	EXPECT_STREQ(response["command"].asCString(), "some_command");
	EXPECT_STREQ(response["type"].asCString(), "command_response");
}

TEST_F(CommandDispatcherTest, RegisterMultipleProtocolCommands)
{
	Dia::Core::StringCRC cmd1("cmd_a");
	Dia::Core::StringCRC cmd2("cmd_b");
	Dia::Core::StringCRC cmd3("cmd_c");

	dispatcher.RegisterProtocolCommand(cmd1, [](const Json::Value&, Json::Value& out) { out["id"] = "a"; });
	dispatcher.RegisterProtocolCommand(cmd2, [](const Json::Value&, Json::Value& out) { out["id"] = "b"; });
	dispatcher.RegisterProtocolCommand(cmd3, [](const Json::Value&, Json::Value& out) { out["id"] = "c"; });

	EXPECT_TRUE(dispatcher.IsProtocolCommand(cmd1));
	EXPECT_TRUE(dispatcher.IsProtocolCommand(cmd2));
	EXPECT_TRUE(dispatcher.IsProtocolCommand(cmd3));

	Json::Value r1 = dispatcher.ExecuteProtocolCommand(cmd1, Json::Value::null, nullptr);
	Json::Value r2 = dispatcher.ExecuteProtocolCommand(cmd2, Json::Value::null, nullptr);

	EXPECT_STREQ(r1["id"].asCString(), "a");
	EXPECT_STREQ(r2["id"].asCString(), "b");
}

TEST_F(CommandDispatcherTest, RegisterProtocolCommand_OverwritesExisting)
{
	Dia::Core::StringCRC cmdName("overwrite_me");

	dispatcher.RegisterProtocolCommand(cmdName, [](const Json::Value&, Json::Value& out) {
		out["version"] = 1;
	});

	dispatcher.RegisterProtocolCommand(cmdName, [](const Json::Value&, Json::Value& out) {
		out["version"] = 2;
	});

	Json::Value response = dispatcher.ExecuteProtocolCommand(cmdName, Json::Value::null, nullptr);
	EXPECT_EQ(response["version"].asInt(), 2);
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

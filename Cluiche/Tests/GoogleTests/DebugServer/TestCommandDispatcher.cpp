#include <gtest/gtest.h>

#include "DiaDebugServer/CommandDispatcher.h"
#include <DiaDebugProtocol/generated/debug_protocol.pb.h>

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
	dispatcher.RegisterProtocolCommand(cmdName, [](const Json::Value&, dia::debug::CommandResponse* out) {
		out->set_success(true);
	});

	EXPECT_TRUE(dispatcher.IsProtocolCommand(cmdName));
}

TEST_F(CommandDispatcherTest, UnregisterProtocolCommand_RemovesIt)
{
	Dia::Core::StringCRC cmdName("get_state");
	dispatcher.RegisterProtocolCommand(cmdName, [](const Json::Value&, dia::debug::CommandResponse* out) {
		out->set_success(true);
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

	dispatcher.RegisterProtocolCommand(cmdName, [&handlerCalled](const Json::Value& payload, dia::debug::CommandResponse* out) {
		handlerCalled = true;
		out->set_success(true);
		out->set_message(payload.get("input", "none").asString());
	});

	Json::Value payload;
	payload["input"] = "hello";

	dia::debug::CommandResponse response;
	dispatcher.ExecuteProtocolCommand(cmdName, payload, nullptr, &response);

	EXPECT_TRUE(handlerCalled);
	EXPECT_TRUE(response.success());
	EXPECT_STREQ(response.message().c_str(), "hello");
}

TEST_F(CommandDispatcherTest, ExecuteProtocolCommand_UnknownReturnsError)
{
	dia::debug::CommandResponse response;
	dispatcher.ExecuteProtocolCommand(
		Dia::Core::StringCRC("unknown_cmd"), Json::Value::null, nullptr, &response);

	EXPECT_FALSE(response.success());
	EXPECT_STREQ(response.message().c_str(), "Unknown protocol command");
}

TEST_F(CommandDispatcherTest, ExecuteDiaAPICommand_UnknownCommandFails)
{
	Json::Value response = dispatcher.ExecuteDiaAPICommand(
		Dia::Core::StringCRC("nonexistent_api_command"), Json::Value::null);

	EXPECT_FALSE(response["success"].asBool());
	EXPECT_STREQ(response["message"].asCString(), "Unknown command");
}

TEST_F(CommandDispatcherTest, RegisterMultipleProtocolCommands)
{
	Dia::Core::StringCRC cmd1("cmd_a");
	Dia::Core::StringCRC cmd2("cmd_b");
	Dia::Core::StringCRC cmd3("cmd_c");

	dispatcher.RegisterProtocolCommand(cmd1, [](const Json::Value&, dia::debug::CommandResponse* out) { out->set_message("a"); });
	dispatcher.RegisterProtocolCommand(cmd2, [](const Json::Value&, dia::debug::CommandResponse* out) { out->set_message("b"); });
	dispatcher.RegisterProtocolCommand(cmd3, [](const Json::Value&, dia::debug::CommandResponse* out) { out->set_message("c"); });

	EXPECT_TRUE(dispatcher.IsProtocolCommand(cmd1));
	EXPECT_TRUE(dispatcher.IsProtocolCommand(cmd2));
	EXPECT_TRUE(dispatcher.IsProtocolCommand(cmd3));

	dia::debug::CommandResponse r1, r2;
	dispatcher.ExecuteProtocolCommand(cmd1, Json::Value::null, nullptr, &r1);
	dispatcher.ExecuteProtocolCommand(cmd2, Json::Value::null, nullptr, &r2);

	EXPECT_STREQ(r1.message().c_str(), "a");
	EXPECT_STREQ(r2.message().c_str(), "b");
}

TEST_F(CommandDispatcherTest, RegisterProtocolCommand_OverwritesExisting)
{
	Dia::Core::StringCRC cmdName("overwrite_me");

	dispatcher.RegisterProtocolCommand(cmdName, [](const Json::Value&, dia::debug::CommandResponse* out) {
		out->set_message("v1");
	});

	dispatcher.RegisterProtocolCommand(cmdName, [](const Json::Value&, dia::debug::CommandResponse* out) {
		out->set_message("v2");
	});

	dia::debug::CommandResponse response;
	dispatcher.ExecuteProtocolCommand(cmdName, Json::Value::null, nullptr, &response);
	EXPECT_STREQ(response.message().c_str(), "v2");
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

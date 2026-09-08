#include <gtest/gtest.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::Editor;
using namespace Dia::Core;

TEST(GameConnectionManagerPendingQueue, QueueDoesNotOverflow_WhenDisconnected)
{
	GameConnectionManager manager;
	manager.Initialize();

	// Queue up to the max (8) pending commands — all should be no-ops when disconnected
	// because SendCommandWithResponse doesn't queue unless the socket is connected.
	for (int i = 0; i < 16; ++i)
	{
		Json::Value args;
		args["idx"] = i;
		manager.SendCommandWithResponse("test_cmd", args,
			[](bool, const Json::Value&) {});
	}

	SUCCEED();
	manager.Shutdown();
}

TEST(GameConnectionManagerPendingQueue, MultipleCommandsWhileDisconnected_NoCallbacks)
{
	GameConnectionManager manager;
	manager.Initialize();

	int callbackCount = 0;
	for (int i = 0; i < 8; ++i)
	{
		Json::Value args;
		manager.SendCommandWithResponse("cmd", args,
			[&](bool, const Json::Value&) { ++callbackCount; });
	}

	EXPECT_EQ(callbackCount, 0);
	manager.Shutdown();
}

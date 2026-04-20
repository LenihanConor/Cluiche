#include <gtest/gtest.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::Editor;
using namespace Dia::Core;

TEST(GameConnectionManager, InitiallyDisconnected)
{
    GameConnectionManager manager;
    EXPECT_FALSE(manager.IsConnected());
}

TEST(GameConnectionManager, Subscribe)
{
    GameConnectionManager manager;
    int callCount = 0;
    manager.Subscribe(StringCRC("test_topic"), [&](const Json::Value&) { ++callCount; });
    EXPECT_EQ(callCount, 0);
}

TEST(GameConnectionManager, ConnectionCallback)
{
    GameConnectionManager manager;
    bool callbackFired = false;
    manager.SetConnectionCallback([&](bool) { callbackFired = true; });
    EXPECT_FALSE(callbackFired);
}

TEST(GameConnectionManager, InitializeAndShutdown)
{
    GameConnectionManager manager;
    manager.Initialize();
    EXPECT_FALSE(manager.IsConnected());
    manager.Shutdown();
    EXPECT_FALSE(manager.IsConnected());
}

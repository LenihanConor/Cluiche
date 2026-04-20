#include <gtest/gtest.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::Editor;
using namespace Dia::Core;

// GameConnectionManager::Connect() requires Initialize() to have been called first.
// Full network integration tests require a running game server.
// These tests verify construction-time and post-Initialize state only.

TEST(GameConnectionManagerIntegration, InitialStateBeforeInit)
{
    GameConnectionManager manager;
    EXPECT_FALSE(manager.IsConnected());
}

TEST(GameConnectionManagerIntegration, SubscribeBeforeInit)
{
    GameConnectionManager manager;
    int received = 0;
    manager.Subscribe(StringCRC("test_topic"), [&](const Json::Value&) { ++received; });
    EXPECT_EQ(received, 0);
    EXPECT_FALSE(manager.IsConnected());
}

TEST(GameConnectionManagerIntegration, InitializedButNotConnected)
{
    GameConnectionManager manager;
    manager.Initialize();
    EXPECT_FALSE(manager.IsConnected());
    manager.Shutdown();
}

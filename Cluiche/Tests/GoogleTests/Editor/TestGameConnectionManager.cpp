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

TEST(GameConnectionManager, Unsubscribe)
{
    GameConnectionManager manager;
    int callCount = 0;
    StringCRC topic("unsub_topic");
    manager.Subscribe(topic, [&](const Json::Value&) { ++callCount; });
    manager.Unsubscribe(topic);
    EXPECT_EQ(callCount, 0);
}

TEST(GameConnectionManager, UnsubscribeNonexistentTopicIsNoop)
{
    GameConnectionManager manager;
    manager.Unsubscribe(StringCRC("nonexistent"));
}

TEST(GameConnectionManager, AutoReconnectDefaultOff)
{
    GameConnectionManager manager;
    EXPECT_FALSE(manager.GetAutoReconnect());
}

TEST(GameConnectionManager, SetAutoReconnect)
{
    GameConnectionManager manager;
    manager.SetAutoReconnect(true);
    EXPECT_TRUE(manager.GetAutoReconnect());
    manager.SetAutoReconnect(false);
    EXPECT_FALSE(manager.GetAutoReconnect());
}

TEST(GameConnectionManager, SetAutoReconnectDelayBeforeInitIsNoop)
{
    GameConnectionManager manager;
    manager.SetAutoReconnectDelay(5.0f);
    manager.SetAutoReconnectMaxAttempts(3);
}

TEST(GameConnectionManager, SetAutoReconnectAfterInit)
{
    GameConnectionManager manager;
    manager.Initialize();
    manager.SetAutoReconnect(true);
    EXPECT_TRUE(manager.GetAutoReconnect());
    manager.SetAutoReconnectDelay(2.0f);
    manager.SetAutoReconnectMaxAttempts(5);
    manager.Shutdown();
}

TEST(GameConnectionManager, SendCommandWhileDisconnectedIsNoop)
{
    GameConnectionManager manager;
    manager.Initialize();
    Json::Value args;
    args["key"] = "value";
    manager.SendCommand("test_cmd", args);
    manager.Shutdown();
}

TEST(GameConnectionManager, SendCommandWithResponseWhileDisconnected)
{
    GameConnectionManager manager;
    manager.Initialize();
    bool callbackFired = false;
    Json::Value args;
    manager.SendCommandWithResponse("test_cmd", args,
        [&](bool, const Json::Value&) { callbackFired = true; });
    EXPECT_FALSE(callbackFired);
    manager.Shutdown();
}

TEST(GameConnectionManager, PublishWhileDisconnectedIsNoop)
{
    GameConnectionManager manager;
    manager.Initialize();
    Json::Value data;
    data["test"] = true;
    manager.Publish(StringCRC("some_topic"), data);
    manager.Shutdown();
}

TEST(GameConnectionManager, SendRawWhileDisconnectedIsNoop)
{
    GameConnectionManager manager;
    manager.Initialize();
    Json::Value msg;
    msg["type"] = "test";
    manager.SendRaw(msg);
    manager.Shutdown();
}

TEST(GameConnectionManager, RawMessageCallback)
{
    GameConnectionManager manager;
    bool callbackSet = false;
    manager.SetRawMessageCallback([&](const Json::Value&) { callbackSet = true; });
    EXPECT_FALSE(callbackSet);
}

TEST(GameConnectionManager, MultipleSubscriptionsSameTopic)
{
    GameConnectionManager manager;
    int count1 = 0, count2 = 0;
    StringCRC topic("multi_topic");
    manager.Subscribe(topic, [&](const Json::Value&) { ++count1; });
    manager.Subscribe(topic, [&](const Json::Value&) { ++count2; });
    manager.Unsubscribe(topic);
    EXPECT_EQ(count1, 0);
    EXPECT_EQ(count2, 0);
}

TEST(GameConnectionManager, GetLastErrorInitiallyEmpty)
{
    GameConnectionManager manager;
    EXPECT_STREQ(manager.GetLastError(), "");
}

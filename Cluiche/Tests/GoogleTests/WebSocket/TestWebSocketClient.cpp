#include <gtest/gtest.h>
#include <DiaWebSocket/Client.h>
#include <DiaCore/Threading/Thread.h>
#include <string>
#include <atomic>

using namespace Dia::WebSocket;
using namespace Dia::Core;

// ==============================================================================
// Client Construction & Initial State
// ==============================================================================

TEST(WebSocketClient, Construction_DefaultState)
{
	Client client;

	EXPECT_FALSE(client.IsConnected());
	EXPECT_EQ(client.GetState(), ConnectionState::kDisconnected);
}

TEST(WebSocketClient, Disconnect_WhenNotConnected_NoIssue)
{
	Client client;

	client.Disconnect();

	EXPECT_FALSE(client.IsConnected());
	EXPECT_EQ(client.GetState(), ConnectionState::kDisconnected);
}

TEST(WebSocketClient, Update_WhenNotConnected_NoIssue)
{
	Client client;

	client.Update();

	SUCCEED();
}

TEST(WebSocketClient, DestructorCleansUp)
{
	{
		Client client;
	}
	SUCCEED();
}

// ==============================================================================
// Configuration Tests
// ==============================================================================

TEST(WebSocketClient, SetReconnectConfig_BeforeConnect)
{
	Client client;

	client.SetReconnectOnDisconnect(true);
	client.SetReconnectDelay(1.0f);
	client.SetReconnectMaxAttempts(3);
	client.SetConnectionTimeout(5.0f);
	client.SetMaxMessageSize(4096);

	EXPECT_EQ(client.GetState(), ConnectionState::kDisconnected);
}

// ==============================================================================
// Callback Registration Tests
// ==============================================================================

TEST(WebSocketClient, SetCallbacks_BeforeConnect)
{
	Client client;

	client.SetMessageCallback([](const Message& msg) {});
	client.SetConnectionCallback([](bool connected) {});
	client.SetErrorCallback([](const Dia::WebSocket::Error& error) {});

	EXPECT_EQ(client.GetState(), ConnectionState::kDisconnected);
}

// ==============================================================================
// Connection Failure Tests
// ==============================================================================

TEST(WebSocketClient, Connect_NoServer_FailsOrTimesOut)
{
	Client client;
	client.SetConnectionTimeout(2.0f);
	client.SetReconnectOnDisconnect(false);

	bool result = client.Connect("ws://127.0.0.1:9200");

	EXPECT_FALSE(result);
	EXPECT_FALSE(client.IsConnected());

	client.Disconnect();
}

TEST(WebSocketClient, Connect_InvalidUrl_Fails)
{
	Client client;
	client.SetConnectionTimeout(2.0f);
	client.SetReconnectOnDisconnect(false);

	bool result = client.Connect("not_a_valid_url");

	EXPECT_FALSE(result);

	client.Disconnect();
}

TEST(WebSocketClient, SendText_WhenNotConnected_NoError)
{
	Client client;

	client.SendText("hello");

	SUCCEED();
}

TEST(WebSocketClient, SendBinary_WhenNotConnected_NoError)
{
	Client client;

	int data = 42;
	client.SendBinary(&data, sizeof(data));

	SUCCEED();
}

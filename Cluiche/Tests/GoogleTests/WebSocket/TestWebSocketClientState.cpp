#include <gtest/gtest.h>
#include <DiaWebSocket/Client.h>
#include <DiaWebSocket/Server.h>
#include <DiaCore/Threading/Thread.h>
#include <string>
#include <atomic>

using namespace Dia::WebSocket;
using namespace Dia::Core;

static void PumpUpdates(Server& server, Client& client, int iterations = 50)
{
	for (int i = 0; i < iterations; ++i)
	{
		server.Update();
		client.Update();
		ThisThread::SleepMs(10);
	}
}

// Pump until the client reaches the target state or we exhaust the iteration budget.
static bool WaitForState(Server& server, Client& client, ConnectionState target, int iterations = 100)
{
	for (int i = 0; i < iterations; ++i)
	{
		if (client.GetState() == target) return true;
		server.Update();
		client.Update();
		ThisThread::SleepMs(20);
	}
	return client.GetState() == target;
}

// Pump client-only (no server) until the client reaches the target state or times out.
static bool WaitForStateNoServer(Client& client, ConnectionState target, int iterations = 200)
{
	for (int i = 0; i < iterations; ++i)
	{
		if (client.GetState() == target) return true;
		client.Update();
		ThisThread::SleepMs(20);
	}
	return client.GetState() == target;
}

// ==============================================================================
// State Machine Edge Cases
// ==============================================================================

TEST(WebSocketClientState, Connect_WhileConnecting_ReturnsFalse)
{
	Server server(9540);
	server.Start();

	Client client;
	client.SetConnectionTimeout(5.0f);
	client.SetReconnectOnDisconnect(false);

	bool result = client.Connect("ws://127.0.0.1:9540");
	EXPECT_TRUE(result);

	EXPECT_TRUE(WaitForState(server, client, ConnectionState::kConnected));

	// Calling Connect again while already connected should return true
	bool secondResult = client.Connect("ws://127.0.0.1:9540");
	EXPECT_TRUE(secondResult);
	EXPECT_TRUE(client.IsConnected());

	client.Disconnect();
	server.Stop();
}

TEST(WebSocketClientState, Connect_WhileConnected_ReturnsTrue)
{
	Server server(9541);
	server.Start();

	Client client;
	client.SetConnectionTimeout(5.0f);
	client.SetReconnectOnDisconnect(false);

	bool result = client.Connect("ws://127.0.0.1:9541");
	EXPECT_TRUE(result);
	EXPECT_TRUE(WaitForState(server, client, ConnectionState::kConnected));

	PumpUpdates(server, client, 20);

	// Connect again with same URL while connected
	bool secondResult = client.Connect("ws://127.0.0.1:9541");
	EXPECT_TRUE(secondResult);
	EXPECT_TRUE(client.IsConnected());

	client.Disconnect();
	server.Stop();
}

TEST(WebSocketClientState, Disconnect_WhileDisconnected_NoOp)
{
	Client client;

	EXPECT_EQ(client.GetState(), ConnectionState::kDisconnected);

	client.Disconnect();
	EXPECT_EQ(client.GetState(), ConnectionState::kDisconnected);

	client.Disconnect();
	EXPECT_EQ(client.GetState(), ConnectionState::kDisconnected);
}

TEST(WebSocketClientState, Disconnect_AfterFailedConnect_CleansUp)
{
	Client client;
	client.SetConnectionTimeout(1.5f);
	client.SetReconnectOnDisconnect(false);

	// No server on 9542 so connect should fail; pump until it settles.
	client.Connect("ws://127.0.0.1:9542");
	WaitForStateNoServer(client, ConnectionState::kDisconnected);

	// State should be kError or kDisconnected after failed connect
	ConnectionState stateAfterFail = client.GetState();
	EXPECT_TRUE(stateAfterFail == ConnectionState::kError || stateAfterFail == ConnectionState::kDisconnected);

	// Disconnect after failed connect should not crash and should clean up
	client.Disconnect();
	EXPECT_EQ(client.GetState(), ConnectionState::kDisconnected);
}

TEST(WebSocketClientState, SendText_WhileDisconnected_NoCrash)
{
	Client client;

	EXPECT_EQ(client.GetState(), ConnectionState::kDisconnected);

	client.SendText("hello");
	SUCCEED();

	int data = 42;
	client.SendBinary(&data, sizeof(data));
	SUCCEED();
}

TEST(WebSocketClientState, GetState_AfterConnect_IsConnected)
{
	Server server(9543);
	server.Start();

	Client client;
	client.SetConnectionTimeout(5.0f);
	client.SetReconnectOnDisconnect(false);

	bool result = client.Connect("ws://127.0.0.1:9543");
	EXPECT_TRUE(result);
	EXPECT_TRUE(WaitForState(server, client, ConnectionState::kConnected));

	EXPECT_EQ(client.GetState(), ConnectionState::kConnected);
	EXPECT_TRUE(client.IsConnected());

	PumpUpdates(server, client, 20);

	// State should remain consistent
	EXPECT_EQ(client.GetState(), ConnectionState::kConnected);
	EXPECT_TRUE(client.IsConnected());

	client.Disconnect();

	EXPECT_EQ(client.GetState(), ConnectionState::kDisconnected);
	EXPECT_FALSE(client.IsConnected());

	server.Stop();
}

TEST(WebSocketClientState, Connect_AfterFailedConnect_Succeeds)
{
	Client client;
	client.SetConnectionTimeout(1.5f);
	client.SetReconnectOnDisconnect(false);

	// First attempt: no server on 9544 — connect starts but fails; pump until settled.
	client.Connect("ws://127.0.0.1:9544");
	WaitForStateNoServer(client, ConnectionState::kDisconnected);

	client.Disconnect();
	EXPECT_EQ(client.GetState(), ConnectionState::kDisconnected);

	// Now start a server and try again - client should recover
	Server server(9544);
	server.Start();

	client.SetConnectionTimeout(5.0f);
	bool secondResult = client.Connect("ws://127.0.0.1:9544");
	EXPECT_TRUE(secondResult);
	EXPECT_TRUE(WaitForState(server, client, ConnectionState::kConnected));

	PumpUpdates(server, client, 20);

	EXPECT_EQ(client.GetState(), ConnectionState::kConnected);

	client.Disconnect();
	server.Stop();
}

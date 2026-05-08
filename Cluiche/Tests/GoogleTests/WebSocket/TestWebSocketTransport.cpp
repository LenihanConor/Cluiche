#include <gtest/gtest.h>
#include <DiaWebSocket/Server.h>
#include <DiaWebSocket/Client.h>
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

// ==============================================================================
// Reconnection & State Reset
// ==============================================================================

TEST(WebSocketTransport, Connect_Disconnect_Reconnect_Succeeds)
{
	Server server(9500);
	server.Start();

	Client client;
	client.SetConnectionTimeout(5.0f);
	client.SetReconnectOnDisconnect(false);

	bool connected = client.Connect("ws://127.0.0.1:9500");
	EXPECT_TRUE(connected);

	PumpUpdates(server, client, 20);
	EXPECT_TRUE(client.IsConnected());

	client.Disconnect();
	ThisThread::SleepMs(100);
	EXPECT_FALSE(client.IsConnected());

	connected = client.Connect("ws://127.0.0.1:9500");
	EXPECT_TRUE(connected);

	PumpUpdates(server, client, 20);
	EXPECT_TRUE(client.IsConnected());

	client.Disconnect();
	server.Stop();
}

TEST(WebSocketTransport, Connect_Timeout_ThenReconnect_Succeeds)
{
	Client client;
	client.SetConnectionTimeout(1.5f);
	client.SetReconnectOnDisconnect(false);

	bool connected = client.Connect("ws://127.0.0.1:9501");
	EXPECT_FALSE(connected);

	client.Disconnect();

	Server server(9501);
	server.Start();

	connected = client.Connect("ws://127.0.0.1:9501");
	EXPECT_TRUE(connected);

	PumpUpdates(server, client, 20);
	EXPECT_TRUE(client.IsConnected());

	client.Disconnect();
	server.Stop();
}

// ==============================================================================
// Port Reuse & Server Restart
// ==============================================================================

TEST(WebSocketTransport, ServerStart_Stop_RestartSamePort)
{
	{
		Server server(9502);
		server.Start();

		Client client;
		client.SetConnectionTimeout(5.0f);
		client.SetReconnectOnDisconnect(false);
		client.Connect("ws://127.0.0.1:9502");

		PumpUpdates(server, client, 20);
		EXPECT_TRUE(client.IsConnected());

		server.Stop();
		client.Disconnect();
	}

	ThisThread::SleepMs(100);

	Server server2(9502);
	server2.Start();

	Client client2;
	client2.SetConnectionTimeout(5.0f);
	client2.SetReconnectOnDisconnect(false);
	client2.Connect("ws://127.0.0.1:9502");

	PumpUpdates(server2, client2, 20);
	EXPECT_TRUE(client2.IsConnected());

	client2.Disconnect();
	server2.Stop();
}

// ==============================================================================
// Disconnect During Teardown
// ==============================================================================

TEST(WebSocketTransport, ClientDisconnect_WhileConnecting_NoHang)
{
	Server server(9503);
	server.Start();

	Client client;
	client.SetConnectionTimeout(5.0f);
	client.SetReconnectOnDisconnect(false);
	client.Connect("ws://127.0.0.1:9503");

	PumpUpdates(server, client, 20);
	EXPECT_TRUE(client.IsConnected());

	server.Stop();
	client.Disconnect();

	EXPECT_FALSE(client.IsConnected());
}

// ==============================================================================
// Server Send During Connection Callback
// ==============================================================================

TEST(WebSocketTransport, ServerSendsDuringClientConnect_NoLoss)
{
	Server server(9504);
	server.Start();

	server.SetConnectionCallback([&](int connId, bool connected) {
		if (connected)
		{
			server.SendText(connId, "welcome");
		}
	});

	std::string clientReceived;
	Client client;
	client.SetConnectionTimeout(5.0f);
	client.SetReconnectOnDisconnect(false);
	client.SetMessageCallback([&](const Message& msg) {
		if (msg.type == MessageType::kText)
		{
			clientReceived = msg.AsText();
		}
	});

	client.Connect("ws://127.0.0.1:9504");
	PumpUpdates(server, client);

	EXPECT_EQ(clientReceived, "welcome");

	client.Disconnect();
	server.Stop();
}

// ==============================================================================
// Rapid Connect/Disconnect Cycles
// ==============================================================================

TEST(WebSocketTransport, MultipleRapidConnectDisconnect_NoStateCorruption)
{
	Server server(9505);
	server.Start();

	for (int cycle = 0; cycle < 3; ++cycle)
	{
		Client client;
		client.SetConnectionTimeout(5.0f);
		client.SetReconnectOnDisconnect(false);
		client.Connect("ws://127.0.0.1:9505");

		PumpUpdates(server, client, 10);
		EXPECT_TRUE(client.IsConnected());

		client.Disconnect();

		for (int i = 0; i < 30; ++i)
		{
			server.Update();
			ThisThread::SleepMs(10);
		}

		EXPECT_EQ(server.GetConnectionCount(), 0);
	}

	server.Stop();
}

// ==============================================================================
// Handshake Without Update Polling
// ==============================================================================

TEST(WebSocketTransport, ClientConnect_ServerNeverCalls_Update_ClientStillConnects)
{
	Server server(9506);
	server.Start();

	Client client;
	client.SetConnectionTimeout(5.0f);
	client.SetReconnectOnDisconnect(false);

	bool connected = client.Connect("ws://127.0.0.1:9506");
	EXPECT_TRUE(connected);
	EXPECT_TRUE(client.IsConnected());

	client.Disconnect();
	server.Stop();
}

// ==============================================================================
// Large Message Delivery
// ==============================================================================

TEST(WebSocketTransport, LargeMessage_RoundTrip)
{
	Server server(9507);
	server.Start();

	std::string receivedText;
	server.SetMessageCallback([&](int connId, const Message& msg) {
		if (msg.type == MessageType::kText)
		{
			receivedText = msg.AsText();
		}
	});

	Client client;
	client.SetConnectionTimeout(5.0f);
	client.SetReconnectOnDisconnect(false);
	client.Connect("ws://127.0.0.1:9507");

	PumpUpdates(server, client, 20);

	std::string largePayload(900, 'X');
	client.SendText(largePayload.c_str());

	PumpUpdates(server, client);

	EXPECT_EQ(receivedText, largePayload);

	client.Disconnect();
	server.Stop();
}

// ==============================================================================
// Client Connection Callbacks
// ==============================================================================

TEST(WebSocketTransport, ClientConnectionCallback_FiresOnConnect)
{
	Server server(9508);
	server.Start();

	std::atomic<bool> callbackFired{false};
	bool callbackConnectedValue = false;

	Client client;
	client.SetConnectionTimeout(5.0f);
	client.SetReconnectOnDisconnect(false);
	client.SetConnectionCallback([&](bool connected) {
		callbackConnectedValue = connected;
		callbackFired = true;
	});

	client.Connect("ws://127.0.0.1:9508");
	PumpUpdates(server, client);

	EXPECT_TRUE(callbackFired.load());
	EXPECT_TRUE(callbackConnectedValue);

	client.Disconnect();
	server.Stop();
}

TEST(WebSocketTransport, ClientConnectionCallback_FiresOnServerStop)
{
	Server server(9509);
	server.Start();

	std::atomic<bool> disconnectFired{false};

	Client client;
	client.SetConnectionTimeout(5.0f);
	client.SetReconnectOnDisconnect(false);
	client.SetConnectionCallback([&](bool connected) {
		if (!connected) disconnectFired = true;
	});

	client.Connect("ws://127.0.0.1:9509");
	PumpUpdates(server, client, 20);
	EXPECT_TRUE(client.IsConnected());

	server.Stop();

	for (int i = 0; i < 50; ++i)
	{
		client.Update();
		ThisThread::SleepMs(10);
	}

	EXPECT_TRUE(disconnectFired.load());

	client.Disconnect();
}

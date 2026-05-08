#include <gtest/gtest.h>
#include <DiaWebSocket/Server.h>
#include <DiaWebSocket/Client.h>
#include <DiaCore/Threading/Thread.h>
#include <string>
#include <atomic>
#include <vector>

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
// Server + Client Connection
// ==============================================================================

TEST(WebSocketIntegration, ClientConnectsToServer)
{
	Server server(9300);
	server.Start();

	std::atomic<bool> serverSawConnect{false};
	server.SetConnectionCallback([&](int connId, bool connected) {
		if (connected) serverSawConnect = true;
	});

	Client client;
	client.SetConnectionTimeout(5.0f);
	client.SetReconnectOnDisconnect(false);

	bool connected = client.Connect("ws://127.0.0.1:9300");
	EXPECT_TRUE(connected);
	EXPECT_TRUE(client.IsConnected());

	PumpUpdates(server, client);

	EXPECT_TRUE(serverSawConnect.load());
	EXPECT_EQ(server.GetConnectionCount(), 1);

	client.Disconnect();
	server.Stop();
}

TEST(WebSocketIntegration, ServerTracksConnectionId)
{
	Server server(9301);
	server.Start();

	int capturedConnId = -1;
	server.SetConnectionCallback([&](int connId, bool connected) {
		if (connected) capturedConnId = connId;
	});

	Client client;
	client.SetConnectionTimeout(5.0f);
	client.SetReconnectOnDisconnect(false);
	client.Connect("ws://127.0.0.1:9301");

	PumpUpdates(server, client);

	EXPECT_GT(capturedConnId, 0);

	auto ids = server.GetActiveConnectionIds();
	EXPECT_EQ(ids.Size(), 1u);
	EXPECT_EQ(ids.At(0), capturedConnId);

	client.Disconnect();
	server.Stop();
}

// ==============================================================================
// Text Message Round Trip
// ==============================================================================

TEST(WebSocketIntegration, ClientSendsText_ServerReceives)
{
	Server server(9302);
	server.Start();

	std::string receivedText;
	int receivedConnId = -1;
	server.SetMessageCallback([&](int connId, const Message& msg) {
		if (msg.type == MessageType::kText)
		{
			receivedText = msg.AsText();
			receivedConnId = connId;
		}
	});

	Client client;
	client.SetConnectionTimeout(5.0f);
	client.SetReconnectOnDisconnect(false);
	client.Connect("ws://127.0.0.1:9302");

	PumpUpdates(server, client, 20);

	client.SendText("hello server");

	PumpUpdates(server, client);

	EXPECT_EQ(receivedText, "hello server");
	EXPECT_GT(receivedConnId, 0);

	client.Disconnect();
	server.Stop();
}

TEST(WebSocketIntegration, ServerSendsText_ClientReceives)
{
	Server server(9303);
	server.Start();

	int serverConnId = -1;
	server.SetConnectionCallback([&](int connId, bool connected) {
		if (connected) serverConnId = connId;
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

	client.Connect("ws://127.0.0.1:9303");

	PumpUpdates(server, client, 20);

	ASSERT_GT(serverConnId, 0);
	server.SendText(serverConnId, "hello client");

	PumpUpdates(server, client);

	EXPECT_EQ(clientReceived, "hello client");

	client.Disconnect();
	server.Stop();
}

TEST(WebSocketIntegration, BroadcastText_ClientReceives)
{
	Server server(9304);
	server.Start();

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

	client.Connect("ws://127.0.0.1:9304");
	PumpUpdates(server, client, 20);

	server.BroadcastText("broadcast message");
	PumpUpdates(server, client);

	EXPECT_EQ(clientReceived, "broadcast message");

	client.Disconnect();
	server.Stop();
}

// ==============================================================================
// Binary Message Round Trip
// ==============================================================================

TEST(WebSocketIntegration, ClientSendsBinary_ServerReceives)
{
	Server server(9305);
	server.Start();

	std::vector<char> receivedData;
	server.SetMessageCallback([&](int connId, const Message& msg) {
		if (msg.type == MessageType::kBinary)
		{
			const char* data = static_cast<const char*>(msg.data);
			receivedData.assign(data, data + msg.length);
		}
	});

	Client client;
	client.SetConnectionTimeout(5.0f);
	client.SetReconnectOnDisconnect(false);
	client.Connect("ws://127.0.0.1:9305");
	PumpUpdates(server, client, 20);

	int payload[] = {1, 2, 3, 4};
	client.SendBinary(payload, sizeof(payload));
	PumpUpdates(server, client);

	ASSERT_EQ(receivedData.size(), sizeof(payload));
	const int* received = reinterpret_cast<const int*>(receivedData.data());
	EXPECT_EQ(received[0], 1);
	EXPECT_EQ(received[1], 2);
	EXPECT_EQ(received[2], 3);
	EXPECT_EQ(received[3], 4);

	client.Disconnect();
	server.Stop();
}

TEST(WebSocketIntegration, ServerSendsBinary_ClientReceives)
{
	Server server(9306);
	server.Start();

	int serverConnId = -1;
	server.SetConnectionCallback([&](int connId, bool connected) {
		if (connected) serverConnId = connId;
	});

	std::vector<char> clientReceivedData;
	Client client;
	client.SetConnectionTimeout(5.0f);
	client.SetReconnectOnDisconnect(false);
	client.SetMessageCallback([&](const Message& msg) {
		if (msg.type == MessageType::kBinary)
		{
			const char* data = static_cast<const char*>(msg.data);
			clientReceivedData.assign(data, data + msg.length);
		}
	});

	client.Connect("ws://127.0.0.1:9306");
	PumpUpdates(server, client, 20);

	ASSERT_GT(serverConnId, 0);
	float values[] = {1.5f, 2.5f};
	server.SendBinary(serverConnId, values, sizeof(values));
	PumpUpdates(server, client);

	ASSERT_EQ(clientReceivedData.size(), sizeof(values));
	const float* received = reinterpret_cast<const float*>(clientReceivedData.data());
	EXPECT_FLOAT_EQ(received[0], 1.5f);
	EXPECT_FLOAT_EQ(received[1], 2.5f);

	client.Disconnect();
	server.Stop();
}

// ==============================================================================
// Multiple Clients
// ==============================================================================

TEST(WebSocketIntegration, MultipleClients_Connect)
{
	Server server(9307);
	server.Start();

	std::atomic<int> connectionCount{0};
	server.SetConnectionCallback([&](int connId, bool connected) {
		if (connected) connectionCount++;
	});

	Client client1;
	client1.SetConnectionTimeout(5.0f);
	client1.SetReconnectOnDisconnect(false);
	client1.Connect("ws://127.0.0.1:9307");

	Client client2;
	client2.SetConnectionTimeout(5.0f);
	client2.SetReconnectOnDisconnect(false);
	client2.Connect("ws://127.0.0.1:9307");

	for (int i = 0; i < 50; ++i)
	{
		server.Update();
		client1.Update();
		client2.Update();
		ThisThread::SleepMs(10);
	}

	EXPECT_EQ(connectionCount.load(), 2);
	EXPECT_EQ(server.GetConnectionCount(), 2);

	client1.Disconnect();
	client2.Disconnect();
	server.Stop();
}

TEST(WebSocketIntegration, BroadcastText_MultipleClients_AllReceive)
{
	Server server(9308);
	server.Start();

	std::atomic<int> receivedCount{0};

	Client client1;
	client1.SetConnectionTimeout(5.0f);
	client1.SetReconnectOnDisconnect(false);
	client1.SetMessageCallback([&](const Message& msg) {
		receivedCount++;
	});
	client1.Connect("ws://127.0.0.1:9308");

	Client client2;
	client2.SetConnectionTimeout(5.0f);
	client2.SetReconnectOnDisconnect(false);
	client2.SetMessageCallback([&](const Message& msg) {
		receivedCount++;
	});
	client2.Connect("ws://127.0.0.1:9308");

	for (int i = 0; i < 30; ++i)
	{
		server.Update();
		client1.Update();
		client2.Update();
		ThisThread::SleepMs(10);
	}

	server.BroadcastText("to all");

	for (int i = 0; i < 50; ++i)
	{
		server.Update();
		client1.Update();
		client2.Update();
		ThisThread::SleepMs(10);
	}

	EXPECT_EQ(receivedCount.load(), 2);

	client1.Disconnect();
	client2.Disconnect();
	server.Stop();
}

// ==============================================================================
// Disconnect & Cleanup
// ==============================================================================

TEST(WebSocketIntegration, ClientDisconnect_ServerNotified)
{
	Server server(9309);
	server.Start();

	std::atomic<bool> sawDisconnect{false};
	server.SetConnectionCallback([&](int connId, bool connected) {
		if (!connected) sawDisconnect = true;
	});

	{
		Client client;
		client.SetConnectionTimeout(5.0f);
		client.SetReconnectOnDisconnect(false);
		client.Connect("ws://127.0.0.1:9309");

		PumpUpdates(server, client, 20);

		EXPECT_EQ(server.GetConnectionCount(), 1);

		server.CloseConnection(1);

		PumpUpdates(server, client, 50);
	}

	for (int i = 0; i < 200; ++i)
	{
		server.Update();
		ThisThread::SleepMs(10);
		if (sawDisconnect.load()) break;
	}

	EXPECT_TRUE(sawDisconnect.load());
	EXPECT_EQ(server.GetConnectionCount(), 0);

	server.Stop();
}

TEST(WebSocketIntegration, ServerCloseConnection_ClientNotified)
{
	Server server(9310);
	server.Start();

	int serverConnId = -1;
	server.SetConnectionCallback([&](int connId, bool connected) {
		if (connected) serverConnId = connId;
	});

	std::atomic<bool> clientSawDisconnect{false};
	Client client;
	client.SetConnectionTimeout(5.0f);
	client.SetReconnectOnDisconnect(false);
	client.SetConnectionCallback([&](bool connected) {
		if (!connected) clientSawDisconnect = true;
	});

	client.Connect("ws://127.0.0.1:9310");
	PumpUpdates(server, client, 20);

	ASSERT_GT(serverConnId, 0);
	server.CloseConnection(serverConnId);

	PumpUpdates(server, client);

	EXPECT_TRUE(clientSawDisconnect.load());

	client.Disconnect();
	server.Stop();
}

// ==============================================================================
// Server Stop While Client Connected
// ==============================================================================

TEST(WebSocketIntegration, ServerStop_ClientHandlesGracefully)
{
	Server server(9311);
	server.Start();

	Client client;
	client.SetConnectionTimeout(5.0f);
	client.SetReconnectOnDisconnect(false);
	client.Connect("ws://127.0.0.1:9311");

	PumpUpdates(server, client, 20);
	EXPECT_TRUE(client.IsConnected());

	server.Stop();

	for (int i = 0; i < 50; ++i)
	{
		client.Update();
		ThisThread::SleepMs(10);
	}

	client.Disconnect();
	EXPECT_FALSE(client.IsConnected());
}

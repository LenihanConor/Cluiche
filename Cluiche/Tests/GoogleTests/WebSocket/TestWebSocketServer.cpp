#include <gtest/gtest.h>
#include <DiaWebSocket/Server.h>
#include <DiaCore/Threading/Thread.h>
#include <DiaCore/Threading/Mutex.h>
#include <thread>
#include <atomic>
#include <string>

using namespace Dia::WebSocket;
using namespace Dia::Core;

// ==============================================================================
// Server Lifecycle Tests
// ==============================================================================

TEST(WebSocketServer, Construction_SetsInitialState)
{
	Server server(9100);

	EXPECT_FALSE(server.IsRunning());
	EXPECT_EQ(server.GetConnectionCount(), 0);
}

TEST(WebSocketServer, Start_BeginsListening)
{
	Server server(9101);

	bool started = server.Start();

	EXPECT_TRUE(started);
	EXPECT_TRUE(server.IsRunning());

	server.Stop();
}

TEST(WebSocketServer, Stop_CleansUp)
{
	Server server(9102);
	server.Start();

	server.Stop();

	EXPECT_FALSE(server.IsRunning());
	EXPECT_EQ(server.GetConnectionCount(), 0);
}

TEST(WebSocketServer, DoubleStart_ReturnsFalse)
{
	Server server(9103);
	server.Start();

	bool secondStart = server.Start();

	EXPECT_FALSE(secondStart);
	server.Stop();
}

TEST(WebSocketServer, DoubleStop_NoIssue)
{
	Server server(9104);
	server.Start();

	server.Stop();
	server.Stop();

	EXPECT_FALSE(server.IsRunning());
}

TEST(WebSocketServer, UpdateWhileStopped_NoIssue)
{
	Server server(9105);

	server.Update();

	SUCCEED();
}

TEST(WebSocketServer, DestructorStopsServer)
{
	{
		Server server(9106);
		server.Start();
		EXPECT_TRUE(server.IsRunning());
	}
	SUCCEED();
}

// ==============================================================================
// Configuration Tests
// ==============================================================================

TEST(WebSocketServer, SetMaxConnections_BeforeStart)
{
	Server server(9110);
	server.SetMaxConnections(4);
	server.SetMaxMessageSize(2048);

	bool started = server.Start();
	EXPECT_TRUE(started);

	server.Stop();
}

TEST(WebSocketServer, GetActiveConnectionIds_WhenEmpty_ReturnsEmpty)
{
	Server server(9111);
	server.Start();

	auto ids = server.GetActiveConnectionIds();

	EXPECT_TRUE(ids.IsEmpty());

	server.Stop();
}

// ==============================================================================
// Callback Registration Tests
// ==============================================================================

TEST(WebSocketServer, SetCallbacks_BeforeStart)
{
	Server server(9115);
	bool messageCalled = false;
	bool connectionCalled = false;
	bool errorCalled = false;

	server.SetMessageCallback([&](int connId, const Message& msg) {
		messageCalled = true;
	});
	server.SetConnectionCallback([&](int connId, bool connected) {
		connectionCalled = true;
	});
	server.SetErrorCallback([&](const Dia::WebSocket::Error& error) {
		errorCalled = true;
	});

	bool started = server.Start();
	EXPECT_TRUE(started);

	server.Stop();
}

// ==============================================================================
// Send Without Connections Tests
// ==============================================================================

TEST(WebSocketServer, BroadcastText_NoConnections_NoError)
{
	Server server(9120);
	server.Start();

	server.BroadcastText("hello");
	server.Update();

	SUCCEED();
	server.Stop();
}

TEST(WebSocketServer, SendText_InvalidConnection_NoError)
{
	Server server(9121);
	server.Start();

	server.SendText(999, "hello");
	server.Update();

	SUCCEED();
	server.Stop();
}

TEST(WebSocketServer, BroadcastBinary_NoConnections_NoError)
{
	Server server(9122);
	server.Start();

	int data = 42;
	server.BroadcastBinary(&data, sizeof(data));
	server.Update();

	SUCCEED();
	server.Stop();
}

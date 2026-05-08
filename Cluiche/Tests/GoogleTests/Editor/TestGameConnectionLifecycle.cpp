#include <gtest/gtest.h>
#include <DiaWebSocket/Server.h>
#include <DiaEditor/LiveConnection/GameConnectionController.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>
#include <DiaDebugProtocol/DiaDebugProtocol.h>
#include <DiaCore/Threading/Thread.h>
#include <string>
#include <cstring>
#include <atomic>

using namespace Dia::WebSocket;
using namespace Dia::Core;
using namespace Dia::Editor;

// Minimal test server that mimics DebugServerModule's connection handshake:
// sends handshake_response + game_info on connect, responds to ping with pong.
class TestDebugServer
{
public:
	TestDebugServer(uint16_t port) : mServer(port) {}

	bool Start()
	{
		mServer.SetConnectionCallback([this](int connId, bool connected) {
			if (connected) OnClientConnected(connId);
		});
		mServer.SetMessageCallback([this](int connId, const Message& msg) {
			OnMessage(connId, msg);
		});
		return mServer.Start();
	}

	void Stop() { mServer.Stop(); }
	void Update() { mServer.Update(); }
	Server& GetServer() { return mServer; }

	std::string lastReceivedText;
	int connectionCount = 0;
	bool respondToPing = false;

private:
	void OnClientConnected(int connId)
	{
		connectionCount++;

		// Send handshake_response exactly as DebugServerModule::HandleConnection
		dia::debug::DebugMessage welcome;
		welcome.set_type(dia::debug::MESSAGE_TYPE_HANDSHAKE_RESPONSE);
		welcome.set_timestamp(1000);
		auto* resp = welcome.mutable_handshake_response();
		resp->set_protocol_version(Dia::DebugProtocol::kProtocolVersion);
		resp->set_accepted(true);
		resp->set_server_name("TestDebugServer");
		resp->set_server_version("1.0.0");

		char json[4096];
		if (Dia::Proto::ToJson(welcome, json, sizeof(json)))
			mServer.SendText(connId, json);

		// Send game_info exactly as DebugServerModule::SendGameInfo
		dia::debug::DebugMessage giMsg;
		giMsg.set_type(dia::debug::MESSAGE_TYPE_GAME_INFO);
		giMsg.set_timestamp(2000);
		auto* info = giMsg.mutable_game_info();
		info->set_name("TestGame");
		info->set_build("test-1.0");
		info->set_processing_unit_count(1);
		info->set_current_phase("Running");

		char giJson[4096];
		if (Dia::Proto::ToJson(giMsg, giJson, sizeof(giJson)))
			mServer.SendText(connId, giJson);
	}

	void OnMessage(int connId, const Message& msg)
	{
		if (msg.type != MessageType::kText) return;

		lastReceivedText = std::string(msg.AsText(), msg.length);

		if (respondToPing)
		{
			dia::debug::DebugMessage protoMsg;
			if (Dia::Proto::FromJson(lastReceivedText.c_str(), &protoMsg))
			{
				if (protoMsg.payload_case() == dia::debug::DebugMessage::kPing)
				{
					dia::debug::DebugMessage pongMsg;
					pongMsg.set_type(dia::debug::MESSAGE_TYPE_PONG);
					pongMsg.set_timestamp(Dia::DebugProtocol::GetTimestampNow());
					pongMsg.mutable_pong()->set_ts(protoMsg.ping().ts());
					char pongJson[4096];
					if (Dia::Proto::ToJson(pongMsg, pongJson, sizeof(pongJson)))
						mServer.SendText(connId, pongJson);
				}
			}
		}
	}

	Server mServer;
};

// Silent server: starts on a port, accepts connections, but sends nothing.
class SilentTestServer
{
public:
	SilentTestServer(uint16_t port) : mServer(port) {}

	bool Start() { return mServer.Start(); }
	void Stop() { mServer.Stop(); }
	void Update() { mServer.Update(); }

private:
	Server mServer;
};

static void PumpAll(TestDebugServer& server, GameConnectionManager& manager,
	GameConnectionController& controller, float dt, int iterations)
{
	for (int i = 0; i < iterations; ++i)
	{
		server.Update();
		manager.Update(dt);
		controller.Update(dt);
		ThisThread::SleepMs(10);
	}
}

static void PumpSilent(SilentTestServer& server, GameConnectionManager& manager,
	GameConnectionController& controller, float dt, int iterations)
{
	for (int i = 0; i < iterations; ++i)
	{
		server.Update();
		manager.Update(dt);
		controller.Update(dt);
		ThisThread::SleepMs(10);
	}
}

// =============================================================================
// 1. ConnectToRealServer_HandshakeCompletes
// =============================================================================

TEST(GameConnectionLifecycle, ConnectToRealServer_HandshakeCompletes)
{
	TestDebugServer server(9520);
	ASSERT_TRUE(server.Start());

	GameConnectionManager manager;
	manager.Initialize();

	GameConnectionController controller;
	controller.Initialize(nullptr, &manager, nullptr);

	controller.AutoConnect("ws://127.0.0.1:9520");

	PumpAll(server, manager, controller, 0.016f, 100);

	EXPECT_EQ(controller.GetState(), GameConnectionController::State::kConnected);

	controller.Shutdown();
	manager.Shutdown();
	server.Stop();
}

// =============================================================================
// 2. ConnectToRealServer_PingPong_Works
// =============================================================================

TEST(GameConnectionLifecycle, ConnectToRealServer_PingPong_Works)
{
	TestDebugServer server(9521);
	server.respondToPing = true;
	ASSERT_TRUE(server.Start());

	GameConnectionManager manager;
	manager.Initialize();

	GameConnectionController controller;
	controller.Initialize(nullptr, &manager, nullptr);

	controller.AutoConnect("ws://127.0.0.1:9521");
	PumpAll(server, manager, controller, 0.016f, 100);
	ASSERT_EQ(controller.GetState(), GameConnectionController::State::kConnected);

	// Pump with large dt to trigger the 10s heartbeat interval.
	PumpAll(server, manager, controller, 5.0f, 10);

	EXPECT_EQ(controller.GetState(), GameConnectionController::State::kConnected);

	controller.Shutdown();
	manager.Shutdown();
	server.Stop();
}

// =============================================================================
// 3. ConnectToRealServer_ServerSendsLog_ControllerReceives
// =============================================================================

TEST(GameConnectionLifecycle, ConnectToRealServer_ServerSendsLog_ControllerReceives)
{
	TestDebugServer server(9522);
	ASSERT_TRUE(server.Start());

	GameConnectionManager manager;
	manager.Initialize();

	GameConnectionController controller;
	controller.Initialize(nullptr, &manager, nullptr);

	controller.AutoConnect("ws://127.0.0.1:9522");
	PumpAll(server, manager, controller, 0.016f, 100);
	ASSERT_EQ(controller.GetState(), GameConnectionController::State::kConnected);

	// Server broadcasts a log proto.
	dia::debug::DebugMessage logMsg;
	logMsg.set_type(dia::debug::MESSAGE_TYPE_LOG);
	logMsg.set_timestamp(3000);
	auto* log = logMsg.mutable_log();
	log->set_level("info");
	log->set_message("Test log entry from server");
	log->set_channel("test");

	char logJson[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(logMsg, logJson, sizeof(logJson)));
	server.GetServer().BroadcastText(logJson);

	PumpAll(server, manager, controller, 0.016f, 50);

	// Smoke test: controller didn't crash or disconnect.
	EXPECT_EQ(controller.GetState(), GameConnectionController::State::kConnected);

	controller.Shutdown();
	manager.Shutdown();
	server.Stop();
}

// =============================================================================
// 4. ConnectToRealServer_Disconnect_CleanShutdown
// =============================================================================

TEST(GameConnectionLifecycle, ConnectToRealServer_Disconnect_CleanShutdown)
{
	TestDebugServer server(9523);
	ASSERT_TRUE(server.Start());

	GameConnectionManager manager;
	manager.Initialize();

	GameConnectionController controller;
	controller.Initialize(nullptr, &manager, nullptr);

	controller.AutoConnect("ws://127.0.0.1:9523");
	PumpAll(server, manager, controller, 0.016f, 100);
	ASSERT_EQ(controller.GetState(), GameConnectionController::State::kConnected);

	manager.Disconnect();
	PumpAll(server, manager, controller, 0.016f, 50);

	EXPECT_EQ(controller.GetState(), GameConnectionController::State::kDisconnected);

	controller.Shutdown();
	manager.Shutdown();
	server.Stop();
}

// =============================================================================
// 5. ConnectToDeadServer_HandshakeTimeout_Fires
// =============================================================================

TEST(GameConnectionLifecycle, ConnectToDeadServer_HandshakeTimeout_Fires)
{
	// Silent server accepts the WebSocket connection but never sends any protos.
	SilentTestServer server(9524);
	ASSERT_TRUE(server.Start());

	GameConnectionManager manager;
	manager.Initialize();

	GameConnectionController controller;
	controller.Initialize(nullptr, &manager, nullptr);

	controller.AutoConnect("ws://127.0.0.1:9524");

	// Pump with dt=1.0f for 6 iterations to exceed the 5s handshake timeout.
	PumpSilent(server, manager, controller, 1.0f, 6);

	EXPECT_EQ(controller.GetState(), GameConnectionController::State::kDisconnected);
	EXPECT_STRNE(controller.GetLastError(), "");

	controller.Shutdown();
	manager.Shutdown();
	server.Stop();
}

// =============================================================================
// 6. Connect_Disconnect_Connect_SecondAttemptSucceeds
// =============================================================================

TEST(GameConnectionLifecycle, Connect_Disconnect_Connect_SecondAttemptSucceeds)
{
	TestDebugServer server(9525);
	ASSERT_TRUE(server.Start());

	GameConnectionManager manager;
	manager.Initialize();

	GameConnectionController controller;
	controller.Initialize(nullptr, &manager, nullptr);

	// First connection.
	controller.AutoConnect("ws://127.0.0.1:9525");
	PumpAll(server, manager, controller, 0.016f, 100);
	ASSERT_EQ(controller.GetState(), GameConnectionController::State::kConnected);

	// Disconnect.
	manager.Disconnect();
	PumpAll(server, manager, controller, 0.016f, 50);
	ASSERT_EQ(controller.GetState(), GameConnectionController::State::kDisconnected);

	// Reconnect using AutoConnect (exercises the reconnect-after-disconnect path).
	controller.AutoConnect("ws://127.0.0.1:9525");
	PumpAll(server, manager, controller, 0.016f, 100);

	EXPECT_EQ(controller.GetState(), GameConnectionController::State::kConnected);

	controller.Shutdown();
	manager.Shutdown();
	server.Stop();
}

// =============================================================================
// 7. ControllerShutdown_DuringConnect_NoAssert
// =============================================================================

TEST(GameConnectionLifecycle, ControllerShutdown_DuringConnect_NoAssert)
{
	SilentTestServer server(9526);
	ASSERT_TRUE(server.Start());

	GameConnectionManager manager;
	manager.Initialize();

	GameConnectionController controller;
	controller.Initialize(nullptr, &manager, nullptr);

	controller.AutoConnect("ws://127.0.0.1:9526");
	PumpSilent(server, manager, controller, 0.016f, 20);

	// Still in kConnecting because the silent server never sends game_info.
	EXPECT_EQ(controller.GetState(), GameConnectionController::State::kConnecting);

	// Shutdown while connecting; should not assert or crash.
	controller.Shutdown();
	manager.Shutdown();
	server.Stop();
}

// =============================================================================
// 8. ManagerUpdate_Required_ForMessages
// =============================================================================

TEST(GameConnectionLifecycle, ManagerUpdate_Required_ForMessages)
{
	TestDebugServer server(9527);
	ASSERT_TRUE(server.Start());

	GameConnectionManager manager;
	manager.Initialize();

	GameConnectionController controller;
	controller.Initialize(nullptr, &manager, nullptr);

	controller.AutoConnect("ws://127.0.0.1:9527");

	// Only pump server and controller -- skip manager.Update().
	for (int i = 0; i < 100; ++i)
	{
		server.Update();
		controller.Update(0.016f);
		ThisThread::SleepMs(10);
	}

	// Without manager.Update(), the WebSocket client's incoming queue is never
	// drained, so game_info never arrives and the controller stays in kConnecting.
	EXPECT_EQ(controller.GetState(), GameConnectionController::State::kConnecting);

	controller.Shutdown();
	manager.Shutdown();
	server.Stop();
}

// =============================================================================
// 9. ServerDies_ControllerDetects
// =============================================================================

TEST(GameConnectionLifecycle, ServerDies_ControllerDetects)
{
	TestDebugServer server(9528);
	ASSERT_TRUE(server.Start());

	GameConnectionManager manager;
	manager.Initialize();

	GameConnectionController controller;
	controller.Initialize(nullptr, &manager, nullptr);

	controller.AutoConnect("ws://127.0.0.1:9528");
	PumpAll(server, manager, controller, 0.016f, 100);
	ASSERT_EQ(controller.GetState(), GameConnectionController::State::kConnected);

	// Kill the server.
	server.Stop();

	// Pump with large dt to exceed the 20s pong timeout.
	for (int i = 0; i < 5; ++i)
	{
		manager.Update(5.0f);
		controller.Update(5.0f);
		ThisThread::SleepMs(10);
	}

	EXPECT_EQ(controller.GetState(), GameConnectionController::State::kDisconnected);

	controller.Shutdown();
	manager.Shutdown();
}

// =============================================================================
// 10. AutoConnect_WithValidServer_Connects
// =============================================================================

TEST(GameConnectionLifecycle, AutoConnect_WithValidServer_Connects)
{
	TestDebugServer server(9529);
	ASSERT_TRUE(server.Start());

	GameConnectionManager manager;
	manager.Initialize();

	GameConnectionController controller;
	controller.Initialize(nullptr, &manager, nullptr);

	controller.AutoConnect("ws://127.0.0.1:9529");

	PumpAll(server, manager, controller, 0.016f, 100);

	EXPECT_EQ(controller.GetState(), GameConnectionController::State::kConnected);

	manager.Disconnect();
	controller.Shutdown();
	manager.Shutdown();
	server.Stop();
}

#include <gtest/gtest.h>
#include <DiaWebSocket/Error.h>
#include <DiaWebSocket/Message.h>
#include <DiaWebSocket/Server.h>
#include <DiaWebSocket/Client.h>
#include <DiaCore/Threading/Thread.h>
#include <cstring>
#include <atomic>
#include <string>

using namespace Dia::WebSocket;
using namespace Dia::Core;

// ==============================================================================
// Error Struct Tests
// ==============================================================================

TEST(WebSocketError, MakeError_SetsAllFields)
{
	Error err = Internal::MakeError(
		ErrorCode::kConnectionFailed,
		ErrorSeverity::kError,
		42,
		"Connection to %s failed", "localhost");

	EXPECT_EQ(err.code, ErrorCode::kConnectionFailed);
	EXPECT_EQ(err.severity, ErrorSeverity::kError);
	EXPECT_EQ(err.connectionId, 42);
	EXPECT_STREQ(err.GetMessage(), "Connection to localhost failed");
}

TEST(WebSocketError, GetCodeName_AllCodes)
{
	EXPECT_STREQ(Internal::MakeError(ErrorCode::kNone, ErrorSeverity::kWarning, -1, "").GetCodeName(), "kNone");
	EXPECT_STREQ(Internal::MakeError(ErrorCode::kConnectionFailed, ErrorSeverity::kError, -1, "").GetCodeName(), "kConnectionFailed");
	EXPECT_STREQ(Internal::MakeError(ErrorCode::kConnectionTimeout, ErrorSeverity::kError, -1, "").GetCodeName(), "kConnectionTimeout");
	EXPECT_STREQ(Internal::MakeError(ErrorCode::kConnectionRejected, ErrorSeverity::kWarning, -1, "").GetCodeName(), "kConnectionRejected");
	EXPECT_STREQ(Internal::MakeError(ErrorCode::kConnectionCloseFailed, ErrorSeverity::kWarning, -1, "").GetCodeName(), "kConnectionCloseFailed");
	EXPECT_STREQ(Internal::MakeError(ErrorCode::kMessageTooLarge, ErrorSeverity::kError, -1, "").GetCodeName(), "kMessageTooLarge");
	EXPECT_STREQ(Internal::MakeError(ErrorCode::kMessageQueueFull, ErrorSeverity::kWarning, -1, "").GetCodeName(), "kMessageQueueFull");
	EXPECT_STREQ(Internal::MakeError(ErrorCode::kSendFailed, ErrorSeverity::kError, -1, "").GetCodeName(), "kSendFailed");
	EXPECT_STREQ(Internal::MakeError(ErrorCode::kReconnectFailed, ErrorSeverity::kWarning, -1, "").GetCodeName(), "kReconnectFailed");
	EXPECT_STREQ(Internal::MakeError(ErrorCode::kReconnectExhausted, ErrorSeverity::kError, -1, "").GetCodeName(), "kReconnectExhausted");
	EXPECT_STREQ(Internal::MakeError(ErrorCode::kInternalError, ErrorSeverity::kError, -1, "").GetCodeName(), "kInternalError");
}

TEST(WebSocketError, GetSeverityName_AllLevels)
{
	Error warning = Internal::MakeError(ErrorCode::kNone, ErrorSeverity::kWarning, -1, "");
	Error error = Internal::MakeError(ErrorCode::kNone, ErrorSeverity::kError, -1, "");
	Error fatal = Internal::MakeError(ErrorCode::kNone, ErrorSeverity::kFatal, -1, "");

	EXPECT_STREQ(warning.GetSeverityName(), "WARNING");
	EXPECT_STREQ(error.GetSeverityName(), "ERROR");
	EXPECT_STREQ(fatal.GetSeverityName(), "FATAL");
}

TEST(WebSocketError, MakeError_LongMessage_Truncated)
{
	char longMsg[512];
	memset(longMsg, 'A', sizeof(longMsg) - 1);
	longMsg[511] = '\0';

	Error err = Internal::MakeError(ErrorCode::kInternalError, ErrorSeverity::kError, -1, "%s", longMsg);

	EXPECT_LT(strlen(err.GetMessage()), 256u);
}

TEST(WebSocketError, MessageH_IncludesErrorH)
{
	Error err;
	err.code = ErrorCode::kNone;
	err.severity = ErrorSeverity::kWarning;
	EXPECT_EQ(err.code, ErrorCode::kNone);
}

// ==============================================================================
// Server Error Callback Integration
// ==============================================================================

TEST(WebSocketError, ServerErrorCallback_ReceivesStructuredError)
{
	Server server(9400);
	server.SetMaxConnections(1);
	server.Start();

	ErrorCode receivedCode = ErrorCode::kNone;
	ErrorSeverity receivedSeverity = ErrorSeverity::kWarning;
	std::string receivedMessage;

	server.SetErrorCallback([&](const Error& error) {
		receivedCode = error.code;
		receivedSeverity = error.severity;
		receivedMessage = error.GetMessage();
	});

	Client client1;
	client1.SetConnectionTimeout(5.0f);
	client1.SetReconnectOnDisconnect(false);
	client1.Connect("ws://127.0.0.1:9400");

	for (int i = 0; i < 30; ++i)
	{
		server.Update();
		client1.Update();
		ThisThread::SleepMs(10);
	}

	EXPECT_EQ(server.GetConnectionCount(), 1);

	Client client2;
	client2.SetConnectionTimeout(3.0f);
	client2.SetReconnectOnDisconnect(false);
	client2.Connect("ws://127.0.0.1:9400");

	for (int i = 0; i < 50; ++i)
	{
		server.Update();
		client2.Update();
		ThisThread::SleepMs(10);
	}

	EXPECT_EQ(receivedCode, ErrorCode::kConnectionRejected);
	EXPECT_EQ(receivedSeverity, ErrorSeverity::kWarning);
	EXPECT_FALSE(receivedMessage.empty());

	client1.Disconnect();
	client2.Disconnect();
	server.Stop();
}

// ==============================================================================
// Client Error Callback Integration
// ==============================================================================

TEST(WebSocketError, ClientErrorCallback_ConnectionFailed)
{
	Client client;
	client.SetConnectionTimeout(2.0f);
	client.SetReconnectOnDisconnect(false);

	std::atomic<bool> errorReceived{false};
	ErrorCode receivedCode = ErrorCode::kNone;

	client.SetErrorCallback([&](const Error& error) {
		receivedCode = error.code;
		errorReceived = true;
	});

	client.Connect("ws://127.0.0.1:9401");

	for (int i = 0; i < 30; ++i)
	{
		client.Update();
		ThisThread::SleepMs(10);
	}

	if (errorReceived.load())
	{
		EXPECT_EQ(receivedCode, ErrorCode::kConnectionFailed);
	}

	client.Disconnect();
}

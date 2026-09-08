#include <gtest/gtest.h>
#include <DiaWebSocket/Server.h>
#include <cstring>

using namespace Dia::WebSocket;

TEST(WebSocketServerBuffer, SendText_LargePayload_NoAssert)
{
	Server server(9130);
	server.Start();

	// Build a payload larger than the old 1024 limit but within the new 8192 limit
	char largePayload[4096];
	memset(largePayload, 'A', sizeof(largePayload) - 1);
	largePayload[sizeof(largePayload) - 1] = '\0';

	// No connected client, so message is queued and dropped — but must not assert
	server.SendText(1, largePayload);
	server.Update();

	SUCCEED();
	server.Stop();
}

TEST(WebSocketServerBuffer, BroadcastText_LargePayload_NoAssert)
{
	Server server(9131);
	server.Start();

	char largePayload[4096];
	memset(largePayload, 'B', sizeof(largePayload) - 1);
	largePayload[sizeof(largePayload) - 1] = '\0';

	server.BroadcastText(largePayload);
	server.Update();

	SUCCEED();
	server.Stop();
}

TEST(WebSocketServerBuffer, SendText_AtBufferLimit_NoAssert)
{
	Server server(9132);
	server.Start();

	// Exactly at the buffer boundary (8191 chars + null)
	char maxPayload[8191];
	memset(maxPayload, 'C', sizeof(maxPayload) - 1);
	maxPayload[sizeof(maxPayload) - 1] = '\0';

	server.SendText(1, maxPayload);
	server.Update();

	SUCCEED();
	server.Stop();
}

TEST(WebSocketServerBuffer, SendBinary_LargePayload_NoAssert)
{
	Server server(9133);
	server.Start();

	char binaryData[4096];
	memset(binaryData, 0x42, sizeof(binaryData));

	server.SendBinary(1, binaryData, sizeof(binaryData));
	server.Update();

	SUCCEED();
	server.Stop();
}

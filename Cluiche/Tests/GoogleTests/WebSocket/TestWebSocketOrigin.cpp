#include <gtest/gtest.h>
#include <DiaWebSocket/Server.h>
#include <DiaCore/Threading/Thread.h>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

using namespace Dia::WebSocket;
using namespace Dia::Core;

namespace
{
	// Sends a raw WebSocket upgrade request (optionally with an Origin header) and
	// returns the first line of the HTTP response ("" if the connection failed).
	std::string Handshake(const char* host, uint16_t port, const char* origin)
	{
		WSADATA wsa;
		if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return "";

		std::string result;
		SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		sockaddr_in addr = {};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		inet_pton(AF_INET, host, &addr.sin_addr);

		if (s != INVALID_SOCKET && connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0)
		{
			DWORD timeoutMs = 3000;
			setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));

			std::string req = "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\n"
				"Connection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
				"Sec-WebSocket-Version: 13\r\n";
			if (origin) req += std::string("Origin: ") + origin + "\r\n";
			req += "\r\n";
			send(s, req.c_str(), static_cast<int>(req.size()), 0);

			char buf[256] = {};
			int n = recv(s, buf, sizeof(buf) - 1, 0);
			if (n > 0)
			{
				result.assign(buf, n);
				size_t eol = result.find("\r\n");
				if (eol != std::string::npos) result.resize(eol);
			}
		}
		if (s != INVALID_SOCKET) closesocket(s);
		WSACleanup();
		return result;
	}
}

TEST(SLOW_WebSocketOrigin, ForeignOriginIsRejected)
{
	Server server(9391);
	ASSERT_TRUE(server.Start());
	ThisThread::SleepMs(100);

	std::string line = Handshake("127.0.0.1", 9391, "https://evil.example");
	EXPECT_NE(line.find("400"), std::string::npos) << line;
	EXPECT_EQ(line.find("101"), std::string::npos) << line;
	server.Stop();
}

TEST(SLOW_WebSocketOrigin, LookalikeLoopbackOriginIsRejected)
{
	Server server(9392);
	ASSERT_TRUE(server.Start());
	ThisThread::SleepMs(100);

	std::string line = Handshake("127.0.0.1", 9392, "http://localhost.evil.example");
	EXPECT_NE(line.find("400"), std::string::npos) << line;
	EXPECT_EQ(line.find("101"), std::string::npos) << line;
	server.Stop();
}

TEST(SLOW_WebSocketOrigin, LoopbackOriginsAreAccepted)
{
	Server server(9393);
	ASSERT_TRUE(server.Start());
	ThisThread::SleepMs(100);

	EXPECT_NE(Handshake("127.0.0.1", 9393, "http://localhost:5173").find("101"), std::string::npos);
	EXPECT_NE(Handshake("127.0.0.1", 9393, "http://127.0.0.1:3000").find("101"), std::string::npos);
	EXPECT_NE(Handshake("127.0.0.1", 9393, "http://[::1]:3000").find("101"), std::string::npos);
	server.Stop();
}

TEST(SLOW_WebSocketOrigin, MissingOriginIsAccepted)
{
	Server server(9394);
	ASSERT_TRUE(server.Start());
	ThisThread::SleepMs(100);

	EXPECT_NE(Handshake("127.0.0.1", 9394, nullptr).find("101"), std::string::npos);
	server.Stop();
}

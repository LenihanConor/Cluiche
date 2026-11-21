#include <gtest/gtest.h>
#include <DiaWebSocket/Server.h>
#include <DiaWebSocket/Client.h>
#include <DiaCore/Threading/Thread.h>

#include <atomic>
#include <string>
#include <vector>

using namespace Dia::WebSocket;
using namespace Dia::Core;

// Loopback tests for messages larger than the old fixed 8 KB message buffer (AD-001).
namespace
{
	std::atomic<int> sNextPort{9700};

	std::string MakeText(size_t size, int seed = 0)
	{
		std::string s(size, 'a');
		for (size_t i = 0; i < size; ++i)
		{
			s[i] = static_cast<char>('a' + ((i + static_cast<size_t>(seed)) % 26));
		}
		return s;
	}

	std::string MakeBinary(size_t size, int seed = 0)
	{
		std::string s(size, '\0');
		for (size_t i = 0; i < size; ++i)
		{
			s[i] = static_cast<char>((i * 7 + static_cast<size_t>(seed)) & 0xFF); // includes embedded zeros
		}
		return s;
	}
}

class SLOW_WebSocketLargeMessagesTest : public ::testing::TestWithParam<size_t>
{
protected:
	void SetUp() override
	{
		mPort = sNextPort++;
		mServer = new Server(static_cast<uint16_t>(mPort));
		mServer->Start();
		mServer->SetMessageCallback([this](int connId, const Message& m) {
			mServerReceived.emplace_back(static_cast<const char*>(m.data), m.length);
			mServerReceivedType.push_back(m.type);
			mServerConnId = connId;
		});
		mServer->SetConnectionCallback([this](int connId, bool connected) {
			if (connected) mServerConnId = connId;
		});

		mClient = new Client();
		mClient->SetConnectionTimeout(5.0f);
		mClient->SetReconnectOnDisconnect(false);
		mClient->SetMessageCallback([this](const Message& m) {
			mClientReceived.emplace_back(static_cast<const char*>(m.data), m.length);
			mClientReceivedType.push_back(m.type);
		});
		ASSERT_TRUE(mClient->Connect(("ws://127.0.0.1:" + std::to_string(mPort)).c_str()));
		PumpUntil([this] { return mClient->IsConnected() && mServerConnId > 0; });
		ASSERT_TRUE(mClient->IsConnected());
		ASSERT_GT(mServerConnId, 0);
	}

	void TearDown() override
	{
		if (mClient) mClient->Disconnect();
		if (mServer) mServer->Stop();
		delete mClient;
		delete mServer;
	}

	template <class Pred>
	bool PumpUntil(Pred pred, int maxMs = 5000)
	{
		for (int t = 0; t < maxMs; t += 5)
		{
			mServer->Update();
			mClient->Update();
			if (pred()) return true;
			ThisThread::SleepMs(5);
		}
		return false;
	}

	int mPort = 0;
	Server* mServer = nullptr;
	Client* mClient = nullptr;
	int mServerConnId = -1;
	std::vector<std::string> mServerReceived;
	std::vector<MessageType> mServerReceivedType;
	std::vector<std::string> mClientReceived;
	std::vector<MessageType> mClientReceivedType;
};

TEST_P(SLOW_WebSocketLargeMessagesTest, ServerToClient_Text_RoundTripsIntact)
{
	const std::string payload = MakeText(GetParam());
	ASSERT_TRUE(mServer->Send(mServerConnId, payload.data(), payload.size(), MessageType::kText));
	ASSERT_TRUE(PumpUntil([this] { return !mClientReceived.empty(); }));
	ASSERT_EQ(mClientReceived.size(), 1u);
	EXPECT_EQ(mClientReceivedType[0], MessageType::kText);
	EXPECT_EQ(mClientReceived[0].size(), payload.size());
	EXPECT_TRUE(mClientReceived[0] == payload);
}

TEST_P(SLOW_WebSocketLargeMessagesTest, ClientToServer_Text_RoundTripsIntact)
{
	const std::string payload = MakeText(GetParam());
	ASSERT_TRUE(mClient->Send(payload.data(), payload.size(), MessageType::kText));
	ASSERT_TRUE(PumpUntil([this] { return !mServerReceived.empty(); }));
	ASSERT_EQ(mServerReceived.size(), 1u);
	EXPECT_EQ(mServerReceivedType[0], MessageType::kText);
	EXPECT_EQ(mServerReceived[0].size(), payload.size());
	EXPECT_TRUE(mServerReceived[0] == payload);
}

TEST_P(SLOW_WebSocketLargeMessagesTest, ServerToClient_Binary_RoundTripsIntact)
{
	const std::string payload = MakeBinary(GetParam());
	ASSERT_TRUE(mServer->Send(mServerConnId, payload.data(), payload.size(), MessageType::kBinary));
	ASSERT_TRUE(PumpUntil([this] { return !mClientReceived.empty(); }));
	ASSERT_EQ(mClientReceived.size(), 1u);
	EXPECT_EQ(mClientReceivedType[0], MessageType::kBinary);
	EXPECT_TRUE(mClientReceived[0] == payload);
}

TEST_P(SLOW_WebSocketLargeMessagesTest, ClientToServer_Binary_RoundTripsIntact)
{
	const std::string payload = MakeBinary(GetParam());
	ASSERT_TRUE(mClient->Send(payload.data(), payload.size(), MessageType::kBinary));
	ASSERT_TRUE(PumpUntil([this] { return !mServerReceived.empty(); }));
	ASSERT_EQ(mServerReceived.size(), 1u);
	EXPECT_EQ(mServerReceivedType[0], MessageType::kBinary);
	EXPECT_TRUE(mServerReceived[0] == payload);
}

TEST_P(SLOW_WebSocketLargeMessagesTest, Broadcast_Text_RoundTripsIntact)
{
	const std::string payload = MakeText(GetParam(), 3);
	mServer->Broadcast(payload.data(), payload.size(), MessageType::kText);
	ASSERT_TRUE(PumpUntil([this] { return !mClientReceived.empty(); }));
	ASSERT_EQ(mClientReceived.size(), 1u);
	EXPECT_TRUE(mClientReceived[0] == payload);
}

INSTANTIATE_TEST_SUITE_P(Sizes, SLOW_WebSocketLargeMessagesTest,
	::testing::Values(size_t(8191), size_t(8192), size_t(8193), size_t(64 * 1024), size_t(1024 * 1024 - 1)));

class SLOW_WebSocketLargeMessagesLimitTest : public SLOW_WebSocketLargeMessagesTest {};

TEST_P(SLOW_WebSocketLargeMessagesLimitTest, ExactlyAtLimit_IsDelivered)
{
	const std::string payload = MakeBinary(1024 * 1024);
	ASSERT_TRUE(mClient->Send(payload.data(), payload.size(), MessageType::kBinary));
	ASSERT_TRUE(PumpUntil([this] { return !mServerReceived.empty(); }));
	EXPECT_TRUE(mServerReceived[0] == payload);
}

TEST_P(SLOW_WebSocketLargeMessagesLimitTest, OverLimit_ServerSendFailsCleanly)
{
	const std::string payload = MakeText(1024 * 1024 + 1);
	EXPECT_FALSE(mServer->Send(mServerConnId, payload.data(), payload.size(), MessageType::kText));
	PumpUntil([this] { return false; }, 200);
	EXPECT_TRUE(mClientReceived.empty());
	EXPECT_TRUE(mClient->IsConnected());
}

TEST_P(SLOW_WebSocketLargeMessagesLimitTest, OverLimit_ClientSendFailsCleanly)
{
	const std::string payload = MakeBinary(1024 * 1024 + 1);
	EXPECT_FALSE(mClient->Send(payload.data(), payload.size(), MessageType::kBinary));
	PumpUntil([this] { return false; }, 200);
	EXPECT_TRUE(mServerReceived.empty());
	EXPECT_TRUE(mClient->IsConnected());

	// Still usable afterwards
	const std::string ok = MakeText(20000);
	EXPECT_TRUE(mClient->Send(ok.data(), ok.size(), MessageType::kText));
	ASSERT_TRUE(PumpUntil([this] { return !mServerReceived.empty(); }));
	EXPECT_TRUE(mServerReceived[0] == ok);
}

TEST_P(SLOW_WebSocketLargeMessagesLimitTest, ManyConsecutiveLargeMessages_AllDeliveredInOrder)
{
	const int kCount = 30;
	const size_t kSize = 64 * 1024 + 5;

	for (int i = 0; i < kCount; ++i)
	{
		const std::string c2s = MakeText(kSize, i);
		const std::string s2c = MakeBinary(kSize, i);
		ASSERT_TRUE(mClient->Send(c2s.data(), c2s.size(), MessageType::kText));
		ASSERT_TRUE(mServer->Send(mServerConnId, s2c.data(), s2c.size(), MessageType::kBinary));
		// Drain between sends so the queues never legitimately fill
		PumpUntil([&] { return static_cast<int>(mServerReceived.size()) > i && static_cast<int>(mClientReceived.size()) > i; });
	}

	ASSERT_EQ(mServerReceived.size(), static_cast<size_t>(kCount));
	ASSERT_EQ(mClientReceived.size(), static_cast<size_t>(kCount));
	for (int i = 0; i < kCount; ++i)
	{
		EXPECT_TRUE(mServerReceived[i] == MakeText(kSize, i)) << "client->server message " << i;
		EXPECT_TRUE(mClientReceived[i] == MakeBinary(kSize, i)) << "server->client message " << i;
	}

	// Queues fully drained: further Updates deliver nothing new
	PumpUntil([] { return false; }, 100);
	EXPECT_EQ(mServerReceived.size(), static_cast<size_t>(kCount));
	EXPECT_EQ(mClientReceived.size(), static_cast<size_t>(kCount));
}

TEST_P(SLOW_WebSocketLargeMessagesLimitTest, BurstOfLargeMessages_BeforeDrain_AllDelivered)
{
	const int kCount = 20;
	const size_t kSize = 100 * 1000;
	for (int i = 0; i < kCount; ++i)
	{
		const std::string s = MakeText(kSize, i);
		ASSERT_TRUE(mClient->Send(s.data(), s.size(), MessageType::kText));
	}
	ASSERT_TRUE(PumpUntil([&] { return static_cast<int>(mServerReceived.size()) >= kCount; }));
	ASSERT_EQ(mServerReceived.size(), static_cast<size_t>(kCount));
	for (int i = 0; i < kCount; ++i)
	{
		EXPECT_TRUE(mServerReceived[i] == MakeText(kSize, i)) << "message " << i;
	}
}

INSTANTIATE_TEST_SUITE_P(Once, SLOW_WebSocketLargeMessagesLimitTest, ::testing::Values(size_t(0)));

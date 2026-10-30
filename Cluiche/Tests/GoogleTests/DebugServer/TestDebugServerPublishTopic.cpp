#include <gtest/gtest.h>

#include <DiaDebugServer/DebugServer.h>
#include <DiaDebugServer/IDebugStateProvider.h>
#include <DiaDebugProtocol/DiaDebugProtocol.h>
#include <DiaProtobuf/ProtoJsonCodec.h>
#include <DiaProtobuf/ProtoStructConverter.h>
#include <DiaWebSocket/Client.h>
#include <DiaCore/Threading/Thread.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <string>
#include <thread>
#include <vector>

// Loopback tests for DebugServer::PublishTopic: real DebugServer + real WebSocket client.
namespace
{
    class PublishTestProvider : public Dia::DebugServer::IDebugStateProvider
    {
    public:
        Dia::Core::StringCRC GetCurrentStage() const override { return Dia::Core::StringCRC("test_stage"); }
        bool IsTransitioning() const override { return false; }
        bool IsShuttingDown()  const override { return false; }
        void GetProcessingUnitIds(Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4>&) const override {}
        void GetModulesInPU(const Dia::Core::StringCRC&,
            Dia::Core::Containers::DynamicArrayC<Dia::DebugServer::DebugModuleInfo, 64>&) const override {}
        Dia::DebugServer::IStreamTapTarget* FindStream(const Dia::Core::StringCRC&) override { return nullptr; }
        Json::Value SerializeStreamPayload(const Dia::Core::StringCRC&, const void*, size_t) override { return Json::Value{}; }
    };

    const char* kTopic = "test.publish.topic";
}

class SLOW_DebugServerPublishTopicTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mServer.SetPort(9461);
        mServer.SetStateProvider(&mProvider);
        mServer.Start();

        mClient.SetConnectionTimeout(5.0f);
        mClient.SetReconnectOnDisconnect(false);
        mClient.SetMessageCallback([this](const Dia::WebSocket::Message& m) { OnMessage(m); });
        ASSERT_TRUE(mClient.Connect("ws://127.0.0.1:9461"));
        Pump();
        ASSERT_TRUE(mClient.IsConnected());
    }

    void TearDown() override
    {
        mClient.Disconnect();
        mServer.Stop();
    }

    void OnMessage(const Dia::WebSocket::Message& m)
    {
        if (m.type != Dia::WebSocket::MessageType::kText) return;
        dia::debug::DebugMessage msg;
        if (!Dia::Proto::FromJson(m.AsText(), &msg)) return;
        if (msg.type() == dia::debug::MESSAGE_TYPE_SUBSCRIBE_ACK)
            ++mAckCount;
        else if (msg.type() == dia::debug::MESSAGE_TYPE_DATA_UPDATE && msg.data_update().data_type() == kTopic)
        {
            Json::Value p = Dia::Proto::ProtoStructToJsonValue(msg.data_update().payload());
            mReceived.push_back(p["v"].asInt());
        }
    }

    void Pump(int iterations = 30)
    {
        for (int i = 0; i < iterations; ++i)
        {
            mServer.Tick(0.016f);
            mClient.Update();
            Dia::Core::ThisThread::SleepMs(10);
        }
    }

    void SendSubscribe(bool subscribe)
    {
        dia::debug::DebugMessage msg;
        msg.set_timestamp(1);
        if (subscribe) { msg.set_type(dia::debug::MESSAGE_TYPE_SUBSCRIBE);   msg.mutable_subscribe()->set_data_type(kTopic); }
        else           { msg.set_type(dia::debug::MESSAGE_TYPE_UNSUBSCRIBE); msg.mutable_unsubscribe()->set_data_type(kTopic); }
        char json[1024];
        ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));
        mClient.SendText(json);
    }

    static Json::Value Make(int v) { Json::Value p; p["v"] = v; return p; }

    PublishTestProvider mProvider;
    Dia::DebugServer::DebugServer mServer;
    Dia::WebSocket::Client mClient;
    std::vector<int> mReceived;
    int mAckCount = 0;
};

TEST_F(SLOW_DebugServerPublishTopicTest, PublishFromOtherThread_TickDeliversToSubscriber)
{
    SendSubscribe(true);
    Pump();
    ASSERT_EQ(mAckCount, 1);
    mReceived.clear();

    std::thread t([&] { mServer.PublishTopic(Dia::Core::StringCRC(kTopic), Make(7)); });
    t.join();
    Pump();

    ASSERT_EQ(mReceived.size(), 1u);
    EXPECT_EQ(mReceived[0], 7);
}

TEST_F(SLOW_DebugServerPublishTopicTest, SubscribeAfterPublish_ReplaysRetainedValue)
{
    mServer.PublishTopic(Dia::Core::StringCRC(kTopic), Make(42));
    Pump();
    EXPECT_TRUE(mReceived.empty());

    SendSubscribe(true);
    Pump();

    ASSERT_EQ(mAckCount, 1);
    ASSERT_EQ(mReceived.size(), 1u);
    EXPECT_EQ(mReceived[0], 42);
}

TEST_F(SLOW_DebugServerPublishTopicTest, PublishedTwiceBeforeDrain_OnlyLatestSent)
{
    SendSubscribe(true);
    Pump();
    mReceived.clear();

    mServer.PublishTopic(Dia::Core::StringCRC(kTopic), Make(1));
    mServer.PublishTopic(Dia::Core::StringCRC(kTopic), Make(2));
    Pump();

    ASSERT_EQ(mReceived.size(), 1u);
    EXPECT_EQ(mReceived[0], 2);
}

TEST_F(SLOW_DebugServerPublishTopicTest, SubscribeBeforeAnyPublish_NoReplay)
{
    SendSubscribe(true);
    Pump();

    EXPECT_EQ(mAckCount, 1);
    EXPECT_TRUE(mReceived.empty());
}

TEST_F(SLOW_DebugServerPublishTopicTest, Unsubscribe_StopsFurtherSends)
{
    SendSubscribe(true);
    Pump();
    mServer.PublishTopic(Dia::Core::StringCRC(kTopic), Make(1));
    Pump();
    ASSERT_EQ(mReceived.size(), 1u);

    SendSubscribe(false);
    Pump();
    mServer.PublishTopic(Dia::Core::StringCRC(kTopic), Make(2));
    Pump();

    EXPECT_EQ(mReceived.size(), 1u);
}

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

// Loopback tests: DebugServer payloads larger than the old 8 KB WebSocket message buffer (AD-001).
namespace
{
    class LargePayloadProvider : public Dia::DebugServer::IDebugStateProvider
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

    const char* kLargeTopic = "test.large.topic";
    const char* kLargeQuery = "test.large.query";

    std::string MakeBlob(size_t size)
    {
        std::string s(size, 'a');
        for (size_t i = 0; i < size; ++i) s[i] = static_cast<char>('a' + (i % 26));
        return s;
    }
}

class SLOW_DebugServerLargePayloadTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mServer.SetPort(9470);
        mServer.SetStateProvider(&mProvider);
        mServer.Start();

        mClient.SetConnectionTimeout(5.0f);
        mClient.SetReconnectOnDisconnect(false);
        mClient.SetMessageCallback([this](const Dia::WebSocket::Message& m) { OnMessage(m); });
        ASSERT_TRUE(mClient.Connect("ws://127.0.0.1:9470"));
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
        {
            ++mAckCount;
        }
        else if (msg.type() == dia::debug::MESSAGE_TYPE_DATA_UPDATE && msg.data_update().data_type() == kLargeTopic)
        {
            Json::Value p = Dia::Proto::ProtoStructToJsonValue(msg.data_update().payload());
            mTopicBlobs.push_back(p["blob"].asString());
        }
        else if (msg.type() == dia::debug::MESSAGE_TYPE_COMMAND_RESPONSE && msg.command_response().command() == kLargeQuery)
        {
            ++mQueryResponses;
            mQuerySuccess = msg.command_response().success();
            Json::Value p = Dia::Proto::ProtoStructToJsonValue(msg.command_response().payload());
            mQueryBlob = p["blob"].asString();
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

    Dia::DebugServer::DebugServer mServer;
    LargePayloadProvider mProvider;
    Dia::WebSocket::Client mClient;
    std::vector<std::string> mTopicBlobs;
    std::string mQueryBlob;
    bool mQuerySuccess = false;
    int mQueryResponses = 0;
    int mAckCount = 0;
};

TEST_F(SLOW_DebugServerLargePayloadTest, PublishTopic_40KbPayload_DeliveredIntact)
{
    dia::debug::DebugMessage sub;
    sub.set_timestamp(1);
    sub.set_type(dia::debug::MESSAGE_TYPE_SUBSCRIBE);
    sub.mutable_subscribe()->set_data_type(kLargeTopic);
    char json[1024];
    ASSERT_TRUE(Dia::Proto::ToJson(sub, json, sizeof(json)));
    mClient.SendText(json);
    Pump();
    ASSERT_EQ(mAckCount, 1);

    const std::string blob = MakeBlob(40 * 1024);
    Json::Value payload;
    payload["blob"] = blob;
    mServer.PublishTopic(Dia::Core::StringCRC(kLargeTopic), payload);
    Pump();

    ASSERT_EQ(mTopicBlobs.size(), 1u);
    EXPECT_EQ(mTopicBlobs[0].size(), blob.size());
    EXPECT_TRUE(mTopicBlobs[0] == blob);
}

TEST_F(SLOW_DebugServerLargePayloadTest, Query_200KbReply_ReachesClientIntact)
{
    const std::string blob = MakeBlob(200 * 1024);
    mServer.GetQueryRegistry().Register(Dia::Core::StringCRC(kLargeQuery),
        [&blob](const Json::Value&) {
            Json::Value r;
            r["blob"] = blob;
            return r;
        });

    dia::debug::DebugMessage req;
    req.set_timestamp(1);
    req.set_type(dia::debug::MESSAGE_TYPE_COMMAND_REQUEST);
    req.mutable_command_request()->set_command(kLargeQuery);
    char json[1024];
    ASSERT_TRUE(Dia::Proto::ToJson(req, json, sizeof(json)));
    mClient.SendText(json);
    Pump(60);

    ASSERT_EQ(mQueryResponses, 1);
    EXPECT_TRUE(mQuerySuccess);
    EXPECT_EQ(mQueryBlob.size(), blob.size());
    EXPECT_TRUE(mQueryBlob == blob);
}

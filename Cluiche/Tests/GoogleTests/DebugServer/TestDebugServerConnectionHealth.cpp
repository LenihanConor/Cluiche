#include <gtest/gtest.h>

#include <DiaDebugServer/DebugServer.h>
#include <DiaDebugServer/IDebugStateProvider.h>
#include <DiaDebugProtocol/DiaDebugProtocol.h>
#include <DiaProtobuf/ProtoStructConverter.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstring>
#include <string>

// ---------------------------------------------------------------------------
// Minimal state provider stub — no streams, no modules.
// ---------------------------------------------------------------------------
namespace
{
    class HealthTestProvider : public Dia::DebugServer::IDebugStateProvider
    {
    public:
        Dia::Core::StringCRC GetCurrentStage() const override
        {
            return Dia::Core::StringCRC("test_stage");
        }

        bool IsTransitioning() const override { return false; }
        bool IsShuttingDown()  const override { return false; }

        void GetProcessingUnitIds(
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4>& /*out*/) const override {}

        void GetModulesInPU(
            const Dia::Core::StringCRC& /*puId*/,
            Dia::Core::Containers::DynamicArrayC<Dia::DebugServer::DebugModuleInfo, 64>& /*out*/) const override {}

        Dia::DebugServer::IStreamTapTarget* FindStream(
            const Dia::Core::StringCRC& /*id*/) override
        {
            return nullptr;
        }

        Json::Value SerializeStreamPayload(
            const Dia::Core::StringCRC& /*dataType*/,
            const void* /*bytes*/,
            size_t /*size*/) override
        {
            return Json::Value{};
        }
    };
}

// ---------------------------------------------------------------------------
// TopicStats: RecordTopicSent — basic tracking
// ---------------------------------------------------------------------------

class DebugServerConnectionHealthTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mServer.EnableAutoStart(false);
        mServer.SetStateProvider(&mProvider);
        mServer.Start();
    }

    void TearDown() override
    {
        mServer.Stop();
    }

    HealthTestProvider mProvider;
    Dia::DebugServer::DebugServer mServer;
};

TEST_F(DebugServerConnectionHealthTest, TopicStats_InitiallyEmpty)
{
    const auto& stats = mServer.GetStats();
    EXPECT_EQ(stats.topicStatCount, 0);
}

TEST_F(DebugServerConnectionHealthTest, NotifySubscribers_NoSubscribers_NoStats)
{
    Json::Value payload;
    payload["test"] = "hello";
    mServer.NotifySubscribers(Dia::Core::StringCRC("some_topic"), payload);

    const auto& stats = mServer.GetStats();
    EXPECT_EQ(stats.topicStatCount, 0);
    EXPECT_EQ(stats.messagesSentTotal, 0);
}

TEST_F(DebugServerConnectionHealthTest, GetServerStats_Query_ReturnsTopicsArray)
{
    auto& registry = mServer.GetQueryRegistry();
    Json::Value result = registry.Execute(Dia::Core::StringCRC("get_server_stats"), Json::Value{});

    ASSERT_TRUE(result.isMember("topics"));
    ASSERT_TRUE(result["topics"].isArray());
    EXPECT_EQ(result["topics"].size(), 0u);
}

TEST_F(DebugServerConnectionHealthTest, GetServerStats_HasExpectedSections)
{
    auto& registry = mServer.GetQueryRegistry();
    Json::Value result = registry.Execute(Dia::Core::StringCRC("get_server_stats"), Json::Value{});

    EXPECT_TRUE(result.isMember("game_side"));
    EXPECT_TRUE(result.isMember("editor_side"));
    EXPECT_TRUE(result.isMember("server"));
    EXPECT_TRUE(result.isMember("topics"));
}

// ---------------------------------------------------------------------------
// SubscribeAck protocol message construction
// ---------------------------------------------------------------------------

TEST(DebugServerProtocolAck, SubscribeAck_Construction_RoundTrips)
{
    dia::debug::DebugMessage msg;
    msg.set_type(dia::debug::MESSAGE_TYPE_SUBSCRIBE_ACK);
    msg.set_timestamp(99999);
    auto* ack = msg.mutable_subscribe_ack();
    ack->set_data_type("observation.log");
    ack->set_success(true);
    ack->set_message("subscribed");

    char json[4096];
    ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));
    ASSERT_GT(strlen(json), 0u);

    dia::debug::DebugMessage parsed;
    ASSERT_TRUE(Dia::Proto::FromJson(json, &parsed));

    EXPECT_EQ(parsed.type(), dia::debug::MESSAGE_TYPE_SUBSCRIBE_ACK);
    EXPECT_EQ(parsed.timestamp(), 99999u);
    EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kSubscribeAck);

    const auto& a = parsed.subscribe_ack();
    EXPECT_EQ(a.data_type(), std::string("observation.log"));
    EXPECT_TRUE(a.success());
    EXPECT_EQ(a.message(), std::string("subscribed"));
}

TEST(DebugServerProtocolAck, SubscribeAck_FailureMessage_RoundTrips)
{
    dia::debug::DebugMessage msg;
    msg.set_type(dia::debug::MESSAGE_TYPE_SUBSCRIBE_ACK);
    msg.set_timestamp(100);
    auto* ack = msg.mutable_subscribe_ack();
    ack->set_data_type("nonexistent.topic");
    ack->set_success(true);
    ack->set_message("no_state_provider_queued");

    char json[4096];
    ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));

    dia::debug::DebugMessage parsed;
    ASSERT_TRUE(Dia::Proto::FromJson(json, &parsed));

    const auto& a = parsed.subscribe_ack();
    EXPECT_STREQ(a.data_type().c_str(), "nonexistent.topic");
    EXPECT_TRUE(a.success());
    EXPECT_STREQ(a.message().c_str(), "no_state_provider_queued");
}

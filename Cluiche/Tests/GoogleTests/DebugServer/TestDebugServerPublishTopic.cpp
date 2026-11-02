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
#ifdef DIA_DEBUG
#include <DiaCellLayers/DebugServerTopicSink.h>
#include <DiaCellLayers/ViewStatePublisher.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/ViewStateTestSupport.h>
#endif

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
        else if (msg.type() == dia::debug::MESSAGE_TYPE_DATA_UPDATE)
        {
            Json::Value p = Dia::Proto::ProtoStructToJsonValue(msg.data_update().payload());
            mAll.push_back({ msg.data_update().data_type(), p });
            if (msg.data_update().data_type() == kTopic)
                mReceived.push_back(p["v"].asInt());
        }
    }

    void SendSubscribeTo(const char* topic, bool subscribe)
    {
        dia::debug::DebugMessage msg;
        msg.set_timestamp(1);
        if (subscribe) { msg.set_type(dia::debug::MESSAGE_TYPE_SUBSCRIBE);   msg.mutable_subscribe()->set_data_type(topic); }
        else           { msg.set_type(dia::debug::MESSAGE_TYPE_UNSUBSCRIBE); msg.mutable_unsubscribe()->set_data_type(topic); }
        char json[1024];
        ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));
        mClient.SendText(json);
    }

    // Payloads received for `topic`, in arrival order.
    std::vector<Json::Value> Received(const char* topic) const
    {
        std::vector<Json::Value> out;
        for (const auto& e : mAll) { if (e.first == topic) out.push_back(e.second); }
        return out;
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
    std::vector<std::pair<std::string, Json::Value>> mAll;
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

// ---- Task 10a exhaustive pass (editor mirror transport) ----

namespace
{
    const char* kTopicB = "test.publish.topic.b";
}

TEST_F(SLOW_DebugServerPublishTopicTest, TwoTopics_DeliverAndReplayIndependently)
{
    mServer.PublishTopic(Dia::Core::StringCRC(kTopic), Make(1));
    mServer.PublishTopic(Dia::Core::StringCRC(kTopicB), Make(2));
    Pump();
    SendSubscribeTo(kTopicB, true);
    Pump();
    EXPECT_TRUE(mReceived.empty()) << "subscribing to B must not replay A";
    ASSERT_EQ(Received(kTopicB).size(), 1u);
    EXPECT_EQ(Received(kTopicB)[0]["v"].asInt(), 2);

    mServer.PublishTopic(Dia::Core::StringCRC(kTopic), Make(3));
    Pump();
    EXPECT_TRUE(mReceived.empty()) << "A has no subscriber on this connection";
    EXPECT_EQ(Received(kTopicB).size(), 1u);
}

TEST_F(SLOW_DebugServerPublishTopicTest, Resubscribe_ReplaysTheLatestRetainedValueAgain)
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
    SendSubscribe(true);
    Pump();
    ASSERT_EQ(mReceived.size(), 2u);
    EXPECT_EQ(mReceived[1], 2) << "a late (re)subscriber gets the current value, not the one it saw last";
}

TEST_F(SLOW_DebugServerPublishTopicTest, ManyThreadsPublishingWhileTicking_NeverReordersPerThreadAndFinalValueWins)
{
    SendSubscribe(true);
    Pump();
    mReceived.clear();

    const int kThreads = 4;
    const int kPerThread = 300;
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([this, t, kPerThread]
        {
            for (int i = 1; i <= kPerThread; ++i)
                mServer.PublishTopic(Dia::Core::StringCRC(kTopic), Make(t * 100000 + i));
        });
    }
    Pump(40);	// ticking on this (Main) thread while the others publish
    for (auto& th : threads) th.join();
    mServer.PublishTopic(Dia::Core::StringCRC(kTopic), Make(999999));
    Pump();

    ASSERT_FALSE(mReceived.empty());
    EXPECT_EQ(mReceived.back(), 999999) << "the last publish after all producers stopped is what subscribers end on";
    int lastSeen[4] = { 0, 0, 0, 0 };
    for (int v : mReceived)
    {
        if (v == 999999) continue;
        const int t = v / 100000;
        const int i = v % 100000;
        ASSERT_GE(t, 0);
        ASSERT_LT(t, kThreads);
        EXPECT_GT(i, lastSeen[t]) << "thread " << t << " values must arrive in publish order (latest wins, never stale)";
        lastSeen[t] = i;
    }
}

TEST_F(SLOW_DebugServerPublishTopicTest, TopicTableFull_DropsNewTopicsButKeepsExistingOnesWorking)
{
    mServer.PublishTopic(Dia::Core::StringCRC(kTopic), Make(1));
    for (int i = 0; i < 31; ++i)
    {
        char name[48];
        snprintf(name, sizeof(name), "test.publish.fill.%d", i);
        mServer.PublishTopic(Dia::Core::StringCRC(name), Make(i));
    }
    // the table (32 entries) is now full
    mServer.PublishTopic(Dia::Core::StringCRC("test.publish.overflow"), Make(7));
    Pump();
    SendSubscribeTo("test.publish.overflow", true);
    Pump();
    EXPECT_TRUE(Received("test.publish.overflow").empty()) << "the dropped topic has nothing retained";

    SendSubscribe(true);
    Pump();
    ASSERT_EQ(mReceived.size(), 1u);
    EXPECT_EQ(mReceived[0], 1);
    mServer.PublishTopic(Dia::Core::StringCRC(kTopic), Make(2));
    Pump();
    ASSERT_EQ(mReceived.size(), 2u);
    EXPECT_EQ(mReceived[1], 2);
}

TEST_F(SLOW_DebugServerPublishTopicTest, ViewStateShapedPayload_RoundTripsNullsEmptyArraysAndHexHash)
{
    Json::Value p(Json::objectValue);
    p["viewVersion"] = 3;
    p["layersVersion"] = 7;
    p["grid"] = Json::Value(Json::nullValue);
    p["base"] = "";
    p["overlays"] = Json::Value(Json::arrayValue);
    Json::Value sel(Json::objectValue);
    sel["count"] = 0;
    sel["version"] = 0;
    sel["hash"] = "0000000000000000";
    p["selection"] = sel;
    p["history"] = Json::Value(Json::nullValue);
    mServer.PublishTopic(Dia::Core::StringCRC("cells.view.state"), p);
    Pump();
    SendSubscribeTo("cells.view.state", true);
    Pump();
    const std::vector<Json::Value> got = Received("cells.view.state");
    ASSERT_EQ(got.size(), 1u);
    const Json::Value& r = got[0];
    EXPECT_EQ(r["viewVersion"].asInt(), 3);
    EXPECT_TRUE(r.isMember("grid"));
    EXPECT_TRUE(r["grid"].isNull());
    EXPECT_TRUE(r.isMember("history"));
    EXPECT_TRUE(r["history"].isNull());
    EXPECT_TRUE(r["overlays"].isArray());
    EXPECT_EQ(r["overlays"].size(), 0u);
    EXPECT_EQ(r["base"].asString(), "");
    EXPECT_EQ(r["selection"]["hash"].asString(), "0000000000000000");
}

#ifdef DIA_DEBUG
TEST_F(SLOW_DebugServerPublishTopicTest, ViewStatePublisherThroughTheSink_LateSubscriberGetsTheCurrentState)
{
    using namespace Dia::CellLayers;
    using namespace Dia::CellLayers::Testing;
    FakePuId() = 1;
    CellLayerRegistry registry(nullptr, &FakePuIdFn);
    Dia::Graphics::CellFieldRegistry cellField;
    LayerUploader uploader(cellField, nullptr);
    CellLayerViewerDomain domain(registry, uploader, nullptr);
    FakeClock clock;
    DebugServerTopicSink sink(mServer);
    ViewStatePublisher publisher(registry, domain, sink, clock);

    EXPECT_EQ(sink.SubscriberCount(ViewStatePublisher::TopicId()), 0u);
    publisher.Update();
    clock.Advance(200);
    TestGrid grid{ 3, 2 };
    StubProvider prov;
    prov.values = std::vector<float>(6, 1.0f);
    ASSERT_EQ(registry.Register(prov.MakeDesc("only", &grid.desc)), CellRegisterResult::Ok);
    FakeDebugDraw draw;
    domain.Update(draw);
    publisher.Update();
    ASSERT_EQ(publisher.PublishCount(), 2u);
    Pump();	// nobody subscribed yet
    EXPECT_TRUE(Received("cells.view.state").empty());

    SendSubscribeTo("cells.view.state", true);
    Pump();
    EXPECT_EQ(sink.SubscriberCount(ViewStatePublisher::TopicId()), 1u);
    const std::vector<Json::Value> got = Received("cells.view.state");
    ASSERT_EQ(got.size(), 1u) << "exactly the current state, not the history";
    EXPECT_EQ(got[0]["viewVersion"].asInt(), 2);
    EXPECT_EQ(got[0]["base"].asString(), "only");
    EXPECT_EQ(got[0]["grid"]["width"].asInt(), 3);

    clock.Advance(200);
    domain.SetBase(Dia::Core::StringCRC("only"));
    CellSelection* selection = domain.ActiveSelection();
    ASSERT_NE(selection, nullptr);
    selection->BeginEdit(); selection->Set(4, true); selection->EndEdit();
    publisher.Update();
    Pump();
    ASSERT_EQ(Received("cells.view.state").size(), 2u);
    EXPECT_EQ(Received("cells.view.state")[1]["selection"]["count"].asInt(), 1);
}
#endif

// A topic payload whose message does not fit the 64 KB send buffer is dropped for every subscriber (a Warning on DebugServer); the topic
// stays usable and the next normal payload is delivered. The retained oversize value is not replayed to a late subscriber either.
TEST_F(SLOW_DebugServerPublishTopicTest, OversizePayload_IsDroppedNotDeliveredAndTheTopicStaysUsable)
{
    SendSubscribeTo("test.oversize.topic", true);
    Pump();
    Json::Value big;
    big["blob"] = std::string(70 * 1024, 'x');
    mServer.PublishTopic(Dia::Core::StringCRC("test.oversize.topic"), big);
    Pump();
    EXPECT_TRUE(Received("test.oversize.topic").empty()) << "over the send buffer: dropped";

    mServer.PublishTopic(Dia::Core::StringCRC("test.oversize.topic"), Make(5));
    Pump();
    const std::vector<Json::Value> got = Received("test.oversize.topic");
    ASSERT_EQ(got.size(), 1u);
    EXPECT_EQ(got[0]["v"].asInt(), 5);
}

TEST_F(SLOW_DebugServerPublishTopicTest, OversizeRetainedPayload_IsNotReplayedToALateSubscriber)
{
    Json::Value big;
    big["blob"] = std::string(70 * 1024, 'y');
    mServer.PublishTopic(Dia::Core::StringCRC("test.oversize.late"), big);
    Pump();
    SendSubscribeTo("test.oversize.late", true);
    Pump();
    EXPECT_TRUE(Received("test.oversize.late").empty());
    EXPECT_EQ(mServer.GetTopicSubscriberCount(Dia::Core::StringCRC("test.oversize.late")), 1u) << "still subscribed, just nothing to replay";
}

TEST_F(SLOW_DebugServerPublishTopicTest, PayloadJustUnderTheBuffer_IsDelivered)
{
    SendSubscribeTo("test.nearcap.topic", true);
    Pump();
    Json::Value p;
    p["blob"] = std::string(60 * 1024, 'z');
    mServer.PublishTopic(Dia::Core::StringCRC("test.nearcap.topic"), p);
    Pump();
    const std::vector<Json::Value> got = Received("test.nearcap.topic");
    ASSERT_EQ(got.size(), 1u);
    EXPECT_EQ(got[0]["blob"].asString().size(), 60u * 1024u);
}

// Publishing and counting on a server that was never started must be harmless (the viewer may publish before the server is up).
TEST(DebugServerPublishTopicUnstarted, PublishAndCountNeverCrashAndCountIsZero)
{
    Dia::DebugServer::DebugServer server;
    Json::Value p;
    p["v"] = 1;
    server.PublishTopic(Dia::Core::StringCRC("cells.view.state"), p);
    server.PublishTopic(Dia::Core::StringCRC("cells.view.state"), p);
    EXPECT_EQ(server.GetTopicSubscriberCount(Dia::Core::StringCRC("cells.view.state")), 0u);
    EXPECT_EQ(server.GetTopicSubscriberCount(Dia::Core::StringCRC("never.published")), 0u);
}

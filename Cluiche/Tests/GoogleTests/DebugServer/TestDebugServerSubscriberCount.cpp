#include <gtest/gtest.h>

#include <DiaDebugServer/DebugServer.h>
#include <DiaDebugServer/IDebugStateProvider.h>
#include <DiaDebugProtocol/DiaDebugProtocol.h>
#include <DiaProtobuf/ProtoJsonCodec.h>
#include <DiaWebSocket/Client.h>
#include <DiaCore/Threading/Thread.h>
#include <DiaCore/CRC/StringCRC.h>

// Loopback tests for DebugServer::GetTopicSubscriberCount: real DebugServer + real WebSocket clients.
namespace
{
    class CountTestProvider : public Dia::DebugServer::IDebugStateProvider
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

    const char* kTopic = "test.count.topic";
    const char* kOtherTopic = "test.count.other";
}

class SLOW_DebugServerSubscriberCountTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mServer.SetPort(9462);
        mServer.SetStateProvider(&mProvider);
        mServer.Start();
        Connect(mClientA);
        Connect(mClientB);
    }

    void TearDown() override
    {
        mClientA.Disconnect();
        mClientB.Disconnect();
        mServer.Stop();
    }

    void Connect(Dia::WebSocket::Client& c)
    {
        c.SetConnectionTimeout(5.0f);
        c.SetReconnectOnDisconnect(false);
        ASSERT_TRUE(c.Connect("ws://127.0.0.1:9462"));
        Pump();
        ASSERT_TRUE(c.IsConnected());
    }

    void Pump(int iterations = 30)
    {
        for (int i = 0; i < iterations; ++i)
        {
            mServer.Tick(0.016f);
            mClientA.Update();
            mClientB.Update();
            Dia::Core::ThisThread::SleepMs(10);
        }
    }

    void Send(Dia::WebSocket::Client& c, bool subscribe, const char* topic = kTopic)
    {
        dia::debug::DebugMessage msg;
        msg.set_timestamp(1);
        if (subscribe) { msg.set_type(dia::debug::MESSAGE_TYPE_SUBSCRIBE);   msg.mutable_subscribe()->set_data_type(topic); }
        else           { msg.set_type(dia::debug::MESSAGE_TYPE_UNSUBSCRIBE); msg.mutable_unsubscribe()->set_data_type(topic); }
        char json[1024];
        ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));
        c.SendText(json);
    }

    unsigned int Count(const char* topic = kTopic) const { return mServer.GetTopicSubscriberCount(Dia::Core::StringCRC(topic)); }

    CountTestProvider mProvider;
    Dia::DebugServer::DebugServer mServer;
    Dia::WebSocket::Client mClientA;
    Dia::WebSocket::Client mClientB;
};

TEST_F(SLOW_DebugServerSubscriberCountTest, UnknownTopic_IsZero)
{
    EXPECT_EQ(Count("never.subscribed"), 0u);
    EXPECT_EQ(Count(), 0u);
}

TEST_F(SLOW_DebugServerSubscriberCountTest, AfterSubscribe_IsOne)
{
    Send(mClientA, true);
    Pump();
    EXPECT_EQ(Count(), 1u);
    EXPECT_EQ(Count(kOtherTopic), 0u);
}

TEST_F(SLOW_DebugServerSubscriberCountTest, SecondSubscriber_IsTwo)
{
    Send(mClientA, true);
    Send(mClientB, true);
    Pump();
    EXPECT_EQ(Count(), 2u);
}

TEST_F(SLOW_DebugServerSubscriberCountTest, DuplicateSubscribeFromOneConnection_CountsOnce)
{
    Send(mClientA, true);
    Send(mClientA, true);
    Pump();
    EXPECT_EQ(Count(), 1u);

    Send(mClientA, false);
    Pump();
    EXPECT_EQ(Count(), 0u);
}

TEST_F(SLOW_DebugServerSubscriberCountTest, AfterUnsubscribe_Decrements)
{
    Send(mClientA, true);
    Send(mClientB, true);
    Pump();
    ASSERT_EQ(Count(), 2u);

    Send(mClientA, false);
    Pump();
    EXPECT_EQ(Count(), 1u);
    Send(mClientB, false);
    Pump();
    EXPECT_EQ(Count(), 0u);
}

TEST_F(SLOW_DebugServerSubscriberCountTest, ClientDisconnect_Decrements)
{
    Send(mClientA, true);
    Send(mClientB, true);
    Pump();
    ASSERT_EQ(Count(), 2u);

    mClientA.Disconnect();
    Pump();
    EXPECT_EQ(Count(), 1u);
}

TEST_F(SLOW_DebugServerSubscriberCountTest, TopicsCountedIndependently)
{
    Send(mClientA, true, kTopic);
    Send(mClientA, true, kOtherTopic);
    Send(mClientB, true, kOtherTopic);
    Pump();
    EXPECT_EQ(Count(kTopic), 1u);
    EXPECT_EQ(Count(kOtherTopic), 2u);
}

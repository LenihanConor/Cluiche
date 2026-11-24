////////////////////////////////////////////////////////////////////////////////
// Filename: TestPerfTimingPublisher.cpp
// GoogleTest suite - Perf Inspector task 3: perf.timing push (10 Hz, subscriber gated).
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <Types/PerfTimingPublisher.h>

#include <DiaObservation/Profile/CoarseChunk.h>
#include <DiaObservation/Profile/CoarseChunkAssembler.h>
#include <DiaObservation/Profile/CoarseChunkBuilder.h>

#include <algorithm>
#include <string>
#include <vector>

using namespace Dia::Observation::Profile;
using Cluiche::AppFlow::IPerfTimingSink;
using Cluiche::AppFlow::PerfTimingPublisher;

namespace
{
    struct FakeSink : IPerfTimingSink
    {
        uint32_t subscribers = 0;
        bool     selfAvailable = true;
        double   selfMs = 0.25;
        std::vector<Json::Value> published;

        uint32_t SubscriberCount() const override { return subscribers; }
        void Publish(const Json::Value& p) override { published.push_back(p); }
        bool TryGetSelfMs(double& out) const override { if (!selfAvailable) return false; out = selfMs; return true; }
    };

    // Feeds one chunk of n entries: module slot s = (s+1) ms, unit tick = 20 ms.
    void Feed(CoarseChunkAssembler& a, uint32_t unitCrc, uint32_t modules, uint32_t n, float hz = 60.0f)
    {
        CoarseChunkBuilder b;
        b.Begin(unitCrc, hz);
        for (uint32_t i = 0; i < n; ++i)
        {
            for (uint32_t m = 0; m < modules; ++m)
                b.RecordModule(m, 0x1000u + m, static_cast<float>(m + 1));
            b.RecordUnitTick(20.0f);
            b.CommitEntry(0.0);
            if ((i + 1) % CoarseChunk::kMaxEntries == 0 || i + 1 == n)
                a.Apply(b.Take(1000.0));   // a chunk holds at most kMaxEntries entries
        }
    }

    struct PerfTimingRig : ::testing::Test
    {
        CoarseChunkAssembler assembler;
        FakeSink sink;
        PerfTimingPublisher pub{ assembler, nullptr, sink };
    };

    std::vector<std::string> Unavailable(const Json::Value& p)
    {
        std::vector<std::string> out;
        for (const auto& v : p["unavailable"]) out.push_back(v.asString());
        return out;
    }
}

TEST_F(PerfTimingRig, NoSubscriber_PublishesNothing)
{
    Feed(assembler, 0x11u, 2, 30);
    sink.subscribers = 0;
    for (int i = 0; i < 50; ++i) pub.Tick(i * 0.05);
    EXPECT_EQ(0u, sink.published.size());
    EXPECT_EQ(0u, pub.GetPublishCount());
}

TEST_F(PerfTimingRig, Subscribed_PublishesImmediatelyThenAtTenHz)
{
    Feed(assembler, 0x11u, 2, 30);
    sink.subscribers = 1;
    pub.Tick(0.0);     // first subscribed tick publishes
    pub.Tick(0.03);
    pub.Tick(0.06);
    pub.Tick(0.099);   // still inside the period
    EXPECT_EQ(1u, sink.published.size());
    pub.Tick(0.10);    // period elapsed
    EXPECT_EQ(2u, sink.published.size());
    pub.Tick(0.15);
    EXPECT_EQ(2u, sink.published.size());
    pub.Tick(0.20);
    EXPECT_EQ(3u, sink.published.size());
}

TEST_F(PerfTimingRig, SixtyHzTicksForOneSecond_PublishAboutTenTimes)
{
    Feed(assembler, 0x11u, 2, 30);
    sink.subscribers = 1;
    for (int i = 0; i < 60; ++i) pub.Tick(i / 60.0);
    EXPECT_GE(sink.published.size(), 9u);
    EXPECT_LE(sink.published.size(), 10u);
}

TEST_F(PerfTimingRig, SubscriberGoneThenBack_ResumesWithoutBurst)
{
    Feed(assembler, 0x11u, 2, 30);
    sink.subscribers = 1;
    pub.Tick(0.0);
    sink.subscribers = 0;
    pub.Tick(5.0);
    EXPECT_EQ(1u, sink.published.size());
    sink.subscribers = 1;
    pub.Tick(5.01);    // long since the last publish: publishes once
    pub.Tick(5.02);    // and is then throttled again
    EXPECT_EQ(2u, sink.published.size());
}

TEST_F(PerfTimingRig, PayloadCarriesPerUnitAndPerModuleStats)
{
    Feed(assembler, 0x11u, 2, 120);
    sink.subscribers = 1;
    pub.Tick(0.0);
    ASSERT_EQ(1u, sink.published.size());
    const Json::Value& p = sink.published[0];
    ASSERT_EQ(1u, p["units"].size());
    const Json::Value& u = p["units"][0];
    EXPECT_EQ("0x00000011", u["unit"].asString());
    EXPECT_EQ(120u, u["samples"].asUInt());
    ASSERT_EQ(2u, u["tick"].size());
    EXPECT_EQ(2000, u["tick"][0].asInt());   // 20 ms in 0.01 ms steps (ewma of constant)
    EXPECT_EQ(2000, u["tick"][1].asInt());
    ASSERT_EQ(2u, u["modules"].size());
    EXPECT_EQ("0x00001000", u["modules"][0].asString());
    EXPECT_EQ(100, u["ewma"][0].asInt());
    EXPECT_EQ(200, u["p95"][1].asInt());
    EXPECT_NEAR(0.25, p["selfMs"].asDouble(), 1e-9);
}

TEST_F(PerfTimingRig, UnavailableListNamesEverythingWithoutAMainPath)
{
    Feed(assembler, 0x11u, 1, 10);
    sink.subscribers = 1;
    pub.Tick(0.0);
    const Json::Value& p = sink.published[0];
    const std::vector<std::string> un = Unavailable(p);
    EXPECT_EQ((std::vector<std::string>{ "latency", "trips", "marker_drops", "suppressed_trips", "suppressed_logs" }), un);
    EXPECT_FALSE(p.isMember("latency"));
    EXPECT_FALSE(p.isMember("markerDrops"));
    EXPECT_TRUE(p.isMember("selfMs"));
}

TEST_F(PerfTimingRig, SelfMsMissing_IsOmittedAndListed)
{
    Feed(assembler, 0x11u, 1, 10);
    sink.subscribers = 1;
    sink.selfAvailable = false;
    pub.Tick(0.0);
    const Json::Value& p = sink.published[0];
    EXPECT_FALSE(p.isMember("selfMs"));
    const std::vector<std::string> un = Unavailable(p);
    EXPECT_NE(un.end(), std::find(un.begin(), un.end(), "self_ms"));
}

TEST_F(PerfTimingRig, NoUnits_PublishesEmptyUnitList)
{
    sink.subscribers = 1;
    pub.Tick(0.0);
    ASSERT_EQ(1u, sink.published.size());
    EXPECT_TRUE(sink.published[0]["units"].isArray());
    EXPECT_EQ(0u, sink.published[0]["units"].size());
}

TEST_F(PerfTimingRig, ThreeUnitsThirteenModules_PayloadUnder50KB)
{
    for (uint32_t u = 0; u < 3; ++u)
        Feed(assembler, 0x11u + u, 13, 60);
    sink.subscribers = 1;
    pub.Tick(0.0);
    const std::string text = Json::FastWriter().write(sink.published[0]);
    EXPECT_LT(text.size(), 50u * 1024u) << "size " << text.size();
    EXPECT_EQ(3u, sink.published[0]["units"].size());
    EXPECT_EQ(13u, sink.published[0]["units"][0]["modules"].size());
}

TEST_F(PerfTimingRig, WorstCase_EightUnitsSixtyFourModules_PayloadUnder50KB)
{
    for (uint32_t u = 0; u < 8; ++u)
        Feed(assembler, 0x11u + u, 64, 30);
    sink.subscribers = 1;
    pub.Tick(0.0);
    const std::string text = Json::FastWriter().write(sink.published[0]);
    EXPECT_LT(text.size(), 50u * 1024u) << "size " << text.size();
}

////////////////////////////////////////////////////////////////////////////////
// Filename: TestPerfTimingJobs.cpp
// GoogleTest suite - Perf Inspector task 13 part B: tier 1 "jobs" object on perf.timing.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <Types/PerfTimingPublisher.h>

#include <DiaObservation/Profile/CoarseChunkAssembler.h>
#include <DiaObservation/Profile/JobChunk.h>
#include <DiaObservation/Profile/JobChunkAssembler.h>

#include <algorithm>
#include <cstring>
#include <memory>
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
        std::vector<Json::Value> published;
        uint32_t SubscriberCount() const override { return subscribers; }
        void Publish(const Json::Value& p) override { published.push_back(p); }
        bool TryGetSelfMs(double& out) const override { out = 0.25; return true; }
    };

    void ApplyChunk(JobChunkAssembler& a, uint32_t unitCrc, uint32_t owners)
    {
        auto c = std::make_unique<JobChunk>();
        std::memset(c.get(), 0, sizeof(JobChunk));
        c->unitCrc = unitCrc;
        c->flags = JobChunk::kFlagPerJob;
        c->ownerCount = owners;
        for (uint32_t o = 0; o < owners; ++o)
        {
            c->owners[o].owner = o + 1 == owners && owners == JobChunk::kMaxOwners ? 0u : 0x500u + o;
            c->owners[o].count = 10 + o;
            c->owners[o].busyNs = 3000000ull * (o + 1);
            c->owners[o].waitP50Ms = 0.5f;
            c->owners[o].waitP95Ms = 1.5f;
            c->owners[o].worstWaitMs = 4.0f;
        }
        c->pool.inlineFallbacks = 3;
        c->pool.inboxDrops = 7;
        a.Apply(*c);
    }

    std::vector<std::string> Unavailable(const Json::Value& p)
    {
        std::vector<std::string> out;
        for (const auto& v : p["unavailable"]) out.push_back(v.asString());
        return out;
    }

    struct PerfTimingJobsRig : ::testing::Test
    {
        CoarseChunkAssembler assembler;
        JobChunkAssembler    jobs;
        FakeSink sink;
        PerfTimingPublisher pub{ assembler, nullptr, sink, &jobs };
    };
}

TEST_F(PerfTimingJobsRig, JobsObjectCarriesOwnerAndPoolTier1)
{
    ApplyChunk(jobs, 0x11u, 2);
    sink.subscribers = 1;
    pub.Tick(0.0);
    ASSERT_EQ(1u, sink.published.size());
    const Json::Value& p = sink.published[0];
    ASSERT_TRUE(p.isMember("jobs"));
    ASSERT_EQ(1u, p["jobs"]["units"].size());
    const Json::Value& u = p["jobs"]["units"][0];
    EXPECT_EQ("0x00000011", u["unit"].asString());
    ASSERT_EQ(2u, u["owners"].size());
    EXPECT_EQ("0x00000500", u["owners"][0]["name"].asString());
    EXPECT_EQ(0x500u, u["owners"][0]["crc"].asUInt());
    EXPECT_EQ(10u, u["owners"][0]["count"].asUInt());
    EXPECT_NEAR(3.0, u["owners"][0]["busyMs"].asDouble(), 1e-6);
    EXPECT_NEAR(6.0, u["owners"][1]["busyMs"].asDouble(), 1e-6);
    EXPECT_NEAR(0.5, u["owners"][0]["waitP50Ms"].asDouble(), 1e-6);
    EXPECT_NEAR(1.5, u["owners"][0]["waitP95Ms"].asDouble(), 1e-6);
    EXPECT_EQ(3u, u["pool"]["inlineFallbacks"].asUInt());
    EXPECT_EQ(7u, u["pool"]["inboxDrops"].asUInt());
    EXPECT_TRUE(u["pool"].isMember("capUse"));
    EXPECT_TRUE(u["pool"].isMember("timeAtCapMs"));
    EXPECT_TRUE(u["pool"].isMember("peakQueueDepth"));
}

TEST_F(PerfTimingJobsRig, UnavailableNamesPoolCountersNothingSamples)
{
    ApplyChunk(jobs, 0x11u, 1);
    sink.subscribers = 1;
    pub.Tick(0.0);
    const std::vector<std::string> un = Unavailable(sink.published[0]);
    for (const char* name : { "cap_use", "peak_queue_depth", "time_at_cap", "latency", "trips" })
        EXPECT_NE(un.end(), std::find(un.begin(), un.end(), name)) << name;
}

TEST_F(PerfTimingJobsRig, OverflowOwnerIsNamed)
{
    ApplyChunk(jobs, 0x11u, JobChunk::kMaxOwners);
    sink.subscribers = 1;
    pub.Tick(0.0);
    const Json::Value& owners = sink.published[0]["jobs"]["units"][0]["owners"];
    ASSERT_EQ(JobChunk::kMaxOwners, owners.size());
    EXPECT_EQ("(other)", owners[JobChunk::kMaxOwners - 1]["name"].asString());
}

TEST_F(PerfTimingJobsRig, NoUnits_EmptyJobsUnitsList)
{
    sink.subscribers = 1;
    pub.Tick(0.0);
    ASSERT_TRUE(sink.published[0]["jobs"]["units"].isArray());
    EXPECT_EQ(0u, sink.published[0]["jobs"]["units"].size());
}

TEST_F(PerfTimingJobsRig, NoSubscriber_NothingBuilt)
{
    ApplyChunk(jobs, 0x11u, 2);
    pub.Tick(0.0);
    EXPECT_EQ(0u, sink.published.size());
}

TEST(PerfTimingJobs, WithoutJobAssemblerPayloadIsUnchanged)
{
    CoarseChunkAssembler assembler;
    FakeSink sink;
    PerfTimingPublisher pub(assembler, nullptr, sink);
    sink.subscribers = 1;
    pub.Tick(0.0);
    EXPECT_FALSE(sink.published[0].isMember("jobs"));
    const std::vector<std::string> un = Unavailable(sink.published[0]);
    EXPECT_EQ(un.end(), std::find(un.begin(), un.end(), "cap_use"));
}

TEST_F(PerfTimingJobsRig, EightUnitsSixteenOwners_PayloadUnder50KB)
{
    for (uint32_t u = 0; u < JobChunkAssembler::kMaxUnits; ++u)
        ApplyChunk(jobs, 0x11u + u, JobChunk::kMaxOwners);
    sink.subscribers = 1;
    pub.Tick(0.0);
    const std::string text = Json::FastWriter().write(sink.published[0]);
    EXPECT_LT(text.size(), 50u * 1024u) << "size " << text.size();
    EXPECT_EQ(8u, sink.published[0]["jobs"]["units"].size());
}

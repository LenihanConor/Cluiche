////////////////////////////////////////////////////////////////////////////////
// Filename: TestPerfEdgeCommands.cpp
// GoogleTest suite - Perf Inspector task 10: edge tests for perf.window / perf.frame / perf.jobs argument
// handling (garbage, NaN, negative, huge) and multi-unit behaviour of PerfWindowCommands.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <Types/PerfWindowCommands.h>

#include <DiaObservation/Profile/CoarseChunk.h>
#include <DiaObservation/Profile/CoarseChunkBuilder.h>
#include <DiaObservation/Profile/JobChunk.h>
#include <DiaStreams/EventStreamStore.h>

#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <string>

using namespace Dia::Observation::Profile;
using Cluiche::AppFlow::PerfWindowCommands;
using Dia::ApplicationFlow::Event;
using Dia::ApplicationFlow::EventStreamStore;
using Dia::Core::StringCRC;

namespace
{
    struct PerfWindowEdgeRig : ::testing::Test
    {
        EventStreamStore<CoarseChunk> coarse{ StringCRC("test_perfedge_coarse"), StringCRC("CoarseChunk"), 32, 1 };
        EventStreamStore<JobChunk>    jobs{ StringCRC("test_perfedge_jobs"), StringCRC("JobChunk"), 16, 1 };
        PerfWindowCommands cmds{ &coarse, nullptr, &jobs };

        void SendCoarse(uint32_t unitCrc, float hz, uint32_t n, uint32_t seq = 0, uint64_t first = 0)
        {
            CoarseChunkBuilder b;
            b.Begin(unitCrc, hz);
            for (uint32_t i = 0; i < n; ++i)
            {
                b.RecordModule(0, 0xAu, 1.0f);
                b.RecordUnitTick(2.0f);
                b.CommitEntry(0.0);
            }
            CoarseChunk c = b.Take(1000.0);
            c.seq = seq;
            c.firstEntry = first;
            Event<CoarseChunk> ev;
            ev.payload = c;
            coarse.Send(ev);
        }

        void SendJobs(uint32_t unitCrc)
        {
            auto c = std::make_unique<JobChunk>();
            std::memset(c.get(), 0, sizeof(JobChunk));
            c->unitCrc = unitCrc;
            c->flags = JobChunk::kFlagPerJob;
            c->rawCount = 3;
            for (uint32_t i = 0; i < 3; ++i) { c->raw[i].startNs = 1000 + i * 100; c->raw[i].durNs = 50; c->raw[i].owner = 7; }
            Event<JobChunk> ev;
            ev.payload = *c;
            jobs.Send(ev);
        }

        static Json::Value JobArgs(const char* unit, Json::Value from, Json::Value to)
        {
            Json::Value a(Json::objectValue);
            a["unit"] = unit;
            a["fromNs"] = from;
            a["toNs"] = to;
            return a;
        }
    };
}

TEST_F(PerfWindowEdgeRig, WindowSecondsGarbageNeverFailsAndClamps)
{
    SendCoarse(0x11u, 30.0f, 64);
    cmds.Drain();
    const Json::Value cases[] = {
        Json::Value(std::numeric_limits<double>::quiet_NaN()),
        Json::Value(-5.0),
        Json::Value(1e300),
        Json::Value(0),
        Json::Value("abc"),
        Json::Value(Json::nullValue),
        Json::Value(Json::arrayValue),
    };
    for (const Json::Value& s : cases)
    {
        Json::Value args(Json::objectValue);
        args["seconds"] = s;
        const Json::Value r = cmds.HandleWindow(args);
        EXPECT_FALSE(r.isMember("error")) << s.toStyledString();
        ASSERT_TRUE(r["units"].isArray());
        EXPECT_EQ(1u, r["units"].size());
        EXPECT_GE(r["seconds"].asInt(), 1);
        EXPECT_LE(r["seconds"].asInt(), 60);
    }
}

TEST_F(PerfWindowEdgeRig, NonObjectArgsAreHandled)
{
    SendCoarse(0x11u, 30.0f, 10);
    cmds.Drain();
    EXPECT_FALSE(cmds.HandleWindow(Json::Value(Json::nullValue)).isMember("error"));
    EXPECT_FALSE(cmds.HandleWindow(Json::Value(Json::arrayValue)).isMember("error"));
    EXPECT_FALSE(cmds.HandleWindow(Json::Value("x")).isMember("error"));
    EXPECT_TRUE(cmds.HandleFrame(Json::Value(Json::nullValue)).isMember("error"));
    EXPECT_TRUE(cmds.HandleFrame(Json::Value(5)).isMember("error"));
    EXPECT_TRUE(cmds.HandleJobs(Json::Value(Json::nullValue)).isMember("error"));
    EXPECT_TRUE(cmds.HandleJobs(Json::Value(Json::arrayValue)).isMember("error"));
}

TEST_F(PerfWindowEdgeRig, FrameGarbageArgsAreErrorsNotExceptions)
{
    SendCoarse(0x11u, 30.0f, 10);
    cmds.Drain();
    Json::Value a(Json::objectValue);
    a["unit"] = "0x00000011";
    a["frame"] = "3";                                   // wrong type
    EXPECT_TRUE(cmds.HandleFrame(a).isMember("error"));
    a["frame"] = -1;
    EXPECT_TRUE(cmds.HandleFrame(a).isMember("error"));
    a["frame"] = 2.5;                                   // not integral
    EXPECT_TRUE(cmds.HandleFrame(a).isMember("error"));
    a["frame"] = Json::Value(std::numeric_limits<double>::quiet_NaN());
    EXPECT_TRUE(cmds.HandleFrame(a).isMember("error"));
    a["frame"] = Json::Value(static_cast<Json::UInt64>(std::numeric_limits<uint64_t>::max()));   // beyond int64
    Json::Value r;
    EXPECT_NO_THROW(r = cmds.HandleFrame(a));
    EXPECT_TRUE(r.isMember("error"));
    a["frame"] = 3;
    a["unit"] = 17;                                     // number, not a string
    EXPECT_TRUE(cmds.HandleFrame(a).isMember("error"));
}

TEST_F(PerfWindowEdgeRig, JobsRangeEqualHugeNegativeAndGarbage)
{
    SendJobs(0x11u);
    cmds.Drain();
    // equal from/to inside a record
    Json::Value r = cmds.HandleJobs(JobArgs("0x00000011", 1120, 1120));
    EXPECT_FALSE(r.isMember("error"));
    EXPECT_EQ(1u, r["count"].asUInt());
    // from == to == 0 : nothing overlaps
    r = cmds.HandleJobs(JobArgs("0x00000011", 0, 0));
    EXPECT_FALSE(r.isMember("error"));
    EXPECT_EQ(0u, r["count"].asUInt());
    // negative
    EXPECT_TRUE(cmds.HandleJobs(JobArgs("0x00000011", -1, 5)).isMember("error"));
    EXPECT_TRUE(cmds.HandleJobs(JobArgs("0x00000011", 0, -5)).isMember("error"));
    // wrong types
    EXPECT_TRUE(cmds.HandleJobs(JobArgs("0x00000011", "a", 5)).isMember("error"));
    EXPECT_TRUE(cmds.HandleJobs(JobArgs("0x00000011", 0.5, 5)).isMember("error"));
    EXPECT_TRUE(cmds.HandleJobs(JobArgs("0x00000011", Json::Value(std::numeric_limits<double>::quiet_NaN()), 5)).isMember("error"));
    // huge, within int64
    r = cmds.HandleJobs(JobArgs("0x00000011", 0, Json::Value(static_cast<Json::Int64>(std::numeric_limits<int64_t>::max()))));
    EXPECT_FALSE(r.isMember("error"));
    EXPECT_EQ(3u, r["count"].asUInt());
    // huge, beyond int64 (uint64 max): must be a clean error or a clean reply, never an exception
    EXPECT_NO_THROW(r = cmds.HandleJobs(JobArgs("0x00000011", 0, Json::Value(static_cast<Json::UInt64>(std::numeric_limits<uint64_t>::max())))));
    EXPECT_NO_THROW(r = cmds.HandleJobs(JobArgs("0x00000011", Json::Value(static_cast<Json::UInt64>(std::numeric_limits<uint64_t>::max())), Json::Value(static_cast<Json::UInt64>(std::numeric_limits<uint64_t>::max())))));
}

TEST_F(PerfWindowEdgeRig, TwelveUnitsKeepFirstEightAndCountDrops)
{
    for (uint32_t u = 0; u < 12; ++u)
        SendCoarse(0x100u + u, 30.0f, 3);
    cmds.Drain();
    EXPECT_EQ(8u, cmds.GetAssembler().GetUnitCount());
    EXPECT_EQ(4u, cmds.GetAssembler().GetDroppedUnknownUnitCount());
    const Json::Value r = cmds.HandleWindow(Json::Value(Json::objectValue));
    EXPECT_EQ(8u, r["units"].size());
    for (const Json::Value& u : r["units"])
        EXPECT_EQ(3u, u["count"].asUInt());
}

TEST_F(PerfWindowEdgeRig, UnitHzZeroAndNegativeStillPackValidWindows)
{
    SendCoarse(0x21u, 0.0f, 5);
    SendCoarse(0x22u, -30.0f, 5);
    SendCoarse(0x23u, std::numeric_limits<float>::quiet_NaN(), 5);
    cmds.Drain();
    const Json::Value r = cmds.HandleWindow(Json::Value(Json::objectValue));
    EXPECT_FALSE(r.isMember("error")) << r.toStyledString();
    ASSERT_EQ(3u, r["units"].size());
    for (const Json::Value& u : r["units"])
    {
        EXPECT_EQ(60.0, u["hz"].asDouble());
        EXPECT_EQ(5u, u["count"].asUInt());
    }
}

TEST_F(PerfWindowEdgeRig, WindowIsStableWhenNothingChangesBetweenPolls)
{
    SendCoarse(0x11u, 30.0f, 10, 0, 0);
    cmds.Drain();
    const Json::Value a = cmds.HandleWindow(Json::Value(Json::objectValue));
    const Json::Value b = cmds.HandleWindow(Json::Value(Json::objectValue));
    EXPECT_EQ(a, b);
    SendCoarse(0x11u, 30.0f, 10, 1, 10);
    cmds.Drain();
    const Json::Value c = cmds.HandleWindow(Json::Value(Json::objectValue));
    EXPECT_EQ(20u, c["units"][0]["count"].asUInt());
    EXPECT_EQ(0u, c["units"][0]["firstEntry"].asUInt());
}

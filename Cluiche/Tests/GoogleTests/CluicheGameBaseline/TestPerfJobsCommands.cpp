////////////////////////////////////////////////////////////////////////////////
// Filename: TestPerfJobsCommands.cpp
// GoogleTest suite - Perf Inspector task 13 part B: perf.jobs, the "jobs" section of perf.window and the
// "$perfjobs" drain in PerfWindowCommands (Main-hosted).
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <Types/PerfWindowCommands.h>

#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaObservation/Profile/CoarseChunk.h>
#include <DiaObservation/Profile/JobChunk.h>
#include <DiaObservation/Profile/JobChunkBuilder.h>
#include <DiaObservation/Profile/JobTimingTable.h>
#include <DiaStreams/EventStreamStore.h>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace Dia::Observation::Profile;
using Cluiche::AppFlow::PerfWindowCommands;
using Dia::ApplicationFlow::Event;
using Dia::ApplicationFlow::EventStreamStore;
using Dia::Core::StringCRC;

namespace
{
    std::vector<uint8_t> DecodeBase64(const std::string& in)
    {
        std::vector<uint8_t> out;
        uint32_t acc = 0;
        int bits = 0;
        for (char c : in)
        {
            int v;
            if (c >= 'A' && c <= 'Z') v = c - 'A';
            else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
            else if (c >= '0' && c <= '9') v = c - '0' + 52;
            else if (c == '+') v = 62;
            else if (c == '/') v = 63;
            else break;
            acc = (acc << 6) | static_cast<uint32_t>(v);
            bits += 6;
            if (bits >= 8) { bits -= 8; out.push_back(static_cast<uint8_t>((acc >> bits) & 0xFF)); }
        }
        return out;
    }

    template <typename T>
    std::vector<T> Decode(const std::string& in)
    {
        const std::vector<uint8_t> raw = DecodeBase64(in);
        std::vector<T> out(raw.size() / sizeof(T));
        for (size_t i = 0; i < out.size(); ++i)
        {
            uint64_t v = 0;
            for (int b = static_cast<int>(sizeof(T)) - 1; b >= 0; --b)
                v = (v << 8) | raw[i * sizeof(T) + static_cast<size_t>(b)];
            out[i] = static_cast<T>(v);
        }
        return out;
    }

    struct PerfJobsRig : ::testing::Test
    {
        EventStreamStore<CoarseChunk> coarse{ StringCRC("test_perfjobs_coarse"), StringCRC("CoarseChunk"), 16, 1 };
        EventStreamStore<JobChunk>    jobs{ StringCRC("test_perfjobs_jobs"), StringCRC("JobChunk"), 16, 1 };
        PerfWindowCommands cmds{ &coarse, nullptr, &jobs };
        uint32_t seq = 0;
        uint64_t nextRaw = 0;
        uint64_t nextAgg = 0;

        // One chunk with n raw records (start = 1 s + i ms, 100 us long, owner 0x42) and one aggregate per record.
        // frame = absolute aggregate index, so the window ends at the newest frame.
        void SendChunk(uint32_t unitCrc, uint32_t n, bool perJob = true)
        {
            auto chunk = std::make_unique<JobChunk>();
            std::memset(chunk.get(), 0, sizeof(JobChunk));
            chunk->unitCrc = unitCrc;
            chunk->seq = seq++;
            chunk->flags = perJob ? JobChunk::kFlagPerJob : 0u;
            chunk->ownerCount = 1;
            chunk->owners[0].owner = 0x42u;
            chunk->owners[0].count = 5;
            chunk->owners[0].busyNs = 2000000;
            chunk->owners[0].waitP50Ms = 0.5f;
            chunk->rawFirst = nextRaw;
            chunk->aggFirst = nextAgg;
            const uint32_t cap = n < JobChunk::kMaxRaw ? n : JobChunk::kMaxRaw;
            chunk->rawCount = cap;
            chunk->aggCount = cap < JobChunk::kMaxAggregates ? cap : JobChunk::kMaxAggregates;
            for (uint32_t i = 0; i < cap; ++i)
            {
                JobRecordPacked& r = chunk->raw[i];
                r.startNs = 1000000000ull + static_cast<uint64_t>(nextRaw + i) * 1000000ull;
                r.durNs = 100000;
                r.owner = 0x42u;
                r.tag = 7;
                r.worker = static_cast<uint16_t>(i % 3);
            }
            for (uint32_t i = 0; i < chunk->aggCount; ++i)
                chunk->aggs[i] = { static_cast<uint32_t>(nextAgg + i), 0x42u, 2, 0.5f, 0.25f };
            nextRaw += cap;
            nextAgg += chunk->aggCount;
            Event<JobChunk> ev;
            ev.payload = *chunk;
            jobs.Send(ev);
        }

        static Json::Value Args(const char* unit, Json::UInt64 from, Json::UInt64 to)
        {
            Json::Value a(Json::objectValue);
            if (unit) a["unit"] = unit;
            a["fromNs"] = from;
            a["toNs"] = to;
            return a;
        }
    };
}

TEST_F(PerfJobsRig, DrainAppliesPendingJobChunks)
{
    SendChunk(0x11u, 10);
    SendChunk(0x11u, 10);
    SendChunk(0x22u, 5);
    EXPECT_EQ(0u, cmds.GetJobsAssembler().GetUnitCount());
    cmds.Drain();
    EXPECT_EQ(2u, cmds.GetJobsAssembler().GetUnitCount());
    EXPECT_EQ(20u, cmds.GetJobsAssembler().GetRawCount(0x11u));
    ASSERT_NE(nullptr, cmds.GetJobsAssembler().GetLatest(0x22u));
    cmds.Drain();   // nothing pending
    EXPECT_EQ(20u, cmds.GetJobsAssembler().GetRawCount(0x11u));
}

TEST_F(PerfJobsRig, DrainWithNullJobsStoreIsHarmless)
{
    PerfWindowCommands none(&coarse, nullptr);
    none.Drain();
    EXPECT_EQ(0u, none.GetJobsAssembler().GetUnitCount());
    EXPECT_TRUE(none.HandleJobs(Args("0x00000011", 0, 10)).isMember("error"));
}

TEST_F(PerfJobsRig, JobsReturnsPackedRecordsInRange)
{
    SendChunk(0x11u, 20);   // starts 1.000 .. 1.019 s, 0.1 ms long
    cmds.Drain();
    // 1.005 s .. 1.009 s inclusive of the 100 us tail of the record starting at 1.004 s is not covered (ends 1.0041).
    const Json::Value r = cmds.HandleJobs(Args("0x00000011", 1005000000ull, 1009000000ull));
    EXPECT_FALSE(r.isMember("error"));
    EXPECT_FALSE(r["truncated"].asBool());
    EXPECT_TRUE(r["perJobRecording"].asBool());
    EXPECT_EQ(5u, r["count"].asUInt());
    const auto start = Decode<uint64_t>(r["startNs"].asString());
    ASSERT_EQ(5u, start.size());
    EXPECT_EQ(1005000000ull, start[0]);
    EXPECT_EQ(1009000000ull, start[4]);
    EXPECT_EQ(5u, Decode<uint16_t>(r["worker"].asString()).size());
    EXPECT_EQ(0x42u, Decode<uint32_t>(r["owner"].asString())[0]);
}

TEST_F(PerfJobsRig, JobsCapsAt2000AndStatesTruncation)
{
    // 2,100 records over 10 chunks of 224 (assembler ring holds 8192).
    for (int i = 0; i < 10; ++i)
        SendChunk(0x11u, 224);
    cmds.Drain();
    ASSERT_EQ(2240u, cmds.GetJobsAssembler().GetRawCount(0x11u));
    const Json::Value r = cmds.HandleJobs(Args("0x00000011", 0, 9000000000ull));
    EXPECT_TRUE(r["truncated"].asBool());
    EXPECT_EQ(2000u, r["count"].asUInt());
    EXPECT_EQ(2000u, Decode<uint64_t>(r["startNs"].asString()).size());
    // Exactly at the cap is not truncated.
    const Json::Value ok = cmds.HandleJobs(Args("0x00000011", 1000000000ull, 1000000000ull + 1999ull * 1000000ull));
    EXPECT_FALSE(ok["truncated"].asBool());
    EXPECT_EQ(2000u, ok["count"].asUInt());
}

TEST_F(PerfJobsRig, JobsErrors)
{
    SendChunk(0x11u, 5);
    cmds.Drain();
    EXPECT_TRUE(cmds.HandleJobs(Args("Nope", 0, 10)).isMember("error"));
    EXPECT_TRUE(cmds.HandleJobs(Args(nullptr, 0, 10)).isMember("error"));
    EXPECT_TRUE(cmds.HandleJobs(Args("0x00000011", 10, 5)).isMember("error"));   // from > to
    Json::Value noTo(Json::objectValue);
    noTo["unit"] = "0x00000011";
    noTo["fromNs"] = 0;
    EXPECT_TRUE(cmds.HandleJobs(noTo).isMember("error"));
    Json::Value negative = Args("0x00000011", 0, 10);
    negative["fromNs"] = -5;
    EXPECT_TRUE(cmds.HandleJobs(negative).isMember("error"));
    EXPECT_FALSE(cmds.HandleJobs(Args("17", 0, 2000000000ull)).isMember("error"));   // decimal crc selector
}

TEST_F(PerfJobsRig, KillSwitchOffChunksGiveEmptyRecordsAndNoAggregates)
{
    SendChunk(0x11u, 20, false);   // per-job recording flag cleared
    cmds.Drain();
    const Json::Value r = cmds.HandleJobs(Args("0x00000011", 0, 9000000000ull));
    EXPECT_FALSE(r.isMember("error"));
    EXPECT_FALSE(r["perJobRecording"].asBool());
    EXPECT_FALSE(r["truncated"].asBool());
    EXPECT_EQ(0u, r["count"].asUInt());
    EXPECT_TRUE(r["startNs"].asString().empty());

    const Json::Value w = cmds.HandleWindow(Json::Value(Json::objectValue));
    ASSERT_TRUE(w.isMember("jobs"));
    EXPECT_FALSE(w["jobs"]["perJobRecording"].asBool());
    ASSERT_EQ(1u, w["jobs"]["units"].size());
    EXPECT_FALSE(w["jobs"]["units"][0]["perJobRecording"].asBool());
    EXPECT_EQ(0u, w["jobs"]["units"][0]["owners"].size());
    EXPECT_EQ(0u, w["jobs"]["units"][0]["bucketCount"].asUInt());
}

TEST_F(PerfJobsRig, WindowCarriesJobsSectionWithOwnersAndEmptySteps)
{
    SendChunk(0x11u, 40);
    cmds.Drain();
    const Json::Value w = cmds.HandleWindow(Json::Value(Json::objectValue));
    ASSERT_TRUE(w.isMember("jobs"));
    EXPECT_EQ(200u, w["jobs"]["bucketMs"].asUInt());
    EXPECT_TRUE(w["jobs"]["steps"].isArray());
    EXPECT_EQ(0u, w["jobs"]["steps"].size());
    ASSERT_EQ(1u, w["jobs"]["units"].size());
    const Json::Value& u = w["jobs"]["units"][0];
    EXPECT_EQ("0x00000011", u["unit"].asString());
    ASSERT_EQ(1u, u["owners"].size());
    EXPECT_EQ(0x42u, u["owners"][0]["crc"].asUInt());
    const uint32_t buckets = u["bucketCount"].asUInt();
    const auto count = Decode<uint16_t>(u["frameAgg"]["count"].asString());
    ASSERT_EQ(buckets, count.size());
    uint32_t total = 0;
    for (uint16_t c : count) total += c;
    EXPECT_EQ(80u, total);   // 40 frames x count 2
    // The coarse part of the v1 reply is unchanged.
    EXPECT_EQ(1, w["v"].asInt());
    EXPECT_TRUE(w["units"].isArray());
}

TEST_F(PerfJobsRig, WindowStepsCarryDeferredStepSummariesInTheInspectorShape)
{
    SendChunk(0x11u, 5);
    cmds.Drain();
    // A chunk from the same unit carrying two step summaries.
    auto chunk = std::make_unique<JobChunk>();
    std::memset(chunk.get(), 0, sizeof(JobChunk));
    chunk->unitCrc = 0x11u;
    chunk->seq = seq++;
    chunk->flags = JobChunk::kFlagPerJob;
    chunk->rawFirst = nextRaw;
    chunk->aggFirst = nextAgg;
    chunk->stepCount = 2;
    chunk->steps[0].channel = 0xABCDu;
    chunk->steps[0].kickKey = 3;
    chunk->steps[0].applyKey = 5;
    chunk->steps[0].ranges = 4;
    chunk->steps[0].kickNs = 5000000000000ull;
    chunk->steps[0].firstStartNs = 5000000001000ull;
    chunk->steps[0].readyNs = 5000000002000ull;
    chunk->steps[0].applyNs = 5000000003000ull;
    chunk->steps[0].stallMs = 1.25f;
    chunk->steps[0].cpuMs = 2.5f;
    chunk->steps[0].capWaitMs = 0.5f;
    chunk->steps[0].workerWaitMs = 0.75f;
    chunk->steps[1].channel = 0xABCDu;
    chunk->steps[1].kickKey = 4;
    Event<JobChunk> ev;
    ev.payload = *chunk;
    jobs.Send(ev);
    cmds.Drain();

    const Json::Value w = cmds.HandleWindow(Json::Value(Json::objectValue));
    const Json::Value& steps = w["jobs"]["steps"];
    ASSERT_TRUE(steps.isArray());
    ASSERT_EQ(2u, steps.size());
    const Json::Value& s = steps[0u];
    EXPECT_EQ("0x00000011", s["unit"].asString());
    EXPECT_EQ("0x0000abcd", s["channel"].asString());   // unresolved channel falls back to hex
    EXPECT_EQ(3u, s["kickKey"].asUInt());
    EXPECT_EQ(5u, s["applyKey"].asUInt());
    EXPECT_EQ(5000000000000ull, s["kick"].asUInt64());
    EXPECT_EQ(5000000001000ull, s["firstStart"].asUInt64());
    EXPECT_EQ(5000000002000ull, s["ready"].asUInt64());
    EXPECT_EQ(5000000003000ull, s["apply"].asUInt64());
    EXPECT_DOUBLE_EQ(1.25, s["stallMs"].asDouble());
    EXPECT_DOUBLE_EQ(2.5, s["cpuMs"].asDouble());
    EXPECT_DOUBLE_EQ(0.5, s["capWaitMs"].asDouble());
    EXPECT_DOUBLE_EQ(0.75, s["workerWaitMs"].asDouble());
    EXPECT_EQ(4u, s["ranges"].asUInt());
    EXPECT_EQ(4u, steps[1u]["kickKey"].asUInt());
}

TEST_F(PerfJobsRig, WindowStepsAreStillEmptyWhenNoSummariesWereShipped)
{
    SendChunk(0x11u, 5);
    cmds.Drain();
    const Json::Value w = cmds.HandleWindow(Json::Value(Json::objectValue));
    EXPECT_TRUE(w["jobs"]["steps"].isArray());
    EXPECT_EQ(0u, w["jobs"]["steps"].size());
}

TEST_F(PerfJobsRig, WindowWithoutJobsStoreStillHasEmptyJobsSection)
{
    PerfWindowCommands none(&coarse, nullptr);
    const Json::Value w = none.HandleWindow(Json::Value(Json::objectValue));
    ASSERT_TRUE(w.isMember("jobs"));
    EXPECT_EQ(0u, w["jobs"]["units"].size());
    EXPECT_EQ(0u, w["jobs"]["steps"].size());
}

TEST_F(PerfJobsRig, EndToEndFromJobTimingTableAndBuilder)
{
    JobTimingTable table;
    table.Init();
    table.SetPerJobRecording(true);
    Dia::Threading::JobTimingRecord rec{};
    rec.submitNs = 900000000ull; rec.startNs = 1000000000ull; rec.endNs = 1002000000ull;
    rec.owner = 0x42u; rec.tag = 3; rec.worker = 1;
    table.Record(rec);
    table.EndFrame(0);
    JobChunkBuilder builder;
    builder.Begin(0x11u);
    Event<JobChunk> ev;
    ev.payload = builder.Take(table, 1000.0);
    jobs.Send(ev);
    cmds.Drain();
    const Json::Value r = cmds.HandleJobs(Args("0x00000011", 0, 2000000000ull));
    EXPECT_EQ(1u, r["count"].asUInt());
    EXPECT_EQ(2000000u, Decode<uint32_t>(r["durNs"].asString())[0]);
    EXPECT_EQ(100000u, Decode<uint32_t>(r["waitUs"].asString())[0]);
}

TEST_F(PerfJobsRig, RegisterDispatchesPerfJobsWithDescriptionAndParams)
{
    cmds.Register();
    cmds.Register();
    SendChunk(0x11u, 5);
    cmds.Drain();
    Json::Value resp = Dia::API::ExecuteCommandJson(StringCRC("perf.jobs"), Args("0x00000011", 0, 9000000000ull));
    EXPECT_TRUE(resp["success"].asBool());
    EXPECT_EQ(5u, resp["data"]["count"].asUInt());

    Dia::Core::Containers::DynamicArrayC<Dia::API::CommandInfoJson, 256> all;
    Dia::API::ListJsonCommandInfos(all);
    unsigned found = 0;
    for (unsigned int i = 0; i < all.Size(); ++i)
    {
        if (!(all[i].name == StringCRC("perf.jobs"))) continue;
        ++found;
        ASSERT_NE(all[i].description, nullptr);
        EXPECT_GT(std::strlen(all[i].description), 0u);
        ASSERT_NE(all[i].params, nullptr);
        EXPECT_EQ(3u, all[i].paramCount);
        for (unsigned p = 0; p < all[i].paramCount; ++p)
        {
            ASSERT_NE(all[i].params[p].description, nullptr);
            EXPECT_GT(std::strlen(all[i].params[p].description), 0u);
        }
    }
    EXPECT_EQ(1u, found);
    cmds.Unregister();
    resp = Dia::API::ExecuteCommandJson(StringCRC("perf.jobs"), Args("0x00000011", 0, 10));
    EXPECT_FALSE(resp["success"].asBool());
}

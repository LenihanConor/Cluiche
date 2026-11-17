////////////////////////////////////////////////////////////////////////////////
// Filename: TestPerfChainCommands.cpp
// GoogleTest suite - Perf Inspector row 6b: perf.chain (Main-hosted), its "$perfspans" drain and history.
// Uses a fake thread resolver and fake edge statistics; the chain logic itself is in TestChainBuilder.cpp.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <Types/PerfChainCommands.h>

#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaObservation/Trace/SpanChunk.h>
#include <DiaStreams/EventStreamStore.h>

#include <cstring>
#include <memory>
#include <string>

using namespace Dia::Observation::Trace;
using Cluiche::AppFlow::PerfChainCommands;
using Dia::ApplicationFlow::Event;
using Dia::ApplicationFlow::EventStreamStore;
using Dia::Core::StringCRC;

namespace
{
    const uint64_t kMs = 1000000ull;

    ChainSpan Span(const char* name, uint64_t trace, uint64_t id, uint64_t parent, uint32_t thread,
                   uint64_t startMs, uint64_t endMs, uint64_t linkTrace = 0, uint64_t linkSpan = 0)
    {
        ChainSpan s{};
        s.traceId = trace; s.spanId = id; s.parentSpanId = parent; s.threadId = thread;
        s.startSteadyNs = startMs * kMs; s.endSteadyNs = endMs * kMs;
        s.linkTraceId = linkTrace; s.linkSpanId = linkSpan;
        std::strncpy(s.name, name, ChainSpan::kNameLength - 1);
        return s;
    }

    struct PerfChainRig : ::testing::Test
    {
        EventStreamStore<SpanChunk> store{ StringCRC("test_perfspans"), StringCRC("SpanChunk"), 128, 1 };
        int statsCalls = 0;
        PerfChainCommands cmds{ &store,
            [](uint32_t t, char* buf, size_t n) { std::snprintf(buf, n, t == 1 ? "Sim" : "Main"); return true; },
            [this](const char*, ChainEdgeStats& s) { ++statsCalls; s.hasBytes = true; s.bytesPerWrite = 100; return true; } };

        void Send(const std::vector<ChainSpan>& spans)
        {
            for (size_t first = 0; first < spans.size(); first += SpanChunk::kMaxSpans)
            {
                auto chunk = std::make_unique<SpanChunk>();
                std::memset(chunk.get(), 0, sizeof(SpanChunk));
                for (size_t i = first; i < spans.size() && chunk->count < SpanChunk::kMaxSpans; ++i)
                    chunk->spans[chunk->count++] = spans[i];
                Event<SpanChunk> ev;
                ev.payload = *chunk;
                store.Send(ev);
            }
        }

        static std::vector<ChainSpan> Chain()
        {
            return {
                Span("DomainStateSnapshotPublisher.Update", 1, 10, 0, 1, 1000, 1007),
                Span("DebugPanelPageModule.PollDomainSnapshot", 2, 21, 20, 2, 1018, 1021, 1, 10),
                Span("DebugPanelPageModule.PushDomainStatesToPanel", 2, 22, 20, 2, 1021, 1023),
            };
        }
    };
}

TEST_F(PerfChainRig, NoSpansIsUnavailableWithReason)
{
    cmds.Drain();
    const Json::Value r = cmds.HandleChain(Json::Value(Json::objectValue));
    EXPECT_FALSE(r["available"].asBool());
    EXPECT_FALSE(r["reason"].asString().empty());
    EXPECT_EQ(0, statsCalls);
}

TEST_F(PerfChainRig, DrainThenChainReturnsRealStagesAndEdge)
{
    Send(Chain());
    cmds.Drain();
    EXPECT_EQ(3u, cmds.SpanCount());
    const Json::Value r = cmds.HandleChain(Json::Value(Json::objectValue));
    ASSERT_TRUE(r["available"].asBool()) << r.toStyledString();
    EXPECT_EQ(3u, r["stages"].size());
    EXPECT_EQ(1u, r["edges"].size());
    EXPECT_EQ("Sim", r["stages"][0u]["unit"].asString());
    EXPECT_DOUBLE_EQ(100.0, r["edges"][0u]["bytes"].asDouble());
}

TEST_F(PerfChainRig, NothingIsBuiltWithoutACall)
{
    Send(Chain());
    cmds.Drain();
    EXPECT_EQ(0, statsCalls);   // stats are only read when perf.chain runs
}

TEST_F(PerfChainRig, HistoryIsBounded)
{
    std::vector<ChainSpan> many;
    for (uint32_t i = 0; i < PerfChainCommands::kHistoryCapacity + 500; ++i)
        many.push_back(Span("Filler", 9, 1000 + i, 0, 1, 1, 2));
    Send(many);
    for (int i = 0; i < 40; ++i) cmds.Drain();
    EXPECT_EQ(PerfChainCommands::kHistoryCapacity, cmds.SpanCount());
}

TEST_F(PerfChainRig, OldChainEvictedByNewerSpansBecomesUnavailable)
{
    Send(Chain());
    std::vector<ChainSpan> many;
    for (uint32_t i = 0; i < PerfChainCommands::kHistoryCapacity; ++i)
        many.push_back(Span("Filler", 9, 1000 + i, 0, 1, 1, 2));
    Send(many);
    for (int i = 0; i < 40; ++i) cmds.Drain();
    EXPECT_FALSE(cmds.HandleChain(Json::Value(Json::objectValue))["available"].asBool());
}

TEST(PerfChainCommands, NullStoreIsHarmless)
{
    PerfChainCommands cmds(nullptr, nullptr);
    cmds.Drain();
    const Json::Value r = cmds.HandleChain(Json::Value());
    EXPECT_FALSE(r["available"].asBool());
}

TEST(PerfChainCommands, DefaultEdgeStatsWithoutMetricsLeavesFieldsAbsent)
{
    EventStreamStore<SpanChunk> store{ StringCRC("test_perfspans2"), StringCRC("SpanChunk"), 128, 1 };
    PerfChainCommands cmds(&store, nullptr);   // default metric reader, stream "DomainStateSnapshot" not registered in this test
    auto chunk = std::make_unique<SpanChunk>();
    std::memset(chunk.get(), 0, sizeof(SpanChunk));
    chunk->spans[chunk->count++] = Span("DomainStateSnapshotPublisher.Update", 1, 10, 0, 1, 1000, 1007);
    chunk->spans[chunk->count++] = Span("DebugPanelPageModule.PollDomainSnapshot", 2, 21, 20, 2, 1018, 1021, 1, 10);
    Event<SpanChunk> ev;
    ev.payload = *chunk;
    store.Send(ev);
    cmds.Drain();
    const Json::Value r = cmds.HandleChain(Json::Value());
    ASSERT_TRUE(r["available"].asBool());
    EXPECT_EQ("thread 1", r["stages"][0u]["unit"].asString());
    ASSERT_EQ(1u, r["edges"].size());
    EXPECT_FALSE(r["edges"][0u].isMember("bytes"));
    EXPECT_FALSE(r["edges"][0u].isMember("ratePerSec"));
}

TEST_F(PerfChainRig, RegisterDispatchesPerfChainWithDescription)
{
    cmds.Register();
    cmds.Register();
    Send(Chain());
    cmds.Drain();
    Json::Value resp = Dia::API::ExecuteCommandJson(StringCRC("perf.chain"), Json::Value(Json::objectValue));
    EXPECT_TRUE(resp["success"].asBool());
    EXPECT_TRUE(resp["data"]["available"].asBool());

    Dia::Core::Containers::DynamicArrayC<Dia::API::CommandInfoJson, 256> all;
    Dia::API::ListJsonCommandInfos(all);
    unsigned found = 0;
    for (unsigned int i = 0; i < all.Size(); ++i)
    {
        if (!(all[i].name == StringCRC("perf.chain"))) continue;
        ++found;
        ASSERT_NE(all[i].description, nullptr);
        EXPECT_GT(std::strlen(all[i].description), 0u);
    }
    EXPECT_EQ(1u, found);
    cmds.Unregister();
    resp = Dia::API::ExecuteCommandJson(StringCRC("perf.chain"), Json::Value(Json::objectValue));
    EXPECT_FALSE(resp["success"].asBool());
}

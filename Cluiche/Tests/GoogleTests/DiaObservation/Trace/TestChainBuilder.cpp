////////////////////////////////////////////////////////////////////////////////
// Filename: TestChainBuilder.cpp
// GoogleTest suite - Perf Inspector row 6b: ChainBuilder (pure span history -> ChainData JSON) and
// SpanChunkSink (tracer sink -> SpanChunk batches).
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <DiaObservation/Trace/ChainBuilder.h>
#include <DiaObservation/Trace/SpanChunk.h>
#include <DiaObservation/Trace/SpanChunkSink.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstring>
#include <string>
#include <vector>

using namespace Dia::Observation::Trace;

namespace
{
    const uint32_t kSimThread  = 111;
    const uint32_t kMainThread = 222;
    const uint64_t kMs         = 1000000ull;

    ChainSpan Span(const char* name, uint64_t traceId, uint64_t spanId, uint64_t parent, uint32_t thread,
                   uint64_t startMs, uint64_t endMs, uint64_t linkTrace = 0, uint64_t linkSpan = 0)
    {
        ChainSpan s{};
        s.traceId = traceId; s.spanId = spanId; s.parentSpanId = parent;
        s.linkTraceId = linkTrace; s.linkSpanId = linkSpan;
        s.startSteadyNs = startMs * kMs; s.endSteadyNs = endMs * kMs;
        s.threadId = thread;
        std::strncpy(s.name, name, ChainSpan::kNameLength - 1);
        uint32_t crc = 0;
        for (const char* p = name; *p; ++p) crc = crc * 31u + static_cast<unsigned char>(*p);
        s.nameCrc = crc;
        return s;
    }

    ChainConfig Config()
    {
        return { "Debug panel card state", "DebugPanelPageModule.PollDomainSnapshot",
                 "DebugPanelPageModule.PushDomainStatesToPanel", "DomainStateSnapshot" };
    }

    bool UnitNames(uint32_t threadId, char* buf, size_t size)
    {
        const char* n = threadId == kSimThread ? "Sim" : threadId == kMainThread ? "Main" : nullptr;
        if (!n) return false;
        std::snprintf(buf, size, "%s", n);
        return true;
    }

    // The realistic S-1 history: Sim trace 1 (Update with two GetJSONState children, one slower), Main trace 2
    // (Update root with Poll linked to Sim's Update, then Push).
    std::vector<ChainSpan> RealisticSpans()
    {
        std::vector<ChainSpan> v;
        v.push_back(Span("IDebugDomain.GetJSONState", 1, 11, 10, kSimThread, 1000, 1001));
        v.push_back(Span("IDebugDomain.GetJSONState", 1, 12, 10, kSimThread, 1001, 1005));
        v.push_back(Span("DomainStateSnapshotPublisher.Update", 1, 10, 0, kSimThread, 1000, 1007));
        v.push_back(Span("DebugPanelPageModule.PollDomainSnapshot", 2, 21, 20, kMainThread, 1018, 1021, 1, 10));
        v.push_back(Span("DebugPanelPageModule.PushDomainStatesToPanel", 2, 22, 20, kMainThread, 1021, 1023));
        v.push_back(Span("DebugPanelPageModule.Update", 2, 20, 0, kMainThread, 1017, 1024));
        return v;
    }

    bool Build(const std::vector<ChainSpan>& spans, Json::Value& out, const EdgeStatsFn& stats = EdgeStatsFn())
    {
        std::string text;
        const bool ok = BuildChainJson(spans.data(), static_cast<uint32_t>(spans.size()), Config(), UnitNames, stats, text);
        Json::Reader reader;
        EXPECT_TRUE(reader.parse(text, out)) << text;
        EXPECT_EQ(ok, out["available"].asBool());
        return ok;
    }

    const Json::Value* FindStage(const Json::Value& v, const char* name)
    {
        for (const auto& s : v["stages"]) if (s["name"].asString() == name) return &s;
        return nullptr;
    }
}

TEST(ChainBuilder, RealisticChainHasAllStagesAndRealNames)
{
    Json::Value v;
    ASSERT_TRUE(Build(RealisticSpans(), v));
    EXPECT_EQ(v["name"].asString(), "Debug panel card state");
    ASSERT_EQ(v["stages"].size(), 4u);
    EXPECT_NE(FindStage(v, "IDebugDomain.GetJSONState"), nullptr);
    EXPECT_NE(FindStage(v, "DomainStateSnapshotPublisher.Update"), nullptr);
    EXPECT_NE(FindStage(v, "DebugPanelPageModule.PollDomainSnapshot"), nullptr);
    EXPECT_NE(FindStage(v, "DebugPanelPageModule.PushDomainStatesToPanel"), nullptr);
    EXPECT_EQ(FindStage(v, "DebugPanelPageModule.Update"), nullptr);   // unrelated root is not part of the chain
}

TEST(ChainBuilder, UnitsComeFromThreadResolverWithFallback)
{
    auto spans = RealisticSpans();
    spans.push_back(Span("DebugPanelPageModule.PollDomainSnapshot", 3, 31, 0, 999, 1030, 1031, 1, 10));
    Json::Value v;
    ASSERT_TRUE(Build(spans, v));   // newest end span is on an unknown thread
    EXPECT_EQ((*FindStage(v, "DebugPanelPageModule.PollDomainSnapshot"))["unit"].asString(), "thread 999");
    EXPECT_EQ((*FindStage(v, "DomainStateSnapshotPublisher.Update"))["unit"].asString(), "Sim");
}

TEST(ChainBuilder, StageTimesAreMillisecondsOfSteadyNs)
{
    Json::Value v;
    ASSERT_TRUE(Build(RealisticSpans(), v));
    const Json::Value& u = *FindStage(v, "DomainStateSnapshotPublisher.Update");
    EXPECT_DOUBLE_EQ(u["startMs"].asDouble(), 1000.0);
    EXPECT_DOUBLE_EQ(u["endMs"].asDouble(), 1007.0);
    EXPECT_EQ(u["id"].asString(), "10");
    EXPECT_EQ(u["unit"].asString(), "Sim");
}

TEST(ChainBuilder, LongestChildPerNameIsKept)
{
    Json::Value v;
    ASSERT_TRUE(Build(RealisticSpans(), v));
    const Json::Value& g = *FindStage(v, "IDebugDomain.GetJSONState");
    EXPECT_EQ(g["id"].asString(), "12");   // the 4 ms one, not the 1 ms one
}

TEST(ChainBuilder, EdgeFollowsFlowLinkWithStats)
{
    EdgeStatsFn stats = [](const char* stream, ChainEdgeStats& s)
    {
        EXPECT_STREQ(stream, "DomainStateSnapshot");
        s.hasBytes = true;     s.bytesPerWrite = 6656;
        s.hasRate = true;      s.ratePerSec = 30;
        s.hasDataAge = true;   s.dataAgeMs = 12;
        s.hasWriteCost = true; s.writeCostMs = 0.2;
        return true;
    };
    Json::Value v;
    ASSERT_TRUE(Build(RealisticSpans(), v, stats));
    ASSERT_EQ(v["edges"].size(), 1u);
    const Json::Value& e = v["edges"][0u];
    EXPECT_EQ(e["stream"].asString(), "DomainStateSnapshot");
    EXPECT_EQ(e["fromId"].asString(), "10");
    EXPECT_EQ(e["toId"].asString(), "21");
    EXPECT_DOUBLE_EQ(e["bytes"].asDouble(), 6656.0);
    EXPECT_DOUBLE_EQ(e["ratePerSec"].asDouble(), 30.0);
    EXPECT_DOUBLE_EQ(e["dataAgeMs"].asDouble(), 12.0);
    EXPECT_NEAR(e["writeCostMs"].asDouble(), 0.2, 1e-9);
}

TEST(ChainBuilder, MissingEdgeMetricsLeaveFieldsAbsent)
{
    EdgeStatsFn stats = [](const char*, ChainEdgeStats& s)
    {
        s.hasDataAge = true; s.dataAgeMs = 5;
        return true;
    };
    Json::Value v;
    ASSERT_TRUE(Build(RealisticSpans(), v, stats));
    const Json::Value& e = v["edges"][0u];
    EXPECT_TRUE(e.isMember("dataAgeMs"));
    EXPECT_FALSE(e.isMember("bytes"));
    EXPECT_FALSE(e.isMember("ratePerSec"));
    EXPECT_FALSE(e.isMember("writeCostMs"));
}

TEST(ChainBuilder, NoStatsSourceStillYieldsEdge)
{
    Json::Value v;
    ASSERT_TRUE(Build(RealisticSpans(), v));
    ASSERT_EQ(v["edges"].size(), 1u);
    EXPECT_FALSE(v["edges"][0u].isMember("bytes"));
}

TEST(ChainBuilder, NewestResolvableChainWins)
{
    auto spans = RealisticSpans();
    // A newer Poll whose writer fell out of history, then the old resolvable one stays chosen.
    spans.push_back(Span("DebugPanelPageModule.PollDomainSnapshot", 4, 41, 0, kMainThread, 1040, 1041, 9, 99));
    Json::Value v;
    ASSERT_TRUE(Build(spans, v));
    EXPECT_EQ((*FindStage(v, "DebugPanelPageModule.PollDomainSnapshot"))["id"].asString(), "21");
}

TEST(ChainBuilder, NewerChainPreferredWhenBothResolve)
{
    auto spans = RealisticSpans();
    spans.push_back(Span("DomainStateSnapshotPublisher.Update", 5, 50, 0, kSimThread, 1100, 1103));
    spans.push_back(Span("DebugPanelPageModule.PollDomainSnapshot", 6, 61, 0, kMainThread, 1110, 1112, 5, 50));
    Json::Value v;
    ASSERT_TRUE(Build(spans, v));
    EXPECT_EQ((*FindStage(v, "DebugPanelPageModule.PollDomainSnapshot"))["id"].asString(), "61");
    EXPECT_EQ(FindStage(v, "DebugPanelPageModule.PushDomainStatesToPanel"), nullptr);   // no tail in trace 6
}

TEST(ChainBuilder, TailBeforeEndSpanIsNotUsed)
{
    auto spans = RealisticSpans();
    spans[4] = Span("DebugPanelPageModule.PushDomainStatesToPanel", 2, 22, 20, kMainThread, 1010, 1011);   // starts before Poll ended
    Json::Value v;
    ASSERT_TRUE(Build(spans, v));
    EXPECT_EQ(FindStage(v, "DebugPanelPageModule.PushDomainStatesToPanel"), nullptr);
}

TEST(ChainBuilder, EmptyHistoryIsUnavailableWithReason)
{
    std::string text;
    EXPECT_FALSE(BuildChainJson(nullptr, 0, Config(), UnitNames, EdgeStatsFn(), text));
    Json::Value v; Json::Reader r; ASSERT_TRUE(r.parse(text, v));
    EXPECT_FALSE(v["available"].asBool());
    EXPECT_FALSE(v["reason"].asString().empty());
}

TEST(ChainBuilder, NoLinkedSpanIsUnavailable)
{
    std::vector<ChainSpan> spans;
    spans.push_back(Span("DebugPanelPageModule.PollDomainSnapshot", 2, 21, 20, kMainThread, 1018, 1021));   // no link
    spans.push_back(Span("DomainStateSnapshotPublisher.Update", 1, 10, 0, kSimThread, 1000, 1007));
    Json::Value v;
    EXPECT_FALSE(Build(spans, v));
    EXPECT_NE(v["reason"].asString().find("no linked chain"), std::string::npos);
}

TEST(ChainBuilder, WriterMissingFromHistoryIsUnavailable)
{
    std::vector<ChainSpan> spans;
    spans.push_back(Span("DebugPanelPageModule.PollDomainSnapshot", 2, 21, 20, kMainThread, 1018, 1021, 1, 10));
    Json::Value v;
    EXPECT_FALSE(Build(spans, v));
    EXPECT_NE(v["reason"].asString().find("not in recent history"), std::string::npos);
}

TEST(ChainBuilder, LinkMustMatchTraceIdToo)
{
    auto spans = RealisticSpans();
    spans[3].linkTraceId = 77;   // right span id, wrong trace
    Json::Value v;
    EXPECT_FALSE(Build(spans, v));
}

TEST(ChainBuilder, MultiHopLinkChainIsFollowed)
{
    std::vector<ChainSpan> spans;
    spans.push_back(Span("A", 1, 1, 0, kSimThread, 1000, 1002));
    spans.push_back(Span("B", 2, 2, 0, kSimThread, 1003, 1005, 1, 1));
    spans.push_back(Span("DebugPanelPageModule.PollDomainSnapshot", 3, 3, 0, kMainThread, 1006, 1008, 2, 2));
    Json::Value v;
    ASSERT_TRUE(Build(spans, v));
    EXPECT_EQ(v["stages"].size(), 3u);
    EXPECT_EQ(v["edges"].size(), 2u);
}

TEST(ChainBuilder, LinkCycleTerminates)
{
    std::vector<ChainSpan> spans;
    spans.push_back(Span("A", 1, 1, 0, kSimThread, 1000, 1002, 1, 1));   // links to itself
    spans.push_back(Span("DebugPanelPageModule.PollDomainSnapshot", 3, 3, 0, kMainThread, 1006, 1008, 1, 1));
    Json::Value v;
    EXPECT_TRUE(Build(spans, v));   // bounded hops, still valid JSON
}

TEST(ChainBuilder, NamesAreJsonEscaped)
{
    auto spans = RealisticSpans();
    std::strncpy(spans[2].name, "Quote\"Zone", ChainSpan::kNameLength - 1);
    Json::Value v;
    ASSERT_TRUE(Build(spans, v));
    EXPECT_NE(FindStage(v, "Quote\"Zone"), nullptr);
}

// --- SpanChunkSink -----------------------------------------------------------------------------

namespace
{
    SpanRecord Record(const char* name, uint64_t spanId, uint64_t endMs)
    {
        SpanRecord r{};
        r.name = Dia::Core::StringCRC(name);
        r.spanId = spanId; r.traceId = 7; r.startSteadyNs = (endMs - 1) * kMs; r.endSteadyNs = endMs * kMs;
        r.threadId = 5; r.linkTraceId = 3; r.linkSpanId = 4;
        return r;
    }
}

TEST(SpanChunkSink, CopiesFieldsAndFlushesOnDemand)
{
    std::vector<SpanChunk> got;
    SpanChunkSink sink([&](const SpanChunk& c) { got.push_back(c); });
    sink.OnSpan(Record("Zone.A", 42, 100));
    EXPECT_TRUE(got.empty());
    sink.Flush();
    ASSERT_EQ(got.size(), 1u);
    ASSERT_EQ(got[0].count, 1u);
    const ChainSpan& s = got[0].spans[0];
    EXPECT_EQ(s.spanId, 42u);
    EXPECT_EQ(s.traceId, 7u);
    EXPECT_EQ(s.linkSpanId, 4u);
    EXPECT_EQ(s.threadId, 5u);
    EXPECT_STREQ(s.name, "Zone.A");
    EXPECT_EQ(s.endSteadyNs, 100 * kMs);
}

TEST(SpanChunkSink, FlushesWhenFull)
{
    std::vector<SpanChunk> got;
    SpanChunkSink sink([&](const SpanChunk& c) { got.push_back(c); });
    for (uint32_t i = 0; i < SpanChunk::kMaxSpans; ++i)
        sink.OnSpan(Record("Z", i + 1, 100));
    ASSERT_EQ(got.size(), 1u);
    EXPECT_EQ(got[0].count, SpanChunk::kMaxSpans);
}

TEST(SpanChunkSink, FlushesAgedChunkBeforeNewSpan)
{
    std::vector<SpanChunk> got;
    SpanChunkSink sink([&](const SpanChunk& c) { got.push_back(c); });
    sink.OnSpan(Record("Z", 1, 100));
    sink.OnSpan(Record("Z", 2, 300));   // 200 ms later
    ASSERT_EQ(got.size(), 1u);
    EXPECT_EQ(got[0].count, 1u);
    EXPECT_EQ(got[0].spans[0].spanId, 1u);
}

TEST(SpanChunkSink, IgnoresSkippedSpansAndEmptyFlush)
{
    std::vector<SpanChunk> got;
    SpanChunkSink sink([&](const SpanChunk& c) { got.push_back(c); });
    sink.OnSpan(Record("Z", 0, 100));   // spanId 0: tracer skipped it
    sink.Flush();
    EXPECT_TRUE(got.empty());
}

TEST(SpanChunkSink, LongNameIsTruncatedAndTerminated)
{
    std::vector<SpanChunk> got;
    SpanChunkSink sink([&](const SpanChunk& c) { got.push_back(c); });
    sink.OnSpan(Record("0123456789012345678901234567890123456789012345678901234567890", 1, 100));
    sink.Flush();
    ASSERT_EQ(got.size(), 1u);
    EXPECT_EQ(std::strlen(got[0].spans[0].name), ChainSpan::kNameLength - 1u);
}

#include <gtest/gtest.h>
#include <DiaObservation/Trace/TraceFileSink.h>
#include <DiaObservation/Trace/SpanRecord.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstring>
#include <fstream>
#include <string>
#include <cstdio>

using namespace Dia::Observation::Trace;

namespace
{
    const char* kTraceFilePath = "test_trace_file_sink.jsonl";
}

struct TraceFileSinkTest : ::testing::Test
{
    void TearDown() override
    {
        remove(kTraceFilePath);
    }

    std::string ReadFile(const char* path)
    {
        std::ifstream f(path);
        if (!f.is_open()) return "";
        return std::string((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());
    }
};

TEST_F(TraceFileSinkTest, OnSpan_WritesValidJsonl)
{
    {
        TraceFileSink sink(kTraceFilePath, "sess-trace-001", 0);
        ASSERT_TRUE(sink.IsOpen());

        SpanRecord rec;
        std::memset(&rec, 0, sizeof(rec));
        rec.traceId = 0x123456789ABCDEF0ULL;
        rec.spanId = 0x00000000DEADBEEFULL;
        rec.parentSpanId = 0;
        rec.name = Dia::Core::StringCRC("TestSpan");
        rec.startSteadyNs = 1000000000ULL;
        rec.endSteadyNs = 2000000000ULL;
        rec.threadId = 42;
        rec.scenarioStep = Dia::Core::StringCRC("step1");

        sink.OnSpan(rec);
    }

    std::string content = ReadFile(kTraceFilePath);
    ASSERT_FALSE(content.empty());

    EXPECT_NE(content.find("\"thread_id\":42"), std::string::npos);
}

TEST_F(TraceFileSinkTest, HexIds_Are16CharZeroPadded)
{
    {
        TraceFileSink sink(kTraceFilePath, "sess", 0);
        ASSERT_TRUE(sink.IsOpen());

        SpanRecord rec;
        std::memset(&rec, 0, sizeof(rec));
        rec.traceId = 0x0000000000000001ULL;
        rec.spanId = 0x00000000000000FFULL;
        rec.parentSpanId = 0x0000000000001234ULL;
        rec.name = Dia::Core::StringCRC("Hex");
        rec.startSteadyNs = 100;
        rec.endSteadyNs = 200;
        rec.threadId = 1;

        sink.OnSpan(rec);
    }

    std::string content = ReadFile(kTraceFilePath);
    // traceId 1 → "0000000000000001"
    EXPECT_NE(content.find("\"trace_id\":\"0000000000000001\""), std::string::npos);
    // spanId 0xFF → "00000000000000ff"
    EXPECT_NE(content.find("\"span_id\":\"00000000000000ff\""), std::string::npos);
    // parentSpanId 0x1234 → "0000000000001234"
    EXPECT_NE(content.find("\"parent_span_id\":\"0000000000001234\""), std::string::npos);
}

TEST_F(TraceFileSinkTest, EpochOffset_AppliedToTimestamps)
{
    const int64_t kOffset = 5000000000LL; // 5 seconds
    {
        TraceFileSink sink(kTraceFilePath, "sess", kOffset);
        ASSERT_TRUE(sink.IsOpen());

        SpanRecord rec;
        std::memset(&rec, 0, sizeof(rec));
        rec.traceId = 1;
        rec.spanId = 2;
        rec.parentSpanId = 0;
        rec.name = Dia::Core::StringCRC("Epoch");
        rec.startSteadyNs = 1000000000ULL; // 1s steady
        rec.endSteadyNs = 2000000000ULL;   // 2s steady
        rec.threadId = 1;

        sink.OnSpan(rec);
    }

    std::string content = ReadFile(kTraceFilePath);
    // start = 1000000000 + 5000000000 = 6000000000
    EXPECT_NE(content.find("\"start_unix_nano\":6000000000"), std::string::npos);
    // end = 2000000000 + 5000000000 = 7000000000
    EXPECT_NE(content.find("\"end_unix_nano\":7000000000"), std::string::npos);
}

TEST_F(TraceFileSinkTest, MultipleSpans_EachOnOwnLine)
{
    {
        TraceFileSink sink(kTraceFilePath, "sess", 0);
        ASSERT_TRUE(sink.IsOpen());

        for (int i = 0; i < 3; ++i)
        {
            SpanRecord rec;
            std::memset(&rec, 0, sizeof(rec));
            rec.traceId = static_cast<uint64_t>(i + 1);
            rec.spanId = static_cast<uint64_t>(i + 100);
            rec.name = Dia::Core::StringCRC("Multi");
            rec.startSteadyNs = static_cast<uint64_t>(i * 100);
            rec.endSteadyNs = static_cast<uint64_t>(i * 100 + 50);
            rec.threadId = 1;
            sink.OnSpan(rec);
        }
    }

    std::string content = ReadFile(kTraceFilePath);
    // Count newlines — each span is one line
    int lineCount = 0;
    for (char c : content)
        if (c == '\n') ++lineCount;
    EXPECT_EQ(lineCount, 3);
}

TEST_F(TraceFileSinkTest, InvalidPath_IsOpenFalse)
{
    TraceFileSink sink("", "sess", 0);
    EXPECT_FALSE(sink.IsOpen());
}

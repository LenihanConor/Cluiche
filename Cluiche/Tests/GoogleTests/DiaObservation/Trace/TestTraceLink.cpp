////////////////////////////////////////////////////////////////////////////////
// Filename: TestTraceLink.cpp
// GoogleTest suite — Trace-Linked Streams Task 2: TraceLink, ScopedZone with
// link, and TraceFileSink link output (AC4 unit tests)
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaObservation/Trace/Tracer.h>
#include <DiaObservation/Trace/ScopedZone.h>
#include <DiaObservation/Trace/TraceCategory.h>
#include <DiaCore/CRC/StringCRC.h>

#include <fstream>
#include <string>
#include <thread>

using namespace Dia::Observation::Trace;
using namespace Dia::Core;

namespace {
    const char* kTestTraceFile = "test_trace_link.jsonl";

    void CleanupTestFile()
    {
        std::remove(kTestTraceFile);
    }

    std::string ReadFileContents(const char* path)
    {
        std::ifstream f(path);
        if (!f.is_open())
            return "";
        std::string contents((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
        return contents;
    }
}

// AC4: GetCurrentLink returns {0,0} when tracer is not started
TEST(TraceLink, GetCurrentLinkReturnsZeroWhenNotStarted)
{
    TraceLink link = Tracer::GetCurrentLink();
    EXPECT_EQ(link.traceId, 0u);
    EXPECT_EQ(link.spanId, 0u);
}

// AC4: GetCurrentLink returns {0,0} when no span is open
TEST(TraceLink, GetCurrentLinkReturnsZeroWhenNoSpanOpen)
{
    CleanupTestFile();
    Tracer& tracer = Tracer::Instance();
    ASSERT_TRUE(tracer.Start(kTestTraceFile, Category::kAll, "test-session", 0));
    tracer.RegisterThreadSpanBuffer();

    TraceLink link = Tracer::GetCurrentLink();
    EXPECT_EQ(link.traceId, 0u) << "No span open, link should be zero";
    EXPECT_EQ(link.spanId, 0u);

    tracer.Stop();
    CleanupTestFile();
}

// AC4: GetCurrentLink returns non-zero when a span is open
TEST(TraceLink, GetCurrentLinkReturnsNonZeroWhenSpanOpen)
{
    CleanupTestFile();
    Tracer& tracer = Tracer::Instance();
    ASSERT_TRUE(tracer.Start(kTestTraceFile, Category::kAll, "test-session", 0));
    tracer.RegisterThreadSpanBuffer();

    {
        ScopedZone zone(StringCRC("WriterSpan"), Category::kAll);
        TraceLink link = Tracer::GetCurrentLink();
        EXPECT_NE(link.traceId, 0u) << "Span is open, traceId should be non-zero";
        EXPECT_NE(link.spanId, 0u) << "Span is open, spanId should be non-zero";
    }

    tracer.Stop();
    CleanupTestFile();
}

// AC4: ScopedZone with link constructor records linkTraceId and linkSpanId
TEST(TraceLink, ScopedZoneWithLinkRecordsLinkFields)
{
    CleanupTestFile();
    Tracer& tracer = Tracer::Instance();
    ASSERT_TRUE(tracer.Start(kTestTraceFile, Category::kAll, "test-session", 0));
    tracer.RegisterThreadSpanBuffer();

    uint64_t writerTraceId = 0;
    uint64_t writerSpanId = 0;

    // Writer thread opens a span and captures its link
    {
        ScopedZone writerZone(StringCRC("WriterSpan"), Category::kAll);
        TraceLink link = Tracer::GetCurrentLink();
        writerTraceId = link.traceId;
        writerSpanId = link.spanId;
        ASSERT_NE(writerTraceId, 0u);
        ASSERT_NE(writerSpanId, 0u);
    }

    // Reader thread opens a span with the writer's link
    {
        TraceLink writerLink = { writerTraceId, writerSpanId };
        ScopedZone readerZone(StringCRC("ReaderSpan"), Category::kAll, writerLink);
        // The reader's span should have its own traceId/spanId but also record the link
    }

    tracer.Stop();

    // Verify the trace file contains link_trace_id and link_span_id for ReaderSpan
    std::string contents = ReadFileContents(kTestTraceFile);
    ASSERT_FALSE(contents.empty()) << "Trace file should not be empty";

    // The ReaderSpan line should have link_trace_id and link_span_id
    EXPECT_NE(contents.find("\"name\":\"ReaderSpan\""), std::string::npos);
    EXPECT_NE(contents.find("\"link_trace_id\":"), std::string::npos)
        << "ReaderSpan should have link_trace_id in trace file";
    EXPECT_NE(contents.find("\"link_span_id\":"), std::string::npos)
        << "ReaderSpan should have link_span_id in trace file";

    // WriterSpan should NOT have link fields (no link was passed)
    size_t writerPos = contents.find("\"name\":\"WriterSpan\"");
    ASSERT_NE(writerPos, std::string::npos);
    size_t nextLinePos = contents.find('\n', writerPos);
    std::string writerLine = contents.substr(writerPos, nextLinePos - writerPos);
    EXPECT_EQ(writerLine.find("\"link_trace_id\":"), std::string::npos)
        << "WriterSpan should NOT have link_trace_id";

    CleanupTestFile();
}

// AC4: Link survives across threads (two-thread test)
TEST(TraceLink, LinkSurvivesAcrossThreads)
{
    CleanupTestFile();
    Tracer& tracer = Tracer::Instance();
    ASSERT_TRUE(tracer.Start(kTestTraceFile, Category::kAll, "test-session", 0));
    tracer.RegisterThreadSpanBuffer();

    uint64_t writerTraceId = 0;
    uint64_t writerSpanId = 0;

    // Main thread: writer span
    {
        ScopedZone writerZone(StringCRC("MainThreadWriter"), Category::kAll);
        TraceLink link = Tracer::GetCurrentLink();
        writerTraceId = link.traceId;
        writerSpanId = link.spanId;
    }

    // Worker thread: reader span with link
    std::thread worker([&]() {
        tracer.RegisterThreadSpanBuffer();
        TraceLink writerLink = { writerTraceId, writerSpanId };
        {
            ScopedZone readerZone(StringCRC("WorkerThreadReader"), Category::kAll, writerLink);
            // Reader span records the link
        }
    });
    worker.join();

    tracer.Stop();

    // Verify the trace file shows both spans and the reader has a link
    std::string contents = ReadFileContents(kTestTraceFile);
    EXPECT_NE(contents.find("\"name\":\"MainThreadWriter\""), std::string::npos);
    EXPECT_NE(contents.find("\"name\":\"WorkerThreadReader\""), std::string::npos);

    size_t readerPos = contents.find("\"name\":\"WorkerThreadReader\"");
    ASSERT_NE(readerPos, std::string::npos);
    size_t nextLinePos = contents.find('\n', readerPos);
    std::string readerLine = contents.substr(readerPos, nextLinePos - readerPos);
    EXPECT_NE(readerLine.find("\"link_trace_id\":"), std::string::npos);
    EXPECT_NE(readerLine.find("\"link_span_id\":"), std::string::npos);

    CleanupTestFile();
}

// AC4: No link recorded when tracer is off
TEST(TraceLink, NoLinkRecordedWhenTracerOff)
{
    TraceLink link = { 0x1234567890abcdefULL, 0xfedcba0987654321ULL };

    // ScopedZone with link, but tracer is not started
    {
        ScopedZone zone(StringCRC("NoTracerSpan"), Category::kAll, link);
        // Nothing should crash; the span is a no-op
    }

    // No file written, no crash
    SUCCEED();
}

// AC4: Link with zero values is treated as no link (not written to file)
TEST(TraceLink, ZeroLinkNotWrittenToFile)
{
    CleanupTestFile();
    Tracer& tracer = Tracer::Instance();
    ASSERT_TRUE(tracer.Start(kTestTraceFile, Category::kAll, "test-session", 0));
    tracer.RegisterThreadSpanBuffer();

    {
        TraceLink zeroLink = { 0, 0 };
        ScopedZone zone(StringCRC("ZeroLinkSpan"), Category::kAll, zeroLink);
    }

    tracer.Stop();

    std::string contents = ReadFileContents(kTestTraceFile);
    ASSERT_NE(contents.find("\"name\":\"ZeroLinkSpan\""), std::string::npos);

    // Zero link should NOT write link fields to the file
    size_t spanPos = contents.find("\"name\":\"ZeroLinkSpan\"");
    size_t nextLinePos = contents.find('\n', spanPos);
    std::string spanLine = contents.substr(spanPos, nextLinePos - spanPos);
    EXPECT_EQ(spanLine.find("\"link_trace_id\":"), std::string::npos)
        << "Zero link should not write link_trace_id";
    EXPECT_EQ(spanLine.find("\"link_span_id\":"), std::string::npos)
        << "Zero link should not write link_span_id";

    CleanupTestFile();
}

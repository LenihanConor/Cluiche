#include <gtest/gtest.h>

#include <DiaLogger/Logger.h>
#include <DiaLogger/ISink.h>
#include <DiaLogger/LogEntry.h>
#include <DiaLogger/LogLevel.h>
#include <DiaCore/CRC/StringCRC.h>

#include <string.h>

using namespace Dia::Logger;

// ==============================================================================
// Test sink that captures entries
// ==============================================================================

class CaptureSink : public ISink
{
public:
	static const unsigned int kMaxEntries = 128;

	CaptureSink() : mEntryCount(0) {}

	void OnLogEntry(const LogEntry& entry) override
	{
		if (mEntryCount < kMaxEntries)
			mEntries[mEntryCount++] = entry;
	}

	const char* GetName() const override { return "CaptureSink"; }

	unsigned int GetEntryCount() const { return mEntryCount; }
	const LogEntry& GetEntry(unsigned int i) const { return mEntries[i]; }

	void Clear() { mEntryCount = 0; }

private:
	LogEntry mEntries[kMaxEntries];
	unsigned int mEntryCount;
};

// ==============================================================================
// Fixture — registers/unregisters thread buffer and sink per test
// ==============================================================================

class LoggerTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		mSink = new CaptureSink();
		mSink->SetLevelThreshold(LogLevel::kTrace);

		Logger::Instance().RegisterThreadBuffer();
		Logger::Instance().RegisterSink(mSink);
	}

	void TearDown() override
	{
		Logger::Instance().UnregisterSink(mSink);
		Logger::Instance().UnregisterThreadBuffer();
		delete mSink;
		mSink = nullptr;
	}

	CaptureSink* mSink;
};

// ==============================================================================
// End-to-End: Log → FlushBuffers → Sink
// ==============================================================================

TEST_F(LoggerTest, LogAndFlush_SinkReceivesEntry)
{
	Logger::Instance().Log(LogLevel::kInfo, Dia::Core::StringCRC("Test"),
		"hello world");

	Logger::Instance().FlushBuffers();

	EXPECT_EQ(1u, mSink->GetEntryCount());
	EXPECT_EQ(LogLevel::kInfo, mSink->GetEntry(0).level);
	EXPECT_STREQ("hello world", mSink->GetEntry(0).message);
}

TEST_F(LoggerTest, LogAndFlush_ChannelPreserved)
{
	Dia::Core::StringCRC channel("Physics");
	Logger::Instance().Log(LogLevel::kWarning, channel, "body fell through floor");

	Logger::Instance().FlushBuffers();

	ASSERT_EQ(1u, mSink->GetEntryCount());
	EXPECT_EQ(channel, mSink->GetEntry(0).channel);
}

TEST_F(LoggerTest, LogAndFlush_FormatString)
{
	Logger::Instance().Log(LogLevel::kInfo, Dia::Core::StringCRC("Test"),
		"value=%d name=%s", 42, "foo");

	Logger::Instance().FlushBuffers();

	ASSERT_EQ(1u, mSink->GetEntryCount());
	EXPECT_STREQ("value=42 name=foo", mSink->GetEntry(0).message);
}

TEST_F(LoggerTest, MultipleEntries_AllFlushed)
{
	for (int i = 0; i < 10; ++i)
	{
		Logger::Instance().Log(LogLevel::kInfo, Dia::Core::StringCRC("Test"),
			"msg_%d", i);
	}

	Logger::Instance().FlushBuffers();

	EXPECT_EQ(10u, mSink->GetEntryCount());
	EXPECT_STREQ("msg_0", mSink->GetEntry(0).message);
	EXPECT_STREQ("msg_9", mSink->GetEntry(9).message);
}

TEST_F(LoggerTest, FlushWithEmptyBuffer_NoEntries)
{
	Logger::Instance().FlushBuffers();
	EXPECT_EQ(0u, mSink->GetEntryCount());
}

TEST_F(LoggerTest, DoubleFlush_SecondFlushEmpty)
{
	Logger::Instance().Log(LogLevel::kInfo, Dia::Core::StringCRC("Test"), "one");

	Logger::Instance().FlushBuffers();
	EXPECT_EQ(1u, mSink->GetEntryCount());

	mSink->Clear();
	Logger::Instance().FlushBuffers();
	EXPECT_EQ(0u, mSink->GetEntryCount());
}

TEST_F(LoggerTest, SinkFilterRespected_BelowThresholdNotDelivered)
{
	mSink->SetLevelThreshold(LogLevel::kError);

	Logger::Instance().Log(LogLevel::kInfo, Dia::Core::StringCRC("Test"), "low");
	Logger::Instance().Log(LogLevel::kWarning, Dia::Core::StringCRC("Test"), "medium");
	Logger::Instance().Log(LogLevel::kError, Dia::Core::StringCRC("Test"), "high");

	Logger::Instance().FlushBuffers();

	EXPECT_EQ(1u, mSink->GetEntryCount());
	EXPECT_STREQ("high", mSink->GetEntry(0).message);
}

// ==============================================================================
// Edge Cases
// ==============================================================================

TEST(TestLoggerEdge, FlushWithNoSinks_NoCrash)
{
	Logger::Instance().RegisterThreadBuffer();

	Logger::Instance().Log(LogLevel::kInfo, Dia::Core::StringCRC("Test"), "orphan");
	Logger::Instance().FlushBuffers();

	Logger::Instance().UnregisterThreadBuffer();
}

TEST(TestLoggerEdge, LogWithoutRegisteredBuffer_NoCrash)
{
	Logger::Instance().Log(LogLevel::kInfo, Dia::Core::StringCRC("Test"),
		"no buffer registered");
}

TEST(TestLoggerEdge, FlushWithEmptyBuffersAndNoSinks_NoCrash)
{
	Logger::Instance().FlushBuffers();
}

TEST(TestLoggerEdge, MultipleSinks_AllReceiveEntries)
{
	CaptureSink sinkA;
	sinkA.SetLevelThreshold(LogLevel::kTrace);
	CaptureSink sinkB;
	sinkB.SetLevelThreshold(LogLevel::kTrace);

	Logger::Instance().RegisterThreadBuffer();
	Logger::Instance().RegisterSink(&sinkA);
	Logger::Instance().RegisterSink(&sinkB);

	Logger::Instance().Log(LogLevel::kInfo, Dia::Core::StringCRC("Test"), "broadcast");
	Logger::Instance().FlushBuffers();

	EXPECT_EQ(1u, sinkA.GetEntryCount());
	EXPECT_EQ(1u, sinkB.GetEntryCount());
	EXPECT_STREQ("broadcast", sinkA.GetEntry(0).message);
	EXPECT_STREQ("broadcast", sinkB.GetEntry(0).message);

	Logger::Instance().UnregisterSink(&sinkA);
	Logger::Instance().UnregisterSink(&sinkB);
	Logger::Instance().UnregisterThreadBuffer();
}

TEST(TestLoggerEdge, UnregisterSink_StopsReceiving)
{
	CaptureSink sink;
	sink.SetLevelThreshold(LogLevel::kTrace);

	Logger::Instance().RegisterThreadBuffer();
	Logger::Instance().RegisterSink(&sink);

	Logger::Instance().Log(LogLevel::kInfo, Dia::Core::StringCRC("Test"), "before");
	Logger::Instance().FlushBuffers();
	EXPECT_EQ(1u, sink.GetEntryCount());

	Logger::Instance().UnregisterSink(&sink);
	sink.Clear();

	Logger::Instance().Log(LogLevel::kInfo, Dia::Core::StringCRC("Test"), "after");
	Logger::Instance().FlushBuffers();
	EXPECT_EQ(0u, sink.GetEntryCount());

	Logger::Instance().UnregisterThreadBuffer();
}

TEST(TestLoggerEdge, RegisterSameSinkTwice_OnlyOneDelivery)
{
	CaptureSink sink;
	sink.SetLevelThreshold(LogLevel::kTrace);

	Logger::Instance().RegisterThreadBuffer();
	Logger::Instance().RegisterSink(&sink);
	Logger::Instance().RegisterSink(&sink);

	Logger::Instance().Log(LogLevel::kInfo, Dia::Core::StringCRC("Test"), "once");
	Logger::Instance().FlushBuffers();

	EXPECT_EQ(1u, sink.GetEntryCount());

	Logger::Instance().UnregisterSink(&sink);
	Logger::Instance().UnregisterThreadBuffer();
}

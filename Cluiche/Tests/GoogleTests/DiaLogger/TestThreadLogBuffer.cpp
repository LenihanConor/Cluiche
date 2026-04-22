#include <gtest/gtest.h>

#include <DiaLogger/ThreadLogBuffer.h>
#include <DiaLogger/LogEntry.h>
#include <DiaLogger/LogLevel.h>
#include <DiaCore/CRC/StringCRC.h>

#include <string.h>
#include <memory>

using namespace Dia::Logger;

// ==============================================================================
// Fixture — heap-allocates ThreadLogBuffer (1MB, too large for stack)
// ==============================================================================

class ThreadLogBufferTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		mBuffer = std::make_unique<ThreadLogBuffer>();
	}

	std::unique_ptr<ThreadLogBuffer> mBuffer;
};

// ==============================================================================
// ThreadLogBuffer Tests
// ==============================================================================

TEST_F(ThreadLogBufferTest, InitialState_IsEmpty)
{
	EXPECT_TRUE(mBuffer->IsEmpty());
	EXPECT_EQ(0u, mBuffer->PendingCount());
}

TEST_F(ThreadLogBufferTest, InitialState_PopReturnsFalse)
{
	LogEntry out;
	EXPECT_FALSE(mBuffer->Pop(out));
}

TEST_F(ThreadLogBufferTest, PushOneEntry_PendingCountIsOne)
{
	LogEntry entry;
	entry.level = LogLevel::kInfo;
	entry.channel = Dia::Core::StringCRC("Test");
	strncpy_s(entry.message, sizeof(entry.message), "hello", _TRUNCATE);

	mBuffer->Push(entry);

	EXPECT_FALSE(mBuffer->IsEmpty());
	EXPECT_EQ(1u, mBuffer->PendingCount());
}

TEST_F(ThreadLogBufferTest, PushThenPop_ReturnsCorrectEntry)
{
	LogEntry entry;
	entry.level = LogLevel::kWarning;
	entry.channel = Dia::Core::StringCRC("Physics");
	strncpy_s(entry.message, sizeof(entry.message), "collision detected", _TRUNCATE);

	mBuffer->Push(entry);

	LogEntry out;
	EXPECT_TRUE(mBuffer->Pop(out));
	EXPECT_EQ(LogLevel::kWarning, out.level);
	EXPECT_EQ(Dia::Core::StringCRC("Physics"), out.channel);
	EXPECT_STREQ("collision detected", out.message);
}

TEST_F(ThreadLogBufferTest, PopDrainsBuffer)
{
	LogEntry entry;
	entry.level = LogLevel::kInfo;
	entry.channel = Dia::Core::StringCRC("Test");
	strncpy_s(entry.message, sizeof(entry.message), "msg", _TRUNCATE);

	mBuffer->Push(entry);

	LogEntry out;
	mBuffer->Pop(out);

	EXPECT_TRUE(mBuffer->IsEmpty());
	EXPECT_EQ(0u, mBuffer->PendingCount());
	EXPECT_FALSE(mBuffer->Pop(out));
}

TEST_F(ThreadLogBufferTest, MultiplePushPop_FIFOOrder)
{
	for (int i = 0; i < 5; ++i)
	{
		LogEntry entry;
		entry.level = LogLevel::kInfo;
		entry.channel = Dia::Core::StringCRC("Test");
		snprintf(entry.message, sizeof(entry.message), "msg_%d", i);
		mBuffer->Push(entry);
	}

	EXPECT_EQ(5u, mBuffer->PendingCount());

	for (int i = 0; i < 5; ++i)
	{
		LogEntry out;
		EXPECT_TRUE(mBuffer->Pop(out));
		char expected[32];
		snprintf(expected, sizeof(expected), "msg_%d", i);
		EXPECT_STREQ(expected, out.message);
	}

	EXPECT_TRUE(mBuffer->IsEmpty());
}

TEST_F(ThreadLogBufferTest, RingWrap_DropsOldestEntries)
{
	for (unsigned int i = 0; i < ThreadLogBuffer::kCapacity + 10; ++i)
	{
		LogEntry entry;
		entry.level = LogLevel::kInfo;
		entry.channel = Dia::Core::StringCRC("Test");
		snprintf(entry.message, sizeof(entry.message), "entry_%u", i);
		mBuffer->Push(entry);
	}

	EXPECT_EQ(ThreadLogBuffer::kCapacity, mBuffer->PendingCount());

	LogEntry first;
	EXPECT_TRUE(mBuffer->Pop(first));

	char expected[32];
	snprintf(expected, sizeof(expected), "entry_%u", 10u);
	EXPECT_STREQ(expected, first.message);
}

TEST_F(ThreadLogBufferTest, Reset_ClearsBuffer)
{
	LogEntry entry;
	entry.level = LogLevel::kInfo;
	entry.channel = Dia::Core::StringCRC("Test");
	strncpy_s(entry.message, sizeof(entry.message), "will be cleared", _TRUNCATE);

	mBuffer->Push(entry);
	mBuffer->Push(entry);
	mBuffer->Push(entry);

	EXPECT_EQ(3u, mBuffer->PendingCount());

	mBuffer->Reset();

	EXPECT_TRUE(mBuffer->IsEmpty());
	EXPECT_EQ(0u, mBuffer->PendingCount());
}

TEST_F(ThreadLogBufferTest, FillExactlyToCapacity_AllReadable)
{
	for (unsigned int i = 0; i < ThreadLogBuffer::kCapacity; ++i)
	{
		LogEntry entry;
		entry.level = LogLevel::kInfo;
		entry.channel = Dia::Core::StringCRC("Test");
		snprintf(entry.message, sizeof(entry.message), "e%u", i);
		mBuffer->Push(entry);
	}

	EXPECT_EQ(ThreadLogBuffer::kCapacity, mBuffer->PendingCount());

	LogEntry first;
	EXPECT_TRUE(mBuffer->Pop(first));
	EXPECT_STREQ("e0", first.message);
}

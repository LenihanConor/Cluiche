////////////////////////////////////////////////////////////////////////////////
// Filename: TestConsoleLogRing.cpp
// Description: Unit tests for ConsoleLogRing (AC-7, AC-8).
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-console.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <DiaVisualDebugger/Console/ConsoleLogRing.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Log/Logger.h>
#include <DiaCore/CRC/StringCRC.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace Dia::VisualDebugger::Console;
using Dia::Observation::Log::LogEntry;
using Dia::Observation::Log::LogLevel;
using Dia::Core::StringCRC;

namespace
{
	LogEntry MakeEntry(const char* message, uint64_t timestampNs, LogLevel level = LogLevel::kInfo, const char* channel = "Test")
	{
		LogEntry entry{};
		entry.level = level;
		entry.channel = StringCRC(channel);
		std::snprintf(entry.message, sizeof(entry.message), "%s", message);
		entry.timestampNs = timestampNs;
		entry.threadId = 0;
		return entry;
	}

	void WriteNumbered(ConsoleLogRing& ring, unsigned int first, unsigned int count)
	{
		for (unsigned int i = first; i < first + count; ++i)
		{
			char msg[32];
			std::snprintf(msg, sizeof(msg), "line %u", i);
			ring.OnLogEntry(MakeEntry(msg, i));
		}
	}

	std::vector<ConsoleLogLine> DrainAll(const ConsoleLogRing& ring, uint64_t& cursor)
	{
		std::vector<ConsoleLogLine> lines(ConsoleLogRing::kCapacity);
		unsigned int n = ring.DrainSince(cursor, lines.data(), static_cast<unsigned int>(lines.size()));
		lines.resize(n);
		return lines;
	}
}

TEST(ConsoleLogRing, StoresFieldsOfOneLine)
{
	auto ring = std::make_unique<ConsoleLogRing>();
	ring->OnLogEntry(MakeEntry("hello", 1234, LogLevel::kWarning, "Debug"));

	uint64_t cursor = 0;
	auto lines = DrainAll(*ring, cursor);
	ASSERT_EQ(lines.size(), 1u);
	EXPECT_EQ(lines[0].sequence, 0u);
	EXPECT_EQ(lines[0].timestampNs, 1234u);
	EXPECT_EQ(lines[0].level, LogLevel::kWarning);
	EXPECT_EQ(lines[0].channel, StringCRC("Debug"));
	EXPECT_STREQ(lines[0].channel.AsChar(), "Debug");
	EXPECT_STREQ(lines[0].message, "hello");
	EXPECT_EQ(cursor, 1u);
	EXPECT_STREQ(ring->GetName(), "ConsoleLogRing");
}

TEST(ConsoleLogRing, MessageTruncatedTo255)
{
	auto ring = std::make_unique<ConsoleLogRing>();
	std::string longMessage(1000, 'm');
	ring->OnLogEntry(MakeEntry(longMessage.c_str(), 1));

	uint64_t cursor = 0;
	auto lines = DrainAll(*ring, cursor);
	ASSERT_EQ(lines.size(), 1u);
	EXPECT_EQ(std::strlen(lines[0].message), static_cast<size_t>(ConsoleLogLine::kMaxMessageLength));
}

TEST(ConsoleLogRing, AC7_KeepsLast500InOrder)
{
	auto ring = std::make_unique<ConsoleLogRing>();
	WriteNumbered(*ring, 0, 1234);

	EXPECT_EQ(ring->GetTotalWritten(), 1234u);
	EXPECT_EQ(ring->GetCount(), ConsoleLogRing::kCapacity);

	uint64_t cursor = 0;
	auto lines = DrainAll(*ring, cursor);
	ASSERT_EQ(lines.size(), static_cast<size_t>(ConsoleLogRing::kCapacity));
	for (unsigned int i = 0; i < lines.size(); ++i)
	{
		const unsigned int expected = 1234 - ConsoleLogRing::kCapacity + i;
		char msg[32];
		std::snprintf(msg, sizeof(msg), "line %u", expected);
		EXPECT_EQ(lines[i].sequence, expected);
		EXPECT_EQ(lines[i].timestampNs, expected);
		EXPECT_STREQ(lines[i].message, msg);
	}
	EXPECT_EQ(cursor, 1234u);
}

TEST(ConsoleLogRing, AC8_DrainSinceReturnsOnlyNewLines)
{
	auto ring = std::make_unique<ConsoleLogRing>();
	uint64_t cursor = 0;

	EXPECT_EQ(DrainAll(*ring, cursor).size(), 0u);
	EXPECT_EQ(cursor, 0u);

	WriteNumbered(*ring, 0, 3);
	auto first = DrainAll(*ring, cursor);
	ASSERT_EQ(first.size(), 3u);
	EXPECT_STREQ(first[2].message, "line 2");
	EXPECT_EQ(cursor, 3u);

	EXPECT_EQ(DrainAll(*ring, cursor).size(), 0u);

	WriteNumbered(*ring, 3, 2);
	auto second = DrainAll(*ring, cursor);
	ASSERT_EQ(second.size(), 2u);
	EXPECT_STREQ(second[0].message, "line 3");
	EXPECT_STREQ(second[1].message, "line 4");
	EXPECT_EQ(cursor, 5u);
}

TEST(ConsoleLogRing, AC8_DrainSinceSurvivesWrapAround)
{
	auto ring = std::make_unique<ConsoleLogRing>();
	uint64_t cursor = 0;

	WriteNumbered(*ring, 0, 100);
	ASSERT_EQ(DrainAll(*ring, cursor).size(), 100u);

	// Writes across the wrap point while the reader keeps up: nothing lost.
	WriteNumbered(*ring, 100, 450);
	auto lines = DrainAll(*ring, cursor);
	ASSERT_EQ(lines.size(), 450u);
	EXPECT_STREQ(lines.front().message, "line 100");
	EXPECT_STREQ(lines.back().message, "line 549");
	EXPECT_EQ(cursor, 550u);

	// Reader falls behind by more than the capacity: starts at the oldest kept line.
	WriteNumbered(*ring, 550, 1200);
	lines = DrainAll(*ring, cursor);
	ASSERT_EQ(lines.size(), static_cast<size_t>(ConsoleLogRing::kCapacity));
	EXPECT_EQ(lines.front().sequence, 1750u - ConsoleLogRing::kCapacity);
	EXPECT_STREQ(lines.back().message, "line 1749");
	for (size_t i = 1; i < lines.size(); ++i)
	{
		EXPECT_EQ(lines[i].sequence, lines[i - 1].sequence + 1);
	}
	EXPECT_EQ(cursor, 1750u);
}

TEST(ConsoleLogRing, DrainSinceRespectsMaxLines)
{
	auto ring = std::make_unique<ConsoleLogRing>();
	WriteNumbered(*ring, 0, 10);

	uint64_t cursor = 0;
	ConsoleLogLine out[4];
	ASSERT_EQ(ring->DrainSince(cursor, out, 4), 4u);
	EXPECT_STREQ(out[3].message, "line 3");
	ASSERT_EQ(ring->DrainSince(cursor, out, 4), 4u);
	EXPECT_STREQ(out[0].message, "line 4");
	ASSERT_EQ(ring->DrainSince(cursor, out, 4), 2u);
	EXPECT_STREQ(out[1].message, "line 9");
	EXPECT_EQ(ring->DrainSince(cursor, out, 4), 0u);
	EXPECT_EQ(ring->DrainSince(cursor, nullptr, 0), 0u);
}

TEST(ConsoleLogRing, DrainSinceClampsCursorAhead)
{
	auto ring = std::make_unique<ConsoleLogRing>();
	WriteNumbered(*ring, 0, 5);

	uint64_t cursor = 999;
	EXPECT_EQ(DrainAll(*ring, cursor).size(), 0u);
	EXPECT_EQ(cursor, 5u);

	WriteNumbered(*ring, 5, 1);
	auto lines = DrainAll(*ring, cursor);
	ASSERT_EQ(lines.size(), 1u);
	EXPECT_STREQ(lines[0].message, "line 5");
}

TEST(ConsoleLogRing, AC7_MultiThreadWritersNoTornLines)
{
	auto ring = std::make_unique<ConsoleLogRing>();
	const unsigned int kThreads = 4;
	const unsigned int kPerThread = 1000;

	// A concurrent reader drains while the writers run.
	std::vector<ConsoleLogLine> drained;
	std::atomic<bool> done(false);
	std::thread reader([&]()
	{
		uint64_t cursor = 0;
		std::vector<ConsoleLogLine> buffer(64);
		while (!done.load())
		{
			unsigned int n = ring->DrainSince(cursor, buffer.data(), static_cast<unsigned int>(buffer.size()));
			drained.insert(drained.end(), buffer.begin(), buffer.begin() + n);
		}
	});

	std::vector<std::thread> writers;
	for (unsigned int t = 0; t < kThreads; ++t)
	{
		writers.emplace_back([&ring, t]()
		{
			const char fill = static_cast<char>('a' + t);
			for (unsigned int i = 0; i < kPerThread; ++i)
			{
				// Self-consistent line: header, then a fill char repeated (i % 200) times.
				char msg[300];
				int len = std::snprintf(msg, sizeof(msg), "t%u i%u ", t, i);
				const unsigned int fillCount = i % 200;
				for (unsigned int f = 0; f < fillCount; ++f)
				{
					msg[len + f] = fill;
				}
				msg[len + fillCount] = '\0';
				ring->OnLogEntry(MakeEntry(msg, static_cast<uint64_t>(t) * 1000000u + i, static_cast<LogLevel>(t % 5)));
			}
		});
	}
	for (auto& w : writers)
	{
		w.join();
	}
	done.store(true);
	reader.join();

	EXPECT_EQ(ring->GetTotalWritten(), static_cast<uint64_t>(kThreads * kPerThread));

	auto checkLine = [](const ConsoleLogLine& line, unsigned int lastIndexPerThread[], bool seen[])
	{
		unsigned int t = 0, i = 0;
		int consumed = 0;
		ASSERT_EQ(std::sscanf(line.message, "t%u i%u %n", &t, &i, &consumed), 2) << line.message;
		ASSERT_LT(t, 4u);
		EXPECT_EQ(line.timestampNs, static_cast<uint64_t>(t) * 1000000u + i);
		EXPECT_EQ(line.level, static_cast<LogLevel>(t % 5));
		const char* fill = line.message + consumed;
		EXPECT_EQ(std::strlen(fill), static_cast<size_t>(i % 200)) << line.message;
		for (const char* p = fill; *p != '\0'; ++p)
		{
			EXPECT_EQ(*p, static_cast<char>('a' + t)) << line.message;
		}
		if (seen[t])
		{
			EXPECT_GT(i, lastIndexPerThread[t]) << "out of order for thread " << t;
		}
		seen[t] = true;
		lastIndexPerThread[t] = i;
	};

	// Final contents: last 500 lines, consecutive sequences, per-thread order kept.
	uint64_t cursor = 0;
	auto lines = DrainAll(*ring, cursor);
	ASSERT_EQ(lines.size(), static_cast<size_t>(ConsoleLogRing::kCapacity));
	unsigned int last[4] = {};
	bool seen[4] = {};
	for (size_t k = 0; k < lines.size(); ++k)
	{
		if (k > 0)
		{
			EXPECT_EQ(lines[k].sequence, lines[k - 1].sequence + 1);
		}
		checkLine(lines[k], last, seen);
	}

	// Everything the concurrent reader saw is also untorn and ordered.
	unsigned int lastR[4] = {};
	bool seenR[4] = {};
	for (size_t k = 0; k < drained.size(); ++k)
	{
		if (k > 0)
		{
			EXPECT_GT(drained[k].sequence, drained[k - 1].sequence);
		}
		checkLine(drained[k], lastR, seenR);
	}
}

TEST(ConsoleLogRing, AC7_ReceivesLinesAsLoggerSink)
{
	auto ring = std::make_unique<ConsoleLogRing>();
	Dia::Observation::Log::Logger& logger = Dia::Observation::Log::Logger::Instance();
	logger.RegisterThreadBuffer();	// Log() drops lines from threads without a buffer
	logger.FlushSync();
	logger.RegisterSink(ring.get());

	DIA_LOG_INFO("Debug", "console ring probe %d", 42);
	logger.FlushSync();

	logger.UnregisterSink(ring.get());
	logger.UnregisterThreadBuffer();

	uint64_t cursor = 0;
	auto lines = DrainAll(*ring, cursor);
	bool found = false;
	for (const auto& line : lines)
	{
		if (std::strcmp(line.message, "console ring probe 42") == 0)
		{
			found = true;
			EXPECT_EQ(line.level, LogLevel::kInfo);
			EXPECT_EQ(line.channel, StringCRC("Debug"));
		}
	}
	EXPECT_TRUE(found) << "ring did not receive the line (Logger sink table full?)";
}

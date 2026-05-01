#include <gtest/gtest.h>

#include <DiaLogger/ISink.h>
#include <DiaLogger/LogEntry.h>
#include <DiaLogger/LogLevel.h>
#include <DiaCore/CRC/StringCRC.h>

#include <string.h>

using namespace Dia::Logger;

// ==============================================================================
// Test sink that records entries for verification
// ==============================================================================

class TestSink : public ISink
{
public:
	static const unsigned int kMaxEntries = 64;

	TestSink() : mEntryCount(0) {}

	void OnLogEntry(const LogEntry& entry) override
	{
		if (mEntryCount < kMaxEntries)
			mEntries[mEntryCount++] = entry;
	}

	const char* GetName() const override { return "TestSink"; }

	unsigned int GetEntryCount() const { return mEntryCount; }
	const LogEntry& GetEntry(unsigned int i) const { return mEntries[i]; }

	void Clear() { mEntryCount = 0; }

private:
	LogEntry mEntries[kMaxEntries];
	unsigned int mEntryCount;
};

// ==============================================================================
// Helper to create a LogEntry
// ==============================================================================

static LogEntry MakeEntry(LogLevel level, const char* channel, const char* message)
{
	LogEntry entry;
	entry.level = level;
	entry.channel = Dia::Core::StringCRC(channel);
	strncpy_s(entry.message, sizeof(entry.message), message, _TRUNCATE);
	return entry;
}

// ==============================================================================
// ISink Level Threshold Tests
// ==============================================================================

TEST(TestISink, DefaultThreshold_IsInfo)
{
	TestSink sink;
	EXPECT_EQ(LogLevel::kInfo, sink.GetLevelThreshold());
}

TEST(TestISink, SetLevelThreshold_Changes)
{
	TestSink sink;
	sink.SetLevelThreshold(LogLevel::kWarning);
	EXPECT_EQ(LogLevel::kWarning, sink.GetLevelThreshold());
}

TEST(TestISink, AcceptsEntry_AtThreshold)
{
	TestSink sink;
	sink.SetLevelThreshold(LogLevel::kWarning);

	LogEntry entry = MakeEntry(LogLevel::kWarning, "Test", "at threshold");
	EXPECT_TRUE(sink.AcceptsEntry(entry));
}

TEST(TestISink, AcceptsEntry_AboveThreshold)
{
	TestSink sink;
	sink.SetLevelThreshold(LogLevel::kWarning);

	LogEntry entry = MakeEntry(LogLevel::kError, "Test", "above threshold");
	EXPECT_TRUE(sink.AcceptsEntry(entry));
}

TEST(TestISink, RejectsEntry_BelowThreshold)
{
	TestSink sink;
	sink.SetLevelThreshold(LogLevel::kWarning);

	LogEntry entry = MakeEntry(LogLevel::kInfo, "Test", "below threshold");
	EXPECT_FALSE(sink.AcceptsEntry(entry));
}

TEST(TestISink, RejectsTrace_WithDefaultThreshold)
{
	TestSink sink;

	LogEntry entry = MakeEntry(LogLevel::kTrace, "Test", "trace msg");
	EXPECT_FALSE(sink.AcceptsEntry(entry));
}

TEST(TestISink, RejectsDebug_WithDefaultThreshold)
{
	TestSink sink;

	LogEntry entry = MakeEntry(LogLevel::kDebug, "Test", "debug msg");
	EXPECT_FALSE(sink.AcceptsEntry(entry));
}

TEST(TestISink, AcceptsInfo_WithDefaultThreshold)
{
	TestSink sink;

	LogEntry entry = MakeEntry(LogLevel::kInfo, "Test", "info msg");
	EXPECT_TRUE(sink.AcceptsEntry(entry));
}

TEST(TestISink, ThresholdTrace_AcceptsAll)
{
	TestSink sink;
	sink.SetLevelThreshold(LogLevel::kTrace);

	EXPECT_TRUE(sink.AcceptsEntry(MakeEntry(LogLevel::kTrace, "Test", "t")));
	EXPECT_TRUE(sink.AcceptsEntry(MakeEntry(LogLevel::kDebug, "Test", "d")));
	EXPECT_TRUE(sink.AcceptsEntry(MakeEntry(LogLevel::kInfo, "Test", "i")));
	EXPECT_TRUE(sink.AcceptsEntry(MakeEntry(LogLevel::kWarning, "Test", "w")));
	EXPECT_TRUE(sink.AcceptsEntry(MakeEntry(LogLevel::kError, "Test", "e")));
}

TEST(TestISink, ThresholdError_RejectsAllBelow)
{
	TestSink sink;
	sink.SetLevelThreshold(LogLevel::kError);

	EXPECT_FALSE(sink.AcceptsEntry(MakeEntry(LogLevel::kTrace, "Test", "t")));
	EXPECT_FALSE(sink.AcceptsEntry(MakeEntry(LogLevel::kDebug, "Test", "d")));
	EXPECT_FALSE(sink.AcceptsEntry(MakeEntry(LogLevel::kInfo, "Test", "i")));
	EXPECT_FALSE(sink.AcceptsEntry(MakeEntry(LogLevel::kWarning, "Test", "w")));
	EXPECT_TRUE(sink.AcceptsEntry(MakeEntry(LogLevel::kError, "Test", "e")));
}

// ==============================================================================
// ISink Channel Filter Tests
// ==============================================================================

TEST(TestISink, NoChannelFilter_AcceptsAnyChannel)
{
	TestSink sink;

	EXPECT_TRUE(sink.AcceptsEntry(MakeEntry(LogLevel::kInfo, "Physics", "a")));
	EXPECT_TRUE(sink.AcceptsEntry(MakeEntry(LogLevel::kInfo, "Rendering", "b")));
	EXPECT_TRUE(sink.AcceptsEntry(MakeEntry(LogLevel::kInfo, "Audio", "c")));
}

TEST(TestISink, ChannelWhitelist_AcceptsListed)
{
	TestSink sink;
	sink.SetChannelFilter(Dia::Core::StringCRC("Physics"), true);

	EXPECT_TRUE(sink.AcceptsEntry(MakeEntry(LogLevel::kInfo, "Physics", "yes")));
}

TEST(TestISink, ChannelWhitelist_RejectsUnlisted)
{
	TestSink sink;
	sink.SetChannelFilter(Dia::Core::StringCRC("Physics"), true);

	EXPECT_FALSE(sink.AcceptsEntry(MakeEntry(LogLevel::kInfo, "Rendering", "no")));
}

TEST(TestISink, ChannelWhitelist_MultipleChannels)
{
	TestSink sink;
	sink.SetChannelFilter(Dia::Core::StringCRC("Physics"), true);
	sink.SetChannelFilter(Dia::Core::StringCRC("Audio"), true);

	EXPECT_TRUE(sink.AcceptsEntry(MakeEntry(LogLevel::kInfo, "Physics", "a")));
	EXPECT_TRUE(sink.AcceptsEntry(MakeEntry(LogLevel::kInfo, "Audio", "b")));
	EXPECT_FALSE(sink.AcceptsEntry(MakeEntry(LogLevel::kInfo, "Rendering", "c")));
}

TEST(TestISink, RemoveChannelFromWhitelist)
{
	TestSink sink;
	sink.SetChannelFilter(Dia::Core::StringCRC("Physics"), true);
	sink.SetChannelFilter(Dia::Core::StringCRC("Audio"), true);

	sink.SetChannelFilter(Dia::Core::StringCRC("Physics"), false);

	EXPECT_FALSE(sink.AcceptsEntry(MakeEntry(LogLevel::kInfo, "Physics", "removed")));
	EXPECT_TRUE(sink.AcceptsEntry(MakeEntry(LogLevel::kInfo, "Audio", "still there")));
}

TEST(TestISink, ClearChannelFilter_AcceptsAllAgain)
{
	TestSink sink;
	sink.SetChannelFilter(Dia::Core::StringCRC("Physics"), true);

	EXPECT_FALSE(sink.AcceptsEntry(MakeEntry(LogLevel::kInfo, "Rendering", "blocked")));

	sink.ClearChannelFilter();

	EXPECT_TRUE(sink.AcceptsEntry(MakeEntry(LogLevel::kInfo, "Rendering", "now accepted")));
	EXPECT_TRUE(sink.AcceptsEntry(MakeEntry(LogLevel::kInfo, "Physics", "also accepted")));
}

// ==============================================================================
// Combined Level + Channel Filtering
// ==============================================================================

TEST(TestISink, CombinedFilter_BothMustPass)
{
	TestSink sink;
	sink.SetLevelThreshold(LogLevel::kWarning);
	sink.SetChannelFilter(Dia::Core::StringCRC("Physics"), true);

	EXPECT_FALSE(sink.AcceptsEntry(MakeEntry(LogLevel::kInfo, "Physics", "level too low")));
	EXPECT_FALSE(sink.AcceptsEntry(MakeEntry(LogLevel::kWarning, "Rendering", "wrong channel")));
	EXPECT_FALSE(sink.AcceptsEntry(MakeEntry(LogLevel::kInfo, "Rendering", "both wrong")));
	EXPECT_TRUE(sink.AcceptsEntry(MakeEntry(LogLevel::kWarning, "Physics", "both pass")));
	EXPECT_TRUE(sink.AcceptsEntry(MakeEntry(LogLevel::kError, "Physics", "error passes too")));
}

TEST(TestISink, DuplicateChannelAdd_NoDuplicates)
{
	TestSink sink;
	sink.SetChannelFilter(Dia::Core::StringCRC("Physics"), true);
	sink.SetChannelFilter(Dia::Core::StringCRC("Physics"), true);
	sink.SetChannelFilter(Dia::Core::StringCRC("Audio"), true);

	sink.SetChannelFilter(Dia::Core::StringCRC("Physics"), false);

	EXPECT_FALSE(sink.AcceptsEntry(MakeEntry(LogLevel::kInfo, "Physics", "was removed")));
	EXPECT_TRUE(sink.AcceptsEntry(MakeEntry(LogLevel::kInfo, "Audio", "still whitelisted")));
}

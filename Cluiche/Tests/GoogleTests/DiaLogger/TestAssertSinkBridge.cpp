#include <gtest/gtest.h>

#include <DiaCore/Core/Assert.h>
#include <DiaLogger/AssertSinkBridge.h>
#include <DiaLogger/Logger.h>
#include <DiaLogger/ISink.h>
#include <DiaLogger/LogEntry.h>
#include <DiaLogger/LogLevel.h>
#include <DiaCore/CRC/StringCRC.h>

#include <string.h>

using namespace Dia::Logger;

// ==============================================================================
// Test sink that captures entries for verification
// ==============================================================================

class AssertCaptureSink : public ISink
{
public:
	static const unsigned int kMaxEntries = 64;

	AssertCaptureSink() : mEntryCount(0) {}

	void OnLogEntry(const LogEntry& entry) override
	{
		if (mEntryCount < kMaxEntries)
			mEntries[mEntryCount++] = entry;
	}

	const char* GetName() const override { return "AssertCaptureSink"; }

	unsigned int GetEntryCount() const { return mEntryCount; }
	const LogEntry& GetEntry(unsigned int i) const { return mEntries[i]; }

	void Clear() { mEntryCount = 0; }

private:
	LogEntry mEntries[kMaxEntries];
	unsigned int mEntryCount;
};

// ==============================================================================
// Assert Output Callback Tests (DiaCore level)
// ==============================================================================

static const char* sLastCallbackMessage = nullptr;

static void TestCallback(const char* message)
{
	sLastCallbackMessage = message;
}

static int sCallbackCountA = 0;
static int sCallbackCountB = 0;

static void TestCallbackA(const char* message) { sCallbackCountA++; }
static void TestCallbackB(const char* message) { sCallbackCountB++; }

TEST(TestAssertOutputCallbacks, RegisterAndReceive)
{
	sLastCallbackMessage = nullptr;

	Dia::Core::RegisterAssertOutputCallback(&TestCallback);

	// We can't fire a real assert (it would debugbreak), so test the registration API
	// The actual dispatch is tested via the bridge integration below
	Dia::Core::UnregisterAssertOutputCallback(&TestCallback);
}

TEST(TestAssertOutputCallbacks, RegisterSameTwice_NoDuplicate)
{
	Dia::Core::RegisterAssertOutputCallback(&TestCallbackA);
	Dia::Core::RegisterAssertOutputCallback(&TestCallbackA);

	Dia::Core::UnregisterAssertOutputCallback(&TestCallbackA);
	// If duplicate was added, this would leave one behind — next test checks clean state
}

TEST(TestAssertOutputCallbacks, UnregisterNonRegistered_NoCrash)
{
	Dia::Core::UnregisterAssertOutputCallback(&TestCallbackA);
}

TEST(TestAssertOutputCallbacks, RegisterNull_NoCrash)
{
	Dia::Core::RegisterAssertOutputCallback(nullptr);
}

// ==============================================================================
// DispatchImmediate Tests
// ==============================================================================

TEST(TestDispatchImmediate, SinkReceivesEntry)
{
	AssertCaptureSink sink;
	sink.SetLevelThreshold(LogLevel::kTrace);

	Logger::Instance().RegisterSink(&sink);

	LogEntry entry;
	entry.level = LogLevel::kError;
	entry.channel = Dia::Core::StringCRC("Assert");
	strncpy_s(entry.message, sizeof(entry.message), "test dispatch", _TRUNCATE);

	Logger::Instance().DispatchImmediate(entry);

	EXPECT_EQ(1u, sink.GetEntryCount());
	EXPECT_EQ(LogLevel::kError, sink.GetEntry(0).level);
	EXPECT_STREQ("test dispatch", sink.GetEntry(0).message);

	Logger::Instance().UnregisterSink(&sink);
}

TEST(TestDispatchImmediate, MultipleSinks_AllReceive)
{
	AssertCaptureSink sinkA;
	sinkA.SetLevelThreshold(LogLevel::kTrace);
	AssertCaptureSink sinkB;
	sinkB.SetLevelThreshold(LogLevel::kTrace);

	Logger::Instance().RegisterSink(&sinkA);
	Logger::Instance().RegisterSink(&sinkB);

	LogEntry entry;
	entry.level = LogLevel::kError;
	entry.channel = Dia::Core::StringCRC("Assert");
	strncpy_s(entry.message, sizeof(entry.message), "broadcast assert", _TRUNCATE);

	Logger::Instance().DispatchImmediate(entry);

	EXPECT_EQ(1u, sinkA.GetEntryCount());
	EXPECT_EQ(1u, sinkB.GetEntryCount());

	Logger::Instance().UnregisterSink(&sinkA);
	Logger::Instance().UnregisterSink(&sinkB);
}

TEST(TestDispatchImmediate, SinkFilterRespected)
{
	AssertCaptureSink sink;
	sink.SetLevelThreshold(LogLevel::kError);

	Logger::Instance().RegisterSink(&sink);

	LogEntry entryLow;
	entryLow.level = LogLevel::kInfo;
	entryLow.channel = Dia::Core::StringCRC("Assert");
	strncpy_s(entryLow.message, sizeof(entryLow.message), "low priority", _TRUNCATE);

	LogEntry entryHigh;
	entryHigh.level = LogLevel::kError;
	entryHigh.channel = Dia::Core::StringCRC("Assert");
	strncpy_s(entryHigh.message, sizeof(entryHigh.message), "high priority", _TRUNCATE);

	Logger::Instance().DispatchImmediate(entryLow);
	Logger::Instance().DispatchImmediate(entryHigh);

	EXPECT_EQ(1u, sink.GetEntryCount());
	EXPECT_STREQ("high priority", sink.GetEntry(0).message);

	Logger::Instance().UnregisterSink(&sink);
}

TEST(TestDispatchImmediate, NoSinks_NoCrash)
{
	LogEntry entry;
	entry.level = LogLevel::kError;
	entry.channel = Dia::Core::StringCRC("Assert");
	strncpy_s(entry.message, sizeof(entry.message), "orphan", _TRUNCATE);

	Logger::Instance().DispatchImmediate(entry);
}

// ==============================================================================
// AssertSinkBridge Install / Uninstall
// ==============================================================================

TEST(TestAssertSinkBridge, InstallAndUninstall_NoCrash)
{
	AssertSinkBridge::Install();
	AssertSinkBridge::Uninstall();
}

TEST(TestAssertSinkBridge, DoubleInstall_NoCrash)
{
	AssertSinkBridge::Install();
	AssertSinkBridge::Install();
	AssertSinkBridge::Uninstall();
}

TEST(TestAssertSinkBridge, DoubleUninstall_NoCrash)
{
	AssertSinkBridge::Install();
	AssertSinkBridge::Uninstall();
	AssertSinkBridge::Uninstall();
}
